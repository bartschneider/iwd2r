#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameAnimationType.h"
#include "CGameAnimationTypeMonster.h"
#include "CGameAnimationTypeMonsterAnkheg.h"
#include "CGameAnimationTypeMonsterMulti.h"
#include "CGameAnimationTypeMonsterOld.h"
#include "CInfinity.h"
#include "CScreenCreateChar.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/png.h"
#include "platform/window.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// The Render functions of the creature animations that had none
// (CGameAnimationTypeMonster 0x6AC310, MonsterOld 0x6B03E0, MonsterMulti
// 0x6B71F0) or a stub (MonsterAnkheg 0x6C3910), drawn with the game's BAMs
// through a standalone CInfinity into DirectDraw shim surfaces: the back
// buffer and the two FX surfaces, keyed on pure green as CVidInf's
// CreateSurfaces keys them. The pixels that change must lie in the bounds
// the animation reports for the frame, a creature that is not fading must be
// drawn solid (FXUnlock gets NULL: a rectangle would dither half of it
// away), and the tint must keep its red. Every engine works on a private
// view of the install (tests/data_view.h).

namespace {

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

// The engine as iwd2r_main builds it (tests/test_gam.cpp's StartEngine).
bool StartEngine(std::string& error)
{
    static bool started = false;
    static std::string startError;
    static bool tried = false;
    if (tried) {
        error = startError;
        return started;
    }
    tried = true;

#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
    static std::unique_ptr<DataView> view;
    const std::string original = DataDir();
    view = std::make_unique<DataView>(original);
#ifdef _WIN32
    _putenv_s("IWD2_DATA", view->Path().c_str());
#else
    setenv("IWD2_DATA", view->Path().c_str(), 1);
#endif
    const bool found = platform::ResolveDataDir();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", original.c_str());
#else
    setenv("IWD2_DATA", original.c_str(), 1);
#endif
    if (!found) {
        startError = "data folder not found";
        return false;
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        return false;
    }
    chitin->Init(NULL);
    POSITION pos = chitin->lEngines.GetHeadPosition();
    while (pos != NULL) {
        CWarp* pEngine = static_cast<CWarp*>(chitin->lEngines.GetNext(pos));
        if (pEngine != NULL) {
            pEngine->EngineInitialized();
        }
    }
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

constexpr int kWidth = 640;
constexpr int kHeight = 480;
// The FX surfaces' colour key and the transparent fill (field_24): pure
// green of the 32-bit format.
constexpr DWORD kKey = 0x0000FF00;

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
void Shot(const std::vector<DWORD>& pixels, const std::string& name)
{
    const char* dir = std::getenv("IWD2R_SHOT_DIR");
    if (dir == nullptr || *dir == '\0' || pixels.size() != static_cast<size_t>(kWidth * kHeight)) {
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

Drawn Count(const std::vector<DWORD>& pixels, const CRect& rBounds)
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

// Renders the animation's current frame at `pos` as CGameSprite::Render
// does (its FX rectangle and reference point, then its bounds) and returns
// what changed. rBounds receives the bounds the animation reports.
Drawn Render(RenderScope& scope, CGameAnimationType* pAnimation, const CPoint& pos, BOOL bFadeOut, COLORREF rgbTint, CRect& rBounds, const std::string& shot)
{
    scope.Back(true, 0);
    CRect rFx;
    CPoint ptReference;
    pAnimation->CalculateFxRect(rFx, ptReference, 0);
    pAnimation->CalculateGCBoundsRect(rBounds, pos, ptReference, 0, rFx.Width(), rFx.Height());
    pAnimation->Render(scope.Infinity(),
        scope.VidMode(),
        CVIDINF_SURFACE_BACK,
        rFx,
        pos,
        ptReference,
        0,
        rgbTint,
        rBounds,
        FALSE,
        bFadeOut,
        0,
        0);
    const std::vector<DWORD> pixels = scope.Back();
    Shot(pixels, shot);
    return Count(pixels, rBounds);
}

// The checks every class shares: solid within its bounds, dithered when
// fading, and a tint without blue keeps the red.
void CheckRender(RenderScope& scope, CGameAnimationType* pAnimation, const CPoint& pos, const std::string& name)
{
    CRect rBounds;
    const Drawn solid = Render(scope, pAnimation, pos, FALSE, RGB(255, 255, 255), rBounds, name);
    INFO("bounds " << rBounds.left << "," << rBounds.top << " - " << rBounds.right << "," << rBounds.bottom);
    INFO("drawn " << solid.changed << ", outside " << solid.outside);
    CHECK(rBounds.Width() > 0);
    CHECK(rBounds.Height() > 0);
    CHECK(solid.changed > 100);
    CHECK(solid.outside == 0);

    // Fading: FXUnlock fills the FX rectangle with the transparent colour
    // through a dithered FillPoly, so about half the pixels stay black.
    const Drawn faded = Render(scope, pAnimation, pos, TRUE, RGB(255, 255, 255), rBounds, name + "_faded");
    INFO("faded " << faded.changed);
    CHECK(faded.changed > 0);
    CHECK(faded.outside == 0);
    CHECK(faded.changed * 4 < solid.changed * 3);
    CHECK(faded.changed * 4 > solid.changed);

    // A yellow tint (no blue): each channel is the cell's plus the tint's
    // less 255, so blue goes to 0 and red stays (red from the low byte).
    const Drawn yellow = Render(scope, pAnimation, pos, FALSE, RGB(255, 255, 0), rBounds, name + "_yellow");
    INFO("yellow " << yellow.changed << ", with red " << yellow.red << ", with blue " << yellow.blue);
    CHECK(yellow.changed > 0);
    CHECK(yellow.outside == 0);
    CHECK(yellow.blue == 0);
    CHECK(yellow.red > yellow.changed / 2);
}

bool Begin(std::string& why)
{
    if (DataDir() == nullptr) {
        why = "IWD2_DATA unset";
        return false;
    }
    std::string error;
    if (!StartEngine(error)) {
        why = error;
        return false;
    }
    return true;
}

}

// A mind flayer (0x7F01, MMIN's split BAMs): MonsterOld::NewSupport accepts
// it, so the factory makes a CGameAnimationTypeMonster. Facing south (0) and
// north-east (12, mirrored: MIRROR_FX).
TEST_CASE("creature render: CGameAnimationTypeMonster draws a mind flayer", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing : { 0, 12 }) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x7F01, colors, facing));
        REQUIRE(dynamic_cast<CGameAnimationTypeMonster*>(pAnimation.get()) != nullptr);
        CheckRender(scope, pAnimation.get(), CPoint(320, 300), "mind_flayer_" + std::to_string(facing));
    }
}

// A wolf (0x7B00, MWLF): NewSupport refuses it, so the factory makes a
// CGameAnimationTypeMonsterOld.
TEST_CASE("creature render: CGameAnimationTypeMonsterOld draws a wolf", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    for (WORD facing : { 0, 12 }) {
        INFO("facing " << facing);
        std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x7B00, colors, facing));
        REQUIRE(dynamic_cast<CGameAnimationTypeMonsterOld*>(pAnimation.get()) != nullptr);
        CheckRender(scope, pAnimation.get(), CPoint(320, 300), "wolf_" + std::to_string(facing));
    }
}

// An ankheg (0x3000). The game has no MAKH BAMs (no creature uses the
// class), so its body and hole cells are given a wolf's and a spider's
// BAMs. Each cell is drawn around its own frame centre: the bounds are the
// union of the two cells' bounds.
TEST_CASE("creature render: CGameAnimationTypeMonsterAnkheg draws its hole and its body", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x3000, colors, 0));
    auto* pAnkheg = dynamic_cast<CGameAnimationTypeMonsterAnkheg*>(pAnimation.get());
    REQUIRE(pAnkheg != nullptr);
    CHECK_FALSE(pAnkheg->IsFalseColor());
    pAnkheg->m_g1VidCellBase.SetResRef(CResRef("MWLFG1"), FALSE, TRUE, TRUE);
    pAnkheg->m_g1VidCellHoleBase.SetResRef(CResRef("MSPIG1"), FALSE, TRUE, TRUE);
    pAnkheg->m_currentBamSequence = 0;
    pAnkheg->ChangeDirection(0);

    const CPoint pos(320, 300);
    CRect rBounds;
    for (CVidCell* pCell : { pAnkheg->m_currentVidCellHole, pAnkheg->m_currentVidCell }) {
        CPoint ptCenter;
        CSize frameSize;
        REQUIRE(pCell->GetCurrentCenterPoint(ptCenter, FALSE));
        REQUIRE(pCell->GetCurrentFrameSize(frameSize, FALSE));
        CRect rCell;
        pAnkheg->CalculateGCBoundsRect(rCell, pos, ptCenter, 0, frameSize.cx, frameSize.cy);
        rBounds.UnionRect(rBounds, rCell);
    }
    INFO("bounds " << rBounds.left << "," << rBounds.top << " - " << rBounds.right << "," << rBounds.bottom);

    auto draw = [&](BOOL bFadeOut, COLORREF rgbTint, const std::string& name) {
        scope.Back(true, 0);
        pAnkheg->Render(scope.Infinity(), scope.VidMode(), CVIDINF_SURFACE_BACK, CRect(), pos, CPoint(), 0, rgbTint, CRect(), FALSE, bFadeOut, 0, 0);
        const std::vector<DWORD> pixels = scope.Back();
        Shot(pixels, name);
        return Count(pixels, rBounds);
    };
    const Drawn solid = draw(FALSE, RGB(255, 255, 255), "ankheg");
    INFO("drawn " << solid.changed << ", outside " << solid.outside);
    CHECK(solid.changed > 100);
    CHECK(solid.outside == 0);
    const Drawn faded = draw(TRUE, RGB(255, 255, 255), "ankheg_faded");
    INFO("faded " << faded.changed);
    CHECK(faded.changed > 0);
    CHECK(faded.changed * 4 < solid.changed * 3);
    // The ankheg sets the tint itself on both cells (no blue: none left).
    const Drawn yellow = draw(FALSE, RGB(255, 255, 0), "ankheg_yellow");
    INFO("yellow " << yellow.changed << ", with red " << yellow.red << ", with blue " << yellow.blue);
    CHECK(yellow.blue == 0);
    CHECK(yellow.red > yellow.changed / 2);
    CHECK(pAnkheg->m_currentVidCell->GetTintColor() == RGB(255, 255, 0));
    CHECK(pAnkheg->m_currentVidCellHole->GetTintColor() == RGB(255, 255, 0));
}
