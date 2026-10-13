# iwd2r Project 2: Verification Harness

Date: 2026-10-12
Status: design approved in conversation; this document is the written spec for review.
Scope: project 2 of the iwd2r program (see the project 1 spec, section 1.1). Project 1 is complete on branch `first`.

## 1. Purpose

Project 3 will finish the roughly 20% of the decompiled engine that is still missing and fix what the existing 80% gets wrong. Project 2 makes that work safe: every function we finish is checked against the original game's own code, with the evidence recorded. The original brief's rule becomes enforceable tooling: a function counts as done only when it compiles, matches the original's behaviour, and its evidence is linked.

## 2. Decisions already made

- **Decompiler:** Ghidra, headless, driven through REA (MIT) as an MCP server.
- **Oracle runtime:** native 32-bit Windows. The vanilla exe's code runs on the real CPU in a process we control, on the user's Windows host, driven from WSL the way `tools/play.py` drives builds.
- **Scope:** function-level comparison and decompiler evidence now; the state-replay recorder is built against the original game now, and comparison switches on screen by screen as project 3 makes screens reachable in our engine, starting with character generation.

## 3. Facts the design rests on

Measured from the vanilla exe preserved by IWD2EE at `<install>/iwd2ee/backup/1000/iwd2.exe` (MD5 `200f038bec3ed50a964d68b3f0f1f0e7`):

| Fact | Consequence |
|---|---|
| PE32, image base `0x400000`, relocations stripped | The image must be mapped at exactly `0x400000`-`0xA21000`; the runner is linked at another base and reserves that range first. |
| Sections `.text` `0x401000` (4.3 MB), `.rdata` `0x847000`, `.data` `0x8A5000` (1.5 MB), `.rsrc` `0xA1B000` | `.data` is the snapshot unit for isolating calls. |
| Imports only Windows system DLLs (KERNEL32 150, USER32 111, GDI32 34, WINMM 5, DDRAW, DSOUND, DPLAYX, IMM32, WS2_32, VERSION, ADVAPI32, COMCTL32, ole32, comdlg32, WINSPOOL) | MFC 6 and the C runtime are statically linked; every import resolves for real on Windows; no stubs are needed for the function oracle. |
| Built with Visual C++ 6 | The CRT startup sequence and `rand` are VC6's: the LCG `state = state * 214013 + 2531011; return (state >> 16) & 0x7FFF`. |
| Our source carries 9,157 `// 0x...` function addresses; IEex's `function_names.db` names 31,522 addresses | Both become Ghidra labels before analysis. |
| The installed game is IWD2EE-modded (10,000+ override files, patched originals) | The oracle needs a vanilla data copy (section 6). |

## 4. Components

```
tools/oracle.py            WSL front end: sync, build x86, run suites, record/compare replays, write runs/
tools/ghidra/              Ghidra and Java install script, project creation, label import, REA config
docs/evidence/<addr>.md    one evidence file per finished function
oracle/loader/             PE mapper, import resolver, entry patch, .data snapshot (x86, Windows)
oracle/runner/             OracleMain, case registry, input generators, comparators, report writer
oracle/cases/              comparison cases, one file per subsystem
oracle/replay/             injector (x86 exe), recorder DLL (x86), dump schema
platform/state_dump.*      our engine's side of the dump (IWD2R_STATE_DUMP)
compat/msvc_rand.*         VC6-compatible rand/srand for the engine on every platform
tools/check_evidence.py    the "done" rule as a check
```

### 4.1 Evidence: Ghidra and REA

- `tools/ghidra/setup.sh` installs a pinned Ghidra release and a pinned Temurin JDK under `~/.local/share/iwd2r/` (no sudo), verifying published checksums.
- `tools/ghidra/make_project.py` creates a Ghidra project from the vanilla exe, imports labels (ours first, from the `// 0x...` comments with the function names that follow them; then IEex's names for addresses we have not named; IEex names prefixed `ieex_` where they would collide), sets the image to VC6/x86 conventions, and runs auto-analysis headlessly.
- REA is registered as an MCP server for this repository (project-scoped `.mcp.json`), pointed at the Ghidra project.
- An evidence file `docs/evidence/<ADDR>.md` holds: address and name, the decompiled pseudo-code from Ghidra at the time, callers and callees, the oracle case that covers it (or why none applies), and notes. `tools/oracle.py evidence <addr|name>` produces a draft through REA.

### 4.2 Function oracle: loader

A 32-bit Windows executable built with the existing `windows-msvc-x86` preset, linked at base `0x10000000` with `/FIXED:NO /DYNAMICBASE`.

1. **Reserve.** Windows maps the code-page table `C_1252.NLS` at `0x400000` before any code of ours runs (TLS callbacks included), so the runner reserves the range from outside. It starts a copy of itself suspended with `--child` inside a kill-on-close job object, reserves `0x400000`-`0xA21000` there with `VirtualAllocEx(MEM_RESERVE)`, and resumes it. The child confirms the reservation is intact before mapping. If the range is taken, the runner exits with an error naming the module that holds it.
2. **Map.** Copy each section from the file to its virtual address, zero-fill virtual sizes beyond raw sizes, and set page protections from the section characteristics.
3. **Resolve imports.** Walk the import directory; `LoadLibraryA` each DLL and fill the IAT with `GetProcAddress` results (by name and by ordinal).
4. **Patch the entry.** The CRT startup calls `WinMain` once. Its call site is located once with Ghidra and recorded in `oracle/loader/vanilla.json` together with the exe's MD5; the loader refuses any exe whose MD5 differs. The loader rewrites that `call` to target `OracleMain` (a `__stdcall` function with `WinMain`'s signature in the runner).
5. **Run.** Jump to the exe's entry point on a dedicated thread. The original CRT initialises its heap, I/O, environment and every static constructor exactly as it always did, then calls `OracleMain`, which runs the requested suite and calls `ExitProcess` with the result.
6. **Snapshot.** At the start of `OracleMain`, copy `.data` into a buffer and restore it before every case, except the C runtime's own bookkeeping ranges (heap handle, small-block-heap headers, lock and stdio tables), which are located once with Ghidra and listed in `vanilla.json`. Restoring those would desynchronise the CRT heap from the blocks it tracks. Objects a case allocates are therefore leaked rather than freed, and each case file runs in a fresh process, which bounds the leak and gives every file a clean start.

### 4.3 Function oracle: cases

A case declares: the original function's address and signature (calling convention included), our function (linked from the engine's object files built for x86), an input generator, and a comparator.

- **Generators:** explicit edge values, full ranges where small, and a seeded random sweep (default 10,000 inputs; the seed is printed so failures replay).
- **Comparators:** return value; out-parameters; named globals read from both sides after the call; for object-backed cases, observable results of follow-up calls.
- **Tiers:**
  - *Pure:* arguments in, value out.
  - *Object-backed:* the fixture constructs the object on both sides with each side's own constructor from the same bytes (for example, a 2DA table parsed from the same text), then compares queries. Memory layouts are never compared, because our classes deliberately differ from MFC 6's.
  - *Engine-state:* out of scope for the function oracle; covered by replays.
- **First reference set:** dice rolling, THAC0 and attack resolution, saving throws, damage calculation, XP and level tables, 2DA lookups. Each is located in Ghidra (names from our source), and both sides' addresses or symbols are recorded in the case.
- **Calling:** both sides are MSVC x86, so `__thiscall`, `__cdecl` and `__stdcall` calls go through typed function pointers. `__thiscall` methods receive a `this` the fixture constructed with the original constructor in original memory.
- **Output:** `runs/<timestamp>/oracle.json` and a readable summary. A mismatch prints the seed, the inputs and both outputs side by side.

### 4.4 Random numbers

- The original's `rand` state is not in `.data`: VC6's multithreaded CRT keeps `_holdrand` per thread, at offset `0x14` of the thread's `_tiddata` (from `_getptd()`). The oracle seeds it through the original's own `srand` (address from Ghidra), and the per-case snapshot saves and restores the main thread's `_tiddata` fields along with `.data`.
- Our engine on Linux currently calls glibc's `rand`, whose sequence differs from VC6's. That is a faithfulness bug: the same seed produces different dice on Linux. `compat/msvc_rand` provides VC6's LCG as `iwd2r_rand`/`iwd2r_srand`, with per-thread state as VC6's multithreaded CRT keeps it, and `compat/mfc.h` maps `rand`/`srand` to them for engine code on every platform. A unit test pins the first values for seeds 0, 1 and 42 to VC6's known outputs, and an oracle case compares 100,000 consecutive values against the original's `rand`.

### 4.5 Vanilla data

`tools/oracle.py vanilla` builds `C:\Users\thier\iwd2r-oracle\game` once: copy the install, then run IWD2EE's own uninstaller (`Setup-iwd2ee.exe --uninstall`, WeiDU) inside the copy, non-interactively. It verifies the result: no `IEex*.dll`, `WeiDU.log` lists no IWD2EE components, and `iwd2.exe`'s MD5 matches section 3. The real install is never modified. Both the original and our engine use this copy for every comparison; our engine runs in vanilla mode.

### 4.6 Replay recorder

- **Injector** (x86 exe): starts the vanilla exe suspended with the vanilla copy as working directory, injects the recorder DLL with `CreateRemoteThread` + `LoadLibraryA`, resumes.
- **Recorder DLL** (x86), on load:
  - hooks the exe's IAT entries for `GetAsyncKeyState`, `GetCursorPos`, `SetCursorPos`, `PeekMessageA`/`GetMessageA` (input), and `timeGetTime`, `GetTickCount`, `timeSetEvent` (time);
  - runs a scenario file (the harness's format) by feeding the hooked input calls frame by frame;
  - drives time from a virtual clock advanced once per engine frame, so frame N means the same moment in every run;
  - seeds `rand` through the original's `srand` at start;
  - every N frames (default 30) and at scenario `checkpoint NAME` lines, writes a state dump.
- **State dump:** JSON with a schema in `oracle/replay/schema.json`: frame number, current screen, RNG state, globals by name, and per-screen sections (character generation first: race, class, ability scores, rolls, skill points, feats, portrait). Field addresses and offsets come from the layout comments in our headers (`/* 0xNNNN */`); a generated table maps schema fields to addresses so the DLL never hard-codes offsets inline.
- **Our side:** `platform/state_dump` writes the same JSON when `IWD2R_STATE_DUMP` is set, reading the same fields through our classes. Scenario files gain a `checkpoint NAME` verb in both drivers.
- **Comparison:** `tools/oracle.py replay compare <scenario>` runs the scenario on both sides with the same seed and reports per-checkpoint, per-field differences. Threads in the original make continuous comparison impossible; checkpoints after input settles are the unit.
- **Staging:** the recorder works against the whole original game now. Comparison is enabled per screen in a list in `oracle/replay/screens.json`, starting with character generation; project 3 extends the list as screens become reachable in our engine.

### 4.7 The "done" rule

`tools/check_evidence.py [--base REV]` lists the functions a change touches (from the `// 0x...` comments of edited function bodies under `src/`) and fails unless each has an evidence file and either an oracle case or a written exemption in its evidence file. It runs in CI (no game data needed). The workflow is documented in `docs/evidence/README.md`.

## 5. Testing and CI

- **Data-free, in CI:** loader parsing against a small synthetic PE32 built by a Python script at test time; `msvc_rand` values; schema validation; `check_evidence.py` on fixture repositories.
- **Local, needs the game:** the function suite, the RNG oracle case, replay record and compare. `tools/oracle.py` runs them; results in `runs/`.
- **Self-test of the oracle:** a case that seeds the original's `rand`, draws 1,000 values, restores `.data` and the thread's `_tiddata`, and draws again, requiring identical sequences (proving isolation works); and a deliberately wrong case that must fail, so a silently passing oracle is detectable.

## 6. Success criteria

1. Ghidra and REA are installed and configured; both label sets are imported; `tools/oracle.py evidence <name>` produces an evidence draft for any named function.
2. The function oracle passes for the first reference set across seeded sweeps of at least 10,000 inputs each, on the x86 build.
3. The engine uses VC6's `rand` on every platform; the unit test and the 100,000-value oracle case pass.
4. A character-generation scenario records against the vanilla game and matches ours at every checkpoint.
5. `check_evidence.py` runs in CI and the workflow is documented.

## 7. Non-goals

- Finishing engine functions (project 3), beyond the reference set's cases and the RNG fix.
- IWD2EE behaviour: the oracle is vanilla. IWD2EE mode is verified against the modded original later, in project 5.
- Running the oracle on Linux or in CI.
- In-world replays: the recorder supports them, but comparison waits for project 3.

## 8. Risks

- **The `0x400000` range may already be in use** by something injected into every process (security software). Mitigation: the parent reserves the range in a suspended child before it runs; a clear error naming the culprit module if it fails.
- **Static constructors with side effects** in the original could touch files or the registry during CRT initialisation. Mitigation: the oracle runs inside the vanilla data copy as working directory; any such effect is logged and reviewed once.
- **Address map errors.** Our `/* 0xNNNN */` layout comments are hand-recovered and may be wrong in places; a wrong offset produces a false difference, not a false match, and the evidence workflow corrects the comment.
- **WeiDU uninstall** might leave residue. Mitigation: the verification in 4.5, plus a hash list of the original GOG files if available from GOG's manifest.
