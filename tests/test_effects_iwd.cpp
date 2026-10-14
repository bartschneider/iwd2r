#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CProjectile850F9C.h"
#include "CProjectileArea.h"
#include "CProjectileSpellHit.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSpell.h"
#include "CUIControlTextDisplay.h"
#include "CVariableHash.h"
#include "IcewindCGameEffects.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

// The ApplyEffect of the Icewind effect classes (IcewindCGameEffects.cpp)
// on a party member standing in AR1000. Each effect is decoded from an
// ITEM_EFFECT as the game decodes it (CGameEffect::DecodeEffect) and applied
// directly. Every engine works on a private view of the install
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

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The party member every case works on, made once.
CGameSprite* Probe()
{
    static CGameSprite* pSprite = NULL;
    if (pSprite == NULL) {
        std::vector<BYTE> cre = FinishedCharacter();
        if (cre.empty()) {
            return NULL;
        }
        pSprite = NewGameInStartArea(cre);
    }
    return pSprite;
}

// An effect of this opcode decoded as the game decodes it, its source the
// sprite itself.
std::unique_ptr<CGameEffect> Decode(CGameSprite* pSprite, WORD effectID, DWORD dwFlags, LONG effectAmount)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    effect.dwFlags = dwFlags;
    effect.effectAmount = effectAmount;
    return std::unique_ptr<CGameEffect>(CGameEffect::DecodeEffect(&effect, pSprite->GetPos(), pSprite->GetId(), pSprite->GetPos()));
}

bool HasPortraitIcon(CGameSprite* pSprite, int icon)
{
    return pSprite->m_portraitIcons.Find(reinterpret_cast<int*>(static_cast<INT_PTR>(icon)), NULL) != NULL;
}

// The objects the area holds in its add lists (what Fire added since the last
// SortLists), in the order they were added.
std::vector<CGameObject*> Added(CGameArea* pArea)
{
    std::vector<CGameObject*> objects;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSortAdd, &pArea->m_lVertSortBackAdd, &pArea->m_lVertSortFlightAdd };
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    for (CTypedPtrList<CPtrList, int*>* pList : lists) {
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(pList->GetNext(pos)));
            CGameObject* pObject;
            if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
                objects.push_back(pObject);
            }
        }
    }
    return objects;
}

// Removes the projectiles the area holds in its add lists and empties them.
void RemoveAdded(CGameArea* pArea)
{
    std::vector<CGameObject*> objects = Added(pArea);
    pArea->SortLists();
    for (CGameObject* pObject : objects) {
        CProjectile* pProjectile = dynamic_cast<CProjectile*>(pObject);
        if (pProjectile != nullptr) {
            pProjectile->RemoveSelf();
        }
    }
}

// The one projectile of this class ApplyEffect added to the area.
template <class T>
T* AddedOne(CGameArea* pArea)
{
    std::vector<CGameObject*> objects = Added(pArea);
    if (objects.size() != 1) {
        return nullptr;
    }
    return dynamic_cast<T*>(objects.front());
}

// The effects a projectile carries.
std::vector<CGameEffect*> Carried(CProjectile* pProjectile)
{
    std::vector<CGameEffect*> effects;
    POSITION pos = pProjectile->m_effectList.GetHeadPosition();
    while (pos != NULL) {
        effects.push_back(pProjectile->m_effectList.GetNext(pos));
    }
    return effects;
}

#define PROBE_OR_SKIP(pSprite)                                          \
    if (DataDir() == nullptr) {                                         \
        SKIP("[iwd2r-skip] IWD2_DATA unset");                           \
    }                                                                   \
    std::string error;                                                  \
    REQUIRE(StartEngine(error));                                        \
    INFO(error);                                                        \
    FramePump pump;                                                     \
    CGameSprite* pSprite = Probe();                                     \
    if (pSprite == NULL) {                                              \
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder"); \
    }

}

TEST_CASE("effects-iwd: immunity to effect (261) adds the opcode to the natural immunities", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_IMMUNITYTOEFFECTRESOURCE, 24, 0);
    REQUIRE(pEffect != nullptr);
    pSprite->GetDerivedStats()->m_naturalImmunities.erase(24);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_naturalImmunities.count(24) == 1);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_naturalImmunities.count(24) == 1);
    pSprite->GetDerivedStats()->m_naturalImmunities.erase(24);
}

TEST_CASE("effects-iwd: visual effect (413) sets the bit and the Lathander and Seven Eyes partners", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::bitset<32> saved = pSprite->GetDerivedStats()->m_visualEffects;
    pSprite->GetDerivedStats()->m_visualEffects.reset();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALEFFECT, IWD_VFX_SHIELD_OF_LATHANDER, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_visualEffects.to_ulong() == 0xC0000);
    pSprite->GetDerivedStats()->m_visualEffects.reset();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALEFFECT, 3, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_visualEffects.to_ulong() == 0x8);
    pSprite->GetDerivedStats()->m_visualEffects = saved;
}

TEST_CASE("effects-iwd: bleeding wounds (416) adds a regenerating persistant effect and its icon", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    for (DWORD flags = 0; flags <= 2; flags++) {
        INT_PTR before = pStats->m_cRegeneratedPersistantEffectList.GetCount();
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_BLEEDINGWOUNDS, flags, 3);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pStats->m_cRegeneratedPersistantEffectList.GetCount() == before + 1);
        CHECK(HasPortraitIcon(pSprite, 85));
    }
    while (!pStats->m_cRegeneratedPersistantEffectList.IsEmpty()) {
        delete pStats->m_cRegeneratedPersistantEffectList.RemoveTail();
    }
}

TEST_CASE("effects-iwd: float text (283) queues the string's message only for type 0", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 0, 4392);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageFloatText* message = dynamic_cast<CMessageFloatText*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_strRef == 4392);
    ClearMessages();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 1, 4392);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(handler.m_messageList.IsEmpty());
}

// 233 itself only queues CMessageFireProjectile and finishes. The queued
// message cannot run yet: CMessageFireProjectile::Run (0x5007F0) is not
// ported, so the base CMessage::Run would assert when the handler's
// AsynchronousUpdate reached it. The case therefore checks the queue and
// removes the message itself; it says nothing about the projectile firing.
TEST_CASE("effects-iwd: visual spell hit (233) queues a CMessageFireProjectile and fires the spell hit", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CGameArea* pArea = pSprite->GetArea();
    pArea->SortLists();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALSPELLHIT, 4, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == FALSE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(!handler.m_messageList.IsEmpty());
    CMessageFireProjectile* message = dynamic_cast<CMessageFireProjectile*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_projectileType == 0x1004);
    CHECK(message->m_projectileTargetId == pSprite->GetId());
    CHECK(message->m_projectileTarget == pSprite->GetPos());
    CHECK(message->m_height == 0);
    CHECK(message->GetSource() == pSprite->GetId());
    CHECK(message->GetTarget() == pSprite->GetId());
    // The spell hit is fired at the sprite and plays in the area.
    CProjectileSpellHit* pHit = AddedOne<CProjectileSpellHit>(pArea);
    REQUIRE(pHit != nullptr);
    CHECK(pHit->GetArea() == pArea);
    RemoveAdded(pArea);
    ClearMessages();

    // A shaft type (45) still decodes to NULL: the message is queued and
    // nothing is fired (the DEVIATION's NULL check).
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALSPELLHIT, 45, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == FALSE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CHECK(Added(pArea).empty());
    ClearMessages();
}

TEST_CASE("effects-iwd: apply effects list (402) stops on a failed condition and is done", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    // 1: only undead. The party member is not: nothing more happens.
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 1, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    // 43: only the source itself, which it is.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 43, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    // 41: compares with the source; an unknown source returns FALSE.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 41, 0);
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == FALSE);

    // 0: no condition. The spell's effects are decoded from the source and
    // added (stats-spell fix round 1: IcewindMisc::GetSpellResrefEffects).
    const INT before = pSprite->m_timedEffectList.GetCount();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 0, 0);
    // SPWI102: AC and timed effects of 3000 ticks.
    pEffect->m_res = "SPWI102";
    pEffect->m_casterLevel = 20;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(pSprite->m_timedEffectList.GetCount() > before);
}

TEST_CASE("effects-iwd: Beltyn's burning blood (247) queues one delayed copy per further round", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CGameEffectList* pList = pSprite->GetTimedEffectList();
    auto delayedCopies = [&]() {
        int n = 0;
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pOther = pList->GetNext(pos);
            if (pOther->m_effectID == ICEWIND_CGAMEEFFECT_BELTYNSBURNINGBLOOD && pOther->m_durationType == 7) {
                n++;
            }
        }
        return n;
    };
    // The copies are delayed (duration type 4), which ResolveEffect turns
    // into 7, waiting for their time.
    const int before = delayedCopies();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_BELTYNSBURNINGBLOOD, 0, 3);
    // An unknown source stops it after the copies: the damage and berserk
    // effects it adds next have no ApplyEffect in CGameEffect.cpp yet.
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(delayedCopies() == before + 2);
}

TEST_CASE("effects-iwd: animation remove (260) and vitriolic sphere (281) apply and finish", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_ANIMATIONREMOVE, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    // Single player: no CMessage90 for the other machines.
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VITRIOLICSPHERE, 0, 0);
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
}

TEST_CASE("effects-iwd: vampiric touch (241) with an unknown source only finishes", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VAMPIRICTOUCH, 0, 5);
    pEffect->m_sourceID = -1;
    const SHORT hp = pSprite->GetBaseStats()->m_hitPoints;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(pSprite->GetBaseStats()->m_hitPoints == hp);
}

TEST_CASE("effects-iwd: death ward (407) sets its state, icon and death immunity once", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    pStats->m_spellStates.reset(SPLSTATE_DEATH_WARD);
    pStats->m_naturalImmunities.erase(ICEWIND_CGAMEEFFECT_MAGIC_DEATH);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_DEATHWARD, 0, 0);
    pEffect->m_done = TRUE;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_spellStates.test(SPLSTATE_DEATH_WARD));
    CHECK(pStats->m_naturalImmunities.count(ICEWIND_CGAMEEFFECT_MAGIC_DEATH) == 1);
    CHECK(HasPortraitIcon(pSprite, 87));
    CHECK(pEffect->m_done == FALSE);
    // Already warded: returns before touching m_done.
    pEffect->m_done = TRUE;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    pStats->m_spellStates.reset(SPLSTATE_DEATH_WARD);
    pStats->m_naturalImmunities.erase(ICEWIND_CGAMEEFFECT_MAGIC_DEATH);
}

TEST_CASE("effects-iwd: persistant effect (434) adds a CPersistantEffect84C4C4 with the source", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    INT_PTR before = pStats->m_cRegeneratedPersistantEffectList.GetCount();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_PERSISTANTEFFECT, 0, 2);
    pEffect->m_sourceRes = "SPWI123";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(pStats->m_cRegeneratedPersistantEffectList.GetCount() == before + 1);
    CPersistantEffect84C4C4* pAdded = dynamic_cast<CPersistantEffect84C4C4*>(pStats->m_cRegeneratedPersistantEffectList.GetTail());
    REQUIRE(pAdded != nullptr);
    CHECK(pAdded->m_sourceID == pSprite->GetId());
    CHECK(strncmp(reinterpret_cast<const char*>(pAdded->field_29), "SPWI123", 8) == 0);
    delete pStats->m_cRegeneratedPersistantEffectList.RemoveTail();
}

TEST_CASE("effects-iwd: restoration (447) picks the lowering ability effects", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::unique_ptr<CGameEffect> pRestoration = Decode(pSprite, ICEWIND_CGAMEEFFECT_RESTORATION, 0, 0);
    IcewindCGameEffectRestoration* pThis = static_cast<IcewindCGameEffectRestoration*>(pRestoration.get());
    // A cumulative STR loss is picked, a gain is not, a set below the
    // current score is picked; a different opcode never.
    std::unique_ptr<CGameEffect> pStr = Decode(pSprite, CGAMEEFFECT_STR, 0, -2);
    CHECK(pThis->sub_56B340(pStr.get(), pSprite) == TRUE);
    pStr = Decode(pSprite, CGAMEEFFECT_STR, 1, 3);
    pSprite->m_tempStats.m_nSTR = 10;
    CHECK(pThis->sub_56B340(pStr.get(), pSprite) == TRUE);
    pStr = Decode(pSprite, CGAMEEFFECT_STR, 2, 50);
    CHECK(pThis->sub_56B340(pStr.get(), pSprite) == TRUE);
    std::unique_ptr<CGameEffect> pOther = Decode(pSprite, CGAMEEFFECT_HASTE, 0, -2);
    CHECK(pThis->sub_56B340(pOther.get(), pSprite) == FALSE);
    CHECK(pRestoration->ApplyEffect(pSprite) == TRUE);
    CHECK(pRestoration->m_done == TRUE);
}

TEST_CASE("effects-iwd: set global (296) sets RETURN_TO_LONELYWOOD and broadcasts it", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_SETGLOBAL, 0, 7);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("RETURN_TO_LONELYWOOD"));
    REQUIRE(pVariable != NULL);
    CHECK(pVariable->m_intValue == 7);
    REQUIRE(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetCount() == 1);
    CHECK(dynamic_cast<CMessageSetVariable*>(g_pBaldurChitin->m_cMessageHandler.m_messageList.GetHead()) != nullptr);
    ClearMessages();
}

TEST_CASE("effects-iwd: mace of disruption (284) and rod of smiting (291) spare a human", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    // The party member is no undead, tanar'ri or golem: nothing is queued.
    for (int n = 0; n < 20; n++) {
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_MACEOFDISRUPTION, 0, 0);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_RODOFSMITING, 0, 0);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
    }
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());
}

TEST_CASE("effects-iwd: shroud of flame (278) sets its state and adds its burning once", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    pStats->m_spellStates.reset(SPLSTATE_SHROUD_OF_FLAME);
    INT_PTR before = pStats->m_cRegeneratedPersistantEffectList.GetCount();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_SHROUDOFFLAME, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_spellStates.test(SPLSTATE_SHROUD_OF_FLAME));
    CHECK(pStats->m_visualEffects.test(IWD_VFX_SHROUD_OF_FLAME));
    CHECK(HasPortraitIcon(pSprite, 98));
    REQUIRE(pStats->m_cRegeneratedPersistantEffectList.GetCount() == before + 1);
    CHECK(dynamic_cast<CPersistantEffect84C4D4*>(pStats->m_cRegeneratedPersistantEffectList.GetTail()) != nullptr);
    // Already burning: only done.
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(pStats->m_cRegeneratedPersistantEffectList.GetCount() == before + 1);
    delete pStats->m_cRegeneratedPersistantEffectList.RemoveTail();
    pStats->m_spellStates.reset(SPLSTATE_SHROUD_OF_FLAME);
    pStats->m_visualEffects.reset(IWD_VFX_SHROUD_OF_FLAME);
}

TEST_CASE("effects-iwd: animal rage (279) type 1 sets the rage state", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    pSprite->GetDerivedStats()->m_spellStates.reset(SPLSTATE_ANIMAL_RAGE);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_ANIMALRAGE, 1, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_spellStates.test(SPLSTATE_ANIMAL_RAGE));
    pSprite->GetDerivedStats()->m_spellStates.reset(SPLSTATE_ANIMAL_RAGE);
    // Type 0 looks for a nearby enemy; the start area has none in range of
    // the party member, so it only marks the sprite.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_ANIMALRAGE, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->field_562C == 1);
}

TEST_CASE("effects-iwd: aegis (426) raises the resistances and saves once", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    pStats->m_spellStates.reset(SPLSTATE_AEGIS);
    const SHORT fire = pStats->m_nResistFire;
    const SHORT magic = pStats->m_nResistMagic;
    const SHORT will = pStats->m_nSaveVSWill;
    const SHORT slashing = pStats->m_nResistSlashing;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_AEGIS, 0, 0);
    pEffect->m_secondaryType = 1;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_nResistFire == fire + 15);
    CHECK(pStats->m_nResistMagic == magic + 3);
    CHECK(pStats->m_nSaveVSWill == will + 2);
    CHECK(pStats->m_nResistSlashing == slashing + 10);
    CHECK(pStats->m_nStoneSkins >= 8);
    CHECK(pStats->m_naturalImmunities.count(218) == 1);
    CHECK(pStats->m_naturalImmunities.count(176) == 1);
    CHECK(HasPortraitIcon(pSprite, 119));
    // A second application changes nothing.
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_nResistFire == fire + 15);
    pStats->m_spellStates.reset(SPLSTATE_AEGIS);
}

TEST_CASE("effects-iwd: fire shield (406) red and blue set their states", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    pStats->m_spellStates.reset(SPLSTATE_RED_FIRESHIELD);
    pStats->m_spellStates.reset(SPLSTATE_BLUE_FIRESHIELD);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FIRESHIELD, 0, 0);
    pEffect->m_done = TRUE;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_spellStates.test(SPLSTATE_RED_FIRESHIELD));
    CHECK(pStats->m_visualEffects.test(IWD_VFX_FIRE_SHIELD_RED));
    CHECK(HasPortraitIcon(pSprite, 121));
    CHECK(pEffect->m_done == FALSE);
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FIRESHIELD, 1, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_spellStates.test(SPLSTATE_BLUE_FIRESHIELD));
    CHECK(pStats->m_visualEffects.test(IWD_VFX_FIRE_SHIELD_BLUE));
    CHECK(HasPortraitIcon(pSprite, 122));
    pStats->m_spellStates.reset(SPLSTATE_RED_FIRESHIELD);
    pStats->m_spellStates.reset(SPLSTATE_BLUE_FIRESHIELD);

    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_WHENSTRUCK, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);

    // Each of the three adds a when-struck entry (stats-spell fix round 1).
    std::list<CWhenStruckEntry>& list = pStats->m_whenStruckList;
    list.clear();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FIRESHIELD, 0, 0);
    pEffect->m_res = "SPWI403";
    pEffect->m_sourceRes = "SPWI404";
    pEffect->m_casterLevel = 0x30A;
    pEffect->m_target = CPoint(5, 6);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(list.size() == 1);
    CHECK(list.back().m_res == CResRef("SPWI403"));
    CHECK(list.back().m_sourceRes == CResRef("SPWI404"));
    CHECK(list.back().m_casterLevel == 0x0A);
    CHECK(list.back().m_target == CPoint(5, 6));
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FIRESHIELD, 1, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(list.size() == 2);
    // Already shielded: nothing more.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FIRESHIELD, 1, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(list.size() == 2);
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_WHENSTRUCK, 0, 0);
    pEffect->m_res = "SPWI999";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(list.size() == 3);
    CHECK(list.back().m_res == CResRef("SPWI999"));
    pStats->m_spellStates.reset(SPLSTATE_RED_FIRESHIELD);
    pStats->m_spellStates.reset(SPLSTATE_BLUE_FIRESHIELD);
    list.clear();
}

TEST_CASE("effects-iwd: remove effect of resource (254) removes the timed effects of that source", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CGameEffectList* pList = pSprite->GetTimedEffectList();
    // Two effects from SPWI999 (one in other case) and one from SPWI998, on
    // the timed list directly.
    std::unique_ptr<CGameEffect> a = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 1, 0);
    std::unique_ptr<CGameEffect> b = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 1, 0);
    std::unique_ptr<CGameEffect> c = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 1, 0);
    a->m_sourceRes = "SPWI999";
    b->m_sourceRes = "spwi999";
    c->m_sourceRes = "SPWI998";
    CGameEffect* pKept = c.get();
    pList->AddTail(a.release());
    pList->AddTail(b.release());
    pList->AddTail(c.release());
    const INT_PTR before = pList->GetCount();

    // Type 1: the equipped list only; the timed ones stay.
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_REMOVEEFFECTOFRESOURCE, 1, 0);
    pEffect->m_res = "SpWi999";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(pList->GetCount() == before);

    // Type 2: the timed list.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_REMOVEEFFECTOFRESOURCE, 2, 0);
    pEffect->m_res = "SpWi999";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pList->GetCount() == before - 2);
    POSITION pos = pList->Find(pKept);
    REQUIRE(pos != NULL);
    pList->RemoveAt(pos);
    std::unique_ptr<CGameEffect> kept(pKept);
}

TEST_CASE("effects-iwd: immunity to resource (258) type 1 checks and clears the variable", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CVariable var;
    var.SetName(CString("IWD2RTST"));
    var.m_intValue = 0;
    g_pBaldurChitin->GetObjectGame()->m_variables.AddKey(var);
    CVariable* pVariable = g_pBaldurChitin->GetObjectGame()->GetVariables()->FindKey(CString("IWD2RTST"));
    REQUIRE(pVariable != NULL);

    // Variable 0: done at once.
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_IMMUNITYTORESOURCE, 1, 0);
    pEffect->m_res = "IWD2RTST";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);

    // Variable set: cleared, and the effect stays.
    pVariable->m_intValue = 3;
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_IMMUNITYTORESOURCE, 1, 0);
    pEffect->m_res = "IWD2RTST";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == FALSE);
    CHECK(pVariable->m_intValue == 0);

    // Type 0 without a resref: done.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_IMMUNITYTORESOURCE, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);

    // Type 0 with a resref: added to the resource immunities.
    pSprite->GetDerivedStats()->m_resourceImmunities.clear();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_IMMUNITYTORESOURCE, 0, 0);
    pEffect->m_res = "SPWI112";
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == FALSE);
    CHECK(pSprite->GetDerivedStats()->m_resourceImmunities.count("SPWI112") == 1);
    pSprite->GetDerivedStats()->m_resourceImmunities.clear();
}

TEST_CASE("effects-iwd: damage reduction (436) and protection from arrows (443) apply", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_DAMAGEREDUCTION, 2, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_PROTECTIONFORMARROWS, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);

    CDamageReductionList& list = pSprite->GetDerivedStats()->m_damageReductions;
    list.clear();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_DAMAGEREDUCTION, 2, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(list.size() == 1);
    CHECK(list.back().m_nAmount == 10);
    CHECK(list.back().m_nEnchantment == 2);
    CHECK(list.back().m_damageTypes.size() == 6);
    CHECK(list.back().IsGeneric() == TRUE);
    // Type 6 becomes 3 first: 15/+3.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_DAMAGEREDUCTION, 6, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_dwFlags == 3);
    CHECK(list.back().m_nAmount == 15);
    CHECK(list.back().m_nEnchantment == 3);
    // An amount: the DEVIATION's constant (ruling M3-8).
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_DAMAGEREDUCTION, 4, 7);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(list.back().m_nAmount == 0x7FFFFFFF);
    CHECK(list.back().m_nEnchantment == 4);
    // Protection from arrows: 10/+flags against two types.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_PROTECTIONFORMARROWS, 5, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(list.size() == 4);
    CHECK(list.back().m_nAmount == 10);
    CHECK(list.back().m_nEnchantment == 5);
    CHECK(list.back().m_damageTypes == std::set<INT> { 0x2000000, 0x4000000 });
    CHECK(list.back().IsGeneric() == FALSE);
    list.clear();
}

TEST_CASE("effects-iwd: the aura and storm effects re-queue themselves and fire their area", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CGameArea* pArea = pSprite->GetArea();
    pArea->SortLists();
    CGameEffectList* pList = pSprite->GetTimedEffectList();
    auto delayed = [&](WORD id) {
        int n = 0;
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pOther = pList->GetNext(pos);
            if (pOther->m_effectID == id && pOther->m_durationType == 7) {
                n++;
            }
        }
        return n;
    };

    // Salamander aura: one copy seven ticks later, and a type 100 area with
    // an immunity and a damage carrying the source resref; type 1 also
    // gives them the caster level.
    for (DWORD type = 0; type <= 1; type++) {
        const int before = delayed(ICEWIND_CGAMEEFFECT_SALAMANDERAURA);
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_SALAMANDERAURA, type, 0);
        pEffect->m_sourceRes = CResRef("SPIN150");
        pEffect->m_casterLevel = 7;
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CHECK(delayed(ICEWIND_CGAMEEFFECT_SALAMANDERAURA) == before + 1);
        CProjectileArea* pAura = AddedOne<CProjectileArea>(pArea);
        REQUIRE(pAura != nullptr);
        CHECK(pAura->m_nRadius == 100);
        CHECK(pAura->m_sourceId == pSprite->GetId());
        CHECK(pAura->m_targetId == pSprite->GetId());
        std::vector<CGameEffect*> carried = Carried(pAura);
        REQUIRE(carried.size() == 2);
        for (CGameEffect* pCarried : carried) {
            CHECK(pCarried->m_sourceRes == CResRef("SPIN150"));
            CHECK(pCarried->m_casterLevel == (type == 1 ? 7u : 0u));
        }
        CHECK(carried[0]->m_dwFlags == (type == 1 ? 25u : 3u));
        RemoveAdded(pArea);
    }

    // Umber hulk gaze: a type 275 area aimed at enemies with four
    // immunities, a confusion and a last immunity, all from SpIn994.
    {
        const int before = delayed(ICEWIND_CGAMEEFFECT_UMBERHULKGAZE);
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_UMBERHULKGAZE, 0, 5);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CHECK(delayed(ICEWIND_CGAMEEFFECT_UMBERHULKGAZE) == before + 1);
        CProjectileArea* pGaze = AddedOne<CProjectileArea>(pArea);
        REQUIRE(pGaze != nullptr);
        CHECK(pGaze->m_nRadius == 275);
        CHECK(pGaze->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ENEMY);
        std::vector<CGameEffect*> carried = Carried(pGaze);
        REQUIRE(carried.size() == 6);
        const DWORD flags[] = { 17, 27, 29, 23 };
        for (int n = 0; n < 4; n++) {
            CHECK(carried[n]->m_dwFlags == flags[n]);
            CHECK(carried[n]->m_duration == 10);
        }
        CHECK(carried[4]->m_duration == 5);
        CHECK(carried[5]->m_dwFlags == 0);
        CHECK(carried[5]->m_duration == 5);
        for (CGameEffect* pCarried : carried) {
            CHECK(pCarried->m_sourceRes == CResRef("SpIn994"));
        }
        RemoveAdded(pArea);
    }

    // Static charge, call lightning, cloak of fear: one copy per further
    // round; an unknown source then only finishes.
    const WORD storms[] = { ICEWIND_CGAMEEFFECT_STATICCHARGE, ICEWIND_CGAMEEFFECT_CALLLIGHTNING, ICEWIND_CGAMEEFFECT_CLOAKOFFEAR };
    for (WORD id : storms) {
        const int before = delayed(id);
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, id, 0, 3);
        pEffect->m_sourceID = -1;
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CHECK(delayed(id) == before + 2);
        CHECK(Added(pArea).empty());
    }

    // With the source: the storm's area carries the spell's effects with the
    // source resref. Static charge and call lightning aim at enemies and
    // take the caster level; cloak of fear has radius 75 and takes level 1.
    CSpell spell(CResRef("SPWI112"));
    spell.Demand();
    REQUIRE(spell.GetAbilityCount() > 0);
    for (WORD id : storms) {
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, id, 0, 0);
        pEffect->m_res = CResRef("SPWI112");
        pEffect->m_sourceRes = CResRef("SPIN200");
        pEffect->m_casterLevel = 20;
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        std::vector<CGameObject*> added = Added(pArea);
        REQUIRE(added.size() == 1);
        CProjectileArea* pStorm = dynamic_cast<CProjectileArea*>(added.front());
        REQUIRE(pStorm != nullptr);
        if (id == ICEWIND_CGAMEEFFECT_STATICCHARGE) {
            CHECK(dynamic_cast<CProjectileArea850078*>(pStorm) != nullptr);
            CHECK(pStorm->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ENEMY);
        } else if (id == ICEWIND_CGAMEEFFECT_CALLLIGHTNING) {
            CHECK(dynamic_cast<CProjectileArea850114*>(pStorm) != nullptr);
            CHECK(pStorm->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ENEMY);
        } else {
            CHECK(dynamic_cast<CProjectileArea8504BC*>(pStorm) != nullptr);
            CHECK(pStorm->m_nRadius == 75);
        }
        std::vector<CGameEffect*> carried = Carried(pStorm);
        // The last ability at the level (20 or 1).
        SHORT nLevel = id == ICEWIND_CGAMEEFFECT_CLOAKOFFEAR ? 1 : 20;
        SHORT nAbility = 0;
        while (nAbility < spell.GetAbilityCount() && spell.GetAbility(nAbility)->minCasterLevel <= nLevel) {
            nAbility++;
        }
        nAbility--;
        const size_t expected = nAbility >= 0 ? static_cast<size_t>(spell.GetAbility(nAbility)->effectCount) : 0;
        CHECK(carried.size() == expected);
        for (CGameEffect* pCarried : carried) {
            CHECK(pCarried->m_sourceRes == CResRef("SPIN200"));
        }
        RemoveAdded(pArea);
    }
    spell.Release();
    ClearMessages();
}

TEST_CASE("effects-iwd: gaze, wail and projectile effects fire their projectiles; area of effect 1 adds its persistant effect", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CGameArea* pArea = pSprite->GetArea();
    pArea->SortLists();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;

    {
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_BEHOLDERDISPELMAGIC, 0, 0);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        RemoveAdded(pArea);
        ClearMessages();
    }

    // Harpy wail: a type 300 area with its sound, SpIn166's effects with
    // that source resref and the caster level.
    {
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_HARPYWAIL, 0, 0);
        pEffect->m_casterLevel = 9;
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CProjectileArea* pWail = AddedOne<CProjectileArea>(pArea);
        REQUIRE(pWail != nullptr);
        CHECK(pWail->m_nRadius == 300);
        CHECK(pWail->m_explosion1.m_sound == "EFF_P111");
        std::vector<CGameEffect*> carried = Carried(pWail);
        CHECK(!carried.empty());
        for (CGameEffect* pCarried : carried) {
            CHECK(pCarried->m_sourceRes == CResRef("SpIn166"));
            CHECK(pCarried->m_casterLevel == 9);
        }
        RemoveAdded(pArea);
    }

    // Jackalwere gaze: the same with SpIn179, without the caster level.
    {
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_JACKALWEREGAZE, 0, 0);
        pEffect->m_casterLevel = 9;
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CProjectileArea* pGaze = AddedOne<CProjectileArea>(pArea);
        REQUIRE(pGaze != nullptr);
        CHECK(pGaze->m_nRadius == 300);
        std::vector<CGameEffect*> carried = Carried(pGaze);
        CHECK(!carried.empty());
        for (CGameEffect* pCarried : carried) {
            CHECK(pCarried->m_sourceRes == CResRef("SpIn179"));
            CHECK(pCarried->m_casterLevel != 9);
        }
        RemoveAdded(pArea);
    }

    // Projectile 430, type 0: projectile 1, the instant projectile, which
    // delivers SPWI112's level 1 effects to the sprite as one group at once.
    {
        ClearMessages();
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_PROJECTILE, 0, 0);
        pEffect->m_res = CResRef("SPWI112");
        pEffect->m_casterLevel = 4;
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CHECK(Added(pArea).empty());
        REQUIRE(!handler.m_messageList.IsEmpty());
        CMessageGroup* pGroup = dynamic_cast<CMessageGroup*>(handler.m_messageList.GetTail());
        REQUIRE(pGroup != nullptr);
        CHECK(pGroup->GetTarget() == pSprite->GetId());
        REQUIRE(!pGroup->m_messages.empty());
        for (CMessage* pMessage : pGroup->m_messages) {
            CMessageAddEffect* pAdd = dynamic_cast<CMessageAddEffect*>(pMessage);
            REQUIRE(pAdd != nullptr);
            CHECK(pAdd->m_effect->m_sourceRes == CResRef("SPWI112"));
            CHECK(pAdd->m_effect->m_casterLevel == 4);
            CHECK(pAdd->m_effect->m_projectileType == 0);
        }
        ClearMessages();
    }

    CDerivedStats* pStats = pSprite->GetDerivedStats();
    INT_PTR before = pStats->m_cRegeneratedPersistantEffectList.GetCount();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_AREAOFEFFECT, 3, 10);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    REQUIRE(pStats->m_cRegeneratedPersistantEffectList.GetCount() == before + 1);
    CPersistantEffect84C4B4* pAdded = dynamic_cast<CPersistantEffect84C4B4*>(pStats->m_cRegeneratedPersistantEffectList.GetTail());
    REQUIRE(pAdded != nullptr);
    CHECK(pAdded->field_58.count(reinterpret_cast<void*>(static_cast<INT_PTR>(pSprite->GetId()))) == 1);
    delete pStats->m_cRegeneratedPersistantEffectList.RemoveTail();

    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_AREAOFEFFECT, 0, 10);
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(Added(pArea).empty());

    // Types 0 and 2 with the source: an area of radius m_effectAmount from
    // the source, carrying the spell's effects with m_res as source resref;
    // type 2 leaves the sprite out.
    for (DWORD type = 0; type <= 2; type += 2) {
        pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_AREAOFEFFECT, type, 120);
        pEffect->m_res = CResRef("SPWI112");
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CProjectileArea* pAoE = AddedOne<CProjectileArea>(pArea);
        REQUIRE(pAoE != nullptr);
        CHECK(pAoE->m_nRadius == 120);
        CHECK(pAoE->m_sourceId == pSprite->GetId());
        CHECK(pAoE->m_excludedTargets.count(pSprite->GetId()) == (type == 2 ? 1u : 0u));
        for (CGameEffect* pCarried : Carried(pAoE)) {
            CHECK(pCarried->m_sourceRes == CResRef("SPWI112"));
        }
        RemoveAdded(pArea);
    }
    ClearMessages();
}

TEST_CASE("effects-iwd: turn undead (280) without a turner and type 1", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    // The turner's id travels in the dice count; an unknown one returns
    // FALSE at once.
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_TURNUNDEAD, 3, 0);
    pEffect->m_numDice = static_cast<DWORD>(-1);
    CHECK(pEffect->ApplyEffect(pSprite) == FALSE);
    CHECK(pEffect->m_done == FALSE);

    // Type 1 (rebuked) from the sprite itself: turned, state 179 and two
    // points off derived field_C.
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    pStats->m_spellStates.reset(SPLSTATE_TURNED);
    pStats->m_spellStates.reset(SPLSTATE_179);
    const short before = pStats->field_C;
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_TURNUNDEAD, 1, 0);
    pEffect->m_numDice = static_cast<DWORD>(pSprite->GetId());
    pEffect->m_secondaryType = 0;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pStats->m_spellStates.test(SPLSTATE_TURNED));
    CHECK(pStats->m_spellStates.test(SPLSTATE_179));
    CHECK(pStats->field_C == before - 2);
    pStats->field_C = before;
    pStats->m_spellStates.reset(SPLSTATE_TURNED);
    pStats->m_spellStates.reset(SPLSTATE_179);
}

TEST_CASE("effects-iwd: turn undead (280) type 0 commands with a dominate effect", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_TURNUNDEAD, 0, 0);
    pEffect->m_numDice = static_cast<DWORD>(pSprite->GetId());
    pEffect->m_secondaryType = 1;
    pEffect->m_duration = 1234;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == FALSE);
    // The string (41023) goes by its own message; find the effect's.
    CMessageAddEffect* message = nullptr;
    POSITION pos = handler.m_messageList.GetHeadPosition();
    while (pos != NULL) {
        CMessage* pMessage = handler.m_messageList.GetNext(pos);
        if (dynamic_cast<CMessageAddEffect*>(pMessage) != nullptr) {
            CHECK(message == nullptr);
            message = static_cast<CMessageAddEffect*>(pMessage);
        }
    }
    REQUIRE(message != nullptr);
    REQUIRE(message->m_effect != NULL);
    CHECK(message->m_effect->m_effectID == ICEWIND_CGAMEEFFECT_DOMINATE);
    CHECK(dynamic_cast<IcewindCGameEffectDominate*>(message->m_effect) != nullptr);
    CHECK(message->m_effect->m_dwFlags == 4);
    CHECK(message->m_effect->m_duration == 1234);
    CHECK(message->m_effect->m_sourceID == pSprite->GetId());
    CHECK(message->GetTarget() == pSprite->GetId());
    ClearMessages();
    pSprite->GetDerivedStats()->m_spellStates.reset(SPLSTATE_TURNED);
}

TEST_CASE("effects-iwd: turn undead (280) type 2 destroys with a CGameEffectDeath", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_TURNUNDEAD, 2, 0);
    pEffect->m_numDice = static_cast<DWORD>(pSprite->GetId());
    pEffect->m_secondaryType = 0;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageAddEffect* message = dynamic_cast<CMessageAddEffect*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CGameEffectDeath* pDeath = dynamic_cast<CGameEffectDeath*>(message->m_effect);
    REQUIRE(pDeath != nullptr);
    CHECK(pDeath->m_effectID == CGAMEEFFECT_DEATH);
    CHECK(pDeath->m_effectAmount == 0);
    CHECK(pDeath->m_dwFlags == 8);
    CHECK(pDeath->m_sourceID == pSprite->GetId());
    CHECK(pDeath->field_18C == 0);
    CHECK(pDeath->m_probabilityUpper == 100);
    CHECK(pDeath->m_probabilityLower == 0);
    CHECK(pDeath->m_secondaryType == 1);
    CHECK(message->GetTarget() == pSprite->GetId());
    ClearMessages();
    pSprite->GetDerivedStats()->m_spellStates.reset(SPLSTATE_TURNED);
}

TEST_CASE("effects-iwd: ranger tracking (286) shows the tracking string under the sprite's name", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CUIControlTextDisplay* pDisplay = g_pBaldurChitin->m_pEngineWorld->m_pActiveDialogDisplay;
    REQUIRE(pDisplay != NULL);
    REQUIRE(pDisplay->m_plstStrings != NULL);
    const INT_PTR before = pDisplay->m_plstStrings->GetCount();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_RANGERTRACKING, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(pDisplay->m_plstStrings->GetCount() > before);
    // The stubbed lookup (0x5466F0) gives the fallback 19534.
    STR_RES strRes;
    g_pBaldurChitin->m_cTlkTable.Fetch(19534, strRes);
    bool found = false;
    POSITION pos = pDisplay->m_plstStrings->GetHeadPosition();
    while (pos != NULL) {
        CDisplayString* pString = pDisplay->m_plstStrings->GetNext(pos);
        if (!strRes.szText.IsEmpty() && pString->GetString().Find(strRes.szText) != -1) {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("effects-iwd: casting glow (235) queues its message and fires the school's glow", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CGameArea* pArea = pSprite->GetArea();
    const WORD types[] = { 0, 113, 115, 117, 112, 114, 116, 110, 111 };
    for (DWORD school = 0; school <= 9; school++) {
        ClearMessages();
        pArea->SortLists();
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_CASTINGGLOW, school, 0);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pEffect->m_done == TRUE);
        CHECK(pEffect->m_dwFlags == school);
        CTypedPtrList<CPtrList, CMessage*>& messages = g_pBaldurChitin->m_cMessageHandler.m_messageList;
        if (school < 1 || school > 8) {
            // Another type is bounded: nothing queued, nothing fired.
            CHECK(messages.IsEmpty());
            CHECK(Added(pArea).empty());
            continue;
        }
        REQUIRE(!messages.IsEmpty());
        CMessageFireProjectile* message = dynamic_cast<CMessageFireProjectile*>(messages.GetHead());
        REQUIRE(message != nullptr);
        CHECK(message->m_projectileType == types[school]);
        CHECK(message->m_projectileTargetId == pSprite->GetId());
        CHECK(message->GetSource() == pSprite->GetId());
        CHECK(message->GetTarget() == pSprite->GetId());
        CProjectile850F9C* pGlow = AddedOne<CProjectile850F9C>(pArea);
        REQUIRE(pGlow != nullptr);
        CHECK(pGlow->m_targetId == pSprite->GetId());
        CHECK(pGlow->m_bHasHeight == TRUE);
        RemoveAdded(pArea);
    }
    ClearMessages();
}
