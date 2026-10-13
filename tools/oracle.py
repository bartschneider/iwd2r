"""Compare iwd2r with the original iwd2.exe and record decompiler evidence.

Usage (from WSL, in the repository):

  python3 tools/oracle.py evidence 0x5845D0
  python3 tools/oracle.py evidence IcewindMisc::Roll
  python3 tools/oracle.py build
  python3 tools/oracle.py run --selftest
  python3 tools/oracle.py run [--file selftest] [--seed N] [--iterations N]

evidence writes a draft docs/evidence/<ADDR>.md for one function of the
vanilla exe: the front matter (address, name, an empty oracle_case,
ghidra_version), Ghidra's decompilation, the callers and the callees. It reads
the labelled Ghidra project that tools/ghidra/make_project.py creates, opened
read-only through tools/ghidra/DecompileToEvidence.java. An existing evidence
file is kept unless --force is given. A name shared by several functions
(overloads) lists them and asks for an address. Fill in oracle_case or
exemption and the Notes by hand (docs/evidence/README.md).

build syncs the checkout to the Windows build folder (play.py's route) and
builds the 32-bit runner iwd2r_oracle with the windows-msvc-x86 preset.

run copies the vanilla iwd2.exe (MD5 from oracle/loader/vanilla.json) to
C:/Users/thier/iwd2r-oracle and runs the runner on it.

Without --selftest it runs comparison cases: the case file named by --file,
or every case file the runner lists, one fresh runner process per file. Each
process writes <file>.json under C:/Users/thier/iwd2r-oracle/out/<stamp>;
they are merged into runs/<stamp>/oracle.json and summarised. A case declared
to fail (selftest/must_fail) is listed under expected_failures and one
declared to crash (selftest/crash) under expected_crashes. The run fails
(exit 1) if any case does not do what it declared, or a runner process ends
without its report.

--selftest checks the loader itself in six runs: the self-test, whose image
bytes and rand() value are checked against the exe file read here; a run with
a wrong WinMain call site and one with a wrong expected WinMain target, both
of which must be refused; a run that hangs, which the runner's --timeout must
kill (exit 10); a run with 0x400000 taken first, which must name the
occupant; and a run on a file that is not the vanilla exe, which must be
refused by MD5. It exits 1 if any run fails.

Each case file's runner gets --timeout 600: the runner itself kills its child
process (through its job) and exits 10, reported as "timed out". Reports are
rewritten after every case, so a killed run keeps its finished cases.

Standard library only.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, 'ghidra'))
sys.path.insert(0, HERE)
import check_evidence  # noqa: E402

EVIDENCE_DIR = os.path.join(ROOT, 'docs', 'evidence')
VERSIONS = os.path.join(HERE, 'ghidra', 'versions.json')
VANILLA_JSON = os.path.join(ROOT, 'oracle', 'loader', 'vanilla.json')
VANILLA_EXE = os.path.expanduser('~/.local/share/iwd2r/vanilla/iwd2.exe')
ORACLE_DIR = '/mnt/c/Users/thier/iwd2r-oracle'
ORACLE_PRESET = 'windows-msvc-x86'
RUNNER_REL = 'build/windows-msvc-x86/oracle/RelWithDebInfo/iwd2r_oracle.exe'
SELFTEST_RAND = 41  # VC6 rand() after srand(1)
RUNS_DIR = os.path.join(ROOT, 'runs')
FILE_TIMEOUT = 600  # seconds for one case file, enforced by the runner
EXIT_TIMEOUT = 10  # the runner's exit code when --timeout ran out
HEAP_CORRUPT = 'heap corrupt after case'


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


def vanilla_md5():
    with open(VANILLA_JSON) as f:
        return json.load(f)['md5']


def file_md5(path):
    with open(path, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()


def pe_sections(data):
    """(image base, [(rva, virtual size, raw offset, raw size)]) of a PE32."""
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count, opt_size = struct.unpack_from('<H', data, pe + 6)[0], \
        struct.unpack_from('<H', data, pe + 20)[0]
    base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
    table = pe + 24 + opt_size
    sections = [struct.unpack_from('<IIII', data, table + 40 * i + 8)
                for i in range(count)]
    return base, [(rva, vsize, off, raw) for vsize, rva, raw, off in sections]


def file_offset(data, address):
    """The file offset of a virtual address, or None outside raw data."""
    base, sections = pe_sections(data)
    rva = address - base
    for sec_rva, vsize, off, raw in sections:
        if sec_rva <= rva < sec_rva + min(raw, vsize or raw):
            return off + rva - sec_rva
    return None


def file_dword(data, address):
    off = file_offset(data, address)
    return None if off is None else struct.unpack_from('<I', data, off)[0]


def check_selftest(stdout, data):
    """Check the runner's --selftest lines against the exe file bytes.

    Returns a list of (ok, message). `mem A V` must equal the file's dword at
    A, unless an `import A dll!name P` line marks A as an IAT slot: then V
    must be P (nonzero), and the file's slot must name the same function."""
    mems, imports, rand = {}, {}, None
    for line in stdout.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[0] == 'mem':
            mems[int(fields[1], 16)] = int(fields[2], 16)
        elif len(fields) == 4 and fields[0] == 'import':
            imports[int(fields[1], 16)] = (fields[2], int(fields[3], 16))
        elif len(fields) == 2 and fields[0] == 'rand':
            rand = int(fields[1])
    results = []
    if 0x847000 not in mems:
        results.append((False, 'no mem line for 0x00847000 (.rdata start)'))
    for address, value in sorted(mems.items()):
        where = '0x%08X' % address
        if address in imports:
            name, resolved = imports[address]
            hint_rva = file_dword(data, address)
            off = None if hint_rva is None or hint_rva & 0x80000000 else \
                file_offset(data, pe_sections(data)[0] + hint_rva + 2)
            file_name = data[off:data.index(b'\0', off)].decode('ascii') \
                if off is not None else '#%d' % ((hint_rva or 0) & 0xFFFF)
            ok = (value == resolved and resolved != 0
                  and name.split('!', 1)[-1] == file_name)
            results.append((ok, '%s IAT slot %s = %08x (GetProcAddress %08x, '
                            'file names %s)' % (where, name, value, resolved,
                                                file_name)))
        else:
            expected = file_dword(data, address)
            results.append((value == expected,
                            '%s = %08x (file %s)' % (
                                where, value, 'none' if expected is None
                                else '%08x' % expected)))
    results.append((rand == SELFTEST_RAND,
                     'rand() after srand(1) = %s (expected %d)'
                     % (rand, SELFTEST_RAND)))
    return results


# The --selftest runs: (name, input, runner flags, expected exit code, a
# regular expression stderr must match or None). input is 'vanilla' for the
# staged iwd2.exe or 'runner' to feed the runner its own exe (wrong MD5).
SELFTEST_RUNS = (
    ('selftest', 'vanilla', ['--selftest'], 0, None),
    ('wrong WinMain call site', 'vanilla',
     ['--selftest', '--test-bad-callsite'], 6,
     r'refusing to patch the WinMain call: byte at 0x007E9B44 is 0x[0-9A-F]{2}'
     r', not a call'),
    ('wrong WinMain target', 'vanilla',
     ['--selftest', '--test-bad-target'], 6,
     r'refusing to patch the WinMain call: call at 0x007E9B43 targets '
     r'0x004218F0, expected WinMain at 0x00421900'),
    ('timeout', 'vanilla',
     ['--selftest', '--test-hang', '--timeout', '3'], EXIT_TIMEOUT,
     r'timed out after 3 s; the runner child was killed'),
    ('range pre-occupied', 'vanilla', ['--selftest', '--test-occupy'], 4,
     r'occupied by private memory at 0x00400000-'),
    ('wrong MD5', 'runner', ['--selftest'], 5,
     r'has MD5 [0-9a-f]{32}; the oracle needs the vanilla iwd2\.exe'),
)


def evaluate_run(expected_code, pattern, code, stderr):
    """(ok, reason) for one runner exit."""
    if code != expected_code:
        return False, 'exit code %d, expected %d' % (code, expected_code)
    if pattern and not re.search(pattern, stderr):
        return False, 'stderr does not match %r' % pattern
    return True, 'exit code %d' % code


def win_path(path):
    return subprocess.run(['wslpath', '-w', path], check=True,
                          capture_output=True, text=True).stdout.strip()


def stage_vanilla(source):
    """Copy the vanilla exe to ORACLE_DIR after checking its MD5."""
    if not os.path.isfile(source):
        sys.exit('oracle: no vanilla exe at %s' % source)
    md5 = vanilla_md5()
    if file_md5(source) != md5:
        sys.exit('oracle: %s is not the vanilla iwd2.exe (MD5 %s)'
                 % (source, md5))
    os.makedirs(ORACLE_DIR, exist_ok=True)
    target = os.path.join(ORACLE_DIR, 'iwd2.exe')
    if not os.path.isfile(target) or file_md5(target) != md5:
        shutil.copyfile(source, target)
    return target


def cmd_build(args):
    import play
    play.sync_windows()
    exe = play.build_windows(preset=ORACLE_PRESET, target='iwd2r_oracle',
                             exe_rel=RUNNER_REL, bat_name='oracle-build.bat')
    print('built %s' % win_path(exe))
    return 0


def case_ok(case):
    """Whether a case did what it declared (its "expect")."""
    expect = case.get('expect', 'pass')
    if expect == 'fail':
        return not case['crashed'] and case['failure_count'] > 0
    if expect == 'crash':
        crash = case.get('crash') or {}
        return (case['crashed'] and case['failure_count'] == 0
                and crash.get('kind') != HEAP_CORRUPT)
    return not case['crashed'] and case['failure_count'] == 0


def merge_reports(seed, iterations, runs):
    """Merge the runner's per-file reports into oracle.json.

    runs is [(file, exit code or None when the process had to be killed
    here, report dict or None, the status the runner printed when its child
    died of an exception, or None)]. A file is ok when its report is complete,
    every case did what it declared, and the exit code agrees (0 when all
    did, 7 when not). The cases of an incomplete report (a process killed or
    dead part way) are still merged."""
    merged = {'seed': seed, 'iterations': iterations, 'files': [],
              'cases': [], 'expected_failures': [], 'expected_crashes': [],
              'unexpected': [], 'passed': True}
    for name, code, report, died in runs:
        entry = {'file': name, 'exit_code': code}
        if code is None or code == EXIT_TIMEOUT:
            ended = 'timed out'
        elif died:
            ended = 'the process died with %s' % died
        else:
            ended = 'exit code %s' % code
        if report is None:
            entry['ok'] = False
            entry['error'] = 'no report (%s)' % ended
            merged['unexpected'].append(name)
        else:
            all_ok = True
            for case in report['cases']:
                case = dict(case, file=name)
                merged['cases'].append(case)
                if not case_ok(case):
                    all_ok = False
                    merged['unexpected'].append(case['case'])
                elif case['expect'] == 'fail':
                    merged['expected_failures'].append(case['case'])
                elif case['expect'] == 'crash':
                    merged['expected_crashes'].append(case['case'])
            complete = report.get('complete', False)
            if not complete and report.get('running'):
                # The case the process was in when it died or was killed.
                case = {'case': report['running'], 'file': name,
                        'expect': '?', 'iterations': 0, 'passed': False,
                        'failure_count': 0, 'failures': [], 'crashed': True,
                        'crash': {'inputs': '', 'exception': died or '?',
                                  'address': '?', 'kind': 'process ended: '
                                  + ended, 'type': ''}}
                merged['cases'].append(case)
                merged['unexpected'].append(case['case'])
            entry['ok'] = complete and all_ok and code == (0 if all_ok else 7)
            if not complete:
                entry['error'] = 'incomplete report (%s)' % ended
                merged['unexpected'].append(name)
            elif all_ok and not entry['ok']:
                entry['error'] = '%s with every case as declared' % ended
                merged['unexpected'].append(name)
        merged['files'].append(entry)
        merged['passed'] = merged['passed'] and entry['ok']
    return merged


def summary_lines(merged):
    """The readable summary of a merged report."""
    lines = []
    for case in merged['cases']:
        if case['crashed']:
            state = 'crashed'
        elif case['failure_count']:
            state = 'failed'
        else:
            state = 'passed'
        if case_ok(case):
            state += ' (expected)' if case['expect'] != 'pass' else ''
        else:
            state = state.upper() + (' (expected %s)' % case['expect']
                                     if case['expect'] != 'pass' else '')
        lines.append('%-32s %s, %d inputs, %d failures' % (
            case['case'], state, case['iterations'], case['failure_count']))
        if not case_ok(case) or case['expect'] != 'pass':
            for f in case['failures'][:3]:
                lines.append('    seed %d  inputs %s\n      original %s\n'
                             '      ours     %s' % (merged['seed'], f['inputs'],
                                                    f['original'], f['ours']))
            crash = case['crash']
            if crash:
                kind = ' '.join(x for x in (crash.get('kind'),
                                            crash.get('type')) if x)
                lines.append('    seed %d  inputs %s\n      %s: exception %s '
                             'at %s' % (merged['seed'], crash['inputs'],
                                        kind or 'crash', crash['exception'],
                                        crash['address']))
    for entry in merged['files']:
        if entry.get('error'):
            lines.append('file %s: %s' % (entry['file'], entry['error']))
    counts = {'pass': 0, 'expected': 0}
    for case in merged['cases']:
        if case_ok(case):
            counts['pass' if case['expect'] == 'pass' else 'expected'] += 1
    lines.append('%d passed, %d expected failures or crashes, %d unexpected; '
                 'run %s' % (counts['pass'], counts['expected'],
                             len(merged['unexpected']),
                             'PASSED' if merged['passed'] else 'FAILED'))
    return lines


def runner_path():
    import play
    runner = os.path.join(play.WIN_CHECKOUT, RUNNER_REL)
    if not os.path.isfile(runner):
        sys.exit('oracle: no %s; run tools/oracle.py build' % runner)
    return runner


def read_report(path):
    """A runner report, or None if it is missing or not valid JSON."""
    try:
        with open(path) as f:
            report = json.load(f)
    except (OSError, ValueError):
        return None
    return report if isinstance(report, dict) and \
        isinstance(report.get('cases'), list) else None


def run_cases(args):
    import play
    runner = runner_path()
    exe = stage_vanilla(args.vanilla)
    if args.file:
        files = [args.file]
    else:
        r = subprocess.run([runner, '--list'], capture_output=True, text=True,
                           timeout=60)
        if r.returncode != 0:
            sys.exit('oracle: %s --list failed: %s' % (runner, r.stderr))
        files = r.stdout.split()
    run_dir = play.make_run_folder(RUNS_DIR, datetime.datetime.now())
    out_dir = os.path.join(ORACLE_DIR, 'out', os.path.basename(run_dir))
    os.makedirs(out_dir)
    runs = []
    try:
        for name in files:
            runs.append(run_case_file(runner, exe, name, args, out_dir))
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    merged = merge_reports(args.seed, args.iterations, runs)
    with open(os.path.join(run_dir, 'oracle.json'), 'w') as f:
        json.dump(merged, f, indent=2)
        f.write('\n')
    for line in summary_lines(merged):
        print(line)
    print('report: %s' % os.path.relpath(os.path.join(run_dir, 'oracle.json'),
                                         ROOT))
    return 0 if merged['passed'] else 1


def run_case_file(runner, exe, name, args, out_dir):
    """Run one case file in a fresh runner; (name, exit code, report)."""
    command = [runner, win_path(exe), '--file', name,
               '--seed', str(args.seed), '--iterations', str(args.iterations),
               '--out', win_path(out_dir), '--timeout', str(FILE_TIMEOUT)]
    print('== %s' % name)
    try:
        # The runner kills its own child at FILE_TIMEOUT; this is a backstop.
        r = subprocess.run(command, cwd=ORACLE_DIR, capture_output=True,
                           text=True, timeout=FILE_TIMEOUT + 60)
        code, stdout, stderr = r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired as e:
        code = None
        stdout = e.stdout.decode() if isinstance(e.stdout, bytes) else \
            (e.stdout or '')
        stderr = e.stderr.decode() if isinstance(e.stderr, bytes) else \
            (e.stderr or '')
    for line in stdout.splitlines():
        print('  %s' % line)
    for line in stderr.splitlines():
        print('  stderr: %s' % line)
    died = re.search(r'child died with exception (0x[0-9A-F]{8} \([^)]*\))',
                     stderr)
    return (name, code, read_report(os.path.join(out_dir, name + '.json')),
            died.group(1) if died else None)


def cmd_run(args):
    if not args.selftest:
        return run_cases(args)
    if args.file:
        sys.exit('oracle: --selftest and --file do not go together')
    runner = runner_path()
    exe = stage_vanilla(args.vanilla)
    with open(exe, 'rb') as f:
        data = f.read()
    failed = 0
    for name, source, flags, expected_code, pattern in SELFTEST_RUNS:
        target = win_path(exe if source == 'vanilla' else runner)
        r = subprocess.run([runner, target, *flags], cwd=ORACLE_DIR,
                           capture_output=True, text=True, timeout=120)
        ok, reason = evaluate_run(expected_code, pattern, r.returncode,
                                  r.stderr)
        print('%s: %s (%s)' % (name, 'PASS' if ok else 'FAIL', reason))
        for line in r.stderr.splitlines():
            print('  stderr: %s' % line)
        if expected_code == 0:
            for line in r.stdout.splitlines():
                print('  stdout: %s' % line)
            for check_ok, message in check_selftest(r.stdout, data):
                print('  %s %s' % ('ok  ' if check_ok else 'FAIL', message))
                ok = ok and check_ok
        failed += not ok
    print('%d of %d runs passed' % (len(SELFTEST_RUNS) - failed,
                                    len(SELFTEST_RUNS)))
    return 1 if failed else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='command', required=True)
    ev = sub.add_parser('evidence', help='draft docs/evidence/<ADDR>.md')
    ev.add_argument('function', help='address (0x5845D0) or full name '
                    '(IcewindMisc::Roll)')
    ev.add_argument('--force', action='store_true',
                    help='replace an existing evidence file')
    ev.set_defaults(run=cmd_evidence)
    build = sub.add_parser('build', help='build the 32-bit runner on Windows')
    build.set_defaults(run=cmd_build)
    run = sub.add_parser('run', help='run the runner on the vanilla exe')
    run.add_argument('--selftest', action='store_true',
                     help='check the loader: image bytes, rand(), refusals')
    run.add_argument('--file', help='run one case file (oracle/cases/<file>'
                     '.cpp); default every case file')
    run.add_argument('--seed', type=int, default=1,
                     help='seed for the original rand and the generators '
                     '(default %(default)s)')
    run.add_argument('--iterations', type=int, default=10000,
                     help='inputs per unbounded generator '
                     '(default %(default)s)')
    run.add_argument('--vanilla', default=VANILLA_EXE,
                     help='the vanilla iwd2.exe (default %(default)s)')
    run.set_defaults(run=cmd_run)
    args = ap.parse_args(argv)
    return args.run(args)


if __name__ == '__main__':
    sys.exit(main())
