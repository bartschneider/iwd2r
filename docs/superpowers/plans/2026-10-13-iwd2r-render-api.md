# iwd2r Project 4: Render API and Classic Renderer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An SDL3 GPU presenter with scaling filters and fallback, true UI scaling through region composition, and the render API with its dispatch points proven, with the classic renderer's pixels unchanged at default settings, except the fixes toward the original listed in the README (`CVidPoly::FillConvexPoly`, `CVidPalette::GetTint`, `CVidMode::ApplyBrightnessContrast` and `CVidPalette::RealizeResource`).

**Architecture:** Task 1 adds the pure composition module and the region hook in `present`, sequentially, because everything else builds on it. Then three streams run in parallel in their own worktrees: presenter (the GPU backend, shaders, settings, hotkey), layout (the second scale, regions, mouse mapping) and render API (the interface, the guards, the recording backend). Task 5 ports the 32-bpp stubs after the guards land, Task 6 integrates.

**Tech Stack:** C++17, SDL3 3.4.18 (GPU API, Vulkan backend), GLSL 450 compiled to SPIR-V by glslang, Catch2, Python 3 unittest, the project-2 harness and oracle.

**Spec:** docs/superpowers/specs/2026-10-13-iwd2r-render-api-design.md

## Global Constraints

- The classic path's pixels do not change at default settings, except the fixes toward the original listed in the README (`CVidPoly::FillConvexPoly`, `CVidPalette::GetTint`, `CVidMode::ApplyBrightnessContrast` and `CVidPalette::RealizeResource`): the smoke test's frame hash, the `--vm linux` and sandbox suites and `replay compare chargen_replay` pass unchanged with default settings at every integration.
- Decompiled code changes are limited to one guard line at the top of each drawing entry point (section 3 of the spec) and the stub ports; each touched function gets `docs/evidence/<ADDR>.md` (`check_evidence.py` enforces it), the guards with an `exemption` naming the render API.
- Never commit game files, dumps, `runs/` or `build/`. Never modify `/mnt/c/Games/Icewind Dale II`. On Windows write only under `C:\Users\thier\iwd2r-build*`, `iwd2r-oracle` and `iwd2r-vm`.
- No per-file header lines. Commit messages end with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- Streams work in `/home/thier/iwd2-re-worktrees/p4-<stream>` on branch `p4/<stream>` from `first` (`tools/worktree.sh new <stream>` once project 3's M1 Task 3 lands; `git worktree add -b p4/<stream> <path> first` before), own disjoint files (section 8 of the spec and the table below), never push, and use the shared-resource locks from M1 wave 0 for Windows builds, replays and the sandbox.
- Gates at every integration: linux-gcc and linux-clang ctest; `tests/test_tools.py`; `check_evidence.py`; `gen_map.py --check`; `gen_fields.py --check`; `tools/shaders.py --check`; the clang-format check; the Windows x64 build; `oracle.py run`; `oracle.py replay compare` for every scenario in `scenarios/`.

## Review Focus

1. The user resizes the window or changes DPI mid-game: regions are recomputed and a click still lands on the control under the cursor. Pinned in Task 3.
2. Vulkan exists but pipeline or shader creation fails (a stale driver): the presenter falls back to classic without crashing, and says why. Pinned in Task 2.
3. Alt-tab or minimise with the GPU presenter: a failed swapchain acquire skips the frame; the game and `PresentedFrames` continue. Pinned in Task 2.
4. `Scale=3` with `World Scale=2`, a non-integer ratio: reserved rectangles are whole pixels and the regions tile the window with no gap or overlap. Pinned in Task 1.
5. A full-UI screen such as the inventory with `World Scale ≠ Scale`: the UI region is centred and scenario clicks land. Pinned in Task 3.

---

## File Structure

| Path | Owner | Responsibility |
|---|---|---|
| `platform/compose.h`, `compose.cpp` | Task 1 | pure region math: region list, window rects, piecewise mapping |
| `platform/present.h`, `present.cpp` | Task 1 then presenter | the front: backend choice, `SetRegions`, region loop, mapping through compose |
| `platform/present_backend.h` | presenter | the backend interface |
| `platform/present_classic.cpp` | presenter | the SDL_Renderer backend (moved) |
| `platform/present_gpu.cpp` | presenter | the SDL3 GPU backend, offscreen path for tests |
| `platform/shaders/quad.vert`, `nearest.frag`, `sharp.frag`, `smooth.frag`, `fsr_easu.frag`, `fsr_rcas.frag`, `nis.frag`, `*.spv` | presenter | shaders and blobs |
| `tools/shaders.py` | presenter | compile and `--check`; downloads glslang into `$IWD2R_HOME/glslang` if absent |
| `platform/window.cpp` (key handler, mapping call sites) | presenter (hotkey), layout (mapping) — coordinated by line ranges | hotkey; mouse mapping |
| `src/ScreenLayout.h`, `.cpp` | layout | `World Scale`, reserved rects, the region list |
| `src/render/api.h`, `render.cpp`, `recording.h`, `recording.cpp` | render API | interface, mode, dispatch, recording backend |
| `src/CVid*.cpp`, `src/CInfinity.cpp` render functions | render API (guards), then Task 5 (stubs) | guards; stub ports |
| `tests/test_compose.cpp`, `test_present.cpp`, `test_layout.cpp`, `test_render_api.cpp` | per task | see tasks |

---

### Task 1: Composition module and the region hook (sequential)

**Files:**
- Create: `platform/compose.h`, `platform/compose.cpp`, `tests/test_compose.cpp`
- Modify: `platform/present.h`, `platform/present.cpp` (`WindowToFrame`/`FrameToWindow` at the mapping functions; add `SetRegions`), `tests/CMakeLists.txt`

**Interfaces:**
- Produces: in `platform::compose`: `struct Region { RECT frame; RECT window; int scale; bool ui; }`; `std::vector<Region> Layout(SIZE frame, SIZE window, int uiScale, int worldScale, const std::vector<RECT>& uiPanelsFrame, const RECT* fullUiFrame)` — `uiPanelsFrame` are the panels' 1× rectangles in frame coordinates, `fullUiFrame` non-null for a full-UI screen (one centred region); with `uiScale == worldScale` the result is one region covering the frame; `bool WindowToFrame(const std::vector<Region>&, float x, float y, float* fx, float* fy)` (piecewise: first region whose `window` contains the point, else the world region; false only when there are no regions); `FrameToWindow` the inverse; `bool Tiles(const std::vector<Region>&, SIZE window)` (no gaps, no overlaps). `present::SetRegions(std::vector<compose::Region>)`; with regions set, `present::WindowToFrame`/`FrameToWindow` use compose; with none, today's behaviour.

- [ ] **Step 1: Tests** (`[compose]`): `one_region_when_scales_equal` (800×600 frame, scale 2 both: one region, window 1600×1200); `reserved_rects_tile_the_window` for frames 800×600, 1024×768, 1920×1080, 2560×1440, 3840×2160 with a bottom toolbar panel (height 100) and a left panel (width 120), every `uiScale`/`worldScale` pair in 1–4 including 3/2 (Review Focus 4): `Tiles` is true and every `window` rect has integer edges; `mapping_round_trips` on every region's corners and centre; `ui_point_maps_inside_panel` (a window point inside the scaled toolbar maps into the toolbar's 1× frame rect); `full_ui_screen_is_centred`.
- [ ] **Step 2:** run (fail). **Step 3:** implement; `present::SetRegions` stores the list and the mapping functions consult it first. **Step 4:** run (pass); `ctest` all green; the smoke hash unchanged.
- [ ] **Step 5: Commit** "Add the region composition module and the present hook".

---

## Wave: three parallel streams

### Task 2: Stream presenter

**Files:** owns `platform/present.h`, `present.cpp`, `present_backend.h`, `present_classic.cpp`, `present_gpu.cpp`, `platform/shaders/`, `tools/shaders.py`, `platform/window.cpp` lines of the key event handler only, `tests/test_present.cpp`, `tests/test_tools.py` (shaders tests), `.github/workflows/ci.yml` (lavapipe), `platform/script.cpp` (`WriteShot` presented mode).

**Interfaces:**
- Consumes: `compose::Region` and `present::SetRegions` from Task 1.
- Produces: `present_backend.h`: `struct Backend { virtual bool Init(SDL_Window*) = 0; virtual void Shutdown() = 0; virtual bool BeginFrame() = 0; virtual void Upload(const uint32_t* xrgb, int w, int h, int pitch) = 0; virtual void DrawRegion(const compose::Region&, Filter) = 0; virtual void EndFrame() = 0; virtual bool Readback(std::vector<uint32_t>* xrgb, int* w, int* h) = 0; }`; `enum class Filter { Nearest, Sharp, Smooth }`; `present::SetBackend(BackendKind auto|gpu|classic)`, `present::ActiveBackend()`, `present::ToggleBackend()`, `present::SetFilter(Filter)`. INI keys read in `iwd2r_main`'s INI pass (`src/main.cpp`, where `Scale` is read): `Program Options/Presenter`, `Scale Filter`. `IWD2R_SHOT_PRESENTED=1` makes `WriteShot` also write `<name>-presented.png` from `Readback`. `tools/shaders.py [--check]`: compiles `platform/shaders/*.{vert,frag}` to `.spv` with `glslangValidator -V`, downloading glslang's pinned release (version in `tools/ghidra/versions.json`-style `tools/shaders_versions.json`) into `$IWD2R_HOME/glslang/` when not on PATH; `--check` recompiles to a temp dir and compares bytes.
- The gpu backend: `SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, debug=false, nullptr)`, `SDL_ClaimWindowForGPUDevice` when a window exists; a sampled texture the frame's size, a transfer buffer, one graphics pipeline per filter with the quad vertex shader; `DrawRegion` sets a viewport/scissor from `window` and draws the quad sampling `frame`; the offscreen path renders to a colour-target texture of the window size when there is no window (tests, headless). The sharp filter: `uv = floor(p) + clamp((fract(p) - 0.5) * scale, -0.5, 0.5) + 0.5` with `scale = outputPixelsPerInputPixel`. Device loss (`SDL_AcquireGPUSwapchainTexture` false or any API returning false during a frame): skip the frame, count it, rebuild once, then fall back.

- [ ] **Step 1: Tests.** `test_tools.py`: `test_shaders_check_detects_stale_blob` (a temp copy with one byte changed in a `.spv` fails `--check`; the real tree passes). `[present]` Catch2: `gpu_offscreen_identity` (device without window; upload a 320×240 synthetic frame with every pixel distinct; `DrawRegion` of the full frame at scale 1 nearest into the offscreen target; `Readback` equals the input byte for byte; skipped with a message when `SDL_CreateGPUDevice` returns null); `gpu_identity_sharp_integer_scale` (scale 2 sharp equals nearest-upscaled input); `fallback_when_no_device` (`SetBackend(gpu)` with device creation forced to fail through a test hook → `ActiveBackend() == classic`, Review Focus 2 with the hook failing at pipeline creation instead); `skipped_frame_on_acquire_failure` (test hook makes acquire fail once; `PresentedFrames` still increments; no crash; Review Focus 3); `toggle_switches_and_reports`.
- [ ] **Step 2:** run (fail). **Step 3:** implement: move the SDL_Renderer code into `present_classic.cpp` behind `Backend`; write the shaders and `shaders.py`; the gpu backend; the INI keys; the hotkey Ctrl+Shift+R in `window.cpp`'s key handler calling `present::ToggleBackend()` and printing `presenter: gpu|classic` to stderr; CI's Linux jobs `apt-get install mesa-vulkan-drivers` so lavapipe runs the offscreen tests. **Step 4:** run (pass) on Linux; on Windows (`python3 tools/play.py --windowed --set "Program Options/Presenter=gpu"` plus `IWD2R_SHOT_PRESENTED=1` through `--set` is not an env; add `--shot-presented` to `play.py` in this stream) the start screen's `-presented.png` equals the CPU shot; the smoke hash and `--vm linux --suite` unchanged with defaults.
- [ ] **Step 5: Commits** per piece: "Split the presenter into a front and the classic backend", "Add the SDL GPU presenter with nearest, sharp and smooth filters", "Compile the presenter shaders and check their blobs", "Toggle the presenter at runtime".

### Task 2b: FSR 1.0 and NIS filters (after Task 2 merges; presenter owner)

**Files:** `platform/shaders/fsr_easu.frag`, `fsr_rcas.frag`, `nis.frag` and their `.spv`; `platform/present_gpu.cpp` (two-pass path for FSR: EASU into an intermediate texture at the output size, then RCAS), `platform/present.cpp` (the `Filter` enum gains `Fsr` and `Nis`; `Sharpness` setting), `src/main.cpp` INI line `Sharpness`, `tests/test_present.cpp`, `tests/test_tools.py` (shader check covers the new blobs), README Run section, `docs/followups.md` (remove the entry).

**Interfaces:** `Filter::Fsr`, `Filter::Nis`; `present::SetSharpness(float)` in 0..1 (default 0.5); the INI keys `Scale Filter=fsr|nis` and `Sharpness`. Sources: AMD FidelityFX FSR 1.0 (`ffx_fsr1.h`, MIT) ported to GLSL 450 as two fragment shaders; NVIDIA Image Scaling SDK (`NIS_Scaler.h`, MIT) ported as one fragment shader with its coefficient texture baked into the shader or uploaded once. Attribution for both goes in `docs/THIRD_PARTY.md` (create it) as their licences require; nothing else about the notice rule changes.

- [ ] **Step 1: Tests** (`[present]`): `fsr_and_nis_pipelines_build` (offscreen device; both filters render a 320×240 frame to 640×480 without error, output is not blank, and differs from nearest by more than 0 and less than a loose bound); `sharpness_changes_output` (RCAS at 0 and 1 differ); `shaders_check_covers_new_blobs` in `test_tools.py`.
- [ ] **Step 2–4:** run (fail), implement, run (pass); `tools/shaders.py --check` passes; the default path and the three existing filters are unchanged (their identity tests still pass).
- [ ] **Step 5: Commit** "Add the FSR 1.0 and NIS presenter filters".

### Task 3: Stream layout

**Files:** owns `src/ScreenLayout.h`, `.cpp`, `tests/test_layout.cpp`, `platform/window.cpp` mapping call sites and `InitScreen` (not the key handler), `src/main.cpp` INI reading of `World Scale` (coordinate the hunk with Task 2's INI lines: Task 3 adds its key below Task 2's; the coordinator merges), `README.md` Run section (the new key).

**Interfaces:**
- Consumes: `compose::Layout`, `present::SetRegions` from Task 1.
- Produces: `ScreenLayout` gains `int uiScale; int worldScale;` and `std::vector<RECT> uiPanelRects` (the panels' 1× rects in frame coordinates for the current screen kind) and `RECT fullUiRect`; `ComputeScreenLayout(int width, int height, int uiScale, int worldScale)` where `width`/`height` are the engine's internal size = window / worldScale and the reserved rects are `panel × uiScale / worldScale`, rounded outward to whole pixels; `ResolveWorldScale(const char* value, int uiScale) -> int` (`auto` per the spec). `window::InitScreen` computes the regions with `compose::Layout` and calls `present::SetRegions` at init, on `WM_SIZE`/`WM_DISPLAYCHANGE`, and whenever the engine switches between a world screen and a full-UI screen (hook: `CBaldurChitin` already sets the layout per screen through `ScreenLayout`; expose `layout::CurrentRegions()` and call `SetRegions` from the same place).

- [ ] **Step 1: Tests** (`[layout]`): `default_world_scale_equals_scale` (regions are one region; `ComputeScreenLayout` with equal scales matches today's output for the four original resolutions, pinned by the existing layout tests); `world_viewport_excludes_reserved_rects` (1920×1080, uiScale 2, worldScale 1: the toolbar's reserved rect is 2× its height and the world viewport's bottom equals its top); `resize_recomputes_regions` (Review Focus 1: after a simulated `WM_SIZE`, `present::WindowToFrame` of the toolbar's window centre lands inside the toolbar's 1× rect); `auto_world_scale`.
- [ ] **Step 2:** run (fail). **Step 3:** implement. **Step 4:** run (pass); on Windows and `--vm linux`: `scenarios/character_editor.txt` and `options.txt` with `--resolution 1920 --set "Program Options/Scale=2" --set "Program Options/World Scale=1"` pass and their shots show the world region at 1× and the UI at 2× (Review Focus 5); the suites with defaults unchanged.
- [ ] **Step 5: Commits** "Reserve UI panels at their own scale in the layout", "Map mouse input through the composition regions".

### Task 4: Stream render API

**Files:** owns `src/render/api.h`, `render.cpp`, `recording.h`, `recording.cpp`, `tests/test_render_api.cpp`, the guard lines in `src/CVidCell.cpp`, `CVidImage.cpp` (CVidTile), `CVidFont.cpp`, `CVidMode.cpp`, `CVidPoly.cpp`, `CVidInf.cpp`, `CVidMosaic.cpp`, `CVidBitmap.cpp`, `CInfinity.cpp` (`RenderLightning`), their evidence files, `README.md` deviations entry, `tests/CMakeLists.txt`.

**Interfaces:**
- Produces: `namespace render { enum class Layer { World, Ui }; struct Backend { virtual void BeginFrame(); virtual void EndFrame(); virtual void SetLayer(Layer); virtual void SetClip(const RECT&); virtual bool DrawCell(const CVidCell&, int frame, POINT pos, const CVidPalette*, DWORD flags, int transVal); virtual bool DrawTile(const CVidTile&, POINT pos, DWORD flags); virtual bool DrawText(const CVidFont&, const char* text, POINT pos, DWORD flags); virtual bool DrawLine(POINT a, POINT b, COLORREF); virtual bool DrawRect(const RECT&, COLORREF, bool fill); virtual bool DrawEllipse(POINT centre, SIZE axes, COLORREF); virtual bool DrawPoly(const POINT* pts, int n, COLORREF, bool fill); virtual bool Fade(int amount); virtual bool Flash(const RECT&, COLORREF); }; Backend* Set(Backend*); Backend* Active(); enum class Mode { Classic, Backend }; Mode Current(); }` — every `Draw*` returns true when it handled the call. Guards: at the top of each entry point in section 3 of the spec, `if (auto* b = render::Active(); b && b->DrawCell(*this, m_nCurrentFrame, {x, y}, m_pPalette, dwFlags, nTransVal)) return TRUE;` (one line, the arguments the body uses; the exact member names from each function's body). `recording::Backend` stores a `std::vector<Call>` with a variant per primitive and returns true for all.
- Evidence: each guarded function gets `docs/evidence/<ADDR>.md` with `exemption: render API dispatch guard` and a Notes line naming the guard; drafted in one `oracle.py evidence ADDR...` call (M1 Task 1's batch mode once merged; one at a time before).

- [ ] **Step 1: Tests** (`[render]`): `classic_when_no_backend` (`Active()` null, `Current()` Classic); `recording_backend_receives_start_screen_draws` (`[data]`, skipped without `IWD2_DATA`: install the recording backend, run the engine to the start screen as `smoke_start_screen.cpp` does, assert at least one `DrawCell` per panel cell and one `DrawText`, positions within the layout's panel rects); `every_entry_point_dispatches` (a synthetic call to each guarded function with a dummy object dispatches exactly once and the body does not run — the body would need a surface; with the recording backend installed the function returns before touching it).
- [ ] **Step 2:** run (fail). **Step 3:** implement the API, the recording backend, the guards and the evidence files; add the README deviation entry. **Step 4:** run (pass); `check_evidence.py --base <stream base>` passes; the smoke hash unchanged (no backend installed by default).
- [ ] **Step 5: Commits** "Add the render API and a recording backend", "Dispatch each drawing entry point through the render API".

---

### Task 5: Port the 32-bpp rendering stubs (after Task 4 merges)

**Files:** owns `src/CVidCell.cpp`, `CVidMode.cpp`, `CVidPoly.cpp`, `CVidBlitter.cpp`, `CVidPalette.cpp`, `CInfinity.cpp` render functions; evidence per address; `oracle/cases/vid.cpp` for pure functions.

**Functions:** `CVidCell::sub_7D0950` and `sub_7D1190`, `CVidMode::PolyLine`, `DrawLightingEffect32`, `DrawThickLine32`, `CVidPoly::DrawHLineShaded32` and the mirrored variant, `CVidBlitter::SoftBlt` and `SoftSrcKeyBlt`, `CVidPalette::RealizeRange`, `CInfinity::FXRenderClippingPolys`, `RenderMessageScreen`, and the `FillConvexPoly` initialisation errors from `docs/followups.md` (left error term `nLeftAdjUp - nLeftDy`; `Dy` from the minimum Y; no fill when `Dy <= 0`), each from the Ghidra decompile (addresses via `query.py`). `CInfinity::Scroll` stays with project 3 M2 (it is logic, not drawing).

- [ ] **Step 1: Tests.** Oracle cases where inputs are plain values (`RealizeRange` over a 256-entry palette and ranges; `DrawHLineShaded32` into a 64×1 buffer compared against the original writing into the same buffer in the runner: build the buffer in leaked memory per ruling R10a); `[vid]` Catch2 for `FillConvexPoly` on a triangle and a rectangle against expected coverage.
- [ ] **Step 2–4:** write failing tests, port, pass; `check_evidence.py` passes; `replay compare chargen_replay` still 20 of 20; the sandbox suite unchanged.
- [ ] **Step 5:** Commits per function.

### Task 6: Integration and the project gate (coordinator)

- [ ] **Step 1:** Merge Task 1, then Tasks 2–4 as their reviews pass (each rebases on `first` first; the coordinator resolves `src/main.cpp` and `platform/window.cpp` hunks), then Task 5.
- [ ] **Step 2:** Gates from Global Constraints after each merge.
- [ ] **Step 3:** The spec's section 7 checks: presented-frame identity on the start screen and at every `chargen_replay` checkpoint on Windows and Linux; the sandbox suite with defaults, with `Presenter=gpu`, and with `Scale=2`/`World Scale=1` at 1920×1080 (shots show the world at 1× and the UI at 2×).
- [ ] **Step 4:** README: Run section documents `Presenter`, `Scale Filter`, `World Scale` and the hotkey; the deviations list has the guard entry; `docs/followups.md` updated. Whole-project review on the most capable model; one fix round; push `first` to `iwd2r` as `main` and `first`.
