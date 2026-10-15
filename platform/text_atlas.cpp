#define NOMINMAX
#include "platform/text_atlas.h"

#include "platform/text_fonts.h"

#include <algorithm>
#include <cmath>

// stb_truetype: public domain (third_party/stb, docs/THIRD_PARTY.md). Its
// implementation lives in this file only.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wcomma"
#endif
#elif defined(_MSC_VER)
// C4505 (an unreferenced static function removed) is reported at the end of
// the file, after the pop: off for the whole file.
#pragma warning(disable : 4505)
#pragma warning(push, 0)
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "third_party/stb/stb_truetype.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace platform::text {

struct Atlas::Impl {
    stbtt_fontinfo info = {};
    bool ok = false;
    float scale = 0.0f; // pixels per font unit
};

namespace {

    // The outline: coverage grown by one pixel in each direction (the
    // maximum over the 3x3 neighbourhood), on a bitmap one pixel larger on
    // every side.
    void Grow(GlyphBitmap& g)
    {
        const int w = g.w + 2;
        const int h = g.h + 2;
        std::vector<uint8_t> cov(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
        for (int y = 0; y < g.h; y++)
            for (int x = 0; x < g.w; x++)
                cov[static_cast<size_t>(y + 1) * static_cast<size_t>(w) + static_cast<size_t>(x + 1)] = g.coverage[static_cast<size_t>(y) * static_cast<size_t>(g.w) + static_cast<size_t>(x)];
        std::vector<uint8_t> out(cov.size(), 0);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                uint8_t m = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        const int sx = x + dx;
                        const int sy = y + dy;
                        if (sx < 0 || sy < 0 || sx >= w || sy >= h)
                            continue;
                        m = std::max(m, cov[static_cast<size_t>(sy) * static_cast<size_t>(w) + static_cast<size_t>(sx)]);
                    }
                }
                out[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = m;
            }
        }
        g.coverage.swap(cov);
        g.outline.swap(out);
        g.w = w;
        g.h = h;
        g.x0 -= 1;
        g.y0 -= 1;
    }

}

Atlas::Atlas(int face, float capPx)
    : m_impl(new Impl)
    , m_face(face)
    , m_capPx(capPx)
{
    const ::platform::text::Face* f = FaceAt(face);
    if (f == nullptr || capPx <= 0.0f)
        return;
    const unsigned char* data = f->data;
    const int offset = stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0 || !stbtt_InitFont(&m_impl->info, data, offset))
        return;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!stbtt_GetCodepointBox(&m_impl->info, 'H', &x0, &y0, &x1, &y1) || y1 <= 0)
        return;
    m_impl->scale = capPx / static_cast<float>(y1);
    m_impl->ok = true;
}

Atlas::~Atlas() = default;

bool Atlas::Ok() const
{
    return m_impl->ok;
}

bool Atlas::Has(uint16_t codepoint) const
{
    return m_impl->ok && stbtt_FindGlyphIndex(&m_impl->info, codepoint) != 0;
}

float Atlas::Advance(uint16_t codepoint) const
{
    if (!m_impl->ok)
        return 0.0f;
    int advance = 0;
    int bearing = 0;
    stbtt_GetCodepointHMetrics(&m_impl->info, codepoint, &advance, &bearing);
    return static_cast<float>(advance) * m_impl->scale;
}

const GlyphBitmap& Atlas::Glyph(uint16_t codepoint, float xScale)
{
    xScale = std::min(1.0f, std::max(0.25f, xScale));
    const int key = static_cast<int>(std::lround(xScale * 64.0f));
    auto it = m_glyphs.find({ codepoint, key });
    if (it != m_glyphs.end())
        return it->second;
    GlyphBitmap& g = m_glyphs[{ codepoint, key }];
    if (!m_impl->ok)
        return g;
    const int index = stbtt_FindGlyphIndex(&m_impl->info, codepoint);
    g.missing = index == 0;
    const float sx = m_impl->scale * static_cast<float>(key) / 64.0f;
    const float sy = m_impl->scale;
    int advance = 0;
    int bearing = 0;
    stbtt_GetGlyphHMetrics(&m_impl->info, index, &advance, &bearing);
    g.advance = static_cast<float>(advance) * sx;
    if (g.missing)
        return g;
    int ix0 = 0;
    int iy0 = 0;
    int ix1 = 0;
    int iy1 = 0;
    stbtt_GetGlyphBitmapBox(&m_impl->info, index, sx, sy, &ix0, &iy0, &ix1, &iy1);
    g.w = ix1 - ix0;
    g.h = iy1 - iy0;
    g.x0 = ix0;
    g.y0 = iy0;
    if (g.w <= 0 || g.h <= 0) {
        g.w = 0;
        g.h = 0;
        return g;
    }
    g.coverage.assign(static_cast<size_t>(g.w) * static_cast<size_t>(g.h), 0);
    stbtt_MakeGlyphBitmap(&m_impl->info, g.coverage.data(), g.w, g.h, g.w, sx, sy, index);
    Grow(g);
    return g;
}

Atlas* Atlases::Get(int face, float capPx)
{
    const int quarter = std::max(1, static_cast<int>(std::lround(capPx * 4.0f)));
    Entry& e = m_atlases[{ face, quarter }];
    if (!e.atlas)
        e.atlas.reset(new Atlas(face, static_cast<float>(quarter) / 4.0f));
    e.used = m_frame;
    return e.atlas->Ok() ? e.atlas.get() : nullptr;
}

void Atlases::Trim(uint32_t frame)
{
    for (auto it = m_atlases.begin(); it != m_atlases.end();) {
        if (it->second.used < m_frame)
            it = m_atlases.erase(it);
        else
            ++it;
    }
    m_frame = frame;
}

void Atlases::Clear()
{
    m_atlases.clear();
}

}
