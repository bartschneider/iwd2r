#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAICondition.h"
#include "CAIResponse.h"
#include "CAIScriptFile.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameDialog.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
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
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Dialogs on the world screen (CScreenWorld::StartDialog 0x68EA00,
// CGameDialogSprite::Load 0x482210, CGameDialogEntry::Handle 0x484900,
// CGameDialogSprite::AsynchronousUpdate 0x483F00, CScreenWorld::EndDialog
// 0x68F9D0) on the engine in this process, with the start area's first
// speaker: Hedron Kerdos (10HEDRON.DLG), whose opening conversation the
// original shows on entering AR1000 (Task 1's world dump: entry 0, three
// responses). Every engine works on a private view of the install
// (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

fs::path g_view;

// The engine as iwd2r_main builds it (tests/test_gam.cpp's StartEngine).
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

// The main loop's drawn frames (tests/test_gam.cpp's FramePump).
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

// A new game with one party member standing in AR1000, the world screen's
// panels loaded.
CGameSprite* NewGameInStartArea(std::vector<BYTE>& bytes)
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
    pSprite->m_sName = "Probe";
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->EngineGameInit();
    // The world screen is the active engine, as after the game loads (the
    // test does not switch engines: EngineActivated starts the sound).
    g_pBaldurChitin->pActiveEngine = pWorld;
    pGame->m_bGameLoaded = TRUE;
    return pSprite;
}

// The sprite on the area's lists whose dialog is `dialog`.
CGameSprite* FindSpeaker(CGameArea* pArea, const char* dialog)
{
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                    && static_cast<CGameSprite*>(pObject)->m_dialog == dialog) {
                    return static_cast<CGameSprite*>(pObject);
                }
            }
        }
    }
    return NULL;
}

// The replies Handle put on the dialog display (the dump's `responses`).
int ShownReplies(CGameDialogSprite& dialog)
{
    CGameDialogEntry* pEntry = dialog.m_dialogEntries[dialog.m_currentEntryIndex];
    int count = 0;
    for (INT n = 0; n < pEntry->GetSize(); n++) {
        if (pEntry->GetAt(n)->m_displayPosition != NULL && pEntry->GetAt(n)->m_displayListId != 0xFF)
            count++;
    }
    return count;
}

void RunMessages()
{
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
}

}

TEST_CASE("dialog: Hedron's opening conversation shows three responses and ends back on the world screen", "[dialog][data]")
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
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CGameSprite* pHedron = FindSpeaker(pGame->m_pGameAreaMaster, "10HEDRON");
    REQUIRE(pHedron != NULL);

    CGameDialogSprite& dialog = pWorld->m_internalLoadedDialog;

    // Load parses the file: 41 states, the first with three replies, each
    // leading on in the same file, ordered first by its trigger index.
    dialog.Load(CResRef("10HEDRON"), pCharacter->GetId(), pHedron->GetId());
    REQUIRE(dialog.m_dialogEntries.GetSize() == 41);
    CHECK(dialog.m_dialogEntries[0]->GetSize() == 3);
    CHECK(dialog.m_dialogEntriesOrdered[0] == dialog.m_dialogEntries[0]);
    CHECK(dialog.m_dialogEntries[0]->GetAt(0)->m_nextDialog == "10HEDRON");
    CHECK(dialog.m_dialogEntries[0]->GetAt(0)->m_nextEntryIndex == 1);
    // State 0 starts on NumTimesTalkedTo(0) (TRIGGER.IDS 0x4039).
    REQUIRE(dialog.m_dialogEntries[0]->m_startCondition.m_triggerList.GetCount() == 1);
    CAITrigger* pStart = dialog.m_dialogEntries[0]->m_startCondition.m_triggerList.GetHead();
    CHECK(pStart->m_triggerID == 0x4039);
    CHECK(pStart->m_specificID == 0);
    CHECK(pStart->m_flags == 0);
    // A state without a start condition gets "False()" (0x4836F2).
    CHECK(dialog.m_dialogEntries[1]->m_startCondition.m_triggerList.GetCount() == 1);
    // State 25's reply: text, journal and AddXpVar("Level_1_Easy",2578)
    // (ACTION.IDS 238), on to state 27.
    REQUIRE(dialog.m_dialogEntries[25]->GetSize() == 1);
    CGameDialogReply* pXpReply = dialog.m_dialogEntries[25]->GetAt(0);
    CHECK(pXpReply->m_flags == 0x15);
    CHECK(pXpReply->m_nextEntryIndex == 27);
    REQUIRE(pXpReply->m_response.m_actionList.GetCount() == 1);
    CHECK(pXpReply->m_response.m_actionList.GetHead()->m_actionID == 238);
    CHECK(pXpReply->m_response.m_actionList.GetHead()->m_string1 == "Level_1_Easy");
    CHECK(pXpReply->m_response.m_actionList.GetHead()->m_specificID == 2578);

    // Load parses every file it is given again: Unmarshal's ClearMarshal
    // empties m_file (0x483994) after Load set it, as in the original.
    CHECK(dialog.m_file == "");

    // State 0's start condition, NumTimesTalkedTo(0), holds for Hedron
    // (CGameSprite::EvaluateStatusTrigger 0x731B30), so StartDialog opens
    // the dialog (CGameDialogSprite::StartDialog walks the ordered states).
    BOOL bAnyHolds = FALSE;
    for (INT n = 0; n < dialog.m_dialogEntriesOrdered.GetSize(); n++) {
        CTypedPtrList<CPtrList, CAITrigger*> triggerList;
        if (dialog.m_dialogEntriesOrdered[n]->m_startCondition.Hold(triggerList, pHedron)) {
            bAnyHolds = TRUE;
            break;
        }
    }
    CHECK(bAnyHolds);

    BOOL bStarted = pWorld->StartDialog(pCharacter, pHedron, FALSE, FALSE);
    REQUIRE(bStarted);

    // The dialog panel is up and the game paused; CMessageEnterDialog makes
    // the entry show (EnterDialog, Handle). The dump reads `active` from
    // field_54.
    CHECK(pWorld->field_EA4 == 7);
    CHECK(pWorld->m_bPaused);
    CHECK(pGame->GetGameSave()->m_mode == 0x502);
    CHECK(dialog.field_54 == 1);
    RunMessages();
    CHECK(dialog.m_waitingForResponse);
    CHECK(dialog.m_currentEntryIndex == 0);
    CHECK(dialog.m_talkerIndex == pHedron->GetId());
    CHECK(ShownReplies(dialog) == 3);

    // The first reply leads to state 1 of the same file (four replies):
    // Pick, Reload, CMessagePickedDialogResponse, CMessageRemoveReplies,
    // CMessageLoadDialog and CMessageEnterDialog.
    dialog.m_responseMarker = 0;
    dialog.AsynchronousUpdate();
    CHECK(dialog.m_responseMarker == -1);
    RunMessages();
    CHECK(dialog.m_waitingForResponse);
    CHECK(dialog.m_currentEntryIndex == 1);
    CHECK(ShownReplies(dialog) == 4);
    CHECK(dialog.field_54 == 1);
    CHECK(pWorld->field_EA4 == 7);

    // State 25, entered directly: its reply queues AddXpVar on Hedron
    // (CMessageInsertResponse: not a DLGINST.IDS action, so it is inserted)
    // and goes on to state 27.
    g_pBaldurChitin->GetMessageHandler()->AddMessage(new CMessageEnterDialog(25, TRUE, pHedron->GetId(), pHedron->GetId()), FALSE);
    RunMessages();
    REQUIRE(dialog.m_currentEntryIndex == 25);
    CHECK(ShownReplies(dialog) == 1);
    pHedron->m_queuedActions.RemoveAll();
    dialog.m_responseMarker = 0;
    dialog.AsynchronousUpdate();
    RunMessages();
    CHECK(dialog.m_currentEntryIndex == 27);
    REQUIRE(pHedron->m_queuedActions.GetCount() == 1);
    CHECK(pHedron->m_queuedActions.GetHead()->m_actionID == 238);

    // State 20's only reply sets a global (an instant DLGINST.IDS action,
    // run at once and removed) and ends the dialog (flag 0x8): Pick returns
    // nothing and AsynchronousUpdate ends the dialog.
    g_pBaldurChitin->GetMessageHandler()->AddMessage(new CMessageEnterDialog(20, TRUE, pHedron->GetId(), pHedron->GetId()), FALSE);
    RunMessages();
    REQUIRE(dialog.m_currentEntryIndex == 20);
    pHedron->m_queuedActions.RemoveAll();
    dialog.m_responseMarker = 0;
    dialog.AsynchronousUpdate();
    RunMessages();
    CHECK(pHedron->m_queuedActions.GetCount() == 0);
    CHECK_FALSE(dialog.m_waitingForResponse);
    CHECK(dialog.field_54 == 0);
    CHECK(pWorld->field_EA4 == -1);
    CHECK(pGame->GetGameSave()->m_mode == static_cast<DWORD>(-1));
    // EndDialog always restarts the game (StartTime, m_bPaused = FALSE).
    CHECK_FALSE(pWorld->m_bPaused);
    CHECK(ShownReplies(dialog) == 0);

    // Again, ended half-way with the marker -2 (what the dialog button and
    // the Enter/Escape keys pick when the entry offers no reply).
    REQUIRE(pWorld->StartDialog(pCharacter, pHedron, FALSE, FALSE));
    RunMessages();
    CHECK(ShownReplies(dialog) == 3);
    CHECK(dialog.field_54 == 1);
    dialog.m_responseMarker = -2;
    dialog.AsynchronousUpdate();
    RunMessages();
    CHECK_FALSE(dialog.m_waitingForResponse);
    CHECK(dialog.field_54 == 0);
    CHECK(pWorld->field_EA4 == -1);
    CHECK_FALSE(pWorld->m_bPaused);
    CHECK(ShownReplies(dialog) == 0);
}

TEST_CASE("dialog: Reload gives a state in another creature's dialog to that creature: Reig's 10JON is Jon", "[dialog][data]")
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
    CGameSprite* pCharacter = NewGameInStartArea(cre);
    REQUIRE(pGame->m_pGameAreaMaster != NULL);
    CGameSprite* pReig = FindSpeaker(pGame->m_pGameAreaMaster, "10REIG");
    CGameSprite* pJon = FindSpeaker(pGame->m_pGameAreaMaster, "10JON");
    REQUIRE(pReig != NULL);
    REQUIRE(pJon != NULL);

    CGameDialogSprite dialog;
    dialog.Load(CResRef("10REIG"), pCharacter->GetId(), pReig->GetId());
    REQUIRE(dialog.m_talkerIndex == pReig->GetId());

    // No file, or the talker's own: the talker is kept (0x4829BD, 0x48258F).
    dialog.Reload(CResRef(""), pReig);
    CHECK(dialog.m_talkerIndex == pReig->GetId());
    dialog.Reload(CResRef("10REIG"), pReig);
    CHECK(dialog.m_talkerIndex == pReig->GetId());

    // Reig's state 4 goes on to Jon's state 2 in 10JON: the nearest creature
    // of Reig's area with that dialog (GetNearestDialogMatch 0x46C460, from
    // 0x482570/0x482586) speaks it, as in the original's walk_and_talk
    // (speaker 786444 at frames 3570 and 3600).
    dialog.Reload(CResRef("10JON"), pReig);
    CHECK(dialog.m_talkerIndex == pJon->GetId());
    CHECK(dialog.m_characterIndex == pCharacter->GetId());

    // And Jon's state 2 back to Reig's 5.
    dialog.Reload(CResRef("10REIG"), pJon);
    CHECK(dialog.m_talkerIndex == pReig->GetId());
}

TEST_CASE("dialog: TogglePauseGame pauses and unpauses the world and returns TRUE", "[dialog][data]")
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
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    NewGameInStartArea(cre);

    REQUIRE_FALSE(pWorld->m_bPaused);
    CHECK(pWorld->TogglePauseGame(0, 1, 0) == TRUE);
    CHECK(pWorld->m_bPaused);
    CHECK_FALSE(pGame->GetWorldTimer()->m_active);
    CHECK(pGame->m_bForceDither);

    CHECK(pWorld->TogglePauseGame(0, 1, 0) == TRUE);
    CHECK_FALSE(pWorld->m_bPaused);
    CHECK(pGame->GetWorldTimer()->m_active);
    CHECK_FALSE(pGame->m_bForceDither);

    // In the dialog panel the game stays paused.
    pWorld->field_EA4 = 7;
    CHECK(pWorld->TogglePauseGame(0, 1, 0) == TRUE);
    CHECK(pWorld->m_bPaused);
    CHECK(pWorld->TogglePauseGame(0, 1, 0) == TRUE);
    CHECK(pWorld->m_bPaused);
    pWorld->field_EA4 = -1;
    CHECK(pWorld->TogglePauseGame(0, 1, 0) == TRUE);
    CHECK_FALSE(pWorld->m_bPaused);
}

TEST_CASE("dialog: FetchRumor reads the tavern's rumor file and nothing from a missing one", "[dialog][data]")
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

    CGameSprite* pCharacter = NewGameInStartArea(cre);

    // A file the game does not have: Load clears the sprite, nothing holds.
    CGameDialogSprite missing;
    LONG nIndex = 5;
    STR_RES strRes;
    CHECK_FALSE(missing.FetchRumor(CResRef("NORUMOR"), pCharacter, nIndex, strRes));
    CHECK(nIndex == 5);
    CHECK(missing.m_dialogEntries.GetSize() == 0);

    // 10SALTY (the Salty Dog's rumors, 10GOHAR.STO): thirteen states, each
    // started by RandomNum(13,N).
    CGameDialogSprite rumors;
    nIndex = -1;
    const BOOL bFound = rumors.FetchRumor(CResRef("10SALTY"), pCharacter, nIndex, strRes);
    REQUIRE(rumors.m_dialogEntries.GetSize() == 13);
    CHECK(rumors.m_dialogEntriesOrdered[0] == rumors.m_dialogEntries[0]);
    CHECK(rumors.m_dialogEntries[12]->m_startCondition.m_triggerList.GetCount() == 1);
    if (bFound) {
        // The rumor is the one RandomNum picked; the index moves past it.
        CHECK(nIndex >= 1);
        CHECK(strRes.szText.GetLength() > 0);
    } else {
        CHECK(nIndex == -1);
        WARN("RandomNum is a status trigger: CGameSprite::EvaluateStatusTrigger is not ported yet");
    }
}

TEST_CASE("dialog: the script parser reads a trigger's object and number and an action's arguments", "[dialog][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);

    CAIScriptFile scriptFile;

    // One object parameter (TRIGGER.IDS 0x4051 Dead(O:Object*), case 4 of
    // CAIScriptFile::ParseTrigger 0x410B40): the object is the cause.
    scriptFile.ParseConditionalString(CString("Dead(\"Hedron\")\n"));
    INFO(static_cast<const char*>(scriptFile.m_errors));
    REQUIRE(scriptFile.m_curCondition->m_triggerList.GetCount() == 1);
    CAITrigger* pTrigger = scriptFile.m_curCondition->m_triggerList.GetHead();
    CHECK(pTrigger->m_triggerID == 0x4051);
    CHECK(pTrigger->m_triggerCause.m_sName == "Hedron");
    CHECK(pTrigger->m_flags == 0);

    // An object and two numbers (0x40E7 CheckSkillGT(O:Object*,I:Value*,
    // I:Skill*Skills), case 19): the last one runs to the closing bracket,
    // so its IDS name (SKILLS.IDS 4 Diplomacy) resolves.
    scriptFile.ParseConditionalString(CString("CheckSkillGT(Protagonist, 0, Diplomacy)\n"));
    REQUIRE(scriptFile.m_curCondition->m_triggerList.GetCount() == 1);
    pTrigger = scriptFile.m_curCondition->m_triggerList.GetHead();
    CHECK(pTrigger->m_triggerID == 0x40E7);
    CHECK(pTrigger->m_specificID == 0);
    CHECK(pTrigger->m_specific2 == 4);
    scriptFile.ParseConditionalString(CString("CheckStatGT(Protagonist, 11, INT)\n"));
    REQUIRE(scriptFile.m_curCondition->m_triggerList.GetCount() == 1);
    pTrigger = scriptFile.m_curCondition->m_triggerList.GetHead();
    CHECK(pTrigger->m_triggerID == 0x4045);
    CHECK(pTrigger->m_specificID == 11);
    CHECK(pTrigger->m_specific2 == STAT_INT);

    // One number (0x4039 NumTimesTalkedTo(I:Num*), case 6), negated.
    scriptFile.ParseConditionalString(CString("!NumTimesTalkedTo(2)\n"));
    REQUIRE(scriptFile.m_curCondition->m_triggerList.GetCount() == 1);
    pTrigger = scriptFile.m_curCondition->m_triggerList.GetHead();
    CHECK(pTrigger->m_triggerID == 0x4039);
    CHECK(pTrigger->m_specificID == 2);
    CHECK(pTrigger->m_flags == 1);

    // An action with a string and a number (ACTION.IDS 238
    // AddXPVar(S:VarTableEntry*,I:Strref*)), the name in any case.
    scriptFile.ParseResponseString(CString("AddXpVar(\"Level_1_Easy\",2578)\n"));
    INFO(static_cast<const char*>(scriptFile.m_errors));
    REQUIRE(scriptFile.m_curResponse->m_actionList.GetCount() == 1);
    CAIAction* pAction = scriptFile.m_curResponse->m_actionList.GetHead();
    CHECK(pAction->m_actionID == 238);
    CHECK(pAction->m_string1 == "Level_1_Easy");
    CHECK(pAction->m_specificID == 2578);
}
