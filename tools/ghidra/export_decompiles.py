"""Export one decompile per function: <exports>/decompile/<ADDR>.c.

    python3 -I tools/ghidra/export_decompiles.py [--exports DIR] [--chunk N]
                                                 [--only ADDR...]

The function list is functions.tsv under the exports dir ($IWD2R_HOME/
ghidra-exports by default). Addresses without a file yet are decompiled in
chunks of N (default 400), each chunk one read-only headless run of
DecompileFunctions.java, so an interrupted export resumes where it stopped.
ADDR is uppercase hex without 0x, as evidence files are named. The script
writes <name>.c, so a chunk never holds two functions of one name (the
repeats go to a later run in the same chunk); files are renamed to ADDR.c.
A function Ghidra could not decompile gets a file whose first line is
"// decompilation failed"; a function it did not report at all is listed in
the log and retried by the next run.

Ghidra is memory-hungry; run it as
    systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- \\
        python3 -I tools/ghidra/export_decompiles.py
"""

import argparse
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import make_project as mp  # noqa: E402

SCRIPT = 'DecompileFunctions.java'


def _exports_default():
    home = (os.environ.get('IWD2R_HOME')
            or os.path.join(os.path.expanduser('~'), '.local', 'share',
                            'iwd2r'))
    return os.path.join(home, 'ghidra-exports')


def read_functions(exports):
    """functions.tsv -> [(addr, name)] in file order."""
    out = []
    with open(os.path.join(exports, 'functions.tsv'), encoding='utf-8',
              errors='replace') as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 2:
                continue
            try:
                out.append((int(parts[0], 16), parts[1]))
            except ValueError:
                continue
    return out


def file_name(addr):
    return f'{addr:X}.c'


def _runs(chunk):
    """Split [(addr, name)] into runs whose names are all distinct."""
    runs = []
    for addr, name in chunk:
        for run in runs:
            if name not in run['names']:
                break
        else:
            run = {'names': set(), 'items': []}
            runs.append(run)
        run['names'].add(name)
        run['items'].append(addr)
    return [r['items'] for r in runs]


def run_batch(addrs, out_dir, tmp, log):
    """One headless run; moves its output to <out_dir>/ADDR.c. Returns the
    addresses written."""
    shutil.rmtree(tmp, ignore_errors=True)
    os.makedirs(tmp)
    args = ['-process', mp.PROGRAM, '-noanalysis', '-readOnly',
            '-postScript', SCRIPT, tmp] + [f'0x{a:X}' for a in addrs]
    r = mp.headless(args, log, read_only=True)
    done = []
    for line in r.stdout.splitlines():
        if 'DECOMPILE ' not in line:
            continue
        text = line.split('DECOMPILE ', 1)[1].rstrip()
        text = text.removesuffix('(GhidraScript)').rstrip()
        parts = text.split('\t')
        if len(parts) < 4:
            continue
        try:
            addr = int(parts[1], 16)
        except ValueError:
            continue
        src = os.path.join(tmp, os.path.basename(parts[3]))
        if addr in addrs and os.path.isfile(src):
            os.replace(src, os.path.join(out_dir, file_name(addr)))
            done.append(addr)
    shutil.rmtree(tmp, ignore_errors=True)
    if r.returncode != 0:
        sys.stderr.write(f'export_decompiles: headless exit {r.returncode}'
                         f'\n{r.stdout[-1500:]}{r.stderr[-1500:]}\n')
    return done


def export(exports, chunk=400, only=None, out=sys.stdout):
    """Returns (runs, written, missing addresses)."""
    out_dir = os.path.join(exports, 'decompile')
    os.makedirs(out_dir, exist_ok=True)
    functions = read_functions(exports)
    if only:
        wanted = {int(a, 16) for a in only}
        names = dict(functions)
        functions = [(a, names.get(a, f'FUN_{a:08X}')) for a in sorted(wanted)]
        pending = functions
    else:
        pending = [(a, n) for a, n in functions
                   if not os.path.isfile(os.path.join(out_dir, file_name(a)))]
    log = os.path.join(mp.PROJECT_DIR, 'headless-decompiles.log')
    tmp = os.path.join(out_dir, '.tmp')
    runs = written = 0
    missing = []
    total = (len(pending) + chunk - 1) // chunk
    for i in range(0, len(pending), chunk):
        part = pending[i:i + chunk]
        t0 = time.time()
        got = 0
        for batch in _runs(part):
            runs += 1
            done = run_batch(set(batch), out_dir, tmp, log)
            got += len(done)
            missing += [a for a in batch if a not in set(done)]
        written += got
        print(f'chunk {i // chunk + 1}/{total}: {got}/{len(part)} written, '
              f'{time.time() - t0:.0f}s, {written} so far', file=out,
              flush=True)
    return runs, written, missing


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--exports', default=_exports_default())
    ap.add_argument('--chunk', type=int, default=400)
    ap.add_argument('--only', nargs='+', metavar='ADDR',
                    help='only these addresses (hex), re-exported')
    a = ap.parse_args(argv)
    if not a.only and not mp.project_exists():
        sys.exit('export_decompiles: no project; run make_project.py')
    runs, written, missing = export(a.exports, a.chunk, a.only)
    print(f'{runs} runs, {written} written, {len(missing)} not reported')
    for addr in missing:
        print(f'not reported: {addr:X}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
