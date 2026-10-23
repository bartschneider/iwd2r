# Frida phase for imports — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the batched Frida differential phase produce verdicts in Windows Sandbox, run it for import batches, close imports only on measured rows, and close the evidence-rule loopholes the import queue exposed.

**Architecture:** Four tasks on one branch. Task 1 fixes the sandbox run lifecycle and the x86 build and proves one MoveToPoint pair. Task 2 wires `truth.frida_run` and the driver phase for both batch kinds, with the evidence rewrite and the import-parked list. Task 3 runs the real calibration and commits its numbers. Task 4 tightens `check_evidence`, fixes the driver's allowlist base and the triage regeneration, and rewrites the import brief.

**Tech Stack:** Python 3 standard library (tools, tests), PowerShell/cmd logon commands staged by `tools/frida/probe.py` into Windows Sandbox, the MSVC x86 RelWithDebInfo build through `tools/play.py`.

**Spec:** `docs/superpowers/specs/2026-10-23-frida-imports-design.md` (sections A–H). Spike facts: `.superpowers/sdd/2026-10-17-wso-integration/frida-spike-report.md` in the `first` worktree; its one-line `--env` diff is at `/home/thier/iwd2r-runs/frida-spike/probe-env-fix.diff`.

## Global Constraints

- Never push. Work only in the worktree named in the dispatch (`/home/thier/iwd2-re-worktrees/p3-frida-trace`, branch `p3/frida-trace`). Commit messages end with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- `src/` is never modified. `docs/wso/wave-state.json` and `docs/wso/import-state.json` are never modified.
- No desktop windows: never set `IWD2R_DESKTOP_OK`, never pass `--host`. Windows Sandbox only through the tooling; at most 2 launches per attempt, `--new-attempt REASON` with a real reason for each further attempt; the per-task launch budgets below are hard caps.
- Windows builds run on the host through `tools/play.py` (a build, not a game window). Linux builds, tool tests and anything heavy run under `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 --`.
- Oracle read rules: never touch `vanilla.lock`, `replay.lock`, `replays/`, `out/`, `tables/` by hand; never modify the Frida venv at `%USERPROFILE%\iwd2r-frida`.
- Run folders stay under the checkout's gitignored `runs/` or `/home/thier/iwd2r-runs/frida-<task>/`; never committed.
- The fork's tree is untrusted input: `git show e8dd16a750966af53f24daaf49dcb4fb13594c13:<path>` only.
- Tool tests: `python3 -I tests/test_tools.py` must pass (696 at the base). Do not run `tools/gates.sh` (the controller gates at merge).
- Another driver may be fast-forwarding `first` and working in `p3-wso-b0NN` worktrees at any time: never touch those, never touch `first`'s checkout.
- Exact values: scenarios `world_entry`, `first_fight`, `walk_and_talk`; x86 preset `windows-msvc-x86`, exe `build/windows-msvc-x86/RelWithDebInfo/iwd2r.exe`, map beside it; loaded symbol `?EngineActivated@CScreenWorld@@UAEXXZ`; MoveToPoint symbol `?MoveToPoint@CGameSprite@@QAEFXZ` (0x1A8A90 in the 2026-10-23 map; original 0x73FEC0); sidecar deadline = scenario timeout + 60 s; `--post-load` default = the scenario's `# timeout: S` header, else 180.

## Review Focus

- The in-box wait for `RESULT:` must have a deadline: a crashed sidecar must still produce `done.txt` and the shutdown within timeout + 60 s. Test in Task 1.
- The original side starts its sidecar with `start` (background); the wait must poll the sidecar's own status path, not the recorder's. Test in Task 1.
- The evidence rewrite must change only front-matter lines (`kind:`, `trace_source:`, `exemption:`) and leave the body, `## WSO delta` and every other line byte-identical. Test in Task 2.
- A parked import row must be reverted exactly: `check_evidence.touched(worktree, merge_base)` must not list it, and its evidence file must be gone. Test in Task 2.
- Calibration counts a scenario as measured only when both sides exited 0 and the compare ran; an exit-2 side is `not measured`. Test in Task 3.

---

### Task 1: the sandbox run completes with a verdict; x86 build; the MoveToPoint pair

**Files:**
- Modify: `tools/frida/probe.py` (`ours_logon`, `original_logon`, the `run` subparser, `run_side`)
- Modify: `tools/frida/orig.py` (move `import frida` out of module scope so `build_parser()` imports on Linux)
- Modify: `tools/play.py` (`build_windows_x86`)
- Modify: `tools/gates.sh` (`--windows` builds the x86 preset through play)
- Modify: `docs/frida-tracing.md` (x86 build, RVAs, `--post-load`, the wait)
- Test: `tests/test_tools.py` (a new `FridaProbeTest` class)
- Launch budget: 2 launches per side per attempt, at most 6 launches in this task.

**Interfaces:**
- Consumes: `tools/winsandbox.replay_driver_config(game_win, env, timeout)` (key `shutdown`), `tools/frida/probe.py` `original_logon(python_win, timeout, args)` / `ours_logon(python_win, game_win, env, timeout, args)` (return the cmd string), `truth.parse_map`.
- Produces: `orig.build_parser() -> argparse.ArgumentParser` (no frida import needed); `probe.sidecar_argv(target, python_win, env, timeout, args) -> list[str]` (the sidecar command as argv, `--env K=V` one pair per flag) and `probe.wait_for_result_cmd(status_win, deadline_s) -> str` (a `powershell -NoProfile -Command` one-liner that returns when the status file has a line starting `RESULT:` or after `deadline_s`); both logon builders assemble `cmd.exe /c <sidecar> & <wait> & echo done > <results>\done.txt & shutdown /s /t 0` from them; `probe.py run --post-load SECONDS` (default: the scenario's `# timeout:` header, else 180) forwarded to the sidecar; `play.build_windows_x86(bat_name='play-build-x86.bat', build=build_windows) -> str` calling `build(preset='windows-msvc-x86', target='iwd2r', exe_rel='build/windows-msvc-x86/RelWithDebInfo/iwd2r.exe', bat_name=bat_name)`.

- [ ] **Step 1: Write the failing tests** in a new `FridaProbeTest`: `test_sidecar_argv_parses_with_orig_parser` (both targets; `orig.build_parser().parse_args(argv[1:])` succeeds and `env` round-trips as a list of `K=V`), `test_logon_waits_for_result_before_done` (for both builders the cmd string has the wait segment before `done.txt`, the wait names the sidecar's status path, and the deadline equals timeout + 60), `test_post_load_default_is_scenario_timeout` (a scenario file with `# timeout: 240` → `--post-load 240` in the argv; `--post-load 30` given → 30; no header → 180), `test_build_windows_x86_uses_x86_preset` (inject `build=` and assert its kwargs).
- [ ] **Step 2: Run them**: `python3 -I tests/test_tools.py FridaProbeTest` — expected: fail with AttributeError/AssertionError.
- [ ] **Step 3: Implement** `orig.build_parser` (frida imported inside the functions that need it), `probe.sidecar_argv`, `probe.wait_for_result_cmd`, the two logon builders on top of them, `--post-load`, `play.build_windows_x86`, and the `gates.sh --windows` step (`play.sync_windows(); play.build_windows_x86()`).
- [ ] **Step 4: Run the suite**: `python3 -I tests/test_tools.py` — expected OK, 700 tests.
- [ ] **Step 5: Commit** `frida: the sandbox waits for the sidecar's verdict; --post-load; x86 build`.
- [ ] **Step 6: The pair (acceptance).** Build x86 from this worktree (`play.sync_windows(); play.build_windows_x86()` with the worktree's Windows checkout; see `play.win_checkout`), resolve the two RVAs from the map with `truth.parse_map`, write the two MoveToPoint tables under `/home/thier/iwd2r-runs/frida-task1/`, run `probe.py run` for the original and for ours (`--scenario world_entry --seed 1 --clicks 3 --hit MTP --hit-min 1`, ours with `--loaded-rva`), then `trace_diff.py compare`. Record in the report: per side the command, wall time, exit code, `RESULT:` line, MTP hit count; the compare exit code and summary. If ours still ends `NOT-EXERCISED`, spend at most one more launch on ours with the trace's `sites`/`loaded` lines and the scenario's click timing in hand, and report the diagnosis (whether the clicks run before the window closes, whether `RunScenario` clicks reach `CGameSprite::MoveToPoint` in our build). A pair that reaches `compare=0` is the acceptance; a diagnosed failure is DONE_WITH_CONCERNS, and Task 3 then waits.
- [ ] **Step 7: Update `docs/frida-tracing.md`** (x86 build command, the wait, `--post-load`, the current RVAs with the map date) and commit `docs: frida-tracing reflects the x86 build and the sidecar wait`.

### Task 2: `frida_run` drives probe.py like a hand run; the driver phase runs for imports; measured rows get trace evidence; unmeasured import rows are parked

**Files:**
- Modify: `tools/truth.py` (`frida_run`, `frida_batch`)
- Modify: `tools/driver.py` (`_frida_phase`, the import mechanical checks, `_write_trace_evidence`, the parked list)
- Modify: `tools/wave.py` (`close --kind import` and the parked list)
- Modify: `docs/wso/import-brief-template.md` (the fix-round rule for unmeasured rows), `docs/wso/README.md` (driver section)
- Create: `docs/wso/import-parked.json` (`[]`, committed)
- Test: `tests/test_tools.py`

**Interfaces:**
- Consumes: Task 1's `--post-load`; `truth.hook_table`, `truth.frida_verdicts`, `truth.trace_source_line(probe, orig_path, ours_path)` (exists; the line `check_evidence` accepts), `wave.IMPORT_KINDS`, `check_evidence.touched(worktree, base)`, `driver.ALLOWED_PATHS`.
- Produces: `truth.frida_run(batch_id, table, scenarios, run=subprocess.run, root=ROOT, loaded_rva=None, clicks=3, hit_min=1, timeout=None)` whose per-scenario dict gains `orig_exit`, `ours_exit` and `result` (`{'orig': 'CLEAN'|'NOT-EXERCISED'|…, 'ours': …}` from the `.status` files) and whose argv per side carries `--clicks N --hit-min 1 --timeout T --new-attempt "driver frida <batch> <scenario>"` plus `--loaded-rva` for ours (no `--hit`: any hook counts); `driver._write_trace_evidence(path, probe, orig_path, ours_path) -> None` (rewrites `kind:` to `trace`, sets `trace_source:` to `truth.trace_source_line(...)`, deletes `exemption:`; every other line unchanged); `docs/wso/import-parked.json` entries `{"address", "name", "file", "batch", "needs"}`; the import check message for an unmeasured row, exactly: `<addr>: exercised by no scenario (world_entry, first_fight, walk_and_talk); revert its body to the merge base, delete docs/evidence/<ADDR>.md and append it to docs/wso/import-parked.json`; `wave.close_batch(..., kind='import')` refuses when a batch row is neither `trace`-or-better nor listed in the parked file, and when a parked row still has a touched body.

- [ ] **Step 1: Write the failing tests**: `test_frida_run_argv_per_side` (fake `run`; asserts the flags above for each scenario and side and `--loaded-rva` only for ours), `test_frida_run_marks_exit2_side_not_measured`, `test_driver_frida_phase_runs_for_import_kind` (fake probe results: one row equal → evidence rewritten; one different → finding text; one not exercised → the exact problem message), `test_write_trace_evidence_touches_only_front_matter` (byte compare of every other line), `test_import_close_accepts_parked_rows`, `test_import_close_refuses_unparked_dropped_row`, `test_import_close_refuses_parked_row_with_body`.
- [ ] **Step 2: Run them** — expected fail.
- [ ] **Step 3: Implement** in the order truth → driver → wave; the parked list is appended by the worker in its fix round (the brief says so) and read by the close.
- [ ] **Step 4: Run the suite** — expected OK.
- [ ] **Step 5: Document** the phase and the parked list in `docs/wso/README.md` and the fix-round rule in `docs/wso/import-brief-template.md`; **commit** `driver: the Frida phase runs for imports; measured rows get trace evidence; unmeasured rows are parked`.

### Task 3: the real calibration

**Files:**
- Modify: `tools/truth.py` (`calibrate` runs the Frida path: build x86 from this worktree, hook tables for the 28 rows, `frida_run` over the three scenarios, results into the record; the "deferred" branch stays only for `--no-frida`)
- Modify: `docs/wso/truth-calibration.json` (written by the run), `docs/wso/xrec-report.md` (regenerated by `wave.py scorecard`)
- Test: `tests/test_tools.py`
- Launch budget: 6 launches (3 scenarios × 2 sides) plus at most 2 retries.

**Interfaces:**
- Consumes: Task 2's `frida_run`; `truth.calibrate(state, rows, results, seed, sample)`.
- Produces: `truth.py calibrate [--no-frida] [--frida-map MAP]` running the path; the record's `paths.frida` with `measured`, `agree`, `disagree`, `of`, no `deferred` key when it ran; `frida_default` per the rule (measured × 5 ≥ sample size).

- [ ] **Step 1: Test** `test_calibrate_runs_frida_with_injected_runner` (injected runner returning equal for 3 of 10 sample rows → `measured 3`, `frida_default True`; exit-2 scenarios not counted).
- [ ] **Step 2: Implement** the wiring.
- [ ] **Step 3: Run it for real** under the launch budget; commit the calibration file and the regenerated report: `truth: calibration run on 2026-10-23 (frida measured N/28)`. Report the numbers and every side that proved nothing, with its `RESULT:` line.

### Task 4: the evidence rules, the driver base, the triage regen, the import brief

**Files:**
- Modify: `tools/check_evidence.py` (`TRACE_SOURCES[0]`, `fork_note_mentions`), `tools/driver.py` (allowlist base), `tools/triage.py` (`main` regen keeps closed entries), `docs/wso/import-brief-template.md` (rules 1, 5, dependencies, refusals), `docs/wso/README.md`
- Test: `tests/test_tools.py`

**Interfaces:**
- Produces: `check_evidence.fork_note_mentions(sha, path, address, name, run=subprocess.run) -> (bool, reason)` using `git show <sha>:<path>`; `TRACE_SOURCES[0]` matches only `WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md`; the check's failure text names the note and what it lacked; the driver's path check uses `git merge-base first <branch>` as its base; `triage.py` (regen) keeps every entry whose address has a verdict in the state and classifies the rest at `--ours-ref`.

- [ ] **Step 1: Tests**: `test_trace_source_requires_recovery_note_path` (a `src/...cpp` path fails the regex), `test_fork_note_must_mention_function` (fixture repo: a note with the address passes, with the short name passes, without either fails, missing note fails), `test_driver_path_check_uses_merge_base` (a commit from `first` merged into the batch is not "outside"), `test_triage_regen_keeps_closed_entries`.
- [ ] **Step 2: Run them** — expected fail. **Step 3: Implement. Step 4: Suite OK.**
- [ ] **Step 5: Rewrite the import brief rules**: rule 1 drops "nothing to adjudicate" and adds "check his body against the disassembly; refuse what the exe contradicts, and record the refusal in the report and in docs/wso/import-parked.json with needs 'fork body contradicts the exe: …'"; a new rule: "a stub dependency inside your files is imported with the row, with its own trailer; a dependency outside your files parks the row with that reason"; the fix-round rule from Task 2 stays. **Commit** `evidence: a trace cites a recovery note that names the function; driver diffs against the merge base; triage keeps closed rows; import brief tells the truth`.
