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
`thunk_` thunks. Recovered: the address has a // 0xADDR marker on a function
whose body in src is not a stub (tools/xrec.py stubby). Verified: recovered
and the evidence file's kind is oracle, replay or trace. A scenario counts as
matched when scenarios/<name>.txt has a <name>.expect.json beside it, the
checkpoints of the original's state dumps (scenarios/pending/ is not counted).
"""
import datetime
import glob
import importlib.util
import json
import os
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


def recovered_addresses(repo):
    xrec = _load('xrec')
    out = set()
    for path in sorted(glob.glob(os.path.join(repo, 'src', '**', '*.cpp'),
                                 recursive=True)):
        with open(path, encoding='utf-8', errors='replace') as f:
            text = f.read()
        for address, body in xrec.funcs(text).values():
            if address and not xrec.stubby(body):
                out.add(_norm(address))
    return out


def evidence_kinds(repo):
    ce = _load('check_evidence')
    out = {}
    for path in glob.glob(os.path.join(repo, 'docs', 'evidence', '*.md')):
        with open(path, encoding='utf-8', errors='replace') as f:
            kind = ce.front_matter(f.read()).get('kind')
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
    rec = recovered_addresses(repo)
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
        f"Generated by `tools/re_status.py compute` on {s['at']}; "
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
        "denominator; a function counts once its body is more than a stub. |",
        f"| Verified against the original | "
        f"**{_pct(v['functions'], g['functions'])}** ({v['functions']:,} / "
        f"{g['functions']:,}; {v['bytes']:,} bytes) | Recovered functions whose "
        f"evidence is an oracle case ({k['oracle']:,}), a replay checkpoint "
        f"({k['replay']:,}) or a Frida trace ({k['trace']:,}); the rest rest "
        "on a reading of the decompile. |",
        f"| Scenarios matched | {len(s['scenarios'])} | {names}: replayed "
        "identically to the original at every checkpoint. |",
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
