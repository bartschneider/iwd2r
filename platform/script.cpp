// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/script.h"

#include "platform/data_dir.h"
#include "platform/png.h"
#include "platform/present.h"
#include "platform/window.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <sstream>

namespace {

using platform::script::Command;
using platform::script::Verb;

struct NamedKey {
    const char* name;
    int vk;
};

// The keys of the window shim's VK table (window_keys.cpp) by their VK_*
// names; letters and digits are handled in VKForName.
const NamedKey kNamedKeys[] = {
    { "BACK", VK_BACK },
    { "TAB", VK_TAB },
    { "RETURN", VK_RETURN },
    { "SHIFT", VK_SHIFT },
    { "CONTROL", VK_CONTROL },
    { "MENU", VK_MENU },
    { "ESCAPE", VK_ESCAPE },
    { "SPACE", VK_SPACE },
    { "PRIOR", VK_PRIOR },
    { "NEXT", VK_NEXT },
    { "END", VK_END },
    { "HOME", VK_HOME },
    { "LEFT", VK_LEFT },
    { "UP", VK_UP },
    { "RIGHT", VK_RIGHT },
    { "DOWN", VK_DOWN },
    { "SNAPSHOT", VK_SNAPSHOT },
    { "INSERT", VK_INSERT },
    { "DELETE", VK_DELETE },
    { "NUMPAD0", VK_NUMPAD0 },
    { "NUMPAD1", VK_NUMPAD1 },
    { "NUMPAD2", VK_NUMPAD2 },
    { "NUMPAD3", VK_NUMPAD3 },
    { "NUMPAD4", VK_NUMPAD4 },
    { "NUMPAD5", VK_NUMPAD5 },
    { "NUMPAD6", VK_NUMPAD6 },
    { "NUMPAD7", VK_NUMPAD7 },
    { "NUMPAD8", VK_NUMPAD8 },
    { "NUMPAD9", VK_NUMPAD9 },
    { "MULTIPLY", VK_MULTIPLY },
    { "ADD", VK_ADD },
    { "SEPARATOR", VK_SEPARATOR },
    { "SUBTRACT", VK_SUBTRACT },
    { "DECIMAL", VK_DECIMAL },
    { "DIVIDE", VK_DIVIDE },
    { "F1", VK_F1 },
    { "F2", VK_F2 },
    { "F3", VK_F3 },
    { "F4", VK_F4 },
    { "F5", VK_F5 },
    { "F6", VK_F6 },
    { "F7", VK_F7 },
    { "F8", VK_F8 },
    { "F9", VK_F9 },
    { "F10", VK_F10 },
    { "F11", VK_F11 },
    { "F12", VK_F12 },
    { "NUMLOCK", VK_NUMLOCK },
    { "LSHIFT", VK_LSHIFT },
    { "RSHIFT", VK_RSHIFT },
    { "LCONTROL", VK_LCONTROL },
    { "RCONTROL", VK_RCONTROL },
    { "LMENU", VK_LMENU },
    { "RMENU", VK_RMENU },
    { "OEM_1", VK_OEM_1 },
    { "OEM_PLUS", VK_OEM_PLUS },
    { "OEM_COMMA", VK_OEM_COMMA },
    { "OEM_MINUS", VK_OEM_MINUS },
    { "OEM_PERIOD", VK_OEM_PERIOD },
    { "OEM_2", VK_OEM_2 },
    { "OEM_3", VK_OEM_3 },
    { "OEM_4", VK_OEM_4 },
    { "OEM_5", VK_OEM_5 },
    { "OEM_6", VK_OEM_6 },
    { "OEM_7", VK_OEM_7 },
    { "OEM_8", VK_OEM_8 },
};

std::string Upper(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool ParseInt(const std::string& token, int& value)
{
    if (token.empty())
        return false;
    for (char c : token) {
        if (!std::isdigit(static_cast<unsigned char>(c)))
            return false;
    }
    errno = 0;
    const long v = std::strtol(token.c_str(), nullptr, 10);
    if (errno != 0 || v > INT_MAX)
        return false;
    value = static_cast<int>(v);
    return true;
}

// One step of a running script: an effect applied when it starts, then a
// hold of some frames and, optionally, until the engine has polled it.
enum class Action { Wait,
    SetCursor,
    Press,
    Release,
    Shot,
    Quit };

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

struct Runtime {
    std::mutex mutex;
    std::vector<Step> steps;
    size_t next = 0;
    bool running = false;
    bool inStep = false;
    int framesLeft = 0;
    int framesWaited = 0;
    bool observed = false;
    POINT cursor = { 0, 0 };
    std::set<int> down;
    std::set<int> fresh; // pressed and not yet polled
};

Runtime& R()
{
    static Runtime runtime;
    return runtime;
}

void AddKey(std::vector<Step>& steps, int vk, int line, bool shift)
{
    Step s;
    s.line = line;
    if (shift) {
        s.action = Action::Press;
        s.vk = VK_SHIFT;
        s.frames = 1;
        steps.push_back(s);
    }
    s.action = Action::Press;
    s.vk = vk;
    s.frames = 2;
    s.observe = true;
    steps.push_back(s);
    s.action = Action::Release;
    steps.push_back(s);
    if (shift) {
        s.action = Action::Release;
        s.vk = VK_SHIFT;
        s.frames = 1;
        s.observe = false;
        steps.push_back(s);
    }
}

std::vector<Step> Expand(const std::vector<Command>& commands)
{
    std::vector<Step> steps;
    for (const Command& c : commands) {
        Step s;
        s.line = c.line;
        switch (c.verb) {
        case Verb::Wait:
            s.action = Action::Wait;
            s.frames = c.x;
            steps.push_back(s);
            break;
        case Verb::Move:
        case Verb::Click:
        case Verb::RClick:
            s.action = Action::SetCursor;
            s.x = c.x;
            s.y = c.y;
            s.frames = 1;
            s.observe = true;
            steps.push_back(s);
            if (c.verb != Verb::Move) {
                s.action = Action::Press;
                s.vk = c.verb == Verb::Click ? VK_LBUTTON : VK_RBUTTON;
                s.frames = 2;
                steps.push_back(s);
                s.action = Action::Release;
                steps.push_back(s);
            }
            break;
        case Verb::Key:
            AddKey(steps, c.vk, c.line, false);
            break;
        case Verb::Type:
            for (char ch : c.text) {
                const unsigned char u = static_cast<unsigned char>(ch);
                if (ch == ' ')
                    AddKey(steps, VK_SPACE, c.line, false);
                else
                    AddKey(steps, std::toupper(u), c.line, std::isupper(u) != 0);
            }
            break;
        case Verb::Shot:
            s.action = Action::Shot;
            s.name = c.text;
            steps.push_back(s);
            break;
        case Verb::Quit:
            s.action = Action::Quit;
            steps.push_back(s);
            break;
        }
    }
    return steps;
}

[[noreturn]] void Fail(const std::string& text)
{
    platform::ReportFatal(text.c_str(), "iwd2r script");
    std::fflush(stdout);
    std::fflush(stderr);
    std::_Exit(5);
}

// Writes the primary surface as <IWD2R_SHOT_DIR>/<name>.png. On failure
// returns false with why set.
bool WriteShot(const std::string& name, std::string& path, std::string& why)
{
    std::string dir;
    if (const char* env = std::getenv("IWD2R_SHOT_DIR"))
        dir = env;
    path = dir;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
        path += path.find('\\') != std::string::npos ? '\\' : '/';
    path += name + ".png";

    const uint32_t* pixels = nullptr;
    int w = 0;
    int h = 0;
    int pitch = 0;
    if (!platform::present::PrimaryFrame(&pixels, &w, &h, &pitch)) {
        why = "no primary surface yet";
        return false;
    }
    if (!platform::WritePng(path.c_str(), pixels, w, h, pitch)) {
        why = "cannot write " + path;
        return false;
    }
    why = std::to_string(w) + "x" + std::to_string(h);
    return true;
}

void TakeShot(const Step& step)
{
    std::string path;
    std::string why;
    if (!WriteShot(step.name, path, why))
        Fail("line " + std::to_string(step.line) + ": shot " + step.name + ": " + why);
    std::printf("script: line %d: shot %s (%s)\n", step.line, path.c_str(), why.c_str());
    std::fflush(stdout);
}

// Applies a step's effect. Returns false for the instant steps that need no
// frames (shot, quit, wait 0).
bool Begin(Runtime& r, const Step& step)
{
    switch (step.action) {
    case Action::Wait:
        break;
    case Action::SetCursor:
        r.cursor.x = step.x;
        r.cursor.y = step.y;
        break;
    case Action::Press:
        r.down.insert(step.vk);
        r.fresh.insert(step.vk);
        break;
    case Action::Release:
        r.down.erase(step.vk);
        r.fresh.erase(step.vk);
        break;
    case Action::Shot:
        TakeShot(step);
        return false;
    case Action::Quit:
        platform::window::EnqueueMessage(WM_CLOSE, 0, 0);
        return false;
    }
    r.framesLeft = step.frames;
    r.framesWaited = 0;
    r.observed = !step.observe;
    return step.frames > 0 || step.observe;
}

}

namespace platform::script {

int VKForName(const std::string& name)
{
    const std::string upper = Upper(name);
    if (upper.size() == 1) {
        const unsigned char c = static_cast<unsigned char>(upper[0]);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            return c;
        return 0;
    }
    for (const NamedKey& key : kNamedKeys) {
        if (upper == key.name)
            return key.vk;
    }
    return 0;
}

bool Parse(const std::string& text, std::vector<Command>& commands, std::string& error)
{
    commands.clear();
    std::istringstream in(text);
    std::string raw;
    int line = 0;
    auto fail = [&](const std::string& why) {
        error = "line " + std::to_string(line) + ": " + why;
        commands.clear();
        return false;
    };
    while (std::getline(in, raw)) {
        ++line;
        const size_t hash = raw.find('#');
        if (hash != std::string::npos)
            raw.erase(hash);
        std::istringstream words(raw);
        std::string verb;
        if (!(words >> verb))
            continue;
        std::vector<std::string> args;
        for (std::string w; words >> w;)
            args.push_back(w);

        Command c;
        c.line = line;
        const std::string v = Upper(verb);
        if (v == "WAIT") {
            c.verb = Verb::Wait;
            if (args.size() != 1 || !ParseInt(args[0], c.x))
                return fail("wait needs one frame count");
        } else if (v == "CLICK" || v == "RCLICK" || v == "MOVE") {
            c.verb = v == "CLICK" ? Verb::Click : v == "RCLICK" ? Verb::RClick
                                                                : Verb::Move;
            if (args.size() != 2 || !ParseInt(args[0], c.x) || !ParseInt(args[1], c.y))
                return fail(Lower(verb) + " needs X Y (non-negative integers)");
        } else if (v == "KEY") {
            if (args.size() != 1)
                return fail("key needs one key name");
            c.verb = Verb::Key;
            c.vk = VKForName(args[0]);
            if (c.vk == 0)
                return fail("unknown key '" + args[0] + "'");
        } else if (v == "TYPE") {
            c.verb = Verb::Type;
            // The text after the verb, single spaces kept.
            const size_t start = raw.find_first_not_of(" \t", raw.find_first_not_of(" \t") + verb.size());
            if (start != std::string::npos)
                c.text = raw.substr(start);
            while (!c.text.empty() && std::isspace(static_cast<unsigned char>(c.text.back())))
                c.text.pop_back();
            if (c.text.empty())
                return fail("type needs text");
            for (char ch : c.text) {
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != ' ')
                    return fail(std::string("type supports letters, digits and spaces, not '") + ch + "'");
            }
        } else if (v == "SHOT") {
            c.verb = Verb::Shot;
            if (args.size() != 1)
                return fail("shot needs one name");
            c.text = args[0];
            if (c.text.find_first_of("/\\:") != std::string::npos)
                return fail("shot name must not contain a path");
        } else if (v == "QUIT") {
            c.verb = Verb::Quit;
            if (!args.empty())
                return fail("quit takes no arguments");
        } else {
            return fail("unknown command '" + verb + "'");
        }
        commands.push_back(c);
    }
    return true;
}

void Start(std::vector<Command> commands)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.steps = Expand(commands);
    r.next = 0;
    r.inStep = false;
    r.down.clear();
    r.fresh.clear();
    const SIZE screen = platform::present::ScreenSize();
    r.cursor.x = screen.cx / 2;
    r.cursor.y = screen.cy / 2;
    r.running = !r.steps.empty();
}

void StartFromEnvironment()
{
    const char* path = std::getenv("IWD2R_SCRIPT");
    if (!path || !*path)
        return;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        Fail(std::string(path) + ": cannot read the script");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<Command> commands;
    std::string error;
    if (!Parse(text, commands, error))
        Fail(std::string(path) + ": " + error);
    std::printf("script: %s (%zu commands)\n", path, commands.size());
    std::fflush(stdout);
    Start(std::move(commands));
}

void Stop()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.steps.clear();
    r.next = 0;
    r.inStep = false;
    r.running = false;
    r.down.clear();
    r.fresh.clear();
}

void Advance()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    if (!r.running)
        return;
    if (r.inStep) {
        if (r.framesLeft > 0)
            r.framesLeft--;
        r.framesWaited++;
        const bool observed = r.observed || r.framesWaited >= kObserveTimeoutFrames;
        if (r.framesLeft > 0 || !observed)
            return;
        r.inStep = false;
        r.next++;
    }
    while (r.next < r.steps.size()) {
        if (Begin(r, r.steps[r.next])) {
            r.inStep = true;
            return;
        }
        r.next++;
    }
    r.running = false;
    r.down.clear();
    r.fresh.clear();
    std::printf("script: finished\n");
    std::fflush(stdout);
}

bool Running()
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    return r.running;
}

bool InjectedCursor(POINT* point)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    if (!r.running)
        return false;
    if (r.inStep && r.steps[r.next].action == Action::SetCursor)
        r.observed = true;
    *point = r.cursor;
    return true;
}

bool InjectedKey(int vk, SHORT* state)
{
    Runtime& r = R();
    std::lock_guard<std::mutex> lock(r.mutex);
    if (!r.running)
        return false;
    if (r.inStep) {
        const Step& step = r.steps[r.next];
        if ((step.action == Action::Press || step.action == Action::Release) && step.vk == vk)
            r.observed = true;
    }
    if (r.down.count(vk) == 0)
        return false;
    SHORT s = static_cast<SHORT>(-32768); // 0x8000: down now
    if (r.fresh.erase(vk) != 0)
        s = static_cast<SHORT>(s | 1); // pressed since the last poll
    *state = s;
    return true;
}

void PollControl()
{
    const char* dir = std::getenv("IWD2R_CONTROL_DIR");
    if (!dir || !*dir)
        return;
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<fs::path> requests;
    for (fs::directory_iterator it(fs::path(dir), ec), end; !ec && it != end; it.increment(ec)) {
        const std::string file = it->path().filename().string();
        if (file.size() > 8 && file.compare(0, 5, "shot-") == 0 && file.compare(file.size() - 4, 4, ".req") == 0)
            requests.push_back(it->path());
    }
    for (const fs::path& request : requests) {
        const std::string file = request.filename().string();
        const std::string name = file.substr(5, file.size() - 9);
        std::string path;
        std::string why;
        if (WriteShot(name, path, why)) {
            std::printf("control: shot %s (%s)\n", path.c_str(), why.c_str());
        } else if (why == "no primary surface yet") {
            continue; // retried on the next frame
        } else {
            std::printf("control: shot %s failed: %s\n", name.c_str(), why.c_str());
        }
        std::fflush(stdout);
        fs::remove(request, ec);
    }
}

}
