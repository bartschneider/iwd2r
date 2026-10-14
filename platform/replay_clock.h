// The replay clock, shared by both sides of a replay comparison: the replay
// recorder in the original (oracle/replay/recorder.cpp) and our engine's
// deterministic replay mode (platform/replay.cpp, active when
// IWD2R_STATE_DUMP is set). Both count a frame at each call of
// CBaldurChitin::SynchronousUpdate that FrameGate lets through and advance
// the clock by exactly the same amount, so the AI thread sees the same
// GetTickCount values, loads included, and the
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

// The order inside a frame. Both sides fire a frame's due timers when
// CChitin::SynchronousUpdate returns (the recorder's OnFrameEnd, ours
// platform::replay::OnFrameEnd), not when the frame starts. The timer wakes
// the AI thread, so the AI update of frame N runs after the main thread has
// drawn frame N and before frame N+1 starts (the main thread waits for the
// update's m_bDisplayStale). The clock still advances at the start of the
// frame, so the frame's render and the AI update read the same time.
//
// Why: with the timers fired at the start of the frame, the AI update ran
// beside the main thread's render, and the order of the two came from the
// machine's thread scheduling, which no virtual clock pins. It shows where
// both take the visible area's list lock (field_214): CGameArea::Render
// (0x477740) holds it from its start until after it clears m_firstRender
// and starts the world timer, while CGameArea::AIUpdate (0x46E3D0) takes it
// in SortLists and returns early if m_firstRender is still 1 after it. The
// original's main thread got there first in walk_and_talk's frame 1498 (its
// AI thread drew 185 rand() calls, a full update); ours spent 2.4 ms in the
// UI render before it, and its AI thread, woken 16 us after the timer, got
// there first (2 calls). Serializing the two removes the race on both sides
// and keeps the order the original showed.
//
// The frame gate: which calls of CBaldurChitin::SynchronousUpdate count as
// frames. Both sides ask it at the start of each call after the first, and
// a call it holds does nothing more: no frame is counted, the clock does
// not move, no timer fires, the scenario does not advance and no dump is
// written. The game still draws it (the progress screen).
//
// The original's loop (CChitin::WinMain 0x7926B0, CBaldurChitin::MainAIThread
// 0x4242B0): the AI thread runs one AsynchronousUpdate per timer wake-up,
// with m_bInAsynchronousUpdate (0x1912) set around it, then sets
// m_bDisplayStale (0x193A); the main thread calls SynchronousUpdate only
// when m_bDisplayStale is set. Normally that is once per AI update. A load
// runs inside one AI update and sets m_bDisplayStale itself to redraw its
// progress (CGameArea::ProgressBarCallback 0x474E10 when 250 ms of
// GetTickCount have passed; CInfGame::sub_59FA00 0x59FA00, which then waits
// for the main thread to take it), so the main thread draws frames while
// the AI thread is inside the update. Without the gate each of them
// advanced the clock and fired the timers, so the AI thread saw an amount
// of time pass during the load that depended on the machine's speed.
//
// Held: a call while the AI thread is inside an update (inUpdate), and the
// one call right after the frame that ended such a stretch when au_counter
// has not moved since that frame (the redraw the update's end requested,
// when the frame that counted was one the load asked for just before it
// ended and the two requests did not merge). Every other call counts, as
// before the gate.
//
// The echo rule covers one call only. The interleaving it misses: a raced
// end-of-load frame F counts for update N; the next call C sees au N+1 and
// counts it, correctly; a further call D, before update N+2 starts, sees au
// N+1 with inUpdate 0 and the echo flag already cleared, and counts a
// second frame for N+1. That does not drift silently: D is a stall (no AI
// update since the previous frame), so ai_stalled_frames becomes 1 and
// tools/oracle.py's invariant fails every later checkpoint.
//
// The main thread cannot wait inside the frame for the load to finish: the
// load waits for the main thread (sub_59FA00 spins while
// m_bInSynchronousUpdate, 0x193E, is set, and WinMain sets it around the
// SynchronousUpdate call whose first instruction is the frame hook).
// Returning at once lets the main thread go on drawing the progress screen.
//
// A load (Loads()): an AI update during which a call was held while
// CChitin::cProgressBar.m_bProgressBarActivated (0x1986, set only by
// CBaldurChitin::SetProgressBar 0x425710) was set. It assumes the bar is
// cleared within the load's own AI update, which holds for the load the
// replays reach today (SetProgressBar TRUE and FALSE at
// src/CScreenSinglePlayer.cpp:1384 and 1642, both inside
// CScreenSinglePlayer::OnMainDoneButtonClick 0x6629E0). A load that keeps
// the bar up across updates would make the count depend on which updates
// happened to draw a held call; it must then become a count of the bar's
// rising edges.
class FrameGate {
public:
    // auCounter, inUpdate and progressBar: CChitin::nAUCounter,
    // m_bInAsynchronousUpdate and cProgressBar.m_bProgressBarActivated, read
    // in that order (if the AI thread starts an update between the reads,
    // inUpdate sees it). True when the call counts as a frame.
    bool Counts(int32_t auCounter, bool inUpdate, bool progressBar)
    {
        const bool echo = m_afterRelease && auCounter == m_releaseAu;
        m_afterRelease = false;
        m_endedStretch = 0;
        if (inUpdate) {
            ++m_heldFrames;
            ++m_stretch;
            if (progressBar && !(m_haveLoadAu && m_loadAu == auCounter)) {
                ++m_loads;
                m_haveLoadAu = true;
                m_loadAu = auCounter;
            }
            return false;
        }
        if (echo) {
            ++m_heldFrames;
            return false;
        }
        if (m_stretch) {
            m_endedStretch = m_stretch;
            m_stretch = 0;
            m_afterRelease = true;
            m_releaseAu = auCounter;
        }
        return true;
    }

    // Calls held so far.
    uint32_t HeldFrames() const { return m_heldFrames; }
    // Loads so far.
    uint32_t Loads() const { return m_loads; }
    // Calls held inside the current update so far (0 after a call that
    // counted).
    uint32_t Stretch() const { return m_stretch; }
    // After a call that counted: the calls held inside the update it
    // followed (0 if none).
    uint32_t EndedStretch() const { return m_endedStretch; }

private:
    uint32_t m_heldFrames = 0;
    uint32_t m_loads = 0;
    uint32_t m_stretch = 0;
    uint32_t m_endedStretch = 0;
    bool m_afterRelease = false;
    int32_t m_releaseAu = 0;
    bool m_haveLoadAu = false;
    int32_t m_loadAu = 0;
};

}
