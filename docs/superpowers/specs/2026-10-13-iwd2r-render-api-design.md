# iwd2r Project 4: Render API and Classic Renderer

Date: 2026-10-13
Status: design approved in conversation; this document is the written spec for review.
Scope: project 4 of the iwd2r program. It runs beside project 3 (finish the simulation) in its own worktrees, with the file boundary in section 8.

## 1. Purpose

Give iwd2r a modern presentation path and the seam that later renderers plug into, without changing a pixel of what the classic renderer draws at the default settings, except the fixes toward the original that the README lists under its deviations (`CVidPoly::FillConvexPoly`, `CVidPalette::GetTint`, `CVidMode::ApplyBrightnessContrast` and `CVidPalette::RealizeResource`, each found by an oracle case against the original's machine code). Day one delivers three things: an SDL3 GPU presenter with scaling filters and a fallback to the current path; true UI scaling, where the world renders at native resolution while the UI panels are scaled up, which is the problem high resolutions such as 2560×1440 have today; and the render API, the interface every later renderer (HD classic, HD lighting) implements, with its dispatch points in place and tested.

## 2. Decisions already made

- **Day one is "same pixels, new pipeline".** The existing software renderer, the decompiled `CVid*` code drawing into a CPU 32-bit back buffer, stays the classic path. Nothing in it is rewritten.
- **GPU API: SDL3 GPU.** SDL3 (3.4.18, static) is already the platform layer and its GPU API is available. Vulkan is the first backend on Linux and Windows; D3D12 and Metal shader blobs are a follow-up.
- **Runtime toggle.** The presenter is chosen by an INI key and can be switched at runtime with a hotkey; the choice is invisible in the pixels at scale 1 with the nearest filter.
- **Faithfulness.** Changes to decompiled code are limited to one guard line at the top of each drawing entry point, documented in the README's deviations list like the replay hook. Everything else is our code under `platform/` and `src/render/`.

## 3. Starting point, measured 2026-10-13

- The engine draws into `CVidInf`'s back surface, a shim `Surface` whose pixels are a `std::vector<uint32_t>` in XRGB8888; 16 and 24 bpp never run (`GetCurrentBitsPerPixels` is stubbed to 32). Assets are 8-bit paletted and are converted while blitting.
- Each screen draws and calls `CVidMode::Flip`; fullscreen goes through `Surface::Flip` to `present::Present`, windowed through `MarkPrimaryDirty` and `present::PresentIfDirty` from the window pump. Every frame reaches `present::Present(xrgb, w, h, pitch)` in `platform/present.cpp`.
- `present.cpp` uses an `SDL_Renderer` with one XRGB8888 streaming texture, `SDL_SetRenderLogicalPresentation` (stretch windowed, letterbox fullscreen) and NEAREST scaling when `Scale > 1`. The engine renders at the internal resolution; the window is internal × Scale; the upscale happens only here.
- `src/ScreenLayout.cpp` resolves `Resolution`, `Height` and `Scale` (1–4 or auto) and computes the panel and viewport rectangles for any resolution.
- Mouse mapping: `GetCursorPos` goes through `present::WindowToFrame`, which uses the renderer's logical presentation, with `FrameToWindow` and `FrameSize` as the inverse and bounds. Scripts and replays inject cursor positions already in engine coordinates.
- Screenshots (`WriteShot`) and the smoke test's frame hash read the CPU primary buffer, not the presented image.
- Drawing entry points the engine calls: `CVidCell::Render`, `CVidTile::Render`, `CVidFont::TextOut`/`TextOutEx`, `CVidMode::DrawLine`/`DrawRect`/`DrawEllipse`/`DrawRecticle`/`OutlinePoly`/`PolyLine`/`EraseScreen` and the fades, `CVidPoly::FillPoly`, `CVidInf::BKRender`/`BKRenderEllipse`/`BKRenderLine`/`BKRenderConvexPoly`/`BKTextOut`, the `FX*` family, `RenderFlash`, `RenderPointer`, `CVidMosaic`, `CVidBitmap`, `CInfinity::RenderLightning`. Direct pixel writes outside `CVid*` happen only in `CBaldurProjector` (Bink movies, inert today).
- Stubs that matter at 32 bpp: `CVidCell::sub_7D0950` and `sub_7D1190`, `CVidMode::PolyLine`, `DrawLightingEffect32`, `DrawThickLine32`, `CVidPoly::DrawHLineShaded32` and its mirrored variant, `CVidBlitter::SoftBlt` and `SoftSrcKeyBlt`, `CVidPalette::RealizeRange`, `CInfinity::FXRenderClippingPolys`, `RenderMessageScreen`, `Scroll`. These were project 3's M5 rendering items and are now project 4's.
- CI runs every job with `SDL_VIDEO_DRIVER=dummy`; the window code treats dummy as headless. There is no shader toolchain on the machine or in the repo.

## 4. Architecture

### 4.1 Presenter

`platform/present` becomes a front with two backends behind the existing API (`Present`, `PresentIfDirty`, `SetPresentTarget`, `SetFullscreen`, `SetDisplayMode`, `WindowToFrame`, `FrameToWindow`, `FrameSize`, `PrimaryFrame`, `HashPrimary`, `PresentedFrames`):

- **classic:** the current SDL_Renderer path, unchanged.
- **gpu:** `SDL_CreateGPUDevice` on the window, a swapchain, one sampled texture for the back buffer (uploaded each presented frame through a transfer buffer), a fullscreen-quad pipeline with one of five fragment shaders: nearest, sharp-bilinear (nearest at the pixel centres, linear within one output pixel of each edge, the crisp-pixels filter), smooth (bilinear), and two spatial upscalers that sharpen the classic frame without motion vectors or depth: AMD FidelityFX Super Resolution 1.0 (EASU plus RCAS, MIT) and NVIDIA Image Scaling (MIT). Both run on any GPU. Temporal upscalers such as DLSS do not fit a 2D sprite engine, which has no motion vectors, depth or jitter, and are out of scope.

Both backends take the same CPU buffer and share the viewport math, so `PrimaryFrame`, `HashPrimary`, screenshots and hit-testing are unchanged. `PresentedFrames` counts in both. The gpu backend releases its device in `SetPresentTarget(nullptr)` before the window is destroyed.

### 4.2 Region composition: the UI-scaling layer

`ScreenLayout` gains a second scale. `Scale` keeps its meaning and becomes the UI scale `S_ui`; a new `World Scale` (`S_world`, 1–4 or `auto`) defaults to `S_ui`, so the layout and the presented image are identical to today unless the keys differ.

When `S_ui ≠ S_world`:

- The engine's internal size is the window size divided by `S_world`. Each UI panel keeps its 1× pixel rectangle in engine coordinates, but the layout reserves `rect × S_ui / S_world` for it, so the world viewport is the remaining area and never overlaps a scaled panel. Full-UI screens (inventory, character, the start screens) are one UI region centred in the window.
- The presenter composites regions of the one CPU back buffer: each UI panel's 1× rectangle is drawn into its reserved window rectangle at `S_ui`; the world viewport is drawn at `S_world`. The reserved area the engine never draws into is never shown.
- Mouse mapping is piecewise: a window point inside a scaled panel rectangle maps to `panel origin + (point - window rect origin) / S_ui`, which lands inside the panel's 1× rectangle where the UI manager's hit tests expect it; any other point maps through `S_world`. `FrameToWindow` is the inverse per region. Scripts and replays keep injecting engine coordinates.
- `ScreenLayout` produces the region list; `present` consumes it; both are our code.

This gives, at 2560×1440 with `Scale=2` and `World Scale=1`, the world at native resolution with the UI at twice its pixel size, with nothing in decompiled UI code changed. Floating text and the cursor drawn in the world region stay at `S_world`, as in IEex.

### 4.3 Render API

`src/render/api.h` defines `render::Backend`, the interface later renderers implement, following the HD spike's contract (textures at N×, every coordinate in original logical units, scale per texture):

- Frame: `BeginFrame()`, `EndFrame()`.
- State: `SetLayer(Layer world|ui)`, `SetClip(rect)`.
- Primitives, each carrying the resource identity and frame rather than pixels: `DrawCell(const CVidCell&, frame, pos, palette, flags)`, `DrawTile(const CVidTile&, pos, flags)`, `DrawText(const CVidFont&, string, pos, colour, flags)`, `DrawLine`, `DrawRect`, `DrawEllipse`, `DrawPoly(points, fill, colour)`, `Fade(amount)`, `Flash(rect, colour)`.
- `render::Mode()` returns `classic` or the active backend; `render::Set(Backend*)` installs one.

Day one has two backends: **classic**, which is the absence of a backend (every entry point falls through to the decompiled body), and **recording**, which stores the calls for tests. Each `CVid*` drawing entry point listed in section 3 gets one guard line at its top: `if (render::Dispatch(...)) return;` with the arguments the body uses. The first GPU drawing backend is the HD classic project's work; project 4 leaves the API proven by the recording backend and the guards.

### 4.4 Settings and toggle

INI keys under `[Program Options]`: `Presenter=auto|gpu|classic` (auto: gpu when a device can be created, else classic), `Scale Filter=nearest|sharp|smooth|fsr|nis` (gpu only; classic is nearest; `fsr` and `nis` add a `Sharpness=0..1` key, default 0.5), `World Scale=1..4|auto` (default: the value of `Scale`; `auto` means 1 when `Scale` resolves to 2 or more, otherwise the value of `Scale`). All three are also `IWD2R_INI_OVERRIDE` keys for the harness. A hotkey, Ctrl+Shift+R, flips the presenter at runtime and prints the new choice on stderr; the options screen gains no new controls in this project.

## 5. Components

| Path | Responsibility |
|---|---|
| `platform/present.h`, `present.cpp` | the front: backend selection, region composition, viewport math, the public API as today |
| `platform/present_classic.cpp` | the SDL_Renderer backend, moved from `present.cpp` unchanged |
| `platform/present_gpu.cpp` | the SDL3 GPU backend |
| `platform/shaders/*.vert`, `*.frag` (GLSL 450), `platform/shaders/*.spv` | the quad vertex shader, the five filter fragment shaders (FSR 1.0 and NIS ported from their MIT sources), their compiled SPIR-V |
| `tools/shaders.py` | regenerates the `.spv` files with `glslangValidator` and checks they match the sources (`--check`) |
| `src/ScreenLayout.h`, `.cpp` | the second scale, the region list, the reserved rectangles |
| `src/render/api.h`, `render.cpp` | the interface, `Mode`, `Set`, `Dispatch` |
| `src/render/recording.h`, `.cpp` | the recording backend for tests |
| `src/CVid*.cpp`, `src/CInfinity.cpp` | one guard line per drawing entry point; the 32-bpp stubs from section 3 ported from the original |
| `platform/window.cpp` | the hotkey; mouse mapping through the region-aware functions |
| `tests/test_present.cpp`, `test_layout.cpp`, `test_render_api.cpp` | see section 7 |

## 6. Failure handling

- No GPU device (CI's dummy driver, a machine without Vulkan): the front logs one stderr line and uses the classic backend; `Presenter=gpu` on such a machine does the same, with the line saying why.
- Device lost during a frame: the gpu backend rebuilds its device and resources once; a second loss falls back to classic for the rest of the run.
- A `.spv` that does not match its GLSL source: `tools/shaders.py --check` fails in CI; at runtime a shader that fails to load falls back to classic with a message.
- A region list that does not tile the window (a layout bug): the layout asserts in debug builds and falls back to `S_world = S_ui` in release, logging it.

## 7. Verification

- **Pixel identity.** With `Scale=1`, `World Scale=1` and `Scale Filter=nearest`, the gpu-presented frame read back from the swapchain (`SDL_DownloadFromGPUTexture`) is byte-identical to `PrimaryFrame` on the start screen and at every checkpoint of `chargen_replay`. `WriteShot` gains a presented-frame mode (`IWD2R_SHOT_PRESENTED=1`) that writes the readback beside the CPU shot. The harness runs this on Linux and Windows; CI skips it (no device).
- **Composition math.** Unit tests on `ScreenLayout`: the region list tiles the window with no overlap for the original four resolutions and for 1920×1080, 2560×1440 and 3840×2160 at every `S_ui`/`S_world` pair in 1–4; `WindowToFrame` then `FrameToWindow` round-trips every region's corners and centre; a point in a scaled panel maps inside the panel's 1× rectangle.
- **Dispatch points.** With the recording backend installed, drawing the start screen records a `DrawCell` for each panel cell and a `DrawText` for each label, with positions matching the layout; every entry point in section 3 appears at least once across the start screen, chargen and the options screen. (As built: the start-screen recording covers the panel cells and labels, and the synthetic `every_entry_point_dispatches` test drives every entry point through the recording backend; together they satisfy this check: the synthetic test proves every entry point's dispatch, whichever screen reaches it, and the recording proves the guards fire from real screen drawing.)
- **Unchanged classic path.** Apart from the fixes toward the original named in section 1, the smoke test's frame hash and the existing sandbox and `--vm linux` suites pass unchanged with the default settings; `replay compare chargen_replay` still matches 20 of 20.
- **New settings.** The sandbox suite runs once more with `Presenter=gpu` and once with `Scale=2`, `World Scale=1` at 1920×1200 or 2560×1440; the screenshots show the world at 1× and the UI at 2×, and the scenarios' clicks still land. (Amended: the first version named 1920×1080, where this cannot pass by construction. The full-UI screens are the 800×600 UI, which needs 1200 rows at 2×, so at 1080 rows `EffectiveUiScales` shows them at 1×; only the world screen's panels, 284 rows deep at most, fit at 2× there.)
- **Ported stubs.** Each of the section 3 stubs gets its evidence file and, where its inputs are plain values, an oracle case; the rest are covered by the replay comparisons once project 3 reaches the screens that use them.

## 8. Boundary with project 3 and execution

Project 4 owns `platform/present*`, `platform/shaders/`, `src/ScreenLayout*`, `src/render/`, `src/CVid*`, `src/CInfinity.cpp`'s render functions, `src/CVisibilityMap.cpp`'s blits, the mouse-mapping functions in `platform/window.cpp`, and `tools/shaders.py`. Project 3 owns the rest of `src/`; its M5 rendering stubs are now section 3's list here. A change needed across the boundary goes through the coordinator.

Execution follows the project 3 model: streams in their own worktrees (`~/iwd2-re-worktrees/p4-<stream>`), the wave 0 locks for shared resources, one review per stream batch and a whole-project review at the end. The streams are presenter (4.1 and the shaders), layout and composition (4.2), render API and guards (4.3), and the stub ports; the first three are independent in files and run in parallel.

## 9. Risks

- **Sharp-bilinear at non-integer scales** can look uneven on 1 px UI lines. The filter choice is per user, and nearest stays the default when `S_ui` is an integer.
- **Region composition and the cursor.** The engine draws the software cursor into the back buffer in fullscreen; inside a UI region it is scaled at `S_ui`, elsewhere at `S_world`, so its size jumps at region edges. Acceptable for day one; a presenter-drawn cursor is a follow-up.
- **Shader toolchain.** `glslangValidator` is installed by the user once (`apt install glslang-tools`); the `.spv` blobs are committed so a build never needs it. D3D12 on Windows is not available until DXIL blobs exist; SDL_GPU uses Vulkan there, which the host's NVIDIA and AMD drivers provide.
- **Guard lines in decompiled code.** Thirty-odd one-line edits in `CVid*` are a deviation; they are the same shape as the replay hook and listed in the README. `check_evidence.py` will require an evidence file for each touched function; the file's exemption records the guard.
- **Overlap with project 3 at `CInfinity.cpp`.** Project 3's M2 touches `CInfinity::Scroll` and `AIUpdate`; project 4 touches its render functions. Both are in the same file; the coordinator merges and the two streams rebase.

## 10. Out of scope

GPU drawing of the primitives (the HD classic project), HD assets, lighting, 3D, the options-screen UI for the new keys, D3D12 and Metal shader blobs, movies, a presenter-drawn cursor.
