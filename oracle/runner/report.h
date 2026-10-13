// The JSON report one runner process writes for its case file.
#pragma once

#include "oracle/runner/registry.h"

#include <string>
#include <vector>

struct CaseResult {
    std::string id;
    OracleExpect expect = OracleExpect::Pass;
    const char* pending = nullptr;
    long long iterations = 0;
    long long failureCount = 0;
    std::vector<OracleFailure> failures;
    bool crashed = false;
    OracleCrash crash;
};

constexpr const char* kHeapCorrupt = "heap corrupt after case";

// Whether a result is what its case declared: a Pass case passed (no failure,
// no crash), a Fail case failed without crashing, a Crash case crashed with
// no failure and without corrupting the heap.
bool AsExpected(const CaseResult& result);

// {"file", "complete", "running", "seed", "iterations", "cases": [{case, expect,
// pending (the reason, or null), iterations, passed, failure_count, failures:
// [{inputs, original, ours}], crashed, crash: {inputs, exception, address,
// kind, type} or null}]}.
// complete is false while cases are still to run; running names the case in
// progress ("running", null between cases).
std::string ReportJson(const std::string& file, uint64_t seed, int iterations, const std::vector<CaseResult>& results,
    bool complete, const char* running);
