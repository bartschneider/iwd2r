#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "ScreenLayout.h"

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
