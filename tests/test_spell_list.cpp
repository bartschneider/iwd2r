#include <catch2/catch_test_macros.hpp>

#include "CGameSpriteSpells.h"

// The sorcerer side of CGameSpriteSpellList (src/CGameSpriteSpells.cpp): a
// level's slots are shared, so one cast must land every entry's castable
// count on the level's new remaining count (sub_725DB0, 0x725DB0). Pure
// arithmetic over the list classes, no game data.

TEST_CASE("spell list: one cast lands every entry's castable count on the level's new remaining slots", "[spell]")
{
    CGameSpriteGroupedSpellList sorcerer;

    // Three spells of the first level, the spell ids of the showcase's
    // consume lines: two with four slots memorized, one with two
    // (Add(nID, level, memorized, castable, 0); castable 0, as the
    // spellbook leaves a freshly memorized spell until the party rests).
    REQUIRE(sorcerer.Add(215, 0, 4, 0, 0));
    REQUIRE(sorcerer.Add(149, 0, 2, 0, 0));
    REQUIRE(sorcerer.Add(178, 0, 4, 0, 0));

    // A rested level: four slots, every entry castable up to what it has
    // memorized (the add-side twin sub_725D30 through its grouped wrapper).
    CGameSpriteSpellList* pLevel = sorcerer.GetSpellsAtLevel(0);
    pLevel->field_14 = 4;
    sorcerer.sub_726150(0, 4, FALSE);

    REQUIRE(pLevel->field_18 == 4);
    REQUIRE(pLevel->m_List.size() == 3);
    REQUIRE(pLevel->m_List[0].field_8 == 4);
    REQUIRE(pLevel->m_List[1].field_8 == 2);
    REQUIRE(pLevel->m_List[2].field_8 == 4);
    REQUIRE(pLevel->field_10 == 10); // 4 + 2 + 4 castable across the entries

    // One cast, the way CGameSprite::Spell spends a sorcerer slot
    // (src/CGameSprite.cpp:21039): sub_7261A0(level - 1, 1, FALSE) ->
    // sub_725DB0(1, FALSE). v1 = field_18 - 1 = 3, and the count removed
    // per entry is field_8 - v1: 1 for the full entries and 2 - 3 for the
    // middle one, which wraps unsigned and still lands on 3 (the
    // "a2=-3(wrap) f8=4->7" case of the post-fix run's consume lines).
    //
    // Under the old flipped operands (v1 - field_8, inherited from the
    // fork and fixed in 6dc52e69) the full entries would take 3 - 4,
    // wrap, and land on 5, the middle one would take 1 and land on 1, and
    // field_10 on 11 - every CHECK below fails. That is why the guard
    // exists: the flip survived a tree-vs-tree compare once
    // (docs/evidence/725DB0.md).
    sorcerer.sub_7261A0(0, 1, FALSE);

    CHECK(pLevel->field_18 == 3);
    for (const CGameSpriteSpellListEntry& entry : pLevel->m_List) {
        CHECK(entry.field_8 == pLevel->field_18);
    }
    // The memorized counts do not move, and field_10 follows: 4 + 2 + 4
    // castable becomes 3 + 3 + 3.
    CHECK(pLevel->m_List[0].field_4 == 4);
    CHECK(pLevel->m_List[1].field_4 == 2);
    CHECK(pLevel->m_List[2].field_4 == 4);
    CHECK(pLevel->field_10 == 9);
}
