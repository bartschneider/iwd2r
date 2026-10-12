// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/ini.h"
#include "compat/path_hook.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::atomic<int> s_counter { 0 };

const char* const kSample = "[Program Options]\nWidth = 800 \nName = \"Icewind Dale II\" \nFlag=1 ; note\n; comment\n[alias]\nHD0:=C:\\Games\\IWD2\\\n";

class TempIni {
public:
    explicit TempIni(const char* content = nullptr)
    {
        compat::ini::FlushCache();
        m_path = (std::filesystem::temp_directory_path() / ("iwd2r_ini_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(s_counter++) + ".ini")).string();
        if (content) {
            std::ofstream out(m_path, std::ios::binary);
            out << content;
        }
    }
    ~TempIni()
    {
        compat::ini::FlushCache();
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }
    const char* f() const { return m_path.c_str(); }
    std::string contents() const
    {
        std::ifstream in(m_path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

private:
    std::string m_path;
};

std::string GetStr(const char* section, const char* key, const char* def, const TempIni& t)
{
    char buf[128];
    DWORD n = GetPrivateProfileStringA(section, key, def, buf, sizeof buf, t.f());
    REQUIRE(n == std::char_traits<char>::length(buf));
    return buf;
}

}

TEST_CASE("ini reads with case-insensitive trimmed lookup", "[ini]")
{
    TempIni t(kSample);
    CHECK(GetPrivateProfileIntA("program options", "width", 0, t.f()) == 800);

    char buf[64];
    CHECK(GetPrivateProfileStringA("Program Options", "Name", "", buf, 64, t.f()) == 15);
    CHECK(std::string(buf) == "Icewind Dale II");
    CHECK(GetStr("Program Options", "Flag", "", t) == "1 ; note");
    CHECK(GetStr("Alias", "HD0:", "", t) == "C:\\Games\\IWD2\\");
}

TEST_CASE("ini defaults and size limits", "[ini]")
{
    TempIni t(kSample);
    char buf[64];
    CHECK(GetPrivateProfileStringA("Program Options", "Nope", "dflt", buf, 64, t.f()) == 4);
    CHECK(std::string(buf) == "dflt");
    CHECK(GetPrivateProfileStringA("Program Options", "Nope", nullptr, buf, 64, t.f()) == 0);
    CHECK(std::string(buf).empty());

    TempIni missing;
    CHECK(GetPrivateProfileStringA("a", "b", "dflt", buf, 64, missing.f()) == 4);
    CHECK(std::string(buf) == "dflt");

    CHECK(GetPrivateProfileStringA("Program Options", "Name", "", buf, 5, t.f()) == 4);
    CHECK(std::string(buf) == "Icew");

    buf[0] = 'x';
    CHECK(GetPrivateProfileStringA("Program Options", "Name", "", buf, 0, t.f()) == 0);
    CHECK(buf[0] == 'x');
    CHECK(GetPrivateProfileStringA("Program Options", "Name", "", nullptr, 64, t.f()) == 0);
}

TEST_CASE("ini enumerates keys and sections", "[ini]")
{
    TempIni t(kSample);
    char buf[64];
    DWORD n = GetPrivateProfileStringA("Program Options", nullptr, "", buf, 64, t.f());
    CHECK(n == 16);
    CHECK(std::string(buf, 17) == std::string("Width\0Name\0Flag\0\0", 17));

    n = GetPrivateProfileStringA("Program Options", nullptr, "", buf, 7, t.f());
    CHECK(n == 5);
    CHECK(std::string(buf, 7) == std::string("Width\0\0", 7));

    n = GetPrivateProfileStringA(nullptr, nullptr, "", buf, 64, t.f());
    CHECK(std::string(buf, 23) == std::string("Program Options\0alias\0\0", 23));
    CHECK(n == 22);
}

TEST_CASE("ini integer parsing", "[ini]")
{
    TempIni t("[s]\na=abc\nb= -12x\nc=+7\nd=\n");
    CHECK(GetPrivateProfileIntA("s", "a", 33, t.f()) == 33);
    CHECK(GetPrivateProfileIntA("s", "b", 0, t.f()) == 0xFFFFFFF4u);
    CHECK(GetPrivateProfileIntA("s", "c", 0, t.f()) == 7);
    CHECK(GetPrivateProfileIntA("s", "d", 5, t.f()) == 5);
    CHECK(GetPrivateProfileIntA("s", "zz", -1, t.f()) == 0xFFFFFFFFu);
}

TEST_CASE("ini write replaces, appends and deletes", "[ini]")
{
    TempIni t(kSample);
    CHECK(WritePrivateProfileStringA("Program Options", "Width", "1024", t.f()) == TRUE);
    CHECK(t.contents() == "[Program Options]\nWidth = 1024\nName = \"Icewind Dale II\" \nFlag=1 ; note\n; comment\n[alias]\nHD0:=C:\\Games\\IWD2\\\n");

    CHECK(WritePrivateProfileStringA("Program Options", "Height", "768", t.f()));
    CHECK(t.contents() == "[Program Options]\nWidth = 1024\nName = \"Icewind Dale II\" \nFlag=1 ; note\n; comment\nHeight=768\n[alias]\nHD0:=C:\\Games\\IWD2\\\n");

    CHECK(WritePrivateProfileStringA("New", "k", "v", t.f()));
    CHECK(t.contents() == "[Program Options]\nWidth = 1024\nName = \"Icewind Dale II\" \nFlag=1 ; note\n; comment\nHeight=768\n[alias]\nHD0:=C:\\Games\\IWD2\\\n\n[New]\nk=v\n");
}

TEST_CASE("ini write new section, delete key and section", "[ini]")
{
    TempIni t("[a]\nx=1\n\n[b]\ny=2\n");
    CHECK(WritePrivateProfileStringA("a", "z", "3", t.f()));
    CHECK(t.contents() == "[a]\nx=1\nz=3\n\n[b]\ny=2\n");
    CHECK(WritePrivateProfileStringA("c", "k", "v", t.f()));
    CHECK(t.contents() == "[a]\nx=1\nz=3\n\n[b]\ny=2\n\n[c]\nk=v\n");
    CHECK(WritePrivateProfileStringA("a", "x", nullptr, t.f()));
    CHECK(t.contents() == "[a]\nz=3\n\n[b]\ny=2\n\n[c]\nk=v\n");
    CHECK(WritePrivateProfileStringA("b", nullptr, nullptr, t.f()));
    CHECK(t.contents() == "[a]\nz=3\n\n[c]\nk=v\n");

    // Reload from disk to prove the file round trips.
    compat::ini::FlushCache();
    CHECK(GetStr("c", "k", "", t) == "v");
    CHECK(GetStr("b", "y", "gone", t) == "gone");
}

TEST_CASE("ini write creates a missing file", "[ini]")
{
    TempIni t;
    CHECK(WritePrivateProfileStringA("Sec", "Key", "Val", t.f()));
    CHECK(t.contents() == "[Sec]\nKey=Val\n");
    compat::ini::FlushCache();
    CHECK(GetStr("sec", "key", "", t) == "Val");
}

TEST_CASE("ini keeps CRLF, BOM and comments", "[ini]")
{
    TempIni t("\xEF\xBB\xBF[a]\r\n# c\r\nx=1\r\n");
    CHECK(GetStr("a", "x", "", t) == "1");
    CHECK(WritePrivateProfileStringA("a", "y", "2", t.f()));
    CHECK(t.contents() == "\xEF\xBB\xBF[a]\r\n# c\r\nx=1\r\ny=2\r\n");
}

TEST_CASE("ini fails to write into a missing directory", "[ini]")
{
    TempIni t;
    std::string bad = (std::filesystem::path(t.f()).parent_path() / "iwd2r_no_such_dir" / "x.ini").string();
    CHECK(WritePrivateProfileStringA("a", "b", "c", bad.c_str()) == FALSE);
}

TEST_CASE("ini file names go through the path resolver", "[ini]")
{
    TempIni t(kSample);
    static std::string s_real;
    s_real = t.f();
    compat::SetPathResolver([](const char*) { return s_real; });
    CHECK(GetPrivateProfileIntA("program options", "width", 0, "Elsewhere.ini") == 800);
    compat::SetPathResolver(nullptr);
}
