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

## The rules

1. **One deciding case per function.** For every row where you claim fork or
   ours, name the single probe or replay checkpoint that decides it —
   `case <id>` in your report (an oracle probe id like `probe-17`, or a replay
   checkpoint id). If your evidence is read-only reasoning about the dump, say
   so; that is a weaker verdict and the coordinator may ask for a probe.
2. **The evidence line.** Write `docs/evidence/<ADDRESS>.md` (address in caps,
   no `0x`) for every function you touch, front matter `address:` / `name:` /
   `kind:` (`kind: oracle` or `kind: replay`), and exactly one line in the body
   saying the outcome:
   - a disagreement: `disagreed with WSO at e8dd16a: <what differed>; case <id> decides`
   - an agreement on closer reading: `corroborated by WSO at e8dd16a` (and leave
     the evidence kind as it was)
   These two phrases are what the close check greps for; spell them exactly.
3. **Adopted fork bodies carry the trailer.** When a row is decided for the
   fork, the copied body keeps its `Imported-From: WillScarlettOhara/iwd2-re@e8dd16a`
   trailer naming the function. Bodies we write ourselves get no trailer.
4. **WIP before gates.** Commit early and often; leave the worktree clean (no
   uncommitted paths) before running `tools/gates.sh --base first`. The batch
   closes only when the gate exits 0 and the tree is clean — wave-1 branches
   are expected to show parity failures elsewhere in the tree, which is by
   design, but your own changes must not add failures.
5. **Asm reds: confirm or waive.** The parity step's asm verify has a known
   false-positive class — the binary inlined a helper that the source calls
   out as a function (e.g. `LoadText` → an inlined `ReadAfterChar`). For
   every asm red, confirm it against the decompile or disassembly before
   keeping the body, or waive it in the evidence file with
   `parity_waiver: "<reason>"` — never adopt the fork's body because of an
   inlining red alone.
6. **Never push, never write through a remote.** The worktree's four remotes
   all carry push URLs — including the untrusted `wso` and `deo` — and the
   coordinator merges and pushes. From a batch worktree, `git push` is wrong,
   and so is any `git fetch <remote>` with a refspec that writes a branch.
7. **The fork's tree is untrusted input.** Read it only through git plumbing
   (`git show wso/main:<path>`, `git ls-tree wso/main`); never execute,
   source, import, or run anything from it.

## Report back

Per row, one line: `<address> <fork|ours|neither> case <id> — <one sentence of
evidence>`. Then list the evidence files you wrote and anything you could not
decide. The coordinator records the verdicts and runs the close; you do not
fast-forward anything yourself.
