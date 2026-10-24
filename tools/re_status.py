"""Recovery status: how much of the engine is recovered and how much of that
is verified against the original, kept in docs/re-status.json and in the
README's `### Recovery status` block.

Usage: python3 -I tools/re_status.py compute | --check | --markdown
                                     [--repo DIR] [--exports DIR]

compute   reads the Ghidra function table (<exports>/functions.tsv, tab
          separated `addr name size`), src/**/*.cpp, docs/evidence/*.md and
          scenarios/, then writes docs/re-status.json and the README block.
--check   exit 1 when the README block differs from the rendering of the
          committed docs/re-status.json (needs no exports; the gate runs it).
--markdown prints the block for the committed docs/re-status.json.

Denominator: the table minus `Unwind@`, `Catch@`, `Finally@` funclets and
`thunk_` thunks. Recovered: a standalone // 0xADDR marker in src/*.cpp or *.h
precedes a definition whose body is not a stub (xrec.stubby, with one-line
return/assignment bodies counted as real). Verified: recovered
and the evidence file's kind is oracle, replay or trace. A scenario counts as
matched when scenarios/<name>.txt has a <name>.expect.json beside it, the
checkpoints of the original's state dumps (scenarios/pending/ is not counted).
"""
import datetime
import glob
import importlib.util
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BEGIN = '<!-- re-status:begin -->'
END = '<!-- re-status:end -->'
FUNCLETS = ('Unwind@', 'Catch@', 'Finally@')
KINDS = ('oracle', 'replay', 'trace')


def _load(name):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(HERE, name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _norm(address):
    a = str(address).strip()
    if a[:2].lower() == '0x':
        a = a[2:]
    return a.upper().lstrip('0') or '0'


def read_table(path):
    rows = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 3:
                continue
            try:
                size = int(parts[2])
            except ValueError:
                continue  # header
            rows.append((_norm(parts[0]), parts[1], size))
    return rows


MARKER = re.compile(r'^\s*//\s*(0x[0-9A-Fa-f]+)\s*$')
OPERATOR = re.compile(r'operator\s*(?:\(\)|[-+*/%^&|!<>=\[\]~,]+)')
INIT = re.compile(r'(?<!:)[:,]\s*[\w:<>]+\s*$')
ASSIGN = re.compile(r'[^=!<>]=[^=]')


def _mask(text):
    """Comments and string/char literals as spaces, offsets kept."""
    pat = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
    return pat.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)


def is_stub(body, stubby):
    """xrec's rule, except that a body under 40 characters holding a return or
    an assignment is a real one-liner (TODO, NotImplemented and assert(false
    still make a stub whatever the length)."""
    if not stubby(body):
        return False
    if ('TODO' in body or 'NotImplemented' in body
            or 'assert(false' in body.replace(' ', '')):
        return True
    return not (len(body) < 40 and ('return' in body or ASSIGN.search(body)))


def _name(head):
    """Our function name from a definition head (text up to its `{`)."""
    head = head.split('(', 1)[0]
    m = re.search(r'((?:[~\w]+::)*operator\s*\S+|[~\w:]+)\s*$', head.strip())
    return m.group(1).replace(' ', '') if m else ''


def markers(text, stubby):
    """[(address, our name, stub?)] for every standalone // 0xADDR marker that
    precedes a function definition. A marker followed by another marker, or by
    a declaration or data definition (`;` or `=` before any body), is none."""
    lines = text.split('\n')
    masked = _mask(text)
    starts = []
    pos = 0
    for ln in lines:
        starts.append(pos)
        pos += len(ln) + 1
    mlines = masked.split('\n')
    scan = OPERATOR.sub(lambda m: 'operator' + ' ' * (len(m.group(0)) - 8), masked)
    out = []
    for i, ln in enumerate(lines):
        m = MARKER.match(ln)
        if not m:
            continue
        j = i + 1
        while j < len(lines) and not mlines[j].strip() and not MARKER.match(lines[j]):
            j += 1
        if j >= len(lines) or MARKER.match(lines[j]):
            continue
        k, paren = starts[j], 0
        end = len(masked)
        while k < end:
            c = scan[k]
            if c == '(':
                paren += 1
            elif c == ')':
                paren -= 1
            elif paren == 0 and c in ';=':
                k = -1
                break
            elif paren == 0 and c == '{':
                if INIT.search(scan[starts[j]:k]):  # member initializer {..}
                    d = 0
                    while k < end:
                        d += (scan[k] == '{') - (scan[k] == '}')
                        if d == 0:
                            break
                        k += 1
                else:
                    break
            k += 1
        if k < 0 or k >= end:
            continue
        depth, e = 0, k
        while e < end:
            if masked[e] == '{':
                depth += 1
            elif masked[e] == '}':
                depth -= 1
                if depth == 0:
                    break
            e += 1
        body = re.sub(r'\s+', '', masked[k:e + 1])
        out.append((m.group(1), _name(masked[starts[j]:k]), is_stub(body, stubby)))
    return out


def recovered_functions(repo):
    """{normalised address: our name} of every marker with a non-stub body."""
    stubby = _load('xrec').stubby
    out = {}
    for ext in ('cpp', 'h'):
        for path in sorted(glob.glob(os.path.join(repo, 'src', '**', '*.' + ext),
                                     recursive=True)):
            with open(path, encoding='utf-8', errors='replace') as f:
                text = f.read()
            for address, name, stub in markers(text, stubby):
                if not stub:
                    out.setdefault(_norm(address), name)
    return out


def evidence_kinds(repo):
    ce = _load('check_evidence')
    out = {}
    for path in glob.glob(os.path.join(repo, 'docs', 'evidence', '*.md')):
        with open(path, encoding='utf-8', errors='replace') as f:
            fm = ce.front_matter(f.read())
        kind = fm.get('kind')
        if kind == 'trace' and not fm.get('trace_source', '').startswith('frida '):
            kind = 'fork-trace'  # the fork's measurement, not ours
        out[_norm(os.path.basename(path)[:-3])] = kind
    return out


def scenarios(repo):
    d = os.path.join(repo, 'scenarios')
    return sorted(os.path.basename(p)[:-4]
                  for p in glob.glob(os.path.join(d, '*.txt'))
                  if os.path.exists(p[:-4] + '.expect.json'))


def compute(repo, exports):
    table = os.path.join(exports, 'functions.tsv')
    if not os.path.isfile(table):
        raise SystemExit(f're_status: no Ghidra function table at {table}; '
                         'run tools/ghidra/export_decompiles.py or pass --exports DIR')
    rows = read_table(table)
    funclets = sum(1 for _, n, _ in rows if n.startswith(FUNCLETS))
    thunks = sum(1 for _, n, _ in rows
                 if n.startswith('thunk_') and not n.startswith(FUNCLETS))
    den = [(a, s) for a, n, s in rows
           if not n.startswith(FUNCLETS) and not n.startswith('thunk_')]
    recfn = recovered_functions(repo)
    rec = set(recfn)
    names = {a: n for a, n, _ in rows}
    mismatched = sorted(
        (a, recfn[a], names[a]) for a in rec & set(names)
        if recfn[a] and recfn[a] != names[a]
        and not names[a].startswith(('FUN_', 'sub_')))
    if mismatched:
        print('warning: %d recovered addresses whose function name differs '
              'from the Ghidra name (address, ours, Ghidra):' % len(mismatched),
              file=sys.stderr)
        for a, ours, theirs in mismatched:
            print(f'  0x{a}  {ours}  {theirs}', file=sys.stderr)
    kinds = evidence_kinds(repo)
    recovered = [(a, s) for a, s in den if a in rec]
    verified = [(a, s) for a, s in recovered if kinds.get(a) in KINDS]
    by_kind = {k: sum(1 for a, _ in verified if kinds[a] == k) for k in KINDS}
    return {
        'at': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%d'),
        'ghidra': {'functions': len(den), 'bytes': sum(s for _, s in den),
                   'excluded': {'seh_funclets': funclets, 'thunks': thunks}},
        'recovered': {'functions': len(recovered),
                      'bytes': sum(s for _, s in recovered)},
        'verified': {'functions': len(verified),
                     'bytes': sum(s for _, s in verified), 'by_kind': by_kind},
        'scenarios': scenarios(repo),
    }


def _pct(n, d):
    return f'{100.0 * n / d:.1f}%' if d else '0.0%'


def render(s):
    g, r, v = s['ghidra'], s['recovered'], s['verified']
    k = v['by_kind']
    ex = g['excluded']
    names = ', '.join(f'`{n}`' for n in s['scenarios'])
    return '\n'.join([
        BEGIN,
        f"Generated by `tools/re_status.py compute` on {s['at']} UTC; "
        "`--check` in the gate keeps it current.",
        '',
        '| Measure | Value | What it counts |',
        '|---|---|---|',
        f"| `.text` code recovered | **{_pct(r['bytes'], g['bytes'])}** "
        f"({r['bytes']:,} / {g['bytes']:,} bytes) | Byte-weighted, over the "
        f"{g['functions']:,} functions Ghidra lists minus {ex['seh_funclets']:,} "
        f"SEH funclets and {ex['thunks']:,} thunks. |",
        f"| Functions recovered | **{_pct(r['functions'], g['functions'])}** "
        f"({r['functions']:,} / {g['functions']:,}) | By count, same "
        "denominator; a function counts once its body is more than a stub (a TODO, an empty or assert(false) body); a one-line body with a return or an assignment counts. |",
        f"| Verified against the original | "
        f"**{_pct(v['functions'], g['functions'])}** ({v['functions']:,} / "
        f"{g['functions']:,}; {v['bytes']:,} bytes) | Recovered functions whose "
        f"evidence is an oracle case ({k['oracle']:,}), a replay checkpoint "
        f"({k['replay']:,}) or a Frida trace of our build against the original ({k['trace']:,}); the rest "
        "depend on a reading of the decompile. |",
        f"| Scenarios matched | {len(s['scenarios'])} | {names}: scenarios "
        "with recorded original state dumps (`scenarios/*.expect.json`), "
        "replayed identically at every checkpoint. |",
        END])


def _snapshot(repo):
    path = os.path.join(repo, 'docs', 're-status.json')
    try:
        with open(path, encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def _splice(text, block):
    """README text with the block in place; None when there is no block."""
    a, b = text.find(BEGIN), text.find(END)
    if a < 0 or b < a:
        return None
    return text[:a] + block + text[b + len(END):]


SECTION = '### Recovery status\n\n'


def _insert(text, block):
    i = text.find('### Roadmap')
    if i < 0:
        raise SystemExit('re_status: README.md has no "### Roadmap" heading '
                         'to put the Recovery status block before')
    return text[:i] + SECTION + block + '\n\n' + text[i:]


def main(argv):
    args = list(argv)
    opts = {'--repo': os.path.dirname(HERE), '--exports': None}
    for o in opts:
        if o in args:
            i = args.index(o)
            opts[o] = args[i + 1]
            del args[i:i + 2]
    repo, exports = opts['--repo'], opts['--exports']
    if len(args) != 1 or args[0] not in ('compute', '--check', '--markdown'):
        print(__doc__.split('\n\n')[0], file=sys.stderr)
        return 2
    cmd = args[0]
    readme = os.path.join(repo, 'README.md')
    if cmd == 'compute':
        if exports is None:
            exports = _load('truth').exports_default()
        snap = compute(repo, exports)
        os.makedirs(os.path.join(repo, 'docs'), exist_ok=True)
        with open(os.path.join(repo, 'docs', 're-status.json'), 'w') as f:
            json.dump(snap, f, indent=2)
            f.write('\n')
        with open(readme, encoding='utf-8') as f:
            text = f.read()
        block = render(snap)
        new = _splice(text, block) or _insert(text, block)
        with open(readme, 'w', encoding='utf-8') as f:
            f.write(new)
        print(block)
        return 0
    snap = _snapshot(repo)
    if snap is None:
        print('docs/re-status.json is missing or unreadable; run '
              'python3 -I tools/re_status.py compute', file=sys.stderr)
        return 1
    if cmd == '--markdown':
        print(render(snap))
        return 0
    try:
        with open(readme, encoding='utf-8') as f:
            text = f.read()
    except OSError:
        text = ''
    if _splice(text, render(snap)) != text:
        print('README.md: the Recovery status block is stale or missing; run '
              'python3 -I tools/re_status.py compute', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
