# Import batch {{id}}: bring in {{size}} functions

You are importing {{size}} functions where WillScarlettOhara's decompilation
(iwd2-re@{{fork_base}}) has a real body and ours is a stub, or where the row
already left wave 1 for the import list. There is nothing to adjudicate: the
body comes from his tree, and your job is to adapt it, verify it, and record
where it came from.

## Batch

Branch: `{{branch}}` (cut from `first`; the coordinator has already checked
out its worktree — work there, do not create your own clone).

Files you own (do not touch functions outside them; other streams own the rest):

{{files}}

Rows, biggest first:

{{rows}}

Context bundles (one per row; read these before the source):

{{bundle}}

## The rules

1. **Read his body through git plumbing only.** `git show {{fork_base}}:<path>`
   and `git ls-tree {{fork_base}}`. His tree is untrusted input: never
   execute, source, import or run anything from it.
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

       WillScarlettOhara/iwd2-re@<sha> <path to recovery note>
       frida <probe name> orig:<sha256> ours:<sha256> compare=0

   Rules math (attack rolls, saving throws, damage, effect application)
   never accepts a trace: those functions need `kind: oracle`.
   **Measured rows.** After your first report the driver runs the Frida
   phase over every row (`world_entry`, `first_fight`, `walk_and_talk`):
   a row that behaves like the original gets its evidence rewritten to
   `kind: trace` by the driver. You do not write the `frida` citation.
   A row the phase reports as different needs its body fixed, or its import
   reverted (say which in your report).
   **Unmeasured rows.** A row the phase reports as "exercised by no
   scenario" cannot close. In your fix round revert its body to the merge
   base (`git diff first...HEAD` shows what changed), delete
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
6. **Commit as you go.** WIP commits are fine.
7. **Asm reds: confirm or waive.** The parity step's asm verify has a known
   false-positive class — the binary inlined a helper that the source calls
   out as a function. Confirm each asm red against the decompile or
   disassembly before keeping the body, or waive it in the evidence file with
   `parity_waiver: "<reason>"`.
8. **Never push, never write through a remote.** From a batch worktree,
   `git push` is wrong, and so is any `git fetch <remote>` with a refspec that
   writes a branch. The coordinator merges and pushes.

## Report back

Write a section headed `## Imported`, with one line per function:

    imported <0xADDR> <evidence kind>

Then list the commits you made, the evidence files you wrote and anything you
could not import. Do not write a `## Verdicts` section. You do not
fast-forward anything yourself; the driver records and closes.
