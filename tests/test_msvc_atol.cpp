#include <catch2/catch_test_macros.hpp>

#include "msvc_atol.h"

TEST_CASE("VC6 atol reads signs, spaces and stops at junk", "[atol]")
{
    REQUIRE(iwd2r_atol("0") == 0);
    REQUIRE(iwd2r_atol("528000") == 528000);
    REQUIRE(iwd2r_atol("+5") == 5);
    REQUIRE(iwd2r_atol("-5") == -5);
    REQUIRE(iwd2r_atol("--5") == 0);
    REQUIRE(iwd2r_atol("+-5") == 0);
    REQUIRE(iwd2r_atol(" \t\r\n\v\f7") == 7);
    REQUIRE(iwd2r_atol("12abc") == 12);
    REQUIRE(iwd2r_atol("0x10") == 0);
    REQUIRE(iwd2r_atol("007") == 7);
    REQUIRE(iwd2r_atol("-0") == 0);
    REQUIRE(iwd2r_atol("") == 0);
    REQUIRE(iwd2r_atol("-") == 0);
    REQUIRE(iwd2r_atol("abc") == 0);
    REQUIRE(iwd2r_atol("- 5") == 0);
    REQUIRE(iwd2r_atol("\xA0"
                       "5")
        == 0);
}

TEST_CASE("VC6 atol wraps modulo 2^32 instead of saturating", "[atol]")
{
    // Values the oracle's tables/getatlong case saw from the original.
    REQUIRE(iwd2r_atol("2147483647") == 2147483647);
    REQUIRE(iwd2r_atol("2147483648") == INT32_MIN);
    REQUIRE(iwd2r_atol("-2147483648") == INT32_MIN);
    REQUIRE(iwd2r_atol("-2147483649") == 2147483647);
    REQUIRE(iwd2r_atol("99999999999") == 1215752191);
    REQUIRE(iwd2r_atol("4294967296") == 0);
    REQUIRE(iwd2r_atol("4294967295") == -1);
}
