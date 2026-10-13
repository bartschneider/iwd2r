#define NOMINMAX
#include "oracle/replay/recorder_script.h"

#include "platform/script_core.h"

#include <mutex>
#include <vector>

namespace {

// Advance runs on the original's main thread, the input polls on its AI
// thread.
std::mutex g_mutex;
std::vector<platform::script::Command> g_commands;
platform::script::Player g_player;

class Bridge : public platform::script::Effects {
public:
    explicit Bridge(recorder::ScriptEvents& events)
        : events_(events)
    {
    }
    void Shot(const std::string& name, int line) override { events_.Shot(name, line); }
    void Quit(int line) override { events_.Quit(line); }
    void Checkpoint(const std::string& name, int line) override { events_.Checkpoint(name, line); }

private:
    recorder::ScriptEvents& events_;
};

}

namespace recorder {

bool LoadScript(const std::string& text, std::string* error, size_t* commandCount)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<platform::script::Command> commands;
    if (!platform::script::Parse(text, commands, *error))
        return false;
    *commandCount = commands.size();
    g_commands = std::move(commands);
    return true;
}

void StartScript(int width, int height)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_player.Start(g_commands, width, height);
}

bool AdvanceScript(ScriptEvents& events)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    Bridge bridge(events);
    return g_player.Advance(bridge);
}

bool ScriptRunning()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_player.Running();
}

int ScriptLine()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_player.Line();
}

bool ScriptCursor(int* x, int* y)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    POINT point;
    if (!g_player.InjectedCursor(&point))
        return false;
    *x = static_cast<int>(point.x);
    *y = static_cast<int>(point.y);
    return true;
}

bool ScriptKey(int vk, short* state)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    SHORT value = 0;
    if (!g_player.InjectedKey(vk, &value))
        return false;
    *state = value;
    return true;
}

}
