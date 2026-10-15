// Scalable text: TrueType glyphs rasterized with stb_truetype
// (third_party/stb), cached per face and pixel size across frames.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace platform::text {

// One rasterized glyph. The bitmap's top left is at (pen x + x0, baseline
// + y0) in output pixels; coverage and outline are w x h, 0-255. outline is
// the coverage grown by one pixel (the dark border drawn under the glyph).
struct GlyphBitmap {
    int w = 0;
    int h = 0;
    int x0 = 0;
    int y0 = 0;
    float advance = 0.0f; // pixels, at xScale
    bool missing = false; // the face has no glyph for the code point
    std::vector<uint8_t> coverage;
    std::vector<uint8_t> outline;
};

// A face at one size: the scale that makes its cap height (the height of
// 'H') capPx pixels.
class Atlas {
public:
    Atlas(int face, float capPx);
    ~Atlas();
    Atlas(const Atlas&) = delete;
    Atlas& operator=(const Atlas&) = delete;

    bool Ok() const;
    int Face() const { return m_face; }
    float CapPx() const { return m_capPx; }
    // The advance of a code point at this size, unscaled horizontally.
    float Advance(uint16_t codepoint) const;
    // The glyph, squeezed horizontally by xScale (0 < xScale <= 1) when it
    // must fit a narrower cell; cached.
    const GlyphBitmap& Glyph(uint16_t codepoint, float xScale);
    size_t Cached() const { return m_glyphs.size(); }
    // Whether the face has a glyph for the code point.
    bool Has(uint16_t codepoint) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    int m_face;
    float m_capPx;
    std::map<std::pair<uint16_t, int>, GlyphBitmap> m_glyphs;
};

// The atlases in use, by (face, cap height in quarter pixels). A size not
// asked for since the last Trim with a later frame number is dropped by
// Trim: a window resize or a scale change rebuilds the glyphs at the new
// size and frees the old ones.
class Atlases {
public:
    Atlas* Get(int face, float capPx);
    void Trim(uint32_t frame);
    void Clear();
    size_t Count() const { return m_atlases.size(); }

private:
    struct Entry {
        std::unique_ptr<Atlas> atlas;
        uint32_t used = 0;
    };
    std::map<std::pair<int, int>, Entry> m_atlases;
    uint32_t m_frame = 0;
};

}
