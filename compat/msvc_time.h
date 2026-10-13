// time() for engine code, with a replaceable wall clock: the deterministic
// replay mode (platform/replay.h) installs a virtual one, so the engine's
// srand(time(NULL)) seeds the same value as the original's under the replay
// recorder. Without one installed it is the C library's time().
#pragma once

#include <ctime>

// The wall clock in seconds since 1970-01-01 UTC; returns false to fall
// back to the C library's time().
using iwd2r_wall_clock_fn = bool (*)(long long* seconds);

// Installs the wall clock (nullptr removes it).
void iwd2r_set_wall_clock(iwd2r_wall_clock_fn clock);

// time(): the installed wall clock's seconds when it answers, else
// std::time. Stores the result in *out too when out is not null.
time_t iwd2r_time(time_t* out);

// True when the installed wall clock answers: its time zone is UTC with no
// daylight saving (CTime's local breakdowns then use UTC).
bool iwd2r_wall_clock_is_virtual();
