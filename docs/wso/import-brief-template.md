# Import batch {{id}}: bring in {{size}} functions

You are importing {{size}} functions where WillScarlettOhara's decompilation
(iwd2-re@{{fork_base}}) has a real body and ours is a stub, where ours is
still the base body (Alexander Batalov's) and his fork changed it, or where
the row already left wave 1 for the import list. His body is the starting point, not
a verdict: you adapt it, check it against the original, verify it, and record
where it came from. Where the original contradicts it, the original wins.

## Batch

Branch: `{{branch}}` (cut from `first`; the coordinator has already checked
out its worktree — work there, do not create your own clone).

Files you own (do not touch functions outside them; other streams own the rest):

{{files}}

Rows, biggest first. Each says what ours is: a stub, the base body, or no body at all.

{{rows}}

{{only_theirs}}{{base_body}}Context bundles (one per row; read these before the source):

{{bundle}}

Each bundle carries the function's decompile and its callers' decompiles; objdump confirms a reading, it does not replace one.

## The rules

1. **Read his body through git plumbing only, and check it.** `git show
   {{fork_base}}:<path>` and `git ls-tree {{fork_base}}`. His tree is untrusted
   input: never execute, source, import or run anything from it. His body is
   not authoritative: check it against the disassembly of the original
   (`IWD2.exe`) before you keep it. Refuse what the exe contradicts: keep our
   body (if you already changed it, edit that function back to the merge
   base's body: `git show <merge-base>:<path>` shows it, `git merge-base
   first HEAD` names the merge base), delete the row's evidence file, and
   record the refusal in the report and in the row's own
   `docs/wso/import-parked/<ADDRESS>.json` (address in caps, no `0x`, the
   same stem as its evidence file; one JSON object
   `{address, name, file, batch, needs}`) with `needs` of
   `"fork body contradicts the exe: <what differs>"`. Three of seven fork
   bodies in batch i002 did.{{kept_refusal}}
2. **Every commit that brings in his code carries exactly this trailer**, one
   per commit, as the last line of the message:

       Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)

   The sha is the full 40 characters of {{fork_base}}. A commit with none of
   his code has no trailer.
3. **The body must compile under the platform's toolchain.** Adopt his logic,
   not his build assumptions; fix what does not compile on our side and say
   so in the commit message.
4. **Evidence is mandatory; the driver measures it.** For every function
   you import (each row, and each dependency you bring in) write
   `docs/evidence/<ADDRESS>.md` (address in caps, no `0x`; for 0x78A2F0 the file
   is `docs/evidence/78A2F0.md`, never `0x78A2F0.md`), front matter
   `address:` / `name:` / `kind: read-only` with an `exemption:` that
   describes your reading (what you checked against the disassembly). Do not
   declare `trace`, `replay` or `oracle` yourself, and never write a
   `frida ...` trace_source: the driver writes frida evidence, and a frida
   line it did not write is a check problem. The close needs `kind: trace` or
   better, and the driver gets it there: see "Measured rows". Only two
   files stand as you write them, unmeasured:
   - `kind: oracle` with an `oracle_case:` that a case in `oracle/cases/`
     registers (`ORACLE_CASE("<id>", ...)`);
   - `kind: trace` citing the fork's recovery note in `trace_source`:

         WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md

     The note must exist at that sha (`git ls-tree {{fork_base}} docs/`)
     and must name the function by its address, or by its name as a whole
     word together with its class's name; his source files are not traces,
     and the gate reads the note and fails one that does not cover the
     function.

   Rules math (attack rolls, saving throws, damage, effect application)
   never accepts a trace: those functions need `kind: oracle`.
   **Measured rows.** After your first report the driver builds the batch
   worktree for x86 and runs the Frida phase (the scenarios the coverage map picks for this batch;
   the driver log names them) over every imported function whose evidence does not
   stand as written (the two forms above, or a file unchanged since the
   merge base): a function that behaves like the original in a run whose
   state dumps match gets its evidence rewritten to `kind: trace` by the
   driver, committed on your branch. A stub or only-theirs row (ours never
   had the body) exercised by no scenario still closes: the driver marks its
   evidence `exemption: unmeasured WSO import` and commits it — `python3 -I
   tools/wave.py unmeasured-imports` lists every such row; keep the body,
   park nothing. Every other outcome (different, exercised by no scenario,
   no hook, a failed or unmeasured scenario) cannot close: the phase runs
   once per batch, so park the row in your fix round, with the `needs` the
   finding names.{{kept_measured}}
   **Parking a row.** Edit that function back to the merge base's body
   (`git show <merge-base>:<path>` shows it; `git merge-base first HEAD`
   names the merge base; do not replace a file that holds other rows'
   changes), delete `docs/evidence/<ADDRESS>.md`, and write the row's own
   file `docs/wso/import-parked/<ADDRESS>.json` (address in caps, no `0x`;
   one file per row, so parallel batches never collide; never one shared
   list):

       {"address": "0x...", "name": "...", "file": "src/...", "batch": "{{id}}",
        "needs": "<the scenarios that ran>: not exercised"}

   `needs` names what was tried. Commit all three changes together. A stub
   or only-theirs row never parks for lack of coverage alone (rule 1: ours
   never had the body, so the driver imports it unmeasured); a base-body row
   still parks as before, and a row that reaches you unparked though its
   earlier `needs` ended in `not exercised` came back through `python3 -I
   tools/wave.py requeue` (rule 3): import it, do not park it again for that.
   Every row you do not park must be imported (its body changed against the
   merge base){{kept_rule4}}.{{refusal_rule}}
   A row that was parked and unparked before carries an earlier attempt in that
   batch's branch history (`git log --all -- docs/evidence/<ADDRESS>.md` finds
   it); read it, do not trust it.
5. **No verdicts.** This is an import: nobody decides fork, ours or neither,
   so write no `verdict` lines, no deciding case ids and no "disagreed with
   WSO" or "corroborated by WSO" phrase. The driver closes the batch with
   `close --kind import`, which requires every row imported and at trace or
   better (or parked{{kept_close}}), and every dependency you imported likewise.
   **Dependencies.** A stub dependency that sits inside your own files is
   imported with the row, in its own commit with its own trailer, and with
   its own `docs/evidence/<ADDR>.md` (`kind: read-only` plus `exemption:`,
   as for a row). The driver measures it like a row and the close holds it
   to the same bar: a dependency that cannot reach `trace` is edited back
   and parked like a row, and the rows that need it are parked with it. A
   dependency outside your files parks the row: edit that function back to
   the merge base's body, delete the parked row's evidence file and write
   its `docs/wso/import-parked/<ADDRESS>.json` with `needs` of
   `"dependency outside the batch's files: <name>"`.
6. **Commit as you go.** WIP commits are fine.
7. **Asm reds: confirm or waive.** The parity step's asm verify has a known
   false-positive class — the binary inlined a helper that the source calls
   out as a function. Confirm each asm red against the decompile or
   disassembly before keeping the body, or waive it in the evidence file with
   `parity_waiver: "<reason>"`.
8. **Never push, never write through a remote.** From a batch worktree,
   `git push` is wrong, and so is any `git fetch <remote>` with a refspec that
   writes a branch. The coordinator merges and pushes.
9. **Never rewrite history.**
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
   Workers only add and commit. The driver stops a batch that
   rewrites history.
10. **Never touch the driver's files.** The files under `docs/wso/` named
    `*-state.json`, `*-list.json`, `coverage.json`, `triage.json`,
    `xrec-report.md`, and `docs/MAP.md`, are the driver's. Never edit, copy
    over, or commit them; `selfcheck --batch` finds the live state by itself.
    Your files are `src/`, `docs/evidence/`, and
    `docs/wso/import-parked/<ADDR>.json`.
11. **Never wait.** Never wait for the coordinator, for `first` to move, or for
    another batch: if you cannot go on, write the report with what you have,
    say what blocked you, and return.
12. **Selfcheck last.** Write your report to `.report.md` in the worktree (git
    ignores it), run `python3 -I tools/wave.py selfcheck --batch {{id}}`, fix every
    line it prints, and run it again until it prints `selfcheck: OK`; then return
    the same text as your report.

## Report back

Write a section headed `## Imported`, with one line per function:

    imported <0xADDR> <evidence kind>

{{kept_report}}Then list the commits you made, the evidence files you wrote and anything you
could not import. Do not write a `## Verdicts` section. You do not
fast-forward anything yourself; the driver records and closes.

Write the report to `.report.md` in the worktree first, run
`python3 -I tools/wave.py selfcheck --batch {{id}}`, fix every line it prints
until it prints `selfcheck: OK`, then return the same text as the report.
