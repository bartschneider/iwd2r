// The world clock's hour, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CTimerWorld::GetCurrentHour (0x45B500), __thiscall with no arguments and
// no stack cleanup: m_gameTime (the dword at +0) modulo
// TIMESCALE_MSEC_PER_DAY, then divided by the msec-per-sec, sec-per-min
// and min-per-hour constants (the original reads them from its .data, and
// ours' statics hold the same values). A zeroed timer with the one dword
// set is all the state either side needs, so the case sweeps the game
// time across the day boundaries, the hour boundaries and the unsigned
// extremes.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CTimerWorld.h"
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

using CurrentHourFn = uint8_t(__fastcall*)(const void* self, void* edx);
constexpr uintptr_t kCurrentHour = 0x45B500;

void CheckHour(CaseContext& ctx, uint32_t gameTime)
{
    ctx.SetInputs("game time " + std::to_string(gameTime));

    auto* ours = static_cast<unsigned char*>(::operator new(sizeof(CTimerWorld)));
    std::memset(ours, 0, sizeof(CTimerWorld));
    std::memcpy(ours, &gameTime, 4);
    const BYTE oursHour = reinterpret_cast<CTimerWorld*>(ours)->CTimerWorld::GetCurrentHour();

    uint8_t originalHour = 0;
    ctx.CallOriginal([&] {
        auto* timer = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(4));
        std::memcpy(timer, &gameTime, 4);
        originalHour = reinterpret_cast<CurrentHourFn>(kCurrentHour)(timer, nullptr);
    });

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalHour, static_cast<uint8_t>(oursHour));
}

// GetCurrentHour over the day, hour and wrap boundaries.
void TimerWorldHour(CaseContext& ctx)
{
    static const uint32_t kTimes[] = {
        0, // the day's start
        4499, 4500, 4501, // the first hour boundary
        10799, // just before the third hour
        359999, 360000, 360001, // the tenth hour boundary
        2159999, 2160000, // the tenth hour of the second day
        43199999, // the last second of the day
        86399999, 86399999 + 1, // the day boundary itself
        2147483647u, // INT_MAX
        4294918496u, // 2^32 - one day: back to hour 0
        4294967295u, // UINT_MAX
    };
    for (const uint32_t gameTime : kTimes) {
        CheckHour(ctx, gameTime);
    }
}

} // namespace

ORACLE_CASE("timer-45B500", TimerWorldHour);
