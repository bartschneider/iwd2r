// Region composition for UI scaling: which part of the engine's one back
// buffer (the frame, engine pixels) is shown where in the window (window
// pixels) and at what scale, and the piecewise mouse mapping that follows.
// Pure math, no SDL: the layout code builds the region list, the presenter
// draws it and maps the mouse through it.
//
// A region shows its frame rectangle at `scale` with the frame rectangle's
// top-left at the window rectangle's top-left. The window rectangle is at
// least frame size x scale; what is left over at its right and bottom (the
// slack, a few pixels from rounding a reserved rectangle outward) is not
// drawn, the presenter clears it.
#pragma once

#include "compat/win_types.h"

#include <vector>

namespace platform::compose {

struct Region {
    RECT frame; // engine pixels
    RECT window; // window pixels
    int scale; // window pixels per engine pixel
    bool ui; // a UI panel (or full-UI screen) region, not the world
};

// The regions for one screen. frame is the engine's internal size (the
// window divided by worldScale, rounded down); window the window's size.
//
// uiScale == worldScale: one world region, the whole frame on the whole
// window, as the presenter has always shown it.
//
// fullUiFrame non-null (a full-UI screen such as the inventory): one UI
// region, *fullUiFrame (its 1x rectangle in the frame) at uiScale centred in
// the window; cropped about its centre to whole engine pixels when it does
// not fit.
//
// Otherwise uiPanelsFrame are the UI panels' 1x rectangles in the frame. The
// result is the world region first (omitted when the panels leave no world)
// and then one UI region per panel, in order. Each panel reserves a band in
// the frame that the world never covers:
// - Its depth axis (the axis on which it touches exactly one frame edge; on
//   a corner, the axis it is thinner along relative to the frame) is its
//   depth x uiScale / worldScale, rounded up to whole engine pixels, from
//   that edge, and never less than the depth itself (with uiScale <
//   worldScale the scaled depth is smaller than the panel the engine draws,
//   which the world must not cover either). Its window band is exactly
//   depth x uiScale from the window's same edge, so the panel's pixels meet
//   the window edge.
// - Along the other axis it spans from the frame edge it touches, or the
//   band of the panel it abuts (a side panel stopping at the toolbar), to
//   the same on the other side; an end that is neither keeps the panel's
//   own position (scaled by worldScale in the window).
// The world region is the frame minus the reserved bands, and its window
// rectangle the window minus the panels' window bands: the world takes the
// rounding slack (under worldScale pixels per band with uiScale >=
// worldScale; depth x (worldScale - uiScale) more below that). Edge-aligned bands
// (IWD2's toolbar, side bars and borders) always leave a rectangle. When
// they do not (a floating panel), Layout asserts in a debug build and in a
// release build uses the largest rectangle left, which then leaves gaps
// that Tiles reports.
//
// A UI region's frame is the panel's 1x rectangle clipped to what fits its
// window band at uiScale; along a spanning axis with uiScale < worldScale the
// panel is narrower than its band and the rest of the band is slack.
std::vector<Region> Layout(SIZE frame, SIZE window, int uiScale, int worldScale,
    const std::vector<RECT>& uiPanelsFrame, const RECT* fullUiFrame);

// Maps a window point to the frame through the first region whose window
// rectangle contains it, else the world region (else the first region, on a
// full-UI screen): frame origin + (point - window origin) / scale. False,
// leaving fx/fy untouched, only when regions is empty. The result is not
// clamped: the caller clamps to the frame.
bool WindowToFrame(const std::vector<Region>& regions, float x, float y, float* fx, float* fy);

// The inverse: through the first UI region whose frame rectangle contains
// the point, else the world region, else the first region.
bool FrameToWindow(const std::vector<Region>& regions, float fx, float fy, float* x, float* y);

// True when the regions' window rectangles cover {0, 0, window} exactly,
// with no gap and no overlap, and none is empty.
bool Tiles(const std::vector<Region>& regions, SIZE window);

}
