// Movement and targeting helpers, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CAIGroup::HandleFollowPath (0x409820) works only on sprites it reaches
// through the original's object array (CGameObjectArray::GetDeny on
// g_pBaldurChitin->m_pObjectGame + 0x372C) and their virtual AddAction and
// InsertAction; with no member granted it does nothing. The original's
// sprites can only come from its CGameSprite constructor (0x6EF990), which
// needs the resource manager and a loaded creature file (R10a), so the case
// is pending. Our side is covered by the Catch2 test "[move]
// HandleFollowPath" on the started engine (tests/test_move.cpp).
//
// CGameSprite::IsVisibleToCaller (0x727B80) reads only the derived general
// state (0x920) and the creature header's byte at 0x8A0 (R10b): a zeroed
// block from the original's operator new with those two set, against zeroed
// CGameSprite storage of ours with the same members set. __thiscall with one
// argument, `ret 4` at 0x727BA0, called through a __fastcall pointer whose
// edx argument is unused.
//
// CGameSprite::GetDirection(ptStart, ptTarget) (0x706DD0) is a pure function
// of the two points' coordinates; both points sit in a block of the
// original's (R10b).
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CAIGroup.h"
#include "CGameSprite.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

using IsVisibleFn = int32_t(__fastcall*)(const void* self, void* edx, int32_t bSeeInvisible);
constexpr uintptr_t kIsVisibleToCaller = 0x727B80;
constexpr size_t kGeneralStateOffset = 0x920;
constexpr size_t kField2FCOffset = 0x8A0;
constexpr size_t kSpriteBlockSize = 0x924;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

// Every combination of the three inputs' interesting values: seeInvisible
// 0, 1 and 2; the general state with and without STATE_INVISIBLE (alone and
// with other bits); field_2FC with and without its low bit.
void IsVisibleToCaller(CaseContext& ctx)
{
    uint8_t* original = nullptr;
    ctx.SetInputs("allocating the original's sprite block");
    ctx.CallOriginal([&] { original = OriginalBlock(kSpriteBlockSize); });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameSprite)));
    std::memset(storage, 0, sizeof(CGameSprite));
    auto* ours = reinterpret_cast<CGameSprite*>(storage);

    const int32_t sees[] = { 0, 1, 2 };
    const uint32_t states[] = { 0, STATE_INVISIBLE, STATE_INVISIBLE | STATE_DEAD, STATE_DEAD, 0xFFFFFFFFu,
        0xFFFFFFFFu & ~static_cast<uint32_t>(STATE_INVISIBLE) };
    const uint8_t fields[] = { 0, 1, 2, 3, 0xFE, 0xFF };

    for (int32_t see : sees) {
        for (uint32_t state : states) {
            for (uint8_t field : fields) {
                ctx.SetInputs("IsVisibleToCaller(" + std::to_string(see) + ") with general state "
                    + std::to_string(state) + " and field_2FC " + std::to_string(field));
                int32_t originalResult = 0;
                ctx.CallOriginal([&] {
                    std::memcpy(original + kGeneralStateOffset, &state, 4);
                    original[kField2FCOffset] = field;
                    originalResult = reinterpret_cast<IsVisibleFn>(kIsVisibleToCaller)(original, nullptr, see);
                });
                ours->m_derivedStats.m_generalState = state;
                ours->m_baseStats.field_2FC = field;
                const BOOL oursResult = ours->IsVisibleToCaller(see);
                ctx.CountInputs(1);
                oracle::Compare(ctx, originalResult, oursResult);
            }
        }
    }
}

// CAIGroup::RotateOffsets(pOffsets, nOffsets, nDirection) (0x4058E0):
// __stdcall, three stack arguments, `ret 0xc` at 0x405A09; reads and writes only the
// array (R10b: an array in the original's memory).
using RotateFn = void(__stdcall*)(void* pOffsets, int32_t nOffsets, int32_t nDirection);
constexpr uintptr_t kRotateOffsets = 0x4058E0;

// Whether RotateOffsets' product for one axis of `offset` lies within 1e-6
// of a whole number (recomputed in doubles as the port does).
bool NearWhole(const CPoint& offset, int32_t direction, bool xAxis)
{
    const double pi = 3.1415926535;
    const double length = std::sqrt(static_cast<double>(offset.y * offset.y + offset.x * offset.x));
    if (length == 0.0) {
        return false;
    }
    double angle = std::acos(static_cast<double>(std::abs(offset.x)) / length);
    if (offset.x < 0) {
        angle = offset.y < 0 ? angle + pi : pi - angle;
    } else if (offset.y < 0) {
        angle = 6.283185307 - angle;
    }
    angle += static_cast<double>(direction * 2) * pi / 360.0;
    const double product = (xAxis ? std::cos(angle) : std::sin(angle)) * length;
    return std::fabs(product - std::floor(product + 0.5)) < 1e-6;
}

// Offsets as GetOffsets makes them (thousandths of a search square, (0, 0)
// for the leader, every quadrant and both axes), counts 1 to 12, directions
// over the circle and beyond.
void RotateOffsets(CaseContext& ctx)
{
    uint64_t state = ctx.seed ^ 0x4058E0ull;
    oracle::RandomGen<uint32_t> bits(oracle::SplitMix64(&state));
    auto between = [&](int32_t lo, int32_t hi) {
        return static_cast<int32_t>(lo + static_cast<int32_t>(*bits.Next() % static_cast<uint32_t>(hi - lo + 1)));
    };

    int32_t* original = nullptr;
    ctx.SetInputs("allocating the original's offsets");
    ctx.CallOriginal([&] { original = reinterpret_cast<int32_t*>(OriginalBlock(12 * 8)); });

    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t count = between(1, 12);
        const int32_t direction = between(-20, 400);
        CPoint ours[12];
        std::string inputs = "RotateOffsets(direction " + std::to_string(direction) + ",";
        for (int32_t n = 0; n < count; ++n) {
            const int32_t kind = between(0, 5);
            ours[n].x = kind == 0 ? 0 : between(-6, 6) * 1000;
            ours[n].y = kind == 1 ? 0 : between(-6, 6) * 1000;
            if (n == 0 && between(0, 1) == 0) {
                ours[n].x = 0;
                ours[n].y = 0;
            }
            original[2 * n] = ours[n].x;
            original[2 * n + 1] = ours[n].y;
            inputs += " (" + std::to_string(ours[n].x) + "," + std::to_string(ours[n].y) + ")";
        }
        ctx.SetInputs(inputs + ")");
        ctx.CallOriginal([&] { reinterpret_cast<RotateFn>(kRotateOffsets)(original, count, direction); });
        CPoint before[12];
        for (int32_t n = 0; n < count; ++n) {
            before[n] = ours[n];
        }
        CAIGroup::RotateOffsets(ours, static_cast<SHORT>(count), static_cast<SHORT>(direction));
        ctx.CountInputs(1);
        for (int32_t n = 0; n < count; ++n) {
            // The original's products are x87 80-bit values; ours doubles.
            // A product within 1e-6 of a whole number may truncate one
            // apart (docs/evidence/4058E0.md); only that is let through.
            if (NearWhole(before[n], direction, true) && std::abs(original[2 * n] - ours[n].x) == 1) {
                ours[n].x = original[2 * n];
            }
            if (NearWhole(before[n], direction, false) && std::abs(original[2 * n + 1] - ours[n].y) == 1) {
                ours[n].y = original[2 * n + 1];
            }
            oracle::Compare(ctx, std::to_string(original[2 * n]) + "," + std::to_string(original[2 * n + 1]),
                std::to_string(ours[n].x) + "," + std::to_string(ours[n].y));
        }
    }
}

// CGameSprite::GetDirection(const CPoint& ptStart, const CPoint& ptTarget)
// (0x706DD0): static __cdecl, the two pointers at [esp+4] and [esp+8], plain
// ret at 0x706DEF (its caller 0x706D50 pops 8 at 0x706DC1); the direction
// in ax.
using GetDirectionFn = int16_t(__cdecl*)(const int32_t* pStart, const int32_t* pTarget);
constexpr uintptr_t kGetDirection = 0x706DD0;

// Every step of up to 40 either way from three starts (the same point, the
// axes, all four quadrants and both sides of every threshold), one of them
// walk_and_talk's up-left step, then random pairs over the area range and
// beyond it on both sides.
void GetDirection(CaseContext& ctx)
{
    int32_t* original = nullptr;
    ctx.SetInputs("allocating the original's points");
    ctx.CallOriginal([&] { original = reinterpret_cast<int32_t*>(OriginalBlock(4 * 4)); });

    auto check = [&](int32_t sx, int32_t sy, int32_t tx, int32_t ty) {
        ctx.SetInputs("GetDirection((" + std::to_string(sx) + "," + std::to_string(sy) + "), ("
            + std::to_string(tx) + "," + std::to_string(ty) + "))");
        int16_t originalResult = 0;
        ctx.CallOriginal([&] {
            original[0] = sx;
            original[1] = sy;
            original[2] = tx;
            original[3] = ty;
            originalResult = reinterpret_cast<GetDirectionFn>(kGetDirection)(original, original + 2);
        });
        const SHORT oursResult = CGameSprite::GetDirection(CPoint(sx, sy), CPoint(tx, ty));
        ctx.CountInputs(1);
        oracle::Compare(ctx, static_cast<int32_t>(originalResult), static_cast<int32_t>(oursResult));
    };

    const int32_t starts[][2] = { { 1000, 1000 }, { 0, 0 }, { 2232, 1208 } };
    for (const auto& start : starts) {
        for (int32_t dx = -40; dx <= 40; ++dx) {
            for (int32_t dy = -40; dy <= 40; ++dy) {
                check(start[0], start[1], start[0] + dx, start[1] + dy);
            }
        }
    }

    uint64_t state = ctx.seed ^ 0x706DD0ull;
    oracle::RandomGen<uint32_t> bits(oracle::SplitMix64(&state));
    auto between = [&](int32_t lo, int32_t hi) {
        return static_cast<int32_t>(lo + static_cast<int32_t>(*bits.Next() % static_cast<uint32_t>(hi - lo + 1)));
    };
    for (int i = 0; i < ctx.iterations; ++i) {
        check(between(-2000, 20000), between(-2000, 20000), between(-2000, 20000), between(-2000, 20000));
    }
}

} // namespace

ORACLE_CASE("move/visible_to_caller", IsVisibleToCaller);
ORACLE_CASE("move/get_direction", GetDirection);
ORACLE_CASE("group/rotate_offsets", RotateOffsets);
ORACLE_CASE_PENDING("move/follow_path", nullptr,
    "needs engine state (sprites from the original's CGameSprite constructor in its object array, the "
    "message handler); ours covered by tests/test_move.cpp");
