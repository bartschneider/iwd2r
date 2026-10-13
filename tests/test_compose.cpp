#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "platform/compose.h"
#include "platform/present.h"

#include <cmath>
#include <string>
#include <vector>

using platform::compose::Layout;
using platform::compose::Region;
using platform::compose::Tiles;

namespace {

RECT Rect(LONG left, LONG top, LONG right, LONG bottom)
{
    RECT r;
    r.left = left;
    r.top = top;
    r.right = right;
    r.bottom = bottom;
    return r;
}

SIZE Size(LONG cx, LONG cy)
{
    SIZE s;
    s.cx = cx;
    s.cy = cy;
    return s;
}

// The test layout: a bottom toolbar 100 high across the frame and a left bar
// 120 wide from the top down to the toolbar.
std::vector<RECT> Panels(SIZE frame)
{
    return { Rect(0, frame.cy - 100, frame.cx, frame.cy), Rect(0, 0, 120, frame.cy - 100) };
}

std::string Describe(SIZE window, int ui, int world)
{
    return std::to_string(window.cx) + "x" + std::to_string(window.cy) + " ui " + std::to_string(ui) + " world " + std::to_string(world);
}

const Region* World(const std::vector<Region>& regions)
{
    for (const Region& r : regions) {
        if (!r.ui)
            return &r;
    }
    return nullptr;
}

bool Contains(const RECT& r, float x, float y)
{
    return x >= static_cast<float>(r.left) && x < static_cast<float>(r.right) && y >= static_cast<float>(r.top) && y < static_cast<float>(r.bottom);
}

void ForEachLayout(void (*check)(SIZE frame, SIZE window, int ui, int world, const std::vector<Region>& regions))
{
    const SIZE windows[] = { Size(800, 600), Size(1024, 768), Size(1920, 1080), Size(2560, 1440), Size(3840, 2160) };
    for (const SIZE& base : windows) {
        for (int ui = 1; ui <= 4; ui++) {
            for (int world = 1; world <= 4; world++) {
                // The frame is the listed size; the window that frame at the
                // world scale.
                const SIZE frame = base;
                const SIZE window = Size(frame.cx * world, frame.cy * world);
                const std::vector<Region> regions = Layout(frame, window, ui, world, Panels(frame), nullptr);
                check(frame, window, ui, world, regions);
            }
        }
    }
}

}

TEST_CASE("one_region_when_scales_equal", "[compose]")
{
    const std::vector<Region> regions = Layout(Size(800, 600), Size(1600, 1200), 2, 2, Panels(Size(800, 600)), nullptr);
    REQUIRE(regions.size() == 1);
    const Region& r = regions[0];
    CHECK_FALSE(r.ui);
    CHECK(r.scale == 2);
    CHECK(r.frame.left == 0);
    CHECK(r.frame.top == 0);
    CHECK(r.frame.right == 800);
    CHECK(r.frame.bottom == 600);
    CHECK(r.window.left == 0);
    CHECK(r.window.top == 0);
    CHECK(r.window.right == 1600);
    CHECK(r.window.bottom == 1200);
    CHECK(Tiles(regions, Size(1600, 1200)));

    // A full-UI screen at equal scales is still the one region.
    const RECT full = Rect(0, 0, 800, 600);
    CHECK(Layout(Size(800, 600), Size(1600, 1200), 2, 2, {}, &full).size() == 1);
}

TEST_CASE("reserved_rects_tile_the_window", "[compose]")
{
    ForEachLayout([](SIZE frame, SIZE window, int ui, int world, const std::vector<Region>& regions) {
        INFO(Describe(window, ui, world));
        if (ui == world) {
            CHECK(regions.size() == 1);
        } else {
            CHECK(regions.size() == 3);
        }
        CHECK(Tiles(regions, window));
        const Region* w = World(regions);
        REQUIRE(w != nullptr);
        for (const Region& r : regions) {
            INFO("region frame " << r.frame.left << "," << r.frame.top << "," << r.frame.right << "," << r.frame.bottom
                                 << " window " << r.window.left << "," << r.window.top << "," << r.window.right << "," << r.window.bottom);
            CHECK(r.scale == (r.ui ? ui : world));
            // Integer edges by construction (LONG); within the window and
            // the frame, and the frame fits its window at the scale.
            CHECK(r.window.left >= 0);
            CHECK(r.window.top >= 0);
            CHECK(r.window.right <= window.cx);
            CHECK(r.window.bottom <= window.cy);
            CHECK(r.frame.left >= 0);
            CHECK(r.frame.top >= 0);
            CHECK(r.frame.right <= frame.cx);
            CHECK(r.frame.bottom <= frame.cy);
            CHECK(r.frame.right > r.frame.left);
            CHECK(r.frame.bottom > r.frame.top);
            CHECK((r.frame.right - r.frame.left) * r.scale <= r.window.right - r.window.left);
            CHECK((r.frame.bottom - r.frame.top) * r.scale <= r.window.bottom - r.window.top);
        }
        if (ui >= world) {
            // The world's slack is under one engine pixel (one band per
            // axis here).
            CHECK((w->window.right - w->window.left) - (w->frame.right - w->frame.left) * world < world);
            CHECK((w->window.bottom - w->window.top) - (w->frame.bottom - w->frame.top) * world < world);
        }
        if (ui != world) {
            // The panels' window bands are exactly their 1x depth at the UI
            // scale, against the window's edges.
            const Region& toolbar = regions[1];
            const Region& left = regions[2];
            CHECK(toolbar.window.bottom == window.cy);
            CHECK(toolbar.window.bottom - toolbar.window.top == 100 * ui);
            CHECK(toolbar.window.left == 0);
            CHECK(toolbar.window.right == window.cx);
            CHECK(left.window.left == 0);
            CHECK(left.window.right == 120 * ui);
            CHECK(left.window.top == 0);
            CHECK(left.window.bottom == toolbar.window.top);
            // The world never covers a reserved band: depth x ui / world
            // rounded up, and never less than the panel itself.
            const int big = ui > world ? ui : world;
            CHECK(w->frame.bottom == frame.cy - (100 * big + world - 1) / world);
            CHECK(w->frame.left == (120 * big + world - 1) / world);
            for (size_t i = 1; i < regions.size(); i++) {
                const RECT& p = regions[i].frame;
                const bool apart = p.right <= w->frame.left || p.left >= w->frame.right || p.bottom <= w->frame.top || p.top >= w->frame.bottom;
                CHECK(apart);
            }
        }
    });
}

TEST_CASE("reserved_rects_tile_at_scale_3_over_2", "[compose]")
{
    // Review Focus 4: a non-integer ratio, and a toolbar depth (101) whose
    // reserved band is not a whole number of engine pixels.
    const SIZE frame = Size(1280, 720);
    const SIZE window = Size(2560, 1440);
    const std::vector<RECT> panels = { Rect(0, 619, 1280, 720), Rect(0, 0, 121, 619) };
    const std::vector<Region> regions = Layout(frame, window, 3, 2, panels, nullptr);
    REQUIRE(regions.size() == 3);
    CHECK(Tiles(regions, window));
    CHECK(regions[0].frame.bottom == 720 - 152); // 101 * 1.5 = 151.5, up to 152
    CHECK(regions[0].frame.left == 182); // 121 * 1.5 = 181.5, up to 182
    CHECK(regions[1].window.top == 1440 - 303);
    CHECK(regions[2].window.right == 363);
}

TEST_CASE("mapping_round_trips", "[compose]")
{
    ForEachLayout([](SIZE, SIZE window, int ui, int world, const std::vector<Region>& regions) {
        INFO(Describe(window, ui, world));
        for (const Region& r : regions) {
            const float l = static_cast<float>(r.frame.left);
            const float t = static_cast<float>(r.frame.top);
            const float rr = static_cast<float>(r.frame.right - 1);
            const float b = static_cast<float>(r.frame.bottom - 1);
            const float points[][2] = { { l, t }, { rr, t }, { l, b }, { rr, b }, { (l + rr) / 2.0f, (t + b) / 2.0f } };
            for (const auto& p : points) {
                INFO("frame point " << p[0] << "," << p[1]);
                float wx = 0.0f;
                float wy = 0.0f;
                REQUIRE(platform::compose::FrameToWindow(regions, p[0], p[1], &wx, &wy));
                float fx = 0.0f;
                float fy = 0.0f;
                REQUIRE(platform::compose::WindowToFrame(regions, wx, wy, &fx, &fy));
                CHECK(std::fabs(fx - p[0]) < 1e-3f);
                CHECK(std::fabs(fy - p[1]) < 1e-3f);
            }
            // From the window side: the region's first pixel and the last
            // pixel it draws, and its centre.
            const float wl = static_cast<float>(r.window.left);
            const float wt = static_cast<float>(r.window.top);
            const float wr = static_cast<float>(r.window.left + (r.frame.right - r.frame.left) * r.scale - 1);
            const float wb = static_cast<float>(r.window.top + (r.frame.bottom - r.frame.top) * r.scale - 1);
            const float wpoints[][2] = { { wl, wt }, { wr, wt }, { wl, wb }, { wr, wb }, { (wl + wr) / 2.0f, (wt + wb) / 2.0f } };
            for (const auto& p : wpoints) {
                INFO("window point " << p[0] << "," << p[1]);
                float fx = 0.0f;
                float fy = 0.0f;
                REQUIRE(platform::compose::WindowToFrame(regions, p[0], p[1], &fx, &fy));
                CHECK(Contains(r.frame, fx, fy));
                float wx = 0.0f;
                float wy = 0.0f;
                REQUIRE(platform::compose::FrameToWindow(regions, fx, fy, &wx, &wy));
                CHECK(std::fabs(wx - p[0]) < 1e-3f);
                CHECK(std::fabs(wy - p[1]) < 1e-3f);
            }
        }
    });
}

TEST_CASE("ui_point_maps_inside_panel", "[compose]")
{
    // 2560x1440 at World Scale 1 with Scale 2: the toolbar is 200 window
    // pixels high at the bottom.
    const SIZE frame = Size(2560, 1440);
    const std::vector<RECT> panels = Panels(frame);
    const std::vector<Region> regions = Layout(frame, frame, 2, 1, panels, nullptr);
    REQUIRE(regions.size() == 3);
    const RECT& toolbar = panels[0];
    float fx = 0.0f;
    float fy = 0.0f;
    REQUIRE(platform::compose::WindowToFrame(regions, 1000.0f, 1300.0f, &fx, &fy));
    CHECK(Contains(toolbar, fx, fy));
    CHECK(fx == 500.0f);
    CHECK(fy == 1340.0f + 30.0f);
    // The toolbar's top-left window pixel is its 1x origin.
    REQUIRE(platform::compose::WindowToFrame(regions, 0.0f, 1240.0f, &fx, &fy));
    CHECK(fx == 0.0f);
    CHECK(fy == 1340.0f);
    // Just above it is the world, at scale 1.
    REQUIRE(platform::compose::WindowToFrame(regions, 1000.0f, 1239.0f, &fx, &fy));
    CHECK(fx == 1000.0f);
    CHECK(fy == 1239.0f);
    // The left bar: a window point maps into its 1x rectangle.
    REQUIRE(platform::compose::WindowToFrame(regions, 239.0f, 10.0f, &fx, &fy));
    CHECK(Contains(panels[1], fx, fy));
    CHECK(fx == 119.5f);
    CHECK(fy == 5.0f);
    // Off the window the world region extends.
    REQUIRE(platform::compose::WindowToFrame(regions, 3000.0f, 100.0f, &fx, &fy));
    CHECK(fx == 3000.0f);

    CHECK_FALSE(platform::compose::WindowToFrame({}, 1.0f, 1.0f, &fx, &fy));
    CHECK_FALSE(platform::compose::FrameToWindow({}, 1.0f, 1.0f, &fx, &fy));
}

TEST_CASE("full_ui_screen_is_centred", "[compose]")
{
    // 2560x1440 at World Scale 1, Scale 2: the 800x600 inventory, drawn
    // centred in the frame, is 1600x1200 centred in the window.
    const SIZE frame = Size(2560, 1440);
    const RECT full = Rect(880, 420, 1680, 1020);
    std::vector<Region> regions = Layout(frame, frame, 2, 1, Panels(frame), &full);
    REQUIRE(regions.size() == 1);
    const Region& r = regions[0];
    CHECK(r.ui);
    CHECK(r.scale == 2);
    CHECK(r.frame.left == 880);
    CHECK(r.frame.top == 420);
    CHECK(r.frame.right == 1680);
    CHECK(r.frame.bottom == 1020);
    CHECK(r.window.left == 480);
    CHECK(r.window.top == 120);
    CHECK(r.window.right == 2080);
    CHECK(r.window.bottom == 1320);
    float fx = 0.0f;
    float fy = 0.0f;
    REQUIRE(platform::compose::WindowToFrame(regions, 1280.0f, 720.0f, &fx, &fy));
    CHECK(fx == 1280.0f);
    CHECK(fy == 720.0f);
    // Outside it the one region still maps (the caller clamps).
    REQUIRE(platform::compose::WindowToFrame(regions, 0.0f, 0.0f, &fx, &fy));
    CHECK(fx == 640.0f);
    CHECK(fy == 360.0f);

    // Too big for the window at Scale 3 (2400x1800 in 2560x1440): cropped
    // about its centre to whole engine pixels, still centred.
    regions = Layout(frame, frame, 3, 1, {}, &full);
    REQUIRE(regions.size() == 1);
    const Region& c = regions[0];
    CHECK(c.frame.right - c.frame.left == 800);
    CHECK(c.frame.bottom - c.frame.top == 480);
    CHECK(c.frame.top == 480);
    CHECK(c.window.top == 0);
    CHECK(c.window.bottom == 1440);
    CHECK(c.window.left == 80);
    CHECK(c.window.right == 2480);
}

TEST_CASE("tiles_rejects_gaps_and_overlaps", "[compose]")
{
    Region a { Rect(0, 0, 100, 50), Rect(0, 0, 100, 50), 1, false };
    Region b { Rect(0, 50, 100, 100), Rect(0, 50, 100, 100), 1, true };
    CHECK(Tiles({ a, b }, Size(100, 100)));
    CHECK_FALSE(Tiles({ a }, Size(100, 100)));
    Region overlap = b;
    overlap.window.top = 49;
    CHECK_FALSE(Tiles({ a, overlap }, Size(100, 100)));
    Region outside = b;
    outside.window.bottom = 101;
    CHECK_FALSE(Tiles({ a, outside }, Size(100, 100)));
    CHECK_FALSE(Tiles({}, Size(100, 100)));
}

TEST_CASE("present maps through the regions when set", "[compose]")
{
    // No window: without regions the mapping divides by WindowScale().
    const int scaleBefore = platform::present::WindowScale();
    platform::present::SetWindowScale(2);
    float fx = 0.0f;
    float fy = 0.0f;
    platform::present::WindowToFrame(100.0f, 100.0f, &fx, &fy);
    CHECK(fx == 50.0f);
    CHECK(fy == 50.0f);

    const SIZE frame = Size(1280, 720);
    platform::present::SetRegions(Layout(frame, Size(2560, 1440), 4, 2, Panels(frame), nullptr));
    // The toolbar is 400 window pixels high at the bottom: (100, 1100) is 60
    // pixels into it, 15 engine pixels below its 1x top.
    platform::present::WindowToFrame(100.0f, 1100.0f, &fx, &fy);
    CHECK(fx == 25.0f);
    CHECK(fy == 620.0f + 15.0f);
    float wx = 0.0f;
    float wy = 0.0f;
    platform::present::FrameToWindow(fx, fy, &wx, &wy);
    CHECK(wx == 100.0f);
    CHECK(wy == 1100.0f);
    int w = 0;
    int h = 0;
    REQUIRE(platform::present::FrameSize(&w, &h));
    CHECK(w == 1280);
    CHECK(h == 720);

    platform::present::SetRegions({});
    platform::present::WindowToFrame(100.0f, 100.0f, &fx, &fy);
    CHECK(fx == 50.0f);
    CHECK_FALSE(platform::present::FrameSize(&w, &h));
    platform::present::SetWindowScale(scaleBefore);
}

TEST_CASE("floating_panel_takes_the_largest_rectangle", "[compose]")
{
#ifndef NDEBUG
    SKIP("a debug build asserts on a non-rectangular world");
#else
    // A panel touching no edge leaves a ring of world around it: a release
    // build takes the largest rectangle (the 800x275 strip below it, at
    // Scale 2 over World Scale 1) and Tiles reports the gaps.
    const std::vector<Region> regions = Layout(Size(800, 600), Size(800, 600), 2, 1, { Rect(350, 250, 450, 300) }, nullptr);
    REQUIRE(regions.size() == 2);
    CHECK_FALSE(regions[0].ui);
    CHECK(regions[0].frame.left == 0);
    CHECK(regions[0].frame.top == 325);
    CHECK(regions[0].frame.right == 800);
    CHECK(regions[0].frame.bottom == 600);
    CHECK(regions[1].window.left == 300);
    CHECK(regions[1].window.top == 225);
    CHECK(regions[1].window.right == 500);
    CHECK(regions[1].window.bottom == 325);
    CHECK_FALSE(Tiles(regions, Size(800, 600)));
#endif
}
