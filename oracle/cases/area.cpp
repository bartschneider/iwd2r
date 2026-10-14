// The area's goal adjustment (CGameArea::AdjustTarget and
// SnapshotAdjustTarget), the original's against ours, over the search map of
// the first area (AR1000SR.BMP from the extracted game data).
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures (R10b, R10c). Both functions read only the area: its visibility
// map (0x860: m_pMap, m_nWidth at 0x866, m_nMapSize at 0x86A) and its search
// bitmap (0x76C: the CVidBitmap's pRes at +0xA4, m_nBitCount at +0xB0 and
// m_bDoubleSize at +0xB6; m_pDynamicCost +0xBA, m_snapshotDynamicCost +0xBE,
// m_snapshotTerrainTable +0xC2, m_GridSquareDimensions +0xE6, m_pArea +0xEE,
// m_snapshotPersonalSpace +0xF2, m_sourceSide +0xF3). The original's area
// is a zeroed block from its operator new holding only those members; its
// CResBitmap is a zeroed block parsed by the original's own
// CResBitmap::Parse (0x77F1C0) from the file's bytes copied into its memory.
// Ours is zeroed CGameArea and CResBitmap storage filled the same way, our
// Parse on our copy of the bytes. The 4-bit search map never reaches the
// terrain-hugging option (only the 8-bit branch reads g_pBaldurChitin), so
// no game is needed. The explored flags and the dynamic costs come from the
// run's seed, the same bytes on both sides. Original objects are never
// freed; ours are leaked too (their destructors would free what they never
// owned).
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CGameArea.h"
#include "CGameObject.h"
#include "CResBitmap.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CGameArea::AdjustTarget(start.x, start.y, goal, personalSpace, tolerance)
// (0x46A3D0) and SnapshotAdjustTarget(start.x, start.y, goal, bBump,
// tolerance) (0x46A630): `ret 0x14` at 0x46A614 and 0x46A797; BOOL in eax.
using AdjustFn = int32_t(__fastcall*)(void* self, void* edx, int32_t x, int32_t y, int32_t* goal, uint32_t arg,
    int32_t tolerance);
constexpr uintptr_t kAdjustTarget = 0x46A3D0;
constexpr uintptr_t kSnapshotAdjustTarget = 0x46A630;
// CResBitmap::Parse(void* pData) (0x77F1C0), `ret 4`.
using ParseFn = int32_t(__fastcall*)(void* self, void* edx, void* data);
constexpr uintptr_t kParse = 0x77F1C0;
// CGameObject::DEFAULT_TERRAIN_TABLE in the original, which
// SnapshotGetCost reads through m_snapshotTerrainTable.
constexpr uintptr_t kDefaultTerrainTable = 0x84C524;

constexpr size_t kAreaSize = 0x900;
constexpr size_t kResBitmapSize = 0x6C;
constexpr size_t kSearch = 0x76C;
constexpr size_t kVisibility = 0x860;

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

std::string ReadData(const CaseContext& ctx, const char* name)
{
    if (ctx.tables.empty()) {
        throw std::runtime_error(std::string("no --tables folder for ") + name);
    }
    const std::string path = ctx.tables + "\\" + name;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + path + " (tools/oracle.py run extracts it)");
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct Picks {
    Picks(uint64_t seed, uint64_t salt)
        : state(seed ^ salt)
        , bits(oracle::SplitMix64(&state))
    {
    }
    uint32_t Bits() { return *bits.Next(); }
    int32_t Between(int32_t lo, int32_t hi)
    {
        const uint32_t span = static_cast<uint32_t>(hi) - static_cast<uint32_t>(lo) + 1u;
        return static_cast<int32_t>(static_cast<uint32_t>(lo) + (span ? Bits() % span : Bits()));
    }

    uint64_t state;
    oracle::RandomGen<uint32_t> bits;
};

struct Fixture {
    uint8_t* original = nullptr;
    CGameArea* ours = nullptr;
    int32_t width = 0; // search squares
    int32_t height = 0;
};

// Both areas over AR1000SR.BMP, with the same explored flags and dynamic
// costs. GetCost and SnapshotGetCost read the dynamic cost of row
// m_GridSquareDimensions.cy and column cx (their bounds are inclusive), so
// both arrays carry a spare row.
Fixture MakeFixture(CaseContext& ctx, Picks& picks)
{
    Fixture f;
    const std::string bmp = ReadData(ctx, "AR1000SR.BMP");
    if (bmp.size() < 0x36) {
        throw std::runtime_error("AR1000SR.BMP is too short");
    }

    // Ours first: our Parse gives the dimensions both sides use.
    auto* oursBytes = new uint8_t[bmp.size()];
    std::memcpy(oursBytes, bmp.data(), bmp.size());
    auto* resStorage = static_cast<unsigned char*>(::operator new(sizeof(CResBitmap)));
    std::memset(resStorage, 0, sizeof(CResBitmap));
    auto* oursRes = reinterpret_cast<CResBitmap*>(resStorage);
    if (!oursRes->Parse(oursBytes)) {
        throw std::runtime_error("our CResBitmap::Parse refused AR1000SR.BMP");
    }
    f.width = oursRes->pBitmapInfoHeader->biWidth;
    f.height = oursRes->pBitmapInfoHeader->biHeight;
    const int16_t bitCount = static_cast<int16_t>(oursRes->pBitmapInfoHeader->biBitCount);

    // The visibility map: one tile per 32x32 pixels of the area (a search
    // square is 16x12), most explored.
    const int16_t visWidth = static_cast<int16_t>((f.width * 16 + 31) / 32);
    const int16_t visHeight = static_cast<int16_t>((f.height * 12 + 31) / 32);
    const int32_t visSize = visWidth * visHeight;
    std::vector<uint8_t> explored(static_cast<size_t>(visSize));
    for (uint8_t& tile : explored) {
        tile = picks.Between(0, 7) == 0 ? 0 : 0x80;
    }
    // The dynamic costs: mostly free, some creatures (bumpable 0x02,
    // 0x10), a door (0x80) and a wall-like 0x01.
    const size_t costSize = static_cast<size_t>(f.width) * static_cast<size_t>(f.height + 1) + 1;
    std::vector<uint8_t> costs(costSize);
    for (uint8_t& cost : costs) {
        const int32_t pick = picks.Between(0, 63);
        cost = pick == 0 ? 0x02 : pick == 1 ? 0x10 : pick == 2 ? 0x80 : pick == 3 ? 0x01 : 0;
    }

    auto* areaStorage = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(areaStorage, 0, sizeof(CGameArea));
    f.ours = reinterpret_cast<CGameArea*>(areaStorage);
    f.ours->m_search.m_resSearch.pRes = oursRes;
    f.ours->m_search.m_resSearch.m_nBitCount = bitCount;
    f.ours->m_search.m_resSearch.m_bDoubleSize = FALSE;
    auto* oursCosts = new uint8_t[costSize];
    std::memcpy(oursCosts, costs.data(), costSize);
    f.ours->m_search.m_pDynamicCost = oursCosts;
    auto* oursSnapshot = new uint8_t[costSize];
    std::memcpy(oursSnapshot, costs.data(), costSize);
    f.ours->m_search.m_snapshotDynamicCost = oursSnapshot;
    f.ours->m_search.m_snapshotTerrainTable = CGameObject::DEFAULT_TERRAIN_TABLE;
    f.ours->m_search.m_GridSquareDimensions.cx = f.width;
    f.ours->m_search.m_GridSquareDimensions.cy = f.height;
    f.ours->m_search.m_pArea = f.ours;
    auto* oursMap = new uint8_t[static_cast<size_t>(visSize)];
    std::memcpy(oursMap, explored.data(), explored.size());
    f.ours->m_visibility.m_pMap = oursMap;
    f.ours->m_visibility.m_nWidth = visWidth;
    f.ours->m_visibility.m_nHeight = visHeight;
    f.ours->m_visibility.m_nMapSize = visSize;

    ctx.SetInputs("building the original's area over AR1000SR.BMP");
    ctx.CallOriginal([&] {
        uint8_t* bytes = OriginalBlock(bmp.size());
        std::memcpy(bytes, bmp.data(), bmp.size());
        uint8_t* res = OriginalBlock(kResBitmapSize);
        if (!reinterpret_cast<ParseFn>(kParse)(res, nullptr, bytes)) {
            throw std::runtime_error("the original's CResBitmap::Parse refused AR1000SR.BMP");
        }
        uint8_t* origCosts = OriginalBlock(costSize);
        std::memcpy(origCosts, costs.data(), costSize);
        uint8_t* origSnapshot = OriginalBlock(costSize);
        std::memcpy(origSnapshot, costs.data(), costSize);
        uint8_t* origMap = OriginalBlock(static_cast<size_t>(visSize));
        std::memcpy(origMap, explored.data(), explored.size());

        uint8_t* area = OriginalBlock(kAreaSize);
        Put<uint32_t>(area, kSearch + 0xA4, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(res)));
        Put<int16_t>(area, kSearch + 0xB0, bitCount);
        Put<uint32_t>(area, kSearch + 0xBA, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(origCosts)));
        Put<uint32_t>(area, kSearch + 0xBE, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(origSnapshot)));
        Put<uint32_t>(area, kSearch + 0xC2, static_cast<uint32_t>(kDefaultTerrainTable));
        Put<int32_t>(area, kSearch + 0xE6, f.width);
        Put<int32_t>(area, kSearch + 0xEA, f.height);
        Put<uint32_t>(area, kSearch + 0xEE, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(area)));
        Put<uint32_t>(area, kVisibility + 0x0, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(origMap)));
        Put<int16_t>(area, kVisibility + 0x6, visWidth);
        Put<int16_t>(area, kVisibility + 0x8, visHeight);
        Put<int32_t>(area, kVisibility + 0xA, visSize);
        f.original = area;
    });
    return f;
}

std::string Point(int32_t x, int32_t y)
{
    return "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
}

// One call of each function on each side; the result and the goal it leaves.
void CompareOne(CaseContext& ctx, Fixture& f, bool snapshot, int32_t sx, int32_t sy, int32_t gx, int32_t gy,
    uint32_t arg, int32_t tolerance, uint8_t sourceSide, uint8_t personalSpace)
{
    ctx.SetInputs(std::string(snapshot ? "SnapshotAdjustTarget" : "AdjustTarget") + "(start " + Point(sx, sy)
        + ", goal " + Point(gx, gy) + (snapshot ? ", bBump " : ", personalSpace ") + std::to_string(arg)
        + ", tolerance " + std::to_string(tolerance)
        + (snapshot ? ", sourceSide " + std::to_string(sourceSide) + ", snapshot personal space "
                     + std::to_string(personalSpace)
                    : "")
        + ")");

    int32_t originalGoal[2] = { gx, gy };
    int32_t originalResult = 0;
    ctx.CallOriginal([&] {
        Put<uint8_t>(f.original, kSearch + 0xF2, personalSpace);
        Put<uint8_t>(f.original, kSearch + 0xF3, sourceSide);
        originalResult = reinterpret_cast<AdjustFn>(snapshot ? kSnapshotAdjustTarget : kAdjustTarget)(f.original,
            nullptr, sx, sy, originalGoal, arg, tolerance);
    });

    f.ours->m_search.m_snapshotPersonalSpace = personalSpace;
    f.ours->m_search.m_sourceSide = sourceSide;
    POINT oursGoal = { gx, gy };
    BOOL oursResult;
    if (snapshot) {
        oursResult = f.ours->SnapshotAdjustTarget(CPoint(sx, sy), &oursGoal, static_cast<BOOL>(arg),
            static_cast<SHORT>(tolerance));
    } else {
        oursResult = f.ours->AdjustTarget(CPoint(sx, sy), &oursGoal, static_cast<BYTE>(arg),
            static_cast<SHORT>(tolerance));
    }

    ctx.CountInputs(1);
    oracle::Compare(ctx, Point(originalResult, 0) + " " + Point(originalGoal[0], originalGoal[1]),
        Point(oursResult, 0) + " " + Point(oursGoal.x, oursGoal.y));
}

// Starts on the map, goals anywhere from 40 squares outside it to 40 inside
// the far edge: reachable goals (kept), goals in water or walls and in
// unexplored tiles (walked back toward the start), goals outside the map
// (walked back onto it), and the start itself. Personal spaces 1 to 7,
// tolerances from 0 to past the map's diagonal.
void AdjustTarget(CaseContext& ctx)
{
    Picks picks(ctx.seed, 0xA4D7A26E7ull);
    Fixture f = MakeFixture(ctx, picks);
    const int32_t tolerances[] = { 0, 1, 2, 5, 10, 40, 400 };
    const uint32_t spaces[] = { 1, 3, 5, 7 };
    const uint8_t sides[] = { 2, 30, 255 };

    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t sx = picks.Between(0, f.width - 1);
        const int32_t sy = picks.Between(0, f.height - 1);
        int32_t gx;
        int32_t gy;
        switch (picks.Between(0, 5)) {
        case 0: // near the start
            gx = sx + picks.Between(-6, 6);
            gy = sy + picks.Between(-6, 6);
            break;
        case 1: // the start itself
            gx = sx;
            gy = sy;
            break;
        case 2: // outside the map
            gx = picks.Between(-40, f.width + 40);
            gy = picks.Between(0, 1) ? picks.Between(-40, -1) : picks.Between(f.height, f.height + 40);
            break;
        default: // anywhere on the map
            gx = picks.Between(0, f.width - 1);
            gy = picks.Between(0, f.height - 1);
            break;
        }
        const int32_t tolerance = tolerances[picks.Between(0, 6)];
        CompareOne(ctx, f, false, sx, sy, gx, gy, spaces[picks.Between(0, 3)], tolerance, 0, 0);
        CompareOne(ctx, f, true, sx, sy, gx, gy, static_cast<uint32_t>(picks.Between(0, 1)), tolerance,
            sides[picks.Between(0, 2)], static_cast<uint8_t>(spaces[picks.Between(0, 3)]));
    }
}

} // namespace

ORACLE_CASE("area/adjust_target", AdjustTarget);
