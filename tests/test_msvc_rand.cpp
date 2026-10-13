// iwd2r. Distributed under LICENSE.md.
#include <catch2/catch_test_macros.hpp>

#define NOMINMAX
#include <thread>

#include "msvc_rand.h"

TEST_CASE("VC6 rand sequence after srand(1)", "[rand]")
{
    iwd2r_srand(1);
    const int expected[] = { 41, 18467, 6334, 26500, 19169 };
    for (int value : expected) {
        REQUIRE(iwd2r_rand() == value);
    }
}

TEST_CASE("VC6 rand first value for other seeds", "[rand]")
{
    iwd2r_srand(0);
    REQUIRE(iwd2r_rand() == 38);
    iwd2r_srand(42);
    REQUIRE(iwd2r_rand() == 175);
}

TEST_CASE("An unseeded thread starts as if seeded with 1", "[rand]")
{
    int first = -1;
    std::thread worker([&] { first = iwd2r_rand(); });
    worker.join();
    REQUIRE(first == 41);
}

TEST_CASE("Threads keep independent sequences", "[rand]")
{
    iwd2r_srand(1);
    int a = -1;
    int b = -1;
    std::thread worker([&] {
        iwd2r_srand(42);
        a = iwd2r_rand();
    });
    worker.join();
    b = iwd2r_rand();
    REQUIRE(a == 175);
    REQUIRE(b == 41);
}

TEST_CASE("rand state is the LCG state", "[rand]")
{
    iwd2r_srand(1);
    (void)iwd2r_rand();
    REQUIRE(iwd2r_rand_state() == static_cast<unsigned int>(1u * 214013u + 2531011u));
}

#include "mfc.h"

TEST_CASE("mfc.h routes rand and srand to the VC6 versions", "[rand]")
{
    srand(1);
    REQUIRE(rand() == 41);
}
