// Comparators for oracle cases: show a value as text and record a mismatch.
#pragma once

#include "oracle/runner/registry.h"

#include <cstdio>
#include <string>
#include <type_traits>

namespace oracle {

// Integers in decimal and, past 9, also in hex; strings quoted.
template <class T>
std::string Show(const T& value)
{
    if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
        const auto v = static_cast<long long>(value);
        std::string text = std::to_string(v);
        if (v > 9 || v < 0) {
            char hex[24];
            std::snprintf(hex, sizeof(hex), " (0x%llX)", static_cast<unsigned long long>(value));
            text += hex;
        }
        return text;
    } else if constexpr (std::is_pointer_v<T>) {
        char hex[24];
        std::snprintf(hex, sizeof(hex), "%p", static_cast<const void*>(value));
        return hex;
    } else {
        return "\"" + std::string(value) + "\"";
    }
}

// Records a failure with the context's current inputs when the values differ.
// Returns true when they are equal.
template <class T, class U>
bool Compare(CaseContext& ctx, const T& original, const U& ours)
{
    if (original == ours) {
        return true;
    }
    ctx.Fail(ctx.Inputs(), Show(original), Show(ours));
    return false;
}

} // namespace oracle
