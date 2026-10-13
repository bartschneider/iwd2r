#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CParticle.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidFont.h"
#include "CVidImage.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "CVidMosaic.h"
#include "CVidPoly.h"
#include "ScreenLayout.h"
#include "compat/ini.h"
#include "install_ini_backup.h"
#include "main.h"
#include "render/api.h"
#include "render/recording.h"

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <tuple>
#include <variant>

namespace rec = render::recording;

namespace {

bool IsState(const rec::Call& call)
{
    return std::holds_alternative<rec::BeginFrame>(call) || std::holds_alternative<rec::EndFrame>(call)
        || std::holds_alternative<rec::SetLayer>(call) || std::holds_alternative<rec::SetClip>(call);
}

// The drawing calls (everything but frame and state calls).
std::vector<rec::Call> Draws(const rec::Backend& backend)
{
    std::vector<rec::Call> out;
    for (const rec::Call& call : backend.calls)
        if (!IsState(call))
            out.push_back(call);
    return out;
}

// Runs one entry point with a fresh recording backend and returns the single
// draw it dispatched, as T.
template <typename T, typename F>
T DispatchOnce(F&& call)
{
    rec::Scope scope;
    call();
    const std::vector<rec::Call> draws = Draws(scope.backend);
    REQUIRE(draws.size() == 1);
    REQUIRE(std::holds_alternative<T>(draws[0]));
    return std::get<T>(draws[0]);
}

void SetEnv(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

bool SameRect(const RECT& a, const RECT& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool SamePoint(const POINT& a, int x, int y)
{
    return a.x == x && a.y == y;
}

}

TEST_CASE("classic_when_no_backend", "[render]")
{
    REQUIRE(render::Active() == nullptr);
    REQUIRE(render::Current() == render::Mode::Classic);

    {
        rec::Scope scope;
        REQUIRE(render::Active() == &scope.backend);
        REQUIRE(render::Current() == render::Mode::Backend);
    }

    REQUIRE(render::Active() == nullptr);
    REQUIRE(render::Current() == render::Mode::Classic);
}

TEST_CASE("base_backend_handles_nothing", "[render]")
{
    // The defaults return false, which leaves every call to the classic body.
    render::Backend base;
    CVidCell cell;
    REQUIRE_FALSE(base.DrawCell(cell, 0, POINT { 0, 0 }, nullptr, 0, -1));
    REQUIRE_FALSE(base.DrawLine(POINT { 0, 0 }, POINT { 1, 1 }, 0));
    REQUIRE_FALSE(base.Flash(RECT { 0, 0, 1, 1 }, 0));
}

// Every guarded entry point, called on objects that own no surface or
// resource: the guard must dispatch exactly one draw and return before the
// body runs (each body dereferences g_pChitin or a locked surface, which a
// test process does not have).
TEST_CASE("every_entry_point_dispatches", "[render]")
{
    const CRect clip(1, 2, 300, 400);

    SECTION("CVidCell::Render 0x7AEAD0")
    {
        CVidCell cell;
        cell.m_nCurrentFrame = 3;
        const rec::Cell c = DispatchOnce<rec::Cell>([&] { REQUIRE(cell.Render(0, 10, 20, clip, nullptr, 0, 0x4, 7)); });
        REQUIRE(c.cell == &cell);
        REQUIRE(c.frame == 3);
        REQUIRE(SamePoint(c.pos, 10, 20));
        REQUIRE(c.palette == &cell.m_cPalette);
        REQUIRE(c.flags == 0x4);
        REQUIRE(c.transVal == 7);
        REQUIRE(SameRect(c.clip, clip));
    }

    SECTION("CVidTile::Render 0x7B1780 and 0x7B1B30")
    {
        CVidTile tile;
        const rec::Tile t = DispatchOnce<rec::Tile>([&] { REQUIRE(tile.Render(nullptr, 64, 32, clip, 0x2)); });
        REQUIRE(t.tile == &tile);
        REQUIRE(SamePoint(t.pos, 64, 32));
        REQUIRE(t.flags == 0x2);
        REQUIRE(SameRect(t.clip, clip));

        const rec::Tile s = DispatchOnce<rec::Tile>([&] { REQUIRE(tile.Render(nullptr, 0, 64, nullptr, clip, 0)); });
        REQUIRE(SamePoint(s.pos, 0, 64));
    }

    SECTION("CVidFont::TextOut 0x793720")
    {
        CVidFont font;
        const rec::Text t = DispatchOnce<rec::Text>([&] { REQUIRE(font.TextOut(CString("Start"), 5, 6, clip, 0)); });
        REQUIRE(t.font == &font);
        REQUIRE(t.text == "Start");
        REQUIRE(SamePoint(t.pos, 5, 6));
        REQUIRE(SameRect(t.clip, clip));
    }

    SECTION("CVidMode drawing")
    {
        CVidInf inf;
        const rec::Line l = DispatchOnce<rec::Line>([&] { REQUIRE(inf.CVidMode::DrawLine(1, 2, 3, 4, 0, clip, RGB(1, 2, 3))); });
        REQUIRE(SamePoint(l.a, 1, 2));
        REQUIRE(SamePoint(l.b, 3, 4));
        REQUIRE(l.colour == RGB(1, 2, 3));
        REQUIRE(SameRect(l.clip, clip));

        const rec::Rect r = DispatchOnce<rec::Rect>([&] { REQUIRE(inf.CVidMode::DrawRect(CRect(5, 6, 7, 8), 0, clip, RGB(4, 5, 6))); });
        REQUIRE(SameRect(r.rect, CRect(5, 6, 7, 8)));
        REQUIRE_FALSE(r.fill);

        const rec::Ellipse e = DispatchOnce<rec::Ellipse>([&] { REQUIRE(inf.CVidMode::DrawEllipse(CPoint(50, 60), CSize(7, 5), clip, RGB(0, 255, 0))); });
        REQUIRE(SamePoint(e.centre, 50, 60));
        REQUIRE((e.axes.cx == 7 && e.axes.cy == 5));

        CVIDMODE_RECTICLE_DESCRIPTION rd {};
        rd.ptCenter = CPoint(70, 80);
        rd.xAxis = 9;
        const rec::Recticle rt = DispatchOnce<rec::Recticle>([&] { REQUIRE(inf.CVidMode::DrawRecticle(rd, clip, RGB(9, 9, 9))); });
        REQUIRE(SamePoint(rt.desc.ptCenter, 70, 80));
        REQUIRE(rt.desc.xAxis == 9);

        // OutlinePoly takes world points and the world-to-screen offset.
        const CPoint poly[3] = { CPoint(110, 120), CPoint(130, 120), CPoint(120, 140) };
        const rec::Poly p = DispatchOnce<rec::Poly>([&] { REQUIRE(inf.CVidMode::OutlinePoly(poly, 3, clip, RGB(1, 1, 1), CPoint(100, 100))); });
        REQUIRE(p.points.size() == 3);
        REQUIRE(SamePoint(p.points[0], 10, 20));
        REQUIRE(SamePoint(p.points[2], 20, 40));
        REQUIRE_FALSE(p.fill);

        CRect surface(clip);
        POINT line[2] = { { 0, 0 }, { 10, 10 } };
        const rec::PolyLine pl = DispatchOnce<rec::PolyLine>([&] { REQUIRE(inf.PolyLine(0, surface, line, 2, RGB(0, 50, 200), 5)); });
        REQUIRE(pl.points.size() == 2);
        REQUIRE(pl.thickness == 5);

        const rec::Rect erase = DispatchOnce<rec::Rect>([&] { REQUIRE(inf.CVidMode::EraseScreen(0, RGB(0, 0, 0))); });
        REQUIRE(erase.fill);
        REQUIRE(SameRect(erase.rect, CRect(0, 0, CVideo::SCREENWIDTH, CVideo::SCREENHEIGHT)));
    }

    SECTION("CVidMode::UpdateFade reports the level and still advances")
    {
        CVidInf inf;
        inf.SetFade(TRUE, 0);
        const int before = inf.m_nFade;
        const rec::Fade f = DispatchOnce<rec::Fade>([&] { inf.UpdateFade(); });
        REQUIRE(f.amount == before);
        REQUIRE(inf.m_nFade == before - 1);
    }

    SECTION("CVidInf FX")
    {
        CVidInf inf;
        inf.m_rLockedRect = CRect(0, 0, 128, 64);
        CVidCell cell;

        const rec::Cell a = DispatchOnce<rec::Cell>([&] { REQUIRE(inf.FXRender(&cell, 12, 13, 0, 50)); });
        REQUIRE(SamePoint(a.pos, 12, 13));
        REQUIRE(a.transVal == 50);
        REQUIRE(SameRect(a.clip, inf.m_rLockedRect));

        const rec::Cell b = DispatchOnce<rec::Cell>([&] { REQUIRE(inf.FXRender(&cell, 14, 15, clip, TRUE, 0)); });
        REQUIRE(SamePoint(b.pos, 14, 15));
        REQUIRE(SameRect(b.clip, clip));

        CParticle particle;
        const rec::Particle p = DispatchOnce<rec::Particle>([&] { REQUIRE(inf.FXRender(&particle, clip, 1, 2)); });
        REQUIRE(p.particle == &particle);
        REQUIRE(SamePoint(p.origin, 0, 0));
        REQUIRE(p.blobSize == 2);

        CPoint dots[2] = { CPoint(1, 1), CPoint(2, 2) };
        const rec::Points d = DispatchOnce<rec::Points>([&] { REQUIRE(inf.FXRender(dots, 2, clip, RGB(1, 1, 1), FALSE)); });
        REQUIRE(d.points.size() == 2);

        CVidFont font;
        const rec::Text t = DispatchOnce<rec::Text>([&] { REQUIRE(inf.FXTextOut(&font, CString("fx"), 3, 4, clip, 0x1, TRUE)); });
        REQUIRE(t.text == "fx");
        REQUIRE(SamePoint(t.pos, 3, 4));
    }

    SECTION("CVidInf BK, in back-buffer coordinates")
    {
        CVidInf inf;
        inf.m_rLockedRect = CRect(100, 200, 400, 500);
        const CRect local(0, 0, 50, 60);
        const CRect absolute(100, 200, 150, 260);
        CVidCell cell;

        const rec::Cell a = DispatchOnce<rec::Cell>([&] { REQUIRE(inf.BKRender(&cell, 5, 6, local, TRUE, 0)); });
        REQUIRE(SamePoint(a.pos, 105, 206));
        REQUIRE(SameRect(a.clip, absolute));

        const rec::Cell b = DispatchOnce<rec::Cell>([&] { REQUIRE(inf.BKRender(&cell, 7, 8, 0, 20)); });
        REQUIRE(SamePoint(b.pos, 107, 208));
        REQUIRE(b.transVal == 20);
        REQUIRE(SameRect(b.clip, inf.m_rLockedRect));

        CParticle particle;
        const rec::Particle p = DispatchOnce<rec::Particle>([&] { REQUIRE(inf.BKRender(&particle, local, 1, 3)); });
        REQUIRE(SamePoint(p.origin, 100, 200));
        REQUIRE(SameRect(p.clip, absolute));

        const rec::Ellipse e = DispatchOnce<rec::Ellipse>([&] { REQUIRE(inf.BKRenderEllipse(CPoint(10, 10), CSize(4, 3), local, RGB(0, 0, 255))); });
        REQUIRE(SamePoint(e.centre, 110, 210));

        const rec::Line l = DispatchOnce<rec::Line>([&] { REQUIRE(inf.BKRenderLine(0, 0, 9, 9, local, RGB(1, 1, 1))); });
        REQUIRE(SamePoint(l.a, 100, 200));
        REQUIRE(SamePoint(l.b, 109, 209));

        // Convex polygons are absolute already (CUIPanel passes screen
        // vertices and the locked rectangle as the clip).
        CVIDPOLY_VERTEX verts[4] = { { 100, 200 }, { 100, 260 }, { 150, 260 }, { 150, 200 } };
        CVidPoly poly;
        poly.SetPoly(verts, 4);
        const rec::Poly cp = DispatchOnce<rec::Poly>([&] { REQUIRE(inf.BKRenderConvexPoly(&poly, absolute, 0, 0x2, absolute.TopLeft())); });
        REQUIRE(cp.points.size() == 4);
        REQUIRE(SamePoint(cp.points[2], 150, 260));
        REQUIRE(cp.fill);
        REQUIRE(SameRect(cp.clip, absolute));

        CVidFont font;
        const rec::Text t = DispatchOnce<rec::Text>([&] { REQUIRE(inf.BKTextOut(&font, CString("bk"), 1, 2, local, 0, TRUE)); });
        REQUIRE(SamePoint(t.pos, 101, 202));
        REQUIRE(SameRect(t.clip, absolute));
    }

    SECTION("CVidInf::RenderFlash 0x79F520")
    {
        CVidInf inf;
        const rec::Flash f = DispatchOnce<rec::Flash>([&] { inf.RenderFlash(0, RGB(255, 255, 255), 0, clip); });
        REQUIRE(SameRect(f.rect, clip));
        REQUIRE(f.colour == RGB(255, 255, 255));
    }

    SECTION("CVidMosaic::Render 0x7B0C80")
    {
        CVidMosaic mosaic;
        const CRect src(0, 0, 640, 480);
        const rec::Mosaic m = DispatchOnce<rec::Mosaic>([&] { REQUIRE(mosaic.Render(0, 80, 60, src, clip, 0, TRUE)); });
        REQUIRE(m.mosaic == &mosaic);
        REQUIRE(SamePoint(m.pos, 80, 60));
        REQUIRE(SameRect(m.src, src));
        REQUIRE(SameRect(m.clip, clip));
    }

    SECTION("CVidBitmap::RenderDirect 0x7B3190")
    {
        CVidBitmap bitmap;
        const rec::Bitmap m = DispatchOnce<rec::Bitmap>([&] { REQUIRE(bitmap.RenderDirect(0, 3, 4, clip, 0, TRUE)); });
        REQUIRE(m.bitmap == &bitmap);
        REQUIRE(SamePoint(m.pos, 3, 4));
    }

    SECTION("CVidInf::RenderPointerImage reaches the backend through CVidCell::Render")
    {
        // Not guarded itself (api.h): with no count to draw it renders the
        // pointer cell once, which dispatches.
        CVidInf inf;
        CVidCell pointer;
        const rec::Cell c = DispatchOnce<rec::Cell>([&] { inf.RenderPointerImage(&pointer, 0, 0, 30, 40, clip); });
        REQUIRE(c.cell == &pointer);
        REQUIRE(SamePoint(c.pos, 30, 40));
    }
}

TEST_CASE("recording_backend_receives_start_screen_draws", "[render][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    const std::string data = env;

    // The screen the engine will use, as smoke_start_screen.cpp resolves it
    // (headless, the dummy video driver reports a 1024x768 desktop).
    const std::string ini = (std::filesystem::path(data) / "icewind2.ini").string();
    char resolution[64];
    char height[64];
    char scale[64];
    GetPrivateProfileStringA("Program Options", "Resolution", "800", resolution, sizeof(resolution), ini.c_str());
    GetPrivateProfileStringA("Program Options", "Height", "", height, sizeof(height), ini.c_str());
    GetPrivateProfileStringA("Program Options", "Scale", "1", scale, sizeof(scale), ini.c_str());
    const iwd2r::ScreenSize screen = iwd2r::ResolveScreenSize(iwd2r::ParseResolution(resolution), iwd2r::ParseHeight(height), iwd2r::ParseScale(scale), 1024, 768);
    if (!screen.ok) {
        SKIP("[iwd2r-skip] icewind2.ini asks for a screen iwd2r refuses: " << screen.error);
    }

    WARN("The engine writes into the install directory when it runs (cache files, temp/), as the original game does on every launch; the INI files are restored afterwards.");

    // The engine runs in this process (the backend must be installed in the
    // process that draws) until the window pump's smoke countdown closes it.
    SetEnv("SDL_VIDEO_DRIVER", "dummy");
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    SetEnv("IWD2R_AUDIO", "null");
    SetEnv("IWD2R_SMOKE_FRAMES", "120");
    SetEnv("IWD2_DATA", data.c_str());

    const std::filesystem::path cwd = std::filesystem::current_path();
    rec::Scope scope;
    int rc = -1;
    {
        InstallIniBackup backup(data);
        char arg0[] = "iwd2r";
        char* argv[] = { arg0, nullptr };
        rc = iwd2r_main(1, argv);
    }
    std::filesystem::current_path(cwd);
    REQUIRE(rc == 0);

    const RECT whole { 0, 0, screen.width, screen.height };
    auto inside = [](const POINT& p, const RECT& r) { return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom; };

    // The background: the start screen's mosaic, at the screen's origin.
    const std::vector<rec::Mosaic> mosaics = scope.backend.Of<rec::Mosaic>();
    REQUIRE_FALSE(mosaics.empty());
    for (const rec::Mosaic& m : mosaics)
        REQUIRE(inside(m.pos, whole));

    // The panel's buttons: each control draws its BAM with the control's
    // rectangle as the clip, so a cell is inside its clip and the clip inside
    // the screen. The start panel has seven buttons (GUISTART.CHU).
    const std::vector<rec::Cell> cells = scope.backend.Of<rec::Cell>();
    REQUIRE_FALSE(cells.empty());
    std::set<std::tuple<std::string, LONG, LONG, LONG, LONG>> buttons;
    for (const rec::Cell& c : cells) {
        INFO(c.resref << " at " << c.pos.x << "," << c.pos.y);
        REQUIRE(inside(c.pos, whole));
        REQUIRE(inside(c.pos, c.clip));
        REQUIRE(c.clip.left >= 0);
        REQUIRE(c.clip.top >= 0);
        REQUIRE(c.clip.right <= whole.right);
        REQUIRE(c.clip.bottom <= whole.bottom);
        if (c.clip.right - c.clip.left < whole.right)
            buttons.insert({ c.resref, c.clip.left, c.clip.top, c.clip.right, c.clip.bottom });
    }
    REQUIRE(buttons.size() >= 7);

    // The labels: every string on the screen, and at least one inside a
    // button's rectangle (the button captions).
    const std::vector<rec::Text> texts = scope.backend.Of<rec::Text>();
    REQUIRE_FALSE(texts.empty());
    bool captioned = false;
    for (const rec::Text& t : texts) {
        INFO(t.text << " at " << t.pos.x << "," << t.pos.y);
        REQUIRE_FALSE(t.text.empty());
        REQUIRE(inside(t.pos, whole));
        for (const auto& b : buttons)
            if (inside(t.pos, RECT { std::get<1>(b), std::get<2>(b), std::get<3>(b), std::get<4>(b) }))
                captioned = true;
    }
    REQUIRE(captioned);
}
