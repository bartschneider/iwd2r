"""Flag ``// 0xADDR`` markers that are not function starts in iwd2.exe.

Adapted from WillScarlettOhara/iwd2-re@e8dd16a scripts/lint_address_markers.py:
the marker collection (regexes, rejects) and the audit split are kept
byte-for-byte; the independent answer is ours. The fork read Ghidra's
``_index.json`` (start addresses only) and asked a ghidra-bridge whether the
binary reaches the address; here functions.tsv carries start *and size* and
callgraph.tsv carries every direct call edge, so:

``unreferenced``
    The marker sits inside a function the binary defines, and nothing calls
    it. Nothing reaches that address, so it is not the start of anything.
    This is what fails the lint. (The fork also accepted vtable pointers
    here; our export has no vtable slots, so a merged-in vtable-only entry
    point reads as ``unreferenced`` -- same as the fork without its bridge.)

``merged``
    The marker sits inside a function but a call does target it: a real
    entry point the export swallowed into its neighbour. Reported for eyes,
    not failed.

``undefined``
    No defined function contains the address. Reported, not failed unless
    --strict.

One deliberate tightening: the fork took a function's extent as running to
the next start because its index carried no size, so padding between
functions read as "inside". functions.tsv has the size, so a marker lands
"inside" only within the owner's own bytes; past them it is ``undefined``
(a real gap or a stale marker -- both worth seeing).

A second deliberate narrowing, in what --strict fails on: the fork's exit
was ``if args.strict and (merged or unchecked or undefined): return 1`` --
strict also failed on merged hits and on unchecked markers (no callgraph to
check against). Ours fails only on ``undefined``, so --strict stays a
statement about markers that name no function start, not about the state
of the export; merged and unchecked are always eyes-only here.

Usage::

    python3 tools/parity/address_markers.py [--source-root src] [--strict]
"""
from __future__ import annotations

import argparse
import bisect
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

FILE_EXTENSIONS = (".cpp", ".h", ".hpp")
ADDR_RE = re.compile(r"^[ \t]*//[ \t]*0x([0-9A-Fa-f]+)\b")
QUAL_RE = re.compile(r"(~?\w+)\s*::\s*(~?\w+|operator\s*(?:\[\]|\(\)|[^\s(]+))\s*\(")
INLINE_RE = re.compile(r"(~?\w+)\s*\(")
CODE_MIN = 0x401000
CODE_MAX = 0x800000
CPP_KEYWORDS = frozenset({
    "if", "while", "for", "switch", "else", "return", "do", "case", "default",
    "goto", "break", "continue", "sizeof", "new", "delete", "catch", "throw",
})


def _signature_line(lines, start):
    """The first code line after the marker, or None if another marker comes first."""
    for j in range(start + 1, len(lines)):
        stripped = lines[j].strip()
        if not stripped or stripped.startswith("//"):
            if ADDR_RE.match(lines[j]):
                return None
            continue
        return lines[j]
    return None


def read_functions(path):
    """functions.tsv -> (names, sizes), keyed by start address."""
    names, sizes = {}, {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 2:
                continue
            addr = int(parts[0], 16)
            names[addr] = parts[1]
            sizes[addr] = int(parts[2]) if len(parts) > 2 and parts[2] else 0
    return names, sizes


def read_callgraph(path):
    """callgraph.tsv -> {address: set(callee addresses)}."""
    edges = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            parts = line.split()
            if parts:
                edges[int(parts[0], 16)] = {int(p, 16) for p in parts[1:]}
    return edges


def collect_markers(source_root, repo=None):
    """Every ``// 0xADDR`` marker that names a function, with its position.

    Mirrors the fork's reagent_address_map.build_map -- same regexes, same
    rejects -- but keeps the line number, which our labels.tsv has no room
    for.
    """
    markers = []

    for root, dirs, files in os.walk(source_root):
        dirs[:] = sorted(d for d in dirs if d != '.git')
        for name in sorted(files):
            if not name.lower().endswith(FILE_EXTENSIONS):
                continue
            path = os.path.join(root, name)
            rel = os.path.relpath(path, repo or source_root).replace(os.sep, '/')
            file_class = os.path.splitext(name)[0]
            with open(path, encoding='utf-8', errors='ignore') as f:
                lines = f.read().splitlines()

            for i, line in enumerate(lines):
                m = ADDR_RE.match(line)
                if not m:
                    continue

                addr = int(m.group(1), 16)
                if not (CODE_MIN <= addr <= CODE_MAX):
                    continue

                sig = _signature_line(lines, i)
                if sig is None:
                    continue

                qm = QUAL_RE.search(sig)
                if qm:
                    cls, func = qm.group(1), qm.group(2)
                else:
                    im = INLINE_RE.search(sig)
                    if not im:
                        continue
                    cls, func = file_class, im.group(1)

                if func in CPP_KEYWORDS:
                    continue

                markers.append({
                    "file": rel,
                    "line": i + 1,
                    "address": addr,
                    "symbol": f"{cls}::{func}",
                })

    return markers


def audit(markers, starts, names, sizes, referenced):
    """Split markers into (ok, inside, undefined) against the function starts.

    ``referenced`` maps an address to True/False (a call targets it or not)
    or None when no call graph is available.
    """
    ok, inside, undefined = [], [], []

    for marker in markers:
        addr = marker["address"]
        if addr in names:
            ok.append(marker)
            continue

        # Nearest function start at or below the marker. Its extent is its
        # own byte size (functions.tsv); without a size we fall back to the
        # fork's next-start rule.
        pos = bisect.bisect_right(starts, addr) - 1
        if pos < 0:
            undefined.append(dict(marker, detail="below the first function"))
            continue

        owner = starts[pos]
        size = sizes.get(owner) or 0
        end = owner + size if size else (
            starts[pos + 1] if pos + 1 < len(starts) else CODE_MAX)
        if addr < end:
            inside.append(dict(marker,
                               owner=owner,
                               owner_name=names[owner],
                               delta=addr - owner,
                               referenced=referenced(addr)))
        else:
            undefined.append(dict(marker, detail="no function defined here"))

    return ok, inside, undefined


def check(source_root, functions_path, callgraph_path, repo=None, strict=False):
    """Run the audit; returns (exit code, report lines).

    --strict (the fork's flag) also fails on markers with no function
    defined at them.
    """
    if not os.path.isdir(source_root):
        return 2, [f"ERROR: source root not found: {source_root}"]
    names, sizes = read_functions(functions_path)
    starts = sorted(names)
    if callgraph_path and os.path.isfile(callgraph_path):
        edges = read_callgraph(callgraph_path)
        targets = set()
        for callees in edges.values():
            targets |= callees
        referenced = lambda addr: addr in targets  # noqa: E731
    else:
        referenced = lambda addr: None  # noqa: E731

    markers = collect_markers(source_root, repo)
    ok, inside, undefined = audit(markers, starts, names, sizes, referenced)

    # referenced is False only when a call graph exists and nothing targets
    # the address; None means no graph, and those stay reported rather than
    # being promoted to a failure on no evidence.
    unreferenced = [f for f in inside if f["referenced"] is False]
    merged = [f for f in inside if f["referenced"] is True]
    unchecked = [f for f in inside if f["referenced"] is None]

    lines = [f"address markers: {len(markers)}  "
             f"function starts: {len(ok)}  "
             f"unreferenced: {len(unreferenced)}  "
             f"merged into a neighbour: {len(merged)}  "
             f"unchecked: {len(unchecked)}  "
             f"no function defined: {len(undefined)}"]

    for f in unreferenced:
        lines.append(
            f"  FAIL {f['file']}:{f['line']}  0x{f['address']:08X} {f['symbol']}"
            f"  ->  nothing calls it; inside 0x{f['owner']:08X} "
            f"{f['owner_name']}+0x{f['delta']:X}")

    # Silence here would be the worst outcome: every finding downgrades to a
    # warning and the run still exits 0. Say it instead.
    if unchecked:
        lines.append(f"  NOTE no callgraph.tsv, so {len(unchecked)} finding(s)"
                     " are unjudged -- run tools/ghidra/ExportCallGraph.java")
        for f in unchecked:
            lines.append(
                f"    ?    {f['file']}:{f['line']}  0x{f['address']:08X} "
                f"{f['symbol']}  ->  inside 0x{f['owner']:08X} "
                f"{f['owner_name']}+0x{f['delta']:X}")

    for f in merged:
        lines.append(
            f"  WARN {f['file']}:{f['line']}  0x{f['address']:08X} {f['symbol']}"
            f"  ->  inside 0x{f['owner']:08X} {f['owner_name']}+0x{f['delta']:X}"
            " (referenced -- the export merged it into its neighbour)")

    if undefined:
        lines.append(f"  no function defined at these "
                     f"(re-export, or a real gap): {len(undefined)}")
        for f in undefined[:10]:
            lines.append(
                f"    {f['file']}:{f['line']}  0x{f['address']:08X} "
                f"{f['symbol']}  ({f['detail']})")

    return (1 if unreferenced or (strict and undefined) else 0), lines


def main(argv=None) -> int:
    import run as parity_run
    ap = argparse.ArgumentParser(
        description=__doc__.splitlines()[0])
    ap.add_argument("--source-root", default=os.path.join(ROOT, "src"))
    ap.add_argument("--exports", help="ghidra-exports dir (default $IWD2R_HOME)")
    ap.add_argument("--repo", default=ROOT)
    ap.add_argument("--strict", action="store_true",
                    help="also fail on markers with no function defined there")
    args = ap.parse_args(argv)

    exports = parity_run.exports_dir(args.exports)
    functions = os.path.join(exports, 'functions.tsv')
    callgraph = os.path.join(exports, 'callgraph.tsv')
    if not os.path.isfile(functions):
        print(f"skip: no function index at {functions}", file=sys.stderr)
        return 0

    code, lines = check(args.source_root, functions, callgraph, repo=args.repo,
                        strict=args.strict)
    for line in lines:
        print(line)
    return code


if __name__ == "__main__":
    raise SystemExit(main())
