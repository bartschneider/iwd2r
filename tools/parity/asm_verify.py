"""Set-diff the callees of a recovered function against the binary's callees.

Adapted from WillScarlettOhara/iwd2-re@e8dd16a scripts/reagent_asm_verify.py:
the verdict logic (buckets, leaf matching, RED/YELLOW/GREEN reasons) is kept
byte-for-byte; the data layer is ours. The fork shelled ghidra-bridge to dump
the function's disassembly and resolved CALL operands through an address map;
here ExportCallGraph.java has already walked the program, so functions.tsv
names the callee addresses and callgraph.tsv lists each function's direct
callees.

The compiler-independent invariant is the call graph: *what* the function
calls, not which instructions (a different register allocator emits different
code for identical behaviour).

KNOWN FALSE POSITIVES (this is a LINT, not an oracle -- a human confirms RED;
declare one with parity_waiver in the evidence file):
  * INLINING -- the dominant one. If the binary inlined a helper our source
    still calls as one function, the helper's own calls surface as direct
    calls of the caller and read as "missing". See the fork's header for the
    C2DArray::Load / CRes::SetResRef case.
  * LEAF COLLISION -- callees match on the bare leaf (``Method``) because
    source calls through ``this`` lose the class. Widens matches (favours
    GREEN), never invents a miss.

One blind spot the fork's asm dump had and a static call graph keeps: virtual
dispatch. Ghidra's call graph lists direct calls, so ``CALL [reg+slot]``
callees are simply absent -- the same calls the fork bucketed as ``indirect``
and never name-diffed. They now just do not appear; a missing virtual is
caught by the vtable check instead.

Usage::

    python3 tools/parity/asm_verify.py --address 0x401000
"""
from __future__ import annotations

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

# A C++ call token in our source.
SRC_CALL_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_:]*)\s*\(")

# Tokens that are not real calls in C++ source.
SRC_SKIP = {
    "if", "for", "while", "switch", "return", "sizeof", "alignof", "decltype",
    "static_cast", "reinterpret_cast", "const_cast", "dynamic_cast", "catch",
    "new", "delete", "do", "else", "operator",
}
# MSVC / CRT helpers the compiler injects -- never in our source, so excluding them
# keeps the diff from raising false under-implementation findings.
CRT_HELPER_RE = re.compile(
    r"(?i)(^_+chkstk|^__alloca|alloca_probe|^__SEH_|security_(check_)?cookie|"
    r"report_gsfailure|CxxFrameHandler|_EH_|except_handler|^__GSHandler|"
    r"^_+ftol|^_+allmul|^_+aulldiv|^_+alldiv|^_+aullrem|^_+allrem|^_+allshl|^_+aullshr|^_+allshr)"
)

# Ghidra's placeholder names: an address, not a recovered name.
UNNAMED_PREFIXES = ("FUN_", "sub_", "thunk")


def leaf(name: str) -> str:
    """Reduce ``CClass::Method`` / ``obj.Method`` to the bare leaf ``Method``."""
    for sep in ("::", "->", "."):
        if sep in name:
            name = name.rsplit(sep, 1)[-1]
    return name


def callee_diff(src_callees: set[int], bin_callees: set[int]) -> tuple[set[int], set[int]]:
    """Address-level diff: (missing in src, extra in src)."""
    return bin_callees - src_callees, src_callees - bin_callees


def src_call_leaves(body: str) -> set[str]:
    """The call tokens of a recovered function body, reduced to leaves.

    Comments are stripped first -- a commented-out call is not a call.
    """
    body = re.sub(r"//.*", "", re.sub(r"/\*.*?\*/", "", body, flags=re.S))
    return {leaf(m.group(1)) for m in SRC_CALL_RE.finditer(body)
            if m.group(1) not in SRC_SKIP}


def game_callees(callees, names: dict[int, str]) -> dict:
    """Classify callgraph.tsv callees into game / crt / unresolved.

    The fork's parse_asm_callees with the address-shaped operands only (the
    asm-text shapes have no counterpart in a call graph). ``game`` maps the
    callee's leaf to its full export name for reporting.
    """
    game: dict[str, str] = {}     # leaf -> full name (for reporting)
    crt: set[str] = set()
    unresolved: set[str] = set()
    for addr in sorted(callees):
        name = names.get(addr)
        if name is None or name.startswith(UNNAMED_PREFIXES):
            unresolved.add(f"0x{addr:06x}")
        elif CRT_HELPER_RE.search(name):
            crt.add(name)
        else:
            game[leaf(name)] = name
    return {"game": game, "crt": sorted(crt), "unresolved": sorted(unresolved)}


def verdict(src_found: bool, missing: list[str], buckets: dict) -> tuple[str, str]:
    """The fork's decision tree, unchanged but for the word 'asm'."""
    if src_found is False:
        return "YELLOW", "source body not found (address not in src/ or no // marker)"
    if missing:
        return "RED", (f"{len(missing)} named callee(s) in the binary not in src "
                       "(confirm not inlined helpers)")
    if buckets["unresolved"]:
        return "YELLOW", "only unresolved calls remain to confirm"
    return "GREEN", "every named callee of the binary is present in src"


def verify(address: int, span_text: str | None, exports) -> tuple[str, str, dict]:
    """Full check for one function: (verdict, reason, detail).

    ``exports`` is the (names, sizes, callees) triple from run.load_exports;
    ``span_text`` is the recovered body (None when the marker has no body).
    """
    names, _, callees = exports
    buckets = game_callees(callees.get(address, ()), names)
    src_leaves = src_call_leaves(span_text) if span_text else set()
    missing = sorted({full for lf, full in buckets["game"].items()
                      if lf not in src_leaves})
    v, reason = verdict(span_text is not None, missing, buckets)
    return v, reason, {
        "missing": missing,
        "game_callees": sorted(set(buckets["game"].values())),
        "crt_ignored": buckets["crt"],
        "unresolved": buckets["unresolved"],
        "src_call_leaves": sorted(src_leaves),
    }


def main(argv=None) -> int:
    import run as parity_run
    ap = argparse.ArgumentParser(
        description=__doc__.splitlines()[0])
    ap.add_argument("--address", required=True,
                    help="target function address (e.g. 0x401000)")
    ap.add_argument("--repo", default=parity_run.ROOT, help="repository root")
    ap.add_argument("--exports", help="ghidra-exports dir (default $IWD2R_HOME)")
    args = ap.parse_args(argv)

    address = int(args.address, 16)
    exports = parity_run.load_exports(args.exports)
    if exports is None:
        sys.exit("asm_verify: no functions.tsv/callgraph.tsv under "
                 f"{args.exports or parity_run.exports_dir()}; "
                 "run tools/ghidra/ExportCallGraph.java")

    path = span_text = None
    src = os.path.join(args.repo, 'src')
    for root, dirs, files in os.walk(src):
        dirs[:] = [d for d in dirs if d != '.git']
        for name in sorted(files):
            if not name.endswith(('.cpp', '.h', '.hpp')):
                continue
            full = os.path.join(root, name)
            with open(full, encoding='utf-8', errors='replace') as f:
                text = f.read()
            span = find_span(text, address)
            if span:
                path, span_text = full, text[span[0]:span[1]]
                break
        if path:
            break

    v, reason, detail = verify(address, span_text, exports)
    print(f"{v}  0x{address:X}  ({reason})")
    print(f"  src: {path}")
    print(f"  named callees: {len(detail['game_callees'])}  "
          f"| crt-ignored: {len(detail['crt_ignored'])}  "
          f"| unresolved: {len(detail['unresolved'])}")
    if detail["missing"]:
        print("  MISSING in src (binary calls, recovered fn does not):")
        for n in detail["missing"]:
            print(f"    - {n}")
        print("  ^ confirm by hand: a common benign cause is the binary inlining a helper")
        print("    your source still calls (the helper's own calls then read as missing).")
    return 0 if v != "RED" else 1


def find_span(text: str, address: int):
    """(start, end) character indexes of the ``// 0xADDR`` function body."""
    marker = re.compile(rf"^// 0x{address:X}\s*$", re.M)
    m = marker.search(text)
    if not m:
        return None
    start = m.start()
    tail = text[m.end():]
    e = re.search(r"^\}", tail, re.M)
    return start, m.end() + (e.end() if e else len(tail))


if __name__ == "__main__":
    raise SystemExit(main())
