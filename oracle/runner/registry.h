// iwd2r. Distributed under LICENSE.md.
// Oracle cases: registration, the per-case context, crash capture around calls
// into the original, and the .data snapshot. 32-bit Windows only (runner).
//
// A case file oracle/cases/<file>.cpp registers its cases with
//
//   ORACLE_CASE("rand/sequence", RandSequence);
//   ORACLE_CASE_EXPECT("selftest/must_fail", MustFail, OracleExpect::Fail);
//
// The runner runs the cases of one file per process (--file <file>), restoring
// the original's .data and its rand state before each.
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
    void (*run)(CaseContext&);
    OracleExpect expect = OracleExpect::Pass;
};

// Every registered case, in registration order.
std::vector<OracleCase>& OracleCases();

// The stem of a case's file: "selftest" for .../oracle/cases/selftest.cpp.
std::string CaseFileName(const OracleCase& c);

struct OracleRegistrar {
    OracleRegistrar(const char* id, const char* file, void (*run)(CaseContext&), OracleExpect expect)
    {
        OracleCases().push_back(OracleCase { id, file, run, expect });
    }
};

#define ORACLE_CONCAT2(a, b) a##b
#define ORACLE_CONCAT(a, b) ORACLE_CONCAT2(a, b)
#define ORACLE_CASE_EXPECT(id, fn, expect) \
    static OracleRegistrar ORACLE_CONCAT(g_oracleCase, __LINE__)(id, __FILE__, fn, expect)
#define ORACLE_CASE(id, fn) ORACLE_CASE_EXPECT(id, fn, OracleExpect::Pass)

struct OracleFailure {
    std::string inputs;
    std::string original;
    std::string ours;
};

struct OracleCrash {
    std::string inputs;
    uint32_t code = 0; // the exception code, such as 0xC0000005
    uint32_t address = 0; // where it was raised
};

// Thrown by CallOriginal after a fault, caught by the runner: the case stops,
// later cases of the file still run.
struct CaseCrashed : std::runtime_error {
    CaseCrashed()
        : std::runtime_error("case crashed")
    {
    }
};

// Calls thunk(arg) under __try/__except. On a fault returns false and fills
// code and address. Defined in snapshot.cpp, the runner's one SEH function.
bool CallGuarded(void (*thunk)(void*), void* arg, uint32_t* code, uint32_t* address);

class CaseContext {
public:
    CaseContext(uint64_t seedValue, int iterationCount)
        : seed(seedValue)
        , iterations(iterationCount)
    {
    }

    uint64_t seed;
    int iterations;

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
        uint32_t code = 0;
        uint32_t address = 0;
        using Fn = std::remove_reference_t<F>;
        auto thunk = [](void* arg) { (*static_cast<Fn*>(arg))(); };
        if (!CallGuarded(thunk, const_cast<void*>(static_cast<const void*>(&fn)), &code, &address)) {
            crashed_ = true;
            crash_ = OracleCrash { inputs_, code, address };
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
