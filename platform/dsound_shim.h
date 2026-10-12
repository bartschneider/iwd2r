// iwd2r. Distributed under LICENSE.md.
// DirectSound shim: the subset of the DirectSound API that the sound mixer,
// voices and the streaming music player call, implemented on miniaudio. The
// interfaces are plain C++ classes with virtual methods, not COM objects.
// Struct field order and constant values follow the public documentation.
#pragma once

#include "compat/win_types.h"

// Results.
#define DS_OK ((HRESULT)0)
#define DSERR_ALLOCATED ((HRESULT)0x8878000A)
#define DSERR_INVALIDPARAM ((HRESULT)0x80070057)
#define DSERR_BUFFERLOST ((HRESULT)0x88780096)

// DSBUFFERDESC::dwFlags and DSBCAPS::dwFlags.
#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define DSBCAPS_STATIC 0x00000002
#define DSBCAPS_CTRL3D 0x00000010
#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLPAN 0x00000040
#define DSBCAPS_CTRLVOLUME 0x00000080
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000

// DSCAPS::dwFlags.
#define DSCAPS_PRIMARYSTEREO 0x00000002
#define DSCAPS_PRIMARY16BIT 0x00000008

// IDirectSound::SetCooperativeLevel.
#define DSSCL_EXCLUSIVE 0x00000003

// IDirectSoundBuffer::Play, GetStatus and Lock.
#define DSBPLAY_LOOPING 0x00000001
#define DSBSTATUS_PLAYING 0x00000001
#define DSBSTATUS_LOOPING 0x00000004
#define DSBLOCK_FROMWRITECURSOR 0x00000001
#define DSBLOCK_ENTIREBUFFER 0x00000002

// Volume in hundredths of a decibel, pan in hundredths of a decibel of
// attenuation on the opposite side, frequency in Hz.
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0
#define DSBPAN_LEFT (-10000)
#define DSBPAN_RIGHT 10000
#define DSBFREQUENCY_ORIGINAL 0

// 3D parameter application mode.
#define DS3D_IMMEDIATE 0x00000000

// IKsPropertySet::QuerySupport. The engine composes its own SETGET mask.
#define KSPROPERTY_SUPPORT_GET 0x00000001
#define KSPROPERTY_SUPPORT_SET 0x00000002

#define WAVE_FORMAT_PCM 1

typedef float D3DVALUE;

// Packed as on Windows, where sizeof(WAVEFORMAT) is 14 and sizeof(WAVEFORMATEX) is 18.
#pragma pack(push, 1)
struct WAVEFORMAT {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
};
struct WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
};
#pragma pack(pop)
typedef WAVEFORMATEX* LPWAVEFORMATEX;

struct DSBUFFERDESC {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwReserved;
    LPWAVEFORMATEX lpwfxFormat;
};
typedef DSBUFFERDESC* LPDSBUFFERDESC;
typedef const DSBUFFERDESC* LPCDSBUFFERDESC;

struct DSCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwMinSecondarySampleRate;
    DWORD dwMaxSecondarySampleRate;
    DWORD dwPrimaryBuffers;
    DWORD dwMaxHwMixingAllBuffers;
    DWORD dwMaxHwMixingStaticBuffers;
    DWORD dwMaxHwMixingStreamingBuffers;
    DWORD dwFreeHwMixingAllBuffers;
    DWORD dwFreeHwMixingStaticBuffers;
    DWORD dwFreeHwMixingStreamingBuffers;
    DWORD dwMaxHw3DAllBuffers;
    DWORD dwMaxHw3DStaticBuffers;
    DWORD dwMaxHw3DStreamingBuffers;
    DWORD dwFreeHw3DAllBuffers;
    DWORD dwFreeHw3DStaticBuffers;
    DWORD dwFreeHw3DStreamingBuffers;
    DWORD dwTotalHwMemBytes;
    DWORD dwFreeHwMemBytes;
    DWORD dwMaxContigFreeHwMemBytes;
    DWORD dwUnlockTransferRateHwBuffers;
    DWORD dwPlayCpuOverheadSwBuffers;
    DWORD dwReserved1;
    DWORD dwReserved2;
};
typedef DSCAPS* LPDSCAPS;

struct DSBCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwBufferBytes;
    DWORD dwUnlockTransferRate;
    DWORD dwPlayCpuOverhead;
};
typedef DSBCAPS* LPDSBCAPS;

extern const GUID IID_IDirectSound3DListener;
extern const GUID IID_IDirectSound3DBuffer;
extern const GUID IID_IKsPropertySet;

// Never created by the shim: QueryInterface(IID_IKsPropertySet) fails, so
// the engine reports no EAX support. Declared for the engine's call sites.
class IKsPropertySet {
public:
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT Set(REFGUID guidPropSet, ULONG dwPropID, LPVOID pInstanceData, ULONG cbInstanceData, LPVOID pPropData, ULONG cbPropData) = 0;
    virtual HRESULT QuerySupport(REFGUID guidPropSet, ULONG dwPropID, ULONG* pTypeSupport) = 0;

protected:
    virtual ~IKsPropertySet() = default;
};

// No-op: every method succeeds.
class IDirectSound3DListener {
public:
    virtual HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT SetOrientation(D3DVALUE xFront, D3DVALUE yFront, D3DVALUE zFront, D3DVALUE xTop, D3DVALUE yTop, D3DVALUE zTop, DWORD dwApply) = 0;
    virtual HRESULT SetPosition(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply) = 0;

protected:
    virtual ~IDirectSound3DListener() = default;
};

// No-op: every method succeeds.
class IDirectSound3DBuffer {
public:
    virtual HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT SetMinDistance(D3DVALUE flMinDistance, DWORD dwApply) = 0;
    virtual HRESULT SetMode(DWORD dwMode, DWORD dwApply) = 0;
    virtual HRESULT SetPosition(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply) = 0;

protected:
    virtual ~IDirectSound3DBuffer() = default;
};

class IDirectSoundBuffer {
public:
    virtual HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT GetCaps(LPDSBCAPS lpDSBufferCaps) = 0;
    virtual HRESULT GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor) = 0;
    virtual HRESULT GetVolume(LONG* lplVolume) = 0;
    virtual HRESULT GetStatus(LPDWORD lpdwStatus) = 0;
    virtual HRESULT Lock(DWORD dwOffset, DWORD dwBytes, LPVOID* ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID* ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags) = 0;
    virtual HRESULT Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) = 0;
    virtual HRESULT SetCurrentPosition(DWORD dwNewPosition) = 0;
    virtual HRESULT SetFormat(const WAVEFORMATEX* pcfxFormat) = 0;
    virtual HRESULT SetVolume(LONG lVolume) = 0;
    virtual HRESULT SetPan(LONG lPan) = 0;
    virtual HRESULT SetFrequency(DWORD dwFrequency) = 0;
    virtual HRESULT Stop() = 0;
    virtual HRESULT Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) = 0;
    virtual HRESULT Restore() = 0;

protected:
    virtual ~IDirectSoundBuffer() = default;
};

class IDirectSound {
public:
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, IDirectSoundBuffer** ppDSBuffer, LPVOID pUnkOuter) = 0;
    virtual HRESULT GetCaps(LPDSCAPS pDSCaps) = 0;
    virtual HRESULT DuplicateSoundBuffer(IDirectSoundBuffer* pDSBufferOriginal, IDirectSoundBuffer** ppDSBufferDuplicate) = 0;
    virtual HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwLevel) = 0;
    virtual HRESULT Compact() = 0;
    virtual HRESULT Initialize(const GUID* pcGuidDevice) = 0;

protected:
    virtual ~IDirectSound() = default;
};

typedef IDirectSound* LPDIRECTSOUND;
typedef IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;
typedef IDirectSound3DBuffer* LPDIRECTSOUND3DBUFFER;
typedef IDirectSound3DListener* LPDIRECTSOUND3DLISTENER;

// C-style call macros used by the music player.
#define IDirectSound_CreateSoundBuffer(p, a, b, c) (p)->CreateSoundBuffer(a, b, c)
#define IDirectSound_GetCaps(p, a) (p)->GetCaps(a)
#define IDirectSound_Release(p) (p)->Release()
#define IDirectSound_SetCooperativeLevel(p, a, b) (p)->SetCooperativeLevel(a, b)
#define IDirectSoundBuffer_GetCurrentPosition(p, a, b) (p)->GetCurrentPosition(a, b)
#define IDirectSoundBuffer_GetStatus(p, a) (p)->GetStatus(a)
#define IDirectSoundBuffer_GetVolume(p, a) (p)->GetVolume(a)
#define IDirectSoundBuffer_Lock(p, a, b, c, d, e, f, g) (p)->Lock(a, b, c, d, e, f, g)
#define IDirectSoundBuffer_Play(p, a, b, c) (p)->Play(a, b, c)
#define IDirectSoundBuffer_Release(p) (p)->Release()
#define IDirectSoundBuffer_Restore(p) (p)->Restore()
#define IDirectSoundBuffer_SetCurrentPosition(p, a) (p)->SetCurrentPosition(a)
#define IDirectSoundBuffer_SetFormat(p, a) (p)->SetFormat(a)
#define IDirectSoundBuffer_SetFrequency(p, a) (p)->SetFrequency(a)
#define IDirectSoundBuffer_SetPan(p, a) (p)->SetPan(a)
#define IDirectSoundBuffer_SetVolume(p, a) (p)->SetVolume(a)
#define IDirectSoundBuffer_Stop(p) (p)->Stop()
#define IDirectSoundBuffer_Unlock(p, a, b, c, d) (p)->Unlock(a, b, c, d)

// Creates a device object backed by a miniaudio engine (44100 Hz stereo
// float; each buffer is resampled to it). lpGuid and pUnkOuter are ignored.
HRESULT DirectSoundCreate(GUID* lpGuid, IDirectSound** ppDS, LPVOID pUnkOuter);

namespace platform::dsound {

// When true, devices created afterwards use miniaudio's null backend: no
// audio hardware is opened, but buffers are still consumed in real time.
void UseNullBackend(bool useNull);

// Effective channel gains (volume times pan, per side) and pitch ratio that
// a buffer applies. A primary buffer reports unity; null reports zero.
struct DebugState {
    float left;
    float right;
    float pitch;
};
DebugState GetDebugState(IDirectSoundBuffer* buffer);

} // namespace platform::dsound
