# iwd2r Project 3: Finish the Simulation

Date: 2026-10-13
Status: design approved in conversation; this document is the written spec for review.
Scope: project 3 of the iwd2r program. Projects 1 (portability) and 2 (verification harness) are complete on branch `first`.

## 1. Purpose

Make vanilla Icewind Dale II playable end to end on iwd2r: finish a character, enter the first area, move, open doors, talk, fight, cast, save and load. Every function ported or fixed on the way matches the original's code, keeps its structure, and carries evidence that CI enforces.

Project 2 built the tools this project runs on: the labelled Ghidra project, the evidence workflow, the x86 oracle runner, the vanilla game copy, the replay recorder for the original and the replay comparison for our engine. Project 3 is the first project that uses them at scale.

## 2. Starting point, measured 2026-10-13

Three research passes over the code and a scripted play-through established where the engine stands.

**What works.** Character generation runs end to end: a scripted run finished a character (gender, race, class, alignment, abilities, skills, feats, appearance, name) and reached Party Formation with the character in slot 1. Character generation matches the original at 20 of 20 replay checkpoints.

**Where it stops.** Party Formation's Done button crashes the engine every time. Three problems stack there:

1. `CInfGame::Unmarshal` (0x5A7E40) is an empty stub. The new-game path saves the game and reloads it; with nothing read from `ICEWIND2.GAM`, no area exists and the world screen dereferences a null area in `CInfGame::WorldEngineActivated`.
2. Our Linux `FindFirstFileA` (platform/fs.cpp) skips `.` and `..`, so a search over an empty folder fails where Windows succeeds. `CInfGame::LoadGame` asserts on it.
3. `CUtil::UtilAssert` assumes backslash paths; on Linux it faults before printing, so the log stays empty.

**The inventory.** 371 `TODO: Incomplete` markers and about 270 stubbed functions, spread as follows. Counts are markers / stubs.

| Subsystem | Markers / stubs | What is missing |
|---|---|---|
| Effects and spells | 6 / 6, plus 107 effect classes with no `ApplyEffect` | `CGameEffect::ApplyEffect` (0x452E40) asserts; `CheckSave` (0x4A42F0) absent; `CProjectile::DeliverEffects` (0x52A1A0) stub |
| Combat | 56 / 34 | `Hit` (0x73B760) and `Damage` (0x73D560) absent; `GetAC` (0x723CC0) and `GetStartingHitPoints` (0x542FA0) return 0; `CItem::Equip`/`Unequip` stubs |
| AI and scripting | 17 / 17 | `CGameAIBase::ProcessAI` (0x45C730) and `ExecuteAction` (0x44DC10) stubs; `CAIObjectType::Decode` (0x40B880); status triggers always false; Attack, Spell, Dialog, MoveToObject and PickUp handlers absent |
| Area and world | 43 / 32 | `GetNearest`, `AdjustTarget`, `ClearBumpPath`, visibility and fog, `CInfinity::Scroll`, spawns, `CTiledObject::Initialize` |
| Game state, save, load | 20 / 20 | `CInfGame::Unmarshal`, `SaveGame`, `SetupCharacters`, `UpdateCharacterSlots`, `SynchronousUpdate` (called every world frame); `CGameSprite::Marshal`; `CGameArea::Marshal` absent |
| Messages | — | `CMessageHandler::AsynchronousUpdate`, `Send`, `Broadcast` stubs: queued game messages never apply |
| Dialog and stores | 31 / 31 | `CGameDialogEntry::Handle`, `EndDialog`, `StartStore`, `CStore` marshal and trade |
| UI screens | 74 / 65 | Character 19, Store 12, Inventory 12, World 10, CreateChar 7, Spellbook 5, WorldMap 3 |
| Sprites and rendering | 60 / 54 | 16/24-bit blit paths, selection circles, clipping behind walls, sprite effects |
| Sound | 0 / 0 | only `CGameSprite::PlaySound` (0x7011E0) |

## 3. Principles

- **Faithfulness first.** Every function is ported from the Ghidra decompile of the vanilla exe and keeps the original's structure and `// 0xADDR` comment. Deviations are minimal, documented in the README's deviations list, and never add functions the original lacks unless they are load-bearing for tests (the `LoadText` precedent).
- **Vanilla first.** Behaviour targets vanilla IWD2. IWD2EE changes are project 5, behind its runtime flag. Multiplayer stays removed: `CMessageHandler` is ported for the local message path only, as ADR 0002 says.
- **Done means proven.** A function is done when it compiles on both platforms, matches the original's behaviour, and has `docs/evidence/<ADDR>.md` (CI enforces this for `src/`; header layout changes are filed by hand, as `docs/evidence/README.md` says). Pure functions also get an oracle case. Engine state is proven by replay checkpoints against the original.
- **Fix the platform as found.** Portability bugs in `platform/` and `compat/` found on the way are fixed immediately with a test, since they can mask engine bugs.
- **Keep the harness green.** The Linux test suite, the Python tool tests, the evidence check, the oracle run and the existing replay comparisons stay passing at every integration.

## 4. Milestones

Each milestone follows the path a player takes and ends with a replay comparison against the original. Function lists name what the research found; a stream may find more on the same path and ports those too, in the same batch.

### M1. Into the world

Exit: a new party stands in the start area on the world screen, on Linux and on Windows, and the `world_entry` replay matches the original at every checkpoint (party formation, the world screen with its area, party count, protagonist position and hit points).

- Game file: `CInfGame::Unmarshal` 0x5A7E40, `SaveGame` 0x5AC430, `SetupCharacters` 0x5BF6A0, `UpdateCharacterSlots` 0x5BD6B0, `SynchronousUpdate` 0x5BE900, `SelectToolbar` 0x5ADAE0, `GetProtagonist` 0x5BFC40, `CharacterExport` 0x5A1AB0; `CGameArea::Marshal` (absent) and the rest of `Unmarshal` 0x472DE0; `CTiledObject::Initialize` 0x54E830 and `SetNewResWED` 0x54E980.
- Creature file: `CGameSprite::Marshal` 0x70B2F0 and the rest of `Unmarshal` 0x70CF90; `CGameSpriteEquipment::Marshal` 0x7124C0; `CDerivedStats` constructor, `Reload` 0x4440F0, `operator=` 0x445330 and `BonusInit`; the remaining `ProcessEffectList` 0x72DE60 and `HandleEffects` 0x734550 TODOs; the `SetStat` DEX case 0x7600F6; the duplicate `sub_72DE60` stub removed and its callers pointed at `ProcessEffectList`.
- Messages: `CMessageHandler::AsynchronousUpdate` 0x4EE020, `Send` 0x4F7830, `Broadcast` 0x4F7620, local path only.
- Platform: `FindFirstFileA` returns `.` and `..` as Windows does, with a test; `UtilAssert` handles `/` paths.
- Character generation fidelity: `CompleteCharacterSkills` 0x612DE0, `CompleteCharacterWrapup` 0x613310, `UpdateCharacterStats` 0x611AF0 TODOs, `UpdateCharacterSummary` 0x610DC0, the auto-pick button 0x617D80, `UpdateMainPanel` 0x607BB0 spells TODO; `CRuleTables::GetStartingHitPoints` 0x542FA0.

The new-game path saves the game and reloads it, so the core save and load of the game, creature and area files belongs here. M4 covers the screens and cross-checks on top of it.

### M2. Move and interact

Exit: the `walk_and_talk` replay matches the original: click-to-move across the start area, open a door, start and end a dialog with the first NPC.

- AI dispatch: `CGameAIBase::ProcessAI` 0x45C730, `ExecuteAction` 0x44DC10 and the action handlers the start area's scripts use (MoveToObject, Dialog, PickUp first), `GetGlobalVariable` 0x4686C0, `CAIObjectType::Decode` 0x40B880, the `EvaluateStatusTrigger` overrides, `CMoveList::AIUpdate` 0x517300, `CAIGroup::GroupAction` 0x404D00 and `HandleFollowPath` 0x409820, `CVariableHash::MarshalToCharacter` 0x550600.
- Area: `CGameArea::GetNearest` 0x46B160, `AdjustTarget` 0x46A3D0, `SnapshotAdjustTarget` 0x46A630, `CGameSprite::ClearBumpPath` 0x6FA900, `CVisibilityMap::MarkTileLine` 0x552A50 and `BltFogOWar` 0x551FF0, `CInfinity::Scroll` 0x5D1380, `CSpawn::ExecuteSpawn` 0x76B160, `CGameSpawning::Spawn` 0x4C9CE0.
- Dialog: `CGameDialogEntry::Handle` 0x484900, `CGameDialogSprite::AsynchronousUpdate` 0x483F00, `FetchRumor` 0x483B70, `CScreenWorld::EndDialog` 0x68F9D0.
- World screen: `TogglePauseGame` 0x68DFD0, `CInfButtonArray::UpdateButtons` 0x58A340, `CGameSprite::SetModalState` 0x71FC00, `PlaySound` 0x7011E0.

### M3. Combat and spells

Exit: the `first_fight` replay matches the original through a melee fight and one spell at the start area: hit points of party and enemies at each checkpoint, and both RNG states.

- Combat: `CGameSprite::Hit` 0x73B760, `Damage` 0x73D560, `GetAC` 0x723CC0, `OneSwing` completion, `SelectWeaponAbility` 0x74F830, the weapon-set function 0x726810, `AddSpecialAbility` 0x71B9C0, the Attack action handler; `CItem::Equip` 0x4E8860, `Unequip` 0x4E8DF0 and constructors.
- Effects: `CGameEffect::CheckSave` 0x4A42F0, `ApplyEffect` for the 107 classes without one (67 in `CGameEffect.cpp`, 40 in `IcewindCGameEffects.cpp`), the four `CPersistantEffect*::ApplyEffect`, `CGameEffectList::RemoveAllOfType` 0x4C08E0, `CProjectile::DeliverEffects` 0x52A1A0, `FireSpell` 0x467C50 and `FireSpellPoint` 0x4681E0, the Spell action handler, `CheckAranceFailure` 0x7243F0's oracle case given a fixture.
- Sprite effects: `UpdateSpriteEffect` 0x709B60, `RenderSpriteEffect` 0x7093E0, `RenderDamageArrow`.
- Oracle: the pending cases from project 2 (`CheckSave`, `Hit`, `Damage`, `ProcessEffectList`) get bodies where a fixture is honest; `GetAC` and `GetStartingHitPoints` get cases.

### M4. Save, load and rest

Exit: a game saved in iwd2r loads in the original and dumps the same state, and the reverse; the `save_load` replay matches; resting works.

- `CScreenSave::RefreshGameSlots` 0x65BCF0 and the save screen, the load screen, `CInfGame::RestParty` 0x5C1160 and `CanRestParty` 0x5C1E30, `ImportCharacter` 0x5A0F50, `CStore::Marshal` 0x54B9A0, journal and variable persistence, `SwapItemPersonalInventory` 0x5BAD70.
- Cross-load: `oracle.py replay compare --load <save>` runs both sides from the same save folder; the recorder's injector already runs the original against the vanilla copy, so the save is copied into both copies' `mpsave`.

### M5. The remaining screens and rendering

Exit: replays for inventory, level-up, a store visit and world-map travel match; the Windows Sandbox suite passes with the new scenarios added.

- UI: `CScreenInventory` (`SwapWithSlot` 0x62F360, `OnDoneButtonClick` 0x6285B0 and its 12 markers), `CScreenCharacter` (`LevelUp` 0x5E56F0 and 19 markers), `CScreenStore` (12 markers, `UpdateStoreItems` 0x676EF0, `OnBuyItemButtonClick` 0x678050), `CScreenWorld::StartStore` 0x6913C0 and `StopStore` 0x691B50, `CStore::AddItemExt` 0x54C5B0, `CScreenWorldMap::OnAreaMouseClick` 0x69ACF0, the Spellbook markers.
- Rendering: `FXRenderClippingPolys` 0x5CE350, `CVidCell::Blt8To16` 0x7AF3C0 and `Blt8To24` 0x7CEEB0, `CVidTile::BltTile8To16` 0x7B22E0, `CVidPoly::FillPoly` 0x7C13A0, `CVidMode::DrawLine16` 0x797D30 and `DrawEllipse24` 0x7C8B40, the Ankheg render 0x6C3910, the `FillConvexPoly` initialisation errors recorded in `docs/followups.md`. These transfer to project 4 the day it starts, since it owns the video layer (section 5.6).
- Followups: VC6 `atol` at the remaining call sites (mapped in `compat/mfc.h` like `rand`, with oracle coverage), code-page upper-casing, the 64-bit cast sites.

## 5. Execution model

### 5.1 Streams and worktrees

Each milestone is split into streams by subsystem. A stream is one implementer agent working in its own git worktree, branched from `first`, on a batch of 10 to 30 functions. All streams of a milestone run at once; a wave is the set of streams running together. Up to six streams run in parallel.

Each stream owns a disjoint set of files for the wave, listed in its brief. `CGameSprite.cpp`, which every subsystem touches, has exactly one owner per wave; other streams that need a change there ask the coordinator, who routes it to the owner. Headers whose layout several streams read (`CGameSprite.h`, `CInfGame.h`, `CGameArea.h`) are changed only by their owner, and the coordinator merges the rest.

M1 streams:

| Stream | Owns | Work |
|---|---|---|
| game-file | `CInfGame.*`, `CGameArea.*`, `CTiledObject.*` | the game and area files, party placement, the world frame update |
| creature-file | `CGameSprite.*`, `CGameSpriteEquipment.*`, `CDerivedStats.*` | the creature file, stats, the effect-list update |
| messages-platform | `CMessage.*`, `platform/fs.cpp`, `CUtil.*` | the local message handler, the two Linux fixes |
| chargen | `CScreenCreateChar.*`, `CRuleTables.*` | character generation fidelity, starting hit points |

Later milestones split the same way: effects, combat, AI, area, dialog, UI and rendering each as a stream, with the ownership table written into the plan.

### 5.2 Coordination

The coordinator (this session) dispatches a wave, merges each stream's branch into `first` as it passes review, runs the integration gates, and dispatches the next wave. A stream rebases on `first` before its review if another stream merged first. Merge conflicts in shared headers are resolved by the coordinator.

Integration gates, run after every merge:

- linux-gcc and linux-clang build and ctest;
- `tests/test_tools.py`, `check_evidence.py`, `gen_map.py --check`, `gen_fields.py --check`, the clang-format check;
- `oracle.py run` (all case files) and `oracle.py replay compare` for every scenario enabled so far;
- the Windows build.

At the end of a milestone, additionally: the milestone's replay scenario, the Windows Sandbox suite, and a whole-milestone review on the most capable model.

### 5.3 Review policy

- A stream batch gets one review on a mid-tier model, with a fix round if needed; batches that touch combat, effects, the save format or the AI dispatcher are reviewed on the most capable model.
- A whole-milestone review on the most capable model closes each milestone.
- Findings that no stream owns are ruled on by the coordinator and recorded in the ledger.

### 5.4 Tooling that parallel work needs first (wave 0)

Before M1's streams start, one short tooling batch removes the collisions between parallel agents:

- **Ghidra.** Headless Ghidra opens the project exclusively. `make_project.headless()` takes a file lock so parallel callers queue instead of failing. `oracle.py evidence` accepts several addresses and decompiles them in one Ghidra run, so a stream drafts a batch's evidence in one call.
- **Windows build folders.** `tools/play.py`'s `WIN_CHECKOUT` becomes per-worktree (derived from the worktree's name, overridable by `IWD2R_WIN_CHECKOUT`), so streams can build x86 and run oracle cases without clobbering each other. The oracle's game copy and tables stay shared; the runner only writes under per-run folders.
- **Replays and the sandbox.** `oracle.py replay record` and `compare` take a lock on the oracle directory, because the original's run writes INI and cache state into the shared vanilla copy. `play.py --vm windows` takes a lock too: Windows runs one sandbox at a time.
- **Worktrees.** A script creates a stream worktree from `first` under `~/iwd2-re-worktrees/p3-<stream>` and removes it after merge.
- **Scenarios.** `scenarios/world_entry.txt` is written from the probe's play-through (character creation to the Done button), with checkpoints, so M1's exit criterion exists before its code.

### 5.5 Batching and evidence

A stream's batch is dispatched with: the function list with addresses, the owned files, the ruling that applies (R10a/R10b/R10c-style fixtures for oracle cases, the no-new-functions rule), and the verification commands. The implementer drafts evidence for the whole batch in one Ghidra call, ports function by function from the decompile, builds and tests on Linux, builds x86 and runs the oracle on Windows where a case exists, and commits in small commits with the attribution line. Its report lists each function as ported, partially ported (with the TODO lines) or blocked, and names any deviation.

### 5.6 Other projects in parallel

Projects 4 and 5 can run beside project 3 in their own worktrees, each with its own spec, plan and ledger, as long as the file boundaries hold:

| Project | Owns | Starts |
|---|---|---|
| 3, simulation | `src/` except the video layer; `compat/`; `platform/` except presentation | now |
| 4, render API and classic renderer | `src/CVid*`, `src/CInfinity.cpp` render functions, `src/CVisibilityMap.cpp` blits, `platform/ddraw_shim*`, `platform/present*`, the new render API | after its own spec and plan; M5's rendering stubs move to it |
| 5, IWD2EE on the player's machine | the installer that applies IWD2EE's game-file edits locally, the Lua runtime, the vanilla/IWD2EE runtime flag, new `mod/` and `tools/mod/` directories | the data and installer half now; the engine hooks after M3, because they land in combat and effects code |

The coordinator merges all three into `first`. A project that needs a change in another's files asks for it through the coordinator. The shared machine resources, Ghidra, the Windows build folders, the replay copy and the sandbox, are serialised by the wave 0 locks, which every project uses.

## 6. Verification

- **Per function:** evidence file; Linux and Windows build; oracle case for pure functions (a function whose inputs can be built from plain values or honest fixtures).
- **Per milestone:** the replay scenario named in section 4, compared with the original at every checkpoint, run twice to show determinism; Linux and Windows screenshots at the exit screen; the Windows Sandbox suite.
- **Known differences:** a difference that needs a later milestone's work is recorded in `oracle/replay/known_differences.json` with evidence and is cleared when that milestone lands; the list must be empty at the end of M5.
- **Dump coverage:** the state dump gains the fields each milestone's checkpoints need (area resref, party positions and hit points in M1; door states and dialog state in M2; enemy hit points and effect lists in M3; save slot contents in M4). `gen_fields.py` keeps the offsets generated from our headers.

## 7. Risks

- **Large functions.** `Hit`, `Damage`, `ExecuteAction` and `CInfGame::Unmarshal` are each thousands of instructions. They are ported in sub-batches with the decompile split by basic blocks, and a partial port is marked with the exact TODO range rather than guessed.
- **Decompile quality.** Ghidra's output for MFC-heavy code (CString, CPtrList) needs the project's labels; where a call target is unlabelled the stream names it in `tools/ghidra/extra_labels.txt` with its evidence.
- **Shared state in the original's data.** Replays depend on the vanilla copy's INI and cache state; the replay lock and the INI backup around each run contain this. The drift item in `docs/followups.md` stays open.
- **Merge conflicts.** File ownership prevents most; the coordinator resolves the rest in shared headers, and a stream rebases before review.
- **Model availability.** If the most capable model is unavailable (usage credits), reviews fall back one tier and the ledger records it.

## 8. Out of scope

IWD2EE and IEex behaviour (project 5). The new renderer and HD assets (project 4 and later). Multiplayer (removed). Movies (a later project). Performance work beyond what playability needs.
