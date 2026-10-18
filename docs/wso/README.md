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
commits past it; ours is 1,295 — about five months of recovered engine code
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

From wave 2 on, every import commit carries exactly:

```
Imported-From: WillScarlettOhara/iwd2-re@<40-char sha> (MIT per author, 2026-10-17)
```

His additions are MIT, as granted to Thierry on 2026-10-17;
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

The first cites his recovery note; the second cites a capture made with the
fork's tracing scripts adopted into `tools/frida/`, where `compare=0` means
the original and our build produced identical traces. A missing, empty or
unrecognizable citation fails the gate; the format contract is in
[docs/evidence/README.md](../evidence/README.md). Rules math — attack
rolls, saving throws, damage, effect application — never accepts a trace:
those functions require `kind: oracle`, imported or not.

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
branch `opengl-hd-beta7` (commit `138495fde69b6092b7318fad462d571864e9d9b8`);
upstream IEex is [RedChimera/IWD2EE](https://github.com/RedChimera/IWD2EE)
at `60de54611b249d3bcca3f6522ef9db5e7ec27339`. Both sides are pinned — DLLs
and Lua from each — so the renderer streams can diff the fork's work
against upstream's. [tools/wso_pins.json](../../tools/wso_pins.json) is the
authoritative list: 25 files — the 3 DLLs, 20 fork Lua patch files (about
53,000 lines; the spec's early count of 16 patch files was an undercount),
and the 2 upstream Lua files.

    python3 -I tools/wso_fetch.py --check    # verify the staged copies
    python3 -I tools/wso_fetch.py            # fetch by pinned hash

Both land under `${IWD2R_HOME:-~/.local/share/iwd2r}/wso/` and nowhere
else: a download is hashed in memory and written only when it matches its
pin.
