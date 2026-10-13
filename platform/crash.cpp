#include "crash.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace platform::crash {

namespace {

#ifdef _WIN32

    LONG WINAPI Filter(EXCEPTION_POINTERS* info)
    {
        std::fprintf(stderr, "crash: exception 0x%08lX at %p\n", static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode),
            info->ExceptionRecord->ExceptionAddress);
        std::fflush(stderr);
        return EXCEPTION_CONTINUE_SEARCH;
    }

#else

    // Async-signal-safe output helpers: write(2) only.
    void WriteStr(const char* s)
    {
        size_t n = std::strlen(s);
        while (n > 0) {
            const ssize_t w = ::write(2, s, n);
            if (w <= 0)
                return;
            s += w;
            n -= static_cast<size_t>(w);
        }
    }

    void WriteHex(unsigned long long v)
    {
        char buf[19];
        int i = 18;
        buf[i] = '\0';
        do {
            buf[--i] = "0123456789abcdef"[v & 15];
            v >>= 4;
        } while (v != 0 && i > 2);
        buf[--i] = 'x';
        buf[--i] = '0';
        WriteStr(buf + i);
    }

    const char* SignalName(int sig)
    {
        switch (sig) {
        case SIGSEGV:
            return "SIGSEGV";
        case SIGABRT:
            return "SIGABRT";
        case SIGFPE:
            return "SIGFPE";
        case SIGILL:
            return "SIGILL";
        case SIGBUS:
            return "SIGBUS";
        default:
            return "signal";
        }
    }

    void Handler(int sig, siginfo_t* info, void*)
    {
        WriteStr("crash: ");
        WriteStr(SignalName(sig));
        WriteStr(" fault address ");
        WriteHex(static_cast<unsigned long long>(reinterpret_cast<unsigned long>(info != nullptr ? info->si_addr : nullptr)));
        WriteStr("\ncrash: backtrace:\n");
        void* frames[64];
        const int n = ::backtrace(frames, 64);
        ::backtrace_symbols_fd(frames, n, 2);
        WriteStr("crash: end of backtrace\n");
        // SA_RESETHAND already restored the default action; re-raise so the exit
        // status is the signal's own (SIGSEGV reads as -11).
        ::signal(sig, SIG_DFL);
        ::raise(sig);
    }

#endif

}

void Install()
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(Filter);
#else
    // backtrace() loads libgcc lazily (malloc); do it now, not in the handler.
    void* warm[2];
    (void)::backtrace(warm, 2);
    // An alternate stack so a stack overflow can still report.
    static char altStack[64 * 1024];
    stack_t ss = {};
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    sigaltstack(&ss, nullptr);
    struct sigaction sa = {};
    sa.sa_sigaction = Handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    const int signals[] = { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS };
    for (const int sig : signals)
        sigaction(sig, &sa, nullptr);
#endif
}

void MaybeTestCrash()
{
    static int calls = 0;
    const char* env = std::getenv("IWD2R_CRASH_TEST");
    if (env == nullptr || std::strcmp(env, "1") != 0)
        return;
    if (++calls == 2) {
        volatile int* p = nullptr;
        *p = 1;
    }
}

void FormatMessageBoxLine(const char* caption, const char* text, char* out, unsigned size)
{
    std::snprintf(out, size, "MessageBoxA: %s: %s\n", caption != nullptr ? caption : "", text != nullptr ? text : "");
}

}
