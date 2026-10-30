#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CScreenLoad.h"
#include "CScreenSave.h"
#include "CTimerWorld.h"
#include "CUIControlEdit.h"
#include "CUIPanel.h"
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

// The save and load screens (CScreenSave, CScreenLoad) and the time string
// their slots show (CTimerWorld::GetCurrentTimeString), on the engine in this
// process. Saves go only into the private view's mpsave folder
// (tests/data_view.h), never into the install.

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

// The engine as iwd2r_main builds it, on a private view of the install (as
// tests/test_gam.cpp does), every engine's panels loaded. Built once and
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

// Stands in for the main loop: the engine's waits for a drawn frame return
// as soon as they begin.
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

// The CRE of the first finished character in the view's Characters folder.
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

// A new game in AR1000 with one party member, as Party Formation's Done
// leaves it before it saves.
void NewGameWithOneCharacter(std::vector<BYTE>& bytes, const char* name)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        static_cast<DWORD>(-1),
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    pSprite->m_sName = name;
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }
}

fs::path SaveRoot()
{
    return Child(g_view, "mpsave");
}

// The save folders in the view, by name.
std::vector<std::string> SaveFolders()
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(SaveRoot(), ec)) {
        if (entry.is_directory())
            names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// Empties the view's mpsave folder (it is the view's own copy).
void ClearSaves()
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(SaveRoot(), ec))
        fs::remove_all(entry.path(), ec);
}

void CopySave(const std::string& from, const std::string& to)
{
    fs::copy(SaveRoot() / from, SaveRoot() / to, fs::copy_options::recursive);
}

std::string Token(const char* name)
{
    CString value;
    g_pBaldurChitin->GetTlkTable().m_mapTokens.Lookup(name, value);
    return std::string(value);
}

// The index of the slot whose file name is `name`, or -1.
template <class Screen>
INT SlotOf(Screen* pScreen, const char* name)
{
    for (INT n = 0; n < pScreen->m_nNumGameSlots; n++) {
        if (pScreen->m_aGameSlots[n]->m_sFileName == name)
            return n;
    }
    return -1;
}

}

// MONTHS.2DA has twelve months of 30 days and five festivals of one day;
// YEARS.2DA starts the game at 878400 seconds (day 122, the first of Mirtul)
// of 1312, and names the day with "<DAY> <MONTHNAME>" (15981) or, on a
// festival, "<MONTHNAME>" (16001). The save slots show 20670,
// "<DURATIONNOAND>", the time played.
TEST_CASE("time: GetCurrentTimeString sets the date and duration tokens from MONTHS and YEARS", "[timer][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CString sTime;
    CTimerWorld::GetCurrentTimeString(0, 20670, sTime);
    CHECK(std::string(sTime) == "0 hours");
    CHECK(Token("MINUTE") == "0");
    CHECK(Token("HOUR") == "0");
    CHECK(Token("DAY") == "1");
    CHECK(Token("MONTH") == "7");
    CHECK(Token("YEAR") == "1312");
    CHECK(Token("MONTHNAME") == "Mirtul");
    CHECK(Token("DAYANDMONTH") == "1 Mirtul");
    CHECK(Token("GAMEDAY") == "0");
    CHECK(Token("GAMEDAYS") == "0");
    CHECK(Token("DURATION") == "0 hours");
    CHECK(Token("DURATIONNOAND") == "0 hours");

    // A day, an hour and three tenths of it (the original's "minute" is a
    // fifth of an hour: 900 ticks).
    CTimerWorld::GetCurrentTimeString(CTimerWorld::TIMESCALE_MSEC_PER_DAY + CTimerWorld::TIMESCALE_MSEC_PER_HOUR + 3 * 900, 20670, sTime);
    CHECK(std::string(sTime) == "1 day 1 hour");
    CHECK(Token("MINUTE") == "3");
    CHECK(Token("HOUR") == "1");
    CHECK(Token("DAY") == "2");
    CHECK(Token("MONTH") == "7");
    CHECK(Token("DURATION") == "1 day and 1 hour");

    // 364 days on: day 121 of the next year, Greengrass, a festival.
    CTimerWorld::GetCurrentTimeString(364 * CTimerWorld::TIMESCALE_MSEC_PER_DAY, 20670, sTime);
    CHECK(std::string(sTime) == "364 days 0 hours");
    CHECK(Token("DAY") == "1");
    CHECK(Token("MONTH") == "6");
    CHECK(Token("YEAR") == "1313");
    CHECK(Token("MONTHNAME") == "Greengrass");
    CHECK(Token("DAYANDMONTH") == "Greengrass");
    CHECK(Token("GAMEDAYS") == "364");
    CHECK(Token("DURATION") == "364 days and 0 hours");
}

// RefreshGameSlots (0x65BCF0) lists every save but "default" and then the
// empty slot a new save goes into; numbers 1 to 3 (the quick and auto
// saves) do not raise the highest slot number.
TEST_CASE("save screen: the slots list each save with its name and time and chapter and then an empty slot", "[save][data]")
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
    ClearSaves();
    NewGameWithOneCharacter(cre, "Probe");
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    REQUIRE(fs::exists(Child(SaveRoot(), "default") / "ICEWIND2.GAM"));
    CopySave("default", "000000002-QuickSave");
    CopySave("default", "000000007-Alpha");
    CopySave("default", "Beta");

    CScreenSave* pSave = g_pBaldurChitin->m_pEngineSave;
    pSave->StartSave(2);
    REQUIRE(pSave->m_nNumGameSlots == 4);
    CHECK(pSave->m_nMaxSlotNumber == 7);
    // The newest slots are in view.
    CHECK(pSave->m_nTopGameSlot == 0);

    const INT alpha = SlotOf(pSave, "000000007-Alpha");
    const INT quick = SlotOf(pSave, "000000002-QuickSave");
    const INT beta = SlotOf(pSave, "Beta");
    REQUIRE(alpha >= 0);
    REQUIRE(quick >= 0);
    REQUIRE(beta >= 0);
    CHECK(pSave->m_aGameSlots[alpha]->m_sSlotName == "Alpha");
    CHECK(pSave->m_aGameSlots[quick]->m_sSlotName == "QuickSave");
    CHECK(pSave->m_aGameSlots[beta]->m_sSlotName == "Beta");

    const std::string gam = ReadFile(Child(SaveRoot(), "default") / "ICEWIND2.GAM");
    for (INT n : { alpha, quick, beta }) {
        CScreenSaveGameSlot* pSlot = pSave->m_aGameSlots[n];
        INFO(pSlot->m_sFileName);
        CHECK(pSlot->m_nTime == Le32(gam, 0x8 + 0x0) * CTimerWorld::TIMESCALE_MSEC_PER_SEC);
        CHECK(pSlot->m_sCharacterName != "");
        CHECK(pSlot->m_sChapter != "");
        CHECK(pSlot->field_314 != "");
    }

    // The last slot is the empty one: no file, "Empty" on the panel.
    CScreenSaveGameSlot* pEmpty = pSave->m_aGameSlots[pSave->m_nNumGameSlots - 1];
    CHECK(pEmpty->m_sFileName == "");
    CHECK(pEmpty->m_cResScreenShot.m_pData == NULL);

    pSave->StopSave();
    CHECK(pSave->m_nNumGameSlots == 0);
    ClearSaves();
}

// CScreenSave::SaveGame (0x65DB30) names a new save one past the highest
// number, keeps an overwritten save's number when it has one, and frees the
// slots as it leaves (StopSave). The load screen then lists the saves
// without an empty slot and loads one.
TEST_CASE("save screen: saving a new slot and overwriting one and the load screen loads it", "[save][load][data]")
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
    ClearSaves();
    NewGameWithOneCharacter(cre, "Probe");
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    CopySave("default", "000000002-QuickSave");

    CScreenSave* pSave = g_pBaldurChitin->m_pEngineSave;

    // A new save in the empty slot: no save above 3, so number 4.
    pSave->StartSave(2);
    REQUIRE(pSave->m_nNumGameSlots == 2);
    CHECK(pSave->m_nMaxSlotNumber == -1);
    pSave->SaveGame(pSave->m_nNumGameSlots - 1, CString("Gamma"));
    CHECK(pSave->m_nNumGameSlots == 0);
    CHECK(fs::exists(Child(Child(SaveRoot(), "000000004-Gamma"), "ICEWIND2.GAM")));

    // Overwriting it keeps its number and takes the new name.
    pSave->StartSave(2);
    REQUIRE(pSave->m_nNumGameSlots == 3);
    CHECK(pSave->m_nMaxSlotNumber == 4);
    const INT gamma = SlotOf(pSave, "000000004-Gamma");
    REQUIRE(gamma >= 0);
    pSave->SaveGame(gamma, CString("Delta"));
    std::vector<std::string> folders = SaveFolders();
    CHECK(std::count(folders.begin(), folders.end(), "000000004-Delta") == 1);
    CHECK(std::count(folders.begin(), folders.end(), "000000004-Gamma") == 0);

    // A second new save goes one past the highest number.
    pSave->StartSave(2);
    pSave->SaveGame(pSave->m_nNumGameSlots - 1, CString("Epsilon"));
    folders = SaveFolders();
    CHECK(std::count(folders.begin(), folders.end(), "000000005-Epsilon") == 1);

    // The load screen: the saves but "default", no empty slot.
    CScreenLoad* pLoad = g_pBaldurChitin->m_pEngineLoad;
    pLoad->StartLoad(2);
    REQUIRE(pLoad->m_nNumGameSlots == 3);
    const INT delta = SlotOf(pLoad, "000000004-Delta");
    REQUIRE(delta >= 0);
    CHECK(pLoad->m_aGameSlots[delta]->m_sSlotName == "Delta");
    CHECK(pLoad->m_aGameSlots[delta]->m_sChapter != "");
    CHECK(pLoad->m_aGameSlots[delta]->m_sFileTime != "");
    CHECK(pLoad->m_aGameSlots[delta]->m_sCharacterName != "");

    // Loading it brings the party back in AR1000.
    pGame->DestroyGame(FALSE, FALSE);
    REQUIRE(delta - pLoad->m_nTopGameSlot >= 0);
    REQUIRE(delta - pLoad->m_nTopGameSlot < 5);
    // The world screen it selects sets the channel volumes: the mixer that
    // CChitin::InitializeServices (0x790240) starts with the video.
    g_pChitin->cSoundMixer.Initialize(&g_pChitin->cWnd, 16, g_pChitin->GetNumberSoundChannels());
    pLoad->LoadGame(delta - pLoad->m_nTopGameSlot);
    CHECK(pLoad->m_nNumGameSlots == 0);
    CHECK(pGame->m_sSaveGame == "000000004-Delta");
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CHECK(pGame->m_pGameAreaMaster->m_resRef == "AR1000");
    CHECK(pGame->m_nCharacters == 1);
    ClearSaves();
}

// The delete popup (OnDeleteButtonClick, OnErrorButtonClick 0x65D970):
// "Cancel" only closes it, "Delete" removes the save and lists the slots
// again.
TEST_CASE("save screen: the delete popup's Cancel keeps the save and its Delete removes it", "[save][data]")
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
    ClearSaves();
    NewGameWithOneCharacter(cre, "Probe");
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));
    CopySave("default", "000000004-Alpha");

    CScreenSave* pSave = g_pBaldurChitin->m_pEngineSave;
    pSave->StartSave(2);
    REQUIRE(pSave->m_nNumGameSlots == 2);
    const INT alpha = SlotOf(pSave, "000000004-Alpha");
    REQUIRE(alpha == 0);

    pSave->OnDeleteButtonClick(alpha - pSave->m_nTopGameSlot);
    REQUIRE(pSave->GetTopPopup() != NULL);
    CHECK(pSave->GetTopPopup()->m_nID == 2);
    pSave->OnErrorButtonClick(1);
    CHECK(pSave->GetTopPopup() == NULL);
    CHECK(fs::exists(SaveRoot() / "000000004-Alpha"));

    // The empty slot has nothing to delete.
    pSave->OnDeleteButtonClick(pSave->m_nNumGameSlots - 1 - pSave->m_nTopGameSlot);
    CHECK(pSave->GetTopPopup() == NULL);

    pSave->OnDeleteButtonClick(alpha - pSave->m_nTopGameSlot);
    pSave->OnErrorButtonClick(0);
    CHECK(pSave->GetTopPopup() == NULL);
    CHECK_FALSE(fs::exists(SaveRoot() / "000000004-Alpha"));
    CHECK(pSave->m_nNumGameSlots == 1);

    pSave->StopSave();
    ClearSaves();
}

// The Save As popup (OnSaveButtonClick, ResetSaveAsPanel 0x65D360,
// OnDoneButtonClick 0x65B6A0): the name is trimmed, a name with a
// forbidden character (the backslash among them) brings up the error popup
// over it, and a good name saves.
TEST_CASE("save screen: the Save As popup checks the name and saves under it trimmed", "[save][data]")
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
    ClearSaves();
    NewGameWithOneCharacter(cre, "Probe");
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));

    CScreenSave* pSave = g_pBaldurChitin->m_pEngineSave;
    pSave->StartSave(2);
    REQUIRE(pSave->m_nNumGameSlots == 1);

    pSave->OnSaveButtonClick(0);
    CUIPanel* pPanel = pSave->GetTopPopup();
    REQUIRE(pPanel != NULL);
    REQUIRE(pPanel->m_nID == 1);
    CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(3));
    REQUIRE(pEdit != NULL);
    // The empty slot's popup offers "Save" and an empty name.
    CHECK(pEdit->GetText() == "");
    CHECK(pSave->m_aGameSlots[0]->m_sChapter != "");

    pEdit->SetText(CString("a\\b"));
    pSave->OnDoneButtonClick();
    REQUIRE(pSave->GetTopPopup() != NULL);
    CHECK(pSave->GetTopPopup()->m_nID == 3);
    CHECK(pSave->m_dwErrorTextId == 17133);
    CHECK(pSave->m_strErrorButtonText[0] == 11973);
    pSave->OnErrorButtonClick(0);
    REQUIRE(pSave->GetTopPopup() != NULL);
    CHECK(pSave->GetTopPopup()->m_nID == 1);

    pEdit->SetText(CString("  Zeta  "));
    pSave->OnDoneButtonClick();
    CHECK(pSave->GetTopPopup() == NULL);
    CHECK(pSave->m_nNumGameSlots == 0);
    CHECK(fs::exists(Child(Child(SaveRoot(), "000000004-Zeta"), "ICEWIND2.GAM")));
    ClearSaves();
}

// CInfGame::AddCharacterToParty (0x5BD070) puts a loaded party member into
// m_characters as well as m_characterPortraits; CInfGame::Marshal (0x5A3230)
// walks m_characters, so without it every save after a load asserted.
TEST_CASE("save screen: a loaded party fills m_characters and saves from the world", "[save][gam][data]")
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
    ClearSaves();
    NewGameWithOneCharacter(cre, "Probe");
    REQUIRE(pGame->SaveGame(FALSE, FALSE, FALSE));

    pGame->DestroyGame(FALSE, FALSE);
    for (int n = 0; n < CINFGAME_MAXCHARACTERS; n++)
        CHECK(pGame->m_characters[n] == CGameObjectArray::INVALID_INDEX);
    pGame->LoadGame(FALSE, FALSE);
    REQUIRE(pGame->m_nCharacters == 1);
    CHECK(pGame->m_characterPortraits[0] != CGameObjectArray::INVALID_INDEX);
    CHECK(pGame->m_characters[0] == pGame->m_characterPortraits[0]);
    for (int n = 1; n < CINFGAME_MAXCHARACTERS; n++)
        CHECK(pGame->m_characters[n] == CGameObjectArray::INVALID_INDEX);

    // The save screen's save of the loaded game marshals the party.
    CScreenSave* pSave = g_pBaldurChitin->m_pEngineSave;
    pSave->StartSave(2);
    REQUIRE(pSave->m_nNumGameSlots == 1);
    pSave->SaveGame(0, CString("Reloaded"));
    const fs::path gam = Child(Child(SaveRoot(), "000000004-Reloaded"), "ICEWIND2.GAM");
    REQUIRE(fs::exists(gam));
    const std::string bytes = ReadFile(gam);
    REQUIRE(bytes.size() >= 0xB4);
    CHECK(Le32(bytes, 0x24) == 1);
    // CInfGame::SaveGame (0x5AC430) writes the world map with the save.
    const std::string wmp = ReadFile(Child(Child(SaveRoot(), "000000004-Reloaded"), "WORLDMAP.WMP"));
    REQUIRE(wmp.size() > 16);
    CHECK(wmp.compare(0, 8, "WMAPV1.0") == 0);
    ClearSaves();
}

// CVidInf::PrintSurfaceToBmp (0x79EC20) writes a rectangle of a surface as a
// 24-bit BMP, each pixel the average of an nScale-square block, the rows
// bottom up, the last column taking what is left of the width. The test
// gives the video mode one surface of the DirectDraw shim (XRGB8888, as the
// running engine's surfaces are) and restores it after.
TEST_CASE("save screen: PrintSurfaceToBmp averages surface blocks into a bottom-up 24-bit BMP", "[save][vid][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();
    REQUIRE(pVidMode != NULL);

    IDirectDraw* pDirectDraw = NULL;
    REQUIRE(DirectDrawCreate(NULL, &pDirectDraw, NULL) == DD_OK);
    DDSURFACEDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OFFSCREENPLAIN;
    desc.dwWidth = 32;
    desc.dwHeight = 32;
    IDirectDrawSurface* pSurface = NULL;
    REQUIRE(pDirectDraw->CreateSurface(&desc, &pSurface, NULL) == DD_OK);

    const UINT nSurfacesBefore = pVidMode->m_nSurfaces;
    IDirectDrawSurface** pSurfacesBefore = pVidMode->pSurfaces;
    const USHORT nBppBefore = g_pChitin->cVideo.m_nBpp;
    const DWORD shifts[3] = { pVidMode->m_dwRBitShift, pVidMode->m_dwGBitShift, pVidMode->m_dwBBitShift };
    IDirectDrawSurface* surfaces[1] = { pSurface };
    pVidMode->m_nSurfaces = 1;
    pVidMode->pSurfaces = surfaces;
    g_pChitin->cVideo.m_nBpp = 32;
    pVidMode->m_dwRBitShift = 16;
    pVidMode->m_dwGBitShift = 8;
    pVidMode->m_dwBBitShift = 0;

    // A 5 by 4 rectangle at 10,20: column x, row y has red 10 * x + y,
    // green 100 + y, blue 7.
    auto value = [](int x, int y, int channel) -> int {
        return channel == 0 ? 10 * x + y : channel == 1 ? 100 + y
                                                        : 7;
    };
    const CRect rect(10, 20, 15, 24);
    DDSURFACEDESC ddsd;
    ddsd.dwSize = sizeof(ddsd);
    REQUIRE(pVidMode->LockSurface(0, &ddsd, rect));
    for (int y = 0; y < 4; y++) {
        DWORD* row = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch);
        for (int x = 0; x < 5; x++)
            row[x] = (DWORD(value(x, y, 0)) << 16) | (DWORD(value(x, y, 1)) << 8) | DWORD(value(x, y, 2));
    }
    pVidMode->UnLockSurface(0, ddsd.lpSurface);

    LPBYTE data = NULL;
    LONG size = 0;
    const BOOLEAN ok = pVidMode->PrintSurfaceToBmp(data, 0, rect, size, 2);

    pVidMode->m_nSurfaces = nSurfacesBefore;
    pVidMode->pSurfaces = pSurfacesBefore;
    g_pChitin->cVideo.m_nBpp = nBppBefore;
    pVidMode->m_dwRBitShift = shifts[0];
    pVidMode->m_dwGBitShift = shifts[1];
    pVidMode->m_dwBBitShift = shifts[2];
    pSurface->Release();
    pDirectDraw->Release();

    REQUIRE(ok);
    REQUIRE(data != NULL);
    // 2 by 2 output pixels (the odd fifth column joins the second), rows of
    // 6 bytes padded to 8.
    CHECK(size == 54 + 8 * 2);
    const std::string bmp(reinterpret_cast<char*>(data), static_cast<size_t>(size));
    CHECK(bmp.compare(0, 2, "BM") == 0);
    CHECK(Le32(bmp, 2) == static_cast<uint32_t>(size));
    CHECK(Le32(bmp, 10) == 54);
    CHECK(Le32(bmp, 14) == 40);
    CHECK(Le32(bmp, 18) == 2);
    CHECK(Le32(bmp, 22) == 2);
    CHECK((static_cast<unsigned char>(bmp[28]) | (static_cast<unsigned char>(bmp[29]) << 8)) == 24);
    CHECK(Le32(bmp, 46) == 0x1000000);

    auto expect = [&](int col, int blockRow, int channel) -> int {
        const int x0 = col * 2;
        const int w = col == 1 ? 3 : 2;
        int sum = 0;
        for (int y = blockRow * 2; y < blockRow * 2 + 2; y++)
            for (int x = x0; x < x0 + w; x++)
                sum += value(x, y, channel);
        return sum / (w * 2);
    };
    // The first stored row is the bottom block row.
    for (int outRow = 0; outRow < 2; outRow++) {
        const int blockRow = 1 - outRow;
        for (int col = 0; col < 2; col++) {
            const size_t at = 54 + static_cast<size_t>(outRow) * 8 + static_cast<size_t>(col) * 3;
            INFO("row " << outRow << " col " << col);
            CHECK(static_cast<unsigned char>(bmp[at + 0]) == expect(col, blockRow, 2));
            CHECK(static_cast<unsigned char>(bmp[at + 1]) == expect(col, blockRow, 1));
            CHECK(static_cast<unsigned char>(bmp[at + 2]) == expect(col, blockRow, 0));
        }
    }
    delete[] data;
}

TEST_CASE("save: the SaveGame action saves through its message under the strref's name (case 275)", "[save][data]")
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
    ClearSaves();
    NewGameWithOneCharacter(cre, "Probe");
    CGameObject* pObject = NULL;
    REQUIRE(pGame->GetObjectArray()->GetShare(pGame->GetCharacterId(0), CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE)
        == CGameObjectArray::SUCCESS);
    CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);
    pArea->m_header.m_flags |= 0x1;
    const DWORD flags = pArea->m_header.m_flags;

    // AR1000.BCS's SaveGame(16202): the action sends the message and is done.
    pSprite->m_curAction = CAIAction(275, CPoint(-1, -1), 16202, 0);
    CHECK(pSprite->CGameAIBase::ExecuteAction() == CGameAIBase::ACTION_DONE);
    pSprite->m_curAction = CAIAction();
    CHECK(SaveFolders().empty());

    // Without 3D acceleration (the original's setting) the Run first draws
    // the save screenshot through CInfGame::SynchronousUpdate unless
    // field_50D8 is set; there is no world screen to draw in this test.
    pGame->field_50D8 = TRUE;

    // Its Run (0x504CA0) saves under "000000000-<39935> - <16202>" and puts
    // each area's flags back as they were (bit 0 is cleared while saving).
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    const std::string name = (LPCSTR)pGame->m_sSaveGame;
    INFO(name);
    CHECK(name.rfind("000000000-", 0) == 0);
    CHECK(name.find(" - ") != std::string::npos);
    CHECK(std::string((LPCSTR)CScreenCharacter::SAVE_NAME) == name);
    CHECK(pArea->m_header.m_flags == flags);
    const std::vector<std::string> folders = SaveFolders();
    CHECK(std::count(folders.begin(), folders.end(), name) == 1);
    CHECK(fs::exists(Child(Child(SaveRoot(), name), "ICEWIND2.GAM")));
    pArea->m_header.m_flags &= ~0x1;
}
