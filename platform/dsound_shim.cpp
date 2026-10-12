// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/dsound_shim.h"

// The one translation unit that compiles miniaudio's implementation. It is
// warning-clean under -Wall -Wextra on GCC 13 and Clang 18.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

DEFINE_GUID(IID_IDirectSound3DListener, 0x279AFA84, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
DEFINE_GUID(IID_IDirectSound3DBuffer, 0x279AFA86, 0x4981, 0x11CE, 0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60);
DEFINE_GUID(IID_IKsPropertySet, 0x31EFAC30, 0x515C, 0x11D0, 0xA9, 0xAA, 0x00, 0xAA, 0x00, 0x61, 0xBE, 0x93);

namespace {

const ma_uint32 kDeviceSampleRate = 44100;
const ma_uint32 kDeviceChannels = 2;
const DWORD kMinFrequency = 100;
const DWORD kMaxFrequency = 100000;
const DWORD kMaxBufferBytes = 0x0FFFFFFF;
// The write cursor leads the play cursor by about one 60 Hz frame of audio.
const DWORD kWriteLeadDivisor = 60;

std::atomic<bool> g_useNullBackend { false };

// Bytes shared by a buffer and its duplicates. The engine writes into
// `staging` through Lock; Unlock copies the written spans into `audible`
// under `mutex`, and the audio thread reads only `audible`, under the same
// mutex. The engine's writes therefore never race with the mixer's reads.
struct PcmRing {
    explicit PcmRing(DWORD bytes)
        : staging(bytes, 0)
        , audible(bytes, 0)
    {
    }

    std::mutex mutex;
    std::vector<BYTE> staging;
    std::vector<BYTE> audible;
};

class SoundBuffer;

// miniaudio requires ma_data_source_base at offset 0 of a data source.
struct RingSource {
    ma_data_source_base base;
    SoundBuffer* owner;
};
static_assert(std::is_standard_layout<RingSource>::value, "RingSource must be standard layout");

float VolumeToGain(LONG volume)
{
    if (volume <= DSBVOLUME_MIN)
        return 0.0f;
    return static_cast<float>(std::pow(10.0, volume / 2000.0));
}

// miniaudio balance pan scales the attenuated side by 1 - |pan|, so the side
// opposite the pan direction gets 10^(-|p| / 2000), as DirectSound defines.
float PanToBalance(LONG pan)
{
    if (pan == 0)
        return 0.0f;
    float sign = pan < 0 ? -1.0f : 1.0f;
    if (pan <= DSBPAN_LEFT || pan >= DSBPAN_RIGHT)
        return sign;
    double attenuation = std::abs(pan) / 2000.0;
    return sign * static_cast<float>(1.0 - std::pow(10.0, -attenuation));
}

bool IsValidSecondaryFormat(const WAVEFORMATEX* wfx)
{
    if (!wfx || wfx->wFormatTag != WAVE_FORMAT_PCM)
        return false;
    if (wfx->nChannels != 1 && wfx->nChannels != 2)
        return false;
    if (wfx->wBitsPerSample != 8 && wfx->wBitsPerSample != 16)
        return false;
    if (wfx->nBlockAlign != wfx->nChannels * wfx->wBitsPerSample / 8)
        return false;
    return wfx->nSamplesPerSec >= kMinFrequency && wfx->nSamplesPerSec <= kMaxFrequency;
}

class Listener3D final : public IDirectSound3DListener {
public:
    HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) override
    {
        if (!ppvObj)
            return DSERR_INVALIDPARAM;
        if (!IsEqualGUID(riid, IID_IDirectSound3DListener)) {
            *ppvObj = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        *ppvObj = this;
        return DS_OK;
    }
    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }
    HRESULT SetOrientation(D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, D3DVALUE, DWORD) override { return DS_OK; }
    HRESULT SetPosition(D3DVALUE, D3DVALUE, D3DVALUE, DWORD) override { return DS_OK; }

private:
    std::atomic<ULONG> m_refs { 1 };
};

class Buffer3D final : public IDirectSound3DBuffer {
public:
    HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) override
    {
        if (!ppvObj)
            return DSERR_INVALIDPARAM;
        if (!IsEqualGUID(riid, IID_IDirectSound3DBuffer)) {
            *ppvObj = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        *ppvObj = this;
        return DS_OK;
    }
    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }
    HRESULT SetMinDistance(D3DVALUE, DWORD) override { return DS_OK; }
    HRESULT SetMode(DWORD, DWORD) override { return DS_OK; }
    HRESULT SetPosition(D3DVALUE, D3DVALUE, D3DVALUE, DWORD) override { return DS_OK; }

private:
    std::atomic<ULONG> m_refs { 1 };
};

class DirectSound final : public IDirectSound {
public:
    HRESULT Init()
    {
        ma_engine_config config = ma_engine_config_init();
        if (g_useNullBackend.load()) {
            ma_backend backends[] = { ma_backend_null };
            if (ma_context_init(backends, 1, nullptr, &m_context) != MA_SUCCESS)
                return E_FAIL;
            m_hasContext = true;
            config.pContext = &m_context;
        }
        config.channels = kDeviceChannels;
        config.sampleRate = kDeviceSampleRate;
        if (ma_engine_init(&config, &m_engine) != MA_SUCCESS)
            return E_FAIL;
        m_hasEngine = true;
        return DS_OK;
    }

    ma_engine* Engine() { return &m_engine; }
    void SetPrimaryFormat(const WAVEFORMATEX& wfx) { m_primaryFormat = wfx; }

    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }

    HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, IDirectSoundBuffer** ppDSBuffer, LPVOID) override;
    HRESULT DuplicateSoundBuffer(IDirectSoundBuffer* pDSBufferOriginal, IDirectSoundBuffer** ppDSBufferDuplicate) override;

    HRESULT GetCaps(LPDSCAPS pDSCaps) override
    {
        if (!pDSCaps)
            return DSERR_INVALIDPARAM;
        std::memset(pDSCaps, 0, sizeof(*pDSCaps));
        pDSCaps->dwSize = sizeof(*pDSCaps);
        pDSCaps->dwFlags = DSCAPS_PRIMARY16BIT | DSCAPS_PRIMARYSTEREO;
        return DS_OK;
    }

    HRESULT SetCooperativeLevel(HWND, DWORD) override { return DS_OK; }
    HRESULT Compact() override { return DS_OK; }
    HRESULT Initialize(const GUID*) override { return DS_OK; }

private:
    ~DirectSound() override
    {
        if (m_hasEngine)
            ma_engine_uninit(&m_engine);
        if (m_hasContext)
            ma_context_uninit(&m_context);
    }

    std::atomic<ULONG> m_refs { 1 };
    ma_context m_context;
    ma_engine m_engine;
    bool m_hasContext = false;
    bool m_hasEngine = false;
    WAVEFORMATEX m_primaryFormat = {};
};

// A primary buffer (no ring, no sound) or a secondary buffer playing its
// PCM ring through its own ma_sound. Holds a reference to its device.
class SoundBuffer final : public IDirectSoundBuffer {
public:
    SoundBuffer(DirectSound* device, DWORD flags)
        : m_device(device)
        , m_flags(flags)
    {
        m_device->AddRef();
    }

    bool IsPrimary() const { return (m_flags & DSBCAPS_PRIMARYBUFFER) != 0; }
    DWORD Flags() const { return m_flags; }

    HRESULT InitSecondary(const WAVEFORMATEX& wfx, std::shared_ptr<PcmRing> ring)
    {
        m_format = wfx;
        m_ring = std::move(ring);
        m_bytes = static_cast<DWORD>(m_ring->staging.size());
        m_frames = m_bytes / m_format.nBlockAlign;

        static const ma_data_source_vtable vtable = {
            OnRead, OnSeek, OnGetDataFormat, OnGetCursor, OnGetLength, nullptr, 0
        };
        m_source.owner = this;
        ma_data_source_config sourceConfig = ma_data_source_config_init();
        sourceConfig.vtable = &vtable;
        if (ma_data_source_init(&sourceConfig, &m_source.base) != MA_SUCCESS)
            return E_FAIL;
        m_hasSource = true;

        if (ma_sound_init_from_data_source(m_device->Engine(), &m_source, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &m_sound) != MA_SUCCESS)
            return E_FAIL;
        m_hasSound = true;
        ma_sound_set_pan_mode(&m_sound, ma_pan_mode_balance);
        return DS_OK;
    }

    // Copies the creation flags, format, data and volume, pan and frequency.
    HRESULT InitDuplicate(const SoundBuffer& original)
    {
        HRESULT hr = InitSecondary(original.m_format, original.m_ring);
        if (FAILED(hr))
            return hr;
        SetVolume(original.m_volume);
        SetPan(original.m_pan);
        SetFrequency(original.m_frequency);
        return DS_OK;
    }

    platform::dsound::DebugState GetDebugState() const
    {
        if (!m_hasSound)
            return { 1.0f, 1.0f, 1.0f };
        float gain = ma_sound_get_volume(&m_sound);
        float pan = ma_sound_get_pan(&m_sound);
        float left = gain * (pan > 0.0f ? 1.0f - pan : 1.0f);
        float right = gain * (pan < 0.0f ? 1.0f + pan : 1.0f);
        return { left, right, ma_sound_get_pitch(&m_sound) };
    }

    HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) override
    {
        if (!ppvObj)
            return DSERR_INVALIDPARAM;
        *ppvObj = nullptr;
        if ((m_flags & DSBCAPS_CTRL3D) == 0)
            return E_NOINTERFACE;
        if (IsPrimary() && IsEqualGUID(riid, IID_IDirectSound3DListener)) {
            *ppvObj = static_cast<IDirectSound3DListener*>(new Listener3D());
            return DS_OK;
        }
        if (!IsPrimary() && IsEqualGUID(riid, IID_IDirectSound3DBuffer)) {
            *ppvObj = static_cast<IDirectSound3DBuffer*>(new Buffer3D());
            return DS_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }

    HRESULT GetCaps(LPDSBCAPS lpDSBufferCaps) override
    {
        if (!lpDSBufferCaps)
            return DSERR_INVALIDPARAM;
        std::memset(lpDSBufferCaps, 0, sizeof(*lpDSBufferCaps));
        lpDSBufferCaps->dwSize = sizeof(*lpDSBufferCaps);
        lpDSBufferCaps->dwFlags = m_flags;
        lpDSBufferCaps->dwBufferBytes = m_bytes;
        return DS_OK;
    }

    HRESULT GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor) override
    {
        DWORD play = 0;
        DWORD write = 0;
        if (!IsPrimary()) {
            play = PlayCursor();
            write = play;
            if (IsPlaying()) {
                DWORD frames = std::max<DWORD>(1, m_format.nSamplesPerSec / kWriteLeadDivisor);
                write = static_cast<DWORD>((play + static_cast<ma_uint64>(frames) * m_format.nBlockAlign) % m_bytes);
            }
        }
        if (lpdwCurrentPlayCursor)
            *lpdwCurrentPlayCursor = play;
        if (lpdwCurrentWriteCursor)
            *lpdwCurrentWriteCursor = write;
        return DS_OK;
    }

    HRESULT GetVolume(LONG* lplVolume) override
    {
        if (!lplVolume)
            return DSERR_INVALIDPARAM;
        *lplVolume = m_volume;
        return DS_OK;
    }

    HRESULT GetStatus(LPDWORD lpdwStatus) override
    {
        if (!lpdwStatus)
            return DSERR_INVALIDPARAM;
        *lpdwStatus = 0;
        if (IsPrimary()) {
            *lpdwStatus = DSBSTATUS_PLAYING;
        } else if (IsPlaying()) {
            *lpdwStatus = DSBSTATUS_PLAYING;
            if (m_looping.load())
                *lpdwStatus |= DSBSTATUS_LOOPING;
        }
        return DS_OK;
    }

    HRESULT Lock(DWORD dwOffset, DWORD dwBytes, LPVOID* ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID* ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags) override
    {
        if (IsPrimary() || !ppvAudioPtr1 || !pdwAudioBytes1)
            return DSERR_INVALIDPARAM;
        if (dwFlags & DSBLOCK_ENTIREBUFFER) {
            dwOffset = 0;
            dwBytes = m_bytes;
        } else if (dwFlags & DSBLOCK_FROMWRITECURSOR) {
            GetCurrentPosition(nullptr, &dwOffset);
        }
        if (dwOffset >= m_bytes || dwBytes == 0 || dwBytes > m_bytes)
            return DSERR_INVALIDPARAM;

        BYTE* base = m_ring->staging.data();
        DWORD bytes1 = std::min(dwBytes, m_bytes - dwOffset);
        DWORD bytes2 = (ppvAudioPtr2 && pdwAudioBytes2) ? dwBytes - bytes1 : 0;
        *ppvAudioPtr1 = base + dwOffset;
        *pdwAudioBytes1 = bytes1;
        if (ppvAudioPtr2)
            *ppvAudioPtr2 = bytes2 != 0 ? base : nullptr;
        if (pdwAudioBytes2)
            *pdwAudioBytes2 = bytes2;
        return DS_OK;
    }

    HRESULT Play(DWORD, DWORD, DWORD dwFlags) override
    {
        if (IsPrimary())
            return DS_OK;
        m_looping.store((dwFlags & DSBPLAY_LOOPING) != 0);
        {
            // A buffer that played to its end starts over, as DirectSound's does.
            std::lock_guard<std::mutex> lock(m_ring->mutex);
            if (m_cursor >= m_frames)
                m_cursor = 0;
        }
        // ma_sound_start seeks a finished sound to frame 0; the cursor was
        // already settled above, which keeps a SetCurrentPosition after the end.
        m_ignoreSeek.store(true);
        ma_result result = ma_sound_start(&m_sound);
        m_ignoreSeek.store(false);
        return result == MA_SUCCESS ? DS_OK : DSERR_INVALIDPARAM;
    }

    HRESULT SetCurrentPosition(DWORD dwNewPosition) override
    {
        if (IsPrimary())
            return DS_OK;
        if (dwNewPosition >= m_bytes)
            return DSERR_INVALIDPARAM;
        std::lock_guard<std::mutex> lock(m_ring->mutex);
        m_cursor = dwNewPosition / m_format.nBlockAlign;
        return DS_OK;
    }

    HRESULT SetFormat(const WAVEFORMATEX* pcfxFormat) override
    {
        if (!IsPrimary() || !pcfxFormat)
            return DSERR_INVALIDPARAM;
        m_device->SetPrimaryFormat(*pcfxFormat);
        return DS_OK;
    }

    HRESULT SetVolume(LONG lVolume) override
    {
        m_volume = std::min<LONG>(std::max<LONG>(lVolume, DSBVOLUME_MIN), DSBVOLUME_MAX);
        if (m_hasSound)
            ma_sound_set_volume(&m_sound, VolumeToGain(m_volume));
        return DS_OK;
    }

    HRESULT SetPan(LONG lPan) override
    {
        m_pan = std::min<LONG>(std::max<LONG>(lPan, DSBPAN_LEFT), DSBPAN_RIGHT);
        if (m_hasSound)
            ma_sound_set_pan(&m_sound, PanToBalance(m_pan));
        return DS_OK;
    }

    HRESULT SetFrequency(DWORD dwFrequency) override
    {
        if (dwFrequency != DSBFREQUENCY_ORIGINAL)
            dwFrequency = std::min(std::max(dwFrequency, kMinFrequency), kMaxFrequency);
        m_frequency = dwFrequency;
        if (m_hasSound) {
            float pitch = dwFrequency == DSBFREQUENCY_ORIGINAL ? 1.0f : static_cast<float>(dwFrequency) / static_cast<float>(m_format.nSamplesPerSec);
            ma_sound_set_pitch(&m_sound, pitch);
        }
        return DS_OK;
    }

    HRESULT Stop() override
    {
        if (m_hasSound)
            ma_sound_stop(&m_sound);
        return DS_OK;
    }

    HRESULT Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) override
    {
        if (IsPrimary())
            return DSERR_INVALIDPARAM;
        if (!IsSpanInRing(pvAudioPtr1, dwAudioBytes1) || !IsSpanInRing(pvAudioPtr2, dwAudioBytes2))
            return DSERR_INVALIDPARAM;
        std::lock_guard<std::mutex> lock(m_ring->mutex);
        Publish(pvAudioPtr1, dwAudioBytes1);
        Publish(pvAudioPtr2, dwAudioBytes2);
        return DS_OK;
    }

    HRESULT Restore() override { return DS_OK; }

private:
    ~SoundBuffer() override
    {
        // Uninitializing the sound detaches it from the graph, so the audio
        // thread no longer calls into the data source.
        if (m_hasSound)
            ma_sound_uninit(&m_sound);
        if (m_hasSource)
            ma_data_source_uninit(&m_source.base);
        m_device->Release();
    }

    bool IsPlaying() const { return m_hasSound && ma_sound_is_playing(&m_sound) && !ma_sound_at_end(&m_sound); }

    DWORD PlayCursor()
    {
        std::lock_guard<std::mutex> lock(m_ring->mutex);
        return static_cast<DWORD>((m_cursor * m_format.nBlockAlign) % m_bytes);
    }

    bool IsSpanInRing(LPVOID ptr, DWORD bytes) const
    {
        if (bytes == 0)
            return true;
        const BYTE* base = m_ring->staging.data();
        const BYTE* p = static_cast<const BYTE*>(ptr);
        return p >= base && p <= base + m_bytes && bytes <= static_cast<DWORD>(base + m_bytes - p);
    }

    // Caller holds the ring mutex.
    void Publish(LPVOID ptr, DWORD bytes)
    {
        if (bytes == 0)
            return;
        size_t offset = static_cast<const BYTE*>(ptr) - m_ring->staging.data();
        std::memcpy(m_ring->audible.data() + offset, ptr, bytes);
    }

    // Audio thread. Loops back to the start when looping; otherwise the
    // cursor stays at the end and reads return MA_AT_END, which stops the
    // sound.
    ma_result Read(void* framesOut, ma_uint64 frameCount, ma_uint64* framesRead)
    {
        ma_uint64 total = 0;
        BYTE* out = static_cast<BYTE*>(framesOut);
        bool looping = m_looping.load();
        std::lock_guard<std::mutex> lock(m_ring->mutex);
        while (total < frameCount) {
            if (m_cursor >= m_frames) {
                if (!looping || m_frames == 0)
                    break;
                m_cursor = 0;
            }
            ma_uint64 count = std::min(frameCount - total, m_frames - m_cursor);
            if (out) {
                std::memcpy(out + total * m_format.nBlockAlign, m_ring->audible.data() + m_cursor * m_format.nBlockAlign,
                    static_cast<size_t>(count * m_format.nBlockAlign));
            }
            m_cursor += count;
            total += count;
        }
        if (framesRead)
            *framesRead = total;
        return total == 0 ? MA_AT_END : MA_SUCCESS;
    }

    static ma_result OnRead(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead)
    {
        return static_cast<RingSource*>(pDataSource)->owner->Read(pFramesOut, frameCount, pFramesRead);
    }

    static ma_result OnSeek(ma_data_source* pDataSource, ma_uint64 frameIndex)
    {
        SoundBuffer* self = static_cast<RingSource*>(pDataSource)->owner;
        if (frameIndex > self->m_frames)
            return MA_INVALID_ARGS;
        if (self->m_ignoreSeek.load())
            return MA_SUCCESS;
        std::lock_guard<std::mutex> lock(self->m_ring->mutex);
        self->m_cursor = frameIndex;
        return MA_SUCCESS;
    }

    static ma_result OnGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat, ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap, size_t channelMapCap)
    {
        const WAVEFORMATEX& wfx = static_cast<RingSource*>(pDataSource)->owner->m_format;
        if (pFormat)
            *pFormat = wfx.wBitsPerSample == 8 ? ma_format_u8 : ma_format_s16;
        if (pChannels)
            *pChannels = wfx.nChannels;
        if (pSampleRate)
            *pSampleRate = wfx.nSamplesPerSec;
        if (pChannelMap)
            ma_channel_map_init_standard(ma_standard_channel_map_default, pChannelMap, channelMapCap, wfx.nChannels);
        return MA_SUCCESS;
    }

    static ma_result OnGetCursor(ma_data_source* pDataSource, ma_uint64* pCursor)
    {
        SoundBuffer* self = static_cast<RingSource*>(pDataSource)->owner;
        std::lock_guard<std::mutex> lock(self->m_ring->mutex);
        *pCursor = std::min(self->m_cursor, self->m_frames);
        return MA_SUCCESS;
    }

    static ma_result OnGetLength(ma_data_source* pDataSource, ma_uint64* pLength)
    {
        *pLength = static_cast<RingSource*>(pDataSource)->owner->m_frames;
        return MA_SUCCESS;
    }

    std::atomic<ULONG> m_refs { 1 };
    DirectSound* m_device;
    DWORD m_flags;
    WAVEFORMATEX m_format = {};
    DWORD m_bytes = 0;
    ma_uint64 m_frames = 0;
    std::shared_ptr<PcmRing> m_ring;
    RingSource m_source = {};
    ma_sound m_sound;
    bool m_hasSource = false;
    bool m_hasSound = false;
    LONG m_volume = DSBVOLUME_MAX;
    LONG m_pan = 0;
    DWORD m_frequency = DSBFREQUENCY_ORIGINAL;
    std::atomic<bool> m_looping { false };
    std::atomic<bool> m_ignoreSeek { false };
    ma_uint64 m_cursor = 0; // Frames consumed; guarded by m_ring->mutex.
};

HRESULT DirectSound::CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, IDirectSoundBuffer** ppDSBuffer, LPVOID)
{
    if (!ppDSBuffer)
        return DSERR_INVALIDPARAM;
    *ppDSBuffer = nullptr;
    if (!pcDSBufferDesc || pcDSBufferDesc->dwSize != sizeof(DSBUFFERDESC))
        return DSERR_INVALIDPARAM;

    DWORD flags = pcDSBufferDesc->dwFlags;
    if (flags & DSBCAPS_PRIMARYBUFFER) {
        *ppDSBuffer = new SoundBuffer(this, flags);
        return DS_OK;
    }

    const WAVEFORMATEX* wfx = pcDSBufferDesc->lpwfxFormat;
    DWORD bytes = pcDSBufferDesc->dwBufferBytes;
    if (!IsValidSecondaryFormat(wfx) || bytes < wfx->nBlockAlign || bytes > kMaxBufferBytes)
        return DSERR_INVALIDPARAM;

    SoundBuffer* buffer = new SoundBuffer(this, flags);
    HRESULT hr = buffer->InitSecondary(*wfx, std::make_shared<PcmRing>(bytes));
    if (FAILED(hr)) {
        buffer->Release();
        return hr;
    }
    *ppDSBuffer = buffer;
    return DS_OK;
}

HRESULT DirectSound::DuplicateSoundBuffer(IDirectSoundBuffer* pDSBufferOriginal, IDirectSoundBuffer** ppDSBufferDuplicate)
{
    if (!ppDSBufferDuplicate)
        return DSERR_INVALIDPARAM;
    *ppDSBufferDuplicate = nullptr;
    SoundBuffer* original = static_cast<SoundBuffer*>(pDSBufferOriginal);
    if (!original || original->IsPrimary())
        return DSERR_INVALIDPARAM;

    SoundBuffer* buffer = new SoundBuffer(this, original->Flags());
    HRESULT hr = buffer->InitDuplicate(*original);
    if (FAILED(hr)) {
        buffer->Release();
        return hr;
    }
    *ppDSBufferDuplicate = buffer;
    return DS_OK;
}

} // namespace

HRESULT DirectSoundCreate(GUID*, IDirectSound** ppDS, LPVOID)
{
    if (!ppDS)
        return DSERR_INVALIDPARAM;
    *ppDS = nullptr;
    DirectSound* device = new DirectSound();
    HRESULT hr = device->Init();
    if (FAILED(hr)) {
        device->Release();
        return hr;
    }
    *ppDS = device;
    return DS_OK;
}

namespace platform::dsound {

void UseNullBackend(bool useNull) { g_useNullBackend.store(useNull); }

DebugState GetDebugState(IDirectSoundBuffer* buffer)
{
    if (!buffer)
        return { 0.0f, 0.0f, 0.0f };
    return static_cast<SoundBuffer*>(buffer)->GetDebugState();
}

} // namespace platform::dsound
