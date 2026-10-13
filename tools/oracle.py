# iwd2r. Distributed under LICENSE.md.
"""Compare iwd2r with the original iwd2.exe and record decompiler evidence.

Usage (from WSL, in the repository):

  python3 tools/oracle.py evidence 0x5845D0
  python3 tools/oracle.py evidence IcewindMisc::Roll

evidence writes a draft docs/evidence/<ADDR>.md for one function of the
vanilla exe: the front matter (address, name, an empty oracle_case,
ghidra_version), Ghidra's decompilation, the callers and the callees. It reads
the labelled Ghidra project that tools/ghidra/make_project.py creates, opened
read-only through tools/ghidra/DecompileToEvidence.java. An existing evidence
file is kept unless --force is given. A name shared by several functions
(overloads) lists them and asks for an address. Fill in oracle_case or
exemption and the Notes by hand (docs/evidence/README.md).

Standard library only.
"""

import argparse
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, 'ghidra'))
sys.path.insert(0, HERE)
import check_evidence  # noqa: E402

EVIDENCE_DIR = os.path.join(ROOT, 'docs', 'evidence')
VERSIONS = os.path.join(HERE, 'ghidra', 'versions.json')


def ghidra_version():
    """The pinned Ghidra version, such as 12.1.4."""
    with open(VERSIONS) as f:
        tag = json.load(f)['ghidra']['tag']
    return tag.removeprefix('Ghidra_').removesuffix('_build')


def parse_decompile(text):
    """Parse DecompileToEvidence.java's output file.

    Returns {'matches': [(addr, name)], 'function': (address, name) or None,
    'callers': [(addr, name, from)], 'callees': [(addr, name)], 'code': str}.
    Addresses other than the function's stay strings as the script wrote
    them."""
    result = {'matches': [], 'function': None, 'callers': [], 'callees': [],
              'code': ''}
    lines = text.splitlines()
    for i, line in enumerate(lines):
        kind, _, rest = line.partition('\t')
        fields = rest.split('\t')
        if kind == 'code':
            result['code'] = '\n'.join(lines[i + 1:]).strip('\n')
            break
        if kind == 'match':
            result['matches'].append((fields[0], fields[1]))
        elif kind == 'function':
            result['function'] = (int(fields[0], 16), fields[1])
        elif kind == 'caller':
            result['callers'].append((fields[0], fields[1], fields[2]))
        elif kind == 'callee':
            result['callees'].append((fields[0], fields[1]))
    return result


def render_evidence(parsed, version):
    """The Markdown draft for a parsed function."""
    address, name = parsed['function']
    out = ['---',
           f'address: 0x{address:X}',
           f'name: {name}',
           'oracle_case:',
           f'ghidra_version: {version}',
           '---',
           '',
           f'# 0x{address:X} {name}',
           '',
           '## Decompiled (Ghidra)',
           '',
           '```c',
           parsed['code'],
           '```',
           '',
           '## Callers',
           '']
    if parsed['callers']:
        for addr, caller, site in parsed['callers']:
            if addr == '<none>':
                out.append(f'- {site} (not in a function)')
            else:
                out.append(f'- {addr} {caller} (call at {site})')
    else:
        out.append('None.')
    out += ['', '## Callees', '']
    if parsed['callees']:
        out += [f'- {addr} {callee}' for addr, callee in parsed['callees']]
    else:
        out.append('None.')
    out += ['', '## Notes', '']
    return '\n'.join(out)


def decompile(query):
    """Run DecompileToEvidence.java on the Ghidra project for query."""
    import make_project as mp
    if not mp.project_exists():
        sys.exit('oracle: no Ghidra project; run tools/ghidra/make_project.py')
    log = os.path.join(mp.PROJECT_DIR, 'headless-evidence.log')
    with tempfile.TemporaryDirectory(prefix='iwd2r-evidence-') as tmp:
        out = os.path.join(tmp, 'evidence.txt')
        r = mp.headless(['-process', mp.PROGRAM, '-noanalysis', '-readOnly',
                         '-postScript', 'DecompileToEvidence.java', query,
                         out], log)
        if (r.returncode != 0 or 'ERROR REPORT' in r.stdout
                or not os.path.exists(out)):
            sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
            sys.exit(f'oracle: analyzeHeadless failed ({r.returncode})')
        with open(out) as f:
            return f.read()


def cmd_evidence(args):
    parsed = parse_decompile(decompile(args.function))
    if len(parsed['matches']) > 1:
        print(f'{args.function} names {len(parsed["matches"])} functions; '
              'pass one address:')
        for addr, name in parsed['matches']:
            print(f'  {addr} {name}')
        return 1
    if parsed['function'] is None:
        print(f'oracle: no function {args.function} in the Ghidra project')
        return 1
    address, name = parsed['function']
    path = os.path.join(EVIDENCE_DIR, check_evidence.evidence_name(address))
    rel = os.path.relpath(path, ROOT)
    if os.path.exists(path) and not args.force:
        print(f'oracle: {rel} exists; --force replaces it')
        return 1
    os.makedirs(EVIDENCE_DIR, exist_ok=True)
    with open(path, 'w') as f:
        f.write(render_evidence(parsed, ghidra_version()))
    print(f'wrote {rel} (0x{address:X} {name}); fill in oracle_case or '
          'exemption and the Notes')
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='command', required=True)
    ev = sub.add_parser('evidence', help='draft docs/evidence/<ADDR>.md')
    ev.add_argument('function', help='address (0x5845D0) or full name '
                    '(IcewindMisc::Roll)')
    ev.add_argument('--force', action='store_true',
                    help='replace an existing evidence file')
    ev.set_defaults(run=cmd_evidence)
    args = ap.parse_args(argv)
    return args.run(args)


if __name__ == '__main__':
    sys.exit(main())
