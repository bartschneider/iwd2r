// The render API: the interface a renderer implements to receive the engine's
// drawing calls instead of the CPU back buffer.
//
// The classic renderer is the absence of a backend. Each drawing entry point
// of the decompiled CVid* classes starts with one guard line,
//
//     if (auto* b = render::Active(); b && b->DrawCell(...)) return TRUE;
//
// (the CVidCell guards return render::CellHandled(...) instead of TRUE; see
// below). The inlined CVidMode::UpdateFade and SetFade, which have no
// address, report the new fade level with one line at their end instead.
//
// so with no backend installed Active() is null, every guard falls through
// and the decompiled body draws exactly as before. With a backend installed
// the guard hands it the call; when the backend returns true the function
// returns at once and its body (which locks and writes a surface) does not
// run. A backend that returns false leaves the call to the classic body.
//
// Every call carries the resource identity (the CVidCell, CVidTile, CVidFont,
// CVidMosaic or CVidBitmap with its current sequence, frame and palette),
// never pixels, and every position, size and clip is in the original's
// logical screen coordinates (docs/research/2026-10-13-hd-upscaling-spike.md,
// "Architecture recommendation"): a backend that draws textures at N times
// the size scales on its side. The clip of a call is set with SetClip just
// before it (Backend::Clipped).
//
// Coordinates per entry point. The CVidInf::BK* functions draw into the back
// surface locked at m_rLockedRect with positions relative to it; their guards
// add the locked rectangle's origin, as the original's 3d path does, so the
// backend sees back-buffer coordinates (BKRenderConvexPoly's vertices and
// clip are already absolute there, also as in the 3d path).
//
// The CVidInf::FX* functions draw into the FX surface, and their guards pass
// FX-surface coordinates unchanged. The placement on screen is not in the
// call: FXPrep chooses the FX rectangle from ptPos and ptReference and the
// mirror flags 0x10/0x20, and FXUnlock (with CInfinity::FXBltFrom) composites
// the FX surface onto the back buffer. Neither is guarded, so a backend that
// handles FX draws cannot place them yet; it needs begin-group and end-group
// calls at FXPrep and FXUnlock (docs/followups.md).
//
// The pointer: CVidInf::RenderPointer draws its cell with
// CVidCell::Render(CVIDINF_SURFACE_FRONT, ...) in screen coordinates, so it
// reaches the backend as a DrawCell that looks like any back-buffer draw; the
// surface is not passed (docs/followups.md: a DrawPointer primitive or the
// surface number).
//
// What a handling backend inherits. When a backend handles a call the body
// does not run, so the backend takes over what the body would have done
// besides drawing:
//   - It fetches the BAM frame itself (CVidCell::GetFrame and a matching
//     release, or its own decoder). cell.m_pFrame may be NULL on entry; when
//     it is not, the caller demanded the resource and the body would have
//     released it. The CVidCell guards end with render::CellHandled, which
//     does exactly the body's tail (release and clear m_pFrame), so demands
//     stay balanced.
//   - CVidFont::TextOut(sString, x, y, rClip, nSurface) is one DrawText: the
//     body's per-character SequenceSet and FrameSet do not run, so the font's
//     current sequence and frame are left as they were.
//   - The CVidMosaic::Render and CVidMode::EraseScreen guards run before the
//     bodies' early-outs (a null clip; a surface number out of range or a
//     missing surface), so the backend also sees calls the body would have
//     ignored.
//
// Guarded entry points, the outermost call the engine makes for each kind of
// drawing (one dispatch per drawing call):
//
//   CVidCell::Render(nSurface, ...)               0x7AEAD0  DrawCell
//   CVidTile::Render(pSurface, ...)               0x7B1780  DrawTile (*)
//   CVidTile::Render(pSurface, ..., pResStencil)  0x7B1B30  DrawTile (*)
//   CVidFont::TextOut(sString, x, y, ..., nSurface) 0x793720 DrawText
//   CVidMode::DrawLine(..., nSurface, ...)        0x797A30  DrawLine
//   CVidMode::DrawRect                            0x7980E0  DrawRect
//   CVidMode::DrawEllipse(..., rClip, ...)        0x796050  DrawEllipse
//   CVidMode::DrawRecticle(rd, rClip, ...)        0x798780  DrawRecticle
//   CVidMode::OutlinePoly                         0x799540  DrawPoly
//   CVidMode::PolyLine                            0x79A080  DrawPolyLine
//   CVidMode::EraseScreen                         0x7999B0  DrawRect (filled)
//   CVidMode::UpdateFade (inlined, no address)              Fade
//   CVidMode::SetFade (inlined, no address)                 Fade
//   CVidInf::FXRender (cell)                      0x79D100  DrawCell
//   CVidInf::FXRender (cell, clip)                0x79D180  DrawCell
//   CVidInf::FXRender (particle)                  0x79D220  DrawParticle
//   CVidInf::FXRender (points)                    0x79D450  DrawPoints
//   CVidInf::FXTextOut                            0x79D970  DrawText
//   CVidInf::BKRender (cell, clip)                0x79DE50  DrawCell
//   CVidInf::BKRender (cell)                      0x79DF60  DrawCell
//   CVidInf::BKRender (particle)                  0x79E060  DrawParticle
//   CVidInf::BKRenderEllipse                      0x79E0D0  DrawEllipse
//   CVidInf::BKRenderLine                         0x79E1D0  DrawLine
//   CVidInf::BKRenderConvexPoly                   0x79E2D0  DrawPoly (filled)
//   CVidInf::BKTextOut                            0x79E340  DrawText
//   CVidInf::RenderFlash                          0x79F520  Flash
//   CVidMosaic::Render                            0x7B0C80  DrawMosaic
//   CVidBitmap::RenderDirect                      0x7B3190  DrawBitmap
//
// (*) Not a screen-level tile draw. CVidTile::Render fills the VRAM tile cache
// from CInfTileSet::RenderToPrimary on a cache miss only, at (0, 0) of the
// cache surface, without the stencil tile (the second overload's pResStencil)
// or fog. Each frame's tiles reach the screen through CInfTileSet::Render
// (0x5CB4A0) and CInfTileSet::RenderBlack, which blit from that cache; a
// screen-level renderer must guard those instead (docs/followups.md).
//
// Internal helpers, not guarded because a guarded caller already dispatches
// them (guarding both levels would dispatch a declined call twice):
//
//   CVidCell::Render(pSurface, lPitch, ...)  0x7AE5D0, 0x7AF060: called only by
//     CVidInf::BKRender, CVidInf::FXRender, CVidFont::TextOut(pSurface, ...)
//     and CVidInf::LoadFogOWarSurfaces (which builds the fog-of-war tiles in
//     an off-screen surface, not a draw).
//   CVidFont::TextOut(sString, pSurface, ...) 0x793820 and
//     CVidFont::TextOutEx 0x793A70: called only by BKTextOut and FXTextOut.
//   CVidMode::DrawLine(..., ddsd, ...) 0x797B80, DrawEllipse(..., ddsd, ...)
//     0x7961D0, DrawRecticle(rd, ddsd, ...) 0x798930: the locked-surface
//     halves of the guarded functions (and of FXPrep's debug outline).
//   CVidPoly::FillPoly 0x7C13A0 and FillConvexPoly 0x7C0F40: called by
//     CVidCell::Render(nSurface) for its occlusion polygons, by
//     CVidInf::FXUnlock for the FX rectangle (guarded or compositing code),
//     by CVidInf::BKRenderConvexPoly (guarded), and by
//     CInfinity::FXRenderClippingPolys (below).
//   CInfinity::FXRenderClippingPolys 0x5CE350: fills the WED wall polygons
//     that cover an object straight into the FX surface with the shadow
//     colour, between FXPrep and FXUnlock: a mask for the FX composite, not
//     a screen draw. A backend that handles FX draws needs it with the FX
//     begin-group and end-group calls (docs/followups.md, FX groups).
//   CInfinity::RenderMessageScreen 0x5D2530: its viewport fill is passed
//     to the backend as a filled DrawRect at the fill (as EraseScreen's), the
//     message bitmap goes through CVidBitmap::RenderDirect (DrawBitmap) and
//     the label through CVidFont::TextOut (DrawText); the function itself is
//     not one draw, so it has no guard at its top.
//   CVidInf::RenderPointer() 0x79F6A0, RenderPointer(nSurface) 0x79F950 and
//     RenderPointerImage 0x79FBA0: the pointer and its count digits are drawn
//     with CVidCell::Render(nSurface), which dispatches each cell; the rest is
//     the pointer's save-under bookkeeping. CVidMode's versions and
//     CVidMode::RenderFlash are empty.
//   CInfinity::RenderLightning 0x5CFB40: it draws with twelve PolyLine calls,
//     which dispatch; guarding it instead would skip its rand() calls and
//     change the game's random sequence whenever a backend is installed.
//
// The *3d variants are not guarded: the OpenGL path is inert.
#ifndef IWD2R_RENDER_API_H_
#define IWD2R_RENDER_API_H_

#include <vector>

#include "mfc.h"

#include "CVidMode.h"

class CParticle;
class CVidBitmap;
class CVidCell;
class CVidFont;
class CVidMosaic;
class CVidPalette;
class CVidPoly;
class CVidTile;

namespace render {

enum class Layer {
    World,
    Ui,
};

// Every Draw* returns true when it handled the call (the classic body is then
// skipped) and false to leave it to the classic body. The defaults handle
// nothing, so a backend can take over one primitive at a time.
struct Backend {
    virtual ~Backend() = default;

    // Frame and state. Nothing in the engine calls these yet; the renderer
    // that needs them wires them to the screens' frame loop.
    virtual void BeginFrame() { }
    virtual void EndFrame() { }
    virtual void SetLayer(Layer) { }
    virtual void SetClip(const RECT&) { }

    // A BAM frame: the cell's current sequence (cell.m_nCurrentSequence) and
    // `frame` at `pos` (the frame's reference point), with the cell's
    // palette. transVal is the translucency value, -1 for none.
    virtual bool DrawCell(const CVidCell&, int /*frame*/, POINT /*pos*/, const CVidPalette*, DWORD /*flags*/, int /*transVal*/) { return false; }
    // A TIS tile (the tile is the CVidTile's resource) with its top left at
    // pos. Both CVidTile::Render overloads dispatch here; the stencil tile of
    // the second is not passed.
    virtual bool DrawTile(const CVidTile&, POINT /*pos*/, DWORD /*flags*/) { return false; }
    // A string in the font's current colours, its start at pos.
    virtual bool DrawText(const CVidFont&, const char* /*text*/, POINT /*pos*/, DWORD /*flags*/) { return false; }
    virtual bool DrawLine(POINT /*a*/, POINT /*b*/, COLORREF) { return false; }
    // fill false draws the outline.
    virtual bool DrawRect(const RECT&, COLORREF, bool /*fill*/) { return false; }
    virtual bool DrawEllipse(POINT /*centre*/, SIZE /*axes*/, COLORREF) { return false; }
    // A closed polygon; fill false draws the outline. flags are the fill
    // flags CVidPoly::FillConvexPoly takes (BKRenderConvexPoly's dwFlags;
    // CUIPanel passes 0x2, translucent, as FillConvexPoly3d reads it); 0 for
    // OutlinePoly.
    virtual bool DrawPoly(const POINT* /*pts*/, int /*n*/, COLORREF, bool /*fill*/, DWORD /*flags*/) { return false; }
    // The fade level after each UpdateFade step and after each SetFade, from
    // 0 (black) to CVidMode::NUM_FADE_FRAMES (none). A notification at the
    // end of those inlined functions: the return value is ignored and the
    // classic fade state always changes, because screens wait on it
    // (CVidMode::GetFullyFaded).
    virtual bool Fade(int /*amount*/) { return false; }
    virtual bool Flash(const RECT&, COLORREF) { return false; }

    // Primitives the engine draws that the calls above cannot express.
    // A MOS: the part rMosaic of the mosaic with its top left at pos.
    virtual bool DrawMosaic(const CVidMosaic&, POINT /*pos*/, const RECT& /*src*/, DWORD /*flags*/) { return false; }
    // A BMP with its top left at pos.
    virtual bool DrawBitmap(const CVidBitmap&, POINT /*pos*/, DWORD /*flags*/) { return false; }
    // A particle (weather, spell sparks). Its positions are relative to
    // origin: the locked back-buffer rectangle's top left for BKRender,
    // (0, 0) of the FX surface for FXRender.
    virtual bool DrawParticle(const CParticle&, POINT /*origin*/, DWORD /*flags*/, int /*blobSize*/) { return false; }
    // Single pixels.
    virtual bool DrawPoints(const POINT* /*pts*/, int /*n*/, COLORREF) { return false; }
    // An open polyline `thickness` pixels wide (lightning bolts).
    virtual bool DrawPolyLine(const POINT* /*pts*/, int /*n*/, COLORREF, int /*thickness*/) { return false; }
    // A selection marker: a dashed ellipse with pie-piece gaps.
    virtual bool DrawRecticle(const CVIDMODE_RECTICLE_DESCRIPTION&, COLORREF) { return false; }

    // SetClip, then the call it is chained to: b->Clipped(r).DrawCell(...).
    Backend& Clipped(const RECT& clip)
    {
        SetClip(clip);
        return *this;
    }
};

namespace detail {
    extern Backend* active;
}

// Installs a backend (nullptr restores the classic renderer); returns the
// previous one. Not thread-safe: install before the engine draws.
Backend* Set(Backend* backend);

// The installed backend, or nullptr for the classic renderer. Every guard
// calls this, so it is one read of a global.
inline Backend* Active()
{
    return detail::active;
}

enum class Mode {
    Classic,
    Backend,
};

inline Mode Current()
{
    return Active() != nullptr ? Mode::Backend : Mode::Classic;
}

// Guard helpers.

// (x, y) relative to a locked rectangle, in screen coordinates.
inline POINT At(int x, int y, const RECT& origin)
{
    return POINT { origin.left + x, origin.top + y };
}

// r relative to a locked rectangle, in screen coordinates.
inline RECT At(const RECT& r, const RECT& origin)
{
    return RECT { origin.left + r.left, origin.top + r.top, origin.left + r.right, origin.top + r.bottom };
}

// The tail of a handled CVidCell draw, as the body's own tail:
// CVidCell::Render(nSurface, ...) (0x7AEAD0) and the pSurface overload
// 0x7AE5D0 (BKRender and FXRender without a clip) release the resource and
// clear m_pFrame when the frame was fetched (here: when the caller had
// fetched it, since the body's own GetFrame did not run). Returns TRUE.
BOOL CellHandled(CVidCell& cell);

// The same for the pSurface overload 0x7AF060 (BKRender and FXRender with a
// clip): clears m_pFrame and releases only when bDemanded is FALSE.
BOOL CellHandled(CVidCell& cell, BOOLEAN bDemanded);

// A CVidPoly's m_nVertices vertices as points (zeros when it has none).
std::vector<POINT> Vertices(const CVidPoly& poly);

// pts[0..n) minus offset.
std::vector<POINT> Minus(const POINT* pts, int n, POINT offset);

}

#endif
