# M3 End-of-Milestone Compare Plan (ruling M2-27) Implementation Plan

> **For agentic workers:** This is an execution runbook, not a code plan —
> drive it step by step in order; every step names the exact command and
> the output that means it passed. Steps use checkbox (`- [ ]`) syntax.
> No source files are created or modified; the only artifacts are run
> folders under /home/thier/iwd2r-runs/ and a report.

**Goal:** Close M3 with the full on-record verification set at one pinned
fix-round head: replay compares twice for world_entry, walk_and_talk and
first_fight, the second save_load run required by M2-27, the function
oracle, and both real-input suites.

**Architecture:** One serialized block of sandbox-bound and host-bound
commands, all at a single recorded commit; every run folder moved out of
the worktree to /home/thier/iwd2r-runs/ as it completes; a results table
reported to the coordinator at the end.

**Tech Stack:** tools/oracle.py (replay compare, run), tools/play.py
(--vm linux/windows --suite), tools/winsandbox.py + tools/sandbox/driver.ps1
(Windows Sandbox), systemd-run (memory cap).

**Spec:** coordinator round-12 directive (2026-10-17); M2-27 as recorded in
`first/.superpowers/sdd/2026-10-14-iwd2r-finish-simulation-m5/progress.md`
("the M3-end compares ... want a SECOND save_load run at the fix-round head
per M2-27"); harness doc `docs/superpowers/plans/2026-10-12-iwd2r-verification-harness.md`.

## Global Constraints

- One Windows Sandbox at a time: `pgrep -af "[w]insandbox"` must come up
  empty before every sandbox-touching step; wait for it to clear, and keep
  host load under 8 (`cat /proc/loadavg`) before starting a compare.
- Never kill by pattern; stop stray processes by exact PID only.
- Our-side engine processes run under the memory cap:
  `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 --`.
- NEVER regenerate expect.json from our side. first_fight's (and any other)
  re-derived expects come from m3-fix's `realinput.py expect` — Block A
  starts only after those are merged (precondition P4).
- Run folders live in `/home/thier/iwd2r-runs/` only: move each
  `runs/<stamp>/` out of the worktree as soon as its command finishes.
- No desktop windows: everything headless or in the sandbox; never set
  IWD2R_DESKTOP_OK; never write to the real install or
  `~/.local/share/iwd2r/game`.
- The whole block runs at ONE head. If `first` moves mid-block, see
  "When first moves" below before continuing anything.

## Measured durations (evidence)

| Item | Measured | Source run |
|---|---|---|
| first_fight replay compare | ~1 m 40 s | 20261016-223838 (1 m 36 s), 20261016-225748 (1 m 41 s, under load > 10) |
| save_load replay compare | ~10 min | 20261017-121626 (12:16:26 start, finished 12:26 EDT) |
| world_entry / walk_and_talk compare | ~1–3 min each (est., shorter scenarios) | — |
| Windows sandbox suite (`--vm windows --suite`) | ~25 min | suite-windows-20261016-205011 (20:50:11 → 21:14:42) |
| Linux suite (`--vm linux --suite --jobs 3`) | ~10–20 min (est.) | — |
| Function oracle (`oracle.py run`) | ~2–5 min (est.) | — |

Serialized total: roughly 60–75 min of machine time.

## Preflight (all blocks depend on this)

- [ ] **P1. Pin the head.** From the executing worktree (p3-vis-probe after
  the coordinator signals; the first worktree works identically):
  `git merge first --no-edit` (or fast-forward), then
  `git rev-parse HEAD first first^{tree} HEAD^{tree}` — record the commit
  hash H in the report; HEAD^{tree} must equal first^{tree}. Every result
  below is reported against H.
- [ ] **P2. Confirm m3-fix's expects are in.**
  `git log --oneline -3 -- scenarios/first_fight.expect.json` shows the
  m3-fix re-derivation commit merged (extended schema: per-creature
  hp/max_hp, effect counts, party XP/morale). If not merged, STOP and
  message the coordinator — Block A cannot run against stale expects.
- [ ] **P3. Build.** `python3 tools/oracle.py build` — expect
  `oracle: build done` plus the oracle/recorder/injector lines. This
  covers both the Windows build (function oracle, sandbox suite) and
  leaves `build/linux-gcc/iwd2r` current for the replay compares.
- [ ] **P4. Sandbox and load check.** `pgrep -af "[w]insandbox"` (expect
  no output; if present, wait — m3-fix's re-records and any coordinator
  compare outrank this block) and `cat /proc/loadavg` (expect 1-min load
  < 8).

## Block A — replay compares (sandbox; serialized)

Run each compare from the worktree root. The command already runs both
sides (our Linux engine + the original in Windows Sandbox) and writes
`runs/<YYYYMMDD-HHMMSS>/compare.json`. Defaults are correct: seed 1,
dump-every 30, timeout from the scenario header — do not override.

- [ ] **A1. world_entry, run 1 of 2.**
  `pgrep -af "[w]insandbox"` immediately before (expect no output), then:
  `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- python3 tools/oracle.py replay compare world_entry`
  Pass: compare.json ends with no `differ`/`mismatch` line,
  `"original_result": 0`, `"our_result": 0`, every checkpoint line says
  `match` with `ai_stalled_frames 0/0` and `ai_overlap_frames 0/0`.
  Then `mv runs/<stamp> /home/thier/iwd2r-runs/m3-end-world_entry-a-<stamp>`.
- [ ] **A2. world_entry, run 2 of 2.** Same command after the sandbox
  clears (`pgrep -af "[w]insandbox"`). Pass: identical outcome to A1 —
  two independent MATCHes are the determinism evidence.
- [ ] **A3–A4. walk_and_talk, runs 1 and 2 of 2.** Same as A1–A2 with
  `walk_and_talk`. Expected duration ~1–3 min each.
- [ ] **A5–A6. first_fight, runs 1 and 2 of 2.** Same as A1–A2 with
  `first_fight` (~1 m 40 s each). These run against m3-fix's re-derived
  expects — a mismatch confined to the NEW schema fields (hp/max_hp,
  effect counts, XP/morale) is a m3-fix derivation question, not an
  engine regression; report it, do not "fix" it here.
- [ ] **A7. save_load, second run at the fix-round head (M2-27).**
  Scenario is pending, so pass the path:
  `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- python3 tools/oracle.py replay compare scenarios/pending/save_load.txt`
  Pass: MATCH 12/12 checkpoints + 129/129 periodic dumps including
  loaded/loaded_settled (first run on record: /home/thier/iwd2r-runs/
  20261017-121626 at f5f5971b), and the run folder's saves/ contains
  both `000000000-Autosave - Prologue`-style and
  `000000004-Probe Save`-style folders. ~10 min.
  Move out as `m3-end-save_load-b-<stamp>`.

## Block B — function oracle (no sandbox)

- [ ] **B1.** `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- python3 tools/oracle.py run`
  (defaults: every oracle/cases/*.cpp, seed 1). Pass: exit 0, all cases
  OK in the runner's summary. If a single case is wanted first,
  `--file <name>.cpp` narrows it, but the block's pass criterion is the
  full default run. ~2–5 min.

## Block C — real-input suites

- [ ] **C1. Linux suite** (no sandbox; may run while the sandbox idles
  ONLY if load stays < 8 — default is to keep it serialized):
  `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- python3 tools/play.py --vm linux --suite --jobs 3`
  Runs every `scenarios/*.txt` (pending/ excluded by design) on an Xvfb
  display driven by xdotool. Pass: every scenario's summary shows its
  checkpoints met and no missing shots; exit 0. ~10–20 min.
  Move the run folder(s) out with `m3-end-suite-linux-` prefixes.
- [ ] **C2. Windows sandbox suite.** Sandbox check first, then:
  `python3 tools/play.py --vm windows --suite`
  (no systemd-run needed — the engine runs inside the sandbox VM; the
  driver is SendInput via tools/sandbox/driver.ps1; the sandbox.lock
  serializes against other worktrees, but the pgrep check above is still
  the gate). Pass: same criterion as C1 on Windows. ~25 min.
  Move out with `m3-end-suite-windows-` prefixes.

## When first moves (before or during execution)

- **Before the block starts:** re-do P1 (merge, verify tree equality,
  rebuild), then start. Nothing is lost.
- **After some steps completed, before the rest:** a completed step is
  pinned to the exe/tree it ran against, which was H. Stop at the next
  step boundary, message the coordinator with the list of old-head
  results, and by default re-run EVERYTHING (Block A onward) at the new
  head after P1–P4 — partial old-head results only stand if the
  coordinator explicitly blesses them (they verify a tree that is no
  longer the milestone head).
- **During a running sandbox compare:** let it finish (it is pinned to
  the already-built exe and checked-out tree), then treat it as an
  old-head result per the previous bullet.

## Reporting

- [ ] **R1.** Append a round-12 section to
  `first/.superpowers/sdd/2026-10-14-iwd2r-finish-simulation-m5/vis-probe-r10-report.md`:
  head H, per-step PASS/FAIL, run folder paths, durations, deviations.
- [ ] **R2.** SendMessage to main with the same table plus the block's
  total wall time, and flag anything that needed the "when first moves"
  rule.

## Review Focus (failure modes this plan must not paper over)

- A compare that MATCHes only because both sides timed out: check
  `original_result`/`our_result` are 0 AND the last checkpoint is the
  scenario's final one, not an early one (the compare.json lines carry
  frame numbers — the last must match the scenario's end).
- ai_overlap_frames > 0 on ours anywhere: that is an M2-26 regression,
  not noise — stop and report, do not collect the remaining runs.
- A save_load run whose saves/ is missing either save artifact: the
  post-load checkpoints can still match while the save side is broken —
  the folder check in A7 is what catches it.
- Suite scenarios that "finish" via their per-scenario timeout: the
  summary lists them as timed out; a timeout is a FAIL for this block,
  not a pass with fewer checkpoints.
- Running any of this at an unpinned head (e.g. after an accidental
  checkout): every report row must name H; if H changed mid-block the
  "when first moves" rule applies retroactively.
