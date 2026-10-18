#!/usr/bin/env python3
"""Audit recovered C++ vtables in src/ against the real vtables in IWD2.exe.

Catches the failure mode that hid CGameSprite::SetCurrAction for days: a virtual
method the binary OVERRIDES, but our source never declared, so calls silently
dispatched to the base. Invisible at every call site; only visible at the vtable.

How it works (no RTTI in IWD2.exe, no Ghidra needed):
  1. Parse src/*.h for each class: its base and its virtual slots, taken from the
     `/* 00NN */ <ret> name(args) [override];` annotations (NN = byte offset in
     the vtable).
  2. Parse src/*.cpp for each method's recovered address: the `// 0xADDR` comment
     immediately above `<ret> Class::method(`.
  3. Locate each class's real vtable in .rdata by anchoring: a slot's recovered
     address A must appear as a dword at vtableBase + NN. The base that satisfies
     the most of the class's own slots is the vtable.
  4. Diff the class vtable against its base-class vtable, slot by slot. Every slot
     where they differ is an override the class performs in the binary -> our
     header MUST declare it at that slot. Flag:
       - MISSING : binary overrides slot NN, no source decl  (today's bug)
       - WRONGADDR: source decl at NN, but // 0xADDR != binary slot
       - SPURIOUS : source decl at NN, but binary == base (not actually overridden)
       - UNRECOV : binary slot points to a function with no // 0xADDR anywhere

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/vtable_audit.py,
byte-for-byte except the exe default (the fork's vendored .bin/iwd2.exe is
this repo's oracle.VANILLA_EXE; IWD2_EXE still overrides), the src_dir
parameter on the parsers (tools/parity/run.py audits another checkout), the
audit() split into locate_vtables + class_findings (the same loop, structured
returns instead of prints, so run.py's check() can reuse it per class), and
check(), the per-function entry run.py drives.

Usage:
  python3 tools/parity/vtable_audit.py                # audit every class
  python3 tools/parity/vtable_audit.py CGameSprite    # one class (+ its base)
  python3 tools/parity/vtable_audit.py --quiet        # only classes with findings

See docs/vtable_audit.md for triage notes, known noise, and example findings.
"""
import os
import re
import sys
import struct
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.dirname(TOOLS)
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import oracle  # noqa: E402
# Same resolution order as sym.py / ctor_vtable_check.py: the pristine copy
# vendored at .bin/iwd2.exe is canonical, so an audit never depends on where a
# game install happens to live.
EXE = os.environ.get("IWD2_EXE") or oracle.VANILLA_EXE
SRC = os.path.join(REPO, "src")

# ---- PE -------------------------------------------------------------------
import pefile


class Image:
    def __init__(self, path):
        pe = pefile.PE(path)
        self.ib = pe.OPTIONAL_HEADER.ImageBase
        self.img = pe.get_memory_mapped_image(ImageBase=self.ib)
        text = next(s for s in pe.sections if b".text" in s.Name)
        self.text_lo = self.ib + text.VirtualAddress
        self.text_hi = self.text_lo + text.Misc_VirtualSize
        # Index every dword in read-only / data sections that points into .text;
        # value -> list of VAs that store it (these are the vtable slots).
        self.slot_index = defaultdict(list)
        for s in pe.sections:
            nm = s.Name.rstrip(b"\x00")
            if nm not in (b".rdata", b".data"):
                continue
            base = self.ib + s.VirtualAddress
            data = s.get_data()
            for off in range(0, len(data) - 3, 4):
                v = struct.unpack_from("<I", data, off)[0]
                if self.text_lo <= v < self.text_hi:
                    self.slot_index[v].append(base + off)

    def d32(self, va):
        o = va - self.ib
        return struct.unpack_from("<I", self.img, o)[0]

    def is_code(self, v):
        return self.text_lo <= v < self.text_hi


# ---- source parsing -------------------------------------------------------
SLOT_RE = re.compile(r"/\*\s*([0-9A-Fa-f]{2,4})\s*\*/\s*(.*)")
NAME_RE = re.compile(r"([~\w]+)\s*\(")
CLASS_RE = re.compile(r"\bclass\s+(\w+)\b\s*(?::\s*(.*?))?\{", re.S)
ADDR_RE = re.compile(
    r"//\s*0x([0-9A-Fa-f]{5,8})\b[^\n]*\n"          # // 0xADDR
    r"(?:\s*//[^\n]*\n)*"                            # skip extra comment lines
    r"\s*[\w:<>,\*&\[\]\s]+?\b(\w+)::(~?\w+)\s*\(",  # ret Class::method(
)


def parse_headers(src_dir=None):
    """class -> {'base': str|None, 'slots': {offset:int -> methodName}}"""
    src = src_dir or SRC
    classes = {}
    for fn in os.listdir(src):
        if not fn.endswith(".h"):
            continue
        text = open(os.path.join(src, fn), encoding="utf-8", errors="replace").read()
        # Walk class headers in file order; assign slot lines to current class.
        marks = [(m.start(), m.group(1), m.group(2)) for m in CLASS_RE.finditer(text)]
        for i, (pos, cname, bases) in enumerate(marks):
            end = marks[i + 1][0] if i + 1 < len(marks) else len(text)
            body = text[pos:end]
            base = None
            if bases:
                bm = re.search(r"public\s+(\w+)", bases)
                if bm:
                    base = bm.group(1)
            slots = {}
            for line in body.splitlines():
                m = SLOT_RE.search(line)
                if not m:
                    continue
                try:
                    off = int(m.group(1), 16)
                except ValueError:
                    continue
                # Strip the trailing // comment first. In a binary-mirror header
                # /* NNNN */ also marks MEMBER offsets, and prose in a member's
                # comment can contain a "word(" that NAME_RE happily reads as a
                # method -- e.g. `/* 05E7 */ unsigned char m_animMode; // ...
                # random-sequence opt-in (==1)` yielded a phantom slot 0x5e7,
                # which pushed maxslot past the vtable and made the audit read
                # 203 findings out of the NEXT class's vtable.
                decl = m.group(2).split("//")[0]
                nm = NAME_RE.search(decl)
                if nm:
                    slots[off] = nm.group(1)
            if slots:
                classes[cname] = {"base": base, "slots": slots}
    return classes


def parse_addrs(src_dir=None):
    """(class, method) -> set of addrs (overloads) ; plus set of all recovered addrs."""
    src = src_dir or SRC
    amap = defaultdict(set)
    allad = set()
    for fn in os.listdir(src):
        if not fn.endswith(".cpp"):
            continue
        text = open(os.path.join(src, fn), encoding="utf-8", errors="replace").read()
        for m in ADDR_RE.finditer(text):
            addr = int(m.group(1), 16)
            amap[(m.group(2), m.group(3))].add(addr)
            allad.add(addr)
    return amap, allad


# ---- vtable location ------------------------------------------------------
def find_vtable(img, cls, classes, amap):
    """Return (vtableBase, matched_slot_count) for cls, or (None, 0)."""
    slots = classes[cls]["slots"]
    anchors = []  # (offset, frozenset-of-addrs)
    for off, name in slots.items():
        s = amap.get((cls, name))
        if s:
            anchors.append((off, s))
    if not anchors:
        return None, 0
    cand = Counter()
    for off, s in anchors:
        for a in s:
            for va in img.slot_index.get(a, ()):
                cand[va - off] += 1
    if not cand:
        return None, 0
    # Best candidate = validates the most of this class's own anchors.
    best, best_n = None, -1
    for base, _ in cand.most_common():
        n = sum(1 for off, s in anchors if img.d32(base + off) in s)
        if n > best_n or (n == best_n and (best is None or base < best)):
            best, best_n = base, n
    return best, best_n


def vtable_len(img, base, hard_cap=0x400, next_base=None):
    """Slots from `base` until the table ends.

    Walking while the dword looks like code is NOT enough on its own: vtables
    are laid out back-to-back in .rdata, so the next table's first entry is a
    code pointer too and the walk runs straight into it until the hard cap.
    `next_base` (the nearest vtable that starts after this one) is the real
    boundary; without it every class reports len 0x400.
    """
    n = 0
    limit = hard_cap if next_base is None else min(hard_cap, next_base - base)
    while n < limit and img.is_code(img.d32(base + n)):
        n += 4
    return n


# ---- audit ----------------------------------------------------------------
def own_slot_decl(classes, cls, off, stop_base):
    """Is slot `off` declared by cls or an ancestor above `stop_base` (exclusive)?
    Returns the declaring class name, or None."""
    c = cls
    while c is not None and c != stop_base:
        info = classes.get(c)
        if info and off in info["slots"]:
            return c
        info = classes.get(c)
        c = info["base"] if info else None
    return None


def locate_vtables(img, classes, amap):
    """Pass 1: locate every class's vtable, so pass 2 knows where each one ENDS.
    A vtable's boundary is the next vtable in .rdata; without this every table
    walks into its neighbour (see vtable_len)."""
    located = {}
    for c in sorted(classes):
        vb, nm = find_vtable(img, c, classes, amap)
        if vb is not None:
            located[c] = (vb, nm)
    return located


def class_findings(img, classes, amap, located, cls):
    """audit()'s per-class loop over one class, structured instead of printed.

    Returns {'findings': [(off, kind, msg)], 'skip': reason-or-None,
    'header': str-or-None}: skip carries the same reason audit() printed and
    went on (vtable not located, match weak); header is the === line for an
    audited class (findings may be empty -> clean).
    """
    all_bases = sorted({vb for vb, _ in located.values()})

    def next_base_after(vb):
        for x in all_bases:
            if x > vb:
                return x
        return None

    info = classes[cls]
    base = info["base"]
    vbase, nmatch = located.get(cls, (None, 0))
    if vbase is None:
        return {"findings": [], "header": None,
                "skip": "could not locate vtable (no anchored slots)"}
    # Require a credible match (>=2 anchors agree, or the only anchor matched).
    own_anchored = sum(1 for nm in info["slots"].values() if amap.get((cls, nm)))
    if nmatch < max(2, own_anchored // 2):
        return {"findings": [], "header": None,
                "skip": f"vtable match weak ({nmatch}/{own_anchored}) @ {vbase:#x}; skip"}

    vlen = vtable_len(img, vbase, next_base=next_base_after(vbase))
    # Cap structural checks at the highest slot we DID declare: a MISSING below
    # that line is a genuine hole between recovered slots (today's bug). Slots
    # above it are trailing new-virtuals / MFC message thunks -> too noisy.
    maxslot = max(info["slots"]) if info["slots"] else 0
    findings = []

    vbbase = None
    blen = 0
    if base and base in classes:
        vbbase, _ = find_vtable(img, base, classes, amap)
        # A base that resolves to the DERIVED class's own table means the
        # base lookup failed -- typically a base whose only anchor is a
        # method the derived class inherits verbatim, so both tables match
        # it. Comparing against it makes every slot look "== base" and
        # reports the whole class as SPURIOUS. Refuse the comparison
        # instead: no base means MISSING/WRONGADDR still work, and the
        # bogus SPURIOUS wave disappears.
        if vbbase == vbase:
            vbbase = None
        if vbbase is not None:
            blen = vtable_len(img, vbbase, next_base=next_base_after(vbbase))

    for off in range(0, min(vlen, maxslot + 4), 4):
        a = img.d32(vbase + off)
        decl_cls = own_slot_decl(classes, cls, off, base)
        decl_here = off in info["slots"]

        # Does the binary override this slot relative to the base vtable?
        overrides = vbbase is not None and (off >= blen or img.d32(vbbase + off) != a)

        if decl_here:
            want = amap.get((cls, info["slots"][off]))
            if want and a not in want:
                findings.append((off, "WRONGADDR",
                                 f"{info['slots'][off]} src {'/'.join(hex(x) for x in sorted(want))} != bin {a:#x}"))
            elif vbbase is not None and off < blen and img.d32(vbbase + off) == a:
                findings.append((off, "SPURIOUS",
                                 f"{info['slots'][off]} declared override but bin == base"))
        elif overrides and decl_cls is None:
            # The binary overrides this slot; no class from cls..base declares it.
            sym = symbolize(amap, a)
            findings.append((off, "MISSING",
                             f"binary overrides -> {a:#x}{sym}, no source decl"))

    header = (f"=== {cls}{' : ' + base if base else ''}  "
              f"vtable {vbase:#x} len {vlen:#x} (anchors {nmatch}/{own_anchored}) ===")
    return {"findings": findings, "skip": None, "header": header}


def audit(img, classes, amap, allad, only=None, quiet=False):
    findings_total = 0
    located = locate_vtables(img, classes, amap)
    for cls in sorted(classes):
        if only and cls != only:
            continue
        res = class_findings(img, classes, amap, located, cls)
        if res["skip"]:
            if only:
                print(f"[{cls}] {res['skip']}")
            continue
        findings = res["findings"]
        if findings or only:
            print(f"\n{res['header']}")
            if not findings:
                print("  clean")
            for off, kind, msg in sorted(findings):
                print(f"  [{kind:9}] slot {off:#06x}: {msg}")
        findings_total += len(findings)
    print(f"\nTOTAL findings: {findings_total}")
    return findings_total


def symbolize(amap, addr):
    for (c, m), s in amap.items():
        if addr in s:
            return f" ({c}::{m})"
    return ""


# ---- tools/parity/run.py entry ---------------------------------------------
# Verdict vocabulary must match tools/parity/run.py's exact strings.
GREEN, YELLOW, RED, SKIP = 'GREEN', 'YELLOW', 'RED', 'SKIP'
# Image() re-indexes every dword in .rdata/.data, and locate_vtables anchors
# every class -- both far too slow per row, so cache per (src dir, exe) for the
# length of one run (the tree does not change under a running gate).
_IMAGES = {}
_PARSED = {}
_LOCATED = {}


def check(address, name, repo, exports, exe):
    """run.py's vtable column: audit the class of this one function.

    Returns (verdict, notes) in run.py's vocabulary: RED on MISSING/WRONGADDR
    (the class's real overrides disagree with the source), YELLOW on SPURIOUS,
    SKIP when the name is not a class method, the class declares no virtual
    slots, or its vtable cannot be located/anchored (audit's own skip reasons).
    """
    cls = name.split('::')[0] if '::' in name else ''
    if not cls or cls == name:
        return SKIP, [f'{name} is not a class method']
    src = os.path.join(repo, 'src')
    if exe not in _IMAGES:
        _IMAGES[exe] = Image(exe)
    if src not in _PARSED:
        _PARSED[src] = (parse_headers(src), parse_addrs(src)[0])
    if (src, exe) not in _LOCATED:
        classes, amap = _PARSED[src]
        _LOCATED[(src, exe)] = locate_vtables(_IMAGES[exe], classes, amap)
    classes, amap = _PARSED[src]
    if cls not in classes:
        return SKIP, [f'{cls} declares no /* 00NN */ virtual slots in src/*.h']
    res = class_findings(_IMAGES[exe], classes, amap, _LOCATED[(src, exe)], cls)
    if res['skip']:
        return SKIP, [res['skip']]
    hard = [f for f in res['findings'] if f[1] in ('MISSING', 'WRONGADDR')]
    soft = [f for f in res['findings'] if f[1] not in ('MISSING', 'WRONGADDR')]
    notes = [res['header']]
    notes += [f'[{kind}] slot {off:#06x}: {msg}'
              for off, kind, msg in sorted(hard + soft)]
    if hard:
        return RED, notes
    if soft:
        return YELLOW, notes
    return GREEN, [f'{cls} vtable overrides match the source ({res["header"]})']


def main():
    args = [a for a in sys.argv[1:]]
    quiet = "--quiet" in args
    args = [a for a in args if not a.startswith("--")]
    only = args[0] if args else None

    img = Image(EXE)
    classes = parse_headers()
    amap, allad = parse_addrs()
    print(f"parsed {len(classes)} classes, {len(amap)} method addrs, "
          f"{len(img.slot_index)} distinct code-pointer slots in .rdata/.data")
    # Exit non-zero on findings so this can be used as a gate (arc.py, hooks).
    return 1 if audit(img, classes, amap, allad, only=only, quiet=quiet) else 0


if __name__ == "__main__":
    raise SystemExit(main())
