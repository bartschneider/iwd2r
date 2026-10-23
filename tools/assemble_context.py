#!/usr/bin/env python3
"""assemble_context.bundle -- one brief per function, not a raw decompile.

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/reagent_assemble_context.py:
the shape is his -- digest, decompile, callees, resolved IDS constants and
resrefs, in that order, with a token estimate on the first line -- and the
pieces come from the sibling tools: fn_digest.digest for the brief,
src_find.find for signature/file/line resolution, bg2_find.lookup for an
optional BG2 layout section, and a copy of his string regex.  His script
read his own _index.json exports and his BG2 index; ours reads the
ghidra-exports TSVs (as written by tools/parity/address_markers.py) and
an ids/ directory of .IDS files.

The first line carries the token estimate (~chars/4) so an agent can
budget before reading.  Every section degrades honestly: a missing
exports tree says so instead of raising.

Process change from the task that introduced this tool: implementer
briefs name functions by bundle, reviewers start from the digest.

Usage::

    python3 tools/assemble_context.py 0x5845D0
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

STR_RE = re.compile(r'"((?:[^"\\]|\\.){2,60})"')  # fn_digest's (his) regex
CONST_RE = re.compile(r"\b[A-Z][A-Z0-9_]{2,}\b")
RESREF_RE = re.compile(r"[A-Z][A-Z0-9]{1,7}$")

CALLEE_CAP = 20
CONST_CAP = 40
RESREF_CAP = 10
BG2_ROW_CAP = 60

BG2_HEADER = "## BG2 layout (BG2EE PDB -- BG2 offsets, NOT IWD2's)"

_tools = {}


def _tool(name):
    """Load a sibling tool by path: tools stay importable standalone
    (python3 -I omits the repo from sys.path, so plain imports of
    siblings would not resolve here)."""
    if name not in _tools:
        path = os.path.join(HERE, name + ".py")
        spec = importlib.util.spec_from_file_location(
            "iwd2r_tool_" + name.replace("-", "_"), path)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        _tools[name] = mod
    return _tools[name]


def _home():
    return (os.environ.get("IWD2R_HOME")
            or os.path.join(os.path.expanduser("~"), ".local", "share",
                            "iwd2r"))


def _default_exports():
    return os.path.join(_home(), "ghidra-exports")


def _read_functions(exports):
    """functions.tsv -> {addr: (name, size)}; address_markers' reader,
    local copy (no sibling import)."""
    functions = {}
    path = os.path.join(exports, "functions.tsv")
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) < 2:
                    continue
                try:
                    addr = int(parts[0], 16)
                except ValueError:
                    continue
                size = int(parts[2]) if len(parts) > 2 and parts[2].isdecimal() else 0
                functions[addr] = (parts[1], size)
    except OSError:
        pass
    return functions


def _read_callgraph(exports):
    """callgraph.tsv -> {from_addr: [to_addr, ...]}; address_markers'
    reader, local copy."""
    callgraph = {}
    path = os.path.join(exports, "callgraph.tsv")
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                parts = line.split()
                if len(parts) < 2:
                    continue
                try:
                    src = int(parts[0], 16)
                    dsts = [int(p, 16) for p in parts[1:]]
                except ValueError:
                    continue
                callgraph.setdefault(src, [])
                for dst in dsts:
                    if dst not in callgraph[src]:
                        callgraph[src].append(dst)
    except OSError:
        pass
    return callgraph


def _ids_map(ids_dir):
    """*.IDS under ids_dir -> {SYMBOL: value}.  Both spellings occur:
    'FOO_BAR 0' and '8 ODD_CASE'.  ';' comment lines are skipped."""
    mapping = {}
    if not ids_dir or not os.path.isdir(ids_dir):
        return mapping
    for name in sorted(os.listdir(ids_dir)):
        if not name.upper().endswith(".IDS"):
            continue
        path = os.path.join(ids_dir, name)
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                for line in f:
                    s = line.strip()
                    if not s or s.startswith(";") or s.startswith("'"):
                        continue
                    parts = s.split()
                    if len(parts) != 2:
                        continue
                    a, b = parts
                    if re.fullmatch(r"-?\d+|0[xX][0-9A-Fa-f]+", a):
                        try:
                            mapping[b.upper()] = int(a, 0)
                        except ValueError:
                            pass
                    else:
                        try:
                            mapping[a.upper()] = int(b, 0)
                        except ValueError:
                            pass
        except OSError:
            continue
    return mapping


def _bg2_lines(fname, bg2_db):
    """The BG2 layout of the function's class, if the index knows it."""
    if bg2_db is False or not fname or "::" not in fname:
        return []
    if not bg2_db or not os.path.isfile(bg2_db):
        return []
    cls = fname.split("::", 1)[0]
    try:
        rows = _tool("bg2_find").lookup(cls + "::", db=bg2_db)
    except Exception:
        return []
    if not rows:
        return [f"(no BG2 layout for {cls})"]
    out = [BG2_HEADER]
    for r in rows[:BG2_ROW_CAP]:
        kind = r.get("kind")
        if kind == "class":
            out.append(f"class {r['name']}  ({r['type']}, sizeof {r['size']})")
        elif kind == "base":
            out.append(f"  : {r['name']}  @ {r['offset']}")
        elif kind == "member":
            static = "static " if r.get("static") else ""
            out.append(f"  @ {r['offset']:<5} {static}{r['type']} {r['name']}")
        elif kind == "method":
            slot = (f"vft+{r['vtable']}" if r.get("vtable", -1) >= 0
                    else "nonvirtual")
            out.append(f"  {r['name']}{r['type']}  [{slot}]")
        elif kind == "enumval":
            out.append(f"  {r['name']} = {r['value']}  (enum {r['enum']})")
        elif kind == "hint":
            out.append(f"? {r['note']}")
    if len(rows) > BG2_ROW_CAP:
        out.append(f"... (+{len(rows) - BG2_ROW_CAP} more rows)")
    return out


CALLER_CAP = 12
CALLER_LINES = 40


def _decompile_path(exports, address, name=None):
    """<exports>/decompile/ADDR.c (tools/ghidra/export_decompiles.py), else
    the older <exports>/<name>.c; None when neither exists."""
    cands = [os.path.join(exports, "decompile", f"{address:X}.c")]
    if name:
        cands.append(os.path.join(exports, name + ".c"))
    for path in cands:
        if os.path.isfile(path):
            return path
    return None


def _read_text(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def bundle(address: int, *, exports=None, src_root=None, ids_dir=None,
           bg2_db=None, decompile_text=None) -> str:
    """The brief for one function: digest, decompile, callees with
    signatures, resolved IDS constants and resrefs -- and, when a BG2
    index exists and the function belongs to a class, that class's BG2
    layout as a hypothesis to check against IWD2."""
    if exports is None:
        exports = _default_exports()
    if src_root is None:
        src_root = os.path.join(os.path.dirname(HERE), "src")

    hexaddr = f"0x{address:x}"
    functions = _read_functions(exports)
    callgraph = _read_callgraph(exports)
    row = functions.get(address)
    fname = row[0] if row else None

    cache = {}
    src_ok = bool(src_root) and os.path.isdir(src_root)

    text = decompile_text
    if text is None:
        path = _decompile_path(exports, address, fname)
        if path:
            text = _read_text(path)

    # ## digest
    if text:
        cg = {"name": fname or hexaddr, "address": address}
        if fname and src_ok:
            hits = _tool("src_find").find(fname, root=src_root, cache=cache)
            if hits:
                cg["signature"] = hits[0][2]
        cg["callees"] = [functions.get(c, (f"0x{c:x}", 0))[0]
                         for c in callgraph.get(address, [])]
        digest_text = _tool("fn_digest").digest(text, cg)
    else:
        digest_text = f"(no digest: no decompile for {hexaddr})"

    # ## decompile
    if text is not None:
        decomp_text = text.rstrip("\n")
    elif fname:
        decomp_text = "decompile: not exported"
    else:
        decomp_text = (f"(no decompile: no functions.tsv under {exports})")

    # ## callers (decompile)
    caller_addrs = sorted(a for a, dsts in callgraph.items()
                          if address in dsts and a != address)
    caller_lines = []
    shown = 0
    for caddr in caller_addrs:
        cname = functions.get(caddr, (f"0x{caddr:x}", 0))[0]
        path = _decompile_path(exports, caddr, functions.get(caddr, ("",))[0])
        if path is None:
            continue
        if shown >= CALLER_CAP:
            caller_lines.append(f"... (+{len(caller_addrs)} callers in all; "
                                f"first {CALLER_CAP} with a decompile shown)")
            break
        shown += 1
        head = _read_text(path).splitlines()[:CALLER_LINES]
        caller_lines.append(f"### {cname} @ 0x{caddr:x}")
        caller_lines.extend(head)
    if not caller_addrs:
        caller_lines = [f"(no callers in the callgraph for {hexaddr})"]
    elif not caller_lines:
        caller_lines = ["decompile: not exported"]

    # ## callees
    callee_addrs = callgraph.get(address, [])
    if not callee_addrs:
        callee_lines = [f"(no callgraph entry for {hexaddr})"]
    else:
        callee_lines = []
        for caddr in callee_addrs[:CALLEE_CAP]:
            cname = functions.get(caddr, (f"0x{caddr:x}", 0))[0]
            hits = []
            if src_ok:
                query = cname if not cname.startswith("0x") else f"0x{caddr:x}"
                hits = _tool("src_find").find(query, root=src_root,
                                              cache=cache)
            if hits:
                rel, line, sig = hits[0]
                callee_lines.append(f"- {cname} @ 0x{caddr:x}  "
                                    f"{rel}:{line} {sig}")
            else:
                callee_lines.append(f"- {cname} @ 0x{caddr:x}  (not in src)")
        if len(callee_addrs) > CALLEE_CAP:
            callee_lines.append(f"... (+{len(callee_addrs) - CALLEE_CAP} more)")

    # ## ids
    ids_lines = []
    if ids_dir and os.path.isdir(ids_dir):
        mapping = _ids_map(ids_dir)
        if text:
            bare = STR_RE.sub('""', text)
            for const in sorted(set(CONST_RE.findall(bare)))[:CONST_CAP]:
                if const in mapping:
                    ids_lines.append(f"{const} = {mapping[const]}")
            seen = set()
            resrefs = []
            for m in STR_RE.finditer(text):
                s = m.group(1)
                if s in seen or not RESREF_RE.fullmatch(s):
                    continue
                seen.add(s)
                resrefs.append(s)
            if resrefs:
                ids_lines.append("resrefs: "
                                 + " | ".join(f'"{r}"' for r in
                                              resrefs[:RESREF_CAP])
                                 + (" ..." if len(resrefs) > RESREF_CAP else ""))
        if not ids_lines:
            ids_lines.append("(no IDS constants resolved)")
    else:
        ids_lines.append("(no ids dir)")

    body = "\n".join(["## digest", digest_text, "",
                      "## Decompile", decomp_text, "",
                      "## Callers (decompile, first 40 lines each)"]
                     + caller_lines + ["",
                      "## callees"] + callee_lines + ["",
                      "## ids"] + ids_lines)

    if bg2_db is None:
        cand = os.path.join(_home(), "wso", "bg2", "bg2_pdb.sqlite")
        bg2_db = cand if os.path.isfile(cand) else False
    bg2 = _bg2_lines(fname, bg2_db)
    if bg2:
        body += "\n\n" + "\n".join(bg2)

    header = (f"context bundle: {hexaddr} {fname or '(unknown)'} "
              f"~{len(body) // 4} tokens")
    return header + "\n" + body


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("address", help="function address, e.g. 0x5845D0")
    ap.add_argument("--exports", help="ghidra-exports dir "
                                      "(default $IWD2R_HOME/ghidra-exports)")
    ap.add_argument("--src", help="source root (default <repo>/src)")
    ap.add_argument("--ids", help="directory of .IDS files")
    ap.add_argument("--bg2-db", help="BG2 layout index "
                                     "(default $IWD2R_HOME/wso/bg2/"
                                     "bg2_pdb.sqlite if present)")
    ap.add_argument("--decompile",
                    help="decompile .c file (default <exports>/<name>.c)")
    args = ap.parse_args(argv)
    text = None
    if args.decompile:
        with open(args.decompile, encoding="utf-8", errors="replace") as f:
            text = f.read()
    bg2_db = False if args.bg2_db == "none" else args.bg2_db
    print(bundle(int(args.address, 16), exports=args.exports, src_root=args.src,
                 ids_dir=args.ids, bg2_db=bg2_db, decompile_text=text))
    return 0


if __name__ == "__main__":
    sys.exit(main())
