#include "render/recording.h"

#include "CVidCell.h"
#include "CVidMosaic.h"

namespace render::recording {

namespace {

    std::vector<POINT> Copy(const POINT* pts, int n)
    {
        if (pts == nullptr || n <= 0)
            return {};
        return std::vector<POINT>(pts, pts + n);
    }

}

void Backend::BeginFrame()
{
    calls.push_back(recording::BeginFrame {});
}

void Backend::EndFrame()
{
    calls.push_back(recording::EndFrame {});
}

void Backend::SetLayer(Layer layer)
{
    calls.push_back(recording::SetLayer { layer });
}

void Backend::SetClip(const RECT& clip)
{
    m_clip = clip;
    calls.push_back(recording::SetClip { clip });
}

bool Backend::DrawCell(const CVidCell& cell, int frame, POINT pos, const CVidPalette* palette, DWORD flags, int transVal)
{
    const std::string resref = static_cast<const char*>(cell.GetResRef().GetResRefStr());
    calls.push_back(Cell { &cell, resref, cell.m_nCurrentSequence, frame, pos, palette, flags, transVal, m_clip });
    return true;
}

bool Backend::DrawTile(const CVidTile& tile, POINT pos, DWORD flags)
{
    calls.push_back(Tile { &tile, pos, flags, m_clip });
    return true;
}

bool Backend::DrawText(const CVidFont& font, const char* text, POINT pos, DWORD flags)
{
    calls.push_back(Text { &font, text != nullptr ? text : "", pos, flags, m_clip });
    return true;
}

bool Backend::DrawLine(POINT a, POINT b, COLORREF colour)
{
    calls.push_back(Line { a, b, colour, m_clip });
    return true;
}

bool Backend::DrawRect(const RECT& rect, COLORREF colour, bool fill)
{
    calls.push_back(Rect { rect, colour, fill, m_clip });
    return true;
}

bool Backend::DrawEllipse(POINT centre, SIZE axes, COLORREF colour)
{
    calls.push_back(Ellipse { centre, axes, colour, m_clip });
    return true;
}

bool Backend::DrawPoly(const POINT* pts, int n, COLORREF colour, bool fill, DWORD flags)
{
    calls.push_back(Poly { Copy(pts, n), colour, fill, flags, m_clip });
    return true;
}

bool Backend::Fade(int amount)
{
    calls.push_back(recording::Fade { amount });
    return true;
}

bool Backend::Flash(const RECT& rect, COLORREF colour)
{
    calls.push_back(recording::Flash { rect, colour });
    return true;
}

bool Backend::DrawMosaic(const CVidMosaic& mosaic, POINT pos, const RECT& src, DWORD flags)
{
    const std::string resref = static_cast<const char*>(mosaic.GetResRef().GetResRefStr());
    calls.push_back(Mosaic { &mosaic, resref, pos, src, flags, m_clip });
    return true;
}

bool Backend::DrawBitmap(const CVidBitmap& bitmap, POINT pos, DWORD flags)
{
    calls.push_back(Bitmap { &bitmap, pos, flags, m_clip });
    return true;
}

bool Backend::DrawParticle(const CParticle& particle, POINT origin, DWORD flags, int blobSize)
{
    calls.push_back(Particle { &particle, origin, flags, blobSize, m_clip });
    return true;
}

bool Backend::DrawPoints(const POINT* pts, int n, COLORREF colour)
{
    calls.push_back(Points { Copy(pts, n), colour, m_clip });
    return true;
}

bool Backend::DrawPolyLine(const POINT* pts, int n, COLORREF colour, int thickness)
{
    calls.push_back(PolyLine { Copy(pts, n), colour, thickness, m_clip });
    return true;
}

bool Backend::DrawRecticle(const CVIDMODE_RECTICLE_DESCRIPTION& desc, COLORREF colour)
{
    calls.push_back(Recticle { desc, colour, m_clip });
    return true;
}

Scope::Scope()
    : m_previous(render::Set(&backend))
{
}

Scope::~Scope()
{
    render::Set(m_previous);
}

}
