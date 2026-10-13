// The replay clock, shared by both sides of a replay comparison: the replay
// recorder in the original (oracle/replay/recorder.cpp) and our engine's
// deterministic replay mode (platform/replay.cpp, active when
// IWD2R_STATE_DUMP is set). Both count a frame at each call of
// CBaldurChitin::SynchronousUpdate and advance the clock by exactly the
// same amount, so the AI thread sees the same GetTickCount values, and the
// wall clock behind time() (the original seeds its AI thread's rand with
// srand(time(NULL)), CScreenConnection::TimerAsynchronousUpdate 0x5FB3E0)
// reads the same second on both sides.
#pragma once

#include <cstdint>

namespace platform::replay_clock {

// GetTickCount (and timeGetTime) before the first frame.
constexpr uint32_t kClockBase = 0x01000000;
// Frames per virtual second: CChitin::TIMER_UPDATES_PER_SECOND's default.
// After frame N the clock reads kClockBase + N * 1000 / kFramesPerSecond.
constexpr uint32_t kFramesPerSecond = 30;

// The virtual clock after frame N (exact on average: 33 or 34 ms a frame).
constexpr uint32_t TickAfterFrame(uint32_t frame)
{
    return kClockBase + static_cast<uint32_t>(static_cast<uint64_t>(frame) * 1000 / kFramesPerSecond);
}

// The wall clock at kClockBase: midnight of Icewind Dale II's release day,
// as local time and as UTC (the time zone is UTC with no daylight saving).
constexpr int kWallClockYear = 2002;
constexpr int kWallClockMonth = 8;
constexpr int kWallClockDay = 27;
// The same instant in seconds since 1970-01-01 UTC: time() at kClockBase.
constexpr int64_t kWallClockEpoch = 1030406400;

// time() for a clock value: whole seconds since kClockBase, from
// kWallClockEpoch (VC6's time() reads GetLocalTime, whose milliseconds it
// drops).
constexpr int64_t WallClockSeconds(uint32_t tick)
{
    return kWallClockEpoch + static_cast<int64_t>((tick - kClockBase) / 1000);
}

}
