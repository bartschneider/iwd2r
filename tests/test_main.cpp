// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "main.h"

TEST_CASE("main accepts only the resolutions that start", "[main]")
{
    CHECK(iwd2r_IsSupportedResolution(800));
    CHECK(iwd2r_IsSupportedResolution(1024));
    CHECK_FALSE(iwd2r_IsSupportedResolution(1280));
    CHECK_FALSE(iwd2r_IsSupportedResolution(1366));
    CHECK_FALSE(iwd2r_IsSupportedResolution(1600));
    CHECK_FALSE(iwd2r_IsSupportedResolution(2048));
    CHECK_FALSE(iwd2r_IsSupportedResolution(0));
}
