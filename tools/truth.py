"""Decide open wave rows by measurement: a purity check, oracle case drafts, and
a reader for the oracle's report.

A function that is pure (arithmetic on its arguments only) can be decided by
calling the original exe's function and ours over swept inputs and comparing
the results. is_pure reads the evidence file's Decompiled (Ghidra) section;
draft_case and write_cases turn a pure function into an oracle case in the
shape of oracle/cases/rand.cpp; probe_verdicts reads what `oracle.py run`
reported for those cases. Nothing here runs the oracle, the game or any exe.

Python 3 standard library only; run under `python3 -I`.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES_DIR = os.path.join(ROOT, 'oracle', 'cases')

SECTION = '## Decompiled (Ghidra)'

KEYWORDS = frozenset({'return', 'sizeof', 'if', 'while', 'for', 'switch',
                      'else', 'do', 'case', 'goto', 'defined'})
# Ghidra pseudo-operations: pure arithmetic helpers, not calls.
PSEUDO_OPS = re.compile(
    r'^(?:CONCAT|SUB|ZEXT|SEXT|SBORROW|SCARRY|CARRY|POPCOUNT|LZCOUNT|ROL|ROR'
    r'|PIECE|MULT|ABS|FLOAT2INT|INT2FLOAT|NAN|TRUNC|CEIL|FLOOR|ROUND|SQRT)\d*$')
CALL = re.compile(r'\b([A-Za-z_][\w:~]*)\s*\(')
DEREF_AFTER = frozenset({'return', 'else', 'case', 'do'})


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


def _is_deref(line):
    """Whether the line has a unary `*` (a pointer read or write)."""
    for i, ch in enumerate(line):
        if ch != '*':
            continue
        before = line[:i].rstrip()
        if not before or before[-1] not in ')]' and not (before[-1].isalnum() or before[-1] == '_'):
            return True
        word = re.search(r'[A-Za-z_]\w*$', before)
        if word and word.group(0) in DEREF_AFTER:
            return True
    return False


def _offence(line, pure_names):
    """The first reason this body line is impure, or None."""
    m = re.search(r'\bDAT_[0-9a-fA-F]+', line)
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
    for m in CALL.finditer(line):
        name = m.group(1)
        if name in KEYWORDS or PSEUDO_OPS.match(name) or name in pure_names:
            continue
        return 'call to %s' % name
    return None


def is_pure(evidence_text, pure_names=frozenset()):
    """(pure, reason) for an evidence file's decompiled function.

    Impure on any DAT_ global, a pointer dereference (a global read through a
    pointer chain, or a write through a pointer parameter), a this member
    access, or a call to a function not in pure_names. The signature line is
    not read for calls. The reason names the first offending line."""
    code = _section_code(evidence_text)
    if code is None:
        return False, 'no Decompiled (Ghidra) section'
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.S)
    code = re.sub(r'//[^\n]*', '', code)
    brace = code.find('{')
    if brace < 0:
        return False, 'no function body in the Decompiled (Ghidra) section'
    for line in code[brace + 1:].split('\n'):
        stripped = line.strip()
        if not stripped:
            continue
        why = _offence(stripped, pure_names)
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


def draft_case(address, name, signature, batch_id, thiscall=False):
    """A C++ oracle case for one pure function, in the shape of
    oracle/cases/rand.cpp, or a one-line skip comment naming the reason.

    Each integer parameter is swept over {min, -1, 0, 1, 7, max} of its type
    (every combination) and the return values are compared. Other parameter
    types, non-integer returns and member calls (a non-static Class::Method
    needs __thiscall and a this: pass thiscall=True) are skipped."""
    addr = address[2:] if address.lower().startswith('0x') else address
    addr = addr.upper()
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
        src = draft_case(row['address'], row['name'], row['signature'], batch_id,
                         row.get('thiscall', False))
        if src.startswith('// skipped'):
            skips.append(src)
            continue
        drafts.append(_split_case(src))
        if '::' in row['name']:
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

HEAP_CORRUPT = 'heap corrupt after case'  # tools/oracle.py HEAP_CORRUPT
WAVE_CASE = re.compile(r'^wave/[^/]+/([0-9A-Fa-f]+)$')


def _case_ok(case):
    """Whether a case did what it declared: tools/oracle.py case_ok, kept in
    step with it (importing oracle.py would pull in its whole toolchain)."""
    if case.get('pending') and case.get('has_body') \
            and not case['iterations'] and not case['crashed']:
        return False
    expect = case.get('expect', 'pass')
    if expect == 'fail':
        return not case['crashed'] and case['failure_count'] > 0
    if expect == 'crash':
        crash = case.get('crash') or {}
        return (case['crashed'] and case['failure_count'] == 0
                and crash.get('kind') != HEAP_CORRUPT)
    return not case['crashed'] and case['failure_count'] == 0


def probe_verdicts(report_json):
    """{'0x<ADDR>': ok} for the wave/<batch>/<ADDR> cases of an oracle.py run
    report (a JSON string or the parsed dict), the keys in the
    disagreements.json format. Other cases are not probes and are ignored."""
    report = json.loads(report_json) if isinstance(report_json, (str, bytes)) else report_json
    verdicts = {}
    for case in report.get('cases', []):
        m = WAVE_CASE.match(case.get('name', ''))
        if m:
            verdicts['0x' + m.group(1).upper()] = _case_ok(case)
    return verdicts
