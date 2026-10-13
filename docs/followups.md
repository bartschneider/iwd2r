# Follow-ups from the verification harness

These items were found while building the harness (project 2) and parked. The first group is engine behaviour for project 3. The rest is hardening of the tools.

## Engine fidelity (project 3)

- **`atol`.** The original's `atol` is VC6's. Our engine maps it only in `C2DArray::GetAtLong` and `CRuleTables::GetNextLevelXP`. Other call sites should use it too. Consider mapping `atol` in `compat/mfc.h` the way `rand` is, with oracle coverage.
- **Upper-casing.** The original upper-cases by code page (`_mbsupr`). Ours handles ASCII only.
- **`CGameSprite::CheckAranceFailure` (0x7243F0).** The original compares 16-bit values. Ours compares 32-bit values. The case needs engine state and is pending.
- **`ProcessEffectList` (0x72DE60).** The address is labelled twice in `src/`: `sub_72DE60` (an empty stub) and `ProcessEffectList`. The Feats step (`UpdateFeatsPanel`, 0x60E3E0) still calls the empty stub, so replays diverge at the feats step.
- **`SetStat`, DEX case (0x7600F6).** The original sets `field_562C` and calls `ProcessEffectList`, which rolls 5 dice. Ours does neither.
- **`FillConvexPoly` (0x7C10D5).** Three differences:
  - the initial left error term should be `nLeftAdjUp - nLeftDy`; ours uses `nRightAdjUp`;
  - `nRightDy` and `nLeftDy` start from the minimum Y, not `nMaxYPt`;
  - when `Dy <= 0` the original leaves Run, AdjUp and ErrTerm unset.
- **`au_counter` after New Game loads.** It lags the frame count by 1 in ours and by 2 in the original. The replay compare reports it and does not compare it.
- **State dump coverage.** The dump lacks gender, alignment, class levels and skills. Widen the schema, and add a screenshot comparison to `replay compare`.
- **Pending oracle cases that need engine state:** `CheckSave` (0x4A42F0), `ProcessEffectList` (0x72DE60), `Hit` (0x73B760), `Damage` (0x73D560), `C2DArray::Load` (0x402B70) and `CheckAranceFailure` (0x7243F0). See the table in [docs/evidence/README.md](evidence/README.md).

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

## Recorder

These are in `oracle/replay/` and `tools/`.

- The dump invariant (`recorder.cpp`) is enforced on checkpoint dumps only, not on periodic ones.
- `oracle.py` reads the injector's pid with a blocking `readline` that has no timeout, so a stuck injector hangs the run.
- A swallowed `WM_KILLFOCUS` skips the `SetCursor` call the original would have made.
- `g_lastAu` starts at 0, so the first stall count can be off by one.
- The frame count drifts across loads. Key checkpoints on the screen settling, not on frame numbers.
- `GetKeyState(VK_CAPITAL)` is not hooked, so Caps Lock state comes from the host.
- `tools/gen_fields.py` does not handle template pointers or declarations with several declarators; such fields are skipped or misread.
- `tools/minischema.py` does not handle array-form `items`, so a schema using them is not validated.
- The replay mode's timers and clock have no data-free test: `platform::replay::SetTimer`, `KillTimer`, `FireTimers` (including its re-arm of periodic timers) and the virtual wall clock (`WallClock`, `NowMs`) in `platform/replay.cpp` are exercised only by the Windows `replay compare`, so a regression there shows up only as a timing difference against the original. `tests/test_clock.cpp` covers the real-time clock, not these.

## Tooling

- `ORACLE_DIR` in `tools/oracle.py` and `WIN_CHECKOUT` in `tools/play.py` are hardcoded to `/mnt/c/Users/thier/...`, so the Windows side works only for this user. Derive them from `%USERPROFILE%` (through `cmd.exe /c echo` or `wslvar`) or an environment variable.
- Windows Sandbox real-input mode (`--vm windows`) is untested. It needs the Windows feature enabled and a reboot.
- `replay compare` reports a `known_differences.json` entry that matched nothing, but does not fail on it. An entry carries no scenario, so one valid for another scenario would look stale. It stays a warning until entries carry a `scenario` field.
- Consider a data-gated test through `CRuleTables::GetClassStringMixed` (the fighter-type string choice: Barbarian 34, otherwise 16799). The id choice is confirmed by the disassembly noted in `docs/evidence/544120.md` and exercised by the chargen replay.

## Render API (project 4)

The guards in `src/render/api.h` leave these for the renderer that first draws the world through a backend.

- **Tiles.** `CVidTile::Render` only fills the VRAM tile cache on a miss, at (0, 0), without the stencil tile or fog. Each frame's tiles reach the screen through `CInfTileSet::Render` (0x5CB4A0) and `CInfTileSet::RenderBlack` (0x5CB630). A screen-level renderer must guard those two, with the tile, the destination and the stencil.
- **FX groups.** FX draws arrive in FX-surface coordinates. The placement comes from `FXPrep` (ptPos, ptReference, mirror flags 0x10/0x20) and the composite from `FXUnlock` and `CInfinity::FXBltFrom`. A backend needs begin-group and end-group calls at `FXPrep` and `FXUnlock`.
- **Pointer.** `RenderPointer` draws its cell to the front surface in screen coordinates, and it reaches the backend as an ordinary `DrawCell`. Add a `DrawPointer` primitive, or pass `nSurface`.
- **Frames and layers.** Nothing in the engine calls `BeginFrame`, `EndFrame` or `SetLayer` yet.
