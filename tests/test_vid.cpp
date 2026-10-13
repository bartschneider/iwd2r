// The 32-bpp pixel functions of the video layer on plain buffers.
//
// Fixture: CVidPoly::FillConvexPoly picks its span function from
// g_pChitin->cVideo.m_nBpp (SetHLineFunction), so a bare CChitin subclass is
// installed as g_pChitin with m_nBpp 32 for each test.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "CResCell.h"
#include "CVidCell.h"
#include "CVidInf.h"
#include "CVidPoly.h"
#include "CWarp.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
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

// The uncompressed paths of the 32-bpp cell blends walk outside the frame
// as the original does (rows four times apart in sub_7D0950, an extra row
// and column in sub_7D1190). Ours writes only inside the range it is given
// (ruling P4-10): a raw frame at the bottom right of an 8x8 surface leaves
// the guard pixels after it untouched, and sub_7D1190 reads no frame data
// past the frame.
TEST_CASE("cell_blend_raw_frames_stay_in_the_surface", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    // A 4x4 raw frame of palette index 9 (bright white), transparent 255.
    std::vector<BYTE> bam(0x40, 0);
    bam[0xB] = 255;
    std::memset(bam.data() + 0x20, 9, 16);
    std::vector<RGBQUAD> palette(256);
    palette[9] = RGBQUAD { 255, 255, 255, 0 };
    FRAMEENTRY frame = {};
    frame.nWidth = 4;
    frame.nHeight = 4;
    frame.nOffset = 0x20 | 0x80000000;

    auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
    std::memset(resStorage, 0, sizeof(CResCell));
    auto* res = reinterpret_cast<CResCell*>(resStorage);
    res->m_bParsed = TRUE;
    res->m_pBamHeader = reinterpret_cast<BAMHEADER*>(bam.data());
    res->m_pPalette = palette.data();
    auto* cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
    std::memset(cellStorage, 0, sizeof(CVidCell));
    auto* cell = reinterpret_cast<CVidCell*>(cellStorage);
    static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
    cell->m_pFrame = &frame;
    cell->m_bShadowOn = TRUE;

    for (int translucent = 0; translucent < 2; ++translucent) {
        // An 8x8 surface followed by guard pixels.
        std::vector<DWORD> pixels(8 * 8 + 4096, 0x00101010);
        const BYTE* begin = reinterpret_cast<const BYTE*>(pixels.data());
        const CVidCell::SurfaceRange range = { begin, begin + 8 * 8 * 4 };
        DWORD* at = pixels.data() + 4 * 8 + 4;
        if (translucent != 0) {
            CHECK(cell->sub_7D0950(at, 8 * 4, 0, 128, range));
        } else {
            CHECK(cell->sub_7D1190(at, 8 * 4, 0, range));
        }
        for (size_t i = 8 * 8; i < pixels.size(); ++i) {
            INFO("guard pixel " << i << ", translucent " << translucent);
            REQUIRE(pixels[i] == 0x00101010);
        }
        // The frame's first pixel is drawn.
        CHECK(pixels[4 * 8 + 4] != 0x00101010);
    }

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(cellStorage);
    ::operator delete(resStorage);
    ::operator delete(static_cast<void*>(engine));
}

// The same through CVidCell::SurfaceExtent: the current CVidInf's locked
// description (an 8x10 rectangle at (3, 2) of a 16-pixel-pitch surface of
// 12 rows, as BKLock locks the viewport) gives the bytes from its first
// pixel to the end of its last row. A raw 4x4 frame drawn at the
// rectangle's (4, 2) by sub_7D1190 walks 5 rows of 5 pixels, 17 pixels
// apart, all inside those bytes. Its first 16 pixels use the frame's data
// and are drawn; the last 9 would use the bytes after the frame data (the
// original reads them) and must stay as they were. In the second pass those
// bytes are index 9, white, so without sub_7D1190's frame-data bound they
// would be drawn and the test fails (checked by removing the bound). In the
// first pass the frame data ends its buffer, which only a sanitizer would
// notice being read past. Every other surface pixel stays as it was.
TEST_CASE("cell_blend_raw_frames_through_the_locked_surface", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->m_dwRBitShift = 16;
    mode->m_dwGBitShift = 8;
    mode->m_dwBBitShift = 0;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;

    constexpr int kPitch = 16;
    std::vector<DWORD> surface(kPitch * 12, 0x00101010);
    DWORD* locked = surface.data() + 2 * kPitch + 3;
    mode->m_SurfaceDesc.lpSurface = locked;
    mode->m_SurfaceDesc.lPitch = kPitch * 4;
    mode->m_SurfaceDesc.dwWidth = 8;
    mode->m_SurfaceDesc.dwHeight = 10;

    const CVidCell::SurfaceRange range = CVidCell::SurfaceExtent(locked, kPitch * 4);
    const BYTE* first = reinterpret_cast<const BYTE*>(locked);
    REQUIRE(range.pBegin == first);
    REQUIRE(range.pEnd == first + 9 * kPitch * 4 + 8 * 4);
    // Another pointer is not the locked surface.
    CHECK(CVidCell::SurfaceExtent(surface.data(), kPitch * 4).pBegin == nullptr);

    std::vector<RGBQUAD> palette(256);
    palette[9] = RGBQUAD { 255, 255, 255, 0 };
    palette[3] = RGBQUAD { 200, 200, 200, 0 };
    FRAMEENTRY frame = {};
    frame.nWidth = 4;
    frame.nHeight = 4;
    frame.nOffset = 0x20 | 0x80000000;

    for (int sentinel = 0; sentinel < 2; ++sentinel) {
        // Without a sentinel the frame data ends the buffer; with one, 16
        // bytes of index 9 follow it.
        std::vector<BYTE> bam(0x20 + 16 + (sentinel != 0 ? 16 : 0), 9);
        std::memset(bam.data(), 0, 0x20);
        bam[0xB] = 255;
        std::memset(bam.data() + 0x20, 3, 16);

        auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
        std::memset(resStorage, 0, sizeof(CResCell));
        auto* res = reinterpret_cast<CResCell*>(resStorage);
        res->m_bParsed = TRUE;
        res->m_pBamHeader = reinterpret_cast<BAMHEADER*>(bam.data());
        res->m_pPalette = palette.data();
        auto* cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
        std::memset(cellStorage, 0, sizeof(CVidCell));
        auto* cell = reinterpret_cast<CVidCell*>(cellStorage);
        static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
        cell->m_pFrame = &frame;
        cell->m_bShadowOn = TRUE;

        std::fill(surface.begin(), surface.end(), 0x00101010);
        DWORD* at = locked + 2 * kPitch + 4;
        CHECK(cell->sub_7D1190(at, kPitch * 4, 0, range));

        std::vector<bool> drawn(surface.size(), false);
        for (int pos = 0; pos < 25; ++pos) {
            DWORD* pixel = at + (pos / 5) * (kPitch + 1) + pos % 5;
            const BYTE* b = reinterpret_cast<const BYTE*>(pixel);
            const bool inside = b >= range.pBegin && b + 4 <= range.pEnd;
            // The whole walk is writable, so only the data bound keeps the
            // last 9 pixels as they were.
            REQUIRE(inside);
            if (pos < 16) {
                drawn[static_cast<size_t>(pixel - surface.data())] = true;
            }
        }
        for (size_t i = 0; i < surface.size(); ++i) {
            INFO("pixel " << i << ", sentinel " << sentinel);
            if (drawn[i]) {
                CHECK(surface[i] != 0x00101010);
            } else {
                CHECK(surface[i] == 0x00101010);
            }
        }

        ::operator delete(cellStorage);
        ::operator delete(resStorage);
    }

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(static_cast<void*>(engine));
}
