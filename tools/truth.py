"""Decide open wave rows by measurement: a purity check, oracle case drafts, and
a reader for the oracle's report.

A function that is pure (arithmetic on its arguments only) can be decided by
calling the original exe's function and ours over swept inputs and comparing
the results. is_pure reads the evidence file's Decompiled (Ghidra) section;
draft_case and write_cases turn a pure function into an oracle case in the
shape of oracle/cases/rand.cpp; probe_verdicts reads what `oracle.py run`
reported for those cases. Nothing here runs the oracle, the game or any exe.

Some rows are decided by tracing instead: hook_table builds one Frida hook table
per target from the rows' signatures and our build's .map, frida_run drives
tools/frida/probe.py (its subprocess injectable; nothing here launches a game
on its own), and frida_verdicts compares the two traces with trace_diff.

Python 3 standard library only; run under `python3 -I`.
"""
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES_DIR = os.path.join(ROOT, 'oracle', 'cases')
FRIDA_DIR = os.path.join(ROOT, 'tools', 'frida')
sys.path.insert(0, FRIDA_DIR)
import trace_diff  # noqa: E402

SECTION = '## Decompiled (Ghidra)'

KEYWORDS = frozenset({
    'return', 'sizeof', 'if', 'while', 'for', 'switch', 'else', 'do', 'case',
    'default', 'break', 'continue', 'goto', 'const', 'static', 'inline',
    'volatile', 'register', 'signed', 'unsigned', 'true', 'false', 'NULL',
    'typedef'})
QUALIFIERS = frozenset({'const', 'static', 'volatile', 'register', 'signed', 'unsigned'})
# C and Ghidra type names (a cast or a local's declaration).
TYPE_NAMES = frozenset({
    'void', 'bool', 'char', 'short', 'int', 'long', 'float', 'double',
    'uchar', 'ushort', 'uint', 'ulong', 'longlong', 'ulonglong', 'byte',
    'sbyte', 'word', 'dword', 'qword', 'size_t', 'intptr_t', 'uintptr_t',
    'wchar_t', 'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t',
    'uint16_t', 'uint32_t', 'uint64_t',
    # Win32 integer typedefs (no HANDLE or other pointer types).
    'DWORD', 'BOOL', 'UINT', 'WORD', 'BYTE', 'LONG', 'ULONG', 'SHORT',
    'USHORT', 'CHAR', 'UCHAR', 'INT'})
TYPE_PATTERN = re.compile(r'^(?:(?:u?int|undefined|float|code)\d*)$')
# Ghidra pseudo-operations: pure arithmetic helpers, not calls.
PSEUDO_OPS = re.compile(
    r'^(?:CONCAT|SUB|ZEXT|SEXT|SBORROW|SCARRY|CARRY|PIECE|MULT)\d*$'
    r'|^(?:POPCOUNT|LZCOUNT|abs|ROL\d*|ROR\d*|ABS|FLOAT2INT|INT2FLOAT|NAN'
    r'|TRUNC|CEIL|FLOOR|ROUND|SQRT)$')
IDENT = re.compile(r'[A-Za-z_]\w*(?:::[A-Za-z_~]\w*)*')
NUMBER = re.compile(r'\b\d[\w.]*')


def _is_type(word):
    return word in TYPE_NAMES or bool(TYPE_PATTERN.match(word))


def _section_code(text):
    """The body of the Decompiled (Ghidra) section as bare code, or None."""
    start = text.find(SECTION)
    if start < 0:
        return None
    rest = text[start + len(SECTION):]
    nxt = re.search(r'^## ', rest, re.M)
    if nxt:
        rest = rest[:nxt.start()]
    lines = [ln for ln in rest.split('\n') if not ln.strip().startswith('```')]
    return '\n'.join(lines)


def _is_cast(inner):
    """Whether the text between parentheses is a bare type (a cast)."""
    words = re.findall(r'[A-Za-z_]\w*|\*', inner)
    return bool(words) and all(w == '*' or w in KEYWORDS or _is_type(w) for w in words) \
        and any(w != '*' for w in words)


def _is_deref(line):
    """Whether the line has a unary `*` (a pointer read or write), including
    one right after a (type) cast."""
    for i, ch in enumerate(line):
        if ch != '*':
            continue
        before = line[:i].rstrip()
        if not before:
            return True
        last = before[-1]
        if last == ')':
            depth, j = 0, len(before) - 1
            while j >= 0:
                depth += (before[j] == ')') - (before[j] == '(')
                if depth == 0:
                    break
                j -= 1
            if j >= 0 and re.search(r'\bsizeof\s*$', before[:j]):
                continue  # sizeof(T) * x is a multiplication, not a cast and a read
            if j >= 0 and _is_cast(before[j + 1:-1]):
                return True
            continue
        if last == ']' or last.isalnum() or last == '_':
            word = re.search(r'[A-Za-z_]\w*$', before)
            if word and word.group(0) in ('return', 'else', 'case', 'do'):
                return True
            continue
        return True
    return False


def _signature_params(signature):
    names = set()
    start, end = signature.find('('), signature.rfind(')')
    if start < 0 or end < start:
        return names
    for part in signature[start + 1:end].split(','):
        words = re.findall(r'[A-Za-z_]\w*', part)
        if words and words != ['void']:
            names.add(words[-1])
    return names


def _body_locals(body):
    """Names declared in the body: statements that start with type words."""
    names = set()
    for stmt in re.split(r'[;{}]', body):
        stmt = stmt.strip()
        if not stmt or not (stmt[0].isalpha() or stmt[0] == '_'):
            continue  # a cast or expression, not a declaration
        if '(' in stmt:
            continue  # a call or cast in it: its commas and names are not declarators
        words = re.findall(r'[A-Za-z_]\w*|\*|,|=', stmt)
        i = 0
        while i < len(words) and (words[i] in QUALIFIERS or _is_type(words[i])):
            i += 1
        if i == 0 or i >= len(words) or all(w in QUALIFIERS for w in words[:i]) \
                and not any(w in ('signed', 'unsigned') for w in words[:i]):
            continue
        for piece in re.split(r',', ' '.join(words[i:])):
            piece = piece.split('=', 1)[0]  # an initializer's names are not declared
            m = re.match(r'^[\s*]*([A-Za-z_]\w*)', piece)
            if m:
                names.add(m.group(1))
    return names


def _offence(line, allowed, pure_names):
    """The first reason this body line is impure, or None."""
    if re.fullmatch(r'LAB_\w+:', line):
        return None
    line = re.sub(r'\bgoto\s+LAB_\w+\s*;', ';', line)
    m = re.search(r'DAT_[0-9a-fA-F]+', line)
    if m:
        return 'global %s' % m.group(0)
    m = re.search(r'\b(?:in|unaff|extraout)_\w+', line)
    if m:
        return 'register state %s' % m.group(0)
    if re.search(r'\bthis\b', line):
        return 'this member access'
    if '->' in line:
        return 'member access through ->'
    if _is_deref(line):
        return 'pointer dereference'
    if re.search(r'[\w)]\s*\[', line):
        return 'indexed memory access'
    text = NUMBER.sub(' ', line)
    for m in IDENT.finditer(text):
        name = m.group(0)
        if name in allowed or name in KEYWORDS or name in pure_names \
                or _is_type(name) or PSEUDO_OPS.match(name):
            continue
        if re.match(r'\s*\(', text[m.end():]):
            return 'call to %s' % name
        return 'identifier %s is not a parameter, local or known type' % name
    return None


def is_pure(evidence_text, pure_names=frozenset()):
    """(pure, reason) for an evidence file's decompiled function.

    An allowlist: every identifier in the body must be a parameter from the
    signature, a local declared in the body, a C keyword or type name
    (Ghidra's included), a Ghidra pseudo-op (CONCAT*, SUB*, ZEXT*, ...), or in
    pure_names. Anything else (a global, a class static, a call) is impure,
    as is any `*` dereference (also after a cast), `->`, this, and in_/
    unaff_/extraout_ register state. The signature is not read as a call. The
    reason names the first offending line."""
    code = _section_code(evidence_text)
    if code is None:
        return False, 'no Decompiled (Ghidra) section'
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    code = re.sub(r'//[^\n]*', '', code)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    brace = code.find('{')
    if brace < 0:
        return False, 'no function body in the Decompiled (Ghidra) section'
    body = code[brace + 1:]
    allowed = _signature_params(code[:brace]) | _body_locals(body)
    for line in body.split('\n'):
        stripped = line.strip()
        if not stripped:
            continue
        why = _offence(stripped, allowed, pure_names)
        if why:
            return False, '%s in: %s' % (why, stripped)
    return True, 'arithmetic on its arguments only'


# ---- case drafts ------------------------------------------------------------

# Ghidra and C spellings of integer types -> (C++ spelling, bits, signed).
INT_TYPES = {
    'int': ('int', 32, True), 'uint': ('unsigned int', 32, False),
    'unsigned int': ('unsigned int', 32, False), 'long': ('long', 32, True),
    'ulong': ('unsigned long', 32, False), 'unsigned long': ('unsigned long', 32, False),
    'short': ('short', 16, True), 'ushort': ('unsigned short', 16, False),
    'unsigned short': ('unsigned short', 16, False),
    'char': ('char', 8, True), 'uchar': ('unsigned char', 8, False),
    'unsigned char': ('unsigned char', 8, False), 'byte': ('unsigned char', 8, False),
    'bool': ('bool', 8, False), 'undefined4': ('unsigned int', 32, False),
    'undefined2': ('unsigned short', 16, False), 'undefined1': ('unsigned char', 8, False),
    'int32_t': ('int', 32, True), 'uint32_t': ('unsigned int', 32, False),
}
MAX_PARAMS = 4
CONVENTIONS = {'__cdecl': '__cdecl', '__stdcall': '__stdcall'}
SIGNATURE = re.compile(r'^\s*(?P<head>.*?)\s*(?P<name>[A-Za-z_~][\w:~]*)\s*\((?P<params>.*)\)\s*;?\s*$')


def _skip(address, name, reason):
    return '// skipped %s (%s): %s\n' % (address, name, reason)


def _norm_type(text):
    return re.sub(r'\s+', ' ', text.replace('const ', '')).strip()


def _parse_signature(signature):
    """(convention, return type, [(type, name)]) or a reason string."""
    m = SIGNATURE.match(signature)
    if not m:
        return 'the signature does not parse'
    head = m.group('head')
    conv = '__cdecl'
    for key in list(CONVENTIONS) + ['__thiscall', '__fastcall']:
        if key in head:
            conv = key
            head = head.replace(key, ' ')
    if conv in ('__thiscall', '__fastcall'):
        return 'member or fastcall convention (%s) needs a this/register setup' % conv
    ret = _norm_type(head)
    if ret not in INT_TYPES:
        return 'return type %r has no honest comparison here (integer returns only)' % ret
    params = []
    raw = m.group('params').strip()
    if raw and raw != 'void':
        for i, part in enumerate(raw.split(',')):
            part = part.strip()
            if not part or '*' in part or '&' in part or '...' in part:
                return 'parameter %r is not integer-like (pointers need a type-appropriate table)' % part
            pm = re.match(r'^(.*?)(?:\s+([A-Za-z_]\w*))?$', part)
            ptype, pname = _norm_type(pm.group(1)), pm.group(2)
            if ptype not in INT_TYPES:
                # "uint" alone parses as type uint with no name; "foo bar"
                # with an unknown type is skipped.
                if part in INT_TYPES:
                    ptype, pname = part, None
                else:
                    return 'parameter type %r is not integer-like (no sweep table)' % ptype
            params.append((ptype, pname or 'p%d' % i))
    if len(params) > MAX_PARAMS:
        return 'more than %d parameters: the sweep product is too large' % MAX_PARAMS
    return conv, ret, params


def draft_case(address, name, signature, batch_id, thiscall=False, header=None):
    """A C++ oracle case for one pure function, in the shape of
    oracle/cases/rand.cpp, or a one-line skip comment naming the reason.

    Each integer parameter is swept over {min, -1, 0, 1, 7, max} of its type
    (every combination) and the return values are compared. Other parameter
    types, non-integer returns and member calls (a non-static Class::Method
    needs __thiscall and a this: pass thiscall=True) are skipped."""
    addr = address[2:] if address.lower().startswith('0x') else address
    addr = addr.upper()
    if not re.match(r'^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$', name or ''):
        return _skip(address, name, 'name is not a callable C++ identifier')
    if re.match(r'^(?:FUN_|thunk_)', name.rsplit('::', 1)[-1]) and not header:
        return _skip(address, name, 'an unnamed Ghidra function: ours has no knowable name (give the row a header)')
    if '::' not in name and not header:
        return _skip(address, name, 'no Class:: in the name and no header: no header for ours is knowable')
    if thiscall:
        return _skip(address, name, 'non-static member: __thiscall needs a this the draft cannot build')
    parsed = _parse_signature(signature)
    if isinstance(parsed, str):
        return _skip(address, name, parsed)
    conv, ret, params = parsed
    cret = INT_TYPES[ret][0]
    ptypes = ', '.join('%s %s' % (INT_TYPES[t][0], n) for t, n in params)
    sig_types = ', '.join(INT_TYPES[t][0] for t, _ in params)
    args = ', '.join(n for _, n in params)
    fn = 'Case_%s' % addr
    out = []
    out.append('// %s at 0x%s, the original\'s against ours over swept integer arguments.' % (name, addr))
    out.append('// Drafted by tools/truth.py for batch %s; each argument takes {min, -1, 0, 1, 7, max}.' % batch_id)
    out.append('using Fn_%s = %s(%s*)(%s);' % (addr, cret, conv, sig_types))
    out.append('constexpr uintptr_t kOriginal_%s = 0x%s;' % (addr, addr))
    out.append('')
    out.append('%s Original_%s(%s)' % (cret, addr, ptypes))
    out.append('{')
    out.append('    return reinterpret_cast<Fn_%s>(kOriginal_%s)(%s);' % (addr, addr, args))
    out.append('}')
    out.append('')
    out.append('void %s(CaseContext& ctx)' % fn)
    out.append('{')
    indent = '    '
    for t, n in params:
        ct = INT_TYPES[t][0]
        out.append('%sfor (const %s %s : Sweep<%s>()) {' % (indent, ct, n, ct))
        indent += '    '
    shown = ' + ", " + '.join('std::to_string(%s)' % n for _, n in params) or '""'
    label = '"%s(" + %s + ")"' % (name, shown) if params else '"%s()"' % name
    out.append('%sctx.SetInputs(%s);' % (indent, label))
    out.append('%s%s original = 0;' % (indent, cret))
    out.append('%sctx.CallOriginal([&] { original = Original_%s(%s); });' % (indent, addr, args))
    out.append('%sconst %s ours = %s(%s);' % (indent, cret, name, args))
    out.append('%sctx.CountInputs(1);' % indent)
    out.append('%soracle::Compare(ctx, original, ours);' % indent)
    for _ in params:
        indent = indent[:-4]
        out.append('%s}' % indent)
    out.append('}')
    out.append('')
    out.append('ORACLE_CASE("wave/%s/%s", %s);' % (batch_id, addr, fn))
    return '\n'.join(out) + '\n'


PRELUDE = '''// Drafted by tools/truth.py for wave batch %(batch)s: pure functions swept over
// integer arguments, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>
// (compat's win_types.h defines the same names): it reaches the original only
// through CallOriginal and the addresses below.
%(includes)s
#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

template <typename T>
std::vector<T> Sweep()
{
    return { std::numeric_limits<T>::min(), static_cast<T>(-1), static_cast<T>(0), static_cast<T>(1),
        static_cast<T>(7), std::numeric_limits<T>::max() };
}

'''


def _split_case(src):
    """(body inside the namespace, registration line) of one drafted case."""
    body, _, reg = src.rpartition('ORACLE_CASE(')
    return body.rstrip() + '\n', 'ORACLE_CASE(' + reg


def write_cases(batch_id, rows, cases_dir=CASES_DIR):
    """Write wave-<batch>.cpp into cases_dir from rows (dicts with address,
    name, signature and optionally thiscall) and return its path. A row the
    draft cannot call honestly becomes a skip comment naming the reason.
    Adding the file to the oracle target is a separate step (the case files
    are listed explicitly in oracle/CMakeLists.txt)."""
    drafts = []
    skips = []
    includes = set()
    for row in rows:
        try:
            src = draft_case(row['address'], row['name'], row['signature'], batch_id,
                             row.get('thiscall', False), row.get('header'))
        except Exception as e:  # one bad row must not break the wave file
            src = _skip(row.get('address', '?'), row.get('name', '?'),
                        'the row could not be drafted (%s: %s)' % (type(e).__name__, e))
        if src.startswith('// skipped'):
            skips.append(src)
            continue
        drafts.append(_split_case(src))
        if row.get('header'):
            includes.add('#include "%s"' % row['header'])
        else:
            includes.add('#include "%s.h"' % row['name'].rsplit('::', 1)[0].split('::')[-1])
    text = PRELUDE % {'batch': batch_id, 'includes': '\n'.join(sorted(includes))}
    text += '\n'.join(body for body, _ in drafts)
    text += '\n} // namespace\n\n'
    text += ''.join(reg for _, reg in drafts)
    if skips:
        text += '\n' + ''.join(skips)
    os.makedirs(cases_dir, exist_ok=True)
    path = os.path.join(cases_dir, 'wave-%s.cpp' % batch_id)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return path


# ---- the oracle's report ------------------------------------------------------

WAVE_CASE = re.compile(r'^wave/([^/]+)/([0-9A-Fa-f]+)$')


def _status(case):
    """(status, detail) of one report case. The real report names a case by
    case['case'] (oracle/runner/report.cpp); tools/oracle.py case_ok is the
    pass test for a declared-pass case, kept in step here (importing oracle.py
    would pull in its whole toolchain)."""
    if case.get('crashed'):
        crash = case.get('crash') or {}
        return 'crash', '%s (%s)' % (crash.get('kind') or 'crashed', crash.get('inputs', ''))
    if case.get('failure_count'):
        return 'fail', 'failure_count %d' % case['failure_count']
    if not case.get('iterations'):
        return 'pending', 'ran no input' + (' (%s)' % case['pending'] if case.get('pending') else '')
    return 'pass', '%d inputs' % case['iterations']


def probe_verdicts(report_json, batch_id=None):
    """{'0x<ADDR>': (status, detail)} for the wave/<batch>/<ADDR> cases of an
    oracle.py run report (a JSON string or the parsed dict), the keys in the
    disagreements.json format. status is 'pass', 'fail' (ran and disagreed),
    'crash' (crashed: inputs outside the domain, say a divide by zero) or
    'pending' (ran nothing). With batch_id only that batch's cases count;
    without it, when two batches probe one address the last in report order
    wins. Other cases are not probes and are ignored."""
    report = json.loads(report_json) if isinstance(report_json, (str, bytes)) else report_json
    verdicts = {}
    for case in report.get('cases', []):
        m = WAVE_CASE.match(case.get('case', ''))
        if m and (batch_id is None or m.group(1) == batch_id):
            verdicts['0x' + m.group(2).upper()] = _status(case)
    return verdicts


# ---- hook tables and the batched Frida run -------------------------------------

HOOK_MAX = 200
FRIDA_TYPES = {
    'int': 's32', 'long': 's32', 'int32_t': 's32', 'uint': 'u32', 'ulong': 'u32',
    'uint32_t': 'u32', 'dword': 'u32', 'undefined4': 'u32', 'short': 's16',
    'int16_t': 's16', 'ushort': 'u16', 'uint16_t': 'u16', 'word': 'u16',
    'undefined2': 'u16', 'char': 'u8', 'uchar': 'u8', 'byte': 'u8', 'bool': 'u8',
    'undefined1': 'u8', 'uint8_t': 'u8', 'float': 'f32',
}
FRIDA_CONVS = {'__thiscall': 'thiscall', '__cdecl': 'cdecl', '__stdcall': 'stdcall'}
MAP_LOAD = re.compile(r'Preferred load address is\s+([0-9A-Fa-f]+)')
MAP_PUBLIC = re.compile(r'^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{8,16})\b')


def hook_name(address):
    """The hook (and trace tag) name of a row: the same on both targets."""
    addr = address[2:] if address.lower().startswith('0x') else address
    return 'fn_' + addr.upper()


def _frida_type(text):
    text = re.sub(r'\bconst\b', '', text).strip()
    if '*' in text or '&' in text:
        return 'ptr'
    words = text.split()
    if words[:1] in (['unsigned'], ['signed']) and len(words) > 1:
        base = FRIDA_TYPES.get(words[1].lower(), 'u32')
        return base if words[0] == 'signed' else base.replace('s', 'u')
    return FRIDA_TYPES.get(text.lower(), 'u32')


def _hook_signature(signature):
    """(conv, args, ret or None) of a C signature. Parameters that are the
    implicit this are dropped for thiscall; void returns log no value."""
    m = SIGNATURE.match(signature or '')
    if not m:
        return None
    head = m.group('head')
    conv = 'cdecl'
    for key, value in FRIDA_CONVS.items():
        if key in head:
            conv = value
            head = head.replace(key, ' ')
    head = re.sub(r'\s+', ' ', head).strip()
    ret = None if head in ('void', '') else _frida_type(head)
    raw = m.group('params').strip()
    args = []
    if raw and raw != 'void':
        for i, part in enumerate(raw.split(',')):
            part = part.strip()
            if part == '...':
                continue
            pm = re.match(r'^(.*?)(?:\s*\b([A-Za-z_]\w*))?$', part)
            ptype = pm.group(1).strip() if pm.group(1).strip() else part
            if ptype.lower() in FRIDA_TYPES or '*' in ptype or ' ' in ptype:
                pass
            else:
                ptype = part  # a lone type name such as "uint"
            if conv == 'thiscall' and i == 0 and pm.group(2) == 'this':
                continue
            args.append(_frida_type(ptype))
    return conv, args, ret


def parse_map(text):
    """{decorated name: rva} from an MSVC link map: the Rva+Base column of the
    publics minus the preferred load address."""
    base = 0x400000
    m = MAP_LOAD.search(text)
    if m:
        base = int(m.group(1), 16)
    out = {}
    for line in text.split('\n'):
        m = MAP_PUBLIC.match(line)
        if m:
            out.setdefault(m.group(1), int(m.group(2), 16) - base)
    return out


def _map_rva(name, publics):
    """The rva of Class::Method (decorated ?Method@Class@@...) or a plain
    name, or None. An ambiguous overload set takes the first public."""
    parts = name.split('::')
    prefix = '?' + '@'.join(reversed(parts)) + '@@'
    for decorated, rva in publics.items():
        if decorated.startswith(prefix) or (len(parts) == 1 and decorated in (name, '_' + name)):
            return rva
    return None


def hook_table(rows, signatures, our_map, scenarios):
    """The hook tables for one batch: {'original': {'hooks': [...]}, 'ours':
    {'hooks': [...]}, 'scenarios': [...], 'skipped': {addr: reason}}.

    rows are dicts with address and name; signatures maps address to a C
    signature; our_map is the build's .map text (or a {name: rva} dict). Each
    row gets one hook per target, named fn_<ADDR> so the traces line up: the
    original's by absolute addr, ours by rva. A row without a parseable
    signature or without a map entry for ours is listed in skipped and has no
    hook on either side (a one-sided table could only say 'different')."""
    publics = parse_map(our_map) if isinstance(our_map, str) else dict(our_map)
    original, ours, skipped = [], [], {}
    for row in rows:
        address = row['address']
        key = '0x' + address[2:].upper() if address.lower().startswith('0x') else '0x' + address.upper()
        sig = _hook_signature(signatures.get(address) or signatures.get(key))
        if sig is None:
            skipped[key] = 'the signature does not parse'
            continue
        if isinstance(our_map, str):
            rva = _map_rva(row['name'], publics)
        else:
            rva = publics.get(row['name'])
        if rva is None:
            skipped[key] = 'no entry for %s in our map' % row['name']
            continue
        conv, args, ret = sig
        hook = {'name': hook_name(key), 'conv': conv, 'args': args}
        if ret:
            hook['ret'] = ret
        hook['max'] = HOOK_MAX
        original.append(dict(hook, addr=key))
        ours.append(dict(hook, rva=hex(rva)))
    return {'original': {'hooks': original}, 'ours': {'hooks': ours},
            'scenarios': list(scenarios), 'skipped': skipped}


def _read_events(path):
    events = []
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        row = json.loads(line)
                    except ValueError:
                        continue
                    if isinstance(row, dict):
                        events.append(row)
    except OSError:
        pass
    return events


def frida_run(batch_id, table, scenarios, run=subprocess.run, root=ROOT):
    """{scenario: (original events, ours events)}: for each scenario, both
    targets run through `probe.py run` (one sandbox each) with this batch's
    hook tables, and the traces they leave under runs/<stamp>/replay/ are read
    back. A side that left no trace gives []. `run` is injectable (the tests
    never launch anything); the real call is made by hand on the Windows
    machine under the one-launch rules."""
    import tempfile
    probe = os.path.join(FRIDA_DIR, 'probe.py')
    traces = {}
    with tempfile.TemporaryDirectory() as tmp:
        tables = {}
        for target, side in (('original', 'original'), ('ours', 'ours')):
            path = os.path.join(tmp, 'hooks-%s-%s.json' % (batch_id, side))
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                json.dump(table[side], f, indent=1)
            tables[target] = path
        for scenario in scenarios:
            sides = []
            for target in ('original', 'ours'):
                out = 'frida-%s-%s-%s.jsonl' % (batch_id, scenario, target)
                cmd = [sys.executable, '-I', probe, 'run', '--target', target,
                       '--probe', tables[target], '--scenario', scenario, '--out', out]
                done = run(cmd, capture_output=True, text=True, cwd=root)
                folder = None
                for line in (getattr(done, 'stdout', '') or '').splitlines():
                    if line.startswith('probe: ') and line[7:].startswith('runs'):
                        folder = line[7:].strip()
                sides.append(_read_events(os.path.join(root, folder, 'replay', out))
                             if folder else [])
            traces[scenario] = (sides[0], sides[1])
    return traces


def _row_events(events, tag):
    """The events of one hooked function (entry and return), plus any terminal
    failure event (a crash never passes)."""
    keep = {tag, tag + '_ret'} | trace_diff.TERMINAL_FAILURE_TAGS
    return [e for e in events if str(e.get('tag', '')).lower() in keep
            or str(e.get('tag', '')) in keep]


def frida_verdicts(traces, rows):
    """{'0x<ADDR>': ('equal'|'different'|'not-exercised', detail)} from the
    {scenario: (orig, ours)} traces frida_run returns. A row is exercised in a
    scenario when either side logged one of its hooks; across scenarios any
    difference wins, otherwise a row exercised anywhere is equal, and one
    exercised nowhere is not-exercised (and stays as it is)."""
    verdicts = {}
    for row in rows:
        address = row['address']
        key = '0x' + (address[2:] if address.lower().startswith('0x') else address).upper()
        tag = hook_name(key)
        different, equal = [], []
        for scenario, (orig, ours) in traces.items():
            o = [e for e in _row_events(orig, tag) if str(e.get('tag', '')).startswith(tag)]
            r = [e for e in _row_events(ours, tag) if str(e.get('tag', '')).startswith(tag)]
            if not o and not r:
                continue
            code = trace_diff.compare(_row_events(orig, tag), _row_events(ours, tag))
            if code == 0:
                equal.append(scenario)
            else:
                different.append('%s (%s)' % (scenario, 'compare=%d' % code))
        if different:
            verdicts[key] = ('different', 'differs in ' + ', '.join(different))
        elif equal:
            verdicts[key] = ('equal', 'identical in ' + ', '.join(equal))
        else:
            verdicts[key] = ('not-exercised', 'no hook fired in %s' % (', '.join(traces) or 'any scenario'))
    return verdicts


def _trace_hash(events):
    blob = json.dumps(events, sort_keys=True, separators=(',', ':')).encode('utf-8')
    return hashlib.sha256(blob).hexdigest()


def trace_source(probe, orig_events, ours_events):
    """The trace_source of a `kind: trace` evidence document, in the form
    tools/check_evidence.py accepts: frida <probe> orig:<sha256>
    ours:<sha256> compare=0. Only meaningful for an equal verdict."""
    return 'frida %s orig:%s ours:%s compare=0' % (
        probe, _trace_hash(orig_events), _trace_hash(ours_events))
