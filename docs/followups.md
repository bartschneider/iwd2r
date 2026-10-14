# Follow-ups from the verification harness

These items were found while building the harness (project 2) and parked. The first group is engine behaviour for project 3. The rest is hardening of the tools.

## Engine fidelity (project 3)

- **Two-area save (M1 Review Focus 2).** The game-file test only checks that a GAM naming an unsaved second area loads without asserting. The real check — load AR1001 alongside AR1000 in-process, save, assert the GAM's current area and what `CInfGame::SaveGame` (0x5AC430) writes for each loaded area, reload — waits for effect 261 (`IcewindCGameEffectImmunityToEffectAndResource`) to get an `ApplyEffect`: AR1001's creatures carry it and loading the area asserts (M3).
- **CD aliases outside the HD0 prefix.** On Windows `platform/fs.cpp` re-roots a drive path onto the data folder only when it starts with the `[Alias] HD0:` install prefix (other drive paths pass through natively). The CD2 alias of the vanilla copy sits under HD0, so `CDimm::CDScan` finds its folder; a CD alias rooted elsewhere would still make the stubbed `CDimm::CDSwitch` (0x7828D0) quit with exit 0 and its stderr line. Port `CDSwitch` or re-root every alias from the INI.
- **MSVC `iwd2r_tests` does not build** (warnings as errors in `tests/test_cre.cpp`, `test_dialog.cpp`, `test_gam.cpp`, `test_layout.cpp`), so compat tests such as the alias re-rooting run on Linux only; fix the warnings and add the Windows ctest to the gate.
- **ASan findings on the world_entry path (M1 streams' files):** `CGameArea::Unmarshal` (src/CGameArea.cpp:~2538) copies 8 bytes from a 6-character area name (heap over-read); `CDimm::Dump` (src/CDimm.cpp:~1152) frees with `delete` what was `malloc`ed. Found by the chapter-song fix's ASan run; fix with evidence at the next integration.
- **`preloadBuffers` (0x7D1EE0) `field_6C`:** the original adds the read count to it after the fill; the port sets it only with a second section, which shifts the running section offset on later refills (bookkeeping, not audio).
- **`CGameArea::SortLists` (0x4794E0) queue loop:** after the assert on a non-DELETED failed share the original still inserts the object into the queued list (`joined_r0x00479643`); ours decrements and asserts but does not insert (assert-only path). Also check whether the queued-list insertion is `InsertAfter` the first node with `y >= obj.y` (the decompile's 0x7FB367 thunk) or our `InsertBefore(pos)`; both tests queue one object, so neither distinguishes them.
- **`CAIObjectType::Decode` party specifiers (4-7, 28, 47, 48 → `CInfGame` 0x5BDFE0 nType 1-7):** the specifier → nType table is verified semantically, not against the original's table at 0x851898-0x8518A4; the `ai/decode` oracle case excludes these specifiers and the unit test has one party member. Add a two-member case (different level, damage, AC) and the specifiers to the oracle case.
- **`atol`.** The original's `atol` is VC6's. Our engine maps it only in `C2DArray::GetAtLong` and `CRuleTables::GetNextLevelXP`. Other call sites should use it too. Consider mapping `atol` in `compat/mfc.h` the way `rand` is, with oracle coverage.
- **Upper-casing.** The original upper-cases by code page (`_mbsupr`). Ours handles ASCII only.
- **`CGameSprite::CheckAranceFailure` (0x7243F0).** The original compares 16-bit values. Ours compares 32-bit values. The case needs engine state and is pending.
- **State dump coverage.** The dump lacks gender, alignment and class levels. Widen the schema, and add a screenshot comparison to `replay compare`.
- **Pending oracle cases that need engine state:** `CheckSave` (0x4A42F0), `ProcessEffectList` (0x72DE60), `Hit` (0x73B760), `Damage` (0x73D560), `C2DArray::Load` (0x402B70) and `CheckAranceFailure` (0x7243F0). See the table in [docs/evidence/README.md](evidence/README.md).

- **Resource signatures compared as big-endian DWORDs.** `CResEffect.cpp:38` ('EFF ', 'V2.0'), `CResStore.cpp:38` ('STOR', 'STOO', 'V9.0'), `CResCHR.cpp:49` ('CHR ') and `CResCellHeader.cpp:125` ('BAH ', 'V1  ') compare a file's first DWORDs with multi-character constants, whose value is the big-endian one; the original compares with the bytes read little-endian (as `CResDLG::Demand`, 0x402782: `cmp [edi], 0x20474C44`, fixed in M2's dialog-world stream; docs/evidence/402760.md). If nothing else gets around it, such a check refuses every real file. Check each against its decompile and fix with its own evidence.

- **Remove the patched-DLG device in tests/test_dialog.cpp (M2).** The Hedron test patches one byte of the cached 10HEDRON.DLG (`TODO(remove)`, only when no start condition holds) so the dialog runs while `CGameSprite::EvaluateStatusTrigger` is unported; remove it once the movement stream's trigger evaluator lands.
- **Sprites answer FALSE for the base's unported status triggers (M2).** `CGameSprite::EvaluateStatusTrigger` (0x731B30) falls through to `CGameAIBase::EvaluateStatusTrigger` (0x453840), which ports True, False, Global/GT/LT, GlobalTimerExpired, RandomNum/GT/LT, Exists, Dead, IsActive, Allegiance, Class, General, Gender, HP/GT/LT, Range, See, EntirePartyOnMap, AreaRestDisabled and InCutsceneMode; every other trigger (NearLocation, Race, Specifics, Alignment, the base TimerActive, ...) answers FALSE for sprites and other objects until the base port covers it (docs/evidence/453840.md lists the TODO range).

## Runner

These are in `oracle/runner/` and `oracle/loader/`.

- The original's `GetModuleHandle(NULL)` returns the runner's `hInstance`, not the exe's. Original code that loads resources by module handle would look in the wrong place.
- `crt_ranges` in `oracle/loader/vanilla.json` is not exhaustive. A C runtime variable outside the listed ranges is restored with `.data` before each case, which can desynchronise the CRT.
- Pages outside the image's sections stay read-write. Writes there are not caught.
- `WriteImage` restores the protection of one page only, so a write spanning pages with different protections leaves the later ones changed.
- `assert(lo <= hi)` in the generators is compiled out in `RelWithDebInfo`, so a reversed range is not caught in the shipped build.
- A heap-corruption break writes `complete: true` into the report, so a run cut short looks complete.
- The per-file timeout is a DWORD and can overflow for very large values.
- A `WAIT_FAILED` from the wait on the child is reported as a timeout, which hides the real error.
- The compare step labels fixed expected values as "original", which misreads in the output of cases that check against constants.
- The runner needs the x86 Visual C++ runtime on the host; `oracle.py build` copies only what the runner imports.
- A fault inside original code that holds a CRT lock leaves the lock held, and later cases in the same file could hang. The fresh process per file and the timeout contain it.

## Vanilla copy

These are in `build_vanilla` and `verify_vanilla` in `tools/oracle.py`.

- `dialog.tlk` and the 188 Override files restored from the install have no reference MD5, so a wrong file there would not be noticed.
- The `mpsave/` folder is kept in the copy.
- UTF-16 INI files and a `resolution=` key in another case are not handled by the INI preparation.
- The vanilla copy drifts across runs while its marker stays valid. Both engines write cache files and INI state into it, and only `replay compare` backs up and restores the INIs; `replay record` alone does not. `ensure_vanilla` checks only the marker (`verify_vanilla` runs when the copy is built), so a later run can start from a different state. Snapshot the writable files (the INIs and the cache) around every run, or add a drift check against the manifest the marker was written from.
  - It already bit the Linux gate: the Linux test copy (`~/.local/share/iwd2r/game`) keeps its `icewind2.ini` aliases (`HD0:=C:\Users\thier\iwd2r-oracle\game\`), so tests read saved games and other state through them from the Windows oracle copy that replays write into. On 2026-10-14 the start-screen smoke hash changed with no code change (Load Game became enabled once saves existed there) and had to be re-pinned. Fix: point the test copy's aliases at itself, or give the smoke test its own empty save folder.

## Recorder

These are in `oracle/replay/` and `tools/`.

- `play.py --vm linux` cannot produce replay dumps: it drives the game through xdotool and drops `IWD2R_SCRIPT`, so replay mode never leaves the menu, and `play.py --linux --headless` without the vanilla copy fails at the save step (CInfGame.cpp:4206). Our half of a comparison is only reachable through `oracle.py replay compare` (or its `run_ours`). Add a `play.py --replay <scenario>` path (or an `oracle.py replay ours`) that runs our replay half against a recorded scenario on Linux.
- The recorder drops a door whose name or flags fail to read, while our dump always includes it; emit the door with null values or log the drop so the two sides cannot disagree silently (M2 Task 1 review).
- The dump invariant (`recorder.cpp`) is enforced on checkpoint dumps only, not on periodic ones.
- `oracle.py` reads the injector's pid with a blocking `readline` that has no timeout, so a stuck injector hangs the run.
- A swallowed `WM_KILLFOCUS` skips the `SetCursor` call the original would have made.
- `g_lastAu` starts at 0, so the first stall count can be off by one.
- Loads are deterministic in the replay model (`platform/replay_clock.h` `FrameGate`): the calls a load draws from inside an AI update are held (no frame, no clock step, no timer), so `au_counter` now equals `frame` and a load costs the same AI updates on every machine (sandbox world_entry identical with every ReadFile off the main thread slowed by 1 ms). Open: a host recording of `scenarios/pending/world_entry.txt` made after the change, to confirm the host matches the sandbox (the pre-change host recordings `runs/20261013-143300`/`-143324` differ in `ai_rng_state` from chapter on). `held_frames` still varies between machines; it is a timing field.
- `GetKeyState(VK_CAPITAL)` is not hooked, so Caps Lock state comes from the host.
- `tools/gen_fields.py` does not handle template pointers or declarations with several declarators; such fields are skipped or misread.
- `tools/minischema.py` does not handle array-form `items`, so a schema using them is not validated.
- A test that aborts inside the engine (an engine assertion, `_Exit`) leaves its `/tmp/iwd2r-view-*` folder behind: `DataView`'s destructor never runs. A normal Catch2 failure cleans up. Sweep stale views at the start of a run, or register the view for removal in the crash handler.
- The replay mode's timers and clock have no data-free test: `platform::replay::SetTimer`, `KillTimer`, `FireTimers` (including its re-arm of periodic timers) and the virtual wall clock (`WallClock`, `NowMs`) in `platform/replay.cpp` are exercised only by the Windows `replay compare`, so a regression there shows up only as a timing difference against the original. `tests/test_clock.cpp` covers the real-time clock, not these.

## Tooling

- `ORACLE_DIR` in `tools/oracle.py` and `WIN_CHECKOUT` in `tools/play.py` are hardcoded to `/mnt/c/Users/thier/...`, so the Windows side works only for this user. Derive them from `%USERPROFILE%` (through `cmd.exe /c echo` or `wslvar`) or an environment variable.
- Windows Sandbox real-input mode (`--vm windows`) is untested. It needs the Windows feature enabled and a reboot.
- `replay compare` reports a `known_differences.json` entry that matched nothing, but does not fail on it. An entry carries no scenario, so one valid for another scenario would look stale. It stays a warning until entries carry a `scenario` field.
- Consider a data-gated test through `CRuleTables::GetClassStringMixed` (the fighter-type string choice: Barbarian 34, otherwise 16799). The id choice is confirmed by the disassembly noted in `docs/evidence/544120.md` and exercised by the chargen replay.

## Project 4 follow-ups

- **`CVidBlitter::SoftBlt` (0x7B7280) and `SoftSrcKeyBlt` (0x7B74F0).** Still stubs. `CVidBlitter::Init` selects them only at 16 bpp (every other depth takes `DxBlt`), and iwd2r runs at 32 bpp, so nothing reaches them; porting them needs `IDirectDrawSurface::GetSurfaceDesc` in `platform/ddraw_shim.h` and two `DDSURFACEDESC` members in `CVidBlitter.h`. Ruling P4-9.
- **Real-GPU runs in a Hyper-V VM (GPU-P).** Windows Sandbox shares the host GPU through GPU partitioning but picks the adapter itself (the AMD iGPU on a hybrid machine, no Vulkan), so `Presenter=gpu` never runs on the RTX there. A Generation 2 Hyper-V VM under `C:\Users\thier\iwd2r-vm` with `Add-VMGpuPartitionAdapter -InstancePath <RTX>` and the host's NVIDIA driver store copied into the guest runs our engine on the real GPU with real Vulkan, driven headlessly over PowerShell Direct (`Invoke-Command -VMName`) like the sandbox driver. Needs one admin setup by the user (Hyper-V is present on this host: Windows 11 Pro, vmms running). Planned as `play.py --vm hyperv` after project 4 Task 6.
- **A free Xvfb display across `play.py` processes.** `tools/realinput.py`'s `start_xvfb` picks the first free display number under a lock that only covers the threads of one process. Two `--vm linux` runs started at the same moment from different worktrees can choose the same number; the second Xvfb exits, but `x_display_ready` may already see the first one's server, and both games would run on one display. Pick and start under a `tools/filelock.py` lock (or check that our Xvfb is still alive when the display answers) so parallel streams can run `--vm linux` at once.
- **D3D12 and Metal shader blobs.** The GPU presenter asks SDL for `SDL_GPU_SHADERFORMAT_SPIRV` only, so it runs on Vulkan alone: a Windows machine without a Vulkan driver, and macOS, get `classic`. Compiling the shaders to DXIL and MSL as well (`tools/shaders.py`, with SDL_shadercross or the vendor compilers) and passing every format SDL accepts would bring the presenter to D3D12 and Metal.
- **`FillPoly` (0x7C13A0).** Still a stub that returns FALSE. `CInfinity::FXRenderClippingPolys`, now ported, calls it for non-convex wall polygons, so sprites behind those walls are not occluded yet; `FillConvexPoly` (fixed, docs/evidence/7C0F40.md) covers the convex ones.
- **The world screen at Scale 2, World Scale 1 has not been seen running.** Before UI scaling counts as done, `scenarios/pending/world_entry.txt` must run at 1920x1200 (or 2560x1440) with `Scale=2` and `World Scale=1`, with shots showing the world at 1x and the UI at 2x. The panels themselves are checked: every GUIW08/GUIW10 panel lies inside `WorldPanelRects`' band for the viewport it is shown over (`world_panels_lie_in_the_band` in `tests/test_layout.cpp`, the CHU rectangles read with `tools/bif.py`); the world screen's UI manager holds only GUIW's ten panels and no control lies outside its panel, so the quick weapon, item and spell choices are drawn by the action bar's (panel 1's) own controls, not by a popup above it.
- **The default stays `Presenter=classic`** until the gpu presenter has run on a real GPU under Windows (ruling P4-13; the Hyper-V GPU-P run above is that run). `auto` behaves as `gpu` once the probe passes.
- **User check of the gpu presenter on real Windows Vulkan (P4-5).** Not yet done: the probe's `presenter: gpu probe ok` line, the swapchain claim on the game's window, Ctrl+Shift+R back to classic on the same HWND, alt-tab, minimise and a high-DPI display.
- **Stub verification (M2/M3).** `replay compare` compares dump fields, not pixels. Add a framebuffer hash or a screenshot comparison, and scenarios that reach wall occlusion (WED wall polygons with a sprite behind), translucent and 0x200 cells, palette range effects, lightning weather and the message screen, so that `FXRenderClippingPolys`, `RenderMessageScreen`, `PolyLine`'s 32-bpp body, `DrawLightingEffect32` and the cell blends get runtime coverage. Any shot comparison must mask (or compare with a tolerance) the six start-screen shots' animated box at x 78-132, y 360-414, which differs between any two runs, including two runs with identical settings.
- **The cell blends' bound** reads the current `CVidInf`'s `m_SurfaceDesc` instead of taking the extent from `BKRender`/`FXRender`, which know the surface they draw into.
- **High-DPI mouse mapping in the gpu presenter.** The whole-frame mapping (`GpuBackend::WindowToFrame`/`FrameToWindow`, `platform/present_gpu.cpp`) scales by `SDL_GetWindowPixelDensity`, while drawing uses the swapchain-to-window size ratio; the two agree only while the swapchain is the window's pixel size. Untested on a high-DPI display; use the drawing's ratio in both.
- **The classic presenter's NEAREST outlives its region list.** A composition region sets the texture's scale mode to nearest (`present_classic.cpp`, `DrawRegion`), and nothing restores linear when the list empties at Scale 1; it only shows when the window is not the frame's size (a resized window, high DPI). Restore the mode the texture was made with when the whole-frame path draws.
- **A rebuild budget per time window.** A run allows one device rebuild; a lost-again device falls back to classic for the rest of the run, and a new window or the hotkey does not bring gpu back (fixed in the project 4 review round: the fallback is sticky). Two genuine acquire failures hours apart in a long session therefore leave classic for good. Allow one rebuild per time window (say one per ten minutes) instead.
- **Untested region edges.** The region matrix test in `tests/test_layout.cpp` skips the vacuous pairs (equal scales); the release-build fallback in `ScreenLayout.cpp`'s `RegionsFor` when a world screen's regions do not tile is unreachable with the current panels and untested; `play.py`'s `window_point` ignores the letterbox offset in a resized window.
- **The mouse mapping off the main thread.** `SetRegions` and `Present` run on the main thread, but `GetCursorPos` maps the mouse from the AI thread too (`CChitin::AsynchronousUpdate`). The region list is a locked snapshot; the backend's whole-frame mapping (`SDL_RenderCoordinatesFromWindow` in the classic backend, the last frame's placement in the gpu one) is read without a lock while the main thread may be replacing the backend (the hotkey, a new window). Guard the backend pointer, or cache the mapping in the front under the same lock.
- **Negative letterbox offsets.** When the window is smaller than the frame times the world scale, `RegionsFor`'s centring offsets go negative: the regions crop at the window's edges while the whole-frame path scales the frame down. Untested.

### Render API

The guards in `src/render/api.h` leave these for the renderer that first draws the world through a backend.

- **Tiles.** `CVidTile::Render` only fills the VRAM tile cache on a miss, at (0, 0), without the stencil tile or fog. Each frame's tiles reach the screen through `CInfTileSet::Render` (0x5CB4A0) and `CInfTileSet::RenderBlack` (0x5CB630). A screen-level renderer must guard those two, with the tile, the destination and the stencil.
- **FX groups.** FX draws arrive in FX-surface coordinates. The placement comes from `FXPrep` (ptPos, ptReference, mirror flags 0x10/0x20) and the composite from `FXUnlock` and `CInfinity::FXBltFrom`. A backend needs begin-group and end-group calls at `FXPrep` and `FXUnlock`. `CInfinity::FXRenderClippingPolys` (0x5CE350) writes the wall-occlusion mask into the FX surface between them with `FillConvexPoly`/`FillPoly`, unguarded; the group calls must carry it too.
- **Pointer.** `RenderPointer` draws its cell to the front surface in screen coordinates, and it reaches the backend as an ordinary `DrawCell`. Add a `DrawPointer` primitive, or pass `nSurface`.
- **Frames and layers.** Nothing in the engine calls `BeginFrame`, `EndFrame` or `SetLayer` yet.
