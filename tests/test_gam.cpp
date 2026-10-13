#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCreateChar.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
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

// The game file (GAM) and the character export (CHR), on the engine in this
// process (NewGame, a sprite from CHARBASE.CRE, the game's own save and load
// functions) and end to end (scenarios/pending/world_entry.txt through
// Party Formation's Done). Every engine works on a private view of the
// install (tests/data_view.h).

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

uint16_t Le16(const std::string& d, size_t at)
{
    return static_cast<uint16_t>(uint16_t(static_cast<unsigned char>(d[at])) | (uint16_t(static_cast<unsigned char>(d[at + 1])) << 8));
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

// The entry of `dir` whose name matches `name` without regard to case.
fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

// ---- The engine in this process ----------------------------------------

// The engine as iwd2r_main builds it, on a private view of the install that
// lives until the process ends (as tests/test_cre.cpp does). Built once and
// leaked: the engine's globals live until the process ends.
fs::path g_view;

bool StartEngine(std::string& error)
{
    static bool started = false;
    static std::string startError;
    static bool tried = false;
    if (tried) {
        error = startError;
        return started;
    }
    tried = true;

#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
    static std::unique_ptr<DataView> view;
    const std::string original = DataDir();
    view = std::make_unique<DataView>(original);
    g_view = view->Path();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", view->Path().c_str());
#else
    setenv("IWD2_DATA", view->Path().c_str(), 1);
#endif
    const bool found = platform::ResolveDataDir();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", original.c_str());
#else
    setenv("IWD2_DATA", original.c_str(), 1);
#endif
    if (!found) {
        startError = "data folder not found";
        return false;
    }
    // SaveGame copies into mpsave/default, which an install made without
    // saves may lack.
    std::error_code ec;
    if (!fs::exists(Child(g_view, "mpsave"), ec)) {
        fs::create_directories(g_view / "mpsave", ec);
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        return false;
    }
    chitin->Init(NULL);
    // CChitin::InitInstance (0x790080) without its threads: every engine
    // loads its panels, which UpdatePortraitToolTip and the save read.
    POSITION pos = chitin->lEngines.GetHeadPosition();
    while (pos != NULL) {
        CWarp* pEngine = static_cast<CWarp*>(chitin->lEngines.GetNext(pos));
        if (pEngine != NULL) {
            pEngine->EngineInitialized();
        }
    }
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

// Stands in for the main loop the tests do not run: the engine's waits for
// a drawn frame (CInfGame::sub_59FA00 spins until m_bDisplayStale clears)
// return as soon as the flag is raised.
class FramePump {
public:
    FramePump()
        : m_running(true)
        , m_thread([this] {
            while (m_running) {
                g_pChitin->m_bDisplayStale = FALSE;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        })
    {
    }
    ~FramePump()
    {
        m_running = false;
        m_thread.join();
    }
    FramePump(const FramePump&) = delete;
    FramePump& operator=(const FramePump&) = delete;

private:
    std::atomic<bool> m_running;
    std::thread m_thread;
};

// The CRE of a finished character: the first "CHR V2.2" file in the view's
// Characters folder (the install ships premade characters). Empty when
// there is none.
std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        const std::string chr = ReadFile(file);
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

// A new game with one party member made from a finished character, the way
// character creation's Accept leaves it before Party Formation's Done.
CGameSprite* NewGameWithOneCharacter(std::vector<BYTE>& bytes, const char* name)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        -1,
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    pSprite->m_sName = name;
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    return pSprite;
}

int LoadedAreas(CInfGame* pGame)
{
    int count = 0;
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            count++;
    }
    return count;
}

// How many times `id` is on the area's object lists, counting the lists of
// objects still waiting to be added (AddToArea queues them until the area's
// next update).
int TimesInArea(CGameArea* pArea, LONG id)
{
    int count = 0;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight,
        &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            if (static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos))) == id)
                count++;
        }
    }
    return count;
}

std::string SavedGame()
{
    return ReadFile(Child(Child(g_view, "mpsave"), "default") / "ICEWIND2.GAM");
}

// Checks the GAMEV2.2 layout CInfGame::Marshal writes.
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
        CHECK(Le32(gam, partyOffset + 0x340 * n + 4) == end);
        // The member's CRE data, copied from the marshal (a zero block when
        // its handle was unknown, compat/marshal_ptr.h).
        CHECK(gam.compare(end, 8, "CRE V2.2") == 0);
        end += size;
    }
    CHECK(Le32(gam, 0x30) == end);
    CHECK(Le32(gam, 0x34) == 0);
}

// ---- The engine as a child process -------------------------------------

#ifndef _WIN32
struct Run {
    int status;
    std::string log;
    bool world;
};

Run RunScenario(const std::string& view, const fs::path& script)
{
    const fs::path shots = fs::path(view).parent_path() / "shots";
    fs::create_directories(shots);
    const fs::path out = fs::path(view).parent_path() / "log.txt";
    const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_AUDIO=null IWD2R_SCRIPT=\"" + script.string()
        + "\" IWD2R_SHOT_DIR=\"" + shots.string() + "\" IWD2_DATA=\"" + view + "\" timeout 150 \"" IWD2R_EXE "\" > \""
        + out.string() + "\" 2>&1";
    const int raw = std::system(full.c_str());
    Run run;
    run.status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
    run.log = ReadFile(out);
    run.world = fs::exists(shots / "world.png");
    return run;
}
#endif

}

TEST_CASE("gam: SaveGame's game file loads back and saves again byte for byte", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    REQUIRE(pSprite->GetArea() == pGame->m_pGameAreaMaster);

    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const std::string first = SavedGame();
    CheckGameFile(first);
    CHECK(Le32(first, 0x24) == 1);
    CHECK(first.compare(0x58, 6, std::string("AR1000", 6)) == 0);
    CHECK(first.compare(0xB4 + 0x18, 6, std::string("AR1000", 6)) == 0);

    // A second save of the same game writes the same bytes.
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    const std::string second = SavedGame();
    CHECK(second == first);

    // Unmarshal of the written file (through LoadGame, which reads it from
    // the save folder) gives the same master area and party count.
    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CHECK(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    CHECK(pGame->m_nCharacters == 1);
    CHECK(LoadedAreas(pGame) == 1);
}

TEST_CASE("gam: a game file naming a second unsaved area loads only the master area", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    // Review Focus 2: the party member stands in AR1001, an area the save has
    // no ARE for; only the master area AR1000 loads, and nothing asserts.
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    NewGameWithOneCharacter(cre, "Probe");
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));

    const fs::path file = Child(Child(g_view, "mpsave"), "default") / "ICEWIND2.GAM";
    std::string gam = ReadFile(file);
    CheckGameFile(gam);
    gam.replace(0xB4 + 0x18, 8, std::string("AR1001\0\0", 8));
    std::ofstream(file, std::ios::binary | std::ios::trunc) << gam;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(file.parent_path(), ec)) {
        CHECK(Lower(entry.path().filename().string()) != "ar1001.are");
    }

    pGame->DestroyGame(FALSE, FALSE);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CHECK(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    CHECK(LoadedAreas(pGame) == 1);
    CHECK(pGame->GetArea(CString("AR1001")) == NULL);
}

TEST_CASE("gam: SetupCharacters twice adds the party once", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    // Review Focus 4: Party Formation's Done clicked twice runs
    // UpdateCharacterSlots and SetupCharacters twice.
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    const LONG id = pSprite->GetId();

    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    REQUIRE(pSprite->GetArea() != NULL);
    const CPoint placed = pSprite->GetPos();
    // The first call placed the sprite: it is no longer at (-1, -1).
    CHECK(placed.x != -1);
    CHECK(TimesInArea(pSprite->GetArea(), id) == 1);

    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    CHECK(pGame->m_nCharacters == 1);
    CHECK(pGame->m_characters[0] == id);
    CHECK(pGame->m_characters[1] == CGameObjectArray::INVALID_INDEX);
    CHECK(LoadedAreas(pGame) == 1);
    CHECK(TimesInArea(pSprite->GetArea(), id) == 1);
    CHECK(pSprite->GetPos().x == placed.x);
    CHECK(pSprite->GetPos().y == placed.y);
}

TEST_CASE("gam: character export overwrites a junk PROBE.CHR and numbers past a valid one", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    // Review Focus 5. CScreenCreateChar::ExportCharacter (0x614AE0) numbers
    // the name (PROBE00, PROBE01...) only when it is on the export list, and
    // CInfGame::GetImportCharacters (0x5C3240) lists only files that start
    // with "CHR V2.2" (the compare at 0x5C341A). So the original exports over
    // a junk PROBE.CHR (CInfGame::CharacterExport writes through the
    // resource and overwrites it), and a second export, with a valid
    // PROBE.CHR now there, goes to PROBE00.CHR and leaves PROBE.CHR alone.
    const fs::path characters = Child(g_view, "Characters");
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(characters, ec)) {
        if (Lower(entry.path().stem().string()).rfind("probe", 0) == 0)
            fs::remove(entry.path());
    }
    std::ofstream(characters / "PROBE.CHR", std::ios::binary) << "junk";

    CGameSprite* pSprite = NewGameWithOneCharacter(cre, "Probe");
    REQUIRE(g_pBaldurChitin->m_pEngineCreateChar->ExportCharacter(pSprite));

    const std::string chr = ReadFile(characters / "PROBE.CHR");
    REQUIRE(chr.size() > 0x224);
    CHECK(!fs::exists(Child(characters, "PROBE00.CHR")));

    // The CHR V2.2 header (IESDP), field by field against the sprite.
    CHECK(chr.compare(0, 8, "CHR V2.2") == 0);
    CHECK(chr.compare(0x08, 6, std::string("Probe\0", 6)) == 0);
    CHECK(Le32(chr, 0x28) == 0x224);
    CHECK(Le32(chr, 0x2C) + 0x224 == chr.size());
    CHECK(chr.compare(0x224, 8, "CRE V2.2") == 0);
    for (int n = 0; n < 8; n++) {
        CHECK(static_cast<SHORT>(Le16(chr, 0x30 + 2 * n)) == pSprite->m_quickWeapons[n].m_abilityId.m_itemNum);
        CHECK(static_cast<SHORT>(Le16(chr, 0x40 + 2 * n)) == pSprite->m_quickWeapons[n].m_abilityId.m_abilityNum);
        CHECK(static_cast<BYTE>(chr[0x1A4 + n]) == pSprite->field_3D3A[n]);
    }
    for (int n = 0; n < 9; n++) {
        BYTE resref[RESREF_SIZE];
        pSprite->m_quickSpells[n].m_abilityId.m_res.GetResRef(resref);
        CHECK(memcmp(chr.data() + 0x50 + RESREF_SIZE * n, resref, RESREF_SIZE) == 0);
        CHECK(static_cast<BYTE>(chr[0x98 + n]) == pSprite->m_quickSpells[n].m_abilityId.m_nClass);
        pSprite->m_quickInnates[n].m_abilityId.m_res.GetResRef(resref);
        CHECK(memcmp(chr.data() + 0xAE + RESREF_SIZE * n, resref, RESREF_SIZE) == 0);
        pSprite->m_quickSongs[n].m_abilityId.m_res.GetResRef(resref);
        CHECK(memcmp(chr.data() + 0xF6 + RESREF_SIZE * n, resref, RESREF_SIZE) == 0);
        CHECK(static_cast<int>(Le32(chr, 0x13E + 4 * n)) == pSprite->field_3D14[n]);
    }
    for (int n = 0; n < 3; n++) {
        CHECK(static_cast<SHORT>(Le16(chr, 0xA2 + 2 * n)) == pSprite->m_quickItems[n].m_abilityId.m_itemNum);
        CHECK(static_cast<SHORT>(Le16(chr, 0xA8 + 2 * n)) == pSprite->m_quickItems[n].m_abilityId.m_abilityNum);
    }
    CHECK(static_cast<BYTE>(chr[0x162]) == pSprite->m_nLastSpellbookClassIndex);
    CHECK(static_cast<BYTE>(chr[0x163]) == pSprite->m_nLastSpellbookSpellLevel);
    CHECK(static_cast<BYTE>(chr[0x178]) == pSprite->m_nLastSong);

    REQUIRE(g_pBaldurChitin->m_pEngineCreateChar->ExportCharacter(pSprite));
    CHECK(ReadFile(characters / "PROBE.CHR") == chr);
    const std::string numbered = ReadFile(Child(characters, "PROBE00.CHR"));
    CHECK(numbered.compare(0, 8, "CHR V2.2") == 0);
    CHECK(numbered.size() == chr.size());
}

#ifndef _WIN32
TEST_CASE("gam: Party Formation's Done saves a game file the engine loads back", "[gam][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }

    DataView view(DataDir());
    // As StartEngine: SaveGame copies into mpsave/default.
    std::error_code ec;
    if (!fs::exists(Child(view.Path(), "mpsave"), ec)) {
        fs::create_directories(fs::path(view.Path()) / "mpsave", ec);
    }
    const Run run = RunScenario(view.Path(), fs::path(IWD2R_SOURCE_DIR) / "scenarios" / "pending" / "world_entry.txt");
    const std::string gam = ReadFile(Child(Child(view.Path(), "mpsave"), "default") / "ICEWIND2.GAM");

    INFO("output:\n"
        << run.log);
    CHECK(run.status == 0);
    // LoadGame read the file back and reached the world screen.
    CHECK(run.world);

    CheckGameFile(gam);
    // One party member in the start area where the original puts it
    // (world_entry's recording: AR1000, 2414,897).
    CHECK(Le32(gam, 0x24) == 1);
    CHECK(gam.compare(0x58, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0x40, 6, std::string("AR1000", 6)) == 0);
    CHECK(gam.compare(0xB4 + 0x18, 6, std::string("AR1000", 6)) == 0);
    CHECK(Le16(gam, 0xB4 + 0x20) == 2414);
    CHECK(Le16(gam, 0xB4 + 0x22) == 897);
}
#endif
