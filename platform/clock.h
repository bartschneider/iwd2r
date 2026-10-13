// Swappable millisecond clock behind GetTickCount and timeGetTime.
#pragma once

#include <cstdint>

namespace platform {

class ClockSource {
public:
    virtual ~ClockSource();
    virtual uint64_t NowMs() = 0;
};

// Installs a clock source (not owned). nullptr restores the default.
void SetClockSource(ClockSource* source);

// The default source: steady_clock milliseconds since the first call.
ClockSource* DefaultClockSource();

} // namespace platform
