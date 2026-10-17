#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/msvc_rand.h"
#include "compat/msvc_time.h"
#include "data_view.h"
#include "platform/replay.h"
#include "platform/replay_clock.h"
#include "platform/replay_probe.h"
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
// scenarios/replay_click.txt (the original writes no search_wait_timeouts:
// only our engine has the search wait).
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
    periodic.aiOverlapFrames = 3;
    periodic.searchWaitTimeouts = 4;
    periodic.engine.screen = "create_char";
    periodic.engine.auCounter = 28;
    periodic.engine.fullscreen = 1;
    periodic.engine.inAsyncUpdate = 1;
    periodic.engine.loading = 1;
    periodic.engine.hasChargen = true;
    periodic.engine.chargen.sprite = false;
    const int rolls[7] = { 1, 2, 3, 4, 5, 6, -7 };
    for (int i = 0; i < 7; ++i)
        periodic.engine.chargen.rolls[i] = rolls[i];
    periodic.engine.chargen.skillPoints = -1;
    periodic.engine.chargen.race = 99; // ignored without a sprite
    REQUIRE(platform::state_dump::ToJson(periodic) == Fixture("state_dump_periodic.json"));

    // The world screen: one party member, and one whose id does not resolve.
    Snapshot world;
    world.frame = 1500;
    world.hasCheckpoint = true;
    world.checkpoint = "world";
    world.rngState = 12345;
    world.hasAiRngState = true;
    world.aiRngState = 77;
    world.engine.screen = "world";
    world.engine.auCounter = 1498;
    world.loads = 1;
    world.heldFrames = 13;
    world.engine.hasWorld = true;
    world.engine.world.area = "AR1000";
    platform::state_dump::PartyMember member;
    member.id = 0x10000;
    member.sprite = true;
    member.x = 1046;
    member.y = 1258;
    member.hp = 9;
    member.maxHp = 12;
    member.skills[1] = 4;
    member.skills[5] = 2;
    member.skills[13] = -1;
    member.action = 23;
    member.modal = 1;
    member.facing = 12;
    member.morale = 10;
    world.engine.world.party.push_back(member);
    platform::state_dump::PartyMember missing;
    missing.id = 0x20001;
    missing.hp = 5; // ignored without a sprite
    missing.action = 3; // ignored without a sprite
    missing.morale = 7; // ignored without a sprite
    world.engine.world.party.push_back(missing);
    world.engine.world.doors.push_back({ "AR1000_Door1", 0, 0 });
    world.engine.world.doors.push_back({ "Door\"2", 1, 1 });
    world.engine.world.exploredCount = 345;
    world.engine.world.creatures = 7;
    // The area's sprites that are not party members, the lists' order.
    platform::state_dump::EnemyCreature goblin;
    goblin.id = 0x30005;
    goblin.hp = 4;
    goblin.maxHp = 7;
    goblin.timedEffects = 2;
    goblin.equipedEffects = 1;
    world.engine.world.enemyCreatures.push_back(goblin);
    platform::state_dump::EnemyCreature corpse; // dead, effects expired
    corpse.id = 0x30006;
    corpse.hp = -3;
    corpse.maxHp = 6;
    world.engine.world.enemyCreatures.push_back(corpse);
    // The game block: one member with three items, one whose id does not
    // resolve.
    world.engine.hasGame = true;
    world.engine.game.partyGold = 1234;
    platform::state_dump::GameMember carried;
    carried.id = 0x10000;
    carried.sprite = true;
    carried.gold = 12;
    carried.xp = 300;
    carried.level = 1;
    carried.classLevels[4] = 1;
    carried.hasItem[0] = true;
    carried.items[0] = { "00HELM01", { 0, 0, 0 }, 1 };
    carried.hasItem[16] = true;
    carried.items[16] = { "00ARRO01", { 40, 0, 0 }, 0 };
    carried.hasItem[20] = true;
    carried.items[20] = { "Q\"2", { 1, 2, 65535 }, 0xFFFFFFFFu };
    carried.items[30] = { "IGNORED", { 1, 1, 1 }, 1 }; // no item in the slot
    world.engine.game.members.push_back(carried);
    platform::state_dump::GameMember gone;
    gone.id = 0x20001;
    gone.gold = 5; // ignored without a sprite
    gone.hasItem[0] = true; // ignored without a sprite
    world.engine.game.members.push_back(gone);
    REQUIRE(platform::state_dump::ToJson(world) == Fixture("state_dump_world.json"));
}

TEST_CASE("state dump writes the doors, the dialog, the explored tiles and the creatures", "[dump]")
{
    Snapshot s = MenuSnapshot();
    s.engine.screen = "world";
    s.engine.hasWorld = true;
    s.engine.world.area = "AR1000";
    s.engine.world.doors.push_back({ "DOOR01", 0, 1 });
    s.engine.world.dialog.active = 1;
    s.engine.world.dialog.speaker = 3;
    s.engine.world.dialog.entry = 0;
    s.engine.world.dialog.responses = 2;
    s.engine.world.exploredCount = 12;
    s.engine.world.creatures = 3;
    const fs::path dir = TempDir("iwd2r-dump-");
    const fs::path path = dir / "checkpoint-world.json";
    REQUIRE(platform::state_dump::Write(path.string().c_str(), s));
    REQUIRE(ReadFile(path).find("\"world\": {\"area\": \"AR1000\", \"party\": [], "
                                "\"doors\": [{\"name\": \"DOOR01\", \"open\": 0, \"locked\": 1}], "
                                "\"dialog\": {\"active\": 1, \"speaker\": 3, \"entry\": 0, \"responses\": 2}, "
                                "\"explored_count\": 12, \"creatures\": 3, \"enemy_creatures\": []},\n  \"game\": null,\n  \"slots\": null\n}\n")
        != std::string::npos);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_CASE("state dump world block with no area and no party", "[dump]")
{
    Snapshot s = MenuSnapshot();
    s.engine.screen = "world";
    s.engine.hasWorld = true;
    REQUIRE(platform::state_dump::ToJson(s).find(
                "\"world\": {\"area\": \"\", \"party\": [], \"doors\": [], "
                "\"dialog\": {\"active\": 0, \"speaker\": -1, \"entry\": -1, \"responses\": 0}, "
                "\"explored_count\": 0, \"creatures\": 0, \"enemy_creatures\": []},\n  \"game\": null,\n  \"slots\": null\n}\n")
        != std::string::npos);
}

TEST_CASE("state dump writes the load and save screens' slots", "[dump]")
{
    Snapshot s = MenuSnapshot();
    s.engine.screen = "load";
    s.engine.hasSlots = true;
    REQUIRE(platform::state_dump::ToJson(s).find("\"slots\": []\n}\n") != std::string::npos);
    platform::state_dump::Slot slot;
    slot.file = "000000004-Probe Save";
    slot.name = "Probe Save";
    slot.character = "Probe";
    slot.time = 25200000;
    slot.chapter = 0;
    slot.chapterText = "Prologue";
    s.engine.slots.push_back(slot);
    slot.file = "crash\"1";
    slot.name = "crash\"1";
    slot.chapter = -1;
    s.engine.slots.push_back(slot);
    REQUIRE(platform::state_dump::ToJson(s).find(
                "\"slots\": [{\"file\": \"000000004-Probe Save\", \"name\": \"Probe Save\", \"character\": \"Probe\", "
                "\"time\": 25200000, \"chapter\": 0, \"chapter_text\": \"Prologue\"}, "
                "{\"file\": \"crash\\\"1\", \"name\": \"crash\\\"1\", \"character\": \"Probe\", \"time\": 25200000, "
                "\"chapter\": -1, \"chapter_text\": \"Prologue\"}]\n}\n")
        != std::string::npos);
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

// The frame gate (both sides' replay mode): what a load looks like from the
// main thread, and the two ways its end can reach it.
TEST_CASE("frame gate holds the calls drawn inside an AI update", "[dump]")
{
    platform::replay_clock::FrameGate gate;
    // Normal frames: one AI update each.
    REQUIRE(gate.Counts(10, false, false));
    REQUIRE(gate.Counts(11, false, false));
    // A load: AI update 12 redraws its progress three times, the first
    // before the progress bar is up.
    REQUIRE_FALSE(gate.Counts(12, true, false));
    REQUIRE(gate.Stretch() == 1);
    REQUIRE(gate.Loads() == 0);
    REQUIRE_FALSE(gate.Counts(12, true, true));
    REQUIRE(gate.Loads() == 1);
    REQUIRE_FALSE(gate.Counts(12, true, true));
    REQUIRE(gate.Loads() == 1); // the same update: one load
    REQUIRE(gate.HeldFrames() == 3);
    // The update ends; the redraw it asks for counts.
    REQUIRE(gate.Counts(12, false, false));
    REQUIRE(gate.EndedStretch() == 3);
    REQUIRE(gate.Stretch() == 0);
    // The next update follows as usual.
    REQUIRE(gate.Counts(13, false, false));
    REQUIRE(gate.EndedStretch() == 0);
    REQUIRE(gate.HeldFrames() == 3);
}

TEST_CASE("frame gate holds the end of a load that raced its last redraw", "[dump]")
{
    platform::replay_clock::FrameGate gate;
    REQUIRE(gate.Counts(10, false, false));
    REQUIRE_FALSE(gate.Counts(11, true, true));
    // The load's last redraw was taken after the update ended: it counts,
    // for update 11.
    REQUIRE(gate.Counts(11, false, false));
    // The redraw the update's end asked for comes next with au_counter
    // unchanged: held, so update 11 has one frame either way.
    REQUIRE_FALSE(gate.Counts(11, false, false));
    REQUIRE(gate.HeldFrames() == 2);
    REQUIRE(gate.Counts(12, false, false));
    // Only right after a held stretch: elsewhere a call with no AI update
    // counts (a stall, which the replay mode reports).
    REQUIRE(gate.Counts(12, false, false));
    REQUIRE(gate.HeldFrames() == 2);
    REQUIRE(gate.Loads() == 1);
}

TEST_CASE("frame gate after a load: an update that already ran counts", "[dump]")
{
    platform::replay_clock::FrameGate gate;
    REQUIRE_FALSE(gate.Counts(5, true, true));
    REQUIRE(gate.Counts(5, false, true)); // a multi-update load: the bar stays up
    // The next update already started when the call reads the gate: held.
    REQUIRE_FALSE(gate.Counts(6, true, true));
    REQUIRE(gate.Loads() == 2);
    REQUIRE(gate.Counts(6, false, false));
    REQUIRE(gate.Counts(7, false, false));
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
    platform::replay::OnFrameEnd();
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

// The replay mode's wait for the search thread (CGameArea::AIUpdate's
// ReplayWaitForSearch) ends at its deadline of real time when the search
// never does (a lost semaphore post; no search thread, where the deadline is
// 0) and counts the expiry in the dump's search_wait_timeouts.
TEST_CASE("replay search wait gives up at its deadline and counts it", "[dump]")
{
    using platform::replay::SearchWaitTimeouts;
    using platform::replay::WaitForSearch;
    const uint32_t before = SearchWaitTimeouts();

    // Done at the first poll: nothing counted.
    int polls = 0;
    CHECK(WaitForSearch(5000, [&](std::string*) {
        ++polls;
        return false;
    }));
    CHECK(polls == 1);
    CHECK(SearchWaitTimeouts() == before);

    // Done at the third poll.
    polls = 0;
    CHECK(WaitForSearch(5000, [&](std::string* what) {
        *what = "object 7";
        return ++polls < 3;
    }));
    CHECK(polls == 3);
    CHECK(SearchWaitTimeouts() == before);

    // Never done: it polls until the deadline has passed, then gives up.
    polls = 0;
    const auto start = std::chrono::steady_clock::now();
    CHECK_FALSE(WaitForSearch(50, [&](std::string* what) {
        *what = "object 7, its request in state 0 (collision delay 0), 1 requests queued in front and 0 in back";
        ++polls;
        return true;
    }));
    const auto waited = std::chrono::steady_clock::now() - start;
    CHECK(waited >= std::chrono::milliseconds(50));
    CHECK(waited < std::chrono::seconds(5));
    CHECK(polls > 1);
    CHECK(SearchWaitTimeouts() == before + 1);

    // No search thread (a deadline of 0): one poll decides.
    polls = 0;
    CHECK_FALSE(WaitForSearch(0, [&](std::string* what) {
        *what = "object 7, no search thread";
        ++polls;
        return true;
    }));
    CHECK(polls == 1);
    CHECK(SearchWaitTimeouts() == before + 2);
}

namespace {

std::vector<std::string> CheckpointNames(const char* scenario)
{
    const std::string script = ReadFile(fs::path(IWD2R_SOURCE_DIR) / "scenarios" / scenario);
    std::vector<platform::script::Command> commands;
    std::string error;
    REQUIRE(platform::script::Parse(script, commands, error));
    std::vector<std::string> names;
    for (const auto& c : commands) {
        if (c.verb == platform::script::Verb::Checkpoint)
            names.push_back(c.text);
    }
    return names;
}

}

TEST_CASE("checkpoint lines parse for the replay drivers", "[dump]")
{
    REQUIRE(CheckpointNames("replay_click.txt")
        == std::vector<std::string> { "menu", "select_party", "party_formation", "chargen" });
    const std::vector<std::string> chargen = CheckpointNames("chargen_replay.txt");
    REQUIRE(chargen.size() == 20);
    REQUIRE(chargen.front() == "menu");
    REQUIRE(chargen[1] == "chargen");
    REQUIRE(chargen.back() == "abilities_done");
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
    // The engine writes into its data folder: it runs on a private view.
    DataView view(data);
    int raw = 0;
    {
#ifdef _WIN32
        const std::string cmd = "set SDL_VIDEO_DRIVER=dummy&& set SDL_AUDIO_DRIVER=dummy&& set IWD2R_SCRIPT=" + script
            + "&& set IWD2R_SHOT_DIR=" + dir.string() + "&& set IWD2R_STATE_DUMP=" + dir.string()
            + "&& set IWD2R_SEED=1&& set \"IWD2R_INI_OVERRIDE=" + overrides + "\"&& set IWD2_DATA=" + view.Path() + "&& \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
        const std::string full = "\"" + cmd + "\"";
#else
        const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SCRIPT=\"" + script
            + "\" IWD2R_SHOT_DIR=\"" + dir.string() + "\" IWD2R_STATE_DUMP=\"" + dir.string()
            + "\" IWD2R_SEED=1 IWD2R_INI_OVERRIDE=\"" + overrides + "\" IWD2_DATA=\"" + view.Path() + "\" timeout 150 \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
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
           "  \"globals\": {\"au_counter\": 61, \"fullscreen\": 0, \"in_async_update\": 0, \"ai_stalled_frames\": 0, \"ai_overlap_frames\": 0, "
           "\"search_wait_timeouts\": 0, \"reinitializing\": 0, \"loading\": 0, \"loads\": 0, \"held_frames\": 0, \"ai_rng_state\": 3861535683},\n  \"chargen\": null,\n  \"world\": null,\n"
           "  \"game\": {\"party_gold\": 0, \"members\": []},\n  \"slots\": null\n}\n");
    REQUIRE(periodic.find("\"frame\": 30,") != std::string::npos);
    REQUIRE(statusText == "frame=61\nscript_line=0\nstate=finished\n");
}

TEST_CASE("replay probe: IWD2R_PROBE's frame range", "[dump]")
{
    using platform::replay_probe::ParseRange;
    using platform::replay_probe::Range;
    Range r;
    CHECK(ParseRange(nullptr, &r));
    CHECK_FALSE(r.on);
    CHECK(ParseRange("", &r));
    CHECK_FALSE(r.Has(0));
    REQUIRE(ParseRange("1496", &r));
    CHECK(r.Has(1496));
    CHECK_FALSE(r.Has(1495));
    CHECK_FALSE(r.Has(1497));
    REQUIRE(ParseRange("1490-1499", &r));
    CHECK(r.Has(1490));
    CHECK(r.Has(1499));
    CHECK_FALSE(r.Has(1500));
    CHECK_FALSE(ParseRange("9-1", &r));
    CHECK_FALSE(ParseRange("-1", &r));
    CHECK_FALSE(ParseRange("1-", &r));
    CHECK_FALSE(ParseRange("1x", &r));
    CHECK_FALSE(r.on);
}

TEST_CASE("replay probe: the block both engines write", "[dump]")
{
    platform::replay_probe::Frame f;
    f.frame = 1496;
    f.screen = "chapter";
    f.characters = 1;
    f.slots[0] = 7143533;
    f.portraits[0] = 7143533;
    f.area.present = true;
    f.area.resref = "AR1000";
    f.area.characters = 1;
    f.area.width = 3;
    f.area.height = 2;
    f.area.size = 6;
    f.area.range = 14;
    f.area.ids[0] = 7143533;
    for (int i = 1; i < 6; ++i)
        f.area.ids[i] = -1;
    f.area.map = { 0x80, 0x81, 0x00, 0xff, 0x0f, 0x10 };
    platform::replay_probe::Object door;
    door.index = 41;
    door.id = 2687017;
    door.type = platform::replay_probe::kTypeDoor;
    door.x = 2377;
    door.y = 601;
    door.here = 1;
    door.speed = 1;
    door.hasName = true;
    door.name = "AR1000_Door2";
    platform::replay_probe::Object hint;
    hint.index = 19;
    hint.id = 1245203;
    hint.type = platform::replay_probe::kTypeSprite;
    hint.x = 2354;
    hint.y = 608;
    hint.here = 1;
    hint.speed = 1;
    hint.seen = 32;
    hint.hasName = true;
    hint.name = "Door_Hint_Text_01";
    hint.sprite = true;
    hint.lastCharacters = 1;
    hint.active = hint.activeAi = hint.activeImprisonment = 1;
    hint.field294 = 1;
    hint.sequence = 6;
    hint.timers = { { 18, 1234 } };
    hint.direction = 6;
    hint.newDirection = 10;
    hint.noActionCount = 76;
    hint.scripts = { { "-", 0 }, { "10FIRTHV", 2 }, { "-", 0 }, { "-", 0 }, { "-", 0 }, { "-", 0 }, { "00AMVR", 2 } };
    f.messages = 3;
    f.setSequences = { { 1245203, 6, 1245203 } };
    f.objects = { hint, door };
    CHECK(platform::replay_probe::Format(f)
        == "frame 1496 screen chapter\n"
           "game characters 1 visible_area 0 slots 7143533,-1,-1,-1,-1,-1 portraits 7143533,-1,-1,-1,-1,-1 time 0 active 0\n"
           "area AR1000 characters 1 view 0,0 vis 3x2 size 6 range 14 ids 7143533,-1,-1,-1,-1,-1\n"
           "messages 3 setseq 1245203:6:1245203\n"
           "obj 19 1245203 type 49 pos 2354,608 here 1 list 0 seen 32 speed 1 inhibitor 0 name Door_Hint_Text_01"
           " last 1 active 1 ai 1 imprisonment 1 visible_monster 0 field_294 1 state 00000000 action 0 sequence 6"
           " queued 0 next -1 triggers 0 effects 0,0 xp 0 hp 0 dest 0,0 path timers 18:1234"
           " scripts -:0 10FIRTHV:2 -:0 -:0 -:0 -:0 00AMVR:2 dir 6:10 noaction 76\n"
           "obj 41 2687017 type 33 pos 2377,601 here 1 list 0 seen 0 speed 1 inhibitor 0 name AR1000_Door2\n"
           "vis 0 808100\n"
           "vis 1 ff0f10\n");
}
