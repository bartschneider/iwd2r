#!/usr/bin/env python3
"""src_find.find -- resolve a name or address to (file, line, signature).

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/src_find.py: the
query grammar (``0xADDR``, ``Class::`` prefix, ``Class::Method``,
bare name), the ``// 0xADDR`` marker + signature scan, and the
skip-blank-and-comment walk to the function line are his.  His version
worked over a flat copy of the fork's src/ tree; ours walks the repo
root given (default: ``<repo>/src``) and reports repo-relative paths,
so an agent can open the hit directly.

The index is built once per root (an os.walk over .cpp/.h/.hpp collecting
``// 0xADDR`` markers and the signature line that follows each) and can
be shared across calls via ``cache={}`` -- assemble_context passes one
cache through a whole bundle.  No grep chains.

One deviation from tools/parity/address_markers.py, on purpose: the
marker lint bounds addresses to the code range (CODE_MIN..CODE_MAX)
because it judges whether an address can start a function; a lookup has
no such question to ask, so any address in a marker is indexed.

Line numbers point at the marker (the fork's dict points at the
definition line); the tests freeze the marker convention, with a
marker on a file's first line reported as line 2.

Usage::

    python3 tools/src_find.py 'CInfGame::GetHappiness'
    python3 tools/src_find.py 0x5845D0 [--root src]
"""
from __future__ import annotations

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DEFAULT_ROOT = os.path.join(ROOT, "src")

FILE_EXTENSIONS = (".cpp", ".h", ".hpp")
ADDR_RE = re.compile(r"^[ \t]*//[ \t]*0x([0-9A-Fa-f]+)\b")
QUAL_RE = re.compile(r"(~?\w+)\s*::\s*(~?\w+|operator\s*(?:\[\]|\(\)|[^\s(]+))\s*\(")
INLINE_RE = re.compile(r"(~?\w+)\s*\(")
CPP_KEYWORDS = frozenset({
    "if", "while", "for", "switch", "else", "return", "do", "case", "default",
    "goto", "break", "continue", "sizeof", "new", "delete", "catch", "throw",
})


def _signature_line(lines, start):
    """The first code line after the marker, or None if another marker
    comes first (a dangling marker names nothing)."""
    for j in range(start + 1, len(lines)):
        stripped = lines[j].strip()
        if not stripped or stripped.startswith("//"):
            if ADDR_RE.match(lines[j]):
                return None
            continue
        return lines[j]
    return None


def _index(root, cache=None):
    """[(address, qualname, rel_path, line, signature)] in file order."""
    key = os.path.abspath(root)
    if cache is not None and key in cache:
        return cache[key]
    entries = []
    if os.path.isdir(root):
        for dirpath, dirs, files in os.walk(root):
            dirs[:] = sorted(d for d in dirs if d != ".git")
            for name in sorted(files):
                if not name.lower().endswith(FILE_EXTENSIONS):
                    continue
                path = os.path.join(dirpath, name)
                rel = os.path.relpath(path, root).replace(os.sep, "/")
                file_class = os.path.splitext(name)[0]
                with open(path, encoding="utf-8", errors="ignore") as f:
                    lines = f.read().splitlines()
                for i, line in enumerate(lines):
                    m = ADDR_RE.match(line)
                    if not m:
                        continue
                    sig = _signature_line(lines, i)
                    if sig is None:
                        continue
                    qm = QUAL_RE.search(sig)
                    if qm:
                        cls = qm.group(1)
                        # Collapse 'operator new' -> 'operatornew' so the
                        # qualname is one token (query 'X::operatornew').
                        func = re.sub(r"\s+", "", qm.group(2))
                    else:
                        im = INLINE_RE.search(sig)
                        if not im:
                            continue
                        cls, func = file_class, im.group(1)
                    if func in CPP_KEYWORDS:
                        continue
                    # Line convention (the tests' frozen contract): the
                    # marker's 1-based line, so an editor lands with the
                    # marker and the signature together on screen.  A
                    # marker on a file's first line reports line 2 (the
                    # signature) instead -- there is no line 0 to open.
                    entries.append((int(m.group(1), 16), f"{cls}::{func}",
                                    rel, max(i + 1, 2), sig.strip()))
        entries.sort(key=lambda e: (e[2], e[3]))
    if cache is not None:
        cache[key] = entries
    return entries


def _is_hex(text):
    return bool(text) and all(c in "0123456789abcdefABCDEF" for c in text)


def find(query: str, root=None, cache=None) -> list:
    """Resolve ``0xADDR``, ``Class::`` prefix, ``Class::Method`` or a bare
    name to [(file, line, signature)], in file order."""
    if root is None:
        root = DEFAULT_ROOT
    entries = _index(root, cache)
    q = query.strip()
    if q.lower().startswith("0x") and _is_hex(q[2:]):
        target = int(q, 16)
        return [(rel, line, sig) for addr, _qual, rel, line, sig in entries
                if addr == target]
    if q.endswith("::"):
        return [(rel, line, sig) for _addr, qual, rel, line, sig in entries
                if qual.startswith(q)]
    if "::" in q:
        return [(rel, line, sig) for _addr, qual, rel, line, sig in entries
                if qual == q]
    return [(rel, line, sig) for _addr, qual, rel, line, sig in entries
            if qual == q or qual.rsplit("::", 1)[-1] == q]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("query", help="0xADDR, 'Class::', 'Class::Method' or name")
    ap.add_argument("--root", default=DEFAULT_ROOT,
                    help="source root to index (default: <repo>/src)")
    args = ap.parse_args(argv)
    hits = find(args.query, root=args.root)
    for rel, line, sig in hits:
        print(f"{rel}:{line}  {sig}")
    if not hits:
        print(f"src_find: no match for {args.query!r} under {args.root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
