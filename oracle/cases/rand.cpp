// The VC6 rand and IcewindMisc::Roll, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>
// (compat's win_types.h defines the same names): it reaches the original only
// through CallOriginal and the addresses in vanilla_config.h.
#include "IcewindMisc.h"
#include "msvc_rand.h"

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

using Srand = void(__cdecl*)(unsigned int);
using Rand = int(__cdecl*)();

// IcewindMisc::Roll at 0x5845D0 is a static member taking (nRolls, nSides) as
// __cdecl: it reads nRolls at [esp+4] and nSides at [esp+8] (after its pushes,
// [esp+0x14]), never touches ecx, and ends in a plain `ret` at 0x5845FF and
// 0x584603. Its three callers in IcewindCGameEffectTensersTransformation::
// ApplyEffect (0x56B177, 0x56B183, 0x56B18F) push two arguments each and pop
// all three calls' at once with `add esp, 0x18` at 0x56B194 (objdump of the
// vanilla exe; tools/ghidra/query.py names it IcewindMisc::Roll, a function).
using Roll = int(__cdecl*)(int nRolls, int nSides);
constexpr uintptr_t kOriginalRoll = 0x5845D0;

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

int OriginalRand()
{
    return reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))();
}

int OriginalRoll(int nRolls, int nSides)
{
    return reinterpret_cast<Roll>(kOriginalRoll)(nRolls, nSides);
}

// 0, 1 and 42, then 10 seeds from the run's --seed: a failure names its seed,
// and the same --seed replays it.
std::vector<unsigned int> Seeds(const CaseContext& ctx)
{
    std::vector<unsigned int> seeds = { 0, 1, 42 };
    auto gen = oracle::Random<uint32_t>(ctx.seed);
    for (int i = 0; i < 10; ++i) {
        seeds.push_back(*gen.Next());
    }
    return seeds;
}

std::string SeedText(unsigned int seed)
{
    return "srand(" + oracle::Show(seed) + ")";
}

constexpr int kSequenceDraws = 100000;

// srand(seed) on both sides, then 100,000 consecutive values of the original
// rand (0x7E8160) against iwd2r_rand. A seed's sequences are only compared up
// to their first difference: after it every value would differ.
void Sequence(CaseContext& ctx)
{
    std::vector<int> original(kSequenceDraws);
    for (const unsigned int seed : Seeds(ctx)) {
        ctx.SetInputs(SeedText(seed) + ", " + std::to_string(kSequenceDraws) + " draws");
        ctx.CallOriginal([&] {
            OriginalSrand(seed);
            for (int& value : original) {
                value = OriginalRand();
            }
        });
        iwd2r_srand(seed);
        for (int i = 0; i < kSequenceDraws; ++i) {
            const int ours = iwd2r_rand();
            if (original[i] != ours) {
                ctx.SetInputs(SeedText(seed) + ", draw " + std::to_string(i));
                oracle::Compare(ctx, original[i], ours);
                break;
            }
        }
        ctx.CountInputs(kSequenceDraws);
    }
}

// The (nRolls, nSides) pairs: every nRolls in [0, 20] with nSides 0, 1 and
// 100; every nSides in [0, 100] with nRolls 0, 1 and 20; then --iterations
// random pairs with nRolls in [0, 20] and nSides in [0, 100].
std::vector<std::pair<int, int>> RollPairs(const CaseContext& ctx)
{
    std::vector<std::pair<int, int>> pairs;
    for (const int sides : { 0, 1, 100 }) {
        for (int rolls = 0; rolls <= 20; ++rolls) {
            pairs.emplace_back(rolls, sides);
        }
    }
    for (const int rolls : { 0, 1, 20 }) {
        for (int sides = 0; sides <= 100; ++sides) {
            pairs.emplace_back(rolls, sides);
        }
    }
    uint64_t state = ctx.seed ^ 0x526F6C6C00000000ull; // "Roll": apart from Seeds()'s stream
    auto rolls = oracle::Random<int>(oracle::SplitMix64(&state), 0, 20);
    auto sides = oracle::Random<int>(oracle::SplitMix64(&state), 0, 100);
    for (int i = 0; i < ctx.iterations; ++i) {
        pairs.emplace_back(*rolls.Next(), *sides.Next());
    }
    return pairs;
}

// For each seed: srand(seed) on both sides, then every pair in turn, without
// reseeding, through the original IcewindMisc::Roll and ours. After each roll
// both sides draw one more value from their own generator (the original rand,
// iwd2r_rand); equal values prove both rolls consumed the same number of
// draws. A seed stops at its first mismatch: the streams have diverged.
void RollCase(CaseContext& ctx)
{
    const std::vector<std::pair<int, int>> pairs = RollPairs(ctx);
    for (const unsigned int seed : Seeds(ctx)) {
        ctx.SetInputs(SeedText(seed));
        ctx.CallOriginal([&] { OriginalSrand(seed); });
        iwd2r_srand(seed);
        for (size_t k = 0; k < pairs.size(); ++k) {
            const int nRolls = pairs[k].first;
            const int nSides = pairs[k].second;
            const std::string inputs = SeedText(seed) + ", pair " + std::to_string(k) + ": Roll(" + std::to_string(nRolls)
                + ", " + std::to_string(nSides) + ")";
            ctx.SetInputs(inputs);
            int original = 0;
            int originalNext = 0;
            ctx.CallOriginal([&] {
                original = OriginalRoll(nRolls, nSides);
                originalNext = OriginalRand();
            });
            const int ours = IcewindMisc::Roll(nRolls, nSides);
            const int oursNext = iwd2r_rand();
            ctx.CountInputs(1);
            bool same = oracle::Compare(ctx, original, ours);
            ctx.SetInputs(inputs + ", the following rand");
            same = oracle::Compare(ctx, originalNext, oursNext) && same;
            if (!same) {
                break;
            }
        }
    }
}

} // namespace

ORACLE_CASE("rand/sequence", Sequence);
ORACLE_CASE("rand/roll", RollCase);
