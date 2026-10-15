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

namespace {

// Fills the polygon with CVidPoly::FillPoly (0x7C13A0) into a cleared 24x16
// buffer clipped to rClip. FillPoly caches edge tables by the vertex
// list's address and count, so each test passes its own static array.
std::vector<DWORD> FillNonConvex(CVIDPOLY_VERTEX* vertices, WORD count, const CRect& rClip, DWORD dwFlags, const CPoint& ptRef = CPoint(0, 0))
{
    std::vector<DWORD> pixels(kWidth * kHeight, 0);
    CVidPoly poly;
    poly.SetPoly(vertices, count);
    DWORD* surface = pixels.data() + rClip.top * kWidth + rClip.left;
    REQUIRE(poly.FillPoly(reinterpret_cast<WORD*>(surface), kWidth * 4, rClip, kColor, dwFlags, ptRef));
    return pixels;
}

// The notched square (2,2) (6,6) (10,2) (10,10) (2,10), by hand from the
// original's edge walk: the edge table holds four edges sorted by top y
// then x (the bottom edge is horizontal and skipped); once the table is
// empty an edge ends when its count reaches -1 (0x7C1911), so the bottom
// row 10 is drawn; spans run between pairs of active edges, inclusive.
std::vector<std::pair<int, std::pair<int, int>>> NotchRows()
{
    return {
        { 3, { 2, 3 } }, { 3, { 9, 10 } },
        { 4, { 2, 4 } }, { 4, { 8, 10 } },
        { 5, { 2, 5 } }, { 5, { 7, 10 } },
        { 6, { 2, 10 } }, { 7, { 2, 10 } }, { 8, { 2, 10 } }, { 9, { 2, 10 } }, { 10, { 2, 10 } },
        { 2, { 2, 2 } }, { 2, { 10, 10 } },
    };
}

}

TEST_CASE("fill_poly_notched_square", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    CHECK(FillNonConvex(notch, 5, kWhole, 0x1) == Expected(NotchRows()));
    // The second fill takes the edge table from the cache (0x7C1DE0) and
    // draws the same pixels.
    CHECK(FillNonConvex(notch, 5, kWhole, 0x1) == Expected(NotchRows()));
}

// The notch clipped to (4,3)-(9,8): rows 3..7, columns 4..8 (the clip's last
// row and column are drawn, as in FillConvexPoly).
TEST_CASE("fill_poly_notched_square_clipped", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    for (const auto& [y, span] : NotchRows()) {
        if (y < 3 || y > 7) {
            continue;
        }
        const int first = std::max(span.first, 4);
        const int last = std::min(span.second, 8);
        if (first <= last) {
            rows.push_back({ y, { first, last } });
        }
    }
    CHECK(FillNonConvex(notch, 5, CRect(4, 3, 9, 8), 0x1) == Expected(rows));
}

// Flag 0x8 flips the rows inside the clip: row y lands on clip.bottom - 1 -
// (y - clip.top).
TEST_CASE("fill_poly_flipped", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    std::vector<std::pair<int, std::pair<int, int>>> rows;
    for (const auto& [y, span] : NotchRows()) {
        rows.push_back({ kHeight - 1 - y, span });
    }
    CHECK(FillNonConvex(notch, 5, kWhole, 0x1 | 0x8) == Expected(rows));
}

// Dithered (flag 0x2, which also skips the cache): DrawHLineDithered32
// fills every other pixel, starting where (x + ref.x + y + ref.y) is odd.
TEST_CASE("fill_poly_dithered", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX notch[] = { { 2, 2 }, { 6, 6 }, { 10, 2 }, { 10, 10 }, { 2, 10 } };
    std::vector<DWORD> expected(kWidth * kHeight, 0);
    for (const auto& [y, span] : NotchRows()) {
        for (int x = span.first; x <= span.second; ++x) {
            if (((x + y) & 1) != 0) {
                expected[y * kWidth + x] = kColor;
            }
        }
    }
    CHECK(FillNonConvex(notch, 5, kWhole, 0x2) == expected);
}

// A polygon whose edges are all horizontal has an empty edge table; the
// original reads through NULL there (bounded: nothing is drawn).
TEST_CASE("fill_poly_flat", "[vid]")
{
    ChitinScope scope;
    static CVIDPOLY_VERTEX flat[] = { { 2, 4 }, { 8, 4 }, { 12, 4 } };
    CHECK(FillNonConvex(flat, 3, kWhole, 0x1) == std::vector<DWORD>(kWidth * kHeight, 0));
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

// The 16- and 24-bit paths. iwd2r runs at 32 bpp only (the DirectDraw shim
// lists no other mode), so the game never reaches these; they are ported
// for the record and checked here by hand and by oracle cases against the
// original (vid/cell_blt8to16, vid/cell_blt8to24, vid/blt_tile8to16,
// vid/draw_line16, vid/draw_ellipse24).
namespace {

// A 4x3 frame, transparent index 0:
//   5 0 6 7
//   0 0 8 0
//   9 9 0 1
const BYTE kFrame[12] = { 5, 0, 6, 7, 0, 0, 8, 0, 9, 9, 0, 1 };
// The same run-length encoded: a transparent pixel and its run - 1.
const BYTE kFrameRle[] = { 5, 0, 0, 6, 7, 0, 1, 8, 0, 0, 9, 9, 0, 0, 1 };

struct CellFixture {
    explicit CellFixture(bool compressed)
    {
        bam.assign(0x40, 0);
        bam[0xB] = 0;
        if (compressed) {
            std::memcpy(bam.data() + 0x20, kFrameRle, sizeof(kFrameRle));
        } else {
            std::memcpy(bam.data() + 0x20, kFrame, sizeof(kFrame));
        }
        frame.nWidth = 4;
        frame.nHeight = 3;
        frame.nOffset = 0x20 | (compressed ? 0u : 0x80000000u);
        resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResCell)));
        std::memset(resStorage, 0, sizeof(CResCell));
        auto* res = reinterpret_cast<CResCell*>(resStorage);
        res->m_bParsed = TRUE;
        res->m_pBamHeader = reinterpret_cast<BAMHEADER*>(bam.data());
        cellStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidCell)));
        std::memset(cellStorage, 0, sizeof(CVidCell));
        cell = reinterpret_cast<CVidCell*>(cellStorage);
        static_cast<CResHelper<CResCell, 1000>*>(cell)->pRes = res;
        cell->m_pFrame = &frame;
        for (int i = 0; i < 256; ++i) {
            CVidImage::rgbTempPal[i] = 0xEE000000u | static_cast<DWORD>(i) * 0x010203u;
        }
    }
    ~CellFixture()
    {
        ::operator delete(cellStorage);
        ::operator delete(resStorage);
    }
    CellFixture(const CellFixture&) = delete;
    CellFixture& operator=(const CellFixture&) = delete;

    std::vector<BYTE> bam;
    FRAMEENTRY frame = {};
    unsigned char* resStorage;
    unsigned char* cellStorage;
    CVidCell* cell;
};

constexpr int kDepthW = 8;
constexpr int kDepthH = 5;

// The pixels { x, y, palette index } a blit of kFrame from source (1, 0) to
// (2, 1) draws: source columns 1..3 land on x 2..4.
std::vector<std::pair<std::pair<int, int>, int>> FrameAt21()
{
    return { { { 3, 1 }, 6 }, { { 4, 1 }, 7 }, { { 3, 2 }, 8 }, { { 2, 3 }, 9 }, { { 4, 3 }, 1 } };
}

}

TEST_CASE("blt8to16_point_and_clip", "[vid]")
{
    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);

        std::vector<WORD> pixels(kDepthW * kDepthH, 0xAAAA);
        REQUIRE(fixture.cell->Blt8To16(pixels.data(), kDepthW * 2, CPoint(2, 1), CRect(0, 0, kDepthW, kDepthH), 0,
            CPoint(1, 0)));
        std::vector<WORD> expected(kDepthW * kDepthH, 0xAAAA);
        for (const auto& [at, index] : FrameAt21()) {
            expected[at.second * kDepthW + at.first] = static_cast<WORD>(CVidImage::rgbTempPal[index]);
        }
        CHECK(pixels == expected);

        // Clipped to (3,0)-(8,3): rows 1 and 2, columns 3 and 4.
        std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0xAAAA));
        REQUIRE(fixture.cell->Blt8To16(pixels.data(), kDepthW * 2, CPoint(2, 1), CRect(3, 0, kDepthW, 3), 0,
            CPoint(1, 0)));
        std::fill(expected.begin(), expected.end(), static_cast<WORD>(0xAAAA));
        expected[1 * kDepthW + 3] = static_cast<WORD>(CVidImage::rgbTempPal[6]);
        expected[1 * kDepthW + 4] = static_cast<WORD>(CVidImage::rgbTempPal[7]);
        expected[2 * kDepthW + 3] = static_cast<WORD>(CVidImage::rgbTempPal[8]);
        CHECK(pixels == expected);
    }
}

TEST_CASE("blt8to24_point", "[vid]")
{
    for (int compressed = 0; compressed < 2; ++compressed) {
        CellFixture fixture(compressed != 0);
        INFO("compressed " << compressed);

        std::vector<BYTE> pixels(kDepthW * kDepthH * 3, 0xAA);
        REQUIRE(fixture.cell->Blt8To24(pixels.data(), kDepthW * 3, CPoint(2, 1), CRect(0, 0, kDepthW, kDepthH), 0,
            CPoint(1, 0)));
        std::vector<BYTE> expected(kDepthW * kDepthH * 3, 0xAA);
        for (const auto& [at, index] : FrameAt21()) {
            const DWORD rgb = CVidImage::rgbTempPal[index];
            BYTE* p = expected.data() + (at.second * kDepthW + at.first) * 3;
            p[0] = static_cast<BYTE>(rgb);
            p[1] = static_cast<BYTE>(rgb >> 8);
            p[2] = static_cast<BYTE>(rgb >> 16);
        }
        CHECK(pixels == expected);
    }
}

// BltTile8To16 writes pixel pairs as one DWORD. Dithered (0x8), even rows
// keep the first pixel of each pair and put field_24's low word in the
// second; odd rows put field_24 in the first and OR its high word into the
// second (0x7B23A1).
TEST_CASE("blt_tile8to16", "[vid]")
{
    ChitinScope scope;
    CWarp* engine = reinterpret_cast<CWarp*>(::operator new(sizeof(CWarp)));
    std::memset(static_cast<void*>(engine), 0, sizeof(CWarp));
    auto mode = std::make_unique<CVidInf>();
    mode->field_24 = 0x12345678;
    engine->pVidMode = mode.get();
    g_pChitin->pActiveEngine = engine;
    for (int i = 0; i < 256; ++i) {
        CVidImage::rgbTempPal[i] = 0x00010000u * static_cast<DWORD>(i) + 0x0100u * static_cast<DWORD>(i);
    }
    auto* tileStorage = static_cast<unsigned char*>(::operator new(sizeof(CVidTile)));
    std::memset(tileStorage, 0, sizeof(CVidTile));
    auto* tile = reinterpret_cast<CVidTile*>(tileStorage);

    // A 4x2 tile from a 6-wide data block (data jump 2) into an 8x2 surface.
    BYTE data[12] = { 1, 2, 3, 4, 99, 99, 5, 6, 7, 8, 99, 99 };
    std::vector<WORD> pixels(8 * 2, 0xAAAA);
    REQUIRE(tile->BltTile8To16(pixels.data(), 8 * 2, data, CSize(4, 2), 2, 0));
    CHECK(pixels == std::vector<WORD> { 0x0100, 0x0200, 0x0300, 0x0400, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
                        0x0500, 0x0600, 0x0700, 0x0800, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA });

    std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0xAAAA));
    REQUIRE(tile->BltTile8To16(pixels.data(), 8 * 2, data, CSize(4, 2), 2, 0x8));
    CHECK(pixels == std::vector<WORD> { 0x0100, 0x5678, 0x0300, 0x5678, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
                        0x5678, 0x0600 | 0x1234, 0x5678, 0x0800 | 0x1234, 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA });

    g_pChitin->pActiveEngine = nullptr;
    ::operator delete(tileStorage);
    ::operator delete(static_cast<void*>(engine));
}

// DrawLine16 from (0,0) to (5,2): a run-sliced line (ScanLine16, 0x79A860)
// with whole step 2: runs of two pixels on each row. Vertical and
// horizontal lines are drawn inclusive of both ends.
TEST_CASE("draw_line16", "[vid]")
{
    CVidInf mode;
    const CRect rSurface(0, 0, kDepthW, kDepthH);
    std::vector<WORD> pixels(kDepthW * kDepthH, 0);
    REQUIRE(mode.DrawLine16(5, 2, 0, 0, pixels.data(), kDepthW, rSurface, 0x7777, TRUE));
    std::vector<WORD> expected(kDepthW * kDepthH, 0);
    for (const auto& [x, y] : std::vector<std::pair<int, int>> { { 0, 0 }, { 1, 0 }, { 2, 1 }, { 3, 1 }, { 4, 2 }, { 5, 2 } }) {
        expected[y * kDepthW + x] = 0x7777;
    }
    CHECK(pixels == expected);

    std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0));
    REQUIRE(mode.DrawLine16(6, 0, 6, 4, pixels.data(), kDepthW, rSurface, 0x1111, TRUE));
    REQUIRE(mode.DrawLine16(1, 4, 3, 4, pixels.data(), kDepthW, rSurface, 0x2222, TRUE));
    std::fill(expected.begin(), expected.end(), static_cast<WORD>(0));
    for (int y = 0; y <= 4; ++y) {
        expected[y * kDepthW + 6] = 0x1111;
    }
    for (int x = 1; x <= 3; ++x) {
        expected[4 * kDepthW + x] = 0x2222;
    }
    CHECK(pixels == expected);
}

// DrawEllipse24 draws the outline through the four ends of its axes, the
// same with the whole surface as the clip as without one, symmetric about
// the centre.
TEST_CASE("draw_ellipse24", "[vid]")
{
    CVidInf mode;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;
    constexpr int kW = 24;
    constexpr int kH = 16;
    std::vector<BYTE> plain(kW * kH * 3, 0);
    std::vector<BYTE> clipped(kW * kH * 3, 0);
    DDSURFACEDESC ddsd = {};
    ddsd.lPitch = kW * 3;
    ddsd.lpSurface = plain.data();
    mode.DrawEllipse24(CPoint(11, 7), CSize(7, 4), ddsd, nullptr, RGB(0x10, 0x20, 0x30));
    ddsd.lpSurface = clipped.data();
    const CRect rWhole(0, 0, kW, kH);
    mode.DrawEllipse24(CPoint(11, 7), CSize(7, 4), ddsd, &rWhole, RGB(0x10, 0x20, 0x30));
    CHECK(plain == clipped);

    auto lit = [&](int x, int y) {
        const BYTE* p = plain.data() + (y * kW + x) * 3;
        return p[0] == 0x30 && p[1] == 0x20 && p[2] == 0x10;
    };
    CHECK(lit(4, 7));
    CHECK(lit(18, 7));
    CHECK(lit(11, 3));
    CHECK(lit(11, 11));
    CHECK_FALSE(lit(11, 7));
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const int mx = 22 - x;
            const int my = 14 - y;
            if (mx >= 0 && mx < kW && my >= 0 && my < kH) {
                INFO("pixel " << x << "," << y);
                CHECK(lit(x, y) == lit(mx, my));
            }
        }
    }
}

// CVidMode::OutlinePoly (0x799540) locks the rectangle of surface 0 and
// draws each edge and the closing one with DrawLine32, the points moved by
// -(ptOffset) relative to the surface (the lock's origin is rSurface's top
// left). Compared with DrawLine32 drawing the same edges into a plain
// buffer; the triangle's top edge and left edge are checked by hand.
TEST_CASE("outline_poly32", "[vid]")
{
    ChitinScope scope;
    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    DDSURFACEDESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
    desc.dwWidth = 16;
    desc.dwHeight = 12;
    IDirectDrawSurface* pSurface = NULL;
    REQUIRE(pDirectDraw->CreateSurface(&desc, &pSurface, NULL) == DD_OK);

    CVidInf mode;
    IDirectDrawSurface* surfaces[1] = { pSurface };
    mode.m_nSurfaces = 1;
    mode.pSurfaces = surfaces;
    mode.m_dwRBitShift = 16;
    mode.m_dwGBitShift = 8;
    mode.m_dwBBitShift = 0;

    auto read = [&]() {
        std::vector<DWORD> pixels(16 * 12);
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 16, 12)));
        for (int y = 0; y < 12; ++y) {
            std::memcpy(pixels.data() + y * 16, reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 16 * 4);
        }
        mode.UnLockSurface(0, ddsd.lpSurface);
        return pixels;
    };
    auto clear = [&]() {
        DDSURFACEDESC ddsd;
        ddsd.dwSize = sizeof(ddsd);
        REQUIRE(mode.LockSurface(0, &ddsd, CRect(0, 0, 16, 12)));
        for (int y = 0; y < 12; ++y) {
            std::memset(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch, 0, 16 * 4);
        }
        mode.UnLockSurface(0, ddsd.lpSurface);
    };

    const CPoint triangle[3] = { CPoint(3, 3), CPoint(11, 3), CPoint(3, 9) };
    const DWORD color = 0x00302010; // RGB(0x30, 0x20, 0x10) with red at bit 16
    for (int shifted = 0; shifted < 2; ++shifted) {
        INFO("shifted " << shifted);
        clear();
        // Shifted: the rectangle (2,1)-(16,12), whose top left the lock
        // returns, and offset (1,1) put the triangle 1 left and 1 up.
        const CRect rSurface = shifted != 0 ? CRect(2, 1, 16, 12) : CRect(0, 0, 16, 12);
        const CPoint ptOffset = shifted != 0 ? CPoint(1, 1) : CPoint(0, 0);
        REQUIRE(mode.OutlinePoly(triangle, 3, rSurface, RGB(0x30, 0x20, 0x10), ptOffset));
        const std::vector<DWORD> pixels = read();

        const int dx = shifted != 0 ? 1 : 0;
        const int dy = shifted != 0 ? 1 : 0;
        std::vector<DWORD> expected(16 * 12, 0);
        const CRect rWhole(0, 0, 16, 12);
        for (int i = 0; i < 3; ++i) {
            const CPoint& a = triangle[i];
            const CPoint& b = triangle[(i + 1) % 3];
            mode.DrawLine32(a.x - dx, a.y - dy, b.x - dx, b.y - dy, expected.data(), 16, rWhole, color, FALSE);
        }
        CHECK(pixels == expected);
        for (int x = 3; x <= 11; ++x) {
            CHECK(pixels[(3 - dy) * 16 + x - dx] == color);
        }
        for (int y = 3; y <= 9; ++y) {
            CHECK(pixels[(y - dy) * 16 + 3 - dx] == color);
        }
    }

    // No vertices: nothing is drawn.
    clear();
    CHECK_FALSE(mode.OutlinePoly(triangle, 0, CRect(0, 0, 16, 12), RGB(1, 2, 3), CPoint(0, 0)));
    CHECK(read() == std::vector<DWORD>(16 * 12, 0));

    mode.m_nSurfaces = 0;
    mode.pSurfaces = NULL;
    pSurface->Release();
    pDirectDraw->Release();
}

// CVidImage's constructor (0x7ACA60) builds the shared blend table on the
// first image: entry (k, m, i) at k * 0x800 + m * 0x20 + i is
// (m * (64 - 2i) + k * 2i) >> 6. The destructor (0x7ACC40) frees it with
// the last image.
TEST_CASE("vid_image_blend_table", "[vid]")
{
    const INT refsBefore = CVidImage::m_nBlendTableRefs;
    {
        CVidImage image;
        CHECK(CVidImage::m_nBlendTableRefs == refsBefore + 1);
        REQUIRE(CVidImage::m_pBlendTable != nullptr);
        for (int k : { 0, 1, 17, 63 }) {
            for (int m : { 0, 5, 40, 63 }) {
                for (int i : { 0, 1, 16, 31 }) {
                    INFO("k " << k << " m " << m << " i " << i);
                    CHECK(CVidImage::m_pBlendTable[k * 0x800 + m * 0x20 + i] == ((m * (64 - 2 * i) + k * 2 * i) >> 6));
                }
            }
        }
    }
    CHECK(CVidImage::m_nBlendTableRefs == refsBefore);
    if (refsBefore == 0) {
        CHECK(CVidImage::m_pBlendTable == nullptr);
    }
}
