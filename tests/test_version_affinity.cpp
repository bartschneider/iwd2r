// The built-in version resource (ruling R28) and the single-core affinity pin
// the engine fetches through GetProcAddress (ruling R29).
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"
#include "platform/win32_api.h"

#include <cstdio>
#include <string>
#include <vector>

#ifdef __linux__
#include <sched.h>
#endif

#ifndef IWD2R_VERSION_STRING
#error IWD2R_VERSION_STRING must be defined by tests/CMakeLists.txt
#endif

namespace {

std::string Query(const std::vector<BYTE>& block, const char* path)
{
    LPVOID buffer = nullptr;
    UINT length = 0;
    if (!VerQueryValueA(block.data(), path, &buffer, &length))
        return "<none>";
    const std::string value(static_cast<const char*>(buffer));
    CHECK(length == value.size() + 1);
    return value;
}

}

TEST_CASE("version info answers the engine's StringFileInfo queries", "[stubs]")
{
    DWORD handle = 7;
    const DWORD size = GetFileVersionInfoSizeA("iwd2r", &handle);
    REQUIRE(size > 0);
    CHECK(handle == 0);
    std::vector<BYTE> block(size);
    CHECK_FALSE(GetFileVersionInfoA("iwd2r", 0, size - 1, block.data()));
    REQUIRE(GetFileVersionInfoA("iwd2r", 0, size, block.data()));

    CHECK(Query(block, "\\StringFileInfo\\040904B0\\ProductName") == "Icewind Dale II");
    CHECK(Query(block, "\\StringFileInfo\\040904B0\\ProductVersion") == "2, 0, 1, 0");
    CHECK(Query(block, "\\StringFileInfo\\040904b0\\FileVersion") == "2, 0, 1, 0");
    CHECK(Query(block, "\\StringFileInfo\\040904B0\\PrivateBuild") == IWD2R_VERSION_STRING);
    CHECK(Query(block, "\\StringFileInfo\\040904B0\\Missing") == "<none>");
    CHECK(Query(block, "\\StringFileInfo\\040904B0\\") == "<none>");
    CHECK(Query(block, "\\") == "<none>");
    CHECK(std::string(IWD2R_VERSION_STRING).rfind("iwd2r ", 0) == 0);
}

TEST_CASE("version info fills the engine's version fields and the options label parses", "[stubs]")
{
    CChitin::GetGameVersionInfo(NULL);
    CHECK(std::string(CChitin::m_sGameName) == "Icewind Dale II");
    CHECK(std::string(CChitin::m_sVersionNumber) == "2, 0, 1, 0");
    CHECK(std::string(CChitin::m_sBuildNumber) == "101615");

    // CScreenOptions' version label, as UpdateMainPanel builds it.
    int major = -1;
    int minor = -1;
    int patch = -1;
    REQUIRE(std::sscanf(CChitin::m_sVersionNumber, "%d, %d, %d", &major, &minor, &patch) == 3);
    CString label;
    label.Format("v%d.%d%d (%s)", major, minor, patch, static_cast<LPCSTR>(CChitin::m_sBuildNumber));
    CHECK(std::string(label) == "v2.01 (101615)");
}

TEST_CASE("GetProcAddress hands out SetProcessAffinityMask only", "[stubs]")
{
    HMODULE kernel32 = GetModuleHandleA("Kernel32");
    REQUIRE(kernel32 != NULL);
    CHECK(GetProcAddress(kernel32, "SetProcessAffinityMask") != NULL);
    CHECK(GetProcAddress(kernel32, "setprocessaffinitymask") == NULL);
    CHECK(GetProcAddress(kernel32, "GetProcessAffinityMask") == NULL);
    CHECK(GetProcAddress(kernel32, NULL) == NULL);
}

TEST_CASE("SetProcessAffinityMask rejects other processes and empty masks", "[stubs]")
{
    HANDLE other = reinterpret_cast<HANDLE>(static_cast<intptr_t>(0x1234));
    SetLastError(0);
    CHECK_FALSE(SetProcessAffinityMask(other, 1));
    CHECK(GetLastError() == ERROR_INVALID_HANDLE);
    CHECK_FALSE(SetProcessAffinityMask(NULL, 0));
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
}

#ifdef __linux__
TEST_CASE("SetProcessAffinityMask through GetProcAddress pins to CPU 0", "[stubs]")
{
    cpu_set_t original;
    CPU_ZERO(&original);
    REQUIRE(sched_getaffinity(0, sizeof(original), &original) == 0);
    if (!CPU_ISSET(0, &original))
        SKIP("CPU 0 is not available to this process");

    // The engine's call: the pointer type it declares, NULL from OpenProcess.
    typedef BOOL(__stdcall SetProcessAffinityMaskFunc)(HANDLE, DWORD);
    SetProcessAffinityMaskFunc* pfn = reinterpret_cast<SetProcessAffinityMaskFunc*>(
        reinterpret_cast<void (*)()>(GetProcAddress(GetModuleHandleA("Kernel32"), "SetProcessAffinityMask")));
    REQUIRE(pfn != nullptr);
    REQUIRE(pfn(NULL, 1));

    cpu_set_t now;
    CPU_ZERO(&now);
    REQUIRE(sched_getaffinity(0, sizeof(now), &now) == 0);
    CHECK(CPU_COUNT(&now) == 1);
    CHECK(CPU_ISSET(0, &now));

    // Restore every thread's original mask.
    DWORD_PTR mask = 0;
    for (int cpu = 0; cpu < 64; cpu++) {
        if (CPU_ISSET(cpu, &original))
            mask |= DWORD_PTR(1) << cpu;
    }
    CHECK(SetProcessAffinityMask(GetCurrentProcess(), mask));
    REQUIRE(sched_setaffinity(0, sizeof(original), &original) == 0);
    CPU_ZERO(&now);
    REQUIRE(sched_getaffinity(0, sizeof(now), &now) == 0);
    CHECK(CPU_EQUAL(&now, &original));
}
#endif
