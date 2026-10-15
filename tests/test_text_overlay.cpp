// Scalable text, task 3: the presenter's text overlay (platform/text_overlay)
// and both presenter backends drawing it.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/ini.h"
#include "data_view.h"
#include "main.h"
#include "platform/compose.h"
#include "platform/present.h"
#include "platform/present_backend.h"
#include "platform/text_fonts.h"
#include "platform/text_layer.h"
#include "platform/text_overlay.h"
#include "platform/window.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace text = platform::text;
namespace present = platform::present;

namespace {

void SetEnv(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value)
        setenv(name, value, 1);
    else
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

// A frame of w x h background pixels.
std::vector<uint32_t> Frame(int w, int h)
{
    return std::vector<uint32_t>(static_cast<size_t>(w) * static_cast<size_t>(h), 0x00204060u);
}

void Tag(std::vector<uint32_t>& frame, int w, const RECT& r, uint8_t tag)
{
    for (LONG y = r.top; y < r.bottom; y++)
        for (LONG x = r.left; x < r.right; x++) {
            uint32_t& p = frame[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
            p = (p & 0xFFFFFFu) | (static_cast<uint32_t>(tag) << 24);
        }
}

// An 'H' in the body face, as the capture records one: the bitmap cell
// (10,10)-(20,24), baseline 22, cap height 9.
text::Glyph H(const RECT& cell = R(10, 10, 20, 24), int baseline = 22)
{
    text::Glyph g;
    g.cell = cell;
    g.box = cell;
    g.clip = R(0, 0, 4000, 4000);
    g.baseline = baseline;
    g.capHeight = 9.0f;
    g.colour = 0xFFFFFF;
    g.outline = 0x000000;
    g.codepoint = 'H';
    g.face = static_cast<uint8_t>(text::FallbackFace());
    g.flags = text::kOutline;
    return g;
}

text::Placement Whole(int frameW, int frameH, float s)
{
    text::Placement p;
    p.frame = R(0, 0, frameW, frameH);
    p.x = 0.0f;
    p.y = 0.0f;
    p.sx = s;
    p.sy = s;
    p.clip = R(0, 0, static_cast<LONG>(static_cast<float>(frameW) * s), static_cast<LONG>(static_cast<float>(frameH) * s));
    return p;
}

// The bounding box of the overlay's painted pixels (alpha > 0) and their
// count.
RECT Painted(const text::Overlay& o, int* count = nullptr)
{
    RECT r = R(1 << 30, 1 << 30, -(1 << 30), -(1 << 30));
    int n = 0;
    for (int y = 0; y < o.height; y++)
        for (int x = 0; x < o.width; x++)
            if ((o.pixels[static_cast<size_t>(y) * static_cast<size_t>(o.width) + static_cast<size_t>(x)] >> 24) != 0) {
                n++;
                r.left = std::min<LONG>(r.left, x);
                r.top = std::min<LONG>(r.top, y);
                r.right = std::max<LONG>(r.right, x + 1);
                r.bottom = std::max<LONG>(r.bottom, y + 1);
            }
    if (count)
        *count = n;
    return r;
}

bool Within(const RECT& inner, const RECT& outer)
{
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right && inner.bottom <= outer.bottom;
}

}

TEST_CASE("text_overlay_region_assignment", "[text]")
{
    // The presenter's UI scaling: an 800x600 frame with a 100-row toolbar
    // at the bottom, the world at 1x and the UI at 1x to 4x.
    const SIZE frame { 800, 600 };
    for (int ui = 1; ui <= 4; ui++) {
        INFO("ui scale " << ui);
        const SIZE window { 800, 600 + 100 * (ui - 1) };
        const std::vector<platform::compose::Region> regions = platform::compose::Layout(frame, window, ui, 1, { R(0, 500, 800, 600) }, nullptr);
        std::vector<text::Placement> placements;
        for (const auto& r : regions) {
            text::Placement p;
            p.frame = r.frame;
            p.x = static_cast<float>(r.window.left);
            p.y = static_cast<float>(r.window.top);
            p.sx = static_cast<float>(r.scale);
            p.sy = static_cast<float>(r.scale);
            p.clip = r.window;
            p.ui = r.ui;
            placements.push_back(p);
        }
        // A caption in the toolbar goes to the UI region, a hit-point label
        // over a creature to the world region.
        const int bar = text::PlacementFor(placements, R(100, 540, 110, 552));
        REQUIRE(bar >= 0);
        REQUIRE(placements[static_cast<size_t>(bar)].ui == (ui != 1));
        const int world = text::PlacementFor(placements, R(300, 40, 310, 52));
        REQUIRE(world >= 0);
        REQUIRE_FALSE(placements[static_cast<size_t>(world)].ui);
        // Frame to window: the toolbar's pixel row 540 is ui * 40 rows below
        // the band's top.
        const text::Placement& p = placements[static_cast<size_t>(bar)];
        float x = 0.0f;
        float y = 0.0f;
        REQUIRE(platform::compose::FrameToWindow(regions, 100.0f, 540.0f, &x, &y));
        CHECK(std::fabs(p.x + (100.0f - static_cast<float>(p.frame.left)) * p.sx - x) < 0.01f);
        CHECK(std::fabs(p.y + (540.0f - static_cast<float>(p.frame.top)) * p.sy - y) < 0.01f);
    }
    // Outside every region: none.
    REQUIRE(text::PlacementFor({ Whole(100, 100, 1.0f) }, R(200, 200, 210, 210)) == -1);
}

TEST_CASE("text_overlay_draws_where_tags_survive", "[text]")
{
    const int fw = 100;
    const int fh = 60;
    for (int s = 1; s <= 4; s++) {
        INFO("scale " << s);
        text::Layer layer;
        text::Composer composer;
        std::vector<uint32_t> frame = Frame(fw, fh);
        const text::Glyph g = H();
        const uint8_t tag = layer.Add(g);
        REQUIRE(tag != 0);
        Tag(frame, fw, g.box, tag);
        const std::vector<text::Placement> placements { Whole(fw, fh, static_cast<float>(s)) };

        REQUIRE(composer.Compose(layer, frame.data(), fw, fh, fw, placements, fw * s, fh * s, 1.0f));
        const text::Overlay& o = composer.Result();
        REQUIRE(o.width == fw * s);
        REQUIRE(o.height == fh * s);
        REQUIRE_FALSE(o.empty);
        int full = 0;
        const RECT painted = Painted(o, &full);
        // Inside the glyph's reach: its cell's columns one pixel wider, two
        // cap heights above the baseline to one below.
        const RECT reach = R((g.cell.left - 1) * s, (g.baseline - 18) * s, (g.cell.right + 1) * s, (g.baseline + 9) * s);
        REQUIRE(Within(painted, reach));
        // The H is about cap height (times the scale) tall, plus the outline.
        CHECK(std::abs(static_cast<int>(painted.bottom - painted.top) - 9 * s) <= 3);
        // White at full coverage somewhere.
        bool white = false;
        for (uint32_t p : o.pixels)
            white = white || p == 0xFFFFFFFFu;
        CHECK(white);

        // The same frame again: nothing to redraw.
        REQUIRE_FALSE(composer.Compose(layer, frame.data(), fw, fh, fw, placements, fw * s, fh * s, 1.0f));

        // The engine draws over the right half of the cell: the glyph stays
        // only where its tag does.
        const LONG mid = (g.box.left + g.box.right) / 2;
        Tag(frame, fw, R(mid, g.box.top, g.box.right, g.box.bottom), 0xFF);
        REQUIRE(composer.Compose(layer, frame.data(), fw, fh, fw, placements, fw * s, fh * s, 1.0f));
        int half = 0;
        Painted(composer.Result(), &half);
        const text::Overlay& o2 = composer.Result();
        int hidden = 0;
        for (int y = 0; y < o2.height; y++)
            for (int x = 0; x < o2.width; x++) {
                const LONG fx = x / s;
                const LONG fy = y / s;
                if ((o2.pixels[static_cast<size_t>(y) * static_cast<size_t>(o2.width) + static_cast<size_t>(x)] >> 24) != 0
                    && fx >= mid && fx < g.box.right && fy >= g.box.top && fy < g.box.bottom)
                    hidden++;
            }
        CHECK(hidden == 0);
        CHECK(half < full);
        CHECK(half > 0);
        CHECK(composer.Result().dirty.right >= painted.right);

        // Drawn over entirely: nothing shows.
        Tag(frame, fw, g.box, 0x00);
        REQUIRE(composer.Compose(layer, frame.data(), fw, fh, fw, placements, fw * s, fh * s, 1.0f));
        REQUIRE(composer.Result().empty);
        int none = -1;
        Painted(composer.Result(), &none);
        REQUIRE(none == 0);
    }
}

TEST_CASE("text_overlay_follows_regions_and_scale_changes", "[text]")
{
    // A 200x100 frame: the world (top 80 rows) at 1x, a 20-row panel at 3x
    // below it in a 600x140 window.
    const int fw = 200;
    const int fh = 100;
    text::Layer layer;
    text::Composer composer;
    std::vector<uint32_t> frame = Frame(fw, fh);
    const text::Glyph inPanel = H(R(50, 82, 60, 96), 94);
    const text::Glyph inWorld = H(R(150, 10, 160, 24), 22);
    Tag(frame, fw, inPanel.box, layer.Add(inPanel));
    Tag(frame, fw, inWorld.box, layer.Add(inWorld));
    text::Placement world;
    world.frame = R(0, 0, 200, 80);
    world.clip = R(0, 0, 600, 80);
    text::Placement panel;
    panel.frame = R(0, 80, 200, 100);
    panel.y = 80.0f;
    panel.sx = 3.0f;
    panel.sy = 3.0f;
    panel.clip = R(0, 80, 600, 140);
    panel.ui = true;
    REQUIRE(composer.Compose(layer, frame.data(), fw, fh, fw, { world, panel }, 600, 140, 1.0f));
    const text::Overlay& o = composer.Result();
    // The panel's glyph lands at 3x below the world, the world's at 1x.
    int panelPixels = 0;
    int worldPixels = 0;
    for (int y = 0; y < o.height; y++)
        for (int x = 0; x < o.width; x++) {
            if ((o.pixels[static_cast<size_t>(y) * 600 + static_cast<size_t>(x)] >> 24) == 0)
                continue;
            if (y >= 80) {
                CHECK(x >= 49 * 3);
                CHECK(x < 61 * 3);
                panelPixels++;
            } else {
                CHECK(x >= 149);
                CHECK(x < 161);
                worldPixels++;
            }
        }
    CHECK(panelPixels > worldPixels * 4);
    REQUIRE(composer.Cache().Count() == 2); // two sizes: 9 px and 27 px

    // The panel's scale changes (a resize, the presenter's hotkey): the
    // glyphs are rebuilt at the new size and the old atlas goes.
    panel.sx = 2.0f;
    panel.sy = 2.0f;
    panel.clip = R(0, 80, 600, 120);
    REQUIRE(composer.Compose(layer, frame.data(), fw, fh, fw, { world, panel }, 600, 120, 1.0f));
    REQUIRE(composer.Cache().Count() == 2); // 9 px and 18 px
    REQUIRE(composer.Result().height == 120);

    // Text Font Scale enlarges the glyphs.
    int base = 0;
    Painted(composer.Result(), &base);
    REQUIRE(composer.Compose(layer, frame.data(), fw, fh, fw, { world, panel }, 600, 120, 1.5f));
    int larger = 0;
    Painted(composer.Result(), &larger);
    CHECK(larger > base);
}

namespace {

// SDL's offscreen video driver (windows without a display; Vulkan through
// VK_EXT_headless_surface for the gpu backend), as test_present.cpp uses it.
struct OffscreenVideo {
    bool ok = false;
    OffscreenVideo()
    {
        SetEnv("IWD2R_GPU_PROBE", "skip");
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "offscreen", SDL_HINT_OVERRIDE);
        ok = SDL_Init(SDL_INIT_VIDEO);
        present::testing::ForgetGpuLoss();
    }
    ~OffscreenVideo()
    {
        text::SetEnabled(false);
        text::BackLayer().Clear();
        text::PresentComposer().Reset();
        present::testing::ForgetGpuLoss();
        present::SetPresentTarget(nullptr);
        present::testing::UseOffscreen(0, 0);
        present::SetBackend(present::BackendKind::Classic);
        present::SetRegions({});
        present::SetPrimary(nullptr, 0, 0);
        present::SetWindowScale(1);
        SDL_Quit();
        SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
        SetEnv("IWD2R_GPU_PROBE", nullptr);
    }
};

std::vector<uint32_t> Presented()
{
    std::vector<uint32_t> out;
    int w = 0;
    int h = 0;
    REQUIRE(present::ReadbackPresented(&out, &w, &h));
    return out;
}

size_t Differences(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, int* maxChannel = nullptr)
{
    REQUIRE(a.size() == b.size());
    size_t n = 0;
    int worst = 0;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] == b[i])
            continue;
        n++;
        for (int shift = 0; shift < 24; shift += 8) {
            const int d = std::abs(static_cast<int>((a[i] >> shift) & 0xFF) - static_cast<int>((b[i] >> shift) & 0xFF));
            worst = std::max(worst, d);
        }
    }
    if (maxChannel)
        *maxChannel = worst;
    return n;
}

}

TEST_CASE("text_overlay_in_both_presenters", "[present][text]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    const int fw = 160;
    const int fh = 120;
    std::vector<uint32_t> px = Frame(fw, fh);
    present::SetPrimary(&px, fw, fh);
    present::SetWindowScale(2);
    SDL_Window* window = SDL_CreateWindow("present text", fw * 2, fh * 2, SDL_WINDOW_HIDDEN);
    REQUIRE(window != nullptr);
    present::SetBackend(present::BackendKind::Classic);
    present::SetPresentTarget(window);
    REQUIRE(present::ActiveBackend() == present::BackendKind::Classic);

    // Text disabled (Text Renderer=classic): the presented frame is the
    // frame, the tags in X ignored, byte for byte.
    const std::vector<uint32_t> plain = Presented();
    const text::Glyph g = H();
    Tag(px, fw, g.box, text::BackLayer().Add(g));
    REQUIRE(Differences(Presented(), plain) == 0);

    // Enabled: the glyph is drawn over the frame, inside its reach.
    text::SetEnabled(true);
    const std::vector<uint32_t> classic = Presented();
    REQUIRE(Differences(classic, plain) > 20);
    for (int y = 0; y < fh * 2; y++)
        for (int x = 0; x < fw * 2; x++)
            if (classic[static_cast<size_t>(y) * static_cast<size_t>(fw * 2) + static_cast<size_t>(x)] != plain[static_cast<size_t>(y) * static_cast<size_t>(fw * 2) + static_cast<size_t>(x)]) {
                CHECK(x >= (g.cell.left - 1) * 2);
                CHECK(x < (g.cell.right + 1) * 2);
            }

    // The gpu presenter draws the same overlay (blending may round one
    // step apart); after a fall back to classic the text stays.
    present::ToggleBackend();
    if (present::ActiveBackend() != present::BackendKind::Gpu) {
        present::SetPresentTarget(nullptr);
        SDL_DestroyWindow(window);
        SKIP("no SDL GPU device: " << present::testing::LastFallbackReason());
    }
    const std::vector<uint32_t> gpu = Presented();
    int worst = 0;
    Differences(gpu, classic, &worst);
    CHECK(worst <= 2);
    REQUIRE(Differences(gpu, plain) > 20);
    present::ToggleBackend();
    REQUIRE(present::ActiveBackend() == present::BackendKind::Classic);
    REQUIRE(Differences(Presented(), classic) == 0);

    present::SetPresentTarget(nullptr);
    SDL_DestroyWindow(window);
}

namespace {

// FNV-1a 64 of a file's bytes, as hex.
std::string FileHash(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(h));
    return bytes.empty() ? std::string() : std::string(hex);
}

// The pinned golden hashes of this machine: <IWD2R_HOME or
// ~/.local/share/iwd2r>/text-golden, lines "<name> <hash>".
std::map<std::string, std::string> Pinned()
{
    std::map<std::string, std::string> pinned;
    std::string home;
    if (const char* h = std::getenv("IWD2R_HOME"))
        home = h;
    else if (const char* user = std::getenv("HOME"))
        home = std::string(user) + "/.local/share/iwd2r";
    std::ifstream in(std::filesystem::path(home) / "text-golden");
    std::string name;
    std::string hash;
    while (in >> name >> hash)
        pinned[name] = hash;
    return pinned;
}

}

// Golden presented frames with scalable text: the main menu, the character
// generation screen and its description panel (scenarios/text_screens.txt)
// at Scale 2 under the classic presenter. Each machine pins its own hashes
// (the game data and the renderer differ), as for the smoke test: a hash not
// pinned yet is printed for <IWD2R_HOME>/text-golden. Inspect the shots once
// by eye before pinning them.
TEST_CASE("text_golden_presented_frames", "[text][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    DataView view(env);
    const std::filesystem::path shots = std::filesystem::path(view.Path()).parent_path() / "text-golden-shots";
    std::filesystem::create_directories(shots);
    const std::string script = std::string(IWD2R_SOURCE_DIR) + "/scenarios/text_screens.txt";
    SetEnv("IWD2R_INI_OVERRIDE", "Program Options/Text Renderer=scalable;Program Options/Resolution=1600;Program Options/Height=1200;Program Options/Scale=2;Program Options/Full Screen=0");
    compat::ini::ResetOverridesForTesting();
    SetEnv("SDL_VIDEO_DRIVER", "dummy");
    SetEnv("SDL_AUDIO_DRIVER", "dummy");
    SetEnv("IWD2R_AUDIO", "null");
    SetEnv("IWD2R_SCRIPT", script.c_str());
    SetEnv("IWD2R_SHOT_DIR", shots.string().c_str());
    SetEnv("IWD2R_SHOT_PRESENTED", "1");
    const char* old = std::getenv("IWD2_DATA");
    const std::string saved = old ? old : "";
    SetEnv("IWD2_DATA", view.Path().c_str());
    const std::filesystem::path cwd = std::filesystem::current_path();
    char arg0[] = "iwd2r";
    char* argv[] = { arg0, nullptr };
    const int rc = iwd2r_main(1, argv);
    std::filesystem::current_path(cwd);
    SetEnv("IWD2_DATA", saved.c_str());
    for (const char* name : { "IWD2R_SCRIPT", "IWD2R_SHOT_DIR", "IWD2R_SHOT_PRESENTED", "IWD2R_INI_OVERRIDE" })
        SetEnv(name, nullptr);
    compat::ini::ResetOverridesForTesting();
    REQUIRE(rc == 0);

    const std::map<std::string, std::string> pinned = Pinned();
    for (const char* shot : { "01_start", "04_chargen", "05_gender" }) {
        const std::string name = std::string("text_") + shot + "-presented";
        const std::string hash = FileHash(shots / (std::string(shot) + "-presented.png"));
        INFO(name);
        REQUIRE_FALSE(hash.empty());
        auto it = pinned.find(name);
        if (it == pinned.end()) {
            WARN("not pinned: " << name << " " << hash << " (add the line to <IWD2R_HOME>/text-golden after looking at " << (shots / (std::string(shot) + "-presented.png")).string() << ")");
            continue;
        }
        CHECK(it->second == hash);
    }
}

TEST_CASE("text_toggle_hotkey", "[text]")
{
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    REQUIRE(SDL_Init(SDL_INIT_VIDEO));
    int toggles = 0;
    text::SetToggleHandler([&toggles] {
        toggles++;
        return true;
    });
    auto press = [](SDL_Scancode sc, SDL_Keymod mod) {
        SDL_Event e;
        SDL_zero(e);
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.scancode = sc;
        e.key.mod = mod;
        e.key.down = true;
        REQUIRE(SDL_PushEvent(&e));
        platform::window::PumpEvents();
    };
    press(SDL_SCANCODE_T, static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT));
    CHECK(toggles == 1);
    // T alone, Ctrl+T and Shift+T are the game's keys.
    press(SDL_SCANCODE_T, SDL_KMOD_NONE);
    press(SDL_SCANCODE_T, SDL_KMOD_LCTRL);
    press(SDL_SCANCODE_T, SDL_KMOD_LSHIFT);
    CHECK(toggles == 1);
    text::SetToggleHandler(nullptr);
    REQUIRE_FALSE(text::Toggle());
    SDL_Quit();
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}
