#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CGameAIBase.h"
#include "CGameArea.h"
#include "CGameDoor.h"
#include "CGameObjectArray.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CVariableHash.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>

namespace {

// The engine in this process, as iwd2r_main builds it, without a game
// loaded: the game object, its variables, object array and message handler
// exist. Built once and leaked: the engine's globals live until the process
// ends. The engine writes into its data folder: it starts on a private view.
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
    const char* install = std::getenv("IWD2_DATA");
    if (install != nullptr && *install != '\0') {
        const std::string original = install;
        view = std::make_unique<DataView>(original);
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
    } else if (!platform::ResolveDataDir()) {
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
    started = true;
    return true;
}

#define REQUIRE_ENGINE()                                    \
    do {                                                    \
        const char* env = std::getenv("IWD2_DATA");         \
        if (env == nullptr || *env == '\0') {               \
            SKIP("[iwd2r-skip] IWD2_DATA unset");           \
        }                                                   \
        std::string error;                                  \
        INFO(error);                                        \
        REQUIRE(StartEngine(error));                        \
    } while (0)

// An AI object in the game's object array, as an area's objects are.
CGameAIBase* AddAIObject()
{
    CGameAIBase* pObject = new CGameAIBase();
    LONG id = CGameObjectArray::INVALID_INDEX;
    REQUIRE(g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&id, pObject, INFINITE)
        == CGameObjectArray::SUCCESS);
    pObject->m_id = id;
    return pObject;
}

// A door: zeroed storage with its members that own memory constructed and
// its flags set. ToggleDoor reads only these when the door has no search
// polygons.
CGameDoor* MakeDoor(DWORD dwFlags)
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameDoor)));
    std::memset(storage, 0, sizeof(CGameDoor));
    CGameDoor* pDoor = reinterpret_cast<CGameDoor*>(storage);
    new (&pDoor->m_sound) CSound();
    new (&pDoor->m_pendingTriggers) CTypedPtrList<CPtrList, CAITrigger*>();
    new (&pDoor->m_curAction) CAIAction();
    // The area: zeroed storage named AR1000, its search map empty (the door
    // has no search polygons).
    auto* area = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
    std::memset(area, 0, sizeof(CGameArea));
    pDoor->m_pArea = reinterpret_cast<CGameArea*>(area);
    std::memcpy(pDoor->m_pArea->m_header.m_areaName, "AR1000", 6);
    pDoor->m_dwFlags = dwFlags;
    pDoor->m_strNotPickable = static_cast<STRREF>(-1);
    pDoor->m_ptOpenDest = CPoint(10, 20);
    pDoor->m_ptClosedDest = CPoint(30, 40);
    // Not in the object array (that would need the door's constructor and an
    // area file): its status messages have no target and are dropped.
    pDoor->m_id = CGameObjectArray::INVALID_INDEX;
    return pDoor;
}

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

}

TEST_CASE("ai: ExecuteAction answers NoAction, an unknown action and SetGlobal", "[ai][data]")
{
    REQUIRE_ENGINE();
    CGameAIBase* pCaller = AddAIObject();

    pCaller->SetCurrAction(CAIAction::NULL_ACTION);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_NO_ACTION);
    CHECK(pCaller->m_nLastActionReturn == CGameAIBase::ACTION_NO_ACTION);

    // Attack (3) is a sprite's action: the base dispatcher reports it.
    CAIAction attack;
    attack.m_actionID = CAIAction::ATTACK;
    pCaller->SetCurrAction(attack);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_ERROR);

    // SetGlobal("ai_test_var","GLOBAL",5): the script compiler's string1 is
    // the scope followed by the name.
    CAIAction setGlobal;
    setGlobal.m_actionID = 30;
    setGlobal.m_string1 = "GLOBALai_test_var";
    setGlobal.m_specificID = 5;
    pCaller->SetCurrAction(setGlobal);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("AI_TEST_VAR"));
    REQUIRE(pVariable != NULL);
    CHECK(pVariable->m_intValue == 5);

    // IncrementGlobal by 3.
    CAIAction increment;
    increment.m_actionID = 109;
    increment.m_string1 = "GLOBALAI_TEST_VAR";
    increment.m_specificID = 3;
    pCaller->SetCurrAction(increment);
    CHECK(pCaller->ExecuteAction() == CGameAIBase::ACTION_DONE);
    CHECK(pVariable->m_intValue == 8);

    // The status triggers read it back: Global, GlobalGT, GlobalLT.
    CAITrigger global(0x400F, 8);
    global.m_string1 = "GLOBALAI_TEST_VAR";
    CHECK(pCaller->EvaluateStatusTrigger(global) == TRUE);
    CAITrigger globalGT(0x4034, 8);
    globalGT.m_string1 = "GLOBALAI_TEST_VAR";
    CHECK(pCaller->EvaluateStatusTrigger(globalGT) == FALSE);
    CAITrigger globalLT(0x4035, 9);
    globalLT.m_string1 = "GLOBALAI_TEST_VAR";
    CHECK(pCaller->EvaluateStatusTrigger(globalLT) == TRUE);
    // A variable nobody set reads as 0.
    CAITrigger missing(0x400F, 0);
    missing.m_string1 = "GLOBALAI_NEVER_SET";
    CHECK(pCaller->EvaluateStatusTrigger(missing) == TRUE);

    ClearMessages();
}

TEST_CASE("ai: a locked door stays closed and says it is locked", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameAIBase* pClicker = AddAIObject();
    CGameDoor* pDoor = MakeDoor(0x2);

    CAIObjectType cause(0, 0, 0, 0, 0, 0, 0, 0, pClicker->m_id, 0, 0);
    pDoor->ToggleDoor(cause, FALSE);

    CHECK((pDoor->m_dwFlags & 0x1) == 0);
    CHECK((pDoor->m_dwFlags & 0x2) != 0);
    CHECK(pDoor->m_pos == CPoint(0, 0));

    // The feedback: "Locked" (15874) to the one who tried it, in the
    // original's colors.
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    auto* message = dynamic_cast<CMessageDisplayTextRefSend*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_text == 15874);
    CHECK(message->m_name == static_cast<STRREF>(-1));
    CHECK(message->m_textColor == RGB(215, 215, 190));
    CHECK(message->m_targetId == pClicker->m_id);
    CHECK(message->m_sourceId == pDoor->m_id);

    // A door with its own "locked" text says that instead.
    ClearMessages();
    pDoor->m_dwFlags |= 0x200;
    pDoor->m_strNotPickable = 1234;
    pDoor->ToggleDoor(cause, FALSE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    message = dynamic_cast<CMessageDisplayTextRefSend*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_text == 1234);
    CHECK((pDoor->m_dwFlags & 0x1) == 0);

    ClearMessages();
}

TEST_CASE("ai: a forced toggle opens a locked door, unlocks it, and closes it again", "[ai][data]")
{
    REQUIRE_ENGINE();
    ClearMessages();
    CGameDoor* pDoor = MakeDoor(0x2);
    CAIObjectType cause(0, 0, 0, 0, 0, 0, 0, 0, CGameObjectArray::INVALID_INDEX, 0, 0);

    // A script's OpenDoor (143) toggles with bForce: the lock is ignored and
    // the door ends unlocked.
    pDoor->ToggleDoor(cause, TRUE);
    CHECK((pDoor->m_dwFlags & 0x1) != 0);
    CHECK((pDoor->m_dwFlags & 0x2) == 0);
    CHECK(pDoor->m_pos == CPoint(30, 40));
    CHECK(pDoor->m_tiledObject.m_wAIState == CTiledObject::STATE_PRIMARY_TILE);

    pDoor->ToggleDoor(cause, FALSE);
    CHECK((pDoor->m_dwFlags & 0x1) == 0);
    CHECK(pDoor->m_pos == CPoint(10, 20));
    CHECK(pDoor->m_tiledObject.m_wAIState == CTiledObject::STATE_SECONDARY_TILE);

    ClearMessages();
}
