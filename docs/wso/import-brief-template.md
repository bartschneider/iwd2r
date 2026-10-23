# Import batch {{id}}: bring in {{size}} functions

You are importing {{size}} functions where WillScarlettOhara's decompilation
(iwd2-re@{{fork_base}}) has a real body and ours is a stub, or where the row
already left wave 1 for the import list. His body is the starting point, not
a verdict: you adapt it, check it against the original, verify it, and record
where it came from. Where the original contradicts it, the original wins.

## Batch

Branch: `{{branch}}` (cut from `first`; the coordinator has already checked
out its worktree — work there, do not create your own clone).

Files you own (do not touch functions outside them; other streams own the rest):

{{files}}

Rows, biggest first:

{{rows}}

Context bundles (one per row; read these before the source):

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
   record the refusal in the report and in `docs/wso/import-parked.json` as
   `{address, name, file, batch, needs}` with `needs` of
   `"fork body contradicts the exe: <what differs>"`. Three of seven fork
   bodies in batch i002 did.
2. **Every commit that brings in his code carries exactly this trailer**, one
   per commit, as the last line of the message:

       Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)

   The sha is the full 40 characters of {{fork_base}}. A commit with none of
   his code has no trailer.
3. **The body must compile under the platform's toolchain.** Adopt his logic,
   not his build assumptions; fix what does not compile on our side and say
   so in the commit message.
4. **Evidence is mandatory, kind `trace` or better.** For every function in
   the batch write `docs/evidence/<ADDRESS>.md` (address in caps, no `0x`),
   front matter `address:` / `name:` / `kind:`. The kind is `trace`,
   `replay` or `oracle`; `read-only` is not enough here. A `trace` file must
   cite its source in `trace_source`, in one of exactly two forms:

       WillScarlettOhara/iwd2-re@<sha> docs/<name>-recovery.md
       frida <probe name> orig:<sha256> ours:<sha256> compare=0

   The note must exist at that sha (`git ls-tree {{fork_base}} docs/`) and
   must name the function by address or short name; his source files are not
   traces, and the gate reads the note and fails one that does not cover the
   function.

   Rules math (attack rolls, saving throws, damage, effect application)
   never accepts a trace: those functions need `kind: oracle`.
   **Measured rows.** After your first report the driver runs the Frida
   phase over every row (`world_entry`, `first_fight`, `walk_and_talk`):
   a row that behaves like the original gets its evidence rewritten to
   `kind: trace` by the driver. You do not write the `frida` citation.
   A row already at `kind: trace`, `replay` or `oracle` is kept: its evidence
   is never rewritten, and if the phase measures it different you only note
   that in your report (nothing undone, no park). A `read-only` row the phase
   reports as different is handled like an unmeasured one (the phase runs
   once per batch): edit it back and park it, with `needs` of
   `frida different: <the diff detail>`.
   **Unmeasured rows.** A row the phase reports as "exercised by no
   scenario" cannot close. In your fix round edit that function back to the
   merge base's body (`git show <merge-base>:<path>` shows it; do not
   replace a file that holds other rows' changes), delete
   `docs/evidence/<ADDRESS>.md`, and append an entry to
   `docs/wso/import-parked.json` (a JSON list; create it as `[]` first if
   missing):

       {"address": "0x...", "name": "...", "file": "src/...", "batch": "{{id}}",
        "needs": "world_entry, first_fight, walk_and_talk: not exercised"}

   `needs` names what was tried. Commit all three changes together.
5. **No verdicts.** This is an import: nobody decides fork, ours or neither,
   so write no `verdict` lines, no deciding case ids and no "disagreed with
   WSO" or "corroborated by WSO" phrase. The driver closes the batch with
   `close --kind import`, which requires only the evidence kind above.
   **Dependencies.** A stub dependency that sits inside your own files is
   imported with the row, in its own commit with its own trailer, and with
   its own `docs/evidence/<ADDR>.md` (the gate demands one for every touched
   function). A dependency outside your files parks the row: edit that
   function back to the merge base's body, delete the parked row's evidence file and append it to `docs/wso/import-parked.json` with `needs`
   of `"dependency outside the batch's files: <name>"`.
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
   changes. Workers only add and commit. The driver stops a batch that
   rewrites history.

## Report back

Write a section headed `## Imported`, with one line per function:

    imported <0xADDR> <evidence kind>

Then list the commits you made, the evidence files you wrote and anything you
could not import. Do not write a `## Verdicts` section. You do not
fast-forward anything yourself; the driver records and closes.
