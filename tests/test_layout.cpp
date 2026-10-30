#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "ScreenLayout.h"
#include "platform/compose.h"
#include "platform/present.h"
#include "platform/win32_api.h"
#include "platform/window.h"

#include <SDL3/SDL.h>

#include <cstring>
#include <string>
#include <vector>

using iwd2r::ComputeScreenLayout;
using iwd2r::LayoutRect;
using iwd2r::ResolveScreenSize;
using iwd2r::ScreenLayout;
using iwd2r::ScreenSize;

namespace {

void CheckRect(const LayoutRect& r, int left, int top, int right, int bottom)
{
    CHECK(r.left == left);
    CHECK(r.top == top);
    CHECK(r.right == right);
    CHECK(r.bottom == bottom);
}

void CheckPanel(const iwd2r::BorderPanelLayout& p, int x, int y, int w, int h)
{
    CHECK(p.x == x);
    CHECK(p.y == y);
    CHECK(p.width == w);
    CHECK(p.height == h);
    // At the original sizes the mosaic is drawn once at the panel's origin.
    CHECK(p.anchorX == x);
    CHECK(p.anchorY == y);
}

// The border panels as the constructor computes them after the switch.
void CheckOriginalBorders(const ScreenLayout& l, int logicalW, int logicalH)
{
    const int side = (logicalW - 800) / 2;
    const int band = (logicalH - 600) / 2;
    CheckPanel(l.borderPanels[0], 0, 0, side, logicalH);
    CheckPanel(l.borderPanels[1], side + 800, 0, side, logicalH);
    CheckPanel(l.borderPanels[2], side, 0, 800, band);
    CheckPanel(l.borderPanels[3], side, band + 600, 800, band);
}

void CheckMosaics(const ScreenLayout& l, bool stone)
{
    const char* names[4] = { "STON10L", "STON10R", "STON10T", "STON10B" };
    for (int i = 0; i < 4; i++) {
        CHECK(std::string(l.borderMosaic[i]) == (stone ? names[i] : ""));
    }
}

}

// Expected values copied from CBaldurChitin::CBaldurChitin()'s switch.
TEST_CASE("layout 2048x1536 matches the original table", "[layout]")
{
    const ScreenLayout l = ComputeScreenLayout(2048, 1536);
    CheckRect(l.stru_8E7538, 0, 0, 960, 658);
    CheckRect(l.stru_8E7548, 0, 0, 2048, 1536);
    CheckRect(l.stru_8E7958, 0, 0, 2048, 968);
    CheckRect(l.stru_8E7988, 0, 0, 2048, 1258);
    CheckRect(l.stru_8E79A8, 256, 150, 768, 534);
    CheckRect(l.stru_8E79B8, 0, 0, 2048, 1202);
    CheckRect(l.stru_8E79C8, 0, 0, 2048, 1416);
    CheckRect(l.stru_8E79E0, 0, 0, 2048, 1322);
    CheckRect(l.stru_8E79F8, 0, 0, 2048, 1288);
    CheckRect(l.stru_8E7A10, 0, 0, 960, 522);
    CHECK(l.screenWidth == 2048);
    CHECK(l.screenHeight == 1536);
    CHECK(std::string(l.guiExt) == "10");
    CheckMosaics(l, true);
    CHECK(l.field_4A2C == 1);
    CHECK(l.field_4A28 == 1);
    CHECK(l.logicalWidth == 1024);
    CHECK(l.logicalHeight == 768);
    CheckOriginalBorders(l, 1024, 768);
    CHECK(l.worldUiShiftX == 0);
    CHECK(l.worldUiShiftY == 0);
    CHECK(l.dialogAutoScrollRange == 490000);
}

TEST_CASE("layout 1600x1200 matches the original table", "[layout]")
{
    const ScreenLayout l = ComputeScreenLayout(1600, 1200);
    CheckRect(l.stru_8E7538, 0, 0, 736, 490);
    CheckRect(l.stru_8E7548, 0, 0, 1600, 1200);
    CheckRect(l.stru_8E7958, 0, 0, 1600, 632);
    CheckRect(l.stru_8E7988, 0, 0, 1600, 922);
    CheckRect(l.stru_8E79A8, 544, 366, 1056, 750);
    CheckRect(l.stru_8E79B8, 0, 0, 1600, 866);
    CheckRect(l.stru_8E79C8, 0, 0, 1600, 1080);
    CheckRect(l.stru_8E79E0, 0, 0, 1600, 986);
    CheckRect(l.stru_8E79F8, 0, 0, 1600, 952);
    CheckRect(l.stru_8E7A10, 0, 0, 736, 354);
    CHECK(l.screenWidth == 1600);
    CHECK(l.screenHeight == 1200);
    CHECK(std::string(l.guiExt) == "08");
    CheckMosaics(l, false);
    CHECK(l.field_4A2C == 1);
    CHECK(l.field_4A28 == 1);
    CHECK(l.logicalWidth == 800);
    CHECK(l.logicalHeight == 600);
    CheckOriginalBorders(l, 800, 600);
    CHECK(l.dialogAutoScrollRange == 490000);
}

TEST_CASE("layout 1024x768 matches the original table", "[layout]")
{
    const ScreenLayout l = ComputeScreenLayout(1024, 768);
    CheckRect(l.stru_8E7538, 0, 0, 960, 658);
    CheckRect(l.stru_8E7548, 0, 0, 1024, 768);
    CheckRect(l.stru_8E7958, 0, 0, 1024, 484);
    CheckRect(l.stru_8E7988, 0, 0, 1024, 629);
    CheckRect(l.stru_8E79A8, 256, 150, 768, 534);
    CheckRect(l.stru_8E79B8, 0, 0, 1024, 601);
    CheckRect(l.stru_8E79C8, 0, 0, 1024, 708);
    CheckRect(l.stru_8E79E0, 0, 0, 1024, 661);
    CheckRect(l.stru_8E79F8, 0, 0, 1024, 644);
    CheckRect(l.stru_8E7A10, 0, 0, 960, 522);
    CHECK(l.screenWidth == 1024);
    CHECK(l.screenHeight == 768);
    CHECK(std::string(l.guiExt) == "10");
    CheckMosaics(l, true);
    CHECK(l.field_4A2C == 0);
    CHECK(l.field_4A28 == 0);
    CHECK(l.logicalWidth == 1024);
    CHECK(l.logicalHeight == 768);
    CheckOriginalBorders(l, 1024, 768);
    CHECK(l.borderPanels[0].width == 112);
    CHECK(l.borderPanels[2].height == 84);
    CHECK(l.worldUiShiftX == 0);
    CHECK(l.worldUiShiftY == 0);
    CHECK(l.dialogAutoScrollRange == 490000);
}

TEST_CASE("layout 800x600 matches the original table", "[layout]")
{
    const ScreenLayout l = ComputeScreenLayout(800, 600);
    CheckRect(l.stru_8E7538, 0, 0, 736, 490);
    CheckRect(l.stru_8E7548, 0, 0, 800, 600);
    CheckRect(l.stru_8E7958, 0, 0, 800, 316);
    CheckRect(l.stru_8E7988, 0, 0, 800, 461);
    CheckRect(l.stru_8E79A8, 144, 66, 656, 450);
    CheckRect(l.stru_8E79B8, 0, 0, 800, 433);
    CheckRect(l.stru_8E79C8, 0, 0, 800, 540);
    CheckRect(l.stru_8E79E0, 0, 0, 800, 493);
    CheckRect(l.stru_8E79F8, 0, 0, 800, 476);
    CheckRect(l.stru_8E7A10, 0, 0, 736, 354);
    CHECK(l.screenWidth == 800);
    CHECK(l.screenHeight == 600);
    CHECK(std::string(l.guiExt) == "08");
    CheckMosaics(l, false);
    CHECK(l.field_4A2C == 0);
    CHECK(l.field_4A28 == 0);
    CHECK(l.logicalWidth == 800);
    CHECK(l.logicalHeight == 600);
    CheckOriginalBorders(l, 800, 600);
    CHECK(l.dialogAutoScrollRange == 490000);
}

TEST_CASE("layout at other sizes keeps the UI's pixel sizes and anchoring", "[layout]")
{
    struct Size {
        int w;
        int h;
        const char* ext;
    };
    const std::vector<Size> sizes = {
        { 1280, 720, "08" },
        { 1366, 768, "10" },
        { 1920, 1080, "10" },
        { 2560, 1440, "10" },
        { 1024, 600, "08" },
        { 800, 720, "08" },
        { 1367, 769, "10" },
    };
    for (const Size& s : sizes) {
        INFO(s.w << "x" << s.h);
        const ScreenLayout l = ComputeScreenLayout(s.w, s.h);
        CHECK(l.screenWidth == s.w);
        CHECK(l.screenHeight == s.h);
        CHECK(l.logicalWidth == s.w);
        CHECK(l.logicalHeight == s.h);
        CHECK(l.field_4A28 == 0);
        CHECK(l.field_4A2C == 0);
        CHECK(std::string(l.guiExt) == s.ext);
        CheckMosaics(l, true);

        // World viewports: the full width, the height above the bottom
        // panels (their heights as in the 800x600 and 1024x768 tables).
        CheckRect(l.stru_8E7548, 0, 0, s.w, s.h);
        CheckRect(l.stru_8E79B8, 0, 0, s.w, s.h - 167);
        CheckRect(l.stru_8E7958, 0, 0, s.w, s.h - 284);
        CheckRect(l.stru_8E7988, 0, 0, s.w, s.h - 139);
        CheckRect(l.stru_8E79F8, 0, 0, s.w, s.h - 124);
        // Console and toolbar bottoms keep their offsets from the bottom.
        CheckRect(l.stru_8E79C8, 0, 0, s.w, s.h - 60);
        CheckRect(l.stru_8E79E0, 0, 0, s.w, s.h - 107);
        // The others keep their offsets from the right and bottom edges, the
        // centred 512x384 box its size.
        CheckRect(l.stru_8E7538, 0, 0, s.w - 64, s.h - 110);
        CheckRect(l.stru_8E7A10, 0, 0, s.w - 64, s.h - 246);
        CHECK(l.stru_8E79A8.right - l.stru_8E79A8.left == 512);
        CHECK(l.stru_8E79A8.bottom - l.stru_8E79A8.top == 384);
        CHECK(l.stru_8E79A8.left == (s.w - 512) / 2);

        // Border panels cover exactly the margins around the centred
        // 800x600 UI, never negative.
        const int ux = (s.w - 800) / 2;
        const int uy = (s.h - 600) / 2;
        const auto& left = l.borderPanels[iwd2r::BORDER_LEFT];
        const auto& right = l.borderPanels[iwd2r::BORDER_RIGHT];
        const auto& top = l.borderPanels[iwd2r::BORDER_TOP];
        const auto& bottom = l.borderPanels[iwd2r::BORDER_BOTTOM];
        for (const auto& p : l.borderPanels) {
            CHECK(p.x >= 0);
            CHECK(p.y >= 0);
            CHECK(p.width >= 0);
            CHECK(p.height >= 0);
        }
        CHECK(left.x == 0);
        CHECK(left.y == 0);
        CHECK(left.width == ux);
        CHECK(left.height == s.h);
        CHECK(right.x == ux + 800);
        CHECK(right.x + right.width == s.w);
        CHECK(right.height == s.h);
        CHECK(top.x == ux);
        CHECK(top.y == 0);
        CHECK(top.width == 800);
        CHECK(top.height == uy);
        CHECK(bottom.x == ux);
        CHECK(bottom.y == uy + 600);
        CHECK(bottom.width == 800);
        CHECK(bottom.y + bottom.height == s.h);
        // Total area: the screen minus the UI.
        long area = 0;
        for (const auto& p : l.borderPanels) {
            area += static_cast<long>(p.width) * p.height;
        }
        CHECK(area == static_cast<long>(s.w) * s.h - 800L * 600L);

        // The mosaics sit where they would around the UI at 1024x768.
        CHECK(left.anchorX == ux - 112);
        CHECK(left.anchorY == uy - 84);
        CHECK(top.anchorY == uy - 84);
        CHECK(bottom.anchorY == uy + 600);

        // World panels bottom-centred.
        const int ww = s.ext[0] == '1' ? 1024 : 800;
        const int wh = s.ext[0] == '1' ? 768 : 600;
        CHECK(l.worldUiShiftX == (s.w - ww) / 2);
        CHECK(l.worldUiShiftY == s.h - wh);

        // IEex: (w / 2)^2 + (h / 2)^2.
        CHECK(l.dialogAutoScrollRange == static_cast<long>((s.w / 2.0) * (s.w / 2.0) + (s.h / 2.0) * (s.h / 2.0)));
    }
    CHECK(ComputeScreenLayout(1920, 1080).dialogAutoScrollRange == 960L * 960 + 540L * 540);
}

TEST_CASE("layout mosaic tiles cover a rectangle from the anchor", "[layout]")
{
    std::vector<std::pair<int, int>> at;
    auto collect = [&](int x, int y) { at.push_back({ x, y }); };

    // The 1024x768 left panel: one copy at the origin.
    iwd2r::ForEachMosaicTile(0, 0, 112, 768, 0, 0, 112, 768, collect);
    REQUIRE(at.size() == 1);
    CHECK(at[0] == std::make_pair(0, 0));

    // 1920x1080 left panel (560 wide), anchored at (448, 156).
    at.clear();
    iwd2r::ForEachMosaicTile(448, 156, 112, 768, 0, 0, 560, 1080, collect);
    CHECK(at.size() == 5 * 3);
    CHECK(at.front() == std::make_pair(0, 156 - 768));
    CHECK(at.back() == std::make_pair(448, 156 + 768));

    // A clip that starts right of the anchor.
    at.clear();
    iwd2r::ForEachMosaicTile(10, 0, 100, 100, 250, 0, 260, 50, collect);
    REQUIRE(at.size() == 1);
    CHECK(at[0] == std::make_pair(210, 0));

    at.clear();
    iwd2r::ForEachMosaicTile(0, 0, 100, 100, 5, 5, 5, 50, collect);
    CHECK(at.empty());
}

TEST_CASE("layout parses Resolution, Height and Scale", "[layout]")
{
    CHECK(iwd2r::ParseResolution("1366") == 1366);
    CHECK(iwd2r::ParseResolution(" 800 ") == 800);
    CHECK(iwd2r::ParseResolution("") == 800);
    CHECK(iwd2r::ParseResolution(NULL) == 800);
    CHECK(iwd2r::ParseResolution("desktop") == iwd2r::kResolutionDesktop);
    CHECK(iwd2r::ParseResolution("Desktop") == iwd2r::kResolutionDesktop);
    CHECK(iwd2r::ParseResolution("wide") == iwd2r::kResolutionInvalid);
    CHECK(iwd2r::ParseResolution("-800") == iwd2r::kResolutionInvalid);
    CHECK(iwd2r::ParseResolution("0") == iwd2r::kResolutionInvalid);

    CHECK(iwd2r::ParseHeight("") == 0);
    CHECK(iwd2r::ParseHeight("720") == 720);
    CHECK(iwd2r::ParseHeight("x") == -1);

    CHECK(iwd2r::ParseScale("") == 1);
    CHECK(iwd2r::ParseScale("2") == 2);
    CHECK(iwd2r::ParseScale("4") == 4);
    CHECK(iwd2r::ParseScale("auto") == iwd2r::kScaleAuto);
    CHECK(iwd2r::ParseScale("AUTO") == iwd2r::kScaleAuto);
    CHECK(iwd2r::ParseScale("5") == iwd2r::kScaleInvalid);
    CHECK(iwd2r::ParseScale("0") == iwd2r::kScaleInvalid);
}

TEST_CASE("layout resolves the screen size", "[layout]")
{
    auto Check = [](const ScreenSize& r, int ow, int oh, int scale, int w, int h) {
        REQUIRE(r.ok);
        CHECK(r.outputWidth == ow);
        CHECK(r.outputHeight == oh);
        CHECK(r.scale == scale);
        CHECK(r.width == w);
        CHECK(r.height == h);
    };

    // Original widths take their 4:3 heights whatever the desktop.
    Check(ResolveScreenSize(800, 0, 1, 1920, 1080), 800, 600, 1, 800, 600);
    Check(ResolveScreenSize(1024, 0, 1, 1920, 1080), 1024, 768, 1, 1024, 768);
    Check(ResolveScreenSize(1600, 0, 1, 1920, 1080), 1600, 1200, 1, 1600, 1200);
    Check(ResolveScreenSize(2048, 0, 1, 1920, 1080), 2048, 1536, 1, 2048, 1536);
    // Other widths without Height take the desktop's aspect.
    Check(ResolveScreenSize(1366, 0, 1, 1920, 1080), 1366, 768, 1, 1366, 768);
    Check(ResolveScreenSize(1280, 0, 1, 2560, 1440), 1280, 720, 1, 1280, 720);
    Check(ResolveScreenSize(1280, 0, 1, 1920, 1200), 1280, 800, 1, 1280, 800);
    // 4:3 when the desktop is unknown.
    Check(ResolveScreenSize(1280, 0, 1, 0, 0), 1280, 960, 1, 1280, 960);
    // Height wins.
    Check(ResolveScreenSize(1280, 720, 1, 1024, 768), 1280, 720, 1, 1280, 720);
    Check(ResolveScreenSize(1024, 600, 1, 1920, 1080), 1024, 600, 1, 1024, 600);
    // desktop.
    Check(ResolveScreenSize(iwd2r::kResolutionDesktop, 0, 1, 1920, 1080), 1920, 1080, 1, 1920, 1080);
    Check(ResolveScreenSize(iwd2r::kResolutionDesktop, 1000, 1, 1920, 1080), 1920, 1000, 1, 1920, 1000);
    CHECK_FALSE(ResolveScreenSize(iwd2r::kResolutionDesktop, 0, 1, 0, 0).ok);

    // Below the minimum.
    const ScreenSize small = ResolveScreenSize(640, 480, 1, 1920, 1080);
    CHECK_FALSE(small.ok);
    CHECK(small.error.find("800x600") != std::string::npos);
    CHECK_FALSE(ResolveScreenSize(800, 599, 1, 1920, 1080).ok);
    CHECK_FALSE(ResolveScreenSize(799, 600, 1, 1920, 1080).ok);
    CHECK_FALSE(ResolveScreenSize(iwd2r::kResolutionInvalid, 0, 1, 1920, 1080).ok);
    CHECK_FALSE(ResolveScreenSize(1280, -1, 1, 1920, 1080).ok);
    CHECK_FALSE(ResolveScreenSize(1280, 720, iwd2r::kScaleInvalid, 1920, 1080).ok);
}

TEST_CASE("layout resolves the scale", "[layout]")
{
    // Scale 2: the engine sees half the output.
    ScreenSize r = ResolveScreenSize(2560, 1440, 2, 2560, 1440);
    REQUIRE(r.ok);
    CHECK(r.scale == 2);
    CHECK(r.width == 1280);
    CHECK(r.height == 720);
    CHECK(r.outputWidth == 2560);
    CHECK(r.outputHeight == 1440);

    // Scale 2 below the minimum inside.
    r = ResolveScreenSize(1920, 1080, 2, 1920, 1080);
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("960x540") != std::string::npos);
    CHECK_FALSE(ResolveScreenSize(1366, 768, 2, 1920, 1080).ok);

    // auto: the largest factor keeping the internal height >= 720 and the
    // internal size >= 800x600.
    r = ResolveScreenSize(1920, 1080, iwd2r::kScaleAuto, 1920, 1080);
    REQUIRE(r.ok);
    CHECK(r.scale == 1);
    r = ResolveScreenSize(2560, 1440, iwd2r::kScaleAuto, 2560, 1440);
    REQUIRE(r.ok);
    CHECK(r.scale == 2);
    CHECK(r.width == 1280);
    CHECK(r.height == 720);
    r = ResolveScreenSize(3840, 2160, iwd2r::kScaleAuto, 3840, 2160);
    REQUIRE(r.ok);
    CHECK(r.scale == 3);
    CHECK(r.width == 1280);
    CHECK(r.height == 720);
    r = ResolveScreenSize(1366, 768, iwd2r::kScaleAuto, 1920, 1080);
    REQUIRE(r.ok);
    CHECK(r.scale == 1);
    CHECK(r.width == 1366);
    r = ResolveScreenSize(iwd2r::kResolutionDesktop, 0, iwd2r::kScaleAuto, 3840, 2160);
    REQUIRE(r.ok);
    CHECK(r.scale == 3);
    // 800x600 never scales.
    r = ResolveScreenSize(800, 0, iwd2r::kScaleAuto, 1920, 1080);
    REQUIRE(r.ok);
    CHECK(r.scale == 1);

    // The internal size is never below 800x600.
    for (int w = 800; w <= 4096; w += 97) {
        for (int h = 600; h <= 2400; h += 89) {
            for (int s = 0; s <= 4; s++) {
                const ScreenSize x = ResolveScreenSize(w, h, s, 1920, 1080);
                if (x.ok) {
                    CHECK(x.width >= 800);
                    CHECK(x.height >= 600);
                    CHECK(x.outputWidth == x.width * x.scale);
                }
            }
        }
    }
}

namespace {

using platform::compose::Region;

bool SameLayoutRect(const LayoutRect& a, const LayoutRect& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

// Every field the engine reads.
void CheckSameLayout(const ScreenLayout& a, const ScreenLayout& b)
{
    CHECK(a.screenWidth == b.screenWidth);
    CHECK(a.screenHeight == b.screenHeight);
    CHECK(SameLayoutRect(a.stru_8E7538, b.stru_8E7538));
    CHECK(SameLayoutRect(a.stru_8E7548, b.stru_8E7548));
    CHECK(SameLayoutRect(a.stru_8E7958, b.stru_8E7958));
    CHECK(SameLayoutRect(a.stru_8E7988, b.stru_8E7988));
    CHECK(SameLayoutRect(a.stru_8E79A8, b.stru_8E79A8));
    CHECK(SameLayoutRect(a.stru_8E79B8, b.stru_8E79B8));
    CHECK(SameLayoutRect(a.stru_8E79C8, b.stru_8E79C8));
    CHECK(SameLayoutRect(a.stru_8E79E0, b.stru_8E79E0));
    CHECK(SameLayoutRect(a.stru_8E79F8, b.stru_8E79F8));
    CHECK(SameLayoutRect(a.stru_8E7A10, b.stru_8E7A10));
    CHECK(std::string(a.guiExt) == b.guiExt);
    for (int i = 0; i < 4; i++) {
        CHECK(std::string(a.borderMosaic[i]) == b.borderMosaic[i]);
        CHECK(a.borderPanels[i].x == b.borderPanels[i].x);
        CHECK(a.borderPanels[i].y == b.borderPanels[i].y);
        CHECK(a.borderPanels[i].width == b.borderPanels[i].width);
        CHECK(a.borderPanels[i].height == b.borderPanels[i].height);
        CHECK(a.borderPanels[i].anchorX == b.borderPanels[i].anchorX);
        CHECK(a.borderPanels[i].anchorY == b.borderPanels[i].anchorY);
    }
    CHECK(a.field_4A28 == b.field_4A28);
    CHECK(a.field_4A2C == b.field_4A2C);
    CHECK(a.logicalWidth == b.logicalWidth);
    CHECK(a.logicalHeight == b.logicalHeight);
    CHECK(a.worldUiShiftX == b.worldUiShiftX);
    CHECK(a.worldUiShiftY == b.worldUiShiftY);
    CHECK(a.dialogAutoScrollRange == b.dialogAutoScrollRange);
}

bool Inside(const RECT& r, float x, float y)
{
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

const Region* WorldRegion(const std::vector<Region>& regions)
{
    for (const Region& r : regions) {
        if (!r.ui) {
            return &r;
        }
    }
    return nullptr;
}

const Region* UiRegion(const std::vector<Region>& regions)
{
    for (const Region& r : regions) {
        if (r.ui) {
            return &r;
        }
    }
    return nullptr;
}

}

TEST_CASE("default_world_scale_equals_scale", "[layout]")
{
    const int sizes[][2] = { { 800, 600 }, { 1024, 768 }, { 1600, 1200 }, { 2048, 1536 }, { 1920, 1080 }, { 1366, 768 } };
    for (const auto& size : sizes) {
        for (int s = 1; s <= iwd2r::kMaxScale; s++) {
            INFO(size[0] << "x" << size[1] << " at scale " << s);
            // The same layout as ever (the original tables at the four sizes,
            // pinned value by value above).
            const ScreenLayout l = ComputeScreenLayout(size[0], size[1], s, s);
            CheckSameLayout(l, ComputeScreenLayout(size[0], size[1]));
            CHECK(l.uiScale == s);
            CHECK(l.worldScale == s);
            CHECK(l.uiWidth == size[0]);
            CHECK(l.uiHeight == size[1]);

            // One region, the whole frame on the whole window, on every
            // screen.
            const SIZE window = { size[0] * s, size[1] * s };
            for (iwd2r::ScreenKind kind : { iwd2r::SCREEN_FULL_UI, iwd2r::SCREEN_WORLD }) {
                const std::vector<Region> regions = iwd2r::RegionsFor(l, kind, l.stru_8E79B8.bottom, window);
                REQUIRE(regions.size() == 1);
                CHECK(regions[0].frame.right == size[0]);
                CHECK(regions[0].frame.bottom == size[1]);
                CHECK(regions[0].window.right == window.cx);
                CHECK(regions[0].window.bottom == window.cy);
                CHECK(regions[0].scale == s);
            }
        }
    }

    // World Scale absent: the screen size as ever; the scales recorded.
    const ScreenSize a = ResolveScreenSize(2560, 1440, 2, 2560, 1440);
    const ScreenSize b = ResolveScreenSize(2560, 1440, 2, 2560, 1440, "");
    for (const ScreenSize& r : { a, b }) {
        REQUIRE(r.ok);
        CHECK(r.scale == 2);
        CHECK(r.uiScale == 2);
        CHECK(r.worldScale == 2);
        CHECK(r.width == 1280);
        CHECK(r.height == 720);
        CHECK(r.outputWidth == 2560);
    }
}

TEST_CASE("world_viewport_excludes_reserved_rects", "[layout]")
{
    // 1920x1080 at Scale=2, World Scale=1: the engine sees 1920x1080.
    const ScreenSize size = ResolveScreenSize(1920, 1080, 2, 1920, 1080, "1");
    REQUIRE(size.ok);
    CHECK(size.width == 1920);
    CHECK(size.height == 1080);
    CHECK(size.scale == 1);
    CHECK(size.uiScale == 2);
    CHECK(size.worldScale == 1);
    CHECK(size.outputWidth == 1920);

    const ScreenLayout l = ComputeScreenLayout(1920, 1080, 2, 1);
    CHECK(l.uiWidth == 960);
    CHECK(l.uiHeight == 540);
    // The UI at 2x has 960x540 frame pixels: the 800x600 panels (GUIW08),
    // centred in the band's 960 columns, bottom-anchored.
    CHECK(std::string(l.guiExt) == "08");
    CHECK(l.field_4A28 == 0);
    CHECK(l.field_4A2C == 0);
    CHECK(l.worldUiShiftX == 80);
    CHECK(l.worldUiShiftY == 480);
    CHECK(l.worldUiShiftX + 800 <= l.uiWidth);

    // The toolbar (167 deep) reserves twice its depth.
    CheckRect(l.stru_8E79B8, 0, 0, 1920, 1080 - 2 * 167);
    CheckRect(l.stru_8E7958, 0, 0, 1920, 1080 - 2 * 284);
    CheckRect(l.stru_8E7988, 0, 0, 1920, 1080 - 2 * 139);
    CheckRect(l.stru_8E79F8, 0, 0, 1920, 1080 - 2 * 124);
    CheckRect(l.stru_8E7548, 0, 0, 1920, 1080);
    REQUIRE(l.uiPanelRects.size() == 1);
    CHECK(l.uiPanelRects[0].top == 1080 - 167);
    CHECK(l.uiPanelRects[0].bottom == 1080);
    CHECK(l.uiPanelRects[0].left == 0);
    CHECK(l.uiPanelRects[0].right == 1920);

    const SIZE window = { 1920, 1080 };
    const std::vector<Region> regions = iwd2r::RegionsFor(l, iwd2r::SCREEN_WORLD, l.stru_8E79B8.bottom, window);
    REQUIRE(regions.size() == 2);
    CHECK(platform::compose::Tiles(regions, window));
    const Region* world = WorldRegion(regions);
    const Region* ui = UiRegion(regions);
    REQUIRE(world != nullptr);
    REQUIRE(ui != nullptr);
    // The world: the viewport at 1x, its bottom the toolbar band's top.
    CHECK(world->scale == 1);
    CHECK(world->frame.top == 0);
    CHECK(world->frame.right == 1920);
    CHECK(world->frame.bottom == l.stru_8E79B8.bottom);
    CHECK(world->window.bottom == l.stru_8E79B8.bottom);
    // The toolbar band: 2 x 167 window rows across the window, showing the
    // band's first 960 columns (the centred panels) at 2x.
    CHECK(ui->scale == 2);
    CHECK(ui->window.top == world->window.bottom);
    CHECK(ui->window.bottom - ui->window.top == 2 * 167);
    CHECK(ui->window.left == 0);
    CHECK(ui->window.right == 1920);
    CHECK(ui->frame.left == 0);
    CHECK(ui->frame.right == 960);
    CHECK(ui->frame.top == 1080 - 167);
    CHECK(ui->frame.bottom == 1080);

    // The dialog panel's viewport reserves its own depth; the hidden
    // interface none.
    const std::vector<Region> dialog = iwd2r::RegionsFor(l, iwd2r::SCREEN_WORLD, l.stru_8E7958.bottom, window);
    REQUIRE(dialog.size() == 2);
    CHECK(WorldRegion(dialog)->frame.bottom == 1080 - 2 * 284);
    CHECK(UiRegion(dialog)->frame.top == 1080 - 284);
    CHECK(platform::compose::Tiles(dialog, window));
    const std::vector<Region> hidden = iwd2r::RegionsFor(l, iwd2r::SCREEN_WORLD, l.stru_8E7548.bottom, window);
    REQUIRE(hidden.size() == 1);
    CHECK_FALSE(hidden[0].ui);
    CHECK(hidden[0].frame.bottom == 1080);

    // A full-UI screen: the 800x600 UI does not fit 1080 rows at 2x, so
    // it is shown at the world scale, the whole frame as ever.
    CHECK(l.worldUiScale == 2);
    CHECK(l.fullUiScale == 1);
    CHECK(size.worldUiScale == 2);
    CHECK(size.fullUiScale == 1);
    CHECK(size.notices.size() == 1);
    CHECK(l.fullUiRect.left == 560);
    CHECK(l.fullUiRect.top == 240);
    CHECK(l.fullUiRect.right == 1360);
    CHECK(l.fullUiRect.bottom == 840);
    const std::vector<Region> full = iwd2r::RegionsFor(l, iwd2r::SCREEN_FULL_UI, 0, window);
    REQUIRE(full.size() == 1);
    CHECK_FALSE(full[0].ui);
    CHECK(full[0].scale == 1);
    CHECK(full[0].frame.right == 1920);
    CHECK(full[0].window.right == 1920);
    CHECK(full[0].window.bottom == 1080);

    // At 2560x1440 it fits at 2x: centred.
    const ScreenLayout q = ComputeScreenLayout(2560, 1440, 2, 1);
    CHECK(q.fullUiScale == 2);
    const SIZE qwin = { 2560, 1440 };
    const std::vector<Region> qfull = iwd2r::RegionsFor(q, iwd2r::SCREEN_FULL_UI, 0, qwin);
    REQUIRE(qfull.size() == 1);
    CHECK(qfull[0].ui);
    CHECK(qfull[0].scale == 2);
    CHECK(qfull[0].frame.left == 880);
    CHECK(qfull[0].frame.top == 420);
    CHECK(qfull[0].frame.right == 1680);
    CHECK(qfull[0].frame.bottom == 1020);
    CHECK(qfull[0].window.left == 480);
    CHECK(qfull[0].window.top == 120);
    CHECK(qfull[0].window.right == 2080);
    CHECK(qfull[0].window.bottom == 1320);

    // The clamps the rulings name: 1920x1080 at 3/1 shows the world
    // screen's panels at 2; 1366x768 at 2/1 at 1, which is the world
    // scale, so the layout and the regions are the old ones.
    const ScreenSize s31 = ResolveScreenSize(1920, 1080, 3, 1920, 1080, "1");
    REQUIRE(s31.ok);
    CHECK(s31.worldUiScale == 2);
    CHECK(s31.fullUiScale == 1);
    CHECK(ComputeScreenLayout(1920, 1080, 3, 1).worldUiScale == 2);
    const ScreenSize s1366 = ResolveScreenSize(1366, 768, 2, 1366, 768, "1");
    REQUIRE(s1366.ok);
    CHECK(s1366.worldUiScale == 1);
    CHECK(s1366.fullUiScale == 1);
    CHECK(s1366.notices.size() == 2);
    const ScreenLayout l1366 = ComputeScreenLayout(1366, 768, 2, 1);
    CheckSameLayout(l1366, ComputeScreenLayout(1366, 768, 1, 1));
    const SIZE w1366 = { 1366, 768 };
    CHECK(iwd2r::RegionsFor(l1366, iwd2r::SCREEN_WORLD, l1366.stru_8E79B8.bottom, w1366).size() == 1);

    // Every UI scale over every world scale at the common sizes: the
    // world viewports clear the reserved bands, the regions tile, and the
    // world screen's toolbar lies inside the columns its region shows.
    const int sizes[][2] = { { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 }, { 1600, 1200 }, { 1366, 768 } };
    for (const auto& output : sizes) {
        for (int uiScale = 1; uiScale <= 4; uiScale++) {
            for (int ws = 1; ws <= 4; ws++) {
                const ScreenSize r = ResolveScreenSize(output[0], output[1], uiScale, output[0], output[1], std::to_string(ws).c_str());
                if (!r.ok) {
                    continue;
                }
                INFO(output[0] << "x" << output[1] << " ui " << uiScale << " world " << ws);
                CHECK(r.worldScale <= r.uiScale);
                CHECK(r.worldUiScale >= r.worldScale);
                CHECK(r.worldUiScale <= r.uiScale);
                const ScreenLayout x = ComputeScreenLayout(r.width, r.height, r.uiScale, r.worldScale);
                CHECK(x.worldUiScale == r.worldUiScale);
                CHECK(x.fullUiScale == r.fullUiScale);
                CHECK(x.worldUiShiftX >= 0);
                const SIZE win = { r.outputWidth, r.outputHeight };
                const std::vector<Region> rs = iwd2r::RegionsFor(x, iwd2r::SCREEN_WORLD, x.stru_8E79B8.bottom, win);
                CHECK(platform::compose::Tiles(rs, win));
                const Region* w = WorldRegion(rs);
                REQUIRE(w != nullptr);
                if (x.fullUiScale == r.worldScale) {
                    CHECK(platform::compose::Tiles(iwd2r::RegionsFor(x, iwd2r::SCREEN_FULL_UI, 0, win), win));
                }
                if (x.worldUiScale == r.worldScale) {
                    // The old layout and the whole frame.
                    CHECK(rs.size() == 1);
                    CHECK(w->frame.bottom == r.height);
                    continue;
                }
                CHECK(x.stru_8E79B8.bottom == r.height - iwd2r::ReservedDepth(167, x.worldUiScale, r.worldScale));
                CHECK(w->frame.bottom == x.stru_8E79B8.bottom);
                const Region* u = UiRegion(rs);
                REQUIRE(u != nullptr);
                const int panelWidth = std::string(x.guiExt) == "10" ? 1024 : 800;
                const RECT toolbar = { x.worldUiShiftX, r.height - 167, x.worldUiShiftX + panelWidth, r.height };
                CHECK(toolbar.left >= u->frame.left);
                CHECK(toolbar.right <= u->frame.right);
                CHECK(toolbar.top >= u->frame.top);
                CHECK(toolbar.bottom <= u->frame.bottom);
            }
        }
    }
}

namespace {

// The world screen's panels as GUIW08.CHU and GUIW10.CHU define them (read
// with tools/bif.py; every control lies inside its panel), each with the
// world viewport CScreenWorld shows it over: the toolbar (comm 0 or 22 with
// the action bar 1; chat 6 in comm's place; the multiplayer bar 9), the
// container (8), death (17) and dialog (7, 19, 21) panels. The hidden
// interface shows none.
enum WorldViewport {
    VIEW_TOOLBAR,
    VIEW_CONTAINER,
    VIEW_DEATH,
    VIEW_DIALOG,
};

struct ChuPanel {
    int id;
    int x;
    int y;
    int w;
    int h;
    WorldViewport view;
};

const ChuPanel kGuiw08[] = {
    { 0, 0, 493, 800, 107, VIEW_TOOLBAR },
    { 1, 0, 433, 800, 60, VIEW_TOOLBAR },
    { 7, 0, 316, 800, 284, VIEW_DIALOG },
    { 8, 0, 476, 800, 124, VIEW_CONTAINER },
    { 9, 0, 568, 800, 32, VIEW_TOOLBAR },
    { 6, 0, 555, 800, 45, VIEW_TOOLBAR },
    { 17, 0, 461, 800, 139, VIEW_DEATH },
    { 19, 0, 316, 800, 284, VIEW_DIALOG },
    { 21, 0, 316, 800, 284, VIEW_DIALOG },
    { 22, 0, 493, 800, 107, VIEW_TOOLBAR },
};

const ChuPanel kGuiw10[] = {
    { 0, 0, 661, 1024, 107, VIEW_TOOLBAR },
    { 1, 0, 601, 1024, 60, VIEW_TOOLBAR },
    { 7, 0, 484, 1024, 284, VIEW_DIALOG },
    { 8, 0, 644, 1024, 124, VIEW_CONTAINER },
    { 9, 112, 736, 786, 32, VIEW_TOOLBAR },
    { 6, 112, 723, 912, 45, VIEW_TOOLBAR },
    { 17, 0, 629, 1024, 139, VIEW_DEATH },
    { 19, 0, 484, 1024, 284, VIEW_DIALOG },
    { 21, 0, 484, 1024, 284, VIEW_DIALOG },
    { 22, 0, 661, 1024, 107, VIEW_TOOLBAR },
};

int ViewportBottom(const ScreenLayout& l, WorldViewport view)
{
    switch (view) {
    case VIEW_TOOLBAR:
        return l.stru_8E79B8.bottom;
    case VIEW_CONTAINER:
        return l.stru_8E79F8.bottom;
    case VIEW_DEATH:
        return l.stru_8E7988.bottom;
    case VIEW_DIALOG:
        return l.stru_8E7958.bottom;
    }
    return l.screenHeight;
}

bool Inside(const RECT& inner, const RECT& outer)
{
    return inner.left >= outer.left && inner.right <= outer.right && inner.top >= outer.top && inner.bottom <= outer.bottom;
}

}

TEST_CASE("world_panels_lie_in_the_band", "[layout]")
{
    // Every GUIW08/GUIW10 panel, where the world screen puts it (shifted by
    // worldUiShiftX/Y, doubled where the original draws the world UI double
    // size), lies inside the band WorldPanelRects gives for its viewport and
    // inside the region that shows it: the UI region at the UI scale when
    // the scales differ (nothing in the rows between the band's top and the
    // world viewport's bottom, which no region shows), the whole frame
    // otherwise.
    struct Case {
        int width;
        int height;
        int ui;
        int world;
    };
    const Case cases[] = {
        { 800, 600, 1, 1 },
        { 1024, 768, 1, 1 },
        { 1600, 1200, 1, 1 },
        { 2048, 1536, 1, 1 },
        { 1920, 1080, 1, 1 },
        { 1920, 1080, 2, 1 },
        { 1920, 1200, 2, 1 },
        { 2560, 1440, 2, 1 },
        { 2560, 1440, 3, 1 },
        { 3840, 2160, 2, 1 },
        { 3840, 2160, 4, 1 },
        { 3840, 2160, 4, 2 },
        { 1366, 768, 1, 1 },
    };
    for (const Case& c : cases) {
        const ScreenLayout l = ComputeScreenLayout(c.width, c.height, c.ui, c.world);
        const bool ten = std::string(l.guiExt) == "10";
        const int d = l.field_4A2C ? 2 : 1;
        const SIZE window = { c.width * c.world, c.height * c.world };
        for (const ChuPanel& p : ten ? kGuiw10 : kGuiw08) {
            INFO(c.width << "x" << c.height << " ui " << c.ui << " world " << c.world << " GUIW" << l.guiExt << " panel " << p.id);
            const RECT panel = { p.x * d + l.worldUiShiftX, p.y * d + l.worldUiShiftY, (p.x + p.w) * d + l.worldUiShiftX, (p.y + p.h) * d + l.worldUiShiftY };
            const int bottom = ViewportBottom(l, p.view);
            const std::vector<RECT> band = iwd2r::WorldPanelRects(l, bottom);
            REQUIRE(band.size() == 1);
            CHECK(Inside(panel, band[0]));
            const std::vector<Region> regions = iwd2r::RegionsFor(l, iwd2r::SCREEN_WORLD, bottom, window);
            const Region* ui = UiRegion(regions);
            if (l.worldUiScale == l.worldScale) {
                REQUIRE(regions.size() == 1);
                CHECK(Inside(panel, regions[0].frame));
            } else {
                REQUIRE(ui != nullptr);
                CHECK(ui->scale == l.worldUiScale);
                CHECK(Inside(panel, ui->frame));
            }
        }
    }
}

TEST_CASE("auto_world_scale", "[layout]")
{
    CHECK(iwd2r::ResolveWorldScale(NULL, 2) == 2);
    CHECK(iwd2r::ResolveWorldScale("", 3) == 3);
    CHECK(iwd2r::ResolveWorldScale(" ", 1) == 1);
    CHECK(iwd2r::ResolveWorldScale("auto", 1) == 1);
    CHECK(iwd2r::ResolveWorldScale("auto", 2) == 1);
    CHECK(iwd2r::ResolveWorldScale("AUTO", 4) == 1);
    CHECK(iwd2r::ResolveWorldScale("3", 1) == 3);
    CHECK(iwd2r::ResolveWorldScale("1", 2) == 1);
    CHECK(iwd2r::ResolveWorldScale("5", 2) == iwd2r::kScaleInvalid);
    CHECK(iwd2r::ResolveWorldScale("0", 2) == iwd2r::kScaleInvalid);
    CHECK(iwd2r::ResolveWorldScale("x", 2) == iwd2r::kScaleInvalid);

    // Scale=auto at 2560x1440 gives 2; World Scale=auto then 1: the world at
    // the desktop's resolution, the UI at 2x.
    ScreenSize r = ResolveScreenSize(2560, 1440, iwd2r::kScaleAuto, 2560, 1440, "auto");
    REQUIRE(r.ok);
    CHECK(r.uiScale == 2);
    CHECK(r.worldScale == 1);
    CHECK(r.scale == 1);
    CHECK(r.width == 2560);
    CHECK(r.height == 1440);
    // Scale 1: auto keeps 1.
    r = ResolveScreenSize(1920, 1080, 1, 1920, 1080, "auto");
    REQUIRE(r.ok);
    CHECK(r.uiScale == 1);
    CHECK(r.worldScale == 1);
    // World Scale above Scale is clamped to it, with a notice.
    r = ResolveScreenSize(1920, 1080, 1, 1920, 1080, "2");
    REQUIRE(r.ok);
    CHECK(r.worldScale == 1);
    CHECK(r.width == 1920);
    REQUIRE(r.notices.size() == 1);
    CHECK(r.notices[0].find("World Scale=2") != std::string::npos);
    r = ResolveScreenSize(3840, 2160, 1, 3840, 2160, "2");
    REQUIRE(r.ok);
    CHECK(r.worldScale == 1);
    CHECK(ComputeScreenLayout(3840, 2160, 1, 2).worldScale == 1);
    // A world scale leaving under 800x600 inside is an error naming it.
    r = ResolveScreenSize(1920, 1080, 4, 1920, 1080, "2");
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("World Scale=2") != std::string::npos);
    CHECK_FALSE(ResolveScreenSize(1920, 1080, 2, 1920, 1080, "9").ok);
    // Defaults print nothing.
    CHECK(ResolveScreenSize(2560, 1440, 2, 2560, 1440).notices.empty());
    // Scale 2 alone is still too small at 1920x1080; with World Scale=1 it
    // runs.
    CHECK_FALSE(ResolveScreenSize(1920, 1080, 2, 1920, 1080).ok);
    CHECK(ResolveScreenSize(1920, 1080, 2, 1920, 1080, "1").ok);

    // Reserved depths: whole pixels, never under the panel, at 3 over 2.
    CHECK(iwd2r::ReservedDepth(167, 2, 1) == 334);
    CHECK(iwd2r::ReservedDepth(167, 3, 2) == 251);
    CHECK(iwd2r::ReservedDepth(167, 1, 2) == 167);
    CHECK(iwd2r::ReservedDepth(0, 2, 1) == 0);
}

namespace {

ScreenLayout& ResizeLayout()
{
    static ScreenLayout l = ComputeScreenLayout(1920, 1080, 2, 1);
    return l;
}

std::vector<Region> ResizeSource(SIZE window)
{
    return iwd2r::RegionsFor(ResizeLayout(), iwd2r::SCREEN_WORLD, ResizeLayout().stru_8E79B8.bottom, window);
}

LRESULT CALLBACK IgnoringProc(HWND, UINT, WPARAM, LPARAM) { return 0; }

void DrainMessages()
{
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE) && m.message != WM_QUIT) {
    }
}

}

TEST_CASE("resize_recomputes_regions", "[layout]")
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    REQUIRE(platform::window::InitScreen(1920, 1080, 1, 2));
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = IgnoringProc;
    wc.lpszClassName = "LayoutResize";
    REQUIRE(RegisterClassExA(&wc) != 0);
    HWND hwnd = CreateWindowExA(0, "LayoutResize", "t", WS_POPUP, 0, 0, 1920, 1080, NULL, NULL, NULL, NULL);
    REQUIRE(hwnd != NULL);
    platform::window::SetRegionSource(ResizeSource);
    platform::window::PumpEvents();
    DrainMessages();

    // The toolbar's 1x rectangle in the frame (GUIW08's bottom panels at
    // their shift) and where its region shows it.
    const ScreenLayout& l = ResizeLayout();
    const RECT toolbar = { l.worldUiShiftX, 1080 - 167, l.worldUiShiftX + 800, 1080 };
    auto Check = [&](int windowWidth, int windowHeight) {
        INFO("window " << windowWidth << "x" << windowHeight);
        // The 1920x1080 output letterboxed (centred) in the window.
        const int ox = (windowWidth - 1920) / 2;
        const int oy = (windowHeight - 1080) / 2;
        // A snapshot: SetRegions cannot free it while it is held.
        const auto current = platform::present::Regions();
        std::vector<Region> regions = *current;
        for (Region& r : regions) {
            r.window.left -= ox;
            r.window.right -= ox;
            r.window.top -= oy;
            r.window.bottom -= oy;
        }
        const SIZE output = { 1920, 1080 };
        CHECK(platform::compose::Tiles(regions, output));
        const Region* ui = UiRegion(*current);
        REQUIRE(ui != nullptr);
        CHECK(ui->window.left == ox);
        CHECK(ui->window.right == ox + 1920);
        CHECK(ui->window.bottom == oy + 1080);
        CHECK(ui->window.bottom - ui->window.top == 2 * 167);
        // The toolbar's centre in the window, as the presenter draws the
        // region, maps back inside the toolbar.
        const float cx = ui->window.left + ((toolbar.left + toolbar.right) / 2.0f - ui->frame.left) * ui->scale;
        const float cy = ui->window.top + ((toolbar.top + toolbar.bottom) / 2.0f - ui->frame.top) * ui->scale;
        CHECK(Inside(ui->window, cx, cy));
        float fx = 0.0f;
        float fy = 0.0f;
        platform::present::WindowToFrame(cx, cy, &fx, &fy);
        CHECK(Inside(toolbar, fx, fy));
        // The world above it at 1x.
        platform::present::WindowToFrame(ox + 100.0f, oy + 100.0f, &fx, &fy);
        CHECK(fx == 100.0f);
        CHECK(fy == 100.0f);
    };
    Check(1920, 1080);

    // The user resizes the window: WM_SIZE, and the regions follow.
    SDL_Window* sdl = nullptr;
    {
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        sdl = count > 0 ? windows[count - 1] : nullptr;
        SDL_free(windows);
    }
    REQUIRE(sdl != nullptr);
    REQUIRE(SDL_SetWindowSize(sdl, 2400, 1350));
    SDL_SyncWindow(sdl);
    platform::window::PumpEvents();
    MSG m;
    bool sized = false;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE) && m.message != WM_QUIT) {
        sized = sized || m.message == WM_SIZE;
    }
    CHECK(sized);
    Check(2400, 1350);

    // Back to equal scales: no regions, the presenter's own path.
    platform::window::SetRegionSource(nullptr);
    DestroyWindow(hwnd);
    DrainMessages();
    REQUIRE(platform::window::InitScreen(1024, 768, 1, 1));
    CHECK(platform::present::Regions()->empty());
}
