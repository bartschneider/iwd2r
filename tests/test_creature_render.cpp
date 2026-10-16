#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CCacheStatus.h"
#include "CCreatureFile.h"
#include "CGameAnimationType.h"
#include "CGameAnimationTypeMonster.h"
#include "CGameAnimationTypeMonsterAnkheg.h"
#include "CGameAnimationTypeMonsterMulti.h"
#include "CGameAnimationTypeMonsterOld.h"
#include "CGameArea.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/png.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The Render functions of the creature animations that had none
// (CGameAnimationTypeMonster 0x6AC310, MonsterOld 0x6B03E0, MonsterMulti
// 0x6B71F0) or a stub (MonsterAnkheg 0x6C3910), drawn with the game's BAMs
// through a standalone CInfinity into DirectDraw shim surfaces: the back
// buffer and the two FX surfaces, keyed on pure green as CVidInf's
// CreateSurfaces keys them. The pixels that change must lie in the bounds
// the animation reports for the frame, a creature that is not fading must be
// drawn solid (FXUnlock gets NULL: a rectangle would dither half of it
// away), and the tint must keep its red. Then the same creatures from the
// game's CRE files standing in AR1000 of a new game, drawn by the area's
// whole render (tiles through VRAM surfaces the test makes, then each
// object), and the door and container highlights drawn there
// (CGameDoor::Render 0x488B30, CGameContainer::Render 0x47F580). With
// IWD2R_SHOT_DIR set, each render is also written there as a PNG. Every
// engine works on a private view of the install (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

fs::path g_view;

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
    g_view = view->Path();
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

// The black dragon (0x1201, 60GUARDI.CRE): a CGameAnimationTypeMonsterMulti
// of nine quadrants with split BAMs, MDR2 + group + quadrant + sequence
// character + direction character. Its ChangeDirection (0x6B5D80) and
// SetSequence (0x6B7660) were missing too: the constructor's facing never
// reached the cells, and no sequence changed them. The groups each
// sequence picks are the ones the game data has (MDR21xx0 .. MDR25xx1).
TEST_CASE("creature render: CGameAnimationTypeMonsterMulti draws the black dragon's quadrants", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    RenderScope scope;
    REQUIRE(scope.Ok());
    BYTE colors[16] = {};
    std::unique_ptr<CGameAnimationType> pAnimation(CGameAnimationType::SetAnimationType(0x1201, colors, 0));
    auto* pDragon = dynamic_cast<CGameAnimationTypeMonsterMulti*>(pAnimation.get());
    REQUIRE(pDragon != nullptr);
    REQUIRE(pDragon->m_nQuadrants == 9);
    CHECK(pDragon->m_currentBamDirection == 0);
    CHECK(pDragon->m_currentVidCell == pDragon->m_g2VidCellBase);
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR22100");
    CHECK(pDragon->m_currentVidCell[8].GetResRef().GetResRefStr() == "MDR22900");
    CheckRender(scope, pDragon, CPoint(320, 330), "dragon_0");

    // Sequence 9: group 4 with the sequence character 2.
    CHECK(pDragon->SetSequence(9) == 9);
    CHECK(pDragon->m_currentVidCell == pDragon->m_g4VidCellBase);
    CHECK(pDragon->m_currentBamSequence == 2);
    CHECK(pDragon->field_44A == &pDragon->field_67C);
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR24120");
    CHECK(pDragon->field_800 == 9);
    // Sequences 14 and 15 become 7 (group 2), 11 to 13 become 0 (group 3).
    CHECK(pDragon->SetSequence(14) == 7);
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR22100");
    CHECK(pDragon->SetSequence(11) == 0);
    CHECK(pDragon->m_currentVidCell == pDragon->m_g3VidCellBase);
    CHECK(pDragon->m_currentVidCell[4].GetResRef().GetResRefStr() == "MDR23500");

    // Facing 12 (north-east) mirrors direction 4: the files of direction 4,
    // sequence 9 * 0 - 12 + 16 = 4.
    pDragon->ChangeDirection(12);
    CHECK(pDragon->m_splitDirectionBamChar == '4');
    CHECK(pDragon->m_currentVidCell[0].GetResRef().GetResRefStr() == "MDR23104");
    CHECK(pDragon->m_currentVidCell[0].m_nCurrentSequence == 4);
    CHECK(pDragon->IsMirroring());
    CheckRender(scope, pDragon, CPoint(320, 330), "dragon_12");
}

namespace {

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

// The main loop's drawn frames (tests/test_gam.cpp's FramePump).
class FramePump {
public:
    FramePump()
        : m_running(true)
        , m_thread([this] {
            while (m_running) {
                g_pChitin->m_bDisplayStale = FALSE;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        })
    {
    }
    ~FramePump()
    {
        m_running = false;
        m_thread.join();
    }
    FramePump(const FramePump&) = delete;
    FramePump& operator=(const FramePump&) = delete;

private:
    std::atomic<bool> m_running;
    std::thread m_thread;
};

// The first finished character in the view's Characters folder
// (tests/test_combat.cpp's FinishedCharacter).
std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

// A new game with one party member standing in AR1000 (tests/test_combat.cpp's
// NewGameInStartArea).
CGameSprite* NewGameInStartArea(std::vector<BYTE>& bytes)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        -1,
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    pSprite->m_sName = "Probe";
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->EngineGameInit();
    g_pBaldurChitin->pActiveEngine = pWorld;
    pGame->m_bGameLoaded = TRUE;
    return pSprite;
}

// A creature of the game's files put on the area's front list at `pos`
// (tests/test_combat.cpp's AddCreature).
CGameSprite* AddCreature(CGameArea* pArea, const char* resRef, const CPoint& pos)
{
    CCreatureFile creatureData;
    creatureData.SetResRef(CResRef(resRef), TRUE, TRUE);
    if (creatureData.GetDataSize() == 0) {
        return NULL;
    }
    CGameSprite* pSprite = new CGameSprite(creatureData.GetData(),
        creatureData.GetDataSize(),
        0,
        0,
        -1,
        0,
        0,
        INT_MAX,
        pos,
        0);
    pSprite->SetResRef(CResRef(resRef));
    creatureData.ReleaseData();
    pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_FRONT);
    return pSprite;
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

Difference Compare(const std::vector<DWORD>& before, const std::vector<DWORD>& after, const CRect& rScreen, const CRect& rIn, DWORD colour = 0xFFFFFFFF)
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

// A new game in AR1000 with its party member, the area explored and
// visible, drawn into AreaScope's surfaces.
struct World {
    CGameSprite* pCharacter = NULL;
    CGameArea* pArea = NULL;
};

bool NewWorld(World& world, std::string& why)
{
    if (!Begin(why)) {
        return false;
    }
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        why = "no CHR V2.2 file in the Characters folder";
        return false;
    }
    world.pCharacter = NewGameInStartArea(cre);
    world.pArea = world.pCharacter->GetArea();
    if (world.pArea == NULL) {
        why = "the party member is in no area";
        return false;
    }
    world.pArea->m_visibility.SetAreaExplored();
    world.pArea->m_visibility.SetAreaVisible(TRUE);
    return true;
}

// The first object of `type` on one of the area's lists for which `accept`
// holds.
template <class T, class F>
T* FindObject(CTypedPtrList<CPtrList, int*>& list, BYTE type, F accept)
{
    CGameObjectArray* pObjects = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list.GetNext(pos)));
        CGameObject* pObject;
        if (pObjects->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
            pObjects->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (pObject->GetObjectType() == type && accept(static_cast<T*>(pObject))) {
                return static_cast<T*>(pObject);
            }
        }
    }
    return NULL;
}

}

// Creatures of the three reachable classes, from the game's own files,
// standing in AR1000 next to a new game's party member and drawn by the
// area's render (CGameArea::Render, then CGameSprite::Render for each)
// with the WED's wall polygons, the area's tint and the viewport: a mind
// flayer (53ZEPH, 0x7F01: CGameAnimationTypeMonster), a wolf (12WOLF,
// 0x7B00: MonsterOld) and the black dragon (60GUARDI, 0x1201:
// MonsterMulti). The area is explored and visible and each creature seen
// for a while, so none is drawn faded. Each creature changes the area's
// picture inside its bounds: the render with it against the render with
// it hidden (CreatureHidden, base stats 0x294, which the wolf and the
// dragon have set in their files and which is cleared here).
TEST_CASE("creature render: creatures of the new Renders stand visible in AR1000", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    const CPoint pos = world.pCharacter->GetPos();

    struct Creature {
        const char* resRef;
        USHORT animationID;
        CPoint offset;
        CGameSprite* pSprite;
    };
    Creature creatures[] = {
        { "53ZEPH", 0x7F01, CPoint(-160, 40), NULL },
        { "12WOLF", 0x7B00, CPoint(110, 50), NULL },
        { "60GUARDI", 0x1201, CPoint(260, 120), NULL },
    };
    for (Creature& c : creatures) {
        c.pSprite = AddCreature(pArea, c.resRef, CPoint(pos.x + c.offset.x, pos.y + c.offset.y));
        REQUIRE(c.pSprite != NULL);
        CHECK(c.pSprite->GetAnimation()->GetAnimationId() == c.animationID);
        c.pSprite->m_canBeSeen = CGameObject::VISIBLE_DELAY + 10;
        c.pSprite->m_baseStats.field_294 = 0;
    }
    pArea->SortLists();

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    scope.CentreOn(CPoint(pos.x + 60, pos.y + 20));
    const std::vector<DWORD> all = scope.RenderArea(pArea);
    scope.Shot(all, "ar1000_world");

    for (Creature& c : creatures) {
        INFO(c.resRef);
        c.pSprite->m_baseStats.field_294 = 1;
        const std::vector<DWORD> without = scope.RenderArea(pArea);
        c.pSprite->m_baseStats.field_294 = 0;

        CRect rFx;
        CPoint ptReference;
        c.pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, c.pSprite->m_posZ);
        CRect rBounds;
        c.pSprite->GetAnimation()->CalculateGCBoundsRect(rBounds, c.pSprite->GetPos(), ptReference, c.pSprite->m_posZ, rFx.Width(), rFx.Height());
        const CRect rScreen = scope.ToScreen(rBounds);
        INFO("bounds on screen " << rScreen.left << "," << rScreen.top << " - " << rScreen.right << "," << rScreen.bottom);
        const Difference inside = Compare(without, all, scope.Screen(), rScreen);
        INFO("changed inside " << inside.changed);
        CHECK(inside.changed > 300);

        // Nothing outside the creature's bounds and the ring the sprite
        // draws around its feet (its ellipse, CGameAnimation::GetEllipseRect)
        // depends on it.
        CRect rEllipse(c.pSprite->GetAnimation()->GetEllipseRect());
        rEllipse.OffsetRect(c.pSprite->GetPos());
        rEllipse.InflateRect(2, 2);
        CRect rAround;
        rAround.UnionRect(rBounds, rEllipse);
        const Difference around = Compare(without, all, scope.Screen(), scope.ToScreen(rAround));
        const Difference whole = Compare(without, all, scope.Screen(), CRect(0, 0, scope.Screen().Width(), scope.Screen().Height()));
        CHECK(whole.changed == around.changed);
    }
}

// The door and container highlights (CGameDoor::Render 0x488B30,
// CGameContainer::Render 0x47F580) on a closed door and a container of
// AR1000. With m_drawPoly set (the outline the game draws for a while, as
// after a script's SetDrawPoly) the polygon is outlined in pure red
// (CInfinity::OutlinePoly) and, with translucent blits on, filled through
// the FX surface (FillClosedPoly 0x48B3C0, FillPoly 0x481A10, then
// CVidInf::FXRender 0x79D680 with flags 0x10) in the red PackColor 0x79ADF0
// packs. Flag 0x10 is DrawHLineShaded32: each pixel of the polygon blended
// towards the colour by its brightness, so on a uniform grey every pixel
// of the polygon becomes the same reddish grey and the dark ones of the
// area stay dark.
TEST_CASE("creature render: AR1000's doors and containers draw their highlight", "[render][data]")
{
    std::string why;
    if (!Begin(why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    FramePump pump;
    World world;
    if (!NewWorld(world, why)) {
        SKIP("[iwd2r-skip] " + why);
    }
    CGameArea* pArea = world.pArea;
    CGameDoor* pDoor = FindObject<CGameDoor>(pArea->m_lVertSort, CGameObject::TYPE_DOOR, [](CGameDoor* p) {
        return (p->m_dwFlags & 0x1) == 0 && (p->m_dwFlags & 0x2080) == 0 && p->m_nClosedPolygon > 2;
    });
    REQUIRE(pDoor != NULL);
    CGameContainer* pContainer = FindObject<CGameContainer>(pArea->m_lVertSortBack, CGameObject::TYPE_CONTAINER, [](CGameContainer* p) {
        return p->m_containerType != 4 && p->m_nPolygon > 2;
    });
    REQUIRE(pContainer != NULL);
    // The rectangles the fills walk: the original's CGameAIBase::sub_481890
    // (0x481890) puts the bounding rectangle itself in the array when it is
    // at most 512 x 512, as these are, else 512 x 512 pieces of it. Ours is
    // still a stub (vis-probe's file) and leaves the arrays empty, so the
    // test fills them as the original would.
    REQUIRE(pDoor->m_rClosedBounding.Width() <= 512);
    REQUIRE(pDoor->m_rClosedBounding.Height() <= 512);
    REQUIRE(pContainer->m_rBounding.Width() <= 512);
    REQUIRE(pContainer->m_rBounding.Height() <= 512);
    pDoor->field_6DA.SetSize(0);
    pDoor->field_6DA.Add(&pDoor->m_rClosedBounding);
    pContainer->field_8D6.SetSize(0);
    pContainer->field_8D6.Add(&pContainer->m_rBounding);

    AreaScope scope(pArea);
    REQUIRE(scope.Ok());
    const BOOLEAN translucentBefore = CInfinity::TRANSLUCENT_BLTS_ON;
    const CRect whole(0, 0, scope.Screen().Width(), scope.Screen().Height());
    // Pure red packed for the 32-bit surface (shifts 16/8/0).
    REQUIRE(scope.VidMode()->PackColor(RGB(255, 0, 0)) == 0xFF0000);

    struct Case {
        const char* name;
        CRect rBounding;
        SHORT* pDrawPoly;
        std::function<void()> fill;
    };
    const Case cases[] = {
        { "door", pDoor->m_rClosedBounding, &pDoor->m_drawPoly, [&] {
             pDoor->FillClosedPoly(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK, scope.VidMode()->PackColor(RGB(255, 0, 0)));
         } },
        { "container", pContainer->m_rBounding, &pContainer->m_drawPoly, [&] {
             pContainer->FillPoly(pArea, scope.VidMode(), CVIDINF_SURFACE_BACK, scope.VidMode()->PackColor(RGB(255, 0, 0)));
         } },
    };
    for (const Case& c : cases) {
        INFO(c.name);
        scope.CentreOn(c.rBounding.CenterPoint());
        const CRect rScreen = scope.ToScreen(c.rBounding);
        CRect rAround(rScreen);
        rAround.InflateRect(1, 1);
        INFO("bounds on screen " << rScreen.left << "," << rScreen.top << " - " << rScreen.right << "," << rScreen.bottom);

        // The fill alone on grey: (255 * 85 + 170 * 0x80) >> 8 = 0xA9 red,
        // 170 * 0x80 >> 8 = 0x55 green and blue.
        const std::vector<DWORD> grey = scope.Back(true, 0x808080);
        c.fill();
        const std::vector<DWORD> greyFilled = scope.Back();
        scope.Shot(greyFilled, std::string("ar1000_") + c.name + "_fill");
        const Difference shaded = Compare(grey, greyFilled, scope.Screen(), rScreen, 0xA95555);
        INFO("fill on grey: changed " << shaded.changed << ", reddish grey " << shaded.exact);
        CHECK(shaded.changed > 50);
        CHECK(shaded.exact == shaded.changed);
        CHECK(Compare(grey, greyFilled, scope.Screen(), whole).changed == shaded.changed);

        // In the area: the outline alone (translucent blits off), then the
        // outline and the fill.
        *c.pDrawPoly = 0;
        CInfinity::TRANSLUCENT_BLTS_ON = TRUE;
        const std::vector<DWORD> plain = scope.RenderArea(pArea);
        *c.pDrawPoly = 100;
        CInfinity::TRANSLUCENT_BLTS_ON = FALSE;
        const std::vector<DWORD> outlined = scope.RenderArea(pArea);
        CInfinity::TRANSLUCENT_BLTS_ON = TRUE;
        const std::vector<DWORD> filled = scope.RenderArea(pArea);
        scope.Shot(filled, std::string("ar1000_") + c.name + "_highlight");
        *c.pDrawPoly = 0;

        const Difference outline = Compare(plain, outlined, scope.Screen(), rAround, 0xFF0000);
        INFO("outline: changed " << outline.changed << ", red " << outline.exact);
        CHECK(outline.exact > 20);
        CHECK(outline.changed <= outline.exact);
        CHECK(Compare(plain, outlined, scope.Screen(), whole).changed == outline.changed);
        // The fill changes nothing outside the polygon's rectangle.
        const Difference fill = Compare(outlined, filled, scope.Screen(), rAround);
        CHECK(Compare(outlined, filled, scope.Screen(), whole).changed == fill.changed);
    }
    CInfinity::TRANSLUCENT_BLTS_ON = translucentBefore;
}
