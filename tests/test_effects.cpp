#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameEffect.h"
#include "CGameEffectList.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCreateChar.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// The effects' ApplyEffect on a sprite made from CHARBASE.CRE, the way
// tests/test_cre.cpp makes one: the engine in this process up to the
// Character Generation screen, built once and leaked.

namespace {

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
    if (install == nullptr || *install == '\0') {
        startError = "IWD2_DATA unset";
        return false;
    }
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
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

std::vector<BYTE> LoadCharBase()
{
    CCreatureFile file;
    file.SetResRef(CResRef("CHARBASE"), TRUE, TRUE);
    BYTE* data = file.GetData();
    std::vector<BYTE> bytes;
    if (data != NULL) {
        bytes.assign(data, data + file.GetDataSize());
    }
    file.Release();
    return bytes;
}

// A fresh sprite from CHARBASE.CRE (StartCreateChar's arguments, 0x610540).
CGameSprite* MakeSprite()
{
    static std::vector<BYTE> bytes = LoadCharBase();
    std::vector<BYTE> copy = bytes;
    return new CGameSprite(copy.data(),
        static_cast<LONG>(copy.size()),
        0,
        0,
        -1,
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
}

#define REQUIRE_ENGINE()                            \
    do {                                            \
        const char* env = std::getenv("IWD2_DATA"); \
        if (env == nullptr || *env == '\0') {       \
            SKIP("[iwd2r-skip] IWD2_DATA unset");   \
        }                                           \
        std::string error;                          \
        INFO(error);                                \
        REQUIRE(StartEngine(error));                \
    } while (0)

// An effect through the original's factory (DecodeEffect 0x49E0E0).
CGameEffect* Make(WORD effectID, DWORD dwFlags, LONG amount, WORD durationType, DWORD duration = 0)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    effect.effectID = effectID;
    effect.dwFlags = dwFlags;
    effect.effectAmount = amount;
    effect.durationType = durationType;
    effect.duration = duration;
    effect.probabilityUpper = 100;
    return CGameEffect::DecodeEffect(&effect, CPoint(-1, -1), -1, CPoint(-1, -1));
}

// Applies one effect and returns its m_done.
BOOL Apply(CGameSprite* pSprite, WORD effectID, DWORD dwFlags, LONG amount, WORD durationType, BOOL secondary = FALSE)
{
    CGameEffect* pEffect = Make(effectID, dwFlags, amount, durationType);
    pEffect->m_secondaryType = secondary;
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    // NOTE: CGameEffect's destructor is not virtual (as in the original);
    // the tests leak their effects rather than delete through the base.
    return pEffect->m_done;
}

}

TEST_CASE("stat modifier effects follow the original's modes", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SECTION("number of attacks (1) clamps to 5 and keeps at least 1 for a percentage")
    {
        pSprite->m_derivedStats.m_nNumberOfAttacks = 2;
        CHECK(Apply(pSprite, CGAMEEFFECT_ATTACKNO, 0, 7, 0) == FALSE);
        CHECK(pSprite->m_derivedStats.m_nNumberOfAttacks == 5);

        pSprite->m_baseStats.m_numberOfAttacksBase = 3;
        CHECK(Apply(pSprite, CGAMEEFFECT_ATTACKNO, 0, 1, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_numberOfAttacksBase == 4);

        pSprite->m_derivedStats.m_nNumberOfAttacks = 2;
        pSprite->m_baseStats.m_numberOfAttacksBase = 1;
        Apply(pSprite, CGAMEEFFECT_ATTACKNO, 2, 50, 0);
        CHECK(pSprite->m_derivedStats.m_nNumberOfAttacks == 1);
    }

    SECTION("charisma (6) counts a temporary bonus once, by its excess")
    {
        pSprite->m_derivedStats.field_13C = 0;
        SHORT bonus = pSprite->m_bonusStats.m_nCHR;
        Apply(pSprite, CGAMEEFFECT_CHR, 0, 2, 0);
        Apply(pSprite, CGAMEEFFECT_CHR, 0, 3, 0);
        Apply(pSprite, CGAMEEFFECT_CHR, 0, 1, 0);
        CHECK(pSprite->m_bonusStats.m_nCHR == bonus + 3);
        CHECK(pSprite->m_derivedStats.field_13C == 3);

        CHECK(Apply(pSprite, CGAMEEFFECT_CHR, 1, 55, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_CHRBase == 40);
        Apply(pSprite, CGAMEEFFECT_CHR, 1, -4, 0);
        CHECK(pSprite->m_derivedStats.m_nCHR == 1);
    }

    SECTION("luck (22) clamps to -20..20; mode 1 reads the amount unsigned")
    {
        Apply(pSprite, CGAMEEFFECT_LUCK, 1, -3, 0);
        CHECK(pSprite->m_derivedStats.m_nLuck == 20);
        Apply(pSprite, CGAMEEFFECT_LUCK, 1, -30, 1);
        CHECK(pSprite->m_baseStats.m_luckBase == -20);
        SHORT bonus = pSprite->m_bonusStats.m_nLuck;
        Apply(pSprite, CGAMEEFFECT_LUCK, 0, 2, 0);
        CHECK(pSprite->m_bonusStats.m_nLuck == bonus + 2);
    }

    SECTION("base attack bonus (54) keeps 1..40")
    {
        pSprite->m_baseStats.m_attackBase = 5;
        Apply(pSprite, CGAMEEFFECT_BASEATTACKBONUS, 0, 3, 1);
        CHECK(pSprite->m_baseStats.m_attackBase == 8);
        Apply(pSprite, CGAMEEFFECT_BASEATTACKBONUS, 1, 0, 0);
        CHECK(pSprite->m_derivedStats.m_nTHAC0 == 1);
        Apply(pSprite, CGAMEEFFECT_BASEATTACKBONUS, 2, 50, 0);
        CHECK(pSprite->m_derivedStats.m_nTHAC0 == 4);
    }

    SECTION("lore (21) and alchemy (61) work on skills 8 and 0, kept in 0..100")
    {
        pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] = 10;
        Apply(pSprite, CGAMEEFFECT_LORE, 2, 50, 0);
        CHECK(pSprite->m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] == 5);
        Apply(pSprite, CGAMEEFFECT_LORE, 1, -5, 1);
        CHECK(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_KNOWLEDGE_ARCANA] == 0);

        signed char bonus = pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY];
        Apply(pSprite, CGAMEEFFECT_SKILLALCHEMY, 0, 4, 0);
        CHECK(pSprite->m_bonusStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] == bonus + 4);
        Apply(pSprite, CGAMEEFFECT_SKILLALCHEMY, 0, 120, 1);
        CHECK(pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] == 100);
    }

    delete pSprite;
}

TEST_CASE("maximum hit point effects (18)", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_baseStats.m_hitPoints = 10;
    pSprite->m_baseStats.m_maxHitPointsBase = 10;
    pSprite->m_derivedStats.m_nMaxHitPoints = 10;
    pSprite->m_tempStats.m_generalState = 0;

    SECTION("mode 0, permanent, raises current and maximum")
    {
        CHECK(Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 15);
        CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 15);
        CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    }

    SECTION("mode 0, temporary, raises current hit points only with the secondary flag")
    {
        Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 0);
        CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 15);
        CHECK(pSprite->m_baseStats.m_hitPoints == 10);
        Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 0, TRUE);
        CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    }

    SECTION("mode 2 heals the gain over the previous maximum")
    {
        pSprite->m_tempStats.m_nMaxHitPoints = 10;
        Apply(pSprite, CGAMEEFFECT_HITPOINTS, 2, 150, 0);
        CHECK(pSprite->m_derivedStats.m_nMaxHitPoints == 15);
        CHECK(pSprite->m_baseStats.m_hitPoints == 15);
    }

    SECTION("a dead sprite is left alone")
    {
        pSprite->m_baseStats.m_hitPoints = 0;
        CHECK(Apply(pSprite, CGAMEEFFECT_HITPOINTS, 0, 5, 1) == TRUE);
        CHECK(pSprite->m_baseStats.m_maxHitPointsBase == 10);
    }

    delete pSprite;
}

TEST_CASE("condition effects set the original's state bits", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    SECTION("invisibility (20)")
    {
        Apply(pSprite, CGAMEEFFECT_INVISIBLE, 0, 0, 0);
        CHECK((pSprite->m_derivedStats.m_generalState & STATE_INVISIBLE) != 0);
        CHECK(pSprite->m_derivedStats.m_visualEffects[IWD_VFX_INVISIBILITY]);
        CHECK(pSprite->m_portraitIcons.Find(reinterpret_cast<int*>(110)) != NULL);

        Apply(pSprite, CGAMEEFFECT_INVISIBLE, 1, 0, 1);
        CHECK((pSprite->m_baseStats.m_generalState & (STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY)) == (STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY));
    }

    SECTION("berserk (3) and blood rage")
    {
        Apply(pSprite, CGAMEEFFECT_BERSERK, 1, 0, 0);
        CHECK((pSprite->m_derivedStats.m_generalState & STATE_BERSERK) != 0);
        CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_BERSERK_ALWAYS_ACTIVE));

        pSprite->m_bonusStats.field_128 = 0;
        pSprite->m_derivedStats.field_128 = 0;
        SHORT str = pSprite->m_bonusStats.m_nSTR;
        Apply(pSprite, CGAMEEFFECT_BERSERK, 2, 0, 0);
        CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_BLOOD_RAGE));
        CHECK(pSprite->m_derivedStats.m_naturalImmunities.count(400) == 1);
        CHECK(pSprite->m_bonusStats.m_nSTR == str + 6);
        CHECK(pSprite->m_derivedStats.field_128 == 6);
    }

    delete pSprite;
}

TEST_CASE("regeneration (98) queues a persistent heal", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    INT count = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount();

    CGameEffect* pEffect = Make(CGAMEEFFECT_REGENERATION, 2, 3, 0, 60);
    pEffect->m_sourceRes = CResRef("SPWI999");
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);

    REQUIRE(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount() == count + 1);
    CPersistantEffect84C494* pRegen = static_cast<CPersistantEffect84C494*>(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetTail());
    CHECK(pRegen->field_34 == 3);
    CHECK(pRegen->field_24 == 15);
    CHECK(pRegen->field_1C == 60);
    CHECK(strncmp(reinterpret_cast<const char*>(pRegen->field_29), "SPWI999", 7) == 0);

    delete pSprite;
}

TEST_CASE("solid colour effects (51, 52) add one colour effect per range", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    // Range 255 walks the original's 17-entry table (0x8A8EE0, 0x8A8F28)
    // up to its -1 terminator: 17 entries, then stop.
    for (WORD effectID : { static_cast<WORD>(CGAMEEFFECT_COLORTINTSOLID), static_cast<WORD>(CGAMEEFFECT_COLORLIGHTSOLID) }) {
        INT before = pSprite->m_derivedStats.m_appliedColorEffects.GetCount();
        Apply(pSprite, effectID, 255, 0x00FF0000, 0);
        CHECK(pSprite->m_derivedStats.m_appliedColorEffects.GetCount() == before + 17);

        before = pSprite->m_derivedStats.m_appliedColorEffects.GetCount();
        Apply(pSprite, effectID, 2, 0x00FF0000, 0);
        CHECK(pSprite->m_derivedStats.m_appliedColorEffects.GetCount() == before + 1);
    }

    delete pSprite;
}

TEST_CASE("spell immunity (206, 290) stays on while it has a resource", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    CGameEffect* pEffect = Make(CGAMEEFFECT_IMMUNITYSPELL, 0, 0, 0);
    pEffect->m_res = CResRef("SPWI112");
    REQUIRE(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == FALSE);

    CGameEffect* pEmpty = Make(CGAMEEFFECT_IMMUNITYSPELL, 0, 0, 0);
    REQUIRE(pEmpty->ApplyEffect(pSprite) == TRUE);
    CHECK(pEmpty->m_done == TRUE);

    // Mode 1 ends the effect on a sprite that is not undead (Evaluate
    // 0x4BF140 holds).
    CGameEffect* pLiving = Make(CGAMEEFFECT_IMMUNITYSPELL, 1, 0, 0);
    pLiving->m_res = CResRef("SPWI112");
    REQUIRE(pLiving->ApplyEffect(pSprite) == TRUE);
    CHECK(pLiving->m_done == TRUE);

    delete pSprite;
}

TEST_CASE("damage (12) takes resistances and leaves the living awake", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    pSprite->m_baseStats.m_hitPoints = 30;
    pSprite->m_baseStats.m_maxHitPointsBase = 30;
    pSprite->m_tempStats.m_nMaxHitPoints = 30;
    pSprite->m_baseStats.m_generalState = 0;

    // Magic and poison damage start no sprite effect (blood and the
    // elemental effects need an effect palette this fixture does not set
    // up), so they carry the checks.
    SECTION("magic, mode 0, less the magic damage resistance")
    {
        pSprite->m_tempStats.m_nMagicDamageResistance = 2;
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400000, 7, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 25);
        CHECK(pSprite->field_5582 == 5);
    }

    SECTION("poison less the poison resistance; a resistance above the amount does nothing")
    {
        pSprite->m_tempStats.m_nResistPoison = 3;
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x200000, 9, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 24);
        pSprite->m_tempStats.m_nResistPoison = 20;
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x200000, 9, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 24);
    }

    SECTION("mode 3 halves, mode 2 is a percentage of the current hit points")
    {
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400003, 9, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 26);
        Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400002, 50, 0);
        CHECK(pSprite->m_baseStats.m_hitPoints == 13);
    }

    SECTION("a cut scene stops it")
    {
        g_pBaldurChitin->GetObjectGame()->m_gameSave.m_cutScene = TRUE;
        CHECK(Apply(pSprite, CGAMEEFFECT_DAMAGE, 0x400000, 7, 0) == TRUE);
        g_pBaldurChitin->GetObjectGame()->m_gameSave.m_cutScene = FALSE;
        CHECK(pSprite->m_baseStats.m_hitPoints == 30);
    }

    delete pSprite;
}

TEST_CASE("RemoveAllOfType removes the matching effects but the one to leave", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    CGameEffectList list;

    CGameEffect* pFirst = Make(CGAMEEFFECT_LUCK, 0, 1, 0);
    CGameEffect* pKeep = Make(CGAMEEFFECT_LUCK, 0, 2, 0);
    CGameEffect* pOther = Make(CGAMEEFFECT_CHR, 0, 1, 0);
    CGameEffect* pLast = Make(CGAMEEFFECT_LUCK, 0, 2, 0);
    list.AddTail(pFirst);
    POSITION posKeep = list.AddTail(pKeep);
    list.AddTail(pOther);
    list.AddTail(pLast);

    SECTION("any amount")
    {
        list.RemoveAllOfType(pSprite, CGAMEEFFECT_LUCK, posKeep, -1);
        REQUIRE(list.GetCount() == 2);
        CHECK(list.GetHead() == pKeep);
        CHECK(list.GetTail() == pOther);

        // The node before the one to leave was visited: m_posNext is the
        // node after it (0x4C097C).
        POSITION posNext = posKeep;
        list.GetNext(posNext);
        CHECK(list.m_posNext == posNext);
    }

    SECTION("a given amount")
    {
        list.RemoveAllOfType(pSprite, CGAMEEFFECT_LUCK, posKeep, 2);
        REQUIRE(list.GetCount() == 3);
        CHECK(list.GetHead() == pFirst);
        CHECK(list.GetTail() == pOther);

        POSITION posNext = posKeep;
        list.GetNext(posNext);
        CHECK(list.m_posNext == posNext);
    }

    SECTION("no node to leave")
    {
        list.RemoveAllOfType(pSprite, CGAMEEFFECT_LUCK, NULL, -1);
        REQUIRE(list.GetCount() == 1);
        CHECK(list.GetHead() == pOther);
    }

    list.RemoveAll();
    delete pSprite;
}

TEST_CASE("visible (47) ends invisibility; the folded no-op effects return 1", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    pSprite->m_tempStats.m_generalState = 0;
    pSprite->m_baseStats.m_generalState |= STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY;
    pSprite->m_derivedStats.m_generalState |= STATE_INVISIBLE;
    CHECK(Apply(pSprite, CGAMEEFFECT_VISIBLE, 0, 0, 0) == TRUE);
    CHECK((pSprite->m_baseStats.m_generalState & (STATE_INVISIBLE | STATE_IMPROVEDINVISIBILITY)) == 0);
    CHECK((pSprite->m_derivedStats.m_generalState & STATE_INVISIBLE) == 0);

    CHECK(Apply(pSprite, CGAMEEFFECT_IDENTIFY, 0, 0, 0) == TRUE);
    CHECK(Apply(pSprite, CGAMEEFFECT_BOUNCESPELL, 0, 0, 0) == FALSE);
    CHECK(Apply(pSprite, CGAMEEFFECT_LEVELDRAIN, 0, 0, 0) == FALSE);

    delete pSprite;
}

TEST_CASE("poison (25) queues a timed poison damage", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();
    INT count = pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount();

    CHECK(Apply(pSprite, CGAMEEFFECT_POISON, 2, 1, 0) == FALSE);
    CHECK((pSprite->m_derivedStats.m_generalState & STATE_POISONED) != 0);
    REQUIRE(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetCount() == count + 1);
    CPersistantEffect84C420* pPoison = static_cast<CPersistantEffect84C420*>(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetTail());
    CHECK(pPoison->field_34 == 0x200000);
    CHECK(pPoison->field_38 == 1);
    CHECK(pPoison->field_24 == 15);

    // Mode 6 also holds the sprite, then poisons as mode 4 (every 105).
    Apply(pSprite, CGAMEEFFECT_POISON, 6, 2, 0);
    CHECK(pSprite->m_derivedStats.m_spellStates.test(SPLSTATE_HELD));
    CHECK((pSprite->m_derivedStats.m_generalState & STATE_HELPLESS) != 0);
    pPoison = static_cast<CPersistantEffect84C420*>(pSprite->m_derivedStats.m_cRegeneratedPersistantEffectList.GetTail());
    CHECK(pPoison->field_24 == 105);

    delete pSprite;
}

TEST_CASE("item effects create and destroy items", "[effects][data]")
{
    REQUIRE_ENGINE();
    CGameSprite* pSprite = MakeSprite();

    // A creature (no portrait) gets the item in the first free slot from 15.
    for (int n = CGameSpriteEquipment::SLOT_MISC; n < 43; n++) {
        pSprite->m_equipment.m_items[n] = NULL;
    }
    CGameEffect* pCreate = Make(CGAMEEFFECT_CREATEITEM, 0, 1, 1);
    pCreate->m_res = CResRef("00POTN03");
    REQUIRE(pCreate->ApplyEffect(pSprite) == TRUE);
    CHECK(pCreate->m_done == TRUE);
    REQUIRE(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC] != NULL);
    CHECK(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC]->cResRef == CResRef("00POTN03"));

    CGameEffect* pDestroy = Make(CGAMEEFFECT_DESTROYITEM, 0, 0, 1);
    pDestroy->m_res = CResRef("00POTN03");
    REQUIRE(pDestroy->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->m_equipment.m_items[CGameSpriteEquipment::SLOT_MISC] == NULL);
    CHECK(pSprite->field_562C == 1);

    delete pSprite;
}
