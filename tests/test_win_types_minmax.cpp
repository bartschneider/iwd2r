// Deliberately no NOMINMAX: exercises the Windows-style min/max macros.
#include <catch2/catch_test_macros.hpp>

#include "compat/win_types.h"

TEST_CASE("min max macros", "[win_types]")
{
    REQUIRE(min(3, 5L) == 3);
    REQUIRE(max(3, 5L) == 5);
}
