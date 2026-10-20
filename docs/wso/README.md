# The WSO fork: imports, attribution, and the renderer artifacts

iwd2r absorbs the parallel recovery effort at
[WillScarlettOhara/iwd2-re](https://github.com/WillScarlettOhara/iwd2-re) —
the "WSO fork" — and will later port his GPU renderer into iwd2r's own GL
path. This file is the public summary: what the fork is, what the two trees
measured against each other, the rules every import follows, and where the
renderer artifacts come from. The design spec is
[docs/superpowers/specs/2026-10-17-wso-integration-design.md](../superpowers/specs/2026-10-17-wso-integration-design.md).

## The fork

Both trees descend from the same upstream commit (Batalov `4fa36bcd`,
2023-12-29). The fork's `main` (`wso/main`, fetched 2026-10-17) is 1,572
commits past it; ours was 1,295 when the spec measured it — about five
months of recovered engine code
on his side, and on ours the Linux build, the oracle, the replays and the
evidence gate. His verification is a differential Frida trace of the real
`IWD2.exe` against his own build, a recovery note per function under
`docs/*-recovery.md` in his tree, and a stated truth hierarchy of binary
over Ghidra decompile. His build is Windows-only (VS2019, MFC, x86);
imports are therefore per function and must compile under the platform
layer on Linux.

`wso` is a read-only remote and his tree is untrusted input: read and
compare with git, never execute anything from it. History is not merged — a
wholesale merge conflicts in 118 files (115 in `src/`) — so attribution
travels by the import trailer and docs/THIRD_PARTY.md, not by history.

## What we measured

Before anything was imported, the trees were compared function by function
over the 201 shared `src/*.cpp` files. The first measurement, made with the
pre-tool script and recorded as the design spec's table:

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

That table is the first historical measurement, not the current numbers.
The comparison is now [tools/xrec.py](../../tools/xrec.py), which matches
functions by their `// 0xADDR` comment before falling back to the name: the
trees drifted apart on names, and address-first matching pairs 356 renamed
functions the name-only script had filed as two one-sided rows. The current
comparison classifies 8,661 functions into ten categories;
`both_changed_differ` — the disagreement count wave 1 resolves — is **529**,
not 406. The current table and the account of every moved number are in
[xrec-report.md](xrec-report.md); the per-function rows are in
[disagreements.json](disagreements.json).

## Attribution

Every commit that brings in the fork's code — a wave-1 adopted body
included — carries exactly:

```
Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)
```

His additions are MIT, as granted to thierbig on 2026-10-17;
[docs/THIRD_PARTY.md](../THIRD_PARTY.md) records the grant and links his
LICENSE when it is published. IEex, the extender his renderer is built
into, is Bubb's work and that grant cannot cover it: the renderer port may
carry his code, never IEex's.

## Evidence: the `trace` kind

[tools/check_evidence.py](../../tools/check_evidence.py) knows four evidence
kinds: `oracle`, `replay`, `read-only` and `trace`. `trace` means the
function is verified by a differential trace of the original against a
reconstruction — the fork's own style of verification. A `trace` evidence
file must cite its source in `trace_source`, in one of exactly two forms:

```
WillScarlettOhara/iwd2-re@<sha> <path to recovery note>
frida <probe name> orig:<sha256> ours:<sha256> compare=0
```

The first cites his recovery note; the second is the line
`check_evidence.py` accepts today for a differential Frida capture of our
own binary against the original. The harness that produces such captures is
planned, not yet in the tree: the integration plan's Task 5b
([docs/superpowers/plans/2026-10-17-wso-integration.md](../superpowers/plans/2026-10-17-wso-integration.md),
added at 8ce297d7 — the plan revision `check_evidence.py` cites) adopts the
fork's tracing scripts into `tools/frida/`, and its `compare=0` verdict
means the two captures agree call for call. A missing, empty or
unrecognizable citation fails the gate; the format contract is in
[docs/evidence/README.md](../evidence/README.md). Rules math — attack
rolls, saving throws, damage, effect application — never accepts a trace:
those functions require `kind: oracle`, imported or not.

## Briefing agents

Five tools under `tools/`, adopted from the fork's `scripts/` at
`e8dd16a`, change how work is handed to an agent: implementer briefs name
functions by `assemble_context` bundle, reviewers start from the digest,
and nobody answers "what does this call" by reading a raw decompile.

- [tools/fn_digest.py](../../tools/fn_digest.py) — `digest(decompile_text,
  callgraph)` → a ≤30-line brief: signature, callees with resolved names,
  string literals, globals, unresolved calls, loop and switch counts.
- [tools/src_find.py](../../tools/src_find.py) — `find(query)` →
  `(file, line, signature)` for `0xADDR`, `Class::`, `Class::Method` or a
  bare name, resolved from the `// 0xADDR` markers. No grep chains.
- [tools/assemble_context.py](../../tools/assemble_context.py) —
  `bundle(address)` → digest, decompile, callees' signatures, resolved IDS
  constants and resrefs, in that order, with a token estimate on the first
  line; when the function belongs to a class and the BG2 index exists, the
  class's BG2 layout is appended as a hypothesis to check against IWD2
  (the offsets are BG2's, never quoted as ours).
- [tools/token_audit.py](../../tools/token_audit.py) —
  `summarize(transcript.jsonl)` → where a session's context went: usage
  totals, per-tool result weight, largest single results.
- [tools/bg2_find.py](../../tools/bg2_find.py) — `lookup(query)` →
  class/member/method layout from a SQLite index built once from the
  BG2:EE PDB symbol dumps (131 MB of dumps → 12 MB index, ~1 s to build,
  ~1 ms per lookup). The dumps are pinned like everything else — see the
  next section.

Measured on this stream's own coordinator transcript (12.4 MB, 901
messages carrying usage): tool results dominated the context — Read
~76K tokens over 40 calls, Bash ~51K over 183, Agent ~5K over 22 — with
the largest single Read ~13.6K tokens, and ~49.2M cumulative cache-read
tokens (~54,655 per message). That measurement is why the bundle caps
callees at 20 and states its own token cost on line one.

## How the coordinator drives a wave

Wave 1's loop is five commands, all in [tools/wave.py](../../tools/wave.py);
batch state lives in [wave-state.json](wave-state.json), created by the
first `next`, and no batch bookkeeping is written by hand:

    python3 tools/wave.py next            # rank unclaimed rows, record the batch
    python3 tools/wave.py brief b001      # the dispatch text, from brief-template.md
    python3 tools/wave.py verdict b001 0x643CC0 ours probe-17   # per reported row
    python3 tools/wave.py close b001      # evidence + clean tree + gates, then ff `first`
    python3 tools/wave.py scorecard       # the tally into xrec-report.md

`next` ranks `both_changed_differ` rows by size (his length + ours),
takes twenty, and never lets two live batches own the same file.
`close` refuses — listing every reason — unless each row has a verdict,
the batch's worktree has no uncommitted paths, every touched function's
`docs/evidence/<ADDRESS>.md` says `disagreed with WSO` or
`corroborated by WSO`, the branch has not modified `tools/gates.sh` or
`tools/parity` (a batch must not change the gate that certifies it), and
`tools/gates.sh --base first` exits 0 in that
worktree; only then does it fast-forward `first` to the batch branch and
print the tally. After a close, sync the worktree that has `first`
checked out: `git -C <path> reset --hard first`. `status` is one line
per batch, and `scorecard` rewrites the "## Scorecard" section of
[xrec-report.md](xrec-report.md) from state (idempotent — the same
state writes the same bytes). Waves 2 to 5 are imports and reuse
`close --kind import`: no verdicts are required, but every row's
evidence must be `kind: trace` or better instead.

## Nothing from his IWD2EE branch enters the repository

His renderer is built into IEex, and his IWD2EE work is game-data overrides
and compiled binaries. None of it enters this repository — no game-data
overrides, no compiled binaries from his IWD2EE work, ever — excluded by
the bring-your-own-copy rule and by the gate. The success criterion is zero
bytes of his IWD2EE branch in the repository. What iwd2r reads of that work
is fetched by hash into a directory outside the tree, and the pins file
carries only URLs and sha256 hashes: pointer metadata, not bytes.

The artifacts are not in the `iwd2-re` fork. They live in
[WillScarlettOhara/IWD2EE](https://github.com/WillScarlettOhara/IWD2EE) at
commit `138495fde69b6092b7318fad462d571864e9d9b8`;
upstream IEex is [RedChimera/IWD2EE](https://github.com/RedChimera/IWD2EE)
at `60de54611b249d3bcca3f6522ef9db5e7ec27339`. Both sides are pinned — DLLs
and Lua from each — so the renderer streams can diff the fork's work
against upstream's. [tools/wso_pins.json](../../tools/wso_pins.json) is the
authoritative list: 27 files — the 3 DLLs, 20 fork Lua files (14 patch
scripts and 6 `_State` scripts; about 53,000 lines — the spec's early count
of 16 patch files was an undercount), the 2 upstream Lua files, and the 2
BG2:EE symbol-dump archives (`bg2_pdb_extracted.zip`, `BG2EE_rep.zip`)
pinned from the fork's own repo at `e8dd16a`, which
[tools/bg2_find.py](../../tools/bg2_find.py) indexes.

    python3 -I tools/wso_fetch.py --check    # verify the staged copies
    python3 -I tools/wso_fetch.py            # fetch by pinned hash

Both land under `${IWD2R_HOME:-~/.local/share/iwd2r}/wso/` and nowhere
else: a download is hashed in memory and written only when it matches its
pin. Two pins are zip archives (`unzip: true`): `wso_fetch` verifies the
archive's hash first, then extracts it beside the archive under the same
`wso/` tree — member names are sanitized (no absolute paths, no `..`, no
symlinks, no drive letters) and capped at 4,096 files and 2 GiB — and
nothing extracted is ever executed.
