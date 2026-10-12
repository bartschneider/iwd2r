// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc.h"

#include <ctime>

TEST_CASE("misc CTime current time is close to time()", "[misc]")
{
    CTime t = CTime::GetCurrentTime();
    time_t now = time(nullptr);
    long long diff = static_cast<long long>(now) - static_cast<long long>(t.GetTime());
    REQUIRE(diff >= -5);
    REQUIRE(diff <= 5);
}

TEST_CASE("misc CTime hour matches localtime", "[misc]")
{
    time_t value = 86400 * 365;
    struct tm local;
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    CTime t(value);
    REQUIRE(t.GetHour() == local.tm_hour);
}

TEST_CASE("misc CWnd attach and detach", "[misc]")
{
    CWnd w;
    REQUIRE(w.GetSafeHwnd() == NULL);
    HWND h = reinterpret_cast<HWND>(0x10);
    REQUIRE(w.Attach(h));
    REQUIRE(w.GetSafeHwnd() == h);
    REQUIRE(w.m_hWnd == h);
    REQUIRE(w.Detach() == h);
    REQUIRE(w.GetSafeHwnd() == NULL);
}

TEST_CASE("misc LoadStringA truncates and reports", "[misc]")
{
    char buf[16];
    memset(buf, 'x', sizeof(buf));
    REQUIRE(LoadStringA(NULL, IDS_ENGLISH, buf, 8) == 7);
    REQUIRE(std::string(buf) == "This ve");
    REQUIRE(buf[7] == '\0');

    char big[256];
    int n = LoadStringA(NULL, IDS_SET_PIXEL_FORMAT, big, sizeof(big));
    REQUIRE(n == 22);
    REQUIRE(std::string(big) == "SetPixelFormat failed.");

    memset(buf, 'x', sizeof(buf));
    REQUIRE(LoadStringA(NULL, 9999, buf, 8) == 0);
    REQUIRE(buf[0] == '\0');
    REQUIRE(LoadStringA(NULL, IDS_ENGLISH, buf, 0) == 0);
    REQUIRE(iwd2r_ResourceString(9999) == NULL);
    REQUIRE(iwd2r_ResourceString(0) == NULL);
}

TEST_CASE("misc resource strings are exact", "[misc]")
{
    REQUIRE(std::string(iwd2r_ResourceString(IDS_EXSLUSIVE_MODE))
        == "This program needs to run in \nDirect Draw Exclusive Mode\n\nPlease restart after closing other \nDirect Draw programs.");
    REQUIRE(std::string(iwd2r_ResourceString(IDS_WINDOWS_FONTS)) == "Window's fonts do not work with 3d, running 2d instead.");
    for (UINT id = 1; id <= 12; ++id)
        REQUIRE(iwd2r_ResourceString(id) != NULL);
}

TEST_CASE("misc aggregator header exposes the shims", "[misc]")
{
    CString s("abc");
    CPtrList list;
    list.AddTail(&s);
    CRect r(0, 0, 10, 20);
    CFile file;
    CCriticalSection cs;
    CSingleLock lock(&cs, TRUE);
    REQUIRE(lock.IsLocked());
    REQUIRE(list.GetCount() == 1);
    REQUIRE(r.Width() == 10);
    REQUIRE(s.GetLength() == 3);
}

TEST_CASE("misc CWnd converts to its HWND", "[misc]")
{
    CWnd w;
    HWND none = w;
    CHECK(none == NULL);
    w.Attach(reinterpret_cast<HWND>(0x10));
    HWND h = w;
    CHECK(h == reinterpret_cast<HWND>(0x10));
    w.Detach();
}
