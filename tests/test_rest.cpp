#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CRuleTables.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenWorld.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Resting: CInfGame::CanRestParty (0x5C1E30), RestParty (0x5C1160) and
// RestPartyPrivate (0x5C1350) on a new game's party in AR1000.

namespace {

namespace fs = std::filesystem;

fs::path g_view;

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

// The engine as iwd2r_main builds it, on a private view of the install
// that lives until the process ends (as tests/test_cre.cpp does). Built
// once and leaked. CBaldurChitin's constructor starts the search thread.
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

// Stands in for the main loop the tests do not run: the engine's waits for
// a drawn frame return as soon as the flag is raised.
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

std::string Lower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

// The CRE of a finished character: the first "CHR V2.2" file in the view's
// Characters folder (the install ships premade characters), as
// tests/test_gam.cpp takes it. Empty when there is none.
std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    fs::path characters = g_view / "Characters";
    for (const auto& entry : fs::directory_iterator(g_view, ec)) {
        if (Lower(entry.path().filename().string()) == "characters")
            characters = entry.path();
    }
    for (const auto& entry : fs::directory_iterator(characters, ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        std::ifstream in(file, std::ios::binary);
        const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

CGameSprite* MakeSprite(std::vector<BYTE>& bytes)
{
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
    pSprite->m_bGlobal = TRUE;
    return pSprite;
}

// A new game whose party (copies of a finished character) stands in the
// first area; empty when the install has no finished character.
std::vector<CGameSprite*> NewGameWithParty(int count)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    std::vector<BYTE> bytes = FinishedCharacter();
    std::vector<CGameSprite*> party;
    if (bytes.empty()) {
        return party;
    }
    for (int n = 0; n < count; n++) {
        CGameSprite* pSprite = MakeSprite(bytes);
        pGame->SetCharacterSlot(n, pSprite->GetId());
        party.push_back(pSprite);
    }
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    return party;
}

BOOL HasQueuedAction(CGameSprite* pSprite, SHORT actionID)
{
    if (pSprite->m_curAction.m_actionID == actionID) {
        return TRUE;
    }
    POSITION pos = pSprite->m_queuedActions.GetHeadPosition();
    while (pos != NULL) {
        if (pSprite->m_queuedActions.GetNext(pos)->m_actionID == actionID) {
            return TRUE;
        }
    }
    return FALSE;
}

}

TEST_CASE("rest: CanRestParty asks to rest a gathered party and refuses a scattered one or a no-rest area", "[rest][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(2);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameArea* pArea = party[0]->GetArea();
    REQUIRE(pArea != NULL);
    REQUIRE(party[1]->GetArea() == pArea);

    STRREF strError = -1;
    // AR1000's header forbids resting (flag 0x2): "You may not rest at
    // this time."
    const DWORD flags = pArea->m_header.m_flags;
    REQUIRE((flags & 0x2) != 0);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    pArea->m_header.m_flags = flags & ~0x2;

    // Both at their start points: the game asks "Are you sure you wish to
    // rest?".
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);
    CHECK(strError == 15358);

    // The second member out of range (the 0x40000 of the squared distance,
    // the vertical part weighted 16/9): "You may not rest when your party
    // is scattered."
    const CPoint home = party[1]->m_pos;
    party[1]->m_pos.x = party[0]->m_pos.x + 600;
    party[1]->m_pos.y = party[0]->m_pos.y;
    strError = -1;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 16457);

    // Just inside: 511^2 + 0 <= 0x40000.
    party[1]->m_pos.x = party[0]->m_pos.x + 511;
    strError = -1;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);

    // Vertically 384 is in range (384^2 * 16 / 9 == 0x40000), 385 is not.
    party[1]->m_pos.x = party[0]->m_pos.x;
    party[1]->m_pos.y = party[0]->m_pos.y + 384;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == TRUE);
    party[1]->m_pos.y = party[0]->m_pos.y + 385;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 16457);

    // The third argument answers TRUE for any gathered party, before the
    // save checks; a scattered one goes on to them.
    party[1]->m_pos = home;
    strError = -1;
    CHECK(pGame->CanRestParty(strError, FALSE, TRUE, FALSE) == TRUE);
    CHECK(strError == 10309);

    // The no-rest flag is ignored when the party rents a room (the second
    // argument) or with the fourth argument.
    pArea->m_header.m_flags |= 0x2;
    strError = -1;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    CHECK(pGame->CanRestParty(strError, TRUE, FALSE, FALSE) == TRUE);
    CHECK(strError == 15358);
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, TRUE) == TRUE);
    pArea->m_header.m_flags = flags & ~0x2;

    // Without the leader permission: the multiplayer refusal.
    pGame->m_singlePlayerPermissions.m_bPermissions[CGamePermission::LEADER] = FALSE;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 17198);
    pGame->m_singlePlayerPermissions.m_bPermissions[CGamePermission::LEADER] = TRUE;

    // A party with nobody alive: nothing but the default refusal.
    const DWORD state0 = party[0]->m_derivedStats.m_generalState;
    const DWORD state1 = party[1]->m_derivedStats.m_generalState;
    party[0]->m_derivedStats.m_generalState |= STATE_DEAD;
    party[1]->m_derivedStats.m_generalState |= STATE_DEAD;
    strError = -1;
    CHECK(pGame->CanRestParty(strError, FALSE, FALSE, FALSE) == FALSE);
    CHECK(strError == 10309);
    party[0]->m_derivedStats.m_generalState = state0;
    party[1]->m_derivedStats.m_generalState = state1;
    pArea->m_header.m_flags = flags;
}

TEST_CASE("rest: RestParty passes the rest time and queues the heal, the Rest action and the rest trigger", "[rest][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(2);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    g_pBaldurChitin->pActiveEngine = g_pBaldurChitin->m_pEngineWorld;

    const BOOL heal = CInfGame::m_bHealPartyOnRest;
    CInfGame::m_bHealPartyOnRest = FALSE;

    for (CGameSprite* pSprite : party) {
        pSprite->m_queuedActions.RemoveAll();
        pSprite->m_pendingTriggers.RemoveAll();
        pSprite->m_baseStats.m_hitPoints = 1;
    }

    const ULONG before = pGame->m_worldTime.m_gameTime;
    CHECK(pGame->RestParty(7, FALSE) == FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    // Eight hours, whatever the wounds.
    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING);

    for (CGameSprite* pSprite : party) {
        // The heal (effect 17, applied as it arrives) gives the points
        // asked for.
        CHECK(pSprite->m_baseStats.m_hitPoints == std::min(8, static_cast<int>(pSprite->m_baseStats.m_maxHitPointsBase)));
        CHECK(HasQueuedAction(pSprite, 96));

        BOOL trigger = FALSE;
        POSITION pos = pSprite->m_pendingTriggers.GetHeadPosition();
        while (pos != NULL) {
            if (pSprite->m_pendingTriggers.GetNext(pos)->m_triggerID == 0x93) {
                trigger = TRUE;
            }
        }
        CHECK(trigger);
    }

    // Renting a room returns TRUE.
    CHECK(pGame->RestParty(7, TRUE) == TRUE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();

    CInfGame::m_bHealPartyOnRest = heal;
}

TEST_CASE("rest: with healing on rest the clock runs by the party's wounds and the heal is full", "[rest][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(2);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    g_pBaldurChitin->pActiveEngine = g_pBaldurChitin->m_pEngineWorld;

    const BOOL heal = CInfGame::m_bHealPartyOnRest;
    CInfGame::m_bHealPartyOnRest = TRUE;

    // The hourly rate: the given points plus, per member, 19 per cleric or
    // druid level plus 5 and, from level 10, 19 per paladin or ranger level
    // less 147.
    INT nRate = 3;
    for (CGameSprite* pSprite : party) {
        INT nLevel = pSprite->m_derivedStats.GetClassMaskLevel(CLASSMASK_CLERIC | CLASSMASK_DRUID);
        if (nLevel > 0) {
            nRate += nLevel * 19 + 5;
        }
        nLevel = pSprite->m_derivedStats.GetClassMaskLevel(CLASSMASK_PALADIN | CLASSMASK_RANGER);
        if (nLevel > 9) {
            nRate += nLevel * 19 - 147;
        }
    }

    // Nobody hurt by more than a point: one rest period.
    for (CGameSprite* pSprite : party) {
        pSprite->m_baseStats.m_hitPoints = pSprite->m_baseStats.m_maxHitPointsBase - 1;
    }
    ULONG before = pGame->m_worldTime.m_gameTime;
    pGame->RestParty(3, FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING);

    // Wounds within an hour's healing: two rest periods.
    party[0]->m_baseStats.m_hitPoints = party[0]->m_baseStats.m_maxHitPointsBase - 2;
    party[1]->m_baseStats.m_hitPoints = party[1]->m_baseStats.m_maxHitPointsBase;
    before = pGame->m_worldTime.m_gameTime;
    pGame->RestParty(3, FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(pGame->m_worldTime.m_gameTime - before == CRuleTables::TIME_RESTING * 2);

    // Deep wounds: the hours of healing, rounded up, in rest periods, plus
    // one; everybody is healed in full (1000 points).
    party[0]->m_baseStats.m_hitPoints = 1;
    party[1]->m_baseStats.m_hitPoints = 2;
    const INT nWounds = party[0]->m_baseStats.m_maxHitPointsBase - 1 + party[1]->m_baseStats.m_maxHitPointsBase - 2;
    const INT nHealed = (nWounds / nRate + (nWounds % nRate != 0 ? 1 : 0)) * nRate;
    REQUIRE(nHealed > nRate);
    before = pGame->m_worldTime.m_gameTime;
    pGame->RestParty(3, FALSE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    const DWORD expected = CRuleTables::TIME_RESTING * static_cast<DWORD>(nHealed) * 100
            / static_cast<DWORD>(nRate * 100)
        + CRuleTables::TIME_RESTING;
    CHECK(pGame->m_worldTime.m_gameTime - before == expected);
    for (CGameSprite* pSprite : party) {
        CHECK(pSprite->m_baseStats.m_hitPoints == pSprite->m_baseStats.m_maxHitPointsBase);
    }

    CInfGame::m_bHealPartyOnRest = heal;
}
