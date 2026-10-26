#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "platform/present.h"
#include "platform/script.h"
#include "platform/win32_api.h"
#include "platform/window.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using platform::script::Command;
using platform::script::Verb;

namespace {

std::vector<Command> ParseOk(const char* text)
{
    std::vector<Command> commands;
    std::string error;
    INFO(text);
    REQUIRE(platform::script::Parse(text, commands, error));
    CHECK(error.empty());
    return commands;
}

std::string ParseError(const char* text)
{
    std::vector<Command> commands;
    std::string error;
    INFO(text);
    REQUIRE_FALSE(platform::script::Parse(text, commands, error));
    CHECK(commands.empty());
    return error;
}

// GetAsyncKeyState values: down (0x8000), and down plus pressed since the
// last poll (0x8001).
const SHORT kDown = -32768;
const SHORT kPressed = -32767;

// The injected state of vk, or 0 when not injected.
SHORT Key(int vk)
{
    SHORT state = 0;
    return platform::script::InjectedKey(vk, &state) ? state : SHORT(0);
}

POINT Cursor()
{
    POINT p = { -1, -1 };
    REQUIRE(platform::script::InjectedCursor(&p));
    return p;
}

void SetEnv(const char* name, const std::string& value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    if (value.empty())
        unsetenv(name);
    else
        setenv(name, value.c_str(), 1);
#endif
}

void SetShotDir(const std::string& dir) { SetEnv("IWD2R_SHOT_DIR", dir); }

}

TEST_CASE("script parser reads every verb", "[script]")
{
    const std::vector<Command> c = ParseOk(
        "# comment line\n"
        "\n"
        "WAIT 60   # trailing comment\n"
        "click 10 20\r\n"
        "RClick 0 599\n"
        "  move 400 300\n"
        "key escape\n"
        "key F1\n"
        "key a\n"
        "key 7\n"
        "type Ab 9\n"
        "shot start_screen\n"
        "Quit\n"
        "checkpoint chargen_done\n");
    REQUIRE(c.size() == 12);
    CHECK(c[0].verb == Verb::Wait);
    CHECK(c[0].x == 60);
    CHECK(c[0].line == 3);
    CHECK(c[1].verb == Verb::Click);
    CHECK(c[1].x == 10);
    CHECK(c[1].y == 20);
    CHECK(c[2].verb == Verb::RClick);
    CHECK(c[2].y == 599);
    CHECK(c[3].verb == Verb::Move);
    CHECK(c[3].x == 400);
    CHECK(c[3].line == 6);
    CHECK(c[4].verb == Verb::Key);
    CHECK(c[4].vk == VK_ESCAPE);
    CHECK(c[5].vk == VK_F1);
    CHECK(c[6].vk == 'A');
    CHECK(c[7].vk == '7');
    CHECK(c[8].verb == Verb::Type);
    CHECK(c[8].text == "Ab 9");
    CHECK(c[9].verb == Verb::Shot);
    CHECK(c[9].text == "start_screen");
    CHECK(c[10].verb == Verb::Quit);
    CHECK(c[10].line == 13);
    CHECK(c[11].verb == Verb::Checkpoint);
    CHECK(c[11].text == "chargen_done");
}

TEST_CASE("script key names", "[script]")
{
    CHECK(platform::script::VKForName("RETURN") == VK_RETURN);
    CHECK(platform::script::VKForName("Space") == VK_SPACE);
    CHECK(platform::script::VKForName("f12") == VK_F12);
    CHECK(platform::script::VKForName("z") == 'Z');
    CHECK(platform::script::VKForName("0") == '0');
    CHECK(platform::script::VKForName("VK_ESCAPE") == 0);
    CHECK(platform::script::VKForName("?") == 0);
    CHECK(platform::script::VKForName("") == 0);
}

TEST_CASE("script parser reports errors with line numbers", "[script]")
{
    CHECK(ParseError("wait 1\njump 3\n") == "line 2: unknown command 'jump'");
    CHECK(ParseError("wait\n").rfind("line 1: wait", 0) == 0);
    CHECK(ParseError("wait -1\n").rfind("line 1: wait", 0) == 0);
    CHECK(ParseError("wait 1 2\n").rfind("line 1:", 0) == 0);
    CHECK(ParseError("\n\nclick 10\n").rfind("line 3: click needs X Y", 0) == 0);
    CHECK(ParseError("move 1 y\n").rfind("line 1: move", 0) == 0);
    CHECK(ParseError("key\n").rfind("line 1: key", 0) == 0);
    CHECK(ParseError("key VK_ESCAPE\n") == "line 1: unknown key 'VK_ESCAPE'");
    CHECK(ParseError("type\n").rfind("line 1: type", 0) == 0);
    CHECK(ParseError("type a\xC3\xA9\n").rfind("line 1: type supports", 0) == 0);
    CHECK(ParseError("shot\n").rfind("line 1: shot", 0) == 0);
    CHECK(ParseError("shot ../x\n").rfind("line 1: shot name", 0) == 0);
    CHECK(ParseError("quit now\n").rfind("line 1: quit", 0) == 0);
    CHECK(ParseError("checkpoint\n") == "line 1: checkpoint needs one name");
    CHECK(ParseError("checkpoint a b\n") == "line 1: checkpoint needs one name");
    for (const char* bad : { "checkpoint a\\b\n", "checkpoint c:x\n", "checkpoint ../x\n", "checkpoint a.b\n",
             "checkpoint a*\n", "checkpoint \xC3\xA9\n" })
        CHECK(ParseError(bad) == "line 1: checkpoint name must be letters, digits, _ or -");
    CHECK(ParseOk("checkpoint Party_formation-2\n")[0].text == "Party_formation-2");
    CHECK(ParseError("checkpoint a\nwait 1\ncheckpoint a\n") == "line 3: duplicate checkpoint 'a' (first on line 1)");
    CHECK(ParseError("checkpoint Chargen\ncheckpoint CHARGEN\n") == "line 2: duplicate checkpoint 'CHARGEN' (first on line 1)");
    CHECK(ParseOk("checkpoint a\ncheckpoint a_2\n").size() == 2);
}

TEST_CASE("script checkpoint is parsed and takes no frame", "[script]")
{
    const std::vector<Command> c = ParseOk("Checkpoint start\nwait 1\n");
    REQUIRE(c.size() == 2);
    CHECK(c[0].verb == Verb::Checkpoint);
    CHECK(c[0].text == "start");
    CHECK(c[0].line == 1);

    // The engine does nothing at a checkpoint (the replay recorder dumps
    // there): it is instant, like shot, and the wait after it starts at once.
    platform::script::Start(c);
    platform::script::Advance(); // checkpoint, then wait 1 starts
    CHECK(platform::script::Running());
    platform::script::Advance();
    CHECK_FALSE(platform::script::Running());
}

namespace {

struct RecordingEffects : platform::script::Effects {
    std::vector<std::string> calls;
    void Shot(const std::string& name, int line) override { calls.push_back("shot " + name + " " + std::to_string(line)); }
    void Quit(int line) override { calls.push_back("quit " + std::to_string(line)); }
    void Checkpoint(const std::string& name, int line) override
    {
        calls.push_back("checkpoint " + name + " " + std::to_string(line));
    }
};

}

TEST_CASE("script player calls its driver's effects", "[script]")
{
    platform::script::Player player;
    RecordingEffects effects;
    player.Start(ParseOk("checkpoint a\nwait 1\nshot b\ncheckpoint c\nquit\n"), 800, 600);
    CHECK(player.Line() == 1);
    CHECK_FALSE(player.Advance(effects)); // checkpoint a, then wait 1 starts
    CHECK(effects.calls == std::vector<std::string> { "checkpoint a 1" });
    CHECK(player.Line() == 2);
    CHECK(player.Advance(effects)); // the rest is instant: finished
    CHECK(effects.calls == std::vector<std::string> { "checkpoint a 1", "shot b 3", "checkpoint c 4", "quit 5" });
    CHECK_FALSE(player.Running());
    CHECK(player.Line() == 0);
}

TEST_CASE("script origin selects UI or screen coordinates", "[script]")
{
    const std::vector<Command> c = ParseOk("click 1 2\norigin screen\nmove 3 4\nORIGIN UI\nrclick 5 6\n");
    REQUIRE(c.size() == 5);
    CHECK(c[0].ui);
    CHECK(c[1].verb == Verb::Origin);
    CHECK_FALSE(c[2].ui);
    CHECK(c[4].ui);
    CHECK(ParseError("origin\n") == "line 1: origin needs ui or screen");
    CHECK(ParseError("origin centre\n") == "line 1: origin needs ui or screen");

    // The 800x600 UI centred, or drawn double size at 1600x1200 and
    // 2048x1536.
    POINT p = platform::script::UiToScreen(645, 410, 800, 600);
    CHECK(p.x == 645);
    CHECK(p.y == 410);
    p = platform::script::UiToScreen(645, 410, 1024, 768);
    CHECK(p.x == 645 + 112);
    CHECK(p.y == 410 + 84);
    p = platform::script::UiToScreen(645, 410, 1920, 1080);
    CHECK(p.x == 645 + 560);
    CHECK(p.y == 410 + 240);
    p = platform::script::UiToScreen(645, 410, 1600, 1200);
    CHECK(p.x == 1290);
    CHECK(p.y == 820);
    p = platform::script::UiToScreen(645, 410, 2048, 1536);
    CHECK(p.x == 224 + 1290);
    CHECK(p.y == 168 + 820);

    // Applied when the script starts, from the engine's screen.
    const SIZE before = platform::present::ScreenSize();
    platform::present::SetScreenSize(1366, 768);
    platform::script::Start(ParseOk("move 10 20\norigin screen\nmove 30 40\n"));
    platform::script::Advance();
    CHECK(Cursor().x == 10 + 283);
    CHECK(Cursor().y == 20 + 84);
    platform::script::Advance();
    platform::script::Advance();
    CHECK(Cursor().x == 30);
    CHECK(Cursor().y == 40);
    platform::script::Stop();
    platform::present::SetScreenSize(before.cx, before.cy);
}

TEST_CASE("script run drives the injected cursor and buttons", "[script]")
{
    // At 800x600 UI coordinates are screen coordinates.
    const SIZE before = platform::present::ScreenSize();
    platform::present::SetScreenSize(800, 600);
    platform::script::Start(ParseOk("wait 2\nclick 100 200\nkey RETURN\n"));
    REQUIRE(platform::script::Running());

    // Before the first move: the screen's centre.
    platform::script::Advance(); // starts wait 2
    const SIZE screen = platform::present::ScreenSize();
    CHECK(Cursor().x == screen.cx / 2);
    platform::script::Advance();
    platform::script::Advance(); // wait done; the move starts
    CHECK(Cursor().x == 100); // also marks the move as polled
    CHECK(Cursor().y == 200);
    CHECK(Key(VK_LBUTTON) == 0);

    platform::script::Advance(); // move held 1 frame and polled: press
    CHECK(Key(VK_LBUTTON) == kPressed); // first poll reports the press
    CHECK(Key(VK_LBUTTON) == kDown);
    platform::script::Advance();
    CHECK(Key(VK_LBUTTON) == kDown);
    platform::script::Advance(); // held 2 frames: release
    CHECK(Key(VK_LBUTTON) == 0);
    platform::script::Advance();
    platform::script::Advance(); // settled 2 frames: key press
    CHECK(Key(VK_RETURN) == kPressed);
    platform::script::Advance();
    platform::script::Advance();
    CHECK(Key(VK_RETURN) == 0);
    CHECK(platform::script::Running());
    platform::script::Advance();
    platform::script::Advance();
    CHECK_FALSE(platform::script::Running());

    // Real input again.
    POINT p;
    CHECK_FALSE(platform::script::InjectedCursor(&p));
    SHORT state = 0;
    CHECK_FALSE(platform::script::InjectedKey(VK_RETURN, &state));
    platform::present::SetScreenSize(before.cx, before.cy);
}

TEST_CASE("script run waits for the engine to poll a press", "[script]")
{
    platform::script::Start(ParseOk("key SPACE\n"));
    platform::script::Advance(); // press
    for (int i = 0; i < 10; i++)
        platform::script::Advance();
    // Never polled: still down after far more than 2 frames.
    CHECK(Key(VK_SPACE) == kPressed);
    platform::script::Advance(); // polled and held long enough: release
    CHECK(Key(VK_SPACE) == 0);

    // Unpolled presses give up after kObserveTimeoutFrames.
    platform::script::Start(ParseOk("key TAB\n"));
    for (int i = 0; i <= platform::script::kObserveTimeoutFrames; i++)
        platform::script::Advance();
    SHORT state = 0;
    CHECK_FALSE(platform::script::InjectedKey(VK_TAB, &state));
    platform::script::Stop();
    CHECK_FALSE(platform::script::Running());
}

TEST_CASE("script type presses shift for capitals", "[script]")
{
    platform::script::Start(ParseOk("type aB\n"));
    platform::script::Advance();
    CHECK(Key('A') == kPressed);
    CHECK(Key(VK_SHIFT) == 0);
    platform::script::Advance();
    platform::script::Advance(); // release A
    CHECK(Key('A') == 0);
    platform::script::Advance();
    platform::script::Advance(); // shift down
    CHECK(Key(VK_SHIFT) != 0);
    platform::script::Advance(); // B down
    CHECK(Key('B') == kPressed);
    CHECK(Key(VK_SHIFT) != 0);
    platform::script::Stop();
}

TEST_CASE("script key modifiers", "[script]")
{
    const std::vector<Command> c = ParseOk("key CTRL+TAB\nkey ctrl+shift+OEM_1\nkey b\n");
    REQUIRE(c.size() == 3);
    CHECK(c[0].verb == Verb::Key);
    CHECK(c[0].vk == VK_TAB);
    CHECK(c[0].mods == std::vector<int> { VK_CONTROL });
    CHECK(c[1].vk == VK_OEM_1);
    CHECK(c[1].mods == std::vector<int> { VK_CONTROL, VK_SHIFT });
    CHECK(c[2].vk == 'B');
    CHECK(c[2].mods.empty());
    CHECK(ParseError("key ctrl+nope+X\n") == "line 1: unknown modifier 'nope' (CTRL, SHIFT, ALT)");
    CHECK(ParseError("key ALT+F4\n") == "line 1: alt+f4 closes the game window; the scenarios cannot");
    CHECK(ParseError("key CTRL+\n") == "line 1: key needs a key name after its modifiers");
    CHECK(ParseOk("type :(\"_.\n").size() == 1);

    // The modifiers come down in written order around the key, up in
    // reverse, with the same frame phases SHIFT uses for a capital.
    platform::script::Start(ParseOk("key CTRL+SHIFT+X\n"));
    platform::script::Advance(); // CTRL down
    CHECK(Key(VK_CONTROL) == kPressed);
    CHECK(Key(VK_SHIFT) == 0);
    platform::script::Advance(); // SHIFT down
    CHECK(Key(VK_CONTROL) != 0);
    CHECK(Key(VK_SHIFT) == kPressed);
    platform::script::Advance(); // X down
    CHECK(Key('X') == kPressed);
    CHECK(Key(VK_CONTROL) != 0);
    CHECK(Key(VK_SHIFT) != 0);
    platform::script::Advance(); // X frame 2
    platform::script::Advance(); // X up
    CHECK(Key('X') == 0);
    platform::script::Advance(); // X release frame 2
    platform::script::Advance(); // SHIFT up, CTRL still down
    CHECK(Key(VK_SHIFT) == 0);
    CHECK(Key(VK_CONTROL) != 0);
    platform::script::Advance(); // CTRL up
    CHECK(Key(VK_CONTROL) == 0);
    platform::script::Advance(); // done
    CHECK_FALSE(platform::script::Running());
    platform::script::Stop();
}

TEST_CASE("script type presses punctuation through the US layout", "[script]")
{
    platform::script::Start(ParseOk("type :_\n"));
    platform::script::Advance(); // shift down for ':'
    CHECK(Key(VK_SHIFT) != 0);
    platform::script::Advance(); // ':' down
    CHECK(Key(VK_OEM_1) == kPressed);
    CHECK(Key(VK_SHIFT) != 0);
    platform::script::Advance(); // ':' frame 2
    platform::script::Advance(); // ':' up
    CHECK(Key(VK_OEM_1) == 0);
    platform::script::Advance(); // ':' release frame 2
    platform::script::Advance(); // shift up
    CHECK(Key(VK_SHIFT) == 0);
    platform::script::Advance(); // shift down again for '_'
    CHECK(Key(VK_SHIFT) != 0);
    platform::script::Advance(); // '_' down
    CHECK(Key(VK_OEM_MINUS) == kPressed);
    platform::script::Stop();
}

TEST_CASE("script shot writes the primary surface", "[script]")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("iwd2r_shot_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    SetShotDir(dir.string());
    std::vector<uint32_t> pixels(4 * 2, 0x00336699u);
    platform::present::SetPrimary(&pixels, 4, 2);

    platform::script::Start(ParseOk("shot first\nwait 1\n"));
    platform::script::Advance(); // the shot is instant; wait 1 starts
    CHECK(std::filesystem::file_size(dir / "first.png") > 8);
    platform::script::Advance();
    CHECK_FALSE(platform::script::Running());

    platform::present::SetPrimary(nullptr, 0, 0);
    SetShotDir("");
    std::filesystem::remove_all(dir);
}

TEST_CASE("control directory answers shot and frame-count requests", "[script][control]")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("iwd2r_control_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::filesystem::path control = dir / "control";
    std::filesystem::create_directories(control);
    SetShotDir(dir.string());
    SetEnv("IWD2R_CONTROL_DIR", control.string());
    const std::filesystem::path request = control / "shot-menu.req";
    std::ofstream(request).put('\n');
    std::ofstream(control / "other.txt").put('\n');

    // No frame presented yet: the request waits.
    platform::present::SetPrimary(nullptr, 0, 0);
    platform::window::PumpEvents();
    CHECK(std::filesystem::exists(request));

    std::vector<uint32_t> pixels(4 * 2, 0x00336699u);
    platform::present::SetPrimary(&pixels, 4, 2);
    const int before = platform::present::PresentedFrames();
    platform::present::MarkPrimaryDirty();
    platform::window::PumpEvents(); // presents one frame
    CHECK(platform::present::PresentedFrames() == before + 1);
    CHECK(std::filesystem::file_size(dir / "menu.png") > 8);
    CHECK_FALSE(std::filesystem::exists(request));
    CHECK(std::filesystem::exists(control / "other.txt"));

    // frames.req: the presented frame count in frames.txt.
    std::ofstream(control / "frames.req").put('\n');
    platform::present::MarkPrimaryDirty();
    platform::window::PumpEvents();
    CHECK_FALSE(std::filesystem::exists(control / "frames.req"));
    int frames = -1;
    {
        // Closed before the directory is removed (Windows keeps open files).
        std::ifstream framesIn(control / "frames.txt");
        framesIn >> frames;
    }
    CHECK(frames == platform::present::PresentedFrames());
    CHECK(frames == before + 2);

    // Without the variable the directory is not looked at.
    SetEnv("IWD2R_CONTROL_DIR", "");
    std::ofstream(control / "shot-later.req").put('\n');
    platform::present::MarkPrimaryDirty();
    platform::window::PumpEvents();
    CHECK(std::filesystem::exists(control / "shot-later.req"));
    CHECK_FALSE(std::filesystem::exists(dir / "later.png"));

    platform::present::SetPrimary(nullptr, 0, 0);
    SetShotDir("");
    std::filesystem::remove_all(dir);
}

TEST_CASE("control directory answers state requests with a checkpoint dump", "[script][control]")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("iwd2r_state_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::filesystem::path control = dir / "control";
    std::filesystem::create_directories(control);
    SetShotDir(dir.string());
    SetEnv("IWD2R_CONTROL_DIR", control.string());
    std::ofstream(control / "state-gender_done.req").put('\n');
    // Not a checkpoint name: refused, and the request is removed.
    std::ofstream(control / "state-a.b.req").put('\n');

    std::vector<uint32_t> pixels(4 * 2, 0x00336699u);
    platform::present::SetPrimary(&pixels, 4, 2);
    platform::present::MarkPrimaryDirty();
    platform::window::PumpEvents(); // presents one frame
    CHECK_FALSE(std::filesystem::exists(control / "state-gender_done.req"));
    CHECK_FALSE(std::filesystem::exists(control / "state-a.b.req"));
    CHECK_FALSE(std::filesystem::exists(dir / "checkpoint-a.b.json"));

    // The replay dump's layout: the presented frame, the name, the screen
    // (none: no engine in this test).
    std::string text;
    {
        std::ifstream in(dir / "checkpoint-gender_done.json");
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const std::string frame = "\"frame\": " + std::to_string(platform::present::PresentedFrames()) + ",";
    CHECK(text.find(frame) != std::string::npos);
    CHECK(text.find("\"checkpoint\": \"gender_done\",") != std::string::npos);
    CHECK(text.find("\"screen\": \"none\",") != std::string::npos);

    SetEnv("IWD2R_CONTROL_DIR", "");
    platform::present::SetPrimary(nullptr, 0, 0);
    SetShotDir("");
    std::filesystem::remove_all(dir);
}

TEST_CASE("control directory reports the engine's mouse-button polls", "[script][control]")
{
    // A real-input driver waits for these between a press and its release
    // and before the next press: the engine reads only "down" and "pressed
    // since the last poll", so a release and a press with no poll between
    // them read as one press.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("iwd2r_polls_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::filesystem::path control = dir / "control";
    std::filesystem::create_directories(control);
    SetShotDir(dir.string());
    SetEnv("IWD2R_CONTROL_DIR", control.string());
    std::vector<uint32_t> pixels(4 * 2, 0x00336699u);
    platform::present::SetPrimary(&pixels, 4, 2);

    const unsigned before = platform::window::ButtonPolls();
    GetAsyncKeyState(VK_LBUTTON);
    GetAsyncKeyState(VK_RBUTTON);
    GetAsyncKeyState('A'); // not a mouse button
    CHECK(platform::window::ButtonPolls() == before + 2);

    // Answered on the next pump after a poll, without a new frame.
    platform::window::PumpEvents();
    std::ofstream(control / "input.req").put('\n');
    platform::window::PumpEvents(); // no frame, no poll: not answered yet
    CHECK(std::filesystem::exists(control / "input.req"));
    GetAsyncKeyState(VK_LBUTTON);
    platform::window::PumpEvents();
    CHECK_FALSE(std::filesystem::exists(control / "input.req"));
    unsigned polls = 0;
    {
        std::ifstream in(control / "input.txt");
        in >> polls;
    }
    CHECK(polls == before + 3);

    SetEnv("IWD2R_CONTROL_DIR", "");
    platform::present::SetPrimary(nullptr, 0, 0);
    SetShotDir("");
    std::filesystem::remove_all(dir);
}
