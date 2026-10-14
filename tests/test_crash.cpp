#define NOMINMAX
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "data_view.h"
#include "platform/crash.h"
#include "platform/win32_api.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef IWD2R_EXE
#error IWD2R_EXE must be defined by tests/CMakeLists.txt
#endif

TEST_CASE("MessageBoxA stderr line is caption then text", "[crash]")
{
    char line[256];
    platform::crash::FormatMessageBoxLine("iwd2r", "Assertion failed: x", line, sizeof(line));
    CHECK(std::string(line) == "MessageBoxA: iwd2r: Assertion failed: x\n");
    platform::crash::FormatMessageBoxLine(nullptr, nullptr, line, sizeof(line));
    CHECK(std::string(line) == "MessageBoxA: : \n");
}

TEST_CASE("crash lines start with the prefix the environment gives", "[crash]")
{
    char line[64];
    platform::crash::FormatCrashPrefix("probe-gpu", line, sizeof(line));
    CHECK(std::string(line) == "probe-gpu crash: ");
    platform::crash::FormatCrashPrefix("", line, sizeof(line));
    CHECK(std::string(line) == "crash: ");
    platform::crash::FormatCrashPrefix(nullptr, line, sizeof(line));
    CHECK(std::string(line) == "crash: ");
    CHECK(std::string(platform::crash::kPrefixVariable) == "IWD2R_CRASH_PREFIX");
}

TEST_CASE("MessageBoxA under a script returns the default button without a window", "[crash]")
{
#ifdef _WIN32
    _putenv_s("IWD2R_SCRIPT", "x.txt");
#else
    setenv("IWD2R_SCRIPT", "x.txt", 1);
#endif
    CHECK(MessageBoxA(NULL, "Are you sure?", "iwd2r", MB_YESNO) == IDYES);
    CHECK(MessageBoxA(NULL, "Sure.", "iwd2r", MB_OK) == IDOK);
#ifdef _WIN32
    _putenv_s("IWD2R_SCRIPT", "");
#else
    unsetenv("IWD2R_SCRIPT");
#endif
}

#ifndef _WIN32
TEST_CASE("crash handler reports signal and backtrace", "[crash][data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    const std::string data = env;
    // The probe child's prefix (IWD2R_CRASH_PREFIX, which gpu_probe::Run
    // sets) starts every line; without it they start with "crash: ".
    const bool prefixed = GENERATE(false, true);
    INFO("prefixed " << prefixed);
    const std::string out = (std::filesystem::temp_directory_path() / ("iwd2r-crash-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")).string();
    int raw = 0;
    {
        DataView view(data);
        const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_AUDIO=null IWD2R_CRASH_TEST=1 IWD2R_SMOKE_FRAMES=300 "
            + std::string(prefixed ? "IWD2R_CRASH_PREFIX=probe-gpu " : "") + "IWD2_DATA=\""
            + view.Path() + "\" timeout 120 \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
        raw = std::system(full.c_str());
    }
    std::ifstream in(out);
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    std::remove(out.c_str());
    const std::string text = ss.str();
    INFO("output:\n"
        << text);
    // Re-raised with the default action: the status stays the signal's. The
    // shell and coreutils timeout between us and the engine report it as 128+11.
    const bool segv = (WIFSIGNALED(raw) && WTERMSIG(raw) == SIGSEGV) || (WIFEXITED(raw) && WEXITSTATUS(raw) == 128 + SIGSEGV);
    CHECK(segv);
    CHECK(text.find("SIGSEGV") != std::string::npos);
    CHECK(text.find("backtrace") != std::string::npos);
    CHECK(text.find("end of backtrace") != std::string::npos);
    const std::string prefix = prefixed ? "probe-gpu crash: " : "crash: ";
    CHECK(text.find(prefix + "SIGSEGV") != std::string::npos);
    CHECK(text.find(prefix + "end of backtrace") != std::string::npos);
    if (!prefixed) {
        CHECK(text.find("probe-gpu crash") == std::string::npos);
    }
}
#endif
