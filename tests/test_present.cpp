#define NOMINMAX
#include "platform/gpu_probe.h"
#include "platform/present.h"
#include "platform/present_backend.h"

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace present = platform::present;
using present::BackendKind;
using present::Filter;
using present::testing::GpuFault;

namespace {

// Sets (value nullptr: unsets) an environment variable for one scope.
struct ScopedEnv {
    std::string name;
    bool had = false;
    std::string old;
    ScopedEnv(const char* n, const char* value)
        : name(n)
    {
        const char* v = std::getenv(n);
        had = v != nullptr;
        old = v ? v : "";
        Set(value);
    }
    ~ScopedEnv() { Set(had ? old.c_str() : nullptr); }
    void Set(const char* value)
    {
#ifdef _WIN32
        _putenv_s(name.c_str(), value ? value : "");
#else
        if (value)
            setenv(name.c_str(), value, 1);
        else
            unsetenv(name.c_str());
#endif
    }
};

// SDL's offscreen video driver: windows without a display, and Vulkan
// through VK_EXT_headless_surface (lavapipe on CI) for the gpu backend.
// The tests create the device in-process (IWD2R_GPU_PROBE=skip) unless a
// test sets the probe up itself.
struct OffscreenVideo {
    bool ok = false;
    ScopedEnv probe { "IWD2R_GPU_PROBE", "skip" };
    OffscreenVideo()
    {
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "offscreen", SDL_HINT_OVERRIDE);
        ok = SDL_Init(SDL_INIT_VIDEO);
        present::testing::ForgetGpuLoss();
    }
    ~OffscreenVideo()
    {
        present::testing::ForgetGpuLoss();
        present::testing::SetProbeExe(nullptr);
        present::SetPresentTarget(nullptr);
        present::testing::UseOffscreen(0, 0);
        present::testing::SetGpuFault(GpuFault::None);
        present::testing::SetSkipRcas(false);
        present::SetBackend(BackendKind::Classic);
        present::SetRegions({});
        present::SetFilter(Filter::Nearest);
        present::SetSharpness(0.5f);
        present::SetPrimary(nullptr, 0, 0);
        present::SetWindowScale(1);
        present::SetFullscreen(false);
        SDL_Quit();
        SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
    }
};

// Every pixel a different XRGB value (an odd multiplier is a bijection
// modulo 2^24).
std::vector<uint32_t> Distinct(int w, int h)
{
    std::vector<uint32_t> px(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (size_t i = 0; i < px.size(); i++)
        px[i] = static_cast<uint32_t>(i * 2654435761u) & 0xFFFFFFu;
    return px;
}

std::vector<uint32_t> Upscaled(const std::vector<uint32_t>& px, int w, int h, int s)
{
    std::vector<uint32_t> out(static_cast<size_t>(w * s) * static_cast<size_t>(h * s));
    for (int y = 0; y < h * s; y++)
        for (int x = 0; x < w * s; x++)
            out[static_cast<size_t>(y) * static_cast<size_t>(w * s) + static_cast<size_t>(x)] = px[static_cast<size_t>(y / s) * static_cast<size_t>(w) + static_cast<size_t>(x / s)];
    return out;
}

// A picture with edges at every angle and smooth areas: a gradient, a disc
// and diagonal stripes, so the upscalers have edges to work on.
std::vector<uint32_t> Picture(int w, int h)
{
    std::vector<uint32_t> px(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t r = static_cast<uint32_t>(x * 255 / (w - 1));
            uint32_t g = static_cast<uint32_t>(y * 255 / (h - 1));
            uint32_t b = 64;
            const int dx = x - w / 3;
            const int dy = y - h / 2;
            if (dx * dx + dy * dy < (h / 4) * (h / 4)) {
                r = 240;
                g = 220;
                b = 40;
            } else if (x > w / 2 && ((x + y) / 6) % 2 == 0) {
                r = 20;
                g = 20;
                b = 200;
            }
            px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = (r << 16) | (g << 8) | b;
        }
    }
    return px;
}

// The mean absolute difference per channel between two images of the same
// size.
double MeanDifference(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b)
{
    REQUIRE(a.size() == b.size());
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); i++) {
        for (int shift = 0; shift < 24; shift += 8) {
            const int ca = static_cast<int>((a[i] >> shift) & 0xFFu);
            const int cb = static_cast<int>((b[i] >> shift) & 0xFFu);
            sum += ca > cb ? ca - cb : cb - ca;
        }
    }
    return sum / (3.0 * static_cast<double>(a.size()));
}

platform::compose::Region MakeRegion(RECT frame, LONG wx, LONG wy, LONG ww, LONG wh, int scale)
{
    platform::compose::Region r = {};
    r.frame = frame;
    r.window = { wx, wy, wx + ww, wy + wh };
    r.scale = scale;
    return r;
}

// A gpu backend without a window, rendering into outW x outH; nullptr when
// this machine has no Vulkan device (the caller skips).
std::unique_ptr<present::Backend> OffscreenGpu(int outW, int outH, std::string& why)
{
    std::unique_ptr<present::Backend> b = present::MakeGpuBackend();
    b->offscreenWidth = outW;
    b->offscreenHeight = outH;
    if (!b->Init(nullptr)) {
        why = b->error;
        b->Shutdown();
        return nullptr;
    }
    return b;
}

// One frame of regions through a backend, read back.
std::vector<uint32_t> Draw(present::Backend& b, const std::vector<uint32_t>& px, int w, int h,
    const std::vector<platform::compose::Region>& regions, Filter filter, int* outW, int* outH)
{
    std::vector<uint32_t> out;
    REQUIRE(b.BeginFrame());
    b.Upload(px.data(), w, h, w * 4);
    for (const auto& r : regions)
        b.DrawRegion(r, filter);
    b.EndFrame();
    REQUIRE_FALSE(b.lost);
    REQUIRE(b.Readback(&out, outW, outH));
    return out;
}

size_t Mismatches(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b)
{
    if (a.size() != b.size())
        return a.size() > b.size() ? a.size() : b.size();
    size_t n = 0;
    for (size_t i = 0; i < a.size(); i++)
        n += a[i] != b[i] ? 1 : 0;
    return n;
}

}

TEST_CASE("gpu_offscreen_identity", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::string why;
    auto b = OffscreenGpu(320, 240, why);
    if (!b)
        SKIP("no SDL GPU device: " << why);
    const std::vector<uint32_t> px = Distinct(320, 240);
    int w = 0;
    int h = 0;

    // One region, the whole frame at scale 1, nearest.
    std::vector<uint32_t> out = Draw(*b, px, 320, 240, { MakeRegion({ 0, 0, 320, 240 }, 0, 0, 320, 240, 1) }, Filter::Nearest, &w, &h);
    CHECK(w == 320);
    CHECK(h == 240);
    CHECK(Mismatches(out, px) == 0);

    // The whole-frame presentation (no region list) is the same at 1:1.
    out = Draw(*b, px, 320, 240, { present::WholeFrameRegion(320, 240) }, Filter::Nearest, &w, &h);
    CHECK(Mismatches(out, px) == 0);

    // Sharp and smooth sample texel centres at scale 1: the same pixels.
    out = Draw(*b, px, 320, 240, { present::WholeFrameRegion(320, 240) }, Filter::Sharp, &w, &h);
    CHECK(Mismatches(out, px) == 0);
    out = Draw(*b, px, 320, 240, { present::WholeFrameRegion(320, 240) }, Filter::Smooth, &w, &h);
    CHECK(Mismatches(out, px) == 0);

    // Fsr and nis draw a region at one texel per pixel with nearest: the
    // native-resolution world is not sharpened.
    const std::vector<uint32_t> picture = Picture(320, 240);
    for (Filter f : { Filter::Fsr, Filter::Nis }) {
        INFO("filter " << (f == Filter::Fsr ? "fsr" : "nis"));
        b->sharpness = 1.0f;
        out = Draw(*b, picture, 320, 240, { present::WholeFrameRegion(320, 240) }, f, &w, &h);
        CHECK(Mismatches(out, picture) == 0);
        out = Draw(*b, picture, 320, 240, { MakeRegion({ 0, 0, 320, 240 }, 0, 0, 320, 240, 1) }, f, &w, &h);
        CHECK(Mismatches(out, picture) == 0);
    }
    b->Shutdown();
}

TEST_CASE("classic_offscreen_identity", "[present]")
{
    // The classic presenter (the default) on the offscreen driver's hidden
    // window: the frame as drawn, read back, is the input at Scale 1 and
    // the nearest-neighbour upscale at Scale 2, through the whole-frame
    // path and through a region.
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPrimary(&px, 320, 240);
    present::SetBackend(BackendKind::Classic);
    for (int s = 1; s <= 2; s++) {
        INFO("scale " << s);
        present::SetWindowScale(s);
        SDL_Window* window = SDL_CreateWindow("present classic", 320 * s, 240 * s, SDL_WINDOW_HIDDEN);
        REQUIRE(window != nullptr);
        present::SetPresentTarget(window);
        REQUIRE(present::ActiveBackend() == BackendKind::Classic);
        const std::vector<uint32_t> want = s == 1 ? px : Upscaled(px, 320, 240, s);
        present::Present(px.data(), 320, 240, 320 * 4);
        std::vector<uint32_t> out;
        int w = 0;
        int h = 0;
        REQUIRE(present::ReadbackPresented(&out, &w, &h));
        CHECK(w == 320 * s);
        CHECK(h == 240 * s);
        CHECK(Mismatches(out, want) == 0);

        present::SetRegions({ MakeRegion({ 0, 0, 320, 240 }, 0, 0, 320 * s, 240 * s, s) });
        out.clear();
        REQUIRE(present::ReadbackPresented(&out, &w, &h));
        CHECK(Mismatches(out, want) == 0);
        present::SetRegions({});
        present::SetPresentTarget(nullptr);
        SDL_DestroyWindow(window);
    }
}

TEST_CASE("gpu_identity_sharp_integer_scale", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    const std::vector<uint32_t> px = Distinct(320, 240);
    for (int s = 2; s <= 3; s++) {
        std::string why;
        auto b = OffscreenGpu(320 * s, 240 * s, why);
        if (!b)
            SKIP("no SDL GPU device: " << why);
        const std::vector<uint32_t> want = Upscaled(px, 320, 240, s);
        int w = 0;
        int h = 0;
        const auto region = MakeRegion({ 0, 0, 320, 240 }, 0, 0, 320 * s, 240 * s, s);
        INFO("scale " << s);
        CHECK(Mismatches(Draw(*b, px, 320, 240, { region }, Filter::Sharp, &w, &h), want) == 0);
        CHECK(w == 320 * s);
        CHECK(Mismatches(Draw(*b, px, 320, 240, { region }, Filter::Nearest, &w, &h), want) == 0);
        b->Shutdown();
    }
}

TEST_CASE("gpu_regions_draw_at_their_window_rects", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    // A 320x240 frame: the world (top 200 rows) at 1x on the left of a
    // 650x490 window, a 40-row panel at 2x below it; the rest is slack.
    std::string why;
    auto b = OffscreenGpu(650, 490, why);
    if (!b)
        SKIP("no SDL GPU device: " << why);
    const std::vector<uint32_t> px = Distinct(320, 240);
    const auto world = MakeRegion({ 0, 0, 320, 200 }, 0, 0, 650, 410, 1);
    const auto panel = MakeRegion({ 0, 200, 320, 240 }, 0, 410, 650, 80, 2);
    int w = 0;
    int h = 0;
    const std::vector<uint32_t> out = Draw(*b, px, 320, 240, { world, panel }, Filter::Sharp, &w, &h);
    REQUIRE(w == 650);
    REQUIRE(h == 490);
    size_t bad = 0;
    for (int y = 0; y < 490; y++) {
        for (int x = 0; x < 650; x++) {
            uint32_t want = 0;
            if (y < 200 && x < 320)
                want = px[static_cast<size_t>(y) * 320 + static_cast<size_t>(x)];
            else if (y >= 410 && x < 640)
                want = px[static_cast<size_t>(200 + (y - 410) / 2) * 320 + static_cast<size_t>(x / 2)];
            bad += out[static_cast<size_t>(y) * 650 + static_cast<size_t>(x)] != want ? 1 : 0;
        }
    }
    CHECK(bad == 0);
    b->Shutdown();
}

TEST_CASE("fallback_when_no_device", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    SDL_Window* window = SDL_CreateWindow("present fallback", 320, 240, SDL_WINDOW_HIDDEN);
    REQUIRE(window != nullptr);
    std::vector<uint32_t> px = Distinct(320, 240);

    present::SetPresentTarget(window);
    for (GpuFault fault : { GpuFault::Device, GpuFault::Pipeline }) {
        INFO("fault " << static_cast<int>(fault));
        present::testing::SetGpuFault(fault);
        present::SetBackend(BackendKind::Gpu);
        CHECK(present::ActiveBackend() == BackendKind::Classic);
        present::SetBackend(BackendKind::Auto);
        CHECK(present::ActiveBackend() == BackendKind::Classic);
        const int frames = present::PresentedFrames();
        present::Present(px.data(), 320, 240, 320 * 4);
        CHECK(present::PresentedFrames() == frames + 1);
    }
    present::SetPresentTarget(nullptr);
    SDL_DestroyWindow(window);
}

TEST_CASE("skipped_frame_on_acquire_failure", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPrimary(&px, 320, 240);
    present::SetBackend(BackendKind::Gpu);
    present::testing::UseOffscreen(320, 240);
    if (present::ActiveBackend() != BackendKind::Gpu)
        SKIP("no SDL GPU device");

    // One failed acquire: the frame is skipped but counted, the device is
    // rebuilt and the next frame draws.
    present::testing::SetGpuFault(GpuFault::AcquireOnce);
    int frames = present::PresentedFrames();
    int skipped = present::SkippedFrames();
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::PresentedFrames() == frames + 1);
    CHECK(present::SkippedFrames() == skipped + 1);
    CHECK(present::ActiveBackend() == BackendKind::Gpu);
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::SkippedFrames() == skipped + 1);
    std::vector<uint32_t> out;
    int w = 0;
    int h = 0;
    REQUIRE(present::ReadbackPresented(&out, &w, &h));
    CHECK(Mismatches(out, px) == 0);

    // Lost again after the rebuild: classic for the rest of the run (which
    // cannot start without a window); frames are still counted.
    present::testing::SetGpuFault(GpuFault::Acquire);
    frames = present::PresentedFrames();
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::PresentedFrames() == frames + 2);
}

TEST_CASE("device_loss_stays_classic", "[present]")
{
    // A device lost again after its rebuild: classic for the rest of the
    // run. A new target (a recreated window) and the hotkey stay classic,
    // and the rebuild budget is not renewed.
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPrimary(&px, 320, 240);
    present::SetBackend(BackendKind::Gpu);
    present::testing::UseOffscreen(320, 240);
    if (present::ActiveBackend() != BackendKind::Gpu)
        SKIP("no SDL GPU device: " << present::testing::LastFallbackReason());
    present::testing::SetGpuFault(GpuFault::AcquireOnce);
    present::Present(px.data(), 320, 240, 320 * 4);
    REQUIRE(present::ActiveBackend() == BackendKind::Gpu);
    present::testing::SetGpuFault(GpuFault::AcquireOnce);
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    present::testing::SetGpuFault(GpuFault::None);

    // Offscreen again (a new target): still classic, which cannot start
    // without a window.
    present::testing::UseOffscreen(0, 0);
    present::testing::UseOffscreen(320, 240);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    CHECK(present::testing::LastFallbackReason() == "lost earlier in this run");

    // A window: classic draws; gpu (the request, the hotkey) does not
    // come back.
    present::testing::UseOffscreen(0, 0);
    SDL_Window* window = SDL_CreateWindow("present lost", 320, 240, SDL_WINDOW_HIDDEN);
    REQUIRE(window != nullptr);
    present::SetPresentTarget(window);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    CHECK(present::ToggleBackend() == BackendKind::Classic);
    const int frames = present::PresentedFrames();
    const int skipped = present::SkippedFrames();
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::PresentedFrames() == frames + 1);
    CHECK(present::SkippedFrames() == skipped);
    present::SetPresentTarget(nullptr);
    SDL_DestroyWindow(window);

    // Forgotten (tests only): gpu starts again.
    present::testing::ForgetGpuLoss();
    present::testing::UseOffscreen(320, 240);
    CHECK(present::ActiveBackend() == BackendKind::Gpu);
}

TEST_CASE("null_swapchain_skips_without_rebuild", "[present]")
{
    // An acquire that succeeds without a swapchain texture (a minimised
    // window): the frame is skipped and counted, nothing is rebuilt, the
    // presenter stays gpu and draws once a texture comes back.
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPrimary(&px, 320, 240);
    present::SetBackend(BackendKind::Gpu);
    present::testing::UseOffscreen(320, 240);
    if (present::ActiveBackend() != BackendKind::Gpu)
        SKIP("no SDL GPU device: " << present::testing::LastFallbackReason());
    present::testing::SetGpuFault(GpuFault::NullSwapchain);
    const int frames = present::PresentedFrames();
    const int skipped = present::SkippedFrames();
    for (int i = 0; i < 3; i++)
        present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::PresentedFrames() == frames + 3);
    CHECK(present::SkippedFrames() == skipped + 3);
    CHECK(present::ActiveBackend() == BackendKind::Gpu);
    present::testing::SetGpuFault(GpuFault::None);
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::SkippedFrames() == skipped + 3);
    std::vector<uint32_t> out;
    int w = 0;
    int h = 0;
    REQUIRE(present::ReadbackPresented(&out, &w, &h));
    CHECK(Mismatches(out, px) == 0);

    // The rebuild was not spent: a loss now is still answered with one.
    present::testing::SetGpuFault(GpuFault::AcquireOnce);
    present::Present(px.data(), 320, 240, 320 * 4);
    CHECK(present::ActiveBackend() == BackendKind::Gpu);
}

TEST_CASE("regions_snapshot_outlives_set_regions", "[present]")
{
    present::SetRegions({ MakeRegion({ 0, 0, 10, 10 }, 0, 0, 20, 20, 2) });
    const auto held = present::Regions();
    present::SetRegions({});
    REQUIRE(held->size() == 1);
    CHECK((*held)[0].scale == 2);
    CHECK(present::Regions()->empty());
}

TEST_CASE("toggle_switches_and_reports", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    SDL_Window* window = SDL_CreateWindow("present toggle", 320, 240, SDL_WINDOW_HIDDEN);
    REQUIRE(window != nullptr);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPrimary(&px, 320, 240);
    present::SetBackend(BackendKind::Classic);
    present::SetPresentTarget(window);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    present::Present(px.data(), 320, 240, 320 * 4);

    const BackendKind toggled = present::ToggleBackend();
    CHECK(toggled == present::ActiveBackend());
    present::Present(px.data(), 320, 240, 320 * 4);
    if (toggled == BackendKind::Gpu) {
        // The windowed gpu path reads back the same pixels.
        std::vector<uint32_t> out;
        int w = 0;
        int h = 0;
        REQUIRE(present::ReadbackPresented(&out, &w, &h));
        CHECK(w == 320);
        CHECK(h == 240);
        CHECK(Mismatches(out, px) == 0);
        float fx = 0.0f;
        float fy = 0.0f;
        present::WindowToFrame(100.0f, 50.0f, &fx, &fy);
        CHECK(fx == 100.0f);
        CHECK(fy == 50.0f);
    } else {
        WARN("gpu presenter unavailable with a window here; toggled back to classic");
    }
    CHECK(present::ToggleBackend() == BackendKind::Classic);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    present::Present(px.data(), 320, 240, 320 * 4);
    present::SetPresentTarget(nullptr);
    SDL_DestroyWindow(window);
}

TEST_CASE("fsr_and_nis_pipelines_build", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::string why;
    auto b = OffscreenGpu(640, 480, why);
    if (!b)
        SKIP("no SDL GPU device: " << why);
    const std::vector<uint32_t> px = Picture(320, 240);
    const std::vector<platform::compose::Region> whole = { present::WholeFrameRegion(320, 240) };
    int w = 0;
    int h = 0;
    const std::vector<uint32_t> nearest = Draw(*b, px, 320, 240, whole, Filter::Nearest, &w, &h);
    REQUIRE(Mismatches(nearest, Upscaled(px, 320, 240, 2)) == 0);
    std::vector<uint32_t> outs[2];
    const Filter filters[2] = { Filter::Fsr, Filter::Nis };
    for (int i = 0; i < 2; i++) {
        INFO("filter " << (i == 0 ? "fsr" : "nis"));
        outs[i] = Draw(*b, px, 320, 240, whole, filters[i], &w, &h);
        CHECK(w == 640);
        CHECK(h == 480);
        // Not blank: as many distinct colours as a real upscale has.
        std::vector<uint32_t> sorted = outs[i];
        std::sort(sorted.begin(), sorted.end());
        const size_t colours = static_cast<size_t>(std::unique(sorted.begin(), sorted.end()) - sorted.begin());
        CHECK(colours > 1000);
        // Close to the nearest upscale, but not equal to it.
        const double diff = MeanDifference(outs[i], nearest);
        CHECK(diff > 0.0);
        CHECK(diff < 8.0);
        CHECK(Mismatches(outs[i], nearest) > 0);
    }
    CHECK(Mismatches(outs[0], outs[1]) > 0);

    // Two regions: each is filtered inside its own frame rectangle, and the
    // slack around them stays black.
    const std::vector<platform::compose::Region> split = {
        MakeRegion({ 0, 0, 160, 240 }, 0, 0, 320, 480, 2),
        MakeRegion({ 160, 0, 320, 240 }, 322, 0, 318, 480, 2),
    };
    // The right half's content changes; the left region's pixels do not,
    // since its taps are clamped to its own frame rectangle.
    std::vector<uint32_t> changed = px;
    for (int y = 0; y < 240; y++)
        for (int x = 160; x < 320; x++)
            changed[static_cast<size_t>(y) * 320 + static_cast<size_t>(x)] ^= 0xFFFFFFu;
    for (Filter f : filters) {
        const std::vector<uint32_t> out = Draw(*b, px, 320, 240, split, f, &w, &h);
        CHECK(out[static_cast<size_t>(100) * 640 + 320] == 0);
        CHECK(out[static_cast<size_t>(100) * 640 + 321] == 0);
        CHECK(out[static_cast<size_t>(100) * 640 + 100] != 0);
        CHECK(out[static_cast<size_t>(100) * 640 + 400] != 0);
        const std::vector<uint32_t> other = Draw(*b, changed, 320, 240, split, f, &w, &h);
        size_t leftDiffs = 0;
        size_t rightDiffs = 0;
        for (int y = 0; y < 480; y++) {
            for (int x = 0; x < 640; x++) {
                const size_t i = static_cast<size_t>(y) * 640 + static_cast<size_t>(x);
                (x < 320 ? leftDiffs : rightDiffs) += out[i] != other[i] ? 1 : 0;
            }
        }
        CHECK(leftDiffs == 0);
        CHECK(rightDiffs > 0);
    }

    // The three plain filters are unchanged by the FSR pass in the same
    // backend: nearest still matches the upscale.
    CHECK(Mismatches(Draw(*b, px, 320, 240, whole, Filter::Nearest, &w, &h), nearest) == 0);
    b->Shutdown();
}

TEST_CASE("sharpness_changes_output", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::string why;
    auto b = OffscreenGpu(640, 480, why);
    if (!b)
        SKIP("no SDL GPU device: " << why);
    const std::vector<uint32_t> px = Picture(320, 240);
    const std::vector<platform::compose::Region> whole = { present::WholeFrameRegion(320, 240) };
    int w = 0;
    int h = 0;
    for (Filter f : { Filter::Fsr, Filter::Nis }) {
        INFO("filter " << (f == Filter::Fsr ? "fsr" : "nis"));
        b->sharpness = 0.0f;
        const std::vector<uint32_t> soft = Draw(*b, px, 320, 240, whole, f, &w, &h);
        b->sharpness = 1.0f;
        const std::vector<uint32_t> sharp = Draw(*b, px, 320, 240, whole, f, &w, &h);
        b->sharpness = 0.0f;
        const std::vector<uint32_t> again = Draw(*b, px, 320, 240, whole, f, &w, &h);
        CHECK(Mismatches(soft, sharp) > 0);
        CHECK(Mismatches(soft, again) == 0);
    }
    b->Shutdown();

    // The front clamps and hands the value to the backend.
    present::SetSharpness(2.0f);
    CHECK(present::CurrentSharpness() == 1.0f);
    present::SetSharpness(-1.0f);
    CHECK(present::CurrentSharpness() == 0.0f);
    present::SetSharpness(0.5f);
    CHECK(present::CurrentSharpness() == 0.5f);
}

TEST_CASE("rcas_at_sharpness_0_keeps_easu_output", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::string why;
    auto b = OffscreenGpu(640, 480, why);
    if (!b)
        SKIP("no SDL GPU device: " << why);
    const std::vector<uint32_t> px = Picture(320, 240);
    const std::vector<platform::compose::Region> whole = { present::WholeFrameRegion(320, 240) };
    int w = 0;
    int h = 0;
    present::testing::SetSkipRcas(true);
    const std::vector<uint32_t> easu = Draw(*b, px, 320, 240, whole, Filter::Fsr, &w, &h);
    present::testing::SetSkipRcas(false);
    b->sharpness = 0.0f;
    const std::vector<uint32_t> rcas0 = Draw(*b, px, 320, 240, whole, Filter::Fsr, &w, &h);
    CHECK(Mismatches(easu, rcas0) == 0);
    b->sharpness = 1.0f;
    CHECK(Mismatches(easu, Draw(*b, px, 320, 240, whole, Filter::Fsr, &w, &h)) > 0);
    b->Shutdown();
}

TEST_CASE("rcas_sharpens_beside_saturated_channels", "[present]")
{
    // Red is 0 (then 255) everywhere and green/blue carry detail: RCAS's
    // red limiter is FSR's NaN case, where the hardware max() keeps the
    // other operand, so sharpening must still apply.
    OffscreenVideo video;
    REQUIRE(video.ok);
    std::string why;
    auto b = OffscreenGpu(640, 480, why);
    if (!b)
        SKIP("no SDL GPU device: " << why);
    const std::vector<platform::compose::Region> whole = { present::WholeFrameRegion(320, 240) };
    int w = 0;
    int h = 0;
    for (uint32_t red : { 0u, 255u }) {
        INFO("red " << red);
        std::vector<uint32_t> px = Picture(320, 240);
        for (uint32_t& p : px)
            p = (p & 0xFFFFu) | (red << 16);
        b->sharpness = 0.0f;
        const std::vector<uint32_t> soft = Draw(*b, px, 320, 240, whole, Filter::Fsr, &w, &h);
        b->sharpness = 1.0f;
        const std::vector<uint32_t> sharp = Draw(*b, px, 320, 240, whole, Filter::Fsr, &w, &h);
        CHECK(Mismatches(soft, sharp) > soft.size() / 20);
        for (size_t i = 0; i < sharp.size(); i += 997)
            CHECK(((sharp[i] >> 16) & 0xFFu) == red);
    }
    b->Shutdown();
}

TEST_CASE("presenter settings parse", "[present]")
{
    BackendKind kind = BackendKind::Classic;
    CHECK(present::ParseBackendKind(" GPU ", &kind));
    CHECK(kind == BackendKind::Gpu);
    CHECK(present::ParseBackendKind("auto", &kind));
    CHECK(kind == BackendKind::Auto);
    CHECK(present::ParseBackendKind("Classic", &kind));
    CHECK(kind == BackendKind::Classic);
    CHECK_FALSE(present::ParseBackendKind("vulkan", &kind));
    Filter filter = Filter::Nearest;
    CHECK(present::ParseFilter("sharp", &filter));
    CHECK(filter == Filter::Sharp);
    CHECK(present::ParseFilter("SMOOTH", &filter));
    CHECK(filter == Filter::Smooth);
    CHECK(present::ParseFilter("nearest", &filter));
    CHECK(filter == Filter::Nearest);
    CHECK(present::ParseFilter(" FSR ", &filter));
    CHECK(filter == Filter::Fsr);
    CHECK(present::ParseFilter("nis", &filter));
    CHECK(filter == Filter::Nis);
    CHECK_FALSE(present::ParseFilter("", &filter));
    CHECK_FALSE(present::ParseFilter("dlss", &filter));
    float sharpness = -1.0f;
    CHECK(present::ParseSharpness("0.25", &sharpness));
    CHECK(sharpness == 0.25f);
    CHECK(present::ParseSharpness(" 1 ", &sharpness));
    CHECK(sharpness == 1.0f);
    CHECK(present::ParseSharpness("0", &sharpness));
    CHECK(sharpness == 0.0f);
    CHECK_FALSE(present::ParseSharpness("1.5", &sharpness));
    CHECK_FALSE(present::ParseSharpness("-0.1", &sharpness));
    CHECK_FALSE(present::ParseSharpness("sharp", &sharpness));
    CHECK_FALSE(present::ParseSharpness("0.5x", &sharpness));
    CHECK_FALSE(present::ParseSharpness("nan", &sharpness));
    CHECK_FALSE(present::ParseSharpness("", &sharpness));
    CHECK(sharpness == 0.0f);
    CHECK(std::string(present::BackendKindName(BackendKind::Gpu)) == "gpu");
}

TEST_CASE("whole frame rect follows SDL's logical presentation", "[present]")
{
    SDL_FRect r = present::WholeFrameRect(800, 600, 1600, 1200, false);
    CHECK(r.x == 0.0f);
    CHECK(r.w == 1600.0f);
    r = present::WholeFrameRect(800, 600, 1600, 1000, true);
    CHECK(r.h == 1000.0f);
    CHECK(r.w == 1333.0f);
    CHECK(r.x == 133.5f);
    r = present::WholeFrameRect(800, 600, 1600, 1000, false);
    CHECK(r.w == 1600.0f);
    CHECK(r.h == 1000.0f);
}

TEST_CASE("probe_fail_falls_back_to_classic", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    ScopedEnv fail("IWD2R_GPU_PROBE", "fail");
    SDL_Window* window = SDL_CreateWindow("present probe", 320, 240, SDL_WINDOW_HIDDEN);
    REQUIRE(window != nullptr);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPresentTarget(window);
    for (BackendKind kind : { BackendKind::Gpu, BackendKind::Auto }) {
        INFO("kind " << present::BackendKindName(kind));
        present::SetBackend(kind);
        CHECK(present::ActiveBackend() == BackendKind::Classic);
        // The stderr line is "presenter: gpu unavailable (<this>), using classic".
        CHECK(present::testing::LastFallbackReason() == "probe: forced by IWD2R_GPU_PROBE=fail");
        const int frames = present::PresentedFrames();
        present::Present(px.data(), 320, 240, 320 * 4);
        CHECK(present::PresentedFrames() == frames + 1);
    }
    // The hotkey runs the same probe.
    present::SetBackend(BackendKind::Classic);
    CHECK(present::ToggleBackend() == BackendKind::Classic);
    CHECK(present::testing::LastFallbackReason() == "probe: forced by IWD2R_GPU_PROBE=fail");
    present::SetPresentTarget(nullptr);
    SDL_DestroyWindow(window);
}

TEST_CASE("probe_skip_starts_gpu", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    present::SetBackend(BackendKind::Gpu);
    present::testing::UseOffscreen(320, 240);
    if (present::ActiveBackend() != BackendKind::Gpu)
        SKIP("no SDL GPU device: " << present::testing::LastFallbackReason());
    CHECK(present::testing::LastFallbackReason().empty());
}

TEST_CASE("probe_exe_reports_the_device", "[present]")
{
    // The real executable's --probe-gpu: ok on a Vulkan device (lavapipe
    // through the offscreen driver), unavailable without Vulkan (dummy).
    {
        ScopedEnv driver("SDL_VIDEO_DRIVER", "offscreen");
        const platform::gpu_probe::Result r = platform::gpu_probe::Run(IWD2R_EXE, 10000);
        INFO("line: " << r.line << "; reason: " << r.reason);
        CHECK(r.outcome != platform::gpu_probe::Outcome::Crashed);
        CHECK(r.outcome != platform::gpu_probe::Outcome::TimedOut);
        if (r.outcome == platform::gpu_probe::Outcome::Ok)
            CHECK(r.line.rfind("probe-gpu: ok ", 0) == 0);
        else
            WARN("no Vulkan device for the probe here: " << r.reason);
    }
    {
        ScopedEnv driver("SDL_VIDEO_DRIVER", "dummy");
        const platform::gpu_probe::Result r = platform::gpu_probe::Run(IWD2R_EXE, 10000);
        INFO("line: " << r.line << "; reason: " << r.reason);
        CHECK(r.outcome == platform::gpu_probe::Outcome::Failed);
        CHECK(r.line.rfind("probe-gpu: unavailable ", 0) == 0);
    }
}

#ifndef _WIN32

#include <unistd.h>

namespace {

// A fresh temporary directory of executable shell scripts, removed with it.
struct ScriptDir {
    std::filesystem::path dir;
    ScriptDir()
        : dir(std::filesystem::temp_directory_path() / ("iwd2r-probe-" + std::to_string(::getpid())))
    {
        std::filesystem::create_directories(dir);
    }
    ~ScriptDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    std::string Script(const char* name, const std::string& body) const
    {
        const std::filesystem::path path = dir / name;
        {
            std::ofstream f(path);
            f << "#!/bin/sh\n"
              << body << "\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_all);
        return path.string();
    }
    // The lines in a file the scripts append to (0 when it does not exist).
    int Lines(const char* name) const
    {
        std::ifstream f(dir / name);
        int n = 0;
        for (std::string line; std::getline(f, line);)
            n++;
        return n;
    }
};

}

TEST_CASE("probe_child_outcomes", "[present]")
{
    namespace probe = platform::gpu_probe;
    const ScriptDir scripts;
    // macOS has false only in /usr/bin.
    const char* falseExe = std::filesystem::exists("/bin/false") ? "/bin/false" : "/usr/bin/false";
    probe::Result r = probe::Run(falseExe, 10000);
    CHECK(r.outcome == probe::Outcome::Failed);
    CHECK(r.reason == "exited with code 1");

    const auto start = std::chrono::steady_clock::now();
    r = probe::Run("/bin/sleep", 300, { "30" });
    CHECK(r.outcome == probe::Outcome::TimedOut);
    CHECK(r.reason == "timed out");
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));

    r = probe::Run(scripts.Script("ok.sh", "echo noise; echo 'probe-gpu: ok fake'"), 10000);
    CHECK(r.outcome == probe::Outcome::Ok);
    CHECK(r.line == "probe-gpu: ok fake");
    CHECK(r.reason == "ok fake");

    r = probe::Run(scripts.Script("no.sh", "echo 'probe-gpu: unavailable no device here'; exit 1"), 10000);
    CHECK(r.outcome == probe::Outcome::Failed);
    CHECK(r.reason == "no device here");

    // A grandchild holding the inherited stdout does not hold the parent:
    // the child's exit ends the wait.
    const auto held = std::chrono::steady_clock::now();
    r = probe::Run(scripts.Script("held.sh", "sleep 5 & echo 'probe-gpu: ok fake'"), 10000);
    CHECK(r.outcome == probe::Outcome::Ok);
    CHECK(std::chrono::steady_clock::now() - held < std::chrono::seconds(3));

    // Only the first 4 KB of output are kept, and a long output never blocks.
    r = probe::Run(scripts.Script("chatty.sh", "head -c 200000 /dev/zero | tr '\\0' x; echo; echo 'probe-gpu: ok late'"), 10000);
    CHECK(r.outcome == probe::Outcome::Failed);
    CHECK(r.line.empty());

    r = probe::Run(scripts.Script("crash.sh", "kill -SEGV $$"), 10000);
    CHECK(r.outcome == probe::Outcome::Crashed);
    CHECK(r.reason == "crashed");

    // The child runs with IWD2R_CRASH_PREFIX=probe-gpu (its crash line
    // reads "probe-gpu crash: ..."), whatever this process has.
    {
        ScopedEnv mine("IWD2R_CRASH_PREFIX", "parent");
        r = probe::Run(scripts.Script("env.sh", "echo \"probe-gpu: ok [$IWD2R_CRASH_PREFIX]\""), 10000);
        CHECK(r.outcome == probe::Outcome::Ok);
        CHECK(r.reason == "ok [probe-gpu]");
    }

    // Exit 0 without the ok line is not a pass.
    r = probe::Run("/bin/true", 10000);
    CHECK(r.outcome == probe::Outcome::Failed);

    r = probe::Run("/nonexistent/iwd2r", 10000);
    CHECK(r.outcome == probe::Outcome::Failed);
    CHECK(r.reason.rfind("could not start", 0) == 0);
}

TEST_CASE("probe_child_gates_the_device", "[present]")
{
    OffscreenVideo video;
    REQUIRE(video.ok);
    ScopedEnv unset("IWD2R_GPU_PROBE", nullptr);
    std::vector<uint32_t> px = Distinct(320, 240);
    present::SetPrimary(&px, 320, 240);
    const ScriptDir scripts;

    // The crashing child counts its runs.
    const std::string counter = (scripts.dir / "runs").string();
    present::testing::SetProbeExe(scripts.Script("crash.sh", "echo run >> '" + counter + "'; kill -SEGV $$").c_str());
    present::SetBackend(BackendKind::Auto);
    present::testing::UseOffscreen(320, 240);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    CHECK(present::testing::LastFallbackReason() == "probe: crashed");

    // The session keeps the verdict: a second start (the hotkey's) does not
    // run the child again.
    CHECK(scripts.Lines("runs") == 1);
    present::SetBackend(BackendKind::Gpu);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    CHECK(present::testing::LastFallbackReason() == "probe: crashed");
    CHECK(scripts.Lines("runs") == 1);

    present::testing::SetProbeExe(scripts.Script("no.sh", "echo 'probe-gpu: unavailable no device here'; exit 1").c_str());
    present::SetBackend(BackendKind::Gpu);
    CHECK(present::ActiveBackend() == BackendKind::Classic);
    CHECK(present::testing::LastFallbackReason() == "probe: no device here");

    // A passing probe lets the device be created in-process.
    present::testing::SetProbeExe(scripts.Script("ok.sh", "echo 'probe-gpu: ok fake'").c_str());
    present::SetBackend(BackendKind::Gpu);
    if (present::ActiveBackend() != BackendKind::Gpu)
        SKIP("no SDL GPU device: " << present::testing::LastFallbackReason());
    CHECK(present::testing::LastFallbackReason().empty());
}

#endif
