"""Look up names or addresses in the vanilla iwd2.exe Ghidra project.

    python3 tools/ghidra/query.py 0x5845D0 IcewindMisc::Roll

Prints one line per match: query, address, full name, function=yes|no.
The project is opened read-only (make_project.py creates it).
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_project as mp  # noqa: E402


def main(argv=None):
    queries = sys.argv[1:] if argv is None else argv
    if not queries:
        sys.exit(__doc__.strip())
    if not mp.project_exists():
        sys.exit('query: no project; run tools/ghidra/make_project.py')
    log = os.path.join(mp.PROJECT_DIR, 'headless-query.log')
    r = mp.headless(['-process', mp.PROGRAM, '-noanalysis', '-readOnly',
                     '-postScript', 'QuerySymbol.java', *queries], log,
                    read_only=True)
    found = [l.split('QUERY ', 1)[1].rstrip().removesuffix('(GhidraScript)')
             .rstrip() for l in r.stdout.splitlines() if 'QUERY ' in l]
    if r.returncode != 0 or not found:
        sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
        sys.exit(f'query: analyzeHeadless failed ({r.returncode})')
    for line in found:
        print(line)
    return 0


if __name__ == '__main__':
    sys.exit(main())
