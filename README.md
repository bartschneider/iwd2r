# iwd2r

iwd2r (Icewind Dale 2 Resurrected) is a cross-platform port of the decompiled Icewind Dale II engine from [alexbatalov/iwd2-re](https://github.com/alexbatalov/iwd2-re). It builds natively on Linux and Windows, runs the original simulation underneath, and is meant to grow a high-fidelity renderer with a runtime toggle back to the classic 2D look and the IWD2EE mod built in. iwd2r is a modified version of iwd2-re.

## What this repository does not contain

- No game content.
- No original binary.

Bring your own copy of Icewind Dale II: Complete from GOG. The DRM-free GOG version is the reference. That is all a player needs: IWD2EE is part of iwd2r's own source, and its edits to the game's files are applied on the player's machine.

## Status

| # | Project | State |
|---|---|---|
| 1 | Portability foundation | Done |
| 2 | Verification harness | Done |
| 3 | Finish the simulation | In progress: milestone 1 (a new party enters the world) in final integration; milestone 2 (move and interact) executing |
| 4 | Render API and classic renderer | Done (classic presenter default; SDL3 GPU presenter opt-in with nearest/sharp/smooth/FSR/NIS filters; the HD classic renderer is a later project) |
| 5 | IEex native layer and Lua API | Not started |
| 6 | Remaster renderer | Not started |

Current numbers: four presets build; 329 tests pass on Linux; character generation matches the original field for field through party formation, and a new party reaches the start area headlessly on Linux and in Windows Sandbox.

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

From WSL, one command syncs the working tree to a Windows checkout, builds it with MSVC and opens the game window on the desktop. Each worktree has its own checkout: `C:\Users\thier\iwd2r-build` for the `first` worktree, `C:\Users\thier\iwd2r-build-<worktree folder>` for any other, or `$IWD2R_WIN_CHECKOUT` when set:

```
python3 tools/play.py
```

An automated check on Linux, headless, driven by an input script that takes screenshots:

```
python3 tools/play.py --linux --headless --script scenarios/options.txt
```

Options are flags, never INI edits: `--windowed` (default) or `--fullscreen`, `--resolution W|desktop` (default 800), `--scale 1|2|3|4|auto` (the `Scale` key), `--set "Section/Key=Value"` for anything else (`--set "Program Options/Height=1080"` for the height), `--frames N`, `--data PATH`, `--shot-presented` (each screenshot also writes `NAME-presented.png`, the frame as the presenter drew it, through `IWD2R_SHOT_PRESENTED`). They reach the engine through `IWD2R_INI_OVERRIDE`. Each run writes `runs/<YYYYMMDD-HHMMSS>/` with `log.txt` (the game's output), the screenshots, `ini-backup/` and `summary.txt`. Every `*.ini` in the install is backed up before the run and restored byte-identical afterwards, also on Ctrl-C, because the engine rewrites `icewind2.ini` on exit. Script commands (`wait`, `click`, `rclick`, `move`, `key`, `type`, `shot`, `quit`, `origin`) are documented in `platform/script.h`; the starter scenarios are in `scenarios/`. Click and move coordinates are in the original's 800x600 UI by default (`origin ui`): at larger sizes the UI is centred, and the engine (or the real-input driver) adds the centring offset, so one scenario clicks the same buttons at every resolution; `origin screen` switches to screen pixels.

The game data is a vanilla copy by default, because the project verifies against vanilla first: Linux runs read `${IWD2R_HOME:-~/.local/share/iwd2r}/game` (as `tools/gates.sh` does), and Windows runs (the host build and `--vm windows`) read `C:\Users\thier\iwd2r-oracle\game`, the verified copy that `python3 tools/oracle.py vanilla` builds and marks with `vanilla.ok`. Make the Linux copy from it with `rsync -a --exclude temp --exclude cache --exclude tempsave --exclude mpsave --exclude ScrnShot /mnt/c/Users/thier/iwd2r-oracle/game/ ~/.local/share/iwd2r/game/`. When the copy is missing, `play.py` stops and says how to make it. Your own install (for example one with IWD2EE) is never the default: point a run at it with `--data PATH` or `IWD2_DATA`, which take precedence.

#### Desktop runs

A run that would open a window on your desktop (`--windowed`, `--fullscreen`, or a Linux run with a display) refuses unless `IWD2R_DESKTOP_OK=1` is set. Automated runs use `--headless`, `--vm linux` or `--vm windows` instead, so an agent's run never steals focus from you. To play yourself:

```
IWD2R_DESKTOP_OK=1 python3 tools/play.py --windowed --data "/mnt/c/Games/Icewind Dale II" --sound
```

#### Real-input runs (`--vm`)

`--vm` runs the same scenario files with genuine operating-system input in an isolated display, so nothing appears on your desktop: mouse and keyboard events travel OS → SDL → window shim → engine, as a player's do, and the in-engine script (`IWD2R_SCRIPT`) stays off.

```
python3 tools/play.py --vm linux --script scenarios/options.txt
python3 tools/play.py --vm linux --suite --jobs 3
python3 tools/play.py --vm all --suite
```

- `--vm linux` builds `linux-gcc`, starts `Xvfb :N` (N from 90 up) at the output size (the requested resolution; its height from `--set "Program Options/Height=H"`, else 4:3 for 800, 1024, 1600 and 2048, else 16:9; `--resolution desktop` is 1920x1080), runs the game windowed on it with the X11 video driver and drives it with xdotool (`tools/realinput.py`). One-time setup: `sudo apt-get install -y xvfb xdotool imagemagick`.
- `--vm windows` runs each scenario in a fresh Windows Sandbox (`tools/winsandbox.py`, `tools/sandbox/driver.ps1`): the worktree's MSVC build (with the MSVC runtime DLLs copied next to `iwd2r.exe`), a copy of the game data in `C:\Users\thier\iwd2r-vm\game`: by default the vanilla copy `C:\Users\thier\iwd2r-oracle\game`, never the install. `C:\Users\thier\iwd2r-vm\game.source.json` records the source path and the SHA-256 of its `vanilla.ok`; a cache from another source (an older cache copied from the install, a rebuilt vanilla copy, or `--data` pointing elsewhere) is deleted and copied again from scratch, a changed `CHITIN.KEY` of the same source is resynced with rsync, and the check runs before every scenario under the sandbox lock, the copy also holding the vanilla copy's `vanilla.lock`; the source folder is never mapped, networking off. The driver uses `SendInput` and captures the screen with System.Drawing. One-time setup, in an admin PowerShell, then reboot: `Enable-WindowsOptionalFeature -Online -FeatureName Containers-DisposableClientVM -All`. Only one sandbox runs at a time, so `--jobs` does not apply, and each scenario holds `C:\Users\thier\iwd2r-vm\sandbox.lock`, so a run from another worktree waits.
- `--vm all --suite` runs the suite on Linux, then on Windows when the sandbox is available, and prints both tables.
- `--suite` runs every `scenarios/*.txt`; `--jobs N` (default 1) runs that many Linux scenarios at once, each on its own display and CPU core and in a private view of the install (links to its files, with its own `cache`, `temp`, `tempsave` and INI copies, since parallel games would otherwise race on them); `--timeout S` (default 180) bounds each scenario.

How commands map to input: `click`/`rclick`/`move` move the real pointer to the game coordinates (UI coordinates plus the centring offset, times the scale: the window's client area is the game screen scaled up) and press the button for 2 frames; `key` presses a real key; `type` types real key strokes; `wait N` lasts N/30 s and until the engine has presented N frames, so slow screen loads do not desynchronise the script; `shot NAME` captures the display (`NAME-screen.png`, the OS view with the engine-drawn pointer) and the engine's framebuffer (`NAME.png`), with `--shot-presented` also the presenter's output (`NAME-presented.png`: the window-sized frame after scaling, region composition and the filter; at `Scale=1` with the nearest filter it is byte-identical to `NAME.png` under either presenter); `quit` closes the window as a window manager's close button does (`WM_DELETE_WINDOW` on X11, `WM_CLOSE` on Windows) and confirms the engine's "quit?" message box with Return. The framebuffer shots and frame counts come from a small control channel: with `IWD2R_CONTROL_DIR` set, the engine answers `shot-NAME.req` and `frames.req` files in that folder once per presented frame; it injects no input.

What lands in `runs/<YYYYMMDD-HHMMSS>/` (one subfolder per scenario with `--suite`, under `linux/` and `windows/` with `--vm all`): `driver.log` (every action with timestamps), `log.txt` (the game's output), `script.txt`, `actions.json` (the parsed scenario), both kinds of screenshots and `summary.txt`; Linux runs add `xvfb.log` and `ini-backup/`, Windows runs `run.wsb`. A suite writes `suite.txt` with the table (scenario, exit code, duration, screenshot count). The exit code is the game's, 124 on a timeout, or non-zero when the driver failed; a scenario that exits 0 but did not write every `shot NAME` as `NAME.png` (and `NAME-presented.png` with `--shot-presented` on Linux) fails with code 1 and "missing shot NAME"; Xvfb, the game and the sandbox are closed on every exit path, including Ctrl-C.

#### Pending scenarios

`scenarios/pending/` holds scenarios our engine cannot finish yet, such as a milestone's exit scenario (`world_entry.txt`: character generation through Party Formation into the start area). They are recorded in the original (`python3 tools/oracle.py replay record scenarios/pending/world_entry.txt`) and run by path, but `--suite` takes only `scenarios/*.txt`, and the integration gate compares only those. A pending scenario moves to `scenarios/` when it passes.

#### Parallel work streams

`tools/worktree.sh new <stream>` creates the worktree `/home/thier/iwd2-re-worktrees/p3-<stream>` on branch `p3/<stream>` from `first` and prints its path; `tools/worktree.sh done <stream>` removes it and deletes the branch once the branch is merged into `first`, and refuses otherwise. Each worktree builds on Windows in its own folder (`C:\Users\thier\iwd2r-build-p3-<stream>`). Shared resources are locked (`tools/filelock.py`): the Ghidra project (one headless run at a time), the vanilla copy's replays and its build (`C:\Users\thier\iwd2r-oracle\replay.lock`, `vanilla.lock`) and Windows Sandbox; a command that has to wait says so after 5 seconds.

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

Screen size, in the `[Program Options]` section of `icewind2.ini`:

- `Resolution` is a width in pixels, such as the 1366 IWD2EE sets, or `desktop` for the primary display's native size.
- `Height` (optional, an iwd2r key) is the height. Without it, 800, 1024, 1600 and 2048 take their original 4:3 heights (600, 768, 1200, 1536) and any other width takes the desktop's aspect (1366 on a 1920x1080 desktop is 1366x768).
- `Scale` (optional, an iwd2r key, default 1) is 1 to 4 or `auto`: the engine runs at the requested size divided by the scale, and every frame is scaled up with nearest-neighbour filtering, windowed and fullscreen; the mouse is mapped back. `auto` picks the largest factor that keeps the engine's height at least 720 (1920x1080 runs at 1; 2560x1440 at 2, inside 1280x720; 3840x2160 at 3, inside 1280x720). With `World Scale` it is the UI's scale only.
- `World Scale` (optional, an iwd2r key, default the value of `Scale`) is 1 to 4 or `auto` (1 when `Scale` is 2 or more, else `Scale`); a value above `Scale` is taken as `Scale`, so the UI is never smaller than the world. The engine runs at the requested size divided by `World Scale`, so the world is drawn at that scale, while the UI panels keep their pixel size and are shown at `Scale`. On the world screen the bottom panels get a band `max(Scale, World Scale) / World Scale` times their height and the world viewport ends above it; every other screen (start, character generation, inventory, options and the rest) shows its 800x600 UI centred. The UI scale is lowered where the UI would not fit, never below `World Scale`, with a line on stderr: on the world screen to the largest scale at which the 800-pixel-wide panels fit the width and the 284-row dialog band the height (1920x1080 at `Scale=3` shows them at 2), on the other screens to the largest at which 800x600 fits (1920x1080 at `Scale=2` shows them at 1, 2560x1440 at 2). The mouse is mapped per region, so clicks land where the panels are shown; in fullscreen the regions are letterboxed like the whole frame. `Resolution=2560`, `Scale=2`, `World Scale=1` gives the world at 2560x1440 with the UI at twice its size.
- `Presenter` (optional, an iwd2r key, default `classic`) is what shows the frames: `classic`, SDL's 2D renderer with one texture scaled nearest-neighbour; `gpu`, the SDL3 GPU presenter on Vulkan, which draws the world and the UI regions at their own scales and applies `Scale Filter`; or `auto`, which behaves as `gpu` (kept for INIs that name it). Before the game creates a GPU device in its own process it runs itself as `iwd2r --probe-gpu` in a child process (start the GPU presenter offscreen at 1x1: a Vulkan device, every filter's pipeline and one frame drawn; print `probe-gpu: ok <driver> <device>` or `probe-gpu: unavailable <reason>`, exit 0 or 1; a crash in the child prints `probe-gpu crash: ...` on stderr), because a broken Vulkan stack (Windows Sandbox's) can crash the process that loads it even when the device creation fails; only a clean `ok` lets the game create its device. A probe that fails, crashes or runs past 10 s, or a device that cannot be created, falls back to `classic` with one line on stderr (`presenter: gpu unavailable (...), using classic`; `(probe: ...)` for the probe), and so does a device lost again after its one rebuild per run. After such a loss the game stays `classic` for the rest of the run: a new window (Alt+Enter) or the hotkey does not start `gpu` again. The probe runs once per session; the Ctrl+Shift+R hotkey reuses its verdict. `IWD2R_GPU_PROBE=skip` creates the device without the probe and `IWD2R_GPU_PROBE=fail` forces the fallback (for tests). At `Scale=1` with `nearest` the two presenters show the same pixels. Windows Sandbox has no Vulkan, so `gpu` and `auto` show `classic` there.
- Ctrl+Shift+R switches the presenter between `classic` and `gpu` while the game runs and prints the one now active on stderr (`presenter: gpu`); the engine does not see the key. When `gpu` cannot start it stays on `classic`; switching goes through the same child-process probe (its verdict kept for the session), so a Vulkan driver that crashes when probed cannot take the game down; the first switch to `gpu` in a session runs the probe, which takes about 150 ms (up to its 10 s limit on a broken driver).
- `Scale Filter` (optional, an iwd2r key, default `nearest`) is how the GPU presenter (`Presenter=gpu`) scales the frame: `nearest`; `sharp` (nearest at texel centres, linear within one output pixel of each texel edge); `smooth` (bilinear); `fsr`, AMD FidelityFX Super Resolution 1.0 (EASU edge-adaptive upscaling, then RCAS sharpening); or `nis`, NVIDIA Image Scaling (a directional 6-tap scaler with adaptive sharpening). Both upscalers are spatial and run on any Vulkan GPU; NVIDIA supports NIS from 1x to 2x, so its quality at `Scale` 3 or 4 is outside that range and untested; their licences are in `docs/THIRD_PARTY.md`. A region shown at one frame pixel per window pixel (the world at `World Scale=1`, or everything at `Scale=1`) is drawn with `nearest` under `fsr` and `nis` too, so the native-resolution picture is never sharpened. The classic presenter always uses nearest; it ignores `Scale Filter` and `Sharpness` (a line on stderr says so when the INI sets them under `Presenter=classic`).
- `Sharpness` (optional, an iwd2r key, default 0.5) is 0 to 1, how much `fsr` and `nis` sharpen: 0 is none, 1 the most. For `fsr` it is RCAS's linear amount (RCAS counts stops below its maximum: 1 is 0 stops, 0.5 one stop, 0 off); for `nis` it is the SDK's own 0 to 1 sharpness. Any other value uses 0.5 with a line on stderr.
- The engine's screen must be at least 800x600; anything smaller is refused at start-up with a message.

At 800x600, 1024x768, 1600x1200 and 2048x1536 the layout is the original's (1600 and 2048 draw the UI double size). At any other size the 800x600 UI is centred, the stone borders are tiled over the margins, the world view spans the screen above the bottom panels and the world screen's panels sit bottom-centre. Override any key for one run with `IWD2R_INI_OVERRIDE="Program Options/Resolution=800"` (entries are `Section/Key=Value`, separated by `;`).

The game window is the engine's screen times `World Scale` (`Scale` unless set); with `Full Screen=1`, or when that is as wide as the desktop (the original's rule), it is letterboxed onto the desktop. Like the original, iwd2r pins itself to one CPU core at start-up, so expect one busy core.

Headless smoke run, which prints a framebuffer hash:

```
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy IWD2R_SMOKE_FRAMES=300 ./build/linux-gcc/iwd2r
```

## Verification

GitHub Actions is disabled while the parallel streams push often (`gh workflow disable CI`; `gh workflow enable CI` turns it back on). `tools/gates.sh` runs the same steps locally, with `--windows` for the Windows builds, and every integration into `first` runs it.

The oracle checks our engine against the original game's own code. The original `iwd2.exe` runs for real on Windows; ours runs next to it; the results are compared. A function counts as done only when it compiles, matches the original, and has an evidence file. The design is in [docs/superpowers/specs/2026-10-12-iwd2r-verification-harness-design.md](docs/superpowers/specs/2026-10-12-iwd2r-verification-harness-design.md) and the reasons for its runtime in [ADR 0003](docs/adr/0003-oracle-runtime.md). Parked items are in [docs/followups.md](docs/followups.md).

`tools/ghidra/setup.sh`, `oracle.py evidence` and `check_evidence.py` work on Linux and WSL. The runner, the vanilla copy and the replays need a Windows host with your own copy of the game, driven from WSL. Nothing from the game is committed. Results go to `runs/<YYYYMMDD-HHMMSS>/`, which is not committed either.

- **Ghidra.** `tools/ghidra/setup.sh` installs pinned Ghidra, Node.js and REA under `~/.local/share/iwd2r/`, with no sudo. `tools/ghidra/make_project.py` builds the labelled project: our `// 0xADDR` names first, then IEex's.
- **Vanilla copy.** `python3 tools/oracle.py vanilla` copies your install to `C:\Users\thier\iwd2r-oracle\game`, runs IWD2EE's uninstaller there, and checks that `iwd2.exe` has the vanilla MD5. Your install is never changed. `--rebuild` starts over.
- **Function oracle.** `python3 tools/oracle.py build` builds the 32-bit runner. `python3 tools/oracle.py run` maps the vanilla exe into it and calls the original's functions and ours with the same inputs. `--file NAME` runs one case file from `oracle/cases/`, `--seed` and `--iterations` set the sweep, and `--selftest` checks the loader. It writes `runs/<stamp>/oracle.json` and exits 1 on any mismatch or crash.
- **Replays.** `python3 tools/oracle.py replay record scenarios/chargen_replay.txt` plays a scenario in the original with a recorder DLL and writes state dumps. `replay compare` plays it in both engines with the same seed and compares every `checkpoint` line field by field. Our engine runs a deterministic replay mode when `IWD2R_STATE_DUMP` names a folder. Screens compared are listed in `oracle/replay/screens.json` (`connection`, `create_char`); `oracle/replay/known_differences.json` lists accepted differences. Both commands run the original inside Windows Sandbox, one fresh sandbox per recording (about 20 s more per run), so the game never takes the focus on your desktop and a stray click cannot land in a recording; `tools/sandbox/replay-driver.ps1` runs the injector there and the results come back to the same `runs/<stamp>/` folders. The original needs DirectPlay, which the sandbox ships only as stub files: the driver copies the host's 32-bit DirectPlay files (read from `C:\Windows\SysWOW64` through a read-only mapping) over the sandbox's stubs at logon, inside the sandbox only, so DirectPlay must be enabled on the host as it is for the original game. `--host` runs the original as a window on your desktop instead, for debugging the recorder. Antivirus software flags the injector (it injects a DLL into another process); with replays in the sandbox, only the function oracle's runner still runs on the host.
- **Evidence.** `python3 tools/oracle.py evidence 0x5845D0` drafts `docs/evidence/5845D0.md` from the Ghidra project. `python3 tools/check_evidence.py --base origin/main` fails when a changed function has no evidence file. See [docs/evidence/README.md](docs/evidence/README.md).

The "done" rule: every function you change under `src/` that carries a `// 0xADDR` comment needs an evidence file with an oracle case or a written reason for none. CI enforces it.

CI runs the parts that need no game data: the PE loader tests (`[pe]`), the VC6 `rand` tests (`[rand]`), the oracle generator tests (`[oracle]`), the state dump and replay tests (`[dump]`), `tests/test_tools.py` and the evidence check. The runner itself needs the vanilla exe, so it runs only on your machine.

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
- Arbitrary resolutions beyond the original four (800, 1024, 1600, 2048), `Resolution=desktop`, the `Height` key and the `Scale` key are an IWD2EE-style extension; the layout at other sizes follows IEex's generalisation of the original table (src/ScreenLayout.cpp). On exit the engine writes `Resolution` back as the INI had it rather than the engine's width.
- Positioned 3D sounds play centred without distance attenuation when the 3D-sound option is on, because the DirectSound 3D interfaces are no-ops.
- The assert-time thread suspension is cooperative only: a thread can suspend itself but cannot freeze others.
- The speaker-portrait click in world dialog panels, which forwarded to the multiplayer screen, is now a no-op.
- The start screen is the original's connection screen with its multiplayer entry hidden.
- `C2DArray::LoadText` has no counterpart in the original. The original parses 2DA text only inside `C2DArray::Load` (0x402B70), after fetching it through its resource manager; ours splits that parse into `LoadText`, unchanged, so the tests and the oracle's table cases can build tables from text without a resource manager. `Load` calls it, so the engine's behaviour is the same.
- The drawing entry points of the `CVid*` classes (`CVidCell::Render`, `CVidTile::Render`, `CVidFont::TextOut`, `CVidMode`'s line, rectangle, ellipse, recticle, outline, polyline, erase and fade functions, `CVidInf`'s `BK*` and `FX*` functions and `RenderFlash`, `CVidMosaic::Render`, `CVidBitmap::RenderDirect`, and the viewport fill in `CInfinity::RenderMessageScreen` in `src/CInfinity.cpp`; the list and the helpers left alone are in `src/render/api.h`) start with one line the original does not have: a guard that hands the call to an installed render backend and returns when the backend handled it (the two inlined fade functions instead end with a line that reports the fade level). It lets later renderers draw the same calls another way (larger textures, a GPU) without changing the decompiled bodies. With no backend installed, the default, every guard falls through and the bodies draw into the back buffer exactly as before. Each guarded function's evidence file carries the exemption "render API dispatch guard".
- The drawing code that project 4 ported from stubs or checked against the original (the 32-bpp paths, palettes, polygon fills) follows the original's machine code where the decompiled source we started from had it wrong, so these functions differ from upstream iwd2-re, not from the original: `CVidPoly::FillConvexPoly`'s edge walk and last clip row ([docs/evidence/7C0F40.md](docs/evidence/7C0F40.md)), `CVidPalette::GetTint` returning 0xFFFF for the combined global and affect tint ([7BF430.md](docs/evidence/7BF430.md)), `CVidMode::ApplyBrightnessContrast`'s byte complement and int gamma scale ([795E00.md](docs/evidence/795E00.md)) and `CVidPalette::RealizeResource`'s grey, add and tint stages ([7C04B0.md](docs/evidence/7C04B0.md)). Each was found by an oracle case that compares the original's function with ours.

## License

[LICENSE.md](LICENSE.md): the Sustainable Use License 1.0 applies to the whole repository, including new code, except the MIT-licensed ports listed in [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md), which keep their notices. The original game, Icewind Dale II, is the property of its rights holders and is not included.

## References

- [alexbatalov/iwd2-re](https://github.com/alexbatalov/iwd2-re), the base of this project.
- IESDP, for file formats.
- GemRB, as a behavioral reference only (GPL; nothing is copied).
- benilla, as a pattern reference for the data environment variable, no bundled assets and generated-map conventions.
- IWD2EE, the mod this engine ships; its code is part of iwd2r.
