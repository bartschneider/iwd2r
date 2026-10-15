// Scalable text (Program Options/Text Renderer=scalable): a render backend
// that handles DrawText only. Every other call returns false, so everything
// else is drawn by the classic bodies as before.
//
// For a string drawn into the back buffer in a mapped font it lays the
// string out exactly as CVidFont::TextOut does (each character's cell from
// the font's frame size and centre point, the same calls the body makes,
// leaving the font's sequence and frame where the body leaves them), tags
// the cells' pixels in the back buffer instead of drawing the bitmap glyphs
// and adds the glyphs to platform::text::BackLayer() for the presenter to
// draw at window resolution (platform/text_layer.h). Line breaks, widths
// and every layout keep using the bitmap metrics, so nothing but the
// presented image changes.
//
// It declines (the bitmap glyphs are drawn) for: fonts the face table does
// not map (platform/text_fonts.h), the FX surface (CVidInf::FXTextOut: the
// tooltips, CInfToolTip::RenderText, composited onto the back buffer later
// by FXBltToBack; a one-time stderr line says so), the original's own
// TrueType path (Font Name or Double Byte Character Support set) and the
// inert 3d path.
#ifndef IWD2R_RENDER_TEXT_CAPTURE_H_
#define IWD2R_RENDER_TEXT_CAPTURE_H_

#include <vector>

#include "render/api.h"

class CVidFont;

namespace render::text_capture {

// One character as CVidFont::TextOut draws it: the font's frame for it with
// its top left at (x, y - centre.y), x advancing by the frame's width.
struct Cell {
    unsigned char byte;
    RECT rect;
};

// The cells of text drawn at pos, computed with the calls CVidFont::TextOut
// (0x793720, 0x793820) makes: SequenceSet(byte - 1), FrameSet(0),
// GetCurrentFrameSize, GetCurrentCenterPoint. Leaves the font's sequence
// and frame as the body does (at the last character).
std::vector<Cell> Cells(CVidFont& font, const char* text, POINT pos);

class Backend : public render::Backend {
public:
    void SetClip(const RECT& clip) override { m_clip = clip; }
    bool DrawText(const CVidFont& font, const char* text, POINT pos, DWORD flags) override;

private:
    RECT m_clip {};
};

// Installs the capture as the render backend (render::Set) and turns the
// presenter's text layer on, or the reverse. The layer is cleared both
// ways: the glyphs tagged before belong to the other mode's frames.
void Install();
void Uninstall();
bool Installed();

// Glyphs tagged since the process started (for tests and the debug line).
unsigned long CapturedGlyphs();

// Ctrl+Shift+T: Install or Uninstall and have the active screen redraw its
// controls (CChitin::RedrawScreen), so the text comes back in the other
// mode at once. Returns whether the capture is installed now.
bool Toggle();

}

#endif
