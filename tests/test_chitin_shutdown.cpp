#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "data_view.h"

#include <chrono>
#include <cstdio>
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

// CChitin::ShutDown (0x7909C0) waits for the AI thread before the engines
// and, after WinMain, main.cpp's CBaldurChitin go (DEVIATION: the original's
// ExitProcess kills the thread instead). Start the engine headless and let it
// quit a few times: every run must end with code 0 and no crash report.
TEST_CASE("chitin: ShutDown waits for the AI thread", "[data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    DataView view(env, DataView::Saves::None);

    for (int run = 0; run < 3; run++) {
        INFO("run " << run);
        const std::string out = (std::filesystem::temp_directory_path()
            / ("iwd2r-shutdown-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt"))
                                    .string();
#ifdef _WIN32
        const std::string cmd = "set SDL_VIDEO_DRIVER=dummy&& set SDL_AUDIO_DRIVER=dummy&& set IWD2R_SMOKE_FRAMES=60&& set IWD2_DATA="
            + view.Path() + "&& \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
        const int status = std::system(("\"" + cmd + "\"").c_str());
#else
        const std::string cmd = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SMOKE_FRAMES=60 IWD2_DATA=\"" + view.Path()
            + "\" timeout 120 \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
        const int raw = std::system(cmd.c_str());
        const int status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
        std::ifstream in(out);
        std::stringstream ss;
        ss << in.rdbuf();
        in.close();
        std::remove(out.c_str());
        INFO("output:\n"
            << ss.str());
        CHECK(status == 0);
        CHECK(ss.str().find("crash:") == std::string::npos);
    }
}
