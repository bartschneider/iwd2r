#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <new>

#include "C2DArray.h"
#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CDerivedStats.h"
#include "CInfGame.h"
#include "CRuleTables.h"
#include "msvc_rand.h"

namespace {

// Three levels of a hit point table, as the game's HP*.2DA lay them out.
const char kD10Table[] = "2DA V1.0\r\n"
                         "0\r\n"
                         "    SIDES   ROLLS   MODIFIER\r\n"
                         "1   10      1       0\r\n"
                         "2   10      1       0\r\n"
                         "3   10      1       0\r\n";
const char kD8Table[] = "2DA V1.0\r\n"
                        "0\r\n"
                        "    SIDES   ROLLS   MODIFIER\r\n"
                        "1   8       1       0\r\n"
                        "2   8       1       0\r\n"
                        "3   8       1       0\r\n";
const char kD4Table[] = "2DA V1.0\r\n"
                        "0\r\n"
                        "    SIDES   ROLLS   MODIFIER\r\n"
                        "1   4       1       0\r\n"
                        "2   4       1       0\r\n"
                        "3   0       0       2\r\n";

unsigned int Advance(unsigned int state, int draws)
{
    for (int i = 0; i < draws; ++i) {
        state = state * 214013u + 2531011u;
    }
    return state;
}

// Rule tables with only the hit point tables constructed, and a
// g_pBaldurChitin whose game holds only the options (what GetStartingHitPoints
// reads). Never freed: CRuleTables' destructor would destroy members never
// constructed.
struct HitPointRules {
    HitPointRules()
    {
        auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
        std::memset(storage, 0, sizeof(CRuleTables));
        rules = reinterpret_cast<CRuleTables*>(storage);
        new (&rules->m_tHitPointsWarrior) C2DArray();
        new (&rules->m_tHitPointsWizard) C2DArray();
        new (&rules->m_tHitPointsPriest) C2DArray();
        new (&rules->m_tHitPointsRogue) C2DArray();
        new (&rules->m_tHitPointsMonk) C2DArray();
        rules->m_tHitPointsWarrior.LoadText(CString(kD10Table));
        rules->m_tHitPointsWizard.LoadText(CString(kD4Table));
        rules->m_tHitPointsPriest.LoadText(CString(kD8Table));
        rules->m_tHitPointsRogue.LoadText(CString(kD8Table));
        rules->m_tHitPointsMonk.LoadText(CString(kD8Table));

        auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CBaldurChitin)));
        std::memset(chitin, 0, sizeof(CBaldurChitin));
        auto* game = static_cast<unsigned char*>(::operator new(sizeof(CInfGame)));
        std::memset(game, 0, sizeof(CInfGame));
        reinterpret_cast<CBaldurChitin*>(chitin)->m_pObjectGame = reinterpret_cast<CInfGame*>(game);
        g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(chitin);
    }

    ~HitPointRules() { g_pBaldurChitin = nullptr; }

    void SetMaxHp(BOOL bMaxHp) { g_pBaldurChitin->m_pObjectGame->m_cOptions.m_bMaxHP = bMaxHp; }

    CRuleTables* rules = nullptr;
};

// RollHitPoints reads no member: zeroed storage stands for the rule tables.
const CRuleTables* NoTables()
{
    static unsigned char storage[sizeof(CRuleTables)] = {};
    return reinterpret_cast<const CRuleTables*>(storage);
}

} // namespace

TEST_CASE("RollHitPoints below level 2 is sides times rolls and draws nothing", "[rules]")
{
    const CRuleTables* rules = NoTables();
    iwd2r_srand(7);
    REQUIRE(rules->RollHitPoints(10, 3, 1, 1, 5) == 30);
    REQUIRE(rules->RollHitPoints(10, 0, 0, 1, 5) == 5);
    REQUIRE(iwd2r_rand_state() == 7);
}

TEST_CASE("RollHitPoints draws three numbers per die, three more unless the roll is below the minimum", "[rules]")
{
    const CRuleTables* rules = NoTables();
    // A minimum above every roll: three draws per die, each die worth the
    // minimum.
    iwd2r_srand(7);
    REQUIRE(rules->RollHitPoints(10, 2, 5, 101, 4) == 202);
    REQUIRE(iwd2r_rand_state() == Advance(7, 6));
    // A minimum of 0: every die drawn again, six draws per die.
    iwd2r_srand(7);
    const INT nHitPoints = rules->RollHitPoints(10, 2, 5, 0, 4);
    REQUIRE(nHitPoints >= 2);
    REQUIRE(nHitPoints <= 20);
    REQUIRE(iwd2r_rand_state() == Advance(7, 12));
    // No rolls: only the modifier.
    iwd2r_srand(7);
    REQUIRE(rules->RollHitPoints(10, 0, 5, 0, 4) == 4);
    REQUIRE(iwd2r_rand_state() == 7);
}

TEST_CASE("GetStartingHitPoints sums each class's hit dice from its table", "[rules]")
{
    HitPointRules fixture;
    CAIObjectType typeAI;
    CDerivedStats oldDStats;
    CDerivedStats newDStats;
    for (int i = 0; i < CAIOBJECT_CLASS_MAX; ++i) {
        oldDStats.m_nClassLevels[i] = 0;
        newDStats.m_nClassLevels[i] = 0;
    }
    fixture.SetMaxHp(FALSE);

    // A first level fighter: one d10 at level 1, the full die.
    newDStats.m_fighterLevel = 1;
    REQUIRE(fixture.rules->GetStartingHitPoints(typeAI, oldDStats, newDStats, 10) == 10);

    // A barbarian reads the warrior table but always rolls a d12.
    newDStats.m_fighterLevel = 0;
    newDStats.m_barbarianLevel = 1;
    REQUIRE(fixture.rules->GetStartingHitPoints(typeAI, oldDStats, newDStats, 10) == 12);

    // Fighter 1 and cleric 1: d10 plus the priest table's d8.
    newDStats.m_barbarianLevel = 0;
    newDStats.m_fighterLevel = 1;
    newDStats.m_clericLevel = 1;
    REQUIRE(fixture.rules->GetStartingHitPoints(typeAI, oldDStats, newDStats, 10) == 18);

    // Maximum hit points: every level's dice at their maximum, and a level
    // with no rolls gives its modifier.
    fixture.SetMaxHp(TRUE);
    newDStats.m_fighterLevel = 0;
    newDStats.m_clericLevel = 0;
    newDStats.m_wizardLevel = 3;
    REQUIRE(fixture.rules->GetStartingHitPoints(typeAI, oldDStats, newDStats, 10) == 4 + 4 + 2);

    // Levels already held are not counted again.
    oldDStats.m_wizardLevel = 2;
    REQUIRE(fixture.rules->GetStartingHitPoints(typeAI, oldDStats, newDStats, 10) == 2);
}
