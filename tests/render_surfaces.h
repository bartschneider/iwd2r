// Test helpers: DirectDraw shim surfaces put into the current video mode
// for the renders the engine draws in a test. RenderScope: a 640 x 480 back
// buffer and the two FX surfaces with a standalone CInfinity; AreaScope: a
// back buffer as large as an area's viewport, the FX surfaces, and on its
// first area render the VRAM tiles and the pointer's surfaces the tile
// renderer needs. Both put the mode back when they go. Header-only.
#pragma once

#include "CBaldurChitin.h"
#include "CCacheStatus.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "platform/png.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace render_surfaces {

inline constexpr int kWidth = 640;
inline constexpr int kHeight = 480;
// The FX surfaces' colour key and the transparent fill (field_24): pure
// green of the 32-bit format.
inline constexpr DWORD kKey = 0x0000FF00;

// The current video mode with shim surfaces: the back buffer, and the FX
// surfaces 2 and 3 keyed as CVidInf::CreateSurfaces keys them, at 32 bpp,
// faded in. A standalone CInfinity shows the area's top left corner over
// the whole back buffer, so a world point is a screen point; it has no WED,
// so FXRenderClippingPolys draws nothing.
class RenderScope {
public:
    RenderScope()
        : m_pVidMode(g_pChitin->GetCurrentVideoMode())
        , m_nSurfacesBefore(m_pVidMode->m_nSurfaces)
        , m_pSurfacesBefore(m_pVidMode->pSurfaces)
        , m_nBppBefore(g_pChitin->cVideo.m_nBpp)
        , m_shifts { m_pVidMode->m_dwRBitShift, m_pVidMode->m_dwGBitShift, m_pVidMode->m_dwBBitShift }
        , m_field24Before(m_pVidMode->field_24)
        , m_nFadeBefore(m_pVidMode->m_nFade)
    {
        for (IDirectDrawSurface*& pSurface : m_surfaces) {
            pSurface = NULL;
        }
        if (DirectDrawCreate(NULL, &m_pDirectDraw, NULL) != DD_OK) {
            m_pDirectDraw = NULL;
            return;
        }
        DDCOLORKEY key = { kKey, kKey };
        m_surfaces[CVIDINF_SURFACE_BACK] = Create(kWidth, kHeight);
        m_surfaces[CVIDINF_SURFACE_2] = Create(CVIDINF_FX_WIDTH, CVIDINF_FX_HEIGHT);
        m_surfaces[CVIDINF_SURFACE_3] = Create(CVIDINF_FX_WIDTH, CVIDINF_FX_HEIGHT);
        if (m_surfaces[CVIDINF_SURFACE_2] != NULL) {
            m_surfaces[CVIDINF_SURFACE_2]->SetColorKey(DDCKEY_SRCBLT, &key);
        }
        if (m_surfaces[CVIDINF_SURFACE_3] != NULL) {
            m_surfaces[CVIDINF_SURFACE_3]->SetColorKey(DDCKEY_SRCBLT, &key);
        }
        m_pVidMode->m_nSurfaces = 9;
        m_pVidMode->pSurfaces = m_surfaces;
        g_pChitin->cVideo.m_nBpp = 32;
        m_pVidMode->m_dwRBitShift = 16;
        m_pVidMode->m_dwGBitShift = 8;
        m_pVidMode->m_dwBBitShift = 0;
        m_pVidMode->field_24 = kKey;
        m_pVidMode->m_nFade = CVidMode::NUM_FADE_FRAMES;

        m_infinity.pVidMode = m_pVidMode;
        m_infinity.rViewPort = CRect(0, 0, kWidth, kHeight);
        m_infinity.nCurrentX = 0;
        m_infinity.nCurrentY = 0;
    }
    ~RenderScope()
    {
        m_pVidMode->m_nSurfaces = m_nSurfacesBefore;
        m_pVidMode->pSurfaces = m_pSurfacesBefore;
        g_pChitin->cVideo.m_nBpp = m_nBppBefore;
        m_pVidMode->m_dwRBitShift = m_shifts[0];
        m_pVidMode->m_dwGBitShift = m_shifts[1];
        m_pVidMode->m_dwBBitShift = m_shifts[2];
        m_pVidMode->field_24 = m_field24Before;
        m_pVidMode->m_nFade = m_nFadeBefore;
        for (IDirectDrawSurface* pSurface : m_surfaces) {
            if (pSurface != NULL) {
                pSurface->Release();
            }
        }
        if (m_pDirectDraw != NULL) {
            m_pDirectDraw->Release();
        }
    }
    RenderScope(const RenderScope&) = delete;
    RenderScope& operator=(const RenderScope&) = delete;

    bool Ok() const
    {
        return m_surfaces[CVIDINF_SURFACE_BACK] != NULL && m_surfaces[CVIDINF_SURFACE_2] != NULL
            && m_surfaces[CVIDINF_SURFACE_3] != NULL;
    }

    CInfinity* Infinity() { return &m_infinity; }
    CVidMode* VidMode() { return m_pVidMode; }

    // The back buffer, after filling it with `fill` when `clear` is set.
    std::vector<DWORD> Back(bool clear = false, DWORD fill = 0)
    {
        std::vector<DWORD> pixels;
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        if (!m_pVidMode->LockSurface(CVIDINF_SURFACE_BACK, &ddsd, CRect(0, 0, kWidth, kHeight))) {
            return pixels;
        }
        for (int y = 0; y < kHeight; y++) {
            DWORD* row = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch);
            if (clear) {
                std::fill(row, row + kWidth, fill);
            }
            pixels.insert(pixels.end(), row, row + kWidth);
        }
        m_pVidMode->UnLockSurface(CVIDINF_SURFACE_BACK, ddsd.lpSurface);
        return pixels;
    }

private:
    IDirectDrawSurface* Create(int width, int height)
    {
        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
        desc.dwWidth = width;
        desc.dwHeight = height;
        IDirectDrawSurface* pSurface = NULL;
        if (m_pDirectDraw->CreateSurface(&desc, &pSurface, NULL) != DD_OK) {
            return NULL;
        }
        return pSurface;
    }

    CVidMode* m_pVidMode;
    IDirectDraw* m_pDirectDraw = NULL;
    IDirectDrawSurface* m_surfaces[9];
    UINT m_nSurfacesBefore;
    IDirectDrawSurface** m_pSurfacesBefore;
    USHORT m_nBppBefore;
    DWORD m_shifts[3];
    COLORREF m_field24Before;
    BYTE m_nFadeBefore;
    CInfinity m_infinity;
};

// With IWD2R_SHOT_DIR set, the back buffer as <dir>/<name>.png, to look at.
inline void Shot(const std::vector<DWORD>& pixels, const std::string& name)
{
    const char* dir = std::getenv("IWD2R_SHOT_DIR");
    if (dir == nullptr || *dir == '\0' || name.empty() || pixels.size() != static_cast<size_t>(kWidth * kHeight)) {
        return;
    }
    const std::string path = std::string(dir) + "/" + name + ".png";
    platform::WritePng(path.c_str(), reinterpret_cast<const uint32_t*>(pixels.data()), kWidth, kHeight, kWidth * 4);
}

// What one Render left in the back buffer, cleared to black first.
struct Drawn {
    int changed = 0; // pixels that are no longer black
    int outside = 0; // of those, outside the expected bounds
    int red = 0; // of those, with some red
    int blue = 0; // of those, with some blue
};

inline Drawn Count(const std::vector<DWORD>& pixels, const CRect& rBounds)
{
    Drawn drawn;
    for (int y = 0; y < kHeight; y++) {
        for (int x = 0; x < kWidth; x++) {
            const DWORD pixel = pixels[y * kWidth + x] & 0xFFFFFF;
            if (pixel == 0) {
                continue;
            }
            drawn.changed++;
            if (!rBounds.PtInRect(CPoint(x, y))) {
                drawn.outside++;
            }
            if ((pixel & 0xFF0000) != 0) {
                drawn.red++;
            }
            if ((pixel & 0xFF) != 0) {
                drawn.blue++;
            }
        }
    }
    return drawn;
}

// The area's video mode with shim surfaces as large as the area's viewport
// reaches (the back buffer) and the two keyed FX surfaces, at 32 bpp, faded
// in; the area's infinity draws with it.
class AreaScope {
public:
    explicit AreaScope(CGameArea* pArea)
        : m_pInfinity(pArea->GetInfinity())
        , m_pVidMode(g_pChitin->GetCurrentVideoMode())
        , m_nSurfacesBefore(m_pVidMode->m_nSurfaces)
        , m_pSurfacesBefore(m_pVidMode->pSurfaces)
        , m_nBppBefore(g_pChitin->cVideo.m_nBpp)
        , m_shifts { m_pVidMode->m_dwRBitShift, m_pVidMode->m_dwGBitShift, m_pVidMode->m_dwBBitShift }
        , m_field24Before(m_pVidMode->field_24)
        , m_nFadeBefore(m_pVidMode->m_nFade)
        , m_pInfinityModeBefore(m_pInfinity->pVidMode)
        , m_rScreen(m_pInfinity->rViewPort)
    {
        for (IDirectDrawSurface*& pSurface : m_surfaces) {
            pSurface = NULL;
        }
        if (m_rScreen.Width() <= 0 || m_rScreen.Height() <= 0 || DirectDrawCreate(NULL, &m_pDirectDraw, NULL) != DD_OK) {
            m_pDirectDraw = NULL;
            return;
        }
        DDCOLORKEY key = { kKey, kKey };
        m_surfaces[CVIDINF_SURFACE_BACK] = Create(m_rScreen.right, m_rScreen.bottom);
        // The tile renderer draws the pointer on the front surface, saving
        // what it covers in surface 4.
        m_surfaces[CVIDINF_SURFACE_FRONT] = Create(m_rScreen.right, m_rScreen.bottom);
        m_surfaces[CVIDINF_SURFACE_2] = Create(CVIDINF_FX_WIDTH, CVIDINF_FX_HEIGHT);
        m_surfaces[CVIDINF_SURFACE_3] = Create(CVIDINF_FX_WIDTH, CVIDINF_FX_HEIGHT);
        m_surfaces[CVIDINF_SURFACE_4] = Create(128, 128);
        for (int n : { CVIDINF_SURFACE_2, CVIDINF_SURFACE_3, CVIDINF_SURFACE_4 }) {
            if (m_surfaces[n] != NULL) {
                m_surfaces[n]->SetColorKey(DDCKEY_SRCBLT, &key);
            }
        }
        m_pVidMode->m_nSurfaces = 9;
        m_pVidMode->pSurfaces = m_surfaces;
        g_pChitin->cVideo.m_nBpp = 32;
        m_pVidMode->m_dwRBitShift = 16;
        m_pVidMode->m_dwGBitShift = 8;
        m_pVidMode->m_dwBBitShift = 0;
        m_pVidMode->field_24 = kKey;
        m_pVidMode->m_nFade = CVidMode::NUM_FADE_FRAMES;
        m_pInfinity->pVidMode = m_pVidMode;
    }
    ~AreaScope()
    {
        if (!m_vram.empty()) {
            g_pBaldurChitin->GetObjectGame()->m_cVRamPool.DetachSurfaces();
            CVidInf* pVidInf = static_cast<CVidInf*>(m_pVidMode);
            pVidInf->m_pVRamSurfaces = NULL;
            pVidInf->m_nVRamSurfaces = 0;
            for (IDirectDrawSurface* pSurface : m_vram) {
                if (pSurface != NULL) {
                    pSurface->Release();
                }
            }
        }
        m_pVidMode->m_nSurfaces = m_nSurfacesBefore;
        m_pVidMode->pSurfaces = m_pSurfacesBefore;
        g_pChitin->cVideo.m_nBpp = m_nBppBefore;
        m_pVidMode->m_dwRBitShift = m_shifts[0];
        m_pVidMode->m_dwGBitShift = m_shifts[1];
        m_pVidMode->m_dwBBitShift = m_shifts[2];
        m_pVidMode->field_24 = m_field24Before;
        m_pVidMode->m_nFade = m_nFadeBefore;
        m_pInfinity->pVidMode = m_pInfinityModeBefore;
        for (IDirectDrawSurface* pSurface : m_surfaces) {
            if (pSurface != NULL) {
                pSurface->Release();
            }
        }
        if (m_pDirectDraw != NULL) {
            m_pDirectDraw->Release();
        }
    }
    AreaScope(const AreaScope&) = delete;
    AreaScope& operator=(const AreaScope&) = delete;

    bool Ok() const
    {
        return m_surfaces[CVIDINF_SURFACE_BACK] != NULL && m_surfaces[CVIDINF_SURFACE_2] != NULL
            && m_surfaces[CVIDINF_SURFACE_3] != NULL;
    }

    CVidMode* VidMode() { return m_pVidMode; }
    const CRect& Screen() const { return m_rScreen; }

    // The area's whole render (tiles, then its objects) on a black back
    // buffer. The tiles go through 64x64 VRAM surfaces (CVRamPool), which
    // the test's video mode does not have: they are made on the first call.
    std::vector<DWORD> RenderArea(CGameArea* pArea)
    {
        CVidInf* pVidInf = static_cast<CVidInf*>(m_pVidMode);
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        if (m_vram.empty()) {
            m_vram.assign(512, nullptr);
            for (IDirectDrawSurface*& pSurface : m_vram) {
                pSurface = Create(64, 64);
            }
            pVidInf->m_pVRamSurfaces = m_vram.data();
            pVidInf->m_nVRamSurfaces = static_cast<int>(m_vram.size());
            pGame->m_cVRamPool.DetachSurfaces();
            pGame->m_cVRamPool.AttachSurfaces(pVidInf);
            m_pInfinity->AttachVRamPool(&(pGame->m_cVRamPool));
        }
        // The cache screen's flag fades the screen out on the area's first
        // render after a load; the game is past that here.
        CCacheStatus::dword_8D0BA8 = FALSE;
        Back(true, 0);
        pArea->Render(m_pVidMode, CVIDINF_SURFACE_BACK);
        return Back();
    }

    // A world rectangle in the back buffer's coordinates.
    CRect ToScreen(const CRect& rWorld) const
    {
        CRect r(rWorld);
        r.OffsetRect(m_pInfinity->rViewPort.left - m_pInfinity->nCurrentX, m_pInfinity->rViewPort.top - m_pInfinity->nCurrentY);
        return r;
    }

    // Puts the viewport's centre on a world point (within the area), with
    // no scroll under way: the infinity's render moves the view towards
    // m_ptScrollDest otherwise.
    void CentreOn(const CPoint& pt)
    {
        m_pInfinity->m_ptScrollDest = CPoint(-1, -1);
        m_pInfinity->SetViewPosition(pt.x - m_rScreen.Width() / 2, pt.y - m_rScreen.Height() / 2, TRUE);
        m_pInfinity->nCurrentX = m_pInfinity->nNewX;
        m_pInfinity->nCurrentY = m_pInfinity->nNewY;
    }

    // The viewport's pixels, after filling them with `fill` when `clear` is
    // set.
    std::vector<DWORD> Back(bool clear = false, DWORD fill = 0)
    {
        std::vector<DWORD> pixels;
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        if (!m_pVidMode->LockSurface(CVIDINF_SURFACE_BACK, &ddsd, m_rScreen)) {
            return pixels;
        }
        for (int y = 0; y < m_rScreen.Height(); y++) {
            DWORD* row = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch);
            if (clear) {
                std::fill(row, row + m_rScreen.Width(), fill);
            }
            pixels.insert(pixels.end(), row, row + m_rScreen.Width());
        }
        m_pVidMode->UnLockSurface(CVIDINF_SURFACE_BACK, ddsd.lpSurface);
        return pixels;
    }

    // The viewport as <IWD2R_SHOT_DIR>/<name>.png when that is set.
    void Shot(const std::vector<DWORD>& pixels, const std::string& name) const
    {
        const char* dir = std::getenv("IWD2R_SHOT_DIR");
        if (dir == nullptr || *dir == '\0' || pixels.size() != static_cast<size_t>(m_rScreen.Width() * m_rScreen.Height())) {
            return;
        }
        const std::string path = std::string(dir) + "/" + name + ".png";
        platform::WritePng(path.c_str(), reinterpret_cast<const uint32_t*>(pixels.data()), m_rScreen.Width(), m_rScreen.Height(),
            m_rScreen.Width() * 4);
    }

private:
    IDirectDrawSurface* Create(int width, int height)
    {
        DDSURFACEDESC desc;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
        desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
        desc.dwWidth = width;
        desc.dwHeight = height;
        IDirectDrawSurface* pSurface = NULL;
        if (m_pDirectDraw->CreateSurface(&desc, &pSurface, NULL) != DD_OK) {
            return NULL;
        }
        return pSurface;
    }

    CInfinity* m_pInfinity;
    CVidMode* m_pVidMode;
    IDirectDraw* m_pDirectDraw = NULL;
    IDirectDrawSurface* m_surfaces[9];
    UINT m_nSurfacesBefore;
    IDirectDrawSurface** m_pSurfacesBefore;
    USHORT m_nBppBefore;
    DWORD m_shifts[3];
    COLORREF m_field24Before;
    BYTE m_nFadeBefore;
    CVidMode* m_pInfinityModeBefore;
    CRect m_rScreen;
    std::vector<IDirectDrawSurface*> m_vram;
};

// The pixels that differ between two renders inside a screen rectangle, and
// how many of the second render's there are exactly `colour` (0x00RRGGBB).
struct Difference {
    int changed = 0;
    int exact = 0;
};

inline Difference Compare(const std::vector<DWORD>& before, const std::vector<DWORD>& after, const CRect& rScreen, const CRect& rIn, DWORD colour = 0xFFFFFFFF)
{
    Difference d;
    CRect r;
    r.IntersectRect(rIn, CRect(0, 0, rScreen.Width(), rScreen.Height()));
    for (int y = r.top; y < r.bottom; y++) {
        for (int x = r.left; x < r.right; x++) {
            const DWORD a = before[y * rScreen.Width() + x] & 0xFFFFFF;
            const DWORD b = after[y * rScreen.Width() + x] & 0xFFFFFF;
            if (a != b) {
                d.changed++;
            }
            if (b == colour) {
                d.exact++;
            }
        }
    }
    return d;
}

} // namespace render_surfaces
