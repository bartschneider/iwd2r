// iwd2r. Distributed under LICENSE.md.
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
//                 digit
//   type TEXT     each letter, digit or space of TEXT as a key press
//   shot NAME     write the current primary surface as NAME.png into
//                 IWD2R_SHOT_DIR (default: the working directory, which is
//                 the install directory once the engine has started)
//   quit          post WM_CLOSE
//
// The script advances once per presented frame. While it runs, the injected
// cursor replaces the real mouse and injected keys read as down before the
// real keyboard; once it ends, real input takes over again. A press or a
// cursor move is held until the engine has polled it at least once (bounded
// by kObserveTimeoutFrames), so the engine's input thread cannot miss it.
#pragma once

#include "compat/win_types.h"

#include <string>
#include <vector>

namespace platform::script {

enum class Verb { Wait,
    Click,
    RClick,
    Move,
    Key,
    Type,
    Shot,
    Quit };

struct Command {
    Verb verb = Verb::Wait;
    int line = 0;
    int x = 0; // wait: the frame count
    int y = 0;
    int vk = 0; // key
    std::string text; // type, shot
};

// Frames a press, release or move waits for the engine to poll it before
// the script moves on regardless.
constexpr int kObserveTimeoutFrames = 60;

// The virtual key for a name: a VK_* name without the prefix (ESCAPE,
// RETURN, SPACE, F1, LEFT...), one letter or one digit; case-insensitive.
// 0 when unknown.
int VKForName(const std::string& name);

// Parses a script. On an unknown or malformed line returns false with error
// set to "line N: <reason>".
bool Parse(const std::string& text, std::vector<Command>& commands, std::string& error);

// Starts running already parsed commands (replacing any running script).
void Start(std::vector<Command> commands);

// Reads and parses the file named by IWD2R_SCRIPT, if set, and starts it.
// On a read or parse error reports one line through platform::ReportFatal
// and exits the process with code 5.
void StartFromEnvironment();

// Stops the running script and clears the injected input.
void Stop();

// Runs the script for one presented frame. Called by the window pump.
void Advance();

// The external control channel, active only when IWD2R_CONTROL_DIR is set:
// for every request file shot-<NAME>.req in that directory, writes the
// primary surface as <NAME>.png into IWD2R_SHOT_DIR (as the shot command
// does) and deletes the request. A request made before the first frame
// waits for it. Called by the window pump once per presented frame; it
// injects no input, so an outside driver can use it alongside real input.
void PollControl();

// True while a started script has commands left.
bool Running();

// The injected cursor while a script runs; false otherwise.
bool InjectedCursor(POINT* point);

// The injected state of vk (0x8000 down, plus 0x0001 on the first poll
// after the press) when the script holds it down; false otherwise.
bool InjectedKey(int vk, SHORT* state);

}
