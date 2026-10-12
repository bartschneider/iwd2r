// iwd2r. Distributed under LICENSE.md.
// Inert Win32 calls: no DLLs, no GDI text, no COM, no input methods, no
// child processes. Each returns what Windows returns when the feature is
// unavailable, so the engine falls back to its own paths.
#define NOMINMAX
#include "platform/win32_api.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

thread_local DWORD t_lastError = 0;

char g_moduleSentinel;

}

HMODULE LoadLibraryA(LPCSTR) { return NULL; }

BOOL FreeLibrary(HMODULE) { return TRUE; }

FARPROC GetProcAddress(HMODULE, LPCSTR) { return NULL; }

HMODULE GetModuleHandleA(LPCSTR) { return reinterpret_cast<HMODULE>(&g_moduleSentinel); }

// Windows XP (5.1.2600), which the engine accepts.
BOOL GetVersionExA(LPOSVERSIONINFOA info)
{
    if (!info || info->dwOSVersionInfoSize < sizeof(OSVERSIONINFOA))
        return FALSE;
    info->dwMajorVersion = 5;
    info->dwMinorVersion = 1;
    info->dwBuildNumber = 2600;
    info->dwPlatformId = VER_PLATFORM_WIN32_NT;
    info->szCSDVersion[0] = '\0';
    return TRUE;
}

void GlobalMemoryStatus(LPMEMORYSTATUS status)
{
    if (!status)
        return;
    const SIZE_T gib = static_cast<SIZE_T>(1024) * 1024 * 1024;
    status->dwLength = sizeof(MEMORYSTATUS);
    status->dwMemoryLoad = 0;
    status->dwTotalPhys = gib;
    status->dwAvailPhys = gib;
    status->dwTotalPageFile = gib;
    status->dwAvailPageFile = gib;
    status->dwTotalVirtual = gib;
    status->dwAvailVirtual = gib;
}

HDC GetDC(HWND) { return NULL; }

int ReleaseDC(HWND, HDC hdc) { return hdc != NULL ? 1 : 0; }

int GetDeviceCaps(HDC, int index)
{
    switch (index) {
    case BITSPIXEL:
        return 32;
    case PLANES:
        return 1;
    case LOGPIXELSX:
    case LOGPIXELSY:
        return 96;
    default:
        return 0;
    }
}

HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return NULL; }

BOOL DeleteObject(HGDIOBJ) { return TRUE; }

HGDIOBJ SelectObject(HDC, HGDIOBJ) { return NULL; }

BOOL ExtTextOutA(HDC, int, int, UINT, const RECT*, LPCSTR, UINT, const INT*) { return FALSE; }

BOOL TextOutA(HDC, int, int, LPCSTR, int) { return FALSE; }

COLORREF SetTextColor(HDC, COLORREF) { return CLR_INVALID; }

COLORREF SetBkColor(HDC, COLORREF) { return CLR_INVALID; }

int SetBkMode(HDC, int) { return 0; }

BOOL GetTextExtentPoint32A(HDC, LPCSTR, int, LPSIZE) { return FALSE; }

DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD handle)
{
    if (handle)
        *handle = 0;
    return 0;
}

BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD, LPVOID) { return FALSE; }

BOOL VerQueryValueA(LPCVOID, LPCSTR, LPVOID*, PUINT) { return FALSE; }

HRESULT CoInitialize(LPVOID) { return S_OK; }

void CoUninitialize() { }

HRESULT CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID* object)
{
    if (object)
        *object = NULL;
    return E_NOINTERFACE;
}

void OutputDebugStringA(LPCSTR text)
{
    if (text)
        std::fputs(text, stderr);
}

LPSTR lstrcpyA(LPSTR destination, LPCSTR source)
{
    if (!destination || !source)
        return NULL;
    return std::strcpy(destination, source);
}

LPSTR lstrcatA(LPSTR destination, LPCSTR source)
{
    if (!destination || !source)
        return NULL;
    return std::strcat(destination, source);
}

int lstrlenA(LPCSTR text) { return text ? static_cast<int>(std::strlen(text)) : 0; }

int lstrcmpA(LPCSTR a, LPCSTR b)
{
    const int r = std::strcmp(a ? a : "", b ? b : "");
    return r < 0 ? -1 : (r > 0 ? 1 : 0);
}

int lstrcmpiA(LPCSTR a, LPCSTR b)
{
    const unsigned char* pa = reinterpret_cast<const unsigned char*>(a ? a : "");
    const unsigned char* pb = reinterpret_cast<const unsigned char*>(b ? b : "");
    for (;; pa++, pb++) {
        const int ca = std::tolower(*pa);
        const int cb = std::tolower(*pb);
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (ca == 0)
            return 0;
    }
}

int wsprintfA(LPSTR buffer, LPCSTR format, ...)
{
    if (!buffer || !format)
        return 0;
    const int limit = 1024;
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, limit, format, args);
    va_end(args);
    if (written < 0) {
        buffer[0] = '\0';
        return 0;
    }
    return written < limit ? written : limit - 1;
}

DWORD GetLastError() { return t_lastError; }

void SetLastError(DWORD error) { t_lastError = error; }

BOOL CreateProcessA(LPCSTR, LPCSTR, LPVOID, LPVOID, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION) { return FALSE; }

HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT) { return NULL; }

HIMC ImmGetContext(HWND) { return NULL; }

BOOL ImmReleaseContext(HWND, HIMC) { return TRUE; }

LONG ImmGetCompositionStringA(HIMC, DWORD, LPVOID, DWORD) { return 0; }

BOOL ImmGetConversionStatus(HIMC, LPDWORD conversion, LPDWORD sentence)
{
    if (conversion)
        *conversion = 0;
    if (sentence)
        *sentence = 0;
    return FALSE;
}

BOOL ImmSetConversionStatus(HIMC, DWORD, DWORD) { return FALSE; }

BOOL ImmGetOpenStatus(HIMC) { return FALSE; }

BOOL ImmSetOpenStatus(HIMC, BOOL) { return FALSE; }

DWORD ImmGetCandidateListA(HIMC, DWORD, LPCANDIDATELIST, DWORD) { return 0; }

UINT ImmGetDescriptionA(HKL, LPSTR description, UINT bufferLength)
{
    if (description && bufferLength > 0)
        description[0] = '\0';
    return 0;
}
