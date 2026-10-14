#include "msvc_rand.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <dlfcn.h>
#endif

namespace {
// A thread that never seeds starts as if seeded with 1, as in VC6.
thread_local unsigned int g_state = 1;

// IWD2R_RAND_TRACE (docs/rand-trace.md): one line per call.
std::atomic<unsigned int> g_traceFrame { 0 };

FILE* TraceFile()
{
    static FILE* file = []() -> FILE* {
        const char* on = std::getenv("IWD2R_RAND_TRACE");
        if (on == nullptr || *on == '\0' || std::string(on) == "0") {
            return nullptr;
        }
        const char* dir = std::getenv("IWD2R_STATE_DUMP");
        std::string path = dir != nullptr && *dir != '\0' ? std::string(dir) + "/rand.txt" : "rand.txt";
        return std::fopen(path.c_str(), "w");
    }();
    return file;
}

void Trace(void* caller)
{
    FILE* file = TraceFile();
    if (file == nullptr) {
        return;
    }
    unsigned long long where = reinterpret_cast<unsigned long long>(caller);
#if !defined(_MSC_VER)
    // An offset into the executable, for addr2line.
    Dl_info info {};
    if (dladdr(caller, &info) != 0 && info.dli_fbase != nullptr) {
        where -= reinterpret_cast<unsigned long long>(info.dli_fbase);
    }
#endif
    std::fprintf(file, "%u %p %u %llx\n", g_traceFrame.load(), static_cast<const void*>(&g_state), g_state,
        where);
    std::fflush(file);
}
} // namespace

int iwd2r_rand()
{
    g_state = g_state * 214013u + 2531011u;
#if defined(_MSC_VER)
    Trace(_ReturnAddress());
#else
    Trace(__builtin_return_address(0));
#endif
    return static_cast<int>((g_state >> 16) & 0x7FFFu);
}

void iwd2r_srand(unsigned int seed)
{
    g_state = seed;
}

unsigned int iwd2r_rand_state()
{
    return g_state;
}

const unsigned int* iwd2r_rand_state_address()
{
    return &g_state;
}

void iwd2r_rand_trace_frame(unsigned int frame)
{
    g_traceFrame.store(frame);
}
