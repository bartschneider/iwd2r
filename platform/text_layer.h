// Scalable text (Program Options/Text Renderer=scalable): the glyphs the
// engine drew into its back buffer, kept like the back buffer's own pixels.
//
// The capture (src/render/text_capture) does not draw a bitmap glyph; it
// writes a tag into the X byte (bits 24-31) of every pixel of the glyph's
// cell and adds the glyph to the layer. The RGB bytes keep the background.
// The tags then live exactly as the glyph's pixels would have lived in the
// classic renderer: whatever draws over a cell clears its tag (the engine's
// own pixels carry 0x00 or 0xFF there), and a blit carries the tags along
// (the pointer's save-under restore puts them back). When the frame is
// presented, a glyph is visible on the pixels of its cell that still hold
// its tag, and the presenter draws its TrueType outline there
// (platform/text_overlay.h).
//
// Two live glyphs whose cells overlap never share a tag. A glyph whose cell
// a new glyph covers completely is dropped (every pixel of it now holds the
// new tag); one covered partly keeps the rest.
#pragma once

#include "compat/win_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace platform::text {

// The tag of an XRGB8888 pixel.
inline uint8_t TagOf(uint32_t pixel)
{
    return static_cast<uint8_t>(pixel >> 24);
}

// Whether a byte can be a tag: 1 to 254, except 0x7F and 0x80. The engine's
// own pixels hold 0x00 or 0xFF in the X byte (realized palettes set 0xFF;
// CVidPoly's translucent fill halves it to 0x7F), so those never mark text.
bool IsTag(uint8_t value);

enum GlyphFlags : uint8_t {
    kOutline = 1, // a one-pixel dark outline (light text, as the bitmap fonts' border)
};

struct Glyph {
    RECT cell = {}; // the bitmap font's frame for the character, frame pixels
    RECT box = {}; // the tagged pixels: the cell inside the clip and the locked rectangle
    int baseline = 0; // frame y of the baseline
    float capHeight = 0.0f; // the bitmap font's cap height, frame pixels
    uint32_t colour = 0; // 0xRRGGBB
    uint32_t outline = 0; // 0xRRGGBB
    uint16_t codepoint = 0; // Unicode, decoded from Windows-1252
    uint8_t face = 0; // platform/text_fonts face
    uint8_t flags = 0; // GlyphFlags
    uint8_t tag = 0; // set by Layer::Add
    uint32_t serial = 0; // set by Layer::Add: the order glyphs were added in
    uint32_t seen = 0; // the present count when the glyph last showed (Layer::Present)
};

class Layer {
public:
    // Adds g with a tag no live glyph overlapping it holds, after dropping
    // the glyphs whose boxes g's box contains. Returns the tag (0 when g's
    // box is empty: nothing to tag, the glyph is not kept). kept, when not
    // null, receives the live glyphs that overlap g and stay: the caller
    // tags only the pixels of g's box that none of them still owns (as a
    // bitmap glyph's transparent pixels leave the text under them), plus
    // g's own ink.
    uint8_t Add(const Glyph& g, std::vector<Glyph>* kept = nullptr);
    void Clear();
    size_t Size() const;
    // Every glyph, in the order they were added.
    std::vector<Glyph> Glyphs() const;
    // After a present: counts it and marks the glyphs that showed, then
    // forgets glyphs that have not shown for a long time while the layer is
    // large (a cell drawn over for good, a screen left).
    void Presented(const std::vector<uint32_t>& visibleSerials);
    uint32_t PresentCount() const;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<uint32_t, Glyph> m_glyphs; // by serial
    uint32_t m_serial = 0;
    uint32_t m_presents = 0;
};

// The layer of the engine's back buffer (one per process).
Layer& BackLayer();

// Whether the capture is installed (Text Renderer=scalable and not toggled
// off): the presenter draws the layer only then.
void SetEnabled(bool enabled);
bool Enabled();

// Program Options/Text Font Scale: a multiplier on the glyph size, 0.5 to
// 2, 1 until set.
void SetFontScale(float scale);
float FontScale();
// "1", "1.25" and so on: false for anything that is not a number from 0.5
// to 2.
bool ParseFontScale(const char* text, float* scale);
// "classic" or "scalable" (any case, blanks ignored); false otherwise.
bool ParseRenderer(const char* text, bool* scalable);

// Ctrl+Shift+T: the handler iwd2r_main installs (it swaps the capture in
// or out and has the screen redrawn). Toggle calls it; without one it does
// nothing and returns false.
void SetToggleHandler(std::function<bool()> handler);
bool Toggle();

}
