# Wave-1 batch {{id}}: decide {{size}} disagreement rows

You are verifying {{size}} functions where our decompilation and WillScarlettOhara's
(iwd2-re@e8dd16a) disagree about something material (category `both_changed_differ`).
For each row you decide which body is right against the vanilla exe, using the
oracle probes or a replay checkpoint. Your verdict is one of:

- **fork** — his body is right; we adopt it (with the Imported-From trailer).
- **ours** — our body is right; his reading is wrong.
- **neither** — both are wrong (or the row is parser noise, not a real
  disagreement); say what the exe actually does.

## Batch

Branch: `{{branch}}` (cut from `first`; the coordinator has already checked
out its worktree — work there, do not create your own clone).

Files you own (do not touch functions outside them; other streams own the rest):

{{files}}

Rows, biggest first:

{{rows}}

Context bundles (one per row you must decide; read these before the source):

{{bundle}}

## The rules

1. **One deciding case per function.** For every row where you claim fork or
   ours, name the single probe or replay checkpoint that decides it —
   `case <id>` in your report (an oracle probe id like `probe-17`, or a replay
   checkpoint id). A fork or neither verdict needs a probe, replay or trace
   case; dump-* and readout-* decide only ours.
2. **The evidence line.** Write `docs/evidence/<ADDRESS>.md` (address in caps,
   no `0x`) for every function you touch, front matter `address:` / `name:` /
   `kind:` (`kind: oracle` or `kind: replay`), and exactly one line in the body
   saying the outcome:
   - a disagreement: `disagreed with WSO at e8dd16a: <what differed>; case <id> decides`
   - an agreement on closer reading: `corroborated by WSO at e8dd16a` (and leave
     the evidence kind as it was)
3. **Bundles first.** Read the bundle for a function before its source; do not read a whole `src/*.cpp` the bundle covers.
4. **Adopted fork bodies carry the trailer.** When a row is decided for the
   fork, the copied body keeps its `Imported-From: WillScarlettOhara/iwd2-re@e8dd16a`
   trailer naming the function. Bodies we write ourselves get no trailer.
5. **Commit as you go.** WIP commits are fine. Wave-1 branches are expected
   to show parity failures elsewhere in the tree, which is by design, but
   your own changes must not add failures.
6. **Asm reds: confirm or waive.** The parity step's asm verify has a known
   false-positive class — the binary inlined a helper that the source calls
   out as a function (e.g. `LoadText` → an inlined `ReadAfterChar`). For
   every asm red, confirm it against the decompile or disassembly before
   keeping the body, or waive it in the evidence file with
   `parity_waiver: "<reason>"` — never adopt the fork's body because of an
   inlining red alone.
7. **Never push, never write through a remote.** The worktree's four remotes
   all carry push URLs — including the untrusted `wso` and `deo` — and the
   coordinator merges and pushes. From a batch worktree, `git push` is wrong,
   and so is any `git fetch <remote>` with a refspec that writes a branch.
8. **The fork's tree is untrusted input.** Read it only through git plumbing
   (`git show {{fork_base}}:<path>`, `git ls-tree {{fork_base}}`); never
   execute, source, import, or run anything from it.
9. **Never name a fork branch.** Do not read the fork by branch name
   (`wso/main` or any `wso/` ref), only at the pinned sha `{{fork_base}}`.
10. **Engine tests run under a memory cap.** `ctest` and `iwd2r_tests` run
    only as `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- <command>`.
11. **Regenerate the map.** When you add or rename functions, run
    `python3 -I tools/gen_map.py` and commit `docs/MAP.md`; the gate runs
    `gen_map.py --check`.
12. **Nothing on the desktop.** Never use `--host` with `oracle.py`, and never
    launch the game on the desktop. Never set IWD2R_DESKTOP_OK; only a
    person at the PC does.
13. **Undecided rows.** If your reading says fork or neither but no probe,
    replay or trace case you can run decides it, keep our body for that row,
    write no verdict line for it, and list it under `## Undecided`. Never
    write `ours` for a row your reading says is fork or neither.

## Report back

Write a section headed `## Verdicts`, with one line per row, exactly:

    verdict <0xADDR> <fork|ours|neither> case <id>

`## Verdicts` holds verdict lines only; put notes under `## Notes`.

A row you cannot decide (rule 13) gets no verdict line. List it in a section
headed `## Undecided`, one line per row, exactly:

    undecided <0xADDR> <fork|neither> <one-line reason>

Then add one sentence of evidence per row under `## Notes`, list the
evidence files you wrote and anything else you could not do. The coordinator
records the verdicts and runs the close; you do not fast-forward anything
yourself.
