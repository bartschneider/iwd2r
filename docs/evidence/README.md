# Decompiler evidence

Every engine function a change touches needs an evidence file here: the
original's decompilation next to our port, and either an oracle case that
compares the two at run time or a reason why none is needed.
`tools/check_evidence.py` enforces it in CI.

## Workflow

1. Change a function under `src/` that carries a `// 0xADDR` comment.
2. Draft its evidence from the labelled Ghidra project
   (`tools/ghidra/make_project.py` creates it):

       python3 tools/oracle.py evidence 0x5845D0
       python3 tools/oracle.py evidence IcewindMisc::Roll

   A name shared by several functions (overloads) lists their addresses;
   run the command again with one of them. An existing file is kept unless
   `--force` is given.
3. Fill in `oracle_case` (or replace it with `exemption`) and the Notes.
   Leave the generated decompilation as Ghidra wrote it.
4. Check before pushing:

       python3 -I tools/check_evidence.py --base origin/main

## Format

The file name is the address in upper-case hex without `0x`:
`docs/evidence/5845D0.md`. It starts with a front-matter block of simple
`key: value` lines (quotes around a value are optional):

    ---
    address: 0x5845D0
    name: IcewindMisc::Roll
    oracle_case: rand/roll
    ghidra_version: 12.1.4
    ---

- `address`: as in the `// 0xADDR` comment of the source, upper-case hex.
- `name`: the function's full name in the Ghidra project.
- `oracle_case`: the oracle case that compares this function with the
  original, or instead
- `exemption`: a short reason no case is needed, such as `"pure data"`.
- `ghidra_version`: the Ghidra that produced the decompilation
  (`tools/ghidra/versions.json`).

Then the sections `Decompiled (Ghidra)`, `Callers` (functions referring to
the entry, with the referring address), `Callees` and `Notes`.

## What counts as touched

`tools/check_evidence.py [--base REV]` reads `git diff <base>...HEAD -- src/`.
A labelled function spans from its `// 0xADDR` comment (directly above the
definition, the rule `tools/ghidra/labels.py` uses for the Ghidra labels) to
the first following line that starts with `}`. A committed change to any line
of a span touches the function. Changes between functions, in headers, and
deleted functions are ignored; `src/` files new in the branch count in full.
