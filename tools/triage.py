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
    r = subprocess.run(['git', 'ls-tree', '-r', '--name-only', ref, 'src'],
                       capture_output=True, text=True)
    return [f for f in r.stdout.splitlines() if f.endswith('.h')]


def load_type_names(show, ours_ref, list_headers=_headers):
    """class X / struct X names declared in src/*.h at ours_ref."""
    names = set()
    for path in list_headers(ours_ref):
        names.update(TYPE_DECL.findall(xrec.blank_comments(show(ours_ref, path))))
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
    for path, group in by_file.items():
        theirs, ours = extract(show(fork_ref, path)), extract(show(ours_ref, path))
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


def main(argv=None):
    root = os.path.dirname(_here)
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fork-ref')
    ap.add_argument('--ours-ref', default='first')
    ap.add_argument('--disagreements', default=os.path.join(root, 'docs/wso/disagreements.json'))
    ap.add_argument('--out', default=os.path.join(root, 'docs/wso/triage.json'))
    a = ap.parse_args(argv)
    fork = a.fork_ref
    if not fork:
        with open(os.path.join(root, 'docs/wso/wave-state.json'), encoding='utf-8') as f:
            fork = json.load(f)['fork_base']
    with open(a.disagreements, encoding='utf-8') as f:
        rows = json.load(f)
    out = triage_rows(rows, fork, a.ours_ref)
    _write_atomic(a.out, out)
    counts = collections.Counter(r['class'] for r in out)
    for c in CLASSES:
        print('%-15s %d' % (c, counts.get(c, 0)))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
