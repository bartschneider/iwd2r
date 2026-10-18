#!/usr/bin/env python3
"""fn_digest.digest -- a <=30-line brief of a decompiled function.

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/fn_digest.py: the
output shapes are his (the string/globals regexes, the dedupe-keep-first
callees in first-seen order, the caps, the exact line spellings), so a
digest reads the same whether it came from his tree or ours.  The
interface is rehosted: his script walked his ``_index.json`` +
per-function JSON exports, ours takes the decompile text and a callgraph
dict directly (from ghidra-exports' ``functions.tsv``/``callgraph.tsv``
and ``<name>.c``, as assembled by tools/assemble_context.py).

The digest carries the signature, callees and callers by name, the
strings referenced, the globals touched, a loop/switch count (ours; his
did not count control flow), and the count of still-unresolved
FUN_/SUB_/LAB_ tokens.  Hard cap: 30 lines.

Usage::

    python3 tools/fn_digest.py DECOMPILE.c [--exports DIR] [--address 0x...]
"""
from __future__ import annotations

import argparse
import os
import re
import sys

# His regexes and caps, byte-for-byte.
STR_RE = re.compile(r'"((?:[^"\\]|\\.){2,60})"')
GLOBALS_RE = re.compile(r"\b(?:DAT_|g_)\w+")
UNRESOLVED_RE = re.compile(r"\b(?:FUN|SUB|LAB)_[0-9A-Fa-f]+\b")
LOOPS_RE = re.compile(r"\b(?:for|while)\s*\(")
SWITCH_RE = re.compile(r"\bswitch\s*\(")

CALLEE_CAP = 20
CALLER_CAP = 12
STRING_CAP = 10
GLOBAL_CAP = 12
LINE_CAP = 30


def _dedup(items):
    """Keep first-seen order, drop repeats."""
    out, seen = [], set()
    for it in items:
        if it not in seen:
            seen.add(it)
            out.append(it)
    return out


def _names(items):
    """Callee/caller entries may be plain names or {name, addr} dicts."""
    out = []
    for it in items or ():
        if isinstance(it, dict):
            it = it.get("name") or f'0x{it.get("addr", 0):x}'
        out.append(str(it))
    return out


def _list_line(label, items, cap):
    line = f"{label} ({len(items)}): " + ", ".join(items[:cap])
    if len(items) > cap:
        line += " ..."
    return line


def digest(decompile_text: str, callgraph: dict) -> str:
    """At most 30 lines: signature, callees, callers, strings, globals,
    loop/switch counts, unresolved-token count."""
    cg = callgraph or {}
    text = decompile_text or ""
    name = cg.get("name") or "(function)"
    addr = cg.get("address")
    nlines = text.count("\n") + 1 if text else 0
    if addr is not None:
        lines = [f"{name} @ {addr:#x}  ({nlines} decompiled lines)"]
    else:
        lines = [f"{name}  ({nlines} decompiled lines)"]

    signature = cg.get("signature")
    if signature:
        lines.append(f"signature: {signature}")

    callees = _dedup(_names(cg.get("callees")))
    if callees:
        lines.append(_list_line("callees", callees, CALLEE_CAP))
    callers = _dedup(_names(cg.get("callers")))
    if callers:
        lines.append(_list_line("callers", callers, CALLER_CAP))

    strings = _dedup(m.group(1) for m in STR_RE.finditer(text)
                     if not m.group(1).startswith("\\"))
    if strings:
        line = "strings: " + " | ".join(f'"{s}"' for s in strings[:STRING_CAP])
        if len(strings) > STRING_CAP:
            line += " ..."
        lines.append(line)

    globals_ = sorted(set(GLOBALS_RE.findall(text)))
    if globals_:
        line = "globals: " + ", ".join(globals_[:GLOBAL_CAP])
        if len(globals_) > GLOBAL_CAP:
            line += " ..."
        lines.append(line)

    # A do-while's `while (` counts as a loop; that is the honest count of
    # back-edges a reader will walk past.
    loops = len(LOOPS_RE.findall(text))
    switches = len(SWITCH_RE.findall(text))
    lines.append(f"loops: {loops}  switches: {switches}")

    unresolved = set(UNRESOLVED_RE.findall(text))
    if unresolved:
        lines.append(f"unresolved tokens: {len(unresolved)}")

    if len(lines) > LINE_CAP:
        extra = len(lines) - LINE_CAP + 1
        lines = lines[:LINE_CAP]
        lines[-1] = f"... (+{extra} more)"
    return "\n".join(lines)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("decompile", help="a decompile .c file")
    ap.add_argument("--exports",
                    help="ghidra-exports dir for functions.tsv/callgraph.tsv "
                         "(default $IWD2R_HOME)")
    ap.add_argument("--address", help="function address for the callgraph "
                                      "lookup (default: from the file's marker)")
    args = ap.parse_args(argv)
    with open(args.decompile, encoding="utf-8", errors="replace") as f:
        text = f.read()
    address = None
    m = re.search(r"//\s*(0x[0-9A-Fa-f]+)\b", text)
    if args.address:
        address = int(args.address, 16)
    elif m:
        address = int(m.group(1), 16)
    callgraph = {"address": address,
                 "name": os.path.splitext(os.path.basename(args.decompile))[0]}
    exports = (args.exports or os.environ.get("IWD2R_HOME")
               or os.path.join(os.path.expanduser("~"), ".local", "share",
                               "iwd2r")) + "/ghidra-exports"
    try:
        with open(os.path.join(exports, "callgraph.tsv")) as f:
            for line in f:
                parts = line.split()
                if parts and int(parts[0], 16) == address:
                    callgraph["callees"] = [
                        f"0x{int(p, 16):x}" for p in parts[1:]]
    except (OSError, ValueError):
        pass
    print(digest(text, callgraph))
    return 0


if __name__ == "__main__":
    sys.exit(main())
