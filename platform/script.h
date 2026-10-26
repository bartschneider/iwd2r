// Internal: scripted input and screenshots for the window shim. A script
// (IWD2R_SCRIPT) is a text file of commands, one per line; '#' starts a
// comment, verbs are case-insensitive:
//
//   wait N        advance N presented frames
//   click X Y     move to X,Y (game coordinates), hold the left button 2
//                 frames, release, settle 2 frames
//   rclick X Y    the same with the right button
//   move X Y      move the cursor to X,Y
//   key NAME      press and release a key over 2 frames; NAME is a VK name
//                 without VK_ (ESCAPE, RETURN, SPACE, F1...) or one letter or
//                 digit; CTRL, SHIFT and ALT may be written before it, joined
//                 with '+' (key CTRL+SHIFT+X), pressed in written order and
//                 released in reverse around the key, with the same frame
//                 phases SHIFT uses for a capital; ALT+F4 is refused (it
//                 would close the window)
//   type TEXT     each character of TEXT (any printable ASCII) as one key
//                 press on a US layout; capitals and shifted symbols have
//                 SHIFT held around the press
//   shot NAME     write the current primary surface as NAME.png into
//                 IWD2R_SHOT_DIR (default: the working directory, which is
//                 the install directory once the engine has started)
//   checkpoint NAME
//                 a named point for state dumps: checkpoint-NAME.json is
//                 written there, by the replay recorder in the original
//                 (oracle/replay) and by our engine's replay mode
//                 (platform/replay.h, IWD2R_STATE_DUMP); outside the replay
//                 mode the script does nothing with it (a real-input
//                 driver asks the control channel instead, PollControl)
//   quit          post WM_CLOSE
//   origin ui|screen
//                 how later X Y are read: ui (the default) is the
//                 original's 800x600 UI, centred on larger screens (and
//                 drawn double size at 1600x1200 and 2048x1536), so one
//                 script clicks the same buttons at every resolution;
//                 screen is the engine's screen pixels
//
// The script advances once per presented frame; in the replay mode
// (platform/replay.h) once per engine frame instead, from frame 1, as the
// replay recorder advances it in the original. While it runs, the injected
// cursor replaces the real mouse and injected keys read as down before the
// real keyboard; once it ends, real input takes over again. A press or a
// cursor move is held until the engine has polled it at least once (bounded
// by kObserveTimeoutFrames), so the engine's input thread cannot miss it.
#pragma once

#include "platform/script_core.h"

#include <string>
#include <vector>

namespace platform::script {

// Starts running already parsed commands (replacing any running script).
void Start(std::vector<Command> commands);

// Reads and parses the file named by IWD2R_SCRIPT, if set, and starts it
// (in the replay mode: keeps it for StartPending). On a read or parse error
// reports one line through platform::ReportFatal and exits the process with
// code 5.
void StartFromEnvironment();

// Replay mode: starts the script StartFromEnvironment kept, for a screen of
// width x height. True when there was one.
bool StartPending(int width, int height);

// Stops the running script and clears the injected input.
void Stop();

// Runs the script for one frame. Called by the window pump once per
// presented frame, or by the replay mode once per engine frame. True on the
// frame it finishes.
bool Advance();

// The script line of the step in progress, 0 when none.
int Line();

// The external control channel, active only when IWD2R_CONTROL_DIR is set:
// for every request file shot-<NAME>.req in that directory, writes the
// primary surface as <NAME>.png into IWD2R_SHOT_DIR (as the shot command
// does) and deletes the request; for a request file frames.req, writes the
// number of presented frames into frames.txt and deletes the request; for
// state-<NAME>.req (NAME a checkpoint name), writes the engine's state now
// as checkpoint-<NAME>.json into IWD2R_SHOT_DIR, in the replay dump's
// layout (platform/state_dump.h: frame is the presented frame count, the
// replay mode's own counts are 0), and deletes the request; for input.req,
// writes platform::window::ButtonPolls() into input.txt and deletes the
// request (the pump also looks after each such poll, not only once per
// frame). A shot request
// made before the first frame waits for it. Called by the window pump once
// per presented frame; it injects no input, so an outside driver can use
// it alongside real input (the real-input drivers ask for a state dump at
// every checkpoint line, tools/realinput.py).
void PollControl();

// True while a started script has commands left.
bool Running();

// The injected cursor while a script runs; false otherwise.
bool InjectedCursor(POINT* point);

// The injected state of vk (0x8000 down, plus 0x0001 on the first poll
// after the press) when the script holds it down; false otherwise.
bool InjectedKey(int vk, SHORT* state);

}
