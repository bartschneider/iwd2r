// Scalable text, task 1: the text-only capture backend (render/text_capture)
// and the back buffer's text layer (platform/text_layer).
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CResRef.h"
#include "CVidFont.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "compat/ini.h"
#include "data_view.h"
#include "main.h"
#include "platform/text_fonts.h"
#include "platform/text_layer.h"
#include "render/api.h"
#include "render/recording.h"
#include "render/text_capture.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace text = platform::text;

namespace {

void SetEnv(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void UnsetEnv(const char* name)
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

RECT R(LONG l, LONG t, LONG r, LONG b)
{
    RECT x;
    x.left = l;
    x.top = t;
    x.right = r;
    x.bottom = b;
    return x;
}

text::Glyph At(RECT box)
{
    text::Glyph g;
    g.cell = box;
    g.box = box;
    return g;
}

bool Same(const RECT& a, const RECT& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool Within(const RECT& inner, const RECT& outer)
{
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right && inner.bottom <= outer.bottom;
}

// Outside any rectangle: an empty bounding box starts here.
const LONG kBig = 1 << 30;

// The bitmap fonts the face table maps (platform/text_fonts.cpp).
const char* const kMapped[] = { "NORMAL", "STONESML", "STONESM2", "STONEBIG", "TOOLFONT", "REALMS" };

}

TEST_CASE("text_layer_tags", "[text]")
{
    text::Layer layer;
    SECTION("tags are never the engine's own X bytes")
    {
        REQUIRE_FALSE(text::IsTag(0x00));
        REQUIRE_FALSE(text::IsTag(0xFF));
        REQUIRE_FALSE(text::IsTag(0x7F));
        REQUIRE_FALSE(text::IsTag(0x80));
        REQUIRE(text::IsTag(0x01));
        REQUIRE(text::IsTag(0xFE));
        REQUIRE(text::TagOf(0x2A123456u) == 0x2A);
    }
    SECTION("overlapping glyphs get different tags; apart they share one")
    {
        const uint8_t a = layer.Add(At(R(0, 0, 10, 12)));
        const uint8_t b = layer.Add(At(R(5, 6, 15, 18)));
        const uint8_t c = layer.Add(At(R(100, 0, 110, 12)));
        REQUIRE(a != 0);
        REQUIRE(b != 0);
        REQUIRE(a != b);
        REQUIRE(c == a);
        REQUIRE(layer.Size() == 3);
    }
    SECTION("a glyph drawn over another's whole box replaces it")
    {
        layer.Add(At(R(0, 0, 10, 12)));
        std::vector<text::Glyph> kept;
        const uint8_t t = layer.Add(At(R(0, 0, 10, 12)), &kept);
        REQUIRE(t != 0);
        REQUIRE(kept.empty());
        REQUIRE(layer.Size() == 1);
        // Partly covered: the old glyph stays and is reported.
        layer.Add(At(R(8, 0, 18, 12)), &kept);
        REQUIRE(kept.size() == 1);
        REQUIRE(layer.Size() == 2);
    }
    SECTION("an empty box is not kept")
    {
        REQUIRE(layer.Add(At(R(5, 5, 5, 9))) == 0);
        REQUIRE(layer.Size() == 0);
    }
    SECTION("glyphs come back in the order they were added")
    {
        layer.Add(At(R(50, 0, 60, 10)));
        layer.Add(At(R(0, 0, 10, 10)));
        const std::vector<text::Glyph> glyphs = layer.Glyphs();
        REQUIRE(glyphs.size() == 2);
        REQUIRE(glyphs[0].box.left == 50);
        REQUIRE(glyphs[1].box.left == 0);
        REQUIRE(glyphs[0].serial < glyphs[1].serial);
    }
    SECTION("a large layer forgets glyphs not shown for a long time")
    {
        for (int i = 0; i < 5000; i++)
            layer.Add(At(R(i * 10, 0, i * 10 + 10, 10)));
        const std::vector<text::Glyph> all = layer.Glyphs();
        std::vector<uint32_t> shown;
        for (size_t i = 0; i < 100; i++)
            shown.push_back(all[i].serial);
        for (int frame = 0; frame < 1802; frame++)
            layer.Presented(shown);
        REQUIRE(layer.Size() == 100);
    }
}

TEST_CASE("text_options_parse", "[text]")
{
    bool scalable = true;
    REQUIRE(text::ParseRenderer(" Classic ", &scalable));
    REQUIRE_FALSE(scalable);
    REQUIRE(text::ParseRenderer("scalable", &scalable));
    REQUIRE(scalable);
    REQUIRE_FALSE(text::ParseRenderer("ttf", &scalable));
    float scale = 0.0f;
    REQUIRE(text::ParseFontScale("1.25", &scale));
    REQUIRE(scale == 1.25f);
    REQUIRE_FALSE(text::ParseFontScale("3", &scale));
    REQUIRE_FALSE(text::ParseFontScale("big", &scale));
    REQUIRE_FALSE(text::ParseFontScale("", &scale));
}

namespace {

// Records the cells CVidFont::TextOut (0x793720) draws: each character is
// one CVidCell::Render(nSurface, ...), whose guard reports the reference
// point; the frame is drawn from there minus its centre point.
struct CellRecorder : render::Backend {
    std::vector<RECT> rects;
    bool DrawCell(const CVidCell& constCell, int, POINT pos, const CVidPalette*, DWORD, int) override
    {
        CVidCell& cell = const_cast<CVidCell&>(constCell);
        CSize size;
        cell.GetCurrentFrameSize(size, FALSE);
        CPoint centre;
        cell.GetCurrentCenterPoint(centre, FALSE);
        rects.push_back(R(pos.x - centre.x, pos.y - centre.y, pos.x - centre.x + size.cx, pos.y - centre.y + size.cy));
        return true;
    }
};

// The capture, wrapped: on the first draw of the run (the engine is up,
// resources load) it checks the cells against the bitmap path's own
// drawing for every mapped font, then captures as the real backend does.
struct ParityBackend : render::Backend {
    render::text_capture::Backend capture;
    bool ran = false;
    int fonts = 0;
    int checked = 0;
    int inked = 0;
    unsigned long capturedBefore = 0;
    std::vector<std::string> failures;

    void SetClip(const RECT& clip) override { capture.SetClip(clip); }
    bool DrawText(const CVidFont& font, const char* t, POINT pos, DWORD flags) override
    {
        if (!ran) {
            ran = true;
            Run();
        }
        return capture.DrawText(font, t, pos, flags);
    }

    void Fail(const std::string& what) { failures.push_back(what); }

    void Run()
    {
        CVidInf* inf = static_cast<CVidInf*>(g_pChitin->GetCurrentVideoMode());
        // The first DrawText comes from inside a BKLock: put the engine's
        // lock aside while this check locks and draws on its own.
        const DDSURFACEDESC savedDesc = inf->m_SurfaceDesc;
        const CRect savedRect = inf->m_rLockedRect;
        inf->m_SurfaceDesc.lpSurface = NULL;
        for (const char* resref : kMapped) {
            CVidFont font;
            font.SetResRef(CResRef(resref), FALSE, TRUE);
            if (font.pRes == NULL) {
                Fail(std::string(resref) + ": not loaded");
                continue;
            }
            fonts++;
            CheckTextOut(resref, font);
            for (int byte = 0x20; byte <= 0xFF; byte++)
                CheckBk(resref, font, inf, static_cast<unsigned char>(byte));
        }
        inf->m_SurfaceDesc = savedDesc;
        inf->m_rLockedRect = savedRect;
        render::Set(this);
        capturedBefore = render::text_capture::CapturedGlyphs();
    }

    // Every byte 0x20-0xFF in one string: the cells equal the frames
    // CVidFont::TextOut renders, in number, place and size.
    void CheckTextOut(const char* resref, CVidFont& font)
    {
        std::string all;
        for (int byte = 0x20; byte <= 0xFF; byte++)
            all.push_back(static_cast<char>(byte));
        const POINT pos { 10, 40 };
        const std::vector<render::text_capture::Cell> cells = render::text_capture::Cells(font, all.c_str(), pos);
        CellRecorder recorder;
        render::Set(&recorder);
        font.TextOut(CString(all.c_str()), pos.x, pos.y, CRect(0, 0, 4000, 200), CVIDINF_SURFACE_BACK);
        render::Set(this);
        if (cells.size() != all.size() || recorder.rects.size() != all.size()) {
            Fail(std::string(resref) + ": " + std::to_string(cells.size()) + " cells, " + std::to_string(recorder.rects.size()) + " frames drawn");
            return;
        }
        for (size_t i = 0; i < cells.size(); i++) {
            if (!Same(cells[i].rect, recorder.rects[i]))
                Fail(std::string(resref) + ": byte " + std::to_string(0x20 + i) + " cell differs from the frame drawn");
        }
    }

    // One character through CVidInf::BKTextOut (0x79E340): classic draws
    // inside the character's cell; the capture changes no colour and tags
    // only pixels of that cell.
    void CheckBk(const char* resref, CVidFont& font, CVidInf* inf, unsigned char byte)
    {
        const RECT region = R(200, 100, 264, 164);
        const char str[2] = { static_cast<char>(byte), '\0' };
        const POINT local { 8, 40 };
        const std::vector<render::text_capture::Cell> cells = render::text_capture::Cells(font, str, POINT { region.left + local.x, region.top + local.y });
        RECT cell = cells.at(0).rect;
        cell.left = std::max(cell.left, region.left);
        cell.top = std::max(cell.top, region.top);
        cell.right = std::min(cell.right, region.right);
        cell.bottom = std::min(cell.bottom, region.bottom);
        const uint32_t sentinel = 0x00123456u;
        IDirectDrawSurface* back = inf->pSurfaces[CVIDINF_SURFACE_BACK];

        auto fill = [&] {
            DDSURFACEDESC d {};
            d.dwSize = sizeof(d);
            back->Lock(NULL, &d, DDLOCK_WAIT, NULL);
            for (LONG y = region.top; y < region.bottom; y++)
                for (LONG x = region.left; x < region.right; x++)
                    static_cast<uint32_t*>(d.lpSurface)[y * (d.lPitch / 4) + x] = sentinel;
            back->Unlock(d.lpSurface);
        };
        auto draw = [&](render::Backend* backend) {
            render::Set(backend);
            CRect rBack(region.left, region.top, region.right, region.bottom);
            inf->BKLock(rBack);
            inf->BKTextOut(&font, CString(str), local.x, local.y, CRect(0, 0, 64, 64), 0, FALSE);
            inf->BKUnlock();
            render::Set(this);
        };
        // (changed RGB bbox, tagged bbox)
        auto scan = [&](RECT* changed, RECT* tagged) {
            *changed = R(kBig, kBig, -kBig, -kBig);
            *tagged = *changed;
            DDSURFACEDESC d {};
            d.dwSize = sizeof(d);
            back->Lock(NULL, &d, DDLOCK_WAIT, NULL);
            for (LONG y = region.top; y < region.bottom; y++) {
                for (LONG x = region.left; x < region.right; x++) {
                    const uint32_t p = static_cast<uint32_t*>(d.lpSurface)[y * (d.lPitch / 4) + x];
                    RECT* r = nullptr;
                    if ((p & 0xFFFFFFu) != sentinel)
                        r = changed;
                    else if (text::IsTag(text::TagOf(p)))
                        r = tagged;
                    if (r == nullptr)
                        continue;
                    r->left = std::min(r->left, x);
                    r->top = std::min(r->top, y);
                    r->right = std::max(r->right, x + 1);
                    r->bottom = std::max(r->bottom, y + 1);
                }
            }
            back->Unlock(d.lpSurface);
        };
        const std::string what = std::string(resref) + ": byte " + std::to_string(byte);
        checked++;

        RECT changed;
        RECT tagged;
        fill();
        draw(nullptr);
        scan(&changed, &tagged);
        const bool ink = changed.left != kBig;
        if (ink) {
            inked++;
            if (!Within(changed, cell))
                Fail(what + ": the bitmap glyph leaves its cell");
        }

        fill();
        draw(&capture);
        scan(&changed, &tagged);
        if (changed.left != kBig)
            Fail(what + ": the capture drew pixels");
        if (ink && tagged.left == kBig)
            Fail(what + ": the capture tagged nothing");
        if (tagged.left != kBig && !Within(tagged, cell))
            Fail(what + ": the capture tagged outside the cell");
        text::BackLayer().Clear();
    }
};

struct OverrideEnv {
    explicit OverrideEnv(const char* value)
    {
        if (value)
            SetEnv("IWD2R_INI_OVERRIDE", value);
        else
            UnsetEnv("IWD2R_INI_OVERRIDE");
        compat::ini::ResetOverridesForTesting();
    }
    ~OverrideEnv()
    {
        UnsetEnv("IWD2R_INI_OVERRIDE");
        compat::ini::ResetOverridesForTesting();
    }
};

// Runs the engine in this process on a private view of the data until the
// smoke countdown closes it; false when there is no data (the test skips).
int RunEngine(const std::string& data, const char* frames)
{
    SetEnv("SDL_VIDEO_DRIVER", "dummy");
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    SetEnv("IWD2R_AUDIO", "null");
    SetEnv("IWD2R_SMOKE_FRAMES", frames);
    const char* old = std::getenv("IWD2_DATA");
    const std::string saved = old ? old : "";
    SetEnv("IWD2_DATA", data.c_str());
    const std::filesystem::path cwd = std::filesystem::current_path();
    char arg0[] = "iwd2r";
    char* argv[] = { arg0, nullptr };
    const int rc = iwd2r_main(1, argv);
    std::filesystem::current_path(cwd);
    if (saved.empty())
        UnsetEnv("IWD2_DATA");
    else
        SetEnv("IWD2_DATA", saved.c_str());
    return rc;
}

}

TEST_CASE("text_capture_cells_match_the_bitmap_path", "[text][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    DataView view(env);
    OverrideEnv overrides(nullptr);

    ParityBackend backend;
    text::SetEnabled(true);
    render::Backend* previous = render::Set(&backend);
    const int rc = RunEngine(view.Path(), "60");
    render::Set(previous);
    text::SetEnabled(false);
    text::BackLayer().Clear();
    REQUIRE(rc == 0);

    REQUIRE(backend.ran);
    for (const std::string& failure : backend.failures)
        FAIL_CHECK(failure);
    REQUIRE(backend.failures.empty());
    REQUIRE(backend.fonts == static_cast<int>(sizeof(kMapped) / sizeof(kMapped[0])));
    REQUIRE(backend.checked == backend.fonts * (0x100 - 0x20));
    REQUIRE(backend.inked > backend.fonts * 90);
    // The start screen's captions were captured after the check.
    REQUIRE(render::text_capture::CapturedGlyphs() > backend.capturedBefore);
}

// One engine run per test case: the engine does not run twice in a process.
TEST_CASE("text_renderer_absent_installs_nothing", "[text][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    DataView view(env);
    OverrideEnv overrides(nullptr);
    render::recording::Scope scope;
    REQUIRE(RunEngine(view.Path(), "30") == 0);
    // The recording backend stayed in place: every caption reached it.
    REQUIRE_FALSE(scope.backend.Of<render::recording::Text>().empty());
    REQUIRE_FALSE(render::text_capture::Installed());
    REQUIRE_FALSE(text::Enabled());
}

TEST_CASE("text_renderer_classic_installs_nothing", "[text][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    DataView view(env);
    OverrideEnv overrides("Program Options/Text Renderer=classic");
    render::recording::Scope scope;
    REQUIRE(RunEngine(view.Path(), "30") == 0);
    REQUIRE_FALSE(scope.backend.Of<render::recording::Text>().empty());
    REQUIRE_FALSE(render::text_capture::Installed());
    REQUIRE_FALSE(text::Enabled());
}

TEST_CASE("text_renderer_scalable_captures", "[text][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    DataView view(env);
    OverrideEnv overrides("Program Options/Text Renderer=scalable;Program Options/Text Font Scale=1.5");
    const unsigned long before = render::text_capture::CapturedGlyphs();
    REQUIRE(RunEngine(view.Path(), "30") == 0);
    REQUIRE(render::text_capture::CapturedGlyphs() > before);
    REQUIRE(text::FontScale() == 1.5f);
    // The run's guard took the capture out again.
    REQUIRE_FALSE(render::text_capture::Installed());
    REQUIRE(render::Active() == nullptr);
    text::SetFontScale(1.0f);
}

TEST_CASE("text_toggle_swaps_the_capture", "[text]")
{
    // Without an engine (nothing to redraw): Ctrl+Shift+T's handler puts
    // the capture in and takes it out again, the layer with it.
    REQUIRE_FALSE(render::text_capture::Installed());
    render::Backend* before = render::Active();
    REQUIRE(render::text_capture::Toggle());
    REQUIRE(render::text_capture::Installed());
    REQUIRE(text::Enabled());
    REQUIRE(render::Active() != before);
    REQUIRE_FALSE(render::text_capture::Toggle());
    REQUIRE_FALSE(render::text_capture::Installed());
    REQUIRE_FALSE(text::Enabled());
    REQUIRE(render::Active() == before);
}
