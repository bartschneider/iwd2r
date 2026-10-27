#include "platform/affinity.h"

#ifdef _WIN32
// Only this file includes <windows.h>: the shim's own Win32 declarations
// (win32_api.h) would clash with it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif !defined(__APPLE__)
#include <sched.h>
#include <sys/types.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#endif

namespace platform {

bool SetNativeProcessAffinity(uint64_t mask)
{
    if (mask == 0)
        return false;
#ifdef _WIN32
    return ::SetProcessAffinityMask(::GetCurrentProcess(), static_cast<DWORD_PTR>(mask)) != 0;
#elif defined(__APPLE__)
    // macOS has no binding affinity API (Apple Silicon ignores even the
    // THREAD_AFFINITY_POLICY hint), so the mask is accepted and not applied.
    return true;
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int cpu = 0; cpu < 64 && cpu < CPU_SETSIZE; cpu++) {
        if (mask & (uint64_t(1) << cpu))
            CPU_SET(cpu, &set);
    }
    // Linux affinity is per thread: the calling thread first (its children
    // inherit it), then every other thread already running.
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        return false;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/self/task", ec)) {
        const std::string name = entry.path().filename().string();
        char* end = nullptr;
        const long tid = std::strtol(name.c_str(), &end, 10);
        if (end && *end == '\0' && tid > 0)
            sched_setaffinity(static_cast<pid_t>(tid), sizeof(set), &set);
    }
    return true;
#endif
}

}
