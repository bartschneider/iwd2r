// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "ScreenLayout.h"
#include "compat/ini.h"
#include "install_ini_backup.h"

#include <zlib.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef IWD2R_EXE
#error IWD2R_EXE must be defined by tests/CMakeLists.txt
#endif
#ifndef IWD2R_SOURCE_DIR
#error IWD2R_SOURCE_DIR must be defined by tests/CMakeLists.txt
#endif

namespace {

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint32_t U32(const std::string& d, size_t at)
{
    return (uint32_t(static_cast<unsigned char>(d[at])) << 24) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 16)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 8) | uint32_t(static_cast<unsigned char>(d[at + 3]));
}

}

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

}

TEST_CASE("start screen scenario screenshot", "[data][scenario]")
{
    const char* env = std::getenv("IWD2_DATA");
    if (env == nullptr || *env == '\0') {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    const std::string data = env;

    // The resolution the engine will use: the INI, or IWD2R_INI_OVERRIDE when
    // set (the child inherits it).
    std::string ini = data;
    if (!ini.empty() && ini.back() != '/' && ini.back() != '\\') {
        ini += '/';
    }
    ini += "icewind2.ini";
    // Headless, the desktop is the dummy video driver's 1024x768.
    const iwd2r::ScreenSize screen = ResolveFromIni(ini);
    if (!screen.ok) {
        SKIP("[iwd2r-skip] icewind2.ini asks for a screen iwd2r refuses: " << screen.error);
    }
    const int resolution = screen.width;
    const int height = screen.height;

    const std::filesystem::path shots = std::filesystem::temp_directory_path() / ("iwd2r-scenario-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(shots);
    const std::string script = (std::filesystem::path(IWD2R_SOURCE_DIR) / "scenarios" / "start_screen.txt").string();
    const std::string out = (shots / "log.txt").string();

    int raw = 0;
    {
        InstallIniBackup backup(data);
#ifdef _WIN32
        const std::string cmd = "set SDL_VIDEO_DRIVER=dummy&& set SDL_AUDIO_DRIVER=dummy&& set IWD2R_SCRIPT=" + script + "&& set IWD2R_SHOT_DIR=" + shots.string() + "&& set IWD2_DATA=" + data + "&& \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
        const std::string full = "\"" + cmd + "\"";
#else
        const std::string full = "SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SCRIPT=\"" + script + "\" IWD2R_SHOT_DIR=\"" + shots.string() + "\" IWD2_DATA=\"" + data + "\" timeout 150 \"" IWD2R_EXE "\" > \"" + out + "\" 2>&1";
#endif
        raw = std::system(full.c_str());
    }
#ifdef _WIN32
    const int status = raw;
#else
    const int status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif

    const std::string log = ReadFile(out);
    const std::string png = ReadFile(shots / "start.png");
    std::error_code ec;
    std::filesystem::remove_all(shots, ec);

    INFO("output:\n"
        << log);
    REQUIRE(status == 0);
    REQUIRE(png.size() > 8 + 25 + 12);
    REQUIRE(png.compare(0, 8, std::string("\x89PNG\r\n\x1A\n", 8)) == 0);

    // Chunks: check CRCs, read IHDR, collect IDAT.
    size_t p = 8;
    uint32_t width = 0;
    uint32_t h = 0;
    std::string idat;
    bool ended = false;
    while (p + 12 <= png.size()) {
        const uint32_t n = U32(png, p);
        REQUIRE(p + 12 + n <= png.size());
        const std::string type = png.substr(p + 4, 4);
        const uLong crc = crc32(0L, reinterpret_cast<const Bytef*>(png.data() + p + 4), static_cast<uInt>(n + 4));
        REQUIRE(U32(png, p + 8 + n) == static_cast<uint32_t>(crc));
        if (type == "IHDR") {
            width = U32(png, p + 8);
            h = U32(png, p + 12);
            CHECK(png[p + 16] == 8);
            CHECK(png[p + 17] == 2);
        } else if (type == "IDAT") {
            idat += png.substr(p + 8, n);
        } else if (type == "IEND") {
            ended = true;
        }
        p += 12 + n;
    }
    CHECK(ended);
    REQUIRE(width == static_cast<uint32_t>(resolution));
    REQUIRE(h == static_cast<uint32_t>(height));

    const size_t row = 1 + 3 * static_cast<size_t>(width);
    std::vector<unsigned char> pixels(row * h);
    uLongf size = static_cast<uLongf>(pixels.size());
    REQUIRE(uncompress(pixels.data(), &size, reinterpret_cast<const Bytef*>(idat.data()), static_cast<uLong>(idat.size())) == Z_OK);
    REQUIRE(size == pixels.size());
    bool varied = false;
    for (size_t y = 0; y < h && !varied; y++) {
        for (size_t x = 0; x < 3 * static_cast<size_t>(width) && !varied; x++) {
            varied = pixels[y * row + 1 + x] != pixels[1 + (x % 3)];
        }
    }
    CHECK(varied);
}
