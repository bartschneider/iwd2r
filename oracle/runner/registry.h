// Oracle cases: registration, the per-case context, crash capture around calls
// into the original, and the .data snapshot. 32-bit Windows only (runner).
//
// A case file oracle/cases/<file>.cpp registers its cases with
//
//   ORACLE_CASE("rand/sequence", RandSequence);
//   ORACLE_CASE_EXPECT("selftest/must_fail", MustFail, OracleExpect::Fail);
//   ORACLE_CASE_PENDING("tables/load", nullptr, "needs engine state");
//
// A pending case is one that cannot compare the original with ours yet; its
// reason goes into the report's "pending" list. With a body it runs and must
// pass like any case (an original-only check, say), and must run at least
// one input: one that runs none fails ("PENDING (ran 0 inputs)"). Without a
// body (nullptr) it is listed and not run.
//
// The runner runs the cases of one file per process (--file <file>), restoring
// the original's .data and its rand state before each.
//
// What a restore does not cover, and so what cases must respect:
// - Heap objects are not restored. A case may allocate (and leak) through the
//   original, but must not free or resize anything reachable from the
//   snapshot-era .data: the restore would bring back a pointer to a freed
//   block. After each case the runner validates the original's CRT heap
//   (HeapValidate). In practice Windows' heap terminates the process on the
//   corruption it finds (fail-fast, 0xC0000374) rather than returning FALSE:
//   the parent then prints the status and exits 11, and the report's
//   "running" field names the case. A FALSE return is reported as a crash
//   ("heap corrupt after case") and the file's remaining cases are skipped.
// - Allocator state is not restored either: the CRT heap's globals and MFC's
//   CFixedAlloc pools that CString buffers come from (vanilla.json's
//   crt_ranges), so original strings a case builds stay valid in later cases.
//   The hazard this leaves: a global CString in .data that a case assigns to
//   (or that original code the case calls assigns to) frees its old buffer
//   into the unrestored pool, and the next restore puts back the pointer to
//   that freed node, which the pool hands out again. So cases must not assign
//   to, or call original code that assigns to, CStrings (or other heap
//   owners) living in snapshot-era .data; build their own strings instead.
// - Of the thread's _tiddata only the rand state (_holdrand) is restored.
//   strtok's position, errno/_doserrno, the asctime/gmtime buffers and the
//   other per-thread fields carry over from case to case.
#pragma once

#include "oracle/runner/generators.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// What a case is declared to do. Fail and Crash exist for the self-test cases
// that prove the oracle can see a mismatch and survive a fault.
enum class OracleExpect { Pass,
    Fail,
    Crash };

class CaseContext;

struct OracleCase {
    const char* id; // "<file>/<name>"
    const char* file; // __FILE__ of the case file; its stem is the file name
    void (*run)(CaseContext&); // nullptr: a pending case that is not run
    OracleExpect expect = OracleExpect::Pass;
    const char* pending = nullptr; // why the case is pending, or nullptr
};

// Every registered case, in registration order.
std::vector<OracleCase>& OracleCases();

// The stem of a case's file: "selftest" for .../oracle/cases/selftest.cpp.
std::string CaseFileName(const OracleCase& c);

struct OracleRegistrar {
    OracleRegistrar(const char* id, const char* file, void (*run)(CaseContext&), OracleExpect expect,
        const char* pending = nullptr)
    {
        OracleCases().push_back(OracleCase { id, file, run, expect, pending });
    }
};

#define ORACLE_CONCAT2(a, b) a##b
#define ORACLE_CONCAT(a, b) ORACLE_CONCAT2(a, b)
#define ORACLE_CASE_EXPECT(id, fn, expect) \
    static OracleRegistrar ORACLE_CONCAT(g_oracleCase, __LINE__)(id, __FILE__, fn, expect)
#define ORACLE_CASE(id, fn) ORACLE_CASE_EXPECT(id, fn, OracleExpect::Pass)
#define ORACLE_CASE_PENDING(id, fn, reason) \
    static OracleRegistrar ORACLE_CONCAT(g_oracleCase, __LINE__)(id, __FILE__, fn, OracleExpect::Pass, reason)

struct OracleFailure {
    std::string inputs;
    std::string original;
    std::string ours;
};

struct OracleCrash {
    std::string inputs;
    uint32_t code = 0; // the exception code, such as 0xC0000005
    uint32_t address = 0; // where it was raised
    std::string kind; // "fault", "original C++ throw" or "heap corrupt after case"
    std::string type; // an original C++ throw's type, such as .PAVCMemoryException@@
};

// What CallGuarded saw. Plain data: it is filled inside an exception filter.
struct GuardedFault {
    uint32_t code;
    uint32_t address;
    bool originalThrow; // a C++ throw whose ThrowInfo is in the original image
    char type[128];
};

// Thrown by CallOriginal after a fault, caught by the runner: the case stops,
// later cases of the file still run.
struct CaseCrashed : std::runtime_error {
    CaseCrashed()
        : std::runtime_error("case crashed")
    {
    }
};

// Calls thunk(arg) under __try/__except. On a fault, or a C++ exception thrown
// by the original (ThrowInfo inside its image), returns false and fills
// *fault. The runner's own C++ throws pass through. Defined in snapshot.cpp,
// the runner's one SEH function.
bool CallGuarded(void (*thunk)(void*), void* arg, GuardedFault* fault);

// Initializes the original's native CRITICAL_SECTION at section, a raw address
// in a block a case allocated on the original's behalf. The engine's own
// CRITICAL_SECTION is a different type (a single shim pointer), while the
// original calls kernel32's on a native one, so a case that makes original code
// enter a critical section must initialize it here. Defined in snapshot.cpp,
// which includes <windows.h>.
void InitializeOriginalCriticalSection(void* section);

class CaseContext {
public:
    CaseContext(uint64_t seedValue, int iterationCount)
        : seed(seedValue)
        , iterations(iterationCount)
    {
    }

    uint64_t seed;
    int iterations;
    // The folder of extracted game text (--tables), without a trailing
    // separator; empty when the runner was given none.
    std::string tables;

    // Records a mismatch. All are counted; the first kKeptFailures are kept
    // for the report.
    void Fail(const std::string& inputs, const std::string& original, const std::string& ours)
    {
        ++failureCount_;
        if (failures_.size() < kKeptFailures) {
            failures_.push_back(OracleFailure { inputs, original, ours });
        }
    }

    // Feeds every value of gen to f(value), or the first `iterations` of an
    // unbounded generator. Inputs() names the current value meanwhile.
    template <class G, class F>
    void ForEachInput(G gen, F f)
    {
        long long limit = iterations;
        if constexpr (G::kBounded) {
            limit = -1; // swept to the end
        }
        long long count = 0;
        while (limit < 0 || count < limit) {
            auto value = gen.Next();
            if (!value) {
                break;
            }
            inputs_ = std::to_string(*value);
            ++count;
            ++inputsRun_;
            f(*value);
        }
        inputs_.clear();
    }

    // Runs fn(), which calls original code, under crash capture. A fault is
    // recorded with Inputs() and ends the case.
    template <class F>
    void CallOriginal(F&& fn)
    {
        GuardedFault fault = {};
        using Fn = std::remove_reference_t<F>;
        auto thunk = [](void* arg) { (*static_cast<Fn*>(arg))(); };
        if (!CallGuarded(thunk, const_cast<void*>(static_cast<const void*>(&fn)), &fault)) {
            crashed_ = true;
            crash_ = OracleCrash { inputs_, fault.code, fault.address,
                fault.originalThrow ? "original C++ throw" : "fault", fault.type };
            throw CaseCrashed();
        }
    }

    // Names the inputs for Fail() and a crash, outside ForEachInput.
    void SetInputs(const std::string& inputs) { inputs_ = inputs; }
    const std::string& Inputs() const { return inputs_; }
    // Counts inputs a case runs without ForEachInput.
    void CountInputs(long long n) { inputsRun_ += n; }

    long long InputsRun() const { return inputsRun_; }
    long long FailureCount() const { return failureCount_; }
    const std::vector<OracleFailure>& Failures() const { return failures_; }
    bool Crashed() const { return crashed_; }
    // Records a crash found after the case, such as a corrupt heap.
    void SetCrash(const OracleCrash& crash)
    {
        crashed_ = true;
        crash_ = crash;
    }
    const OracleCrash& Crash() const { return crash_; }

    static constexpr size_t kKeptFailures = 20;

private:
    std::string inputs_;
    long long inputsRun_ = 0;
    long long failureCount_ = 0;
    std::vector<OracleFailure> failures_;
    bool crashed_ = false;
    OracleCrash crash_;
};

// The original's .data (minus the CRT ranges in vanilla.json) and the main
// thread's _tiddata rand state, saved once by TakeSnapshot in OracleMain on
// the thread that runs the cases.
struct DataRange {
    uint32_t start;
    uint32_t end;
};
bool TakeSnapshot(DataRange data, std::string* error);
// Puts the snapshot back. The runner calls it before every case.
void RestoreData();
// HeapValidate on the original's CRT heap (_crtheap); the runner calls it
// after every case.
bool OriginalHeapValid();
