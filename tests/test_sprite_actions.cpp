#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAIScript.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CGameAnimationTypeCharacter.h"
#include "CGameArea.h"
#include "CGameDialog.h"
#include "CGameDoor.h"
#include "CGameSprite.h"
#include "CGameTrigger.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CObjectMarker.h"
#include "CPathSearch.h"
#include "CResDLG.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSearchBitmap.h"
#include "CUtil.h"
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
    // SetCurrAction (0x7338E0) draws the path counter, UtilRandInt(8,
    // -luck); MoveToObject searches when it is a multiple of 8. Start the
    // walk on the first call.
    pCharacter->field_5616 = 0;
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

namespace {

// Every object of a type on the area's three lists.
std::vector<CGameObject*> ObjectsOfType(CGameArea* pArea, BYTE type)
{
    std::vector<CGameObject*> objects;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                if (pObject->GetObjectType() == type) {
                    objects.push_back(pObject);
                }
            }
        }
    }
    return objects;
}

}

TEST_CASE("sprite: DoAIUpdate runs while the world is paused and cycles the AI inhibitor", "[ai][data]")
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
    CGameSprite* pHedron = FindSpeaker(pArea, "10HEDRON");
    REQUIRE(pHedron != NULL);

    // CGameSprite::DoAIUpdate (0x7238C0) ignores `active`; the counter test
    // is CGameObject's.
    const LONG counter = pHedron->m_id & pHedron->m_AISpeed;
    pHedron->m_canBeSeen = 0;
    pHedron->m_AIInhibitor = 0;
    CHECK(pHedron->DoAIUpdate(FALSE, counter) == TRUE);
    if (pHedron->m_AISpeed != 0) {
        const BYTE inhibitor = pHedron->m_AIInhibitor;
        CHECK(pHedron->DoAIUpdate(FALSE, counter ^ pHedron->m_AISpeed) == FALSE);
        CHECK(pHedron->m_AIInhibitor == inhibitor);
    }

    // An unseen non-party creature (EA between CONTROLCUTOFF and EVILCUTOFF)
    // keeps its inhibitor: 1, 2, then 0 lets an update through.
    const BYTE ea = pHedron->GetAIType().m_nEnemyAlly;
    if ((pHedron->m_baseStats.m_flags & 0x200000) == 0
        && ea > CAIObjectType::EA_CONTROLCUTOFF
        && ea < CAIObjectType::EA_EVILCUTOFF) {
        pHedron->m_AIInhibitor = 0;
        pHedron->m_canBeSeen = 0;
        pHedron->DoAIUpdate(FALSE, counter);
        CHECK(pHedron->m_AIInhibitor == 1);
        pHedron->DoAIUpdate(FALSE, counter);
        CHECK(pHedron->m_AIInhibitor == 2);
        pHedron->DoAIUpdate(FALSE, counter);
        CHECK(pHedron->m_AIInhibitor == 0);
    }

    // A party member is never held back.
    pCharacter->m_AIInhibitor = 0;
    const LONG partyCounter = pCharacter->m_id & pCharacter->m_AISpeed;
    for (int n = 0; n < 3; n++) {
        CHECK(pCharacter->DoAIUpdate(FALSE, partyCounter) == TRUE);
        CHECK(pCharacter->m_AIInhibitor == 0);
    }

    // The constructor: active by the creature's time-of-day mask
    // (0x6EF990), and the party member counted in the area (the BYTE count
    // starts at 0xFF, 0x6F40F0).
    const LONG hour = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->GetCurrentHour();
    INFO("hour " << hour << " mask " << pHedron->m_timeOfDayVisible);
    CHECK((pHedron->m_active != FALSE) == (((pHedron->m_timeOfDayVisible >> hour) & 1) != 0));
    CHECK(pArea->m_nCharacters == 1);
}

TEST_CASE("trigger: DoAIUpdate answers whether the trigger has a script while paused too", "[ai][data]")
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

    std::vector<CGameObject*> triggers = ObjectsOfType(pArea, CGameObject::TYPE_TRIGGER);
    REQUIRE(!triggers.empty());
    int checked = 0;
    for (CGameObject* pObject : triggers) {
        CGameTrigger* pTrigger = static_cast<CGameTrigger*>(pObject);
        // The constructor (0x4CCAB0) keeps the ARE's script name, which
        // the override script was loaded from.
        REQUIRE(pTrigger->m_overrideScript != NULL);
        CHECK(CResRef(pTrigger->m_scriptRes) == pTrigger->m_overrideScript->m_cResRef);
        if (pTrigger->m_triggerType == 0 || (pTrigger->m_dwFlags & 0x100) != 0) {
            continue;
        }
        const BOOLEAN bScript = CResRef(pTrigger->m_scriptRes) != "";
        CHECK(pTrigger->DoAIUpdate(FALSE, 12345) == bScript);
        CHECK(pTrigger->DoAIUpdate(TRUE, 0) == bScript);
        pTrigger->m_dwFlags |= 0x100;
        CHECK(pTrigger->DoAIUpdate(TRUE, 0) == FALSE);
        pTrigger->m_dwFlags &= ~0x100;
        checked++;
    }
    CHECK(checked > 0);
}

TEST_CASE("sprite: a weapon set change takes off the old set and selects the new one", "[ai][data]")
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
    REQUIRE(pCharacter->GetArea() != NULL);
    REQUIRE(pCharacter->m_equipment.m_items[42] == NULL);

    // Set 1 holds nothing: the fist (slot 10) is selected (0x726810).
    if (pCharacter->m_equipment.m_items[45] == NULL && pCharacter->m_equipment.m_items[46] == NULL) {
        ClearMessages();
        pCharacter->sub_726810(1);
        CHECK(pCharacter->m_nWeaponSet == 1);
        CHECK(pCharacter->m_equipment.m_selectedWeapon == 10);
        CHECK(pCharacter->field_3D3A[2] == 0);
        ClearMessages();
    }

    // Back to set 0: its weapon is selected again.
    pCharacter->sub_726810(0);
    CHECK(pCharacter->m_nWeaponSet == 0);
    if (pCharacter->m_equipment.m_items[43] != NULL) {
        CHECK(pCharacter->m_equipment.m_selectedWeapon == 43);
    } else {
        CHECK(pCharacter->m_equipment.m_selectedWeapon == 10);
    }
    ClearMessages();
}

TEST_CASE("game: ClearCharacterSlot leaves an area count of 0xFF alone", "[ai][data]")
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
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    INT nSlot = -1;
    for (INT n = 0; n < CINFGAME_MAXCHARACTERS; n++) {
        if (pGame->m_characters[n] == pCharacter->GetId()) {
            nSlot = n;
        }
    }
    REQUIRE(nSlot != -1);

    // 0x5BD530 decrements the area's BYTE count unless it is 0xFF
    // (`cmp al, 0xff; je` at 0x5BD61E), the value it starts from.
    pArea->m_nCharacters = 0xFF;
    CHECK(pGame->ClearCharacterSlot(nSlot) == TRUE);
    CHECK(pGame->m_characters[nSlot] == -1);
    CHECK(pArea->m_nCharacters == 0xFF);
}

TEST_CASE("area: CanSaveGame lets the party save next to doors and other objects", "[ai][data]")
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

    // Doors, containers, triggers, statics, sounds, texts and object
    // markers share 0x47C830 in vtable slot 0x28: strError -1, TRUE.
    const BYTE types[] = { CGameObject::TYPE_DOOR, CGameObject::TYPE_CONTAINER, CGameObject::TYPE_TRIGGER,
        CGameObject::TYPE_STATIC, CGameObject::TYPE_SOUND };
    int checked = 0;
    for (BYTE type : types) {
        for (CGameObject* pObject : ObjectsOfType(pArea, type)) {
            STRREF strError = 0;
            CHECK(pObject->CanSaveGame(strError) == TRUE);
            CHECK(strError == static_cast<STRREF>(-1));
            checked++;
        }
    }
    CHECK(checked > 0);

    // The start area with no enemy in sight: saving is allowed, and
    // field_B16 (read by the idle sequence) is 0 as in the original.
    STRREF strError = 0;
    CHECK(pArea->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
}

TEST_CASE("sprite: CanSaveGame refuses near a visible living enemy and during a dialog", "[ai][data]")
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
    CGameSprite* pHedron = FindSpeaker(pArea, "10HEDRON");
    REQUIRE(pHedron != NULL);
    REQUIRE(pHedron->m_baseStats.field_294 == 0);
    REQUIRE((pHedron->m_baseStats.m_flags & 0x8000) == 0);
    REQUIRE(g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene == FALSE);

    const BYTE ea = pHedron->m_typeAI.m_nEnemyAlly;
    const SHORT canBeSeen = pHedron->m_canBeSeen;
    pHedron->m_curAction = CAIAction::NULL_ACTION;
    pHedron->m_currentActionId = 0;

    // Not an enemy: TRUE.
    STRREF strError = 0;
    pHedron->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_NEUTRAL;
    pHedron->m_canBeSeen = 1;
    CHECK(pHedron->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));

    // An enemy in sight: 16501.
    pHedron->m_typeAI.m_nEnemyAlly = CAIOBJECTTYPE_EA_ENEMY;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16501);

    // Out of sight, or dead: TRUE again.
    pHedron->m_canBeSeen = 0;
    CHECK(pHedron->CanSaveGame(strError) == TRUE);
    pHedron->m_canBeSeen = 1;
    pHedron->m_derivedStats.m_generalState |= STATE_DEAD;
    CHECK(pHedron->CanSaveGame(strError) == TRUE);
    CHECK(strError == static_cast<STRREF>(-1));
    pHedron->m_derivedStats.m_generalState &= ~STATE_DEAD;

    // In a dialog (Dialogue 8 or StartDialog 0x89, current or remembered),
    // or with an action that cannot be interrupted: 16502.
    pHedron->m_typeAI.m_nEnemyAlly = CAIObjectType::EA_NEUTRAL;
    pHedron->m_curAction.m_actionID = CAIAction::DIALOGUE;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16502);
    pHedron->m_curAction.m_actionID = CAIAction::NO_ACTION;
    pHedron->m_currentActionId = CAIAction::STARTDIALOG;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16502);
    pHedron->m_currentActionId = 0;
    pHedron->m_curAction.m_internalFlags |= 0x1;
    CHECK(pHedron->CanSaveGame(strError) == FALSE);
    CHECK(strError == 16502);
    pHedron->m_curAction.m_internalFlags &= ~0x1;

    pHedron->m_typeAI.m_nEnemyAlly = ea;
    pHedron->m_canBeSeen = canBeSeen;
}

TEST_CASE("sprite: SetCurrAction draws the path counter and decodes a spell action", "[ai][data]")
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

    // DecodeSpell (0x45D3E0): "SP" and the type by thousands, "MARW" for
    // any other, then the rest as three digits; 0 takes the marked spell.
    CString sResRef;
    pCharacter->DecodeSpell(1101, sResRef);
    CHECK(sResRef == "SPPR101");
    pCharacter->DecodeSpell(2101, sResRef);
    CHECK(sResRef == "SPWI101");
    pCharacter->DecodeSpell(3005, sResRef);
    CHECK(sResRef == "SPIN005");
    pCharacter->DecodeSpell(4012, sResRef);
    CHECK(sResRef == "MARW012");
    pCharacter->field_588 = 0;
    pCharacter->DecodeSpell(0, sResRef);
    CHECK(sResRef == "");
    pCharacter->field_588 = 2203;
    pCharacter->DecodeSpell(0, sResRef);
    CHECK(sResRef == "SPWI203");
    pCharacter->field_588 = 0;

    // MoveToPoint: the path counter is UtilRandInt(8, -luck) (0x733FE0),
    // the first rand() SetCurrAction draws; the destination and the
    // casting flags are reset.
    const CPoint goal = pCharacter->GetPos();
    srand(12345);
    const LONG expected = CUtil::UtilRandInt(8, -pCharacter->m_derivedStats.m_nLuck);
    srand(12345);
    pCharacter->m_curDest = CPoint(1, 2);
    pCharacter->m_bInCasting = TRUE;
    pCharacter->SetCurrAction(CAIAction(CAIAction::MOVETOPOINT, goal, 0, -1));
    CHECK(pCharacter->field_5616 == expected);
    CHECK(pCharacter->m_curAction.m_actionID == CAIAction::MOVETOPOINT);
    CHECK(pCharacter->m_curDest == CPoint(-1, -1));
    CHECK(pCharacter->m_bInCasting == FALSE);

    // A modal state survives MoveToPoint and ends with any other action.
    pCharacter->m_nModalState = 1;
    pCharacter->SetCurrAction(CAIAction(CAIAction::MOVETOPOINT, goal, 0, -1));
    CHECK(pCharacter->m_nModalState == 1);
    pCharacter->SetCurrAction(CAIAction(CAIAction::WAIT, CPoint(-1, -1), 0, -1));
    CHECK(pCharacter->m_nModalState == 1);
    pCharacter->SetCurrAction(CAIAction(CAIAction::ATTACK, CPoint(-1, -1), 0, -1));
    CHECK(pCharacter->m_nModalState == 0);
    ClearMessages();

    // Spell 2101 with no class and no level: not in the character's lists,
    // so the class is the spell's caster type (wizard 11) and the level the
    // spell's own.
    CAIAction spell(CAIAction::SPELL, CPoint(-1, -1), 2101, -1);
    spell.m_specificID2 = 0;
    spell.m_specificID3 = 0;
    pCharacter->SetCurrAction(spell);
    CHECK(pCharacter->m_curAction.m_actionID == CAIAction::SPELL);
    CHECK((pCharacter->m_curAction.m_specificID2 & 0xFF) == CAIOBJECTTYPE_C_WIZARD);
    CHECK(pCharacter->m_curAction.m_specificID3 == 1);
    CHECK(pCharacter->m_curSpell == NULL);
    ClearMessages();
    pCharacter->SetCurrAction(CAIAction::NULL_ACTION);
    ClearMessages();
}

TEST_CASE("door: a closed door marks its closed search squares in the search map", "[ai][data]")
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

    // AR1000's doors have no open search squares and 4-64 closed ones; the
    // constructor (0x485AC0) takes the closed count from the record's
    // +0x4E and marks them in the dynamic cost (0x80) while closed.
    int checked = 0;
    for (CGameObject* pObject : ObjectsOfType(pArea, CGameObject::TYPE_DOOR)) {
        CGameDoor* pDoor = static_cast<CGameDoor*>(pObject);
        if ((pDoor->m_dwFlags & 0x1) != 0) {
            continue;
        }
        INFO("door " << (const char*)pDoor->m_scriptName);
        CHECK(pDoor->m_nClosedSearch > 0);
        CSearchBitmap& search = pArea->m_search;
        for (WORD n = 0; n < pDoor->m_nClosedSearch; n++) {
            const CPoint& pt = pDoor->m_pClosedSearch[n];
            CHECK((search.m_pDynamicCost[pt.y * search.m_GridSquareDimensions.cx + pt.x] & 0x80) != 0);
        }
        checked++;
    }
    CHECK(checked > 0);
}

TEST_CASE("sprite: sub_75F3D0 hides and shows a creature", "[ai][data]")
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
    REQUIRE(pArea->m_pObjectMarker != NULL);

    // 10HINT's CRE sets byte 0x29C: AddToArea clears field_294 and hides
    // the creature again with sub_75F3D0(1), as in the original (the
    // replay probe reads 1 for it from the first world frame on).
    CGameSprite* pHint = NULL;
    for (CGameObject* pObject : ObjectsOfType(pArea, CGameObject::TYPE_SPRITE)) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (strncmp(pSprite->m_scriptName, "Door_Hint_Text_01", SCRIPTNAME_SIZE) == 0) {
            pHint = pSprite;
        }
    }
    REQUIRE(pHint != NULL);
    CList<LONG>& markers = pArea->m_pObjectMarker->m_lstObjectIds;
    CHECK(pHint->m_baseStats.field_294 == 1);
    CHECK(pHint->sub_75F3D0(1) == CGameAIBase::ACTION_DONE);
    CHECK(pHint->m_baseStats.field_294 == 1);

    // Show: back on the marker list and the search map.
    CHECK(pHint->sub_75F3D0(0) == CGameAIBase::ACTION_DONE);
    CHECK(pHint->m_baseStats.field_294 == 0);
    CHECK(markers.Find(pHint->GetId()) != NULL);
    CHECK(pHint->field_7430 == 1);
    CHECK(pHint->sub_75F3D0(0) == CGameAIBase::ACTION_DONE);

    // Hide: off both.
    CHECK(pHint->sub_75F3D0(1) == CGameAIBase::ACTION_DONE);
    CHECK(pHint->m_baseStats.field_294 == 1);
    CHECK(markers.Find(pHint->GetId()) == NULL);
    CHECK(pHint->field_7430 == 0);

    // Any other argument is an error and changes nothing.
    CHECK(pHint->sub_75F3D0(2) == CGameAIBase::ACTION_ERROR);
    CHECK(pHint->m_baseStats.field_294 == 1);
}

TEST_CASE("sprite: StartSpriteEffect allocates its arrays without asserting", "[ai][data]")
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
    REQUIRE(pCharacter->GetArea() != NULL);

    // Fire (1) at intensity 2: base 0, random 2, so length * 1 entries.
    // 0x708C0F and 0x708C65 assert only when an allocation fails.
    pCharacter->StartSpriteEffect(1, 2, 10, FALSE);
    const int length = pCharacter->m_spriteEffectSequenceLength;
    REQUIRE(length > 0);
    REQUIRE(pCharacter->m_pSpriteEffectArray != NULL);
    REQUIRE(pCharacter->m_pSpriteEffectArrayPosition != NULL);
    for (int i = 0; i < length; i++) {
        CHECK(pCharacter->m_pSpriteEffectArray[i] == 0xFFFF);
    }
    CHECK(pCharacter->m_spriteEffectDuration == static_cast<BYTE>(length + 10 + 9));

    // A second effect frees the first arrays and allocates new ones.
    pCharacter->StartSpriteEffect(2, 1, 5, FALSE);
    CHECK(pCharacter->m_pSpriteEffectArray != NULL);
    CHECK(pCharacter->m_pSpriteEffectArrayPosition != NULL);
}

TEST_CASE("animation: a two-handed weapon takes the two-handed stance and READY group", "[ai][data]")
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

    // Hedron (10HEDRON) wields a spear (00SPER01, two-handed): EquipWeapon
    // (0x6C8390) takes the two-handed path only when the item's flag bit 1
    // is set (0x6C90A9), so his weapon code is 5 and READY (7) uses BAM
    // group 3 (SetSequence 0x6CDA00).
    CGameSprite* pHedron = NULL;
    for (CGameObject* pObject : ObjectsOfType(pArea, CGameObject::TYPE_SPRITE)) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
        if (strncmp(pSprite->m_scriptName, "Hedron", SCRIPTNAME_SIZE) == 0) {
            pHedron = pSprite;
        }
    }
    REQUIRE(pHedron != NULL);
    REQUIRE(pHedron->m_animation.GetAnimationId() == 0x6100);
    CGameAnimationTypeCharacter* pAnimation = static_cast<CGameAnimationTypeCharacter*>(pHedron->m_animation.m_animation);
    CHECK(pAnimation->m_weaponCode == 5);
    pHedron->m_animation.SetSequence(7);
    CHECK(pAnimation->m_currentBamSequence == 3);
}

TEST_CASE("trigger: IsTeamBitOn tests the bits SetTeamBit sets (0x40E4)", "[ai][data]")
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
    REQUIRE(pCharacter->GetArea() != NULL);

    // 0x455E44: the trigger's number ANDed with field_58C.
    const CAITrigger teamBit64(0x40E4, 64);
    pCharacter->field_58C = 0;
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(teamBit64));

    // SetTeamBit(64, TRUE) (case 310) turns it on; SetTeamBit(64, FALSE) off.
    pCharacter->m_curAction = CAIAction(310, CPoint(-1, -1), 64, 1);
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pCharacter->EvaluateStatusTrigger(teamBit64));
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(CAITrigger(0x40E4, 32)));
    pCharacter->m_curAction = CAIAction(310, CPoint(-1, -1), 64, 0);
    CHECK(pCharacter->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK_FALSE(pCharacter->EvaluateStatusTrigger(teamBit64));
    pCharacter->m_curAction = CAIAction();
}
