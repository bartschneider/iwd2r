// Projectile direction math, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CProjectile::GetDirection (0x52A480) reads the projectile's position
// (CPoint m_pos: LONG x at +6, y at +0xA in the original, the decompile's
// *(this + 6) and *(this + 10)), scales both points' y by 4/3 and asks
// CGameSprite::GetDirection (0x706DD0) for the direction from the scaled
// start to the scaled target. __thiscall with the CPoint target by value:
// two LONG arguments, x then y, `ret 8`. The original gets a zeroed block
// from its operator new with only m_pos set at +6, and calls the real
// 0x706DD0; ours gets zeroed storage with m_pos set through the member and
// calls ours, which calls ours' ported CGameSprite::GetDirection. The
// offsets differ: ours' CGameObject compiles at default packing
// (BalDataTypes.h's pack(2) window closes before CGameObject.h's class
// body), so ours' m_pos sits at +8, four-aligned after the type byte,
// while the original's is packed at +6. The case writes each side at its
// own real offset. The SHORT that comes back is compared.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CProjectile.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CProjectile::GetDirection(CPoint target) (0x52A480): __thiscall, the
// point by value as two LONG arguments, SHORT in ax.
using GetDirectionFn = int16_t(__fastcall*)(const void* self, void* edx, int32_t targetX, int32_t targetY);
constexpr uintptr_t kGetDirection = 0x52A480;

constexpr size_t kPosOffset = 0x6; // the original's CPoint m_pos: x at +0x6, y at +0xA

struct Point {
    int32_t x;
    int32_t y;
};

// One projectile position and target on each side: the SHORT direction both
// return from the same scaled points.
void CheckDirection(CaseContext& ctx, const Point& pos, const Point& target)
{
    ctx.SetInputs("m_pos (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + "), target ("
        + std::to_string(target.x) + ", " + std::to_string(target.y) + ")");

    int16_t original = 0;
    ctx.CallOriginal([&] {
        auto* self = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(0x100));
        std::memset(self, 0, 0x100);
        std::memcpy(self + kPosOffset, &pos.x, 4);
        std::memcpy(self + kPosOffset + 4, &pos.y, 4);
        original = reinterpret_cast<GetDirectionFn>(kGetDirection)(self, nullptr, target.x, target.y);
    });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CProjectile)));
    std::memset(storage, 0, sizeof(CProjectile));
    auto* ours = reinterpret_cast<CProjectile*>(storage);
    ours->m_pos.x = pos.x;
    ours->m_pos.y = pos.y;
    const int16_t oursResult = ours->CProjectile::GetDirection(CPoint(target.x, target.y));
    ::operator delete(storage);

    ctx.CountInputs(1);
    oracle::Compare(ctx, original, oursResult);
}

// CProjectile::GetDirection over a grid of positions and targets: the
// origin, points with the 4:3 scaling exact and inexact, and the extremes
// where (y << 2) overflows nothing but stays large.
void ProjectileDirection(CaseContext& ctx)
{
    static const Point kPositions[] = {
        { 0, 0 },
        { 100, 75 },
        { -100, -75 },
        { 32000, 24000 },
        { -32000, -24000 },
    };
    static const Point kTargets[] = {
        { 0, 0 },
        { 1000, 750 },
        { -1000, -750 },
        { 30000, 30000 },
        { -30000, -30000 },
    };
    for (const Point& pos : kPositions) {
        for (const Point& target : kTargets) {
            CheckDirection(ctx, pos, target);
        }
    }
}

} // namespace

ORACLE_CASE("projectile-52A480", ProjectileDirection);
