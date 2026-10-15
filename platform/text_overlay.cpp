#define NOMINMAX
#include "platform/text_overlay.h"

#include "platform/text_fonts.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace platform::text {

namespace {

    const uint64_t kFnvOffset = 14695981039346656037ull;
    const uint64_t kFnvPrime = 1099511628211ull;

    uint64_t Mix(uint64_t hash, const void* data, size_t size)
    {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; i++) {
            hash ^= bytes[i];
            hash *= kFnvPrime;
        }
        return hash;
    }

    template <typename T>
    uint64_t MixValue(uint64_t hash, const T& value)
    {
        return Mix(hash, &value, sizeof(value));
    }

    bool Empty(const RECT& r)
    {
        return r.right <= r.left || r.bottom <= r.top;
    }

    RECT Union(const RECT& a, const RECT& b)
    {
        if (Empty(a))
            return b;
        if (Empty(b))
            return a;
        RECT r;
        r.left = std::min(a.left, b.left);
        r.top = std::min(a.top, b.top);
        r.right = std::max(a.right, b.right);
        r.bottom = std::max(a.bottom, b.bottom);
        return r;
    }

    RECT Intersect(const RECT& a, const RECT& b)
    {
        RECT r;
        r.left = std::max(a.left, b.left);
        r.top = std::max(a.top, b.top);
        r.right = std::min(a.right, b.right);
        r.bottom = std::min(a.bottom, b.bottom);
        if (Empty(r))
            r = RECT { 0, 0, 0, 0 };
        return r;
    }

    struct Visible {
        const Glyph* glyph = nullptr;
        int placement = -1;
        RECT box = {}; // the area the glyph may draw in, inside the frame
        std::vector<uint8_t> mask; // box-sized: where it may draw
    };

    // Premultiplied ARGB "over": src over dst.
    uint32_t Over(uint32_t src, uint32_t dst)
    {
        const uint32_t sa = src >> 24;
        if (sa == 255)
            return src;
        const uint32_t inv = 255 - sa;
        uint32_t out = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            const uint32_t s = (src >> shift) & 0xFF;
            const uint32_t d = (dst >> shift) & 0xFF;
            const uint32_t v = s + (d * inv + 127) / 255;
            out |= std::min<uint32_t>(v, 255) << shift;
        }
        return out;
    }

    // A colour at a coverage, premultiplied.
    uint32_t Premultiplied(uint32_t rgb, uint32_t alpha)
    {
        const uint32_t r = ((rgb >> 16) & 0xFF) * alpha / 255;
        const uint32_t g = ((rgb >> 8) & 0xFF) * alpha / 255;
        const uint32_t b = (rgb & 0xFF) * alpha / 255;
        return (alpha << 24) | (r << 16) | (g << 8) | b;
    }

}

int PlacementFor(const std::vector<Placement>& placements, const RECT& cell)
{
    const LONG cx = (cell.left + cell.right) / 2;
    const LONG cy = (cell.top + cell.bottom) / 2;
    int other = -1;
    for (size_t i = 0; i < placements.size(); i++) {
        const RECT& f = placements[i].frame;
        if (cx < f.left || cx >= f.right || cy < f.top || cy >= f.bottom)
            continue;
        if (placements[i].ui)
            return static_cast<int>(i);
        if (other < 0)
            other = static_cast<int>(i);
    }
    return other;
}

void Composer::Reset()
{
    m_overlay = Overlay();
    m_atlases.Clear();
    m_signature = 0;
    m_drawn = RECT { 0, 0, 0, 0 };
}

bool Composer::Compose(Layer& layer, const uint32_t* frame, int frameW, int frameH, int pitchPixels,
    const std::vector<Placement>& placements, int outW, int outH, float fontScale)
{
    m_frames++;
    const std::vector<Glyph> glyphs = layer.Glyphs();
    std::vector<Visible> visible;
    std::vector<uint32_t> serials;
    const RECT frameRect { 0, 0, frameW, frameH };

    uint64_t signature = kFnvOffset;
    signature = MixValue(signature, outW);
    signature = MixValue(signature, outH);
    signature = MixValue(signature, fontScale);
    for (const Placement& p : placements) {
        signature = MixValue(signature, p.frame);
        signature = MixValue(signature, p.x);
        signature = MixValue(signature, p.y);
        signature = MixValue(signature, p.sx);
        signature = MixValue(signature, p.sy);
        signature = MixValue(signature, p.clip);
        signature = MixValue(signature, p.ui);
    }

    for (const Glyph& g : glyphs) {
        if (frame == nullptr || FaceAt(g.face) == nullptr)
            continue;
        const int placement = PlacementFor(placements, g.cell);
        if (placement < 0)
            continue;
        const RECT box = Intersect(g.box, frameRect);
        if (Empty(box))
            continue;
        Visible v;
        v.glyph = &g;
        v.placement = placement;
        // The area the TrueType glyph may cover: its cell's columns one
        // pixel wider, from two cap heights above the baseline to one below
        // (an i's dot or a descender the bitmap glyph's frame does not
        // reach), inside the draw's clip. In its own box it shows only where
        // the tag survives; around it, wherever the clip allows.
        const LONG cap = static_cast<LONG>(std::ceil(g.capHeight));
        RECT reach = { g.cell.left - 1, std::min(box.top, g.baseline - 2 * cap), g.cell.right + 1, std::max(box.bottom, g.baseline + cap) };
        v.box = Intersect(Intersect(reach, g.clip), frameRect);
        if (Empty(v.box))
            continue;
        const int w = v.box.right - v.box.left;
        const int h = v.box.bottom - v.box.top;
        v.mask.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 1);
        bool any = false;
        for (LONG y = box.top; y < box.bottom; y++) {
            const uint32_t* row = frame + static_cast<size_t>(y) * static_cast<size_t>(pitchPixels);
            uint8_t* mrow = v.mask.data() + static_cast<size_t>(y - v.box.top) * static_cast<size_t>(w);
            for (LONG x = box.left; x < box.right; x++) {
                const bool mine = TagOf(row[x]) == g.tag;
                mrow[x - v.box.left] = mine ? 1 : 0;
                any = any || mine;
            }
        }
        if (!any)
            continue;
        serials.push_back(g.serial);
        signature = MixValue(signature, g.serial);
        signature = MixValue(signature, placement);
        signature = Mix(signature, v.mask.data(), v.mask.size());
        visible.push_back(std::move(v));
    }
    layer.Presented(serials);

    const bool resized = m_overlay.width != outW || m_overlay.height != outH;
    if (!resized && signature == m_signature)
        return false;
    m_signature = signature;

    if (resized) {
        m_overlay.width = std::max(outW, 0);
        m_overlay.height = std::max(outH, 0);
        m_overlay.pixels.assign(static_cast<size_t>(m_overlay.width) * static_cast<size_t>(m_overlay.height), 0u);
        m_drawn = RECT { 0, 0, 0, 0 };
    } else if (!Empty(m_drawn)) {
        for (LONG y = m_drawn.top; y < m_drawn.bottom; y++)
            std::fill(m_overlay.pixels.begin() + y * m_overlay.width + m_drawn.left, m_overlay.pixels.begin() + y * m_overlay.width + m_drawn.right, 0u);
    }
    const RECT out { 0, 0, m_overlay.width, m_overlay.height };
    RECT drawn { 0, 0, 0, 0 };

    for (const Visible& v : visible) {
        const Glyph& g = *v.glyph;
        const Placement& p = placements[static_cast<size_t>(v.placement)];
        // The glyph from its face, else the fallback face, else a stand-in
        // character (the soft hyphen as a hyphen).
        const float capPx = g.capHeight * p.sy * fontScale;
        uint16_t codepoint = g.codepoint;
        Atlas* atlas = m_atlases.Get(g.face, capPx);
        if (atlas != nullptr && !atlas->Has(codepoint) && FallbackFace() != g.face)
            atlas = m_atlases.Get(FallbackFace(), capPx);
        if (atlas == nullptr)
            continue;
        if (!atlas->Has(codepoint))
            codepoint = Substitute(codepoint);
        const float cellW = static_cast<float>(g.cell.right - g.cell.left) * p.sx;
        const float advance = atlas->Advance(codepoint);
        const float squeeze = advance > cellW && advance > 0.0f ? cellW / advance : 1.0f;
        const GlyphBitmap& b = atlas->Glyph(codepoint, squeeze);
        if (b.w <= 0 || b.h <= 0)
            continue;
        const float penX = p.x + static_cast<float>(g.cell.left - p.frame.left) * p.sx + (cellW - b.advance) / 2.0f;
        const float baseY = p.y + static_cast<float>(g.baseline - p.frame.top) * p.sy;
        const int ox0 = static_cast<int>(std::lround(penX)) + b.x0;
        const int oy0 = static_cast<int>(std::lround(baseY)) + b.y0;
        const RECT area = Intersect(Intersect(RECT { ox0, oy0, ox0 + b.w, oy0 + b.h }, out), p.clip);
        if (Empty(area))
            continue;
        const bool outlined = (g.flags & kOutline) != 0;
        const int boxW = v.box.right - v.box.left;
        RECT painted { 0, 0, 0, 0 };
        for (LONG oy = area.top; oy < area.bottom; oy++) {
            const LONG fy = p.frame.top + static_cast<LONG>(std::floor((static_cast<float>(oy) + 0.5f - p.y) / p.sy));
            if (fy < v.box.top || fy >= v.box.bottom)
                continue;
            const size_t brow = static_cast<size_t>(oy - oy0) * static_cast<size_t>(b.w);
            uint32_t* orow = m_overlay.pixels.data() + static_cast<size_t>(oy) * static_cast<size_t>(m_overlay.width);
            for (LONG ox = area.left; ox < area.right; ox++) {
                const LONG fx = p.frame.left + static_cast<LONG>(std::floor((static_cast<float>(ox) + 0.5f - p.x) / p.sx));
                if (fx < v.box.left || fx >= v.box.right)
                    continue;
                if (!v.mask[static_cast<size_t>(fy - v.box.top) * static_cast<size_t>(boxW) + static_cast<size_t>(fx - v.box.left)])
                    continue;
                const size_t bi = brow + static_cast<size_t>(ox - ox0);
                const uint32_t cov = b.coverage[bi];
                const uint32_t edge = outlined ? b.outline[bi] : 0;
                if (cov == 0 && edge == 0)
                    continue;
                uint32_t src = Premultiplied(g.colour, cov);
                if (edge > cov)
                    src = Over(src, Premultiplied(g.outline, edge));
                orow[ox] = Over(src, orow[ox]);
                painted = Union(painted, RECT { ox, oy, ox + 1, oy + 1 });
            }
        }
        drawn = Union(drawn, painted);
    }

    m_overlay.dirty = resized ? out : Union(m_drawn, drawn);
    m_drawn = drawn;
    m_overlay.empty = Empty(drawn);
    m_overlay.version++;
    m_atlases.Trim(m_frames);
    return true;
}

Composer& PresentComposer()
{
    static Composer composer;
    return composer;
}

}
