#!/usr/bin/env python3
"""Guardrail: flag source classes whose ctors install DIFFERENT binary vtables.

The blind spot that cost a session on the Fireball green-flames bug: our recovery
modelled TWO distinct binary classes (vtable 0x84eeb8 spell-hit / 0x85ac48 SPFLAMES)
as ONE CGameAnimationTypeEffect, and gave it the wrong virtual Render (0x6A36A0, the
0x85ac48 render) -- the spell-hit anim's real render is 0x55d950 (vtable 0x84eeb8 slot).
vtable_audit.py could not catch it: it anchors a class to ONE vtable, so it never saw
the second.

This check is the complement: for each class, disassemble EVERY recovered ctor and read
the vtable it installs (`mov dword ptr [reg], <vtable>` near the top, the standard MSVC
this->vftable = &Class::`vftable'). If a class's ctors install more than one distinct
vtable, the source class CONFLATES multiple binary classes -> split them, and verify each
split's virtuals against ITS vtable (sym.py vtable / the slot, NOT the Ghidra symbol name).

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/ctor_vtable_check.py,
byte-for-byte except the exe default (the fork's vendored .bin/iwd2.exe is
this repo's oracle.VANILLA_EXE; IWD2_EXE still overrides), the exe/src_dir
parameters on load_pe/find_ctors (tools/parity/run.py audits another
checkout), and check(), the per-function entry run.py drives.

Usage:
  python3 tools/parity/ctor_vtable_check.py                 # all classes
  python3 tools/parity/ctor_vtable_check.py CGameAnimationTypeEffect
  python3 tools/parity/ctor_vtable_check.py --quiet         # only conflations
"""
import os
import re
import sys
import glob
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.dirname(TOOLS)
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import oracle  # noqa: E402
# Default to the CANONICAL pristine IWD2.exe (md5 25cb3d8a) vendored at REPO/.bin/,
# NOT the host's IWD2EE-modded install whose .data/.text bytes differ. IWD2_EXE overrides.
EXE = os.environ.get("IWD2_EXE", oracle.VANILLA_EXE)
SRC = os.path.join(REPO, "src")

# The recovered-code address MARKER: a comment line that STARTS with the address
# (`// 0xADDR ...`). Anchored so prose that merely mentions an address ("the 0x55D3A0
# twin") is NOT mistaken for the marker.
ADDR_RE = re.compile(r"^\s*//\s*(0x[0-9A-Fa-f]+)\b")
COMMENTISH = re.compile(r"^\s*(//|/\*|\*|$)")   # comment or blank line
CTOR_RE = re.compile(r"\b(\w+)::(\w+)\s*\(")


def load_pe(exe=None):
    import pefile
    pe = pefile.PE(exe or EXE, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    secs = {}
    for s in pe.sections:
        name = s.Name.rstrip(b"\x00").decode("latin1")
        secs[name] = (base + s.VirtualAddress,
                      base + s.VirtualAddress + s.Misc_VirtualSize,
                      s.get_data())
    return pe, base, secs


def read(secs, va, n):
    for lo, hi, data in secs.values():
        if lo <= va < hi:
            off = va - lo
            return data[off:off + n]
    return b""


def is_vtable(secs, va):
    """A vtable: lives in .rdata/.data and its first dword is a .text code pointer."""
    rd = secs.get(".rdata")
    da = secs.get(".data")
    in_ro = (rd and rd[0] <= va < rd[1]) or (da and da[0] <= va < da[1])
    if not in_ro:
        return False
    first = read(secs, va, 4)
    if len(first) != 4:
        return False
    ptr = struct.unpack("<I", first)[0]
    tx = secs.get(".text")
    return bool(tx and tx[0] <= ptr < tx[1])


def find_ctors(src_dir=None):
    """class -> {addr: 'file:line'} for every recovered ctor (Name::Name with a // 0xADDR)."""
    src = src_dir or SRC
    out = {}
    for cpp in glob.glob(os.path.join(src, "*.cpp")):
        lines = open(cpp, encoding="utf-8", errors="replace").read().splitlines()
        for i, line in enumerate(lines):
            m = CTOR_RE.search(line)
            if not m or m.group(1) != m.group(2):
                continue                      # not a ctor (Class::Class)
            # walk back through THIS function's comment block (contiguous comment/blank
            # lines above the signature) to its // 0xADDR marker; stop at the first code
            # line (= previous function) so we never grab the wrong function's address.
            addr = None
            for j in range(i - 1, max(-1, i - 26), -1):
                a = ADDR_RE.match(lines[j])
                if a:
                    addr = int(a.group(1), 16)
                    break
                if not COMMENTISH.match(lines[j]):
                    break
            if addr is None:
                continue
            out.setdefault(m.group(1), {})[addr] = "%s:%d" % (os.path.basename(cpp), i + 1)
    return out


def installed_vtable(secs, ctor_va, window=0x600):
    """Disassemble the ctor; return the first vtable it stores into [reg] (this->vftable)."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    code = read(secs, ctor_va, window)
    for ins in md.disasm(code, ctor_va):
        if ins.mnemonic == "ret":
            break
        if ins.mnemonic != "mov" or len(ins.operands) != 2:
            continue
        dst, src = ins.operands
        if dst.type != capstone.x86.X86_OP_MEM or src.type != capstone.x86.X86_OP_IMM:
            continue
        # this->vftable = &vtable : store to [reg] or [reg+0], no index
        if dst.mem.index != 0 or dst.mem.disp != 0:
            continue
        if is_vtable(secs, src.imm & 0xffffffff):
            return src.imm & 0xffffffff
    return None


# ---- tools/parity/run.py entry ---------------------------------------------
# Verdict vocabulary must match tools/parity/run.py's exact strings.
GREEN, YELLOW, RED, SKIP = 'GREEN', 'YELLOW', 'RED', 'SKIP'
# load_pe() maps every section, find_ctors() re-reads all of src/*.cpp -- both
# far too slow per row, so cache per exe / src dir for the length of one run.
_SECS = {}
_CTORS = {}


def check(address, name, repo, exports, exe):
    """run.py's ctor-conflation column for this one function's class.

    Returns (verdict, notes) in run.py's vocabulary: RED when the class's
    recovered ctors install more than one distinct vtable (two binary classes
    merged), YELLOW when a ctor's installed vtable could not be read, GREEN on
    a single shared vtable, SKIP when the name is not a class method or the
    class has no recovered ctors.
    """
    cls = name.split('::')[0] if '::' in name else ''
    if not cls or cls == name:
        return SKIP, [f'{name} is not a class method']
    src = os.path.join(repo, 'src')
    if exe not in _SECS:
        _SECS[exe] = load_pe(exe)[2]
    if src not in _CTORS:
        _CTORS[src] = find_ctors(src)
    ctors = _CTORS[src].get(cls)
    if not ctors:
        return SKIP, [f'{cls} has no recovered ctors (Name::Name with a // 0xADDR) in src/*.cpp']
    secs = _SECS[exe]
    vts = {}                              # vtable -> [src locs]
    unresolved = []
    for addr, loc in sorted(ctors.items()):
        vt = installed_vtable(secs, addr)
        if vt is None:
            unresolved.append((addr, loc))
        else:
            vts.setdefault(vt, []).append((addr, loc))
    notes = [f'{cls}: {len(ctors)} ctor(s), {len(vts)} distinct installed vtable(s)']
    notes += [f'vtable 0x{vt:x} <- ctor 0x{addr:x} ({loc})'
              for vt, locs in sorted(vts.items()) for addr, loc in locs]
    notes += [f'vtable <unresolved> <- ctor 0x{addr:x} ({loc})' for addr, loc in unresolved]
    if len(vts) > 1:
        notes.append('likely two binary classes merged -- split them and verify '
                     "each split's virtuals against ITS vtable")
        return RED, notes
    if unresolved:
        return YELLOW, notes
    return GREEN, notes


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    quiet = "--quiet" in sys.argv
    only = set(args)

    pe, base, secs = load_pe()
    ctors = find_ctors()

    conflations = 0
    for cls in sorted(ctors):
        if only and cls not in only:
            continue
        vts = {}                              # vtable -> [src locs]
        unresolved = []
        for addr, loc in sorted(ctors[cls].items()):
            vt = installed_vtable(secs, addr)
            if vt is None:
                unresolved.append((addr, loc))
            else:
                vts.setdefault(vt, []).append((addr, loc))
        if len(vts) <= 1:
            if not quiet and (only or len(ctors[cls]) > 1):
                tag = ("0x%x" % next(iter(vts))) if vts else "none-found"
                print("[ok       ] %-44s %d ctor(s) -> %s" % (cls, len(ctors[cls]), tag))
            continue
        conflations += 1
        print("[CONFLATION] %s : %d ctors install %d DIFFERENT vtables -- likely two binary classes merged:" % (
            cls, len(ctors[cls]), len(vts)))
        for vt, locs in sorted(vts.items()):
            for addr, loc in locs:
                print("    vtable 0x%-7x  ctor 0x%-7x  (%s)" % (vt, addr, loc))
        for addr, loc in unresolved:
            print("    vtable <none>      ctor 0x%-7x  (%s)" % (addr, loc))
        print("    -> split the class; verify each split's virtuals against ITS vtable slot,")
        print("       not the Ghidra symbol name (sym.py vtable / readptr [vtableBase+slot]).")

    if not only:
        print("\n%d class(es) with ctors scanned; %d conflation(s)." % (len(ctors), conflations))
    sys.exit(1 if conflations else 0)


if __name__ == "__main__":
    main()
