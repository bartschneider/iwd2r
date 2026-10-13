#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_file.h"
#include "compat/path_hook.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>

namespace {

std::atomic<int> s_counter { 0 };

class TempDir {
public:
    TempDir()
    {
        m_path = std::filesystem::temp_directory_path() / ("iwd2r_file_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(s_counter++));
        std::filesystem::remove_all(m_path);
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        compat::SetPathResolver(nullptr);
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }
    std::string file(const char* name) const { return (m_path / name).string(); }

private:
    std::filesystem::path m_path;
};

std::string appendTxt(const char* path) { return std::string(path) + ".txt"; }

}

TEST_CASE("file: open, write, read and seek", "[file]")
{
    TempDir dir;
    const std::string path = dir.file("a.bin");

    CFile missing;
    CHECK(missing.Open(dir.file("missing").c_str(), CFile::modeRead) == FALSE);
    CHECK(missing.m_hFile == CFile::hFileNull);

    CFile out;
    REQUIRE(out.Open(path.c_str(), CFile::modeWrite | CFile::modeCreate));
    out.Write("abc", 3);
    out.Close();
    CHECK(out.m_hFile == CFile::hFileNull);
    out.Close();

    CFile in;
    REQUIRE(in.Open(path.c_str(), CFile::modeRead | CFile::typeBinary));
    CHECK(in.GetLength() == 3);
    char buffer[10];
    CHECK(in.Read(buffer, 10) == 3);
    CHECK(in.Read(buffer, 10) == 0);
    CHECK(in.Seek(-1, CFile::end) == 2);
    CHECK(in.GetPosition() == 2);
    CHECK(in.SeekToEnd() == 3);
    CHECK(in.Seek(10, CFile::begin) == 10);
    CHECK(in.GetLength() == 3);
    in.SeekToBegin();
    CHECK(in.GetPosition() == 0);
    CHECK(in.GetFilePath() == CString(path.c_str()));
    CHECK(in.GetFileName() == CString("a.bin"));
}

TEST_CASE("file: create and truncate modes", "[file]")
{
    TempDir dir;
    const std::string path = dir.file("b.bin");
    {
        CFile out(path.c_str(), CFile::modeWrite | CFile::modeCreate);
        REQUIRE(out.m_hFile != CFile::hFileNull);
        out.Write("abc", 3);
    }
    {
        CFile rw;
        REQUIRE(rw.Open(path.c_str(), CFile::modeReadWrite | CFile::modeCreate | CFile::modeNoTruncate));
        CHECK(rw.GetLength() == 3);
        rw.SetLength(8);
        CHECK(rw.GetLength() == 8);
        rw.SetLength(2);
        CHECK(rw.GetLength() == 2);
    }
    CFile noCreate;
    CHECK(noCreate.Open(dir.file("nope").c_str(), CFile::modeWrite) == FALSE);
    {
        CFile fresh;
        REQUIRE(fresh.Open(dir.file("c.bin").c_str(), CFile::modeReadWrite | CFile::modeCreate | CFile::modeNoTruncate));
        CHECK(fresh.GetLength() == 0);
    }
    {
        CFile trunc;
        REQUIRE(trunc.Open(path.c_str(), CFile::modeCreate | CFile::modeWrite));
        CHECK(trunc.GetLength() == 0);
    }
}

TEST_CASE("file: status, remove and rename", "[file]")
{
    TempDir dir;
    const std::string path = dir.file("d.bin");
    {
        CFile out(path.c_str(), CFile::modeWrite | CFile::modeCreate);
        out.Write("abc", 3);
    }
    CFileStatus status;
    REQUIRE(CFile::GetStatus(path.c_str(), status));
    CHECK(status.m_size == 3);
    CHECK((status.m_attribute & CFile::directory) == 0);
    CHECK(std::string(status.m_szFullName) == path);
    CHECK(status.m_mtime.GetTime() > 0);
    CHECK(status.m_mtime.GetYear() >= 2024);

    CFileStatus dirStatus;
    REQUIRE(CFile::GetStatus(dir.file("").c_str(), dirStatus));
    CHECK((dirStatus.m_attribute & CFile::directory) != 0);
    CHECK(CFile::GetStatus(dir.file("missing").c_str(), status) == FALSE);

    const std::string renamed = dir.file("e.bin");
    CFile::Rename(path.c_str(), renamed.c_str());
    CHECK(CFile::GetStatus(path.c_str(), status) == FALSE);
    CHECK(CFile::GetStatus(renamed.c_str(), status));
    CFile::Remove(renamed.c_str());
    CHECK(CFile::GetStatus(renamed.c_str(), status) == FALSE);
    CFile::Remove(renamed.c_str());
}

TEST_CASE("file: path resolver hook", "[file]")
{
    TempDir dir;
    const std::string base = dir.file("name");
    {
        CFile out(base.c_str(), CFile::modeWrite | CFile::modeCreate);
        out.Write("x", 1);
    }
    compat::SetPathResolver(appendTxt);
    CFile hooked;
    CHECK(hooked.Open(base.c_str(), CFile::modeRead) == FALSE);
    REQUIRE(hooked.Open(base.c_str(), CFile::modeWrite | CFile::modeCreate));
    hooked.Close();
    CHECK(std::filesystem::exists(base + ".txt"));
    compat::SetPathResolver(nullptr);
    CHECK(compat::ResolvePath("same") == "same");
    CFile plain;
    CHECK(plain.Open(base.c_str(), CFile::modeRead) == TRUE);
}

TEST_CASE("file: CTime basics", "[file]")
{
    const CTime now = CTime::GetCurrentTime();
    CHECK(now.GetMonth() >= 1);
    CHECK(now.GetMonth() <= 12);
    CHECK(now.GetDayOfWeek() >= 1);
    CHECK(now.GetDayOfWeek() <= 7);
    CHECK((CTime(now.GetTime() + 90) - now).GetTotalSeconds() == 90);
}

TEST_CASE("file: mixed reads and writes on one stream", "[file]")
{
    TempDir dir;
    const std::string path = dir.file("rw.bin");
    CFile f;
    REQUIRE(f.Open(path.c_str(), CFile::modeReadWrite | CFile::modeCreate));
    f.Write("abcd", 4);
    f.SeekToBegin();
    char buffer[4];
    REQUIRE(f.Read(buffer, 2) == 2);
    CHECK(std::string(buffer, 2) == "ab");
    f.Write("XY", 2);
    CHECK(f.GetLength() == 4);
    f.SeekToBegin();
    REQUIRE(f.Read(buffer, 4) == 4);
    CHECK(std::string(buffer, 4) == "abXY");
    f.Write("Z", 1);
    REQUIRE(f.Read(buffer, 1) == 0);
    CHECK(f.GetLength() == 5);
}

TEST_CASE("file: SetFilePath and the open file's status", "[file]")
{
    TempDir dir;
    const std::string path = dir.file("s.bin");
    {
        CFile out(path.c_str(), CFile::modeWrite | CFile::modeCreate);
        out.Write("abcd", 4);
    }
    CFile named;
    named.SetFilePath(path.c_str());
    CHECK(std::string(named.GetFilePath()) == path);
    CFileStatus status;
    CHECK(named.GetStatus(status) == FALSE);

    CFile file;
    REQUIRE(file.Open(path.c_str(), CFile::modeRead));
    REQUIRE(file.GetStatus(status));
    CHECK(status.m_size == 4);
}

#ifndef _MSC_VER
TEST_CASE("file: _mkdir, _chdir and _access use the path resolver", "[file]")
{
    TempDir dir;
    static std::string s_root;
    s_root = dir.file("");
    compat::SetPathResolver([](const char* path) {
        std::string p(path);
        for (char& c : p)
            if (c == '\\')
                c = '/';
        return p.rfind("X:/", 0) == 0 ? s_root + p.substr(3) : p;
    });
    char cwd[4096];
    REQUIRE(_getcwd(cwd, sizeof(cwd)) != nullptr);
    CHECK(_mkdir("X:\\made") == 0);
    CHECK(std::filesystem::is_directory(dir.file("made")));
    CHECK(_access("X:\\made", 0) == 0);
    CHECK(_chdir("X:\\made") == 0);
    CHECK(_chdir(cwd) == 0);
}
#endif
