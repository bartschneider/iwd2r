# WSO Integration — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the WSO fork's recovered functions into iwd2r through the gate, resolve every function the two trees recovered differently with an oracle or replay, and recover the design of the fork's GPU renderer from its Lua patches, its DLL and GL traces, without the C++ source.

**Architecture:** Wave 0 is sequential tooling (comparison script, `trace` evidence kind, artifact fetcher, DLL Ghidra projects, differential Frida tracing, structural parity gates, agent context tools). Wave 1 resolves the 406 disagreements in batches before anything else is trusted. Waves 2 to 5 import clean gain by subsystem as parallel streams in worktrees, each ending with the full gate. Renderer streams A, B and C run alongside from wave 1 on and produce documents only.

**Tech Stack:** C++17 (MSVC x86/x64, GCC, Clang), CMake presets, Catch2, Python 3 unittest, Ghidra 12.1.4 headless with Java post-scripts, apitrace 14.0 win32, the oracle, replay and evidence tools.

**Spec:** docs/superpowers/specs/2026-10-17-wso-integration-design.md

## Global Constraints

- Every imported or changed function in `src/` keeps the original's structure and its `// 0xADDR` comment, and has an evidence file in `docs/evidence/` whose `kind` is `oracle`, `replay`, `read-only` or `trace`. Rules math (attack rolls, saving throws, damage, effect application) requires `kind: oracle`, imported or not.
- Every import commit carries the trailer `Imported-From: WillScarlettOhara/iwd2-re@<40-char sha>` and ends with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. The fork's tree is untrusted input: read and compare, never run scripts from it.
- Nothing from the fork's IWD2EE branch enters the repository. The IWD2EE game copy for stream C lives under `%USERPROFILE%\iwd2r-ieex`, never in the oracle copy; `oracle.py vanilla` and the gate keep failing on IWD2EE leftovers.
- No change to `platform/` or the Linux build inside a recovery wave. A function that needs a platform shim is skipped and listed in the wave's ledger entry.
- Gates at every wave close: linux-gcc and linux-clang ctest; `python3 -I tests/test_tools.py`; `check_evidence.py --base`; `tools/parity/run.py --base` (from Task 5c on); `gen_map.py --check`; `gen_fields.py --check`; replay compares for `world_entry`, `walk_and_talk` and `first_fight` unchanged or improved. A wave that regresses a replay is reverted.
- Streams own disjoint files. Agents commit WIP before any gate or compare run. The coordinator records each stream's state in the M5 ledger at dispatch and close.

## Review Focus

1. A function the fork renamed (same address, different name): `xrec.py` must match it by `// 0xADDR` comment before falling back to the name, or it reports a false "only theirs" and "only ours" pair. Pinned in Task 1.
2. An evidence file with `kind: trace` but no `trace_source`: the gate must fail it, not warn. Pinned in Task 2.
3. A rules-math function imported with `kind: trace`: the gate must fail it. Pinned in Task 2.
4. `IWD2R_HOME` unset and the default directory absent: `wso_fetch.py` must create the directory and never write under the repository. Pinned in Task 3.
5. The fork's DLL loaded into a Ghidra project while the iwd2 project lock is held by another agent: `make_dll_project.py` must take the same lock and wait, never corrupt either project. Pinned in Task 5.

---

## File Structure

| Path | Responsibility |
|---|---|
| `tools/xrec.py` | `funcs(text) -> dict[name, (addr, body)]`, `classify(theirs, ours, base) -> Counter`, `report(base, theirs, ours, out_json)`; CLI prints the spec's table and writes the ranked disagreement list |
| `tools/check_evidence.py` | `KINDS` gains `trace`; `trace_source` required with it; rules-math rule unchanged |
| `tools/wso_fetch.py` + `tools/wso_pins.json` | downloads the pinned DLLs and Lua files into `$IWD2R_HOME/wso`, verifies sha256; `--check` only verifies |
| `tools/ghidra/make_dll_project.py` | `make(name, dll_path)` creates `$IWD2R_HOME/ghidra-projects/<name>` under the shared Ghidra lock |
| `tools/ghidra/DllScope.java`, `tools/ghidra/DllDelta.java`, `tools/ghidra/DecompileFunctions.java` | scope line; functions in one program absent from another; decompile named functions to files |
| `tools/frida/{probe,hooks,orig,trace_diff}.py` | differential tracing of the original and our build on one scenario; `trace_diff.compare` is the verdict |
| `tools/parity/*.py`, `tools/ghidra/ExportCallGraph.java` | structural parity of changed functions with the binary: callee sets, vtables, layouts, member offsets, address markers, RE lints; a gate step |
| `tools/fn_digest.py`, `tools/src_find.py`, `tools/assemble_context.py`, `tools/token_audit.py` | token-cheap agent briefing and transcript cost audit |
| `tools/gltrace.py` | runs apitrace around `IWD2EE.exe` for a scenario, writes the `.trace` under `runs/` and a per-frame summary markdown |
| `tools/recovery_status.py` | byte-weighted `.text` recovery figure from the iwd2 Ghidra project and `src/` |
| `docs/wso/` | xrec report, disagreement list, wave ledgers' public summaries |
| `docs/renderer/wso/` | stream A notes (one per patch file), `dll/<helper>.md` (stream B), `gltrace/<scenario>.md` (stream C) |
| `docs/THIRD_PARTY.md` | entry for the fork |

---

## Wave 0: tooling (sequential, one implementer)

### Task 1: `xrec.py` as a tested module

**Files:** Modify `tools/xrec.py`; Test `tests/test_tools.py` (add `xrec = load_tool('xrec')` next to the other loaders).

**Interfaces:** Produces `funcs(text: str) -> dict[str, tuple[str | None, str]]` mapping function name to `(address from the "// 0xADDR" comment on the signature line or None, normalised body)`; `classify(theirs: dict, ours: dict, base: dict) -> collections.Counter` with the ten keys of the spec's table; `report(base_ref, theirs_ref, ours_ref, out_json) -> Counter` which also writes `[{"file", "name", "address", "category", "their_len", "our_len"}]` sorted by category then length descending.

- [ ] **Step 1: Tests.** `test_xrec_funcs_parses_signature_and_address` (a two-function snippet with `// 0x401000` on one signature; assert names, address, body stripped of whitespace and comments); `test_xrec_matches_renamed_function_by_address` (same address, different names in theirs and ours: classified once, not as only_theirs plus only_ours); `test_xrec_classify_table` (hand-built dicts covering all ten categories, assert each count is 1).
- [ ] **Step 2:** run `python3 -I tests/test_tools.py`, expect the three failures.
- [ ] **Step 3:** refactor `xrec.py` into the three functions; address match first, name second; CLI `python3 tools/xrec.py <base> <theirs> <ours> --json docs/wso/disagreements.json`.
- [ ] **Step 4:** run the tests (pass); run the CLI against `wso/main` and `first`; commit `docs/wso/xrec-report.md` (the printed table) and `docs/wso/disagreements.json`.
- [ ] **Step 5: Commit** "Make the cross-recovery comparison a tested tool and record its first report".

### Task 2: `kind: trace` in the evidence gate

**Files:** Modify `tools/check_evidence.py:44` (`KINDS`) and the field check near line 226; Test `tests/test_tools.py`.

**Interfaces:** An evidence file may carry `kind: trace` only with `trace_source: "WillScarlettOhara/iwd2-re@<sha> <path to recovery note>"`. `check_evidence.py` output gains `trace` in the per-kind split.

- [ ] **Step 1: Tests.** `test_trace_kind_accepted_with_source`, `test_trace_kind_without_source_fails` (an error line naming the file, exit status non-zero), `test_rules_math_rejects_trace` (a rules-math address with `kind: trace` fails with the `RULES_MATH_NOTE`).
- [ ] **Step 2:** run, expect failures.
- [ ] **Step 3:** implement; keep the three existing kinds' behaviour byte-identical.
- [ ] **Step 4:** run the tests and `python3 -I tools/check_evidence.py --base iwd2r/main` (0 touched functions).
- [ ] **Step 5: Commit** "Accept trace-verified evidence from the WSO fork, except for rules math".

### Task 3: the artifact fetcher

**Files:** Create `tools/wso_fetch.py`, `tools/wso_pins.json`; Test `tests/test_tools.py`.

**Interfaces:** `wso_fetch.py` reads `wso_pins.json` (`{"fork_commit", "upstream_commit", "files": [{"url", "dest", "sha256"}]}`), downloads into `${IWD2R_HOME:-~/.local/share/iwd2r}/wso/<dest>`, verifies sha256, refuses to write anywhere else. `--check` verifies existing files only. Pins: the fork at `138495fde69b6092b7318fad462d571864e9d9b8`, upstream IWD2EE at `60de54611b249d3bcca3f6522ef9db5e7ec27339`; files: `IEexHelper.dll` from both, `IEex.dll` from the fork, the 16 modified or added Lua files from the fork, `IEex_Render_Patch.lua` and `IEex_Gui_State.lua` from upstream. The sha256 values come from the copies already staged under `~/.local/share/iwd2r/wso` on Thierry's machine.
- [ ] **Step 1: Tests.** `test_wso_fetch_dest_under_home` (with `IWD2R_HOME` set to a temp dir, destinations resolve under it; a `dest` containing `..` is rejected); `test_wso_fetch_check_reports_mismatch` (a file with the wrong hash is listed, exit non-zero); `test_wso_fetch_creates_dir_when_absent`.
- [ ] **Step 2:** run, expect failures. **Step 3:** implement with `urllib` only. **Step 4:** run the tests; run `python3 tools/wso_fetch.py --check` on the staged copies (all OK).
- [ ] **Step 5: Commit** "Fetch the WSO fork's renderer artifacts by pinned hash".

### Task 4: attribution and the public summary

**Files:** Modify `docs/THIRD_PARTY.md`; Create `docs/wso/README.md`.

- [ ] **Step 1:** THIRD_PARTY gains a row: our files "functions carrying the `Imported-From` trailer", ported from `WillScarlettOhara/iwd2-re`, copyright the fork's author, MIT as granted to Thierry on 2026-10-17 (link to his LICENSE when published).
- [ ] **Step 2:** `docs/wso/README.md`: what the fork is, the numbers from the spec's table, the trailer format, the `trace` kind, and the rule that nothing from the IWD2EE branch enters the repo.
- [ ] **Step 3: Commit** "Credit the WSO fork and document the import rules".

### Task 5: DLL Ghidra projects and scripts

**Files:** Create `tools/ghidra/make_dll_project.py`, `tools/ghidra/DllScope.java` (from `~/.local/share/iwd2r/wso/ghidra/`), `tools/ghidra/DllDelta.java`, `tools/ghidra/DecompileFunctions.java`; Test `tests/test_tools.py`.

**Interfaces:** `make_dll_project.make(name: str, dll: str) -> str` returns the project dir `$IWD2R_HOME/ghidra-projects/<name>`, holds `make_project`'s lock while headless runs, is a no-op when the `.gpr` exists. `DllDelta.java` args: `<other .gpr path> <other program name> <out.tsv>`; writes `address\tname\tsize\tstrings` for every function of the current program with no counterpart in the other, where counterpart means same set of referenced string literals and imported calls, size within 25%. `DecompileFunctions.java` args: `<out dir> <name or address>...`; one `<name>.c` per function.
- [ ] **Step 1: Tests.** `test_make_dll_project_paths_and_noop` (project dir under a temp `IWD2R_HOME`; second call does not invoke headless, patched); `test_make_dll_project_holds_lock` (patch `filelock.hold`, assert it is entered with the iwd2 lock path).
- [ ] **Step 2:** run, expect failures. **Step 3:** implement; the Java scripts are compiled by Ghidra at run time, keep them under 150 lines each.
- [ ] **Step 4:** run the tests; `python3 tools/ghidra/make_dll_project.py ieexhelper-fork $IWD2R_HOME/wso/dll/IEexHelper.fork.dll` and the same for `ieexhelper-upstream`; run `DllScope.java` on both and record the two lines in `docs/renderer/wso/scope.md` (expected: 1585/353902 and 1298/236197); run `DllDelta.java` fork against upstream, commit the TSV's row count in the same note.
- [ ] **Step 5: Commit** "Ghidra projects and delta listing for the IEexHelper DLLs".

### Task 5b: differential Frida tracing, adopted from the fork

**Files:** Create `tools/frida/probe.py`, `tools/frida/hooks.py`, `tools/frida/orig.py`, `tools/frida/trace_diff.py` (adapted from `wso/main:scripts/frida_probe.py`, `frida_hooks.py`, `frida_orig.py`, `frida_trace_diff.py`; read them with `git show wso/main:<path>`, never run them from the fork's tree), `docs/frida-tracing.md` (from his `docs/frida-trace-comparison.md` and `docs/frida-differential-tracing.md`, rewritten for our paths); Test `tests/test_tools.py` (`trace_diff = load_tool('frida/trace_diff')` or an equivalent loader for the subpackage).

**Interfaces:** `trace_diff.normalize(events: Iterable[dict]) -> list[dict]` strips harness events, timestamps, thread and process ids and backtraces, and maps pointer fields to `<null>` or `<ptr>` unless `raw_pointers=True`. `trace_diff.compare(orig: list[dict], ours: list[dict]) -> int` returns 0 when calls per tag, call order and per-occurrence arguments all match, 1 on any behavioural difference, 2 on malformed or empty input; any `crash`, `exception`, `assert` or `ERROR` event returns non-zero. `probe.py run --target original|ours --probe <hooks.json> --scenario <name> --out <file.jsonl>` launches the target through `cmd.exe` with the Windows Python at `%USERPROFILE%\iwd2r-frida\venv\Scripts\python.exe` (resolved via `winhome.py`), the original from the oracle copy, ours from `play.win_checkout`'s `windows-msvc-x86` build, drives the scenario with `realinput.py`, and writes the capture under `runs/<stamp>/`. An evidence file produced this way is `kind: trace` with `trace_source: "frida <probe name> orig:<sha256 of orig.jsonl> ours:<sha256 of ours.jsonl> compare=0"`; the captures stay under `runs/`, never committed.

- [ ] **Step 0 (once, host):** on Windows, `py -3 -m venv %USERPROFILE%\iwd2r-frida\venv` and `pip install frida-tools` in it. Frida attaches to a 32-bit process from a 64-bit Python without special handling. Record the frida version in `docs/frida-tracing.md`.
- [ ] **Step 1: Tests.** `test_trace_diff_normalize_drops_harness_and_pointers` (five events incl. a `ready` and a `this: 0x1234`; assert the remaining events and `<ptr>`); `test_trace_diff_compare_match_order_args` (two identical captures: 0; same calls, different order: 1; one argument differs: 1); `test_trace_diff_crash_never_passes` (a capture with an `assert` event against itself: non-zero); `test_trace_diff_empty_is_2`.
- [ ] **Step 2:** run, expect failures.
- [ ] **Step 3:** adapt the four scripts; keep his hook-table JSON format unchanged so his probes work as-is; route every Windows path through `winhome.py`.
- [ ] **Step 4:** run the tests (pass). Smoke on the host: a probe on `CGameSprite::MoveToPoint` (0x73F706 region, from his pathfinding notes) over `world_entry`, original versus ours, `compare` exit 0; put the command and the exit code in `docs/frida-tracing.md`.
- [ ] **Step 5: Commit** "Adopt differential Frida tracing from the WSO fork".

### Task 5c: structural parity gates, adopted from the fork

**Files:** Create `tools/parity/asm_verify.py`, `vtable_audit.py`, `ctor_vtable_check.py`, `struct_layout_audit.py`, `parity_offsets.py`, `address_markers.py`, `re_lint.py` and the six `lint_*.py` (adapted from `wso/main:scripts/reagent_asm_verify.py`, `vtable_audit.py`, `ctor_vtable_check.py`, `struct_layout_audit.py`, `parity_offsets.py`, `lint_address_markers.py`, `re_lint.py`, `lint_*.py`; read with `git show wso/main:<path>`); Create `tools/parity/run.py`, `tools/parity/requirements.txt` (pinned `pefile`, `capstone`), `tools/parity/setup.sh` (venv at `$IWD2R_HOME/parity-venv`, like the glslang pin); Create `tools/ghidra/ExportCallGraph.java`; Modify `tools/gates.sh` (new step after "decompiler evidence"); Test `tests/test_tools.py`.

**Interfaces:** `ExportCallGraph.java` writes `$IWD2R_HOME/ghidra-exports/functions.tsv` (`address\tname\tsize`) and `callgraph.tsv` (`address\tcallee_address...`) from the iwd2 project, under `make_project`'s lock. `parity.run.main(base_ref: str, changed_only: bool = True) -> int` finds the functions changed since `base_ref` by their `// 0xADDR` comments, runs every check on them, prints one table (`address | name | asm | vtable | layout | offsets | markers | lints`), and returns 1 on any RED. `asm_verify.callee_diff(src_callees: set[int], bin_callees: set[int]) -> tuple[set[int], set[int]]` returns (missing in src, extra in src). An evidence file may carry `parity_waiver: "<reason>"` to turn an `asm` RED into a warning (the fork's "a human confirms RED"); the other checks have no waiver. `gates.sh` runs `python3 tools/parity/run.py --base "$BASE"` only when `oracle.VANILLA_EXE` exists and the venv is present; otherwise it prints `structural parity: skipped (no vanilla exe)` and continues, so CI, which has no game, is unaffected.

- [ ] **Step 1: Tests.** `test_parity_callee_diff` (src {1,2,3}, bin {2,3,4}: missing {4}, extra {1}); `test_parity_address_markers_flags_non_start` (a marker 2 bytes into a function in a fixture `functions.tsv` is RED); `test_parity_run_skips_without_exe` (patch `oracle.VANILLA_EXE` to a missing path: exit 0, output contains "skipped"); `test_parity_waiver_downgrades_asm` (evidence with `parity_waiver` makes an asm RED a warning, exit 0); `test_parity_changed_only_uses_markers` (a diff touching two functions yields exactly their two addresses).
- [ ] **Step 2:** run, expect failures.
- [ ] **Step 3:** adapt the scripts; replace every `.ghidra-exports` and `address_map.json` read with `functions.tsv`/`callgraph.tsv`; replace `IWD2.exe` paths with `oracle.VANILLA_EXE`; keep the checks' logic byte-for-byte where it does not touch paths.
- [ ] **Step 4:** run the tests (pass); `ExportCallGraph.java` once on the iwd2 project; `python3 tools/parity/run.py --base iwd2r/main --changed-only` on `first` (expect 0 changed functions, exit 0); then on a recent recovery commit's range to see a real table in the commit message.
- [ ] **Step 5: Commit** "Gate recovered functions on call-graph, vtable and layout parity with the binary".

### Task 5d: agent context and token tools, adopted from the fork

**Files:** Create `tools/fn_digest.py`, `tools/src_find.py`, `tools/assemble_context.py`, `tools/token_audit.py` (from `wso/main:scripts/fn_digest.py`, `src_find.py`, `reagent_assemble_context.py`, `token_audit.py`); Modify `docs/wso/README.md` (a "briefing agents" section); Test `tests/test_tools.py`.

**Interfaces:** `fn_digest.digest(decompile_text: str, callgraph: dict) -> str` returns at most 30 lines: signature, callees by name, strings referenced, globals touched, loop and switch count. `src_find.find(query: str) -> list[tuple[str, int, str]]` resolves `Class::Method`, `Class::` and `0xADDR` to `(file, line, signature)` using the `// 0xADDR` markers and function signatures, no grep chains. `assemble_context.bundle(address: int) -> str` concatenates the digest, the full decompile, the callees' signatures from `src_find`, and the resolved IDS constants and resrefs, in that order, with a token estimate on the first line. `token_audit.summarize(transcript_jsonl: str) -> dict` returns tokens per tool name and the ten largest single tool results.

- [ ] **Step 1: Tests.** `test_fn_digest_under_30_lines` (a 400-line fixture decompile), `test_src_find_by_address_and_name` (two fixture files), `test_assemble_context_order_and_estimate`, `test_token_audit_totals` (a 20-line fixture transcript with known usage fields).
- [ ] **Step 2–4:** fail, implement, pass. Run `token_audit.py` on the coordinator session transcript and put its top three hotspots in `docs/wso/README.md`.
- [ ] **Step 5: Commit** "Give agents digests, a symbol index and a context bundle instead of raw decompiles".

Process change from this task on: every implementer brief names the function by `assemble_context` bundle, not by pasting the decompile; reviewers start from the digest.

## Wave 1: disagreements (parallel batches of 20, one stream per batch)

### Task 6 (template, instantiated per batch): resolve a disagreement batch

**Files:** `src/<the batch's files>`, `docs/evidence/<addr>.md` per function, `oracle/cases/` or `scenarios/` as needed. The coordinator assigns batches from `docs/wso/disagreements.json` ranked by `their_len + our_len`, disjoint files per concurrent batch.

- [ ] **Step 1:** for each function, read both bodies and the Ghidra decompile; write the oracle case (pure function), a Frida probe (Task 5b; any function reachable in a scenario), or the replay checkpoint that distinguishes them. Run it against the original.
- [ ] **Step 2:** keep the body the case agrees with. If it is the fork's, apply it with the trailer. If neither matches, port from the decompile and note both errors.
- [ ] **Step 3:** evidence file: `kind: oracle` or `replay`, one line "disagreed with WSO at <sha>: <what differed>; case <id> decides". Where the fork and ours agree after all (parser noise), add "corroborated by WSO at <sha>" and leave the kind.
- [ ] **Step 4:** gate; **Step 5: Commit** per batch "Resolve WSO disagreement batch N: <subsystem>".

## Waves 2 to 5: clean-gain imports (parallel streams, one per subsystem)

### Task 7 (template): import a subsystem

**Files:** the subsystem's `src/` files from `docs/wso/disagreements.json` categories `theirs_changed_ours_base`, `theirs_real_ours_stub`, `only_theirs`, and for wave 5 the 14 files only the fork has. Wave 2: `CProjectile*.cpp`, effect lifecycle files. Wave 3: `CScreenStore.cpp`, `CScreenInventory.cpp`, `CScreenCharacter.cpp`, `CScreenCreateChar.cpp`. Wave 4: `CGameAIBase.cpp`, `CMessage.cpp`, `CNetwork.cpp`, `CGameArea.cpp`. Wave 5: the rest.

- [ ] **Step 1:** for a file only the fork changed: `git checkout wso/main -- <file>`; otherwise copy function bodies one by one. Keep our `// 0xADDR` comments; add his where missing.
- [ ] **Step 2:** per imported function, an evidence file with `kind: trace` and `trace_source` naming his `docs/*-recovery.md` note when one covers it; `kind: read-only` with "checked against the disassembly" when none does; `kind: oracle` for rules math (write the case).
- [ ] **Step 3:** build both Linux compilers; a function that needs a platform shim is reverted and listed in the ledger entry.
- [ ] **Step 4:** gate including replays; **Step 5: Commit** per file group with the trailer: "Import <subsystem> from the WSO fork".

## Wave 6: status

### Task 8: `tools/recovery_status.py`

**Files:** Create `tools/recovery_status.py`; Test `tests/test_tools.py`; Modify `README.md` (a status line).

**Interfaces:** `recovered_bytes(functions: list[tuple[int, int]], recovered: set[int]) -> tuple[int, int]` returns (recovered bytes, total bytes) where `functions` is `(address, size)` from the iwd2 Ghidra project (via `QuerySymbol.java` or a new query) excluding `Unwind@`/`Catch@` funclets and thunks, and `recovered` is the set of addresses that have an evidence file or a non-stub body in `src/` with a `// 0xADDR` comment.
- [ ] **Step 1: Tests.** `test_recovery_status_bytes` (three functions, two recovered, assert the pair); `test_recovery_status_excludes_funclets`.
- [ ] **Step 2–4:** fail, implement, pass; run it and put the figure in README next to the fork's 78.9% with the date.
- [ ] **Step 5: Commit** "Track byte-weighted recovery".

## Renderer streams (from wave 1 on; documents only; disjoint from the waves)

### Task A1: Lua to design notes

**Files:** Create `docs/renderer/wso/<patch-file-stem>.md` for each of the 16 Lua files in `$IWD2R_HOME/wso/lua/` (compare against the two `.upstream.lua` copies for the modified ones).

- [ ] **Step 1:** per file: intent (from its comments), table of hooks `address | engine function | iwd2r file:line (via git grep "// 0x<ADDR>") | helper called | what changes`, data flow in prose, and the list of `IEex_Helper_*` entry points it needs from the DLL.
- [ ] **Step 2:** `docs/renderer/wso/index.md` grouping the notes by feature: tile cache and atlas, FBO present, fog, HD sprites, UI scale, zoom, fonts, Bink, pathfinding, audio, fixes.
- [ ] **Step 3: Commit** per note "Renderer notes: <feature>".

### Task B1: DLL delta notes

**Files:** Create `docs/renderer/wso/dll/<helper>.md` for every function in the Task 5 delta TSV that any Lua note names, then the remaining delta functions reachable from them.

- [ ] **Step 1:** `DecompileFunctions.java` on the batch into `runs/dll-decomp/` (ignored), one agent per 30 functions.
- [ ] **Step 2:** per helper: the decompiled C (cleaned, named), then a prose summary: inputs, GL calls made through resolved procs, buffers and sizes, threads and synchronisation (the "Present Thread" strings name the states), failure fallbacks (the "disabled" strings).
- [ ] **Step 3: Commit** per batch "Renderer notes: IEexHelper <feature> helpers".

### Task C1: GL traces

**Files:** Create `tools/gltrace.py`; Test `tests/test_tools.py`; Create `docs/renderer/wso/gltrace/<scenario>.md`.

**Interfaces:** `gltrace.py <scenario> --game %USERPROFILE%\iwd2r-ieex\game --apitrace %USERPROFILE%\iwd2r-apitrace` runs `apitrace trace --api gl IWD2EE.exe` through `cmd.exe` the way `play.py` launches the Windows build, drives the scenario with `realinput.py`, stops after the scenario's last checkpoint, writes `runs/<stamp>/<scenario>.trace`, then `apitrace dump` piped into `summarize(lines: Iterable[str]) -> dict` producing per-frame counts of draw calls, texture uploads with size and format, FBO binds, and the distinct texture dimensions seen.
- [ ] **Step 0 (once, by Thierry or a host agent):** install IWD2EE from the fork's `iwd2ee-installer-windows-x86_64.exe` into a copy of the GOG install at `%USERPROFILE%\iwd2r-ieex\game`; unpack apitrace 14.0 win32 to `%USERPROFILE%\iwd2r-apitrace`. Never touch the oracle copy.
- [ ] **Step 1: Tests.** `test_gltrace_summarize_counts` on a 12-line fake dump (two frames, one glTexImage2D 2048x2048 RGBA, one glBindFramebuffer, three draws; assert the dict).
- [ ] **Step 2–4:** fail, implement, pass; capture `world_entry` and `first_fight`; commit the two summaries (never the `.trace` files).
- [ ] **Step 5: Commit** "Capture the WSO renderer's GL stream per scenario".
