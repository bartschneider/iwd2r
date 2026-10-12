# iwd2r Project 1: Portability Foundation

Date: 2026-10-12
Status: design approved in conversation; this document is the written spec for review.
Scope: project 1 of six. The program is described in section 1; this spec designs only project 1.

## 1. Context

**iwd2r** (Icewind Dale 2 Resurrected) is a native engine that plays Icewind Dale II faithfully, supports the IWD2EE mod out of the box, and adds a high-fidelity renderer with a runtime toggle back to classic 2D, the way Diablo II: Resurrected did it. The original simulation runs underneath; renderers draw on top.

The base is a history-preserving fork of [alexbatalov/iwd2-re](https://github.com/alexbatalov/iwd2-re), the decompiled C++ source of IWD2. Measured state of the fork at the start of this project:

| Measure | Value |
|---|---|
| Source | 266,750 lines; 202 .cpp, 209 .h, plus `src/music/` |
| Platform | Win32 x86, MFC (`_AFXDLL`), DirectDraw, DirectSound, DirectPlay, OpenGL "3D mode"; MSVC 2019 only |
| Completeness | ~80% of functions, ~53% of code by size; 371 `TODO: Incomplete` markers |
| Reach | UI screens run (start, options, keymaps, party selection, character editor); no world screen |
| Evidence convention | 2,515 `// 0x4xxxxx` address comments link functions to the original binary |

### 1.1 Program decomposition

The whole effort is six projects, each with its own spec and plan, in this order:

1. **Portability foundation** (this spec): build and run on Linux and Windows without MFC or DirectX.
2. **Verification harness**: the vanilla exe as oracle, our code native, function-level and state-replay diffs.
3. **Finish the simulation** to a playable world screen.
4. **Render API boundary** and the classic renderer behind it.
5. **IEex native feature layer and Lua API**, with IWD2EE regression replays.
6. **Remaster renderer**, after its own ADR.

### 1.2 Decisions already made

- **Cross-platform from the start.** Linux and Windows are first-class from project 1. This is why portability precedes the verification harness: the harness compares our native build against the oracle, so our code must build natively first.
- **Multiplayer is dropped permanently.** It cannot sit behind an option, so it is recorded as a documented deviation from the original (ADR 0002).
- **Shim the APIs, keep the code.** MFC, Win32 and DirectX are replaced by subset implementations with the same names and semantics, so decompiled function bodies stay textually close to the binary. Rewriting call sites and dual MFC/shim builds were considered and rejected (ADR 0001).
- **Name.** The project, executable and CMake target are `iwd2r`. The repository keeps upstream history; it is not restarted in a new folder.

### 1.3 Hard rules

1. **Bring your own copy.** No original game files, no original binary, no IWD2EE files are ever committed, bundled or distributed. The engine reads the player's install through `IWD2_DATA`. The reference install is the DRM-free GOG version.
2. **No DRM or copy-protection work.**
3. **Faithfulness first.** The simulation matches the original, quirks included. Changes go behind options or, when an option is impossible, into a documented deviations list.

### 1.4 References

- IESDP for file formats; GemRB (GPL, read only, never copied) for behavior; benilla for repository patterns (data env var, no bundled assets, generated subsystem map, format code with no engine dependency).
- Test install on the development machine: `/mnt/c/Games/Icewind Dale II` (GOG, with IWD2EE installed; vanilla exe preserved in `iwd2ee/backup/1000/iwd2.exe`).

## 2. Goals and success criteria

Project 1 is done when all of the following hold:

1. `cmake --preset linux-gcc`, `linux-clang`, `windows-msvc-x64` and `windows-msvc-x86` configure and build with no MFC, no DirectX SDK and no Win32 call outside `platform/`.
2. With `IWD2_DATA` pointing at a GOG install, the executable reaches the start screen and the same UI screens upstream reaches today, on Linux and on Windows.
3. `iwd2r_tests` passes on every preset with no game data present.
4. CI builds all four presets, runs the tests, checks formatting and checks that `docs/MAP.md` is current.
5. NOTICE.md exists, every modified upstream file carries the notice line, and the README states what the repository does not contain.
6. ADR 0001 and ADR 0002 are committed.

## 3. Non-goals

- Finishing any `TODO: Incomplete` function. That is project 3.
- Any new rendering logic. The DirectDraw shim is a backend for the existing software renderer, nothing more.
- The render API, the oracle harness, IEex features, Lua, or the remaster renderer.
- Restoring the OpenGL 3D mode, movies, IME text entry or GDI text. These are made inert and listed as deviations until later projects restore them.
- Renaming the repository folder or remote. That is a one-line operation the owner performs when convenient.
- A separate user data directory. Saves, characters, cache and INI files are written into the install directory, as the original does, so they stay interchangeable with the original game and IWD2EE.

## 4. Changes to the upstream tree

### 4.1 Removed

| What | Size | Note |
|---|---|---|
| DirectPlay and lobby internals of `CNetwork` | most of 4,135 lines | The class survives as a loopback; see 7.7. |
| `CScreenMultiPlayer` | 3,637 lines | Deleted. The references in `CBaldurChitin` and `CScreenConnection` are removed. |
| `third_party/directplay` | headers | Deleted. |
| `register/reg32.exe` launch, the app mutex and the hardcoded GOG path in `main.cpp` | ~30 lines | Replaced by data resolution (7.6) and `SDL_main`. |
| `resource.rc` and `resource.h` | | The string table moves to a C++ header used on every platform. The only other content is VERSIONINFO, which CMake generates on Windows from the project version. |
| MFC and DirectX includes in `src/` | | Every `#include <afx*.h>`, `<ddraw.h>`, `<dsound.h>`, `<dplay.h>`, `<mmsystem.h>` and `<windows.h>` is replaced by a `compat/` or `platform/` header. |

### 4.2 Kept, contrary to a first reading

- **`CScreenConnection`** (5,170 lines) is the start screen; `CBaldurChitin` creates it at startup. It stays whole. Only its branch into `CScreenMultiPlayer` is removed and the multiplayer entry is hidden.
- **`CMultiplayerSettings`** (1,052 lines) is owned by `CInfGame` and holds party permissions that single-player screens load. Kept.
- **`CChatBuffer`** (81 lines) is used by the world, world-map and store screens and by `CMessage`. Kept.
- **`CGameSpy`** (60 lines) is already a stub and is referenced from several headers. Kept as the stub it is.
- **`CMessage`** (16,806 lines) is the engine's action queue even in single player and one of the most incomplete files. Kept whole; project 3 finishes it against the loopback.

### 4.3 Compiled but inert

Each item below keeps compiling, is forced off at runtime, and is listed in the README deviations until a later project restores it.

| Feature | Where | How it is made inert |
|---|---|---|
| OpenGL 3D mode | 319 `gl*`/`wgl*` calls across ten video files; `CVideo3d` (2,464 lines), `CChitin3d` (127) | A GL compatibility header declares the functions used; `wgl*` calls return failure; the 3D video mode is refused at startup so the 2D path is always taken. |
| Movies | `CBaldurProjector` loads Bink through LoadLibrary and GetProcAddress | The loader returns failure, so `PlayMovie` returns immediately. A later project may decode Bink with FFmpeg, which has a free decoder. |
| IME text composition | `CImm` (418 lines), `Imm*` calls | The `Imm*` functions return failure; composition never starts. |
| GDI text | `CVidFont` uses CreateFont and ExtTextOut on a surface DC | `GetDC` on a shim surface returns failure, so this path is never entered. |

## 5. Repository layout

```
src/            decompiled engine, structure unchanged; includes compat/ only
compat/         mfc.h, win_types.h, ini.h; MFC and Win32 subsets; no SDL dependency
platform/       window, events, clock, threads, fs, ddraw_shim, dsound_shim, network_loopback;
                the only code that includes SDL3 or miniaudio
tools/          gen_map.py writes docs/MAP.md from the source tree
tests/          Catch2 tests for compat/ and platform/, no game data needed
docs/adr/       0001-platform-layer.md, 0002-single-player-only.md
docs/MAP.md     generated subsystem map
docs/superpowers/specs/   this and later specs
third_party/    zlib stays vendored; directplay removed; SDL3, miniaudio, Catch2 fetched by CMake
NOTICE.md       modification notice required by LICENSE.md
```

`src/` depends on `compat/` and on the interface headers of `platform/`. `compat/` depends on the C++17 standard library only. `platform/` depends on `compat/`, SDL3 and miniaudio. Nothing depends on `tests/` or `tools/`.

## 6. The compat layer

### 6.1 `compat/win_types.h`

Fixed-width typedefs that keep the original's meaning on 64-bit and non-Windows targets:

- `BYTE` = `uint8_t`, `WORD`/`USHORT` = `uint16_t`, `SHORT` = `int16_t`, `DWORD`/`ULONG`/`UINT` = `uint32_t`, `LONG`/`INT` = `int32_t`, `BOOL` = `int32_t`, `BOOLEAN` = `uint8_t`, `COLORREF` = `uint32_t` with `RGB`, `GetRValue`, `GetGValue`, `GetBValue`.
- `WPARAM` = `uintptr_t`, `LPARAM` = `intptr_t`, `LRESULT` = `intptr_t`. The tree has 45 uses, all pointer-width safe.
- `POINT`, `SIZE`, `RECT` structs with `LONG` members, and `SetRect`, `PtInRect`, `IntersectRect`, `IsRectEmpty`, `OffsetRect`, `InflateRect`, `UnionRect`, `CopyRect`, `EqualRect`.
- `HANDLE`, `HWND`, `HINSTANCE`, `HDC`, `HFONT` as opaque pointers. `HWND` points at the platform window object.
- `VK_*` key codes with their Windows values, because Keymap.ini stores them. `MAX_PATH` = 260.
- `__stdcall`, `__cdecl`, `CALLBACK`, `WINAPI`, `__declspec(noreturn)` expand to nothing or the compiler equivalent off MSVC. The 420 `__stdcall` uses are OpenGL and Bink function-pointer typedefs.
- `min` and `max` function templates matching the 726 uses of the Windows macros.
- `_strnicmp`, `_stricmp`, `_vsnprintf`, `_chdir`, `_getcwd`, `_mkdir` map to the POSIX equivalents.
- `TRUE`, `FALSE`, `NULL`, `LOWORD`, `HIWORD`, `MAKELONG`, `INFINITE`.

`#pragma pack` is used at 18 sites for file-format structs and works unchanged on GCC, Clang and MSVC.

### 6.2 `compat/mfc.h`

Each class keeps the MFC name, method names, argument order and return conventions used in the tree. Semantics reproduce MFC where the engine could observe a difference. The measured usage that sizes the subset is in Appendix A.

**`CString`.** A single pointer member to a character buffer whose length and capacity are stored in a header before the first character, as MFC does. Two consequences the code relies on: `sizeof(CString)` equals a pointer, so structs keep their size; and passing a `CString` through varargs to `Format("%s", s)` passes the character pointer. Methods: constructors from `const char*`, `(char, count)` and copies; `GetLength`, `IsEmpty`, `Empty`, `GetAt`, `SetAt`, `operator[]`, `operator const char*`; `Format` and `FormatV` with printf semantics; `Compare` (strcmp), `CompareNoCase` (ASCII, like `_stricmp`); `Left`, `Right`, `Mid` with MFC clamping and no exceptions; `Find` for char and substring with an optional start index, `FindOneOf`, `ReverseFind`; `TrimLeft` and `TrimRight` with no argument (whitespace), a char, or a set; `MakeUpper`, `MakeLower` (ASCII); `GetBuffer`, `ReleaseBuffer`; `SpanIncluding`, `SpanExcluding`; `Remove`, `Replace`, `Delete`, `Insert`; `operator+`, `+=`, `==`, `!=`, `<`, `>` for `CString` and `const char*` on either side. Copies are deep; no reference counting is needed for correctness.

**`CPoint`, `CSize`, `CRect`.** Inherit from `POINT`, `SIZE`, `RECT` so `&rect` converts to `RECT*`. Methods and operators used by the tree: `Offset`, `operator+`, `-`, `+=`, `-=`, `==`, `!=`; `CRect::Width`, `Height`, `TopLeft`, `BottomRight`, `CenterPoint`, `PtInRect`, `IsRectEmpty`, `SetRect`, `SetRectEmpty`, `NormalizeRect`, `InflateRect`, `DeflateRect`, `OffsetRect`, `IntersectRect`, `UnionRect`, `EqualRect`, `CopyRect`.

**`CPtrList`, `CObList`, `CList<T, ARG>`, `CTypedPtrList<BASE, T*>`.** A doubly linked list whose nodes are the `POSITION` values. `GetHeadPosition`, `GetTailPosition`, `GetNext(pos)` and `GetPrev(pos)` advance the position and return the element; `GetAt(pos)`, `SetAt(pos, v)`; `AddHead`, `AddTail` return the new position; `RemoveHead`, `RemoveTail`, `RemoveAt(pos)`, `RemoveAll`; `GetHead`, `GetTail`, `GetCount`, `IsEmpty`; `Find(value, startAfter)`, `FindIndex(n)`; `InsertBefore`, `InsertAfter`. `RemoveAt` on a position obtained before `GetNext` advanced is valid, since the engine does this while iterating. `CTypedPtrList<CPtrList, POSITION>` (a list of positions) must also compile.

**`CArray<T, ARG>`, `CPtrArray`, `CTypedPtrArray<BASE, T*>`, `CDWordArray`.** A growable array: `Add`, `SetSize`, `GetSize`, `GetUpperBound`, `GetAt`, `SetAt`, `SetAtGrow`, `operator[]`, `InsertAt`, `RemoveAt`, `RemoveAll`, `GetData`.

**`CMapWordToPtr`, `CMapStringToString`.** One use each: `Lookup`, `SetAt`, `RemoveKey`, `GetStartPosition`, `GetNextAssoc`, `RemoveAll`. Backed by `std::unordered_map`; string keys compare case-sensitively as in MFC.

**`CFile`.** `Open(path, flags)` with `modeRead`, `modeWrite`, `modeReadWrite`, `modeCreate`, `modeNoTruncate`, `shareDenyNone`, `shareDenyWrite`, `shareExclusive`, `typeBinary`; `Read` returns the byte count; `Write`; `Seek(offset, begin|current|end)`; `SeekToBegin`, `SeekToEnd`; `GetLength`, `GetPosition`, `SetLength`, `Close`, `Abort`; static `GetStatus`, `SetStatus`, `Remove`. Errors are reported by return value. The tree never catches `CFileException`, so the shim never throws. Paths pass through the platform path resolver (7.5).

**`CCriticalSection`, `CSingleLock`, `CMutex`, `CEvent`.** `CCriticalSection` is recursive. `CSingleLock(&cs, TRUE)` locks in the constructor; `Lock`, `Unlock`, `IsLocked`. The 294 `CSingleLock` uses follow this pattern.

**`CTime`.** `GetCurrentTime`, `GetHour`, `GetMinute`, `GetSecond`, `GetTime`, and the static `GetTickCount` alias used once.

**`CWnd`.** One instance exists, `CChitin::cWnd`. The shim exposes `GetSafeHwnd`, which returns the platform window handle, and nothing else.

### 6.3 `compat/ini.h`

The tree makes 245 profile calls: `GetPrivateProfileStringA` (27), `GetPrivateProfileIntA` (105) and `WritePrivateProfileStringA` (113), against `icewind2.ini`, `Party.ini`, `Keymap.ini`, `Language.ini` and `Chitin.ini`. The module reproduces the Win32 rules the engine can observe:

- Section and key lookup is case-insensitive. Leading and trailing whitespace around keys and values is trimmed. A value wrapped in matching single or double quotes has them removed.
- `GetPrivateProfileString` copies at most `nSize - 1` characters and returns the count copied. A missing key returns the default. A null key enumerates key names, a null section enumerates section names, each as a double-null-terminated list, truncated to `nSize - 2`.
- `GetPrivateProfileInt` parses a leading decimal integer with an optional sign; a value that does not start with a digit returns the default.
- `WritePrivateProfileString` creates the file and section if needed, replaces the first matching key in place, appends new keys at the end of the section, and with a null value deletes the key, or with a null key deletes the section. Writes go through to disk immediately.
- A relative file name resolves against the game directory (7.6), not against a Windows directory. Absolute Windows paths are re-rooted by the path resolver (7.5).
- Comment lines beginning with `;` and blank lines are preserved on write.

Files are cached in memory after first read and invalidated by the module's own writes.

## 7. The platform layer

Interfaces are declared in `platform/*.h` with no SDL or miniaudio types exposed; `src/` includes those headers through `compat/`.

### 7.1 Window and events

`CChitin` registers a window class and installs a window procedure. The shim replaces this with an SDL3 window created by `platform::Window::Create(title, width, height, fullscreen)` and an event pump, `platform::PumpEvents()`, called where `CChitin` runs its `PeekMessage` loop today. The pump translates SDL events into calls on the same handler entry points the window procedure reaches now:

| Window message handled by `CChitin` | SDL source | Action |
|---|---|---|
| `WM_CLOSE`, `WM_DESTROY`, `WM_QUERYENDSESSION` | quit request | Same close-confirmation path, then shutdown. |
| `WM_SIZE`, `WM_MOVE`, `WM_DISPLAYCHANGE` | window resized, moved, display changed | Recompute the client rectangle, redraw, and run the existing alt-tab logic when the window minimizes or restores. |
| `WM_ACTIVATEAPP`, `WM_KILLFOCUS` | focus gained, focus lost | Same handlers. |
| `WM_MOUSEWHEEL` | mouse wheel | Append to `m_lMouseWheel` as today. |
| `WM_PAINT`, `WM_ERASEBKGND`, `WM_SETCURSOR` | none | Not needed; the engine redraws every frame and owns its cursor. |
| `WM_SYSKEYDOWN` | key down with Alt | Same handler. |
| `WM_IME_*`, `WM_INPUTLANGCHANGE` | none | Inert in project 1 (section 4.3). |

Input is polled, not event-driven. The engine calls `GetAsyncKeyState` (15 sites) and `GetCursorPos` (1 site) from its update functions. The shim serves `GetAsyncKeyState` from `SDL_GetKeyboardState` through a VK-to-scancode table that covers every `VK_*` the engine references, and the mouse buttons from `SDL_GetMouseState`. `GetCursorPos`, `ClientToScreen` and `ScreenToClient` use the window's position so the engine's screen-space arithmetic is unchanged. `GetKeyboardLayout` returns a constant US layout. `GetSystemMetrics` for screen size reads the current display mode. `MessageBoxA` maps to `SDL_ShowSimpleMessageBox`. `LoadCursor`/`SetCursor` for the arrow cursor map to the SDL system cursor; the engine draws its own in-game cursors.

Rendering and presenting stay on the main thread, as in the original, where the message loop thread also renders.

### 7.2 DirectDraw shim

`platform/ddraw_shim.h` declares `IDirectDraw`, `IDirectDraw2`, `IDirectDrawSurface`, `IDirectDrawClipper` and `IDirectDrawPalette` as plain C++ classes with the method subset the tree calls, plus the `DDSURFACEDESC`, `DDPIXELFORMAT`, `DDBLTFX`, `DDCOLORKEY`, `DDCAPS` structs with the fields the tree reads, and the `DD_OK`, `DDERR_*`, `DDSD_*`, `DDSCAPS_*`, `DDLOCK_*`, `DDBLT_*`, `DDBLTFAST_*`, `DDCKEY_*` constants used. `DirectDrawCreate` returns the shim object.

| Method | Calls | Behavior |
|---|---|---|
| `QueryInterface`, `AddRef`, `Release` | 11, 223 | Reference counting; `QueryInterface` for `IDirectDraw2` returns the same object. |
| `SetCooperativeLevel`, `SetDisplayMode`, `RestoreDisplayMode`, `GetCaps` | 5, 3, 1, 2 | Record mode; 32-bpp only, windowed or borderless fullscreen (see below). `GetCaps` reports no hardware acceleration and no palette, which steers the engine to the software paths the fork already implements. |
| `CreateSurface` | 18 | Primary with an attached back buffer, or offscreen plain. Each surface owns a 32-bpp XRGB8888 buffer with a 4-byte-aligned pitch. |
| `GetAttachedSurface` | 1 | Returns the back buffer. |
| `Lock`, `Unlock` | 14, 20 | Fill `lpSurface`, `lPitch`, `dwWidth`, `dwHeight` and the pixel format; optional rectangle. |
| `Blt`, `BltFast` | 1, 1 | Software copy with clipping to the destination and, when `DDBLTFAST_SRCCOLORKEY` or `DDBLT_KEYSRC` is set, the source color key. `DDBLT_COLORFILL` fills. |
| `SetColorKey` | 14 | Store the key. |
| `Flip` | 23 | Swap primary and back buffer, upload the primary to one streaming `SDL_Texture`, render it to the window, present. |
| `Restore`, `IsLost` | 10 | No-ops returning success; shim surfaces are never lost. |
| `SetPalette`, `CreatePalette`, `GetPixelFormat`, `CreateClipper`, `SetClipper`, `SetHWnd` | 5, 1, 1, 1, 1, 1 | Palette calls succeed and are ignored in 32-bpp; pixel format reports the XRGB masks; clipper is recorded and ignored since blits already clip. |
| `GetDC`, `ReleaseDC` | 6, 6 | Return `DDERR_UNSUPPORTED`, which makes the GDI text path inert. |

Requesting 8, 16 or 24 bits per pixel returns `DDERR_INVALIDMODE`. Only 32-bpp is implemented upstream, and the engine already falls back on that error. A fullscreen request (`DDSCL_FULLSCREEN` followed by `SetDisplayMode`) is honored as a borderless fullscreen window at the desktop resolution, with the primary surface presented scaled to fit and its aspect ratio preserved. The engine's own resolution and every simulation coordinate are unchanged.

### 7.3 DirectSound shim

`platform/dsound_shim.h` declares `IDirectSound`, `IDirectSoundBuffer` and `IDirectSound3DListener` with the subset the tree calls and the `DSBUFFERDESC`, `WAVEFORMATEX`, `DSCAPS` structs and `DSBPLAY_*`, `DSBSTATUS_*`, `DSBCAPS_*`, `DSBVOLUME_*`, `DSBPAN_*` constants used. `DirectSoundCreate` returns the shim object. Backing is miniaudio with one engine, one device, and one `ma_sound` per secondary buffer reading from a memory data source.

| Method | Behavior |
|---|---|
| `SetCooperativeLevel`, `Initialize`, `GetCaps` | Record and succeed. |
| `CreateSoundBuffer` | Primary: `SetFormat` records the device format. Secondary: allocate the PCM buffer of the requested size and format. |
| `Lock`, `Unlock` | Return pointers into the PCM buffer, with wraparound as DirectSound does (two pointer-length pairs). The streamed music in `src/music/` writes through this into a ring. |
| `Play` | Start; `DSBPLAY_LOOPING` loops. `Stop` stops and keeps position. |
| `SetVolume` | Hundredths of a decibel from -10000 to 0, converted to linear gain; -10000 is silence. `GetVolume` returns the stored value. |
| `SetPan` | -10000 to 10000 in hundredths of a decibel of attenuation applied to the opposite channel, as DirectSound defines it. |
| `SetFrequency`, `GetFrequency` | Playback rate in Hz; `DSBFREQUENCY_ORIGINAL` restores the buffer's rate. Implemented as a pitch ratio. |
| `GetStatus` | `DSBSTATUS_PLAYING` and `DSBSTATUS_LOOPING` reflect the sound's state. |
| `SetCurrentPosition`, `GetCurrentPosition` | Byte offsets into the PCM buffer. |
| `DuplicateSoundBuffer` | New sound sharing the same PCM data. |
| 3D listener methods | No-ops returning success. |

The null miniaudio backend is used in tests so they run without an audio device.

### 7.4 Clock and threads

- `GetTickCount` (71 sites) and `timeGetTime` read `platform::Clock::NowMs()`, milliseconds since process start from a monotonic source. The source is an interface with a swappable implementation, so project 2 can drive replays with a deterministic clock. `Sleep` maps to `std::this_thread::sleep_for`.
- The two multimedia timers become platform timer threads that call the same callbacks at the same periods: `timeSetEvent` in `CChitin` drives `AsynchronousUpdate` at `TIMER_UPDATES_PER_SECOND`, and `timeSetEvent` in `src/music/sound.cpp` drives the 40 ms sound fade step. `timeKillEvent` stops them; `timeBeginPeriod` is a no-op. The timer user value is pointer-width.
- The five `_beginthread` sites (`SearchThreadMain` in `CInfGame`; `RSThreadMain`, `MessageThreadMain`, `MusicThreadMain`, `MainAIThread` in `CChitin`) become `platform::Thread::Start(fn, arg)` returning a `HANDLE` that `WaitForSingleObject` joins and `CloseHandle` releases. `CreateEvent`, `SetEvent`, `ResetEvent` and waiting on an event handle map to a small `Event` class on a condition variable.
- Threading structure is the original's and is kept. Determinism across threads is a project 2 concern.

### 7.5 Filesystem and paths

`platform/fs.h` implements the Win32 file functions the tree calls, on `std::filesystem`:

- `FindFirstFileA`, `FindNextFileA`, `FindClose` with Windows wildcard semantics (`*` and `?`, case-insensitive), filling `WIN32_FIND_DATAA` with name, attributes, size and times.
- `GetFileAttributesA`, `DeleteFileA`, `CopyFileA`, `CreateDirectoryA`, `GetDiskFreeSpaceA`, `GetCurrentDirectoryA`, `SetCurrentDirectoryA`, `GetModuleFileNameA`.
- `GetLogicalDrives`, `GetDriveTypeA` and `GetVolumeInformationA` return "no drives". The CD-ROM scan in `CBaldurChitin` then finds nothing, and the `cd%d:\` aliases resolve through the INI as on a full install.

**Path resolution** applies to every path the engine opens, through `CFile`, the INI module and the functions above:

1. Backslashes become the native separator.
2. A path with a drive letter is re-rooted: if it lies under the install prefix recorded in the INI's alias section, the prefix is replaced with `IWD2_DATA`; otherwise it is treated as relative to `IWD2_DATA`. This keeps an INI written by the Windows installer, with entries like `HD0:=C:\Games\Icewind Dale II\`, working on Linux.
3. On case-sensitive filesystems each component is matched case-insensitively against a cached directory listing, so `override/sw1h01.itm` finds `Override/SW1H01.ITM`.

### 7.6 Game data resolution

At startup, in order: the `IWD2_DATA` environment variable; a link or directory named `IWD2` next to the executable, then at the repository root; the current directory if it contains `CHITIN.KEY`. The directory must contain `CHITIN.KEY`; the first candidate that does is used. If none does, a message box names the variable and the process exits with a non-zero code. The chosen directory becomes the working directory, which is what the original assumes.

### 7.7 Loopback network

`CNetwork` keeps its name and the members that kept code uses (Appendix A.5). The DirectPlay interfaces, lobby, session enumeration, player threads and `CGameSpy` traffic are removed from it. Behavior:

- Exactly one player exists, the local player, who is the host. `GetPlayerID`, `GetHostPlayerID`, `GetPlayerName`, `GetHostPlayerName`, `FindPlayerLocationByID`, `FindPlayerIDByName` answer for that player.
- `GetSessionOpen` and `GetSessionHosting` return what the original returns in a single-player game: open and hosting once the single-player flow starts a game, closed before. The exact transitions are driven by the same call sites that drive them today (`CScreenConnection` and `CScreenSinglePlayer`), unchanged.
- `SendSpecificMessage` appends to the local inbound queue that `PeekSpecificMessage` and `FetchSpecificMessage` read, so `CMessage` traffic to the local player round-trips without a transport.
- `GetServiceProvider` and `GetServiceProviderType` report "none". `KickPlayer`, `MakePlayersVisible`, `SetJoinEnabled`, `SetInSessionOptions` and `CloseSession` update state and succeed.

Project 2's oracle confirms these values against the vanilla exe in single-player runs; project 3 finishes `CMessage` against this loopback.

## 8. Build

- CMake 3.28 (Ubuntu 24.04's version) with `CMakePresets.json`: `linux-gcc`, `linux-clang`, `windows-msvc-x64`, `windows-msvc-x86`. The x86 preset exists so project 2 can compare struct layouts against the 32-bit oracle.
- Targets: `iwd2r_compat` (static), `iwd2r_platform` (static), `iwd2r` (executable, `SDL_main`), `iwd2r_tests`.
- Dependencies through `FetchContent` at pinned release tags: SDL3, miniaudio, Catch2 v3. zlib stays vendored under `third_party/`. Ubuntu 24.04 packages SDL2 only, which is why SDL3 is fetched.
- Flags: C++17; `-fsigned-char` on GCC and Clang so `char` matches MSVC on every architecture; decompiled code builds with warnings allowed; `compat/`, `platform/`, `tools/` and `tests/` build with `-Wall -Wextra -Werror` or `/W4 /WX`.
- `CMAKE_MFC_FLAG`, `_AFXDLL`, `_MBCS` and the DirectX link libraries are removed.

## 9. Governance

**License and notices.** LICENSE.md (Sustainable Use License 1.0) is unchanged and applies to the whole repository, new files included. `NOTICE.md` states that iwd2r is a modified version of alexbatalov/iwd2-re, with the upstream URL and the date of the fork. The README links NOTICE.md in its first screen. Every upstream file this project modifies gets one leading comment line, `// Modified for iwd2r; see NOTICE.md`. New files start with `// iwd2r. Distributed under LICENSE.md.`

**README.** Rewritten for iwd2r: what it is, the program decomposition, the status table, how to build, how to point it at a GOG install, the statement that the repository contains no game content, no original binary and no IWD2EE files, the IWD2EE support goal, and a **Deviations** section listing: multiplayer removed; OpenGL 3D mode, movies, IME text entry and GDI text inert pending later projects.

**ADRs** in `docs/adr/`, each with Context, Decision, Alternatives, Consequences:

- `0001-platform-layer.md`: shim the APIs over rewriting call sites or a dual MFC/shim build; SDL3 fetched and pinned; miniaudio for audio; 64-bit default with a 32-bit Windows preset for the oracle.
- `0002-single-player-only.md`: multiplayer removed permanently; `CNetwork` kept as a loopback; `CMessage` kept as the action queue; recorded as a deviation.

**Generated map.** `tools/gen_map.py` (Python 3, standard library only) walks `src/`, `compat/` and `platform/` and writes `docs/MAP.md` with one row per subsystem family (`CAI*`, `CGame*`, `CScreen*`, `CUI*`, `CVid*` and `CInfinity`, `CRes*` and `CDimm*`, `CSound*` and `music/`, `CMessage` and `CNetwork`, and everything else), giving files, lines, remaining `TODO: Incomplete` markers, address-comment count, and which `platform/` headers the family includes. The CI map check regenerates and diffs.

## 10. Testing and CI

Framework: Catch2 v3. All tests except the last run with no game data.

| Area | What is asserted |
|---|---|
| `CString` | Every method in 6.2 against MFC's documented results: clamping in `Left`, `Right`, `Mid`; `Find` with start index; the three `Trim` forms; `Format` with a `CString` argument through `%s`; `CompareNoCase` on mixed case; `GetBuffer`/`ReleaseBuffer` round trip; `sizeof(CString) == sizeof(void*)`. |
| Lists and arrays | `POSITION` iteration order; `RemoveAt` of the position just returned while iterating; `Find` with a start position; `FindIndex`; `SetAtGrow` growth; `CTypedPtrList<CPtrList, POSITION>` compiles and works. |
| INI | Each rule in 6.3, round-tripped through a temporary file, including truncation counts, enumeration lists, key and section deletion, and comment preservation. |
| Filesystem | Case-insensitive resolution on a case-sensitive temp tree; wildcard matching; drive-letter re-rooting with and without the install prefix. |
| DirectDraw shim | Lock reports the requested pitch and format; color-keyed `BltFast` skips keyed pixels; blits clip at every edge; `Flip` swaps buffers and produces a presented frame on a hidden window. |
| DirectSound shim | Decibel to gain at -10000, -600 and 0; pan at the extremes and center; `GetStatus` after `Play`, looping `Play` and `Stop`; `Lock` wraparound pointers. Null audio backend. |
| Clock | A swapped source changes `GetTickCount`; the default source is monotonic. |
| Loopback | `SendSpecificMessage` then `FetchSpecificMessage` returns the same bytes; player and host queries describe one local host. |
| Smoke (local only, tag `[data]`) | With `IWD2_DATA` set: start, reach the start screen, run 300 frames, exit with code 0, print a hash of the final framebuffer for manual comparison between platforms. Skipped when `IWD2_DATA` is unset. |

**CI** (`.github/workflows/ci.yml`): jobs on `ubuntu-24.04` with GCC and with Clang, and on `windows-latest` with MSVC x64 and x86. Each job configures its preset, builds, runs `iwd2r_tests`, runs `clang-format --dry-run --Werror` on new and modified files, and regenerates `docs/MAP.md` and fails on a diff. No game data is ever present in CI.

## 11. Sequencing for the implementation plan

1. `compat/` headers with their tests, built and tested alone on Linux.
2. MSVC x64 build of the whole tree against `compat/` with MFC removed, still using the real DirectX for one step. This proves the shim is complete before the platform changes.
3. `platform/` shims, deletions and inert stubs from section 4; MSVC build switches to SDL3 and miniaudio; Windows smoke test passes.
4. Linux build with GCC and Clang; Linux smoke test passes.
5. NOTICE.md, file notice lines, README, ADRs, `gen_map.py`, `CMakePresets.json`, CI.

## 12. Risks

- **MFC quirks the shim misses.** The tree may depend on an MFC behavior not listed in 6.2. Mitigation: step 2 of the sequencing compiles the whole tree against the shim on Windows, where any behavioral difference shows up against the known-good upstream build before platform changes add noise.
- **Timing-dependent behavior.** The original runs five threads and a multimedia timer. Reproducing their scheduling exactly is impossible; project 2's replay diffs will show where it matters, and the swappable clock is the hook for fixing it.
- **64-bit layout.** `/* 0xNNNN */` offset comments document the 32-bit layout and are not relied on by code. Only two pointer-to-integer casts exist, one in `CVidMode` and one passing a pointer as the timer user value in `src/music/sound.cpp`; both are rewritten to pointer-width types. Save-file structs are packed and pointer-free. Project 2 verifies save round trips.
- **Loopback session semantics.** Section 7.7 states the intended behavior; the exact transitions are confirmed only when the oracle exists. The UI screens upstream reaches today are the acceptance test for project 1.

## Appendix A: Measurements

Counts are occurrences in `src/` at commit 4fa36bc.

### A.1 MFC classes

| Class | Uses | | Class | Uses |
|---|---|---|---|---|
| `CString` | 3,757 | | `CFile` | 97 |
| `CPoint` | 3,690 | | `CCriticalSection` | 27 |
| `CRect` | 975 | | `CList` | 24 |
| `CSingleLock` | 294 | | `CObList` | 15 |
| `CSize` | 234 | | `CTypedPtrArray` | 10 |
| `CPtrList` | 121 | | `CWnd`, `CTime`, `CArray` | 7 each |
| `CTypedPtrList` | 113 | | `CDWordArray`, `CMapWordToPtr`, `CMapStringToString` | 1 each |

### A.2 `CString` methods

GetLength 386, Format 341, IsEmpty 286, GetAt 256, Right 115, TrimLeft 101, Find 99, TrimRight 94, Left 94, GetBuffer 47, CompareNoCase 45, MakeUpper 40, Compare 40, Mid 39, SetAt 38, FindOneOf 34, ReleaseBuffer 17, ReverseFind 12, SpanExcluding 10, Remove 10, Empty 7, MakeLower 6, Replace 3, SpanIncluding 2, Delete 2.

### A.3 List and array methods

AddTail 233, GetNext 225, GetHeadPosition 221, GetCount 107, Find 99, GetTailPosition 69, RemoveAll 65, GetTail 64, RemoveAt 56, RemoveHead 38, FindIndex 25, GetSize 24, Add 23, GetPrev 22, SetSize 17, RemoveTail 17, GetHead 17, AddHead 9, InsertBefore 6, Lookup 4, RemoveKey 2, GetStartPosition 2, GetNextAssoc 2, SetAtGrow 1, InsertAfter 1, GetUpperBound 1.

### A.4 Win32 functions outside video, sound and network files

GetPrivateProfileInt 105, WritePrivateProfileString 113, GetPrivateProfileString 27, GetTickCount 71, GetProcAddress 59, FindFirstFile 22, GetSystemMetrics 20, FindClose 19, GetAsyncKeyState 15, MessageBox 13, Sleep 13, CloseHandle 12, FindNextFile 11, DeleteFile 9, GetDC/ReleaseDC 8 each, WaitForSingleObject 6, SendMessage 6, lstrlen 6, LoadLibrary 4, CopyFile 4, GetFileAttributes 4, DestroyWindow 4, SetWindowPos 4, GetVersionEx 3, SetCurrentDirectory 3, ClientToScreen 3, lstrcpy 3, ResetEvent 2, GetDiskFreeSpace 2, GetClientRect 2, ScreenToClient 2, PostMessage 2, CreateFont 2, GetKeyboardLayout 2, and one each of CreateEvent, SetEvent, GetCurrentDirectory, GetModuleFileName, GetModuleHandle, CreateDirectory, GetDriveType, GetLogicalDrives, GetCursorPos, GlobalMemoryStatus, PeekMessage, GetMessage, DispatchMessage, TranslateMessage, CoInitialize, CoCreateInstance, DirectDrawCreate, DirectPlayCreate, DirectPlayLobbyCreate; DirectSoundCreate 2. Thread starts: `_beginthread` 5. IME: ImmGetCompositionString 8, ImmGetContext 7.

### A.5 `CNetwork` members used by kept code

GetSessionOpen 284, GetSessionHosting 243, GetServiceProvider 97, m_idLocalPlayer 43, SendSpecificMessage 43, FindPlayerLocationByID 26, GetPlayerID 24, CloseSession 24, GetHostPlayerName 22, GetPlayerName 14, GetHostPlayerID 13, m_nLocalPlayer 10, m_nHostPlayer 8, PeekSpecificMessage 6, FindPlayerIDByName 6, FetchSpecificMessage 6, m_nServiceProvider 4, KickPlayer 4, m_psPlayerName 3, MakePlayersVisible 3, GetServiceProviderType 3, FindPlayerLocationByName 2, and one each of m_sSessionPassword, m_sSessionNameToMake, m_sLocalPlayerName, m_bAllowNewConnections, SetJoinEnabled, SetInSessionOptions, OnCloseSession, GetRawPlayerName, GetRawPlayerID. Referenced from 42 files outside the network and multiplayer screens.

### A.6 DirectX and window surface

DirectDraw: Release 223, Flip 23, Unlock 20, CreateSurface 18, SetColorKey 14, Lock 14, QueryInterface 11, Restore 10, GetDC 6, ReleaseDC 6, SetPalette 5, SetCooperativeLevel 5, SetDisplayMode 3, GetCaps 2, and one each of SetHWnd, SetClipper, RestoreDisplayMode, GetPixelFormat, GetAttachedSurface, CreateClipper, CreatePalette, BltFast, Blt. DirectSound: SetVolume 9, CreateSoundBuffer 4, GetStatus 3, SetFrequency 2, Play 2, Stop 1, SetPan 1, SetFormat 1, Initialize 1, 3D listener 3. Window messages handled: WM_MOUSEWHEEL, WM_DESTROY, WM_CLOSE, WM_QUERYENDSESSION, WM_SIZE, WM_MOVE, WM_DISPLAYCHANGE, WM_ACTIVATEAPP, WM_KILLFOCUS, WM_PAINT, WM_ERASEBKGND, WM_SETCURSOR, WM_SYSKEYDOWN, WM_INPUTLANGCHANGE and seven WM_IME messages. OpenGL: 319 `gl*`/`wgl*` calls in CVidImage, CParticle, CInfinity, CVidBitmap, CVidFont, CVideo3d, CVidMosaic, CVidCell, CVidInf, CVidPoly.

### A.7 Portability markers

`__stdcall` 420 (406 in CVideo3d.h, 13 in CBaldurProjector.h, 1 in CChitin.cpp); `CALLBACK` 11; `WINAPI` 6; `__declspec(noreturn)` 1; `#pragma pack` 18; `min`/`max` 726; `lParam`/`wParam` 45; pointer-to-integer casts 2; `_chdir` 9, `MAX_PATH` 7, `_strnicmp` 5, `_vsnprintf` 4, `_getcwd` 3, `_mkdir` 2.
