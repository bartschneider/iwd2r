#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "ScreenLayout.h"
#include "compat/ini.h"
#include "data_view.h"

#include <cctype>
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

namespace {

// iwd2r::ReadScreenSize as the headless child sees it (the dummy video
// driver reports a 1024x768 desktop).
iwd2r::ScreenSize ResolveFromIni(const std::string& ini)
{
    char resolution[64];
    char height[64];
    char scale[64];
    GetPrivateProfileStringA("Program Options", "Resolution", "800", resolution, sizeof(resolution), ini.c_str());
    GetPrivateProfileStringA("Program Options", "Height", "", height, sizeof(height), ini.c_str());
    GetPrivateProfileStringA("Program Options", "Scale", "1", scale, sizeof(scale), ini.c_str());
    return iwd2r::ResolveScreenSize(iwd2r::ParseResolution(resolution), iwd2r::ParseHeight(height), iwd2r::ParseScale(scale), 1024, 768);
}

// A TZ value under which the local time is 12:00:00 now. The main menu
// (CScreenConnection::UpdateMainPanel) shows its day background START from
// 07:00 to 17:59 local time and the night one STARTN, with an animated
// torch, otherwise, so the frame depends on the hour the test runs. POSIX
// and the MSVC CRT read TZ as "name[+|-]hh[:mm[:ss]]", hours west of UTC.
std::string NoonTimeZone()
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

}

TEST_CASE("start screen smoke", "[data]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    const std::string data = env;

    // The engine refuses a screen below 800x600 (exit code 3). Read the
    // player's INI without modifying it; headless, the desktop is the dummy
    // video driver's 1024x768.
    std::string ini = data;
    if (!ini.empty() && ini.back() != '/' && ini.back() != '\\') {
        ini += '/';
    }
    ini += "icewind2.ini";
    const iwd2r::ScreenSize screen = ResolveFromIni(ini);
    if (!screen.ok) {
        SKIP("[iwd2r-skip] icewind2.ini asks for a screen iwd2r refuses: " << screen.error);
    }

    // The engine writes into its data folder when it runs (cache files, temp/, the INI files), as the original does: it runs on a private view.
    // The view has no saves, so Quick Load is disabled whatever saves earlier runs left in the install.
    DataView view(data, DataView::Saves::None);
    // The local time is noon at the start (day background); the run is far shorter than the six hours to the night background at 18:00.
    const std::string tz = NoonTimeZone();

    const std::string out = (std::filesystem::temp_directory_path() / ("iwd2r-smoke-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")).string();
#ifdef _WIN32
    // No timeout on Windows.
    const std::string cmd = "set SDL_VIDEO_DRIVER=dummy&& set SDL_AUDIO_DRIVER=dummy&& set TZ=" + tz + "&& set IWD2R_SMOKE_FRAMES=300&& set IWD2_DATA=" + view.Path() + "&& \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
    const std::string full = "\"" + cmd + "\"";
#else
    // coreutils timeout bounds the run to 120 seconds.
    const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy TZ=" + tz + " IWD2R_SMOKE_FRAMES=300 IWD2_DATA=\"" + view.Path() + "\" timeout 120 \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
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
    // IWD2R_SMOKE_HASH pins the frame (tools/gates.sh sets it from this
    // machine's smoke-hash file: the hash depends on the game copy's data and
    // INI files, not on the time of day or on saves); unset,
    // the hash is only reported.
    const char* pinned = std::getenv("IWD2R_SMOKE_HASH");
    std::string expected = pinned != nullptr ? pinned : "";
    while (!expected.empty() && std::isspace(static_cast<unsigned char>(expected.back()))) {
        expected.pop_back();
    }
    if (expected.empty()) {
        WARN("framebuffer-hash: " << m[1].str());
    } else {
        REQUIRE(m[1].str() == expected);
    }
}
