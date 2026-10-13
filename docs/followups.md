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

- The original sees the runner's `hInstance` from `GetModuleHandle(NULL)`, not its own.
- `crt_ranges` in `oracle/loader/vanilla.json` is not exhaustive.
- Pages outside the image's sections stay read-write.
- `WriteImage` restores one page's protection.
- `assert(lo <= hi)` is compiled out in `RelWithDebInfo`.
- A heap-corruption break writes `complete: true` into the report.
- The timeout is a DWORD and can overflow.
- `WAIT_FAILED` is reported as a timeout.
- The compare step labels fixed expected values as "original".
- The runner needs the x86 Visual C++ runtime.
- A fault inside original code that holds a CRT lock leaves the lock held. The fresh process per file and the timeout contain it.

## Vanilla copy

- `dialog.tlk` and the 188 restored Override files have no reference MD5.
- The `mpsave/` folder is kept.
- UTF-16 INI files and a case-insensitive `Resolution=` key are not handled.

## Recorder

- The invariant is enforced on checkpoint dumps only.
- Reading the injector's pid has no timeout.
- A swallowed `WM_KILLFOCUS` skips `SetCursor`.
- `g_lastAu` starts at 0.
- The frame count drifts across loads. Key checkpoints on the screen settling.
- `GetKeyState(VK_CAPITAL)` is not hooked.
- `gen_fields` does not handle template pointers or declarations with several declarators.
- `minischema` does not handle array-form `items`.

## Tooling

- All worktrees share one Windows build folder, `C:\Users\thier\iwd2r-build`.
- Windows Sandbox real-input mode (`--vm windows`) is untested. It needs the Windows feature enabled and a reboot.
- `replay compare` reports a `known_differences.json` entry that matched nothing, but does not fail on it.
