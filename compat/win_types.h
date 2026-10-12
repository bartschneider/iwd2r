// iwd2r. Distributed under LICENSE.md.
// Windows typedefs and helpers for the cross-platform port. This header is the
// only source of these definitions on every platform; it includes no Windows
// SDK header.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// The Windows-style min/max macros at the end of this header break libstdc++
// headers included after them. Pull in the standard headers the shims and the
// engine use first, so later includes of them are no-ops. This lives here, not
// in mfc.h, because some engine headers reach this file through a shim header
// (dsound_shim.h, ddraw_shim.h) without mfc.h.
#ifndef NOMINMAX
#include <algorithm>
#include <bitset>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <cstdarg>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#endif

#ifdef _MSC_VER
// The engine's own <direct.h> and <io.h> includes were dropped (they do not
// exist elsewhere); <process.h> is never included because it declares
// _beginthread with C linkage (see platform/win32_api.h).
#include <direct.h>
#include <io.h>
#else
#include <fcntl.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

typedef int32_t BOOL;
typedef uint8_t BOOLEAN;
typedef uint8_t BYTE;
typedef uint8_t UCHAR;
typedef char CHAR;
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef uint16_t WORD;
typedef int32_t INT;
typedef uint32_t UINT;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef uint32_t DWORD;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef float FLOAT;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef ULONG_PTR DWORD_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef UINT MMRESULT;
typedef LONG HRESULT;
typedef DWORD COLORREF;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef const char* LPCSTR;
typedef DWORD* LPDWORD;
typedef void VOID;
typedef BYTE* LPBYTE;
typedef UINT* PUINT;
typedef ULONG_PTR SIZE_T;
typedef DWORD_PTR* PDWORD_PTR;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu
#define DWORD_MAX 0xFFFFFFFFu
#define WAIT_OBJECT_0 0
#define WAIT_ABANDONED 0x80
#define WAIT_TIMEOUT 0x102

// Opaque handles: distinct pointer types so overloads can tell them apart.
typedef void* HANDLE;
struct HWND__;
typedef HWND__* HWND;
struct HINSTANCE__;
typedef HINSTANCE__* HINSTANCE;
typedef HINSTANCE HMODULE;

struct RGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
};

// Windows makes this a union with QuadPart over an anonymous struct; standard
// C++ cannot spell that, and the engine only reads LowPart and HighPart.
struct ULARGE_INTEGER {
    DWORD LowPart;
    DWORD HighPart;
};
typedef ULARGE_INTEGER* PULARGE_INTEGER;

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

struct WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow;
    char cFileName[MAX_PATH];
    char cAlternateFileName[14];
};

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#define FILE_ATTRIBUTE_READONLY 0x01
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_CDROM 5

typedef WORD LANGID;
#define MAKELANGID(p, s) ((LANGID)((((WORD)(s)) << 10) | (WORD)(p)))
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3FF)
#define SUBLANGID(lgid) ((WORD)(lgid) >> 10)
#define LANG_NEUTRAL 0x00
#define LANG_CHINESE 0x04
#define LANG_ENGLISH 0x09
#define LANG_JAPANESE 0x11
#define LANG_KOREAN 0x12
#define SUBLANG_DEFAULT 0x01
#define SUBLANG_CHINESE_TRADITIONAL 0x01
#define SUBLANG_CHINESE_SIMPLIFIED 0x02
#define SUBLANG_ENGLISH_US 0x01

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5

#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_INVALID_HANDLE 6

struct SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
};
typedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES;

// Field layout as stored in .bmp files; the file header is 2-byte packed.
#pragma pack(push, 2)
struct BITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
};
#pragma pack(pop)
static_assert(sizeof(BITMAPFILEHEADER) == 14, "BITMAPFILEHEADER is 14 bytes");
typedef BITMAPFILEHEADER* LPBITMAPFILEHEADER;

struct BITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
};
static_assert(sizeof(BITMAPINFOHEADER) == 40, "BITMAPINFOHEADER is 40 bytes");
typedef BITMAPINFOHEADER* LPBITMAPINFOHEADER;
struct HDC__;
typedef HDC__* HDC;
struct HGLRC__;
typedef HGLRC__* HGLRC;
struct HFONT__;
typedef HFONT__* HFONT;
// Untyped like Windows, so every GDI handle converts to it implicitly.
typedef void* HGDIOBJ;
typedef HANDLE HGLOBAL;
struct HBRUSH__;
typedef HBRUSH__* HBRUSH;
struct HPALETTE__;
typedef HPALETTE__* HPALETTE;
struct HICON__;
typedef HICON__* HICON;
struct HMENU__;
typedef HMENU__* HMENU;
struct HKEY__;
typedef HKEY__* HKEY;
struct HCURSOR__;
typedef HCURSOR__* HCURSOR;
struct HKL__;
typedef HKL__* HKL;
struct HIMC__;
typedef HIMC__* HIMC;

#ifndef _MSC_VER
#define CALLBACK
#define WINAPI
#define APIENTRY
#define __stdcall
#define __cdecl
#define __declspec(x)
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _vsnprintf vsnprintf
#define _snprintf snprintf
#define _chdir chdir
#define _getcwd getcwd
#define _access access
inline int _mkdir(const char* path) { return mkdir(path, 0777); }
// The MSVC CRT's current offset of a file descriptor.
inline long tell(int fd) { return static_cast<long>(lseek(fd, 0, SEEK_CUR)); }
#endif

struct POINT {
    LONG x, y;
};
struct SIZE {
    LONG cx, cy;
};
struct RECT {
    LONG left, top, right, bottom;
};
typedef POINT* LPPOINT;
typedef SIZE* LPSIZE;
typedef RECT* LPRECT;
typedef const RECT* LPCRECT;

struct OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR szCSDVersion[128];
};
typedef OSVERSIONINFOA* LPOSVERSIONINFOA;

struct OSVERSIONINFOEXA {
    DWORD dwOSVersionInfoSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformId;
    CHAR szCSDVersion[128];
    WORD wServicePackMajor;
    WORD wServicePackMinor;
    WORD wSuiteMask;
    BYTE wProductType;
    BYTE wReserved;
};

inline BOOL SetRect(RECT* r, int left, int top, int right, int bottom)
{
    r->left = left;
    r->top = top;
    r->right = right;
    r->bottom = bottom;
    return TRUE;
}

inline BOOL SetRectEmpty(RECT* r) { return SetRect(r, 0, 0, 0, 0); }

inline BOOL CopyRect(RECT* dst, const RECT* src)
{
    *dst = *src;
    return TRUE;
}

inline BOOL EqualRect(const RECT* a, const RECT* b)
{
    return a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom;
}

inline BOOL IsRectEmpty(const RECT* r) { return r->right <= r->left || r->bottom <= r->top; }

// Right and bottom edges are exclusive.
inline BOOL PtInRect(const RECT* r, POINT p)
{
    return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom;
}

inline BOOL OffsetRect(RECT* r, int dx, int dy)
{
    r->left += dx;
    r->right += dx;
    r->top += dy;
    r->bottom += dy;
    return TRUE;
}

inline BOOL InflateRect(RECT* r, int dx, int dy)
{
    r->left -= dx;
    r->right += dx;
    r->top -= dy;
    r->bottom += dy;
    return TRUE;
}

inline BOOL IntersectRect(RECT* out, const RECT* a, const RECT* b)
{
    RECT r;
    r.left = a->left > b->left ? a->left : b->left;
    r.top = a->top > b->top ? a->top : b->top;
    r.right = a->right < b->right ? a->right : b->right;
    r.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (IsRectEmpty(&r)) {
        SetRectEmpty(out);
        return FALSE;
    }
    *out = r;
    return TRUE;
}

inline BOOL UnionRect(RECT* out, const RECT* a, const RECT* b)
{
    const BOOL aEmpty = IsRectEmpty(a);
    const BOOL bEmpty = IsRectEmpty(b);
    if (aEmpty && bEmpty) {
        SetRectEmpty(out);
        return FALSE;
    }
    RECT r;
    if (aEmpty) {
        r = *b;
    } else if (bEmpty) {
        r = *a;
    } else {
        r.left = a->left < b->left ? a->left : b->left;
        r.top = a->top < b->top ? a->top : b->top;
        r.right = a->right > b->right ? a->right : b->right;
        r.bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
    }
    *out = r;
    return TRUE;
}

#define RGB(r, g, b) ((COLORREF)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) ((BYTE)((rgb) & 0xFF))
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))
#define GetBValue(rgb) ((BYTE)(((rgb) >> 16) & 0xFF))

#define LOWORD(l) ((WORD)((DWORD)(l) & 0xFFFF))
#define HIWORD(l) ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOBYTE(w) ((BYTE)((w) & 0xFF))
#define HIBYTE(w) ((BYTE)(((w) >> 8) & 0xFF))
#define MAKEWORD(a, b) ((WORD)(((BYTE)((a) & 0xFF)) | ((WORD)((BYTE)((b) & 0xFF))) << 8))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELONG(a, b) ((LONG)(((WORD)((a) & 0xFFFF)) | ((DWORD)((WORD)((b) & 0xFFFF))) << 16))

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
};
typedef const GUID& REFIID;
typedef const GUID& REFGUID;

extern const GUID GUID_NULL;

inline BOOL IsEqualGUID(const GUID& a, const GUID& b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }

// Always defines the constant; use from a single .cpp file.
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern const GUID name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_NUMLOCK 0x90
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_8 0xDF
#define VK_PROCESSKEY 0xE5

#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUERYENDSESSION 0x0011
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_INPUTLANGCHANGE 0x0051
#define WM_DISPLAYCHANGE 0x007E
#define WM_SYSKEYDOWN 0x0104
#define WM_IME_STARTCOMPOSITION 0x010D
#define WM_IME_ENDCOMPOSITION 0x010E
#define WM_IME_COMPOSITION 0x010F
#define WM_MOUSEWHEEL 0x020A
#define WHEEL_DELTA 120
#define WM_IME_SETCONTEXT 0x0281
#define WM_IME_NOTIFY 0x0282
#define WM_IME_CONTROL 0x0283
#define WM_IME_COMPOSITIONFULL 0x0284
#define WM_USER 0x0400
#ifndef NOMINMAX
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
