// CWeather::GetWind and CRuleTables::GetLayOnHandsAmount, the original's
// against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. Both functions only read members, so the original's objects are
// zeroed blocks from its operator new holding just those:
// - GetWind (0x5564F0) reads the CWeather's m_bWindOn (byte, 0x158),
//   m_nWindLevel (short, 0x18) and m_nDurationCounter (dword, 0x1E), and the
//   static WEATHER_TRANSITION_TIME (0x8E1C50, a word). That static lives in
//   .data's zero-filled tail and is set by the original's static
//   constructors (10 * CTimerWorld::TIMESCALE_MSEC_PER_SEC, 150), which the
//   runner does not run; the case stores 150 there (the runner restores .data
//   before every case). Ours is zeroed storage of our CWeather's size with
//   the same three members set (our constructor builds the wind and rain
//   sounds, which want the engine).
// - GetLayOnHandsAmount (0x544930) never reads `this`; it reads the
//   CAIObjectType's class mask byte (0x18), the CDerivedStats' paladin level
//   through CDerivedStats::GetClassLevel (0x447990: the byte at this + 0x46 +
//   class) and its charisma (short, 0x5E). Ours gets a CAIObjectType and a
//   CDerivedStats from our constructors with the same members set, and is
//   called on zeroed storage of our CRuleTables' size (it reads no table).
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CAIObjectType.h"
#include "CDerivedStats.h"
#include "CRuleTables.h"
#include "CWeather.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CWeather::GetWind() (0x5564F0): the share in al, plain ret.
using GetWindFn = uint8_t(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kGetWind = 0x5564F0;
constexpr uintptr_t kTransitionTime = 0x8E1C50;
// Up to m_nWindVolumeLevel (0x15E), rounded up.
constexpr size_t kWeatherSize = 0x164;

// CRuleTables::GetLayOnHandsAmount(typeAI, DStats) (0x544930): two stack
// arguments, `ret 8`, the amount in eax.
using LayOnHandsFn = int32_t(__fastcall*)(const void* self, void* edx, const void* typeAI, void* dstats);
constexpr uintptr_t kLayOnHands = 0x544930;
constexpr size_t kTypeAISize = 0x40;
constexpr size_t kDerivedStatsSize = 0x100;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

// GetWind (0x5564F0): the wind off and on, the levels 0, 16 and 32, and the
// counter at 0, around the first fifth of the transition (30), around 150
// and at the signed compare's edges (0x7FFFFFFF, 0x80000000, 0xFFFFFFFF);
// then --iterations random states.
void GetWind(CaseContext& ctx)
{
    std::unique_ptr<uint8_t[]> storage(new uint8_t[sizeof(CWeather)]());
    CWeather* ours = reinterpret_cast<CWeather*>(storage.get());
    auto gen = oracle::Random<uint32_t>(ctx.seed ^ 0x57696E6400000000ull); // "Wind"
    auto check = [&](uint8_t bWindOn, int16_t nWindLevel, uint32_t nCounter) {
        ctx.SetInputs("wind on " + std::to_string(bWindOn) + ", level " + std::to_string(nWindLevel) + ", counter "
            + std::to_string(nCounter));
        uint8_t original = 0;
        ctx.CallOriginal([&] {
            const uint16_t nTransition = 150;
            std::memcpy(reinterpret_cast<void*>(kTransitionTime), &nTransition, sizeof(nTransition));
            uint8_t* weather = OriginalBlock(kWeatherSize);
            weather[0x158] = bWindOn;
            std::memcpy(weather + 0x18, &nWindLevel, sizeof(nWindLevel));
            std::memcpy(weather + 0x1E, &nCounter, sizeof(nCounter));
            original = reinterpret_cast<GetWindFn>(kGetWind)(weather, nullptr);
        });
        ours->m_bWindOn = bWindOn;
        ours->m_nWindLevel = nWindLevel;
        ours->m_nDurationCounter = nCounter;
        ctx.CountInputs(1);
        oracle::Compare(ctx, original, ours->GetWind());
    };
    for (const int nWindOn : { 0, 1 }) {
        for (const int nWindLevel : { 0, 16, 32 }) {
            for (const uint32_t nCounter : { 0u, 1u, 15u, 29u, 30u, 31u, 149u, 150u, 151u, 1000u, 0x7FFFFFFFu,
                     0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu }) {
                check(static_cast<uint8_t>(nWindOn), static_cast<int16_t>(nWindLevel), nCounter);
            }
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const uint32_t bits = *gen.Next();
        check(static_cast<uint8_t>(bits & 1), static_cast<int16_t>((bits & 2) ? 16 : (bits >> 2) % 64),
            *gen.Next() % 400);
    }
}

// GetLayOnHandsAmount (0x544930): the paladin bit on and off in the object
// type and in the derived stats' mask, paladin levels 0-30 and charisma -2
// to 60; then --iterations random states.
void LayOnHands(CaseContext& ctx)
{
    std::unique_ptr<CDerivedStats> stats = std::make_unique<CDerivedStats>();
    CAIObjectType typeAI;
    std::unique_ptr<uint8_t[]> storage(new uint8_t[sizeof(CRuleTables)]());
    const CRuleTables* rules = reinterpret_cast<const CRuleTables*>(storage.get());
    auto gen = oracle::Random<uint32_t>(ctx.seed ^ 0x50616C6164696E00ull); // "Paladin"
    auto check = [&](uint8_t nTypeMask, uint32_t nStatsMask, uint8_t nLevel, int16_t nCHR) {
        ctx.SetInputs("type class mask " + std::to_string(nTypeMask) + ", stats class mask "
            + std::to_string(nStatsMask) + ", paladin level " + std::to_string(nLevel) + ", charisma "
            + std::to_string(nCHR));
        int32_t original = 0;
        ctx.CallOriginal([&] {
            uint8_t* type = OriginalBlock(kTypeAISize);
            uint8_t* dstats = OriginalBlock(kDerivedStatsSize);
            type[0x18] = nTypeMask;
            std::memcpy(dstats + 0x42, &nStatsMask, sizeof(nStatsMask));
            dstats[0x46 + CAIOBJECTTYPE_C_PALADIN] = nLevel;
            std::memcpy(dstats + 0x5E, &nCHR, sizeof(nCHR));
            original = reinterpret_cast<LayOnHandsFn>(kLayOnHands)(nullptr, nullptr, type, dstats);
        });
        typeAI.m_nClassMask = nTypeMask;
        stats->m_classMask = nStatsMask;
        stats->m_nClassLevels[CAIOBJECTTYPE_C_PALADIN - 1] = nLevel;
        stats->m_nCHR = nCHR;
        ctx.CountInputs(1);
        // Ours keeps the amount as SHORT (CRuleTables.h); the original's
        // callers store it in the SHORT m_nLayOnHandsAmount.
        oracle::Compare(ctx, static_cast<int16_t>(original), rules->GetLayOnHandsAmount(typeAI, *stats));
    };
    for (const int nTypeMask : { 0x00, CLASSMASK_PALADIN, 0xFF }) {
        for (const uint32_t nStatsMask : { 0x00u, static_cast<uint32_t>(CLASSMASK_PALADIN) }) {
            for (int nLevel = 0; nLevel <= 30; nLevel += 5) {
                for (int nCHR = -2; nCHR <= 60; nCHR++) {
                    check(static_cast<uint8_t>(nTypeMask), nStatsMask, static_cast<uint8_t>(nLevel), static_cast<int16_t>(nCHR));
                }
            }
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const uint32_t bits = *gen.Next();
        check(static_cast<uint8_t>(bits & 0xFF), (bits >> 8) & 0xFF, static_cast<uint8_t>((bits >> 16) % 31),
            static_cast<int16_t>(*gen.Next() % 100));
    }
}

} // namespace

ORACLE_CASE("weather/get_wind", GetWind);
ORACLE_CASE("rules/lay_on_hands_amount", LayOnHands);
