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
#include <vector>

#include "mfc.h"
#include "platform/compose.h"

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

    // UI scaling (Program Options/Scale and World Scale): window pixels per
    // engine pixel for the UI panels and for the world. The engine's screen
    // is the window divided by worldScale. uiScale is Scale as resolved;
    // worldUiScale and fullUiScale are the scales the world screen's panels
    // and the full-UI screens are shown at (EffectiveUiScales). When both
    // equal worldScale every field above is what it has always been and the
    // presenter shows the whole frame at that scale.
    int uiScale;
    int worldScale;
    int worldUiScale;
    int fullUiScale;

    // The world screen's UI panels as compose::Layout takes them: their 1x
    // rectangles in frame coordinates, one full-width band along the bottom
    // edge as deep as the toolbar (the panels themselves sit in the band's
    // first uiWidth columns, see worldUiShiftX). WorldPanelRects gives the
    // band for another viewport (dialog, death, hidden interface).
    //
    // Every GUIW08/GUIW10 panel the world screen shows lies in one
    // full-width band along the bottom edge whose depth is the distance
    // from the world screen's requested view size (SetNewViewSize) to the
    // frame bottom: each panel lies within the bottom band (the action bar is stacked above the toolbar) and no deeper than the
    // viewport it is shown over (toolbar 167 for the comm, action, chat and
    // multiplayer bars, container 124, death 139, dialog 284), and every
    // control lies inside its panel (tests/test_layout.cpp
    // world_panels_lie_in_the_band holds the CHU rectangles). A panel
    // outside the band would not be shown at the UI scale: with the UI
    // scale above the world scale, no region shows the frame rows between
    // the world viewport's bottom and the band's top (the reserved depth
    // minus the band's), so a panel there would be hidden, and one above
    // them would be shown at the world scale inside the world region.
    std::vector<RECT> uiPanelRects;
    // A full-UI screen's 1x rectangle: the 800x600 UI centred in the frame,
    // where every screen but the world screen shifts its panels.
    RECT fullUiRect;
    // The frame size the world screen's panels have at worldUiScale: the
    // frame times worldScale / worldUiScale, the frame itself with equal
    // scales.
    int uiWidth;
    int uiHeight;
};

// The original's dialog auto-scroll constant (cmp edx, 490000 at 0x484BA0).
const long kOriginalDialogAutoScrollRange = 490000;

// The smallest screen the generalised layout supports.
const int kMinScreenWidth = 800;
const int kMinScreenHeight = 600;

// The deepest world-screen panel band (the dialog panel's, 284 rows).
const int kDeepestWorldPanel = 284;

struct EffectiveScales {
    int worldUi; // the world screen's panels
    int fullUi; // the full-UI screens
};

// The UI scales actually used in an output (window) of outputWidth x
// outputHeight, never below worldScale (worldScale <= uiScale):
// - world screen: the largest scale up to uiScale at which the 800-wide
//   panels fit the width and the deepest panel band the height,
//   min(uiScale, W / 800, H / kDeepestWorldPanel);
// - full-UI screens: the largest scale up to uiScale at which the 800x600
//   UI (fullUiRect; guiExt only selects GUIW) fits, min(uiScale, W / 800,
//   H / 600).
EffectiveScales EffectiveUiScales(int outputWidth, int outputHeight, int uiScale, int worldScale);

// width x height (the engine's internal size, the window / worldScale) must
// be at least kMinScreenWidth x kMinScreenHeight. worldScale above uiScale
// is taken as uiScale.
//
// When EffectiveUiScales gives worldScale for both kinds of screen, exactly
// the layout ComputeScreenLayout has always given (the original's table at
// its four sizes). Otherwise the generalised layout at every size (the
// engine never doubles its own UI, field_4A28/field_4A2C stay 0) with, at
// worldUiScale:
// - guiExt chosen and the world screen's panels centred for the UI's size
//   (uiWidth x uiHeight), so they lie within the columns the UI band shows;
// - every world viewport's distance from the bottom (a bottom panel's
//   depth) reserved at depth x max(worldUiScale, worldScale) / worldScale,
//   rounded up, as compose::Layout reserves it, so the world never covers a
//   scaled panel.
ScreenLayout ComputeScreenLayout(int width, int height, int uiScale, int worldScale);

// At the scales ReadScreenSize last resolved (1 and 1 before it runs), for
// CBaldurChitin's constructor.
ScreenLayout ComputeScreenLayout(int width, int height);

// The frame rows a bottom panel depth pixels deep reserves:
// ceil(depth x max(uiScale, worldScale) / worldScale), never less than depth.
int ReservedDepth(int depth, int uiScale, int worldScale);

// The world screen's UI panels (as uiPanelRects) for a world viewport whose
// bottom edge is viewportBottom: a full-width bottom band whose reserved
// depth is the viewport's distance from the frame bottom. Empty when the
// viewport reaches the bottom (the interface hidden).
std::vector<RECT> WorldPanelRects(const ScreenLayout& layout, int viewportBottom);

// Which kind of screen the engine shows: the world screen, whose UI manager
// is initialised with field_4A2C ("world UI drawn double size"; GUIW08 or
// GUIW10, the panels along the bottom edge and the area behind them), or a
// full-UI screen, every screen initialised with field_4A28 (start,
// character generation, inventory, character record, options, map,
// journal, store, load/save...) and the progress screen, which draw one
// 800x600 UI centred in the border mosaics.
enum ScreenKind {
    SCREEN_FULL_UI = 0,
    SCREEN_WORLD = 1,
};

// The composition regions (compose::Layout) of a screen of that kind in a
// window of that size. viewportBottom is the world viewport's bottom edge
// (SCREEN_WORLD only). The regions cover the letterboxed output, the frame
// times worldScale centred in the window (the window itself unless it was
// resized or is a fullscreen display of another size); the presenter
// clears the rest. One region showing the whole frame when the screen's UI
// scale equals worldScale. World screens whose regions do not tile (a
// panel outside the bottom band) assert in a debug build and fall back to
// that one region in a release build, with a line on stderr.
std::vector<platform::compose::Region> RegionsFor(const ScreenLayout& layout, ScreenKind kind, int viewportBottom, SIZE window);

// RegionsFor the current layout and the screen the engine shows now (the
// active engine, the world screen's view size, the progress screen).
std::vector<platform::compose::Region> CurrentRegions(SIZE window);

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

// World Scale for a resolved Scale (uiScale, 1-4): NULL or "" gives
// uiScale, "auto" 1 when uiScale is 2 or more (else uiScale), 1-4 their
// value, anything else kScaleInvalid. ResolveScreenSize clamps a value
// above uiScale to uiScale.
int ResolveWorldScale(const char* value, int uiScale);

struct ScreenSize {
    bool ok;
    std::string error; // why, when !ok

    int outputWidth; // the window or fullscreen display: internal * scale
    int outputHeight;
    int scale; // window pixels per engine pixel: worldScale
    int width; // the engine's (internal) screen
    int height;
    int uiScale; // Scale, resolved
    int worldScale; // World Scale, resolved and clamped to uiScale (scale)
    int worldUiScale; // EffectiveUiScales for the output
    int fullUiScale;
    // One line each for the clamps applied (World Scale above Scale, the UI
    // scale lowered to fit), for main to print on stderr.
    std::vector<std::string> notices;
};

// The decision main and CBaldurChitin's constructor share.
//
// iniWidth: ParseResolution's result. iniHeight: ParseHeight's result (0
// when the key is absent). iniScale: ParseScale's result. desktopWidth and
// desktopHeight: the primary display's size, 0 when unknown. worldScale:
// the World Scale text (ResolveWorldScale), NULL for the value of Scale.
//
// The requested (output) size is the width, or the desktop for
// kResolutionDesktop, with Height when given, else the original 4:3 height
// for 800, 1024, 1600 and 2048, else the desktop's aspect (4:3 when the
// desktop is unknown). The internal size is the output divided by the
// scale; auto picks the largest factor up to kMaxScale that keeps the
// internal height at least 720 and the internal size at least 800x600 (1
// when none does). The internal size is the output divided by the world
// scale (the UI scale unless World Scale says otherwise). An internal size
// below 800x600 is an error.
ScreenSize ResolveScreenSize(int iniWidth, int iniHeight, int iniScale, int desktopWidth, int desktopHeight,
    const char* worldScale = NULL);

// Reads Program Options/Resolution, Height, Scale and World Scale from
// iniFile (through the INI override like every other read) and the primary
// display's size, and resolves them.
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
