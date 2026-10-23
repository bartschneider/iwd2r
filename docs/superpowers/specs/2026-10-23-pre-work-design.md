# Before the recovery waves: six improvements

Date: 2026-10-23. Status: ordered by thierbig ("do all the six things"). Runs beside the
Frida plan (`docs/superpowers/plans/2026-10-23-frida-imports.md`), whose Task 5 (typed hook
tables and a frame window) is improvement (a).

## Why (what today showed)

- (a) 236 of 240 verdicts rest on a dump reading; the Frida compare flags noise (pointer-sized
  ints typed as ints, byte returns read as 32 bits, per-frame hit totals). → Frida plan, Task 5.
- (b) Functions no scenario reaches cannot be measured; ten scenarios sit in `scenarios/pending/`
  unrecorded (store, inventory, level_up, load_game, world_map, action_bar, biography,
  equip_shots, save_load_screens, save_load).
- (c) The context bundles are thin because `ghidra-exports/` holds only `functions.tsv` and
  `callgraph.tsv`, no per-function decompile; b013's Flash worker spent 300 turns running
  objdump by hand (848 credits, 90 minutes for 20 rows).
- (d) A worker reverted a trunk commit (i002) and another reset its branch mid-batch (b013);
  the transcript checks refuse launches and whole-file reads but not history rewrites.
- (e) One gate (about 10 minutes, two presets built serially, no compiler cache) caps closes at
  five or six an hour; two workers per pool are blocked by the caveats in the wave-0b morning
  report (signal-handler lock, a stop mid git write, a stop that still launches, MAP.md sync
  between parallel batches, uncommitted import-list/import-state, GLM wrapper isolation,
  two tests leaking files under -W error).
- (f) Will pushes daily; the wave compares against a pin from last week and the re-comparison
  at his tip is manual.

## What changes

**C. Decompiles in the bundles.** A one-time headless Ghidra export writes one
`<ADDR>.c` per function of the vanilla exe under
`${IWD2R_HOME:-~/.local/share/iwd2r}/ghidra-exports/decompile/` (the existing
`tools/ghidra/DecompileFunctions.java`, driven by `tools/ghidra/make_project.py`'s headless
runner under its lock, in address-range chunks so a crash loses one chunk). `tools/
assemble_context.py bundle` includes the row's decompile and the first 40 lines of each
direct caller's; the briefs tell workers that objdump is for confirming a reading, not for
finding it. Exports live outside the repo (like the other exports); the repo gains the
export command (`tools/ghidra/export_decompiles.py`) and a test on a fixture.

**D. Workers never rewrite history.** `driver.transcript_checks` gains the kind `history`:
a Bash command in a worker transcript matching `git` with `reset`, `rebase`, `revert`,
`restore`, `checkout --`/`checkout <path>` on a tracked path, `clean`, `stash`, `commit
--amend`, `filter-repo`, `push`, `branch -D/-d`, `update-ref`, `reflog expire`, or
`gc --prune` is a terminal exception, like `launch`; `git add`, `commit` (no `--amend`),
`status`, `diff`, `log`, `show`, `ls-tree`, `merge-base`, `rev-parse`, `blame` are the
allowed set. The brief template states the rule in one line.

**E. Two workers per pool and a faster gate.** The seven caveats are closed one by one with a
test each; MAP.md is regenerated once at close on the merged tree (not in the batch), so
parallel batches never conflict on it; import-list and import-state are committed at close
like wave-state; the `claude-glm` wrapper takes a lock while it re-links its config dir, so
concurrent launches do not race. The gate uses `ccache` for both presets when it is installed
(CMAKE_CXX_COMPILER_LAUNCHER through the presets) and reports the per-step time in its
summary, so the next gate change is measured.

**F. Will's tip, automatically.** `wave.py recompare` fetches `wso`, runs `tools/xrec.py
<base> <tip> first --json`, and appends to the wave state the `both_changed_differ` rows that
are not yet decided and not already queued, under a new `fork_base` recorded per batch (each
batch carries the pin its rows were compared at; the brief, the evidence phrase and the close
check use the batch's pin). The driver runs `recompare` once when the wave-1 list is exhausted
and reports `recompare: N new rows at <tip7>` before exiting 0 or continuing.

**B. Scenario recording.** Each pending scenario is recorded through `tools/oracle.py replay
compare` (original in the sandbox, ours headless), checked for a MATCH at every checkpoint,
and promoted from `scenarios/pending/` to `scenarios/` with its `.expect.json`; one that
cannot match is left pending with the divergence recorded in the ledger. These captures feed
the Frida phase's scenario list (`truth.FRIDA_SCENARIOS`) once they exist.

## Not in scope

Parallel gates with separate build directories (measured first, decided later); the
history rewrite (cancelled for good); engine gaps (queued as a batch after wave 1, listed in
`docs/wso/engine-gaps.json`).

## Testing

Tool tests, no credits: the export command on a fixture (chunking, resume, one file per
function); the bundle with and without a decompile present; the `history` transcript kind
(every forbidden form, every allowed form); each caveat's regression test; the gate summary
format; `recompare` on a fixture repo with a moved fork (new rows appended, decided and queued
rows untouched, per-batch pin used by cut/brief/close). Acceptance: the real export (one
run), one two-worker driver run on a disposable state copy (dry-run), one recompare against
Will's real tip, and the scenario captures (launch budget per scenario: 2).
