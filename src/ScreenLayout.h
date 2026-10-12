// iwd2r. Distributed under LICENSE.md.
// The screen layout CBaldurChitin's constructor sets up for a resolution, and
// the decision of which resolution to use.
//
// The original switches on four widths (800, 1024, 1600, 2048) and sets a
// table of CInfinity rectangles, the GUI extension, the border panels and the
// logical screen; ComputeScreenLayout returns exactly that table for those
// four sizes and generalises it to any size of at least 800x600, as IWD2EE's
// IEex does for the same engine.
#ifndef IWD2R_SCREENLAYOUT_H_
#define IWD2R_SCREENLAYOUT_H_

#include <string>

#include "mfc.h"

class CVidMosaic;

namespace iwd2r {

struct LayoutRect {
    int left;
    int top;
    int right;
    int bottom;
};

// One stone border panel (m_aBorderPanels[i], panel IDs -5..-2). x, y,
// width and height are the UI_PANELHEADER fields; anchorX/anchorY is where
// the border mosaic's origin goes in the same coordinates. The mosaic is
// tiled from the anchor across the panel, so at the original resolutions,
// where the anchor is the panel's origin and the panel is the mosaic's size,
// it is drawn once exactly as the original draws it.
struct BorderPanelLayout {
    int x;
    int y;
    int width;
    int height;
    int anchorX;
    int anchorY;
};

// Indices into ScreenLayout::borderPanels and m_aBorderPanels.
enum BorderSide {
    BORDER_LEFT = 0,
    BORDER_RIGHT = 1,
    BORDER_TOP = 2,
    BORDER_BOTTOM = 3,
};

struct ScreenLayout {
    int screenWidth; // CVideo::SCREENWIDTH
    int screenHeight; // CVideo::SCREENHEIGHT

    // CInfinity statics, by name (addresses in CInfinity.cpp).
    LayoutRect stru_8E7538;
    LayoutRect stru_8E7548; // world viewport, interface hidden
    LayoutRect stru_8E7958; // world viewport under the dialog panel
    LayoutRect stru_8E7988; // world viewport under the death panel
    LayoutRect stru_8E79A8;
    LayoutRect stru_8E79B8; // world viewport over the toolbar
    LayoutRect stru_8E79C8; // console region (bottom at 0x8E79D4)
    LayoutRect stru_8E79E0; // toolbar region (bottom at 0x8E79EC)
    LayoutRect stru_8E79F8; // world viewport over the container panel
    LayoutRect stru_8E7A10;

    const char* guiExt; // CBaldurChitin::CHUI_GUIEXT, "08" or "10"
    const char* borderMosaic[4]; // refMosaic per side, "" for none
    int field_4A28; // UI drawn double size
    int field_4A2C; // world UI drawn double size
    int logicalWidth; // nLogicalScreenWidth
    int logicalHeight; // nLogicalScreenHeight
    BorderPanelLayout borderPanels[4];

    // Offset added to the world screen's panels (GUIW08/GUIW10 are laid out
    // for 800x600 and 1024x768): bottom-centred at other sizes, zero at the
    // original four.
    int worldUiShiftX;
    int worldUiShiftY;

    // Squared distance beyond which a dialog's speaker is scrolled to
    // instantly rather than smoothly (the constants at 0x484BA2 and
    // 0x6902D4): the original's 700 * 700 at its four resolutions, IEex's
    // (w / 2)^2 + (h / 2)^2 elsewhere.
    long dialogAutoScrollRange;
};

// The original's dialog auto-scroll constant (cmp edx, 490000 at 0x484BA0).
const long kOriginalDialogAutoScrollRange = 490000;

// The smallest screen the generalised layout supports.
const int kMinScreenWidth = 800;
const int kMinScreenHeight = 600;

// width x height must be at least kMinScreenWidth x kMinScreenHeight.
ScreenLayout ComputeScreenLayout(int width, int height);

// True for the four sizes the original's switch handles.
bool IsOriginalResolution(int width, int height);

// INI values. Resolution is a width or "desktop"; Scale is 1-4 or "auto".
const int kResolutionDesktop = -1;
const int kResolutionInvalid = -2;
const int kScaleAuto = 0;
const int kScaleInvalid = -1;
const int kMaxScale = 4;

// "desktop" (any case) gives kResolutionDesktop, a positive decimal number
// its value, anything else kResolutionInvalid. NULL or "" gives 800, the
// engine's default.
int ParseResolution(const char* value);

// A positive decimal number gives its value, empty or missing gives 0 (no
// height), anything else -1.
int ParseHeight(const char* value);

// "auto" gives kScaleAuto, 1-4 their value, NULL or "" gives 1, anything
// else kScaleInvalid.
int ParseScale(const char* value);

struct ScreenSize {
    bool ok;
    std::string error; // why, when !ok

    int outputWidth; // the window or fullscreen display: internal * scale
    int outputHeight;
    int scale;
    int width; // the engine's (internal) screen
    int height;
};

// The decision main and CBaldurChitin's constructor share.
//
// iniWidth: ParseResolution's result. iniHeight: ParseHeight's result (0
// when the key is absent). iniScale: ParseScale's result. desktopWidth and
// desktopHeight: the primary display's size, 0 when unknown.
//
// The requested (output) size is the width, or the desktop for
// kResolutionDesktop, with Height when given, else the original 4:3 height
// for 800, 1024, 1600 and 2048, else the desktop's aspect (4:3 when the
// desktop is unknown). The internal size is the output divided by the
// scale; auto picks the largest factor up to kMaxScale that keeps the
// internal height at least 720 and the internal size at least 800x600 (1
// when none does). An internal size below 800x600 is an error.
ScreenSize ResolveScreenSize(int iniWidth, int iniHeight, int iniScale, int desktopWidth, int desktopHeight);

// Reads Program Options/Resolution, Height and Scale from iniFile (through
// the INI override like every other read) and the primary display's size,
// and resolves them.
ScreenSize ReadScreenSize(const char* iniFile);

// The Resolution value as the INI had it, for the engine to write back on
// exit (it would otherwise write the internal width over "desktop" or a
// scaled width). Set by ReadScreenSize.
const std::string& IniResolutionText();

// The layout CBaldurChitin's constructor applied; the border panel renderers
// read their mosaic anchors from it.
void SetCurrentScreenLayout(const ScreenLayout& layout);
const ScreenLayout& CurrentScreenLayout();

// The border side of a panel ID (-5..-2), or -1.
int BorderSideForPanelID(int nPanelID);

// Calls fn(x, y) for each position the border mosaic (mosaicWidth x
// mosaicHeight) is drawn at so that copies tiled from (anchorX, anchorY)
// cover the rectangle (left, top, right, bottom). Nothing when the mosaic or
// the rectangle is empty.
template <typename Fn>
void ForEachMosaicTile(int anchorX, int anchorY, int mosaicWidth, int mosaicHeight,
    int left, int top, int right, int bottom, Fn fn)
{
    if (mosaicWidth <= 0 || mosaicHeight <= 0 || right <= left || bottom <= top) {
        return;
    }
    // The first copy at or before left/top.
    int startX = anchorX - ((anchorX - left + mosaicWidth - 1) / mosaicWidth) * mosaicWidth;
    if (anchorX < left) {
        startX = anchorX + ((left - anchorX) / mosaicWidth) * mosaicWidth;
    }
    int startY = anchorY - ((anchorY - top + mosaicHeight - 1) / mosaicHeight) * mosaicHeight;
    if (anchorY < top) {
        startY = anchorY + ((top - anchorY) / mosaicHeight) * mosaicHeight;
    }
    for (int y = startY; y < bottom; y += mosaicHeight) {
        for (int x = startX; x < right; x += mosaicWidth) {
            fn(x, y);
        }
    }
}

// Draws a stone border panel's mosaic tiled from its anchor over the part of
// the panel inside rClip (screen coordinates), for the panel IDs -5..-2 at a
// non-original size. False, drawing nothing, at the original four sizes or
// for any other panel: the caller then draws as the original does.
bool RenderBorderMosaic(CVidMosaic& mosaic, int nPanelID, int nSurface, const CRect& rClip, DWORD dwFlags, BOOL bDemanded);

}

#endif /* IWD2R_SCREENLAYOUT_H_ */
