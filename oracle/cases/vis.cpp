// The visibility map's line-of-sight marking and its sweeps, the original's
// against ours, over AR1000's search bitmap.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures (R10a, R10b, R10c). The search bitmap is the game's AR1000SR.BMP,
// extracted from the vanilla copy by oracle.py run (TABLES) and read here
// from --tables. On the original's side its CResBitmap comes from the
// original's constructor (0x77EB90) and is parsed by the original's
// CResBitmap::Parse (0x77F1C0) on a copy of the file in leaked original
// memory; ours is a CResBitmap from our constructor parsed by ours. The
// functions only read the CVisibilityMap and its CSearchBitmap, so both are
// zeroed blocks of the original's sizes holding only the members read:
// the map's m_pMap, m_nWidth, m_nHeight, m_nMapSize, m_bOutDoor,
// m_pSearchMap and the two ellipse arcs (field_4, field_E, field_10,
// field_12, field_16, field_1A, the values CVisibilityMap's constructor
// gives them through CVidMode::GetEllipseArcPixelList); the search map's
// m_resSearch (pRes, m_nBitCount 8 as CVidBitmap's constructor sets it,
// m_bDoubleSize 0 as CSearchBitmap::Init sets it), m_pDynamicCost and
// m_GridSquareDimensions. The visibility map is sized as
// CVisibilityMap::Init sizes it for the area (3200 x 2304 pixels: the search
// bitmap's 200 x 192 cells of 16 x 12). The tile bytes the functions write
// live in original memory on the original's side and in ours on ours, and
// are compared whole after every call. Original objects are never freed.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CResBitmap.h"
#include "CSearchBitmap.h"
#include "CVisibility.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CResBitmap::CResBitmap() (0x77EB90), plain ret; CResBitmap::Parse(void*)
// (0x77F1C0), `ret 4`.
using ResBitmapCtorFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kResBitmapCtor = 0x77EB90;
using ResBitmapParseFn = uint32_t(__fastcall*)(void* self, void* edx, void* data);
constexpr uintptr_t kResBitmapParse = 0x77F1C0;
// CRes (0x50) and nColors, nXWords, bParsed, pBitmapData and the three
// header pointers. (Our classes are not packed as the original's: our side
// sets its members by name.)
constexpr size_t kResBitmapSize = 0x6C;

// CVisibilityMap::MarkTileLine(ptView, ptSearchMapView, nDestX, nDestY,
// ptOffset, charId, pVisibleTerrainTable, bLookingUp) (0x552A50), `ret 0x20`.
using MarkTileLineFn = void(__fastcall*)(void* self, void* edx, const CPoint* ptView, const CPoint* ptSearchMapView,
    int32_t nDestX, int32_t nDestY, const CPoint* ptOffset, uint32_t charId, const BYTE* table, uint32_t bLookingUp);
constexpr uintptr_t kMarkTileLine = 0x552A50;
// sub_553800 and sub_553990 (ptView, ptOffset, ptSearchMapView, charId,
// pVisibleTerrainTable), `ret 0x14`.
using SweepFn = void(__fastcall*)(void* self, void* edx, const CPoint* ptView, const CPoint* ptOffset,
    const CPoint* ptSearchMapView, uint32_t charId, const BYTE* table);
constexpr uintptr_t kSweepVertical = 0x553800;
constexpr uintptr_t kSweepHorizontal = 0x553990;
// sub_553440 and sub_5535D0 (pt, charId), `ret 8`.
using ClearFn = void(__fastcall*)(void* self, void* edx, const CPoint* pt, uint32_t charId);
constexpr uintptr_t kClearVertical = 0x553440;
constexpr uintptr_t kClearHorizontal = 0x5535D0;

// CVisibilityMap members (CVisibility.h).
constexpr size_t kVisSize = 0x5C;
constexpr size_t kVisMap = 0x0;
constexpr size_t kVisArcLength = 0x4;
constexpr size_t kVisWidth = 0x6;
constexpr size_t kVisHeight = 0x8;
constexpr size_t kVisMapSize = 0xA;
constexpr size_t kVisRadiusX = 0xE;
constexpr size_t kVisRadiusY = 0x10;
constexpr size_t kVisArc = 0x12;
constexpr size_t kVisArcSide = 0x16;
constexpr size_t kVisArcSideLength = 0x1A;
constexpr size_t kVisOutDoor = 0x1B;
constexpr size_t kVisSearchMap = 0x3C;
// CSearchBitmap members (CSearchBitmap.h, CVidBitmap.h, CRes.h).
constexpr size_t kSearchSize = 0xF4;
constexpr size_t kSearchRes = 0xA4;
constexpr size_t kSearchBitCount = 0xB0;
constexpr size_t kSearchDoubleSize = 0xB6;
constexpr size_t kSearchDynamicCost = 0xBA;
constexpr size_t kSearchDimensions = 0xE6;

// CVisibilityMap's constructor: the ellipse radii 14 x 10 and the arcs
// CVidMode::GetEllipseArcPixelList(14, 10) and (10, 14) give.
constexpr int16_t kRadiusX = 14;
constexpr int16_t kRadiusY = 10;
const uint8_t kArc[kRadiusX] = { 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 1 };
constexpr uint8_t kArcLength = 12;
const uint8_t kArcSide[kRadiusY] = { 0, 0, 1, 0, 1, 1 };
constexpr uint8_t kArcSideLength = 6;

// CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE (0x84C514), which sprites and
// areas copy as their visible terrain table.
const BYTE kTerrainTable[16] = { 255, 5, 5, 5, 5, 5, 5, 5, 5, 5, 255, 5, 5, 255, 5, 5 };

// The party's start positions (STARTPOS.2DA), four near the map's corners
// and three a few cells below obstacles (search index 0, the only
// structure AR1000 has: no area's search bitmap uses the taller 10 and 13),
// so that the upward lines climb walls.
const CPoint kPositions[] = { { 2414, 897 }, { 2321, 926 }, { 2383, 931 }, { 2383, 867 }, { 2351, 894 },
    { 2330, 851 }, { 40, 30 }, { 3190, 2290 }, { 3180, 20 }, { 25, 2280 }, { 2344, 534 }, { 328, 450 },
    { 760, 618 } };

void* OriginalNew(size_t size)
{
    return reinterpret_cast<OriginalNewFn>(kOriginalNew)(size);
}

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(OriginalNew(size));
    std::memset(at, 0, size);
    return at;
}

template <class T>
void Put(uint8_t* at, size_t offset, T value)
{
    std::memcpy(at + offset, &value, sizeof(value));
}

uint32_t Address(const void* p)
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

std::vector<uint8_t> ReadGameFile(const CaseContext& ctx, const std::string& name)
{
    if (ctx.tables.empty()) {
        throw std::runtime_error("no --tables folder for " + name);
    }
    const std::string path = ctx.tables + "\\" + name;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + path + " (tools/oracle.py run extracts it)");
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Both sides' visibility map over AR1000's search bitmap.
struct VisPair {
    uint8_t* original = nullptr;
    uint8_t* originalTiles = nullptr;
    uint8_t* originalDynamic = nullptr;
    CVisibilityMap* ours = nullptr;
    uint8_t* oursDynamic = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t size = 0;
    int32_t cells = 0;
};

VisPair MakeVisPair(CaseContext& ctx)
{
    const std::vector<uint8_t> bmp = ReadGameFile(ctx, "AR1000SR.BMP");
    if (bmp.size() < 0x36) {
        throw std::runtime_error("AR1000SR.BMP is too short");
    }
    int32_t cx = 0;
    int32_t cy = 0;
    std::memcpy(&cx, bmp.data() + 18, 4);
    std::memcpy(&cy, bmp.data() + 22, 4);

    VisPair pair;
    // CVisibilityMap::Init: nAreaX / 32 + 1 by nAreaY / 32 + 1.
    pair.width = cx * 16 / 32 + 1;
    pair.height = cy * 12 / 32 + 1;
    pair.size = pair.width * pair.height;
    pair.cells = cx * cy;

    ctx.SetInputs("building the AR1000 fixture");
    ctx.CallOriginal([&] {
        uint8_t* data = OriginalBlock(bmp.size());
        std::memcpy(data, bmp.data(), bmp.size());
        uint8_t* res = OriginalBlock(kResBitmapSize);
        reinterpret_cast<ResBitmapCtorFn>(kResBitmapCtor)(res, nullptr);
        if (reinterpret_cast<ResBitmapParseFn>(kResBitmapParse)(res, nullptr, data) == 0) {
            throw std::runtime_error("the original's CResBitmap::Parse refused AR1000SR.BMP");
        }

        uint8_t* search = OriginalBlock(kSearchSize);
        pair.originalDynamic = OriginalBlock(static_cast<size_t>(pair.cells));
        Put<uint32_t>(search, kSearchRes, Address(res));
        Put<int16_t>(search, kSearchBitCount, 8);
        Put<int32_t>(search, kSearchDoubleSize, 0);
        Put<uint32_t>(search, kSearchDynamicCost, Address(pair.originalDynamic));
        Put<int32_t>(search, kSearchDimensions, cx);
        Put<int32_t>(search, kSearchDimensions + 4, cy);

        uint8_t* arc = OriginalBlock(kRadiusX);
        std::memcpy(arc, kArc, kRadiusX);
        uint8_t* arcSide = OriginalBlock(kRadiusY);
        std::memcpy(arcSide, kArcSide, kRadiusY);

        pair.original = OriginalBlock(kVisSize);
        pair.originalTiles = OriginalBlock(static_cast<size_t>(pair.size));
        Put<uint32_t>(pair.original, kVisMap, Address(pair.originalTiles));
        Put<uint8_t>(pair.original, kVisArcLength, kArcLength);
        Put<int16_t>(pair.original, kVisWidth, static_cast<int16_t>(pair.width));
        Put<int16_t>(pair.original, kVisHeight, static_cast<int16_t>(pair.height));
        Put<int32_t>(pair.original, kVisMapSize, pair.size);
        Put<int16_t>(pair.original, kVisRadiusX, kRadiusX);
        Put<int16_t>(pair.original, kVisRadiusY, kRadiusY);
        Put<uint32_t>(pair.original, kVisArc, Address(arc));
        Put<uint32_t>(pair.original, kVisArcSide, Address(arcSide));
        Put<uint8_t>(pair.original, kVisArcSideLength, kArcSideLength);
        Put<uint32_t>(pair.original, kVisSearchMap, Address(search));
    });

    // Ours: the same members in zeroed blocks of our types, leaked.
    auto* data = new uint8_t[bmp.size()];
    std::memcpy(data, bmp.data(), bmp.size());
    auto* res = new CResBitmap();
    if (!res->Parse(data)) {
        throw std::runtime_error("our CResBitmap::Parse refused AR1000SR.BMP");
    }

    auto* searchStorage = static_cast<uint8_t*>(::operator new(sizeof(CSearchBitmap)));
    std::memset(searchStorage, 0, sizeof(CSearchBitmap));
    auto* search = reinterpret_cast<CSearchBitmap*>(searchStorage);
    pair.oursDynamic = new uint8_t[static_cast<size_t>(pair.cells)]();
    search->m_resSearch.pRes = res;
    search->m_resSearch.m_nBitCount = 8;
    search->m_resSearch.m_bDoubleSize = FALSE;
    search->m_pDynamicCost = pair.oursDynamic;
    search->m_GridSquareDimensions.cx = cx;
    search->m_GridSquareDimensions.cy = cy;

    auto* arc = new uint8_t[kRadiusX];
    std::memcpy(arc, kArc, kRadiusX);
    auto* arcSide = new uint8_t[kRadiusY];
    std::memcpy(arcSide, kArcSide, kRadiusY);

    auto* visStorage = static_cast<uint8_t*>(::operator new(sizeof(CVisibilityMap)));
    std::memset(visStorage, 0, sizeof(CVisibilityMap));
    pair.ours = reinterpret_cast<CVisibilityMap*>(visStorage);
    pair.ours->m_pMap = new uint8_t[static_cast<size_t>(pair.size)]();
    pair.ours->field_4 = kArcLength;
    pair.ours->m_nWidth = static_cast<SHORT>(pair.width);
    pair.ours->m_nHeight = static_cast<SHORT>(pair.height);
    pair.ours->m_nMapSize = pair.size;
    pair.ours->field_E = kRadiusX;
    pair.ours->field_10 = kRadiusY;
    pair.ours->field_12 = arc;
    pair.ours->field_16 = arcSide;
    pair.ours->field_1A = kArcSideLength;
    pair.ours->m_pSearchMap = search;
    return pair;
}

void SetOutDoor(VisPair& pair, bool outDoor)
{
    pair.original[kVisOutDoor] = outDoor ? 1 : 0;
    pair.ours->m_bOutDoor = outDoor ? TRUE : FALSE;
}

void ClearTiles(VisPair& pair)
{
    std::memset(pair.originalTiles, 0, static_cast<size_t>(pair.size));
    std::memset(pair.ours->m_pMap, 0, static_cast<size_t>(pair.size));
}

// The dynamic cost bits CSearchBitmap::GetLOSCost reads (0x80 an opaque
// door, 0x01 an impassable one): none, or a block of each by the start
// positions.
void SetDynamicCost(VisPair& pair, bool doors)
{
    std::memset(pair.originalDynamic, 0, static_cast<size_t>(pair.cells));
    std::memset(pair.oursDynamic, 0, static_cast<size_t>(pair.cells));
    if (!doors) {
        return;
    }
    const int32_t cx = pair.ours->m_pSearchMap->m_GridSquareDimensions.cx;
    for (int32_t y = 70; y < 76; y++) {
        for (int32_t x = 140; x < 146; x++) {
            pair.originalDynamic[y * cx + x] = 0x80;
            pair.oursDynamic[y * cx + x] = 0x80;
        }
        for (int32_t x = 154; x < 157; x++) {
            pair.originalDynamic[y * cx + x] = 0x01;
            pair.oursDynamic[y * cx + x] = 0x01;
        }
    }
}

// The tile bytes of both sides: the count of differing bytes and the first
// few, or empty when they are equal.
std::string TileDifferences(const VisPair& pair, std::string* original, std::string* ours)
{
    int32_t count = 0;
    std::string where;
    for (int32_t i = 0; i < pair.size; i++) {
        if (pair.originalTiles[i] != pair.ours->m_pMap[i]) {
            if (count < 6) {
                char text[64];
                std::snprintf(text, sizeof(text), " (%d,%d)", i % pair.width, i / pair.width);
                where += text;
                char a[8];
                char b[8];
                std::snprintf(a, sizeof(a), " %02X", pair.originalTiles[i]);
                std::snprintf(b, sizeof(b), " %02X", pair.ours->m_pMap[i]);
                *original += a;
                *ours += b;
            }
            count++;
        }
    }
    if (count == 0) {
        return std::string();
    }
    return std::to_string(count) + " tiles differ at" + where;
}

void CompareTiles(CaseContext& ctx, const VisPair& pair)
{
    std::string original;
    std::string ours;
    const std::string differences = TileDifferences(pair, &original, &ours);
    if (!differences.empty()) {
        ctx.Fail(ctx.Inputs() + ": " + differences, original, ours);
    }
}

int32_t ExploredTiles(const uint8_t* tiles, int32_t size)
{
    int32_t count = 0;
    for (int32_t i = 0; i < size; i++) {
        if ((tiles[i] & 0x80) != 0) {
            count++;
        }
    }
    return count;
}

CPoint ViewOf(const CPoint& pos)
{
    return CPoint(pos.x / 32, pos.y / 32);
}

CPoint OffsetOf(const CPoint& pos)
{
    return CPoint(pos.x % 32, pos.y % 32);
}

CPoint SearchOf(const CPoint& pos)
{
    return CPoint(pos.x / 16, pos.y / 12);
}

std::string Describe(const char* what, const CPoint& pos, bool outDoor, bool doors)
{
    char text[128];
    std::snprintf(text, sizeof(text), "%s from (%d,%d)%s%s", what, static_cast<int>(pos.x),
        static_cast<int>(pos.y), outDoor ? " outdoor" : "", doors ? " doors" : "");
    return text;
}

// ---------------------------------------------------------------------------
// CVisibilityMap::MarkTileLine (0x552A50)

void MarkLine(CaseContext& ctx, VisPair& pair, const CPoint& view, const CPoint& search, int32_t destX,
    int32_t destY, const CPoint& offset, uint8_t charId, bool lookingUp)
{
    ctx.CallOriginal([&] {
        reinterpret_cast<MarkTileLineFn>(kMarkTileLine)(pair.original, nullptr, &view, &search, destX, destY, &offset,
            charId, kTerrainTable, lookingUp ? 1 : 0);
    });
    pair.ours->MarkTileLine(view, search, destX, destY, offset, charId, kTerrainTable, lookingUp ? TRUE : FALSE);
    ctx.CountInputs(1);
    CompareTiles(ctx, pair);
}

// From every position: lines to the eight directions at the vision radii,
// to points past each edge of the map (the clip), and of zero length (a
// search cell that is its own destination), looking up and not, indoors and
// out, with and without door bits. Each line starts on a cleared map; the
// lines of one position then run again on one map, as the sweeps do.
void MarkTileLineCase(CaseContext& ctx)
{
    VisPair pair = MakeVisPair(ctx);
    const int32_t dirs[8][2] = { { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 } };
    for (bool doors : { false, true }) {
        SetDynamicCost(pair, doors);
        for (bool outDoor : { false, true }) {
            SetOutDoor(pair, outDoor);
            for (const CPoint& pos : kPositions) {
                const CPoint view = ViewOf(pos);
                const CPoint offset = OffsetOf(pos);
                const CPoint search = SearchOf(pos);
                std::vector<std::pair<int32_t, int32_t>> dests;
                for (const auto& d : dirs) {
                    dests.emplace_back(view.x + d[0] * kRadiusX, view.y + d[1] * kRadiusY);
                    dests.emplace_back(view.x + d[0] * 3, view.y + d[1] * 2);
                }
                dests.emplace_back(-6, view.y + 4);
                dests.emplace_back(pair.width + 5, view.y - 3);
                dests.emplace_back(view.x + 2, -4);
                dests.emplace_back(view.x - 5, pair.height + 7);
                dests.emplace_back(-20, -20);
                dests.emplace_back(pair.width + 30, pair.height + 30);
                for (bool lookingUp : { true, false }) {
                    uint8_t charId = 1;
                    for (const auto& dest : dests) {
                        char text[96];
                        std::snprintf(text, sizeof(text), " to (%d,%d) charId %u%s", dest.first, dest.second,
                            charId, lookingUp ? " looking up" : "");
                        ctx.SetInputs(Describe("line", pos, outDoor, doors) + text);
                        ClearTiles(pair);
                        MarkLine(ctx, pair, view, search, dest.first, dest.second, offset, charId, lookingUp);
                        charId = static_cast<uint8_t>(charId == 0x20 ? 1 : charId << 1);
                    }
                    ClearTiles(pair);
                    for (const auto& dest : dests) {
                        ctx.SetInputs(Describe("all lines on one map", pos, outDoor, doors));
                        MarkLine(ctx, pair, view, search, dest.first, dest.second, offset, 4, lookingUp);
                    }
                }
                // Zero length: the start cell is the destination's.
                const CPoint cell(view.x * 2, CVisibilityMap::VIS_TO_SEARCH_Y[view.y % 3] + (view.y / 3) * 8);
                for (bool lookingUp : { true, false }) {
                    ctx.SetInputs(Describe("zero-length line", pos, outDoor, doors));
                    ClearTiles(pair);
                    MarkLine(ctx, pair, view, cell, view.x, view.y, offset, 2, lookingUp);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// sub_553800 and sub_553990 (PrivateAddCharacter's two sweeps), sub_553440
// and sub_5535D0 (RemoveCharacter's and UpDate's two clears)

void Sweep(CaseContext& ctx, VisPair& pair, uintptr_t address, bool vertical, const CPoint& pos, uint8_t charId)
{
    const CPoint view = ViewOf(pos);
    const CPoint offset = OffsetOf(pos);
    const CPoint search = SearchOf(pos);
    ctx.CallOriginal([&] {
        reinterpret_cast<SweepFn>(address)(pair.original, nullptr, &view, &offset, &search, charId, kTerrainTable);
    });
    if (vertical) {
        pair.ours->sub_553800(view, offset, search, charId, kTerrainTable);
    } else {
        pair.ours->sub_553990(view, offset, search, charId, kTerrainTable);
    }
    ctx.CountInputs(1);
    CompareTiles(ctx, pair);
}

void Clear(CaseContext& ctx, VisPair& pair, uintptr_t address, bool vertical, const CPoint& pos, uint8_t charId)
{
    const CPoint view = ViewOf(pos);
    ctx.CallOriginal([&] { reinterpret_cast<ClearFn>(address)(pair.original, nullptr, &view, charId); });
    if (vertical) {
        pair.ours->sub_553440(view, charId);
    } else {
        pair.ours->sub_5535D0(view, charId);
    }
    ctx.CountInputs(1);
    CompareTiles(ctx, pair);
}

// One sweep per position on a cleared map, then all six party members'
// both sweeps on one map (as six AddCharacter calls leave it).
void SweepCase(CaseContext& ctx, uintptr_t address, bool vertical)
{
    VisPair pair = MakeVisPair(ctx);
    for (bool doors : { false, true }) {
        SetDynamicCost(pair, doors);
        for (bool outDoor : { false, true }) {
            SetOutDoor(pair, outDoor);
            for (const CPoint& pos : kPositions) {
                ctx.SetInputs(Describe(vertical ? "sub_553800" : "sub_553990", pos, outDoor, doors));
                ClearTiles(pair);
                Sweep(ctx, pair, address, vertical, pos, 1);
            }
            ClearTiles(pair);
            for (int i = 0; i < 6; i++) {
                ctx.SetInputs(Describe("party sweep", kPositions[i], outDoor, doors));
                Sweep(ctx, pair, address, vertical, kPositions[i], static_cast<uint8_t>(1 << i));
            }
        }
    }
}

void SweepVerticalCase(CaseContext& ctx)
{
    SweepCase(ctx, kSweepVertical, true);
}

void SweepHorizontalCase(CaseContext& ctx)
{
    SweepCase(ctx, kSweepHorizontal, false);
}

// The six party members' sweeps (both sides' own, already compared by the
// sweep cases) and then one member's clear at its tile and at a tile it
// moved to, and on an all-visible map at every position.
void ClearCase(CaseContext& ctx, uintptr_t address, bool vertical)
{
    VisPair pair = MakeVisPair(ctx);
    SetOutDoor(pair, false);
    SetDynamicCost(pair, false);
    for (int member = 0; member < 6; member++) {
        ClearTiles(pair);
        for (int i = 0; i < 6; i++) {
            const CPoint& pos = kPositions[i];
            const uint8_t charId = static_cast<uint8_t>(1 << i);
            ctx.SetInputs(Describe("party sweeps", pos, false, false));
            Sweep(ctx, pair, kSweepVertical, true, pos, charId);
            Sweep(ctx, pair, kSweepHorizontal, false, pos, charId);
        }
        const CPoint moved(kPositions[member].x + 70, kPositions[member].y - 40);
        ctx.SetInputs(Describe("clear at a moved tile", moved, false, false));
        Clear(ctx, pair, address, vertical, moved, static_cast<uint8_t>(1 << member));
        ctx.SetInputs(Describe("clear", kPositions[member], false, false));
        Clear(ctx, pair, address, vertical, kPositions[member], static_cast<uint8_t>(1 << member));
    }
    for (const CPoint& pos : kPositions) {
        std::memset(pair.originalTiles, 0xBF, static_cast<size_t>(pair.size));
        std::memset(pair.ours->m_pMap, 0xBF, static_cast<size_t>(pair.size));
        ctx.SetInputs(Describe("clear on an all-visible map", pos, false, false));
        Clear(ctx, pair, address, vertical, pos, 0x10);
    }
}

void ClearVerticalCase(CaseContext& ctx)
{
    ClearCase(ctx, kClearVertical, true);
}

void ClearHorizontalCase(CaseContext& ctx)
{
    ClearCase(ctx, kClearHorizontal, false);
}

// The explored-tile count both sides' sweeps leave for the start party, the
// value the replay's world.explored_count reads: compared, and printed for
// the report.
void PartyExploredCase(CaseContext& ctx)
{
    VisPair pair = MakeVisPair(ctx);
    SetOutDoor(pair, false);
    SetDynamicCost(pair, false);
    ClearTiles(pair);
    for (int i = 0; i < 6; i++) {
        ctx.SetInputs(Describe("party sweeps", kPositions[i], false, false));
        Sweep(ctx, pair, kSweepVertical, true, kPositions[i], static_cast<uint8_t>(1 << i));
        Sweep(ctx, pair, kSweepHorizontal, false, kPositions[i], static_cast<uint8_t>(1 << i));
    }
    ctx.SetInputs("explored tiles after the start party's sweeps");
    const int32_t original = ExploredTiles(pair.originalTiles, pair.size);
    const int32_t ours = ExploredTiles(pair.ours->m_pMap, pair.size);
    std::printf("vis/party_explored: original %d, ours %d\n", original, ours);
    oracle::Compare(ctx, original, ours);
}

} // namespace

ORACLE_CASE("vis/mark_tile_line", MarkTileLineCase);
ORACLE_CASE("vis/sweep_vertical", SweepVerticalCase);
ORACLE_CASE("vis/sweep_horizontal", SweepHorizontalCase);
ORACLE_CASE("vis/clear_vertical", ClearVerticalCase);
ORACLE_CASE("vis/clear_horizontal", ClearHorizontalCase);
ORACLE_CASE("vis/party_explored", PartyExploredCase);
