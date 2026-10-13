#define NOMINMAX
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "platform/dsound_shim.h"

#include <chrono>
#include <cstring>
#include <thread>

namespace {

WAVEFORMATEX Format22050Stereo16()
{
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 22050;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = 4;
    wfx.nAvgBytesPerSec = 22050 * 4;
    wfx.cbSize = 0;
    return wfx;
}

IDirectSound* NewDirectSound()
{
    platform::dsound::UseNullBackend(true);
    IDirectSound* ds = nullptr;
    REQUIRE(DirectSoundCreate(nullptr, &ds, nullptr) == DS_OK);
    REQUIRE(ds != nullptr);
    REQUIRE(ds->SetCooperativeLevel(nullptr, DSSCL_EXCLUSIVE) == DS_OK);
    return ds;
}

IDirectSoundBuffer* NewSecondary(IDirectSound* ds, WAVEFORMATEX* wfx, DWORD flags, DWORD bytes = 8820)
{
    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = flags;
    desc.dwBufferBytes = bytes;
    desc.lpwfxFormat = wfx;
    IDirectSoundBuffer* buf = nullptr;
    REQUIRE(ds->CreateSoundBuffer(&desc, &buf, nullptr) == DS_OK);
    REQUIRE(buf != nullptr);
    return buf;
}

const DWORD kControls = DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME;

template <typename Pred>
bool WaitFor(Pred pred)
{
    for (int i = 0; i < 400; ++i) {
        if (pred())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

} // namespace

TEST_CASE("dsound device, caps and primary buffer", "[dsound]")
{
    IDirectSound* ds = NewDirectSound();

    DSCAPS caps = {};
    caps.dwSize = sizeof(caps);
    REQUIRE(ds->GetCaps(&caps) == DS_OK);
    CHECK(caps.dwSize == sizeof(caps));
    CHECK((caps.dwFlags & DSCAPS_PRIMARY16BIT) != 0);
    CHECK((caps.dwFlags & DSCAPS_PRIMARYSTEREO) != 0);

    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRL3D;
    IDirectSoundBuffer* primary = nullptr;
    REQUIRE(ds->CreateSoundBuffer(&desc, &primary, nullptr) == DS_OK);
    REQUIRE(primary != nullptr);

    WAVEFORMATEX wfx = Format22050Stereo16();
    CHECK(primary->SetFormat(&wfx) == DS_OK);
    CHECK(primary->Play(0, 0, DSBPLAY_LOOPING) == DS_OK);
    DWORD status = 0;
    CHECK(primary->GetStatus(&status) == DS_OK);
    CHECK((status & DSBSTATUS_PLAYING) != 0);

    IDirectSound3DListener* listener = nullptr;
    REQUIRE(primary->QueryInterface(IID_IDirectSound3DListener, reinterpret_cast<LPVOID*>(&listener)) == DS_OK);
    REQUIRE(listener != nullptr);
    CHECK(listener->SetOrientation(0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0) == DS_OK);
    CHECK(listener->SetPosition(1.0f, 2.0f, 3.0f, DS3D_IMMEDIATE) == DS_OK);
    CHECK(listener->Release() == 0);

    CHECK(primary->Release() == 0);
    CHECK(ds->Release() == 0);
}

TEST_CASE("dsound Lock spans wrap around the ring", "[dsound]")
{
    IDirectSound* ds = NewDirectSound();
    WAVEFORMATEX wfx = Format22050Stereo16();
    IDirectSoundBuffer* buf = NewSecondary(ds, &wfx, kControls);

    DSBCAPS caps = {};
    caps.dwSize = sizeof(caps);
    REQUIRE(buf->GetCaps(&caps) == DS_OK);
    CHECK(caps.dwFlags == kControls);
    CHECK(caps.dwBufferBytes == 8820);

    LPVOID p1 = nullptr;
    LPVOID p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;
    REQUIRE(buf->Lock(0, 8820, &p1, &b1, &p2, &b2, 0) == DS_OK);
    CHECK(p1 != nullptr);
    CHECK(b1 == 8820);
    CHECK(p2 == nullptr);
    CHECK(b2 == 0);
    std::memset(p1, 0x11, b1);
    REQUIRE(buf->Unlock(p1, b1, p2, b2) == DS_OK);
    BYTE* base = static_cast<BYTE*>(p1);

    REQUIRE(buf->Lock(8000, 2000, &p1, &b1, &p2, &b2, 0) == DS_OK);
    CHECK(b1 == 820);
    CHECK(b2 == 1180);
    CHECK(p1 == base + 8000);
    CHECK(p2 == base);
    REQUIRE(buf->Unlock(p1, b1, p2, b2) == DS_OK);

    // A stopped buffer's write cursor equals its play cursor.
    REQUIRE(buf->SetCurrentPosition(4000) == DS_OK);
    DWORD play = 0;
    DWORD write = 0;
    REQUIRE(buf->GetCurrentPosition(&play, &write) == DS_OK);
    CHECK(play == 4000);
    CHECK(write == 4000);
    REQUIRE(buf->Lock(0, 100, &p1, &b1, &p2, &b2, DSBLOCK_FROMWRITECURSOR) == DS_OK);
    CHECK(p1 == base + 4000);
    CHECK(b1 == 100);
    REQUIRE(buf->Unlock(p1, b1, p2, b2) == DS_OK);

    CHECK(buf->Lock(8820, 4, &p1, &b1, &p2, &b2, 0) == DSERR_INVALIDPARAM);

    CHECK(buf->Release() == 0);
    CHECK(ds->Release() == 0);
}

TEST_CASE("dsound Play, GetStatus and Stop", "[dsound]")
{
    IDirectSound* ds = NewDirectSound();
    WAVEFORMATEX wfx = Format22050Stereo16();
    IDirectSoundBuffer* buf = NewSecondary(ds, &wfx, kControls | DSBCAPS_GETCURRENTPOSITION2);

    DWORD status = 0xFFFF;
    REQUIRE(buf->GetStatus(&status) == DS_OK);
    CHECK(status == 0);

    REQUIRE(buf->Play(0, 0, DSBPLAY_LOOPING) == DS_OK);
    REQUIRE(buf->GetStatus(&status) == DS_OK);
    CHECK((status & DSBSTATUS_PLAYING) != 0);
    CHECK((status & DSBSTATUS_LOOPING) != 0);

    // The play cursor advances with the frames the mixer consumes and the
    // write cursor leads it while playing.
    CHECK(WaitFor([&] {
        DWORD play = 0;
        DWORD write = 0;
        buf->GetCurrentPosition(&play, &write);
        return play != 0 && write != play;
    }));

    REQUIRE(buf->Stop() == DS_OK);
    REQUIRE(buf->GetStatus(&status) == DS_OK);
    CHECK((status & DSBSTATUS_PLAYING) == 0);

    // Without DSBPLAY_LOOPING the 0.1 s buffer ends by itself.
    REQUIRE(buf->SetCurrentPosition(0) == DS_OK);
    REQUIRE(buf->Play(0, 0, 0) == DS_OK);
    REQUIRE(buf->GetStatus(&status) == DS_OK);
    CHECK((status & DSBSTATUS_LOOPING) == 0);
    CHECK(WaitFor([&] {
        DWORD s = 0;
        buf->GetStatus(&s);
        return (s & DSBSTATUS_PLAYING) == 0;
    }));

    CHECK(buf->Release() == 0);
    CHECK(ds->Release() == 0);
}

TEST_CASE("dsound volume, pan and frequency", "[dsound]")
{
    IDirectSound* ds = NewDirectSound();
    WAVEFORMATEX wfx = Format22050Stereo16();
    IDirectSoundBuffer* buf = NewSecondary(ds, &wfx, kControls);

    platform::dsound::DebugState st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == Catch::Approx(1.0f));
    CHECK(st.right == Catch::Approx(1.0f));
    CHECK(st.pitch == Catch::Approx(1.0f));

    REQUIRE(buf->SetVolume(DSBVOLUME_MIN) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == 0.0f);
    CHECK(st.right == 0.0f);

    REQUIRE(buf->SetVolume(-600) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == Catch::Approx(0.501).margin(1e-3));
    CHECK(st.right == Catch::Approx(0.501).margin(1e-3));
    LONG volume = 0;
    REQUIRE(buf->GetVolume(&volume) == DS_OK);
    CHECK(volume == -600);

    REQUIRE(buf->SetVolume(DSBVOLUME_MAX) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == Catch::Approx(1.0f));
    CHECK(st.right == Catch::Approx(1.0f));

    REQUIRE(buf->SetPan(DSBPAN_LEFT) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == Catch::Approx(1.0f));
    CHECK(st.right == 0.0f);

    REQUIRE(buf->SetPan(0) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == Catch::Approx(1.0f));
    CHECK(st.right == Catch::Approx(1.0f));

    REQUIRE(buf->SetPan(2000) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == Catch::Approx(0.1).margin(1e-3));
    CHECK(st.right == Catch::Approx(1.0f));

    REQUIRE(buf->SetPan(DSBPAN_RIGHT) == DS_OK);
    st = platform::dsound::GetDebugState(buf);
    CHECK(st.left == 0.0f);
    CHECK(st.right == Catch::Approx(1.0f));

    REQUIRE(buf->SetFrequency(44100) == DS_OK);
    CHECK(platform::dsound::GetDebugState(buf).pitch == Catch::Approx(2.0f));
    REQUIRE(buf->SetFrequency(DSBFREQUENCY_ORIGINAL) == DS_OK);
    CHECK(platform::dsound::GetDebugState(buf).pitch == Catch::Approx(1.0f));

    CHECK(buf->Release() == 0);
    CHECK(ds->Release() == 0);
}

TEST_CASE("dsound 3D buffer interface and property sets", "[dsound]")
{
    IDirectSound* ds = NewDirectSound();
    WAVEFORMATEX wfx = Format22050Stereo16();
    IDirectSoundBuffer* buf3d = NewSecondary(ds, &wfx, DSBCAPS_CTRL3D | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME);

    LPDIRECTSOUND3DBUFFER b3d = nullptr;
    REQUIRE(buf3d->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<LPVOID*>(&b3d)) == DS_OK);
    REQUIRE(b3d != nullptr);
    CHECK(b3d->SetPosition(10.0f, 20.0f, 0.0f, DS3D_IMMEDIATE) == DS_OK);
    CHECK(b3d->SetMinDistance(128.0f, 0) == DS_OK);
    CHECK(b3d->SetMode(1, 0) == DS_OK);

    IKsPropertySet* props = reinterpret_cast<IKsPropertySet*>(0x1);
    CHECK(b3d->QueryInterface(IID_IKsPropertySet, reinterpret_cast<LPVOID*>(&props)) == E_NOINTERFACE);
    CHECK(props == nullptr);
    props = reinterpret_cast<IKsPropertySet*>(0x1);
    CHECK(buf3d->QueryInterface(IID_IKsPropertySet, reinterpret_cast<LPVOID*>(&props)) == E_NOINTERFACE);
    CHECK(props == nullptr);

    LPDIRECTSOUND3DBUFFER again = nullptr;
    REQUIRE(b3d->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<LPVOID*>(&again)) == DS_OK);
    CHECK(again == b3d);
    CHECK(again->Release() == 1);
    CHECK(b3d->Release() == 0);

    IDirectSoundBuffer* buf2d = NewSecondary(ds, &wfx, kControls);
    LPDIRECTSOUND3DBUFFER none = nullptr;
    CHECK(buf2d->QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<LPVOID*>(&none)) == E_NOINTERFACE);
    CHECK(none == nullptr);

    CHECK(buf2d->Release() == 0);
    CHECK(buf3d->Release() == 0);
    CHECK(ds->Release() == 0);
}

TEST_CASE("dsound DuplicateSoundBuffer shares data, not state", "[dsound]")
{
    IDirectSound* ds = NewDirectSound();
    WAVEFORMATEX wfx = Format22050Stereo16();
    IDirectSoundBuffer* buf = NewSecondary(ds, &wfx, kControls);

    LPVOID p1 = nullptr;
    LPVOID p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;
    REQUIRE(buf->Lock(0, 8820, &p1, &b1, &p2, &b2, 0) == DS_OK);
    std::memset(p1, 0x5A, b1);
    REQUIRE(buf->Unlock(p1, b1, p2, b2) == DS_OK);

    IDirectSoundBuffer* dup = nullptr;
    REQUIRE(ds->DuplicateSoundBuffer(buf, &dup) == DS_OK);
    REQUIRE(dup != nullptr);
    CHECK(dup != buf);

    REQUIRE(dup->SetVolume(-10000) == DS_OK);
    CHECK(platform::dsound::GetDebugState(dup).left == 0.0f);
    CHECK(platform::dsound::GetDebugState(buf).left == Catch::Approx(1.0f));

    REQUIRE(dup->Play(0, 0, DSBPLAY_LOOPING) == DS_OK);
    DWORD status = 0;
    REQUIRE(buf->GetStatus(&status) == DS_OK);
    CHECK((status & DSBSTATUS_PLAYING) == 0);

    // The duplicate outlives the original and still sees its data.
    CHECK(buf->Release() == 0);
    REQUIRE(dup->Lock(0, 8820, &p1, &b1, &p2, &b2, 0) == DS_OK);
    CHECK(b1 == 8820);
    CHECK(static_cast<BYTE*>(p1)[0] == 0x5A);
    CHECK(static_cast<BYTE*>(p1)[8819] == 0x5A);
    REQUIRE(dup->Unlock(p1, b1, p2, b2) == DS_OK);

    // Buffers keep the device alive, so release order does not matter.
    CHECK(ds->Release() == 1);
    CHECK(dup->Release() == 0);
}

TEST_CASE("dsound C-style macros", "[dsound]")
{
    platform::dsound::UseNullBackend(true);
    IDirectSound* ds = nullptr;
    REQUIRE(DirectSoundCreate(nullptr, &ds, nullptr) == DS_OK);
    REQUIRE(IDirectSound_SetCooperativeLevel(ds, nullptr, DSSCL_EXCLUSIVE) == DS_OK);
    DSCAPS caps = {};
    caps.dwSize = sizeof(caps);
    REQUIRE(IDirectSound_GetCaps(ds, &caps) == DS_OK);

    WAVEFORMATEX wfx = Format22050Stereo16();
    DSBUFFERDESC desc = {};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = 8820;
    desc.lpwfxFormat = &wfx;
    IDirectSoundBuffer* buf = nullptr;
    REQUIRE(IDirectSound_CreateSoundBuffer(ds, &desc, &buf, NULL) == DS_OK);

    LPVOID p1 = nullptr;
    LPVOID p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;
    REQUIRE(IDirectSoundBuffer_Lock(buf, 0, 8820, &p1, &b1, &p2, &b2, DSBLOCK_FROMWRITECURSOR) == DS_OK);
    REQUIRE(IDirectSoundBuffer_Unlock(buf, p1, b1, p2, b2) == DS_OK);
    REQUIRE(IDirectSoundBuffer_SetVolume(buf, -600) == DS_OK);
    LONG volume = 0;
    REQUIRE(IDirectSoundBuffer_GetVolume(buf, &volume) == DS_OK);
    CHECK(volume == -600);
    REQUIRE(IDirectSoundBuffer_SetPan(buf, 0) == DS_OK);
    REQUIRE(IDirectSoundBuffer_SetFrequency(buf, DSBFREQUENCY_ORIGINAL) == DS_OK);
    REQUIRE(IDirectSoundBuffer_SetCurrentPosition(buf, 0) == DS_OK);
    REQUIRE(IDirectSoundBuffer_Play(buf, 0, 0, DSBPLAY_LOOPING) == DS_OK);
    DWORD status = 0;
    REQUIRE(IDirectSoundBuffer_GetStatus(buf, &status) == DS_OK);
    CHECK((status & (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING)) == (DSBSTATUS_PLAYING | DSBSTATUS_LOOPING));
    DWORD play = 0;
    DWORD write = 0;
    REQUIRE(IDirectSoundBuffer_GetCurrentPosition(buf, &play, &write) == DS_OK);
    REQUIRE(IDirectSoundBuffer_Stop(buf) == DS_OK);
    REQUIRE(IDirectSoundBuffer_Restore(buf) == DS_OK);
    CHECK(IDirectSoundBuffer_SetFormat(buf, &wfx) == DSERR_INVALIDPARAM);
    CHECK(IDirectSoundBuffer_Release(buf) == 0);
    CHECK(IDirectSound_Release(ds) == 0);
}

TEST_CASE("dsound wave format headers keep their Windows sizes", "[dsound]")
{
    STATIC_REQUIRE(sizeof(WAVEFORMAT) == 14);
    STATIC_REQUIRE(sizeof(WAVEFORMATEX) == 18);
}
