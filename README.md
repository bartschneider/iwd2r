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

Current numbers: four presets build; 175 tests pass on Linux; the start screen is reached headlessly on Linux and Windows.

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

Point the engine at your install with `IWD2_DATA`:

```
IWD2_DATA=/path/to/Icewind\ Dale\ II ./build/linux-gcc/iwd2r
```

Alternatively, place a link named `IWD2` next to the executable or at the repository root. The directory must contain `CHITIN.KEY`. The engine writes saves, cache and INI files into it exactly as the original does.

`Resolution` in `icewind2.ini` must be 800 or 1024 for now. The IWD2EE mod sets 1366, so set it back to 800 to run iwd2r.

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
- Fullscreen presents as a borderless window scaled to the desktop with aspect preserved.
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
