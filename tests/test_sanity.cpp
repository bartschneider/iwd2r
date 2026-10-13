#include "compat/version.h"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("version string is iwd2r", "[sanity]")
{
    REQUIRE(std::string(iwd2r_version()) == "iwd2r 0.1.0");
}
