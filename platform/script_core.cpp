#define NOMINMAX
#include "platform/script_core.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <map>
#include <sstream>

namespace {

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

// The modifiers `key` accepts before the key name (`key CTRL+SHIFT+X`); the
// parser refuses ALT+F4 outright (it would close the game window, not feed
// the engine).
const NamedKey kModifiers[] = {
    { "CTRL", VK_CONTROL },
    { "SHIFT", VK_SHIFT },
    { "ALT", VK_MENU },
};

int ModifierForName(const std::string& name)
{
    const std::string upper = Upper(name);
    for (const NamedKey& mod : kModifiers) {
        if (upper == mod.name)
            return mod.vk;
    }
    return 0;
}

// `type` maps each punctuation and symbol to one key press on a US layout:
// the key and whether SHIFT is held around the press (letters, digits and
// space are handled inline in Expand; the original's edit boxes translate
// the same main-row keys with their own US-layout tables, so what lands in
// the game is what the scenario says on that one layout).
struct TypeKey {
    char ch;
    int vk;
    bool shift;
};

const TypeKey kTypeKeys[] = {
    { ')', '0', true }, { '!', '1', true }, { '@', '2', true },
    { '#', '3', true }, { '$', '4', true }, { '%', '5', true },
    { '^', '6', true }, { '&', '7', true }, { '*', '8', true },
    { '(', '9', true },
    { ';', VK_OEM_1, false }, { ':', VK_OEM_1, true },
    { '=', VK_OEM_PLUS, false }, { '+', VK_OEM_PLUS, true },
    { ',', VK_OEM_COMMA, false }, { '<', VK_OEM_COMMA, true },
    { '-', VK_OEM_MINUS, false }, { '_', VK_OEM_MINUS, true },
    { '.', VK_OEM_PERIOD, false }, { '>', VK_OEM_PERIOD, true },
    { '/', VK_OEM_2, false }, { '?', VK_OEM_2, true },
    { '`', VK_OEM_3, false }, { '~', VK_OEM_3, true },
    { '[', VK_OEM_4, false }, { '{', VK_OEM_4, true },
    { '\\', VK_OEM_5, false }, { '|', VK_OEM_5, true },
    { ']', VK_OEM_6, false }, { '}', VK_OEM_6, true },
    { '\'', VK_OEM_7, false }, { '"', VK_OEM_7, true },
};

std::vector<std::string> SplitOn(const std::string& s, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = s.find(sep); i != std::string::npos;
         i = s.find(sep, start)) {
        out.push_back(s.substr(start, i - start));
        start = i + 1;
    }
    out.push_back(s.substr(start));
    return out;
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

POINT UiToScreen(int x, int y, int screenWidth, int screenHeight)
{
    const bool doubled = (screenWidth == 1600 && screenHeight == 1200) || (screenWidth == 2048 && screenHeight == 1536);
    const int scale = doubled ? 2 : 1;
    POINT pt;
    pt.x = (screenWidth - 800 * scale) / 2 + x * scale;
    pt.y = (screenHeight - 600 * scale) / 2 + y * scale;
    return pt;
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
    bool ui = true;
    // Checkpoint names, upper-cased, and the line each first appeared on.
    std::map<std::string, int> checkpoints;
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
            c.ui = ui;
        } else if (v == "KEY") {
            if (args.size() != 1)
                return fail("key needs one key name");
            c.verb = Verb::Key;
            // `key CTRL+SHIFT+X`: modifiers before the key, joined with '+'
            // (no key name is itself '+', so the split is unambiguous).
            const std::vector<std::string> parts = SplitOn(args[0], '+');
            const std::string& name = parts.back();
            bool alt = false;
            for (size_t i = 0; i + 1 < parts.size(); ++i) {
                const int mod = ModifierForName(parts[i]);
                if (mod == 0)
                    return fail("unknown modifier '" + Lower(parts[i]) + "' (CTRL, SHIFT, ALT)");
                alt = alt || mod == VK_MENU;
                c.mods.push_back(mod);
            }
            if (name.empty())
                return fail("key needs a key name after its modifiers");
            c.vk = VKForName(name);
            if (c.vk == 0)
                return fail("unknown key '" + name + "'");
            if (alt && Upper(name) == "F4")
                return fail("alt+f4 closes the game window; the scenarios cannot");
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
                const unsigned char u = static_cast<unsigned char>(ch);
                if (u < 0x20 || u > 0x7e)
                    return fail(std::string("type supports printable ASCII characters, not '") + ch + "'");
            }
        } else if (v == "SHOT") {
            c.verb = Verb::Shot;
            if (args.size() != 1)
                return fail("shot needs one name");
            c.text = args[0];
            if (c.text.find_first_of("/\\:") != std::string::npos)
                return fail("shot name must not contain a path");
        } else if (v == "CHECKPOINT") {
            c.verb = Verb::Checkpoint;
            if (args.size() != 1)
                return fail("checkpoint needs one name");
            c.text = args[0];
            // It names a file, checkpoint-<NAME>.json.
            for (char ch : c.text) {
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-')
                    return fail("checkpoint name must be letters, digits, _ or -");
            }
            // Names are compared case-insensitively, as the dump files they
            // name may land on a case-insensitive file system.
            const auto first = checkpoints.emplace(Upper(c.text), line);
            if (!first.second)
                return fail("duplicate checkpoint '" + c.text + "' (first on line " + std::to_string(first.first->second) + ")");
        } else if (v == "ORIGIN") {
            c.verb = Verb::Origin;
            const std::string mode = args.size() == 1 ? Upper(args[0]) : std::string();
            if (mode != "UI" && mode != "SCREEN")
                return fail("origin needs ui or screen");
            ui = mode == "UI";
            c.ui = ui;
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

void Player::AddKey(std::vector<Step>& steps, int vk, int line, bool shift,
                    const std::vector<int>& mods)
{
    Step s;
    s.line = line;
    for (int mod : mods) {
        s.action = Action::Press;
        s.vk = mod;
        s.frames = 1;
        steps.push_back(s);
    }
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
    // The modifiers come up in reverse order, the last one first.
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
        s.action = Action::Release;
        s.vk = *it;
        s.frames = 1;
        s.observe = false;
        steps.push_back(s);
    }
}

std::vector<Player::Step> Player::Expand(const std::vector<Command>& commands, int screenWidth, int screenHeight)
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
            if (c.ui) {
                const POINT pt = UiToScreen(c.x, c.y, screenWidth, screenHeight);
                s.x = pt.x;
                s.y = pt.y;
            } else {
                s.x = c.x;
                s.y = c.y;
            }
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
            AddKey(steps, c.vk, c.line, false, c.mods);
            break;
        case Verb::Type:
            for (char ch : c.text) {
                const unsigned char u = static_cast<unsigned char>(ch);
                if (ch == ' ')
                    AddKey(steps, VK_SPACE, c.line, false);
                else if (std::isalnum(u))
                    AddKey(steps, std::toupper(u), c.line, std::isupper(u) != 0);
                else {
                    const TypeKey* key = nullptr;
                    for (const TypeKey& k : kTypeKeys) {
                        if (k.ch == ch)
                            key = &k;
                    }
                    AddKey(steps, key->vk, c.line, key->shift);
                }
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
        case Verb::Checkpoint:
            s.action = Action::Checkpoint;
            s.name = c.text;
            steps.push_back(s);
            break;
        case Verb::Origin:
            break;
        }
    }
    return steps;
}

// Applies a step's effect. Returns false for the instant steps that need no
// frames (shot, quit, checkpoint, wait 0).
bool Player::Begin(const Step& step, Effects& effects)
{
    switch (step.action) {
    case Action::Wait:
        break;
    case Action::SetCursor:
        cursor_.x = step.x;
        cursor_.y = step.y;
        break;
    case Action::Press:
        down_.insert(step.vk);
        fresh_.insert(step.vk);
        break;
    case Action::Release:
        down_.erase(step.vk);
        fresh_.erase(step.vk);
        break;
    case Action::Shot:
        effects.Shot(step.name, step.line);
        return false;
    case Action::Quit:
        effects.Quit(step.line);
        return false;
    case Action::Checkpoint:
        effects.Checkpoint(step.name, step.line);
        return false;
    }
    framesLeft_ = step.frames;
    framesWaited_ = 0;
    observed_ = !step.observe;
    return step.frames > 0 || step.observe;
}

void Player::Start(const std::vector<Command>& commands, int screenWidth, int screenHeight)
{
    steps_ = Expand(commands, screenWidth, screenHeight);
    next_ = 0;
    inStep_ = false;
    down_.clear();
    fresh_.clear();
    cursor_.x = screenWidth / 2;
    cursor_.y = screenHeight / 2;
    running_ = !steps_.empty();
}

void Player::Stop()
{
    steps_.clear();
    next_ = 0;
    inStep_ = false;
    running_ = false;
    down_.clear();
    fresh_.clear();
}

bool Player::Advance(Effects& effects)
{
    if (!running_)
        return false;
    if (inStep_) {
        if (framesLeft_ > 0)
            framesLeft_--;
        framesWaited_++;
        const bool observed = observed_ || framesWaited_ >= kObserveTimeoutFrames;
        if (framesLeft_ > 0 || !observed)
            return false;
        inStep_ = false;
        next_++;
    }
    while (next_ < steps_.size()) {
        if (Begin(steps_[next_], effects)) {
            inStep_ = true;
            return false;
        }
        next_++;
    }
    running_ = false;
    down_.clear();
    fresh_.clear();
    return true;
}

bool Player::Running() const
{
    return running_;
}

bool Player::InjectedCursor(POINT* point)
{
    if (!running_)
        return false;
    if (inStep_ && steps_[next_].action == Action::SetCursor)
        observed_ = true;
    *point = cursor_;
    return true;
}

bool Player::InjectedKey(int vk, SHORT* state)
{
    if (!running_)
        return false;
    if (inStep_) {
        const Step& step = steps_[next_];
        if ((step.action == Action::Press || step.action == Action::Release) && step.vk == vk)
            observed_ = true;
    }
    if (down_.count(vk) == 0)
        return false;
    SHORT s = static_cast<SHORT>(-32768); // 0x8000: down now
    if (fresh_.erase(vk) != 0)
        s = static_cast<SHORT>(s | 1); // pressed since the last poll
    *state = s;
    return true;
}

int Player::Line() const
{
    return running_ && next_ < steps_.size() ? steps_[next_].line : 0;
}

}
