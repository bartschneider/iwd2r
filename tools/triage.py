#!/usr/bin/env python3
"""Classify a disagreement row from the two function bodies.

The mechanical cases (a rename, a changed signature, a stub on one side)
need no agent; everything else is 'ambiguous' and goes to a worker."""
import argparse
import collections
import importlib.util
import json
import os
import re
import subprocess
import tempfile

_here = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('xrec', os.path.join(_here, 'xrec.py'))
xrec = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(xrec)

CLASSES = ('rename-only', 'signature-only', 'fork-stub', 'ours-stub', 'ambiguous')
# disagreements.json was computed against our tree at this commit; the closed
# batches have since adopted fork bodies into later trees, so calibration must
# classify the already-closed rows here, never at the ref used for new batches
CALIBRATION_OURS_REF = '82d75232'
AUTO_OURS = ('rename-only', 'signature-only', 'fork-stub')
CASE_IDS = {'rename-only': 'triage-rename', 'signature-only': 'triage-sig',
            'fork-stub': 'triage-stub'}

KEYWORDS = frozenset('''
alignas alignof and and_eq asm auto bitand bitor bool break case catch char
class compl const const_cast constexpr continue decltype default delete do
double dynamic_cast else enum explicit export extern false float for friend
goto if inline int long mutable namespace new noexcept not not_eq nullptr
operator or or_eq private protected public register reinterpret_cast return
short signed sizeof static static_assert static_cast struct switch template
this throw true try typedef typeid typename union unsigned using virtual void
volatile wchar_t while xor xor_eq override final
'''.split())

TOKEN = re.compile(r'''
    "(?:\\.|[^"\\])*" | '(?:\\.|[^'\\])*'          # string and char literals
  | 0[xX][0-9A-Fa-f]+[uUlL]* | \d+\.?\d*(?:[eE][+-]?\d+)?[fFuUlL]*   # numbers
  | [A-Za-z_]\w*                                   # identifiers
  | ::|->|<<=|>>=|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||[-+*/%&|^]=
  | \S                                             # any other punctuation
''', re.X)
IDENT = re.compile(r'[A-Za-z_]\w*$')


def _tokens(text):
    return TOKEN.findall(xrec.blank_comments(text))


MEMBER_OPS = frozenset(('->', '.', '::'))


def _plain(toks, i, type_names):
    """True when toks[i] is an identifier that gets renamed: not a keyword,
    a type name, a call target, or a member after ->, . or ::."""
    t = toks[i]
    return (IDENT.match(t) and t not in KEYWORDS and t not in type_names
            and not (i + 1 < len(toks) and toks[i + 1] == '(')
            and not (i and toks[i - 1] in MEMBER_OPS))


def _names(text, type_names):
    """Renamable identifiers of text in first-seen order."""
    toks = _tokens(text)
    seen = []
    for i, t in enumerate(toks):
        if _plain(toks, i, type_names) and t not in seen:
            seen.append(t)
    return seen


def normalize(body, type_names, seed=()):
    """Token stream with every plain identifier replaced by v<n> in
    first-seen order (names in seed are numbered first). Keywords, type
    names, call targets, members and literals stay."""
    toks = _tokens(body)
    names = {}
    for n in seed:
        names.setdefault(n, 'v%d' % (len(names) + 1))
    out = []
    for i, t in enumerate(toks):
        if _plain(toks, i, type_names):
            t = names.setdefault(t, 'v%d' % (len(names) + 1))
        out.append(t)
    return ' '.join(out)


def callees(body):
    """Names that are called: an identifier followed by '('."""
    toks = _tokens(body)
    return frozenset(t for i, t in enumerate(toks[:-1])
                     if toks[i + 1] == '(' and IDENT.match(t) and t not in KEYWORDS)


def _split(text):
    i = text.find('{')
    return (text, '') if i < 0 else (text[:i], text[i:])


def _is_stub(body):
    """A TODO / NotImplemented / assert(false) marker or no statements at
    all. xrec.stubby also calls any body under 40 characters a stub, which
    would swallow short real bodies, so the length rule is padded out."""
    if not re.sub(r'[\s{}]', '', xrec.blank_comments(body)):
        return True
    return xrec.stubby(body + 'x' * 40)


def classify(theirs, ours, type_names):
    """(class, reason) for one pair of full function texts."""
    tsig, tbody = _split(theirs)
    osig, obody = _split(ours)
    tstub, ostub = _is_stub(tbody), _is_stub(obody)
    if ostub and not tstub:
        return 'ours-stub', 'our body is a stub and theirs is not'
    if tstub and not ostub:
        return 'fork-stub', 'their body is a stub and ours is not'
    if tstub and ostub:
        return 'ambiguous', 'both bodies are stubs'
    tl = len(re.sub(r'\s+', '', xrec.blank_comments(tbody)))
    ol = len(re.sub(r'\s+', '', xrec.blank_comments(obody)))
    if tl < ol / 4 and callees(tbody) <= callees(obody):
        return 'fork-stub', 'their body is under a quarter of ours and calls nothing new'
    if normalize(theirs, type_names) == normalize(ours, type_names):
        return 'rename-only', 'bodies and signatures match after renaming identifiers'
    # bodies under a mapping seeded from each signature's parameters, so a
    # changed use order inside the body cannot hide behind a matching signature
    tseed, oseed = _names(tsig, type_names), _names(osig, type_names)
    if (normalize(tbody, type_names, tseed) == normalize(obody, type_names, oseed)
            and normalize(tsig, type_names) != normalize(osig, type_names)):
        return 'signature-only', 'bodies match under the signature parameters; signatures differ'
    return 'ambiguous', 'the bodies differ beyond identifier names'


# like xrec.sig, but a plain function name also matches
SIG = re.compile(r'^(?:[A-Za-z_][\w:<>,*&\s]*?\s+)?([A-Za-z_]\w*(?:::[~A-Za-z_]\w*)*'
                 r'|[A-Za-z_]\w*::operator\S*)\s*\([^;{]*\)\s*(?:const)?\s*\{', re.M)
TYPE_DECL = re.compile(r'\b(?:class|struct)\s+([A-Za-z_]\w*)')
TYPEDEF_FN = re.compile(r'\btypedef\b[^;{]*\(\s*[*&]\s*([A-Za-z_]\w*)\s*\)')
TYPEDEF = re.compile(r'\btypedef\b[^;{]*(?:\{[^{}]*\}[^;{]*)?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*;')
USING = re.compile(r'\busing\s+([A-Za-z_]\w*)\s*=')
BUILTIN_TYPES = frozenset('''
LONG ULONG INT UINT BYTE WORD DWORD BOOL BOOLEAN SHORT USHORT CHAR UCHAR WCHAR
FLOAT DOUBLE SIZE_T LPSTR LPCSTR LPVOID HANDLE COLORREF RESREF STRREF CString
CPoint CRect CSize POSITION unsigned signed char short long float double bool
int void
'''.split())


def extract(text):
    """name -> (address, full text of signature and body, whitespace kept,
    comments blanked). Overloads collapse to the last."""
    masked = xrec.blank_comments(text)
    out = {}
    for m in SIG.finditer(masked):
        if m.group(1) in KEYWORDS:
            continue
        i = m.end() - 1
        depth, j = 0, i
        while j < len(masked):
            if masked[j] == '{':
                depth += 1
            elif masked[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        out[m.group(1)] = (xrec.signature_addr(text, masked, m), masked[m.start():j + 1])
    return out


def _headers(ref):
    r = subprocess.run(['git', 'ls-tree', '-r', '--name-only', ref, 'src', 'compat'],
                       capture_output=True, text=True)
    return [f for f in r.stdout.splitlines() if f.endswith('.h')]


def load_type_names(show, ours_ref, list_headers=_headers):
    """Type names: class / struct names, typedef and using aliases declared
    in src/*.h and compat/*.h at ours_ref, plus the fixed Win32 and compat
    builtins. A change of type alone must never normalise equal."""
    names = set(BUILTIN_TYPES)
    for path in list_headers(ours_ref):
        text = xrec.blank_comments(show(ours_ref, path))
        for rx in (TYPE_DECL, TYPEDEF_FN, TYPEDEF, USING):
            names.update(rx.findall(text))
    return frozenset(names)


def _find(table, name, address):
    if name in table:
        return table[name][1]
    if address:
        for addr, text in table.values():
            if addr and addr.lower() == address.lower():
                return text
    return None


def triage_rows(rows, fork_ref, ours_ref, show=xrec.show, type_names=None):
    """Entries {address, file, name, class, reason} for every
    both_changed_differ row."""
    if type_names is None:
        type_names = load_type_names(show, ours_ref)
    by_file = collections.OrderedDict()
    for r in rows:
        if r['category'] == 'both_changed_differ':
            by_file.setdefault(r['file'], []).append(r)
    out = []
    memo = {}

    def cached(ref, path):
        if (ref, path) not in memo:
            memo[(ref, path)] = show(ref, path)
        return memo[(ref, path)]

    for path, group in by_file.items():
        theirs, ours = extract(cached(fork_ref, path)), extract(cached(ours_ref, path))
        for r in group:
            t, o = _find(theirs, r['name'], r['address']), _find(ours, r['name'], r['address'])
            if t is None or o is None:
                cls, why = 'ambiguous', 'function not found in %s' % (
                    'both bodies' if t is None and o is None else
                    'the fork' if t is None else 'our tree')
            else:
                cls, why = classify(t, o, type_names)
            out.append({'address': r['address'], 'file': path, 'name': r['name'],
                        'class': cls, 'reason': why})
    return out


def _write_atomic(path, data):
    d = os.path.dirname(os.path.abspath(path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.triage-', suffix='.json')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
            f.write('\n')
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def calibrate(state, triaged):
    """class -> {agree, total, mismatches}: each triaged row that has a
    recorded verdict in a closed batch, agreeing when the side is 'ours'.
    Auto verdicts (case triage-*) are skipped: they are always 'ours', so
    a class would vouch for itself."""
    cls_of = {r['address']: r.get('auto_class', r['class']) for r in triaged}
    cal = {c: {'agree': 0, 'total': 0, 'mismatches': []} for c in CLASSES}
    for b in state.get('batches', {}).values():
        if b.get('status') != 'closed':
            continue
        for addr, v in (b.get('verdicts') or {}).items():
            if str(v.get('case') or '').startswith('triage-'):
                continue
            c = cls_of.get(addr)
            if c is None:
                continue
            cal[c]['total'] += 1
            if v.get('side') == 'ours':
                cal[c]['agree'] += 1
            else:
                cal[c]['mismatches'].append(addr)
    return cal


def enabled_classes(calibration):
    return frozenset(c for c in AUTO_OURS
                     if calibration[c]['total'] > 0
                     and calibration[c]['agree'] == calibration[c]['total'])


def calibration_text(calibration):
    enabled = enabled_classes(calibration)
    lines = ['%-15s %5s %5s  %s' % ('class', 'agree', 'total', 'status')]
    for c in CLASSES:
        e = calibration[c]
        status = ('enabled' if c in enabled else
                  'never enabled' if c not in AUTO_OURS else
                  'disabled (no verdicts)' if not e['total'] else 'disabled by calibration')
        lines.append('%-15s %5d %5d  %s' % (c, e['agree'], e['total'], status))
        if e['mismatches']:
            lines.append('    mismatches: ' + ', '.join(e['mismatches']))
    return '\n'.join(lines)


def closed_rows(state, rows):
    """Disagreement rows whose address has a verdict in a closed batch."""
    done = {a for b in state.get('batches', {}).values() if b.get('status') == 'closed'
            for a in (b.get('verdicts') or {})}
    return [r for r in rows if r['address'] in done]


def calibration_triage(state, rows, fork_ref, show=xrec.show, type_names=None):
    """The closed-batch rows classified at CALIBRATION_OURS_REF."""
    return triage_rows(closed_rows(state, rows), fork_ref, CALIBRATION_OURS_REF,
                       show=show, type_names=type_names)


def require_ref(ref):
    r = subprocess.run(['git', 'rev-parse', '--verify', '--quiet', ref + '^{commit}'],
                       capture_output=True, text=True)
    if r.returncode:
        raise SystemExit('triage: ref %r is missing or unreachable' % ref)


def apply_calibration(triaged, calibration):
    """Rows of a disabled auto class become 'ambiguous' (the original class
    is kept as auto_class so a later calibration still sees it)."""
    on = enabled_classes(calibration)
    out = []
    for r in triaged:
        r = dict(r)
        c = r.get('auto_class', r['class'])
        if c in AUTO_OURS and c not in on:
            r.update({'auto_class': c, 'class': 'ambiguous', 'reason': 'disabled by calibration'})
        out.append(r)
    return out


def main(argv=None):
    root = os.path.dirname(_here)
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fork-ref')
    ap.add_argument('--ours-ref', default='first')
    ap.add_argument('--disagreements', default=os.path.join(root, 'docs/wso/disagreements.json'))
    ap.add_argument('--calibrate', action='store_true')
    ap.add_argument('--out', default=os.path.join(root, 'docs/wso/triage.json'))
    a = ap.parse_args(argv)
    with open(os.path.join(root, 'docs/wso/wave-state.json'), encoding='utf-8') as f:
        state = json.load(f)
    fork = a.fork_ref or state['fork_base']
    with open(a.disagreements, encoding='utf-8') as f:
        rows = json.load(f)
    for ref in (fork, CALIBRATION_OURS_REF) + (() if a.calibrate else (a.ours_ref,)):
        require_ref(ref)
    cal = calibrate(state, calibration_triage(state, rows, fork))
    if a.calibrate:
        print(calibration_text(cal))
        return 1 if any(cal[c]['mismatches'] for c in AUTO_OURS) else 0
    out = apply_calibration(triage_rows(rows, fork, a.ours_ref), cal)
    for r in out:
        r['ours_ref'] = a.ours_ref
    _write_atomic(a.out, out)
    counts = collections.Counter(r['class'] for r in out)
    for c in CLASSES:
        print('%-15s %d' % (c, counts.get(c, 0)))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
