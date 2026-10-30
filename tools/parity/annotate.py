#!/usr/bin/env python3
"""An annotated disassembly of one exe function, for reviews.

    python3 -I tools/parity/annotate.py 0xADDR [--end 0xADDR] [--exe PATH]

Disassembles the function at ADDR from the vanilla exe (the path run.py
uses; --exe overrides) with capstone, the way parity_offsets.py opens the
exe. The end is the function's size from the exports (run.load_exports,
functions.tsv); --end overrides it.

One line per instruction: address, mnemonic and operands, plus the
annotations a reviewer would otherwise look up by hand:

  `; <name>` on a direct call or jmp
      The target's name: our tree's `// 0xADDR` marker first (the source
      scan run.py/address_markers.py use -- a name there is what a PR's
      diff must agree with), else the export's name (functions.tsv, which
      carries the IEex names when the Ghidra project was built with
      --ieex), else `?`. An in-function jump reads `?` too: it has no name
      anywhere, and the address says where it lands.
  `; "text"` on an immediate or displacement
      The operand points into the exe's data at a NUL-terminated printable
      string (bytes 0x20-0x7E plus \\t\\r\\n, at least one char; data
      sections only, so code bytes never read as a literal). Escaped, cut
      at 60 chars. This is the column PR #47 needed: the exe's separator
      at 0x8BDA54 is " :  ", and nothing in the asm-vs-body comparison
      said so.
  `; <name>` instead of the string
      When the address is one our tree labels -- a `// 0xADDR` line above
      a global (`const SHORT CAIAction::NO_ACTION = 0;` and its ~1700
      siblings) -- the label wins: the source uses the name, not the
      literal. A `// 0xADDR` function marker likewise names an immediate
      that holds a function pointer.

Stdlib + pefile + capstone only (tools/parity/setup.sh). A missing exe,
missing exports (run tools/ghidra/ExportCallGraph.java once) or missing
pefile/capstone prints a SKIP line and exits 0, as the parity columns do.

Output is meant to be pasted into an evidence file or PR comment next to
the recovered body, so it stays plain text and self-describing: the first
line names the function and the end address the listing ran to.
"""
from __future__ import annotations

import argparse
import os
import re
import sys

try:
    import pefile
    import capstone
except ImportError:  # the SKIP contract, not a crash
    pefile = None
    capstone = None

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.dirname(TOOLS)
if HERE not in sys.path:
    sys.path.insert(0, HERE)
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)

import address_markers  # noqa: E402  (the `// 0xADDR` scan run.py uses)
import oracle  # noqa: E402  (the exe default)
import run as parity_run  # noqa: E402  (exports_dir, load_exports)

# A literal's alphabet: printable ASCII plus the three control characters
# C string literals legitimately carry.
STRING_CHARS = frozenset(bytes(range(0x20, 0x7F)) + b'\t\r\n')
STRING_MAX = 60
STRING_WINDOW = 4096  # NUL hunt past a long literal's first bytes

EXECUTABLE = 0x20000000  # IMAGE_SCN_MEM_EXECUTE: code bytes, no literals

# The declared name of a labelled global: the identifier (a `Class::Name`
# qualification included) that a `=`, `[`, `;` or `{` closes -- the first
# declarator of the line, which is the one the address belongs to.
GLOBAL_NAME_RE = re.compile(r"([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*(?:=|\[|;|\{)")


_SECTIONS_CACHE: dict = {}


def sections(exe):
    """{name: (lo, hi, bytes, executable)} -- the exe's sections as VAs.

    The way parity_offsets._secs reads them (pefile, fast_load, base +
    VirtualAddress .. + Misc_VirtualSize), plus each section's
    IMAGE_SCN_MEM_EXECUTE flag. Memoized per path: run.py's strings column
    reads the same exe once per row.
    """
    if exe in _SECTIONS_CACHE:
        return _SECTIONS_CACHE[exe]
    pe = pefile.PE(exe, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    out = {}
    for s in pe.sections:
        name = s.Name.rstrip(b'\x00').decode('latin1')
        out[name] = (base + s.VirtualAddress,
                     base + s.VirtualAddress + s.Misc_VirtualSize,
                     s.get_data(),
                     bool(s.Characteristics & EXECUTABLE))
    _SECTIONS_CACHE[exe] = out
    return out


def read_bytes(secs, va, n):
    """n bytes at va, or what the section still holds, or b''."""
    for lo, hi, data, _ in secs.values():
        if lo <= va < hi:
            return data[va - lo:va - lo + n]
    return b''


def string_at(secs, va):
    """The NUL-terminated printable string at va, else None.

    Data sections only: an executable section's printable run is code
    bytes, not a literal. `end <= 0` covers both an empty string (NUL
    first) and no NUL in reach -- neither is a text annotation.
    """
    for lo, hi, data, executable in secs.values():
        if executable or not lo <= va < hi:
            continue
        raw = data[va - lo:va - lo + STRING_WINDOW]
        end = raw.find(0)
        if end <= 0:
            return None
        text = raw[:end]
        if all(b in STRING_CHARS for b in text):
            return text.decode('latin1')
        return None
    return None


def inside_string(secs, va):
    """True when va lies strictly inside a longer string.

    Step C's rule 2: the byte before va is a printable string byte rather
    than NUL or a non-string byte, so a pointer to va reads a longer
    literal's tail ("C" out of "BSC") -- not a string of its own. A
    section start has no byte before it, and no data section holding va
    has no string around it: neither counts as inside.
    """
    for lo, hi, data, executable in secs.values():
        if executable or not lo <= va < hi:
            continue
        return va > lo and data[va - lo - 1] in STRING_CHARS
    return False


def quote(text):
    """Escaped for a C-style literal (the alphabet is already ASCII)."""
    for char, esc in (('\\', '\\\\'), ('"', '\\"'), ('\t', '\\t'),
                      ('\r', '\\r'), ('\n', '\\n')):
        text = text.replace(char, esc)
    return text


def clipped(text):
    """text, cut at STRING_MAX chars with a visible ... when cut."""
    if len(text) <= STRING_MAX:
        return text
    return text[:STRING_MAX] + '...'


def marker_names(source_root):
    """({address: symbol} functions, {address: name} globals) from src/.

    The functions are exactly address_markers.collect_markers' -- the scan
    run.py's gate uses, byte for byte. The globals are the tree's other
    `// 0xADDR` labels: a marker whose following code line declares
    something rather than defines a function. Both walks share the
    address_markers regexes and rejects, so a label lands on one side or
    the other, never both.
    """
    functions = {m['address']: m['symbol']
                 for m in address_markers.collect_markers(source_root)}
    globals_ = {}
    for root, dirs, files in os.walk(source_root):
        dirs[:] = sorted(d for d in dirs if d != '.git')
        for name in sorted(files):
            if not name.lower().endswith(address_markers.FILE_EXTENSIONS):
                continue
            path = os.path.join(root, name)
            with open(path, encoding='utf-8', errors='ignore') as f:
                lines = f.read().splitlines()
            for i, line in enumerate(lines):
                m = address_markers.ADDR_RE.match(line)
                if not m:
                    continue
                addr = int(m.group(1), 16)
                if not (address_markers.CODE_MIN <= addr
                        <= address_markers.CODE_MAX):
                    continue
                sig = address_markers._signature_line(lines, i)
                if sig is None or address_markers.QUAL_RE.search(sig) \
                        or address_markers.INLINE_RE.search(sig):
                    continue
                gm = GLOBAL_NAME_RE.search(sig)
                if gm:
                    globals_.setdefault(addr, gm.group(1))
    return functions, globals_


def operand_addrs(ins):
    """The data addresses an instruction's operands hold.

    Immediates and memory displacements. A direct call/jmp's immediate is
    control flow, not data (line_for names the target instead); an indirect
    one's displacement is a data address like any other.
    """
    if ins.mnemonic in ('call', 'jmp') and ins.operands and \
            ins.operands[0].type == capstone.x86.X86_OP_IMM:
        return []
    out = []
    for op in ins.operands:
        if op.type == capstone.x86.X86_OP_IMM:
            out.append(op.imm & 0xFFFFFFFF)
        elif op.type == capstone.x86.X86_OP_MEM:
            out.append(op.mem.disp & 0xFFFFFFFF)
    return out


def line_for(ins, secs, fn_names, labels, export_names):
    """One disassembly line: address, mnemonic, operands, annotations."""
    text = f'0x{ins.address:x}:\t{ins.mnemonic}\t{ins.op_str}'
    notes = []
    if ins.mnemonic in ('call', 'jmp') and ins.operands and \
            ins.operands[0].type == capstone.x86.X86_OP_IMM:
        target = ins.operands[0].imm & 0xFFFFFFFF
        notes.append(fn_names.get(target) or export_names.get(target) or '?')
    else:
        for cand in operand_addrs(ins):
            label = labels.get(cand)
            if label:
                notes.append(label)
                continue
            literal = string_at(secs, cand)
            if literal:
                notes.append('"%s"' % quote(clipped(literal)))
    for note in notes:
        text += f'  ; {note}'
    return text


def referenced_strings(address, end, exe, labels=None):
    """({va: literal}, {va: label}, {call target: pushed string vas}).

    One walk of [address, end) collecting the strings the body points at,
    narrowed by the two noise rules Step C measured in: only an
    immediate operand counts -- Step B's noise arrived as the
    displacement of a pointer load, which reads pointer-table bytes as
    text -- and an address strictly inside a longer string never does (a
    pointer to a literal's tail, "C" out of "BSC", is not a reference to
    "C"; the annotate listing still annotates displacements, this
    collection is the column's). `labelled` is which of those addresses
    our tree names, and `pushed` maps a direct call target to the string
    vas among the four immediate pushes before any call to it -- how a
    macro hides its own arguments: UTIL_ASSERT's caller pushes the file
    path and the expression text the source never writes. `labels` is the
    merged marker_names pair (run.py caches that scan across rows).
    """
    labels = labels or {}
    secs = sections(exe)
    strings, labelled, pushed = {}, {}, {}
    window = []  # the last four immediate pushes, in order
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    code = read_bytes(secs, address, end - address)
    for ins in md.disasm(code, address):
        if ins.address + ins.size > end:
            break
        direct = ins.mnemonic in ('call', 'jmp') and ins.operands and \
            ins.operands[0].type == capstone.x86.X86_OP_IMM
        for op in ins.operands:
            if op.type != capstone.x86.X86_OP_IMM:
                continue  # rule 1: a displacement read is not a literal use
            if direct and op is ins.operands[0]:
                continue  # the call/jmp target is control flow, not data
            cand = op.imm & 0xFFFFFFFF
            literal = string_at(secs, cand)
            if not literal or inside_string(secs, cand):
                continue  # rule 2: a pointer to a longer string's tail
            strings.setdefault(cand, literal)
            if cand in labels:
                labelled.setdefault(cand, labels[cand])
        if ins.mnemonic == 'push' and ins.operands and \
                ins.operands[0].type == capstone.x86.X86_OP_IMM:
            window.append(ins.operands[0].imm & 0xFFFFFFFF)
            del window[:-4]
        elif ins.mnemonic == 'call' and ins.operands and \
                ins.operands[0].type == capstone.x86.X86_OP_IMM:
            target = ins.operands[0].imm & 0xFFFFFFFF
            for va in window:
                if va in strings:
                    pushed.setdefault(target, set()).add(va)
    return strings, labelled, pushed


def annotate(address, end, exe, exports, source_root):
    """The annotated disassembly lines of [address, end).

    `exports` is run.load_exports' triple; fn_names beat export names for
    call/jmp targets, and labels (functions + globals) beat strings for
    data operands. A function whose bytes cross a section boundary stops
    there -- no exe function does.
    """
    fn_names, global_names = marker_names(source_root)
    labels = {**global_names, **fn_names}
    export_names = exports[0]
    secs = sections(exe)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    code = read_bytes(secs, address, end - address)
    lines = []
    for ins in md.disasm(code, address):
        if ins.address + ins.size > end:
            break
        lines.append(line_for(ins, secs, fn_names, labels, export_names))
    return lines


def skip(reason):
    """The SKIP contract: a line, exit 0 -- never a failure."""
    print(f'annotate: skipped ({reason})')
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__.splitlines()[0],
        epilog='Annotations: `; <name>` names a direct call/jmp target '
               '(source `// 0xADDR` marker, else functions.tsv, else ?) '
               'and a labelled data address; `; "text"` shows the string '
               'literal an immediate or displacement points at.')
    ap.add_argument('addr', help='the function\'s start, e.g. 0x7C3140')
    ap.add_argument('--end',
                    help='end address (default: functions.tsv\'s size)')
    ap.add_argument('--exe', help='the exe to read '
                    '(default: oracle.VANILLA_EXE, the path run.py uses)')
    args = ap.parse_args(argv)

    if pefile is None or capstone is None:
        return skip('pefile/capstone missing; tools/parity/setup.sh')
    address = int(args.addr, 16)
    exe = args.exe or oracle.VANILLA_EXE
    if not os.path.isfile(exe):
        return skip(f'no exe at {exe}; --exe overrides')
    exports = parity_run.load_exports()
    if exports is None:
        return skip('no functions.tsv/callgraph.tsv under '
                    f'{parity_run.exports_dir()}; run '
                    'tools/ghidra/ExportCallGraph.java')
    names, sizes, _ = exports
    end = int(args.end, 16) if args.end else address + sizes.get(address, 0)
    if end <= address:
        return skip(f'no size for 0x{address:X} in functions.tsv; '
                    '--end overrides')
    print(f'== 0x{address:X} {names.get(address) or "?"} (end 0x{end:X})')
    for line in annotate(address, end, exe, exports,
                         os.path.join(REPO, 'src')):
        print(line)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
