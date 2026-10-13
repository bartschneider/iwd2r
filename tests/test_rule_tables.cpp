#include <catch2/catch_test_macros.hpp>

#include "CAIObjectType.h"
#include "CRuleTables.h"

// GetClassStringMixed itself reads the string table, which needs the game's
// data. Its choice of string for the fighter type does not.
TEST_CASE("The fighter type string is Barbarian for barbarians, else Fighter", "[rules]")
{
    REQUIRE(CRuleTables::GetFighterTypeStrRef(CAIOBJECTTYPE_C_BARBARIAN, 0) == 34);
    REQUIRE(CRuleTables::GetFighterTypeStrRef(CAIOBJECTTYPE_C_FIGHTER, 0) == 16799);
    REQUIRE(CRuleTables::GetFighterTypeStrRef(CAIOBJECTTYPE_C_BARD, 0) == 16799);
}

TEST_CASE("The fallen flags override the class for the fighter type string", "[rules]")
{
    REQUIRE(CRuleTables::GetFighterTypeStrRef(CAIOBJECTTYPE_C_BARBARIAN, 0x200) == 10371);
    REQUIRE(CRuleTables::GetFighterTypeStrRef(CAIOBJECTTYPE_C_FIGHTER, 0x400) == 10369);
}
