#!/usr/bin/env python3
r"""bg2_find -- class/member/method layout lookups from the BG2:EE PDB dumps.

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/bg2_find.py: the
record regexes, the two-pass type parsing (records, then continuations),
the forward-ref rule (a class commits only when its field list is a real
index and sizeof is non-zero), the primitive/pointer/modifier/array/
bitfield type resolution, the noise prefixes for his WebRTC baggage, the
SQLite schema and its NOCASE indexes are his, byte-for-byte where paths
are not involved.  Two rehosts, and one deliberate deviation from his
spellings:

* his build read the PDB via llvm-pdbutil or his extracted zip members;
  ours reads the dumps extracted under ``$IWD2R_HOME/wso/bg2/`` by
  tools/wso_fetch.py (UTF-16 text, BOM-sniffed).  The dumps are data:
  nothing here executes them, and the SQLite index treats their text as
  data.  Build once (~132 MB of text), then queries are instant.
* his CLI printed results; ours exposes ``lookup(query) -> list[dict]``
  (the CLI is a thin wrapper over it).
* the inline-type capture.  His spellings kept the inline type in a
  separate pattern, ``RE_TI_INLINE = (0x...) \(([^()]*)\)``, consulted
  as a second channel beside the type index (``ret_inline or
  resolve(ret_ti)``); ours folds it into each record regex as the shared
  ``_T`` group above, and RE_TI_INLINE is gone.  ``_T`` tolerates one
  level of nested parens where his ``[^()]*`` stops at the first inner
  one: a function pointer's inline spelling carries its own parameter
  list, and a flat capture would truncate such a line and drop the
  member.  (Measured over the real dumps: no line exercises the nesting
  today, so the tolerance is currently free.)

The engine is shared with IWD2: names carry over (roughly 70% in
practice), offsets and sizes DO NOT -- they are BG2's.  Every answer is
a hypothesis about IWD2's layout, to be checked against the real thing.

Queries: ``CGameSprite::`` (whole class), ``CGameSprite::Render``,
``CItem::`` for an enum view when the name is an enum, or a bare
``m_flags`` / ``GetHappiness`` / class name.

Usage::

    python3 tools/bg2_find.py --build
    python3 tools/bg2_find.py 'CGameSprite::'
    python3 tools/bg2_find.py 'CGameSprite::Render'
"""
from __future__ import annotations

import argparse
import io
import os
import re
import sqlite3
import sys

DB_NAME = "bg2_pdb.sqlite"
STREAM_FILES = {
    "types": "bg2_pdb_types.txt",
    "globals": "bg2_pdb_globals.txt",
    "modules": "bg2_pdb_symbols.txt",
}
MAX_ROWS = 400

CAVEAT = "[BG2EE 2.5 PDB -- offsets/sizes are BG2's, NOT IWD2's]"

# His noise prefixes: non-engine libraries in the PDB (WebRTC, breakpad,
# ATL, the CRT) whose thousands of classes only slow every scan down.
NOISE_PREFIXES = (
    "std::", "talk_base::", "buzz::", "cricket::", "rtc::", "webrtc::",
    "sigslot::", "testing::", "google::", "google_breakpad::", "boost::",
    "Concurrency::", "ATL::", "<lambda", "__vc_attributes", "`anonymous",
    "_", "$",
)

PRIMS = {
    0x0003: "void", 0x0008: "HRESULT", 0x0010: "signed char",
    0x0011: "short", 0x0012: "long", 0x0013: "__int64",
    0x0020: "unsigned char", 0x0021: "unsigned short",
    0x0022: "unsigned long", 0x0023: "unsigned __int64",
    0x0030: "bool", 0x0040: "float", 0x0041: "double",
    0x0042: "long double", 0x0070: "char", 0x0071: "wchar_t",
    0x0074: "int", 0x0075: "unsigned",
}

RE_RECORD = re.compile(r"^\s+(0x[0-9A-F]+) \| (LF_\w+)(?: \[size = \d+\])?(?: \x60(.*)\x60)?\s*$")
# An inline type spelling in parens can itself contain parens (a function
# pointer's parameter list), so the capture tolerates one level of nesting.
_T = r"(?: \(([^)]*(?:\([^()]*\)[^)]*)*)\))?"
RE_MEMBER = re.compile(r"- LF_MEMBER \[name = \x60(.*?)\x60, Type = (0x[0-9A-F]+)" + _T + r", offset = (\d+), attrs = ([^\]]+)\]")
RE_STMEMBER = re.compile(r"- LF_STMEMBER \[name = \x60(.*?)\x60, type = (0x[0-9A-F]+)" + _T + r", attrs = ([^\]]+)\]")
RE_ONEMETHOD = re.compile(r"- LF_ONEMETHOD \[name = \x60(.*)\x60\]\s*$")
RE_METHOD = re.compile(r"- LF_METHOD \[name = \x60(.*)\x60, # overloads = (\d+), overload list = (0x[0-9A-F]+)\]")
RE_ENUMERATE = re.compile(r"- LF_ENUMERATE \[(.+) = (-?\d+)\]")
RE_INDEX = re.compile(r"- LF_INDEX \[type = (0x[0-9A-F]+)\]")
RE_MLIST_ENTRY = re.compile(r"- Method \[type = (0x[0-9A-F]+), vftable offset = (-?\d+), attrs = ([^\]]+)\]")
RE_ARG_LINE = re.compile(r"^\s+(0x[0-9A-F]+)" + _T + r": \x60(.*)\x60\s*$")
RE_SYM = re.compile(r"^\s+\d+ \| (S_\w+) \[size = \d+\] \x60(.*)\x60\s*$")

RE_FIELDLIST = re.compile(r"field list: (0x[0-9A-F]+|<no type>)")
RE_OPTIONS = re.compile(r"options: (.*?), sizeof (\d+)")
RE_REFERENT = re.compile(r"referent = (0x[0-9A-F]+)")
RE_MODIFIER = re.compile(r"referent = (0x[0-9A-F]+)" + _T + r", modifiers = (.+)")
RE_ARRAY = re.compile(r"size: (\d+), index type: .*?, element type: (0x[0-9A-F]+)")
RE_BITFIELD = re.compile(r"type = (0x[0-9A-F]+)" + _T + r", bit offset = (\d+), # bits = (\d+)")
RE_RETURN_TYPE = re.compile(r"return type = (0x[0-9A-F]+)" + _T + r", # args = \d+, param list = (0x[0-9A-F]+)")
RE_CALLING_CONV = re.compile(r"calling conv = ([\w ]+?),")
RE_MFUNC_ONEMETHOD = re.compile(r"type = (0x[0-9A-F]+), vftable offset = (-?\d+), attrs = (.+)")
RE_MFUNC_BASE = re.compile(r"type = (0x[0-9A-F]+), offset = (\d+)")
RE_GDATA = re.compile(r"type = (0x[0-9A-F]+)" + _T + r", addr = (\d+:\d+)")
RE_CONSTANT = re.compile(r"type = (0x[0-9A-F]+)" + _T + r", value = (.+)")
RE_MODULE_REF = re.compile(r"module = (\d+)")
RE_MODULE_LINE = re.compile(r"Mod (\d+) \| \x60(.*?)\x60")

NAMED_KINDS = ("LF_CLASS", "LF_STRUCTURE", "LF_UNION", "LF_ENUM", "LF_INTERFACE")
LIST_KINDS = ("LF_FIELDLIST", "LF_METHODLIST", "LF_ARGLIST")


def is_noise(name: str) -> bool:
    return name.startswith(NOISE_PREFIXES)


def default_dumps() -> str:
    home = os.environ.get("IWD2R_HOME") or os.path.join(
        os.path.expanduser("~"), ".local", "share", "iwd2r")
    return os.path.join(home, "wso", "bg2")


def open_stream(dumps, stream):
    """A text handle on an extracted dump (BOM-sniffed: llvm-pdbutil
    writes UTF-16), or None when that stream was not extracted."""
    path = os.path.join(dumps, STREAM_FILES[stream])
    if not os.path.isfile(path):
        return None
    raw = open(path, "rb")
    try:
        bom = raw.read(2)
        raw.seek(0)
        enc = "utf-16" if bom in (b"\xff\xfe", b"\xfe\xff") else "utf-8-sig"
        return io.TextIOWrapper(raw, encoding=enc, errors="replace")
    except BaseException:
        raw.close()
        raise


class _Types:
    """Parsed state of the types stream, plus his resolve()/sig()."""

    def __init__(self):
        self.named = {}        # ti -> (kind, name)
        self.classes = {}      # name -> {kind, name, fl, fwd, size}
        self.enums = {}        # name -> fieldlist ti
        self.fieldlists = {}   # ti -> [entry, ...]
        self.methodlists = {}  # shared with fieldlists (keyed by ti)
        self.arglists = {}     # ti -> ["type", ...]
        self.ptrs = {}         # ti -> (referent ti, "*" | "&")
        self.mods = {}         # ti -> (referent ti, "const")
        self.arrays = {}       # ti -> (element ti, nbytes)
        self.bitfields = {}    # ti -> (base ti, bits, bit offset)
        self.mfuncs = {}       # ti -> [ret ti, ret inline, arglist ti, conv]

    def resolve(self, t, depth=0):
        """Type index -> a printable C type. Best effort."""
        if depth > 8:
            return hex(t)
        if t < 0x1000:
            if t in PRIMS:
                return PRIMS[t]
            if (t & 0xF00) in (0x400, 0x600) and (t & 0xFF) in PRIMS:
                return PRIMS[t & 0xFF] + "*"
            return hex(t)
        if t in self.named:
            return self.named[t][1]
        if t in self.ptrs:
            ref, suffix = self.ptrs[t]
            return self.resolve(ref, depth + 1) + suffix
        if t in self.mods:
            ref, mod = self.mods[t]
            return mod + " " + self.resolve(ref, depth + 1)
        if t in self.arrays:
            elem, nbytes = self.arrays[t]
            return f"{self.resolve(elem, depth + 1)}[{nbytes}b]"
        if t in self.bitfields:
            base, bits, _off = self.bitfields[t]
            return f"{self.resolve(base, depth + 1)}:{bits}"
        return hex(t)

    def sig(self, ft):
        """A method's type index -> '(args) -> ret [conv]'."""
        mf = self.mfuncs.get(ft)
        if not mf:
            return "(?)"
        ret_ti, ret_inline, arg_ti, conv = mf
        args = ", ".join(self.arglists.get(arg_ti, []))
        ret = ret_inline or self.resolve(ret_ti)
        s = f"({args})"
        if ret != "void":
            s += f" -> {ret}"
        if conv and conv not in ("thiscall", "cdecl"):
            s += f" [{conv}]"
        return s

    def fl_entries(self, fl):
        """A fieldlist's entries, following LF_INDEX continuation links."""
        out, seen = [], set()
        while fl and fl not in seen:
            seen.add(fl)
            entries = self.fieldlists.get(fl, [])
            out.extend(e for e in entries if e[0] != "chain")
            nxt = next((e[1] for e in entries if e[0] == "chain"), 0)
            fl = nxt
        return out


def parse_types(lines) -> _Types:
    """Two passes over the stream, as records then continuations: his
    state machine, kept."""
    st = _Types()
    cur_class = None
    cur_list = None
    cur_ti = None
    cur_kind = None
    pending = None

    for line in lines:
        m = RE_RECORD.match(line)
        if m:
            cur_ti = int(m.group(1), 16)
            cur_kind = m.group(2)
            name = m.group(3)
            pending = None
            cur_class = None
            cur_list = None
            if cur_kind in NAMED_KINDS and name:
                kind = "enum" if cur_kind == "LF_ENUM" else cur_kind[3:].lower()
                st.named[cur_ti] = (kind, name)
                cur_class = {"kind": kind, "name": name, "fl": 0,
                             "fwd": False, "size": 0}
            elif cur_kind == "LF_ARGLIST":
                cur_list = st.arglists.setdefault(cur_ti, [])
            elif cur_kind == "LF_METHODLIST":
                cur_list = st.methodlists.setdefault(cur_ti, [])
            elif cur_kind == "LF_FIELDLIST":
                cur_list = st.fieldlists.setdefault(cur_ti, [])
            continue

        # Class/enum continuation lines.
        if cur_class is not None:
            m = RE_FIELDLIST.search(line)
            if m:
                cur_class["fl"] = (0 if m.group(1) == "<no type>"
                                   else int(m.group(1), 16))
                continue
            m = RE_OPTIONS.search(line)
            if m:
                cur_class["fwd"] = "forward ref" in m.group(1)
                cur_class["size"] = int(m.group(2))
                if not cur_class["fwd"] and not is_noise(cur_class["name"]):
                    if cur_class["kind"] == "enum":
                        st.enums[cur_class["name"]] = cur_class["fl"]
                    else:
                        st.classes[cur_class["name"]] = dict(cur_class)
                cur_class = None
                continue
            if cur_class["kind"] == "enum" and "options:" in line:
                # An enum's options can carry no comma'd sizeof clause.
                if "forward ref" not in line:
                    st.enums[cur_class["name"]] = cur_class["fl"]
                cur_class = None
                continue
            cur_class = None if "options:" in line else cur_class
            continue

        # Scalar/pointer/procedure continuation lines.
        if cur_ti is not None and cur_kind == "LF_POINTER":
            m = RE_REFERENT.search(line)
            if m:
                ref = int(m.group(1), 16)
                # Only the mode decides; ' referent' in the raw line would
                # make any substring test for ' ref' a false positive.
                suffix = "&" if "mode = ref" in line else "*"
                st.ptrs[cur_ti] = (ref, suffix)
            continue
        if cur_ti is not None and cur_kind == "LF_MODIFIER":
            m = RE_MODIFIER.search(line)
            if m:
                st.mods[cur_ti] = (int(m.group(1), 16), m.group(3).strip())
            continue
        if cur_ti is not None and cur_kind == "LF_ARRAY":
            m = RE_ARRAY.search(line)
            if m:
                st.arrays[cur_ti] = (int(m.group(2), 16), int(m.group(1)))
            continue
        if cur_ti is not None and cur_kind == "LF_BITFIELD":
            m = RE_BITFIELD.search(line)
            if m:
                st.bitfields[cur_ti] = (int(m.group(1), 16),
                                        int(m.group(4)), int(m.group(3)))
            continue
        if cur_ti is not None and cur_kind in ("LF_MFUNCTION", "LF_PROCEDURE"):
            m = RE_RETURN_TYPE.search(line)
            if m:
                st.mfuncs[cur_ti] = [int(m.group(1), 16), m.group(2),
                                     int(m.group(3), 16), ""]
            elif "calling conv = " in line and cur_ti in st.mfuncs:
                mc = RE_CALLING_CONV.search(line)
                if mc:
                    st.mfuncs[cur_ti][3] = mc.group(1)
            continue

        if cur_list is None:
            continue

        # Fieldlist / methodlist / arglist entry lines.
        if cur_kind == "LF_ARGLIST":
            m = RE_ARG_LINE.match(line)
            if m:
                cur_list.append(m.group(3))
            continue
        if cur_kind == "LF_METHODLIST":
            m = RE_MLIST_ENTRY.search(line)
            if m:
                cur_list.append((int(m.group(1), 16), int(m.group(2)),
                                 m.group(3).strip()))
            continue

        m = RE_MFUNC_ONEMETHOD.search(line)
        if m and pending is not None:
            cur_list.append(("method", pending, int(m.group(1), 16),
                             int(m.group(2)), m.group(3).strip()))
            pending = None
            continue
        m = RE_MFUNC_BASE.search(line)
        if m and pending == "\x00base":
            cur_list.append(("base", int(m.group(1), 16), int(m.group(2))))
            pending = None
            continue
        pending = None
        m = RE_MEMBER.search(line)
        if m:
            cur_list.append(("member", m.group(1), int(m.group(2), 16),
                             m.group(3), int(m.group(4)), 0))
            continue
        m = RE_STMEMBER.search(line)
        if m:
            cur_list.append(("member", m.group(1), int(m.group(2), 16),
                             m.group(3), -1, 1))
            continue
        m = RE_ONEMETHOD.search(line)
        if m:
            pending = m.group(1)
            continue
        m = RE_METHOD.search(line)
        if m:
            cur_list.append(("overloads", m.group(1), int(m.group(3), 16)))
            continue
        m = RE_ENUMERATE.search(line)
        if m:
            cur_list.append(("enumval", m.group(1), int(m.group(2))))
            continue
        m = RE_INDEX.search(line)
        if m:
            cur_list.append(("chain", int(m.group(1), 16)))
            continue
        if "- LF_BCLASS" in line:
            pending = "\x00base"
    return st


def parse_modules(lines):
    """Symbols stream -> {module id: module name}, for S_PROCREF joins."""
    modules = {}
    for line in lines:
        m = RE_MODULE_LINE.search(line)
        if m:
            stem = os.path.splitext(m.group(2))[0]
            modules[int(m.group(1))] = stem
    return modules


def parse_globals(lines, modules, st):
    """Globals stream -> (funcs [(name, module)], data/consts [(name, type,
    addr-or-value, kind)]).  Two-line records: the symbol line names it,
    the indented line that follows types it."""
    funcs, globs = [], []
    pending = None
    for line in lines:
        m = RE_SYM.match(line)
        if m:
            sym, name = m.group(1), m.group(2)
            if sym in ("S_PROCREF", "S_LPROCREF"):
                pending = ("proc", name)
            elif sym in ("S_GDATA32", "S_LDATA32"):
                pending = ("data", name)
            elif sym == "S_CONSTANT":
                pending = ("const", name)
            else:
                pending = None
            continue
        if pending is None:
            continue
        if pending[0] == "proc":
            mm = RE_MODULE_REF.search(line)
            if mm:
                funcs.append((pending[1], modules.get(int(mm.group(1)), "")))
                pending = None
            continue
        if pending[0] == "data":
            mm = RE_GDATA.search(line)
            if mm:
                ty = mm.group(2) or st.resolve(int(mm.group(1), 16))
                globs.append((pending[1], ty, mm.group(3), "data"))
                pending = None
            continue
        mm = RE_CONSTANT.search(line)
        if mm:
            ty = mm.group(2) or st.resolve(int(mm.group(1), 16))
            globs.append((pending[1], ty, mm.group(3).strip(), "const"))
            pending = None
    return funcs, globs


def _emit(st):
    """Parsed types -> flat rows for the database."""
    members, methods, bases, classrows, enumvals = [], [], [], [], []
    for cname, c in st.classes.items():
        nm = nf = 0
        for e in st.fl_entries(c["fl"]):
            tag = e[0]
            if tag == "member":
                _, mname, mti, minline, off, static = e
                nf += 1
                members.append((cname, mname, minline or st.resolve(mti),
                                off, static))
            elif tag == "method":
                _, mname, fti, vft, attrs = e
                nm += 1
                methods.append((cname, mname, st.sig(fti), attrs, vft))
            elif tag == "overloads":
                _, mname, mlist = e
                entries = st.methodlists.get(mlist, [])
                nm += len(entries)
                for fti, vft, attrs in entries:
                    methods.append((cname, mname, st.sig(fti), attrs, vft))
            elif tag == "base":
                bases.append((cname, st.resolve(e[1]), e[2]))
        classrows.append((cname, c["kind"], c["size"], nm, nf))
    for ename, fl in st.enums.items():
        for e in st.fl_entries(fl):
            if e[0] == "enumval":
                enumvals.append((ename, e[1], e[2]))
    return members, methods, bases, classrows, enumvals


SCHEMA = """
CREATE TABLE classes(name TEXT, kind TEXT, size INT, nmembers INT, nmethods INT);
CREATE TABLE bases(class TEXT, base TEXT, off INT);
CREATE TABLE members(class TEXT, name TEXT, type TEXT, off INT, static INT);
CREATE TABLE methods(class TEXT, name TEXT, sig TEXT, attrs TEXT, vft INT);
CREATE TABLE enumvals(enum TEXT, name TEXT, value INT);
CREATE TABLE funcs(name TEXT, module TEXT);
CREATE TABLE globals(name TEXT, type TEXT, addr TEXT, kind TEXT);
CREATE INDEX i_classes ON classes(name COLLATE NOCASE);
CREATE INDEX i_members ON members(name COLLATE NOCASE);
CREATE INDEX i_members_class ON members(class COLLATE NOCASE);
CREATE INDEX i_methods ON methods(name COLLATE NOCASE);
CREATE INDEX i_methods_class ON methods(class COLLATE NOCASE);
CREATE INDEX i_enumvals ON enumvals(name COLLATE NOCASE);
CREATE INDEX i_funcs ON funcs(name COLLATE NOCASE);
CREATE INDEX i_globals ON globals(name COLLATE NOCASE);
"""


def build_db(dumps=None, db=None, log=None) -> str:
    """Parse the extracted dumps once into a SQLite index (atomically
    replaced); returns the db path.  The dump text is data: nothing is
    executed, and parsing never writes outside the db file."""
    dumps = dumps or default_dumps()
    if db is None:
        db = os.path.join(dumps, DB_NAME)

    stream = open_stream(dumps, "types")
    if stream is None:
        raise FileNotFoundError(
            f"no {STREAM_FILES['types']} under {dumps} (run "
            "tools/wso_fetch.py to fetch and extract the BG2 dumps)")
    with stream:
        st = parse_types(stream)
    if log:
        log(f"types: {len(st.classes)} classes, {len(st.enums)} enums")

    modules = {}
    stream = open_stream(dumps, "modules")
    if stream is not None:
        with stream:
            modules = parse_modules(stream)

    funcs, globs = [], []
    stream = open_stream(dumps, "globals")
    if stream is not None:
        with stream:
            funcs, globs = parse_globals(stream, modules, st)
    if log:
        log(f"globals: {len(funcs)} functions, {len(globs)} data/consts")

    members, methods, bases, classrows, enumvals = _emit(st)

    tmp = f"{db}.part"
    os.makedirs(os.path.dirname(os.path.abspath(db)), exist_ok=True)
    con = sqlite3.connect(tmp)
    try:
        con.executescript(SCHEMA)
        con.executemany("INSERT INTO classes VALUES (?,?,?,?,?)", classrows)
        con.executemany("INSERT INTO bases VALUES (?,?,?)", bases)
        con.executemany("INSERT INTO members VALUES (?,?,?,?,?)", members)
        con.executemany("INSERT INTO methods VALUES (?,?,?,?,?)", methods)
        con.executemany("INSERT INTO enumvals VALUES (?,?,?)", enumvals)
        con.executemany("INSERT INTO funcs VALUES (?,?)", set(funcs))
        con.executemany("INSERT INTO globals VALUES (?,?,?,?)", set(globs))
        con.commit()
    finally:
        con.close()
    os.replace(tmp, db)
    return str(db)


def _class_view(con, cname):
    row = con.execute(
        "SELECT kind, size FROM classes WHERE name = ? COLLATE NOCASE",
        (cname,)).fetchone()
    if row is None:
        hints = [r[0] for r in con.execute(
            "SELECT name FROM classes WHERE name LIKE ? COLLATE NOCASE "
            "ORDER BY length(name) LIMIT 10", (f"%{cname}%",))]
        if hints:
            return [{"kind": "hint",
                     "note": f"no class {cname!r} in the BG2 PDB",
                     "suggestions": hints}]
        return []
    rows = [{"kind": "class", "name": cname, "type": row[0], "size": row[1]}]
    rows.extend({"kind": "base", "class": cname, "name": base, "offset": off}
                for base, off in con.execute(
                    "SELECT base, off FROM bases WHERE class = ? "
                    "COLLATE NOCASE ORDER BY off", (cname,)))
    rows.extend({"kind": "member", "class": cname, "name": nm, "type": ty,
                 "offset": off, "static": bool(static)}
                for nm, ty, off, static in con.execute(
                    "SELECT name, type, off, static FROM members "
                    "WHERE class = ? COLLATE NOCASE ORDER BY static, off",
                    (cname,)))
    rows.extend({"kind": "method", "class": cname, "name": nm, "type": sig,
                 "vtable": vft, "attrs": attrs}
                for nm, sig, attrs, vft in con.execute(
                    "SELECT name, sig, attrs, vft FROM methods "
                    "WHERE class = ? COLLATE NOCASE "
                    "ORDER BY (vft < 0), vft, name", (cname,)))
    return rows


def _enum_view(con, ename):
    return [{"kind": "enumval", "enum": ename, "name": nm, "value": v}
            for nm, v in con.execute(
                "SELECT name, value FROM enumvals WHERE enum = ? "
                "COLLATE NOCASE ORDER BY value, name", (ename,))]


def _scoped(con, query):
    cls, name = query.rsplit("::", 1)
    rows = [{"kind": "method", "class": c, "name": nm, "type": sig,
             "vtable": vft, "attrs": attrs}
            for c, nm, sig, attrs, vft in con.execute(
                "SELECT class, name, sig, attrs, vft FROM methods "
                "WHERE class = ? COLLATE NOCASE AND name = ? COLLATE NOCASE",
                (cls, name))]
    rows.extend({"kind": "member", "class": c, "name": nm, "type": ty,
                 "offset": off, "static": bool(static)}
                for c, nm, ty, off, static in con.execute(
                    "SELECT class, name, type, off, static FROM members "
                    "WHERE class = ? COLLATE NOCASE AND name = ? "
                    "COLLATE NOCASE", (cls, name)))
    return rows


def _bare(con, query):
    rows = [{"kind": "class", "name": query, "type": kind, "size": size}
            for kind, size in con.execute(
                "SELECT kind, size FROM classes WHERE name = ? COLLATE NOCASE",
                (query,))]
    rows.extend({"kind": "member", "class": c, "name": nm, "type": ty,
                 "offset": off, "static": bool(static)}
                for c, nm, ty, off, static in con.execute(
                    "SELECT class, name, type, off, static FROM members "
                    "WHERE name = ? COLLATE NOCASE ORDER BY class", (query,)))
    rows.extend({"kind": "method", "class": c, "name": nm, "type": sig,
                 "vtable": vft, "attrs": attrs}
                for c, nm, sig, attrs, vft in con.execute(
                    "SELECT class, name, sig, attrs, vft FROM methods "
                    "WHERE name = ? COLLATE NOCASE ORDER BY class", (query,)))
    rows.extend({"kind": "fn", "name": nm, "module": module}
                for nm, module in con.execute(
                    "SELECT name, module FROM funcs "
                    "WHERE name = ? COLLATE NOCASE LIMIT 20", (query,)))
    rows.extend({"kind": "global", "name": nm, "type": ty, "addr": addr,
                 "gkind": gk}
                for nm, ty, addr, gk in con.execute(
                    "SELECT name, type, addr, kind FROM globals "
                    "WHERE name = ? COLLATE NOCASE LIMIT 20", (query,)))
    rows.extend({"kind": "enumval", "enum": en, "name": nm, "value": v}
                for en, nm, v in con.execute(
                    "SELECT enum, name, value FROM enumvals "
                    "WHERE name = ? COLLATE NOCASE ORDER BY value", (query,)))
    return rows


def lookup(query: str, db=None, dumps=None) -> list:
    """Query the BG2 layout index; builds it once if missing.  Returns
    row dicts (see the module docstring); [] when nothing is known or
    the dumps are absent."""
    q = str(query or "").strip()
    if not q:
        return []
    dumps = dumps or default_dumps()
    if db is None:
        db = os.path.join(dumps, DB_NAME)
    if not os.path.isfile(db):
        try:
            build_db(dumps=dumps, db=db)
        except (OSError, sqlite3.Error):
            return []
    if not os.path.isfile(db):
        return []
    con = sqlite3.connect(f"file:{os.path.abspath(db)}?mode=ro", uri=True)
    try:
        if q.endswith("::"):
            rows = _enum_view(con, q[:-2])
            return (rows or _class_view(con, q[:-2]))[:MAX_ROWS]
        if "::" in q:
            return _scoped(con, q)[:MAX_ROWS]
        return _bare(con, q)[:MAX_ROWS]
    finally:
        con.close()


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("query", nargs="?", help="'Class::', 'Class::Method', name")
    ap.add_argument("--build", action="store_true",
                    help="(re)build the SQLite index from the dumps")
    ap.add_argument("--db", help="index path (default <dumps>/bg2_pdb.sqlite)")
    ap.add_argument("--dumps", help="dump dir (default $IWD2R_HOME/wso/bg2)")
    args = ap.parse_args(argv)
    if args.build:
        db = build_db(dumps=args.dumps, db=args.db, log=lambda m: print(m))
        print(f"bg2_find: index at {db}  {CAVEAT}")
        return 0
    if not args.query:
        ap.error("a query is required unless --build")
    rows = lookup(args.query, db=args.db, dumps=args.dumps)
    if not rows:
        print(f"bg2_find: nothing for {args.query!r}  {CAVEAT}")
        return 1
    for r in rows:
        if r["kind"] == "hint":
            print(f"? {r['note']}; similar: {', '.join(r['suggestions'])}")
        elif r["kind"] == "class":
            print(f"class {r['name']}  ({r['type']}, sizeof {r['size']})")
        elif r["kind"] == "base":
            print(f"  : {r['name']}  @ {r['offset']}")
        elif r["kind"] == "member":
            static = "static " if r["static"] else ""
            print(f"  @ {r['offset']:<5} {static}{r['type']} {r['name']}"
                  f"  (class {r['class']})")
        elif r["kind"] == "method":
            slot = f"vft+{r['vtable']}" if r["vtable"] >= 0 else "nonvirtual"
            print(f"  {r['name']}{r['type']}  [{slot}] {r['attrs']}"
                  f"  (class {r['class']})")
        elif r["kind"] == "enumval":
            print(f"  {r['name']} = {r['value']}  (enum {r['enum']})")
        elif r["kind"] == "fn":
            print(f"fn {r['name']}  (module {r['module'] or '?'})")
        else:
            print(f"global {r['name']}  {r['type']} @ {r['addr']} ({r['gkind']})")
    print(CAVEAT)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        # The reader closed the pipe (`... | head`); that is the reader's
        # right, not a failure.  Point stdout at devnull so the shutdown
        # flush cannot raise a second time.
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        sys.exit(0)
