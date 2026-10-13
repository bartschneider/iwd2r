#include "ScreenLayout.h"

#include "CBaldurChitin.h"
#include "CInfinity.h"
#include "CScreenWorld.h"
#include "CVidMosaic.h"
#include "platform/window.h"

#include <cassert>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace iwd2r {

namespace {

    // The original's 800x600 UI, which every screen but the world screen
    // centres (CBaldurChitin::DEFAULT_SCREEN_WIDTH/HEIGHT).
    const int kUiWidth = 800;
    const int kUiHeight = 600;

    // The stone border mosaics (STON10L/R 112x768, STON10T/B 800x84) frame
    // the 800x600 UI inside 1024x768.
    const int kStoneSideWidth = 112;
    const int kStoneTopHeight = 84;

    const char* const kStone10[4] = { "STON10L", "STON10R", "STON10T", "STON10B" };

    LayoutRect R(int left, int top, int right, int bottom)
    {
        LayoutRect r = { left, top, right, bottom };
        return r;
    }

    // The border panels as CBaldurChitin's constructor computes them after
    // its switch (0x41E2F0 onwards), from the logical screen size.
    void OriginalBorderPanels(ScreenLayout& l)
    {
        const int side = (l.logicalWidth - kUiWidth) / 2;
        const int band = (l.logicalHeight - kUiHeight) / 2;
        l.borderPanels[BORDER_LEFT] = { 0, 0, side, l.logicalHeight, 0, 0 };
        l.borderPanels[BORDER_RIGHT] = { side + kUiWidth, 0, side, l.logicalHeight, side + kUiWidth, 0 };
        l.borderPanels[BORDER_TOP] = { side, 0, kUiWidth, band, side, 0 };
        l.borderPanels[BORDER_BOTTOM] = { side, band + kUiHeight, kUiWidth, band, side, band + kUiHeight };
    }

    void SetStoneMosaics(ScreenLayout& l, bool on)
    {
        for (int i = 0; i < 4; i++) {
            l.borderMosaic[i] = on ? kStone10[i] : "";
        }
    }

    // The original switch, value for value.
    bool OriginalLayout(int width, int height, ScreenLayout& l)
    {
        l.worldUiShiftX = 0;
        l.worldUiShiftY = 0;
        l.dialogAutoScrollRange = kOriginalDialogAutoScrollRange;
        l.field_4A28 = 0;
        l.field_4A2C = 0;

        if (width == 2048 && height == 1536) {
            l.stru_8E7538 = R(0, 0, 960, 658);
            l.stru_8E7548 = R(0, 0, 2048, 1536);
            l.stru_8E7958 = R(0, 0, 2048, 968);
            l.stru_8E7988 = R(0, 0, 2048, 1258);
            l.stru_8E79A8 = R(256, 150, 768, 534);
            l.stru_8E79B8 = R(0, 0, 2048, 1202);
            l.stru_8E79C8 = R(0, 0, 2048, 1416);
            l.stru_8E79E0 = R(0, 0, 2048, 1322);
            l.stru_8E79F8 = R(0, 0, 2048, 1288);
            l.stru_8E7A10 = R(0, 0, 960, 522);
            l.screenWidth = 2048;
            l.screenHeight = 1536;
            l.guiExt = "10";
            SetStoneMosaics(l, true);
            l.field_4A2C = 1;
            l.field_4A28 = 1;
            l.logicalWidth = 1024;
            l.logicalHeight = 768;
        } else if (width == 1600 && height == 1200) {
            l.stru_8E7538 = R(0, 0, 736, 490);
            l.stru_8E7548 = R(0, 0, 1600, 1200);
            l.stru_8E7958 = R(0, 0, 1600, 632);
            l.stru_8E7988 = R(0, 0, 1600, 922);
            l.stru_8E79A8 = R(544, 366, 1056, 750);
            l.stru_8E79B8 = R(0, 0, 1600, 866);
            l.stru_8E79C8 = R(0, 0, 1600, 1080);
            l.stru_8E79E0 = R(0, 0, 1600, 986);
            l.stru_8E79F8 = R(0, 0, 1600, 952);
            l.stru_8E7A10 = R(0, 0, 736, 354);
            l.screenWidth = 1600;
            l.screenHeight = 1200;
            l.guiExt = "08";
            SetStoneMosaics(l, false);
            l.field_4A2C = 1;
            l.field_4A28 = 1;
            l.logicalWidth = 800;
            l.logicalHeight = 600;
        } else if (width == 1024 && height == 768) {
            l.stru_8E7538 = R(0, 0, 960, 658);
            l.stru_8E7548 = R(0, 0, 1024, 768);
            l.stru_8E7958 = R(0, 0, 1024, 484);
            l.stru_8E7988 = R(0, 0, 1024, 629);
            l.stru_8E79A8 = R(256, 150, 768, 534);
            l.stru_8E79B8 = R(0, 0, 1024, 601);
            l.stru_8E79C8 = R(0, 0, 1024, 708);
            l.stru_8E79E0 = R(0, 0, 1024, 661);
            l.stru_8E79F8 = R(0, 0, 1024, 644);
            l.stru_8E7A10 = R(0, 0, 960, 522);
            l.screenWidth = 1024;
            l.screenHeight = 768;
            l.guiExt = "10";
            SetStoneMosaics(l, true);
            l.logicalWidth = 1024;
            l.logicalHeight = 768;
        } else if (width == 800 && height == 600) {
            l.stru_8E7538 = R(0, 0, 736, 490);
            l.stru_8E7548 = R(0, 0, 800, 600);
            l.stru_8E7958 = R(0, 0, 800, 316);
            l.stru_8E7988 = R(0, 0, 800, 461);
            l.stru_8E79A8 = R(144, 66, 656, 450);
            l.stru_8E79B8 = R(0, 0, 800, 433);
            l.stru_8E79C8 = R(0, 0, 800, 540);
            l.stru_8E79E0 = R(0, 0, 800, 493);
            l.stru_8E79F8 = R(0, 0, 800, 476);
            l.stru_8E7A10 = R(0, 0, 736, 354);
            l.screenWidth = 800;
            l.screenHeight = 600;
            l.guiExt = "08";
            SetStoneMosaics(l, false);
            l.logicalWidth = 800;
            l.logicalHeight = 600;
        } else {
            return false;
        }

        OriginalBorderPanels(l);
        return true;
    }

    std::string Trim(const char* value)
    {
        std::string s = value != NULL ? value : "";
        size_t begin = 0;
        while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
            begin++;
        }
        size_t end = s.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            end--;
        }
        return s.substr(begin, end - begin);
    }

    // A positive decimal number without sign or junk, or -1.
    int PositiveNumber(const std::string& s)
    {
        if (s.empty() || s.size() > 6) {
            return -1;
        }
        for (char c : s) {
            if (c < '0' || c > '9') {
                return -1;
            }
        }
        const int value = std::atoi(s.c_str());
        return value > 0 ? value : -1;
    }

    bool EqualsIgnoreCase(const std::string& a, const char* b)
    {
        if (a.size() != std::strlen(b)) {
            return false;
        }
        for (size_t i = 0; i < a.size(); i++) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

    std::string& IniResolution()
    {
        static std::string text = "800";
        return text;
    }

    ScreenLayout& Current()
    {
        static ScreenLayout layout = ComputeScreenLayout(800, 600, 1, 1);
        return layout;
    }

    // The scales ReadScreenSize resolved last.
    int& ResolvedUiScale()
    {
        static int scale = 1;
        return scale;
    }

    int& ResolvedWorldScale()
    {
        static int scale = 1;
        return scale;
    }

    // The bottom panels' depths at every non-original size (the 800x600 and
    // 1024x768 tables agree): the toolbar under the world viewport.
    const int kToolbarDepth = 167;

    RECT MakeRect(int left, int top, int right, int bottom)
    {
        RECT r;
        r.left = left;
        r.top = top;
        r.right = right;
        r.bottom = bottom;
        return r;
    }

    // The UI-scaling fields every layout carries.
    void SetRegionFields(ScreenLayout& l, int uiScale, int worldScale, const EffectiveScales& eff)
    {
        l.uiScale = uiScale;
        l.worldScale = worldScale;
        l.worldUiScale = eff.worldUi;
        l.fullUiScale = eff.fullUi;
        l.uiWidth = static_cast<int>(static_cast<long long>(l.screenWidth) * worldScale / eff.worldUi);
        l.uiHeight = static_cast<int>(static_cast<long long>(l.screenHeight) * worldScale / eff.worldUi);
        const int ux = (l.screenWidth - kUiWidth) / 2;
        const int uy = (l.screenHeight - kUiHeight) / 2;
        l.fullUiRect = MakeRect(ux, uy, ux + kUiWidth, uy + kUiHeight);
        l.uiPanelRects = WorldPanelRects(l, l.stru_8E79B8.bottom);
    }

}

bool IsOriginalResolution(int width, int height)
{
    return (width == 800 && height == 600)
        || (width == 1024 && height == 768)
        || (width == 1600 && height == 1200)
        || (width == 2048 && height == 1536);
}

int ReservedDepth(int depth, int uiScale, int worldScale)
{
    uiScale = uiScale >= 1 ? uiScale : 1;
    worldScale = worldScale >= 1 ? worldScale : 1;
    if (depth <= 0) {
        return 0;
    }
    const int scale = uiScale > worldScale ? uiScale : worldScale;
    return (depth * scale + worldScale - 1) / worldScale;
}

EffectiveScales EffectiveUiScales(int outputWidth, int outputHeight, int uiScale, int worldScale)
{
    uiScale = uiScale >= 1 ? uiScale : 1;
    worldScale = worldScale >= 1 ? worldScale : 1;
    if (worldScale > uiScale) {
        worldScale = uiScale;
    }
    auto fit = [&](int height) {
        int s = uiScale;
        if (outputWidth / kUiWidth < s) {
            s = outputWidth / kUiWidth;
        }
        if (outputHeight / height < s) {
            s = outputHeight / height;
        }
        return s > worldScale ? s : worldScale;
    };
    EffectiveScales e;
    e.worldUi = fit(kDeepestWorldPanel);
    e.fullUi = fit(kUiHeight);
    return e;
}

ScreenLayout ComputeScreenLayout(int width, int height, int uiScale, int worldScale)
{
    uiScale = uiScale >= 1 ? uiScale : 1;
    worldScale = worldScale >= 1 ? worldScale : 1;
    if (worldScale > uiScale) {
        worldScale = uiScale;
    }
    const EffectiveScales eff = EffectiveUiScales(width * worldScale, height * worldScale, uiScale, worldScale);
    const bool scaled = eff.worldUi != worldScale || eff.fullUi != worldScale;

    ScreenLayout l {};
    if (!scaled && OriginalLayout(width, height, l)) {
        SetRegionFields(l, uiScale, worldScale, eff);
        return l;
    }
    // The world screen's panels' scale is the one the layout reserves for.
    const int panelScale = eff.worldUi;

    // Any other size: one engine pixel per screen pixel (no double size),
    // the 800x600 UI centred, every rectangle as the original's 800x600 and
    // 1024x768 tables have it (the two agree: each keeps its offsets from the
    // bottom and right edges), so the world viewport spans the full width and
    // the height above the bottom panels.
    //
    // With a UI scale of its own the UI is laid out for the frame at that
    // scale (uiWidth x uiHeight) and every bottom offset is the depth the
    // panel reserves at it.
    l.screenWidth = width;
    l.screenHeight = height;
    l.logicalWidth = width;
    l.logicalHeight = height;
    l.field_4A28 = 0;
    l.field_4A2C = 0;
    const int uiWidth = static_cast<int>(static_cast<long long>(width) * worldScale / panelScale);
    const int uiHeight = static_cast<int>(static_cast<long long>(height) * worldScale / panelScale);
    l.guiExt = uiWidth >= 1024 && uiHeight >= 768 ? "10" : "08";
    SetStoneMosaics(l, true);

    auto bottom = [&](int depth) { return height - ReservedDepth(depth, panelScale, worldScale); };
    auto right = [&](int depth) { return width - ReservedDepth(depth, panelScale, worldScale); };
    l.stru_8E7538 = R(0, 0, right(64), bottom(110));
    l.stru_8E7548 = R(0, 0, width, height);
    l.stru_8E7958 = R(0, 0, width, bottom(284));
    l.stru_8E7988 = R(0, 0, width, bottom(139));
    l.stru_8E79A8 = R((width - 512) / 2, (height - 384) / 2 - 42, (width - 512) / 2 + 512, (height - 384) / 2 - 42 + 384);
    l.stru_8E79B8 = R(0, 0, width, bottom(kToolbarDepth));
    l.stru_8E79C8 = R(0, 0, width, bottom(60));
    l.stru_8E79E0 = R(0, 0, width, bottom(107));
    l.stru_8E79F8 = R(0, 0, width, bottom(124));
    l.stru_8E7A10 = R(0, 0, right(64), bottom(246));

    // The UI's top-left corner, where every screen but the world screen
    // shifts its panels ((SCREENWIDTH - 800) / 2, (SCREENHEIGHT - 600) / 2).
    const int ux = (width - kUiWidth) / 2;
    const int uy = (height - kUiHeight) / 2;
    // The 1024x768 frame the stone mosaics were drawn for, around the UI.
    const int fx = ux - kStoneSideWidth;
    const int fy = uy - kStoneTopHeight;
    l.borderPanels[BORDER_LEFT] = { 0, 0, ux, height, fx, fy };
    l.borderPanels[BORDER_RIGHT] = { ux + kUiWidth, 0, width - ux - kUiWidth, height, ux + kUiWidth, fy };
    l.borderPanels[BORDER_TOP] = { ux, 0, kUiWidth, uy, ux, fy };
    l.borderPanels[BORDER_BOTTOM] = { ux, uy + kUiHeight, kUiWidth, height - uy - kUiHeight, ux, uy + kUiHeight };

    // The world screen's panels bottom-centred in the columns the UI band
    // shows (the whole width with equal scales): compose::Layout shows a
    // full-width bottom band's first uiWidth columns.
    const int worldWidth = l.guiExt[0] == '1' ? 1024 : 800;
    const int worldHeight = l.guiExt[0] == '1' ? 768 : 600;
    l.worldUiShiftX = (uiWidth - worldWidth) / 2;
    l.worldUiShiftY = height - worldHeight;

    // IEex_Extern_InitResolution: just outside the viewport.
    const double hw = width / 2.0;
    const double hh = height / 2.0;
    l.dialogAutoScrollRange = static_cast<long>(hw * hw + hh * hh);
    SetRegionFields(l, uiScale, worldScale, eff);
    return l;
}

ScreenLayout ComputeScreenLayout(int width, int height)
{
    return ComputeScreenLayout(width, height, ResolvedUiScale(), ResolvedWorldScale());
}

std::vector<RECT> WorldPanelRects(const ScreenLayout& l, int viewportBottom)
{
    const int reserved = l.screenHeight - viewportBottom;
    if (reserved <= 0 || viewportBottom <= 0) {
        return {};
    }
    // The deepest panel whose reserved depth fits above the viewport's
    // bottom: the panel itself when the viewport is one of the layout's.
    int depth = 1;
    while (depth < l.screenHeight && ReservedDepth(depth + 1, l.worldUiScale, l.worldScale) <= reserved) {
        depth++;
    }
    return { MakeRect(0, l.screenHeight - depth, l.screenWidth, l.screenHeight) };
}

std::vector<platform::compose::Region> RegionsFor(const ScreenLayout& l, ScreenKind kind, int viewportBottom, SIZE window)
{
    using platform::compose::Region;
    SIZE frame;
    frame.cx = l.screenWidth;
    frame.cy = l.screenHeight;
    // The letterboxed output: the frame at the world scale, centred.
    SIZE output;
    output.cx = frame.cx * l.worldScale;
    output.cy = frame.cy * l.worldScale;
    const LONG ox = (window.cx - output.cx) / 2;
    const LONG oy = (window.cy - output.cy) / 2;

    std::vector<Region> regions;
    if (kind == SCREEN_FULL_UI) {
        regions = platform::compose::Layout(frame, output, l.fullUiScale, l.worldScale, {}, &l.fullUiRect);
    } else {
        regions = platform::compose::Layout(frame, output, l.worldUiScale, l.worldScale, WorldPanelRects(l, viewportBottom), NULL);
        if (!platform::compose::Tiles(regions, output)) {
            // Spec section 6: a world screen's panels must leave a
            // rectangle for the world.
            assert(!"world screen regions do not tile the window");
            static bool reported = false;
            if (!reported) {
                reported = true;
                std::fprintf(stderr, "iwd2r: the world screen's UI panels do not tile the window; showing it at World Scale\n");
            }
            regions = platform::compose::Layout(frame, output, l.worldScale, l.worldScale, {}, NULL);
        }
    }
    for (Region& r : regions) {
        r.window.left += ox;
        r.window.right += ox;
        r.window.top += oy;
        r.window.bottom += oy;
    }
    return regions;
}

std::vector<platform::compose::Region> CurrentRegions(SIZE window)
{
    const ScreenLayout& l = Current();
    ScreenKind kind = SCREEN_FULL_UI;
    int viewportBottom = l.stru_8E79B8.bottom;
    // Our hook reads the engine's state rather than being called from its
    // screen switches: the active engine is the world screen and no
    // progress screen covers it.
    if (g_pBaldurChitin != NULL
        && g_pBaldurChitin->m_pEngineWorld != NULL
        && g_pBaldurChitin->pActiveEngine == g_pBaldurChitin->m_pEngineWorld
        && !g_pBaldurChitin->cProgressBar.m_bProgressBarActivated) {
        kind = SCREEN_WORLD;
        // The view size the world screen asked for last (SetNewViewSize),
        // (0, 0, 1, 1) until it first does; the toolbar's viewport before.
        const CRect& view = g_pBaldurChitin->m_pEngineWorld->m_newViewSize;
        if (view.bottom > 1) {
            viewportBottom = view.bottom;
        } else {
            viewportBottom = CInfinity::stru_8E79B8.bottom;
        }
    }
    return RegionsFor(l, kind, viewportBottom, window);
}

int ParseResolution(const char* value)
{
    const std::string s = Trim(value);
    if (s.empty()) {
        return 800;
    }
    if (EqualsIgnoreCase(s, "desktop")) {
        return kResolutionDesktop;
    }
    const int n = PositiveNumber(s);
    return n > 0 ? n : kResolutionInvalid;
}

int ParseHeight(const char* value)
{
    const std::string s = Trim(value);
    if (s.empty()) {
        return 0;
    }
    return PositiveNumber(s);
}

int ParseScale(const char* value)
{
    const std::string s = Trim(value);
    if (s.empty()) {
        return 1;
    }
    if (EqualsIgnoreCase(s, "auto")) {
        return kScaleAuto;
    }
    const int n = PositiveNumber(s);
    return n >= 1 && n <= kMaxScale ? n : kScaleInvalid;
}

int ResolveWorldScale(const char* value, int uiScale)
{
    const std::string s = Trim(value);
    if (s.empty()) {
        return uiScale;
    }
    if (EqualsIgnoreCase(s, "auto")) {
        return uiScale >= 2 ? 1 : uiScale;
    }
    const int n = PositiveNumber(s);
    return n >= 1 && n <= kMaxScale ? n : kScaleInvalid;
}

ScreenSize ResolveScreenSize(int iniWidth, int iniHeight, int iniScale, int desktopWidth, int desktopHeight,
    const char* worldScaleText)
{
    ScreenSize r;
    r.ok = false;
    r.outputWidth = 0;
    r.outputHeight = 0;
    r.scale = 1;
    r.width = 0;
    r.height = 0;
    r.uiScale = 1;
    r.worldScale = 1;
    r.worldUiScale = 1;
    r.fullUiScale = 1;

    const bool desktopKnown = desktopWidth > 0 && desktopHeight > 0;

    if (iniWidth == kResolutionInvalid || iniWidth == 0 || iniWidth < kResolutionDesktop) {
        r.error = "Resolution in icewind2.ini must be a width in pixels or \"desktop\".";
        return r;
    }
    if (iniHeight < 0) {
        r.error = "Height in icewind2.ini must be a height in pixels, or absent.";
        return r;
    }
    if (iniScale == kScaleInvalid || iniScale < 0 || iniScale > kMaxScale) {
        r.error = "Scale in icewind2.ini must be 1, 2, 3, 4 or \"auto\".";
        return r;
    }

    int width;
    int height;
    if (iniWidth == kResolutionDesktop) {
        if (!desktopKnown) {
            r.error = "Resolution=desktop in icewind2.ini, but the desktop size is unknown.";
            return r;
        }
        width = desktopWidth;
        height = iniHeight > 0 ? iniHeight : desktopHeight;
    } else {
        width = iniWidth;
        if (iniHeight > 0) {
            height = iniHeight;
        } else {
            switch (width) {
            case 800:
                height = 600;
                break;
            case 1024:
                height = 768;
                break;
            case 1600:
                height = 1200;
                break;
            case 2048:
                height = 1536;
                break;
            default:
                if (desktopKnown) {
                    height = static_cast<int>((static_cast<long long>(width) * desktopHeight + desktopWidth / 2) / desktopWidth);
                } else {
                    height = width * 3 / 4;
                }
                break;
            }
        }
    }

    int scale = iniScale;
    if (scale == kScaleAuto) {
        scale = 1;
        for (int s = kMaxScale; s > 1; s--) {
            if (height / s >= 720 && width / s >= kMinScreenWidth && height / s >= kMinScreenHeight) {
                scale = s;
                break;
            }
        }
    }

    int worldScale = ResolveWorldScale(worldScaleText, scale);
    if (worldScale == kScaleInvalid) {
        r.error = "World Scale in icewind2.ini must be 1, 2, 3, 4 or \"auto\", or absent.";
        return r;
    }
    if (worldScale > scale) {
        // The UI is never smaller than the world.
        r.notices.push_back("World Scale=" + std::to_string(worldScale) + " is above Scale=" + std::to_string(scale)
            + "; using World Scale=" + std::to_string(scale) + ".");
        worldScale = scale;
    }

    r.uiScale = scale;
    r.worldScale = worldScale;
    r.scale = worldScale;
    r.width = width / worldScale;
    r.height = height / worldScale;
    r.outputWidth = r.width * worldScale;
    r.outputHeight = r.height * worldScale;
    const EffectiveScales eff = EffectiveUiScales(r.outputWidth, r.outputHeight, scale, worldScale);
    r.worldUiScale = eff.worldUi;
    r.fullUiScale = eff.fullUi;
    const std::string size = std::to_string(r.outputWidth) + "x" + std::to_string(r.outputHeight);
    if (eff.worldUi < scale) {
        r.notices.push_back("The world screen's panels do not fit " + size + " at Scale=" + std::to_string(scale)
            + "; showing them at " + std::to_string(eff.worldUi) + ".");
    }
    if (eff.fullUi < scale) {
        r.notices.push_back("The 800x600 screens do not fit " + size + " at Scale=" + std::to_string(scale)
            + "; showing them at " + std::to_string(eff.fullUi) + ".");
    }

    if (r.width < kMinScreenWidth || r.height < kMinScreenHeight) {
        r.error = "The screen size " + std::to_string(width) + "x" + std::to_string(height);
        if (worldScale != scale) {
            r.error += " at World Scale=" + std::to_string(worldScale) + " gives an internal screen of "
                + std::to_string(r.width) + "x" + std::to_string(r.height) + ";";
        } else if (scale > 1) {
            r.error += " at Scale=" + std::to_string(scale) + " gives an internal screen of "
                + std::to_string(r.width) + "x" + std::to_string(r.height) + ";";
        } else {
            r.error += " is too small;";
        }
        r.error += " iwd2r needs at least 800x600. Set Resolution (and Height, Scale, World Scale) in"
                   " the [Program Options] section of icewind2.ini.";
        return r;
    }
    if (r.width > SHRT_MAX || r.height > SHRT_MAX) {
        r.error = "The screen size " + std::to_string(width) + "x" + std::to_string(height) + " is too large.";
        return r;
    }

    r.ok = true;
    return r;
}

ScreenSize ReadScreenSize(const char* iniFile)
{
    char resolution[64];
    char height[64];
    char scale[64];
    char worldScale[64];
    GetPrivateProfileStringA("Program Options", "Resolution", "800", resolution, sizeof(resolution), iniFile);
    GetPrivateProfileStringA("Program Options", "Height", "", height, sizeof(height), iniFile);
    GetPrivateProfileStringA("Program Options", "Scale", "1", scale, sizeof(scale), iniFile);
    GetPrivateProfileStringA("Program Options", "World Scale", "", worldScale, sizeof(worldScale), iniFile);
    IniResolution() = Trim(resolution);

    int desktopWidth = 0;
    int desktopHeight = 0;
    platform::window::NativeDesktopSize(&desktopWidth, &desktopHeight);

    const ScreenSize r = ResolveScreenSize(ParseResolution(resolution), ParseHeight(height), ParseScale(scale), desktopWidth, desktopHeight, worldScale);
    if (r.ok) {
        ResolvedUiScale() = r.uiScale;
        ResolvedWorldScale() = r.worldScale;
    }
    return r;
}

const std::string& IniResolutionText()
{
    return IniResolution();
}

void SetCurrentScreenLayout(const ScreenLayout& layout)
{
    Current() = layout;
}

const ScreenLayout& CurrentScreenLayout()
{
    return Current();
}

int BorderSideForPanelID(int nPanelID)
{
    switch (nPanelID) {
    case -5:
        return BORDER_LEFT;
    case -4:
        return BORDER_RIGHT;
    case -3:
        return BORDER_TOP;
    case -2:
        return BORDER_BOTTOM;
    default:
        return -1;
    }
}

bool RenderBorderMosaic(CVidMosaic& mosaic, int nPanelID, int nSurface, const CRect& rClip, DWORD dwFlags, BOOL bDemanded)
{
    const ScreenLayout& l = Current();
    const int side = BorderSideForPanelID(nPanelID);
    if (side < 0 || IsOriginalResolution(l.screenWidth, l.screenHeight)) {
        return false;
    }
    if (mosaic.pRes == NULL) {
        return true;
    }

    CSize size;
    mosaic.GetSize(size, bDemanded);

    const BorderPanelLayout& p = l.borderPanels[side];
    CRect rPanel(p.x, p.y, p.x + p.width, p.y + p.height);
    CRect r;
    if (!r.IntersectRect(rPanel, rClip)) {
        return true;
    }

    ForEachMosaicTile(p.anchorX, p.anchorY, size.cx, size.cy, r.left, r.top, r.right, r.bottom, [&](int x, int y) {
        CRect rMosaic(r.left - x, r.top - y, r.right - x, r.bottom - y);
        rMosaic.IntersectRect(rMosaic, CRect(0, 0, size.cx, size.cy));
        mosaic.Render(nSurface, x, y, rMosaic, r, dwFlags, bDemanded);
    });
    return true;
}

}
