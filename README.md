# iwd2r

iwd2r (Icewind Dale 2 Resurrected) is a cross-platform port of the decompiled Icewind Dale II engine from [alexbatalov/iwd2-re](https://github.com/alexbatalov/iwd2-re). It builds natively on Linux and Windows, runs the original simulation underneath, and is meant to grow a high-fidelity renderer with a runtime toggle back to the classic 2D look and support for the IWD2EE mod. It is a modified version of iwd2-re; see [NOTICE.md](NOTICE.md).

## What this repository does not contain

- No game content.
- No original binary.
- No IWD2EE files.

Bring your own copy of Icewind Dale II: Complete from GOG. The DRM-free GOG version is the reference.

## Status

| # | Project | State |
|---|---|---|
| 1 | Portability foundation | Implemented on branch `first`, pending final review and a visual check |
| 2 | Verification harness | Not started |
| 3 | Finish the simulation | Not started |
| 4 | Render API and classic renderer | Not started |
| 5 | IEex native layer and Lua API | Not started |
| 6 | Remaster renderer | Not started |

Current numbers: four presets build; 184 tests pass on Linux; the start screen is reached headlessly on Linux and Windows.

Engine sites that still truncate pointers to 32 bits on 64-bit targets are listed for project 3 in [docs/64bit-cast-sites.md](docs/64bit-cast-sites.md).

## Build

Requires CMake 3.28. Presets: `linux-gcc`, `linux-clang`, `windows-msvc-x64`, `windows-msvc-x86`.

Ubuntu packages:

```
build-essential cmake ninja-build clang pkg-config libasound2-dev libpulse-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev libdrm-dev libgbm-dev libgl1-mesa-dev libegl1-mesa-dev libwayland-dev libdecor-0-dev libudev-dev libdbus-1-dev libpipewire-0.3-dev
```

Windows: Visual Studio 2022 and CMake.

SDL3, miniaudio and Catch2 are fetched by CMake at pinned versions.

```
cmake --preset linux-gcc && cmake --build --preset linux-gcc && ctest --preset linux-gcc
```

## Run

### Play

From WSL, one command syncs the working tree to a Windows checkout (`C:\Users\thier\iwd2r-build`), builds it with MSVC and opens the game window on the desktop:

```
python3 tools/play.py
```

An automated check on Linux, headless, driven by an input script that takes screenshots:

```
python3 tools/play.py --linux --headless --script scenarios/options.txt
```

Options are flags, never INI edits: `--windowed` (default) or `--fullscreen`, `--resolution 800|1024` (default 800), `--set "Section/Key=Value"` for anything else, `--frames N`, `--data PATH`. They reach the engine through `IWD2R_INI_OVERRIDE`. Each run writes `runs/<YYYYMMDD-HHMMSS>/` with `log.txt` (the game's output), the screenshots, `ini-backup/` and `summary.txt`. Every `*.ini` in the install is backed up before the run and restored byte-identical afterwards, also on Ctrl-C, because the engine rewrites `icewind2.ini` on exit. Script commands (`wait`, `click`, `rclick`, `move`, `key`, `type`, `shot`, `quit`) are documented in `platform/script.h`; the starter scenarios are in `scenarios/`.

#### Real-input runs (`--vm`)

`--vm` runs the same scenario files with genuine operating-system input in an isolated display, so nothing appears on your desktop: mouse and keyboard events travel OS → SDL → window shim → engine, as a player's do, and the in-engine script (`IWD2R_SCRIPT`) stays off.

```
python3 tools/play.py --vm linux --script scenarios/options.txt
python3 tools/play.py --vm linux --suite --jobs 3
python3 tools/play.py --vm all --suite
```

- `--vm linux` builds `linux-gcc`, starts `Xvfb :N` (N from 90 up) at the game resolution (800x600 or 1024x768), runs the game windowed on it with the X11 video driver and drives it with xdotool (`tools/realinput.py`). One-time setup: `sudo apt-get install -y xvfb xdotool imagemagick`.
- `--vm windows` runs each scenario in a fresh Windows Sandbox (`tools/winsandbox.py`, `tools/sandbox/driver.ps1`): the MSVC build from `C:\Users\thier\iwd2r-build` (with the MSVC runtime DLLs copied next to `iwd2r.exe`), a copy of the install in `C:\Users\thier\iwd2r-vm\game` (made once with rsync, refreshed when `CHITIN.KEY` changes; the real install is never mapped), networking off. The driver uses `SendInput` and captures the screen with System.Drawing. One-time setup, in an admin PowerShell, then reboot: `Enable-WindowsOptionalFeature -Online -FeatureName Containers-DisposableClientVM -All`. Only one sandbox runs at a time, so `--jobs` does not apply.
- `--vm all --suite` runs the suite on Linux, then on Windows when the sandbox is available, and prints both tables.
- `--suite` runs every `scenarios/*.txt`; `--jobs N` (default 1) runs that many Linux scenarios at once, each on its own display and CPU core; `--timeout S` (default 180) bounds each scenario.

How commands map to input: `click`/`rclick`/`move` move the real pointer to the game coordinates (the window's client area is the game screen) and press the button for 2 frames; `key` presses a real key; `type` types real key strokes; `wait N` lasts N/30 s and until the engine has presented N frames, so slow screen loads do not desynchronise the script; `shot NAME` captures the display (`NAME-screen.png`, the OS view with the engine-drawn pointer) and the engine's framebuffer (`NAME.png`); `quit` closes the window as a window manager's close button does (`WM_DELETE_WINDOW` on X11, `WM_CLOSE` on Windows) and confirms the engine's "quit?" message box with Return. The framebuffer shots and frame counts come from a small control channel: with `IWD2R_CONTROL_DIR` set, the engine answers `shot-NAME.req` and `frames.req` files in that folder once per presented frame; it injects no input.

What lands in `runs/<YYYYMMDD-HHMMSS>/` (one subfolder per scenario with `--suite`, under `linux/` and `windows/` with `--vm all`): `driver.log` (every action with timestamps), `log.txt` (the game's output), `script.txt`, `actions.json` (the parsed scenario), both kinds of screenshots and `summary.txt`; Linux runs add `xvfb.log` and `ini-backup/`, Windows runs `run.wsb`. A suite writes `suite.txt` with the table (scenario, exit code, duration, screenshot count). The exit code is the game's, 124 on a timeout, or non-zero when the driver failed; Xvfb, the game and the sandbox are closed on every exit path, including Ctrl-C.

### Manual

Point the engine at your install with `IWD2_DATA`:

```
IWD2_DATA=/path/to/Icewind\ Dale\ II ./build/linux-gcc/iwd2r
```

On Windows (Command Prompt, then PowerShell):

```
set IWD2_DATA=C:\Games\Icewind Dale II
$env:IWD2_DATA = "C:\Games\Icewind Dale II"
```

Alternatively, place a link named `IWD2` next to the executable or at the repository root. The directory must contain `CHITIN.KEY`. The engine writes saves, cache and INI files into it exactly as the original does.

`Resolution` must be 800 or 1024 for now. The IWD2EE mod sets 1366; rather than editing the file, override it for the run with `IWD2R_INI_OVERRIDE="Program Options/Resolution=800"` (entries are `Section/Key=Value`, separated by `;`), which `tools/play.py` does by default.

The game window is the size of the game resolution (800x600 or 1024x768); with `Full Screen=1` it is letterboxed onto the desktop. Like the original, iwd2r pins itself to one CPU core at start-up, so expect one busy core.

Headless smoke run, which prints a framebuffer hash:

```
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SMOKE_FRAMES=300 ./build/linux-gcc/iwd2r
```

## Deviations from the original

- Multiplayer is removed permanently: DirectPlay and the multiplayer screen are deleted, and the engine runs the original's single-player null-provider path.
- The OpenGL 3D video mode is inert: OpenGL calls are no-ops, and a failed 3D init falls back to 2D with a line on stderr instead of a dialog.
- Movies are skipped: Bink is not loaded, and a later project may decode them with FFmpeg.
- IME text composition is inert.
- GDI (TrueType) text is inert.
- Fullscreen presents as a borderless window scaled to the desktop with aspect preserved. Windowed, the window's client area is the game's whole screen, so it is game-sized rather than a desktop-sized popup.
- The single-instance check is removed: two copies can run against the same install, where the original refused to start a second.
- On Windows the executable is a console program for now, so a console window appears alongside the game window.
- The version resource is built in: the game name, version (`2, 0, 1, 0`, shown as v2.01 in the options screen) and build number come from strings compiled into iwd2r rather than from an `.exe` resource, and PrivateBuild names the iwd2r version and commit.
- Only `Resolution` 800 and 1024 are accepted at start-up; 1280, 1600 and 2048 crash inside the ported UI code and are refused with a message.
- Positioned 3D sounds play centred without distance attenuation when the 3D-sound option is on, because the DirectSound 3D interfaces are no-ops.
- The assert-time thread suspension is cooperative only: a thread can suspend itself but cannot freeze others.
- The speaker-portrait click in world dialog panels, which forwarded to the multiplayer screen, is now a no-op.
- The start screen is the original's connection screen with its multiplayer entry hidden.

## License

[LICENSE.md](LICENSE.md): the Sustainable Use License 1.0 applies to the whole repository, including new code. [NOTICE.md](NOTICE.md) records the modification. The original game, Icewind Dale II, is the property of its rights holders and is not included.

## References

- [alexbatalov/iwd2-re](https://github.com/alexbatalov/iwd2-re), the base of this project.
- IESDP, for file formats.
- GemRB, as a behavioral reference only (GPL; nothing is copied).
- benilla, as a pattern reference for the data environment variable, no bundled assets and generated-map conventions.
- IWD2EE, the mod this engine targets; it is not included.
