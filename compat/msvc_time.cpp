#include "msvc_time.h"

#include <atomic>

namespace {
std::atomic<iwd2r_wall_clock_fn> g_clock { nullptr };
}

void iwd2r_set_wall_clock(iwd2r_wall_clock_fn clock)
{
    g_clock.store(clock);
}

time_t iwd2r_time(time_t* out)
{
    long long seconds = 0;
    const iwd2r_wall_clock_fn clock = g_clock.load();
    const time_t now = clock && clock(&seconds) ? static_cast<time_t>(seconds) : std::time(nullptr);
    if (out)
        *out = now;
    return now;
}

bool iwd2r_wall_clock_is_virtual()
{
    long long seconds = 0;
    const iwd2r_wall_clock_fn clock = g_clock.load();
    return clock && clock(&seconds);
}
