// iwd2r. Distributed under LICENSE.md.
#include "oracle/runner/report.h"

#include <cstdio>

namespace {

std::string Quote(const std::string& text)
{
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20 || c >= 0x7F) {
            char escaped[8];
            std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
            out += escaped;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", value);
    return Quote(text);
}

const char* ExpectName(OracleExpect expect)
{
    switch (expect) {
    case OracleExpect::Fail:
        return "fail";
    case OracleExpect::Crash:
        return "crash";
    default:
        return "pass";
    }
}

} // namespace

bool AsExpected(const CaseResult& result)
{
    switch (result.expect) {
    case OracleExpect::Fail:
        return !result.crashed && result.failureCount > 0;
    case OracleExpect::Crash:
        return result.crashed;
    default:
        return !result.crashed && result.failureCount == 0;
    }
}

std::string ReportJson(const std::string& file, uint64_t seed, int iterations, const std::vector<CaseResult>& results)
{
    std::string out = "{\n  \"file\": " + Quote(file) + ",\n  \"seed\": " + std::to_string(seed)
        + ",\n  \"iterations\": " + std::to_string(iterations) + ",\n  \"cases\": [";
    for (size_t i = 0; i < results.size(); ++i) {
        const CaseResult& r = results[i];
        out += i ? ",\n" : "\n";
        out += "    {\"case\": " + Quote(r.id) + ", \"expect\": " + Quote(ExpectName(r.expect))
            + ", \"iterations\": " + std::to_string(r.iterations)
            + ", \"passed\": " + (!r.crashed && r.failureCount == 0 ? "true" : "false")
            + ", \"failure_count\": " + std::to_string(r.failureCount) + ", \"failures\": [";
        for (size_t f = 0; f < r.failures.size(); ++f) {
            const OracleFailure& failure = r.failures[f];
            out += f ? ", " : "";
            out += "{\"inputs\": " + Quote(failure.inputs) + ", \"original\": " + Quote(failure.original)
                + ", \"ours\": " + Quote(failure.ours) + "}";
        }
        out += std::string("], \"crashed\": ") + (r.crashed ? "true" : "false") + ", \"crash\": ";
        if (r.crashed) {
            out += "{\"inputs\": " + Quote(r.crash.inputs) + ", \"exception\": " + Hex(r.crash.code)
                + ", \"address\": " + Hex(r.crash.address) + "}";
        } else {
            out += "null";
        }
        out += "}";
    }
    return out + "\n  ]\n}\n";
}
