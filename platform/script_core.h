// The scenario format shared by every driver: the parser and the frame-by-
// frame player, with no dependency on the rest of the platform layer, so the
// replay recorder (oracle/replay/recorder.cpp) compiles the same source into
// its x86 DLL. platform/script.h describes the format; the engine drives a
// Player from its window pump, the recorder from the original's frame.
#pragma once

#include "compat/win_types.h"

#include <set>
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
    Quit,
    Origin,
    Checkpoint };

struct Command {
    Verb verb = Verb::Wait;
    int line = 0;
    int x = 0; // wait: the frame count
    int y = 0;
    int vk = 0; // key
    std::vector<int> mods; // key: the CTRL/SHIFT/ALT held around the press, written order
    std::string text; // type, shot, checkpoint
    bool ui = true; // move, click, rclick: X Y in UI coordinates; origin: the mode set
};

// A point in the original's 800x600 UI coordinates on a screen of
// screenWidth x screenHeight engine pixels: offset by the centred UI's
// corner ((w - 800) / 2, (h - 600) / 2), and doubled at 1600x1200 and
// 2048x1536, where the original draws the UI double size.
POINT UiToScreen(int x, int y, int screenWidth, int screenHeight);

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

// What the instant steps do; each driver supplies its own.
class Effects {
public:
    virtual ~Effects() = default;
    virtual void Shot(const std::string& name, int line) = 0;
    virtual void Quit(int line) = 0;
    virtual void Checkpoint(const std::string& name, int line) = 0;
};

// Runs parsed commands, one Advance per frame. Not thread-safe: a driver
// whose input is polled on other threads locks around every call.
class Player {
public:
    // Replaces any running script. Screen coordinates of UI points are
    // computed for a screen of screenWidth x screenHeight; the cursor starts
    // at its centre.
    void Start(const std::vector<Command>& commands, int screenWidth, int screenHeight);
    // Stops the script and clears the injected input.
    void Stop();
    // Runs the script for one frame. Returns true on the frame it finishes.
    bool Advance(Effects& effects);
    // True while a started script has steps left.
    bool Running() const;
    // The injected cursor while a script runs; false otherwise.
    bool InjectedCursor(POINT* point);
    // The injected state of vk (0x8000 down, plus 0x0001 on the first poll
    // after the press) when the script holds it down; false otherwise.
    bool InjectedKey(int vk, SHORT* state);
    // The script line of the step in progress, 0 when none.
    int Line() const;

private:
    enum class Action { Wait,
        SetCursor,
        Press,
        Release,
        Shot,
        Quit,
        Checkpoint };

    struct Step {
        Action action = Action::Wait;
        int frames = 0;
        bool observe = false;
        int x = 0;
        int y = 0;
        int vk = 0;
        std::string name;
        int line = 0;
    };

    static void AddKey(std::vector<Step>& steps, int vk, int line, bool shift,
        const std::vector<int>& mods = {});
    static std::vector<Step> Expand(const std::vector<Command>& commands, int screenWidth, int screenHeight);
    bool Begin(const Step& step, Effects& effects);

    std::vector<Step> steps_;
    size_t next_ = 0;
    bool running_ = false;
    bool inStep_ = false;
    int framesLeft_ = 0;
    int framesWaited_ = 0;
    bool observed_ = false;
    POINT cursor_ = { 0, 0 };
    std::set<int> down_;
    std::set<int> fresh_; // pressed and not yet polled
};

}
