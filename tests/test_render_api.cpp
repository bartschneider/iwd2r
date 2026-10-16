#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
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
#include "data_view.h"
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

TEST_CASE("cell_guard_tail_with_no_frame", "[render]")
{
    // A handled cell draw whose caller had not fetched a frame leaves nothing
    // to release (the body's own GetFrame did not run); the release path is
    // covered with real resources by the start-screen case.
    CVidCell cell;
    REQUIRE(cell.m_pFrame == NULL);
    REQUIRE(render::CellHandled(cell) == TRUE);
    REQUIRE(render::CellHandled(cell, FALSE) == TRUE);
    REQUIRE(cell.m_pFrame == NULL);
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

    SECTION("CVidMode::SetFade and UpdateFade report the level after the change")
    {
        CVidInf inf;
        const rec::Fade set = DispatchOnce<rec::Fade>([&] { inf.SetFade(TRUE, 0); });
        REQUIRE(set.amount == CVidMode::NUM_FADE_FRAMES);
        REQUIRE(inf.m_nFade == CVidMode::NUM_FADE_FRAMES);
        const rec::Fade step = DispatchOnce<rec::Fade>([&] { inf.UpdateFade(); });
        REQUIRE(inf.m_nFade == CVidMode::NUM_FADE_FRAMES - 1);
        REQUIRE(step.amount == inf.m_nFade);
        const rec::Fade in = DispatchOnce<rec::Fade>([&] { inf.SetFade(FALSE, 0); });
        REQUIRE(in.amount == 0);
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

        // The polygon fill (0x79D680) of the container and door
        // highlights: FX-surface coordinates, filled, with its flags.
        CVIDPOLY_VERTEX verts[3] = { { 10, 10 }, { 40, 10 }, { 25, 30 } };
        CVidPoly poly;
        poly.SetPoly(verts, 3);
        const rec::Poly f = DispatchOnce<rec::Poly>([&] { REQUIRE(inf.FXRender(&poly, clip, RGB(0, 250, 0), 0x10, CPoint(0, 0), FALSE)); });
        REQUIRE(f.points.size() == 3);
        REQUIRE(SamePoint(f.points[2], 25, 30));
        REQUIRE(f.fill);
        REQUIRE(f.flags == 0x10);
        REQUIRE(SameRect(f.clip, clip));
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
        REQUIRE(cp.flags == 0x2);
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

namespace {

// While the engine draws the start screen, the first time a control's button
// cell (a real CVidCell with a loaded resource) reaches the backend, this
// backend calls the guarded cell entry points on it itself and checks their
// tails: afterwards the cell's m_pFrame is NULL and its resource's demand
// count (CRes::field_40, which CDimm::Demand raises and CDimm::Release
// lowers) is what the body would have left. The probe restores the cell as
// it found it (no frame fetched), so the engine's own draw continues as
// before.
class TailCheckingBackend : public rec::Backend {
public:
    int probes = 0;
    std::vector<std::string> failures;

    bool DrawCell(const CVidCell& cell, int frame, POINT pos, const CVidPalette* palette, DWORD flags, int transVal) override
    {
        if (m_probing) {
            m_nested++;
            return true;
        }
        rec::Backend::DrawCell(cell, frame, pos, palette, flags, transVal);
        if (probes == 0 && cell.pRes != NULL && cell.m_pFrame == NULL && std::get<rec::Cell>(calls.back()).resref == "GBTNMED2")
            Probe(const_cast<CVidCell&>(cell));
        return true;
    }

private:
    // name: the entry point; fetch: how the caller fetched the frame before
    // the call (0 none, 1 GetFrame(FALSE), 2 the caller's own Demand and
    // GetFrame(TRUE)); expected: the demand count after, relative to before.
    template <typename F>
    void Run(const char* name, CVidCell& cell, int fetch, int expected, F&& call)
    {
        const int before = cell.pRes->field_40;
        if (fetch == 1)
            cell.GetFrame(FALSE);
        if (fetch == 2) {
            cell.pRes->Demand();
            cell.GetFrame(TRUE);
        }
        m_nested = 0;
        call();
        const int after = cell.pRes->field_40;
        if (m_nested != 1)
            failures.push_back(std::string(name) + ": dispatched " + std::to_string(m_nested) + " times");
        if (cell.m_pFrame != NULL)
            failures.push_back(std::string(name) + ": m_pFrame left set");
        if (after - before != expected)
            failures.push_back(std::string(name) + " (fetch " + std::to_string(fetch) + "): demand count " + std::to_string(before) + " -> " + std::to_string(after));
        // Give back the caller's own demand (fetch 2), as that caller would.
        if (fetch == 2)
            cell.pRes->Release();
        cell.m_pFrame = NULL;
    }

    void Probe(CVidCell& cell)
    {
        probes++;
        m_probing = true;
        CVidInf* inf = static_cast<CVidInf*>(g_pChitin->GetCurrentVideoMode());
        const CRect clip(0, 0, 100, 100);
        for (int fetch = 0; fetch < 2; fetch++) {
            Run("CVidCell::Render", cell, fetch, 0, [&] { cell.Render(0, 10, 10, clip, NULL, 0, 0, -1); });
            Run("BKRender", cell, fetch, 0, [&] { inf->BKRender(&cell, 10, 10, 0, -1); });
            Run("FXRender", cell, fetch, 0, [&] { inf->FXRender(&cell, 10, 10, 0, -1); });
            Run("BKRender clip", cell, fetch, 0, [&] { inf->BKRender(&cell, 10, 10, clip, FALSE, 0); });
            Run("FXRender clip", cell, fetch, 0, [&] { inf->FXRender(&cell, 10, 10, clip, FALSE, 0); });
        }
        // bDemanded: the caller holds the demand, the body keeps it.
        Run("BKRender clip demanded", cell, 2, 1, [&] { inf->BKRender(&cell, 10, 10, clip, TRUE, 0); });
        Run("FXRender clip demanded", cell, 2, 1, [&] { inf->FXRender(&cell, 10, 10, clip, TRUE, 0); });
        m_probing = false;
    }

    bool m_probing = false;
    int m_nested = 0;
};

}

TEST_CASE("recording_backend_receives_start_screen_draws", "[render][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    // A private view of the data: the engine writes cache and temp files
    // as the original does, and parallel runs must not share them.
    DataView view(env);
    const std::string data = view.Path();

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

    // The engine runs in this process (the backend must be installed in the
    // process that draws) until the window pump's smoke countdown closes it.
    SetEnv("SDL_VIDEO_DRIVER", "dummy");
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    SetEnv("IWD2R_AUDIO", "null");
    SetEnv("IWD2R_SMOKE_FRAMES", "120");
    SetEnv("IWD2_DATA", data.c_str());

    const std::filesystem::path cwd = std::filesystem::current_path();
    TailCheckingBackend backend;
    render::Backend* previous = render::Set(&backend);
    int rc = -1;
    {
        char arg0[] = "iwd2r";
        char* argv[] = { arg0, nullptr };
        rc = iwd2r_main(1, argv);
    }
    std::filesystem::current_path(cwd);
    render::Set(previous);
    REQUIRE(rc == 0);

    // The handled cell draws left their frames and demands as the bodies
    // would have.
    REQUIRE(backend.probes == 1);
    for (const std::string& failure : backend.failures)
        FAIL_CHECK(failure);
    REQUIRE(backend.failures.empty());

    const RECT whole { 0, 0, screen.width, screen.height };
    auto inside = [](const POINT& p, const RECT& r) { return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom; };

    // The background: the start screen's mosaic, at the screen's origin.
    const std::vector<rec::Mosaic> mosaics = backend.Of<rec::Mosaic>();
    REQUIRE_FALSE(mosaics.empty());
    for (const rec::Mosaic& m : mosaics)
        REQUIRE(inside(m.pos, whole));

    // The panel's buttons: each control draws its BAM with the control's
    // rectangle as the clip, so a cell is inside its clip and the clip inside
    // the screen. The start panel has seven buttons (GUISTART.CHU).
    const std::vector<rec::Cell> cells = backend.Of<rec::Cell>();
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
    const std::vector<rec::Text> texts = backend.Of<rec::Text>();
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
