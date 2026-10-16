// The deterministic replay mode: our engine's half of a replay comparison
// (tools/oracle.py replay compare). It is on only when IWD2R_STATE_DUMP
// names a folder; otherwise every function here is a no-op or answers
// false, and the engine runs exactly as without it.
//
// It reproduces the replay recorder's model of the original
// (oracle/replay/recorder.cpp) exactly:
//   - the frame is a call of CBaldurChitin::SynchronousUpdate, which calls
//     OnFrame first (the recorder's trampoline at 0x425FB0), unless the
//     frame gate holds it (platform/replay_clock.h FrameGate: a call drawn
//     while the AI thread is inside an update, as a load redraws its
//     progress), and then OnFrame returns at once;
//   - the clock (GetTickCount, timeGetTime) is virtual: kClockBase, then one
//     step per frame (platform/replay_clock.h, shared with the recorder);
//   - timeSetEvent timers run on that clock, fired on the main thread by
//     OnFrameEnd, which CBaldurChitin::SynchronousUpdate calls after
//     CChitin::SynchronousUpdate returns (the recorder's FrameTrampoline
//     calls it there too), so the AI thread wakes once per frame and its
//     update runs after the frame's render while the frame takes less
//     than the AI thread's 100 ms wait; OnFrameEnd counts a frame where
//     it did not in ai_overlap_frames (platform/replay_clock.h, "The order
//     inside a frame");
//   - time() (and CTime) reads a virtual wall clock derived from it, UTC;
//   - frame 1 seeds the main thread's rand with IWD2R_SEED (default 1), as
//     the recorder runs the original's srand;
//   - input comes only from the scenario (IWD2R_SCRIPT), advanced once per
//     frame from OnFrame: keys up and the last cursor outside it;
//   - focus and activation never reach the engine (the recorder's focus
//     guard), and message boxes take their first button unseen;
//   - the stall count (ai_stalled_frames) is kept the same way;
//   - a path search an object's update queues is done before the AI thread
//     updates the next object (CGameArea::AIUpdate, ReplayWaitForSearch),
//     so the search thread's snapshot of the search map and the frame its
//     result is first seen do not depend on thread scheduling: the original
//     shows that timing in every recording, and without the wait about one
//     run of ours in five walked a different path in walk_and_talk.
//
// OnFrame writes frame-<N>.json every IWD2R_DUMP_EVERY frames (default 30,
// 0 for none), checkpoint-<NAME>.json at the scenario's checkpoint lines,
// and status.txt (frame, script_line, state), all in IWD2R_STATE_DUMP.
#pragma once

#include "platform/replay_probe.h"
#include "platform/win32_api.h"

#include <cstdint>
#include <functional>
#include <string>

namespace platform::replay {

// True when IWD2R_STATE_DUMP is set and not empty (read once).
bool Active();

// The virtual clock in ms (replay_clock::kClockBase before frame 1).
uint32_t NowMs();

// Frames counted so far.
uint32_t Frame();

// The frame: CBaldurChitin::SynchronousUpdate calls it first. Does nothing
// unless Active().
void OnFrame();

// The end of the frame: CBaldurChitin::SynchronousUpdate calls it after
// CChitin::SynchronousUpdate. When the call counted as a frame it fires the
// due timers, waking the AI thread. Does nothing unless Active().
void OnFrameEnd();

// Writes checkpoint-<name>.json now (the scenario's checkpoint verb).
void Checkpoint(const std::string& name, int line);

// timeSetEvent and timeKillEvent on the virtual clock.
MMRESULT SetTimer(UINT delayMs, LPTIMECALLBACK callback, DWORD_PTR user, UINT flags);
bool KillTimer(UINT id);

// The input the engine polls: the scenario's cursor (remembered as the last
// one), else the last cursor; the scenario's key state, else up. Both note
// the first thread other than the main one that polls, whose rand state the
// dump reports as ai_rng_state.
POINT Cursor();
SHORT Key(int vk);
// SetCursorPos: the last cursor.
void SetCursor(int x, int y);

// The wait for the search thread (CGameArea::AIUpdate's ReplayWaitForSearch)
// must end even when the search never does: the search thread can lose a
// request's semaphore post (SearchThreadMain 0x5492E0 tests its lists and
// posts to itself without field_1B58 after some results, and the
// semaphore's maximum is 1), or there may be no search thread. WaitForSearch
// calls pending, which answers whether the wait goes on and says what it
// waits for in *what, every millisecond until it answers false (then it
// returns true) or deadlineMs of real time have passed since the call
// (std::chrono::steady_clock, never the virtual clock); it always polls at
// least once. On expiry it counts the wait in the dump's
// search_wait_timeouts, which tools/oracle.py fails a checkpoint on, writes
// one line to stderr with the frame and *what (for the first
// kSearchWaitLines expiries; later ones are only counted) and returns false.
// It does not test Active(): the caller does.
constexpr uint32_t kSearchWaitMs = 5000;
constexpr uint32_t kSearchWaitLines = 10;
bool WaitForSearch(uint32_t deadlineMs, const std::function<bool(std::string* what)>& pending);

// The waits WaitForSearch gave up so far (search_wait_timeouts).
uint32_t SearchWaitTimeouts();

// The probe (platform/replay_probe.h): with IWD2R_PROBE naming frame N,
// OnFrame calls the source after the frame's dump and appends the block to
// probe.txt. src/StateDumpSources.cpp registers the engine's source;
// without one nothing is written.
using ProbeSource = void (*)(replay_probe::Frame* out);
void SetProbeSource(ProbeSource source);

}
