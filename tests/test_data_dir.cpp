#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/path_hook.h"
#include "compat/win_types.h"
#include "platform/data_dir.h"
#include "platform/fs.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

namespace stdfs = std::filesystem;

void SetEnv(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value)
        setenv(name, value, 1);
    else
        unsetenv(name);
#endif
}

// Restores IWD2_DATA and the current directory, even when an assertion throws.
struct EnvCwdGuard {
    bool hadEnv;
    std::string savedEnv;
    stdfs::path savedCwd = stdfs::current_path();
    std::string savedRoot = platform::fs::DataRoot();
    EnvCwdGuard()
    {
        const char* v = std::getenv("IWD2_DATA");
        hadEnv = v != nullptr;
        if (v)
            savedEnv = v;
        SetEnv("IWD2_DATA", nullptr);
    }
    ~EnvCwdGuard()
    {
        std::error_code ec;
        stdfs::current_path(savedCwd, ec);
        compat::SetPathResolver(nullptr);
        platform::fs::SetDataRoot(savedRoot);
        SetEnv("IWD2_DATA", hadEnv ? savedEnv.c_str() : nullptr);
    }
};

struct TempDir {
    stdfs::path root;
    TempDir()
    {
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = stdfs::temp_directory_path() / ("iwd2r_data_dir_" + std::to_string(stamp));
        stdfs::create_directories(root);
    }
    ~TempDir()
    {
        std::error_code ec;
        stdfs::remove_all(root, ec);
    }
    stdfs::path Make(const char* name, const char* key = nullptr)
    {
        stdfs::path dir = root / name;
        stdfs::create_directories(dir);
        if (key)
            std::ofstream(dir / key) << "x";
        return dir;
    }
};

std::string g_fatalText;
std::string g_fatalCaption;
int g_fatalCalls = 0;
void TestFatal(const char* text, const char* caption)
{
    g_fatalText = text;
    g_fatalCaption = caption;
    ++g_fatalCalls;
}

}

TEST_CASE("data_dir env var with chitin.key resolves as env", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto data = tmp.Make("data", "chitin.key");
    SetEnv("IWD2_DATA", data.string().c_str());
    auto r = platform::FindDataDir(nullptr, nullptr);
    REQUIRE(r.has_value());
    CHECK(r->source == "env");
    CHECK(r->path == data.string());
}

TEST_CASE("data_dir empty env var is skipped", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto empty = tmp.Make("empty");
    stdfs::current_path(empty);
    SetEnv("IWD2_DATA", "");
    CHECK_FALSE(platform::FindDataDir(nullptr, nullptr).has_value());
}

TEST_CASE("data_dir env without key is skipped and exe symlink wins", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto bad = tmp.Make("bad");
    auto real = tmp.Make("real", "CHITIN.KEY");
    auto exeDir = tmp.Make("exe");
    std::error_code ec;
    stdfs::create_directory_symlink(real, exeDir / "IWD2", ec);
    if (ec == std::errc::operation_not_permitted || ec == std::errc::permission_denied)
        SKIP("cannot create symlinks here");
    REQUIRE_FALSE(ec);
    SetEnv("IWD2_DATA", bad.string().c_str());
    auto r = platform::FindDataDir(exeDir.string().c_str(), nullptr);
    REQUIRE(r.has_value());
    CHECK(r->source == "exe-link");
    CHECK(r->path == (exeDir / "IWD2").string());
}

TEST_CASE("data_dir repo hint and cwd candidates", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto empty = tmp.Make("empty");
    auto repo = tmp.Make("repo");
    tmp.Make("repo/IWD2", "Chitin.Key");
    stdfs::current_path(empty);
    auto r = platform::FindDataDir("", repo.string().c_str());
    REQUIRE(r.has_value());
    CHECK(r->source == "repo-link");

    auto cwdDir = tmp.Make("cwd", "chitin.key");
    stdfs::current_path(cwdDir);
    auto c = platform::FindDataDir(nullptr, nullptr);
    REQUIRE(c.has_value());
    CHECK(c->source == "cwd");
}

TEST_CASE("data_dir nothing valid yields empty", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto empty = tmp.Make("empty");
    auto exeDir = tmp.Make("exe");
    tmp.Make("exe/IWD2");
    stdfs::current_path(empty);
    CHECK_FALSE(platform::FindDataDir(exeDir.string().c_str(), exeDir.string().c_str()).has_value());
}

TEST_CASE("data_dir ResolveDataDirFrom failure reports through handler", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto empty = tmp.Make("empty");
    stdfs::current_path(empty);
    g_fatalCalls = 0;
    platform::SetFatalMessageHandler(TestFatal);
    std::string rootBefore = platform::fs::DataRoot();
    bool ok = platform::ResolveDataDirFrom("", "");
    platform::SetFatalMessageHandler(nullptr);
    CHECK_FALSE(ok);
    CHECK(g_fatalCalls == 1);
    CHECK(g_fatalText == "Set IWD2_DATA to your Icewind Dale II install directory (the one containing CHITIN.KEY).");
    CHECK(g_fatalCaption == "iwd2r");
    CHECK(platform::fs::DataRoot() == rootBefore);
    CHECK(stdfs::equivalent(stdfs::current_path(), empty));
}

TEST_CASE("data_dir ResolveDataDirFrom success configures the engine", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto data = tmp.Make("data", "chitin.key");
    auto elsewhere = tmp.Make("elsewhere");
    stdfs::current_path(elsewhere);
    SetEnv("IWD2_DATA", data.string().c_str());
    bool ok = platform::ResolveDataDirFrom("", "");
    REQUIRE(ok);
    CHECK(std::string(platform::DataDirSource()) == "env");
    CHECK(platform::fs::DataRoot() == data.generic_string());
    CHECK(stdfs::equivalent(stdfs::current_path(), data));
    // Case-insensitive (Windows, macOS's default APFS): the resolver keeps
    // the spelling it was given.
    if (stdfs::exists(data / "CHITIN.KEY"))
        CHECK(compat::ResolvePath("CHITIN.KEY") == "CHITIN.KEY");
    else
        CHECK(compat::ResolvePath("CHITIN.KEY") == "chitin.key");
}

TEST_CASE("data_dir ReportFatal goes through the installed handler", "[data_dir]")
{
    g_fatalCalls = 0;
    platform::SetFatalMessageHandler(TestFatal);
    platform::ReportFatal("text", "caption");
    platform::SetFatalMessageHandler(nullptr);
    CHECK(g_fatalCalls == 1);
    CHECK(g_fatalText == "text");
    CHECK(g_fatalCaption == "caption");
}

// CDimm::CDScan (_chdir) and CDimm::WriteSetUp (_chdir, _mkdir) take the
// INI's [Alias] paths, which name the folder the game was installed to: on
// every platform they reach the data directory through the resolver, not
// the CRT as they are (on Windows a copy elsewhere quit at the world).
TEST_CASE("data_dir: _chdir, _mkdir and _access re-root alias paths", "[data_dir]")
{
    EnvCwdGuard guard;
    TempDir tmp;
    auto data = tmp.Make("data", "chitin.key");
    stdfs::create_directories(data / "CD2");
    platform::fs::SetDataRoot(data.string());
    platform::fs::SetInstallPrefix("C:\\Nowhere\\Icewind Dale II\\");
    platform::fs::InstallResolver();

    CHECK(_chdir("C:\\Nowhere\\Icewind Dale II\\CD2\\") == 0);
    CHECK(stdfs::equivalent(stdfs::current_path(), data / "CD2"));
    CHECK(_mkdir("C:\\Nowhere\\Icewind Dale II\\temp") == 0);
    CHECK(stdfs::is_directory(data / "temp"));
    CHECK(_access("C:\\Nowhere\\Icewind Dale II\\temp", 0) == 0);
    CHECK(_access("C:\\Nowhere\\Icewind Dale II\\missing", 0) != 0);

    platform::fs::SetInstallPrefix("");
}
