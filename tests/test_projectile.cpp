#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CProjectile.h"
#include "CProjectileArea.h"
#include "CProjectileBAM.h"
#include "CProjectileSpellHit.h"
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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The projectiles (CProjectile*.cpp) and the messages they send
// (CMessageGroup, CMessageFireProjectile) on a party member standing in
// AR1000. Every engine works on a private view of the install
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

// A message that records the order it ran in and that it was deleted.
class RecordingMessage : public CMessage {
public:
    RecordingMessage(std::vector<int>& runs, int& deleted, int tag)
        : CMessage(CGameObjectArray::INVALID_INDEX, CGameObjectArray::INVALID_INDEX)
        , m_runs(runs)
        , m_deleted(deleted)
        , m_tag(tag)
    {
    }
    ~RecordingMessage() override
    {
        m_deleted++;
    }
    void Run() override
    {
        m_runs.push_back(m_tag);
    }

private:
    std::vector<int>& m_runs;
    int& m_deleted;
    int m_tag;
};

// The CMessageGroup the handler holds as its last message, or NULL.
CMessageGroup* LastGroup()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    if (handler.m_messageList.IsEmpty()) {
        return NULL;
    }
    return dynamic_cast<CMessageGroup*>(handler.m_messageList.GetTail());
}

}

TEST_CASE("projectile: a message group runs its messages in order and deletes them", "[projectile]")
{
    std::vector<int> runs;
    int deleted = 0;
    std::list<CMessage*> messages;
    messages.push_back(new RecordingMessage(runs, deleted, 1));
    messages.push_back(new RecordingMessage(runs, deleted, 2));
    messages.push_back(new RecordingMessage(runs, deleted, 3));

    CMessageGroup* pGroup = new CMessageGroup(messages, 7, 9);
    CHECK(pGroup->GetSource() == 7);
    CHECK(pGroup->GetTarget() == 9);
    CHECK(pGroup->GetCommType() == CMessage::SEND);
    CHECK(pGroup->GetMsgType() == CBaldurMessage::MSG_TYPE_CMESSAGE);
    CHECK(pGroup->GetMsgSubType() == 105);
    REQUIRE(pGroup->m_messages.size() == 3);

    pGroup->Run();
    CHECK(runs == std::vector<int> { 1, 2, 3 });
    CHECK(deleted == 0);

    delete pGroup;
    CHECK(deleted == 3);
}

TEST_CASE("projectile: DeliverEffects sends the effects to the target as one message group", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(1, NULL, 0);
    REQUIRE(pProjectile != nullptr);
    CHECK(pProjectile->m_projectileType == 0);
    pProjectile->m_sourceId = pSprite->GetId();
    pProjectile->m_targetId = pSprite->GetId();

    std::unique_ptr<CGameEffect> pFirst = Decode(pSprite, CGAMEEFFECT_AC, 0, 0);
    std::unique_ptr<CGameEffect> pSecond = Decode(pSprite, CGAMEEFFECT_AC, 0, 0);
    REQUIRE(pFirst != nullptr);
    REQUIRE(pSecond != nullptr);
    CGameEffect* first = pFirst.release();
    CGameEffect* second = pSecond.release();
    pProjectile->AddEffect(first);
    pProjectile->AddEffect(second);

    pProjectile->DeliverEffects();
    CHECK(pProjectile->m_effectList.IsEmpty());

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->GetSource() == pSprite->GetId());
    CHECK(pGroup->GetTarget() == pSprite->GetId());
    REQUIRE(pGroup->m_messages.size() == 2);
    CMessageAddEffect* pAddFirst = dynamic_cast<CMessageAddEffect*>(pGroup->m_messages.front());
    CMessageAddEffect* pAddSecond = dynamic_cast<CMessageAddEffect*>(pGroup->m_messages.back());
    REQUIRE(pAddFirst != nullptr);
    REQUIRE(pAddSecond != nullptr);
    CHECK(pAddFirst->m_effect == first);
    CHECK(pAddSecond->m_effect == second);
    CHECK(pAddFirst->GetSource() == pSprite->GetId());
    CHECK(pAddFirst->GetTarget() == pSprite->GetId());

    ClearMessages();
    delete pProjectile;
}

TEST_CASE("projectile: DeliverEffects drops the effects of an immune target", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(1, NULL, 0);
    pProjectile->m_sourceId = pSprite->GetId();
    pProjectile->m_targetId = pSprite->GetId();

    SECTION("immune to the projectile")
    {
        pProjectile->m_projectileType = 77;
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.AddTail(reinterpret_cast<DWORD*>(77));
        pProjectile->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
        pProjectile->DeliverEffects();
        pSprite->GetDerivedStats()->m_cImmunitiesProjectile.RemoveTail();
    }
    SECTION("immune to the spell level")
    {
        pProjectile->m_nSpellLevel = 3;
        BOOL saved = pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[3];
        pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[3] = TRUE;
        pProjectile->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
        pProjectile->DeliverEffects();
        pSprite->GetDerivedStats()->m_cImmunitiesSpellLevel.m_levels[3] = saved;
    }

    CHECK(pProjectile->m_effectList.IsEmpty());
    CHECK(LastGroup() == nullptr);

    ClearMessages();
    delete pProjectile;
}

TEST_CASE("projectile: the instant projectile delivers on Fire and the handler runs the group", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeProjectile(1, NULL, 0);
    pProjectile->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
    // Fire delivers the effects and deletes the projectile.
    pProjectile->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->m_messages.size() == 1);

    handler.AsynchronousUpdate();
    CHECK(handler.m_messageList.IsEmpty());

    ClearMessages();
}

TEST_CASE("projectile: CMessageFireProjectile runs without a service provider and fires nothing", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    CMessage* message = new CMessageFireProjectile(0x1004,
        pSprite->GetId(),
        pSprite->GetPos(),
        0,
        pSprite->GetId(),
        pSprite->GetId(),
        0);
    handler.AddMessage(message, FALSE);
    REQUIRE(handler.m_messageList.GetCount() == 1);

    handler.AsynchronousUpdate();
    CHECK(handler.m_messageList.IsEmpty());

    ClearMessages();
}

TEST_CASE("projectile: the spell hit decoder builds the school's animation, sound and glow", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    INT nTimed = pSprite->m_timedEffectList.GetCount();
    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(1, pSprite, FALSE);
    CProjectileSpellHit* pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
    REQUIRE(pSpellHit != nullptr);
    CHECK(pSpellHit->m_fireSoundRef == "EFF_M02");
    CHECK(pSpellHit->m_bPickRandomSequence == TRUE);
    CHECK(pSpellHit->m_bDestroyOnEndOfSequence == TRUE);
    CHECK(pSpellHit->m_bHasHeight == FALSE);
    CHECK(pSprite->m_timedEffectList.GetCount() >= nTimed);
    delete pSpellHit;

    pProjectile = CProjectile::DecodeSpellHitProjectile(1, NULL, TRUE);
    pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
    REQUIRE(pSpellHit != nullptr);
    CHECK(pSpellHit->m_fireSoundRef == "EFF_P01");
    delete pSpellHit;

    pProjectile = CProjectile::DecodeSpellHitProjectile(74, NULL, FALSE);
    pSpellHit = dynamic_cast<CProjectileSpellHit*>(pProjectile);
    REQUIRE(pSpellHit != nullptr);
    CHECK(pSpellHit->m_bDrawBehind == TRUE);
    CHECK(pSpellHit->m_bLoopingSequence == TRUE);
    CHECK(pSpellHit->m_nLoopingSequence == 1);
    CHECK(pSpellHit->m_nLoopCount == 0xA5);
    CHECK(pSpellHit->m_bPickRandomSequence == FALSE);
    CHECK(pSpellHit->m_bDestroyOnEndOfSequence == FALSE);
    delete pSpellHit;

    pProjectile = CProjectile::DecodeSpellHitProjectile(0, NULL, FALSE);
    CHECK(dynamic_cast<CProjectile84D8B8*>(pProjectile) != nullptr);
    delete pProjectile;

    // DecodeProjectile hands a type above 0x1000 to the spell hit decoder.
    pProjectile = CProjectile::DecodeProjectile(0x1002, NULL, 0);
    CHECK(dynamic_cast<CProjectileSpellHit*>(pProjectile) != nullptr);
    CHECK(pProjectile->m_projectileType == 0x1001);
    delete pProjectile;

    ClearMessages();
}

TEST_CASE("projectile: a fired spell hit plays in the area and removes itself at the end", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectile* pProjectile = CProjectile::DecodeSpellHitProjectile(9, NULL, FALSE);
    REQUIRE(pProjectile != nullptr);
    pProjectile->m_targetId = pSprite->GetId();
    pProjectile->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);

    LONG id = pProjectile->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pProjectile->GetPos().x == pSprite->GetPos().x);
    CHECK(pProjectile->GetPos().y == pSprite->GetPos().y + 1);
    CHECK(pProjectile->GetArea() == pSprite->GetArea());

    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    bool removed = false;
    for (int frame = 0; frame < 2000 && !removed; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            removed = true;
            break;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    CHECK(removed);

    ClearMessages();
}

TEST_CASE("projectile: an area projectile explodes and sends its effects to the targets in range", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectileArea* pArea = new CProjectileArea(300);
    CHECK(pArea->m_nRadius == 300);
    CHECK(pArea->m_nSecondaryType == 78);
    CHECK(pArea->m_nDuration == 45);

    pArea->TargetEnemy(pSprite);
    CHECK(pArea->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ENEMY);
    pArea->TargetFriend(pSprite);
    CHECK(pArea->m_targetType.m_nEnemyAlly == CAIObjectType::EA_ALLY);

    pArea->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
    pArea->Fire(pSprite->GetArea(), pSprite->GetId(), pSprite->GetId(), pSprite->GetPos(), 0, 0);
    LONG id = pArea->GetId();
    REQUIRE(id != CGameObjectArray::INVALID_INDEX);
    CHECK(pArea->GetPos() == pSprite->GetPos());

    // It does not travel: Fire explodes it where it is.
    CHECK(pArea->m_bExploded == TRUE);
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    // The area puts it on its vertical list (as its AI update does between
    // the frames); the next update finds the party member in range and fires
    // the secondary projectile (type 79, an invisible CProjectileBAM) at it.
    pSprite->GetArea()->SortLists();
    pArea->AIUpdate();
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());
    CHECK(pArea->m_targetHits.count(pSprite->GetId()) == 1);
    CHECK(pArea->m_targetHits[pSprite->GetId()] == 1);

    CTypedPtrList<CPtrList, int*>& added = pSprite->GetArea()->m_lVertSortAdd;
    REQUIRE(!added.IsEmpty());
    LONG secondaryId = static_cast<LONG>(reinterpret_cast<INT_PTR>(added.GetTail()));
    CGameObjectArray* pArray = g_pBaldurChitin->GetObjectGame()->GetObjectArray();
    CGameObject* pSecondary;
    REQUIRE(pArray->GetShare(secondaryId, CGameObjectArray::THREAD_ASYNCH, &pSecondary, INFINITE) == CGameObjectArray::SUCCESS);
    pArray->ReleaseShare(secondaryId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    CProjectileBAM* pBAM = dynamic_cast<CProjectileBAM*>(pSecondary);
    REQUIRE(pBAM != nullptr);
    CHECK(pBAM->m_projectileType == 78);
    CHECK(pBAM->m_sourceId == id);
    CHECK(pBAM->m_targetId == pSprite->GetId());
    CHECK(pBAM->field_1DE == FALSE);

    // At the target, it delivers a copy of the effect and removes itself.
    pBAM->AIUpdate();
    CMessageGroup* pGroup = LastGroup();
    REQUIRE(pGroup != nullptr);
    CHECK(pGroup->GetTarget() == pSprite->GetId());
    CHECK(pGroup->GetSource() == id);
    REQUIRE(pGroup->m_messages.size() == 1);
    CMessageAddEffect* pAdd = dynamic_cast<CMessageAddEffect*>(pGroup->m_messages.front());
    REQUIRE(pAdd != nullptr);
    CHECK(pAdd->m_effect->m_effectID == CGAMEEFFECT_AC);
    CHECK(pArray->GetShare(secondaryId, CGameObjectArray::THREAD_ASYNCH, &pSecondary, INFINITE) != CGameObjectArray::SUCCESS);
    ClearMessages();
    pSprite->GetArea()->SortLists();

    // The next target search skips the sprite until it has been found ten
    // times.
    std::list<LONG> targets = pArea->GetTargets();
    CHECK(targets.empty());
    CHECK(pArea->m_targetHits[pSprite->GetId()] == 2);

    // It removes itself when its duration runs out.
    bool removed = false;
    for (int frame = 0; frame < 100 && !removed; frame++) {
        CGameObject* pObject;
        if (pArray->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) != CGameObjectArray::SUCCESS) {
            removed = true;
            break;
        }
        pArray->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        pObject->AIUpdate();
    }
    CHECK(removed);

    ClearMessages();
}

TEST_CASE("projectile: the cloak of fear area leaves its source out", "[projectile][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();

    CProjectileArea8504BC* pArea = new CProjectileArea8504BC(256);
    pArea->m_sourceId = pSprite->GetId();
    pArea->AddEffect(Decode(pSprite, CGAMEEFFECT_AC, 0, 0).release());
    std::list<LONG> targets;
    targets.push_back(pSprite->GetId());
    pArea->ApplyToTargets(targets);
    CHECK(targets.empty());
    CHECK(LastGroup() == nullptr);

    delete pArea;
    ClearMessages();
}
