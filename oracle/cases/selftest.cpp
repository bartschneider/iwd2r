// iwd2r. Distributed under LICENSE.md.
// Self-test of the oracle runner: isolation between cases works, a wrong
// result is seen, and a fault in original code is caught and reported.
#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <string>
#include <vector>

namespace {

using Srand = void(__cdecl*)(unsigned int);
using Rand = int(__cdecl*)();

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

int OriginalRand()
{
    return reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))();
}

constexpr int kDraws = 1000;
// The first dword of .data, outside every CRT range.
constexpr uintptr_t kDataSentinel = 0x8A5000;

// Seeds the original rand as the runner did before its snapshot, draws 1,000
// values, restores, and draws again: the sequences must match, which they only
// do if RestoreData puts back the per-thread _holdrand. The next 1,000 draws
// without a restore must differ, so the match is not vacuous. A .data dword
// changed before the restore must come back too.
void Isolation(CaseContext& ctx)
{
    const unsigned int seed = static_cast<unsigned int>(ctx.seed);
    std::vector<int> first;
    std::vector<int> second;
    std::vector<int> continued;
    ctx.SetInputs("srand(" + std::to_string(seed) + ")");
    ctx.CallOriginal([&] {
        OriginalSrand(seed);
        for (int i = 0; i < kDraws; ++i) {
            first.push_back(OriginalRand());
        }
    });
    auto* sentinel = reinterpret_cast<volatile uint32_t*>(kDataSentinel);
    const uint32_t before = *sentinel;
    *sentinel = ~before;
    RestoreData();
    ctx.SetInputs(".data 0x008A5000 after RestoreData");
    oracle::Compare(ctx, before, static_cast<uint32_t>(*sentinel));
    ctx.CallOriginal([&] {
        for (int i = 0; i < kDraws; ++i) {
            second.push_back(OriginalRand());
        }
        for (int i = 0; i < kDraws; ++i) {
            continued.push_back(OriginalRand());
        }
    });
    for (int i = 0; i < kDraws; ++i) {
        ctx.SetInputs("srand(" + std::to_string(seed) + "), draw " + std::to_string(i) + " before and after RestoreData");
        oracle::Compare(ctx, first[i], second[i]);
    }
    if (continued == first) {
        ctx.Fail("draws 1000-1999 without RestoreData", "a different sequence", "the same sequence");
    }
    ctx.CountInputs(kDraws);
}

// Compares the original rand() with rand() + 1: every input must fail. The
// runner lists it under expected failures and fails the run if it passes.
void MustFail(CaseContext& ctx)
{
    ctx.ForEachInput(oracle::IntRange(1, 100), [&](int) {
        int original = 0;
        ctx.CallOriginal([&] { original = OriginalRand(); });
        oracle::Compare(ctx, original, original + 1);
    });
}

// 0x401000 is MSVC's `vector constructor iterator` (__stdcall: ptr, size,
// count, ctor; it calls ctor(ptr) count times with ptr in ecx). Count 1 and
// ctor 0x10 make it call into the unmapped first page: an access violation at
// 0x00000010, every time.
void Crash(CaseContext& ctx)
{
    using VectorCtor = void(__stdcall*)(void*, unsigned int, int, void*);
    ctx.SetInputs("0x401000(ptr 0x10, size 4, count 1, ctor 0x10)");
    ctx.CallOriginal([] {
        reinterpret_cast<VectorCtor>(static_cast<uintptr_t>(0x401000))(
            reinterpret_cast<void*>(0x10), 4, 1, reinterpret_cast<void*>(0x10));
    });
    ctx.Fail(ctx.Inputs(), "an access violation", "returned normally");
}

} // namespace

// The crash runs before must_fail, so the report shows a case after a fault
// still running in the same process.
ORACLE_CASE("selftest/isolation", Isolation);
ORACLE_CASE_EXPECT("selftest/crash", Crash, OracleExpect::Crash);
ORACLE_CASE_EXPECT("selftest/must_fail", MustFail, OracleExpect::Fail);
