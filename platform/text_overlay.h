// Scalable text: the presenter's text overlay. After a frame's regions are
// drawn, the glyphs of the back buffer's text layer (platform/text_layer.h)
// that the presented frame still shows are drawn on top at window
// resolution: each glyph's TrueType outline sized from its bitmap font's
// cap height times its region's scale (times Text Font Scale), on the
// bitmap font's baseline, centred in its cell (squeezed to fit when wider),
// limited to the frame pixels that still hold the glyph's tag, with a
// one-pixel dark outline for light text.
//
// The overlay is composed on the CPU into a window-sized premultiplied
// ARGB8888 image that both presenter backends blend over their output
// (Backend::DrawText), so the classic and gpu presenters show the same
// text. It is recomposed only when what it shows changes.
#pragma once

#include "compat/win_types.h"
#include "platform/text_atlas.h"
#include "platform/text_layer.h"

#include <cstdint>
#include <vector>

namespace platform::text {

// Where one region of the frame lands in the output, in output pixels:
// frame pixel (fx, fy) maps to (x + (fx - frame.left) * sx, y + (fy -
// frame.top) * sy). clip is the output rectangle the region may draw in.
struct Placement {
    RECT frame = {};
    float x = 0.0f;
    float y = 0.0f;
    float sx = 1.0f;
    float sy = 1.0f;
    RECT clip = {};
    bool ui = false;
};

struct Overlay {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels; // premultiplied ARGB8888, width x height
    // Bumped whenever the pixels change; dirty is the part that changed
    // since the previous version (all of it after a resize).
    uint64_t version = 0;
    RECT dirty = {};
    bool empty = true; // no glyph drawn
};

// The glyph's placement: the first UI placement whose frame rectangle holds
// the glyph cell's centre, else the first other one that does; -1 when
// none does.
int PlacementFor(const std::vector<Placement>& placements, const RECT& cell);

class Composer {
public:
    // Composes the visible glyphs of layer over frame (XRGB8888, its tags
    // in the X byte) for an output of outW x outH. Returns true when the
    // overlay changed. Tells the layer which glyphs showed.
    bool Compose(Layer& layer, const uint32_t* frame, int frameW, int frameH, int pitchPixels,
        const std::vector<Placement>& placements, int outW, int outH, float fontScale);
    const Overlay& Result() const { return m_overlay; }
    void Reset();
    const Atlases& Cache() const { return m_atlases; }

private:
    Overlay m_overlay;
    Atlases m_atlases;
    uint64_t m_signature = 0;
    RECT m_drawn = {}; // the bounding box of what the overlay holds
    uint32_t m_frames = 0;
};

// The presenter's composer (one per process).
Composer& PresentComposer();

}
