#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef IWD2R_EXE
#error IWD2R_EXE must be defined by tests/CMakeLists.txt
#endif
#ifndef IWD2R_SOURCE_DIR
#error IWD2R_SOURCE_DIR must be defined by tests/CMakeLists.txt
#endif

// The game file round trip, end to end: scenarios/pending/world_entry.txt
// creates a character and clicks Party Formation's Done, which runs
// CInfGame::SetupCharacters, SaveGame (Marshal, CGameArea::Marshal) and
// LoadGame (Unmarshal) before the world screen. The engine runs against an
// overlay of the install (symbolic links to the game data, private temp,
// tempsave, mpsave, cache and Characters folders and INI copies), so the
// install is only read and runs do not share save folders. Linux only (the
// overlay uses symbolic links).

#ifndef _WIN32

namespace {

namespace fs = std::filesystem;

std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Builds the overlay of the install at `data` in a new temporary folder.
fs::path MakeOverlay(const fs::path& data)
{
    const fs::path overlay = fs::temp_directory_path() / ("iwd2r-gam-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(overlay);
    for (const auto& entry : fs::directory_iterator(data)) {
        const std::string name = entry.path().filename().string();
        const std::string lower = Lower(name);
        if (lower == "temp" || lower == "tempsave" || lower == "mpsave") {
            fs::create_directories(overlay / name);
        } else if (lower == "cache" || lower == "characters") {
            fs::copy(entry.path(), overlay / name, fs::copy_options::recursive);
        } else if (entry.is_regular_file() && entry.path().extension().string().size() == 4
            && Lower(entry.path().extension().string()) == ".ini") {
            fs::copy_file(entry.path(), overlay / name);
        } else {
            fs::create_symlink(entry.path(), overlay / name);
        }
    }
    return overlay;
}

struct Run {
    int status;
    std::string log;
    bool world;
};

Run RunScenario(const fs::path& overlay, const fs::path& script)
{
    const fs::path shots = overlay / "shots";
    fs::create_directories(shots);
    const fs::path out = overlay / "log.txt";
    const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_AUDIO=null IWD2R_SCRIPT=\"" + script.string()
        + "\" IWD2R_SHOT_DIR=\"" + shots.string() + "\" IWD2_DATA=\"" + overlay.string() + "\" timeout 300 \"" IWD2R_EXE "\" > \""
        + out.string() + "\" 2>&1";
    const int raw = std::system(full.c_str());
    Run run;
    run.status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
    run.log = ReadFile(out);
    run.world = fs::exists(shots / "world.png");
    return run;
}

// The game file a Done of Party Formation leaves in mpsave/default.
std::string SavedGame(const fs::path& overlay)
{
    return ReadFile(overlay / "mpsave" / "default" / "ICEWIND2.GAM");
}

// Checks the GAMEV2.2 layout CInfGame::Marshal writes: the header's tables
// follow each other and end with the area-level records.
void CheckGameFile(const std::string& gam)
{
    REQUIRE(gam.size() >= 0xB4);
    CHECK(gam.compare(0, 8, "GAMEV2.2") == 0);
    const uint32_t partyOffset = Le32(gam, 0x20);
    const uint32_t partyCount = Le32(gam, 0x24);
    CHECK(partyOffset == 0xB4);
    REQUIRE(partyOffset + 0x340 * partyCount <= gam.size());
    uint32_t end = partyOffset + 0x340 * partyCount;
    for (uint32_t n = 0; n < partyCount; n++) {
        const uint32_t size = Le32(gam, partyOffset + 0x340 * n + 8);
        CHECK(size > 0);
        end += size;
    }
    CHECK(Le32(gam, 0x30) == end);
    CHECK(Le32(gam, 0x34) == 0);
}

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

}

TEST_CASE("Party Formation's Done saves a game file the engine loads back", "[gam][data]")
{
    const char* data = DataDir();
    if (data == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }

    const fs::path overlay = MakeOverlay(data);
    const Run run = RunScenario(overlay, fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "pending" / "world_entry.txt");
    const std::string gam = SavedGame(overlay);
    std::error_code ec;
    fs::remove_all(overlay, ec);

    INFO("output:\n"
        << run.log);
    CHECK(run.status == 0);
    // LoadGame read the file back and reached the world screen.
    CHECK(run.world);

    CheckGameFile(gam);
    // One party member; the master area and the current area are the start
    // area, and the member stands where the original puts it (world_entry's
    // recording: AR1000, 2414,897).
    CHECK(Le32(gam, 0x24) == 1);
    CHECK(gam.compare(0x58, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0x40, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0xB4 + 0x18, 6, std::string("AR1000", 6)) == 0);
    CHECK((Le32(gam, 0xB4 + 0x20) & 0xFFFF) == 2414);
    CHECK((Le32(gam, 0xB4 + 0x20) >> 16) == 897);
}

TEST_CASE("Party Formation's Done clicked twice starts one game with one party", "[gam][data]")
{
    const char* data = DataDir();
    if (data == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }

    // world_entry with Party Formation's Done clicked a second time two
    // frames after the first (Review Focus 4).
    std::string script = ReadFile(fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "pending" / "world_entry.txt");
    const std::string done = "click 634 507\n";
    const size_t at = script.find(done);
    REQUIRE(at != std::string::npos);
    script.insert(at + done.size(), "wait 2\n" + done);

    const fs::path overlay = MakeOverlay(data);
    const fs::path scriptPath = overlay / "double_done.txt";
    std::ofstream(scriptPath, std::ios::binary) << script;
    const Run run = RunScenario(overlay, scriptPath);
    const std::string gam = SavedGame(overlay);
    std::error_code ec;
    fs::remove_all(overlay, ec);

    INFO("output:\n"
        << run.log);
    CHECK(run.status == 0);
    CHECK(run.world);
    CheckGameFile(gam);
    CHECK(Le32(gam, 0x24) == 1);
}

TEST_CASE("character export over an existing file in Characters does not crash", "[gam][data]")
{
    const char* data = DataDir();
    if (data == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }

    // Review Focus 5. CInfGame::CharacterExport writes through the CHR
    // resource of the name it is given and so overwrites an existing file;
    // choosing a free name (PROBE00 and so on) is the caller's job
    // (CScreenCreateChar). A junk PROBE.CHR must not stop character creation.
    const fs::path overlay = MakeOverlay(data);
    fs::path characters;
    for (const auto& entry : fs::directory_iterator(overlay)) {
        if (Lower(entry.path().filename().string()) == "characters")
            characters = entry.path();
    }
    REQUIRE(!characters.empty());
    for (const auto& entry : fs::directory_iterator(characters)) {
        if (Lower(entry.path().stem().string()).rfind("probe", 0) == 0)
            fs::remove(entry.path());
    }
    std::ofstream(characters / "PROBE.CHR", std::ios::binary) << "junk";

    const Run run = RunScenario(overlay, fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "pending" / "world_entry.txt");
    int exports = 0;
    for (const auto& entry : fs::directory_iterator(characters)) {
        const std::string stem = Lower(entry.path().stem().string());
        if (stem.rfind("probe", 0) == 0 && Lower(entry.path().extension().string()) == ".chr") {
            const std::string chr = ReadFile(entry.path());
            if (chr.compare(0, 8, "CHR V2.2") == 0) {
                exports++;
                CHECK(Le32(chr, 0x20) == 0x224);
                CHECK(Le32(chr, 0x24) + 0x224 == chr.size());
            }
        }
    }
    std::error_code ec;
    fs::remove_all(overlay, ec);

    INFO("output:\n"
        << run.log);
    CHECK(run.status == 0);
    CHECK(exports == 1);
}

#endif
