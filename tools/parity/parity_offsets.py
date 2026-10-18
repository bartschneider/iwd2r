#!/usr/bin/env python3
"""Member-offset parity: catch "right callee, right count, WRONG member" bugs.

`re-agent parity` checks callees and call-counts -- both match for the whole
class of bugs where a recovered function calls the right method on the WRONG
member (e.g. the 3-yr-old corpse-tint bug: ClearColorEffectsAll's extend block
called DeleteResPaletteAffect on m_*VidCellBase instead of m_*VidCellExtend).
The only discriminating signal is the per-thiscall `this` OFFSET, which the
high-level decompile masks but the asm exposes cleanly:

    lea ecx, [esi + 0x4EC]      ; this = &member-at-0x4EC
    call CVidCell::DeleteResPaletteAffect

This tool extracts the SET of `this`-relative offsets the ORIGINAL binary
function touches, maps them to members via the class header `/* 0xNNN */`
comments (the recovery's own name->offset annotations -- no PDB needed), and
diffs against the members the C++ source actually references. A member the
BINARY touches but the SOURCE never names = a wrong/missing member.

    python3 tools/parity/parity_offsets.py 0x6E6490
    python3 tools/parity/parity_offsets.py 0x6E6490 --src src/Foo.cpp:1802-1909 --class CClass
    python3 tools/parity/parity_offsets.py --sweep "src/CGameAnimationType*.cpp"   # scan a glob

Function bounds come from the Ghidra export set (functions.tsv, every fn). A
(member,method) is only flagged when the source already calls that method on some
OTHER member -- the copy-paste / incomplete-member-set signature -- so whole-method
gaps (stub/incomplete recoveries like ProcessEffectList) are not mistaken for it.

SWEET SPOT: straight-line member-heavy fns that call methods on direct member
OBJECTS (the anim cell/colour methods). Pointer-heavy render fns (Blt*, Render3d)
mis-attribute the `this` (an &member ARG looks like a receiver) -> verify those by
hand. Needs capstone + pefile (tools/parity/setup.sh). Run from anywhere.

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/parity_offsets.py,
byte-for-byte in the pairing/adj/flag logic, except: disasm() reads the exe
directly with capstone (the fork shelled out to scripts/sym.py, which is not
part of this tree) and annotates direct call targets from functions.tsv;
resolve_source() scans the src/ `// 0xADDR` markers (scripts/src_find.py is
not part of this tree either); header_offset_map() finds the class's header
wherever it lives under src/ (the fork assumed src/<Class>.h); function
bounds come from the ExportCallGraph functions.tsv (same file run.py reads);
and check(), the per-function entry tools/parity/run.py drives.
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from collections import Counter
from pathlib import Path

import pefile  # noqa: E402  (absent -> tools/parity/run.py SKIPs this column)
import capstone  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = Path(__file__).resolve().parents[2]
if HERE not in sys.path:
    sys.path.insert(0, HERE)
import asm_verify  # noqa: E402
import address_markers  # noqa: E402  (functions.tsv parser, shared with run.py)
import oracle  # noqa: E402  (exe default for the standalone CLI)

SRC = REPO / "src"

# `this` is parked in one of these by the prologue (`mov <reg>, ecx`).
THIS_REGS = ("esi", "edi", "ebx", "ebp")
ECX_LEA_RE = re.compile(r"\b(?:lea|mov)\s+ecx,\s*\[(e[a-z][a-z]) \+ (0x[0-9a-fA-F]+)\]")
ECX_OTHER_RE = re.compile(r"\bmov\s+ecx,")          # ecx set from something else -> this unknown
CALL_RE = re.compile(r"\bcall\b.*?;\s*(\S+)")        # call ... ; Class::Method
LINE_ADDR_RE = re.compile(r"^0x([0-9a-fA-F]+):")     # disasm line address
ADDR_COMMENT_RE = re.compile(r"//\s*0x([0-9A-Fa-f]+)\b")  # `// 0xADDR` recovery markers
MOV_THIS_RE = re.compile(r"\bmov\s+(e[a-z][a-z]),\s+ecx\b")
DECL_RE = re.compile(r"/\*\s*([0-9A-Fa-f]+)\s*\*/\s*[^;{]*?\bm_(\w+)\s*(?:\[[^\]]*\])?\s*;")
# member method call: m_foo.Bar(  /  m_foo->Bar(
MEMBER_CALL_RE = re.compile(r"\bm_(\w+)\s*(?:\.|->)\s*(\w+)\s*\(")


# The exe capstone reads (set by check() / main(); the fork shelled out to
# scripts/sym.py, not part of this tree). disasm() keeps sym.py's output shape
# (`0x<addr>:` lines, `; <name>` call annotations) so binary_call_pairs is
# unchanged.
_DISASM_EXE = None
_DISASM_NAMES: dict[int, str] = {}
_SECS_CACHE: dict = {}


def _secs(exe) -> dict:
    if exe not in _SECS_CACHE:
        pe = pefile.PE(exe, fast_load=True)
        base = pe.OPTIONAL_HEADER.ImageBase
        secs = {}
        for s in pe.sections:
            nm = s.Name.rstrip(b"\x00").decode("latin1")
            secs[nm] = (base + s.VirtualAddress,
                        base + s.VirtualAddress + s.Misc_VirtualSize,
                        s.get_data())
        _SECS_CACHE[exe] = secs
    return _SECS_CACHE[exe]


def _read(secs, va, n):
    for lo, hi, data in secs.values():
        if lo <= va < hi:
            off = va - lo
            return data[off:off + n]
    return b""


def disasm(addr: int, count: int) -> list[str]:
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    code = _read(_secs(_DISASM_EXE), addr, count * 16 + 16)
    out = []
    for ins in md.disasm(code, addr):
        line = f"0x{ins.address:x}:\t{ins.mnemonic}\t{ins.op_str}"
        if ins.mnemonic == "call":                    # direct target -> annotate
            try:
                nm = _DISASM_NAMES.get(int(ins.op_str, 16))
            except ValueError:
                nm = None
            if nm:
                line += f"  ; {nm}"
        out.append(line)
        if len(out) >= count:
            break
    return out


def binary_call_pairs(addr: int, count: int, end_addr: int | None) -> tuple[Counter, str]:
    """Multiset of (this_offset, method) for each thiscall: `lea ecx,[this+off]; call M`.

    Stops at end_addr (the next function start) so it never bleeds into the next
    function and invents pairs it never makes.
    """
    lines = disasm(addr, count)
    this_reg = "esi"
    for ln in lines[:14]:
        m = MOV_THIS_RE.search(ln)
        if m and m.group(1) in THIS_REGS:
            this_reg = m.group(1)
            break

    pairs: Counter = Counter()
    pending = None  # this-offset staged for the next call
    for ln in lines:
        if end_addr is not None:
            am = LINE_ADDR_RE.match(ln)
            if am and int(am.group(1), 16) >= end_addr:
                break
        m = ECX_LEA_RE.search(ln)
        if m:
            pending = int(m.group(2), 16) if m.group(1) == this_reg else None
            continue
        if ECX_OTHER_RE.search(ln):  # ecx loaded from non-this -> unknown receiver
            pending = None
            continue
        c = CALL_RE.search(ln)
        if c:
            if pending is not None and not _is_noise_callee(c.group(1)):
                method = c.group(1).replace("__", "::").split("::")[-1]
                pairs[(pending, method)] += 1
            pending = None
    return pairs, this_reg


def _is_noise_callee(callee: str) -> bool:
    """Skip implicit ctor/dtor and free asserts -- never written in the source body, so
    they always look 'missing' and drown the real wrong-member signal."""
    parts = callee.replace("__", "::").split("::")
    method = parts[-1]
    cls = parts[-2] if len(parts) > 1 else ""
    if method.startswith("~") or method == cls:          # dtor / ctor
        return True
    if "Assert" in method or "operator" in method:       # UTIL_ASSERT, operator=, etc.
        return True
    return False


def _hdr_for(class_name: str, src: Path):
    """The src/*.h declaring `class <name>` (classes do not all live in <name>.h)."""
    pat = re.compile(rf"\bclass\s+{re.escape(class_name)}\b")
    for h in sorted(src.glob("*.h")):
        if pat.search(h.read_text(errors="replace")):
            return h
    return None


def header_offset_map(class_name: str, src_dir: Path | None = None) -> dict[str, int]:
    """member-name -> declared offset, from `/* 0xNNN */ ... m_name;` comments.

    Walks the class header and its transitive base-class headers. The fork
    opened src/<Class>.h only; here the primary header is whichever src/*.h
    declares the class.
    """
    src = src_dir or SRC
    out: dict[str, int] = {}
    seen: set[Path] = set()

    def hdr_for(name: str) -> Path:
        h = _hdr_for(name, src)
        return h if h is not None else src / f"{name}.h"

    def absorb(hpath: Path):
        if hpath in seen or not hpath.exists():
            return
        seen.add(hpath)
        text = hpath.read_text(errors="replace")
        for m in DECL_RE.finditer(text):
            out.setdefault(f"m_{m.group(2)}", int(m.group(1), 16))
        for bm in re.finditer(r":\s*(?:public|protected|private)?\s*(\w+)", text):
            absorb(hdr_for(bm.group(1)))

    absorb(hdr_for(class_name))
    return out


def resolve_source(addr: int, src_dir: Path | None = None):
    """(file, start, end, class) for the // 0xADDR body in src/*.cpp.

    The fork resolved this via scripts/src_find.py (not part of this tree);
    the same `// 0xADDR` marker scan as the gate's asm column gives the span.
    """
    src = src_dir or SRC
    for cpp in sorted(src.glob("*.cpp")):
        text = cpp.read_text(errors="replace")
        span = asm_verify.find_span(text, addr)
        if span is None:
            continue
        start = text.count("\n", 0, span[0]) + 1
        end = text.count("\n", 0, span[1]) + 1
        cm = re.search(r"\b(\w+)::~?\w+", text[span[0]:span[1]])
        return cpp, start, end, (cm.group(1) if cm else None)
    return None


def _exports_dir() -> Path:
    """The same rule tools/parity/run.py's exports_dir() applies: $IWD2R_HOME
    (else ~/.local/share/iwd2r) + /ghidra-exports."""
    home = os.environ.get("IWD2R_HOME") or os.path.expanduser("~/.local/share/iwd2r")
    return Path(home) / "ghidra-exports"


_EXPORT_ADDRS: list[int] | None = None


def _export_addrs() -> list[int]:
    """Sorted start addresses of EVERY Ghidra function (recovered or not) -- the
    authoritative function boundaries. Beats source `// 0xADDR` markers, which only
    cover recovered fns (so they overshoot into an unrecovered following function)."""
    global _EXPORT_ADDRS
    if _EXPORT_ADDRS is None:
        tsv = _exports_dir() / "functions.tsv"
        if tsv.is_file():
            _EXPORT_ADDRS = sorted(address_markers.read_functions(tsv)[1])
        else:
            _EXPORT_ADDRS = []
    return _EXPORT_ADDRS


def function_end(path: Path, addr: int) -> int | None:
    """Next function start = the boundary. Prefer the Ghidra export set (all fns);
    fall back to the file's `// 0xADDR` markers."""
    import bisect

    exps = _export_addrs()
    i = bisect.bisect_right(exps, addr)
    if i < len(exps):
        return exps[i]
    addrs = sorted({int(m.group(1), 16) for m in ADDR_COMMENT_RE.finditer(path.read_text(errors="replace"))})
    nexts = [a for a in addrs if a > addr]
    return nexts[0] if nexts else None


def source_pairs(path: Path, start: int, end: int) -> Counter:
    lines = path.read_text(errors="replace").splitlines()
    body = "\n".join(lines[start - 1 : end])
    return Counter((f"m_{m.group(1)}", m.group(2)) for m in MEMBER_CALL_RE.finditer(body))


def method_adj(offsets: list[int], hvals: set[int]) -> int:
    """Per-method base-subobject shift: this-offset = member_offset + adj (CVidImage=+4, CVidCell=+0)."""
    deltas: Counter = Counter()
    for off in offsets:
        for d in (0, 4, 8, 12, 16):
            if off - d in hvals:
                deltas[d] += 1
    return deltas.most_common(1)[0][0] if deltas else 0


_HDR_CACHE: dict[str, dict[str, int]] = {}


def header_map_cached(cls: str) -> dict[str, int]:
    if cls not in _HDR_CACHE:
        _HDR_CACHE[cls] = header_offset_map(cls)
    return _HDR_CACHE[cls]


def analyze(addr: int, path: Path, start: int, end: int, cls: str, base_count: int):
    """-> (missing, n_bin, n_src, this_reg, hdr, adj, bin_pairs) or None if not analysable."""
    hdr = header_map_cached(cls)
    if not hdr:
        return None
    end_addr = function_end(path, addr)
    count = base_count
    if end_addr is not None:
        count = max(count, (end_addr - addr) // 2 + 32)
    pairs, this_reg = binary_call_pairs(addr, count, end_addr)
    if not pairs:
        return None
    off2name = {off: name for name, off in hdr.items()}
    hvals = set(hdr.values())
    by_method: dict[str, list[int]] = {}
    for (off, meth), n in pairs.items():
        by_method.setdefault(meth, []).extend([off] * n)
    adj = {meth: method_adj(offs, hvals) for meth, offs in by_method.items()}
    bin_pairs: Counter = Counter()
    for (off, meth), n in pairs.items():
        name = off2name.get(off - adj[meth])
        if name:
            bin_pairs[(name, meth)] += n
    src = source_pairs(path, start, end)
    src_methods = {meth for _, meth in src}
    # Only flag (member, method) the binary calls but the source doesn't, WHERE the
    # source already calls that method on some OTHER member. That's the copy-paste /
    # incomplete-member-set signature (corpse-tint, the LayeredSpell weapon cells).
    # If the method is absent from the source entirely, it's an incomplete/stub
    # recovery (e.g. ProcessEffectList) -- a different problem, not "wrong member".
    missing = sorted(p for p in (set(bin_pairs) - set(src)) if p[1] in src_methods)
    return missing, len(bin_pairs), len(set(src)), this_reg, hdr, adj, bin_pairs


# ---- tools/parity/run.py entry ---------------------------------------------
# Verdict vocabulary must match tools/parity/run.py's exact strings.
GREEN, YELLOW, RED, SKIP = 'GREEN', 'YELLOW', 'RED', 'SKIP'


def check(address, name, repo, exports, exe):
    """run.py's member-offset column for this one function.

    Returns (verdict, notes) in run.py's vocabulary: RED when the binary calls
    a (member, method) the source never names while the source calls that
    method on some OTHER member (the wrong-member signature); SKIP when the
    name is not a class method, no src header declares the class, the address
    has no `// 0xADDR` marker, or analyze() cannot analyse the function (no
    /* 0xNNN */ header offsets or no thiscalls). There is no YELLOW:
    analyze() drops binary pairs it cannot map to a header member, exactly as
    the fork's CLI does.
    """
    global _DISASM_EXE, _DISASM_NAMES, _EXPORT_ADDRS
    cls = name.split('::')[0] if '::' in name else ''
    if not cls or cls == name:
        return SKIP, [f'{name} is not a class method']
    src = Path(repo) / 'src'
    if _hdr_for(cls, src) is None:
        return SKIP, [f'no src/*.h declares class {cls}']
    r = resolve_source(address, src)
    if r is None:
        return SKIP, [f'no // 0x{address:X} marker in src/*.cpp']
    path, start, end, found = r
    kls = found or cls
    _DISASM_EXE = exe
    _DISASM_NAMES = exports[0]
    _EXPORT_ADDRS = sorted(exports[1])
    # analyze() resolves the header through header_map_cached (keyed by class
    # name only); seed it from the audited checkout, not this repo's src/.
    _HDR_CACHE[kls] = header_offset_map(kls, src)
    res = analyze(address, path, start, end, kls, 600)
    if res is None:
        return SKIP, [f'{kls} at 0x{address:X} is not analysable '
                      '(no /* 0xNNN */ header offsets or no thiscalls)']
    missing, n_bin, n_src, this_reg, hdr, adj, bin_pairs = res
    if missing:
        notes = [f'{kls}: binary {n_bin} (member,method) pair(s), source {n_src}, '
                 f'this={this_reg}; binary calls, source never does:']
        notes += [f'{nm}.{meth}()  (member 0x{hdr[nm]:X}, '
                  f'this 0x{hdr[nm] + adj[meth]:X}, {bin_pairs[(nm, meth)]}x)'
                  for nm, meth in missing]
        return RED, notes
    return GREEN, [f'{kls}: {n_bin} binary (member,method) pair(s) all named in '
                   f'source (this={this_reg})']


FUNC_SIG_RE = re.compile(r"^[A-Za-z][\w:<>,\*&\s]*?\b(\w+)::~?(\w+)\s*\(")


def enumerate_functions(path: Path):
    """(addr, class, method, start_line, end_line) for each top-level recovered fn in the file."""
    lines = path.read_text(errors="replace").splitlines()
    raw = []  # (addr, cls, method, defline_idx)
    pending = None
    for i, ln in enumerate(lines):
        if ln.startswith("// 0x"):                       # top-level address marker only
            m = ADDR_COMMENT_RE.search(ln)
            if m:
                pending = int(m.group(1), 16)
            continue
        if pending is not None and not ln.startswith(("//", " ", "\t", "/*", "*")):
            sm = FUNC_SIG_RE.match(ln)
            if sm and not ln.rstrip().endswith(";"):
                raw.append((pending, sm.group(1), sm.group(2), i))
                pending = None
    out = []
    for j, (addr, cls, meth, di) in enumerate(raw):
        end_line = raw[j + 1][3] - 1 if j + 1 < len(raw) else len(lines)
        out.append((addr, cls, meth, di + 1, end_line))
    return out


def report_one(addr: int, cls: str, path: Path, start: int, end: int, res) -> None:
    missing, n_bin, n_src, this_reg, hdr, adj, bin_pairs = res
    print(f"== {cls}::?  0x{addr:X}  ({path.name}:{start}-{end})  this={this_reg}  bin={n_bin} src={n_src}")
    if missing:
        print("  !! WRONG/MISSING MEMBER -- binary calls these, source never does:")
        for name, meth in missing:
            off = hdr[name]
            print(f"     {name}.{meth}()  (member 0x{off:X}, this 0x{off + adj[meth]:X}, {bin_pairs[(name, meth)]}x)")
    else:
        print("  OK")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", nargs="?", help="binary function address, e.g. 0x6E6490")
    ap.add_argument("--count", type=int, default=600, help="instructions to disasm")
    ap.add_argument("--src", help="file.cpp:start-end (override src_find)")
    ap.add_argument("--class", dest="cls", help="class name (override)")
    ap.add_argument("--sweep", nargs="+", metavar="FILE", help="scan every recovered fn in these .cpp files")
    args = ap.parse_args()

    # The standalone CLI reads the canonical vendored exe and, when the Ghidra
    # export exists, annotates direct calls / bounds from functions.tsv; the
    # gate's check() sets the same globals per row instead.
    global _DISASM_EXE, _DISASM_NAMES
    _DISASM_EXE = os.environ.get("IWD2_EXE") or oracle.VANILLA_EXE
    tsv = _exports_dir() / "functions.tsv"
    if tsv.is_file():
        _DISASM_NAMES.update(address_markers.read_functions(tsv)[0])

    if args.sweep:
        files = []
        for pat in args.sweep:
            p = Path(pat)
            files.extend(sorted(SRC.glob(pat)) if not p.exists() else [p])
        hits = 0
        checked = 0
        for f in files:
            for addr, cls, meth, start, end in enumerate_functions(f):
                res = analyze(addr, f, start, end, cls, args.count)
                if res is None:
                    continue
                checked += 1
                missing = res[0]
                if missing:
                    hits += len(missing)
                    print(f"\n!! {cls}::{meth}  0x{addr:X}  ({f.name}:{start}-{end})")
                    _, _, _, _, hdr, adj, bin_pairs = res
                    for name, mm in missing:
                        off = hdr[name]
                        print(f"     {name}.{mm}()  (member 0x{off:X}, this 0x{off + adj[mm]:X}, {bin_pairs[(name, mm)]}x)")
        print(f"\n== sweep done: {checked} member-heavy fn(s) checked, {hits} suspect (member,method) pair(s).")
        return 1 if hits else 0

    if not args.addr:
        ap.error("give an addr or --sweep FILES")
    addr = int(args.addr, 16)
    if args.src:
        f, rng = args.src.split(":")
        s, e = rng.split("-")
        path, start, end, cls = REPO / f, int(s), int(e), args.cls
    else:
        r = resolve_source(addr)
        if not r:
            print(f"no // {args.addr} marker in src/*.cpp; pass --src", file=sys.stderr)
            return 2
        path, start, end, cls = r
        if args.cls:
            cls = args.cls
    if not cls:
        print("could not determine class; pass --class", file=sys.stderr)
        return 2

    res = analyze(addr, path, start, end, cls, args.count)
    if res is None:
        print(f"not analysable (no header offsets or no thiscalls) for {cls}", file=sys.stderr)
        return 2
    missing = res[0]
    report_one(addr, cls, path, start, end, res)
    return 1 if missing else 0


if __name__ == "__main__":
    raise SystemExit(main())
