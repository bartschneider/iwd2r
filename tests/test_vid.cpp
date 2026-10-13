// The 32-bpp pixel functions of the video layer on plain buffers.
//
// Fixture: CVidPoly::FillConvexPoly picks its span function from
// g_pChitin->cVideo.m_nBpp (SetHLineFunction), so a bare CChitin subclass is
// installed as g_pChitin with m_nBpp 32 for each test.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CVidPoly.h"

#include <cstdint>
#include <utility>
#include <vector>

extern CChitin* g_pChitin;

namespace {

class TestChitin : public CChitin {
public:
    void MessageThreadMain(void*) override { }
    void RSThreadMain(void*) override { }
    void MainAIThread(void*) override { }
    void MusicThreadMain(void*) override { }
};

class ChitinScope {
public:
    ChitinScope()
        : m_previous(g_pChitin)
    {
        m_chitin.cVideo.m_nBpp = 32;
        g_pChitin = &m_chitin;
    }
    ~ChitinScope() { g_pChitin = m_previous; }

    ChitinScope(const ChitinScope&) = delete;
    ChitinScope& operator=(const ChitinScope&) = delete;

private:
    CChitin* m_previous;
    TestChitin m_chitin;
};

constexpr int kWidth = 24;
constexpr int kHeight = 16;
constexpr DWORD kColor = 0x00C0FFEE;

// Fills the polygon solid (flag 0x1, DrawHLine32) into a cleared 24x16
// buffer clipped to rClip and returns the buffer.
std::vector<DWORD> Fill(std::vector<CVIDPOLY_VERTEX> vertices, const CRect& rClip)
{
    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    CVidPoly poly;
    poly.SetPoly(vertices.data(), static_cast<WORD>(vertices.size()));
    // The surface pointer is the clip rectangle's top left, as
    // CVidInf::BKRenderConvexPoly locks it.
    DWORD* surface = pixels.data() + rClip.top * kWidth + rClip.left;
    REQUIRE(poly.FillConvexPoly(reinterpret_cast<WORD*>(surface), kWidth * 4, rClip, kColor, 0x1, CPoint(0, 0)));
    return pixels;
}

// The buffer with rows[y] = {first, last} filled (inclusive), others clear.
std::vector<DWORD> Expected(const std::vector<std::pair<int, std::pair<int, int>>>& rows)
{
    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    for (const auto& [y, span] : rows) {
        for (int x = span.first; x <= span.second; ++x) {
            pixels[y * kWidth + x] = kColor;
        }
    }
    return pixels;
}

const CRect kWhole(0, 0, kWidth, kHeight);

}

// A triangle whose right edge (10,0)-(13,4) steps fractionally: Dx 3, Dy 4,
// so its error term carries on three rows. The spans follow the original's
// edge walk (0x7C0F40): the error term starts at AdjUp - Dy and drops by Dy
// on a carry; both edges' Dy are measured from the top vertex.
TEST_CASE("fill_convex_poly_triangle", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 10, 0 }, { 13, 4 }, { 6, 4 } }, kWhole);
    CHECK(pixels == Expected({ { 0, { 10, 10 } }, { 1, { 9, 11 } }, { 2, { 8, 12 } }, { 3, { 7, 12 } }, { 4, { 6, 13 } } }));
}

// A taller triangle with a long fractional left edge, (12,1)-(3,13): Dx 9,
// Dy 12.
TEST_CASE("fill_convex_poly_long_edge", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 12, 1 }, { 20, 13 }, { 3, 13 } }, kWhole);
    // Left: x = 12 - (9 * dy + 12 - 3 - 1) / 12 walking down, i.e. the
    // original's run 0, AdjUp 9, ErrTerm -3, carry when the term turns
    // positive. Right: Dx 8, Dy 12, run 0, AdjUp 8, ErrTerm -4.
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    int left = 12;
    int leftErr = 9 - 12;
    int right = 12;
    int rightErr = 8 - 12;
    for (int y = 1; y <= 12; ++y) {
        rows.push_back({ y, { left, right } });
        leftErr += 9;
        if (leftErr > 0) {
            --left;
            leftErr -= 12;
        }
        rightErr += 8;
        if (rightErr > 0) {
            ++right;
            rightErr -= 12;
        }
    }
    rows.push_back({ 13, { 3, 20 } });
    // The hand walk ends at the bottom vertices, as an edge walk must.
    REQUIRE(left == 3);
    REQUIRE(right == 20);
    CHECK(pixels == Expected(rows));
}

// A rectangle fills its rows and columns inclusive.
TEST_CASE("fill_convex_poly_rectangle", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 2, 3 }, { 8, 3 }, { 8, 7 }, { 2, 7 } }, kWhole);
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    for (int y = 3; y <= 7; ++y) {
        rows.push_back({ y, { 2, 8 } });
    }
    CHECK(pixels == Expected(rows));
}

// The same rectangle clipped to (4,4)-(7,6): the clip's last row and column
// are drawn (rClip.right - 1 and rClip.bottom - 1 inclusive).
TEST_CASE("fill_convex_poly_rectangle_clipped", "[vid]")
{
    ChitinScope scope;
    const auto pixels = Fill({ { 2, 3 }, { 8, 3 }, { 8, 7 }, { 2, 7 } }, CRect(4, 4, 7, 6));
    CHECK(pixels == Expected({ { 4, { 4, 6 } }, { 5, { 4, 6 } } }));
}

// DrawHLineShaded32 blends a pixel toward the colour by the colour's
// intensity when the pixel is not dark (sum > 31) and its sum / 8 is at most
// that intensity; the top byte ends up clear.
TEST_CASE("draw_hline_shaded32_blends", "[vid]")
{
    CVidPoly poly;
    // Colour (r 90, g 60, b 30): intensity 60, inverse 195.
    const DWORD color = 0x005A3C1E;
    DWORD row[6] = {
        0xFF102030, // sum 96, 96 / 8 = 12 <= 60: blended
        0x00050505, // sum 15: too dark, kept
        0x00FFFFFF, // sum 765, 95 > 60: kept
        0x00808080, // sum 384, 48 <= 60: blended
        0x00010203, // outside the span
        0x00102030, // outside the span
    };
    poly.DrawHLineShaded32(row, 0, 3, color, CRect(0, 0, 6, 1), CPoint(0, 0));
    auto blend = [](DWORD c, DWORD p) { return (c * 60 + 195 * p) >> 8; };
    CHECK(row[0] == ((blend(90, 0x10) << 16) | (blend(60, 0x20) << 8) | blend(30, 0x30)));
    CHECK(row[1] == 0x00050505);
    CHECK(row[2] == 0x00FFFFFF);
    CHECK(row[3] == ((blend(90, 0x80) << 16) | (blend(60, 0x80) << 8) | blend(30, 0x80)));
    CHECK(row[4] == 0x00010203);
    CHECK(row[5] == 0x00102030);

    // Mirrored: xMin 0 is the pixel at rSurface's width, walking left.
    DWORD mirrored[6] = { 0, 0, 0, 0, 0x00808080, 0x00808080 };
    poly.DrawHLineShadedMirrored32(mirrored, 0, 0, color, CRect(0, 0, 5, 1), CPoint(0, 0));
    CHECK(mirrored[4] == 0x00808080);
    CHECK(mirrored[5] == ((blend(90, 0x80) << 16) | (blend(60, 0x80) << 8) | blend(30, 0x80)));
}
