// A render backend that records every call and handles all of them, for
// tests: with it installed no guarded entry point touches a surface.
#ifndef IWD2R_RENDER_RECORDING_H_
#define IWD2R_RENDER_RECORDING_H_

#include <string>
#include <variant>
#include <vector>

#include "render/api.h"

namespace render::recording {

struct BeginFrame { };
struct EndFrame { };
struct SetLayer {
    Layer layer;
};
struct SetClip {
    RECT clip;
};

// Each draw records the clip set before it (Backend::Clipped).
struct Cell {
    const CVidCell* cell;
    std::string resref;
    int sequence;
    int frame;
    POINT pos;
    const CVidPalette* palette;
    DWORD flags;
    int transVal;
    RECT clip;
};
struct Tile {
    const CVidTile* tile;
    POINT pos;
    DWORD flags;
    RECT clip;
};
struct Text {
    const CVidFont* font;
    std::string text;
    POINT pos;
    DWORD flags;
    RECT clip;
};
struct Line {
    POINT a;
    POINT b;
    COLORREF colour;
    RECT clip;
};
struct Rect {
    RECT rect;
    COLORREF colour;
    bool fill;
    RECT clip;
};
struct Ellipse {
    POINT centre;
    SIZE axes;
    COLORREF colour;
    RECT clip;
};
struct Poly {
    std::vector<POINT> points;
    COLORREF colour;
    bool fill;
    RECT clip;
};
struct Fade {
    int amount;
};
struct Flash {
    RECT rect;
    COLORREF colour;
};
struct Mosaic {
    const CVidMosaic* mosaic;
    std::string resref;
    POINT pos;
    RECT src;
    DWORD flags;
    RECT clip;
};
struct Bitmap {
    const CVidBitmap* bitmap;
    POINT pos;
    DWORD flags;
    RECT clip;
};
struct Particle {
    const CParticle* particle;
    POINT origin;
    DWORD flags;
    int blobSize;
    RECT clip;
};
struct Points {
    std::vector<POINT> points;
    COLORREF colour;
    RECT clip;
};
struct PolyLine {
    std::vector<POINT> points;
    COLORREF colour;
    int thickness;
    RECT clip;
};
struct Recticle {
    CVIDMODE_RECTICLE_DESCRIPTION desc;
    COLORREF colour;
    RECT clip;
};

using Call = std::variant<BeginFrame, EndFrame, SetLayer, SetClip, Cell, Tile, Text, Line, Rect, Ellipse, Poly, Fade, Flash, Mosaic, Bitmap, Particle, Points, PolyLine, Recticle>;

class Backend : public render::Backend {
public:
    std::vector<Call> calls;

    void BeginFrame() override;
    void EndFrame() override;
    void SetLayer(Layer layer) override;
    void SetClip(const RECT& clip) override;
    bool DrawCell(const CVidCell& cell, int frame, POINT pos, const CVidPalette* palette, DWORD flags, int transVal) override;
    bool DrawTile(const CVidTile& tile, POINT pos, DWORD flags) override;
    bool DrawText(const CVidFont& font, const char* text, POINT pos, DWORD flags) override;
    bool DrawLine(POINT a, POINT b, COLORREF colour) override;
    bool DrawRect(const RECT& rect, COLORREF colour, bool fill) override;
    bool DrawEllipse(POINT centre, SIZE axes, COLORREF colour) override;
    bool DrawPoly(const POINT* pts, int n, COLORREF colour, bool fill) override;
    bool Fade(int amount) override;
    bool Flash(const RECT& rect, COLORREF colour) override;
    bool DrawMosaic(const CVidMosaic& mosaic, POINT pos, const RECT& src, DWORD flags) override;
    bool DrawBitmap(const CVidBitmap& bitmap, POINT pos, DWORD flags) override;
    bool DrawParticle(const CParticle& particle, POINT origin, DWORD flags, int blobSize) override;
    bool DrawPoints(const POINT* pts, int n, COLORREF colour) override;
    bool DrawPolyLine(const POINT* pts, int n, COLORREF colour, int thickness) override;
    bool DrawRecticle(const CVIDMODE_RECTICLE_DESCRIPTION& desc, COLORREF colour) override;

    // The calls of type T, in order.
    template <typename T>
    std::vector<T> Of() const
    {
        std::vector<T> out;
        for (const Call& call : calls)
            if (const T* p = std::get_if<T>(&call))
                out.push_back(*p);
        return out;
    }

private:
    RECT m_clip {};
};

// Installs a recording backend for its lifetime and restores the previous
// one afterwards.
class Scope {
public:
    Scope();
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    Backend backend;

private:
    render::Backend* m_previous;
};

}

#endif
