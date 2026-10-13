// The VC6 C runtime's atol(), so engine code converts table text the way the
// original exe does on every platform: 32-bit, wrapping on overflow.
#pragma once

#include <cstdint>

// Skips leading spaces (0x09-0x0D, 0x20), takes one optional '+' or '-',
// then accumulates decimal digits modulo 2^32 and negates for '-'. Stops at
// the first other character; no digits give 0.
int32_t iwd2r_atol(const char* text);
