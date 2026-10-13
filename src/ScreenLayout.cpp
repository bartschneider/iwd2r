#include "ScreenLayout.h"

#include "CVidMosaic.h"
#include "platform/window.h"

#include <cctype>
#include <climits>
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
        static ScreenLayout layout = ComputeScreenLayout(800, 600);
        return layout;
    }

}

bool IsOriginalResolution(int width, int height)
{
    return (width == 800 && height == 600)
        || (width == 1024 && height == 768)
        || (width == 1600 && height == 1200)
        || (width == 2048 && height == 1536);
}

ScreenLayout ComputeScreenLayout(int width, int height)
{
    ScreenLayout l;
    std::memset(&l, 0, sizeof(l));
    if (OriginalLayout(width, height, l)) {
        return l;
    }

    // Any other size: one engine pixel per screen pixel (no double size),
    // the 800x600 UI centred, every rectangle as the original's 800x600 and
    // 1024x768 tables have it (the two agree: each keeps its offsets from the
    // bottom and right edges), so the world viewport spans the full width and
    // the height above the bottom panels.
    l.screenWidth = width;
    l.screenHeight = height;
    l.logicalWidth = width;
    l.logicalHeight = height;
    l.field_4A28 = 0;
    l.field_4A2C = 0;
    l.guiExt = width >= 1024 && height >= 768 ? "10" : "08";
    SetStoneMosaics(l, true);

    l.stru_8E7538 = R(0, 0, width - 64, height - 110);
    l.stru_8E7548 = R(0, 0, width, height);
    l.stru_8E7958 = R(0, 0, width, height - 284);
    l.stru_8E7988 = R(0, 0, width, height - 139);
    l.stru_8E79A8 = R((width - 512) / 2, (height - 384) / 2 - 42, (width - 512) / 2 + 512, (height - 384) / 2 - 42 + 384);
    l.stru_8E79B8 = R(0, 0, width, height - 167);
    l.stru_8E79C8 = R(0, 0, width, height - 60);
    l.stru_8E79E0 = R(0, 0, width, height - 107);
    l.stru_8E79F8 = R(0, 0, width, height - 124);
    l.stru_8E7A10 = R(0, 0, width - 64, height - 246);

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

    const int worldWidth = l.guiExt[0] == '1' ? 1024 : 800;
    const int worldHeight = l.guiExt[0] == '1' ? 768 : 600;
    l.worldUiShiftX = (width - worldWidth) / 2;
    l.worldUiShiftY = height - worldHeight;

    // IEex_Extern_InitResolution: just outside the viewport.
    const double hw = width / 2.0;
    const double hh = height / 2.0;
    l.dialogAutoScrollRange = static_cast<long>(hw * hw + hh * hh);
    return l;
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

ScreenSize ResolveScreenSize(int iniWidth, int iniHeight, int iniScale, int desktopWidth, int desktopHeight)
{
    ScreenSize r;
    r.ok = false;
    r.outputWidth = 0;
    r.outputHeight = 0;
    r.scale = 1;
    r.width = 0;
    r.height = 0;

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

    r.scale = scale;
    r.width = width / scale;
    r.height = height / scale;
    r.outputWidth = r.width * scale;
    r.outputHeight = r.height * scale;

    if (r.width < kMinScreenWidth || r.height < kMinScreenHeight) {
        r.error = "The screen size " + std::to_string(width) + "x" + std::to_string(height);
        if (scale > 1) {
            r.error += " at Scale=" + std::to_string(scale) + " gives an internal screen of "
                + std::to_string(r.width) + "x" + std::to_string(r.height) + ";";
        } else {
            r.error += " is too small;";
        }
        r.error += " iwd2r needs at least 800x600. Set Resolution (and Height, Scale) in"
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
    GetPrivateProfileStringA("Program Options", "Resolution", "800", resolution, sizeof(resolution), iniFile);
    GetPrivateProfileStringA("Program Options", "Height", "", height, sizeof(height), iniFile);
    GetPrivateProfileStringA("Program Options", "Scale", "1", scale, sizeof(scale), iniFile);
    IniResolution() = Trim(resolution);

    int desktopWidth = 0;
    int desktopHeight = 0;
    platform::window::NativeDesktopSize(&desktopWidth, &desktopHeight);

    return ResolveScreenSize(ParseResolution(resolution), ParseHeight(height), ParseScale(scale), desktopWidth, desktopHeight);
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
