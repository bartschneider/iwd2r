#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_filefind.h"
#include "compat/path_hook.h"
#include "platform/fs.h"
#include "platform/win32_api.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

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

// Whether the temp directory's filesystem tells "A" from "a". Windows and
// macOS (APFS by default) do not.
bool TempIsCaseInsensitive()
{
    static const bool insensitive = [] {
        const stdfs::path probe = stdfs::temp_directory_path() / ("iwd2r_fs_case_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::ofstream(probe, std::ios::binary) << "x";
        std::string upper = probe.filename().string();
        for (char& c : upper)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 'a' + 'A');
        std::error_code ec;
        const bool result = stdfs::exists(probe.parent_path() / upper, ec);
        stdfs::remove(probe, ec);
        return result;
    }();
    return insensitive;
}

// Resolved paths keep the on-disk case on case-sensitive filesystems. On a
// case-insensitive one the resolver keeps the spelling it was given, so only
// the letters are compared there.
std::string CaseKey(std::string s)
{
    if (TempIsCaseInsensitive()) {
        for (char& c : s)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool SamePath(const std::string& actual, const std::string& expected) { return CaseKey(actual) == CaseKey(expected); }

bool EndsWithPath(const std::string& s, const std::string& suffix) { return EndsWith(CaseKey(s), CaseKey(suffix)); }

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
        CHECK(EndsWithPath(platform::fs::Resolve("override/sw1h01.itm"), "Override/sw1h01.itm"));
    CHECK(EndsWithPath(platform::fs::Resolve("override/SW1H01.itm"), "Override/SW1H01.ITM"));

    TempTree other;
    Touch(other.root / "Override" / "SW1H01.ITM");
    platform::fs::SetDataRoot(other.str());
    platform::fs::InvalidateCache();
    CHECK(EndsWithPath(platform::fs::Resolve("override/sw1h01.itm"), "Override/SW1H01.ITM"));
    CHECK(EndsWithPath(platform::fs::Resolve("OVERRIDE\\sw1h01.itm"), "Override/SW1H01.ITM"));
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
    CHECK(EndsWithPath(platform::fs::Resolve("d/b.txt"), "d/B.TXT"));
}

TEST_CASE("fs resolve re-roots drive letter paths", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();

    platform::fs::SetInstallPrefix("C:\\Games\\IWD2\\");
    CHECK(EndsWithPath(platform::fs::Resolve("C:\\Games\\IWD2\\Data\\a.bif"), "Data/a.bif"));
    CHECK(SamePath(platform::fs::Resolve("c:\\games\\iwd2\\data\\A.BIF"), tree.str() + "/Data/a.bif"));
#ifdef _WIN32
    // Outside the install prefix a drive letter is a native path.
    CHECK(platform::fs::Resolve("D:\\Backup\\save.gam") == "D:/Backup/save.gam");
#else
    CHECK(platform::fs::Resolve("D:\\Backup\\save.gam") == tree.str() + "/Backup/save.gam");
    CHECK(platform::fs::Resolve("D:save.gam") == tree.str() + "/save.gam");

    platform::fs::SetInstallPrefix("");
    CHECK(platform::fs::Resolve("C:\\Games\\IWD2\\Data\\a.bif") == tree.str() + "/Games/IWD2/Data/a.bif");
#endif
}

TEST_CASE("fs resolve keeps missing components, relative and absolute paths", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    CHECK(SamePath(platform::fs::Resolve("missing/dir/x.y"), "missing/dir/x.y"));
    CHECK(SamePath(platform::fs::Resolve("data/Missing/x.y"), "Data/Missing/x.y"));
    CHECK(SamePath(platform::fs::Resolve((tree.str() + "/data/A.bif").c_str()), tree.str() + "/Data/a.bif"));

    platform::fs::SetDataRoot("");
    CHECK(SamePath(platform::fs::Resolve("Some\\Path\\File.ITM"), "Some/Path/File.ITM"));
    CHECK(SamePath(platform::fs::Resolve("C:\\Some\\File"), "C:/Some/File"));
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
    // Win32: a trailing ".*" also matches names without an extension.
    CHECK(platform::fs::WildcardMatch("*.*", "000000001-Quick-Save"));
    CHECK(platform::fs::WildcardMatch("*.*", "a.b.c"));
    CHECK(platform::fs::WildcardMatch("save.*", "SAVE"));
    CHECK_FALSE(platform::fs::WildcardMatch("save.*", "saves"));
    CHECK_FALSE(platform::fs::WildcardMatch("*.*x", "abc"));
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

namespace {

std::vector<std::string> FindAll(const std::string& pattern, std::vector<DWORD>* attributes = nullptr)
{
    std::vector<std::string> names;
    WIN32_FIND_DATAA data {};
    HANDLE h = FindFirstFileA(pattern.c_str(), &data);
    if (h == INVALID_HANDLE_VALUE)
        return names;
    do {
        names.push_back(data.cFileName);
        if (attributes)
            attributes->push_back(data.dwFileAttributes);
    } while (FindNextFileA(h, &data));
    FindClose(h);
    return names;
}

}

// Windows lists "." and ".." first for "*" and "*.*" in a non-root directory,
// so CDimm::DirectoryCopyFiles of an empty mpsave\default succeeds.
TEST_CASE("fs find lists the dot entries first for star patterns", "[fs]")
{
    TempTree tree;
    stdfs::create_directories(tree.root / "Empty");
    const std::string empty = tree.str() + "/Empty";

    std::vector<DWORD> attributes;
    CHECK(FindAll(empty + "/*", &attributes) == std::vector<std::string> { ".", ".." });
    for (DWORD a : attributes)
        CHECK((a & FILE_ATTRIBUTE_DIRECTORY) != 0);
    CHECK(FindAll(empty + "/*.*") == std::vector<std::string> { ".", ".." });
    CHECK(FindAll(empty + "\\*.*") == std::vector<std::string> { ".", ".." });

    WIN32_FIND_DATAA data {};
    CHECK(FindFirstFileA((empty + "/*.sav").c_str(), &data) == INVALID_HANDLE_VALUE);
    CHECK(FindFirstFileA((tree.str() + "/Missing/*").c_str(), &data) == INVALID_HANDLE_VALUE);

    Touch(tree.root / "One" / "ICEWIND2.GAM");
    CHECK(FindAll(tree.str() + "/One/*") == std::vector<std::string> { ".", "..", "ICEWIND2.GAM" });
    CHECK(FindAll(tree.str() + "/One/*.*") == std::vector<std::string> { ".", "..", "ICEWIND2.GAM" });
    CHECK(FindAll(tree.str() + "/One/*.GAM") == std::vector<std::string> { "ICEWIND2.GAM" });
}

TEST_CASE("fs find lists names in NTFS's upper-case order", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Order" / "a.txt");
    Touch(tree.root / "Order" / "B.txt");
    Touch(tree.root / "Order" / "_x");
    Touch(tree.root / "Order" / "Ab");
    // Upper-cased: "A.TXT" < "AB" ('.' < 'B') < "B.TXT" < "_X" ('_' after letters).
    CHECK(FindAll(tree.str() + "/Order/*") == std::vector<std::string> { ".", "..", "a.txt", "Ab", "B.txt", "_x" });
    CHECK(FindAll(tree.str() + "/Order/*.txt") == std::vector<std::string> { "a.txt", "B.txt" });
    if (tree.caseSensitive) {
        // Same upper-cased name: the raw name decides.
        Touch(tree.root / "Ties" / "ab");
        Touch(tree.root / "Ties" / "AB");
        CHECK(FindAll(tree.str() + "/Ties/*") == std::vector<std::string> { ".", "..", "AB", "ab" });
    }
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
    CHECK(SamePath(platform::fs::Resolve("data/NEW.BIF"), "Data/NEW.BIF"));
    const std::string src = tree.str() + "/Data/a.bif";
    const std::string dst = tree.str() + "/Data/New.Bif";
    REQUIRE(CopyFileA(src.c_str(), dst.c_str(), TRUE) == TRUE);
    CHECK(SamePath(platform::fs::Resolve("data/NEW.BIF"), "Data/New.Bif"));
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
#if defined(__linux__) || defined(__APPLE__)
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
    CHECK(SamePath(compat::ResolvePath("override\\x.itm"), "Override/X.ITM"));
    compat::SetPathResolver(nullptr);
}

TEST_CASE("fs cache is invalidated for relative paths", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Data" / "a.bif");
    platform::fs::SetDataRoot(tree.str());
    CwdGuard cwd(tree.root);
    CHECK(SamePath(platform::fs::Resolve("data/NEW.BIF"), "Data/NEW.BIF"));
    REQUIRE(CopyFileA("Data/a.bif", "Data/New.Bif", TRUE) == TRUE);
    CHECK(SamePath(platform::fs::Resolve("data/NEW.BIF"), "Data/New.Bif"));

    CHECK(SamePath(platform::fs::Resolve("top.txt"), "top.txt"));
    Touch(tree.root / "Top.txt");
    platform::fs::InvalidateCache();
    CHECK(SamePath(platform::fs::Resolve("TOP.TXT"), "Top.txt"));
    REQUIRE(DeleteFileA("Top.txt") == TRUE);
    CHECK(SamePath(platform::fs::Resolve("top.txt"), "top.txt"));
}

TEST_CASE("fs rescans after a miss for files created outside the shim", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Save" / "other.gam");
    platform::fs::SetDataRoot(tree.str());
    CHECK(SamePath(platform::fs::Resolve("save/SAVE.GAM"), "Save/SAVE.GAM"));
    Touch(tree.root / "Save" / "Save.gam");
    CHECK(SamePath(platform::fs::Resolve("save/SAVE.GAM"), "Save/Save.gam"));
}

TEST_CASE("fs DeleteFileA reports a missing file", "[fs]")
{
    TempTree tree;
    SetLastError(0);
    CHECK(DeleteFileA((tree.str() + "/nothing.bmp").c_str()) == FALSE);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    SetLastError(0);
}

TEST_CASE("fs SetFileAttributesA toggles read-only", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "a.txt");
    const std::string file = tree.str() + "/a.txt";
    REQUIRE(SetFileAttributesA(file.c_str(), FILE_ATTRIBUTE_READONLY) == TRUE);
    CHECK((GetFileAttributes(file.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);
    REQUIRE(SetFileAttributes(file.c_str(), GetFileAttributes(file.c_str()) & ~FILE_ATTRIBUTE_READONLY) == TRUE);
    CHECK((GetFileAttributesA(file.c_str()) & FILE_ATTRIBUTE_READONLY) == 0);
    CHECK(SetFileAttributesA((tree.str() + "/nothing").c_str(), FILE_ATTRIBUTE_NORMAL) == FALSE);
    SetLastError(0);
}

TEST_CASE("fs RemoveDirectoryA removes only empty directories", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "full" / "x.txt");
    stdfs::create_directories(tree.root / "empty");
    CHECK(RemoveDirectoryA((tree.str() + "/full").c_str()) == FALSE);
    CHECK(stdfs::exists(tree.root / "full" / "x.txt"));
    CHECK(RemoveDirectoryA((tree.str() + "/empty").c_str()) == TRUE);
    CHECK_FALSE(stdfs::exists(tree.root / "empty"));
    CHECK(RemoveDirectoryA((tree.str() + "/empty").c_str()) == FALSE);
    SetLastError(0);
}

TEST_CASE("fs CreateFileA and ReadFile read through a kernel handle", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Save" / "ICEWIND2.GAM", "GAMEV2.2rest");
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    CwdGuard cwd(tree.root);

    HANDLE h = CreateFileA("save\\icewind2.gam", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    REQUIRE(h != INVALID_HANDLE_VALUE);
    char buffer[8];
    DWORD read = 0;
    CHECK(ReadFile(h, buffer, sizeof(buffer), &read, NULL) == TRUE);
    CHECK(read == 8);
    CHECK(std::string(buffer, 8) == "GAMEV2.2");
    char rest[16];
    CHECK(ReadFile(h, rest, sizeof(rest), &read, NULL) == TRUE);
    CHECK(read == 4);
    CHECK(WaitForSingleObject(h, 0) == WAIT_OBJECT_0);
    CHECK(CloseHandle(h) == TRUE);

    SetLastError(0);
    CHECK(CreateFileA("Save\\missing.gam", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);

    HANDLE created = CreateFileA("Save\\new.bin", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    REQUIRE(created != INVALID_HANDLE_VALUE);
    CHECK(CloseHandle(created) == TRUE);
    CHECK(stdfs::exists(tree.root / "Save" / "new.bin"));
    CHECK(CreateFileA("Save\\new.bin", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL) == INVALID_HANDLE_VALUE);

    HANDLE event = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(ReadFile(event, buffer, sizeof(buffer), &read, NULL) == FALSE);
    CHECK(read == 0);
    CloseHandle(event);
    SetLastError(0);
}

TEST_CASE("fs CFileFind walks a directory like MFC", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "Dir" / "a.txt");
    Touch(tree.root / "Dir" / "Sub" / "b.txt");
    const std::string dir = tree.str() + "/Dir";

    CFileFind self;
    REQUIRE(self.FindFile(dir.c_str()) == TRUE);
    CHECK(self.GetFileName() == CString()); // nothing current before FindNextFile
    CHECK(self.FindNextFile() == FALSE);
    CHECK(self.IsDirectory());
    CHECK_FALSE(self.IsDots());
    CHECK(self.GetFileName() == CString("Dir"));

    CFileFind find;
    std::set<std::string> files, dirs;
    BOOL working = find.FindFile((dir + "\\*.*").c_str());
    REQUIRE(working);
    while (working) {
        working = find.FindNextFile();
        if (find.IsDots())
            continue;
        const std::string path = static_cast<const char*>(find.GetFilePath());
        (find.IsDirectory() ? dirs : files).insert(path);
    }
    CHECK(files == std::set<std::string> { dir + "\\a.txt" });
    CHECK(dirs == std::set<std::string> { dir + "\\Sub" });
    CHECK(find.FindFile((tree.str() + "/none/*.*").c_str()) == FALSE);
}

#ifdef _WIN32
TEST_CASE("fs skips names outside the ANSI code page", "[fs]")
{
    TempTree tree;
    Touch(tree.root / "a.txt");
    // U+F05C is how WSL stores a backslash in a file name on NTFS.
    std::ofstream(tree.root / stdfs::path(L"\uF05C.txt")) << "x";
    WIN32_FIND_DATAA data {};
    HANDLE h = FindFirstFileA((tree.str() + "/*.txt").c_str(), &data);
    REQUIRE(h != INVALID_HANDLE_VALUE);
    CHECK(std::string(data.cFileName) == "a.txt");
    CHECK(FindNextFileA(h, &data) == FALSE);
    FindClose(h);
    platform::fs::SetDataRoot(tree.str());
    platform::fs::InvalidateCache();
    CHECK(SamePath(platform::fs::Resolve("A.TXT"), "a.txt"));
}
#endif
