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
//     update runs after the frame's render, never beside it
//     (platform/replay_clock.h, "The order inside a frame");
//   - time() (and CTime) reads a virtual wall clock derived from it, UTC;
//   - frame 1 seeds the main thread's rand with IWD2R_SEED (default 1), as
//     the recorder runs the original's srand;
//   - input comes only from the scenario (IWD2R_SCRIPT), advanced once per
//     frame from OnFrame: keys up and the last cursor outside it;
//   - focus and activation never reach the engine (the recorder's focus
//     guard), and message boxes take their first button unseen;
//   - the stall count (ai_stalled_frames) is kept the same way.
//
// OnFrame writes frame-<N>.json every IWD2R_DUMP_EVERY frames (default 30,
// 0 for none), checkpoint-<NAME>.json at the scenario's checkpoint lines,
// and status.txt (frame, script_line, state), all in IWD2R_STATE_DUMP.
#pragma once

#include "platform/win32_api.h"

#include <cstdint>
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

}
