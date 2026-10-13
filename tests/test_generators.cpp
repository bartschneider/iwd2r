// iwd2r. Distributed under LICENSE.md.
#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <cstdint>
#include <vector>

#include "oracle/runner/generators.h"

namespace {

template <class G>
std::vector<typename G::value_type> Take(G gen, size_t limit)
{
    std::vector<typename G::value_type> out;
    while (out.size() < limit) {
        auto value = gen.Next();
        if (!value) {
            break;
        }
        out.push_back(*value);
    }
    return out;
}

} // namespace

TEST_CASE("SplitMix64 gives the reference sequence", "[oracle]")
{
    uint64_t state = 0;
    CHECK(oracle::SplitMix64(&state) == 0xE220A8397B1DCDAFull);
    CHECK(oracle::SplitMix64(&state) == 0x6E789E6AA1B965F4ull);
    CHECK(oracle::SplitMix64(&state) == 0x06C45D188009454Full);
}

TEST_CASE("IntRange sweeps every value including both ends", "[oracle]")
{
    CHECK(Take(oracle::IntRange(-2, 2), 100) == std::vector<int> { -2, -1, 0, 1, 2 });
    CHECK(Take(oracle::IntRange(5, 5), 100) == std::vector<int> { 5 });
    CHECK(Take(oracle::IntRange(3, 2), 100).empty());
    // Ends at the type's maximum without wrapping.
    CHECK(Take(oracle::IntRange<uint8_t>(254, 255), 100) == std::vector<uint8_t> { 254, 255 });
    CHECK(Take(oracle::IntRange(INT_MAX - 1, INT_MAX), 100) == std::vector<int> { INT_MAX - 1, INT_MAX });
    CHECK(oracle::IntRangeGen<int>::kBounded);
}

TEST_CASE("Edges yields the listed values in order", "[oracle]")
{
    CHECK(Take(oracle::Edges({ 0, -1, INT_MIN, INT_MAX }), 100) == std::vector<int> { 0, -1, INT_MIN, INT_MAX });
    CHECK(oracle::EdgesGen<int>::kBounded);
}

TEST_CASE("Random is deterministic per seed and pinned to splitmix64", "[oracle]")
{
    CHECK(Take(oracle::Random<int>(42), 1000) == Take(oracle::Random<int>(42), 1000));
    CHECK(Take(oracle::Random<int>(42), 1000) != Take(oracle::Random<int>(43), 1000));
    CHECK(Take(oracle::Random<uint64_t>(0), 2) == std::vector<uint64_t> { 0xE220A8397B1DCDAFull, 0x6E789E6AA1B965F4ull });
    // Low 32 bits of splitmix64(42): 0x2FEB6E95, 0xB266F103.
    CHECK(Take(oracle::Random<int32_t>(42), 2) == std::vector<int32_t> { 803958421, -1301876477 });
    // Ranged: lo + bits % span.
    CHECK(Take(oracle::Random<int>(42, 10, 20), 2) == std::vector<int> { 19, 15 });
    for (int value : Take(oracle::Random<int>(7, -3, 3), 1000)) {
        CHECK(value >= -3);
        CHECK(value <= 3);
    }
    CHECK_FALSE(oracle::RandomGen<int>::kBounded);
}
