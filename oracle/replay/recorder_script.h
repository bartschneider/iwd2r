// The recorder's view of the scenario player (platform/script_core.h). Plain
// types only: recorder.cpp includes <windows.h>, script_core.h includes
// compat/win_types.h, and the two must not meet in one file.
#pragma once

#include <cstddef>
#include <string>

namespace recorder {

// What the scenario's instant steps do in the recorder.
class ScriptEvents {
public:
    virtual ~ScriptEvents() = default;
    virtual void Shot(const std::string& name, int line) = 0;
    virtual void Quit(int line) = 0;
    virtual void Checkpoint(const std::string& name, int line) = 0;
};

// Parses scenario text with the engine's own parser. On failure returns
// false with error set to "line N: <reason>".
bool LoadScript(const std::string& text, std::string* error, size_t* commandCount);

// Starts the loaded script for a screen of width x height pixels (UI
// coordinates are centred in it as in the engine).
void StartScript(int width, int height);

// Runs one frame of the script; true on the frame it finishes.
bool AdvanceScript(ScriptEvents& events);

bool ScriptRunning();

// The script line in progress, 0 when none.
int ScriptLine();

// The scenario cursor in client pixels while the script runs.
bool ScriptCursor(int* x, int* y);

// The scenario's state of a virtual key (GetAsyncKeyState's value) when it
// holds the key down.
bool ScriptKey(int vk, short* state);

}
