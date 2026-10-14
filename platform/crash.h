#pragma once

namespace platform::crash {

// Installs the fatal-signal handlers (Linux: SIGSEGV, SIGABRT, SIGFPE, SIGILL,
// SIGBUS; Windows: an unhandled-exception filter). The report goes to stderr;
// the process then dies as it would have without the handler.
void Install();

// The environment variable whose value starts every line the handlers
// write: the GPU probe's child runs with IWD2R_CRASH_PREFIX=probe-gpu, so
// its crash reads "probe-gpu crash: ..." in the game's log.
inline constexpr char kPrefixVariable[] = "IWD2R_CRASH_PREFIX";

// What the handlers' lines start with: "<prefix> crash: ", or "crash: " for
// a null or empty prefix (truncated to size).
void FormatCrashPrefix(const char* prefix, char* out, unsigned size);

// IWD2R_CRASH_TEST=1: dereferences null once, on the second call (the call
// after the first frame). A no-op otherwise.
void MaybeTestCrash();

// The text MessageBoxA writes to stderr: "MessageBoxA: <caption>: <text>\n".
// Null strings read as empty.
void FormatMessageBoxLine(const char* caption, const char* text, char* out, unsigned size);

}
