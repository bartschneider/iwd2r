"""Run every structural parity check on the functions a change touches.

    python3 tools/parity/run.py --base iwd2r/main [--all] [--repo DIR]

A touched function is a labelled function (a `// 0xADDR` comment above a
definition) in src/**/*.cpp whose lines changed since --base -- the same rule
tools/check_evidence.py enforces evidence for. For each one this prints a row
of the table

    address | name | asm | vtable | layout | offsets | markers | lints

where the columns are:

    asm      the binary's callees (callgraph.tsv) are all named in the
             recovered body (asm_verify.py)
    vtable   the class's vtable overrides match the source declarations
             (vtable_audit.py)
    layout   the class's /* 0xNNN */ member offsets are laid out in order
             (struct_layout_audit.py; compiled spacing needs MSVC)
    offsets  every member the binary's thiscalls touch is named in the body
             (parity_offsets.py)
    markers  the marker is a function start in the export (address_markers.py)
    lints    re_lint + the six lint_* passes find nothing in the span

Verdicts: GREEN / YELLOW / RED, plus SKIP (check could not run -- no pefile,
no compiler dump) and WARNING (a RED a human waived with `parity_waiver` in
the evidence file; asm only, the fork's "a human confirms RED"). RED and
ERROR exit 1; everything else never fails the run.

Skips (exit 0, never a failure): no vanilla exe at oracle.VANILLA_EXE, a
base that cannot be diffed, or missing exports (run
tools/ghidra/ExportCallGraph.java once to produce them).

Needs no third-party packages for asm/markers/lints; vtable/layout/offsets
additionally use pefile and capstone (tools/parity/setup.sh).
"""
from __future__ import annotations

import argparse
import importlib
import os
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
ROOT = os.path.dirname(TOOLS)
if HERE not in sys.path:
    sys.path.insert(0, HERE)
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)

import address_markers  # noqa: E402
import asm_verify  # noqa: E402
import check_evidence  # noqa: E402
import oracle  # noqa: E402

GREEN, YELLOW, RED = 'GREEN', 'YELLOW', 'RED'
SKIP, WARNING, ERROR = 'SKIP', 'WARNING', 'ERROR'
FAILING = (RED, ERROR)

COLUMNS = ('asm', 'vtable', 'layout', 'offsets', 'markers', 'lints')
LINT_NAMES = ('re_lint', 'lint_extend_cells', 'lint_infinite_loop',
              'lint_pragma_pack_balance', 'lint_stuck_loop_index',
              'lint_twin_symmetry', 'lint_uninit_member')

# --exports override for the CLI (load_exports's dirpath argument).
EXPORTS_DIR = None

touched = check_evidence.touched


def exports_dir(dirpath=None):
    """The ghidra-exports dir: an explicit path, $IWD2R_HOME, or the default."""
    if dirpath:
        return dirpath
    home = os.environ.get('IWD2R_HOME') or os.path.expanduser(
        '~/.local/share/iwd2r')
    return os.path.join(home, 'ghidra-exports')


def load_exports(dirpath=None):
    """(names, sizes, callees) from functions.tsv + callgraph.tsv, else None.

    Both files are required: a call-graph-less run would let asm_verify read
    GREEN vacuously (no callees listed -> nothing missing), and silence is
    the one outcome this gate must not have.
    """
    d = exports_dir(dirpath if dirpath is not None else EXPORTS_DIR)
    functions = os.path.join(d, 'functions.tsv')
    callgraph = os.path.join(d, 'callgraph.tsv')
    if not (os.path.isfile(functions) and os.path.isfile(callgraph)):
        return None
    names, sizes = address_markers.read_functions(functions)
    callees = address_markers.read_callgraph(callgraph)
    return names, sizes, callees


def waiver(repo, address):
    """The evidence file's parity_waiver value ('' when absent or unwaived)."""
    path = os.path.join(repo, check_evidence.EVIDENCE_DIR,
                        check_evidence.evidence_name(address))
    try:
        with open(path, encoding='utf-8') as f:
            text = f.read()
    except OSError:
        return ''
    return str(check_evidence.front_matter(text).get('parity_waiver', '') or '')


# ---- the checks ------------------------------------------------------------
# Each takes (address, name, ctx) -> (verdict, notes); ctx carries the row's
# function span, the exports triple and the run-wide caches.

def asm_check(address, name, ctx):
    v, reason, detail = asm_verify.verify(address, ctx['span_text'],
                                          ctx['exports'])
    notes = [reason]
    by_name = {n: a for a, n in ctx['exports'][0].items()}
    for full in detail['missing']:
        where = by_name.get(full)
        notes.append('  missing in src: ' + full +
                     (f' (0x{where:X})' if where else ''))
    if v == RED:
        w = waiver(ctx['repo'], address)
        if w:
            return WARNING, notes + [f'  waived: {w}']
    return v, notes


def _module(name):
    try:
        return importlib.import_module(name)
    except ImportError:
        return None


def _delegated(module_name, column):
    """A check() on the exe-based module, SKIP when it cannot be imported."""
    def check(address, name, ctx):
        mod = _module(module_name)
        if mod is None:
            return SKIP, [f'{module_name} not importable '
                          '(pefile/capstone missing? tools/parity/setup.sh)']
        try:
            return mod.check(address, name, ctx['repo'], ctx['exports'],
                             ctx['exe'])
        except Exception as exc:  # a broken check must not kill the run
            return ERROR, [f'{column} check raised: {exc!r}']
    return check


def markers_check(address, name, ctx):
    names, sizes, _ = ctx['exports']
    marker = {'file': ctx['rel'], 'line': ctx['first'] + 1,
              'address': address, 'symbol': name}
    ok, inside, undefined = address_markers.audit(
        [marker], ctx['starts'], names, sizes, lambda a: a in ctx['targets'])
    if ok:
        return GREEN, ['marker is a function start in the export']
    if inside:
        f = inside[0]
        where = (f"inside 0x{f['owner']:X} {f['owner_name']}"
                 f"+0x{f['delta']:X}")
        if f['referenced'] is False:
            return RED, [f'nothing calls 0x{address:X}; {where}']
        if f['referenced'] is True:
            return YELLOW, [f'{where} but a call targets it '
                            '(the export merged two functions?)']
        return YELLOW, [f'{where}; no callgraph to judge reachability']
    return YELLOW, [f.get('detail', 'no function defined here')
                    for f in undefined] or ['no function defined here']


def lints_check(address, name, ctx):
    cache = ctx['lint_cache']
    rel = ctx['rel']
    if rel not in cache:
        full = os.path.join(ctx['repo'], rel)
        findings, failed = {}, []
        for lint_name, mod in ctx['lints']:
            try:
                findings[lint_name] = mod.scan(full)
            except Exception as exc:
                failed.append((lint_name, exc))
        cache[rel] = (findings, failed)
    findings, failed = cache[rel]
    lo, hi = ctx['first'] + 1, ctx['last'] + 1   # spans are 0-based; lints 1-based
    hits, notes = [], []
    for lint_name, found in sorted(findings.items()):
        for line, message in found:
            if line is None or lo <= line <= hi:
                hits.append((lint_name, line, message))
    notes.extend(f'{n}: ' + (f'L{l}: ' if l else '') + m
                 for n, l, m in hits)
    notes.extend(f'{n}: lint failed: {exc!r}' for n, exc in failed)
    if hits:
        return RED, notes
    if failed:
        return YELLOW, notes or ['lints could not run']
    return GREEN, ['no lint findings in this function']


def load_lints():
    lints = []
    for lint_name in LINT_NAMES:
        mod = _module(lint_name)
        if mod is not None:
            lints.append((lint_name, mod))
    return lints


# ---- assembling the work list ----------------------------------------------

def read_lines(repo, rel):
    with open(os.path.join(repo, rel), encoding='utf-8', errors='replace') as f:
        return f.read().splitlines()


def locate(lines, address):
    """(first, last) of the labelled function at address, or (None, None)."""
    for first, last, a, _ in check_evidence.spans(lines):
        if a == address:
            return first, last
    return None, None


def collect(repo, base_ref):
    """Work items for every labelled function changed since base_ref."""
    work = []
    for rel, address, name in sorted(touched(repo, base_ref)):
        try:
            lines = read_lines(repo, rel)
        except OSError:
            continue
        first, last = locate(lines, address)
        work.append((rel, address, name, lines, first, last))
    return work


def all_functions(repo):
    """Work items for every labelled function in src/ (--all)."""
    work = []
    src = os.path.join(repo, 'src')
    for root, dirs, files in os.walk(src):
        dirs[:] = sorted(d for d in dirs if d != '.git')
        for fname in sorted(files):
            if not fname.endswith('.cpp'):
                continue
            full = os.path.join(root, fname)
            rel = os.path.relpath(full, repo).replace(os.sep, '/')
            lines = read_lines(repo, rel)
            for first, last, address, name in check_evidence.spans(lines):
                work.append((rel, address, name, lines, first, last))
    return work


def run_row(item, repo, exports, exe, lints, lint_cache):
    rel, address, name, lines, first, last = item
    if first is None:
        row = {col: (SKIP, ['marker not in the working tree'])
               for col in COLUMNS}
        return address, name, row
    ctx = {
        'repo': repo, 'rel': rel, 'exports': exports, 'exe': exe,
        'starts': sorted(exports[0]),
        'targets': {c for cs in exports[2].values() for c in cs},
        'lints': lints, 'lint_cache': lint_cache,
        'span_text': '\n'.join(lines[first:last + 1]),
        'first': first, 'last': last,
    }
    row = {}
    for column, check in CHECKS:
        try:
            row[column] = check(address, name, ctx)
        except Exception as exc:
            row[column] = (ERROR, [f'{column} check raised: {exc!r}'])
    return address, name, row


CHECKS = (
    ('asm', asm_check),
    ('vtable', _delegated('vtable_audit', 'vtable')),
    ('layout', _delegated('struct_layout_audit', 'layout')),
    ('offsets', _delegated('parity_offsets', 'offsets')),
    ('markers', markers_check),
    ('lints', lints_check),
)


# ---- the table -------------------------------------------------------------

SHORT = {GREEN: 'green', YELLOW: 'yellow', RED: 'red', SKIP: 'skip',
         WARNING: 'warning', ERROR: 'error'}


def print_table(base_ref, exe, rows):
    widths = [len(c) for c in ('address', 'name', *COLUMNS)]
    cells = [[f'0x{a:X}', name] + [SHORT[v] for v, _ in row.values()]
             for a, name, row in rows]
    for c in cells:
        for i, text in enumerate(c):
            widths[i] = max(widths[i], len(text))
    header = ('address', 'name', *COLUMNS)
    print(f'structural parity: {len(rows)} function(s) since {base_ref} '
          f'(exe: {exe})')
    print('  '.join(h.ljust(widths[i]) for i, h in enumerate(header)))
    for c in cells:
        print('  '.join(t.ljust(widths[i]) for i, t in enumerate(c)))
    print()
    for (address, name, row), c in zip(rows, cells):
        for column, (v, notes) in row.items():
            if v in (GREEN, SKIP) or not notes:
                continue
            print(f'  {c[0]} {name} [{column}] {SHORT[v]}')
            for note in notes:
                print(f'    {note}')


def main(base_ref, changed_only=True, repo=None):
    repo = os.path.abspath(repo or ROOT)
    exe = oracle.VANILLA_EXE
    if not os.path.isfile(exe):
        print('structural parity: skipped (no vanilla exe)')
        return 0
    if not check_evidence.diffable(repo, base_ref):
        print(f'structural parity: skipped (cannot diff against {base_ref})')
        return 0
    exports = load_exports()
    if exports is None:
        print('structural parity: skipped (no functions.tsv/callgraph.tsv '
              f'under {exports_dir()}; run tools/ghidra/ExportCallGraph.java)')
        return 0

    work = collect(repo, base_ref) if changed_only else all_functions(repo)
    if not work:
        print(f'structural parity: 0 functions changed since {base_ref}')
        return 0

    lints = load_lints()
    lint_cache = {}
    rows = [run_row(item, repo, exports, exe, lints, lint_cache)
            for item in work]
    print_table(base_ref, exe, rows)

    counts = Counter(v for _, _, row in rows for v, _ in row.values())
    print(f"structural parity: {counts[RED]} RED, {counts[ERROR]} error, "
          f"{counts[YELLOW]} yellow, {counts[WARNING]} warning, "
          f"{counts[SKIP]} skipped, {counts[GREEN]} clean")
    return 1 if any(v in FAILING for _, _, row in rows
                    for v, _ in row.values()) else 0


def cli(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--base', required=True,
                    help='compare HEAD against this commit')
    ap.add_argument('--all', dest='changed_only', action='store_false',
                    help='every labelled function, not just changed ones')
    ap.add_argument('--repo', help='repository root (default: this repo)')
    ap.add_argument('--exports', help='ghidra-exports dir (default $IWD2R_HOME)')
    args = ap.parse_args(argv)
    global EXPORTS_DIR
    EXPORTS_DIR = args.exports
    return main(args.base, changed_only=args.changed_only, repo=args.repo)


if __name__ == '__main__':
    raise SystemExit(cli())
