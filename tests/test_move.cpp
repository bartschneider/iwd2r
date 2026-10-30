#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIGroup.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CMoveList.h"
#include "CPathSearch.h"
#include "CScreenCreateChar.h"
#include "CSearchBitmap.h"
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

// Movement on the engine in this process: the limbo list (CMoveList::
// AIUpdate), the group's follow path (CAIGroup::HandleFollowPath) and a
// sprite walking in the first area through the search thread
// (CGameSprite::SetTarget, SearchThreadMain, AIUpdateWalk). Every engine
// works on a private view of the install (tests/data_view.h).

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
        static_cast<DWORD>(-1),
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

int CountIn(CMoveList& list, CMoveListEntry* pEntry)
{
    int count = 0;
    POSITION pos = list.GetHeadPosition();
    while (pos != NULL) {
        if (list.GetNext(pos) == pEntry) {
            count++;
        }
    }
    return count;
}

// The search square of a point.
CPoint Square(const CPoint& pt)
{
    return CPoint(pt.x / CPathSearch::GRID_SQUARE_SIZEX, pt.y / CPathSearch::GRID_SQUARE_SIZEY);
}

// Whether the terrain of a square is open to the sprite, whoever stands on
// it.
BOOL TerrainPassable(CGameSprite* pSprite, const CPoint& square)
{
    CVidBitmap& bitmap = pSprite->GetArea()->m_search.m_resSearch;
    BYTE value = bitmap.GetPixelValue(square.x, square.y, TRUE);
    if (bitmap.GetBitCount(TRUE) == 8) {
        value >>= 4;
    }
    return pSprite->m_terrainTable[value] != CPathSearch::COST_IMPASSABLE;
}

// The search thread's snapshot treats unexplored squares as walls for the
// party (CSearchBitmap::SnapshotGetCost); the party's sight explores the
// area in the game. Here the whole area is explored.
void ExploreArea(CGameSprite* pSprite)
{
    pSprite->GetArea()->m_visibility.SetAreaExplored();
}

// Runs the sprite's walk as the world's AI update does, one call per frame,
// with the message handler pumped after it, until the sprite has neither a
// path nor a search request, or `limit` frames. While the search thread
// still works on the request the frame waits for it (the replay holds the
// frame the same way). Returns the frames run.
int Walk(CGameSprite* pSprite, int limit)
{
    int frames = 0;
    while (frames < limit) {
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
        }

        if (std::getenv("IWD2R_TEST_TRACE") != nullptr) {
            std::fprintf(stderr, "frame %d request %d path %d pos %d,%d\n", frames,
                pSprite->m_currentSearchRequest != NULL ? pSprite->m_currentSearchRequest->m_serviceState : -1,
                pSprite->m_pPath != NULL ? pSprite->m_nPath : 0, pSprite->GetPos().x, pSprite->GetPos().y);
        }
        pSprite->AIUpdateWalk();
        g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
        frames++;

        if (pSprite->m_pPath == NULL && pSprite->m_currentSearchRequest == NULL) {
            break;
        }
    }
    return frames;
}

}

TEST_CASE("move: the limbo list counts down and hands a move to an unloaded area to the move list", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    if (NewGameWithParty(1).empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    REQUIRE(pGame->GetArea(CString("AR1000")) != NULL);
    REQUIRE(pGame->GetArea(CString("AR9999")) == NULL);

    // A move to an area that is not loaded waits out its delay in the limbo
    // list, then goes to the game's move list.
    CMoveListEntry* pWaiting = new CMoveListEntry();
    pWaiting->m_nSpriteId = CGameObjectArray::INVALID_INDEX;
    pWaiting->m_areaResRef = "AR9999";
    pWaiting->m_nDelay = 2;
    pGame->m_cLimboList.AddTail(pWaiting);

    // A move to a loaded area whose sprite is gone is dropped (the original
    // leaks the node).
    CMoveListEntry* pGone = new CMoveListEntry();
    pGone->m_nSpriteId = 0x7FFF0000;
    pGone->m_areaResRef = "AR1000";
    pGone->m_nDelay = 0;
    pGame->m_cLimboList.AddTail(pGone);

    pGame->m_cLimboList.AIUpdate();
    CHECK(CountIn(pGame->m_cLimboList, pWaiting) == 1);
    CHECK(pWaiting->m_nDelay == 1);
    CHECK(CountIn(pGame->m_cLimboList, pGone) == 0);
    CHECK(CountIn(pGame->m_cMoveList, pGone) == 0);

    pGame->m_cLimboList.AIUpdate();
    CHECK(CountIn(pGame->m_cLimboList, pWaiting) == 0);
    CHECK(CountIn(pGame->m_cMoveList, pWaiting) == 1);
    CHECK(pWaiting->m_nDelay == 0);

    // A move with no delay left is handed over on the next update too.
    pGame->m_cMoveList.RemoveAll();
    delete pWaiting;
    delete pGone;
}

TEST_CASE("move: Marshal queues the moving sprite in the area's saved list", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = party[0];
    CGameArea* pArea = pSprite->GetArea();
    REQUIRE(pArea != NULL);

    // A sprite in transit belongs to no area list; Marshal puts its id in the
    // area's field_AAA, which CGameArea::Marshal saves with the creatures.
    CMoveListEntry* pEntry = new CMoveListEntry();
    pEntry->m_nSpriteId = pSprite->GetId();
    pEntry->m_areaResRef = "AR1000";
    pEntry->m_ptDestination = pSprite->GetPos();
    pGame->m_cMoveList.AddTail(pEntry);

    pArea->field_AAA.RemoveAll();
    pGame->m_cMoveList.Marshal(pArea);

    REQUIRE(pArea->field_AAA.GetCount() == 1);
    CHECK(reinterpret_cast<INT_PTR>(pArea->field_AAA.GetHead()) == pSprite->GetId());

    pGame->m_cMoveList.RemoveAll();
    delete pEntry;
}

TEST_CASE("move: HandleFollowPath replaces the followers' actions with the leader's path", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(3);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(party.size() == 3);

    CAIGroup group;
    for (CGameSprite* pSprite : party) {
        group.m_memberList.AddTail(reinterpret_cast<LONG*>(static_cast<INT_PTR>(pSprite->GetId())));
        pSprite->m_queuedActions.RemoveAll();
        pSprite->m_interrupt = FALSE;
    }

    LONG path[3] = { 1000, 1001, 1002 };
    const CPoint target(500, 400);
    const CPoint start(300, 250);

    group.HandleFollowPath(path, 3, target, start, FALSE);

    // The leader gets no action and no path.
    CHECK(party[0]->m_queuedActions.GetCount() == 0);
    CHECK(party[0]->m_pPathTemp.GetCount() == 0);
    CHECK(party[0]->m_inFormation == TRUE);

    for (size_t n = 1; n < party.size(); n++) {
        INFO("follower " << n);
        CGameSprite* pSprite = party[n];
        // AddAction puts each at the head; the queue runs from the
        // tail, so the Follow comes first.
        REQUIRE(pSprite->m_queuedActions.GetCount() == 3);
        POSITION pos = pSprite->m_queuedActions.GetHeadPosition();
        CAIAction* p99 = pSprite->m_queuedActions.GetNext(pos);
        CHECK(p99->m_actionID == 99);
        CAIAction* pWait = pSprite->m_queuedActions.GetNext(pos);
        CHECK(pWait->m_actionID == CAIAction::SMALLWAIT);
        CHECK(pWait->m_specificID == 5);
        CAIAction* pFollow = pSprite->m_queuedActions.GetNext(pos);
        CHECK(pFollow->m_actionID == 89);
        CHECK(pFollow->m_dest == target);
        CHECK(pFollow->m_specificID == static_cast<LONG>(n));
        CHECK(pFollow->m_specificID2 == start.y);

        CHECK(pSprite->m_followLeaderNext
            == (n + 1 < party.size() ? party[n + 1]->GetId() : CGameObjectArray::INVALID_INDEX));
        CHECK(pSprite->m_userCommandPause == CGameSprite::USER_OVERRIDE_COUNT);
        CHECK(pSprite->m_triggerId == CGameObjectArray::INVALID_INDEX);
        CHECK(pSprite->m_interrupt == TRUE);
        CHECK(pSprite->m_inFormation == TRUE);

        REQUIRE(pSprite->m_pPathTemp.GetCount() == 1);
        REQUIRE(pSprite->m_nPathTemp.GetCount() == 1);
        const int* pCopy = pSprite->m_pPathTemp.GetHead();
        CHECK(pCopy != reinterpret_cast<int*>(path));
        CHECK(std::memcmp(pCopy, path, sizeof(path)) == 0);
        CHECK(static_cast<int>(reinterpret_cast<INT_PTR>(pSprite->m_nPathTemp.GetHead())) == 3);
    }
}

TEST_CASE("move: HandleFollowPath adds the follow to the actions of moving followers", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(3);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(party.size() == 3);

    CAIGroup group;
    for (CGameSprite* pSprite : party) {
        group.m_memberList.AddTail(reinterpret_cast<LONG*>(static_cast<INT_PTR>(pSprite->GetId())));
        pSprite->m_queuedActions.RemoveAll();
        pSprite->m_interrupt = FALSE;
    }

    LONG path[3] = { 1000, 1001, 1002 };
    const CPoint target(500, 400);
    const CPoint start(300, 250);

    CAIAction move(CAIAction::MOVETOPOINT, CPoint(10, 10), 0, -1);
    party[1]->m_curAction = move;
    party[2]->m_curAction = CAIAction();

    group.HandleFollowPath(path, 3, target, start, TRUE);

    // A follower whose current action is a move gets only the Follow.
    REQUIRE(party[1]->m_queuedActions.GetCount() == 1);
    CHECK(party[1]->m_queuedActions.GetHead()->m_actionID == 89);
    CHECK(party[1]->m_interrupt == FALSE);

    // Another follower gets the Follow, the wait and the 99: InsertAction
    // puts each at the tail, last first, so the Follow runs first.
    REQUIRE(party[2]->m_queuedActions.GetCount() == 3);
    CHECK(party[2]->m_queuedActions.GetHead()->m_actionID == 99);
    CHECK(party[2]->m_queuedActions.GetTail()->m_actionID == 89);
    CHECK(party[2]->m_interrupt == TRUE);
    CHECK(party[2]->m_pPathTemp.GetCount() == 1);
}

TEST_CASE("move: a sprite walks to a reachable point through the search thread", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pSprite = party[0];
    REQUIRE(pSprite->GetArea() != NULL);
    // Objects AddToArea queued join the area's lists on its next update.
    pSprite->GetArea()->SortLists();
    ExploreArea(pSprite);

    const CPoint from = pSprite->GetPos();
    const CPoint fromSquare = Square(from);
    INFO("start " << from.x << "," << from.y);

    // A square some distance to the side with open ground and no creature
    // on the way (two rows either side), so the walk is not bumped.
    CSearchBitmap& search = pSprite->GetArea()->m_search;
    auto Clear = [&](const CPoint& square) {
        if (square.x < 0 || square.y < 0 || square.x >= search.m_GridSquareDimensions.cx
            || square.y >= search.m_GridSquareDimensions.cy) {
            return false;
        }
        return TerrainPassable(pSprite, square)
            && search.m_pDynamicCost[square.y * search.m_GridSquareDimensions.cx + square.x] == 0;
    };
    CPoint goalSquare(-1, -1);
    const CPoint directions[] = { CPoint(1, 0), CPoint(-1, 0), CPoint(0, 1), CPoint(0, -1), CPoint(1, 1),
        CPoint(-1, 1), CPoint(1, -1), CPoint(-1, -1) };
    for (int distance = 16; distance >= 6 && goalSquare.x == -1; distance--) {
        for (const CPoint& direction : directions) {
            bool open = true;
            for (int step = 3; step <= distance + 1 && open; step++) {
                open = Clear(CPoint(fromSquare.x + direction.x * step, fromSquare.y + direction.y * step));
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
    INFO("goal " << goal.x << "," << goal.y);

    pSprite->m_curDest = goal;
    pSprite->SetTarget(goal, FALSE);
    REQUIRE(pSprite->m_currentSearchRequest != NULL);

    const int frames = Walk(pSprite, 2000);
    INFO("frames " << frames);
    CHECK(frames < 2000);
    CHECK(pSprite->m_pPath == NULL);
    CHECK(pSprite->m_currentSearchRequest == NULL);
    CHECK(pSprite->m_pathSearchInvalidDest == FALSE);
    // The walk ends in the goal's square.
    CHECK(Square(pSprite->GetPos()) == goalSquare);
    // It took at least one frame per square of the distance at the
    // animation's move scale (pixels per frame).
    const int distance = std::max(std::abs(goal.x - from.x), std::abs(goal.y - from.y));
    CHECK(frames >= distance / (pSprite->GetAnimation()->GetMoveScale() + 1));
}

TEST_CASE("move: a sprite sent into a wall stops short and never loops", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pSprite = party[0];
    REQUIRE(pSprite->GetArea() != NULL);
    pSprite->GetArea()->SortLists();
    ExploreArea(pSprite);

    const CPoint fromSquare = Square(pSprite->GetPos());

    // The nearest impassable square in a line from the start.
    CPoint wallSquare(-1, -1);
    for (int d = 2; d < 60 && wallSquare.x == -1; d++) {
        const CPoint around[] = { CPoint(fromSquare.x + d, fromSquare.y), CPoint(fromSquare.x - d, fromSquare.y),
            CPoint(fromSquare.x, fromSquare.y + d), CPoint(fromSquare.x, fromSquare.y - d) };
        for (const CPoint& candidate : around) {
            if (candidate.x > 0 && candidate.y > 0
                && candidate.x < pSprite->GetArea()->m_search.m_GridSquareDimensions.cx
                && candidate.y < pSprite->GetArea()->m_search.m_GridSquareDimensions.cy
                && !TerrainPassable(pSprite, candidate)) {
                wallSquare = candidate;
                break;
            }
        }
    }
    REQUIRE(wallSquare.x != -1);
    const CPoint wall(wallSquare.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
        wallSquare.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);
    INFO("wall " << wall.x << "," << wall.y);

    pSprite->m_curDest = wall;
    pSprite->SetTarget(wall, FALSE);

    const int frames = Walk(pSprite, 3000);
    INFO("frames " << frames);
    // Bounded: the walk ends, with no path and no request left.
    CHECK(frames < 3000);
    CHECK(pSprite->m_pPath == NULL);
    CHECK(pSprite->m_currentSearchRequest == NULL);
    // The sprite stands on open ground, not in the wall.
    CHECK(Square(pSprite->GetPos()) != wallSquare);
    CHECK(TerrainPassable(pSprite, Square(pSprite->GetPos())));
}

// The original's walk_and_talk recording (seed 1, dumped every frame): after
// the opening dialog the party member (Probe) stands at 2414,897; the click
// on the deck sends it to search square (145, 75), and it walks a straight
// smoothed line in nine steps of about 9.5 pixels (move scale 10), one step
// every second frame (frames 2557-2573), stopping at 2328,905 with its
// action cleared at frame 2575; `moved` (2643) has it there. These are the
// positions after each step.
TEST_CASE("move: the first click-to-move of walk_and_talk steps where the original's does", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pSprite = party[0];
    REQUIRE(pSprite->GetArea() != NULL);
    pSprite->GetArea()->SortLists();
    ExploreArea(pSprite);
    REQUIRE(pSprite->GetPos() == CPoint(2414, 897));
    // The premade character is not Probe; it walks at Probe's move scale.
    pSprite->GetAnimation()->SetMoveScale(10);
    REQUIRE(pSprite->GetAnimation()->GetPathSmooth());

    const CPoint goal(145 * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
        75 * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);
    pSprite->m_curDest = goal;
    pSprite->SetTarget(goal, FALSE);

    std::vector<CPoint> steps;
    CPoint last = pSprite->GetPos();
    for (int frame = 0; frame < 200; frame++) {
        Walk(pSprite, 1);
        if (pSprite->GetPos() != last) {
            last = pSprite->GetPos();
            steps.push_back(last);
        }
        if (pSprite->m_pPath == NULL && pSprite->m_currentSearchRequest == NULL && frame > 0) {
            break;
        }
    }

    const CPoint original[] = { CPoint(2404, 897), CPoint(2394, 898), CPoint(2385, 899), CPoint(2375, 900),
        CPoint(2366, 901), CPoint(2356, 902), CPoint(2347, 903), CPoint(2337, 904), CPoint(2328, 905) };
    std::string ours;
    for (const CPoint& pt : steps) {
        ours += " " + std::to_string(pt.x) + "," + std::to_string(pt.y);
    }
    INFO("ours:" << ours);
    REQUIRE(steps.size() == std::size(original));
    for (size_t n = 0; n < steps.size(); n++) {
        CHECK(steps[n] == original[n]);
    }
    CHECK(pSprite->m_pPath == NULL);
    CHECK(pSprite->m_currentSearchRequest == NULL);
}

TEST_CASE("move: AIUpdateWalk leaves a sprite with no path where it is", "[move][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    std::vector<CGameSprite*> party = NewGameWithParty(1);
    if (party.empty()) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    CGameSprite* pSprite = party[0];
    REQUIRE(pSprite->GetArea() != NULL);
    pSprite->GetArea()->SortLists();
    ExploreArea(pSprite);

    // Idle: no path, no request.
    REQUIRE(pSprite->m_pPath == NULL);
    REQUIRE(pSprite->m_currentSearchRequest == NULL);
    const CPoint idle = pSprite->GetPos();
    const SHORT sequence = pSprite->m_nSequence;
    for (int frame = 0; frame < 20; frame++) {
        pSprite->AIUpdateWalk();
    }
    CHECK(pSprite->GetPos() == idle);
    CHECK(pSprite->m_nSequence == sequence);
    CHECK(pSprite->m_pPath == NULL);

    // A walk whose path is dropped half way stops where it is.
    const CPoint goal(145 * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
        75 * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);
    pSprite->m_curDest = goal;
    pSprite->SetTarget(goal, FALSE);
    Walk(pSprite, 4);
    REQUIRE(pSprite->m_pPath != NULL);
    pSprite->DropPath();
    const CPoint dropped = pSprite->GetPos();
    for (int frame = 0; frame < 20; frame++) {
        pSprite->AIUpdateWalk();
    }
    CHECK(pSprite->GetPos() == dropped);
    CHECK(pSprite->m_pPath == NULL);
}
