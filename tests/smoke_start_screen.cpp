// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/ini.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef IWD2R_EXE
#error IWD2R_EXE must be defined by tests/CMakeLists.txt
#endif

TEST_CASE("start screen smoke", "[data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    const std::string data = env;

    // The engine refuses resolutions other than 800 and 1024 (exit code 3).
    // Read the player's INI without modifying it.
    std::string ini = data;
    if (!ini.empty() && ini.back() != '/' && ini.back() != '\\') {
        ini += '/';
    }
    ini += "icewind2.ini";
    const int resolution = static_cast<int>(GetPrivateProfileIntA("Program Options", "Resolution", 800, ini.c_str()));
    if (resolution != 800 && resolution != 1024) {
        SKIP("[iwd2r-skip] icewind2.ini has Resolution=" << resolution << "; only 800 and 1024 are supported in project 1");
    }

    WARN("The engine writes into the install directory when it runs (cache files, temp/, icewind2.ini rewrites), as the original game does on every launch.");

    const std::string out = (std::filesystem::temp_directory_path() / ("iwd2r-smoke-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")).string();
#ifdef _WIN32
    // No timeout on Windows.
    const std::string cmd = "set SDL_VIDEO_DRIVER=dummy&& set SDL_AUDIO_DRIVER=dummy&& set IWD2R_SMOKE_FRAMES=300&& set IWD2_DATA=" + data + "&& \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
    const std::string full = "\"" + cmd + "\"";
#else
    // coreutils timeout bounds the run to 120 seconds.
    const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SMOKE_FRAMES=300 IWD2_DATA=\"" + data + "\" timeout 120 \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
#endif
    const int raw = std::system(full.c_str());
#ifdef _WIN32
    const int status = raw;
#else
    const int status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif

    std::ifstream in(out);
    std::stringstream ss;
    ss << in.rdbuf();
    in.close();
    std::remove(out.c_str());
    const std::string text = ss.str();

    INFO("output:\n"
        << text);
    // Status 124 means coreutils timeout killed the run.
    REQUIRE(status == 0);
    std::smatch m;
    const std::regex re("framebuffer-hash: ([0-9a-f]{16})(\\r?\\n|$)");
    REQUIRE(std::regex_search(text, m, re));
    WARN("framebuffer-hash: " << m[1].str());
}
