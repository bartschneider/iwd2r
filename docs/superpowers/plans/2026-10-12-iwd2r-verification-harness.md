# iwd2r Verification Harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compare our engine's functions and screens against the original IWD2 executable, and record decompiler evidence for every function we finish.

**Architecture:** Ghidra (headless) plus REA as an MCP server produce evidence. A 32-bit Windows runner maps the vanilla exe at `0x400000`, lets the exe's own C runtime initialise, then calls original functions and ours side by side. A recorder DLL injected into the vanilla game drives scenario input and dumps state at checkpoints; our engine dumps the same schema.

**Tech Stack:** C++17 (MSVC x86 for the oracle), Python 3 standard library for tooling, Ghidra (user-local) on OpenJDK 21 (apt), Node.js 22 LTS and REA (user-local), the existing CMake presets and Catch2.

**Spec:** `docs/superpowers/specs/2026-10-12-iwd2r-verification-harness-design.md` (section numbers below refer to it).

## Global Constraints

- The vanilla exe is `<install>/iwd2ee/backup/1000/iwd2.exe`, MD5 `200f038bec3ed50a964d68b3f0f1f0e7`, image base `0x400000`, image size `0x621000`, `.data` at `0x8A5000`. The loader refuses any other MD5.
- The real install `/mnt/c/Games/Icewind Dale II` is never modified. Oracle runs use the vanilla copy at `C:\Users\thier\iwd2r-oracle\game` (Task 7).
- Oracle code is Windows x86 only, built with the `windows-msvc-x86` preset through the same cmd.exe route `tools/play.py` uses (`/mnt/c/Users/thier/iwd2r-build`). Data-free parts (parsers, schema, rand, checkers) also build and test on Linux and in CI.
- Tools: OpenJDK 21 from apt (the user installs `openjdk-21-jdk-headless unzip`); Ghidra (latest stable release at execution time, version and SHA-256 recorded in `tools/ghidra/versions.json`) and Node.js 22 LTS install user-locally under `~/.local/share/iwd2r/` (Ubuntu 24.04's nodejs is v18, too old for REA).
- Header lines: new C++ `// iwd2r. Distributed under LICENSE.md.`; new Python, shell and PowerShell `# iwd2r. Distributed under LICENSE.md.`; modified upstream files keep `// Modified for iwd2r; see NOTICE.md` once on line 1; never remove `// 0x...` comments.
- `compat/`, `platform/`, `oracle/`, `tests/` build with `-Wall -Wextra -Werror` (MSVC `/W4 /WX`). `tools/check_notices.py`, `tools/gen_map.py --check` (regenerate `docs/MAP.md` when counts change) and the clang-format 18 check stay green.
- Commits: one-line imperative subjects ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No pushes from tasks.

## Review Focus

1. A second process or security software already holding part of `0x400000`-`0xA21000`: the runner must exit with a message naming the occupying module, never crash. Pinned in Task 5.
2. A Ghidra-derived address in `vanilla.json` that is wrong (for example the `WinMain` call site): the loader must verify the bytes it patches (an `E8` call whose target is `WinMain`'s recorded address) and refuse otherwise. Pinned in Task 5.
3. An oracle case whose original function crashes (access violation in original code): the runner must catch it (structured exception handler around each call), report the case as crashed with inputs, and continue with the next case. Pinned in Task 6.
4. The WeiDU uninstall leaving a partially vanilla copy (interrupted run): `oracle.py vanilla` must detect it on the next run through the verification step and rebuild from scratch. Pinned in Task 7.
5. The recorder's virtual clock making the original game hang (a wait on real time that never advances): every replay has a wall-clock timeout that kills the game and reports the last frame reached. Pinned in Task 12.

---

## File Structure

| Path | Responsibility |
|---|---|
| `compat/msvc_rand.h/.cpp` | VC6 `rand`/`srand` (per-thread state) for engine code |
| `tools/ghidra/setup.sh`, `versions.json` | install JDK, Ghidra, Node, REA user-locally |
| `tools/ghidra/labels.py` | extract labels from our source and IEex's map |
| `tools/ghidra/make_project.py`, `ImportLabels.java` | headless project creation, label import, analysis |
| `tools/oracle.py` | front end: `setup`, `vanilla`, `build`, `run`, `evidence`, `replay record|compare` |
| `tools/check_evidence.py`, `docs/evidence/README.md` | the "done" rule and its workflow |
| `oracle/loader/pe.h/.cpp` | portable PE32 parsing (headers, sections, imports) |
| `oracle/loader/map_win32.cpp`, `vanilla.json` | reserve, map, resolve imports, patch, enter, snapshot (Windows) |
| `oracle/runner/*.h/.cpp` | `OracleMain`, case registry, generators, comparators, SEH guard, reports |
| `oracle/cases/*.cpp` | comparison cases by subsystem |
| `oracle/replay/injector.cpp`, `recorder.cpp`, `schema.json`, `fields.json`, `screens.json` | replay side (Windows x86) |
| `platform/state_dump.h/.cpp` | our engine's dump |
| `oracle/CMakeLists.txt` | targets `iwd2r_oracle_pe` (portable), `iwd2r_oracle` (x86 exe), `iwd2r_injector`, `iwd2r_recorder` |
| `tests/test_msvc_rand.cpp`, `tests/test_pe.cpp`, `tests/test_state_dump.cpp`, `tests/test_tools.py` additions | data-free tests |

---

### Task 1: VC6-compatible rand

**Files:** Create `compat/msvc_rand.h`, `compat/msvc_rand.cpp`, `tests/test_msvc_rand.cpp`. Modify `compat/mfc.h`, `compat/CMakeLists.txt`, `tests/CMakeLists.txt`.

**Interfaces:**
- Produces: `int iwd2r_rand()`, `void iwd2r_srand(unsigned int seed)`, `unsigned int iwd2r_rand_state()` (current thread's state, for dumps); `compat/mfc.h` defines `rand()` as `iwd2r_rand()` and `srand(s)` as `iwd2r_srand(s)` after including `<cstdlib>`, so engine code gets VC6 behaviour and standard headers are unaffected.

- [ ] **Step 1: Write the failing tests** (`[rand]`): after `iwd2r_srand(1)` the first five values are `41, 18467, 6334, 26500, 19169`; after `iwd2r_srand(0)` the first value is `38`; after `iwd2r_srand(42)` the first value is `175`; a thread that never seeds starts as if seeded with 1 (VC6 behaviour); two threads seeded differently produce independent sequences; `iwd2r_rand_state()` after `srand(1)` and one `rand()` equals `1*214013+2531011` truncated to 32 bits.
- [ ] **Step 2: Run** `cmake --build --preset linux-gcc && ctest --preset linux-gcc -R rand`; expect a compile failure.
- [ ] **Step 3: Implement** with `thread_local unsigned int` state initialised to 1; `rand` returns `((state = state * 214013u + 2531011u) >> 16) & 0x7FFF`.
- [ ] **Step 4: Run** the test on both Linux presets; expect pass. Run the full suite and the six harness scenarios headless (`python3 tools/play.py --linux --headless --suite`) to confirm nothing depended on glibc's sequence; regenerate any golden hash the smoke test records.
- [ ] **Step 5: Commit** "Use the VC6 rand sequence in engine code on every platform".

### Task 2: Tool install: Ghidra, Node, REA

**Files:** Create `tools/ghidra/setup.sh`, `tools/ghidra/versions.json`, `.mcp.json`. Modify `.gitignore` if needed.

**Interfaces:** Produces the install root `~/.local/share/iwd2r/` with `ghidra/`, `node/`, `rea/`; `versions.json` fields `ghidra.tag`, `ghidra.url`, `ghidra.sha256`, `node.version`, `node.sha256`, `rea.version`; `.mcp.json` registers REA as server `rea` with `GHIDRA_INSTALL_DIR` pointing into the install root and `JAVA_HOME` at the apt JDK (`/usr/lib/jvm/java-21-openjdk-amd64`). `setup.sh` checks `java -version` reports 21 and stops with the apt command if not.

- [ ] **Step 1:** Resolve the current stable Ghidra release and Node 22 LTS for linux-x64 with their published SHA-256 values (GitHub API / vendor checksum files) and write them to `versions.json`.
- [ ] **Step 2:** Write `setup.sh`: idempotent, downloads into a temp dir, verifies SHA-256 (abort on mismatch), extracts, installs REA with the bundled `npm` into `rea/` following REA's README (`npm install` of the published package at a pinned version), prints the final paths. Read REA's installation docs (`https://github.com/morluto/rea/blob/main/docs/installation.md`) for its Ghidra bridge configuration and follow them.
- [ ] **Step 3: Run** `bash tools/ghidra/setup.sh` twice; the second run reports everything present without downloading. `~/.local/share/iwd2r/ghidra/support/analyzeHeadless -help` and `~/.local/share/iwd2r/node/bin/node --version` succeed.
- [ ] **Step 4:** Write `.mcp.json` per REA's docs. Verify the server starts: run its CLI self-check or `--help` from the install.
- [ ] **Step 5: Commit** "Install Ghidra, Node and REA user-locally".

### Task 3: Ghidra project with imported labels

**Files:** Create `tools/ghidra/labels.py`, `tools/ghidra/make_project.py`, `tools/ghidra/ImportLabels.java`. Modify `tests/test_tools.py`.

**Interfaces:**
- Produces: `labels.py` functions `ours(src_dir) -> list[tuple[int, str]]` (address from each `// 0xADDR` line, name from the definition that follows: `Class::Method`, or the free function name; skip lines not followed by a definition within three lines), `ieex(db_path) -> list[tuple[int, str]]` (skip `FUN_` names), `merge(ours, ieex) -> list[tuple[int, str]]` (ours win; IEex names that collide with an existing name get the prefix `ieex_`); `make_project.py` creates `~/.local/share/iwd2r/ghidra-projects/iwd2/` from the vanilla exe, runs `ImportLabels.java` as a pre-script and full analysis, and is idempotent (re-imports labels on later runs without re-creating).

- [ ] **Step 1: Tests** in `tests/test_tools.py`: on a fixture source tree, `ours` returns `(0x5845D0, "IcewindMisc::Roll")` for a `// 0x5845D0` line followed by `INT IcewindMisc::Roll(INT nRolls, INT nSides)`; skips an address comment followed by a blank line and a `}`; `ieex` skips `FUN_00401000`; `merge` keeps ours on equal addresses and prefixes a colliding IEex name.
- [ ] **Step 2: Run** `python3 -I tests/test_tools.py`; expect failures. **Step 3: Implement.** **Step 4:** tests pass.
- [ ] **Step 5: Run** `python3 tools/ghidra/make_project.py` against the vanilla exe (copy it from the install to the scratch path the script uses; never modify the install). Expect analysis to finish and a summary: label count imported from ours (about 9,000) and from IEex. Verify in a headless script query that `0x5845D0` is named `IcewindMisc::Roll`.
- [ ] **Step 6: Commit** "Create the Ghidra project for the vanilla exe with imported labels".

### Task 4: Evidence workflow and check_evidence

**Files:** Create `tools/check_evidence.py`, `docs/evidence/README.md`, `docs/evidence/5845D0.md` (first real evidence file). Modify `tools/oracle.py` (create it with the `evidence` subcommand), `tests/test_tools.py`, `.github/workflows/ci.yml`.

**Interfaces:**
- Produces: evidence file format (Markdown with a front-matter block `address`, `name`, `oracle_case` or `exemption`, `ghidra_version`); `oracle.py evidence <addr|name>` writes a draft through REA (decompiled pseudo-code, callers, callees) to `docs/evidence/<ADDR>.md` with the front matter filled except `oracle_case`; `check_evidence.py [--base REV]` exits 1 listing each touched function lacking an evidence file, or whose evidence file has neither `oracle_case` nor `exemption`.
- Touched functions: function bodies under `src/` whose lines changed in `git diff <base>...HEAD`, identified by the nearest preceding `// 0xADDR` comment.

- [ ] **Step 1: Tests** in `tests/test_tools.py` with a temp git repo: a change inside a function body with no evidence fails naming the address; adding an evidence file with `exemption: "pure data"` passes; a change outside any function passes; a deleted function is ignored.
- [ ] **Step 2-4:** run (fail), implement, run (pass).
- [ ] **Step 5: Run** `python3 tools/oracle.py evidence IcewindMisc::Roll`; inspect the draft; set `oracle_case: rand/roll` (Task 9 creates it). CI: add `python3 -I tools/check_evidence.py --base origin/main` to the `linux-gcc` job, running only on pull requests (pushes to main compare against the previous commit: use `--base HEAD~1`).
- [ ] **Step 6: Commit** "Add the evidence workflow and its CI check".

### Task 5: PE loader

**Files:** Create `oracle/CMakeLists.txt`, `oracle/loader/pe.h`, `oracle/loader/pe.cpp`, `oracle/loader/map_win32.cpp`, `oracle/loader/vanilla.json`, `tests/test_pe.cpp`, `tests/make_test_pe.py`. Modify root `CMakeLists.txt` (add `oracle/`), `tests/CMakeLists.txt`.

**Interfaces:**
- Produces (portable, `iwd2r_oracle_pe`): `struct PeImage { uint32_t imageBase, imageSize, entryRva; std::vector<PeSection> sections; std::vector<PeImport> imports; }`, `struct PeSection { std::string name; uint32_t rva, virtualSize, rawOffset, rawSize, characteristics; }`, `struct PeImport { std::string dll; std::string name; uint16_t ordinal; uint32_t iatRva; }` (name empty when by ordinal), `bool ParsePe(const std::vector<uint8_t>& file, PeImage* out, std::string* error)`.
- Produces (Windows x86 only): `bool ReserveImageRange(std::string* occupant)` (called from a TLS callback; on failure names the module owning the conflicting region via `VirtualQuery` + `GetModuleFileNameA`), `bool MapImage(const PeImage&, const std::vector<uint8_t>& file, std::string* error)`, `bool ResolveImports(const PeImage&, std::string* error)`, `bool PatchWinMainCall(uint32_t callSite, uint32_t expectedTarget, void* replacement, std::string* error)` (verifies `0xE8` and the rel32 target before writing), `[[noreturn]] void EnterImage(const PeImage&)`; `vanilla.json` keys `md5`, `winmain_call_site`, `winmain`, `crt_ranges` (list of `[start, end)` pairs excluded from snapshots), `srand`, `rand` addresses.

- [ ] **Step 1: Tests** (`[pe]`, Linux and Windows): `make_test_pe.py` writes a minimal PE32 (two sections, one import by name, one by ordinal); `ParsePe` returns its base, sections and imports exactly; truncated files, a wrong `MZ`, a PE32+ file and a section pointing past the end each fail with an error string.
- [ ] **Step 2-4:** run (fail), implement `pe.cpp`, run (pass).
- [ ] **Step 5:** Fill `vanilla.json` from the Ghidra project (Task 3): the `call` to `WinMain` in the CRT entry, `WinMain`'s address, the CRT bookkeeping ranges in `.data` (heap handle, small-block-heap globals, lock table, stdio `_iob`, `_tlsindex`), and the original `srand`/`rand` addresses; record how each was found in the file's `notes` field.
- [ ] **Step 6:** Implement `map_win32.cpp` and a minimal `iwd2r_oracle` exe whose `OracleMain` writes the 4 bytes at `0x847000` (start of `.rdata`) and the value of `rand()` after the original `srand(1)` to stdout, then exits 0. Link at `/BASE:0x10000000`.
- [ ] **Step 7: Run** through `tools/oracle.py build` and `oracle.py run --selftest` (create those subcommands now, reusing `play.py`'s Windows build helpers by importing them): expect the `.rdata` bytes to match the file and `rand()` to be `41`. Then the Review Focus cases: run with a deliberately wrong `winmain_call_site` and expect a clean refusal; run with the range pre-occupied (a test switch that `VirtualAlloc`s `0x400000` first) and expect a message naming the occupant.
- [ ] **Step 8: Commit** "Map the vanilla exe in a 32-bit runner and enter it through its own CRT".

### Task 6: Runner framework

**Files:** Create `oracle/runner/oracle_main.cpp`, `registry.h`, `generators.h`, `compare.h`, `report.cpp`, `snapshot.cpp`, `oracle/cases/selftest.cpp`.

**Interfaces:**
- Produces: `struct OracleCase { const char* id; const char* file; void (*run)(CaseContext&); }`, `ORACLE_CASE(id, fn)` macro registering a case; `CaseContext` with `uint64_t seed`, `int iterations`, `void Fail(const std::string& inputs, const std::string& original, const std::string& ours)`, `template<class F> void ForEachInput(Generator, F)`; generators `IntRange(lo, hi)`, `Edges({...})`, `Random<T>(seed)`; `void RestoreData()` (restores `.data` minus `crt_ranges`); each call into original code goes through `CallOriginal(fn)` wrapped in `__try/__except` reporting `crashed` with the inputs; report `runs/<ts>/oracle.json` `{case, iterations, passed, failures:[{inputs, original, ours}], crashed}`; command line `iwd2r_oracle --file <name> --seed N --iterations N --out <dir>`; `oracle.py run` runs one fresh process per case file.

- [ ] **Step 1:** Self-test cases (`selftest.cpp`): `selftest/isolation` seeds the original `rand`, draws 1,000 values, `RestoreData()`, draws again, requires equality; `selftest/must_fail` compares the original `rand()` against `rand()+1` and is expected to fail (the runner reports it in a separate "expected failures" list and fails the run if it passes); `selftest/crash` calls address `0x401000` with garbage arguments in a way that faults and must be reported as crashed without stopping the run.
- [ ] **Step 2: Run** `python3 tools/oracle.py run --file selftest`; expect `isolation` passed, `must_fail` an expected failure, `crash` reported crashed, exit 0.
- [ ] **Step 3: Commit** "Add the oracle runner with isolation, crash capture and reports".

### Task 7: Vanilla data copy

**Files:** Modify `tools/oracle.py` (`vanilla` subcommand), `tests/test_tools.py`.

**Interfaces:** Produces `C:\Users\thier\iwd2r-oracle\game` and a marker `vanilla.ok` written only after verification passes; `oracle.py vanilla [--rebuild]`; other subcommands call it implicitly when the marker is missing.

- [ ] **Step 1: Tests** for the pure parts: the verification function returns failure for a directory containing `IEex.dll`, for a `WeiDU.log` listing `IWD2EE.TP2`, and for an `iwd2.exe` with the wrong MD5; success for a clean fixture.
- [ ] **Step 2-4:** run (fail), implement, run (pass).
- [ ] **Step 5: Run** `python3 tools/oracle.py vanilla`: copy (rsync) the install, run `Setup-iwd2ee.exe --uninstall` through cmd.exe inside the copy non-interactively (WeiDU's `--uninstall` with `--force-uninstall-list` or the documented non-interactive flags; read WeiDU's `--help` from the copy), verify, write the marker. Expect `iwd2.exe` in the copy to have the vanilla MD5 (WeiDU restores it from backup). Review Focus 4: delete the marker mid-way (simulate with `--rebuild` after removing `vanilla.ok`) and confirm the next run rebuilds from scratch. Confirm the real install's `icewind2.ini` still shows `Resolution=1366`.
- [ ] **Step 6: Commit** "Build a verified vanilla copy of the game for the oracle".

### Task 8: Engine objects for x86 in the runner

**Files:** Modify `oracle/CMakeLists.txt`.

**Interfaces:** `iwd2r_oracle` links `$<TARGET_OBJECTS:iwd2r_engine>`, `iwd2r_compat`, `iwd2r_platform`, zlib, with `src/` on the include path, so cases can call our functions by name. Our `main`/`entry.cpp` are not linked (only engine objects).

- [ ] **Step 1:** Link and build; resolve duplicate-symbol or startup conflicts (our globals' static constructors run under the runner's CRT, the original's under its own; they never share state).
- [ ] **Step 2: Run** `oracle.py run --file selftest` again; expect the same results.
- [ ] **Step 3: Commit** "Link our engine objects into the oracle runner".

### Task 9: RNG and dice cases

**Files:** Create `oracle/cases/rand.cpp`.

**Interfaces:** Cases `rand/sequence` (seed both sides with each of 0, 1, 42 and 10 random seeds; compare 100,000 consecutive values of the original `rand` at `vanilla.json:rand` with `iwd2r_rand`), `rand/roll` (`IcewindMisc::Roll` at `0x5845D0` vs ours: seed both, call with `nRolls` in `[0,20]` and `nSides` in `[0,100]` including 0, 10,000 random pairs, compare results and the following `rand` value).

- [ ] **Step 1:** Write both cases. **Step 2: Run** `oracle.py run --file rand`; expect pass. If `rand/roll` fails, record the first mismatch, fix our `IcewindMisc::Roll` only if the difference is ours (write its evidence file first), and re-run.
- [ ] **Step 3:** Update `docs/evidence/5845D0.md` (`oracle_case: rand/roll`). **Step 4: Commit** "Compare rand and dice rolls against the original".

### Task 10: Table and rule cases

**Files:** Create `oracle/cases/tables.cpp`, `docs/evidence/` files for each function covered.

**Interfaces:** Cases for functions present on both sides: `C2DArray::GetAt(CString,CString)` `0x403340`, `GetAtLong` `0x403440`, `GetAt(CPoint)` `0x622320`, `Find(CString&, CPoint&)` `0x4038E0`; `CRuleTables::GetNextLevelXP` `0x5438D0`, `GetXPCap` `0x544C70`; `CGameSprite::CheckAranceFailure` `0x7243F0`. Fixtures construct objects with each side's own constructor; a `C2DArray` is filled through the class's text-parsing routine on both sides from the same 2DA text (locate the original routine in Ghidra by following `C2DArray::Load`). If the original has no routine that parses text without its resource manager, mark that case `pending: needs engine state` and cover it in replays instead; calling the original resource manager from the oracle is out of scope. `CRuleTables` cases load the XP table from the vanilla copy's `XPLEVEL.2DA` text through the same path.

- [ ] **Step 1:** Write the cases with edge inputs (missing row and column labels, numeric strings with signs, empty cells, level 0, level beyond the table) and 10,000 random lookups each. **Step 2: Run** `oracle.py run --file tables`; record results. **Step 3:** For each mismatch, write the evidence file and fix ours if the difference is ours; re-run until pass or until a mismatch is proven to be the original's own bug (recorded in the evidence file; our code keeps the original behaviour). **Step 4: Commit** "Compare 2DA, XP and spell-failure functions against the original".

### Task 11: Pending cases for project 3

**Files:** Create `oracle/cases/pending.cpp`, `docs/evidence/README.md` section "Pending cases".

**Interfaces:** Cases that call the original's saving throw, THAC0/attack-bonus and damage-roll functions (locate them in Ghidra: search callers of `IcewindMisc::Roll` in `CGameSprite`, `CGameEffect` and `CRuleTables`; name them in the Ghidra project as found), marked `pending: ours not implemented`, run against the original only and checking determinism; the runner reports them in a "pending" list without failing.

- [ ] **Step 1:** Locate, name and record the functions (addresses and signatures in the case file and evidence drafts). **Step 2:** Write the cases. **Step 3: Run**; expect them listed as pending. **Step 4: Commit** "Add pending oracle cases for saves, THAC0 and damage".

### Task 12: Replay recorder in the original

**Files:** Create `oracle/replay/injector.cpp`, `oracle/replay/recorder.cpp`, `oracle/replay/schema.json`, `oracle/replay/fields.json`, `tools/gen_fields.py`. Modify `oracle/CMakeLists.txt`, `tools/oracle.py` (`replay record`), `tests/test_tools.py`.

**Interfaces:**
- `schema.json`: JSON Schema for a dump `{frame, screen, rng_state, globals:{...}, chargen:{race, subrace, class, abilities:[6], rolls:[...], skill_points, feats:[...], portrait}}`.
- `gen_fields.py`: reads `/* 0xNNNN */` offset comments from the headers of the classes the schema names, writes `fields.json` `{field: {base: "<global address or class>", offset, type}}`; tested in `test_tools.py` on a fixture header.
- `injector.exe <exe> <workdir> <dll> [args]`: starts suspended, injects with `CreateRemoteThread(LoadLibraryA)`, resumes, waits, returns the game's exit code.
- `recorder.dll`: IAT hooks for `GetAsyncKeyState`, `GetCursorPos`, `SetCursorPos`, `PeekMessageA`, `GetMessageA`, `timeGetTime`, `GetTickCount`, `timeSetEvent`; virtual clock advanced once per frame (identify the per-frame function in Ghidra and hook it by patching its first instruction with a jump to a trampoline that advances the clock, runs the scenario step, writes due dumps, then continues); seeds the original `srand` at start; reads `IWD2R_SCRIPT`, `IWD2R_REPLAY_DIR`, `IWD2R_DUMP_EVERY` (default 30); writes `frame-<N>.json` and `checkpoint-<name>.json`.
- `oracle.py replay record <scenario> [--seed N] [--timeout S]` (default timeout 180 s; on timeout kill the game and report the last frame reached).

- [ ] **Step 1:** `gen_fields.py` with its tests; run, implement, pass.
- [ ] **Step 2:** Build injector and recorder; record `scenarios/start_screen.txt` with a `checkpoint start` line added; expect `checkpoint-start.json` validating against the schema and `screen: "start"`.
- [ ] **Step 3:** Review Focus 5: a scenario that waits 100,000 frames with a 20 s timeout must be killed and reported with the last frame.
- [ ] **Step 4: Commit** "Record replays from the original game with an injected DLL".

### Task 13: Our dump and replay comparison

**Files:** Create `platform/state_dump.h/.cpp`, `tests/test_state_dump.cpp`, `oracle/replay/screens.json`, `scenarios/chargen_replay.txt`. Modify `platform/script.cpp` and `tools/realinput.py` (`checkpoint NAME` verb), `platform/window.cpp` (dump hook in the pump), `tools/oracle.py` (`replay compare`).

**Interfaces:** `platform::state_dump::Write(const char* path, const DumpSource&)` where `DumpSource` exposes the schema fields through callbacks the engine registers (the engine-side registration lives in a new `src/StateDumpSources.cpp` we own, reading our classes); env `IWD2R_STATE_DUMP=<dir>`, `IWD2R_DUMP_EVERY`; `checkpoint NAME` writes `checkpoint-<NAME>.json` immediately; `screens.json` lists screens whose comparison is enabled (`["start", "chargen"]`); `oracle.py replay compare <scenario> --seed N` runs both sides with the same seed and prints per-checkpoint, per-field differences, exit 1 on any difference in enabled screens.

- [ ] **Step 1: Tests** (`[dump]`): the writer produces JSON that validates against `schema.json` (validate with a small schema check in the test, or with Python in `test_tools.py`); `checkpoint` parsing in both drivers.
- [ ] **Step 2-4:** run (fail), implement, run (pass).
- [ ] **Step 5:** Write `scenarios/chargen_replay.txt`: start screen → New Game → Character Generation, choose gender, race, class, roll abilities (checkpoint after each roll), stop before naming. Run `oracle.py replay compare scenarios/chargen_replay.txt --seed 1` and iterate: each difference is either a dump bug (fix the dump), or a real engine difference (write evidence, fix ours if the difference is ours, re-run). Expect a clean match at every checkpoint.
- [ ] **Step 6: Commit** "Compare character generation against the original at every checkpoint".

### Task 14: Documentation and CI

**Files:** Modify `README.md` (a "Verification" section), `docs/evidence/README.md`, `.github/workflows/ci.yml`, `docs/MAP.md` (regenerated); create `docs/adr/0003-oracle-runtime.md`.

- [ ] **Step 1:** README: what the oracle is, `tools/ghidra/setup.sh`, `oracle.py vanilla|run|replay|evidence`, where results go, the "done" rule. ADR 0003: native x86 runner over emulation or Wine; vanilla first, IWD2EE in project 5; VC6 rand.
- [ ] **Step 2:** CI: the `linux-gcc` job builds and runs the data-free tests added here (`[pe]`, `[rand]`, `[dump]`, the Python tools tests) and the evidence check.
- [ ] **Step 3: Run** the full local verification: both Linux presets' ctest, `test_tools.py`, the notices, map and format checks, `oracle.py run` (all files), `oracle.py replay compare scenarios/chargen_replay.txt --seed 1`.
- [ ] **Step 4: Commit** "Document the verification harness and run its data-free parts in CI".
