#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/msvc_rand.h"
#include "compat/msvc_time.h"
#include "install_ini_backup.h"
#include "platform/replay.h"
#include "platform/replay_clock.h"
#include "platform/script_core.h"
#include "platform/state_dump.h"
#include "platform/win32_api.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef IWD2R_EXE
#error IWD2R_EXE must be defined by tests/CMakeLists.txt
#endif
#ifndef IWD2R_SOURCE_DIR
#error IWD2R_SOURCE_DIR must be defined by tests/CMakeLists.txt
#endif

namespace {

namespace fs = std::filesystem;
using platform::state_dump::Snapshot;

std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// The golden dumps: tests/test_tools.py validates the same files against
// oracle/replay/schema.json, so the writer's output is schema-valid.
std::string Fixture(const char* name)
{
    return ReadFile(fs::path(IWD2R_SOURCE_DIR) / "tests" / "fixtures" / name);
}

fs::path TempDir(const char* tag)
{
    const fs::path dir = fs::temp_directory_path()
        / (std::string(tag) + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    return dir;
}

// A checkpoint dump on the main menu, as both sides wrote it for
// scenarios/replay_click.txt.
Snapshot MenuSnapshot()
{
    Snapshot s;
    s.frame = 61;
    s.hasCheckpoint = true;
    s.checkpoint = "menu";
    s.rngState = 1;
    s.hasAiRngState = true;
    s.aiRngState = 3861535683u;
    s.engine.screen = "connection";
    s.engine.auCounter = 61;
    return s;
}

std::atomic<int> g_fired { 0 };

void CALLBACK CountFired(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    g_fired++;
}

// Days from 1970-01-01 to a civil date (proleptic Gregorian).
long long DaysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

}

TEST_CASE("state dump JSON matches the golden dumps", "[dump]")
{
    REQUIRE(platform::state_dump::ToJson(MenuSnapshot()) == Fixture("state_dump_menu.json"));

    Snapshot chargen;
    chargen.frame = 791;
    chargen.hasCheckpoint = true;
    chargen.checkpoint = "str_up_1";
    chargen.rngState = 1;
    chargen.hasAiRngState = true;
    chargen.aiRngState = 149961326u;
    chargen.engine.screen = "create_char";
    chargen.engine.auCounter = 789;
    chargen.engine.hasChargen = true;
    platform::state_dump::Chargen& c = chargen.engine.chargen;
    c.sprite = true;
    c.race = 2;
    c.subrace = 0;
    c.klass = 5;
    const int abilities[6] = { 11, 12, 8, 10, 10, 10 };
    for (int i = 0; i < 6; ++i)
        c.abilities[i] = abilities[i];
    c.feats[0] = 8;
    c.feats[1] = 0x03F01FC0; // CHARBASE.CRE's, as both sides read it
    c.portrait = "2MELF1_L";
    REQUIRE(platform::state_dump::ToJson(chargen) == Fixture("state_dump_chargen.json"));

    // A periodic dump (no checkpoint) with no sprite and no input thread yet.
    Snapshot periodic;
    periodic.frame = 30;
    periodic.rngState = 0xFFFFFFFFu;
    periodic.aiStalledFrames = 2;
    periodic.engine.screen = "create_char";
    periodic.engine.auCounter = 28;
    periodic.engine.fullscreen = 1;
    periodic.engine.inAsyncUpdate = 1;
    periodic.engine.hasChargen = true;
    periodic.engine.chargen.sprite = false;
    const int rolls[7] = { 1, 2, 3, 4, 5, 6, -7 };
    for (int i = 0; i < 7; ++i)
        periodic.engine.chargen.rolls[i] = rolls[i];
    periodic.engine.chargen.skillPoints = -1;
    periodic.engine.chargen.race = 99; // ignored without a sprite
    REQUIRE(platform::state_dump::ToJson(periodic) == Fixture("state_dump_periodic.json"));
}

TEST_CASE("state dump strings are escaped as the recorder escapes them", "[dump]")
{
    Snapshot s = MenuSnapshot();
    s.checkpoint = "a\"b\\c";
    s.engine.screen = std::string("x\x01\xE9", 3);
    const std::string json = platform::state_dump::ToJson(s);
    REQUIRE(json.find("\"checkpoint\": \"a\\\"b\\\\c\"") != std::string::npos);
    REQUIRE(json.find("\"screen\": \"x\\u0001\\u00e9\"") != std::string::npos);
}

TEST_CASE("state dump writes through a temporary file", "[dump]")
{
    const fs::path dir = TempDir("iwd2r-dump-");
    const fs::path path = dir / "checkpoint-menu.json";
    REQUIRE(platform::state_dump::Write(path.string().c_str(), MenuSnapshot()));
    REQUIRE(ReadFile(path) == Fixture("state_dump_menu.json"));
    REQUIRE_FALSE(fs::exists(path.string() + ".tmp"));
    // Overwrites.
    Snapshot other = MenuSnapshot();
    other.frame = 62;
    REQUIRE(platform::state_dump::Write(path.string().c_str(), other));
    REQUIRE(ReadFile(path).find("\"frame\": 62,") != std::string::npos);
    REQUIRE_FALSE(platform::state_dump::Write((dir / "no" / "such" / "x.json").string().c_str(), other));
    REQUIRE_FALSE(platform::state_dump::Write(nullptr, other));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("replay clock constants", "[dump]")
{
    namespace rc = platform::replay_clock;
    STATIC_REQUIRE(rc::TickAfterFrame(0) == rc::kClockBase);
    STATIC_REQUIRE(rc::TickAfterFrame(1) == rc::kClockBase + 33);
    STATIC_REQUIRE(rc::TickAfterFrame(2) == rc::kClockBase + 66);
    STATIC_REQUIRE(rc::TickAfterFrame(3) == rc::kClockBase + 100);
    STATIC_REQUIRE(rc::TickAfterFrame(30) == rc::kClockBase + 1000);
    STATIC_REQUIRE(rc::WallClockSeconds(rc::kClockBase) == rc::kWallClockEpoch);
    STATIC_REQUIRE(rc::WallClockSeconds(rc::kClockBase + 999) == rc::kWallClockEpoch);
    STATIC_REQUIRE(rc::WallClockSeconds(rc::TickAfterFrame(61)) == rc::kWallClockEpoch + 2);
    // The epoch is midnight UTC of the wall clock's date.
    REQUIRE(rc::kWallClockEpoch
        == DaysFromCivil(rc::kWallClockYear, rc::kWallClockMonth, rc::kWallClockDay) * 86400);
}

// ctest never sets IWD2R_STATE_DUMP: everything stays as without the
// replay mode.
TEST_CASE("replay mode is off without IWD2R_STATE_DUMP", "[dump]")
{
    const char* dir = std::getenv("IWD2R_STATE_DUMP");
    if (dir && *dir)
        SKIP("[iwd2r-skip] IWD2R_STATE_DUMP is set for this test run");
    REQUIRE_FALSE(platform::replay::Active());

    const unsigned int rngBefore = iwd2r_rand_state();
    platform::replay::OnFrame();
    platform::replay::Checkpoint("nothing", 1);
    REQUIRE(platform::replay::Frame() == 0);
    REQUIRE(iwd2r_rand_state() == rngBefore); // no srand(seed)

    // time() and CTime are the C library's.
    REQUIRE_FALSE(iwd2r_wall_clock_is_virtual());
    const std::time_t before = std::time(nullptr);
    const std::time_t now = iwd2r_time(nullptr);
    REQUIRE(now >= before);
    REQUIRE(now > platform::replay_clock::kWallClockEpoch + 365LL * 86400 * 20);

    // Real timers: a one-shot timeSetEvent fires on its own thread, with
    // no frame to fire it.
    g_fired = 0;
    const MMRESULT id = timeSetEvent(5, 1, CountFired, 0, 0);
    REQUIRE(id != 0);
    for (int i = 0; i < 200 && g_fired.load() == 0; ++i)
        Sleep(5);
    REQUIRE(g_fired.load() == 1);
    REQUIRE(timeKillEvent(id) == MMSYSERR_INVALPARAM); // a fired one-shot is gone
}

TEST_CASE("checkpoint lines parse for the replay drivers", "[dump]")
{
    const std::string script = ReadFile(fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "replay_click.txt");
    std::vector<platform::script::Command> commands;
    std::string error;
    REQUIRE(platform::script::Parse(script, commands, error));
    std::vector<std::string> names;
    for (const auto& c : commands) {
        if (c.verb == platform::script::Verb::Checkpoint)
            names.push_back(c.text);
    }
    REQUIRE(names == std::vector<std::string> { "menu", "select_party", "party_formation", "chargen" });
}

// End to end: our engine in the replay mode on the start screen scenario
// writes the checkpoint the original writes there (oracle.py replay record
// start_screen): the main menu (connection), frame 61, the seeded main
// thread, and the AI thread's rand seeded from the virtual wall clock.
TEST_CASE("replay mode dumps the start screen checkpoint", "[dump][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0')
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    const std::string data = env;
    const fs::path dir = TempDir("iwd2r-replay-");
    const std::string script = (fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "start_screen.txt").string();
    const std::string out = (dir / "log.txt").string();
    const std::string overrides = "Program Options/Resolution=800;Program Options/Full Screen=0";
    int raw = 0;
    {
        InstallIniBackup backup(data);
#ifdef _WIN32
        const std::string cmd = "set SDL_VIDEO_DRIVER=dummy&& set SDL_AUDIO_DRIVER=dummy&& set IWD2R_SCRIPT=" + script
            + "&& set IWD2R_SHOT_DIR=" + dir.string() + "&& set IWD2R_STATE_DUMP=" + dir.string()
            + "&& set IWD2R_SEED=1&& set \"IWD2R_INI_OVERRIDE=" + overrides + "\"&& set IWD2_DATA=" + data + "&& \""
            IWD2R_EXE "\" > \"" + out + "\" 2>&1";
        const std::string full = "\"" + cmd + "\"";
#else
        const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SCRIPT=\"" + script
            + "\" IWD2R_SHOT_DIR=\"" + dir.string() + "\" IWD2R_STATE_DUMP=\"" + dir.string()
            + "\" IWD2R_SEED=1 IWD2R_INI_OVERRIDE=\"" + overrides + "\" IWD2_DATA=\"" + data + "\" timeout 150 \""
            IWD2R_EXE "\" > \"" + out + "\" 2>&1";
#endif
        raw = std::system(full.c_str());
    }
#ifdef _WIN32
    const int status = raw;
#else
    const int status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
    const std::string log = ReadFile(out);
    const std::string checkpoint = ReadFile(dir / "checkpoint-start.json");
    const std::string periodic = ReadFile(dir / "frame-30.json");
    const std::string statusText = ReadFile(dir / "status.txt");
    std::error_code ec;
    fs::remove_all(dir, ec);
    INFO("output:\n"
        << log);
    REQUIRE(status == 0);
    REQUIRE(checkpoint
        == "{\n  \"frame\": 61,\n  \"checkpoint\": \"start\",\n  \"screen\": \"connection\",\n  \"rng_state\": 1,\n"
           "  \"globals\": {\"au_counter\": 61, \"fullscreen\": 0, \"in_async_update\": 0, \"ai_stalled_frames\": 0, "
           "\"reinitializing\": 0, \"ai_rng_state\": 3861535683},\n  \"chargen\": null\n}\n");
    REQUIRE(periodic.find("\"frame\": 30,") != std::string::npos);
    REQUIRE(statusText == "frame=61\nscript_line=0\nstate=finished\n");
}
