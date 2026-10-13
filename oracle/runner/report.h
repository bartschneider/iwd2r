// iwd2r. Distributed under LICENSE.md.
// The JSON report one runner process writes for its case file.
#pragma once

#include "oracle/runner/registry.h"

#include <string>
#include <vector>

struct CaseResult {
    std::string id;
    OracleExpect expect = OracleExpect::Pass;
    long long iterations = 0;
    long long failureCount = 0;
    std::vector<OracleFailure> failures;
    bool crashed = false;
    OracleCrash crash;
};

// Whether a result is what its case declared: a Pass case passed (no failure,
// no crash), a Fail case failed without crashing, a Crash case crashed.
bool AsExpected(const CaseResult& result);

// {"file", "seed", "iterations", "cases": [{case, expect, iterations, passed,
// failure_count, failures: [{inputs, original, ours}], crashed, crash}]}
std::string ReportJson(const std::string& file, uint64_t seed, int iterations, const std::vector<CaseResult>& results);
