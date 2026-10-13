# iwd2r Project 3, Milestone 2: Move and Interact — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The `walk_and_talk` replay matches the original at every checkpoint: click-to-move across the start area, open a door, start and end a dialog with the first NPC.

**Architecture:** Wave 0 (Tasks 1–2) is sequential tooling: the dump gains the door, dialog and action fields M2's checkpoints need, and the exit scenario is written against the original before any engine code changes. Wave 1 (Tasks 3–6) is four parallel streams with disjoint files — AI dispatch, movement, area state, dialog and world screen — each porting its functions from the decompile and proving them with oracle cases, Catch2 tests and the checkpoints it owns. Task 7 is the coordinator's integration and gate.

**Tech Stack:** C++17 (MSVC x86/x64, GCC, Clang), CMake presets, Catch2, Python 3 unittest, Ghidra 12.1.4 headless, the oracle runner and the replay tools, Windows Sandbox for every run of the original.

**Spec:** docs/superpowers/specs/2026-10-13-iwd2r-finish-simulation-design.md (sections 3, 4 M2, 5, 6, 7).

## Global Constraints

- Every function ported or changed in `src/` is ported from the Ghidra decompile of the vanilla exe, keeps the original's structure and its `// 0xADDR` comment, and has `docs/evidence/<ADDR>.md` with `oracle_case:` or `exemption:` (`check_evidence.py`). Pure functions get an oracle case under rulings R10a/R10b/R10c (original objects only from original constructors in leaked memory; zeroed partial fixtures for blocks the function only reads; tables from the extracted 2DA files).
- Vanilla IWD2 behaviour only; IWD2EE is project 5. Multiplayer stays removed: message paths are ported for the local player only (ADR 0002).
- An original bug is reproduced when it is visible and bounded when it is memory corruption on our side (ruling P4-10); either way a NOTE in the code and in the evidence Notes.
- Platform bugs found on the way are fixed immediately in `platform/` or `compat/` with a test.
- Nothing automated opens a window on the user's desktop: the original runs in Windows Sandbox (`oracle.py replay`, the default), ours runs in the sandbox (`play.py --vm windows`) or headless (`--vm linux`, `--headless`); `IWD2R_DESKTOP_OK` is the user's switch only. Everything is silent.
- Never commit game files, extracted game data, dumps, `runs/` or `build/`. Never modify `/mnt/c/Games/Icewind Dale II`. On Windows write only under `C:\Users\thier\iwd2r-build*`, `C:\Users\thier\iwd2r-oracle` and `C:\Users\thier\iwd2r-vm`.
- No per-file header or notice lines. Commit messages end with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- Gates at every integration: linux-gcc and linux-clang ctest serial with `IWD2R_AUDIO=null SDL_AUDIO_DRIVER=dummy IWD2_DATA=$HOME/.local/share/iwd2r/game`; `python3 -I tests/test_tools.py`; `check_evidence.py --base <last pushed>`; `gen_map.py --check`; `gen_fields.py --check`; `shaders.py --check`; the clang-format check on non-`src/` files; `oracle.py build` and `run`; `oracle.py replay compare` for every scenario in `scenarios/`. Pushes are chained on `tools/gates.sh`'s exit status.
- Streams own disjoint files (table below). `CGameSprite.cpp` has one owner this wave (Task 4). A stream that needs a change in another's file asks the coordinator. Each stream works in `/home/thier/iwd2-re-worktrees/p3-<stream>` on branch `p3/<stream>` (`tools/worktree.sh new <stream>`), merges `first` before reporting, and never pushes.
- Project 4 owns `src/CVisibilityMap.cpp`'s blits (`BltFogOWar` 0x551FF0 stays with it) and `CInfinity.cpp`'s render functions; `CInfinity::Scroll` 0x5D1380 is simulation and belongs to Task 4, coordinated at merge.
- Combat is M3. A field that differs only because hostiles reached the party is recorded in `oracle/replay/known_differences.json` with the evidence and cleared in M3; a checkpoint is never moved to hide a difference.

## Review Focus

1. A click on an unreachable spot (water, a wall, outside the search map): the sprite walks to the nearest reachable point and stops; no endless path search, no frame-time spike. Pinned in Task 4.
2. A click on a locked door: the door stays closed, the original's feedback plays (the "locked" string/sound), and the dump's door state is unchanged. Pinned in Task 3.
3. Talking to an NPC from across the area: the sprite walks there first (MoveToObject, then Dialog), and the dialog opens only when within range. Pinned in Tasks 3 and 6.
4. Ending a dialog with Escape half-way: the world screen returns, the game's pause state is what it was before the dialog, and the NPC's script resumes. Pinned in Task 6.
5. A spawn point whose table has no entry for the party's total level: nothing spawns and nothing asserts. Pinned in Task 5.

---

## File Structure

| Path | Responsibility |
|---|---|
| `oracle/replay/schema.json`, `fields.json` (generated), `recorder.cpp`, `src/StateDumpSources.cpp`, `platform/state_dump.{h,cpp}`, `tests/fixtures/state_dump_*.json` | M2 dump fields (Task 1) |
| `scenarios/pending/walk_and_talk.txt` | M2's exit scenario, moved to `scenarios/` by Task 7 |
| `src/CGameAIBase.*`, `src/CAIObjectType.*`, `src/CVariableHash.*`, `src/CGameDoor.*`, `src/CGameContainer.*` | Task 3, stream ai-dispatch |
| `src/CGameSprite.*`, `src/CMoveList.*`, `src/CAIGroup.*`, `src/CGameArea.*`, `src/CInfinity.cpp` (`Scroll` only), `src/CSearchBitmap.*` | Task 4, stream movement |
| `src/CVisibility.*` (except the blits), `src/CSpawn.*`, `src/CGameSpawning.*` | Task 5, stream area-state |
| `src/CGameDialog.*`, `src/CScreenWorld.*`, `src/CInfButtonArray.*` | Task 6, stream dialog-world |

---

## Wave 0: tooling (sequential, one implementer)

### Task 1: M2 dump fields

**Files:**
- Modify: `oracle/replay/schema.json`, `oracle/replay/fields.json` (regenerate with `tools/gen_fields.py`), `oracle/replay/recorder.cpp`, `src/StateDumpSources.cpp`, `platform/state_dump.{h,cpp}`, `tests/fixtures/state_dump_*.json`
- Test: `tests/test_tools.py` (schema samples), `tests/test_state_dump.cpp`

**Interfaces:**
- Produces, under `world`: `doors` — array of `{name, open, locked}` in the area's door-list order (`name` is the door's script name, `open`/`locked` are 0/1 from the door's flags); `dialog` — `{active, speaker, entry, responses}` (`active` 0/1 from the world screen's dialog state, `speaker` the talking sprite's object id or -1, `entry` the current DLG state index or -1, `responses` the number of responses shown); `party[]` gains `action` (the current action id or -1), `modal` (modal state byte), `facing` (direction byte); `explored_count` — the number of explored tiles in the visible area's visibility map; `creatures` — the number of sprites on the visible area's creature list. Offsets come from `/* 0xNNNN */` comments in `CGameDoor.h`, `CScreenWorld.h`, `CGameDialog.h`, `CGameSprite.h`, `CGameAIBase.h`; where a comment is missing, read the layout from Ghidra (`oracle.py evidence` on a reader of the field) and file the evidence as `docs/evidence/README.md` says for headers.
- `oracle.py`'s compare treats every new field as compared (not timing).

- [ ] **Step 1: Tests.** `test_tools.py`: a schema sample with `world.doors[0] = {name:"DOOR01", open:0, locked:1}` and `world.dialog = {active:1, speaker:3, entry:0, responses:2}` validates; `open` as a string fails; a `party[0]` without `action` fails. `[dump]` Catch2: `Write` of a `Snapshot` with one door, an active dialog, 12 explored tiles and 3 creatures produces the `doors`, `dialog`, `explored_count` and `creatures` values.
- [ ] **Step 2–4:** run (fail), implement both sides, run (pass); `python3 tools/gen_fields.py --check` passes after regenerating. Then `python3 tools/oracle.py replay record scenarios/pending/world_entry.txt --seed 1` (sandbox): `checkpoint-world.json` lists the start area's doors with `open` 0 and `dialog.active` 0. Record the door names in the task report; Task 2 uses them.
- [ ] **Step 5: Commit** "Dump doors, dialog state and the party's actions on the world screen".

### Task 2: The walk_and_talk scenario

**Files:**
- Create: `scenarios/pending/walk_and_talk.txt`
- Modify: `oracle/replay/screens.json` only if a new screen name is needed (dialog runs inside the world screen; expect none)
- Test: `tests/test_tools.py` (scenario parses; every `checkpoint` name unique)

**Interfaces:**
- Consumes: Task 1's fields. The scenario is `world_entry.txt`'s steps and checkpoints, then the M2 steps.
- Produces: checkpoints `world_settled` (already), `moved` (after a click-to-move completes: `party[0].x/y` changed and `action` is -1 again), `door_open` (after clicking the first door: its `open` is 1), `dialog` (after clicking the NPC: `dialog.active` 1, `responses` > 0), `dialog_end` (after the responses that end it: `dialog.active` 0). Coordinates for Resolution=800 in `origin ui`.

- [ ] **Step 1: Probe the original.** Record, in the sandbox, `world_entry.txt` extended with `wait 60` / `shot probe-N` pairs for 600 frames after `world_settled`, and read the shots and the periodic dumps: which door is nearest the party, which NPC has a dialog (its `speaker` id when clicked), and when, if at all, hostiles reach the party (`party[0].hp` falls). Write those facts as comments at the top of the scenario. If hostiles engage before the door and the NPC can be reached, the scenario moves away from them first; if contact is unavoidable, the checkpoints stay where they are and the fields that differ because of combat go into `known_differences.json` with the evidence (Global Constraints).
- [ ] **Step 2: Write the scenario** with the five checkpoints above, each preceded by a `wait` long enough for the original to settle (the sprite stops: `action` -1 for two consecutive periodic dumps).
- [ ] **Step 3: Verify determinism.** `python3 tools/oracle.py replay record scenarios/pending/walk_and_talk.txt --seed 1` twice: the two recordings' checkpoint dumps are identical (`ai_rng_state` included). Record the values of `moved`, `door_open`, `dialog`, `dialog_end` in the task report; they are M2's target.
- [ ] **Step 4: Our side today.** `python3 tools/play.py --vm linux --script scenarios/pending/walk_and_talk.txt` with `IWD2R_STATE_DUMP`: report where ours stops (expected: the sprite does not move, since `ProcessAI` is a stub) and the first differing field per checkpoint.
- [ ] **Step 5: Commit** "Add the walk_and_talk scenario".

---

## Wave 1: four parallel streams (one implementer each, own worktree)

Each stream: `tools/worktree.sh new <stream>`; draft evidence for the whole batch with one `python3 tools/oracle.py evidence ADDR...`; port function by function from the decompile (a function of thousands of instructions in sub-batches by basic block, a partial port marked with its exact TODO range); build linux-gcc and run ctest after each function; commit per function or small group; Windows check through `python3 tools/oracle.py build` and `run` in the stream's own build folder; `git merge first` before reporting; report per function: ported / partial (TODO lines) / blocked, plus the scenario checkpoints the stream now matches.

### Task 3: Stream ai-dispatch

**Files:** owns `src/CGameAIBase.cpp/.h`, `src/CAIObjectType.cpp/.h`, `src/CVariableHash.cpp/.h`, `src/CGameDoor.cpp/.h`, `src/CGameContainer.cpp/.h`; evidence for each address.

**Functions:** `CGameAIBase::ProcessAI` 0x45C730; `ExecuteAction` 0x44DC10 — the dispatcher skeleton plus the handlers the start area's scripts and the player's clicks use, in this order: MoveToObject/MoveToPoint, Dialog, PickUp, ToggleDoor/OpenDoor (the door click's action), then the others `ProcessAI` reaches on the `walk_and_talk` path (list every action id seen in the recordings' `party[].action` and the area's scripts' actions, found by decompiling `CAIAction` decoding); `GetGlobalVariable` 0x4686C0; `CAIObjectType::Decode` 0x40B880; the `EvaluateStatusTrigger` overrides (`CGameSprite`, `CGameDoor`, `CGameContainer` — the sprite's goes to Task 4 through the coordinator); `CVariableHash::MarshalToCharacter` 0x550600; the door's open/close path reached from the click (`CGameDoor` functions on it that are stubs; list them).

**Interfaces:**
- Consumes: `CGameSprite::SetModalState(BYTE, BOOL)` and `PlaySound(BYTE, BOOL, BOOL, BOOL)` from Task 4 (signatures already in `CGameSprite.h`); `CMoveList::AIUpdate` from Task 4 for the movement part of MoveToObject.
- Produces: `ExecuteAction` returning the original's codes (0 continue, 1 done, 2 … as the decompile names them — document the enum in `CGameAIBase.h`); a door toggled by the player changes `world.doors[i].open`.

- [ ] **Step 1: Tests.** Oracle cases: `ai/decode` (`CAIObjectType::Decode` over object-type specs: EA/general/race/class/specific/gender/alignment combinations, the "nearest" specifiers, invalid bytes) and `ai/global_variable` (`GetGlobalVariable` with scopes GLOBAL, LOCALS, MYAREA and an area resref scope, present and absent names; the original's `CVariableHash` built by its constructor at 0x54F990). `[ai][data]` Catch2: a sprite given `MoveToPoint` reaches the point and its action clears; `ToggleDoor` on a locked door leaves `open` 0 and the door's "locked" feedback runs (Review Focus 2); `Dialog` from out of range first produces a MoveToObject toward the speaker (Review Focus 3, the dispatch half).
- [ ] **Step 2–4:** write failing tests, port, pass. Verification: with Task 4 merged, `oracle.py replay compare scenarios/pending/walk_and_talk.txt --seed 1` matches through `door_open`.
- [ ] **Step 5:** Commits per function, e.g. "Port CGameAIBase::ExecuteAction's MoveToObject handler from the original".

### Task 4: Stream movement

**Files:** owns `src/CGameSprite.cpp/.h`, `src/CMoveList.cpp/.h`, `src/CAIGroup.cpp/.h`, `src/CGameArea.cpp/.h`, `src/CSearchBitmap.cpp/.h`, and `CInfinity::Scroll` in `src/CInfinity.cpp` (that function only); evidence for each address.

**Functions:** `CMoveList::AIUpdate` 0x517300; `CAIGroup::GroupAction` 0x404D00 and `HandleFollowPath` 0x409820; `CGameSprite::ClearBumpPath` 0x6FA900, `SetModalState` 0x71FC00, `PlaySound` 0x7011E0, the sprite's `EvaluateStatusTrigger` override; `CGameArea::GetNearest` 0x46B160, `AdjustTarget` 0x46A3D0, `SnapshotAdjustTarget` 0x46A630; `CInfinity::Scroll` 0x5D1380; the path-finding helpers these reach that are stubs (`CSearchBitmap` functions; list them).

**Interfaces:**
- Consumes: `ExecuteAction`'s MoveToObject handler from Task 3 calls into `CMoveList::AIUpdate`; until Task 3 merges, drive movement from a test that sets the sprite's destination directly, the way the handler will.
- Produces: `CMoveList::AIUpdate()` moving the sprite one step per AI update along its path; `CGameArea::AdjustTarget(CPoint start, POINT* goal, BYTE personalSpace, SHORT tolerance, ...)` returning the nearest reachable goal; `CInfinity::Scroll(CPoint ptDest, SHORT speed)` moving the viewport.

- [ ] **Step 1: Tests.** Oracle cases: `area/adjust_target` (`AdjustTarget` and `SnapshotAdjustTarget` over a search bitmap fixture built from the extracted `AR1000SR.BMP`: reachable goals unchanged, goals in water or walls moved to the nearest reachable point, goals outside the map clamped — Review Focus 1), `move/follow_path` (`HandleFollowPath` with a path array and the original's group built by `CAIGroup`'s constructor). `[move][data]` Catch2: a sprite at the start position given a reachable destination arrives within the original's step count (take the count from the `moved` checkpoint's frame delta in Task 2's recording); an unreachable destination stops at the adjusted point and never loops (bounded by frames).
- [ ] **Step 2–4:** write failing tests, port, pass. Verification: `oracle.py replay compare scenarios/pending/walk_and_talk.txt --seed 1` matches `moved` (position and `action`) once Task 3's MoveToPoint handler is merged; report the frame at which the sprite stops on both sides.
- [ ] **Step 5:** Commits per function.

### Task 5: Stream area-state

**Files:** owns `src/CVisibility.cpp/.h` (except `BltFogOWar` and the other blits, project 4's), `src/CSpawn.cpp/.h`, `src/CGameSpawning.cpp/.h`; evidence for each address.

**Functions:** `CVisibilityMap::MarkTileLine` 0x552A50 and the explored-tile update it feeds; `CSpawn::ExecuteSpawn` 0x76B160; `CGameSpawning::Spawn` 0x4C9CE0; the spawn-table readers they reach that are stubs.

**Interfaces:**
- Consumes: the party's position from `CGameArea` (read only).
- Produces: explored bits that match the original's after `moved` (Task 1's `world.explored_count`); spawns that create the original's creatures at the original's AI update (Task 1's `world.creatures`).

- [ ] **Step 1: Tests.** Oracle case `vis/mark_tile_line` (`MarkTileLine` from the party's position over the extracted `AR1000` visibility fixture: lines in all eight directions, a line leaving the map, a zero-length line). `[spawn][data]` Catch2: `CGameSpawning::Spawn` with a total character level the table lacks spawns nothing and does not assert (Review Focus 5); with the start party's level it spawns what the original's recording shows at `world_settled` (the `creatures` count).
- [ ] **Step 2–4:** write failing tests, port, pass. Verification: `explored_count` and `creatures` match the original at `world_settled` and `moved`.
- [ ] **Step 5:** Commits per function.

### Task 6: Stream dialog-world

**Files:** owns `src/CGameDialog.cpp/.h`, `src/CScreenWorld.cpp/.h`, `src/CInfButtonArray.cpp/.h`; evidence for each address.

**Functions:** `CGameDialogEntry::Handle` 0x484900; `CGameDialogSprite::AsynchronousUpdate` 0x483F00; `FetchRumor` 0x483B70; `CScreenWorld::EndDialog` 0x68F9D0; `TogglePauseGame` 0x68DFD0; `CInfButtonArray::UpdateButtons` 0x58A340; the dialog-panel functions on the path from the NPC click to the response click that are stubs (list them).

**Interfaces:**
- Consumes: Task 3's `Dialog` action handler starts the dialog by calling into `CGameDialogSprite`; until it merges, start the dialog from a test the way the handler will (set the dialog sprite's state directly).
- Produces: `world.dialog` fields changing as the original's do: `active` 1 with `responses` > 0 at `dialog`, `active` 0 at `dialog_end`; `TogglePauseGame` returning the previous state.

- [ ] **Step 1: Tests.** Oracle case `dialog/fetch_rumor` (`FetchRumor` with the extracted `RUMORS`-style DLG resources: present and absent rumor entries). `[dialog][data]` Catch2: a dialog started with the first NPC's DLG shows the original's response count at the first state (from Task 2's `dialog` checkpoint); Escape half-way ends it, the world screen is back, and the pause state equals what it was before (Review Focus 4); a dialog started from out of range does not open until the sprite arrives (Review Focus 3, the dialog half, together with Task 3's handler once merged).
- [ ] **Step 2–4:** write failing tests, port, pass. Verification: `oracle.py replay compare scenarios/pending/walk_and_talk.txt --seed 1` matches `dialog` and `dialog_end` once Tasks 3 and 4 are merged.
- [ ] **Step 5:** Commits per function.

---

## Integration

### Task 7: M2 integration and gate (coordinator)

- [ ] **Step 1:** Merge each stream into `first` as its review passes (`add_executable` and oracle case-list conflicts resolved as the union of both sides with the non-source tail kept; `docs/MAP.md` regenerated); a stream merges `first` before its review if another merged first.
- [ ] **Step 2:** Run the gates from Global Constraints after each merge; push only on `tools/gates.sh`'s success.
- [ ] **Step 3:** When all four are in: `python3 tools/oracle.py replay compare scenarios/pending/walk_and_talk.txt --seed 1` prints `MATCH` at every checkpoint, twice; `play.py --vm linux` and `--vm windows` runs of the scenario each exit 0 with a `dialog` shot showing the dialog panel; move `scenarios/pending/walk_and_talk.txt` to `scenarios/`; run both suites. `known_differences.json` entries added in M2 each name a combat field and M3.
- [ ] **Step 4:** Whole-milestone review on the most capable model; one fix round; push `first` to `iwd2r` as `main` and `first`.
- [ ] **Step 5:** Update `docs/followups.md` (remove what landed, add what was parked) and `docs/MAP.md`; commit "Milestone 2: the party moves, opens a door and talks".
