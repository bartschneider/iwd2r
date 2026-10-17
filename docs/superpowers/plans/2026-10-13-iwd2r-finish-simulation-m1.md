# iwd2r Project 3, Milestone 1: Into the World — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new party stands in the start area on the world screen, on Linux and Windows, and the `world_entry` replay matches the original at every checkpoint.

**Architecture:** Wave 0 (Tasks 1–4) is sequential tooling that lets parallel agents share Ghidra, the Windows build folders, the replay copy and the sandbox, and gives M1 its exit scenario and dump fields before any engine code changes. Wave 1 (Tasks 5–8) is four streams, each in its own worktree with disjoint file ownership, porting from the Ghidra decompile of the vanilla exe. Task 9 is the coordinator's integration and the milestone gate.

**Tech Stack:** C++17 (MSVC x86/x64, GCC, Clang), CMake presets, Catch2, Python 3 unittest, Ghidra 12.1.4 headless, the project-2 oracle and replay tools.

**Spec:** docs/superpowers/specs/2026-10-13-iwd2r-finish-simulation-design.md (sections 2, 3, 4 M1, 5, 6).

## Global Constraints

- Every function ported or changed in `src/` is ported from the Ghidra decompile of the vanilla exe, keeps the original's structure and its `// 0xADDR` comment, and has `docs/evidence/<ADDR>.md` (`check_evidence.py` enforces this; header layout changes are filed by hand per `docs/evidence/README.md`). No new functions the original lacks, except test seams that are load-bearing and listed in the README's deviations.
- Vanilla IWD2 behaviour only. IWD2EE changes are project 5. Multiplayer stays removed: `CMessageHandler` is ported for the local path only (ADR 0002).
- Platform bugs found on the way are fixed immediately in `platform/` or `compat/` with a test.
- Never commit game files, extracted game data, dumps, `runs/` or `build/`. Never modify `/mnt/c/Games/Icewind Dale II`. On Windows write only under `%USERPROFILE%\iwd2r-build*`, `%USERPROFILE%\iwd2r-oracle` and `%USERPROFILE%\iwd2r-vm`.
- No per-file header or notice lines. Commit messages end with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- Gates at every integration: linux-gcc and linux-clang ctest; `python3 -I tests/test_tools.py`; `check_evidence.py`; `gen_map.py --check`; `gen_fields.py --check`; the CI clang-format check; `oracle.py run`; `oracle.py replay compare` for every scenario in `scenarios/`; the Windows x64 build.
- Streams own disjoint files (section 5.1 of the spec). A stream that needs a change in another stream's file asks the coordinator. Each stream works in `~/iwd2-re-worktrees/p3-<stream>` on branch `p3/<stream>`, branched from `first`, and never pushes.
- Ghidra: one headless process at a time (the lock from Task 1); never modify the project except through `make_project.py`.

## Review Focus

1. A fresh install whose `mpsave/default` is empty or absent: starting a new game must not assert or crash. Pinned in Task 7 (the `FindFirstFileA` test) and Task 5 (the `LoadGame` path).
2. The original game's `ICEWIND2.GAM` from a real save, with areas the party has not visited: `CInfGame::Unmarshal` must load the master area only and leave the rest unloaded without touching missing files. Pinned in Task 5.
3. A CRE whose equipped item resource is missing from the install: `CGameSpriteEquipment::Unmarshal` must leave the slot empty and continue, never dereference a null item. Pinned in Task 6.
4. Done on Party Formation clicked twice quickly: one game starts, not two. Pinned in Task 5.
5. A character name colliding with an existing file in `characters/`: export must overwrite or number the file the way the original does, never crash on the open. Pinned in Task 5.

---

## File Structure

| Path | Responsibility |
|---|---|
| `tools/filelock.py` | `hold(path, what)` context manager: blocking `fcntl.flock`, logs when it waits |
| `tools/ghidra/make_project.py` | `headless()` holds the Ghidra lock |
| `tools/ghidra/DecompileToEvidence.java` | several queries per run |
| `tools/oracle.py` | `evidence` takes several functions; replay and vanilla-build locks |
| `tools/play.py` | per-worktree `WIN_CHECKOUT`; sandbox lock |
| `tools/worktree.sh` | stream worktree create/remove |
| `scenarios/pending/world_entry.txt` | M1's exit scenario, moved to `scenarios/` when it passes |
| `oracle/replay/schema.json`, `fields.json`, `recorder.cpp`, `src/StateDumpSources.cpp` | world dump fields |
| `src/CInfGame.cpp`, `src/CGameArea.cpp`, `src/CTiledObject.cpp` | game-file stream |
| `src/CGameSprite.cpp`, `src/CGameSpriteEquipment.cpp`, `src/CDerivedStats.cpp` | creature-file stream |
| `src/CMessage.cpp`, `platform/fs.cpp`, `src/CUtil.cpp` | messages-platform stream |
| `src/CScreenCreateChar.cpp`, `src/CRuleTables.cpp` | chargen stream |

---

## Wave 0: tooling (sequential, one implementer)

### Task 1: Ghidra lock and batch evidence

**Files:**
- Create: `tools/filelock.py`
- Modify: `tools/ghidra/make_project.py:70-85` (`headless`), `tools/ghidra/DecompileToEvidence.java`, `tools/oracle.py:267-290` (`cmd_evidence`, `decompile`, `parse_decompile`, the `evidence` argparse)
- Test: `tests/test_tools.py`

**Interfaces:**
- Produces: `filelock.hold(path: str, what: str, timeout: float | None = None)` context manager; blocking `fcntl.flock(LOCK_EX)`; prints `<what>: waiting for <path>` to stderr after 5 s; raises `TimeoutError` if `timeout` elapses. `make_project.headless(args, log)` unchanged in signature, now wrapped in `hold(LOCK_PATH, 'ghidra')` with `LOCK_PATH = os.path.join(PROJECT_DIR, '..', 'iwd2.lock')`. `DecompileToEvidence.java OUT QUERY...` writes one `QUERY <text>` record before each query's records. `oracle.py evidence FUNC [FUNC...] [--force]` drafts every function in one Ghidra run; ambiguous or missing names are listed and the command exits 1 after writing the rest.

- [ ] **Step 1: Tests.** In `test_tools.py`: `test_hold_blocks_second_holder` (a thread holds the lock; `hold(path, 'x', timeout=0.2)` raises `TimeoutError`; after release it succeeds); `test_parse_decompile_several_queries` (a fixture with two `QUERY` records parses into two entries, each with its own function, callers and callees); `test_evidence_writes_each_function` (run `cmd_evidence` with a stubbed `decompile` returning the two-query fixture; two files written; `--force` semantics per file).
- [ ] **Step 2:** run, expect failures on missing module and old formats.
- [ ] **Step 3:** implement `filelock.py`, the `headless` wrap, the Java multi-query loop (reuse `findFunctions` per query), `decompile(queries: list[str])` and `parse_decompile` returning a list.
- [ ] **Step 4:** run `python3 -I tests/test_tools.py` (pass); then `python3 tools/oracle.py evidence IcewindMisc::Roll CRuleTables::GetXPCap --force` on the real project (both files rewritten, content unchanged apart from Notes you restore from git) and `git checkout docs/evidence` to discard.
- [ ] **Step 5: Commit** "Lock the Ghidra project and draft several evidence files per run".

### Task 2: Per-worktree Windows builds and the shared-resource locks

**Files:**
- Modify: `tools/play.py:54` (`WIN_CHECKOUT`), `tools/play.py` sandbox run site (~610), `tools/oracle.py` (`replay record`/`compare` entry points ~1275–1320, `build_vanilla` ~1013), `tools/winsandbox.py:36` (`VM_ROOT` unchanged; lock path added)
- Test: `tests/test_tools.py`

**Interfaces:**
- Produces: `play.win_checkout(root: str, env: dict) -> str`: `env['IWD2R_WIN_CHECKOUT']` if set; else `/mnt/c/Users/<you>/iwd2r-build` when `basename(root) == 'first'`; else `/mnt/c/Users/<you>/iwd2r-build-<basename(root)>`. `WIN_CHECKOUT = win_checkout(ROOT, os.environ)`. Locks: `ORACLE_DIR/replay.lock` held for the whole of `replay record` and `replay compare` (`what='replay'`); `ORACLE_DIR/vanilla.lock` held around `build_vanilla`; `VM_ROOT/sandbox.lock` held around each `--vm windows` scenario run (`what='sandbox'`).

- [ ] **Step 1: Tests.** `test_win_checkout_defaults` (the three cases above); `test_replay_holds_lock` (patch `filelock.hold` with a recorder; `replay record` with a stubbed runner records `replay.lock`); same for the sandbox run with a stubbed sandbox.
- [ ] **Step 2–4:** run (fail), implement, run (pass). Then `python3 tools/play.py --linux --headless --frames 60 --data /mnt/c/Users/<you>/iwd2r-oracle/game` still runs; `python3 tools/oracle.py run --selftest` still passes (the `first` worktree keeps its existing build folder).
- [ ] **Step 5: Commit** "Give each worktree its own Windows build folder and lock the shared resources".

### Task 3: Stream worktrees and the world_entry scenario

**Files:**
- Create: `tools/worktree.sh`, `scenarios/pending/world_entry.txt`
- Modify: `tools/play.py` (`--suite` globs `scenarios/*.txt` only; document `scenarios/pending/`), `README.md` (Run section: pending scenarios)
- Test: `tests/test_tools.py`

**Interfaces:**
- Produces: `tools/worktree.sh new <stream>` creates `~/iwd2-re-worktrees/p3-<stream>` on branch `p3/<stream>` from the current `first` HEAD and prints the path; `tools/worktree.sh done <stream>` removes the worktree and deletes the branch only if it is merged into `first`, else refuses. `scenarios/pending/world_entry.txt`: `chargen_replay.txt`'s steps with its checkpoints, then skills (Intimidate + ×4), feats (Blind-Fight, Dash), appearance, sound, name `Probe`, with checkpoints `skills_done`, `feats_done`, `named`, `party_formation`, then Done (634,507), `wait 150`, `checkpoint world`, `wait 300`, `checkpoint world_settled`, `shot world`, `quit`. Coordinates at 800×600 from `/tmp/claude-probe/play_to_world.txt` (copy it; it is outside the repo).

- [ ] **Step 1: Tests.** `test_worktree_script_new_and_done` in a temporary repo with a `first` branch (`new` creates the directory and branch; `done` refuses while unmerged, succeeds after `git merge`). `test_suite_ignores_pending` (a `scenarios/pending/x.txt` fixture is not in the suite list).
- [ ] **Step 2–4:** run (fail), implement, run (pass). Then validate the scenario on the original: `python3 tools/oracle.py replay record scenarios/pending/world_entry.txt --seed 1` must exit 0 and `checkpoint-world.json` must have `screen: "world"`; record it twice and diff the dumps (identical). Record the exact frame numbers in the task report.
- [ ] **Step 5: Commit** "Add the stream worktree script and the world_entry scenario".

### Task 4: World dump fields

**Files:**
- Modify: `oracle/replay/schema.json`, `oracle/replay/fields.json` (generated), `tools/gen_fields.py` (only if a new header pattern needs it), `oracle/replay/recorder.cpp`, `src/StateDumpSources.cpp`, `oracle/replay/screens.json` (add `"world"`)
- Test: `tests/test_tools.py` (schema sample), `tests/test_state_dump.cpp`

**Interfaces:**
- Produces: dump keys under `world`: `area` (string resref of `m_gameAreas[m_visibleArea]`, `""` when none), `party` (array of `{id, x, y, hp, max_hp, skills: [int...]}` for party members in slot order; `skills` from the sprite's skill array), and `screen: "world"` when the world engine is active. Offsets come from `/* 0xNNNN */` comments in `CInfGame.h`, `CGameArea.h`, `CGameSprite.h`, `CDerivedStats.h`; add the comments where missing by reading the original layout from Ghidra (`oracle.py evidence` on the readers, e.g. `CInfGame::GetProtagonist` 0x5BFC40, `CGameArea::GetResRef`) and file evidence per `docs/evidence/README.md`.

- [ ] **Step 1: Tests.** A schema sample with the `world` block validates; a sample with `party[0].hp` as a string fails. `[dump]` Catch2: `Write` with a `Snapshot` carrying one party member produces the `world` block.
- [ ] **Step 2–4:** run (fail), implement both sides, run (pass); `python3 tools/gen_fields.py --check` passes after regenerating. Then `oracle.py replay record scenarios/pending/world_entry.txt --seed 1`: `checkpoint-world.json` shows the start area's resref (from `STARTARE.2DA`) and one party member with non-zero `max_hp`. Record the values in the task report; they are M1's target.
- [ ] **Step 5: Commit** "Dump the area and the party on the world screen".

---

## Wave 1: four parallel streams (one implementer each, own worktree)

Each stream: `tools/worktree.sh new <stream>`; draft evidence for the whole batch with one `python3 tools/oracle.py evidence ADDR...`; port function by function from the decompile; build linux-gcc and run ctest after each function; commit per function or small group; run the stream's Windows check through `python3 tools/oracle.py build` and `run` in its own worktree (own build folder from Task 2); report per function: ported / partial (TODO lines) / blocked.

### Task 5: Stream game-file

**Files:** owns `src/CInfGame.cpp`, `src/CInfGame.h`, `src/CGameArea.cpp`, `src/CGameArea.h`, `src/CTiledObject.cpp`, `src/CTiledObject.h`; evidence files for each address below.

**Functions:** `CInfGame::Unmarshal` 0x5A7E40 (`BOOL Unmarshal(BYTE* pGame, LONG nGame, BOOLEAN bProgressBarInPlace)`), `SaveGame` 0x5AC430, `SetupCharacters` 0x5BF6A0, `UpdateCharacterSlots` 0x5BD6B0, `SynchronousUpdate` 0x5BE900, `SelectToolbar` 0x5ADAE0, `GetProtagonist` 0x5BFC40, `CharacterExport` 0x5A1AB0; `CGameArea::Marshal` (find its address: the caller is `CInfGame::SaveGame`; it is absent from `src/`, add the declaration matching the original's signature) and the rest of `CGameArea::Unmarshal` 0x472DE0 (the TODO at :1951 onward); `CTiledObject::Initialize` 0x54E830, `SetNewResWED` 0x54E980; any helper on these paths that is a stub (list them in the report).

**Interfaces:**
- Consumes: `CGameSprite::Marshal`/`Unmarshal` and `CGameSpriteEquipment::Marshal` from Task 6 (signatures already in `CGameSprite.h:480-482`); until Task 6 merges, call them as declared and expect the stubs.
- Produces: the GAM round trip. `SaveGame` writes `mpsave/default/ICEWIND2.GAM` and the area `.ARE` files; `Unmarshal` reads them back and loads the master area through `LoadArea`.

- [ ] **Step 1: Tests.** `[gam][data]` Catch2 (skipped without `IWD2_DATA`): after a `SaveGame` of a game built by `NewGame` plus one sprite from `CHARBASE.CRE`, `Unmarshal` of the written bytes yields the same master area resref and party count; a second `SaveGame` produces byte-identical `ICEWIND2.GAM`. Review Focus 2: `Unmarshal` of a GAM listing two areas with only the master area's ARE present loads one area and reports no error. Review Focus 4: `SetupCharacters` called twice adds the party once (assert party count). Review Focus 5: `CharacterExport` with an existing `characters/<name>.chr` behaves as the original's `Marshal` path (read the decompile for overwrite vs. numbering) and the test pins that behaviour.
- [ ] **Step 2–4:** write failing tests, port, pass. Verification: `python3 tools/play.py --vm linux --script scenarios/pending/world_entry.txt --data /mnt/c/Users/<you>/iwd2r-oracle/game` reaches the world screen (the `world` shot shows the start area) once Tasks 6 and 7 are merged; before that, report how far it gets and the exact assertion text.
- [ ] **Step 5:** Commits per function, e.g. "Port CInfGame::Unmarshal from the original".

### Task 6: Stream creature-file

**Files:** owns `src/CGameSprite.cpp`, `src/CGameSprite.h`, `src/CGameSpriteEquipment.cpp`, `src/CGameSpriteEquipment.h`, `src/CDerivedStats.cpp`, `src/CDerivedStats.h`; evidence per address.

**Functions:** `CGameSprite::Marshal` 0x70B2F0 (all three overloads at `CGameSprite.h:480-482`; find each one's address through `query.py`), the rest of `CGameSprite::Unmarshal` 0x70CF90 (the TODO at :6923, the CRE upgrade path), `CGameSpriteEquipment::Marshal` 0x7124C0, `CDerivedStats` constructor, `Reload` 0x4440F0, `operator=` 0x445330, `BonusInit` (all 11 markers), the remaining `ProcessEffectList` 0x72DE60 TODOs and `HandleEffects` 0x734550, the `SetStat` DEX case at 0x7600F6, and remove the duplicate `sub_72DE60` stub (callers: `CScreenCreateChar::UpdateFeatsPanel`; coordinate through the coordinator since that file belongs to Task 8: Task 6 removes the stub and declaration, Task 8 repoints the call).

**Interfaces:**
- Produces: a CRE round trip. `Unmarshal` then `Marshal` of `CHARBASE.CRE` gives bytes that, unmarshalled and marshalled again, are identical to the first marshal.

- [ ] **Step 1: Tests.** `[cre][data]` Catch2: the round-trip stability above, plus spot checks (abilities in STR, INT, WIS, DEX, CON, CHR order; `feats[1] == 0x03F01FC0`; hit points). Review Focus 3: a CRE with an equipped item whose resref is `ZZZZZZZZ` unmarshals with that slot empty and no crash.
- [ ] **Step 2–4:** write failing tests, port, pass. Oracle: give `pending/save_rolls` (0x72DE60) a body if a zeroed-sprite fixture is honest per ruling R10b; otherwise leave it listed and say why.
- [ ] **Step 5:** Commits per function.

### Task 7: Stream messages-platform

**Files:** owns `src/CMessage.cpp`, `src/CMessage.h`, `platform/fs.cpp`, `platform/fs.h`, `src/CUtil.cpp`; evidence per address; `tests/test_fs.cpp` (or the existing fs test file).

**Functions:** `CMessageHandler::AsynchronousUpdate` 0x4EE020, `Send` 0x4F7830, `Broadcast` 0x4F7620, local path only (the network branches stay as ADR 0002 left them; the decompile's non-local branches are left out with a comment naming the ADR). Platform: `FindFirstFileA`/`FindNextFileA` return `.` and `..` for patterns `*` and `*.*` (first, in that order) as Windows does, and not for other patterns; `CUtil::UtilAssert` (address via `python3 tools/ghidra/query.py CUtil::UtilAssert`) takes the file name after the last `\\` or `/`.

- [ ] **Step 1: Tests.** `[fs]` Catch2: an empty directory searched with `dir/*` yields `.`, `..`, then none; `dir/*.sav` on an empty directory yields none (Review Focus 1); a directory with one file yields `.`, `..`, the file. For `UtilAssert`: a scenario run on Linux at today's `first` (before Tasks 5–6 merge) must end with the assertion text `Couldn't copy files from Temp to Save directories` in `log.txt` instead of a silent SIGSEGV; record the log excerpt in the report.
- [ ] **Step 2–4:** write failing tests, port and fix, pass.
- [ ] **Step 5:** Commits: "Return the dot entries from an empty directory search as Windows does", "Accept forward-slash paths in UtilAssert", "Port the local CMessageHandler dispatch".

### Task 8: Stream chargen

**Files:** owns `src/CScreenCreateChar.cpp`, `src/CScreenCreateChar.h`, `src/CRuleTables.cpp`, `src/CRuleTables.h`; evidence per address.

**Functions:** `CompleteCharacterSkills` 0x612DE0, `CompleteCharacterWrapup` 0x613310, `UpdateCharacterStats` 0x611AF0 TODOs, `UpdateCharacterSummary` 0x610DC0, the auto-pick button 0x617D80, `UpdateMainPanel` 0x607BB0 spells TODO, `CRuleTables::GetStartingHitPoints` 0x542FA0; repoint `UpdateFeatsPanel`'s `sub_72DE60` call at `ProcessEffectList` (Task 6 removes the stub).

**Interfaces:**
- Produces: the `skills_done`, `feats_done` and `named` checkpoints of `world_entry` match the original's dumps for `party[0].skills`, `sprite_feats` and `hp`.

- [ ] **Step 1: Tests.** Oracle case `rules/starting_hp` comparing `GetStartingHitPoints` with the original over classes 1–11 and levels 1–30 (pure function; if it reads a table, load it through `LoadText` from the extracted tables as `tables.cpp` does). `[rules]` Catch2 for any pure helper.
- [ ] **Step 2–4:** write failing tests, port, pass. Verification: `oracle.py replay compare scenarios/pending/world_entry.txt` matches at `skills_done`, `feats_done` and `named` (the later checkpoints may differ until Tasks 5–7 merge; report them separately).
- [ ] **Step 5:** Commits per function.

---

## Integration

### Task 9: M1 integration and gate (coordinator)

- [ ] **Step 1:** Merge each stream into `first` as its review passes, in the order they finish; a stream rebases on `first` before review if another merged first. Resolve conflicts in `CScreenCreateChar.cpp` (Task 6/8 handoff) by hand.
- [ ] **Step 2:** Run the gates from Global Constraints after each merge.
- [ ] **Step 3:** When all four are in: `python3 tools/oracle.py replay compare scenarios/pending/world_entry.txt --seed 1` must print `MATCH` at every checkpoint, twice; `python3 tools/play.py --vm linux --script scenarios/pending/world_entry.txt --data <vanilla copy>` and the Windows run (`--vm windows`) each produce a `world` shot showing the start area with the party. Move `scenarios/pending/world_entry.txt` to `scenarios/world_entry.txt`; run `play.py --vm windows --suite`.
- [ ] **Step 4:** Whole-milestone review on the most capable model; one fix round; push `first` to `iwd2r` as `main` and `first`.
- [ ] **Step 5:** Update `docs/followups.md` (remove what landed, add what was parked) and `docs/MAP.md`; commit "Milestone 1: a new party enters the world".
