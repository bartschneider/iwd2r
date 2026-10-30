// Scalable text, task 2: the faces, the character set and the glyph atlas.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "platform/text_atlas.h"
#include "platform/text_fonts.h"

#include <algorithm>
#include <string>
#include <vector>

namespace text = platform::text;

namespace {

// Every Windows-1252 code point a string from dialog.tlk can hold: the
// printable ASCII range, the assigned 0x80-0x9F bytes and 0xA0-0xFF.
std::vector<uint16_t> Cp1252Printable()
{
    std::vector<uint16_t> out;
    for (int byte = 0x20; byte <= 0xFF; byte++) {
        if (byte == 0x7F)
            continue;
        const uint16_t cp = text::DecodeCp1252(static_cast<uint8_t>(byte));
        if (byte >= 0x80 && byte < 0xA0 && cp == byte)
            continue; // unassigned
        out.push_back(cp);
    }
    return out;
}

int InkRows(const text::GlyphBitmap& g)
{
    int top = -1;
    int bottom = -1;
    for (int y = 0; y < g.h; y++)
        for (int x = 0; x < g.w; x++)
            if (g.coverage[static_cast<size_t>(y) * static_cast<size_t>(g.w) + static_cast<size_t>(x)] >= 128) {
                if (top < 0)
                    top = y;
                bottom = y;
            }
    return top < 0 ? 0 : bottom - top + 1;
}

}

TEST_CASE("text_cp1252_decoding", "[text]")
{
    REQUIRE(text::DecodeCp1252('A') == 'A');
    REQUIRE(text::DecodeCp1252(0x20) == 0x20);
    REQUIRE(text::DecodeCp1252(0x80) == 0x20AC); // euro sign
    REQUIRE(text::DecodeCp1252(0x85) == 0x2026); // ellipsis
    REQUIRE(text::DecodeCp1252(0x92) == 0x2019); // right single quote (dialog.tlk's apostrophe)
    REQUIRE(text::DecodeCp1252(0x93) == 0x201C);
    REQUIRE(text::DecodeCp1252(0x96) == 0x2013);
    REQUIRE(text::DecodeCp1252(0x97) == 0x2014);
    REQUIRE(text::DecodeCp1252(0x9F) == 0x0178);
    REQUIRE(text::DecodeCp1252(0xA9) == 0x00A9);
    REQUIRE(text::DecodeCp1252(0xE9) == 0x00E9);
    REQUIRE(text::DecodeCp1252(0xFF) == 0x00FF);
    const uint8_t unassignedCodes[] = { 0x81, 0x8D, 0x8F, 0x90, 0x9D };
    for (uint8_t unassigned : unassignedCodes)
        REQUIRE(text::DecodeCp1252(unassigned) == unassigned);
}

TEST_CASE("text_face_table", "[text]")
{
    int face = -1;
    float factor = 0.0f;
    REQUIRE(text::FaceFor("NORMAL", &face, &factor));
    REQUIRE(face == text::FallbackFace());
    REQUIRE(factor > 0.0f);
    REQUIRE(text::FaceFor("normal", &face, &factor));
    REQUIRE(text::FaceFor("STONEBIG", &face, &factor));
    REQUIRE(text::FaceFor("STONESML", &face, &factor));
    REQUIRE(text::FaceFor("REALMS", &face, &factor));
    REQUIRE(text::FaceFor("TOOLFONT", &face, &factor));
    // Drop caps and symbol fonts keep their bitmaps.
    REQUIRE_FALSE(text::FaceFor("INITIALS", &face, &factor));
    REQUIRE_FALSE(text::FaceFor("STATES2", &face, &factor));
    REQUIRE_FALSE(text::FaceFor("NUMBER", &face, &factor));
    REQUIRE_FALSE(text::FaceFor("NORMALX", &face, &factor));

    REQUIRE(text::FaceCount() >= 1);
    for (int i = 0; i < text::FaceCount(); i++) {
        INFO("face " << i);
        REQUIRE(text::FaceAt(i) != nullptr);
        REQUIRE(text::FaceAt(i)->size > 1000);
        text::Atlas atlas(i, 12.0f);
        REQUIRE(atlas.Ok());
    }
    REQUIRE(text::FaceAt(text::FaceCount()) == nullptr);
    REQUIRE(text::FaceAt(-1) == nullptr);
}

TEST_CASE("text_faces_cover_windows_1252", "[text]")
{
    // The fallback face draws every character; another face may lack a few
    // (the overlay then takes them from the fallback), but not letters.
    const std::vector<uint16_t> all = Cp1252Printable();
    text::Atlas body(text::FallbackFace(), 12.0f);
    REQUIRE(body.Ok());
    for (uint16_t cp : all) {
        INFO("U+" << std::hex << cp);
        REQUIRE((body.Has(cp) || body.Has(text::Substitute(cp))));
    }
    for (int face = 0; face < text::FaceCount(); face++) {
        text::Atlas atlas(face, 12.0f);
        REQUIRE(atlas.Ok());
        for (int c = 'A'; c <= 'Z'; c++) {
            INFO("face " << face << " " << static_cast<char>(c));
            REQUIRE(atlas.Has(static_cast<uint16_t>(c)));
            REQUIRE(atlas.Has(static_cast<uint16_t>(c + 32)));
        }
        const uint16_t accentedCodes[] = { 0xE9, 0xE8, 0xE0, 0xFC, 0xF1, 0xC9 };
        for (uint16_t accented : accentedCodes) {
            INFO("face " << face << " U+" << std::hex << accented);
            REQUIRE(atlas.Has(accented));
        }
    }
}

TEST_CASE("text_atlas_glyphs", "[text]")
{
    text::Atlas atlas(text::FallbackFace(), 24.0f);
    REQUIRE(atlas.Ok());
    SECTION("an H is the cap height tall and sits on the baseline")
    {
        const text::GlyphBitmap& h = atlas.Glyph('H', 1.0f);
        REQUIRE_FALSE(h.missing);
        REQUIRE(std::abs(InkRows(h) - 24) <= 1);
        // The bitmap's bottom (minus the outline's pixel) is the baseline.
        REQUIRE(std::abs(h.y0 + h.h - 1) <= 1);
        REQUIRE(h.advance > 10.0f);
    }
    SECTION("the outline covers the glyph and one pixel more")
    {
        const text::GlyphBitmap& a = atlas.Glyph('a', 1.0f);
        REQUIRE(a.outline.size() == a.coverage.size());
        int grown = 0;
        for (size_t i = 0; i < a.coverage.size(); i++) {
            REQUIRE(a.outline[i] >= a.coverage[i]);
            if (a.outline[i] > 0 && a.coverage[i] == 0)
                grown++;
        }
        REQUIRE(grown > 0);
        // The border row and column are coverage-free.
        for (int x = 0; x < a.w; x++)
            REQUIRE(a.coverage[static_cast<size_t>(x)] == 0);
    }
    SECTION("a space has an advance and no pixels")
    {
        const text::GlyphBitmap& sp = atlas.Glyph(' ', 1.0f);
        REQUIRE(sp.w == 0);
        REQUIRE(sp.advance > 0.0f);
    }
    SECTION("squeezing narrows the glyph")
    {
        const text::GlyphBitmap& wide = atlas.Glyph('W', 1.0f);
        const float w = wide.advance;
        const int ww = wide.w;
        const text::GlyphBitmap& narrow = atlas.Glyph('W', 0.5f);
        REQUIRE(narrow.advance < w * 0.6f);
        REQUIRE(narrow.w < ww);
        REQUIRE(atlas.Cached() == 2);
    }
    SECTION("accented letters from Windows-1252 are glyphs, not boxes")
    {
        const text::GlyphBitmap& e = atlas.Glyph(text::DecodeCp1252(0xE9), 1.0f);
        const text::GlyphBitmap& plain = atlas.Glyph('e', 1.0f);
        REQUIRE_FALSE(e.missing);
        REQUIRE(e.h > plain.h); // the accent above
        REQUIRE_FALSE(atlas.Glyph(text::DecodeCp1252(0x92), 1.0f).missing);
    }
}

TEST_CASE("text_atlases_rebuild_on_size_change", "[text]")
{
    text::Atlases atlases;
    text::Atlas* small = atlases.Get(text::FallbackFace(), 12.0f);
    REQUIRE(small != nullptr);
    REQUIRE(atlases.Get(text::FallbackFace(), 12.1f) == small); // the same quarter pixel
    const int smallRows = InkRows(small->Glyph('H', 1.0f));
    atlases.Trim(1);
    REQUIRE(atlases.Count() == 1);
    // A new scale: the next frame asks for another size only.
    text::Atlas* large = atlases.Get(text::FallbackFace(), 24.0f);
    REQUIRE(large != nullptr);
    REQUIRE(large != small);
    REQUIRE(InkRows(large->Glyph('H', 1.0f)) > smallRows + 10);
    atlases.Trim(2);
    REQUIRE(atlases.Count() == 1);
    REQUIRE(atlases.Get(text::FallbackFace(), 24.0f) == large);
    REQUIRE(atlases.Get(-1, 12.0f) == nullptr);
}
