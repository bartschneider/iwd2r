# iwd2r Portability Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the decompiled Icewind Dale II engine build and reach its start screen on Linux (GCC, Clang) and Windows (MSVC) with MFC, DirectX and all other Win32 dependencies replaced by subset shims, so that every later project works on a native cross-platform build.

**Architecture:** Decompiled code under `src/` keeps its MFC and Win32 shape and includes only `compat/` headers. `compat/` reimplements the MFC classes, Windows typedefs and INI functions on the C++17 standard library. `platform/` implements the Win32, DirectDraw and DirectSound functions the engine calls, on SDL3 and miniaudio, and delivers synthesized window messages to the engine's own window procedure. Multiplayer transport is cut out of `CNetwork`, leaving the original's null-provider single-player path.

**Tech Stack:** C++17, CMake 3.28 with presets, SDL3 `release-3.4.18`, miniaudio `0.11.25`, Catch2 `v3.16.1`, vendored zlib, Python 3 (standard library) for tooling, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-12-iwd2r-portability-foundation-design.md` (read it first; section numbers below refer to it).

## Global Constraints

- Project, executable and CMake target name: `iwd2r`. CMake minimum 3.28. C++17, no extensions.
- Dependencies by FetchContent at exactly: SDL3 `release-3.4.18`, miniaudio `0.11.25`, Catch2 `v3.16.1`. zlib stays in `third_party/`. `third_party/directplay` is deleted.
- `src/` includes only headers from `compat/` and the interface headers of `platform/`. `compat/` depends on the C++17 standard library only. Only `platform/*.cpp` may include SDL3 or miniaudio headers.
- No file in the repository includes `<windows.h>`, `<afx*.h>`, `<ddraw.h>`, `<dsound.h>`, `<dplay.h>`, `<mmsystem.h>`, `<gl/gl.h>`, `<imm.h>` or `<winsock2.h>`. `compat/win_types.h` is the only source of `DWORD`, `LONG`, `BOOL`, `RECT` and the other Windows typedefs on every platform.
- Typedef widths: `DWORD`, `ULONG`, `UINT` = `uint32_t`; `LONG`, `INT`, `BOOL` = `int32_t`; `BOOLEAN`, `BYTE` = `uint8_t`; `WORD`, `USHORT` = `uint16_t`; `SHORT` = `int16_t`; `WPARAM` = `uintptr_t`; `LPARAM`, `LRESULT` = `intptr_t`; `DWORD_PTR` = `uintptr_t`. `MAX_PATH` = 260. `VK_*` and `WM_*` keep their Windows values.
- `-fsigned-char` on GCC and Clang. `compat/`, `platform/`, `tools/`, `tests/` build with `-Wall -Wextra -Werror` (MSVC: `/W4 /WX`). `src/` builds with warnings allowed.
- Every upstream file modified gets a first line `// Modified for iwd2r; see NOTICE.md`. Every new file starts with `// iwd2r. Distributed under LICENSE.md.` (Python files: `# iwd2r. Distributed under LICENSE.md.`).
- Decompiled function bodies are edited only where a shim cannot absorb the difference; each such edit is the smallest change that compiles, and the `// 0x...` address comments are never removed.
- No game data, binary or IWD2EE file is ever committed. Tests needing data carry the Catch2 tag `[data]` and call `SKIP()` when `IWD2_DATA` is unset.
- Commits are small, one per task step that says "Commit", with a one-line imperative subject.

## Review Focus

Inputs the spec implies but no task's tests fully exercise, most likely to bite first. Each has a pinning test added in the task named.

1. An INI value with surrounding spaces and a quoted string such as `Name = "Icewind Dale II" ` must return `Icewind Dale II` without quotes or padding; a value `Width=800 ; comment` must return `800 ; comment` (Win32 does not strip trailing comments). Pinned in Task 10.
2. A Windows path from the installer's INI that points outside the install prefix, such as `D:\Backup\save.gam`, must resolve relative to `IWD2_DATA` rather than be used verbatim or fail. Pinned in Task 12.
3. Two files in one directory differing only by case on a case-sensitive filesystem: the resolver must prefer an exact-case match and otherwise pick the first in sorted order deterministically. Pinned in Task 12.
4. `CString::Format` with `%s` and a `CString` argument whose buffer is reallocated by a concurrent `+=` on the same object is undefined in MFC too, but `Format` where the destination is also an argument (`s.Format("%s!", s)`) works in MFC because formatting goes through a temporary. Pinned in Task 3.
5. A blit whose source rectangle lies partly outside the source surface must clip rather than read out of bounds, and a lock with a rectangle at the surface edge must return a pointer to that rectangle's first pixel. Pinned in Task 14.

---

## File Structure

| Path | Responsibility |
|---|---|
| `CMakeLists.txt`, `CMakePresets.json` | Top-level build: targets `iwd2r_compat`, `iwd2r_platform`, `iwd2r_engine` (object library of `src/`), `iwd2r`, `iwd2r_tests`; FetchContent; presets. |
| `compat/win_types.h` | Windows typedefs, `POINT`/`SIZE`/`RECT` and helpers, `GUID`, `HRESULT` macros, `VK_*`, `WM_*`, `WAIT_*`, calling-convention macros, `min`/`max`, MSVC CRT aliases. |
| `compat/mfc_string.h/.cpp` | `CString`. |
| `compat/mfc_geometry.h` | `CPoint`, `CSize`, `CRect`. |
| `compat/mfc_list.h` | `CPtrList`, `CObList`, `CList`, `CTypedPtrList`, `POSITION`. |
| `compat/mfc_array.h` | `CArray`, `CPtrArray`, `CTypedPtrArray`, `CDWordArray`. |
| `compat/mfc_map.h` | `CMapWordToPtr`, `CMapStringToString`. |
| `compat/mfc_file.h/.cpp` | `CFile`, `CFileStatus`. |
| `compat/mfc_sync.h` | `CCriticalSection`, `CSingleLock`, `CMutex`, `CEvent`. |
| `compat/mfc_time.h/.cpp` | `CTime`. |
| `compat/mfc_wnd.h` | `CWnd`. |
| `compat/mfc_resource.h/.cpp` | The string table from `src/resource.rc`; `LoadStringA`. |
| `compat/path_hook.h/.cpp` | Resolver hook `compat::ResolvePath`, installed by `platform/fs`. |
| `compat/ini.h/.cpp` | `GetPrivateProfileStringA`, `GetPrivateProfileIntA`, `WritePrivateProfileStringA`. |
| `compat/mfc.h` | Aggregator included by `src/`; also includes `platform/win32_api.h`. |
| `platform/win32_api.h` | Declarations of every Win32-named function the engine calls, grouped by implementing file. |
| `platform/threads.cpp` | `_beginthread`, handles, events, semaphores, mutex, `CRITICAL_SECTION`, `WaitForSingleObject`, `CloseHandle`. |
| `platform/clock.h/.cpp` | `platform::Clock` source interface; `GetTickCount`, `timeGetTime`, `Sleep`, `SleepEx`, `timeSetEvent`, `timeKillEvent`, `timeBeginPeriod`, `timeEndPeriod`. |
| `platform/fs.h/.cpp` | Path resolver (case folding, drive-letter re-rooting), `FindFirstFileA` family, attributes, copy, delete, directories, free space, current directory. |
| `platform/data_dir.h/.cpp` | `platform::ResolveDataDir`. |
| `platform/ddraw_shim.h/.cpp` | DirectDraw subset on software surfaces; presents through SDL. |
| `platform/dsound_shim.h/.cpp` | DirectSound subset on miniaudio; defines `MINIAUDIO_IMPLEMENTATION`. |
| `platform/window.h/.cpp` | SDL window, message queue, synthesized `WM_*` delivery to the engine's `WNDPROC`, polled input, cursor, metrics, message box. |
| `platform/win32_stubs.cpp` | Library loading, version info, GDI, COM, IME, memory status: all inert. |
| `platform/gl_compat.h/.cpp` | `GL_*` constants and no-op `gl*`, `glu*`, `wgl*` functions. |
| `platform/entry.cpp` | `main` via `SDL_main.h`; calls `iwd2r_main`. |
| `src/main.cpp` | `iwd2r_main(argc, argv)` replacing `WinMain`. |
| `tests/*.cpp` | Catch2 tests, one file per shim area; `tests/smoke_start_screen.cpp` is the `[data]` test. |
| `tools/gen_map.py`, `tools/check_notices.py` | Map generator; notice-line checker against upstream commit `4fa36bc`. |
| `docs/adr/0001-platform-layer.md`, `docs/adr/0002-single-player-only.md`, `docs/MAP.md`, `NOTICE.md`, `README.md` | Governance. |
| `.github/workflows/ci.yml` | Replaces `ci-build.yml`. |

Deleted: `src/mfc.h`, `src/resource.rc`, `src/CScreenMultiPlayer.cpp/.h`, `third_party/directplay/`, `.github/workflows/ci-build.yml`.

Discovery commands used by several tasks (run from the repo root):

```bash
# VK codes the engine references
grep -rhoE '\bVK_[A-Z0-9_]+' src | sort -u
# OpenGL functions and constants
grep -rhoE '\b(gl|glu|wgl)[A-Z][A-Za-z0-9]*\(' src | sort -u ; grep -rhoE '\bGL_[A-Z0-9_]+' src | sort -u
# DirectDraw / DirectSound constants
grep -rhoE '\b(DD|DDS|DDSD|DDLOCK|DDBLT|DDBLTFAST|DDCKEY|DDERR|DDSCL|DDPF|DDFLIP|DDCAPS|DDSCAPS|DDPCAPS|DDWAITVB)_[A-Z0-9_]+' src | sort -u
grep -rhoE '\b(DS|DSB|DSBCAPS|DSBPLAY|DSBSTATUS|DSBVOLUME|DSBPAN|DSBFREQUENCY|DSSCL|DSERR|DSBLOCK|DSCAPS|DS3DMODE)_[A-Z0-9_]+' src | sort -u
# C-style DirectSound macros used by src/music
grep -rhoE '\bIDirectSound[A-Za-z0-9]*_[A-Za-z]+\(' src | sort -u
# DirectDraw/DirectSound methods called, by name
grep -rhoE '[A-Za-z0-9_\]\)]+->[A-Z][A-Za-z0-9]+\(' src/CVid*.cpp src/CVideo*.cpp src/CInfinity.cpp src/CSound*.cpp src/CVoice.cpp src/music/*.cpp | sed -E 's/.*->//' | sort | uniq -c | sort -rn
```

Numeric values for Windows, DirectDraw and DirectSound constants and struct layouts come from public documentation or the MinGW-w64 headers, which are permissively licensed. Never copy from Wine or the Windows SDK.

---

### Task 1: Build skeleton and test harness

**Files:**
- Modify: `CMakeLists.txt` (full rewrite; keep the `src/` source list for Task 17)
- Create: `CMakePresets.json`, `compat/CMakeLists.txt`, `platform/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/test_sanity.cpp`, `compat/version.cpp`
- Modify: `.gitignore` (add `build/`, `build-*/`)

**Interfaces:**
- Produces: targets `iwd2r_compat` (STATIC, sources under `compat/`), `iwd2r_platform` (STATIC, sources under `platform/`, links `iwd2r_compat`, `SDL3::SDL3-static`, miniaudio include dir), `iwd2r_tests` (links both, Catch2::Catch2WithMain). The engine object library and executable are added in Task 17. Presets: `linux-gcc`, `linux-clang` (Ninja, RelWithDebInfo), `windows-msvc-x64` (generator "Visual Studio 17 2022", `-A x64`), `windows-msvc-x86` (`-A Win32`). Each preset has a matching build preset and test preset. Function `iwd2r_strict_warnings(target)` applies `-Wall -Wextra -Werror` or `/W4 /WX`. Compile option `-fsigned-char` on non-MSVC is applied globally.
- Consumes: nothing.

- [ ] **Step 1: Write the failing test**

`tests/test_sanity.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include "compat/version.h"
TEST_CASE("version string is iwd2r", "[sanity]") {
    REQUIRE(std::string(iwd2r_version()) == "iwd2r 0.1.0");
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cmake --preset linux-gcc && cmake --build --preset linux-gcc`
Expected: FAIL (no CMakePresets.json, or `compat/version.h` missing).

- [ ] **Step 3: Write the build files and `const char* iwd2r_version()` in `compat/version.h/.cpp`**

`CMakeLists.txt`: `project(iwd2r VERSION 0.1.0 LANGUAGES C CXX)`, C++17, FetchContent for the three dependencies at the pinned tags with `SDL_SHARED OFF`, `SDL_STATIC ON`, `SDL_TEST_LIBRARY OFF`, miniaudio exposed as an INTERFACE target `miniaudio` with its include directory, `add_subdirectory(third_party/zlib)`, then `compat`, `platform`, `tests`. `enable_testing()` and `catch_discover_tests(iwd2r_tests)`. The presets file defines the four configure presets with `binaryDir` `build/${presetName}`.

- [ ] **Step 4: Run the test**

Run: `cmake --preset linux-gcc && cmake --build --preset linux-gcc && ctest --preset linux-gcc --output-on-failure`
Expected: 1 test passed. Also `cmake --preset linux-clang` builds (install `clang` and `ninja-build` if missing: `sudo apt-get install -y cmake ninja-build clang`; SDL3 needs the dev packages listed in Task 25's CI step, install them now too).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt CMakePresets.json compat platform tests .gitignore
git commit -m "Add iwd2r build skeleton with Catch2 tests and presets"
```

---

### Task 2: Windows typedefs and helpers

**Files:**
- Create: `compat/win_types.h`, `compat/win_types.cpp` (for `GUID_NULL`), `tests/test_win_types.cpp`

**Interfaces:**
- Produces: the typedefs in Global Constraints; `TRUE`, `FALSE`, `NULL`; `struct POINT{LONG x,y;}`, `struct SIZE{LONG cx,cy;}`, `struct RECT{LONG left,top,right,bottom;}` with `LPPOINT`, `LPRECT`, `LPSIZE`; functions `SetRect`, `SetRectEmpty`, `CopyRect`, `EqualRect`, `IsRectEmpty`, `PtInRect`, `OffsetRect`, `InflateRect`, `IntersectRect`, `UnionRect` with Win32 signatures and return rules (`IntersectRect` and `UnionRect` return `FALSE` and set an empty rect when the result is empty); `COLORREF`, `RGB`, `GetRValue`, `GetGValue`, `GetBValue`; `LOWORD`, `HIWORD`, `LOBYTE`, `HIBYTE`, `MAKELONG`, `MAKEWORD`; `HRESULT`, `S_OK`, `S_FALSE`, `E_FAIL`, `E_NOINTERFACE`, `E_OUTOFMEMORY`, `SUCCEEDED`, `FAILED`; `struct GUID`, `REFIID`, `GUID_NULL`, `IsEqualGUID`, `DEFINE_GUID(name, l, w1, w2, b1..b8)` which always defines the constant; `HANDLE`, `HWND`, `HINSTANCE`, `HMODULE`, `HDC`, `HFONT`, `HGDIOBJ`, `HCURSOR`, `HKL`, `HIMC` as distinct opaque pointer typedefs; `INFINITE`, `WAIT_OBJECT_0` (0), `WAIT_ABANDONED` (0x80), `WAIT_TIMEOUT` (0x102); `CALLBACK`, `WINAPI`, `APIENTRY`, `__stdcall`, `__cdecl` (empty off MSVC); `__declspec(noreturn)` mapped to `[[noreturn]]` off MSVC via macro `IWD2R_NORETURN` only if the one use needs it; every `VK_*` from the discovery command and every `WM_*` the engine references (`grep -rhoE '\bWM_[A-Z_]+' src | sort -u`) plus `WM_QUIT`, `WM_USER`; `MAX_PATH`; `min`/`max` macros guarded by `#ifndef NOMINMAX`; `_stricmp`, `_strnicmp`, `_vsnprintf`, `_snprintf`, `_chdir`, `_getcwd`, `_mkdir`, `_access` mapped to POSIX names off MSVC; `MMRESULT`, `UINT_PTR`, `LPVOID`, `LPCVOID`, `LPSTR`, `LPCSTR`, `CHAR`, `UCHAR`, `FLOAT`, `size_t` includes.
- Consumes: nothing.

- [ ] **Step 1: Write the failing tests**

`tests/test_win_types.cpp` with `TEST_CASE("typedef widths", "[win_types]")` asserting `sizeof(DWORD)==4`, `sizeof(LONG)==4`, `sizeof(BOOL)==4`, `sizeof(BOOLEAN)==1`, `sizeof(WORD)==2`, `sizeof(WPARAM)==sizeof(void*)`; `TEST_CASE("rect helpers")`: `IntersectRect` of `{0,0,10,10}` and `{5,5,20,20}` gives `{5,5,10,10}` and returns `TRUE`; of `{0,0,10,10}` and `{10,0,20,10}` returns `FALSE` and leaves `{0,0,0,0}`; `PtInRect({0,0,10,10},{10,10})` is `FALSE` (right and bottom are exclusive); `UnionRect` of an empty and a non-empty rect is the non-empty one; `TEST_CASE("guid")`: `IsEqualGUID(GUID_NULL, GUID{})` is true and a `DEFINE_GUID`'d value differs from `GUID_NULL`; `TEST_CASE("vk and wm values")`: `VK_LBUTTON==1`, `VK_ESCAPE==0x1B`, `VK_OEM_1==0xBA`, `WM_CLOSE==0x10`, `WM_MOUSEWHEEL==0x20A`, `WM_SYSKEYDOWN==0x104`; `TEST_CASE("min max macros")`: `min(3, 5L) == 3` compiles with mixed types.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R win_types`
Expected: compile failure, header missing.

- [ ] **Step 3: Implement `compat/win_types.h` and `win_types.cpp`**

The test file defines `NOMINMAX` before including Catch2 and includes `win_types.h` after, except for the `min max` test case which is in its own TU without `NOMINMAX`.

- [ ] **Step 4: Run the tests**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R win_types --output-on-failure`
Expected: all pass. Repeat with `--preset linux-clang`.

- [ ] **Step 5: Commit**

```bash
git add compat/win_types.h compat/win_types.cpp tests/test_win_types.cpp tests/CMakeLists.txt compat/CMakeLists.txt
git commit -m "Add Windows typedefs and helpers to compat"
```

---

### Task 3: CString

**Files:**
- Create: `compat/mfc_string.h`, `compat/mfc_string.cpp`, `tests/test_cstring.cpp`

**Interfaces:**
- Produces: `class CString` with one data member `char* m_pchData` pointing at the first character of a buffer whose header (`struct CStringData { int32_t nRefs; int32_t nDataLength; int32_t nAllocLength; }`) precedes it; an empty string points at a shared static empty buffer and never frees it. `static_assert(sizeof(CString) == sizeof(void*))`. Methods (spec 6.2): constructors `CString()`, `CString(const CString&)`, `CString(const char*)`, `CString(const char*, int len)`, `CString(char ch, int repeat = 1)`; `~CString`; `operator=(const CString&)`, `operator=(const char*)`, `operator=(char)`; `operator const char*() const`; `int GetLength() const`; `BOOL IsEmpty() const`; `void Empty()`; `char GetAt(int) const`; `char operator[](int) const`; `void SetAt(int, char)`; `int Compare(const char*) const`; `int CompareNoCase(const char*) const`; `CString Mid(int first) const`, `CString Mid(int first, int count) const`, `CString Left(int) const`, `CString Right(int) const`; `int Find(char) const`, `int Find(const char*) const`, `int Find(char, int start) const`, `int Find(const char*, int start) const`; `int FindOneOf(const char*) const`; `int ReverseFind(char) const`; `void TrimLeft()`, `void TrimLeft(char)`, `void TrimLeft(const char*)`, same three for `TrimRight`; `void MakeUpper()`, `void MakeLower()`, `void MakeReverse()`; `char* GetBuffer(int minLen)`, `void ReleaseBuffer(int newLen = -1)`, `char* GetBufferSetLength(int)`; `CString SpanIncluding(const char*) const`, `CString SpanExcluding(const char*) const`; `int Remove(char)`, `int Replace(char, char)`, `int Replace(const char*, const char*)`, `int Insert(int, char)`, `int Insert(int, const char*)`, `int Delete(int index, int count = 1)`; `void FormatV(const char* fmt, va_list)`; `template <class... Args> void Format(const char* fmt, Args&&... args)` which forwards each argument through `compat::detail::FormatArg` (a `CString` or `const CString&` becomes `const char*`; everything else is passed unchanged after decay) to `snprintf`; operators `+=` for `CString`, `const char*`, `char`; free `operator+` for the six combinations; `==`, `!=`, `<`, `>`, `<=`, `>=` for `CString`/`const char*` on either side using `strcmp`. Also `typedef const char* LPCTSTR` aliases used by the tree (`LPCTSTR`, `LPTSTR`).
- Consumes: `compat/win_types.h`.

- [ ] **Step 1: Write the failing tests**

`tests/test_cstring.cpp`, one `TEST_CASE` per group, all tagged `[cstring]`:
- layout: `sizeof(CString) == sizeof(void*)`; `CString().GetLength() == 0`; copy of a string shares nothing: modify copy, original unchanged.
- clamping: `CString("abc").Mid(1) == "bc"`, `.Mid(5) == ""`, `.Mid(1, 50) == "bc"`, `.Left(50) == "abc"`, `.Left(-1) == ""`, `.Right(2) == "bc"`, `.Right(50) == "abc"`.
- find: `CString("a,b,c").Find(',') == 1`, `.Find(',', 2) == 3`, `.Find("c") == 4`, `.Find('x') == -1`, `.FindOneOf("xc") == 4`, `.ReverseFind(',') == 3`, `.Find(',', 99) == -1`.
- trim: `CString("  a b \t").TrimLeft()` gives `"a b \t"`; `TrimRight()` on `"  a b \t"` gives `"  a b"`; `TrimLeft('x')` on `"xxay"` gives `"ay"`; `TrimRight("yx")` on `"axyxy"` gives `"a"`.
- compare: `CString("ABC").CompareNoCase("abc") == 0`; `CString("abc").Compare("abd") < 0`; `CString("a") == "a"`; `"a" == CString("a")`.
- case: `MakeUpper` on `"aBc1"` gives `"ABC1"`; `MakeLower` gives `"abc1"`.
- format: `s.Format("%d-%s-%c", 7, CString("x"), 'y')` gives `"7-x-y"`; `s.Format("%s!", s)` where `s == "hi"` gives `"hi!"` (Review Focus 4); `s.Format("%5.2f", 3.14159)` gives `" 3.14"`; `s.Format("%s", "")` gives `""`; a 5000-character `%s` argument round-trips.
- buffer: `char* p = s.GetBuffer(10); strcpy(p, "abc"); s.ReleaseBuffer(); s == "abc" && s.GetLength() == 3`; `ReleaseBuffer(2)` gives `"ab"`.
- span and edit: `CString("abc123").SpanIncluding("abc") == "abc"`, `.SpanExcluding("12") == "abc"`; `Remove('b')` on `"abcb"` returns 2 and gives `"ac"`; `Replace("ab", "x")` on `"abab"` returns 2 and gives `"xx"`; `Delete(1, 2)` on `"abcd"` gives `"ad"`; `Insert(1, "XY")` on `"ad"` gives `"aXYd"`.
- operators: `CString("a") + "b" + CString("c") + 'd' == "abcd"`; `+=` each type; `GetAt(1)` and `SetAt(1, 'Z')`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R cstring`
Expected: compile failure.

- [ ] **Step 3: Implement `compat/mfc_string.h/.cpp`**

Allocation: `malloc` of header plus `nAllocLength + 1` bytes, grown geometrically; `Format` computes the size with a first `vsnprintf(nullptr, 0, ...)` into a temporary, then assigns, so the destination may appear among the arguments.

- [ ] **Step 4: Run the tests**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R cstring --output-on-failure` and the same with `linux-clang`.
Expected: all pass, no warnings.

- [ ] **Step 5: Commit**

```bash
git add compat/mfc_string.h compat/mfc_string.cpp tests/test_cstring.cpp compat/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add CString shim with variadic Format"
```

---

### Task 4: CPoint, CSize, CRect

**Files:**
- Create: `compat/mfc_geometry.h`, `tests/test_geometry.cpp`

**Interfaces:**
- Produces: `class CPoint : public POINT`, `class CSize : public SIZE`, `class CRect : public RECT`. `CPoint`: constructors `()`, `(int, int)`, `(POINT)`, `(SIZE)`, `(DWORD packed)` (low word x, high word y); `Offset(int, int)`, `Offset(POINT)`, `Offset(SIZE)`; `==`, `!=`, `+=`, `-=` with `POINT` and `SIZE`; `+`, `-` giving `CPoint`/`CSize` (point minus point is `CSize`). `CSize`: `()`, `(int, int)`, `(SIZE)`, `(POINT)`, `(DWORD)`; `==`, `!=`, `+=`, `-=`, `+`, `-`, unary `-`. `CRect`: `()`, `(int l, int t, int r, int b)`, `(const RECT&)`, `(const RECT*)`, `(POINT topLeft, SIZE size)`, `(POINT, POINT)`; `Width()`, `Height()`, `Size()`, `TopLeft()`, `BottomRight()` (by reference as in MFC), `CenterPoint()`; `IsRectEmpty()`, `IsRectNull()`, `PtInRect(POINT)`; `SetRect(int,int,int,int)`, `SetRect(POINT, POINT)`, `SetRectEmpty()`, `CopyRect(const RECT*)`, `EqualRect(const RECT*)`; `InflateRect(int, int)`, `InflateRect(SIZE)`, `InflateRect(const RECT*)`, `InflateRect(int,int,int,int)`, `DeflateRect` same four; `OffsetRect(int,int)`, `OffsetRect(POINT)`, `OffsetRect(SIZE)`; `NormalizeRect()`; `BOOL IntersectRect(const RECT*, const RECT*)`, `BOOL UnionRect(const RECT*, const RECT*)`; `operator const RECT*() const` (and non-const), `==`, `!=`, `+=`/`-=` with `POINT`, `SIZE`, `const RECT*`, `&=`, `|=`, and `+`, `-`, `&`, `|`.
- Consumes: Task 2.

- [ ] **Step 1: Write the failing tests**

`[geometry]` cases: `CRect(0,0,10,20).Width()==10` and `.Height()==20`; `CRect(10,10,0,0).NormalizeRect()` gives `{0,0,10,10}`; `CRect(0,0,10,10) & CRect(5,5,20,20) == CRect(5,5,10,10)`; `CRect(0,0,10,10) | CRect(20,20,30,30) == CRect(0,0,30,30)`; `CRect(0,0,10,10).PtInRect(CPoint(9,9))` true and `CPoint(10,9)` false; `CPoint(1,2) + CSize(3,4) == CPoint(4,6)`; `CPoint(4,6) - CPoint(1,2) == CSize(3,4)`; `CPoint(DWORD(0x00020001)) == CPoint(1,2)`; `CRect r(1,1,2,2); r.InflateRect(1,2)` gives `{0,-1,3,4}`; `CRect(1,1,2,2).OffsetRect(CPoint(5,5)) == CRect(6,6,7,7)`; `CRect r; RECT* p = r;` compiles; `sizeof(CRect)==sizeof(RECT)`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R geometry`
Expected: compile failure.

- [ ] **Step 3: Implement `compat/mfc_geometry.h`**

Header-only, inline.

- [ ] **Step 4: Run the tests**

Run: `ctest --preset linux-gcc -R geometry --output-on-failure` after building, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/mfc_geometry.h tests/test_geometry.cpp tests/CMakeLists.txt
git commit -m "Add CPoint, CSize and CRect shims"
```

---

### Task 5: Lists and POSITION

**Files:**
- Create: `compat/mfc_list.h`, `tests/test_list.cpp`

**Interfaces:**
- Produces: `struct __POSITION; typedef __POSITION* POSITION;` where a `POSITION` is the address of a list node. `template <class TYPE, class ARG_TYPE = const TYPE&> class CList` with: `CList(int blockSize = 10)`; `int GetCount() const`, `int GetSize() const`, `BOOL IsEmpty() const`; `TYPE& GetHead()`, `const TYPE& GetHead() const`, same for `GetTail`; `TYPE RemoveHead()`, `TYPE RemoveTail()`; `POSITION AddHead(ARG_TYPE)`, `POSITION AddTail(ARG_TYPE)`, `void AddHead(CList*)`, `void AddTail(CList*)`; `void RemoveAll()`; `POSITION GetHeadPosition() const`, `POSITION GetTailPosition() const`; `TYPE& GetNext(POSITION&)`, `const TYPE& GetNext(POSITION&) const`, same for `GetPrev` (advance then return the element at the old position; position becomes `NULL` past the end); `TYPE& GetAt(POSITION)`, `const TYPE& GetAt(POSITION) const`, `void SetAt(POSITION, ARG_TYPE)`; `void RemoveAt(POSITION)`; `POSITION InsertBefore(POSITION, ARG_TYPE)`, `POSITION InsertAfter(POSITION, ARG_TYPE)`; `POSITION Find(ARG_TYPE searchValue, POSITION startAfter = NULL) const` (compares with `==`); `POSITION FindIndex(int) const`. `class CPtrList : public CList<void*, void*>`, `class CObList : public CList<class CObject*, class CObject*>` with `class CObject { public: virtual ~CObject() {} }`, `template <class BASE_CLASS, class TYPE> class CTypedPtrList : public BASE_CLASS` re-exposing every method with `TYPE` in place of `void*`, including `CTypedPtrList<CPtrList, POSITION>`.
- Consumes: Task 2.

- [ ] **Step 1: Write the failing tests**

`[list]` cases with `CTypedPtrList<CPtrList, int*>` and `CList<DWORD, DWORD>`: iteration order with `GetHeadPosition`/`GetNext` visits `1,2,3`; `GetNext` leaves `pos == NULL` after the tail; removing while iterating in the engine's pattern (`POSITION pos = l.GetHeadPosition(); while (pos) { POSITION cur = pos; int* v = l.GetNext(pos); if (*v == 2) l.RemoveAt(cur); }`) leaves `1,3`; `Find(v2)` returns the position whose `GetAt` is `v2`, `Find(v3, posOf2)` finds `3`, `Find(v1, posOf1)` returns `NULL`; `FindIndex(1)` is the second element and `FindIndex(5)` is `NULL`; `InsertBefore(head, x)` makes `x` the head; `RemoveHead` returns the value and shrinks; `AddTail(&other)` appends all; `CTypedPtrList<CPtrList, POSITION>` stores and returns positions; `GetCount` after `RemoveAll` is 0.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R '^list'`
Expected: compile failure.

- [ ] **Step 3: Implement `compat/mfc_list.h`**

Doubly linked nodes allocated with `new`; `POSITION` is `reinterpret_cast<POSITION>(node)`.

- [ ] **Step 4: Run the tests**

Run: build and `ctest --preset linux-gcc -R '^list' --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/mfc_list.h tests/test_list.cpp tests/CMakeLists.txt
git commit -m "Add MFC list shims with POSITION semantics"
```

---

### Task 6: Arrays and maps

**Files:**
- Create: `compat/mfc_array.h`, `compat/mfc_map.h`, `tests/test_array_map.cpp`

**Interfaces:**
- Produces: `template <class TYPE, class ARG_TYPE = const TYPE&> class CArray` with `GetSize()`, `GetCount()`, `GetUpperBound()`, `SetSize(int newSize, int growBy = -1)`, `FreeExtra()`, `RemoveAll()`, `GetAt(int)`, `SetAt(int, ARG_TYPE)`, `ElementAt(int)`, `GetData()`, `SetAtGrow(int, ARG_TYPE)`, `Add(ARG_TYPE)` returning the new index, `Append(const CArray&)`, `Copy(const CArray&)`, `operator[]` both forms, `InsertAt(int, ARG_TYPE, int count = 1)`, `RemoveAt(int, int count = 1)`. `class CPtrArray : public CArray<void*, void*>`, `class CDWordArray : public CArray<DWORD, DWORD>`, `template <class BASE_CLASS, class TYPE> class CTypedPtrArray : public BASE_CLASS` re-exposing with `TYPE`. `class CMapWordToPtr` and `class CMapStringToString` with `BOOL Lookup(key, value&) const`, `void SetAt(key, value)`, `value& operator[](key)`, `BOOL RemoveKey(key)`, `void RemoveAll()`, `int GetCount() const`, `POSITION GetStartPosition() const`, `void GetNextAssoc(POSITION&, key&, value&) const` iterating a stable snapshot order; `CMapStringToString` keys compare case-sensitively.
- Consumes: Tasks 2, 3, 5 (`POSITION`).

- [ ] **Step 1: Write the failing tests**

`[array]`: `SetAtGrow(5, 7)` on an empty `CArray<int,int>` gives size 6 with `[5]==7` and `[0]==0`; `Add` returns 0 then 1; `GetUpperBound()==GetSize()-1`; `InsertAt(0, 9)` shifts; `RemoveAt(0, 2)` shrinks by 2; `CTypedPtrArray<CPtrArray, int*>` returns `int*`. `[map]`: `SetAt(3, p); Lookup(3, q)` gives `q==p`; `Lookup(4, q)` is `FALSE`; `RemoveKey(3)` then `Lookup` false; `CMapStringToString` with `"A"` and `"a"` are two keys; iteration with `GetStartPosition`/`GetNextAssoc` visits every pair once.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R 'array|map'`
Expected: compile failure.

- [ ] **Step 3: Implement both headers**

`CArray` over `std::vector<TYPE>`; maps over `std::unordered_map` plus a `std::vector` of keys for iteration order.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R 'array|map' --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/mfc_array.h compat/mfc_map.h tests/test_array_map.cpp tests/CMakeLists.txt
git commit -m "Add MFC array and map shims"
```

---

### Task 7: Synchronization and threads

**Files:**
- Create: `compat/mfc_sync.h`, `platform/win32_api.h` (threads section), `platform/threads.cpp`, `tests/test_sync.cpp`

**Interfaces:**
- Produces in `compat/mfc_sync.h`: `class CCriticalSection { public: BOOL Lock(); BOOL Lock(DWORD timeout); BOOL Unlock(); }` on `std::recursive_mutex`; `class CMutex` and `class CEvent` with the same `Lock`/`Unlock` (`CEvent(BOOL initiallyOwn = FALSE, BOOL manualReset = FALSE)`, `SetEvent()`, `ResetEvent()`, `PulseEvent()`); `class CSyncObject` base; `class CSingleLock { public: CSingleLock(CSyncObject*, BOOL initialLock = FALSE); ~CSingleLock(); BOOL Lock(DWORD timeout = INFINITE); BOOL Unlock(); BOOL IsLocked(); }`.
- Produces in `platform/win32_api.h` and `threads.cpp`: `uintptr_t _beginthread(void (*start)(void*), unsigned stackSize, void* arg)` returning a `HANDLE` value that `WaitForSingleObject` joins; `DWORD WaitForSingleObject(HANDLE, DWORD ms)` for thread, event, semaphore and mutex handles returning `WAIT_OBJECT_0` or `WAIT_TIMEOUT`; `BOOL CloseHandle(HANDLE)`; `HANDLE CreateEventA(LPVOID attrs, BOOL manualReset, BOOL initialState, LPCSTR name)`, `BOOL SetEvent(HANDLE)`, `BOOL ResetEvent(HANDLE)`; `HANDLE CreateSemaphoreA(LPVOID, LONG initial, LONG maximum, LPCSTR)`, `BOOL ReleaseSemaphore(HANDLE, LONG count, LONG* previous)`; `HANDLE CreateMutexA(LPVOID, BOOL initialOwner, LPCSTR)` returning a new handle each call (no cross-process semantics); `struct CRITICAL_SECTION { void* impl; }` with `InitializeCriticalSection`, `EnterCriticalSection`, `LeaveCriticalSection`, `DeleteCriticalSection`, `TryEnterCriticalSection`; `DWORD GetCurrentThreadId()`. `win32_api.h` includes only `compat/win_types.h`.
- Consumes: Task 2.

- [ ] **Step 1: Write the failing tests**

`[sync]`: `CCriticalSection` locks recursively twice from one thread without deadlock; `CSingleLock(&cs, TRUE).IsLocked()` is `TRUE` and the destructor releases (a second thread can lock afterwards within 100 ms); a manual-reset event created signaled returns `WAIT_OBJECT_0` immediately and still does after a second wait; `ResetEvent` then `WaitForSingleObject(h, 50) == WAIT_TIMEOUT`; `SetEvent` from another thread wakes a waiter; a semaphore created with count 0 times out, `ReleaseSemaphore(h, 1, &prev)` sets `prev == 0` and a wait then succeeds and a second wait times out; `_beginthread` runs the function with its argument and `WaitForSingleObject(h, INFINITE)` returns after the function returns; `CRITICAL_SECTION` recursive enter twice, leave twice, `TryEnterCriticalSection` from another thread while held returns `FALSE`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R sync`
Expected: compile failure.

- [ ] **Step 3: Implement `compat/mfc_sync.h` and `platform/threads.cpp`**

Handles are pointers to a tagged `struct HandleObject { enum Kind; ... }` so `WaitForSingleObject` dispatches on kind. A thread handle holds a `std::thread` joined on wait and detached on `CloseHandle` if still running.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R sync --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/mfc_sync.h platform/win32_api.h platform/threads.cpp tests/test_sync.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add MFC sync classes and Win32 thread, event and semaphore handles"
```

---

### Task 8: CFile and the path hook

**Files:**
- Create: `compat/path_hook.h`, `compat/path_hook.cpp`, `compat/mfc_time.h`, `compat/mfc_time.cpp`, `compat/mfc_file.h`, `compat/mfc_file.cpp`, `tests/test_file.cpp`

**Interfaces:**
- Produces: `namespace compat { using PathResolver = std::string (*)(const char*); void SetPathResolver(PathResolver); std::string ResolvePath(const char*); }` where the default resolver returns its input unchanged. `class CFile` with `enum OpenFlags { modeRead = 0x0000, modeWrite = 0x0001, modeReadWrite = 0x0002, shareCompat = 0x0000, shareExclusive = 0x0010, shareDenyWrite = 0x0020, shareDenyRead = 0x0030, shareDenyNone = 0x0040, modeNoInherit = 0x0080, modeCreate = 0x1000, modeNoTruncate = 0x2000, typeText = 0x4000, typeBinary = 0x8000 }`, `enum SeekPosition { begin = 0, current = 1, end = 2 }`, `static const HANDLE hFileNull`; `CFile()`, `CFile(const char* path, UINT flags)` (throws nothing; use `m_hFile` to test), `virtual ~CFile()` closes; `virtual BOOL Open(const char* path, UINT flags, void* pException = NULL)`; `virtual UINT Read(void*, UINT)`; `virtual void Write(const void*, UINT)`; `virtual LONG Seek(LONG offset, UINT from)`; `void SeekToBegin()`, `DWORD SeekToEnd()`; `virtual DWORD GetLength() const`; `virtual DWORD GetPosition() const`; `virtual void SetLength(DWORD)`; `virtual void Close()`; `virtual void Abort()`; `virtual void Flush()`; `CString GetFilePath() const`, `CString GetFileName() const`; `static BOOL GetStatus(const char*, CFileStatus&)`; `static void SetStatus(const char*, const CFileStatus&)`; `static void Remove(const char*)`; `static void Rename(const char*, const char*)`; member `HANDLE m_hFile`. `struct CFileStatus { CTime m_ctime, m_mtime, m_atime; ULONGLONG m_size; BYTE m_attribute; char m_szFullName[MAX_PATH]; }` with `m_attribute` bits `CFile::readOnly = 0x01, hidden = 0x02, system = 0x04, directory = 0x10, archive = 0x20`. Every path passes through `compat::ResolvePath` first. `modeCreate` without `modeNoTruncate` truncates; `modeCreate | modeNoTruncate` opens or creates. Failed `Open` returns `FALSE`, `Read` past end returns the bytes available, `Seek` past end is allowed. Also `class CTime { public: CTime(); CTime(time_t); static CTime GetCurrentTime(); time_t GetTime() const; int GetYear(), GetMonth(), GetDay(), GetHour(), GetMinute(), GetSecond(), GetDayOfWeek() const; }` and `class CTimeSpan` with `GetTotalSeconds()`, in `compat/mfc_time.h`, since `CFileStatus` holds three `CTime` values; `GetStatus` fills `m_mtime` from the file's modification time. Its tests are in Task 9.
- Consumes: Tasks 2, 3.

- [ ] **Step 1: Write the failing tests**

`[file]` in a temporary directory created with `std::filesystem::temp_directory_path()`: `Open(missing, modeRead)` returns `FALSE` and `m_hFile == CFile::hFileNull`; `Open(path, modeWrite | modeCreate)` then `Write("abc", 3)`, `Close()`, reopen `modeRead | typeBinary`, `GetLength() == 3`, `Read` of 10 returns 3, `Seek(-1, end) == 2`, `GetPosition() == 2`; `modeReadWrite | modeCreate | modeNoTruncate` on the existing file keeps its length; `modeCreate` alone truncates to 0; `GetStatus` fills `m_size == 3` and `directory` bit clear, returns `FALSE` for a missing path; `Remove` deletes; `SetPathResolver` with a resolver that appends `".txt"` makes `Open("name")` open `name.txt`; `SetPathResolver(nullptr)` restores identity.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R file`
Expected: compile failure.

- [ ] **Step 3: Implement the four files**

`CFile` on `FILE*` opened with `fopen` modes derived from the flags; `m_hFile` holds the `FILE*`.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R file --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/path_hook.h compat/path_hook.cpp compat/mfc_time.h compat/mfc_time.cpp compat/mfc_file.h compat/mfc_file.cpp tests/test_file.cpp compat/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add CFile, CTime and the path resolver hook"
```

---

### Task 9: CWnd, resource strings and the mfc.h aggregator

**Files:**
- Create: `compat/mfc_wnd.h`, `compat/mfc_resource.h`, `compat/mfc_resource.cpp`, `compat/mfc.h`, `tests/test_misc_mfc.cpp`

**Interfaces:**
- Produces: `class CWnd { public: BOOL Attach(HWND); HWND Detach(); HWND GetSafeHwnd() const; HWND m_hWnd; }`. `const char* iwd2r_ResourceString(UINT id)` returning the text of the twelve `IDS_*` strings from `src/resource.rc` (keep the exact text including the typo `IDS_EXSLUSIVE_MODE`), `NULL` for unknown ids; `int LoadStringA(HINSTANCE, UINT id, LPSTR buffer, int bufferMax)` copying with truncation and returning the length copied, 0 when unknown. `compat/mfc.h` includes, in order: `win_types.h`, `mfc_string.h`, `mfc_geometry.h`, `mfc_list.h`, `mfc_array.h`, `mfc_map.h`, `mfc_time.h`, `mfc_file.h`, `mfc_sync.h`, `mfc_wnd.h`, `mfc_resource.h`, `ini.h` (Task 10), `../platform/win32_api.h`, and the standard headers `<cstdio>`, `<cstdlib>`, `<cstring>`, `<cstdarg>`, `<cmath>`. It also defines `ASSERT(x)` as a no-op, `TRACE(...)` as a no-op, `VERIFY(x)` as `(void)(x)`, and `AFX_INLINE` as `inline`.
- Consumes: Tasks 2 to 8.

- [ ] **Step 1: Write the failing tests**

`[misc]`: `CTime(0).GetYear() == 1970` in UTC terms is not required; instead `CTime t = CTime::GetCurrentTime(); t.GetTime()` is within 5 seconds of `time(nullptr)`, and `CTime(86400 * 365).GetHour()` equals `localtime`'s hour for that value; `CWnd w; w.GetSafeHwnd() == NULL; w.Attach((HWND)0x10); w.GetSafeHwnd() == (HWND)0x10; w.Detach() == (HWND)0x10; w.GetSafeHwnd() == NULL`; `LoadStringA(NULL, IDS_ENGLISH, buf, 8)` copies 7 characters and returns 7; unknown id returns 0; a TU that includes only `compat/mfc.h` and uses `CString`, `CPtrList`, `CRect`, `CFile`, `CSingleLock`, `GetTickCount` (declared in `win32_api.h`, defined in Task 11) compiles.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R misc`
Expected: compile failure.

- [ ] **Step 3: Implement the files**

`compat` must not depend on `src`, so move the `#define IDS_*` lines from `src/resource.h` into `compat/mfc_resource.h` and make `src/resource.h` a one-line `#include "compat/mfc_resource.h"` with the notice line above it.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R misc --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/mfc_wnd.h compat/mfc_resource.h compat/mfc_resource.cpp compat/mfc.h src/resource.h tests/test_misc_mfc.cpp compat/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add CWnd, resource strings and the mfc.h aggregator"
```

---

### Task 10: INI module

**Files:**
- Create: `compat/ini.h`, `compat/ini.cpp`, `tests/test_ini.cpp`

**Interfaces:**
- Produces: `DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR out, DWORD size, LPCSTR file)`, `UINT GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT def, LPCSTR file)`, `BOOL WritePrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file)`, plus `namespace compat::ini { void FlushCache(); }` for tests. File names pass through `compat::ResolvePath`. Rules are spec 6.3 verbatim.
- Consumes: Tasks 2, 8.

- [ ] **Step 1: Write the failing tests**

`[ini]` against a temp file written by the test: `[Program Options]\nWidth = 800 \nName = "Icewind Dale II" \nFlag=1 ; note\n; comment\n[alias]\nHD0:=C:\\Games\\IWD2\\\n`:
- `GetPrivateProfileIntA("program options", "width", 0, f) == 800` (case-insensitive section and key, trimmed);
- `GetPrivateProfileStringA("Program Options", "Name", "", buf, 64, f)` returns 15 and `buf == "Icewind Dale II"`;
- `"Flag"` returns `"1 ; note"` (Review Focus 1);
- missing key returns the default and its length; missing file returns the default;
- `size == 5` on `"Icewind Dale II"` copies `"Icew"` and returns 4;
- null key enumerates `"Width\0Name\0Flag\0\0"` and returns 16 (total minus two) when the buffer is large, and with `size == 7` returns 5 with `"Width\0\0"`;
- null section enumerates `"Program Options\0alias\0\0"`;
- `GetPrivateProfileIntA` on `"abc"` returns the default, on `" -12x"` returns -12 as `UINT` (0xFFFFFFF4);
- `WritePrivateProfileStringA("Program Options", "Width", "1024", f)` replaces in place and preserves the comment line and other keys in order; writing a new key appends at the end of its section before the next section header; a new section appends at end of file; null value removes the key; null key removes the whole section; writing to a missing file creates it; `GetPrivateProfileStringA("Alias", "HD0:", ...)` returns `C:\Games\IWD2\`.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R ini`
Expected: compile failure.

- [ ] **Step 3: Implement `compat/ini.cpp`**

Parse the file into a line list kept in a per-file cache (`std::map<std::string, Document>`); writes rewrite the line list and the file, keeping line endings as read (`\r\n` if the file had them).

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R ini --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add compat/ini.h compat/ini.cpp tests/test_ini.cpp compat/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add INI module with Win32 profile semantics"
```

---

### Task 11: Clock, sleep and multimedia timers

**Files:**
- Create: `platform/clock.h`, `platform/clock.cpp`, `tests/test_clock.cpp`
- Modify: `platform/win32_api.h` (clock section)

**Interfaces:**
- Produces: `namespace platform { class ClockSource { public: virtual ~ClockSource(); virtual uint64_t NowMs() = 0; }; void SetClockSource(ClockSource*); ClockSource* DefaultClockSource(); }`; the default is `std::chrono::steady_clock` relative to process start. Win32-named: `DWORD GetTickCount()`, `DWORD timeGetTime()` (both `NowMs()` truncated to 32 bits), `void Sleep(DWORD ms)`, `DWORD SleepEx(DWORD ms, BOOL alertable)` returning 0, `typedef void (CALLBACK* LPTIMECALLBACK)(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)`, `MMRESULT timeSetEvent(UINT delayMs, UINT resolution, LPTIMECALLBACK, DWORD_PTR user, UINT flags)` returning a nonzero id, `MMRESULT timeKillEvent(UINT id)`, `MMRESULT timeBeginPeriod(UINT)`, `MMRESULT timeEndPeriod(UINT)` returning `TIMERR_NOERROR` (0); `TIME_PERIODIC` = 1, `TIME_ONESHOT` = 0. A periodic timer runs on its own `std::thread`, calling the callback with `(id, 0, user, 0, 0)` every `delayMs` measured on the steady clock; `timeKillEvent` joins it.
- Consumes: Task 2, Task 7.

- [ ] **Step 1: Write the failing tests**

`[clock]`: default `GetTickCount()` is non-decreasing across two calls with a 10 ms sleep between and advances by at least 10; a `FakeClock : ClockSource` returning a settable value makes `GetTickCount() == 1234` after `SetClockSource(&fake)` and `fake.value = 1234`, and `SetClockSource(nullptr)` restores the default; `timeSetEvent(20, 1, cb, 42, TIME_PERIODIC)` invokes `cb` at least 3 times within 200 ms with `user == 42`, and after `timeKillEvent` no further calls occur within 100 ms; `TIME_ONESHOT` calls exactly once.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R clock`
Expected: compile failure.

- [ ] **Step 3: Implement `platform/clock.cpp`**

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R clock --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add platform/clock.h platform/clock.cpp platform/win32_api.h tests/test_clock.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add swappable clock, sleep and multimedia timer shims"
```

---

### Task 12: Filesystem and path resolution

**Files:**
- Create: `platform/fs.h`, `platform/fs.cpp`, `tests/test_fs.cpp`
- Modify: `platform/win32_api.h` (filesystem section)

**Interfaces:**
- Produces: `namespace platform::fs { void SetDataRoot(const std::string& root); const std::string& DataRoot(); void SetInstallPrefix(const std::string& windowsPrefix); std::string Resolve(const char* path); bool WildcardMatch(const char* pattern, const char* name); void InstallResolver(); }` where `Resolve` applies spec 7.5 rules 1 to 3 in order and `InstallResolver` calls `compat::SetPathResolver` with it. Re-rooting: a path with a drive letter whose case-insensitive prefix equals the install prefix (set from the INI `HD0:` alias by Task 19's startup code, or empty) has the prefix replaced by the data root; any other drive-letter path has its drive and leading separators stripped and is joined under the data root. Case folding: when a path component does not exist, the directory is scanned once and cached, an exact-case entry wins, otherwise the lexicographically first case-insensitive match (Review Focus 3); components that match nothing are kept as written. Win32-named: `struct WIN32_FIND_DATAA { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; DWORD nFileSizeHigh, nFileSizeLow; char cFileName[MAX_PATH]; char cAlternateFileName[14]; }`, `HANDLE FindFirstFileA(LPCSTR pattern, WIN32_FIND_DATAA*)` returning `INVALID_HANDLE_VALUE` when nothing matches, `BOOL FindNextFileA(HANDLE, WIN32_FIND_DATAA*)`, `BOOL FindClose(HANDLE)`, `DWORD GetFileAttributesA(LPCSTR)` (`INVALID_FILE_ATTRIBUTES` = 0xFFFFFFFF, `FILE_ATTRIBUTE_DIRECTORY` = 0x10, `FILE_ATTRIBUTE_NORMAL` = 0x80, `FILE_ATTRIBUTE_READONLY` = 0x01), `BOOL DeleteFileA`, `BOOL CopyFileA(LPCSTR, LPCSTR, BOOL failIfExists)`, `BOOL CreateDirectoryA(LPCSTR, LPVOID)`, `BOOL GetDiskFreeSpaceA(LPCSTR root, DWORD* sectorsPerCluster, DWORD* bytesPerSector, DWORD* freeClusters, DWORD* totalClusters)` reporting `std::filesystem::space` with 512-byte sectors and 8-sector clusters, `DWORD GetCurrentDirectoryA(DWORD, LPSTR)`, `BOOL SetCurrentDirectoryA(LPCSTR)`, `DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD)` (the executable path from `/proc/self/exe` or `_pgmptr`-equivalent; on failure the empty string), `DWORD GetLogicalDrives()` returning 0, `UINT GetDriveTypeA(LPCSTR)` returning `DRIVE_NO_ROOT_DIR` (1), `BOOL GetVolumeInformationA(...)` returning `FALSE`. `FILETIME` is two `DWORD`s.
- Consumes: Tasks 2, 8.

- [ ] **Step 1: Write the failing tests**

`[fs]` in a temp tree `root/Override/SW1H01.ITM`, `root/Override/sw1h01.itm` only on case-sensitive filesystems (skip that assertion when the two paths alias), `root/Data/a.bif`, `root/Data/b.BIF`: `SetDataRoot(root)`; `Resolve("override/sw1h01.itm")` ends with `Override/sw1h01.itm` when both exist (exact match wins) and with `Override/SW1H01.ITM` when only the uppercase one exists; `Resolve("C:\\Games\\IWD2\\Data\\a.bif")` after `SetInstallPrefix("C:\\Games\\IWD2\\")` ends with `Data/a.bif`; `Resolve("D:\\Backup\\save.gam")` equals `root + "/Backup/save.gam"` (Review Focus 2); `Resolve("missing/dir/x.y")` keeps the components; `WildcardMatch("*.bif", "b.BIF")` true, `("sw?h01.itm", "SW1H01.ITM")` true, `("*.chr", "a.cre")` false; `FindFirstFileA(root + "/Data/*.bif")` yields both files (any order) then `FindNextFileA` returns `FALSE`; `GetFileAttributesA(root + "/Data")` has the directory bit; a missing path returns `INVALID_FILE_ATTRIBUTES`; `CopyFileA` then `DeleteFileA`; `GetDiskFreeSpaceA(root, ...)` returns `TRUE` with nonzero clusters.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R '^fs'`
Expected: compile failure.

- [ ] **Step 3: Implement `platform/fs.cpp`**

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R '^fs' --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add platform/fs.h platform/fs.cpp platform/win32_api.h tests/test_fs.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add filesystem shim with case-insensitive and re-rooted path resolution"
```

---

### Task 13: Game data directory resolution

**Files:**
- Create: `platform/data_dir.h`, `platform/data_dir.cpp`, `tests/test_data_dir.cpp`

**Interfaces:**
- Produces: `namespace platform { struct DataDirResult { std::string path; std::string source; }; std::optional<DataDirResult> FindDataDir(const char* exeDir, const char* repoRootHint); bool ResolveDataDir(); }`. `FindDataDir` tries, in order, `getenv("IWD2_DATA")`, `exeDir + "/IWD2"`, `repoRootHint + "/IWD2"`, the current directory; the first candidate containing a file named `CHITIN.KEY` (case-insensitively) wins, with `source` one of `"env"`, `"exe-link"`, `"repo-link"`, `"cwd"`. `ResolveDataDir` calls it with the executable's directory and its parent's parent, sets `platform::fs::SetDataRoot`, installs the resolver, changes the current directory to it, and returns `true`; otherwise it shows `MessageBoxA` text `Set IWD2_DATA to your Icewind Dale II install directory (the one containing CHITIN.KEY).` and returns `false`.
- Consumes: Task 12.

- [ ] **Step 1: Write the failing tests**

`[data_dir]` with temp dirs: env var pointing at a dir with `chitin.key` resolves with source `"env"`; env var pointing at a dir without it is skipped and `exeDir/IWD2` (a symlink to a valid dir) resolves with `"exe-link"`; with nothing valid and cwd lacking the key, `FindDataDir` is empty. Use `setenv`/`unsetenv` (on MSVC `_putenv_s`) around each case.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R data_dir`
Expected: compile failure.

- [ ] **Step 3: Implement `platform/data_dir.cpp`**

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R data_dir --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add platform/data_dir.h platform/data_dir.cpp tests/test_data_dir.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add IWD2_DATA resolution"
```

---

### Task 14: DirectDraw shim

**Files:**
- Create: `platform/ddraw_shim.h`, `platform/ddraw_shim.cpp`, `platform/present.h` (internal), `tests/test_ddraw.cpp`

**Interfaces:**
- Produces: the structs `DDPIXELFORMAT`, `DDCOLORKEY`, `DDSCAPS`, `DDSURFACEDESC` (with `LPDDSURFACEDESC`), `DDBLTFX`, `DDCAPS`, `PALETTEENTRY`; every constant from the discovery command with its documented value; typedefs `LPDIRECTDRAW`, `LPDIRECTDRAW2`, `LPDIRECTDRAWSURFACE`, `LPDIRECTDRAWCLIPPER`, `LPDIRECTDRAWPALETTE`; `extern const GUID IID_IDirectDraw2;` and `HRESULT DirectDrawCreate(GUID*, IDirectDraw**, void*)`. Classes `IDirectDraw` (with `typedef IDirectDraw IDirectDraw2`), `IDirectDrawSurface`, `IDirectDrawClipper`, `IDirectDrawPalette` as plain C++ classes with virtual methods whose names, argument lists and return types match the call sites found by the discovery command (declare both `SetDisplayMode(DWORD, DWORD, DWORD)` and the five-argument form if both are called). Behavior per spec 7.2. Internal, in `platform/present.h` (may include SDL; never included by `src/`): `namespace platform::present { void SetPresentTarget(SDL_Window*); void SetScreenSize(int w, int h); SIZE ScreenSize(); void SetFullscreen(bool); void Present(const uint32_t* xrgb, int width, int height, int pitchBytes); void MarkPrimaryDirty(); void PresentIfDirty(); uint64_t HashPrimary(); int PresentedFrames(); }` used by Task 16's window code and message pump and by Task 22's smoke hook. `ScreenSize` defaults to 1024x768 until Task 16 sets it from the display. Surfaces are `std::vector<uint32_t>` with pitch `width * 4`; the "primary" in `DDSCL_NORMAL` mode is `ScreenSize()`; in fullscreen mode it is the `SetDisplayMode` size. `SetCooperativeLevel` with `DDSCL_FULLSCREEN` calls `present::SetFullscreen(true)`, which switches the target window to borderless desktop fullscreen through `SDL_SetWindowFullscreen`; `RestoreDisplayMode` and `DDSCL_NORMAL` switch it back. `Lock` with a `RECT` returns `lpSurface` pointing at that rectangle's first pixel and the full pitch (Review Focus 5). `Blt`/`BltFast` clip the source rectangle to the source surface and the destination rectangle to the destination surface before copying (Review Focus 5). `Flip` swaps the primary's and back buffer's pixel vectors, then calls `Present` with the primary. `Unlock` and `Blt` on the primary call `MarkPrimaryDirty`.
- Consumes: Task 2. With no present target set, `Present` only counts frames, which is what the tests use.

- [ ] **Step 1: Write the failing tests**

`[ddraw]` with `SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy")` set in the test before any SDL use: `DirectDrawCreate` succeeds and `QueryInterface(IID_IDirectDraw2, ...)` returns the same pointer with refcount 2; `GetCaps` reports `dwCaps` without `DDCAPS_BLT` hardware bits and `DDCAPS_NOHARDWARE` set; `SetCooperativeLevel(NULL, DDSCL_NORMAL)` then `CreateSurface` of a 64x32 offscreen surface: `Lock(NULL, &ddsd, DDLOCK_WAIT, NULL)` gives `lPitch == 256`, `dwWidth == 64`, `ddpfPixelFormat.dwRGBBitCount == 32`, `dwRBitMask == 0xFF0000`; writing pixel (5,3) through `lpSurface` then `Lock` with `RECT{5,3,6,4}` returns `lpSurface` pointing at that value; `SetColorKey(DDCKEY_SRCBLT, key 0xFF00FF)` then `BltFast(10, 10, src, NULL, DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT)` into a 100x100 surface copies non-key pixels and leaves key pixels untouched; `BltFast(90, 90, src64x32, NULL, DDBLTFAST_WAIT)` clips at the destination edge and does not crash (ASAN run in CI covers reads); `BltFast` with a source `RECT{60,0,80,32}` clips to the 4 columns that exist; `Blt(&dst, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx{dwFillColor = 0x123456})` fills; a primary with `DDSD_BACKBUFFERCOUNT 1` and `DDSCAPS_FLIP | DDSCAPS_COMPLEX` yields a back buffer through `GetAttachedSurface(&caps{DDSCAPS_BACKBUFFER})`; writing to the back buffer then `Flip(NULL, DDFLIP_WAIT)` makes `Lock` on the primary show the written pixel and `present::HashPrimary()` changes; `GetDC` returns `DDERR_UNSUPPORTED`; `Restore` and `IsLost` return `DD_OK`; `SetDisplayMode(w, h, 16)` returns `DDERR_INVALIDMODE`; `Release` to zero deletes without leaks (ASAN).
- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R ddraw`
Expected: compile failure.

- [ ] **Step 3: Implement `platform/ddraw_shim.cpp` and `present.h/.cpp`**

`Present` creates, on first use and when the target window exists, one `SDL_Renderer` (software is acceptable) and one streaming `SDL_Texture` of the primary's size with `SDL_PIXELFORMAT_XRGB8888`, uploads with `SDL_UpdateTexture`, uses `SDL_SetRenderLogicalPresentation(w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX)` in fullscreen mode, and calls `SDL_RenderPresent`. With no window it only increments the frame counter.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R ddraw --output-on-failure`, both presets. Also configure once with `-DCMAKE_CXX_FLAGS=-fsanitize=address` and run the ddraw tests.
Expected: pass, no sanitizer reports.

- [ ] **Step 5: Commit**

```bash
git add platform/ddraw_shim.h platform/ddraw_shim.cpp platform/present.h platform/present.cpp tests/test_ddraw.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add DirectDraw shim on software surfaces with SDL presentation"
```

---

### Task 15: DirectSound shim

**Files:**
- Create: `platform/dsound_shim.h`, `platform/dsound_shim.cpp`, `tests/test_dsound.cpp`

**Interfaces:**
- Produces: `WAVEFORMATEX` (`wFormatTag, nChannels, nSamplesPerSec, nAvgBytesPerSec, nBlockAlign, wBitsPerSample, cbSize`, `LPWAVEFORMATEX`), `DSBUFFERDESC` (`dwSize, dwFlags, dwBufferBytes, dwReserved, lpwfxFormat`, `LPCDSBUFFERDESC`), `DSCAPS`, `DSBCAPS`, every `DS*` constant from the discovery command, `IID_IDirectSound3DListener`, `IID_IDirectSound3DBuffer`, `IID_IKsPropertySet`; classes `IDirectSound`, `IDirectSoundBuffer`, `IDirectSound3DListener`, `IDirectSound3DBuffer` (all methods no-ops returning `DS_OK`) with the method set the call sites use; `IKsPropertySet` is never created: `QueryInterface(IID_IKsPropertySet)` returns `E_NOINTERFACE`. `HRESULT DirectSoundCreate(GUID*, IDirectSound**, void*)`. The C-style macros from the discovery command, each expanding to the method call. Behavior per spec 7.3; volume `gain = (vol <= -10000) ? 0 : pow(10, vol / 2000.0)`; pan: with `p = |pan| / 2000.0`, the attenuated side's gain is `pow(10, -p)` and the other side stays 1, implemented through miniaudio balance pan `sign(pan) * (1 - pow(10, -p))`; `SetFrequency(f)` sets pitch `f / nSamplesPerSec` and `DSBFREQUENCY_ORIGINAL` (0) restores 1.0; `GetStatus` sets `DSBSTATUS_PLAYING` while playing and `DSBSTATUS_LOOPING` when started with `DSBPLAY_LOOPING`; `Lock(offset, bytes, &p1, &b1, &p2, &b2, flags)` returns two spans with wraparound and `DSBLOCK_FROMWRITECURSOR` starting at the write cursor; `GetCurrentPosition` reports play and write cursors in bytes, advancing with frames consumed; `Unlock` makes the written bytes audible. `namespace platform::dsound { void UseNullBackend(bool); }` for tests, called before `DirectSoundCreate`.
- Consumes: Task 2.

- [ ] **Step 1: Write the failing tests**

`[dsound]` with `UseNullBackend(true)`: create, `SetCooperativeLevel(NULL, DSSCL_EXCLUSIVE) == DS_OK`; primary buffer with `DSBCAPS_PRIMARYBUFFER` and `SetFormat(22050 Hz, 16-bit stereo) == DS_OK`; secondary of 8820 bytes (0.1 s): `Lock(0, 8820, ...)` gives `b1 == 8820`, `p2 == NULL`; `Lock(8000, 2000, ...)` gives `b1 == 820`, `b2 == 1180`; `Play(0, 0, DSBPLAY_LOOPING)` then `GetStatus` has both bits; `Stop` clears `PLAYING`; `SetVolume(-10000)` then internal gain is 0, `SetVolume(-600)` gives gain within 1e-3 of 0.501, `SetVolume(0)` gives 1; `SetPan(-10000)` gives right gain 0 and left 1, `SetPan(0)` gives both 1, `SetPan(2000)` gives left within 1e-3 of 0.1; `SetFrequency(44100)` on a 22050 buffer gives pitch 2.0 and `SetFrequency(DSBFREQUENCY_ORIGINAL)` gives 1.0 (expose gains and pitch through `platform::dsound::DebugState(IDirectSoundBuffer*)` returning a struct `{ float left, right, pitch; }`); `QueryInterface(IID_IDirectSound3DBuffer)` on a `DSBCAPS_CTRL3D` buffer succeeds and its `SetPosition` returns `DS_OK`; `QueryInterface(IID_IKsPropertySet)` returns `E_NOINTERFACE`; `DuplicateSoundBuffer` yields an independent buffer sharing data; `Release` chains free without leaks.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R dsound`
Expected: compile failure.

- [ ] **Step 3: Implement `platform/dsound_shim.cpp`**

`#define MINIAUDIO_IMPLEMENTATION` in this file only. One `ma_engine` created lazily by `DirectSoundCreate` with `ma_engine_config.pContext` pointing at a context whose backend list is `{ma_backend_null}` when `UseNullBackend(true)`. Each secondary buffer owns a ring of PCM bytes and a custom `ma_data_source` reading from it with looping, feeding an `ma_sound`.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R dsound --output-on-failure`, both presets, plus the ASAN configuration.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add platform/dsound_shim.h platform/dsound_shim.cpp tests/test_dsound.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add DirectSound shim on miniaudio"
```

---

### Task 16: Window, message queue, input and Win32 stubs

**Files:**
- Create: `platform/window.h`, `platform/window.cpp`, `platform/win32_stubs.cpp`, `platform/gl_compat.h`, `platform/gl_compat.cpp`, `platform/imm_stub.h`, `tests/test_window.cpp`
- Modify: `platform/win32_api.h` (window, input, stubs sections)

**Interfaces:**
- Produces in `win32_api.h`, Win32-shaped: `struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; }`, `struct WNDCLASSEXA { UINT cbSize; UINT style; LRESULT (CALLBACK* lpfnWndProc)(HWND, UINT, WPARAM, LPARAM); int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HANDLE hIcon, hCursor, hbrBackground; LPCSTR lpszMenuName, lpszClassName; HANDLE hIconSm; }`, `typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM)`, `struct PAINTSTRUCT`, the `WS_*`, `WS_EX_*`, `SW_*`, `SM_CXSCREEN`/`SM_CYSCREEN`/the other `SM_*` the engine uses, `PM_NOREMOVE`/`PM_REMOVE`, `MB_*`, `IDOK`/`IDYES`/`IDNO`/`IDCANCEL`, `IDC_ARROW`, `HWND_TOP`, `SWP_*`, `VER_PLATFORM_WIN32_NT`, `OSVERSIONINFOA`, `MEMORYSTATUS`; functions `RegisterClassExA` (stores `lpfnWndProc`; returns nonzero), `CreateWindowExA(DWORD exStyle, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h, HWND parent, HANDLE menu, HINSTANCE, LPVOID)` which creates the SDL window (borderless when `WS_POPUP`, at the given position and size, `SDL_WINDOW_HIDDEN` unless `WS_VISIBLE`), calls `present::SetPresentTarget` with it and `present::SetScreenSize` with the primary display's size, and returns its `HWND`, `DestroyWindow` (delivers `WM_DESTROY` synchronously to the window procedure, then destroys the SDL window and marks the queue as quitting), `ShowWindow`, `SetWindowPos`, `GetClientRect`, `ClientToScreen`, `ScreenToClient`, `GetSystemMetrics`, `PeekMessageA(MSG*, HWND, UINT, UINT, UINT remove)` (pumps SDL events into the queue first; `PM_NOREMOVE` leaves the message), `GetMessageA` (blocks with `SDL_WaitEventTimeout(…, 10)` until a message; returns `FALSE` for `WM_QUIT`, which is queued once the window has been destroyed and the queue is empty), `TranslateMessage` (no-op), `DispatchMessageA` (calls the registered window procedure), `PostMessageA` (enqueues), `SendMessageA` (calls the window procedure synchronously), `PostQuitMessage`, `DefWindowProcA` (returns 0), `RegisterWindowMessageA` (distinct ids from 0xC000 upward, stable per name), `SetCursor`, `LoadCursorA` (returns a non-null handle for `IDC_ARROW`), `GetCursorPos` (window position plus `SDL_GetGlobalMouseState`), `SetCursorPos`, `ShowCursor`, `SHORT GetAsyncKeyState(int vk)` (0x8000 bit from `SDL_GetKeyboardState` through `platform::ScancodeForVK`, or from `SDL_GetMouseState` for `VK_LBUTTON`, `VK_RBUTTON`, `VK_MBUTTON`), `HKL GetKeyboardLayout(DWORD)` (constant 0x04090409), `GetUpdateRect` (returns `FALSE`), `BeginPaint`/`EndPaint` (no-ops), `int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)` (`SDL_ShowSimpleMessageBox`; returns `IDOK`, or `IDYES` for `MB_YESNO` through `SDL_ShowMessageBox` with two buttons), `SetRect` is already in Task 2. In `window.h`: `namespace platform::window { int ScancodeForVK(int vk); int VKForScancode(int sc); void PumpEvents(); void EnqueueMessage(UINT, WPARAM, LPARAM); HWND MainWindow(); void SetSmokeFrames(int n); }`. The pump translates: `SDL_EVENT_QUIT` and `SDL_EVENT_WINDOW_CLOSE_REQUESTED` to `WM_CLOSE`; `WINDOW_RESIZED` to `WM_SIZE`, `WINDOW_MOVED` to `WM_MOVE`, `WINDOW_MINIMIZED`/`RESTORED` to `WM_SIZE` (and `GetClientRect` returns an empty rect while minimized); `WINDOW_FOCUS_GAINED` to `WM_ACTIVATEAPP` with `wParam 1`, `FOCUS_LOST` to `WM_ACTIVATEAPP` with 0 then `WM_KILLFOCUS`; `MOUSE_WHEEL` to `WM_MOUSEWHEEL` with `wParam = (WORD)(delta_y * 120) << 16`; `KEY_DOWN` with Alt held to `WM_SYSKEYDOWN` with `wParam = VKForScancode`; `DISPLAY_*` to `WM_DISPLAYCHANGE`. After pumping, `present::PresentIfDirty()` runs, and if smoke frames are set and the presented-frame count reaches them, the pump prints `framebuffer-hash: %016llx` with `present::HashPrimary()` to stdout and enqueues `WM_DESTROY`.
- Produces in `win32_stubs.cpp`: `LoadLibraryA` (returns `NULL`), `FreeLibrary`, `GetProcAddress` (returns `NULL`), `GetModuleHandleA` (returns a non-null sentinel), `GetVersionExA` (fills `dwPlatformId = VER_PLATFORM_WIN32_NT`, major 5, minor 1, returns `TRUE`), `GlobalMemoryStatus` (fills `dwTotalPhys` and `dwAvailPhys` with 1 GiB), `GetDC` (returns `NULL`), `ReleaseDC`, `GetDeviceCaps` (`BITSPIXEL` 32, `PLANES` 1), `CreateFontA` (returns `NULL`), `DeleteObject`, `SelectObject`, `ExtTextOutA`, `TextOutA`, `SetTextColor`, `SetBkColor`, `SetBkMode`, `GetTextExtentPoint32A` (returns `FALSE`), `GetFileVersionInfoSizeA` (0), `GetFileVersionInfoA` (`FALSE`), `VerQueryValueA` (`FALSE`), `CoInitialize` (`S_OK`), `CoUninitialize`, `CoCreateInstance` (`E_NOINTERFACE`), `OutputDebugStringA` (stderr), `lstrcpyA`, `lstrcatA`, `lstrlenA`, `lstrcmpA`, `lstrcmpiA`, `wsprintfA`, `GetLastError` (0), `CreateProcessA` (`FALSE`), `ShellExecuteA` (0), and the IME set in `imm_stub.h`: `ImmGetContext` (`NULL`), `ImmReleaseContext`, `ImmGetCompositionStringA` (0), `ImmGetConversionStatus` (`FALSE`), `ImmSetOpenStatus`, `ImmSetConversionStatus`, `ImmGetCandidateListA` (0), `ImmGetOpenStatus` (`FALSE`), `ImmGetDescriptionA` (0), with the `GCS_*`, `IME_CMODE_*`, `CANDIDATELIST`, `HIMC` definitions the engine uses. `gl_compat.h` declares every `GL_*` constant and `gl*`/`glu*`/`wgl*` function from the discovery command with the OpenGL 1.1 signature; `gl_compat.cpp` defines them as no-ops (`wglCreateContext` returns `NULL`, `wglMakeCurrent` returns `FALSE`, `glGetError` returns 0, `glGetString` returns `""`).
- Consumes: Tasks 2, 11, 14.

- [ ] **Step 1: Write the failing tests**

`[window]` with the dummy video driver: `RegisterClassExA` returns nonzero and `CreateWindowExA(0, "C", "t", WS_POPUP, 0, 0, 320, 200, NULL, NULL, NULL, NULL)` returns non-null; `GetClientRect` gives `{0,0,320,200}`; `PostMessageA(h, WM_USER + 1, 7, 8)` then `PeekMessageA(&m, NULL, 0, 0, PM_NOREMOVE)` returns `TRUE` with `message == WM_USER + 1` and a second `PeekMessageA` with `PM_REMOVE` returns it again, then a third returns `FALSE`; `DispatchMessageA` on a message calls the registered procedure (a test procedure records `(msg, wParam, lParam)`); `DestroyWindow(h)` delivers `WM_DESTROY` to the procedure synchronously, and afterwards `GetMessageA` returns `FALSE`; `RegisterWindowMessageA("A")` is `>= 0xC000`, equal on repeat, and differs from `"B"`; `ScancodeForVK(VK_ESCAPE) == SDL_SCANCODE_ESCAPE`, `VK_RETURN` to `RETURN`, `VK_OEM_1` to `SEMICOLON`, `VK_F1` to `F1`, `'A'` to `A`, `VK_NUMPAD0` to `KP_0`, and every VK from the discovery command maps to a nonzero scancode; `GetAsyncKeyState(VK_ESCAPE)` is 0 with no keys down; `GetVersionExA` reports NT; `LoadLibraryA("binkw32") == NULL`; `GetDeviceCaps(NULL, BITSPIXEL) == 32`; `MessageBoxA` returns `IDOK` in a separate test case tagged `[.manual]`, run by hand with a real display.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R window`
Expected: compile failure.

- [ ] **Step 3: Implement the files**

The VK table is a static array of `{vk, scancode}` pairs covering every VK the engine uses plus letters, digits, function keys and numpad.

- [ ] **Step 4: Run the tests**

Run: build, `ctest --preset linux-gcc -R window --output-on-failure`, both presets.
Expected: pass.

- [ ] **Step 5: Commit**

```bash
git add platform/window.h platform/window.cpp platform/win32_stubs.cpp platform/gl_compat.h platform/gl_compat.cpp platform/imm_stub.h platform/win32_api.h tests/test_window.cpp platform/CMakeLists.txt tests/CMakeLists.txt
git commit -m "Add SDL window shim with Win32 message queue, polled input and inert stubs"
```

---

### Task 17: Engine object library and include swap

**Files:**
- Modify: `CMakeLists.txt` (add `iwd2r_engine` OBJECT library from the former source list minus deletions, include dirs `src`, `compat`, repo root; `EXCLUDE_FROM_ALL` until Task 21), `platform/entry.cpp` (create), `src/main.cpp`
- Delete: `src/mfc.h`, `src/resource.rc`, `third_party/directplay/`
- Modify (include lines only, each file gets the notice line): `src/CVideo.h`, `src/CVidMode.h`, `src/CVidImage.h`, `src/CVidBlitter.h`, `src/CInfinity.h`, `src/CVideo3d.h`, `src/CSound.h`, `src/CSoundMixer.h`, `src/CSoundProperties.h`, `src/CSoundProperties.cpp`, `src/CVoice.h`, `src/CResWave.h`, `src/music/sound.h`, `src/CNetwork.h`, `src/CGameSpy.cpp`, `src/CChitin.cpp`

**Interfaces:**
- Produces: target `iwd2r_engine`; `int iwd2r_main(int argc, char** argv)` declared in `src/main.h` (create) and defined in `src/main.cpp`; `platform/entry.cpp` defines `main` through `#include <SDL3/SDL_main.h>` and returns `iwd2r_main(argc, argv)`.
- Consumes: all shims.

- [ ] **Step 1: Delete `src/mfc.h` and add `compat` to the engine include path so `#include "mfc.h"` resolves to `compat/mfc.h`; replace the external includes:**

`<ddraw.h>` and `<gl/gl.h>` and `<gl/glu.h>` lines become `#include "platform/ddraw_shim.h"` and `#include "platform/gl_compat.h"`; `<dsound.h>` and `<mmsystem.h>` become `#include "platform/dsound_shim.h"`; `<dplay.h>`, `<dplobby.h>`, `<winsock2.h>`, `<initguid.h>`, `<winver.h>` lines are removed. The repo root is on the include path so `platform/...` resolves.

- [ ] **Step 2: Rewrite `src/main.cpp` as `iwd2r_main`**

Keep: the `CBaldurChitin baldurChitin;` construction, the `field_1932` check, `baldurChitin.Init(NULL)` and `baldurChitin.WinMain(NULL, NULL, cmdLine, SW_SHOW)` where `cmdLine` is `argv[1..]` joined by single spaces. Remove: the OS version checks, the mutex, the `register`/`reg32.exe` launch, the hardcoded directory. Insert before `Init`: `if (!platform::ResolveDataDir()) return 2;`. Return `WinMain`'s result.

- [ ] **Step 3: Verify a leaf translation unit compiles against the shims**

Run: `cmake --preset linux-gcc && cmake --build --preset linux-gcc --target CMakeFiles/iwd2r_engine.dir/src/C2DArray.cpp.o` (with Ninja, object targets are addressable this way; confirm the exact name with `ninja -C build/linux-gcc -t targets all | grep C2DArray`).
Expected: compiles. Do the same for `src/CResRef.cpp.o` and `src/CUtil.cpp.o`.

- [ ] **Step 4: Commit**

```bash
git add -A CMakeLists.txt platform/entry.cpp src third_party
git commit -m "Compile engine against compat and platform shims; drop MFC, DirectX and DirectPlay includes"
```

---

### Task 18: Cut DirectPlay out of CNetwork

**Files:**
- Modify: `src/CNetwork.h`, `src/CNetwork.cpp`, `src/CChitin.cpp:200-206` (remove the `0x405` and `0x406` cases), `src/CBaldurChitin.h:31,179`, `src/CBaldurChitin.cpp:33,169,673-675,857,884,1974-1976,2019-2021`, `src/CScreenConnection.cpp:10,1460-1463,1878-1880,3413-3416,3727-3729`
- Delete: `src/CScreenMultiPlayer.cpp`, `src/CScreenMultiPlayer.h`

**Interfaces:**
- Produces: `CNetwork` with every member in spec Appendix A.5 plus `EnumerateServiceProviders`, `EnumerateSessions`, `HostNewSession`, `GetSessionGUID`, `CreatePlayer`, `UnselectSession`, `UnselectModemAddress`, `SelectServiceProvider`, `RemoveInitializeConnection`, `JoinSelectedSession`, `InitializeConnectionToServiceProvider`, `ThreadLoop`, `m_bConnectionInitialized`, `m_bServiceProviderEnumerated`, `m_sIPAddress`, `m_nTotalSessions`, `m_nTotalServiceProviders`, `m_nSession`, `m_nApplicationGuid`, `m_bSessionSelected`, `m_bSessionPasswordEnabled`, `m_bApplicationGuidDefined`, the `SERV_PROV_*` constants and the `SEND_*` flag constants, all with unchanged signatures. Removed: `m_lpDirectPlay`, `m_lpDirectPlayLobby`, `m_sessionDesc`, `m_pDirectPlayAddress`, `CreateDirectPlayInterface`, `CreateDirectPlayLobbyInterface`, `CreateDirectPlayAddress`, `InitDirectPlay`, `FreeDirectPlay`, the `CNetworkEnum*Callback` functions, modem and serial methods, `SlidingWindow*`, `FrameSend`, `FetchFrame`, `ReceiveCall`, `SendCall`, `HandleSystemMessage`, `CheckSessionStatus`, `CNetworkWindow`, `AddMessageToWindow`.
- Consumes: Task 17.

- [ ] **Step 1: Prune `CNetwork.h` and `CNetwork.cpp`**

For each kept method, keep the body and delete the branches guarded by `m_nServiceProvider != SERV_PROV_NULL` or `m_nServiceProvider != 0` and any statement touching a removed member. Specifically: `EnumerateServiceProviders` becomes `m_bServiceProviderEnumerated = TRUE; m_nTotalServiceProviders = 1; m_serviceProviderGuids[0] = GUID_NULL; return TRUE;` after its existing early returns; `InitializeConnectionToServiceProvider` keeps only the null-provider path ending in `m_bConnectionInitialized = TRUE; return TRUE;`; `HostNewSession` keeps the checks at its top and the assignments at `CNetwork.cpp:2581-2585`; `CreatePlayer` keeps the slot search at `2795-2811` and `AddPlayerToList`; `EnumerateSessions`, `JoinSelectedSession`, `GetSessionGUID` return `FALSE`; `UnselectSession`, `UnselectModemAddress`, `RemoveInitializeConnection` keep their flag resets; `ThreadLoop` keeps the `!m_bConnectionEstablished` and timing code and loses the receive loop; `OnCloseSession` loses `DestroyPlayer` and `Close`; `SendMessage_`, `PeekMessage_`, `FetchMessage` keep the queue code and lose the DirectPlay send; the constructor and destructor lose DirectPlay initialization. `CGameSpy` methods that are called from `CNetwork` keep being called; `CGameSpy.cpp` is already stubs.

- [ ] **Step 2: Remove the multiplayer screen and its references**

In `CBaldurChitin`: remove the forward declaration, member, include, allocation at 857, `AddEngine` at 884, deletion at 673 to 675, and the two `ClearChatMessages` blocks. In `CScreenConnection`: remove the include and the four blocks that reach `m_pEngineMultiPlayer`, replacing the branch at 1460 to 1463 and 3727 to 3729 with nothing so the single-player branch is the only one. In `CChitin.cpp` remove the two cases.

- [ ] **Step 3: Verify the touched translation units compile**

Run: `cmake --build --preset linux-gcc --target CMakeFiles/iwd2r_engine.dir/src/CNetwork.cpp.o CMakeFiles/iwd2r_engine.dir/src/CBaldurChitin.cpp.o CMakeFiles/iwd2r_engine.dir/src/CScreenConnection.cpp.o CMakeFiles/iwd2r_engine.dir/src/CChitin.cpp.o`
Expected: `CNetwork.cpp.o` compiles; the others may still fail on symbols later tasks provide, but no error may mention `CScreenMultiPlayer`, `IDirectPlay` or `DPSESSIONDESC2` (check with `2>&1 | grep -E 'MultiPlayer|IDirectPlay|DPSESSION'` returning nothing).

- [ ] **Step 4: Write the network test**

`tests/test_network.cpp`, `[network]`, links `iwd2r_engine` objects (add `$<TARGET_OBJECTS:iwd2r_engine>` to `iwd2r_tests` only for this and later engine-level tests; keep a separate executable `iwd2r_engine_tests` if link time becomes a problem): construct `CNetwork n;` then `n.EnumerateServiceProviders()`, `n.SelectServiceProvider(CNetwork::SERV_PROV_NULL)`, `n.InitializeConnectionToServiceProvider(TRUE)`, set `m_sSessionNameToMake = "s"`, `m_bSessionNameToMake = TRUE`, `m_sLocalPlayerName = "p"`, `m_bPlayerNameToMake = TRUE`, `n.HostNewSession()`, `INT err; n.CreatePlayer(err)`; assert `GetSessionOpen()`, `GetSessionHosting()`, `m_idLocalPlayer == 1`, `GetHostPlayerName(s)` gives `"p"`, `FindPlayerLocationByID(1, FALSE) == 0`, `SendSpecificMessage("p", 0, 1, 1, data, 4) == TRUE`, `PeekSpecificMessage("p", 1, 1) == FALSE`; `CloseSession(TRUE)` then `GetSessionOpen() == FALSE` and `m_idLocalPlayer == 0`. This test is enabled in Task 21 once the engine links; until then it is written and excluded with `[.pending]`.

- [ ] **Step 5: Commit**

```bash
git add -A src tests
git commit -m "Remove DirectPlay and the multiplayer screen; keep CNetwork's null-provider path"
```

---

### Task 19: Window, main loop, timers and threads in the engine

**Files:**
- Modify: `src/CChitin.cpp` (lines 26, 2200: `TimerFunction` parameters `DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2`; lines 58-212 `WndProc`: remove the `GetUpdateRect`/`BeginPaint`/`EndPaint` block under `WM_PAINT` and return 1 directly; keep everything else), `src/CChitin.h`, `src/music/sound.cpp:1583` (`(DWORD)fadeSounds` becomes `reinterpret_cast<DWORD_PTR>(fadeSounds)`), `src/CBaldurChitin.cpp:553-560` (after reading `m_ptScreen`, read the `Alias` `HD0:` value with `GetPrivateProfileStringA("Alias", "HD0:", "", buf, MAX_PATH, GetIniFileName())` and call `platform::fs::SetInstallPrefix(buf)`), `src/CInfGame.cpp`, `src/CSearchBitmap.cpp` (no changes expected; verify)

**Interfaces:**
- Consumes: Tasks 7, 11, 16.

- [ ] **Step 1: Apply the edits above**

- [ ] **Step 2: Check for `PostQuitMessage` and `WM_QUIT` use**

Run: `grep -rn 'PostQuitMessage\|WM_QUIT' src`. If `ShutDown` posts quit, nothing to do; if not, the shim's rule (quit after `DestroyWindow`) covers it. Record the finding in the commit message.

- [ ] **Step 3: Verify the translation units compile**

Run: `cmake --build --preset linux-gcc --target CMakeFiles/iwd2r_engine.dir/src/CChitin.cpp.o CMakeFiles/iwd2r_engine.dir/src/CBaldurChitin.cpp.o CMakeFiles/iwd2r_engine.dir/src/CInfGame.cpp.o CMakeFiles/iwd2r_engine.dir/src/CSearchBitmap.cpp.o CMakeFiles/iwd2r_engine.dir/src/music/sound.cpp.o`
Expected: `CChitin.cpp.o`, `CInfGame.cpp.o`, `CSearchBitmap.cpp.o` compile. `CBaldurChitin.cpp.o` and `sound.cpp.o` may fail only on symbols Task 20 provides (verify the errors mention `CVid`, `CSound`, `IDirectSound` or `Bink` names only).

- [ ] **Step 4: Commit**

```bash
git add src
git commit -m "Run the engine window, timers and threads on the platform shims"
```

---

### Task 20: Video, sound, fonts, movies, 3D mode and IME

**Files:**
- Modify: `src/CVideo.cpp`, `src/CVidMode.cpp`, `src/CVidInf.cpp`, `src/CVidFont.cpp`, `src/CUtil.cpp`, `src/CSoundMixer.cpp`, `src/CSound.cpp`, `src/CSoundProperties.cpp`, `src/music/sound.cpp`, `src/CBaldurProjector.cpp`, `src/CChitin3d.cpp`, `src/CVideo3d.cpp`, `src/CImm.cpp`, `src/CResCache.cpp`

**Interfaces:**
- Consumes: Tasks 14, 15, 16.

- [ ] **Step 1: Compile each listed translation unit and fix what the shims do not absorb**

Run for each: `cmake --build --preset linux-gcc --target CMakeFiles/iwd2r_engine.dir/src/<file>.o`. Expected categories of fixes, each the smallest change: a DirectDraw or DirectSound method declared with a different argument count than the call site uses (fix the shim's declaration, not the engine); a `CString` passed to a C varargs function such as `sprintf` (add an explicit `(LPCSTR)` cast at that call site); a Win32 function not yet declared (add it to `win32_api.h` and `win32_stubs.cpp` as an inert stub, or to the owning platform file if it must work). Keep a list of every such edit for the commit message.

- [ ] **Step 2: Guard the movie player**

In `src/CBaldurProjector.cpp`, confirm every `m_pfnBink*` call is reached only when `m_hBinkDLL != NULL`. If a path calls a function pointer without that guard, add `if (m_hBinkDLL == NULL) return FALSE;` (or the function's failure value) at the top of that function.

- [ ] **Step 3: Confirm the 3D mode and IME are inert**

`CChitin3d` returns `FALSE` when `LoadLibraryA("opengl32.dll")` returns `NULL` (line 55 to 61); confirm the caller treats `FALSE` as "use 2D". `CImm` calls return failure through the stubs; confirm no call dereferences a `NULL` `HIMC`.

- [ ] **Step 4: Verify every video and sound translation unit compiles**

Run: `cmake --build --preset linux-gcc --target $(ninja -C build/linux-gcc -t targets all | grep -E 'iwd2r_engine.dir/src/(CVid|CVideo|CInfinity|CSound|CVoice|CBaldurProjector|CChitin3d|CImm|CResCache|CUtil|music/)' | cut -d: -f1 | tr '\n' ' ')`
Expected: all compile.

- [ ] **Step 5: Commit**

```bash
git add src platform
git commit -m "Compile video, sound, font, movie, 3D and IME code against the shims"
```

---

### Task 21: Whole-tree build and link on Linux and Windows

**Files:**
- Modify: `CMakeLists.txt` (remove `EXCLUDE_FROM_ALL` from `iwd2r_engine`; add `add_executable(iwd2r platform/entry.cpp $<TARGET_OBJECTS:iwd2r_engine>)` linking `iwd2r_platform`, `iwd2r_compat`, zlib; add `$<TARGET_OBJECTS:iwd2r_engine>` to `iwd2r_tests` and enable `tests/test_network.cpp`), `src/**` as needed

**Interfaces:**
- Produces: the `iwd2r` executable on all four presets.
- Consumes: everything above.

- [ ] **Step 1: Build everything, keep going past errors, and fix each translation unit**

Run: `cmake --build --preset linux-gcc -- -k 0 2>&1 | tee build/linux-gcc/errors.log; grep -c 'error:' build/linux-gcc/errors.log`
Fix in the same categories as Task 20 step 1 until the count is 0, then link. Link errors name Win32 functions still undeclared or undefined: add each to `win32_api.h` and the owning platform file.

- [ ] **Step 2: Build with Clang**

Run: `cmake --preset linux-clang && cmake --build --preset linux-clang`
Expected: links. Fix Clang-only diagnostics (the `-Wnon-pod-varargs` class is an error: add `(LPCSTR)` casts at those call sites).

- [ ] **Step 3: Build on Windows**

On the Windows host, from a Developer PowerShell: `cmake --preset windows-msvc-x64; cmake --build --preset windows-msvc-x64; cmake --preset windows-msvc-x86; cmake --build --preset windows-msvc-x86`
Expected: both link. MSVC-only fixes go in `compat/win_types.h` under `#ifdef _MSC_VER`.

- [ ] **Step 4: Run all tests on each preset, including the network test**

Run: `ctest --preset linux-gcc --output-on-failure` and the equivalents.
Expected: pass.

- [ ] **Step 5: Run the game by hand on Linux and on Windows**

Run: `IWD2_DATA="/mnt/c/Games/Icewind Dale II" ./build/linux-gcc/iwd2r` (under WSL this needs a display; use the Windows build for the visual check if WSLg is unavailable). With `Full Screen=0` and then `Full Screen=1` in `icewind2.ini`.
Expected: the start screen appears, options and keymaps screens open, the character editor opens, Escape and the close button exit cleanly with code 0. Note which screens work and any difference from the upstream build in `docs/superpowers/plans/2026-10-12-iwd2r-portability-foundation.md` under a "Manual check" heading appended at the end.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Link iwd2r on Linux and Windows and reach the start screen"
```

---

### Task 22: Smoke test

**Files:**
- Create: `tests/smoke_start_screen.cpp`
- Modify: `platform/window.cpp` (read `IWD2R_SMOKE_FRAMES` in `CreateWindowExA` and call `SetSmokeFrames`), `tests/CMakeLists.txt` (pass the `iwd2r` executable path as a compile definition `IWD2R_EXE`)

**Interfaces:**
- Consumes: Task 16's `SetSmokeFrames`, Task 21's executable.

- [ ] **Step 1: Write the test**

`TEST_CASE("start screen smoke", "[data]")`: `SKIP()` unless `IWD2_DATA` is set; run `IWD2R_EXE` with environment `IWD2R_SMOKE_FRAMES=300` through `std::system` redirecting stdout to a temp file; assert exit code 0 and that the output contains a line matching `framebuffer-hash: [0-9a-f]{16}`; print the hash with `WARN()` so it appears in the log.

- [ ] **Step 2: Run it without data to verify it skips**

Run: `env -u IWD2_DATA ctest --preset linux-gcc -R smoke`
Expected: reported as skipped, not failed.

- [ ] **Step 3: Run it with data**

Run: `IWD2_DATA="/mnt/c/Games/Icewind Dale II" ctest --preset linux-gcc -R smoke --output-on-failure` and the Windows equivalent.
Expected: pass; the two hashes are recorded in the plan's "Manual check" section (they may differ between platforms because of font or timing differences; equality is not required in project 1).

- [ ] **Step 4: Commit**

```bash
git add tests/smoke_start_screen.cpp tests/CMakeLists.txt platform/window.cpp
git commit -m "Add start-screen smoke test gated on IWD2_DATA"
```

---

### Task 23: Notices, README and ADRs

**Files:**
- Create: `NOTICE.md`, `docs/adr/0001-platform-layer.md`, `docs/adr/0002-single-player-only.md`, `tools/check_notices.py`, `tests/test_tools.py` (not a Catch2 test; run by CI with `python3 -I`)
- Modify: `README.md`, every upstream file changed since commit `4fa36bc` (notice line), every new file (header line)

**Interfaces:**
- Produces: `tools/check_notices.py [--base 4fa36bc]` exits 1 and prints each path when a file that differs from the base commit lacks `// Modified for iwd2r; see NOTICE.md` as its first line, or a file absent from the base lacks the new-file header; exits 0 otherwise. Files under `third_party/`, `docs/`, `.github/` and non-source files (`*.md`, `*.json`, `*.txt`, `*.yml`) are exempt.
- Consumes: nothing.

- [ ] **Step 1: Write the checker test**

`tests/test_tools.py` using `unittest`: in a temp git repo with one committed file, modify it without a notice: checker exits 1 naming it; add the line: exits 0; add a new `.cpp` without the header: exits 1; with it: exits 0.

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -I tests/test_tools.py`
Expected: fails, module missing.

- [ ] **Step 3: Write `tools/check_notices.py`, then run it on the repo and add every missing line it names**

- [ ] **Step 4: Write NOTICE.md, README.md and the two ADRs**

NOTICE.md: "iwd2r is a modified version of iwd2-re by alexbatalov (https://github.com/alexbatalov/iwd2-re), forked on 2026-10-11 at commit 4fa36bc. Modifications are marked by a notice line at the top of each changed file. Distributed under the Sustainable Use License in LICENSE.md." README.md sections, in order: title and one-paragraph description, "What this repository does not contain" (no game content, no original binary, no IWD2EE files; bring your own GOG copy), "Status" (the six-project table with project 1 marked done when it is), "Build" (presets, Linux packages, Windows requirements), "Run" (`IWD2_DATA`, the `IWD2` link alternative), "Deviations from the original" (multiplayer removed; OpenGL 3D mode, movies, IME text entry and GDI text inert pending later projects; fullscreen presents as a borderless window), "License" (LICENSE.md and NOTICE.md), "References" (iwd2-re, IESDP, GemRB as behavioral reference only, benilla as pattern reference). Each ADR has sections Context, Decision, Alternatives considered, Consequences, with the content of spec sections 1.2, 7, 8 and 9 for 0001 and spec 4.1, 4.2 and 7.7 for 0002.

- [ ] **Step 5: Run the checker and the test**

Run: `python3 -I tests/test_tools.py && python3 -I tools/check_notices.py`
Expected: both exit 0.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Add NOTICE, README, ADRs and the notice-line checker"
```

---

### Task 24: Generated subsystem map

**Files:**
- Create: `tools/gen_map.py`, `docs/MAP.md`
- Modify: `tests/test_tools.py`

**Interfaces:**
- Produces: `tools/gen_map.py [--check]` writes `docs/MAP.md` (or, with `--check`, exits 1 if the file would change). The map has a header line with the commit short hash and totals, then one table with columns Family, Files, Lines, `TODO: Incomplete`, Address comments, Platform headers included; families in this order with these prefixes: AI (`CAI`), Game (`CGame`, `CInfGame`, `CDungeonMaster`, `CSpawn`, `CTimer`, `CWeather`), Screens (`CScreen`), UI (`CUI`, `CButton`), Video (`CVid`, `CVideo`, `CInfinity`, `CChitin3d`, `CBlood`, `CColor`, `CParticle`), Resources (`CRes`, `CDimm`, `CCache`, `C2DArray`, `CCreatureFile`, `FileFormat`), Sound (`CSound`, `CVoice`, `music/`), Messaging (`CMessage`, `CNetwork`, `CGameSpy`, `CChatBuffer`, `CMultiplayerSettings`), Engine core (`CChitin`, `CBaldur`, `main`), Other (everything else), then rows for `compat` and `platform`. "Platform headers included" lists the distinct `platform/*.h` names found in `#include` lines of the family's files.
- Consumes: nothing.

- [ ] **Step 1: Add tests**

In `tests/test_tools.py`: a temp tree with `src/CAIAction.cpp` (3 lines, one `// TODO: Incomplete.`, one `// 0x401000`) and `src/CVidMode.cpp` including `platform/ddraw_shim.h`; the generated map has an AI row with Files 1, Lines 3, Incomplete 1, Address 1 and a Video row listing `ddraw_shim.h`; `--check` exits 0 right after generation and 1 after appending a line to a source file.

- [ ] **Step 2: Run to verify it fails, implement, run to verify it passes**

Run: `python3 -I tests/test_tools.py`

- [ ] **Step 3: Generate the real map**

Run: `python3 -I tools/gen_map.py && python3 -I tools/gen_map.py --check`
Expected: `docs/MAP.md` written; check exits 0.

- [ ] **Step 4: Commit**

```bash
git add tools/gen_map.py docs/MAP.md tests/test_tools.py
git commit -m "Add generated subsystem map"
```

---

### Task 25: Continuous integration

**Files:**
- Create: `.github/workflows/ci.yml`
- Delete: `.github/workflows/ci-build.yml`

**Interfaces:**
- Consumes: all presets, `iwd2r_tests`, the two tools.

- [ ] **Step 1: Write the workflow**

Jobs: `linux-gcc` and `linux-clang` on `ubuntu-24.04` with `apt-get install -y build-essential cmake ninja-build clang pkg-config libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev libwayland-dev libdecor-0-dev libudev-dev libdbus-1-dev libpipewire-0.3-dev`; `windows-x64` and `windows-x86` on `windows-latest`. Each job: checkout, configure preset, build preset, `ctest --preset <p> --output-on-failure` with `SDL_VIDEO_DRIVER=dummy` and `SDL_AUDIO_DRIVER=dummy` in the environment, then on `linux-gcc` only: `python3 -I tests/test_tools.py`, `python3 -I tools/check_notices.py`, `python3 -I tools/gen_map.py --check`, and `clang-format --dry-run --Werror $(git diff --name-only 4fa36bc -- '*.cpp' '*.h' | grep -v '^src/')` (format new and platform code only; decompiled files keep upstream formatting). Cache the FetchContent directory with `actions/cache` keyed on `CMakeLists.txt`. Triggers: push to `main` and `first`, pull requests.

- [ ] **Step 2: Verify locally**

Run: `act -j linux-gcc` if available; otherwise run each command of the linux-gcc job by hand in a clean clone (`git clone . /tmp/iwd2r-ci && cd /tmp/iwd2r-ci && ...`).
Expected: every step exits 0.

- [ ] **Step 3: Commit and push to trigger CI**

```bash
git add .github
git rm .github/workflows/ci-build.yml
git commit -m "Add CI for Linux and Windows presets with tests, notices, map and format checks"
```

Push the branch and confirm all four jobs are green before declaring project 1 done.

---

## Manual check

(Appended by Task 21 step 5 and Task 22 step 3.)

### Task 21: headless runs against the real install (2026-10-12)

Ruling R21: no display is reachable from the implementer (WSL has no WSLg, and a window on the Windows desktop cannot be seen), so step 5 ran headless with `IWD2R_SMOKE_FRAMES=300` against `C:\Games\Icewind Dale II` (the user's IWD2EE install). The INI files were backed up first and restored afterwards.

| Platform | Build | Command | Exit code | Hash line | Banner |
|---|---|---|---|---|---|
| Linux (WSL2) | linux-gcc, RelWithDebInfo | `SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2_DATA=... IWD2R_SMOKE_FRAMES=300 ./build/linux-gcc/iwd2r` | 0 | `framebuffer-hash: 16fe678f282a063e` | `BEGIN LOGGING SESSION` |
| Linux (WSL2) | linux-clang, RelWithDebInfo | same | 0 | `framebuffer-hash: 16fe678f282a063e` | yes |
| Linux (WSL2), `Full Screen=0` | linux-gcc | same | 0 | `1992366f4cdd973e`, then `c1a7d222523b573e` (windowed: not stable between runs) | yes |
| Windows host | windows-msvc-x64, RelWithDebInfo | `set IWD2_DATA=C:\Games\Icewind Dale II&& set IWD2R_SMOKE_FRAMES=300&& iwd2r.exe` | 0 | `framebuffer-hash: 63d1e53302479eff` | yes |
| Windows host | windows-msvc-x86, RelWithDebInfo | same | 0 | `framebuffer-hash: 63d1e53302479eff` | yes |

The fullscreen Linux hash was the same over four runs (GCC and Clang); the two Windows builds agree with each other. Linux and Windows differ, which project 1 allows. An all-black 800x600 frame would hash to `2bf050200a5e1325`, so the frames are not blank, but nothing here proves which screen was showing.

The runs need one INI change: the install's `icewind2.ini` has `Resolution=1366` (an IWD2EE setting). The decompiled engine only knows 800, 1024, 1600 and 2048: 1366 falls to the 800x600 layout while `CVideo::SCREENWIDTH` stays 1366, and `CScreenConnection::EnableMainPanel` then dereferences the missing border panel -5 (NULL, `CUIPanel::SetEnabled`, segfault before the first frame). All the runs above used `Resolution=800` in the install's INI, which was then restored. The crash follows from the decompiled code's own logic, not from a shim, but a user who keeps this INI sees it.

Files created or modified in the install by the runs (from `find -newer marker`):

- `icewind2.ini`: rewritten by the engine on exit (`First Run=0` became `1`), plus the `Resolution` edit; restored from the backup.
- `temp/default.toh`, `temp/default.tot`: the engine's talk-table override files, as the original creates them.
- A directory named `\` in the install root, created by the first Linux run: `CDimm::WriteSetUp` calls `_mkdir("\\")` (the decompiled loop passes a one-character string). On Windows `_chdir("\\")` reaches the drive root and nothing is created; on Linux the shim's `_mkdir` did not resolve paths. Fixed in this task (`_chdir`/`_mkdir`/`_access` now go through the resolver, which maps `\` to `/`). The directory is still in the install (nothing in the install is deleted by the implementer). Windows shows it under a private-use character name; it is empty and safe to delete.

Pending the user (visual check, plan step 5): the start screen appears; the options and keymaps screens open; the character editor opens; Escape and the window's close button exit with code 0; all of this with `Full Screen=0` and `Full Screen=1`, on Windows (`build\windows-msvc-x64\RelWithDebInfo\iwd2r.exe` with `IWD2_DATA` set) and, where a display is available, on Linux. None of these screens could be checked here.
