// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/ddraw_shim.h"

#include "platform/present.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

DEFINE_GUID(IID_IDirectDraw2, 0xB3A6F3E0, 0x2B43, 0x11CF, 0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);

namespace {

const DWORD kVideoMemory = 64u * 1024u * 1024u;
const DWORD kRefreshRate = 60;
const DWORD kMaxDimension = 16384;

void FillPixelFormat(DDPIXELFORMAT& pf)
{
    std::memset(&pf, 0, sizeof(pf));
    pf.dwSize = sizeof(pf);
    pf.dwFlags = DDPF_RGB;
    pf.dwRGBBitCount = 32;
    pf.dwRBitMask = 0xFF0000u;
    pf.dwGBitMask = 0x00FF00u;
    pf.dwBBitMask = 0x0000FFu;
}

class Clipper final : public IDirectDrawClipper {
public:
    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }
    HRESULT SetHWnd(DWORD, HWND hWnd) override
    {
        m_hWnd = hWnd;
        return DD_OK;
    }

private:
    ULONG m_refs = 1;
    HWND m_hWnd = nullptr;
};

class Palette final : public IDirectDrawPalette {
public:
    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }

private:
    ULONG m_refs = 1;
};

class Surface final : public IDirectDrawSurface {
public:
    Surface(int width, int height, DWORD caps)
        : m_width(width)
        , m_height(height)
        , m_caps(caps)
        , m_pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u)
    {
        if (IsPrimary())
            platform::present::SetPrimary(&m_pixels, m_width, m_height);
    }

    // Takes over the creation reference of the back buffer.
    void AttachBackBuffer(Surface* back) { m_back = back; }

    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }

    HRESULT Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD, HANDLE) override
    {
        if (!lpDDSurfaceDesc)
            return DDERR_INVALIDPARAMS;
        RECT r = { 0, 0, m_width, m_height };
        if (lpDestRect) {
            r = *lpDestRect;
            if (r.left < 0 || r.top < 0 || r.left >= r.right || r.top >= r.bottom || r.right > m_width || r.bottom > m_height)
                return DDERR_INVALIDPARAMS;
        }
        DDSURFACEDESC& d = *lpDDSurfaceDesc;
        std::memset(&d, 0, sizeof(d));
        d.dwSize = sizeof(d);
        d.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_LPSURFACE;
        d.dwWidth = static_cast<DWORD>(r.right - r.left);
        d.dwHeight = static_cast<DWORD>(r.bottom - r.top);
        d.lPitch = Pitch();
        d.lpSurface = Row(r.top) + r.left;
        FillPixelFormat(d.ddpfPixelFormat);
        d.ddsCaps.dwCaps = m_caps;
        return DD_OK;
    }

    HRESULT Unlock(LPVOID) override
    {
        MarkDirtyIfPrimary();
        return DD_OK;
    }

    HRESULT Blt(LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) override
    {
        const RECT dst = lpDestRect ? *lpDestRect : Bounds();
        if (dwFlags & DDBLT_COLORFILL) {
            if (!lpDDBltFx)
                return DDERR_INVALIDPARAMS;
            Fill(dst, lpDDBltFx->dwFillColor);
            MarkDirtyIfPrimary();
            return DD_OK;
        }
        if (!lpDDSrcSurface)
            return DDERR_INVALIDPARAMS;
        Surface* src = static_cast<Surface*>(lpDDSrcSurface);
        const RECT srcRect = lpSrcRect ? *lpSrcRect : src->Bounds();
        const DWORD ddfx = (dwFlags & DDBLT_DDFX) && lpDDBltFx ? lpDDBltFx->dwDDFX : 0;
        Copy(dst.left, dst.top, dst.right - dst.left, dst.bottom - dst.top, *src, srcRect, (dwFlags & DDBLT_KEYSRC) != 0,
            (ddfx & DDBLTFX_MIRRORLEFTRIGHT) != 0, (ddfx & DDBLTFX_MIRRORUPDOWN) != 0);
        MarkDirtyIfPrimary();
        return DD_OK;
    }

    HRESULT BltFast(DWORD dwX, DWORD dwY, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) override
    {
        if (!lpDDSrcSurface)
            return DDERR_INVALIDPARAMS;
        Surface* src = static_cast<Surface*>(lpDDSrcSurface);
        const RECT srcRect = lpSrcRect ? *lpSrcRect : src->Bounds();
        // The engine computes origins as signed ints before passing them as DWORD.
        const long long x = static_cast<int32_t>(dwX);
        const long long y = static_cast<int32_t>(dwY);
        Copy(x, y, static_cast<long long>(srcRect.right) - srcRect.left, static_cast<long long>(srcRect.bottom) - srcRect.top, *src,
            srcRect, (dwTrans & DDBLTFAST_SRCCOLORKEY) != 0, false, false);
        MarkDirtyIfPrimary();
        return DD_OK;
    }

    HRESULT Flip(IDirectDrawSurface*, DWORD) override
    {
        if (!m_back)
            return DDERR_UNSUPPORTED;
        std::swap(m_pixels, m_back->m_pixels);
        platform::present::Present(m_pixels.data(), m_width, m_height, Pitch());
        return DD_OK;
    }

    HRESULT GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface) override
    {
        if (!lpDDSCaps || !lplpDDAttachedSurface)
            return DDERR_INVALIDPARAMS;
        *lplpDDAttachedSurface = nullptr;
        if (!m_back || (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) == 0)
            return DDERR_NOTFOUND;
        m_back->AddRef();
        *lplpDDAttachedSurface = m_back;
        return DD_OK;
    }

    HRESULT SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override
    {
        if ((dwFlags & DDCKEY_SRCBLT) == 0)
            return DDERR_UNSUPPORTED;
        m_hasKey = lpDDColorKey != nullptr;
        m_key = lpDDColorKey ? *lpDDColorKey : DDCOLORKEY {};
        return DD_OK;
    }

    HRESULT GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override
    {
        if ((dwFlags & DDCKEY_SRCBLT) == 0)
            return DDERR_UNSUPPORTED;
        if (!lpDDColorKey)
            return DDERR_INVALIDPARAMS;
        *lpDDColorKey = m_key;
        return DD_OK;
    }

    HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override
    {
        if (!lpDDPixelFormat)
            return DDERR_INVALIDPARAMS;
        FillPixelFormat(*lpDDPixelFormat);
        return DD_OK;
    }

    HRESULT SetClipper(IDirectDrawClipper* lpDDClipper) override
    {
        if (lpDDClipper)
            lpDDClipper->AddRef();
        if (m_clipper)
            m_clipper->Release();
        m_clipper = lpDDClipper;
        return DD_OK;
    }

    HRESULT SetPalette(IDirectDrawPalette* lpDDPalette) override
    {
        if (lpDDPalette)
            lpDDPalette->AddRef();
        if (m_palette)
            m_palette->Release();
        m_palette = lpDDPalette;
        return DD_OK;
    }

    HRESULT GetPalette(IDirectDrawPalette** lplpDDPalette) override
    {
        if (!lplpDDPalette)
            return DDERR_INVALIDPARAMS;
        *lplpDDPalette = m_palette;
        if (!m_palette)
            return DDERR_NOTFOUND;
        m_palette->AddRef();
        return DD_OK;
    }

    HRESULT GetDC(HDC* lphDC) override
    {
        if (lphDC)
            *lphDC = nullptr;
        return DDERR_UNSUPPORTED;
    }

    HRESULT ReleaseDC(HDC) override { return DDERR_UNSUPPORTED; }
    HRESULT Restore() override { return DD_OK; }
    HRESULT IsLost() override { return DD_OK; }

private:
    ~Surface() override
    {
        if (platform::present::Primary() == &m_pixels)
            platform::present::SetPrimary(nullptr, 0, 0);
        if (m_back)
            m_back->Release();
        if (m_clipper)
            m_clipper->Release();
        if (m_palette)
            m_palette->Release();
    }

    bool IsPrimary() const { return (m_caps & DDSCAPS_PRIMARYSURFACE) != 0; }
    RECT Bounds() const { return RECT { 0, 0, m_width, m_height }; }
    LONG Pitch() const { return m_width * 4; }
    uint32_t* Row(long long y) { return m_pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(m_width); }

    void MarkDirtyIfPrimary()
    {
        if (IsPrimary())
            platform::present::MarkPrimaryDirty();
    }

    void Fill(const RECT& r, uint32_t color)
    {
        const long long left = std::max<long long>(r.left, 0);
        const long long top = std::max<long long>(r.top, 0);
        const long long right = std::min<long long>(r.right, m_width);
        const long long bottom = std::min<long long>(r.bottom, m_height);
        if (left >= right || top >= bottom)
            return;
        for (long long y = top; y < bottom; y++)
            std::fill(Row(y) + left, Row(y) + right, color);
    }

    // Copies a w x h block of src starting at srcRect's origin to (dx, dy),
    // where w and h are already the smaller of the two rectangles (no
    // stretching). Destination pixel (dx + u, dy + v) takes source pixel
    // (sx(u), sy(v)); u and v are clipped so both stay inside their surfaces.
    void Copy(long long dx, long long dy, long long w, long long h, Surface& src, const RECT& srcRect, bool useKey, bool mirrorX, bool mirrorY)
    {
        w = std::min(w, static_cast<long long>(srcRect.right) - srcRect.left);
        h = std::min(h, static_cast<long long>(srcRect.bottom) - srcRect.top);
        if (w <= 0 || h <= 0)
            return;
        const long long sl = srcRect.left;
        const long long st = srcRect.top;

        // Unmirrored: sx = sl + u. Mirrored: sx = sl + w - 1 - u.
        long long uLo = std::max(0LL, -dx);
        long long uHi = std::min(w, m_width - dx);
        long long vLo = std::max(0LL, -dy);
        long long vHi = std::min(h, m_height - dy);
        if (mirrorX) {
            uLo = std::max(uLo, sl + w - src.m_width);
            uHi = std::min(uHi, sl + w);
        } else {
            uLo = std::max(uLo, -sl);
            uHi = std::min(uHi, src.m_width - sl);
        }
        if (mirrorY) {
            vLo = std::max(vLo, st + h - src.m_height);
            vHi = std::min(vHi, st + h);
        } else {
            vLo = std::max(vLo, -st);
            vHi = std::min(vHi, src.m_height - st);
        }
        if (uLo >= uHi || vLo >= vHi)
            return;

        // A blit within one surface reads from a snapshot so overlap is safe.
        std::vector<uint32_t> snapshot;
        const uint32_t* srcPixels = src.m_pixels.data();
        if (&src == this) {
            snapshot = m_pixels;
            srcPixels = snapshot.data();
        }
        const uint32_t key = src.m_key.dwColorSpaceLowValue;
        useKey = useKey && src.m_hasKey;

        for (long long v = vLo; v < vHi; v++) {
            const long long sy = mirrorY ? st + h - 1 - v : st + v;
            const uint32_t* srcRow = srcPixels + static_cast<size_t>(sy) * static_cast<size_t>(src.m_width);
            uint32_t* dstRow = Row(dy + v);
            if (!mirrorX && !useKey) {
                std::memcpy(dstRow + (dx + uLo), srcRow + (sl + uLo), static_cast<size_t>(uHi - uLo) * sizeof(uint32_t));
                continue;
            }
            for (long long u = uLo; u < uHi; u++) {
                const uint32_t pixel = srcRow[mirrorX ? sl + w - 1 - u : sl + u];
                if (!useKey || pixel != key)
                    dstRow[dx + u] = pixel;
            }
        }
    }

    ULONG m_refs = 1;
    int m_width;
    int m_height;
    DWORD m_caps;
    std::vector<uint32_t> m_pixels;
    bool m_hasKey = false;
    DDCOLORKEY m_key = {};
    Surface* m_back = nullptr;
    IDirectDrawClipper* m_clipper = nullptr;
    IDirectDrawPalette* m_palette = nullptr;
};

class DirectDraw final : public IDirectDraw {
public:
    HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) override
    {
        if (!ppvObj)
            return DDERR_INVALIDPARAMS;
        if (!IsEqualGUID(riid, IID_IDirectDraw2)) {
            *ppvObj = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        *ppvObj = this;
        return DD_OK;
    }

    ULONG AddRef() override { return ++m_refs; }
    ULONG Release() override
    {
        ULONG refs = --m_refs;
        if (refs == 0)
            delete this;
        return refs;
    }

    HRESULT SetCooperativeLevel(HWND, DWORD dwFlags) override
    {
        if (dwFlags & DDSCL_FULLSCREEN) {
            m_fullscreen = true;
            platform::present::SetFullscreen(true);
            return DD_OK;
        }
        if (dwFlags & DDSCL_NORMAL) {
            m_fullscreen = false;
            platform::present::SetFullscreen(false);
            return DD_OK;
        }
        return DDERR_INVALIDPARAMS;
    }

    HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) override { return SetDisplayMode(dwWidth, dwHeight, dwBPP, 0, 0); }

    HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD, DWORD) override
    {
        if (dwBPP != 32)
            return DDERR_INVALIDMODE;
        if (dwWidth == 0 || dwHeight == 0 || dwWidth > kMaxDimension || dwHeight > kMaxDimension)
            return DDERR_INVALIDPARAMS;
        m_modeWidth = dwWidth;
        m_modeHeight = dwHeight;
        platform::present::SetDisplayMode(static_cast<int>(dwWidth), static_cast<int>(dwHeight));
        return DD_OK;
    }

    HRESULT RestoreDisplayMode() override
    {
        m_modeWidth = 0;
        m_modeHeight = 0;
        platform::present::SetDisplayMode(0, 0);
        return DD_OK;
    }

    HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override
    {
        if (!lpDDSurfaceDesc)
            return DDERR_INVALIDPARAMS;
        DWORD width;
        DWORD height;
        ModeSize(width, height);
        FillModeDesc(*lpDDSurfaceDesc, width, height);
        return DD_OK;
    }

    // Offers one 32-bpp mode: the size the caller asks about, since any size
    // is presented scaled in a borderless window, or the screen size.
    HRESULT EnumDisplayModes(DWORD, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) override
    {
        if (!lpEnumModesCallback)
            return DDERR_INVALIDPARAMS;
        const SIZE screen = platform::present::ScreenSize();
        DWORD width = static_cast<DWORD>(screen.cx);
        DWORD height = static_cast<DWORD>(screen.cy);
        if (lpDDSurfaceDesc && lpDDSurfaceDesc->dwWidth != 0 && lpDDSurfaceDesc->dwHeight != 0) {
            width = lpDDSurfaceDesc->dwWidth;
            height = lpDDSurfaceDesc->dwHeight;
        }
        DDSURFACEDESC mode;
        FillModeDesc(mode, width, height);
        lpEnumModesCallback(&mode, lpContext);
        return DD_OK;
    }

    HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) override
    {
        if (!lpDDDriverCaps && !lpDDHELCaps)
            return DDERR_INVALIDPARAMS;
        for (DDCAPS* caps : { lpDDDriverCaps, lpDDHELCaps }) {
            if (!caps)
                continue;
            std::memset(caps, 0, sizeof(*caps));
            caps->dwSize = sizeof(*caps);
            caps->dwCaps = DDCAPS_NOHARDWARE;
            caps->dwVidMemTotal = kVideoMemory;
            caps->dwVidMemFree = kVideoMemory;
        }
        return DD_OK;
    }

    HRESULT CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, IDirectDrawSurface** lplpDDSurface, LPVOID) override
    {
        if (!lpDDSurfaceDesc || !lplpDDSurface)
            return DDERR_INVALIDPARAMS;
        *lplpDDSurface = nullptr;
        const DDSURFACEDESC& d = *lpDDSurfaceDesc;
        const DWORD caps = (d.dwFlags & DDSD_CAPS) ? d.ddsCaps.dwCaps : 0;

        if (caps & DDSCAPS_PRIMARYSURFACE) {
            DWORD width;
            DWORD height;
            ModeSize(width, height);
            Surface* primary = new Surface(static_cast<int>(width), static_cast<int>(height), caps);
            const bool flips = (caps & DDSCAPS_FLIP) && (d.dwFlags & DDSD_BACKBUFFERCOUNT) && d.dwBackBufferCount >= 1;
            if (flips)
                primary->AttachBackBuffer(new Surface(static_cast<int>(width), static_cast<int>(height),
                    DDSCAPS_BACKBUFFER | DDSCAPS_FLIP | DDSCAPS_COMPLEX));
            *lplpDDSurface = primary;
            return DD_OK;
        }

        if ((d.dwFlags & (DDSD_WIDTH | DDSD_HEIGHT)) != (DDSD_WIDTH | DDSD_HEIGHT) || d.dwWidth == 0 || d.dwHeight == 0
            || d.dwWidth > kMaxDimension || d.dwHeight > kMaxDimension)
            return DDERR_INVALIDPARAMS;
        *lplpDDSurface = new Surface(static_cast<int>(d.dwWidth), static_cast<int>(d.dwHeight), caps);
        return DD_OK;
    }

    HRESULT CreateClipper(DWORD, IDirectDrawClipper** lplpDDClipper, LPVOID) override
    {
        if (!lplpDDClipper)
            return DDERR_INVALIDPARAMS;
        *lplpDDClipper = new Clipper();
        return DD_OK;
    }

    HRESULT CreatePalette(DWORD, LPPALETTEENTRY, IDirectDrawPalette** lplpDDPalette, LPVOID) override
    {
        if (!lplpDDPalette)
            return DDERR_INVALIDPARAMS;
        *lplpDDPalette = new Palette();
        return DD_OK;
    }

private:
    ~DirectDraw() override = default;

    // The SetDisplayMode size in fullscreen mode, otherwise the virtual screen
    // (the game resolution; see present::ScreenSize).
    void ModeSize(DWORD& width, DWORD& height) const
    {
        if (m_fullscreen && m_modeWidth != 0) {
            width = m_modeWidth;
            height = m_modeHeight;
            return;
        }
        const SIZE screen = platform::present::ScreenSize();
        width = static_cast<DWORD>(screen.cx);
        height = static_cast<DWORD>(screen.cy);
    }

    static void FillModeDesc(DDSURFACEDESC& d, DWORD width, DWORD height)
    {
        std::memset(&d, 0, sizeof(d));
        d.dwSize = sizeof(d);
        d.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE;
        d.dwWidth = width;
        d.dwHeight = height;
        d.lPitch = static_cast<LONG>(width * 4);
        d.dwRefreshRate = kRefreshRate;
        FillPixelFormat(d.ddpfPixelFormat);
    }

    ULONG m_refs = 1;
    bool m_fullscreen = false;
    DWORD m_modeWidth = 0;
    DWORD m_modeHeight = 0;
};

}

HRESULT DirectDrawCreate(GUID*, IDirectDraw** lplpDD, LPVOID)
{
    if (!lplpDD)
        return DDERR_INVALIDPARAMS;
    *lplpDD = new DirectDraw();
    return DD_OK;
}
