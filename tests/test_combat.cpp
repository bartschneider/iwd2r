#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CAIAction.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CPathSearch.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CUtil.h"
#include "ScreenLayout.h"
#include "compat/msvc_rand.h"
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
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Combat on the engine in this process (milestone M3): CGameSprite::Hit
// 0x73B760, Damage 0x73D560, GetAC 0x723CC0, OneSwing 0x7446F0, Swing
// 0x739B60 and the Attack action 0x737EF0, with a party member of a new
// game in AR1000 and a goblin (10GOB.CRE) put next to it. The rand draws
// of each function are pinned against the VC6 generator. Every engine works
// on a private view of the install (tests/data_view.h).

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

// A creature of the game's files put on the area's front list at `pos`.
CGameSprite* AddCreature(CGameArea* pArea, const char* resRef, const CPoint& pos)
{
    CCreatureFile creatureData;
    creatureData.SetResRef(CResRef(resRef), TRUE, TRUE);
    if (creatureData.GetDataSize() == 0) {
        return NULL;
    }
    CGameSprite* pSprite = new CGameSprite(creatureData.GetData(),
        creatureData.GetDataSize(),
        0,
        0,
        -1,
        0,
        0,
        INT_MAX,
        pos,
        0);
    pSprite->SetResRef(CResRef(resRef));
    creatureData.ReleaseData();
    pSprite->AddToArea(pArea, pos, 0, CGameObject::LIST_FRONT);
    return pSprite;
}

// The number of VC6 rand() draws that lead from `before` to `after` (the
// generator's state), or -1 beyond `limit`.
int Draws(unsigned int before, unsigned int after, int limit = 1000)
{
    unsigned int state = before;
    for (int n = 0; n <= limit; n++) {
        if (state == after) {
            return n;
        }
        state = state * 214013u + 2531011u;
    }
    return -1;
}

// The values rand() returns from the state `state` on.
std::vector<int> NextRands(unsigned int state, int count)
{
    std::vector<int> values;
    for (int n = 0; n < count; n++) {
        state = state * 214013u + 2531011u;
        values.push_back(static_cast<int>((state >> 16) & 0x7FFF));
    }
    return values;
}

// A seed for the n-th case: consecutive small seeds start the generator
// with nearly equal first values.
unsigned int Seed(unsigned int n)
{
    return n * 0x9E3779B1u;
}

// CUtil::UtilRandInt on a given rand() value.
LONG RandInt(LONG nRange, LONG nWeight, int value)
{
    LONG nValue = nWeight + ((nRange * value) >> 15);
    if (nValue >= nRange) {
        nValue = nRange - 1;
    }
    if (nValue < 0) {
        nValue = 0;
    }
    return nValue;
}

void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// A new game with the party member and a goblin next to it, both on the
// area's front list.
struct Fight {
    CGameSprite* pCharacter = NULL;
    CGameSprite* pGoblin = NULL;
    CGameArea* pArea = NULL;
};

bool SetUpFight(Fight& fight, std::string& why)
{
    std::string error;
    if (!StartEngine(error)) {
        why = error;
        return false;
    }
    std::vector<BYTE> cre = FinishedCharacter();
    if (cre.empty()) {
        why = "no CHR V2.2 file in the Characters folder";
        return false;
    }
    fight.pCharacter = NewGameInStartArea(cre);
    fight.pArea = fight.pCharacter->GetArea();
    if (fight.pArea == NULL) {
        why = "the party member is in no area";
        return false;
    }
    fight.pArea->m_visibility.SetAreaExplored();
    const CPoint& pos = fight.pCharacter->GetPos();
    fight.pGoblin = AddCreature(fight.pArea, "10GOB", CPoint(pos.x + 2 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    if (fight.pGoblin == NULL) {
        why = "10GOB.CRE not found";
        return false;
    }
    fight.pArea->SortLists();
    ClearMessages();
    return true;
}

// One tick as the area runs it, for the fighters and the projectiles in
// flight only: the game time, each object's AIUpdate when DoAIUpdate lets
// it, then the messages. (Not the whole world screen: the area's scripts
// start AR1000's cut-scene, which switches engines and starts the sound.)
void Tick(CGameArea* pArea, std::initializer_list<CGameSprite*> fighters)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->GetWorldTimer()->UpdateTime(FALSE);
    g_pChitin->nAUCounter++;
    for (CGameSprite* pFighter : fighters) {
        if (pFighter->DoAIUpdate(pGame->GetWorldTimer()->m_active, g_pChitin->nAUCounter)) {
            pFighter->AIUpdate();
        }
    }

    std::vector<LONG> projectiles;
    CTypedPtrList<CPtrList, int*>* lists[] = { &pArea->m_lVertSort, &pArea->m_lVertSortBack, &pArea->m_lVertSortFlight };
    for (CTypedPtrList<CPtrList, int*>* list : lists) {
        POSITION pos = list->GetHeadPosition();
        while (pos != NULL) {
            LONG id = static_cast<LONG>(reinterpret_cast<INT_PTR>(list->GetNext(pos)));
            CGameObject* pObject;
            if (pGame->GetObjectArray()->GetShare(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
                if (pObject->IsProjectile()) {
                    projectiles.push_back(id);
                }
                pGame->GetObjectArray()->ReleaseShare(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
            }
        }
    }
    for (LONG id : projectiles) {
        CGameObject* pObject;
        if (pGame->GetObjectArray()->GetDeny(id, CGameObjectArray::THREAD_ASYNCH, &pObject, INFINITE) == CGameObjectArray::SUCCESS) {
            if (pObject->DoAIUpdate(pGame->GetWorldTimer()->m_active, g_pChitin->nAUCounter)) {
                pObject->AIUpdate();
            }
            pGame->GetObjectArray()->ReleaseDeny(id, CGameObjectArray::THREAD_ASYNCH, INFINITE);
        }
    }

    g_pBaldurChitin->GetMessageHandler()->AsynchronousUpdate();
}

}

TEST_CASE("combat: a party member melees a goblin in AR1000 until it dies and casts a spell", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    // The goblin is hostile to the party.
    REQUIRE(pGoblin->GetAIType().m_nEnemyAlly > CAIObjectType::EA_GOODCUTOFF);
    const SHORT goblinStart = pGoblin->GetBaseStats()->m_hitPoints;
    const SHORT characterStart = pCharacter->GetBaseStats()->m_hitPoints;
    REQUIRE(goblinStart > 0);
    INFO("goblin hit points " << goblinStart << ", character " << characterStart);

    CAIAction attack(CAIAction::ATTACK, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pGoblin->GetId()), 0, 0, 0);
    pCharacter->ClearActions(FALSE);
    pCharacter->AddAction(attack);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    int swings = 0;
    SHORT lowest = goblinStart;
    SHORT previous = goblinStart;
    bool hpRose = false;
    while (frames < 6000) {
        Tick(fight.pArea, { pCharacter, pGoblin });
        if (pCharacter->m_attackFrame == 0) {
            swings++;
        }
        SHORT hp = pGoblin->GetBaseStats()->m_hitPoints;
        if (hp > previous) {
            hpRose = true;
        }
        previous = hp;
        lowest = std::min(lowest, hp);
        frames++;
        if ((pGoblin->GetDerivedStats()->m_generalState & STATE_DEAD) != 0) {
            break;
        }
    }
    INFO("frames " << frames << ", rounds " << swings << ", goblin hit points " << pGoblin->GetBaseStats()->m_hitPoints);
    CHECK((pGoblin->GetDerivedStats()->m_generalState & STATE_DEAD) != 0);
    CHECK(lowest < goblinStart);
    CHECK_FALSE(hpRose);
    // The character keeps hit points between none and its start (the goblin
    // may hit back).
    CHECK(pCharacter->GetBaseStats()->m_hitPoints <= characterStart);
    CHECK(pCharacter->GetBaseStats()->m_hitPoints >= 0);

    // One spell: Magic Missile (SPWI112) on a second goblin a few squares
    // off, through CGameAIBase::FireSpell (0x467C50), its projectile flying
    // until it delivers the damage.
    const CPoint& pos = pCharacter->GetPos();
    CGameSprite* pTarget = AddCreature(fight.pArea, "10GOB", CPoint(pos.x - 6 * CPathSearch::GRID_SQUARE_SIZEX, pos.y));
    REQUIRE(pTarget != NULL);
    fight.pArea->SortLists();
    const SHORT targetStart = pTarget->GetBaseStats()->m_hitPoints;
    REQUIRE(targetStart > 0);
    CHECK(pCharacter->FireSpell(CResRef("SPWI112"), pTarget) == CGameAIBase::ACTION_DONE);
    int spellFrames = 0;
    while (spellFrames < 600 && pTarget->GetBaseStats()->m_hitPoints == targetStart) {
        Tick(fight.pArea, { pCharacter, pTarget });
        spellFrames++;
    }
    INFO("spell frames " << spellFrames << ", target hit points " << targetStart << " -> " << pTarget->GetBaseStats()->m_hitPoints);
    // Magic Missile does 1d4+1 at the first caster levels.
    CHECK(pTarget->GetBaseStats()->m_hitPoints < targetStart);
    CHECK(pTarget->GetBaseStats()->m_hitPoints >= targetStart - 5);
}

TEST_CASE("combat: GetAC sums the armour class modifiers and the capped dexterity bonus", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pGoblin = fight.pGoblin;
    CDerivedStats* pStats = pGoblin->GetDerivedStats();
    REQUIRE(pGoblin->GetClassLevel(CAIOBJECTTYPE_C_MONK) == 0);

    CItem* pArmor = pGoblin->m_equipment.m_items[1];
    pGoblin->m_equipment.m_items[1] = NULL;
    pStats->field_6 = 1;
    pStats->field_8 = 2;
    pStats->field_A = 3;
    pStats->field_C = 4;
    pStats->m_nDEX = 14;
    pStats->m_generalState &= ~STATE_BLIND;

    srand(7);
    const unsigned int before = iwd2r_rand_state();
    CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 + 2);
    // The touch armour class leaves out field_6.
    CHECK(pGoblin->sub_723DB0() == 2 + 3 + 4 + 10 + 2);
    CHECK(iwd2r_rand_state() == before);

    // Blind, without blind fight, the dexterity bonus is lost; a penalty
    // stays.
    pStats->m_generalState |= STATE_BLIND;
    if (!pGoblin->sub_763150(CGAMESPRITE_FEAT_BLIND_FIGHT)) {
        CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10);
        pStats->m_nDEX = 6;
        CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 - 2);
    }
    pStats->m_generalState &= ~STATE_BLIND;

    // Armour caps the dexterity bonus by its type (the goblin's own, when it
    // wears any).
    pGoblin->m_equipment.m_items[1] = pArmor;
    if (pArmor != NULL) {
        pStats->m_nDEX = 30;
        INT nCap;
        switch (pArmor->GetItemType()) {
        case 60:
            nCap = 6;
            break;
        case 61:
            nCap = 5;
            break;
        case 62:
            nCap = 2;
            break;
        case 63:
        case 64:
            nCap = 0;
            break;
        case 65:
            nCap = 1;
            break;
        case 66:
            nCap = 4;
            break;
        case 68:
            nCap = 3;
            break;
        default:
            nCap = 10;
            break;
        }
        CHECK(pGoblin->GetAC() == 1 + 2 + 3 + 4 + 10 + std::min(nCap, 10));
    }
}

TEST_CASE("combat: Hit draws the d20 and the confirmation d20 of a threat", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    // The fist: a melee weapon every character has.
    CItem* pWeapon = pCharacter->m_equipment.m_items[10];
    REQUIRE(pWeapon != NULL);
    pWeapon->Demand();
    const ITEM_ABILITY* pAbility = pWeapon->GetAbility(0);
    REQUIRE(pAbility != NULL);
    REQUIRE(pAbility->type == 1);
    const SHORT nRange = pWeapon->sub_4E9840(pAbility);
    pWeapon->Release();
    const SHORT nBonus = pCharacter->GetCriticalHitBonus();
    const SHORT nLuck = pCharacter->GetDerivedStats()->m_nLuck;
    const bool bImmune = (pGoblin->GetBaseStats()->field_2FB & 0x2) != 0;

    int threats = 0;
    int hits = 0;
    for (unsigned int seed = 1; seed <= 200; seed++) {
        srand(Seed(seed));
        const unsigned int before = iwd2r_rand_state();
        const int value = NextRands(before, 1)[0];
        const LONG nRoll = RandInt(20, nLuck, value) + 1;
        BOOL bCritical = FALSE;
        const BOOL bHit = pCharacter->Hit(pGoblin, pWeapon, 0, &bCritical, FALSE, 1);
        const int draws = Draws(before, iwd2r_rand_state());
        const bool bThreat = nRange + nBonus + nRoll > 19 && !bImmune;
        INFO("seed " << seed << ", roll " << nRoll);
        CHECK(draws == (bThreat ? 2 : 1));
        if (nRoll == 20) {
            CHECK(bHit);
        }
        if (nRoll == 1) {
            CHECK_FALSE(bHit);
        }
        if (bCritical) {
            CHECK(bThreat);
            CHECK(bHit);
        }
        threats += bThreat ? 1 : 0;
        hits += bHit ? 1 : 0;
        ClearMessages();
    }
    CHECK(threats > 0);
    CHECK(hits > 0);
    CHECK(hits < 200);
}

TEST_CASE("combat: Damage rolls each die of the weapon once and Kai maximises them", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    // No sneak attack: its dice would draw too.
    pCharacter->GetBaseStats()->m_rogueLevel = 0;

    CItem* pWeapon = pCharacter->m_equipment.m_items[10];
    REQUIRE(pWeapon != NULL);
    pWeapon->Demand();
    const ITEM_ABILITY* pAbility = pWeapon->GetAbility(0);
    REQUIRE(pAbility != NULL);
    const int nDice = pAbility->damageDiceCount;
    const int nSize = pAbility->damageDice;
    pWeapon->Release();
    const SHORT nLuck = pCharacter->GetDerivedStats()->m_nLuck;
    REQUIRE(nDice > 0);

    for (unsigned int seed = 1; seed <= 50; seed++) {
        INFO("seed " << seed);
        pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = false;
        srand(Seed(seed));
        unsigned int before = iwd2r_rand_state();
        std::vector<int> values = NextRands(before, nDice);
        CGameEffect* pRolled = pCharacter->Damage(pWeapon, NULL, 0, FALSE, pGoblin->m_typeAI,
            pGoblin->m_nDirection, pCharacter->GetDirection(pGoblin->GetPos()), pGoblin, FALSE);
        CHECK(Draws(before, iwd2r_rand_state()) == nDice);
        REQUIRE(pRolled != NULL);
        CHECK(pRolled->m_effectID == CGAMEEFFECT_DAMAGE);
        CHECK(pRolled->m_sourceID == pCharacter->GetId());
        CHECK(pRolled->m_effectAmount >= 1);
        int nRolled = 0;
        for (int value : values) {
            nRolled += RandInt(nSize, nLuck, value) + 1;
        }

        pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = true;
        srand(Seed(seed));
        before = iwd2r_rand_state();
        CGameEffect* pMaximised = pCharacter->Damage(pWeapon, NULL, 0, FALSE, pGoblin->m_typeAI,
            pGoblin->m_nDirection, pCharacter->GetDirection(pGoblin->GetPos()), pGoblin, FALSE);
        CHECK(Draws(before, iwd2r_rand_state()) == 0);
        REQUIRE(pMaximised != NULL);
        CHECK(pMaximised->m_effectAmount - pRolled->m_effectAmount == nDice * nSize - nRolled);

        // A critical multiplies the damage (the fist's multiplier).
        pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = true;
        CGameEffect* pCritical = pCharacter->Damage(pWeapon, NULL, 0, TRUE, pGoblin->m_typeAI,
            pGoblin->m_nDirection, pCharacter->GetDirection(pGoblin->GetPos()), pGoblin, FALSE);
        REQUIRE(pCritical != NULL);
        CHECK(pCritical->m_effectAmount
            == static_cast<SHORT>(pMaximised->m_effectAmount * static_cast<SHORT>(pWeapon->GetCriticalHitMultiplier())));

        delete pRolled;
        delete pMaximised;
        delete pCritical;
        ClearMessages();
    }
    pCharacter->GetDerivedStats()->m_spellStates[SPLSTATE_KAI_64] = false;
}

TEST_CASE("combat: OneSwing and Swing draw the attack speed at the start of a round", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;

    srand(11);
    unsigned int before = iwd2r_rand_state();
    int value = NextRands(before, 1)[0];
    pCharacter->m_attackFrame = 0;
    pCharacter->OneSwing();
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CHECK(pCharacter->m_speedFactor == std::max(0, std::min(10, 5 + value % 6 - 3)));

    before = iwd2r_rand_state();
    pCharacter->m_attackFrame = 3;
    pCharacter->OneSwing();
    CHECK(Draws(before, iwd2r_rand_state()) == 0);
    ClearMessages();

    // Swing at the start of a round: the speed takes one UtilRandInt and the
    // round's frame starts at 0.
    srand(12);
    before = iwd2r_rand_state();
    pCharacter->m_attackFrame = -1;
    pCharacter->m_equipment.m_selectedWeapon = 10;
    pCharacter->m_equipment.m_selectedWeaponAbility = 0;
    pCharacter->Swing(pGoblin);
    CHECK(Draws(before, iwd2r_rand_state()) == 1);
    CHECK(pCharacter->m_attackFrame == 0);
    CHECK(pCharacter->m_speedFactor >= 0);
    CHECK(pCharacter->m_speedFactor <= 10);
    ClearMessages();
}

TEST_CASE("combat: a goblin given AttackReevaluate swings at the party member", "[combat][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    Fight fight;
    std::string why;
    if (!SetUpFight(fight, why)) {
        if (why.find("CHR") != std::string::npos) {
            SKIP("[iwd2r-skip] " + why);
        }
        FAIL(why);
    }
    CGameSprite* pCharacter = fight.pCharacter;
    CGameSprite* pGoblin = fight.pGoblin;
    const SHORT characterStart = pCharacter->GetBaseStats()->m_hitPoints;

    // AttackReevaluate (134) on the party member, the target chosen again
    // after 2000 AI updates (the action's count).
    CAIAction attack(134, CAIObjectType(0, 0, 0, 0, 0, 0, 0, 0, pCharacter->GetId()), 2000, 0, 0);
    pGoblin->ClearActions(FALSE);
    pGoblin->AddAction(attack);
    pCharacter->ClearActions(FALSE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (!pGame->GetWorldTimer()->m_active) {
        pGame->GetWorldTimer()->StartTime();
    }
    int frames = 0;
    int blows = 0;
    SHORT previous = characterStart;
    bool hpRose = false;
    while (frames < 1500 && pCharacter->GetBaseStats()->m_hitPoints > 0) {
        Tick(fight.pArea, { pCharacter, pGoblin });
        if (pGoblin->m_attackFrame == 0) {
            blows++;
        }
        SHORT hp = pCharacter->GetBaseStats()->m_hitPoints;
        if (hp > previous) {
            hpRose = true;
        }
        previous = hp;
        frames++;
    }
    INFO("frames " << frames << ", rounds " << blows << ", character hit points " << characterStart << " -> "
                   << pCharacter->GetBaseStats()->m_hitPoints);
    CHECK(blows > 2);
    CHECK(pGoblin->m_curAction.m_actionID == 134);
    CHECK_FALSE(hpRose);
    CHECK(pCharacter->GetBaseStats()->m_hitPoints <= characterStart);
}
