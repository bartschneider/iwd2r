"""Write the parity exports: <exports>/functions.tsv and <exports>/callgraph.tsv.

    python3 -I tools/ghidra/export_callgraph.py [--exports DIR]

One read-only headless run of ExportCallGraph.java on the iwd2 project: the
whole-program function table (address, name, size) and its direct call graph,
the two files tools/parity/run.py reads. The exports dir is
$IWD2R_HOME/ghidra-exports by default and is rewritten in place, so a run
after a re-labelled project picks the new names up.

Ghidra is memory-hungry; run it as
    systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=0 -- \\
        python3 -I tools/ghidra/export_callgraph.py
"""

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import make_project as mp  # noqa: E402

SCRIPT = 'ExportCallGraph.java'
FILES = ('functions.tsv', 'callgraph.tsv')


def export(exports, log=None, out=sys.stdout):
    """Run ExportCallGraph.java read-only; returns the two written paths."""
    os.makedirs(exports, exist_ok=True)
    log = log or os.path.join(mp.PROJECT_DIR,
                              'headless-export-callgraph.log')
    r = mp.headless(['-process', mp.PROGRAM, '-noanalysis', '-readOnly',
                     '-postScript', SCRIPT, exports], log, read_only=True)
    if r.returncode != 0 or 'ERROR REPORT' in r.stdout:
        sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
        sys.exit(f'export_callgraph: analyzeHeadless failed ({r.returncode})')
    # The script prints its own counts: "EXPORTCALLGRAPH  N functions  M edges".
    for line in r.stdout.splitlines():
        if 'EXPORTCALLGRAPH' in line:
            text = line.split('EXPORTCALLGRAPH', 1)[1].strip()
            print(text.removesuffix('(GhidraScript)').strip(), file=out)
            break
    paths = [os.path.join(exports, name) for name in FILES]
    for path in paths:
        # An empty file reads as "no functions", which parity would believe.
        if not os.path.isfile(path) or os.path.getsize(path) == 0:
            sys.exit(f'export_callgraph: {path} was not written')
    return paths


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--exports', default=os.path.join(mp.HOME,
                                                      'ghidra-exports'),
                    help='the ghidra-exports dir run.py reads '
                         '(default: %(default)s)')
    a = ap.parse_args(argv)
    if not mp.project_exists():
        sys.exit('export_callgraph: no project; run make_project.py')
    paths = export(a.exports)
    print('wrote ' + ' and '.join(paths))
    return 0


if __name__ == '__main__':
    sys.exit(main())
