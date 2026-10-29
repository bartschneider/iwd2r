#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CVidBlitter.h"
#include "platform/ddraw_shim.h"
#include "platform/present.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void UseDummyVideo() { SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"); }

IDirectDraw* NewDirectDraw(DWORD coopFlags)
{
    UseDummyVideo();
    IDirectDraw* dd = nullptr;
    REQUIRE(DirectDrawCreate(nullptr, &dd, nullptr) == DD_OK);
    REQUIRE(dd != nullptr);
    REQUIRE(dd->SetCooperativeLevel(nullptr, coopFlags) == DD_OK);
    return dd;
}

IDirectDrawSurface* NewOffscreen(IDirectDraw* dd, DWORD width, DWORD height)
{
    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    ddsd.dwWidth = width;
    ddsd.dwHeight = height;
    IDirectDrawSurface* surface = nullptr;
    REQUIRE(dd->CreateSurface(&ddsd, &surface, nullptr) == DD_OK);
    REQUIRE(surface != nullptr);
    return surface;
}

// Locks the whole surface and returns the descriptor; the caller unlocks.
DDSURFACEDESC LockAll(IDirectDrawSurface* surface)
{
    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    REQUIRE(surface->Lock(nullptr, &ddsd, DDLOCK_WAIT, nullptr) == DD_OK);
    return ddsd;
}

uint32_t PixelAt(const DDSURFACEDESC& ddsd, int x, int y)
{
    const uint8_t* row = static_cast<const uint8_t*>(ddsd.lpSurface) + y * ddsd.lPitch;
    uint32_t value;
    std::memcpy(&value, row + x * 4, sizeof(value));
    return value;
}

void SetPixel(const DDSURFACEDESC& ddsd, int x, int y, uint32_t value)
{
    uint8_t* row = static_cast<uint8_t*>(ddsd.lpSurface) + y * ddsd.lPitch;
    std::memcpy(row + x * 4, &value, sizeof(value));
}

// A 16-bit lane at the start of each 32-bit shim surface row: the software
// blitter copies 2-byte pixels, so a row of `nWidth` pixels is the row's first
// 2 * nWidth bytes.
void FillLanes16(IDirectDrawSurface* surface, int nWidth, WORD (*Pattern16)(int x, int y))
{
    DDSURFACEDESC ddsd = LockAll(surface);
    for (DWORD y = 0; y < ddsd.dwHeight; y++) {
        uint8_t* row = static_cast<uint8_t*>(ddsd.lpSurface) + y * ddsd.lPitch;
        for (int x = 0; x < nWidth; x++) {
            const WORD value = Pattern16(x, static_cast<int>(y));
            std::memcpy(row + x * 2, &value, sizeof(value));
        }
        std::memset(row + nWidth * 2, 0xEE, ddsd.dwWidth * 4 - nWidth * 2);
    }
    REQUIRE(surface->Unlock(nullptr) == DD_OK);
}

WORD Lane16(IDirectDrawSurface* surface, int x, int y)
{
    DDSURFACEDESC ddsd = LockAll(surface);
    const uint8_t* row = static_cast<const uint8_t*>(ddsd.lpSurface) + y * ddsd.lPitch;
    WORD value = 0;
    std::memcpy(&value, row + x * 2, sizeof(value));
    REQUIRE(surface->Unlock(nullptr) == DD_OK);
    return value;
}

void FillSurface(IDirectDrawSurface* surface, uint32_t value)
{
    DDSURFACEDESC ddsd = LockAll(surface);
    for (DWORD y = 0; y < ddsd.dwHeight; y++)
        for (DWORD x = 0; x < ddsd.dwWidth; x++)
            SetPixel(ddsd, static_cast<int>(x), static_cast<int>(y), value);
    REQUIRE(surface->Unlock(ddsd.lpSurface) == DD_OK);
}

// Source pattern: every pixel distinct, so a wrong offset shows up.
uint32_t Pattern(int x, int y) { return 0x010000u * static_cast<uint32_t>(y) + static_cast<uint32_t>(x) + 1u; }

void FillPattern(IDirectDrawSurface* surface)
{
    DDSURFACEDESC ddsd = LockAll(surface);
    for (DWORD y = 0; y < ddsd.dwHeight; y++)
        for (DWORD x = 0; x < ddsd.dwWidth; x++)
            SetPixel(ddsd, static_cast<int>(x), static_cast<int>(y), Pattern(static_cast<int>(x), static_cast<int>(y)));
    REQUIRE(surface->Unlock(nullptr) == DD_OK);
}

}

TEST_CASE("ddraw create and QueryInterface IDirectDraw2", "[ddraw]")
{
    UseDummyVideo();
    IDirectDraw* dd = nullptr;
    REQUIRE(DirectDrawCreate(nullptr, &dd, nullptr) == DD_OK);
    REQUIRE(dd != nullptr);

    IDirectDraw2* dd2 = nullptr;
    REQUIRE(dd->QueryInterface(IID_IDirectDraw2, reinterpret_cast<LPVOID*>(&dd2)) == DD_OK);
    CHECK(dd2 == dd);
    CHECK(dd2->AddRef() == 3);
    CHECK(dd2->Release() == 2);

    GUID other = IID_IDirectDraw2;
    other.Data1 ^= 1;
    void* none = &other;
    CHECK(dd->QueryInterface(other, &none) == E_NOINTERFACE);
    CHECK(none == nullptr);

    CHECK(dd2->Release() == 1);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw GetCaps reports software only", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    DDCAPS hw;
    DDCAPS hel;
    std::memset(&hw, 0xCD, sizeof(hw));
    std::memset(&hel, 0xCD, sizeof(hel));
    hw.dwSize = sizeof(hw);
    hel.dwSize = sizeof(hel);
    REQUIRE(dd->GetCaps(&hw, &hel) == DD_OK);
    for (const DDCAPS* caps : { &hw, &hel }) {
        CHECK(caps->dwSize == sizeof(DDCAPS));
        CHECK((caps->dwCaps & DDCAPS_NOHARDWARE) != 0);
        CHECK((caps->dwCaps & DDCAPS_BLT) == 0);
        CHECK(caps->dwCaps2 == 0);
        CHECK(caps->dwCKeyCaps == 0);
        CHECK(caps->dwFXCaps == 0);
        CHECK(caps->dwPalCaps == 0);
        CHECK(caps->dwVidMemTotal == 64u * 1024u * 1024u);
        CHECK(caps->dwVidMemFree == 64u * 1024u * 1024u);
    }
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw Lock reports pitch format and rectangle origin", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* surface = NewOffscreen(dd, 64, 32);

    DDSURFACEDESC ddsd = LockAll(surface);
    CHECK(ddsd.lPitch == 256);
    CHECK(ddsd.dwWidth == 64);
    CHECK(ddsd.dwHeight == 32);
    CHECK(ddsd.ddpfPixelFormat.dwRGBBitCount == 32);
    CHECK(ddsd.ddpfPixelFormat.dwRBitMask == 0xFF0000u);
    CHECK(ddsd.ddpfPixelFormat.dwGBitMask == 0x00FF00u);
    CHECK(ddsd.ddpfPixelFormat.dwBBitMask == 0x0000FFu);
    CHECK((ddsd.ddpfPixelFormat.dwFlags & DDPF_RGB) != 0);
    SetPixel(ddsd, 5, 3, 0xABCDEFu);
    REQUIRE(surface->Unlock(ddsd.lpSurface) == DD_OK);

    RECT r = { 5, 3, 6, 4 };
    DDSURFACEDESC sub = {};
    sub.dwSize = sizeof(sub);
    REQUIRE(surface->Lock(&r, &sub, DDLOCK_WAIT, nullptr) == DD_OK);
    CHECK(*static_cast<const uint32_t*>(sub.lpSurface) == 0xABCDEFu);
    CHECK(sub.lPitch == 256);
    CHECK(sub.dwWidth == 1);
    CHECK(sub.dwHeight == 1);
    REQUIRE(surface->Unlock(sub.lpSurface) == DD_OK);

    DDPIXELFORMAT pf = {};
    pf.dwSize = sizeof(pf);
    REQUIRE(surface->GetPixelFormat(&pf) == DD_OK);
    CHECK(pf.dwRGBBitCount == 32);
    CHECK(pf.dwRBitMask == 0xFF0000u);

    CHECK(surface->Release() == 0);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw BltFast honors source color key", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* dst = NewOffscreen(dd, 100, 100);
    IDirectDrawSurface* src = NewOffscreen(dd, 4, 2);
    FillSurface(dst, 0x111111u);

    DDSURFACEDESC s = LockAll(src);
    // Row 0: key, red, key, green. Row 1: blue, key, white, key.
    const uint32_t key = 0xFF00FFu;
    const uint32_t pixels[2][4] = { { key, 0xFF0000u, key, 0x00FF00u }, { 0x0000FFu, key, 0xFFFFFFu, key } };
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            SetPixel(s, x, y, pixels[y][x]);
    REQUIRE(src->Unlock(s.lpSurface) == DD_OK);

    DDCOLORKEY ck = { key, key };
    REQUIRE(src->SetColorKey(DDCKEY_SRCBLT, &ck) == DD_OK);
    DDCOLORKEY readBack = {};
    REQUIRE(src->GetColorKey(DDCKEY_SRCBLT, &readBack) == DD_OK);
    CHECK(readBack.dwColorSpaceLowValue == key);

    REQUIRE(dst->BltFast(10, 10, src, nullptr, DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT) == DD_OK);
    DDSURFACEDESC d = LockAll(dst);
    for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 4; x++) {
            const uint32_t expected = pixels[y][x] == key ? 0x111111u : pixels[y][x];
            CHECK(PixelAt(d, 10 + x, 10 + y) == expected);
        }
    }
    CHECK(PixelAt(d, 9, 10) == 0x111111u);
    CHECK(PixelAt(d, 14, 10) == 0x111111u);
    CHECK(PixelAt(d, 10, 12) == 0x111111u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Without the flag the key pixels are copied too.
    REQUIRE(dst->BltFast(20, 20, src, nullptr, DDBLTFAST_WAIT) == DD_OK);
    d = LockAll(dst);
    CHECK(PixelAt(d, 20, 20) == key);
    CHECK(PixelAt(d, 21, 20) == 0xFF0000u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    CHECK(src->Release() == 0);
    CHECK(dst->Release() == 0);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw BltFast clips destination and source", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* dst = NewOffscreen(dd, 100, 100);
    IDirectDrawSurface* src = NewOffscreen(dd, 64, 32);
    FillSurface(dst, 0u);
    FillPattern(src);

    // Destination edge: only a 10x10 corner fits.
    REQUIRE(dst->BltFast(90, 90, src, nullptr, DDBLTFAST_WAIT) == DD_OK);
    DDSURFACEDESC d = LockAll(dst);
    CHECK(PixelAt(d, 90, 90) == Pattern(0, 0));
    CHECK(PixelAt(d, 99, 99) == Pattern(9, 9));
    CHECK(PixelAt(d, 89, 90) == 0u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Source rectangle past the source's right edge: only columns 60..63 exist.
    FillSurface(dst, 0u);
    RECT srcRect = { 60, 0, 80, 32 };
    REQUIRE(dst->BltFast(0, 0, src, &srcRect, DDBLTFAST_WAIT) == DD_OK);
    d = LockAll(dst);
    for (int x = 0; x < 4; x++) {
        CHECK(PixelAt(d, x, 0) == Pattern(60 + x, 0));
        CHECK(PixelAt(d, x, 31) == Pattern(60 + x, 31));
    }
    CHECK(PixelAt(d, 4, 0) == 0u);
    CHECK(PixelAt(d, 0, 32) == 0u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Negative source origin: the part before column 0 is dropped and the
    // copy lands where column 0 would have landed.
    FillSurface(dst, 0u);
    RECT negRect = { -3, -2, 5, 4 };
    REQUIRE(dst->BltFast(10, 10, src, &negRect, DDBLTFAST_WAIT) == DD_OK);
    d = LockAll(dst);
    CHECK(PixelAt(d, 12, 11) == 0u);
    CHECK(PixelAt(d, 13, 12) == Pattern(0, 0));
    CHECK(PixelAt(d, 17, 15) == Pattern(4, 3));
    CHECK(PixelAt(d, 18, 15) == 0u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Entirely off the destination: nothing happens, nothing crashes.
    REQUIRE(dst->BltFast(500, 500, src, nullptr, DDBLTFAST_WAIT) == DD_OK);

    CHECK(src->Release() == 0);
    CHECK(dst->Release() == 0);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw Blt copies clips color fills and mirrors", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* dst = NewOffscreen(dd, 100, 100);
    IDirectDrawSurface* src = NewOffscreen(dd, 64, 32);
    FillSurface(dst, 0u);
    FillPattern(src);

    DDBLTFX fx = {};
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = 0x123456u;
    RECT fillRect = { 10, 20, 30, 25 };
    REQUIRE(dst->Blt(&fillRect, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx) == DD_OK);
    DDSURFACEDESC d = LockAll(dst);
    CHECK(PixelAt(d, 10, 20) == 0x123456u);
    CHECK(PixelAt(d, 29, 24) == 0x123456u);
    CHECK(PixelAt(d, 30, 24) == 0u);
    CHECK(PixelAt(d, 29, 25) == 0u);
    CHECK(PixelAt(d, 9, 20) == 0u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Whole-surface fill, and a fill rectangle hanging off every edge.
    fx.dwFillColor = 0xABCDEFu;
    REQUIRE(dst->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx) == DD_OK);
    RECT wide = { -50, -50, 150, 150 };
    fx.dwFillColor = 0x00FF00u;
    REQUIRE(dst->Blt(&wide, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx) == DD_OK);
    d = LockAll(dst);
    CHECK(PixelAt(d, 0, 0) == 0x00FF00u);
    CHECK(PixelAt(d, 99, 99) == 0x00FF00u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Mismatched sizes copy without scaling, clipped to the smaller.
    FillSurface(dst, 0u);
    RECT dstRect = { 50, 50, 150, 150 };
    RECT srcRect = { 2, 1, 6, 4 };
    REQUIRE(dst->Blt(&dstRect, src, &srcRect, DDBLT_WAIT, nullptr) == DD_OK);
    d = LockAll(dst);
    CHECK(PixelAt(d, 50, 50) == Pattern(2, 1));
    CHECK(PixelAt(d, 53, 52) == Pattern(5, 3));
    CHECK(PixelAt(d, 54, 52) == 0u);
    CHECK(PixelAt(d, 53, 53) == 0u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Color-keyed Blt.
    FillSurface(dst, 0u);
    DDCOLORKEY ck = { Pattern(1, 0), Pattern(1, 0) };
    REQUIRE(src->SetColorKey(DDCKEY_SRCBLT, &ck) == DD_OK);
    RECT keyDst = { 0, 0, 3, 1 };
    RECT keySrc = { 0, 0, 3, 1 };
    REQUIRE(dst->Blt(&keyDst, src, &keySrc, DDBLT_KEYSRC | DDBLT_WAIT, nullptr) == DD_OK);
    d = LockAll(dst);
    CHECK(PixelAt(d, 0, 0) == Pattern(0, 0));
    CHECK(PixelAt(d, 1, 0) == 0u);
    CHECK(PixelAt(d, 2, 0) == Pattern(2, 0));
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    // Mirrored Blt (CVidCell::BltFromFX), clipped at the left edge of the
    // destination: the dropped columns are the ones mirrored past it.
    FillSurface(dst, 0u);
    DDBLTFX mirror = {};
    mirror.dwSize = sizeof(mirror);
    mirror.dwDDFX = DDBLTFX_MIRRORLEFTRIGHT | DDBLTFX_MIRRORUPDOWN;
    RECT mDst = { -1, 0, 3, 2 };
    RECT mSrc = { 0, 0, 4, 2 };
    REQUIRE(dst->Blt(&mDst, src, &mSrc, DDBLT_DDFX | DDBLT_WAIT, &mirror) == DD_OK);
    d = LockAll(dst);
    CHECK(PixelAt(d, 0, 0) == Pattern(2, 1));
    CHECK(PixelAt(d, 2, 0) == Pattern(0, 1));
    CHECK(PixelAt(d, 0, 1) == Pattern(2, 0));
    CHECK(PixelAt(d, 3, 0) == 0u);
    REQUIRE(dst->Unlock(nullptr) == DD_OK);

    CHECK(src->Release() == 0);
    CHECK(dst->Release() == 0);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw fullscreen primary flips its back buffer", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);
    CHECK(dd->SetDisplayMode(640, 480, 16) == DDERR_INVALIDMODE);
    CHECK(dd->SetDisplayMode(640, 480, 24, 60, 0) == DDERR_INVALIDMODE);
    REQUIRE(dd->SetDisplayMode(640, 480, 32, 60, 0) == DD_OK);

    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    ddsd.dwBackBufferCount = 1;
    IDirectDrawSurface* front = nullptr;
    REQUIRE(dd->CreateSurface(&ddsd, &front, nullptr) == DD_OK);

    DDSURFACEDESC mode = {};
    mode.dwSize = sizeof(mode);
    REQUIRE(dd->GetDisplayMode(&mode) == DD_OK);
    CHECK(mode.dwWidth == 640);
    CHECK(mode.dwHeight == 480);
    CHECK(mode.ddpfPixelFormat.dwRGBBitCount == 32);

    DDSCAPS caps = { DDSCAPS_BACKBUFFER };
    IDirectDrawSurface* back = nullptr;
    REQUIRE(front->GetAttachedSurface(&caps, &back) == DD_OK);
    REQUIRE(back != nullptr);
    CHECK(back != front);

    DDSURFACEDESC f = LockAll(front);
    CHECK(f.dwWidth == 640);
    CHECK(f.dwHeight == 480);
    CHECK(f.lPitch == 640 * 4);
    CHECK((f.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0);
    CHECK(PixelAt(f, 7, 9) == 0u);
    REQUIRE(front->Unlock(nullptr) == DD_OK);

    const uint64_t hashBefore = platform::present::HashPrimary();
    const int framesBefore = platform::present::PresentedFrames();

    DDSURFACEDESC b = LockAll(back);
    CHECK((b.ddsCaps.dwCaps & DDSCAPS_BACKBUFFER) != 0);
    SetPixel(b, 7, 9, 0x00C0FFEEu);
    REQUIRE(back->Unlock(b.lpSurface) == DD_OK);

    REQUIRE(front->Flip(nullptr, DDFLIP_WAIT) == DD_OK);
    f = LockAll(front);
    CHECK(PixelAt(f, 7, 9) == 0x00C0FFEEu);
    REQUIRE(front->Unlock(nullptr) == DD_OK);
    b = LockAll(back);
    CHECK(PixelAt(b, 7, 9) == 0u);
    REQUIRE(back->Unlock(nullptr) == DD_OK);

    CHECK(platform::present::HashPrimary() != hashBefore);
    CHECK(platform::present::PresentedFrames() == framesBefore + 1);

    CHECK(front->GetDC(nullptr) == DDERR_UNSUPPORTED);
    CHECK(front->ReleaseDC(nullptr) == DDERR_UNSUPPORTED);
    CHECK(front->Restore() == DD_OK);
    CHECK(front->IsLost() == DD_OK);
    CHECK(back->Restore() == DD_OK);

    IDirectDrawPalette* palette = nullptr;
    PALETTEENTRY entries[256] = {};
    REQUIRE(dd->CreatePalette(0, entries, &palette, nullptr) == DD_OK);
    REQUIRE(palette != nullptr);
    REQUIRE(front->SetPalette(palette) == DD_OK);
    IDirectDrawPalette* got = nullptr;
    REQUIRE(front->GetPalette(&got) == DD_OK);
    CHECK(got == palette);
    CHECK(got->Release() == 2);

    REQUIRE(dd->RestoreDisplayMode() == DD_OK);

    // The engine releases the back buffer and the primary in either order.
    CHECK(back->Release() == 1);
    CHECK(front->Release() == 0);
    CHECK(palette->Release() == 0);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw windowed primary uses the screen size and marks dirty", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    const SIZE screen = platform::present::ScreenSize();

    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface* front = nullptr;
    REQUIRE(dd->CreateSurface(&ddsd, &front, nullptr) == DD_OK);

    DDSCAPS caps = { DDSCAPS_BACKBUFFER };
    IDirectDrawSurface* back = reinterpret_cast<IDirectDrawSurface*>(&caps);
    CHECK(front->GetAttachedSurface(&caps, &back) != DD_OK);
    CHECK(back == nullptr);

    DDSURFACEDESC f = LockAll(front);
    CHECK(f.dwWidth == static_cast<DWORD>(screen.cx));
    CHECK(f.dwHeight == static_cast<DWORD>(screen.cy));
    REQUIRE(front->Unlock(nullptr) == DD_OK);

    IDirectDrawClipper* clipper = nullptr;
    REQUIRE(dd->CreateClipper(0, &clipper, nullptr) == DD_OK);
    REQUIRE(clipper != nullptr);
    REQUIRE(clipper->SetHWnd(0, nullptr) == DD_OK);
    REQUIRE(front->SetClipper(clipper) == DD_OK);

    // Unlock on the primary marks it dirty; PresentIfDirty presents once.
    platform::present::PresentIfDirty();
    const int frames = platform::present::PresentedFrames();
    platform::present::PresentIfDirty();
    CHECK(platform::present::PresentedFrames() == frames);
    f = LockAll(front);
    SetPixel(f, 0, 0, 0x55u);
    REQUIRE(front->Unlock(f.lpSurface) == DD_OK);
    platform::present::PresentIfDirty();
    CHECK(platform::present::PresentedFrames() == frames + 1);

    // Blt onto the primary marks it dirty too.
    IDirectDrawSurface* src = NewOffscreen(dd, 8, 8);
    REQUIRE(front->Blt(nullptr, src, nullptr, DDBLT_WAIT, nullptr) == DD_OK);
    platform::present::PresentIfDirty();
    CHECK(platform::present::PresentedFrames() == frames + 2);

    REQUIRE(front->SetClipper(nullptr) == DD_OK);
    CHECK(clipper->Release() == 0);
    CHECK(src->Release() == 0);
    CHECK(front->Release() == 0);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw EnumDisplayModes offers 32 bpp", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    struct Seen {
        int count = 0;
        DWORD bpp = 0;
        DWORD width = 0;
        DWORD height = 0;
        DWORD refresh = 0;
    } seen;
    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwWidth = 800;
    ddsd.dwHeight = 600;
    auto callback = [](LPDDSURFACEDESC desc, LPVOID context) -> HRESULT {
        Seen* s = static_cast<Seen*>(context);
        s->count++;
        s->bpp = desc->ddpfPixelFormat.dwRGBBitCount;
        s->width = desc->dwWidth;
        s->height = desc->dwHeight;
        s->refresh = desc->dwRefreshRate;
        return DDENUMRET_OK;
    };
    REQUIRE(dd->EnumDisplayModes(0, &ddsd, &seen, callback) == DD_OK);
    CHECK(seen.count == 1);
    CHECK(seen.bpp == 32);
    CHECK(seen.width == 800);
    CHECK(seen.height == 600);
    CHECK(seen.refresh == 60);
    CHECK(dd->Release() == 0);
}

TEST_CASE("ddraw Present uploads to a real window", "[ddraw]")
{
    UseDummyVideo();
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("ddraw test", 64, 32, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        SDL_Quit();
        SKIP("dummy video driver could not create a window: " << SDL_GetError());
    }
    SDL_Renderer* probe = SDL_CreateRenderer(window, "software");
    if (probe == nullptr) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        SKIP("dummy video driver has no software renderer: " << SDL_GetError());
    }
    SDL_DestroyRenderer(probe);

    platform::present::SetPresentTarget(window);
    const int frames = platform::present::PresentedFrames();
    uint32_t pixels[32 * 64] = {};
    pixels[0] = 0xFF0000u;
    platform::present::Present(pixels, 64, 32, 64 * 4);
    platform::present::SetFullscreen(true);
    platform::present::Present(pixels, 32, 16, 64 * 4);
    platform::present::SetFullscreen(false);
    CHECK(platform::present::PresentedFrames() == frames + 2);
    platform::present::SetPresentTarget(nullptr);

    SDL_DestroyWindow(window);
    SDL_Quit();
}

TEST_CASE("ddraw display mode sets the screen size and RestoreDisplayMode returns it", "[ddraw]")
{
    const SIZE before = platform::present::ScreenSize();
    platform::present::SetScreenSize(800, 600);
    IDirectDraw* dd = NewDirectDraw(DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);
    CHECK(platform::present::ScreenSize().cx == 800);
    CHECK(platform::present::ScreenSize().cy == 600);
    REQUIRE(dd->SetDisplayMode(1024, 768, 32) == DD_OK);
    CHECK(platform::present::ScreenSize().cx == 1024);
    CHECK(platform::present::ScreenSize().cy == 768);
    REQUIRE(dd->RestoreDisplayMode() == DD_OK);
    CHECK(platform::present::ScreenSize().cx == 800);
    CHECK(platform::present::ScreenSize().cy == 600);

    // Windowed, the primary is the virtual screen.
    REQUIRE(dd->SetCooperativeLevel(nullptr, DDSCL_NORMAL) == DD_OK);
    DDSURFACEDESC ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface* front = nullptr;
    REQUIRE(dd->CreateSurface(&ddsd, &front, nullptr) == DD_OK);
    DDSURFACEDESC f = LockAll(front);
    CHECK(f.dwWidth == 800u);
    CHECK(f.dwHeight == 600u);
    REQUIRE(front->Unlock(nullptr) == DD_OK);
    CHECK(front->Release() == 0);
    CHECK(dd->Release() == 0);
    platform::present::SetScreenSize(before.cx, before.cy);
}

TEST_CASE("ddraw WindowToFrame follows the logical presentation", "[ddraw]")
{
    UseDummyVideo();
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    SDL_Window* window = SDL_CreateWindow("ddraw map", 1600, 1200, SDL_WINDOW_HIDDEN);
    if (window == nullptr) {
        SDL_Quit();
        SKIP("dummy video driver could not create a window: " << SDL_GetError());
    }
    platform::present::SetPresentTarget(window);
    float fx = -1.0f;
    float fy = -1.0f;
    // Before the first frame: the identity.
    platform::present::WindowToFrame(5.0f, 6.0f, &fx, &fy);
    CHECK(fx == 5.0f);
    CHECK(fy == 6.0f);
    int w = 0;
    int h = 0;
    CHECK_FALSE(platform::present::FrameSize(&w, &h));

    std::vector<uint32_t> pixels(800 * 600, 0u);
    platform::present::Present(pixels.data(), 800, 600, 800 * 4);
    SDL_Renderer* renderer = SDL_GetRenderer(window);
    if (renderer == nullptr) {
        platform::present::SetPresentTarget(nullptr);
        SDL_DestroyWindow(window);
        SDL_Quit();
        SKIP("dummy video driver has no renderer: " << SDL_GetError());
    }
    REQUIRE(platform::present::FrameSize(&w, &h));
    CHECK(w == 800);
    CHECK(h == 600);

    // A 1600x1200 window shows an 800x600 frame at twice the size.
    platform::present::WindowToFrame(800.0f, 600.0f, &fx, &fy);
    CHECK(fx == 400.0f);
    CHECK(fy == 300.0f);
    platform::present::WindowToFrame(0.0f, 0.0f, &fx, &fy);
    CHECK(fx == 0.0f);
    CHECK(fy == 0.0f);
    float wx = 0.0f;
    float wy = 0.0f;
    platform::present::FrameToWindow(400.0f, 300.0f, &wx, &wy);
    CHECK(wx == 800.0f);
    CHECK(wy == 600.0f);

    // Letterboxed into 1600x1000 (as fullscreen presents): scale 5/3, the
    // frame is about 1333 wide and starts about 133 in; the bar maps outside it.
    REQUIRE(SDL_SetWindowSize(window, 1600, 1000));
    SDL_SyncWindow(window);
    REQUIRE(SDL_SetRenderLogicalPresentation(renderer, 800, 600, SDL_LOGICAL_PRESENTATION_LETTERBOX));
    platform::present::WindowToFrame(800.0f, 500.0f, &fx, &fy);
    CHECK(std::fabs(fx - 400.0f) < 0.01f);
    CHECK(std::fabs(fy - 300.0f) < 0.01f);
    platform::present::WindowToFrame(50.0f, 500.0f, &fx, &fy);
    CHECK(fx < 0.0f);
    platform::present::FrameToWindow(0.0f, 0.0f, &wx, &wy);
    CHECK(std::fabs(wx - 400.0f / 3.0f) < 0.5f); // SDL rounds the bar to whole pixels.
    CHECK(std::fabs(wy) < 0.01f);

    platform::present::SetPresentTarget(nullptr);
    platform::present::WindowToFrame(5.0f, 6.0f, &fx, &fy);
    CHECK(fx == 5.0f);
    CHECK(fy == 6.0f);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// CVidBlitter::SoftBlt (0x7B7280): the 16-bpp software blit the classic
// renderer selects at 16 bpp. Its pixels are 2 bytes, so on a 32-bpp shim
// surface a row of N pixels is the row's first 2 * N bytes and the row step is
// the surface pitch; the rect starts at the surface origin because the shim's
// Lock offsets in its own 32-bit pixels. Ported for the record (iwd2r runs at
// 32 bpp only) and pinned here by hand - there is no oracle case, the blits
// need DD surfaces.
TEST_CASE("ddraw SoftBlt copies the source rows, plain and mirrored", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* dst = NewOffscreen(dd, 16, 8);
    IDirectDrawSurface* src = NewOffscreen(dd, 16, 8);

    const auto source = [](int x, int y) { return static_cast<WORD>(0x1000 + y * 0x10 + x); };
    const auto marker = [](int, int) { return static_cast<WORD>(0xEEEE); };
    FillLanes16(src, 4, source);

    CVidBlitter blitter;
    RECT rSrc = { 0, 0, 4, 3 };
    RECT rDst = { 0, 0, 4, 3 };
    DDBLTFX fx = {};
    fx.dwSize = sizeof(fx);

    // Plain: the 4x3 block, 16-bit pixels, nothing after the block moves.
    FillLanes16(dst, 16, marker);
    REQUIRE(blitter.SoftBlt(dst, &rDst, src, &rSrc, 0, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("plain " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == source(x, y));
        }
        CHECK(Lane16(dst, 4, y) == 0xEEEE);
    }
    CHECK(Lane16(dst, 0, 3) == 0xEEEE);

    // Mirrored left to right: the columns reverse inside the same rect.
    FillLanes16(dst, 16, marker);
    fx.dwDDFX = DDBLTFX_MIRRORLEFTRIGHT;
    REQUIRE(blitter.SoftBlt(dst, &rDst, src, &rSrc, DDBLT_DDFX, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("mirror lr " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == source(3 - x, y));
        }
    }

    // Mirrored up and down: the rows reverse.
    FillLanes16(dst, 16, marker);
    fx.dwDDFX = DDBLTFX_MIRRORUPDOWN;
    REQUIRE(blitter.SoftBlt(dst, &rDst, src, &rSrc, DDBLT_DDFX, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("mirror ud " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == source(x, 2 - y));
        }
    }

    // Both at once.
    FillLanes16(dst, 16, marker);
    fx.dwDDFX = DDBLTFX_MIRRORLEFTRIGHT | DDBLTFX_MIRRORUPDOWN;
    REQUIRE(blitter.SoftBlt(dst, &rDst, src, &rSrc, DDBLT_DDFX, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("mirror both " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == source(3 - x, 2 - y));
        }
    }

    // A destination rect outside the surface is the lock's refusal, passed on.
    RECT rBad = { 0, 6, 4, 9 };
    CHECK(blitter.SoftBlt(dst, &rBad, src, &rSrc, 0, &fx) == DDERR_INVALIDPARAMS);

    CHECK(dst->Release() == 0);
    CHECK(src->Release() == 0);
    CHECK(dd->Release() == 0);
}

// CVidBlitter::SoftSrcKeyBlt (0x7B74F0): the same blit with the source's
// DDCKEY_SRCBLT key, a key pixel leaving the destination's pixel as it was.
TEST_CASE("ddraw SoftSrcKeyBlt leaves the key pixels alone", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* dst = NewOffscreen(dd, 16, 8);
    IDirectDrawSurface* src = NewOffscreen(dd, 16, 8);

    const auto source = [](int x, int y) { return static_cast<WORD>(0x2000 + y * 0x10 + x); };
    const auto marker = [](int, int) { return static_cast<WORD>(0xEEEE); };
    FillLanes16(src, 4, source);
    DDCOLORKEY key = { source(1, 0), source(1, 0) };
    REQUIRE(src->SetColorKey(DDCKEY_SRCBLT, &key) == DD_OK);

    CVidBlitter blitter;
    RECT rSrc = { 0, 0, 4, 3 };
    RECT rDst = { 0, 0, 4, 3 };
    DDBLTFX fx = {};
    fx.dwSize = sizeof(fx);

    FillLanes16(dst, 16, marker);
    REQUIRE(blitter.SoftSrcKeyBlt(dst, &rDst, src, &rSrc, 0, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("key " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == (source(x, y) == key.dwColorSpaceLowValue ? 0xEEEE : source(x, y)));
        }
    }
    CHECK(Lane16(dst, 1, 0) == 0xEEEE);
    CHECK(Lane16(dst, 4, 0) == 0xEEEE);

    // Mirrored left to right the key still skips: source(1, 0) lands at x 2.
    FillLanes16(dst, 16, marker);
    fx.dwDDFX = DDBLTFX_MIRRORLEFTRIGHT;
    REQUIRE(blitter.SoftSrcKeyBlt(dst, &rDst, src, &rSrc, DDBLT_DDFX, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("key lr " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == (source(3 - x, y) == key.dwColorSpaceLowValue ? 0xEEEE : source(3 - x, y)));
        }
    }
    CHECK(Lane16(dst, 2, 0) == 0xEEEE);

    CHECK(dst->Release() == 0);
    CHECK(src->Release() == 0);
    CHECK(dd->Release() == 0);
}

// CVidBlitter::SoftColorFill (0x7B7070), reached through SoftBlt's
// DDBLT_COLORFILL: the destination rect filled with the DDBLTFX's fill colour.
TEST_CASE("ddraw SoftBlt fills a rect for DDBLT_COLORFILL", "[ddraw]")
{
    IDirectDraw* dd = NewDirectDraw(DDSCL_NORMAL);
    IDirectDrawSurface* dst = NewOffscreen(dd, 16, 8);

    const auto marker = [](int, int) { return static_cast<WORD>(0xEEEE); };
    FillLanes16(dst, 16, marker);

    CVidBlitter blitter;
    RECT rDst = { 0, 0, 4, 3 };
    RECT rSrc = { 0, 0, 0, 0 };
    DDBLTFX fx = {};
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = 0x1234;

    REQUIRE(blitter.SoftBlt(dst, &rDst, nullptr, &rSrc, DDBLT_COLORFILL, &fx) == DD_OK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            INFO("fill " << x << ", " << y);
            CHECK(Lane16(dst, x, y) == 0x1234);
        }
        CHECK(Lane16(dst, 4, y) == 0xEEEE);
    }
    CHECK(Lane16(dst, 0, 3) == 0xEEEE);

    CHECK(dst->Release() == 0);
    CHECK(dd->Release() == 0);
}
