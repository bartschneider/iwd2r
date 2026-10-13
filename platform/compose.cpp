#define NOMINMAX
#include "platform/compose.h"

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace platform::compose {

namespace {

    // Rectangle edges by axis (0 x, 1 y) and end (0 low, 1 high).
    LONG Edge(const RECT& r, int axis, int end)
    {
        if (axis == 0)
            return end == 0 ? r.left : r.right;
        return end == 0 ? r.top : r.bottom;
    }

    void SetEdge(RECT* r, int axis, int end, LONG value)
    {
        if (axis == 0)
            (end == 0 ? r->left : r->right) = value;
        else
            (end == 0 ? r->top : r->bottom) = value;
    }

    LONG Extent(SIZE s, int axis) { return axis == 0 ? s.cx : s.cy; }

    RECT MakeRect(LONG left, LONG top, LONG right, LONG bottom)
    {
        RECT r;
        r.left = left;
        r.top = top;
        r.right = right;
        r.bottom = bottom;
        return r;
    }

    bool Empty(const RECT& r) { return r.right <= r.left || r.bottom <= r.top; }

    bool Intersects(const RECT& a, const RECT& b)
    {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }

    int64_t Area(const RECT& r)
    {
        return Empty(r) ? 0 : static_cast<int64_t>(r.right - r.left) * static_cast<int64_t>(r.bottom - r.top);
    }

    LONG Clamp(LONG v, LONG lo, LONG hi) { return std::min(std::max(v, lo), hi); }

    // How one panel sits in the frame.
    struct Panel {
        RECT frame; // the 1x rectangle, clipped to the frame
        bool touch[2][2]; // [axis][end]: on that frame edge
        int depth; // the depth axis, -1 for a floating panel
        bool placed;
        RECT band[2]; // [space]: its reserved band in the frame (0) and the window (1)
    };

    // The two spaces bands are built in: the frame, where depths are rounded
    // up to whole engine pixels, and the window, where they are exact at the
    // UI scale.
    struct Space {
        SIZE size;
        int uiScale;
        int worldScale;
        bool window;

        LONG Depth(LONG d) const
        {
            if (window)
                return d * uiScale;
            const LONG big = std::max(uiScale, worldScale);
            return (d * big + worldScale - 1) / worldScale;
        }

        // A frame position no edge or neighbour pins, in this space.
        LONG Position(LONG framePos) const { return window ? framePos * worldScale : framePos; }
    };

    void Classify(Panel* p, SIZE frame)
    {
        for (int axis = 0; axis < 2; axis++) {
            p->touch[axis][0] = Edge(p->frame, axis, 0) <= 0;
            p->touch[axis][1] = Edge(p->frame, axis, 1) >= Extent(frame, axis);
        }
        const bool one[2] = { p->touch[0][0] != p->touch[0][1], p->touch[1][0] != p->touch[1][1] };
        if (one[0] && !one[1]) {
            p->depth = 0;
        } else if (one[1] && !one[0]) {
            p->depth = 1;
        } else if (one[0] && one[1]) {
            // A corner: the axis it is thinner along, relative to the frame.
            const int64_t w = static_cast<int64_t>(p->frame.right - p->frame.left) * frame.cy;
            const int64_t h = static_cast<int64_t>(p->frame.bottom - p->frame.top) * frame.cx;
            p->depth = w <= h ? 0 : 1;
        } else {
            p->depth = -1;
        }
    }

    // The placed panel whose 1x rectangle abuts p's end on axis, or null.
    const Panel* Abutting(const std::vector<Panel>& panels, const Panel& p, int axis, int end)
    {
        const int other = 1 - axis;
        const LONG at = Edge(p.frame, axis, end);
        for (const Panel& q : panels) {
            if (!q.placed || &q == &p)
                continue;
            if (Edge(q.frame, axis, 1 - end) != at)
                continue;
            if (Edge(q.frame, other, 0) < Edge(p.frame, other, 1) && Edge(p.frame, other, 0) < Edge(q.frame, other, 1))
                return &q;
        }
        return nullptr;
    }

    bool Ready(const std::vector<Panel>& panels, const Panel& p)
    {
        if (p.depth < 0)
            return true;
        const int along = 1 - p.depth;
        for (int end = 0; end < 2; end++) {
            if (!p.touch[along][end] && !Abutting(panels, p, along, end))
                return false;
        }
        return true;
    }

    RECT Band(const std::vector<Panel>& panels, const Panel& p, const Space& s, int spaceIndex)
    {
        RECT r = MakeRect(0, 0, 0, 0);
        if (p.depth < 0) {
            // Floating (or spanning both axes): scaled about its centre.
            for (int axis = 0; axis < 2; axis++) {
                const LONG size = Extent(s.size, axis);
                if (p.touch[axis][0] && p.touch[axis][1]) {
                    SetEdge(&r, axis, 0, 0);
                    SetEdge(&r, axis, 1, size);
                    continue;
                }
                const LONG lo = Edge(p.frame, axis, 0);
                const LONG hi = Edge(p.frame, axis, 1);
                const LONG d = s.Depth(hi - lo);
                const LONG start = s.Position(lo + hi) / 2 - d / 2;
                SetEdge(&r, axis, 0, Clamp(start, 0, size));
                SetEdge(&r, axis, 1, Clamp(start + d, 0, size));
            }
            return r;
        }
        const int depth = p.depth;
        const int along = 1 - depth;
        const LONG size = Extent(s.size, depth);
        const LONG d = std::min(s.Depth(Edge(p.frame, depth, 1) - Edge(p.frame, depth, 0)), size);
        if (p.touch[depth][0]) {
            SetEdge(&r, depth, 0, 0);
            SetEdge(&r, depth, 1, d);
        } else {
            SetEdge(&r, depth, 0, size - d);
            SetEdge(&r, depth, 1, size);
        }
        for (int end = 0; end < 2; end++) {
            LONG v = 0;
            if (p.touch[along][end])
                v = end == 0 ? 0 : Extent(s.size, along);
            else if (const Panel* q = Abutting(panels, p, along, end))
                v = Edge(q->band[spaceIndex], along, 1 - end);
            else
                v = s.Position(Edge(p.frame, along, end));
            SetEdge(&r, along, end, Clamp(v, 0, Extent(s.size, along)));
        }
        return r;
    }

    // The largest rectangle of {0, 0, size} that no band covers, and whether
    // the uncovered area is exactly that rectangle.
    RECT Remainder(const std::vector<RECT>& bands, SIZE size, bool* rectangular)
    {
        std::vector<LONG> xs = { 0, size.cx };
        std::vector<LONG> ys = { 0, size.cy };
        for (const RECT& b : bands) {
            xs.push_back(Clamp(b.left, 0, size.cx));
            xs.push_back(Clamp(b.right, 0, size.cx));
            ys.push_back(Clamp(b.top, 0, size.cy));
            ys.push_back(Clamp(b.bottom, 0, size.cy));
        }
        std::sort(xs.begin(), xs.end());
        xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
        std::sort(ys.begin(), ys.end());
        ys.erase(std::unique(ys.begin(), ys.end()), ys.end());

        auto clear = [&bands](const RECT& r) {
            for (const RECT& b : bands) {
                if (Intersects(r, b))
                    return false;
            }
            return true;
        };
        // Every rectangle on the grid of band edges; growing one only adds
        // area, so a covered one ends its row.
        RECT best = MakeRect(0, 0, 0, 0);
        for (size_t x0 = 0; x0 + 1 < xs.size(); x0++) {
            for (size_t y0 = 0; y0 + 1 < ys.size(); y0++) {
                for (size_t x1 = x0 + 1; x1 < xs.size(); x1++) {
                    if (!clear(MakeRect(xs[x0], ys[y0], xs[x1], ys[y0 + 1])))
                        break;
                    for (size_t y1 = y0 + 1; y1 < ys.size(); y1++) {
                        const RECT r = MakeRect(xs[x0], ys[y0], xs[x1], ys[y1]);
                        if (!clear(r))
                            break;
                        if (Area(r) > Area(best))
                            best = r;
                    }
                }
            }
        }

        // Rectangular when every grid cell outside the bands is inside best.
        *rectangular = true;
        for (size_t i = 0; i + 1 < xs.size(); i++) {
            for (size_t j = 0; j + 1 < ys.size(); j++) {
                const RECT cell = MakeRect(xs[i], ys[j], xs[i + 1], ys[j + 1]);
                if (clear(cell) && !Intersects(cell, best))
                    *rectangular = false;
            }
        }
        return best;
    }

    bool Contains(const RECT& r, float x, float y)
    {
        return x >= static_cast<float>(r.left) && x < static_cast<float>(r.right) && y >= static_cast<float>(r.top) && y < static_cast<float>(r.bottom);
    }

    const Region* WorldRegion(const std::vector<Region>& regions)
    {
        for (const Region& r : regions) {
            if (!r.ui)
                return &r;
        }
        return nullptr;
    }

}

std::vector<Region> Layout(SIZE frame, SIZE window, int uiScale, int worldScale,
    const std::vector<RECT>& uiPanelsFrame, const RECT* fullUiFrame)
{
    uiScale = std::max(uiScale, 1);
    worldScale = std::max(worldScale, 1);
    const RECT wholeFrame = MakeRect(0, 0, frame.cx, frame.cy);
    const RECT wholeWindow = MakeRect(0, 0, window.cx, window.cy);

    if (uiScale == worldScale)
        return { Region { wholeFrame, wholeWindow, worldScale, false } };

    if (fullUiFrame) {
        // One UI region centred in the window, cropped about its centre to
        // whole engine pixels when it does not fit.
        RECT f = *fullUiFrame;
        RECT w = MakeRect(0, 0, 0, 0);
        for (int axis = 0; axis < 2; axis++) {
            const LONG lo = Edge(f, axis, 0);
            const LONG n = Edge(f, axis, 1) - lo;
            const LONG fit = std::max(std::min(n, Extent(window, axis) / uiScale), LONG(0));
            const LONG start = lo + (n - fit) / 2;
            SetEdge(&f, axis, 0, start);
            SetEdge(&f, axis, 1, start + fit);
            const LONG wStart = (Extent(window, axis) - fit * uiScale) / 2;
            SetEdge(&w, axis, 0, wStart);
            SetEdge(&w, axis, 1, wStart + fit * uiScale);
        }
        return { Region { f, w, uiScale, true } };
    }

    std::vector<Panel> panels;
    for (const RECT& r : uiPanelsFrame) {
        Panel p {};
        p.frame = MakeRect(Clamp(r.left, 0, frame.cx), Clamp(r.top, 0, frame.cy), Clamp(r.right, 0, frame.cx), Clamp(r.bottom, 0, frame.cy));
        if (Empty(p.frame))
            continue;
        Classify(&p, frame);
        panels.push_back(p);
    }

    // Place the panels whose along-axis ends are frame edges or placed
    // neighbours, round by round; when none is ready the rest keep their
    // own positions.
    const Space spaces[2] = { { frame, uiScale, worldScale, false }, { window, uiScale, worldScale, true } };
    size_t placed = 0;
    bool force = false;
    while (placed < panels.size()) {
        bool progress = false;
        for (Panel& p : panels) {
            if (p.placed || (!force && !Ready(panels, p)))
                continue;
            for (int s = 0; s < 2; s++)
                p.band[s] = Band(panels, p, spaces[s], s);
            p.placed = true;
            placed++;
            progress = true;
        }
        if (!progress)
            force = true;
    }

    std::vector<Region> regions;
    std::vector<RECT> bands[2];
    for (const Panel& p : panels) {
        bands[0].push_back(p.band[0]);
        bands[1].push_back(p.band[1]);
    }
    bool rectangular[2] = { true, true };
    RECT worldFrame = Remainder(bands[0], frame, &rectangular[0]);
    const RECT worldWindow = Remainder(bands[1], window, &rectangular[1]);
    // Edge-aligned panels leave one rectangle; anything else is a layout
    // IWD2 never makes, and a release build goes on with the largest
    // rectangle (see the header).
    assert(rectangular[0] && rectangular[1] && "compose::Layout: the UI panels leave a non-rectangular world");
    (void)rectangular;
    if (!Empty(worldFrame) && !Empty(worldWindow)) {
        // The world takes the slack: its frame fits its window at the scale.
        worldFrame.right = std::min(worldFrame.right, worldFrame.left + (worldWindow.right - worldWindow.left) / worldScale);
        worldFrame.bottom = std::min(worldFrame.bottom, worldFrame.top + (worldWindow.bottom - worldWindow.top) / worldScale);
        regions.push_back(Region { worldFrame, worldWindow, worldScale, false });
    }

    for (const Panel& p : panels) {
        const RECT& w = p.band[1];
        RECT f = p.frame;
        for (int axis = 0; axis < 2; axis++) {
            const LONG n = std::min(Edge(f, axis, 1) - Edge(f, axis, 0), (Edge(w, axis, 1) - Edge(w, axis, 0)) / uiScale);
            // A panel on the high edge keeps its outer pixels when cropped.
            if (axis == p.depth && !p.touch[axis][0])
                SetEdge(&f, axis, 0, Edge(f, axis, 1) - n);
            else
                SetEdge(&f, axis, 1, Edge(f, axis, 0) + n);
        }
        regions.push_back(Region { f, w, uiScale, true });
    }
    return regions;
}

bool WindowToFrame(const std::vector<Region>& regions, float x, float y, float* fx, float* fy)
{
    if (regions.empty())
        return false;
    const Region* hit = nullptr;
    for (const Region& r : regions) {
        if (Contains(r.window, x, y)) {
            hit = &r;
            break;
        }
    }
    if (!hit)
        hit = WorldRegion(regions);
    if (!hit)
        hit = &regions.front();
    const float scale = static_cast<float>(hit->scale);
    *fx = static_cast<float>(hit->frame.left) + (x - static_cast<float>(hit->window.left)) / scale;
    *fy = static_cast<float>(hit->frame.top) + (y - static_cast<float>(hit->window.top)) / scale;
    return true;
}

bool FrameToWindow(const std::vector<Region>& regions, float fx, float fy, float* x, float* y)
{
    if (regions.empty())
        return false;
    const Region* hit = nullptr;
    for (const Region& r : regions) {
        if (r.ui && Contains(r.frame, fx, fy)) {
            hit = &r;
            break;
        }
    }
    if (!hit)
        hit = WorldRegion(regions);
    if (!hit)
        hit = &regions.front();
    const float scale = static_cast<float>(hit->scale);
    *x = static_cast<float>(hit->window.left) + (fx - static_cast<float>(hit->frame.left)) * scale;
    *y = static_cast<float>(hit->window.top) + (fy - static_cast<float>(hit->frame.top)) * scale;
    return true;
}

bool Tiles(const std::vector<Region>& regions, SIZE window)
{
    if (regions.empty())
        return false;
    int64_t total = 0;
    for (size_t i = 0; i < regions.size(); i++) {
        const RECT& w = regions[i].window;
        if (Empty(w) || w.left < 0 || w.top < 0 || w.right > window.cx || w.bottom > window.cy)
            return false;
        for (size_t j = 0; j < i; j++) {
            if (Intersects(w, regions[j].window))
                return false;
        }
        total += Area(w);
    }
    return total == static_cast<int64_t>(window.cx) * static_cast<int64_t>(window.cy);
}

}
