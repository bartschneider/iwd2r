# Imports under the trace bar: a Frida phase that runs

Date: 2026-10-23. Status: approved in principle by thierbig ("fix Frida the best way"; the
import evidence bar stays `trace` or better). Follows the measured-verdicts section of
`docs/superpowers/specs/2026-10-20-wave-driver-triage-design.md` and the Frida adoption in
`docs/frida-tracing.md`.

## Why

The import queue (`tools/wave.py run --queue import`) cannot close a batch today. Its rule
is `kind: trace` or better, and a trace has two legal sources: the fork's recovery note or a
Frida differential compare. Measured on 2026-10-23: 6 of the 33 import rows are named in the
fork's 130 notes; `tools/check_evidence.py` accepts any path after the fork sha, so batch
i002's worker cited the fork's source files as "traces"; and the batched Frida phase has never
produced a verdict. A spike the same day (`.superpowers/sdd/2026-10-17-wso-integration/
frida-spike-report.md`) found why, in order:

1. `tools/frida/probe.py` runs our **x86** RelWithDebInfo build; `play.build_windows()` and
   the gate build x64. The x86 build was stale (2026-10-12) until the spike rebuilt it.
2. Original side: the in-box driver writes `done.txt` and runs `shutdown /s /t 0` the moment
   the injector exits, killing the background Frida sidecar before it writes `RESULT:`.
3. Ours side: `ours_logon` emits `--env A=1 B=2 …` for an `append` option, so `orig.py` dies
   in argparse (one-line fix, verified).
4. Ours side, after the fix: `NOT-EXERCISED (MTP hit x0)`; the sidecar's 20 s `--post-load`
   window is not exposed by probe.py and likely ends before the scenario's trailing clicks.
5. `truth.frida_run` passes no `--clicks`, `--hit`, `--hit-min` or `--loaded-rva`; the doc's
   RVAs are stale (MoveToPoint is 0x1A8A90, EngineActivated 0x330E60 in the 2026-10-23 map).
6. The driver skips the Frida phase for `kind == 'import'` and its allowlist check diffs
   against the cut base, so a `first` commit merged into a batch reads as an out-of-scope edit
   (i002 reverted the plan commit be17ecaf for that reason).

Also found this week: `triage.py` regenerated at a moving `first` reclassifies adopted fork
bodies and breaks the calibration test; `theirs_real_ours_stub` rows are not "nothing to
adjudicate" (3 of 7 fork bodies in i002 contradicted the binary).

## What changes

**A. The sandbox run completes with a verdict (both sides).** The in-box logon command waits
for the sidecar's status file to carry a `RESULT:` line, bounded by the scenario timeout plus
60 s, before `done.txt` and the shutdown. `--env` is emitted one pair per flag. probe.py
exposes `--post-load SECONDS`; the default is the scenario's timeout (header `# timeout: S`,
else 180), not 20 s. A test parses every generated logon command with `orig.py`'s own
argparse.

**B. Our x86 build is the one under test.** `tools/play.py` gains `build_windows_x86()`
(preset `windows-msvc-x86`, RelWithDebInfo, the linker map) and `tools/gates.sh --windows`
builds it. The driver builds it from the **batch worktree's** Windows checkout before the
phase, so RVAs match the bodies under test, and `truth.parse_map` resolves every hook RVA and
the `loaded` site (`?EngineActivated@CScreenWorld@@UAEXXZ`) from that map. A missing symbol is
a phase error for that row, not a crash.

**C. `truth.frida_run` drives probe.py the way a hand run does.** Per scenario and side:
`--clicks` from the table (`clicks`, default 3 for scenarios without their own clicks),
`--hit any`/`--hit-min 1` (the table's hook names), `--loaded-rva` from the map, the
scenario's timeout, `--new-attempt "driver frida <batch> <scenario>"`. Scenarios:
`world_entry`, `first_fight`, `walk_and_talk`. One sandbox at a time (the locks exist).
Each side's exit code and `RESULT:` go into the result; a side that proves nothing (exit 2)
marks the scenario `not measured`, never fatal.

**D. The driver runs the phase for imports and writes what it measured.** The `kind ==
'import'` exclusion goes. After the phase, per row:
- exercised in at least one scenario and `trace_diff compare` exit 0 everywhere it was
  exercised: the driver rewrites the row's evidence front matter to `kind: trace` and
  `trace_source: frida <probe> orig:<sha256> ours:<sha256> compare=0` (the sha256 of the two
  binaries, the line `check_evidence` already accepts), dropping `exemption:`; a wave-1 row
  gets its verdict recorded with `measured: frida` as the 2026-10-20 spec says;
- exercised and different: a finding to the worker's fix round with the diff summary; the
  worker fixes the body or reverts the import and says which;
- exercised nowhere: for an import, the row cannot close. The mechanical check lists it; the
  worker's fix round reverts that function to its pre-batch body, deletes its evidence file,
  and appends `{address, name, file, batch, needs}` to `docs/wso/import-parked.json` with
  `needs` naming the scenarios tried. `close --kind import` accepts a batch when every
  remaining row is `trace` or better and every dropped row is in the parked list. A wave-1
  row stays a dump reading, as today.

**E. Calibration decides the default.** `truth.py calibrate` runs path C over the 28
calibration rows (18 rule-2 rows + the seeded 10) and writes `docs/wso/truth-calibration.json`
for real: `frida_default` becomes true when at least a fifth of the sample measured, as the
spec says. If it stays false, the driver keeps `--no-frida`, imports stay stopped, and the
numbers are published; that is an honest outcome, not a failure of this spec.

**F. The evidence rules close their loophole.** `check_evidence.py`'s fork form becomes
`WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md`; the checker reads the note at that
sha with `git show` and requires the function's address (hex, case-insensitive, with or
without `0x`) or its unqualified name in it. A note that cannot be read is a failure, not a
pass.

**G. Driver and triage hygiene.** The allowlist check diffs the batch branch against
`git merge-base first <branch>`, so merged `first` commits are not the worker's edits.
`triage.py` regenerates only rows without a recorded verdict at `--ours-ref` and copies the
closed rows' entries from the committed file (their classes were calibrated at
`CALIBRATION_OURS_REF`).

**H. The import brief tells the truth.** It drops "nothing to adjudicate": the worker checks
the fork's body against the disassembly and refuses what the exe contradicts (recorded in the
report and the parked list with `needs: "fork body contradicts the exe: …"`); stub dependencies
inside the worker's own files are imported with the row (one trailer each); a dependency
outside its files parks the row with that reason.

## Not in scope

New scenarios (store, inventory, containers, chargen-only); the desktop (`--host`) path;
Frida on the desktop; the renderer streams; any `src/` change outside an import batch.

## Interfaces

- `tools/frida/probe.py run`: `--post-load SECONDS` (default: scenario timeout). Logon
  commands wait for `RESULT:` before `done.txt`.
- `tools/play.py`: `build_windows_x86(checkout=WIN_CHECKOUT) -> exe path`; the map beside it.
- `tools/truth.py`: `frida_run(batch_id, table, scenarios, run, root, loaded_rva, clicks=3,
  hit_min=1, timeout=None)` returns `{scenario: {'orig': path|None, 'ours': path|None,
  'orig_exit': int, 'ours_exit': int, 'result': {'orig': str, 'ours': str}}}`;
  `calibrate` runs it.
- `tools/driver.py`: phase for both kinds; `_write_trace_evidence(path, probe, orig_sha,
  ours_sha)`; the import close check and `docs/wso/import-parked.json`
  (`[{address, name, file, batch, needs}]`, committed).
- `tools/check_evidence.py`: `TRACE_SOURCES[0]` tightened; `fork_note_mentions(sha, path,
  address, name) -> bool`.
- `docs/wso/import-brief-template.md`: rule 1 reworded, new rules for refusals and
  dependencies.
- `docs/wso/truth-calibration.json`: written by a real run.

## Testing

Tool tests, no credits, no sandbox: logon commands parse with orig.py's parser; `frida_run`
argv per scenario and side (fake `run`); the driver phase with fake probe results covering
equal / different / not exercised for both kinds; the evidence front-matter rewrite; import
close with a parked row and with an unparked dropped row (refused); the fork-note check with
a fixture repo; the allowlist against a merge base; triage regen keeping closed entries.
Acceptance, with launches: one MoveToPoint pair that reaches `compare=0` (≤2 launches per side
per attempt), then the calibration run.

## Order and estimate

1. A + B + the MoveToPoint pair (the smoke): half a day of Sonnet work plus launches.
2. C + D + the import close: a day.
3. E: one calibration run (28 rows × 3 scenarios × 2 sides, batched by hook table, so 6
   launches), then the numbers.
4. F + G + H + docs: half a day.
