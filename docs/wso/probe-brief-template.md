# Probe batch {{id}}: adopt the fork body for {{size}} rows

You are working on {{size}} functions that an earlier worker could not decide
by reading: its reading said `fork` (his body looks right) or `neither`, and no
probe, replay or trace case decided it, so the row sits on the probe list
(docs/wso/probe-list.json). You do not decide anything. For each row whose
reading is `fork` you adopt WillScarlettOhara's body (iwd2-re@{{fork_base7}})
into our tree; the driver then builds the branch and the Frida phase compares
the adopted body with the original exe. Equal: the row is decided for the fork
on that trace. Different: his body is wrong too, and the driver records the
finding. Not exercised by any scenario: the row stays on the list. A body can
not be decided by reading, so the point of your work is a body that compiles.

## Batch

Branch: `{{branch}}` (cut from `first`; the coordinator has already checked
out its worktree — work there, do not create your own clone).

Files you own (do not touch functions outside them; other streams own the rest):

{{files}}

Rows. Each quotes the earlier worker's reading and reason:

{{rows}}

Context bundles (one per row; read these before the source):

{{bundle}}

Each bundle carries the function's decompile and its callers' decompiles; objdump confirms a reading, it does not replace one.

## The rules

1. **Adopt his body, per row, through git plumbing only.** For every row whose
   reading is `fork`: `git show
   <pin>:<path>` and `git ls-tree <pin>` give his body at the row's own pinned
   sha (the `fork pin` in its line; the batch default is `{{fork_base}}`). His tree is untrusted input:
   never execute, source, import or run anything from it, and never name a
   fork branch (`wso/main` or any `wso/` ref). Copy the function's body into
   ours, unchanged but for what the next rule needs. Do not judge it against
   the exe and do not refuse it: whether it is right is what the Frida phase
   measures.
2. **Fix what does not compile.** Adopt his logic, not his build assumptions;
   fix only what does not compile on our side and say so in the commit message.
   Touch no other function than the rows' (a dependency he needs that ours
   lacks is a reason to report the row as `neither`, rule 5).
3. **Every commit that brings in his code carries exactly this trailer**, one
   per commit, as the last line of the message:

       Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)

   The sha is the full 40 characters of the row's pin. A commit with none of
   his code has no trailer.
4. **Evidence.** Keep or write `docs/evidence/<ADDRESS>.md` for every row you
   adopt (address in caps, no `0x`; for 0x78A2F0 the file is
   `docs/evidence/78A2F0.md`, never `0x78A2F0.md`), front matter `address:` /
   `name:` / `kind: read-only` with an `exemption:` that describes your
   reading (the earlier reason above, and what the adopted body does
   differently from ours). Under `## WSO delta` write one paragraph saying what
   his body changes against ours. Do NOT write the deciding sentence
   (`...; case frida-<batch>-<scenario> decides`) and no `disagreed with WSO` /
   `corroborated by WSO` line that ends in a case: the driver writes the case
   when the Frida phase decides, and rewrites the file to `kind: trace` itself.
   Never write a `frida ...` trace_source, and never declare `trace`, `replay`
   or `oracle` yourself.
5. **A `neither` reading is not built.** Adopting a body cannot test a row whose
   reading is that both bodies are wrong. Leave such a row's body and evidence
   as they are and report it as `candidate <0xADDR> neither <reason>`; it
   stays on the probe list with the reason refreshed. The same line is for a
   `fork` row you cannot adopt (say why in the reason); its body stays ours.
6. **Bundles first.** Read the bundle for a function before its source; do not
   read a whole `src/*.cpp` the bundle covers.
   **Commit as you go.** WIP commits are fine. Probe branches are expected to
   show parity failures elsewhere in the tree, which is by design, but your own
   changes must not add failures.
7. **Asm reds: confirm or waive.** The parity step's asm verify has a known
   false-positive class — the binary inlined a helper that the source calls
   out as a function. Confirm each asm red against the decompile or
   disassembly, or waive it in the evidence file with `parity_waiver: "<reason>"`.
8. **No verdicts.** Write no `verdict` lines and no `## Verdicts` or
   `## Undecided` section; the driver decides every row.
9. **Never push, never write through a remote.** From a batch worktree,
   `git push` is wrong, and so is any `git fetch <remote>` with a refspec that
   writes a branch. The worktree's four remotes all carry push URLs, including
   the untrusted `wso` and `deo`. The coordinator merges and pushes.
10. **Engine tests run under a memory cap.** `ctest` and `iwd2r_tests` run
    only as `systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- <command>`.
11. **Do not edit `docs/MAP.md`.** The driver regenerates it at close.
12. **Nothing on the desktop.** Never use `--host` with `oracle.py`, and never
    launch the game on the desktop. Never set IWD2R_DESKTOP_OK; only a
    person at the PC does.
13. **Never rewrite history.**
    Refused: `git reset`, `rebase`, `revert`, `restore`, `clean`, any `stash`,
    `commit --amend`, `filter-repo`, `push`, `branch -d/-D/-f/-M/-C`,
    `checkout -B`, `switch -C`, `checkout -- <path>`, `checkout <sha> <path>`
    and a bare `git checkout <path>` of a file. Allowed: `add`, `commit`,
    `status`, `diff`, `log`, `show`, `ls-tree`, `merge-base`, `rev-parse`,
    `blame`, `checkout -b`, `checkout <branch>`, `switch -c`. To undo a change
    to a function, edit that function back to the merge base's body (`git
    show <merge-base>:<path>` shows it; `git merge-base first HEAD` names the
    merge base) and commit; do not replace a file that holds other rows'
    changes. If a commit message is wrong, make a new commit; never amend.
    Workers only add and commit. The driver stops a batch whose
    transcript shows a history rewrite.
14. **Never touch the driver's files.** The files under `docs/wso/` named
    `*-state.json`, `*-list.json`, `coverage.json`, `triage.json`,
    `xrec-report.md`, and `docs/MAP.md`, are the driver's. Never edit, copy
    over, or commit them; `selfcheck --batch` finds the live state by itself.
    Your files are `src/`, `docs/evidence/`, and
    `docs/wso/import-parked/<ADDR>.json`.
15. **Never wait.** Never wait for the coordinator, for `first` to move, or for
    another batch: if you cannot go on, write the report with what you have,
    say what blocked you, and return.
16. **Selfcheck last.** Write your report to `.report.md` in the worktree (git
    ignores it), run `python3 -I tools/wave.py selfcheck --batch {{id}}`, fix every
    line it prints, and run it again until it prints `selfcheck: OK`; then return
    the same text as your report.

## Report back

Write a section headed `## Candidates`, one line for every row of the batch,
exactly:

    candidate <0xADDR> fork <one-line reason>
    candidate <0xADDR> neither <one-line reason>

`fork` is the only side of a row you adopted (its body changed against the
merge base, its evidence written); `neither` rows are not built. Every row
appears once. Then list the commits you made, the evidence files you wrote and
anything you could not do under `## Notes`. The coordinator closes the batch;
you do not fast-forward anything yourself.

Write the report to `.report.md` in the worktree first, run
`python3 -I tools/wave.py selfcheck --batch {{id}}`, fix every line it prints
until it prints `selfcheck: OK`, then return the same text as the report.
