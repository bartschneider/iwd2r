#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "data_view.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path TempFolder()
{
    const fs::path dir = fs::temp_directory_path() / ("iwd2r-dataview-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    return dir;
}

void Write(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::string Read(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// A folder link: a symlink elsewhere, a junction on a Windows drive
// (tests/data_view.cpp LinkEntry; std::filesystem does not report a
// junction as a symlink).
bool IsDirLink(const fs::path& path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    return fs::is_symlink(path);
#endif
}

// A file link: a symlink elsewhere, a hard link to the install's file on a
// Windows drive (LinkEntry copies only when no hard link can be made).
bool IsFileLink(const fs::path& path)
{
#ifdef _WIN32
    std::error_code ec;
    return fs::hard_link_count(path, ec) > 1;
#else
    return fs::is_symlink(path);
#endif
}

}

TEST_CASE("the shared rules file lists the engine's writable entries", "[dataview]")
{
    const DataViewRules rules = LoadDataViewRules(fs::path(IWD2R_SOURCE_DIR) / "tools" / "dataview_rules.json");
    CHECK(rules.emptyDirs == std::vector<std::string> { "temp", "tempsave", "cache", "save", "ScrnShot" });
    CHECK(rules.copiedDirs == std::vector<std::string> { "mpsave", "Characters" });
    CHECK(rules.copiedFiles == std::vector<std::string> { "*.ini" });
}

TEST_CASE("data view globs ignore case", "[dataview]")
{
    CHECK(DataViewGlob("*.ini", "Icewind2.INI"));
    CHECK(DataViewGlob("*.ini", ".ini"));
    CHECK_FALSE(DataViewGlob("*.ini", "icewind2.ini.bak"));
    CHECK(DataViewGlob("a?c*", "ABCdef"));
    CHECK_FALSE(DataViewGlob("a?c", "ac"));
}

TEST_CASE("a data view links the install and keeps writes private", "[dataview]")
{
    const fs::path tmp = TempFolder();
    const fs::path data = tmp / "install";
    Write(data / "CHITIN.KEY", "key");
    Write(data / "Data" / "a.bif", "bif");
    Write(data / "icewind2.ini", "a=1");
    Write(data / "Keymap.INI", "k=1");
    Write(data / "Temp" / "stale", "x");
    Write(data / "cache" / "c.bif", "x");
    Write(data / "mpsave" / "default" / "s.sav", "save");
    Write(data / "Characters" / "A.CHR", "chr");
    const fs::path view = tmp / "view";
    const DataViewRules rules = LoadDataViewRules(fs::path(IWD2R_SOURCE_DIR) / "tools" / "dataview_rules.json");
    MakeDataView(data, view, rules);

    CHECK(IsFileLink(view / "CHITIN.KEY"));
    CHECK(IsDirLink(view / "Data"));
    for (const char* name : { "Temp", "cache", "tempsave", "save", "ScrnShot" }) {
        INFO(name);
        CHECK(fs::is_directory(view / name));
        CHECK_FALSE(IsDirLink(view / name));
        CHECK(fs::is_empty(view / name));
    }
    CHECK_FALSE(IsFileLink(view / "icewind2.ini"));
    CHECK(Read(view / "icewind2.ini") == "a=1");
    CHECK_FALSE(IsFileLink(view / "Keymap.INI"));
    CHECK_FALSE(IsDirLink(view / "mpsave"));
    CHECK(Read(view / "mpsave" / "default" / "s.sav") == "save");
    CHECK_FALSE(IsDirLink(view / "Characters"));
    CHECK(Read(view / "Characters" / "A.CHR") == "chr");

    // Writes stay in the view.
    Write(view / "icewind2.ini", "changed");
    Write(view / "temp" / "t", "t");
    fs::remove(view / "mpsave" / "default" / "s.sav");
    CHECK(Read(data / "icewind2.ini") == "a=1");
    CHECK(fs::exists(data / "mpsave" / "default" / "s.sav"));
    CHECK(Read(data / "Temp" / "stale") == "x");

    // Idempotent: a second call keeps what the view holds.
    MakeDataView(data, view, rules);
    CHECK(Read(view / "icewind2.ini") == "changed");

    // Refuses a view inside the install, and an install inside the view.
    CHECK_THROWS(MakeDataView(data, data / "view", rules));
    CHECK_THROWS(MakeDataView(data, tmp, rules));

    RemoveDataView(view);
    CHECK_FALSE(fs::exists(view));
    CHECK(Read(data / "Data" / "a.bif") == "bif");
    CHECK(Read(data / "cache" / "c.bif") == "x");
    CHECK(Read(data / "CHITIN.KEY") == "key");
    std::error_code ec;
    fs::remove_all(tmp, ec);
}

TEST_CASE("DataView is a temporary view removed with it", "[dataview]")
{
    const fs::path tmp = TempFolder();
    Write(tmp / "install" / "CHITIN.KEY", "key");
    fs::path path;
    {
        DataView view((tmp / "install").string());
        path = view.Path();
        CHECK(fs::exists(fs::path(path) / "CHITIN.KEY"));
        CHECK(fs::is_directory(fs::path(path) / "temp"));
    }
    CHECK_FALSE(fs::exists(path));
    CHECK(fs::exists(tmp / "install" / "CHITIN.KEY"));
    std::error_code ec;
    fs::remove_all(tmp, ec);
}

TEST_CASE("a data view without saves has empty save folders", "[dataview]")
{
    const fs::path tmp = TempFolder();
    const fs::path data = tmp / "install";
    Write(data / "CHITIN.KEY", "key");
    Write(data / "MPSave" / "000000002-Quick-Save" / "ICEWIND2.GAM", "GAMEV2.2");
    Write(data / "save" / "s" / "ICEWIND2.GAM", "GAMEV2.2");
    Write(data / "Characters" / "A.CHR", "chr");
    const DataViewRules rules = WithoutSaves(LoadDataViewRules(fs::path(IWD2R_SOURCE_DIR) / "tools" / "dataview_rules.json"));
    CHECK(rules.copiedDirs == std::vector<std::string> { "Characters" });
    const fs::path view = tmp / "view";
    MakeDataView(data, view, rules);
    for (const char* name : { "MPSave", "save", "tempsave" }) {
        INFO(name);
        CHECK(fs::is_directory(view / name));
        CHECK_FALSE(IsDirLink(view / name));
        CHECK(fs::is_empty(view / name));
    }
    CHECK(Read(view / "Characters" / "A.CHR") == "chr");
    RemoveDataView(view);
    CHECK(Read(data / "MPSave" / "000000002-Quick-Save" / "ICEWIND2.GAM") == "GAMEV2.2");

    // The DataView form.
    {
        DataView saveless(data.string(), DataView::Saves::None);
        CHECK(fs::is_empty(fs::path(saveless.Path()) / "MPSave"));
    }
    std::error_code ec;
    fs::remove_all(tmp, ec);
}
