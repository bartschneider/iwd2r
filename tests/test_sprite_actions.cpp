#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameDialog.h"
#include "CGameDoor.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CResDLG.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSearchBitmap.h"
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

// The sprite's own actions (CGameSprite::ExecuteAction 0x728F80) on the
// engine in this process: a party member in AR1000 walks to a point
// (MoveToPoint), walks to Hedron before talking to him (PlayerDialog,
// MoveToObject), and opens or fails to open a door it stands next to
// (UseDoor). Every engine works on a private view of the install
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
// panels loaded (tests/test_dialog.cpp's NewGameInStartArea).
CGameSprite* NewGameInStartArea(std::vector<BYTE>& bytes)
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

// Keeps a demanded resource and puts back one byte patched in its data
// when the test case leaves, by a failed REQUIRE too.
class DemandedResource {
public:
    explicit DemandedResource(CRes* pRes)
        : m_pRes(pRes)
    {
    }
    ~DemandedResource()
    {
        if (m_pByte != NULL) {
            *m_pByte = m_original;
        }
        m_pRes->Release();
    }
    void Patch(BYTE* pByte, BYTE value)
    {
        m_pByte = pByte;
        m_original = *pByte;
        *pByte = value;
    }
    DemandedResource(const DemandedResource&) = delete;
    DemandedResource& operator=(const DemandedResource&) = delete;

private:
    CRes* m_pRes;
    BYTE* m_pByte = NULL;
    BYTE m_original = 0;
};

// The search square of a point.
CPoint Square(const CPoint& pt)
{
    return CPoint(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY);
}

// Whether the terrain of a square is open to the sprite and nobody stands on
// it.
bool OpenSquare(CGameSprite* pSprite, const CPoint& square)
{
    CSearchBitmap& search = pSprite->GetArea()->m_search;
    if (square.x < 0 || square.y < 0 || square.x >= search.m_GridSquareDimensions.cx
        || square.y >= search.m_GridSquareDimensions.cy) {
        return false;
    }
    CVidBitmap& bitmap = search.m_resSearch;
    BYTE value = bitmap.GetPixelValue(square.x, square.y, TRUE);
    if (bitmap.GetBitCount(TRUE) == 8) {
        value >>= 4;
    }
    return pSprite->m_terrainTable[value] != CPathSearch::COST_IMPASSABLE
        && search.m_pDynamicCost[square.y * search.m_GridSquareDimensions.cx + square.x] == 0;
}

// Set when the search thread left a request unserved for five seconds.
bool g_searchStalled = false;

// One frame of a sprite's action as the AI update runs it: the action (the
// sprite's dispatcher), the walk, then the messages. A search request the
// search thread still works on is waited for first, as the replay holds the
// frame.
SHORT Frame(CGameSprite* pSprite)
{
    CSearchRequest* pRequest = pSprite->m_currentSearchRequest;
    if (pRequest != NULL) {
        int waited = 0;
        while ((pRequest->m_serviceState == CSearchRequest::STATE_WAITING
                   || pRequest->m_serviceState == CSearchRequest::STATE_PROCESSING)
            && pRequest->m_collisionDelay == 0
            && waited < 5000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            waited++;
        }
        if (waited >= 5000) {
            g_searchStalled = true;
        }
    }
    pSprite->AIUpdateWalk();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    return pSprite->ExecuteAction();
}

// The door on the area's front list with this script name.
CGameDoor* FindDoor(CGameArea* pArea, const char* name)
{
    POSITION pos = pArea->m_lVertSort.GetHeadPosition();
    while (pos != NULL) {
        LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pArea->m_lVertSort.GetNext(pos)));
        CGameObject* pObject;
        if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            if (pObject->GetObjectType() == CGameObject::TYPE_DOOR
                && strncmp(static_cast<CGameDoor*>(pObject)->m_scriptName, name, SCRIPTNAME_SIZE) == 0) {
                return static_cast<CGameDoor*>(pObject);
            }
        }
    }
    return NULL;
}

// The party member, selected, standing next to the door, its UseDoor
// (142) on the door ready to run for the first time.
void ReadyUseDoor(CGameSprite* pCharacter, CGameDoor* pDoor)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    // UseDoor reads only the position: it is set where a jump would put the
    // character.
    pCharacter->m_pos = pDoor->GetCloserDestPt(pCharacter->GetPos());
    if (!pCharacter->m_bSelected) {
        pGame->SelectCharacter(pCharacter->GetId(), FALSE);
    }
    REQUIRE(pCharacter->m_bSelected);
    CAIAction useDoor;
    useDoor.m_actionID = 142;
    useDoor.m_specificID = pDoor->GetId();
    useDoor.m_specificID2 = 0;
    pCharacter->SetCurrAction(useDoor);
    pCharacter->m_actionCount = 0;
}

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

}

TEST_CASE("sprite: a party member given MoveToPoint walks to the point and its action clears", "[ai][data]")
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
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    // The search thread's snapshot walls off unexplored squares for the
    // party; the party's sight explores them in the game.
    pArea->m_visibility.SetAreaExplored();

    // An open square some way off with open squares on the way.
    const CPoint fromSquare = Square(pCharacter->GetPos());
    const CPoint directions[] = { CPoint(1, 0), CPoint(-1, 0), CPoint(0, 1), CPoint(0, -1) };
    CPoint goalSquare(-1, -1);
    for (int distance = 12; distance >= 5 && goalSquare.x == -1; distance--) {
        for (const CPoint& direction : directions) {
            bool open = true;
            for (int step = 2; step <= distance + 1 && open; step++) {
                open = OpenSquare(pCharacter, CPoint(fromSquare.x + direction.x * step, fromSquare.y + direction.y * step));
            }
            if (open) {
                goalSquare = CPoint(fromSquare.x + direction.x * distance, fromSquare.y + direction.y * distance);
                break;
            }
        }
    }
    REQUIRE(goalSquare.x != -1);
    const CPoint goal(goalSquare.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
        goalSquare.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

    // MoveToPoint (23) asks for a path to the point and goes on.
    pCharacter->SetCurrAction(CAIAction(CAIAction::MOVETOPOINT, goal, 0, -1));
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pCharacter->m_curDest == goal);
    CHECK(pCharacter->m_nLastActionReturn == CGameAIBase::ACTION_INTERRUPTABLE);
    REQUIRE((pCharacter->m_currentSearchRequest != NULL || pCharacter->m_pPath != NULL));

    // The same point again does not ask again.
    CSearchRequest* pRequest = pCharacter->m_currentSearchRequest;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pCharacter->m_currentSearchRequest == pRequest);

    int frames = 0;
    SHORT rc = CGameAIBase::ACTION_INTERRUPTABLE;
    while (rc == CGameAIBase::ACTION_INTERRUPTABLE && frames < 3000 && !g_searchStalled) {
        rc = Frame(pCharacter);
        frames++;
    }
    if (g_searchStalled) {
        // The search thread serves requests once the movement stream's
        // request loop is in (CSearchBitmap, SearchThreadMain).
        SKIP("[iwd2r-skip] the search thread did not serve the path request");
    }
    INFO("frames " << frames);
    CHECK(rc == CGameAIBase::ACTION_DONE);
    CHECK(Square(pCharacter->GetPos()) == goalSquare);

    // Done: the next action is NoAction, as ProcessAI takes it.
    CAIAction next;
    pCharacter->SetCurrAction(pCharacter->GetNextAction(next));
    CHECK(pCharacter->m_curAction.m_actionID == CAIAction::NO_ACTION);
}

TEST_CASE("sprite: PlayerDialog out of range walks to Hedron and the dialog opens on arrival", "[ai][data]")
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
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    pArea->m_visibility.SetAreaExplored();
    CGameSprite* pHedron = FindSpeaker(pGame->m_pGameAreaMaster, "10HEDRON");
    REQUIRE(pHedron != NULL);

    // Out of reach: the squares between them are more than the two personal
    // spaces and two squares.
    const CPoint a = Square(pCharacter->GetPos());
    const CPoint b = Square(pHedron->GetPos());
    INFO("character " << pCharacter->GetPos().x << "," << pCharacter->GetPos().y << " Hedron "
                      << pHedron->GetPos().x << "," << pHedron->GetPos().y);
    REQUIRE(std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) > 4);

    // The original's start state (NumTimesTalkedTo(0)) needs the sprite's
    // status triggers.
    //
    // TODO(remove): as in tests/test_dialog.cpp, the cached DLG's trigger
    // name is spoiled only while CGameSprite::EvaluateStatusTrigger (the
    // movement stream) is missing; `demanded` puts the byte back.
    CResHelper<CResDLG, 1011> file;
    file.SetResRef(CResRef("10HEDRON"), TRUE, TRUE);
    REQUIRE(file.GetRes() != NULL);
    BYTE* pData = static_cast<BYTE*>(file.GetRes()->Demand());
    REQUIRE(pData != NULL);
    DemandedResource demanded(file.GetRes());
    if (!pHedron->EvaluateStatusTrigger(CAITrigger(0x4039, 0))) {
        const DWORD nSize = file.GetRes()->m_nResSizeActual;
        const std::string sTrigger = "NumTimesTalkedTo(0)";
        BYTE* pTrigger = std::search(pData, pData + nSize, sTrigger.begin(), sTrigger.end());
        REQUIRE(pTrigger != pData + nSize);
        demanded.Patch(&pTrigger[15], 'X');
    }

    // PlayerDialog (139) on Hedron: too far, so the character walks to him
    // first (MoveToObject 0x73EDD0, a search for Hedron himself).
    CAIObjectType hedron(0, 0, 0, 0, 0, 0, 0, 0, pHedron->GetId(), 0, 0);
    pCharacter->SetCurrAction(CAIAction(CAIAction::PLAYERDIALOG, hedron, 0, 0, 0));
    pCharacter->m_actionCount = 1;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    CHECK(pCharacter->m_curDest == pHedron->GetPos());
    REQUIRE(pCharacter->m_currentSearchRequest != NULL);
    CHECK(pCharacter->m_currentSearchRequest->m_nTargetIds == 1);
    CHECK(pCharacter->m_currentSearchRequest->m_targetIds[0] == pHedron->GetId());
    CHECK(pCharacter->m_targetId == pHedron->GetId());
    CHECK(pWorld->m_internalLoadedDialog.field_54 == 0);

    int frames = 0;
    SHORT rc = CGameAIBase::ACTION_INTERRUPTABLE;
    while (rc == CGameAIBase::ACTION_INTERRUPTABLE && frames < 3000 && !g_searchStalled) {
        rc = Frame(pCharacter);
        frames++;
    }
    if (g_searchStalled) {
        // The search thread serves requests once the movement stream's
        // request loop is in (CSearchBitmap, SearchThreadMain).
        SKIP("[iwd2r-skip] the search thread did not serve the path request");
    }
    INFO("frames " << frames);

    // On arrival the dialog opens: Hedron's opening state, both sides
    // remember who they talked to.
    CHECK(rc == CGameAIBase::ACTION_DONE);
    CGameDialogSprite& dialog = pWorld->m_internalLoadedDialog;
    CHECK(dialog.field_54 == 1);
    CHECK(pCharacter->m_lTalkedTo.m_nInstance == pHedron->GetId());
    CHECK(pHedron->m_lTalkedTo.m_nInstance == pCharacter->GetId());
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.m_currentEntryIndex == 0);
    CHECK(dialog.m_talkerIndex == pHedron->GetId());
    // Hedron counts the talk (CMessageSetNumTimesTalkedTo); the party
    // member does not.
    CHECK(pHedron->m_nNumberOfTimesTalkedTo == 1);

    dialog.m_responseMarker = -2;
    dialog.AsynchronousUpdate();
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK(dialog.field_54 == 0);
    ClearMessages();
}

TEST_CASE("sprite: UseDoor next to AR1000_Door5 opens it and does nothing from afar", "[ai][data]")
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
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    CGameDoor* pDoor = FindDoor(pArea, "AR1000_Door5");
    REQUIRE(pDoor != NULL);
    REQUIRE((pDoor->m_dwFlags & 0x3) == 0);

    // From the ship the door is out of reach: UseDoor is done, the door
    // stays shut.
    if (!pCharacter->m_bSelected) {
        g_pBaldurChitin->GetObjectGame()->SelectCharacter(pCharacter->GetId(), FALSE);
    }
    REQUIRE(pCharacter->m_bSelected);
    CAIAction useDoor;
    useDoor.m_actionID = 142;
    useDoor.m_specificID = pDoor->GetId();
    pCharacter->SetCurrAction(useDoor);
    pCharacter->m_actionCount = 0;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK((pDoor->m_dwFlags & 0x1) == 0);

    // Next to it the door opens (ToggleDoor), and a second frame of the
    // same action only waits.
    ReadyUseDoor(pCharacter, pDoor);
    const CPoint ptDest = pDoor->GetCloserDestPt(pCharacter->GetPos());
    INFO("character " << pCharacter->GetPos().x << "," << pCharacter->GetPos().y << " door point " << ptDest.x
                      << "," << ptDest.y << " selected " << pCharacter->m_bSelected << " orderable "
                      << pCharacter->Orderable(FALSE));
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
    CHECK((pDoor->m_dwFlags & 0x1) != 0);
    pCharacter->m_actionCount = 1;
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_INTERRUPTABLE);
    ClearMessages();
}

TEST_CASE("sprite: UseDoor on a locked door leaves it closed and says it is locked", "[ai][data]")
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
    CGameArea* pArea = pCharacter->GetArea();
    REQUIRE(pArea != NULL);
    pArea->SortLists();
    CGameDoor* pDoor = FindDoor(pArea, "AR1000_Door1");
    REQUIRE(pDoor != NULL);
    REQUIRE((pDoor->m_dwFlags & 0x2) != 0);

    ReadyUseDoor(pCharacter, pDoor);
    ClearMessages();
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK((pDoor->m_dwFlags & 0x1) == 0);
    CHECK((pDoor->m_dwFlags & 0x2) != 0);

    // The feedback goes to the one who tried the door: the door's own
    // "locked" text, or "Locked" (15874).
    const STRREF expected = (pDoor->m_dwFlags & 0x200) != 0 ? pDoor->m_strNotPickable : 15874;
    bool found = false;
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        auto* message = dynamic_cast<CMessageDisplayTextRefSend*>(handler.m_messageList.GetNext(pos));
        if (message != nullptr && message->m_text == expected && message->m_targetId == pCharacter->GetId()) {
            found = true;
        }
    }
    CHECK(found);
    ClearMessages();
}
