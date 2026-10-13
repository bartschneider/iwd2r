"""Compare iwd2r with the original iwd2.exe and record decompiler evidence.

Usage (from WSL, in the repository):

  python3 tools/oracle.py evidence 0x5845D0
  python3 tools/oracle.py evidence IcewindMisc::Roll
  python3 tools/oracle.py build
  python3 tools/oracle.py run --selftest
  python3 tools/oracle.py run [--file selftest] [--seed N] [--iterations N]
  python3 tools/oracle.py vanilla [--rebuild]
  python3 tools/oracle.py replay record <scenario> [--seed N] [--timeout S]
                                        [--dump-every N]

evidence writes a draft docs/evidence/<ADDR>.md for one function of the
vanilla exe: the front matter (address, name, an empty oracle_case,
ghidra_version), Ghidra's decompilation, the callers and the callees. It reads
the labelled Ghidra project that tools/ghidra/make_project.py creates, opened
read-only through tools/ghidra/DecompileToEvidence.java. An existing evidence
file is kept unless --force is given. A name shared by several functions
(overloads) lists them and asks for an address. Fill in oracle_case or
exemption and the Notes by hand (docs/evidence/README.md).

build syncs the checkout to the Windows build folder (play.py's route) and
builds the 32-bit runner iwd2r_oracle, the replay injector and the recorder
DLL with the windows-msvc-x86 preset.

run copies the vanilla iwd2.exe (MD5 from oracle/loader/vanilla.json) to
C:/Users/thier/iwd2r-oracle and runs the runner on it.

Without --selftest it runs comparison cases: the case file named by --file,
or every case file the runner lists, one fresh runner process per file. Each
process writes <file>.json under C:/Users/thier/iwd2r-oracle/out/<stamp>;
they are merged into runs/<stamp>/oracle.json and summarised. A case declared
to fail (selftest/must_fail) is listed under expected_failures and one
declared to crash (selftest/crash) under expected_crashes. A case declared
pending (ORACLE_CASE_PENDING, with its reason) is listed under pending when it
did what a passing case does; one with no body is not run, and one with a body
that ran no input ("has_body" true, 0 iterations) is unexpected and shown as
PENDING (ran 0 inputs). The run fails
(exit 1) if any case does not do what it declared, or a runner process ends
without its report.

Before the cases run, the 2DA texts they read (TABLES) are extracted from the
vanilla copy by tools/bif.py into C:/Users/thier/iwd2r-oracle/tables, which
the runner gets as --tables.

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

vanilla builds C:/Users/thier/iwd2r-oracle/game, a vanilla copy of the game
for the original engine: rsync of the real install (read only, never written),
WeiDU's uninstall of every IWD2EE component run inside the copy, deletion of
the IEex and IWD2EE files WeiDU does not track, and icewind2.ini's [Alias]
paths pointed at the copy with Resolution=800. The copy is verified (no IEex
file, no IWD2EE in WeiDU.log, the vanilla iwd2.exe MD5, no INI naming the
install, the install's WeiDU.log and icewind2.ini unchanged) and only then
gets the marker game/vanilla.ok. A copy without the marker (an interrupted
build) or --rebuild is deleted and built from scratch; run without --selftest
builds it when the marker is missing. The log goes to runs/<stamp>/.

replay record runs the original iwd2.exe of the vanilla copy (built first
when it has no marker) under a scenario (scenarios/<name>.txt or a path, the
format of platform/script.h) with the recorder DLL injected
(oracle/replay/injector.cpp, recorder.cpp): windowed (the copy's
icewind2.ini gets Full Screen=0 and keeps Resolution=800), with the copy as
its working directory, a virtual clock, scenario input instead of the real
mouse and keyboard, and the original srand seeded with --seed (default 1).
It writes frame-<N>.json every --dump-every frames (default 30, 0 for none)
and checkpoint-<NAME>.json at the scenario's checkpoint lines, under
C:/Users/thier/iwd2r-oracle/replays/<stamp>, then moves them with
recorder.log, status.txt and replay.json (the summary) to
runs/<stamp>/replay/. Every dump is validated against
oracle/replay/schema.json (tools/minischema.py). The injector kills the game
after --timeout seconds (default 180) and exits 10; the run then reports the
last frame the recorder reached (its status.txt) and exits 10. Any other
failure (a dump that does not validate, a game exit code other than 0, a
scenario that did not finish) exits 1.

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
import bif  # noqa: E402
import check_evidence  # noqa: E402
import minischema  # noqa: E402

EVIDENCE_DIR = os.path.join(ROOT, 'docs', 'evidence')
VERSIONS = os.path.join(HERE, 'ghidra', 'versions.json')
VANILLA_JSON = os.path.join(ROOT, 'oracle', 'loader', 'vanilla.json')
VANILLA_EXE = os.path.expanduser('~/.local/share/iwd2r/vanilla/iwd2.exe')
ORACLE_DIR = '/mnt/c/Users/thier/iwd2r-oracle'
INSTALL = '/mnt/c/Games/Icewind Dale II'
INSTALL_WIN = 'C:\\Games\\Icewind Dale II'
GAME_DIR = ORACLE_DIR + '/game'
TABLES_DIR = ORACLE_DIR + '/tables'
# The game text the cases read (runner --tables), extracted from the vanilla
# copy by tools/bif.py before every run. Never committed.
TABLES = ('XPLEVEL.2DA',)
MARKER = 'vanilla.ok'
WEIDU_SETUP = 'Setup-iwd2ee.exe'
WEIDU_TIMEOUT = 3600  # seconds for the whole uninstall
VANILLA_RESOLUTION = '800'
ORACLE_PRESET = 'windows-msvc-x86'
RUNNER_REL = 'build/windows-msvc-x86/oracle/RelWithDebInfo/iwd2r_oracle.exe'
SELFTEST_RAND = 41  # VC6 rand() after srand(1)
RUNS_DIR = os.path.join(ROOT, 'runs')
FILE_TIMEOUT = 600  # seconds for one case file, enforced by the runner
EXIT_TIMEOUT = 10  # the runner's exit code when --timeout ran out
HEAP_CORRUPT = 'heap corrupt after case'
REPLAY_DIR_REL = 'build/windows-msvc-x86/oracle/RelWithDebInfo'
INJECTOR = 'injector.exe'
RECORDER = 'recorder.dll'
REPLAYS_DIR = ORACLE_DIR + '/replays'
REPLAY_SCHEMA = os.path.join(ROOT, 'oracle', 'replay', 'schema.json')
REPLAY_TIMEOUT = 180  # seconds, enforced by the injector
REPLAY_DUMP_EVERY = 30
INJECTOR_TIMEOUT = 10  # the injector's exit code when --timeout ran out
INJECTOR_DIED = 11
INJECTOR_NO_DLL = 12


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
    exe = play.build_windows(preset=ORACLE_PRESET,
                             target='iwd2r_oracle iwd2r_injector '
                             'iwd2r_recorder',
                             exe_rel=RUNNER_REL, bat_name='oracle-build.bat')
    print('built %s' % win_path(exe))
    for name in (INJECTOR, RECORDER):
        path = os.path.join(play.WIN_CHECKOUT, REPLAY_DIR_REL, name)
        if not os.path.isfile(path):
            sys.exit('oracle: the build produced no %s' % path)
        print('built %s' % win_path(path))
    return 0


def ran_nothing(case):
    """A pending case with a body that ran no input: it checked nothing."""
    return bool(case.get('pending') and case.get('has_body')
                and not case['iterations'] and not case['crashed'])


def case_ok(case):
    """Whether a case did what it declared (its "expect")."""
    if ran_nothing(case):
        return False
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
              'pending': [], 'unexpected': [], 'passed': True}
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
                elif case.get('pending'):
                    merged['pending'].append('%s: %s' % (case['case'],
                                                         case['pending']))
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
        if ran_nothing(case):
            state = 'PENDING (ran 0 inputs): %s' % case['pending']
        elif case_ok(case):
            state += ' (expected)' if case['expect'] != 'pass' else ''
            if case.get('pending'):
                state = ('pending: %s' % case['pending'] if not
                         case['iterations'] else '%s, pending: %s'
                         % (state, case['pending']))
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
    counts = {'pass': 0, 'expected': 0, 'pending': 0}
    for case in merged['cases']:
        if case_ok(case):
            if case['expect'] != 'pass':
                counts['expected'] += 1
            else:
                counts['pending' if case.get('pending') else 'pass'] += 1
    lines.append('%d passed, %d expected failures or crashes, %d pending, '
                 '%d unexpected; run %s'
                 % (counts['pass'], counts['expected'], counts['pending'],
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


def extract_tables(game, out_dir=TABLES_DIR, names=TABLES):
    """Drop the 2DA texts the cases read under out_dir (Override first,
    then the KEY/BIFF archives of the vanilla copy)."""
    try:
        return bif.extract_to(game, out_dir, names)
    except (bif.BifError, OSError) as e:
        sys.exit('oracle: cannot extract the case tables: %s' % e)


def run_cases(args):
    import play
    extract_tables(ensure_vanilla())
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
               '--out', win_path(out_dir), '--tables', win_path(TABLES_DIR),
               '--timeout', str(FILE_TIMEOUT)]
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


# ---------------------------------------------------------------------------
# The vanilla copy of the game (oracle.py vanilla)

# Files IWD2EE and IEex leave that WeiDU does not track, by name in the root
# of the copy (case-insensitive glob), and the ones in Override/. Shaders is
# cnc-ddraw's, copied from iwd2ee/ddrawfix with the ddraw.dll WeiDU removes.
LEFTOVER_ROOT = ('ieex*.dll', 'ieex.ini', 'ieex.log', 'iwd2ee.exe',
                 'setup-iwd2ee*.exe', 'setup-iwd2ee.debug', 'iwd2ee',
                 'iwd2ee_mod_compatibility_patch', 'shaders')
LEFTOVER_OVERRIDE = ('ieex_*.lua',)
WEIDU_LINE = re.compile(r'^~([^~]+)~\s+#(\d+)\s+#(\d+)')


def weidu_components(text):
    """The installed components a WeiDU.log lists, oldest first, as
    (tp2, language, component)."""
    out = []
    for line in text.splitlines():
        m = WEIDU_LINE.match(line.strip())
        if m:
            out.append((m.group(1), int(m.group(2)), int(m.group(3))))
    return out


def rewrite_ini(text, old, new):
    """An INI's text with every path under old (a Windows path, any case)
    moved under new, and Resolution= set to VANILLA_RESOLUTION."""
    text = re.sub(re.escape(old), lambda m: new, text, flags=re.IGNORECASE)
    return re.sub(r'^(Resolution=)[^\r\n]*', r'\g<1>' + VANILLA_RESOLUTION,
                  text, flags=re.MULTILINE)


def _matches(name, patterns):
    import fnmatch
    return any(fnmatch.fnmatch(name.lower(), p) for p in patterns)


def leftovers(game):
    """The untracked IWD2EE and IEex files in game, relative paths (sorted).
    A folder is listed once, not its contents."""
    out = [n for n in os.listdir(game) if _matches(n, LEFTOVER_ROOT)]
    for override in os.listdir(game):
        if override.lower() == 'override' and \
                os.path.isdir(os.path.join(game, override)):
            out += [override + '/' + n
                    for n in os.listdir(os.path.join(game, override))
                    if _matches(n, LEFTOVER_OVERRIDE)]
    return sorted(out)


def read_text(path):
    with open(path, 'rb') as f:
        return f.read().decode('latin-1')


def verify_vanilla(game, md5, install_win=INSTALL_WIN):
    """Every reason game is not a clean vanilla copy (empty when it is)."""
    failures = []
    names = {n.lower(): n for n in os.listdir(game)}
    if 'chitin.key' not in names:
        failures.append('no CHITIN.KEY')
    exe = names.get('iwd2.exe')
    if exe is None:
        failures.append('no iwd2.exe')
    elif file_md5(os.path.join(game, exe)) != md5:
        failures.append('%s has MD5 %s, not the vanilla %s'
                        % (exe, file_md5(os.path.join(game, exe)), md5))
    log = names.get('weidu.log')
    if log is not None:
        comps = [(tp2, comp) for tp2, _, comp in weidu_components(
            read_text(os.path.join(game, log)))
            if tp2.lower().endswith('iwd2ee.tp2')]
        if comps:
            failures.append('%s still lists %s components %s' % (
                log, comps[0][0], ' '.join(str(c) for _, c in comps)))
    left = leftovers(game)
    for rel in left:
        failures.append('IWD2EE leftover %s' % rel)
    for dirpath, dirnames, filenames in os.walk(game):
        dirnames.sort()
        for name in dirnames:
            rel = os.path.relpath(os.path.join(dirpath, name),
                                  game).replace(os.sep, '/')
            if name.lower().startswith('ieex') and \
                    not any(rel == x or rel.startswith(x + '/') for x in left):
                failures.append('IEex folder %s' % rel)
        for name in sorted(filenames):
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, game).replace(os.sep, '/')
            low = name.lower()
            if any(rel == x or rel.startswith(x + '/') for x in left):
                continue
            if low.startswith('ieex'):
                failures.append('IEex file %s' % rel)
            if low.endswith('.ini') and \
                    install_win.lower() in read_text(path).lower():
                failures.append('%s still refers to %s' % (rel, install_win))
    ini = names.get('icewind2.ini')
    if ini is None:
        failures.append('no icewind2.ini')
    elif not re.search(r'^Resolution=%s\s*$' % VANILLA_RESOLUTION,
                       read_text(os.path.join(game, ini)), re.MULTILINE):
        failures.append('%s does not set Resolution=%s'
                        % (ini, VANILLA_RESOLUTION))
    return failures


def uninstall_bat(game_win, setup, components, log_win):
    """The batch file that runs WeiDU's uninstall inside the copy."""
    numbers = ' '.join(str(c) for _, _, c in components)
    languages = sorted({lang for _, lang, _ in components})
    if len(languages) != 1:
        raise ValueError('components in several languages: %s' % languages)
    return '\n'.join([
        '@echo off',
        'cd /d "%s" || exit /b 1' % game_win,
        '"%s" --noautoupdate --no-exit-pause --skip-at-view --language %d '
        '--log "%s" --force-uninstall-list %s < nul'
        % (setup, languages[0], log_win, numbers),
        'exit /b %errorlevel%',
    ]) + '\n'


def ini_resolution(path):
    m = re.search(r'^Resolution=([^\r\n]*)', read_text(path), re.MULTILINE)
    return m.group(1) if m else None


def install_state(install=INSTALL):
    """What the real install must still look like after a build."""
    return {'WeiDU.log md5': file_md5(os.path.join(install, 'WeiDU.log')),
            'icewind2.ini md5': file_md5(os.path.join(install,
                                                      'icewind2.ini')),
            'icewind2.ini Resolution': ini_resolution(
                os.path.join(install, 'icewind2.ini'))}


def say(log, message):
    print(message)
    log.write(message + '\n')
    log.flush()


def _resolved(path):
    """path resolved through links, case-folded, no trailing separator."""
    out = os.path.normcase(os.path.realpath(path)).lower().rstrip('/\\')
    return out or '/'


def drvfs_win(path):
    """The Windows path of a /mnt/<drive>/... path, or None."""
    m = re.match(r'^/mnt/([A-Za-z])(/.*)?$', path.rstrip('/') or '/')
    if not m:
        return None
    return '%s:\\%s' % (m.group(1).upper(),
                         (m.group(2) or '').strip('/').replace('/', '\\'))


def run_cmd(argv):
    """Run a cmd.exe command line; a CompletedProcess."""
    import play
    return subprocess.run([play.CMD_EXE, '/c'] + argv, cwd='/mnt/c',
                          capture_output=True, text=True, errors='replace',
                          timeout=60)


def windows_reparse(path, run=run_cmd):
    """None when Windows says path is not a reparse point (junction,
    symlink), else the reason to refuse. Fails closed: dir /AL /B on the
    parent must exit 0 without naming path, or exit 1 with exactly "File
    Not Found" and no output; anything else, or a path off drvfs, refuses."""
    path = path.rstrip('/')
    parent, name = os.path.split(path)
    win_parent = drvfs_win(parent)
    if win_parent is None or not name:
        return '%s is not on a Windows drive (/mnt/<drive>/)' % path
    try:
        r = run(['dir', '/AL', '/B', win_parent])
    except FileNotFoundError as e:
        return 'cannot run cmd.exe to check %s for reparse points: %s' % (
            path, e)
    except subprocess.TimeoutExpired:
        return 'cmd.exe timed out checking %s for reparse points' % path
    except OSError as e:
        return 'cannot check %s for reparse points: %s' % (path, e)
    stdout, stderr = r.stdout or '', r.stderr or ''
    if r.returncode == 0:
        listed = [line.strip().lower() for line in stdout.splitlines()]
        if name.lower() in listed:
            return '%s is a reparse point' % path
        return None
    if r.returncode == 1 and not stdout.strip() and \
            stderr.strip() == 'File Not Found':
        return None
    return ('dir /AL /B %s gave exit %d, stdout %r, stderr %r; cannot tell '
            'whether %s is a reparse point' % (win_parent, r.returncode,
                                               stdout[:200], stderr[:200],
                                               path))


def unsafe_target(game, install, oracle_dir=ORACLE_DIR,
                  reparse=windows_reparse):
    """Why game may not be deleted, rsynced into or run in, or None."""
    g, i, o = _resolved(game), _resolved(install), _resolved(oracle_dir)
    if g == i:
        return '%s is the install %s' % (game, install)
    if g.startswith(i + '/'):
        return '%s is inside the install %s' % (game, install)
    if i.startswith(g + '/'):
        return 'the install %s is inside %s' % (install, game)
    if os.path.dirname(g) != o:
        return '%s (%s) is not directly in %s' % (game, g, oracle_dir)
    for path in (game.rstrip('/'), oracle_dir.rstrip('/')):
        if os.path.islink(path):
            return '%s is a symlink' % path
        reason = reparse(path)
        if reason:
            return reason
    return None


def assert_safe_target(game, install):
    reason = unsafe_target(game, install)
    if reason:
        sys.exit('oracle: refusing to touch the copy: %s' % reason)


def links_in(root):
    """Every symlink under root (relative paths). On drvfs, Windows
    junctions and symlinks show up as symlinks too."""
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        for name in dirnames + filenames:
            if os.path.islink(os.path.join(dirpath, name)):
                out.append(os.path.relpath(os.path.join(dirpath, name), root))
    return sorted(out)


def marker_ok(game, md5=None):
    """Whether game has a marker that parses and names the vanilla MD5."""
    try:
        with open(os.path.join(game, MARKER)) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return False
    return isinstance(data, dict) and data.get('md5') == (md5 or vanilla_md5())


def write_marker(game, data):
    """Write the marker atomically: a temporary name, then a rename."""
    tmp = os.path.join(game, MARKER + '.tmp')
    with open(tmp, 'w') as f:
        json.dump(data, f, indent=2)
        f.write('\n')
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, os.path.join(game, MARKER))


def prove_separate(game, install, log):
    """Exit unless game is a real directory of its own, not a link or
    junction to install."""
    for path in (ORACLE_DIR, game):
        if os.path.islink(path):
            sys.exit('oracle: %s is a link; refusing to run WeiDU' % path)
    if os.path.realpath(game) != os.path.abspath(game):
        sys.exit('oracle: %s resolves to %s' % (game, os.path.realpath(game)))
    if os.path.samefile(game, install):
        sys.exit('oracle: %s is the install itself' % game)
    probe = '.iwd2r-copy-probe-%d' % os.getpid()
    with open(os.path.join(game, probe), 'w') as f:
        f.write('probe\n')
    leaked = os.path.exists(os.path.join(install, probe))
    os.remove(os.path.join(game, probe))
    if leaked:
        sys.exit('oracle: a file written to %s appeared in %s' % (game,
                                                                   install))
    # Windows' own view of the copy (fails closed).
    reason = windows_reparse(game)
    if reason:
        sys.exit('oracle: %s' % reason)
    say(log, 'copy is separate: no link, own inode, probe file not in the '
        'install, Windows lists no reparse point at %s' % drvfs_win(game))


# The destructive steps of build_vanilla, as functions so tests can record
# them.
def remove_tree(path):
    shutil.rmtree(path)


def rsync_copy(install, game, log):
    import play
    return play.stream(['rsync', '-a', '--delete', install + '/',
                        game + '/'], log)


def run_weidu(bat, game, log):
    import play
    return play.stream([play.CMD_EXE, '/c', win_path(bat)], log, cwd=game,
                       stdin=subprocess.DEVNULL)


def delete_leftover(path):
    if os.path.isdir(path) and not os.path.islink(path):
        shutil.rmtree(path)
    else:
        os.remove(path)


def build_vanilla(game, install=INSTALL, md5=None):
    """Copy install to game, uninstall IWD2EE there, clean up and verify;
    write the marker only when the copy is clean. Exits on failure."""
    import play
    md5 = md5 or vanilla_md5()
    run_dir = play.make_run_folder(RUNS_DIR, datetime.datetime.now())
    os.makedirs(ORACLE_DIR, exist_ok=True)
    with open(os.path.join(run_dir, 'vanilla.log'), 'w') as log:
        before = install_state(install)
        say(log, 'install before: %s' % json.dumps(before))
        assert_safe_target(game, install)
        marker = os.path.join(game, MARKER)
        if os.path.lexists(marker):
            os.remove(marker)
        if os.path.exists(game):
            say(log, 'removing the old copy %s' % game)
            remove_tree(game)
        os.makedirs(game)
        prove_separate(game, install, log)
        say(log, 'copying %s to %s' % (install, game))
        assert_safe_target(game, install)
        if rsync_copy(install, game, log) != 0:
            sys.exit('oracle: rsync failed')
        prove_separate(game, install, log)
        links = links_in(game)
        if links:
            sys.exit('oracle: the copy holds links or junctions: %s'
                     % ' '.join(links))
        say(log, 'no symlinks or junctions in the copy')
        game_win = win_path(game)
        if game_win.lower().startswith(INSTALL_WIN.lower()):
            sys.exit('oracle: %s is inside the install' % game_win)
        names = {n.lower(): n for n in os.listdir(game)}
        setup = names.get(WEIDU_SETUP.lower())
        components = weidu_components(read_text(
            os.path.join(game, names['weidu.log']))) \
            if 'weidu.log' in names else []
        say(log, 'WeiDU.log lists %d components: %s' % (
            len(components), ' '.join(str(c) for _, _, c in components)))
        others = [c for c in components
                  if not c[0].lower().endswith('iwd2ee.tp2')]
        if others:
            sys.exit('oracle: WeiDU.log lists components of other mods: %s'
                     % others)
        if components:
            if setup is None:
                sys.exit('oracle: no %s in the copy' % WEIDU_SETUP)
            weidu_log = os.path.join(ORACLE_DIR, 'vanilla-weidu.log')
            bat = os.path.join(ORACLE_DIR, 'vanilla-uninstall.bat')
            script = uninstall_bat(game_win, setup, components,
                                   win_path(weidu_log))
            with open(bat, 'w', newline='\r\n') as f:
                f.write(script)
            say(log, 'running WeiDU in %s: %s' % (game_win,
                                                   script.splitlines()[2]))
            assert_safe_target(game, install)
            code = run_weidu(bat, game, log)
            if os.path.exists(weidu_log):
                shutil.copyfile(weidu_log,
                                os.path.join(run_dir, 'weidu-uninstall.log'))
            say(log, 'WeiDU exit code %d' % code)
            if code != 0:
                sys.exit('oracle: WeiDU uninstall failed (exit %d); see %s'
                         % (code, run_dir))
        for rel in leftovers(game):
            path = os.path.join(game, rel)
            say(log, 'deleting leftover %s' % rel)
            assert_safe_target(game, install)
            delete_leftover(path)
        for name in os.listdir(game):
            path = os.path.join(game, name)
            if name.lower().endswith('.ini') and os.path.isfile(path):
                text = read_text(path)
                new = rewrite_ini(text, INSTALL_WIN, game_win) \
                    if name.lower() == 'icewind2.ini' else \
                    re.sub(re.escape(INSTALL_WIN), lambda m: game_win, text,
                           flags=re.IGNORECASE)
                if new != text:
                    say(log, 'rewrote %s' % name)
                    with open(path, 'wb') as f:
                        f.write(new.encode('latin-1'))
        override = os.path.join(game, names.get('override', 'Override'))
        rest = sorted(os.listdir(override)) if os.path.isdir(override) else []
        say(log, 'Override holds %d files: %s' % (len(rest), ' '.join(rest)))
        failures = verify_vanilla(game, md5)
        after = install_state(install)
        say(log, 'install after: %s' % json.dumps(after))
        if after != before:
            failures.append('the real install changed: %s -> %s'
                            % (before, after))
        for failure in failures:
            say(log, 'FAIL %s' % failure)
        if failures:
            sys.exit('oracle: the vanilla copy failed verification; see %s'
                     % os.path.relpath(run_dir, ROOT))
        say(log, 'verified: %s is vanilla (iwd2.exe MD5 %s)' % (game, md5))
        write_marker(game, {'md5': md5, 'install': before,
                            'built': datetime.datetime.now().isoformat(
                                timespec='seconds')})
        say(log, 'wrote %s' % marker)
    return game


def ensure_vanilla(rebuild=False, game=GAME_DIR, build=build_vanilla):
    """The vanilla copy's path, building it first when it has no marker
    (never built, or an interrupted build), a marker that does not parse
    or name the vanilla MD5, or rebuild is set."""
    if rebuild or not marker_ok(game):
        build(game)
    return game


def cmd_vanilla(args):
    game = ensure_vanilla(args.rebuild)
    print('vanilla copy: %s' % game)
    return 0


# ---------------------------------------------------------------------------
# Replays in the original (oracle.py replay record)

def replay_ini(text):
    """icewind2.ini's text for a recording: windowed (Full Screen=0) at
    Resolution=VANILLA_RESOLUTION, everything else kept. Raises ValueError
    when either key is missing, rather than guessing where it belongs."""
    for key in ('Full Screen', 'Resolution'):
        if not re.search(r'^%s=' % key, text, re.MULTILINE):
            raise ValueError('icewind2.ini has no %s= line' % key)
    text = re.sub(r'^(Full Screen=)[^\r\n]*', r'\g<1>0', text,
                  flags=re.MULTILINE)
    return re.sub(r'^(Resolution=)[^\r\n]*',
                  r'\g<1>' + VANILLA_RESOLUTION, text, flags=re.MULTILINE)


def prepare_replay_ini(game, install=INSTALL):
    """Set the copy's icewind2.ini up for a recording (replay_ini); only the
    copy is written, after the same guard as every write to it."""
    assert_safe_target(game, install)
    names = {n.lower(): n for n in os.listdir(game)}
    if 'icewind2.ini' not in names:
        sys.exit('oracle: the copy has no icewind2.ini')
    path = os.path.join(game, names['icewind2.ini'])
    text = read_text(path)
    try:
        new = replay_ini(text)
    except ValueError as e:
        sys.exit('oracle: %s: %s' % (path, e))
    if new != text:
        with open(path, 'wb') as f:
            f.write(new.encode('latin-1'))
        print('set Full Screen=0, Resolution=%s in %s'
              % (VANILLA_RESOLUTION, path))


def scenario_path(name):
    """scenarios/<name>.txt, or name itself when it is a file."""
    if os.path.isfile(name):
        return name
    path = os.path.join(ROOT, 'scenarios', name + '.txt')
    if not os.path.isfile(path):
        sys.exit('oracle: no scenario %s (looked for %s)' % (name, path))
    return path


def parse_status(text):
    """The recorder's status.txt as a dict (frame and script_line as ints)."""
    out = {}
    for line in text.splitlines():
        key, sep, value = line.partition('=')
        if sep:
            out[key.strip()] = int(value) if value.strip().isdigit() \
                else value.strip()
    return out


def read_status(replay_dir):
    try:
        with open(os.path.join(replay_dir, 'status.txt')) as f:
            return parse_status(f.read())
    except OSError:
        return {}


def check_dumps(replay_dir, schema):
    """{file name: [schema errors]} for every dump in replay_dir."""
    out = {}
    for name in sorted(os.listdir(replay_dir)):
        if not (name.endswith('.json') and (name.startswith('frame-')
                                             or name.startswith(
                                                 'checkpoint-'))):
            continue
        try:
            with open(os.path.join(replay_dir, name)) as f:
                dump = json.load(f)
        except ValueError as e:
            out[name] = ['not JSON: %s' % e]
            continue
        out[name] = minischema.validate(dump, schema)
    return out


def replay_outcome(code, status, timeout):
    """(exit code for oracle.py, one-line summary) of an injector exit."""
    where = 'last frame %s' % status.get('frame', '?')
    if status.get('script_line'):
        where += ', scenario line %s' % status['script_line']
    if code is None or code == INJECTOR_TIMEOUT:
        return (INJECTOR_TIMEOUT,
                'timed out after %d s; the game was killed (%s)'
                % (timeout, where))
    if code == INJECTOR_DIED:
        return 1, 'the game died of an exception (%s)' % where
    if code == INJECTOR_NO_DLL:
        return 1, 'the recorder did not load (see recorder.log)'
    if code != 0:
        return 1, 'the game exited with code %d (%s)' % (code, where)
    if status.get('state') == 'running':
        return 1, 'the game exited before the scenario finished (%s)' % where
    return 0, 'the game exited with code 0 (%s, scenario %s)' % (
        where, status.get('state', '?'))


def cmd_replay_record(args):
    import play
    script = scenario_path(args.scenario)
    import realinput
    try:
        actions = realinput.load_scenario(script)
    except realinput.ScenarioError as e:
        sys.exit('oracle: %s' % e)
    build = os.path.join(play.WIN_CHECKOUT, REPLAY_DIR_REL)
    injector = os.path.join(build, INJECTOR)
    recorder = os.path.join(build, RECORDER)
    for path in (injector, recorder):
        if not os.path.isfile(path):
            sys.exit('oracle: no %s; run tools/oracle.py build' % path)
    game = ensure_vanilla()
    prepare_replay_ini(game)
    with open(REPLAY_SCHEMA) as f:
        schema = json.load(f)
    run_dir = play.make_run_folder(RUNS_DIR, datetime.datetime.now())
    stamp = os.path.basename(run_dir)
    replay_dir = os.path.join(REPLAYS_DIR, stamp)
    os.makedirs(replay_dir)
    shutil.copyfile(script, os.path.join(replay_dir, 'scenario.txt'))
    env = dict(os.environ)
    names = {'IWD2R_SCRIPT': win_path(os.path.join(replay_dir,
                                                   'scenario.txt')),
             'IWD2R_REPLAY_DIR': win_path(replay_dir),
             'IWD2R_DUMP_EVERY': str(args.dump_every),
             'IWD2R_SEED': str(args.seed)}
    env.update(names)
    env['WSLENV'] = ':'.join([v for v in env.get('WSLENV', '').split(':')
                              if v] + sorted(names))
    command = [injector, win_path(os.path.join(game, 'iwd2.exe')),
               win_path(game), win_path(recorder),
               '--timeout', str(args.timeout)]
    print('replay %s (%d actions), seed %d, timeout %d s, dumps every %d '
          'frames' % (os.path.relpath(script, ROOT), len(actions), args.seed,
                      args.timeout, args.dump_every))
    print('running %s' % ' '.join(command))
    try:
        # The injector kills the game at --timeout; this is a backstop.
        r = subprocess.run(command, cwd=ORACLE_DIR, env=env,
                           capture_output=True, text=True,
                           timeout=args.timeout + 60)
        code, stdout, stderr = r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        code, stdout, stderr = None, '', ''
    except KeyboardInterrupt:
        # Killing the injector closes its job, which kills the game.
        subprocess.run([play.CMD_EXE, '/c', 'taskkill', '/F', '/IM',
                        INJECTOR], cwd=ORACLE_DIR, capture_output=True)
        raise
    for line in stdout.splitlines():
        print('  %s' % line)
    for line in stderr.splitlines():
        print('  stderr: %s' % line)
    status = read_status(replay_dir)
    result, summary = replay_outcome(code, status, args.timeout)
    dumps = check_dumps(replay_dir, schema)
    out_dir = os.path.join(run_dir, 'replay')
    shutil.copytree(replay_dir, out_dir)
    shutil.rmtree(replay_dir, ignore_errors=True)
    invalid = {name: errors for name, errors in dumps.items() if errors}
    report = {'scenario': os.path.relpath(script, ROOT), 'seed': args.seed,
              'timeout': args.timeout, 'dump_every': args.dump_every,
              'injector_exit_code': code, 'status': status,
              'outcome': summary, 'dumps': sorted(dumps),
              'invalid_dumps': invalid}
    with open(os.path.join(out_dir, 'replay.json'), 'w') as f:
        json.dump(report, f, indent=2)
        f.write('\n')
    checkpoints = [n for n in sorted(dumps) if n.startswith('checkpoint-')]
    print(summary)
    print('%d dumps (%d checkpoints: %s), %d invalid'
          % (len(dumps), len(checkpoints), ' '.join(checkpoints) or 'none',
             len(invalid)))
    for name, errors in sorted(invalid.items()):
        for error in errors[:5]:
            print('  INVALID %s: %s' % (name, error))
    print('replay: %s' % os.path.relpath(out_dir, ROOT))
    if result == 0 and invalid:
        result = 1
    return result


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
    van = sub.add_parser('vanilla', help='build the verified vanilla copy '
                         'of the game')
    van.add_argument('--rebuild', action='store_true',
                     help='delete the copy and build it again')
    van.set_defaults(run=cmd_vanilla)
    replay = sub.add_parser('replay', help='replays in the original')
    replay_sub = replay.add_subparsers(dest='replay_command', required=True)
    record = replay_sub.add_parser('record', help='record a scenario in the '
                                   'original with the recorder DLL')
    record.add_argument('scenario', help='scenarios/<name>.txt by name, '
                        'or a path')
    record.add_argument('--seed', type=int, default=1,
                        help='seed for the original srand (default '
                        '%(default)s)')
    record.add_argument('--timeout', type=int, default=REPLAY_TIMEOUT,
                        help='seconds before the game is killed (default '
                        '%(default)s)')
    record.add_argument('--dump-every', type=int, default=REPLAY_DUMP_EVERY,
                        help='a dump every N frames, 0 for none (default '
                        '%(default)s)')
    record.set_defaults(run=cmd_replay_record)
    args = ap.parse_args(argv)
    return args.run(args)


if __name__ == '__main__':
    sys.exit(main())
