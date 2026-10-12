// iwd2r. Distributed under LICENSE.md.
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
        "Quit\n");
    REQUIRE(c.size() == 11);
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
    CHECK(ParseError("type a-b\n").rfind("line 1: type supports", 0) == 0);
    CHECK(ParseError("shot\n").rfind("line 1: shot", 0) == 0);
    CHECK(ParseError("shot ../x\n").rfind("line 1: shot name", 0) == 0);
    CHECK(ParseError("quit now\n").rfind("line 1: quit", 0) == 0);
}

TEST_CASE("script run drives the injected cursor and buttons", "[script]")
{
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
    std::ifstream framesIn(control / "frames.txt");
    int frames = -1;
    framesIn >> frames;
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
