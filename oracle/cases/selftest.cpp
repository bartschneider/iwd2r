// Self-test of the oracle runner: isolation between cases works, a wrong
// result is seen, a fault in original code is caught and reported, and our
// engine objects are linked in.
#include "oracle/cases/selftest_ours.h"
#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstring>
#include <intrin.h>
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

// An exception registration on the stack whose handler lives in executable
// private memory, as every handler of the mapped original does (its image is
// MEM_PRIVATE, not MEM_IMAGE). The handler counts its calls and returns
// ExceptionContinueSearch. A fault below it must reach the runner's __except
// through it: the handler ran at least once and the crash was recorded. With
// DEP on, x86 Windows refuses such handlers and the process dies instead.
struct Registration {
    Registration* next;
    void* handler;
};

// C4733: the registration is deliberately not a SafeSEH handler of this
// image; it stands in for a handler in the mapped original.
#pragma warning(push)
#pragma warning(disable : 4733)
void OriginalSehFrame(CaseContext& ctx)
{
    auto* page = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    if (!page) {
        ctx.Fail("VirtualAlloc", "an executable page", "none");
        return;
    }
    auto* counter = reinterpret_cast<volatile uint32_t*>(page + 0x20);
    const uint32_t counterAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(counter));
    // inc dword [counter]; mov eax, 1; ret
    const uint8_t stub[] = { 0xFF, 0x05, 0, 0, 0, 0, 0xB8, 1, 0, 0, 0, 0xC3 };
    std::memcpy(page, stub, sizeof(stub));
    std::memcpy(page + 2, &counterAddress, 4);
    FlushInstructionCache(GetCurrentProcess(), page, sizeof(stub));
    ctx.SetInputs("fault at 0x10 below a registration with handler at private " + oracle::Show(counterAddress - 0x20));
    try {
        ctx.CallOriginal([page] {
            Registration registration;
            registration.next = reinterpret_cast<Registration*>(static_cast<uintptr_t>(__readfsdword(0)));
            registration.handler = page;
            __writefsdword(0, static_cast<unsigned long>(reinterpret_cast<uintptr_t>(&registration)));
            *reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(0x10)) = 1;
            __writefsdword(0, static_cast<unsigned long>(reinterpret_cast<uintptr_t>(registration.next)));
        });
    } catch (const CaseCrashed&) {
        if (*counter == 0) {
            ctx.Fail(ctx.Inputs(), "the private handler called", "never called");
        }
        throw;
    }
    ctx.Fail(ctx.Inputs(), "an access violation", "returned normally");
}
#pragma warning(pop)

// CDungeonMaster::TimerSynchronousUpdate (0x599760, thiscall) registers a
// __CxxFrameHandler frame (handler stub 0x8217B8, no try blocks) and then
// reads [this+4] while its state is still -1. With this = 0x10 it faults at
// 0x599779 under a real original C++ EH frame.
void OriginalCxxFrame(CaseContext& ctx)
{
    using Method = void(__fastcall*)(void* self, void* unused);
    ctx.SetInputs("CDungeonMaster::TimerSynchronousUpdate(this 0x10)");
    ctx.CallOriginal([] {
        reinterpret_cast<Method>(static_cast<uintptr_t>(0x599760))(reinterpret_cast<void*>(0x10), nullptr);
    });
    ctx.Fail(ctx.Inputs(), "an access violation", "returned normally");
}

// AfxThrowMemoryException (0x7FB057): `throw &_simpleMemoryException` through
// the original _CxxThrowException (0x7E8930) with the original's ThrowInfo
// for CMemoryException* (0x8A1848). Nothing in the call catches it.
void OriginalThrow(CaseContext& ctx)
{
    using Throw = void(__cdecl*)();
    ctx.SetInputs("AfxThrowMemoryException()");
    ctx.CallOriginal([] { reinterpret_cast<Throw>(static_cast<uintptr_t>(0x7FB057))(); });
    ctx.Fail(ctx.Inputs(), "an uncaught original C++ throw", "returned normally");
}

// Our engine is linked into the runner: our IcewindMisc::Roll(3, 6) after our
// srand(1) draws the VC6 sequence 41, 18467, 6334 (6 + 6 + 5 = 17), and our
// generator's next value is 26500. This proves the link only; rand.cpp
// compares ours with the original.
void OursLinked(CaseContext& ctx)
{
    ctx.SetInputs("ours: iwd2r_srand(1), IcewindMisc::Roll(3, 6)");
    oracle::Compare(ctx, 17, selftest_ours::SeededRoll(1, 3, 6));
    ctx.SetInputs("ours: iwd2r_rand() after the roll");
    oracle::Compare(ctx, 26500, selftest_ours::NextRand());
    ctx.CountInputs(2);
}

} // namespace

// The crash runs before must_fail, so the report shows a case after a fault
// still running in the same process.
ORACLE_CASE("selftest/isolation", Isolation);
ORACLE_CASE_EXPECT("selftest/crash", Crash, OracleExpect::Crash);
ORACLE_CASE_EXPECT("selftest/original_seh_frame", OriginalSehFrame, OracleExpect::Crash);
ORACLE_CASE_EXPECT("selftest/original_cxx_frame", OriginalCxxFrame, OracleExpect::Crash);
ORACLE_CASE_EXPECT("selftest/original_throw", OriginalThrow, OracleExpect::Crash);
ORACLE_CASE_EXPECT("selftest/must_fail", MustFail, OracleExpect::Fail);
ORACLE_CASE("selftest/ours_linked", OursLinked);
