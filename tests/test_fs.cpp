// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/path_hook.h"
#include "platform/fs.h"
#include "platform/win32_api.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace {

namespace stdfs = std::filesystem;

void Touch(const stdfs::path& path, const char* content = "x")
{
    stdfs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << content;
}

bool EndsWith(const std::string& s, const std::string& suffix)
{
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The engine runs with the data root as its working directory; relative paths rely on it.
struct CwdGuard {
    stdfs::path saved = stdfs::current_path();
    explicit CwdGuard(const stdfs::path& to) { stdfs::current_path(to); }
    ~CwdGuard() { stdfs::current_path(saved); }
};

struct TempTree {
    stdfs::path root;
    bool caseSensitive = true;

    TempTree()
    {
        static int counter = 0;
        root = stdfs::temp_directory_path() / ("iwd2r_fs_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(counter++));
        stdfs::create_directories(root / "probe");
        Touch(root / "probe" / "SW1H01.ITM");
        Touch(root / "probe" / "sw1h01.itm");
        size_t entries = 0;
        for (const auto& e : stdfs::directory_iterator(root / "probe")) {
            (void)e;
            ++entries;
        }
        caseSensitive = entries == 2;
        stdfs::remove_all(root / "probe");
    }

    ~TempTree()
    {
        platform::fs::SetDataRoot("");
        platform::fs::SetInstallPrefix("");
        std::error_code ec;
        stdfs::remove_all(root, ec);
    }

    std::string str() const { return root.generic_string(); }
};

}

TEST_CASE("fs resolve prefers exact case then the only case-insensitive match", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Override" / "SW1H01.ITM");
    if (tree.caseSensitive)
        Touch(tree.root / "Override" / "sw1h01.itm");

    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    if (tree.caseSensitive)
        CHECK(EndsWith(platform::fs::Resolve("override/sw1h01.itm"), "Override/sw1h01.itm"));
    CHECK(EndsWith(platform::fs::Resolve("override/SW1H01.itm"), "Override/SW1H01.ITM"));

    TempTree other;
    Touch(other.root / "Override" / "SW1H01.ITM");
    platform::fs::SetDataRoot(other.str());
    platform::fs::InvalidateCache();
    CHECK(EndsWith(platform::fs::Resolve("override/sw1h01.itm"), "Override/SW1H01.ITM"));
    CHECK(EndsWith(platform::fs::Resolve("OVERRIDE\\sw1h01.itm"), "Override/SW1H01.ITM"));
}

TEST_CASE("fs resolve picks the smallest case-insensitive match when no exact one exists", "[fs]")
{
    TempTree tree;
    if (!tree.caseSensitive)
        SKIP("case-insensitive filesystem");
    Touch(tree.root / "d" / "B.TXT");
    Touch(tree.root / "d" / "b.Txt");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    CHECK(EndsWith(platform::fs::Resolve("d/b.txt"), "d/B.TXT"));
}

TEST_CASE("fs resolve re-roots drive letter paths", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();

    platform::fs::SetInstallPrefix("C:\\Games\\IWD2\\");
    CHECK(EndsWith(platform::fs::Resolve("C:\\Games\\IWD2\\Data\\a.bif"), "Data/a.bif"));
    CHECK(platform::fs::Resolve("c:\\games\\iwd2\\data\\A.BIF") == tree.str() + "/Data/a.bif");
    CHECK(platform::fs::Resolve("D:\\Backup\\save.gam") == tree.str() + "/Backup/save.gam");
    CHECK(platform::fs::Resolve("D:save.gam") == tree.str() + "/save.gam");

    platform::fs::SetInstallPrefix("");
    CHECK(platform::fs::Resolve("C:\\Games\\IWD2\\Data\\a.bif") == tree.str() + "/Games/IWD2/Data/a.bif");
}

TEST_CASE("fs resolve keeps missing components, relative and absolute paths", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    CHECK(platform::fs::Resolve("missing/dir/x.y") == "missing/dir/x.y");
    CHECK(platform::fs::Resolve("data/Missing/x.y") == "Data/Missing/x.y");
    CHECK(platform::fs::Resolve((tree.str() + "/data/A.bif").c_str()) == tree.str() + "/Data/a.bif");

    platform::fs::SetDataRoot("");
    CHECK(platform::fs::Resolve("Some\\Path\\File.ITM") == "Some/Path/File.ITM");
    CHECK(platform::fs::Resolve("C:\\Some\\File") == "C:/Some/File");
}

TEST_CASE("fs wildcard match", "[fs]")
{
    CHECK(platform::fs::WildcardMatch("*.bif", "b.BIF"));
    CHECK(platform::fs::WildcardMatch("sw?h01.itm", "SW1H01.ITM"));
    CHECK_FALSE(platform::fs::WildcardMatch("*.chr", "a.cre"));
    CHECK_FALSE(platform::fs::WildcardMatch("*.bif", "a.bifx"));
    CHECK(platform::fs::WildcardMatch("*", ""));
    CHECK(platform::fs::WildcardMatch("a*b*c", "aXXbYYc"));
    CHECK_FALSE(platform::fs::WildcardMatch("a?", "a"));
    CHECK(platform::fs::WildcardMatch("Exact.Txt", "exact.txt"));
    CHECK_FALSE(platform::fs::WildcardMatch("exact", "exact.txt"));
}

TEST_CASE("fs find enumerates matching entries", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif", "12345");
    Touch(tree.root / "Data" / "b.BIF");
    Touch(tree.root / "Data" / "c.txt");
    stdfs::create_directories(tree.root / "Data" / "sub.bif");

    WIN32_FIND_DATAA data {};
    const std::string pattern = tree.str() + "/Data/*.bif";
    HANDLE h = FindFirstFileA(pattern.c_str(), &data);
    REQUIRE(h != INVALID_HANDLE_VALUE);
    std::set<std::string> names;
    int count = 1;
    names.insert(data.cFileName);
    if (std::string(data.cFileName) == "a.bif") {
        CHECK(data.nFileSizeLow == 5);
        CHECK(data.nFileSizeHigh == 0);
        CHECK((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
        CHECK((data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) != 0);
        CHECK((data.ftLastWriteTime.dwHighDateTime | data.ftLastWriteTime.dwLowDateTime) != 0);
    }
    while (FindNextFileA(h, &data)) {
        names.insert(data.cFileName);
        ++count;
    }
    CHECK(FindNextFileA(h, &data) == FALSE);
    CHECK(FindClose(h) == TRUE);
    CHECK(count == 3);
    CHECK(names == std::set<std::string> { "a.bif", "b.BIF", "sub.bif" });

    CHECK(FindFirstFileA((tree.str() + "/Data/*.chr").c_str(), &data) == INVALID_HANDLE_VALUE);
    CHECK(FindFirstFileA((tree.str() + "/Nope/*.bif").c_str(), &data) == INVALID_HANDLE_VALUE);

    platform::fs::SetDataRoot(tree.str());
    CwdGuard cwd(tree.root);
    h = FindFirstFileA("data\\*.TXT", &data);
    REQUIRE(h != INVALID_HANDLE_VALUE);
    CHECK(std::string(data.cFileName) == "c.txt");
    FindClose(h);
}

TEST_CASE("fs attributes copy move delete and directories", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();

    CwdGuard cwd(tree.root);
    const std::string dir = tree.str() + "/Data";
    CHECK((GetFileAttributesA(dir.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0);
    CHECK(GetFileAttributesA((tree.str() + "/nothing").c_str()) == INVALID_FILE_ATTRIBUTES);
    CHECK(GetFileAttributesA((dir + "/a.bif").c_str()) == FILE_ATTRIBUTE_NORMAL);
    CHECK((GetFileAttributesA("data/A.BIF") & FILE_ATTRIBUTE_DIRECTORY) == 0);

    const std::string src = dir + "/a.bif";
    const std::string dst = dir + "/copy.bif";
    CHECK(CopyFileA(src.c_str(), dst.c_str(), TRUE) == TRUE);
    CHECK(CopyFileA(src.c_str(), dst.c_str(), TRUE) == FALSE);
    CHECK(CopyFileA(src.c_str(), dst.c_str(), FALSE) == TRUE);
    CHECK(GetFileAttributesA(dst.c_str()) != INVALID_FILE_ATTRIBUTES);

    const std::string moved = dir + "/moved.bif";
    CHECK(MoveFileA(dst.c_str(), moved.c_str()) == TRUE);
    CHECK(GetFileAttributesA(dst.c_str()) == INVALID_FILE_ATTRIBUTES);
    CHECK(DeleteFileA(moved.c_str()) == TRUE);
    CHECK(DeleteFileA(moved.c_str()) == FALSE);

    const std::string made = tree.str() + "/Saves";
    CHECK(CreateDirectoryA(made.c_str(), nullptr) == TRUE);
    CHECK(CreateDirectoryA(made.c_str(), nullptr) == FALSE);
    CHECK(stdfs::is_directory(tree.root / "Saves"));
}

TEST_CASE("fs cache sees entries created through the shim", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    CHECK(platform::fs::Resolve("data/NEW.BIF") == "Data/NEW.BIF");
    const std::string src = tree.str() + "/Data/a.bif";
    const std::string dst = tree.str() + "/Data/New.Bif";
    REQUIRE(CopyFileA(src.c_str(), dst.c_str(), TRUE) == TRUE);
    CHECK(platform::fs::Resolve("data/NEW.BIF") == "Data/New.Bif");
}

TEST_CASE("fs disk space and current directory", "[fs]")
{
    TempTree tree;
    DWORD spc = 0, bps = 0, freeClusters = 0, totalClusters = 0;
    CHECK(GetDiskFreeSpaceA(tree.str().c_str(), &spc, &bps, &freeClusters, &totalClusters) == TRUE);
    CHECK(spc == 8);
    CHECK(bps == 512);
    CHECK(totalClusters > 0);
    CHECK(freeClusters > 0);

    const stdfs::path before = stdfs::current_path();
    CHECK(SetCurrentDirectoryA(tree.str().c_str()) == TRUE);
    char buffer[1024];
    const DWORD length = GetCurrentDirectoryA(sizeof(buffer), buffer);
    CHECK(length == std::string(buffer).size());
    CHECK(stdfs::equivalent(buffer, tree.root));
    char tiny[2];
    CHECK(GetCurrentDirectoryA(sizeof(tiny), tiny) == length + 1);
    CHECK(SetCurrentDirectoryA((tree.str() + "/nonexistent").c_str()) == FALSE);
    stdfs::current_path(before);
}

TEST_CASE("fs drive stubs and module path", "[fs]")
{
    CHECK(GetLogicalDrives() == 0);
    CHECK(GetDriveTypeA("C:\\") == DRIVE_NO_ROOT_DIR);
    CHECK(GetVolumeInformationA("C:\\", nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0) == FALSE);
    char buffer[1024];
    const DWORD n = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
#ifdef __linux__
    CHECK(n > 0);
    CHECK(n == std::string(buffer).size());
#else
    CHECK(n == std::string(buffer).size());
#endif
}

TEST_CASE("fs installed resolver is used by the compat hook", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Override" / "X.ITM");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    platform::fs::InstallResolver();
    CHECK(compat::ResolvePath("override\\x.itm") == "Override/X.ITM");
    compat::SetPathResolver(nullptr);
}

TEST_CASE("fs cache is invalidated for relative paths", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    CwdGuard cwd(tree.root);
    CHECK(platform::fs::Resolve("data/NEW.BIF") == "Data/NEW.BIF");
    REQUIRE(CopyFileA("Data/a.bif", "Data/New.Bif", TRUE) == TRUE);
    CHECK(platform::fs::Resolve("data/NEW.BIF") == "Data/New.Bif");

    CHECK(platform::fs::Resolve("top.txt") == "top.txt");
    Touch(tree.root / "Top.txt");
    platform::fs::InvalidateCache();
    CHECK(platform::fs::Resolve("TOP.TXT") == "Top.txt");
    REQUIRE(DeleteFileA("Top.txt") == TRUE);
    CHECK(platform::fs::Resolve("top.txt") == "top.txt");
}

TEST_CASE("fs rescans after a miss for files created outside the shim", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Save" / "other.gam");
    platform::fs::SetDataRoot(tree.str());
    CHECK(platform::fs::Resolve("save/SAVE.GAM") == "Save/SAVE.GAM");
    Touch(tree.root / "Save" / "Save.gam");
    CHECK(platform::fs::Resolve("save/SAVE.GAM") == "Save/Save.gam");
}
