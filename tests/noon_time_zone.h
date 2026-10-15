#pragma once

#include <chrono>
#include <cstdio>
#include <string>

// A TZ value under which the local time is 12:00:00 now. The main menu
// (CScreenConnection::UpdateMainPanel) shows its day background START from
// 07:00 to 17:59 local time and the night one STARTN, with an animated
// torch, otherwise, so its frame depends on the hour the test runs. POSIX
// and the MSVC CRT read TZ as "name[+|-]hh[:mm[:ss]]", hours west of UTC.
inline std::string NoonTimeZone()
{
    // system_clock, not time(): compat's headers route time() to the engine's clock.
    const long long now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    long long offset = ((now % 86400) + 86400) % 86400 - 12 * 3600; // UTC minus local, in seconds
    const char sign = offset < 0 ? '-' : '+';
    if (offset < 0)
        offset = -offset;
    char tz[32];
    std::snprintf(tz, sizeof(tz), "IWD%c%02lld:%02lld:%02lld", sign, offset / 3600, (offset / 60) % 60, offset % 60);
    return tz;
}
