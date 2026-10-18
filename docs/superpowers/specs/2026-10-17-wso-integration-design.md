# WSO integration — design (2026-10-17)

Status: draft for Thierry's review. Nothing in this document runs until he approves it and a plan is written from it.

## 1. Goal

Absorb the parallel recovery effort at `WillScarlettOhara/iwd2-re` (the "WSO fork") into iwd2r without lowering the verification bar, and later port his GPU renderer into iwd2r's platform layer. The aim is to reach a playable engine sooner than either project would alone: his tree carries about five months of recovered engine code; ours carries the Linux build, the oracle, the replays and the evidence gate.

## 2. Inputs and what we measured

Both trees descend from the same upstream commit (Batalov `4fa36bcd`, 2023-12-29). The fork's `main` (`wso/main`, fetched 2026-10-17 at 22:20 EDT) is 1,572 commits past it; ours (`first`) is 1,295.

Function-level comparison over the 201 shared `src/*.cpp` files (`tools/xrec.py`, a brace-matching parser that normalises bodies; approximate but symmetric):

| Category | Functions |
|---|---|
| Identical real bodies in both trees | 4,337 |
| Stubs in both | 1,971 |
| His recovery where ours is still at upstream | 528 |
| His real body where ours is a stub | 26 |
| Functions only he has in shared files | 746 |
| Real functions in his 14 files we do not have | 159 |
| Our recovery where his is still at upstream | 279 |
| Our real body where his is a stub | 85 |
| Both recovered independently, bodies differ | 406 |

A wholesale `git merge` of `wso/main` into `first` produces 118 conflicted files (115 in `src/`), with `CGameSprite.cpp` carrying about 18K added lines on each side. Wholesale merge is ruled out.

His verification: Frida traces of the real `IWD2.exe` compared against his build, with a recovery note per function under `docs/*-recovery.md`, and a stated truth hierarchy of binary over Ghidra decompile. His build is Windows-only (VS2019, MFC, x86).

## 3. Licensing and attribution

- The engine base stays under the Sustainable Use License, as upstream requires.
- The fork's author has told Thierry his additions are MIT. This is not yet recorded in writing where others can see it. Thierry chose to proceed without waiting. Until a written grant exists, every import commit carries an `Imported-From: WillScarlettOhara/iwd2-re@<sha> (MIT per author, 2026-10-17)` trailer, and `docs/THIRD_PARTY.md` gets an entry naming him, the repository, and the grant. When he publishes a LICENSE or posts the grant on GitHub, the entry gets the link.
- IEex, the extender his renderer is built into, is Bubb's work. His MIT grant cannot cover that base. The renderer port (section 7) must not carry IEex code, only his own.
- Nothing from his IWD2EE branch enters this repository: it is game-data overrides and compiled binaries, excluded by the bring-your-own-copy rule and by the gate.

## 4. Scope

In scope: the recovery waves in section 6 and the tooling in section 5. The renderer port is designed here only to the extent needed to reserve its place; it gets its own spec once the source is in hand.

Out of scope: any change to the platform layer or the Linux build during the recovery waves; wholesale merge; adopting his build system; his test scaffolding (`AutoUI.cpp`) unless a later wave shows a need.

## 5. Mechanism

**Remote.** `wso` is a remote of the repository, fetched read-only. His tree is untrusted input: read and compare, never execute scripts from it.

**Import unit.** The function, grouped into waves by subsystem. For a file where only he changed anything, the whole file comes over. For a shared file, the import is per function: his body replaces ours where ours is upstream or a stub; where both recovered, section 6 wave 1 decides.

**Authorship.** Imports are made with `git checkout wso/main -- <file>` or manual per-function edits, committed by the importing agent with the trailer above. History is not merged; the trailer and THIRD_PARTY carry the attribution. (Merging with history was considered and rejected: it forces the 118-conflict resolution into one step and blocks running the gate per wave.)

**Evidence.** `tools/check_evidence.py` gains a kind, `trace`, meaning "verified by the WSO fork's Frida-trace comparison; see its recovery note". An evidence file of that kind cites the fork commit and the note path. It satisfies the gate for imported functions outside rules math. Rules math (attack rolls, saving throws, damage, effect application) keeps the oracle requirement from the 2026-10-17 process note, imported or not. Where the two trees agree on a function we recovered read-only, the evidence file records the agreement as corroboration without changing its kind.

**Gate per wave.** Each wave ends with the full gate on both compilers, the tool tests, the evidence check, and the replay compares (`world_entry`, `walk_and_talk`, `first_fight`) unchanged or improved. A wave that regresses a replay is reverted, not patched forward.

## 6. Waves

0. **Tooling.** `tools/xrec.py` lands in the repo with a test; `check_evidence.py` learns `kind: trace`; THIRD_PARTY entry; the coordinator note. No engine code.
1. **Disagreements.** The 406 functions both trees recovered differently, ranked by size and by whether we hold oracle or replay evidence. For each: an oracle case where the function is pure, a replay checkpoint otherwise; the case decides which body is right, and the losing side's error gets a one-line note in the evidence file. Expected yield: real bugs on both sides, and corroborated evidence for the rest.
2. **Clean gain, projectiles and spell visual effects.** `CProjectile.cpp` (+11K lines on his side) and the effect lifecycle files. Highest value per import; rules-math adjacent, so oracle coverage on the damage paths.
3. **Clean gain, screens.** Store, inventory, character, character creation. Replay-verified via the chargen and store scenarios.
4. **Clean gain, AI and messaging.** `CGameAIBase`, `CMessage`, `CNetwork`, `CGameArea`. Replay-verified.
5. **His files we lack.** The 14 files and 159 functions, plus the 746 functions only he has in shared files. Each gets evidence of kind `trace` or better.
6. **Status.** Adopt his byte-weighted `.text` recovery measure as a tracked number in the README, computed with our own script.

Waves 2 to 5 can run as parallel streams in worktrees after wave 1, each owning disjoint files, the usual rule.

## 7. Renderer port (blocked on source)

What exists: a GPU pipeline inside `IEexHelper.dll` with a tile atlas (upload and draw), fog of war as a texture, an off-thread present, HUD compositing from its own framebuffer, scaled present, and a 3D lightning polyline. His IWD2EE branch ships it compiled; the C++ is not public.

Blocked until the author provides the C++ for IEexHelper's renderer. When it arrives, a separate spec covers: mapping his hook points to the engine functions we own, placing the pipeline behind the existing GPU presenter, keeping the classic presenter as the replay reference, and what of it is his code versus IEex's.

## 8. Process and budget

The coordinator session stays on glm-5.3; subagents on glm-5.3-flash under the forced override. The 5-hour Z.ai allowance is 12,000 credits; the Flash window of 2026-10-17 used 5,571 in 4 h 40 min with up to eight agents. Agents commit WIP before any gate or compare run. The coordinator records each wave's state in the M5 ledger at dispatch and close.

## 9. Risks

- The function parser is approximate; wave 0 includes a test and a manual spot check of 20 classifications.
- His code assumes Windows and MFC in places; imports must compile under the platform layer on Linux or wait for a shim decided case by case, never by editing the platform layer inside a recovery wave.
- The MIT grant is verbal until he writes it down. If he withdraws, imported functions must be re-recovered; the trailers make them findable.
- Name drift between the trees (same function, different names) hides matches from the parser; wave 1 reports unmatched-but-similar functions by address when the evidence files name one.

## 10. Success criteria

- All 406 disagreements resolved with oracle or replay evidence.
- At least 1,400 imported functions with evidence of kind `trace` or better, zero without.
- Gate green on both compilers after every wave; replay compares never regress.
- README byte-recovery figure at or above his 78.9%, computed by our script.
- Zero bytes of his IWD2EE branch in the repository.
