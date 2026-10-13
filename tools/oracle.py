"""Compare iwd2r with the original iwd2.exe and record decompiler evidence.

Usage (from WSL, in the repository):

  python3 tools/oracle.py evidence 0x5845D0
  python3 tools/oracle.py evidence IcewindMisc::Roll
  python3 tools/oracle.py evidence 0x5845D0 CRuleTables::GetXPCap [--force]
  python3 tools/oracle.py build
  python3 tools/oracle.py run --selftest
  python3 tools/oracle.py run [--file selftest] [--seed N] [--iterations N]
  python3 tools/oracle.py vanilla [--rebuild]
  python3 tools/oracle.py replay record <scenario> [--seed N] [--timeout S]
                                        [--dump-every N] [--test-steal-focus N]
                                        [--test-no-focus-guard]
                                        [--test-slow-reads MS] [--host]
  python3 tools/oracle.py replay compare <scenario> [--seed N] [--timeout S]
                                         [--dump-every N] [--no-build]
                                         [--host]

evidence writes a draft docs/evidence/<ADDR>.md for each function named, all
in one Ghidra run: the front matter (address, name, an empty oracle_case,
ghidra_version), Ghidra's decompilation, the callers and the callees. It reads
the labelled Ghidra project that tools/ghidra/make_project.py creates, opened
read-only through tools/ghidra/DecompileToEvidence.java; Ghidra runs one at a
time across worktrees (make_project.py's lock). An existing evidence file is
kept unless --force is given. A name shared by several functions (overloads)
lists them and asks for an address. Such names, missing ones and kept files
are listed, and the command exits 1 after writing the rest. Fill in
oracle_case or exemption and the Notes by hand (docs/evidence/README.md).

build syncs the checkout to this worktree's Windows build folder (play.py's
route and play.win_checkout) and builds the 32-bit runner iwd2r_oracle, the replay injector and the recorder
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

Before the cases run, the game files they read (TABLES) are extracted from the
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
failure exits 1: a dump that does not validate, a game exit code other than
0, a scenario that did not finish, a scenario checkpoint with no dump, or a
checkpoint dump that breaks the recorder's invariant (invariant_problem:
not reinitializing, no frame without an AI update of its own,
ai_stalled_frames 0, and no load in progress, loading and in_async_update
0). The calls a load draws from inside an AI update are held, not frames:
the clock stands still for them and no timer fires, so a load costs the
same AI updates on every machine (platform/replay_clock.h FrameGate,
globals.loads and held_frames). --test-steal-focus N makes a window of the
recorder take the foreground at frame N, and --test-no-focus-guard lets
activation messages reach the game, to show what the focus guard prevents;
--test-slow-reads MS slows every ReadFile off the main thread by MS ms, to
show a slower disk does not change the dumps.
On Ctrl+C only this run's injector is killed (by the pid it prints first),
which closes its job and so kills its game.

The original runs inside Windows Sandbox, one fresh sandbox per recording
(tools/winsandbox.py run_replay, tools/sandbox/replay-driver.ps1), so it
never takes the focus on the desktop and no stray click reaches it. The
sandbox maps the oracle build output (C:\\iwd2r\\oracle) and the run's
inputs (C:\\iwd2r\\tools) read-only, and the vanilla copy (at its own path,
which its icewind2.ini names) and a results folder (C:\\iwd2r\\results)
read-write. The driver replaces the sandbox's DirectPlay stubs with the
host's DirectPlay files (iwd2.exe imports DPLAYX.dll), runs the injector
with the host mode's arguments and timeout, and writes done.txt; the
dumps come back to the same runs/<stamp>/ folder, the driver's files to
its sandbox/ subfolder. --host runs the original on the host's desktop
instead, for debugging.

replay compare runs the scenario twice with the same seed: in the original
as replay record does (into runs/<stamp>/original), then in our engine
(build/linux-gcc/iwd2r, built first unless --no-build) headless on the same
vanilla copy, with Resolution=800 and Full Screen=0 as INI overrides and the
deterministic replay mode on (IWD2R_STATE_DUMP, platform/replay.h: the
recorder's frame, virtual clock and wall clock, timers, seeding, input and
focus model), into runs/<stamp>/ours. The copy's INI files are restored
before and after our run, so both sides start from the same ones. Both
sides' dumps get replay record's checks (schema, invariant, every
checkpoint present). Then every scenario checkpoint is compared field by
field (lists element by element); frame, au_counter, ai_stalled_frames,
in_async_update and held_frames are timing, reported and not compared. A
checkpoint is compared when its screen on either side is listed in
oracle/replay/screens.json; a difference elsewhere is shown as not enabled.
A difference listed in oracle/replay/known_differences.json (field,
checkpoint or "*", reason, evidence) is reported as KNOWN and does not
fail. It exits 0 on a match (known differences allowed) and 1 on any other
difference in an enabled screen, a missing checkpoint dump, or a failed run
on either side; runs/<stamp>/compare.json keeps the result.

The vanilla copy is shared by every worktree: replay record and replay
compare each hold C:/Users/thier/iwd2r-oracle/replay.lock for the whole run,
and building the copy holds vanilla.lock beside it (tools/filelock.py). In
the sandbox, a recording also holds C:/Users/thier/iwd2r-vm/sandbox.lock
(one sandbox at a time, shared with play.py --vm windows), always taken
inside replay.lock. A run that has to wait says so on stderr after 5
seconds.

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
import dataview  # noqa: E402
import filelock  # noqa: E402
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
# The game files the cases read (runner --tables), extracted from the vanilla
# copy by tools/bif.py before every run. Never committed.
TABLES = ('XPLEVEL.2DA', 'HPWAR.2DA', 'HPWIZ.2DA', 'HPPRS.2DA', 'HPROG.2DA',
          'HPMONK.2DA', 'SPLAUTOP.2DA', 'RT_NORM.2DA', 'RT_FURY.2DA',
          'RNDTRES.2DA', '00SWDL01.ITM', '00CHAN01.ITM', '00SCRL01.ITM',
          '00POTN01.ITM', '00AMUL01.ITM', '00GBGA01.ITM', 'MISC07.ITM')
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
# Shared by every worktree (filelock.hold): one replay of the vanilla copy at
# a time, and one build of that copy.
REPLAY_LOCK = ORACLE_DIR + '/replay.lock'
VANILLA_LOCK = ORACLE_DIR + '/vanilla.lock'
REPLAY_SCHEMA = os.path.join(ROOT, 'oracle', 'replay', 'schema.json')
REPLAY_TIMEOUT = 180  # seconds, enforced by the injector
REPLAY_DUMP_EVERY = 30
INJECTOR_TIMEOUT = 10  # the injector's exit code when --timeout ran out
INJECTOR_DIED = 11
INJECTOR_NO_DLL = 12
INJECTOR_WAIT_FAILED = 13


def ghidra_version():
    """The pinned Ghidra version, such as 12.1.4."""
    with open(VERSIONS) as f:
        tag = json.load(f)['ghidra']['tag']
    return tag.removeprefix('Ghidra_').removesuffix('_build')


def parse_decompile(text):
    """Parse DecompileToEvidence.java's output file.

    Returns a list with one entry per QUERY record, in order:
    {'query': str, 'matches': [(addr, name)], 'function': (address, name) or
    None, 'callers': [(addr, name, from)], 'callees': [(addr, name)],
    'code': str}. Addresses other than the function's stay strings as the
    script wrote them. Records before the first QUERY record (or a file with
    none) form an entry whose query is None."""
    entries = []
    entry = None
    in_code = False
    code = []

    def finish():
        if entry is not None:
            entry['code'] = '\n'.join(code).strip('\n')

    for line in text.splitlines():
        kind, _, rest = line.partition('\t')
        if kind == 'QUERY':
            finish()
            entry = {'query': rest, 'matches': [], 'function': None,
                     'callers': [], 'callees': [], 'code': ''}
            entries.append(entry)
            in_code = False
            code = []
            continue
        if entry is None:
            entry = {'query': None, 'matches': [], 'function': None,
                     'callers': [], 'callees': [], 'code': ''}
            entries.append(entry)
        if in_code:
            code.append(line)
            continue
        fields = rest.split('\t')
        if kind == 'code':
            in_code = True
        elif kind == 'match':
            entry['matches'].append((fields[0], fields[1]))
        elif kind == 'function':
            entry['function'] = (int(fields[0], 16), fields[1])
        elif kind == 'caller':
            entry['callers'].append((fields[0], fields[1], fields[2]))
        elif kind == 'callee':
            entry['callees'].append((fields[0], fields[1]))
    finish()
    return entries


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


def decompile(queries):
    """Run DecompileToEvidence.java once on the Ghidra project for every
    query; returns the output file's text."""
    import make_project as mp
    if not mp.project_exists():
        sys.exit('oracle: no Ghidra project; run tools/ghidra/make_project.py')
    log = os.path.join(mp.PROJECT_DIR, 'headless-evidence.log')
    with tempfile.TemporaryDirectory(prefix='iwd2r-evidence-') as tmp:
        out = os.path.join(tmp, 'evidence.txt')
        r = mp.headless(['-process', mp.PROGRAM, '-noanalysis', '-readOnly',
                         '-postScript', 'DecompileToEvidence.java', out,
                         *queries], log)
        if (r.returncode != 0 or 'ERROR REPORT' in r.stdout
                or not os.path.exists(out)):
            sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
            sys.exit(f'oracle: analyzeHeadless failed ({r.returncode})')
        with open(out) as f:
            return f.read()


def cmd_evidence(args):
    """Draft every function in one Ghidra run. Ambiguous, missing and
    existing (without --force) functions are listed and skipped; the
    command then exits 1 after writing the rest."""
    entries = parse_decompile(decompile(args.function))
    failed = False
    for entry in entries:
        query = entry['query']
        if len(entry['matches']) > 1:
            print(f'{query} names {len(entry["matches"])} functions; '
                  'pass one address:')
            for addr, name in entry['matches']:
                print(f'  {addr} {name}')
            failed = True
            continue
        if entry['function'] is None:
            print(f'oracle: no function {query} in the Ghidra project')
            failed = True
            continue
        address, name = entry['function']
        path = os.path.join(EVIDENCE_DIR,
                            check_evidence.evidence_name(address))
        rel = os.path.relpath(path, ROOT)
        if os.path.exists(path) and not args.force:
            print(f'oracle: {rel} exists; --force replaces it')
            failed = True
            continue
        os.makedirs(EVIDENCE_DIR, exist_ok=True)
        with open(path, 'w') as f:
            f.write(render_evidence(entry, ghidra_version()))
        print(f'wrote {rel} (0x{address:X} {name}); fill in oracle_case or '
              'exemption and the Notes')
    return 1 if failed else 0


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
    moved under new, Resolution= set to VANILLA_RESOLUTION, and the volume
    keys set to 0 (mute_ini)."""
    text = re.sub(re.escape(old), lambda m: new, text, flags=re.IGNORECASE)
    text = re.sub(r'^(Resolution=)[^\r\n]*', r'\g<1>' + VANILLA_RESOLUTION,
                  text, flags=re.MULTILINE)
    return mute_ini(text)


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
    or name the vanilla MD5, or rebuild is set. The build holds
    VANILLA_LOCK, and the marker is checked again under it, so a copy
    another worktree has just built is not built twice."""
    if rebuild or not marker_ok(game):
        with filelock.hold(VANILLA_LOCK, 'vanilla'):
            if rebuild or not marker_ok(game):
                build(game)
    return game


def cmd_vanilla(args):
    game = ensure_vanilla(args.rebuild)
    print('vanilla copy: %s' % game)
    return 0


# ---------------------------------------------------------------------------
# Replays in the original (oracle.py replay record)

VOLUME_KEYS = ('Volume Movie', 'Volume Music', 'Volume Voices',
               'Volume Ambients', 'Volume SFX')


def mute_ini(text):
    """icewind2.ini's text with every volume key set to 0: the original runs
    on the host's desktop during recordings and would play through the
    speakers. Keys that are absent stay absent."""
    return re.sub(r'^(Volume (?:Movie|Music|Voices|Ambients|SFX)=)[^\r\n]*',
                  r'\g<1>0', text, flags=re.MULTILINE)


def replay_ini(text):
    """icewind2.ini's text for a recording: windowed (Full Screen=0) at
    Resolution=VANILLA_RESOLUTION, muted, everything else kept. Raises
    ValueError when either key is missing, rather than guessing where it
    belongs."""
    for key in ('Full Screen', 'Resolution'):
        if not re.search(r'^%s=' % key, text, re.MULTILINE):
            raise ValueError('icewind2.ini has no %s= line' % key)
    text = re.sub(r'^(Full Screen=)[^\r\n]*', r'\g<1>0', text,
                  flags=re.MULTILINE)
    text = re.sub(r'^(Resolution=)[^\r\n]*',
                  r'\g<1>' + VANILLA_RESOLUTION, text, flags=re.MULTILINE)
    return mute_ini(text)


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
    if code == INJECTOR_WAIT_FAILED:
        return 1, 'the injector could not wait for the game (%s)' % where
    if code != 0:
        return 1, 'the game exited with code %d (%s)' % (code, where)
    if status.get('state') == 'running':
        return 1, 'the game exited before the scenario finished (%s)' % where
    return 0, 'the game exited with code 0 (%s, scenario %s)' % (
        where, status.get('state', '?'))


def invariant_problem(name, dump):
    """Why a checkpoint dump breaks the recorder's invariant, or None.

    The invariant (schema.json "globals"): the game is not reinitializing,
    no frame so far stalled (ai_stalled_frames is 0), and no load is in
    progress (in_async_update and loading are 0). The virtual clock wakes
    the AI thread once per frame, so each frame has an AI update of its
    own; the calls a load draws from inside an AI update are held, not
    frames (platform/replay_clock.h FrameGate). A focus loss stops the AI
    updates from counting while frames go on: those frames are stalls. A
    checkpoint taken while the progress bar is up (loading) or inside an
    AI update would land in the middle of a load."""
    g = dump.get('globals') or {}
    frame = dump.get('frame')
    if g.get('reinitializing'):
        return ('%s: reinitializing is %s at frame %s: the game stopped '
                'updating (a focus loss?)' % (name, g['reinitializing'],
                                              frame))
    if g.get('ai_stalled_frames') != 0:
        return ('%s: %s frames by frame %s had no AI update (au_counter %s):'
                ' the AI thread stalled' % (name, g.get('ai_stalled_frames'),
                                            frame, g.get('au_counter')))
    if g.get('loading') or g.get('in_async_update'):
        return ('%s: a load is in progress at frame %s (loading %s, '
                'in_async_update %s): the checkpoint lands inside it'
                % (name, frame, g.get('loading'), g.get('in_async_update')))
    return None


def invariant_problems(replay_dir, dumps):
    """invariant_problem for every schema-valid checkpoint dump."""
    out = []
    for name in sorted(dumps):
        if not name.startswith('checkpoint-') or dumps[name]:
            continue
        with open(os.path.join(replay_dir, name)) as f:
            problem = invariant_problem(name, json.load(f))
        if problem:
            out.append(problem)
    return out


def missing_checkpoints(expected, dumps):
    """The scenario's checkpoint names with no checkpoint-<name>.json."""
    return [n for n in expected if 'checkpoint-%s.json' % n not in dumps]


def kill_windows_pid(pid):
    """Kill one Windows process (and so the injector's job) by id."""
    import play
    subprocess.run([play.CMD_EXE, '/c', 'taskkill', '/F', '/PID', str(pid)],
                   cwd=ORACLE_DIR, capture_output=True)


def run_injector(command, env, backstop):
    """Run the injector; (exit code or None, stdout, stderr). Its first line
    names its Windows pid: on Ctrl+C, or when the backstop timeout runs out
    (the injector kills the game at --timeout itself), exactly that process
    is killed, which closes its job and so kills this run's game."""
    proc = subprocess.Popen(command, cwd=ORACLE_DIR, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True)
    pid = None
    try:
        first = proc.stdout.readline()
        m = re.match(r'injector: pid (\d+)', first)
        pid = int(m.group(1)) if m else None
        out, err = proc.communicate(timeout=backstop)
        return proc.returncode, first + out, err
    except subprocess.TimeoutExpired:
        if pid:
            kill_windows_pid(pid)
        proc.kill()
        out, err = proc.communicate()
        return None, out, err
    except KeyboardInterrupt:
        if pid:
            kill_windows_pid(pid)
        proc.kill()
        raise


def recorder_env(script_win, replay_dir_win, seed, dump_every,
                 steal_focus=None, no_focus_guard=False, slow_reads=None):
    """The recorder's environment (oracle/replay/recorder.cpp), with
    Windows paths, in both modes."""
    names = {'IWD2R_SCRIPT': script_win,
             'IWD2R_REPLAY_DIR': replay_dir_win,
             'IWD2R_DUMP_EVERY': str(dump_every),
             'IWD2R_SEED': str(seed)}
    if steal_focus:
        names['IWD2R_TEST_STEAL_FOCUS'] = str(steal_focus)
    if no_focus_guard:
        names['IWD2R_TEST_NO_FOCUS_GUARD'] = '1'
    if slow_reads:
        names['IWD2R_TEST_SLOW_READS'] = str(slow_reads)
    return names


def record_original(script, actions, seed, timeout, dump_every, run_dir,
                    subdir='replay', steal_focus=None, no_focus_guard=False,
                    sandbox=True, slow_reads=None):
    """Record script in the original with the recorder DLL into
    run_dir/subdir, inside Windows Sandbox (record_in_sandbox) or with
    sandbox False on the host's desktop; prints the run as replay record
    does and returns its exit code (0, 1 or INJECTOR_TIMEOUT)."""
    import play
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
    stamp = os.path.basename(run_dir)
    out_dir = os.path.join(run_dir, subdir)
    print('replay %s (%d actions), seed %d, timeout %d s, dumps every %d '
          'frames, %s' % (os.path.relpath(script, ROOT), len(actions), seed,
                          timeout, dump_every,
                          'in Windows Sandbox' if sandbox else 'on the host'))
    extra = {}
    if sandbox:
        code, stdout, stderr, extra = record_in_sandbox(
            build, game, script, seed, timeout, dump_every, stamp, subdir,
            out_dir, steal_focus, no_focus_guard, slow_reads=slow_reads)
    else:
        code, stdout, stderr = record_on_host(
            injector, recorder, game, script, seed, timeout, dump_every,
            stamp, out_dir, steal_focus, no_focus_guard,
            slow_reads=slow_reads)
    for line in stdout.splitlines():
        print('  %s' % line)
    for line in stderr.splitlines():
        print('  stderr: %s' % line)
    status = read_status(out_dir)
    result, summary = replay_outcome(code, status, timeout)
    if extra.get('sandbox_error'):
        summary = 'sandbox: %s; %s' % (extra['sandbox_error'], summary)
        if result == 0:
            result = 1
    report = dict({'scenario': os.path.relpath(script, ROOT), 'seed': seed,
                   'timeout': timeout, 'dump_every': dump_every,
                   'mode': 'sandbox' if sandbox else 'host',
                   'injector_exit_code': code, 'status': status,
                   'outcome': summary}, **extra)
    return finish_replay(out_dir, schema, actions, result, summary, report)


def record_on_host(injector, recorder, game, script, seed, timeout,
                   dump_every, stamp, out_dir, steal_focus, no_focus_guard,
                   slow_reads=None):
    """The host mode: the injector runs on the host's desktop, the recorder
    writes into REPLAYS_DIR/<stamp>, moved to out_dir afterwards. Returns
    (exit code or None, stdout, stderr)."""
    replay_dir = os.path.join(REPLAYS_DIR, stamp)
    os.makedirs(replay_dir)
    shutil.copyfile(script, os.path.join(replay_dir, 'scenario.txt'))
    env = dict(os.environ)
    names = recorder_env(win_path(os.path.join(replay_dir, 'scenario.txt')),
                         win_path(replay_dir), seed, dump_every,
                         steal_focus, no_focus_guard, slow_reads)
    env.update(names)
    env['WSLENV'] = ':'.join([v for v in env.get('WSLENV', '').split(':')
                              if v] + sorted(names))
    command = [injector, win_path(os.path.join(game, 'iwd2.exe')),
               win_path(game), win_path(recorder),
               '--timeout', str(timeout)]
    print('running %s' % ' '.join(command))
    code, stdout, stderr = run_injector(command, env, timeout + 60)
    shutil.copytree(replay_dir, out_dir)
    shutil.rmtree(replay_dir, ignore_errors=True)
    return code, stdout, stderr


def sandbox_log(message):
    print('sandbox: %s' % message, flush=True)


def record_in_sandbox(build, game, script, seed, timeout, dump_every, stamp,
                      subdir, out_dir, steal_focus, no_focus_guard,
                      slow_reads=None):
    """The default mode: one fresh Windows Sandbox per recording
    (winsandbox.run_replay, tools/sandbox/replay-driver.ps1), holding
    winsandbox.SANDBOX_LOCK inside the caller's REPLAY_LOCK. Returns (exit
    code or None, stdout, stderr, extra report fields)."""
    import winsandbox
    if not winsandbox.available():
        sys.exit('oracle: %s; or record on the host with --host'
                 % winsandbox.ENABLE_HINT.replace('play: ', '', 1))
    winsandbox.deploy_runtime(os.path.join(build, INJECTOR), sandbox_log,
                              arch='x86')
    env = recorder_env(winsandbox.BOX_REPLAY_DIR + '\\scenario.txt',
                       winsandbox.BOX_REPLAY_DIR, seed, dump_every,
                       steal_focus, no_focus_guard, slow_reads)
    host_run = os.path.join(winsandbox.VM_ROOT, 'replays',
                            '%s-%s' % (stamp, subdir))
    with filelock.hold(winsandbox.SANDBOX_LOCK, 'sandbox'):
        try:
            result = winsandbox.run_replay(build, game, script, env,
                                           timeout, host_run, out_dir,
                                           sandbox_log)
        except RuntimeError as e:  # setting the run up failed
            sys.exit('oracle: %s' % e)
    shutil.rmtree(host_run, ignore_errors=True)
    print('sandbox: %.1f s from start to results (logs in %s)'
          % (result['seconds'], os.path.relpath(
              os.path.join(out_dir, 'sandbox'), ROOT)))
    extra = {'sandbox_seconds': round(result['seconds'], 1)}
    if result['error']:
        extra['sandbox_error'] = result['error']
    return result['code'], result['stdout'], result['stderr'], extra


def finish_replay(out_dir, schema, actions, result, summary, report):
    """Check a replay's dumps in out_dir (schema, invariant, the scenario's
    checkpoints), write out_dir/replay.json and print the summary; the exit
    code: result, or 1 when result is 0 and a check failed."""
    dumps = check_dumps(out_dir, schema)
    broken = invariant_problems(out_dir, dumps)
    expected = [a['name'] for a in actions if a['op'] == 'checkpoint']
    missing = missing_checkpoints(expected, dumps)
    invalid = {name: errors for name, errors in dumps.items() if errors}
    report = dict(report, dumps=sorted(dumps), invalid_dumps=invalid,
                  invariant_broken=broken, missing_checkpoints=missing)
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
    for problem in broken:
        print('  INVARIANT %s' % problem)
    if missing:
        print('  MISSING checkpoint dumps: %s'
              % ' '.join('checkpoint-%s.json' % n for n in missing))
    print('replay: %s' % os.path.relpath(out_dir, ROOT))
    if result == 0 and (invalid or broken or missing):
        result = 1
    return result


def load_actions(script):
    import realinput
    try:
        return realinput.load_scenario(script)
    except realinput.ScenarioError as e:
        sys.exit('oracle: %s' % e)


def cmd_replay_record(args):
    with filelock.hold(REPLAY_LOCK, 'replay'):
        return replay_record(args)


def replay_record(args):
    import play
    script = scenario_path(args.scenario)
    actions = load_actions(script)
    run_dir = play.make_run_folder(RUNS_DIR, datetime.datetime.now())
    return record_original(script, actions, args.seed, args.timeout,
                           args.dump_every, run_dir, 'replay',
                           args.test_steal_focus, args.test_no_focus_guard,
                           sandbox=not args.host,
                           slow_reads=args.test_slow_reads)


# ---------------------------------------------------------------------------
# Replay comparison (oracle.py replay compare)

def our_overrides():
    """The INI overrides our engine runs a comparison with: the copy's
    resolution, windowed, as the recorder runs the original."""
    return ['Program Options/Resolution=%s' % VANILLA_RESOLUTION,
            'Program Options/Full Screen=0']


def our_env(base, game, script, out_dir, seed, dump_every):
    """Our engine's environment for a comparison run: headless (SDL dummy
    drivers), the scenario, and the deterministic replay mode
    (IWD2R_STATE_DUMP, platform/replay.h) dumping into out_dir."""
    import play
    # Absolute: the engine changes into the game folder at start-up.
    env = play.child_env(base, game, our_overrides(), True, None,
                         os.path.abspath(script), os.path.abspath(out_dir))
    for name in ('IWD2R_SMOKE_FRAMES', 'IWD2R_CONTROL_DIR'):
        env.pop(name, None)
    env['IWD2R_STATE_DUMP'] = os.path.abspath(out_dir)
    env['IWD2R_SEED'] = str(seed)
    env['IWD2R_DUMP_EVERY'] = str(dump_every)
    return env


def our_outcome(code, status, timeout):
    """(exit code for oracle.py, one-line summary) of our engine's run."""
    where = 'last frame %s' % status.get('frame', '?')
    if status.get('script_line'):
        where += ', scenario line %s' % status['script_line']
    if code is None:
        return (INJECTOR_TIMEOUT, 'our engine timed out after %d s and was '
                'killed (%s)' % (timeout, where))
    if code != 0:
        return 1, 'our engine exited with code %d (%s)' % (code, where)
    if status.get('state') != 'finished':
        return 1, 'our engine exited before the scenario finished (%s)' % \
            where
    return 0, 'our engine exited with code 0 (%s, scenario finished)' % where


def run_ours(exe, game, script, actions, seed, timeout, dump_every, run_dir,
             subdir='ours'):
    """Run script in our engine's replay mode into run_dir/subdir; prints
    the run as record_original does and returns its exit code."""
    with open(REPLAY_SCHEMA) as f:
        schema = json.load(f)
    out_dir = os.path.join(run_dir, subdir)
    os.makedirs(out_dir)
    shutil.copyfile(script, os.path.join(out_dir, 'scenario.txt'))
    # A private view of the game folder: engines at once do not share its
    # temp/cache folders, and it is never written (IWD2R_KEEP_DATA_VIEW=1
    # keeps the view in the run folder).
    view = dataview.make_view(game, os.path.join(run_dir, 'data-' + subdir))
    env = our_env(os.environ, view, script, out_dir, seed, dump_every)
    print('ours %s (%d actions), seed %d, timeout %d s, dumps every %d '
          'frames' % (os.path.relpath(script, ROOT), len(actions), seed,
                      timeout, dump_every))
    print('running %s headless on %s' % (os.path.relpath(exe, ROOT), game))
    log_path = os.path.join(out_dir, 'log.txt')
    with open(log_path, 'w') as log:
        proc = subprocess.Popen([exe], cwd=ROOT, env=env, stdout=log,
                                stderr=subprocess.STDOUT)
        try:
            code = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
            code = None
        except KeyboardInterrupt:
            proc.kill()
            raise
        finally:
            if os.environ.get('IWD2R_KEEP_DATA_VIEW') != '1':
                dataview.remove_view(view)
    status = read_status(out_dir)
    result, summary = our_outcome(code, status, timeout)
    report = {'scenario': os.path.relpath(script, ROOT), 'seed': seed,
              'timeout': timeout, 'dump_every': dump_every,
              'exit_code': code, 'status': status, 'outcome': summary,
              'overrides': our_overrides()}
    return finish_replay(out_dir, schema, actions, result, summary, report)


# Reported for each checkpoint, never compared: when a frame lands and how
# the AI thread's updates line up with it are timing (ruling R13c), and the
# invariant on them is enforced on each side by finish_replay.
TIMING_FIELDS = ('frame', 'globals.au_counter', 'globals.ai_stalled_frames',
                 'globals.in_async_update', 'globals.held_frames')
SCREENS_JSON = os.path.join(ROOT, 'oracle', 'replay', 'screens.json')
KNOWN_DIFFERENCES = os.path.join(ROOT, 'oracle', 'replay',
                                 'known_differences.json')


def flatten(value, prefix=''):
    """A dump as {field path: value}: objects by dotted keys, lists by
    [index] (chargen.abilities[2]), so each difference names one value."""
    out = {}
    if isinstance(value, dict):
        for key, item in value.items():
            out.update(flatten(item, '%s.%s' % (prefix, key) if prefix
                               else key))
    elif isinstance(value, list):
        for i, item in enumerate(value):
            out.update(flatten(item, '%s[%d]' % (prefix, i)))
    else:
        out[prefix] = value
    return out


def field_differences(original, ours):
    """[(field, original value, our value)] over every compared field (all
    but 'checkpoint' and TIMING_FIELDS), in field order; a field one side
    lacks reads as '<missing>'."""
    a = flatten(original)
    b = flatten(ours)
    out = []
    for name in sorted(set(a) | set(b)):
        if name == 'checkpoint' or name in TIMING_FIELDS:
            continue
        va = a.get(name, '<missing>')
        vb = b.get(name, '<missing>')
        if va != vb:
            out.append((name, va, vb))
    return out


def load_screens(path=SCREENS_JSON):
    with open(path) as f:
        screens = json.load(f)
    if not isinstance(screens, list) or not all(isinstance(s, str)
                                                for s in screens):
        sys.exit('oracle: %s must be a list of screen names' % path)
    return screens


def known_evidence_path(evidence):
    """The evidence file an entry names: an address such as 0x72DE60 or a
    path relative to the repository."""
    if re.fullmatch(r'0[xX][0-9A-Fa-f]+', evidence):
        return os.path.join(ROOT, 'docs', 'evidence',
                            evidence[2:].upper() + '.md')
    return os.path.join(ROOT, evidence)


def load_known(path=KNOWN_DIFFERENCES):
    """known_differences.json: a list of {field, checkpoint, reason,
    evidence}; field is a flatten() path, checkpoint a scenario checkpoint
    name or "*" for every checkpoint. evidence names an existing file under
    docs/evidence, by address or path."""
    with open(path) as f:
        known = json.load(f)
    keys = {'field', 'checkpoint', 'reason', 'evidence'}
    if not isinstance(known, list):
        sys.exit('oracle: %s must be a list' % path)
    for entry in known:
        if not isinstance(entry, dict) or set(entry) != keys or not all(
                isinstance(entry[k], str) and entry[k] for k in keys):
            sys.exit('oracle: %s: each entry needs exactly %s, non-empty: %r'
                     % (path, ', '.join(sorted(keys)), entry))
    for entry in known:
        if not os.path.isfile(known_evidence_path(entry['evidence'])):
            sys.exit('oracle: %s: the evidence file %s does not exist'
                     % (path, entry['evidence']))
    return known


def stale_known(known, known_hits):
    """The known_differences entries that matched no difference."""
    used = {id(hit[4]) for hit in known_hits}
    return [entry for entry in known if id(entry) not in used]


def known_entry(known, checkpoint, field):
    """The known difference covering field at checkpoint, or None."""
    for entry in known:
        if entry['field'] == field and entry['checkpoint'] in ('*',
                                                               checkpoint):
            return entry
    return None


def compare_checkpoints(original_dir, our_dir, names, screens, known,
                        counts=None):
    """Compare the checkpoint dumps of two replay folders.

    Returns (lines, failures, known_hits): the report lines, the number of
    differences that fail the comparison (outside known_differences, at a
    checkpoint whose screen on either side is in screens, or a dump missing
    on one side) and the known differences met. A difference at a
    checkpoint where neither side is on an enabled screen is reported as
    not enabled and does not fail. A dict passed as counts receives
    'enabled', the checkpoints present on both sides with an enabled
    screen, and 'outside', the other present checkpoints that differed."""
    if counts is None:
        counts = {}
    counts['enabled'] = 0
    counts['outside'] = 0
    lines = []
    failures = 0
    known_hits = []
    for name in names:
        file_name = 'checkpoint-%s.json' % name
        dumps = []
        for folder in (original_dir, our_dir):
            try:
                with open(os.path.join(folder, file_name)) as f:
                    dumps.append(json.load(f))
            except (OSError, ValueError):
                dumps.append(None)
        original, ours = dumps
        if original is None or ours is None:
            lines.append('checkpoint %s: MISSING on %s' % (
                name, 'both sides' if original is None and ours is None
                else 'the original' if original is None else 'our side'))
            failures += 1
            continue
        timing = ', '.join('%s %s/%s' % (
            t.split('.')[-1], flatten(original).get(t, '?'),
            flatten(ours).get(t, '?')) for t in TIMING_FIELDS)
        enabled = original.get('screen') in screens or \
            ours.get('screen') in screens
        if enabled:
            counts['enabled'] += 1
        diffs = field_differences(original, ours)
        unknown = []
        for field, va, vb in diffs:
            entry = known_entry(known, name, field)
            if entry:
                known_hits.append((name, field, va, vb, entry))
            else:
                unknown.append((field, va, vb))
        if not unknown:
            state = 'match'
        elif enabled:
            state = 'DIFFERENT'
            failures += len(unknown)
        else:
            state = 'different, screen not enabled'
            counts['outside'] += 1
        lines.append('checkpoint %s (%s): %s (%s; original/ours)' % (
            name, original.get('screen'), state, timing))
        for field, va, vb in unknown:
            lines.append('  %s %s: original %s, ours %s' % (
                'DIFF' if enabled else 'diff', field, json.dumps(va),
                json.dumps(vb)))
        for hit in known_hits:
            if hit[0] == name:
                lines.append('  KNOWN %s: original %s, ours %s (%s; '
                             'evidence %s)' % (hit[1], json.dumps(hit[2]),
                                               json.dumps(hit[3]),
                                               hit[4]['reason'],
                                               hit[4]['evidence']))
    return lines, failures, known_hits


def cmd_replay_compare(args):
    with filelock.hold(REPLAY_LOCK, 'replay'):
        return replay_compare(args)


def replay_compare(args):
    import play
    script = scenario_path(args.scenario)
    actions = load_actions(script)
    names = [a['name'] for a in actions if a['op'] == 'checkpoint']
    if not names:
        sys.exit('oracle: %s has no checkpoint lines to compare' % script)
    screens = load_screens()
    known = load_known()
    exe = os.path.join(ROOT, 'build', play.LINUX_PRESET, 'iwd2r') \
        if args.no_build else play.build_linux()
    game = ensure_vanilla()
    prepare_replay_ini(game)
    run_dir = play.make_run_folder(RUNS_DIR, datetime.datetime.now())
    # Both sides start from the same INI files: the original rewrites
    # icewind2.ini on exit, and so does ours.
    backup = os.path.join(run_dir, 'ini-backup')
    ini_names = play.backup_inis(game, backup)
    try:
        print('== the original')
        original = record_original(script, actions, args.seed, args.timeout,
                                   args.dump_every, run_dir, 'original',
                                   sandbox=not args.host)
        play.restore_inis(backup, game, ini_names)
        print('== ours')
        ours = run_ours(exe, game, script, actions, args.seed, args.timeout,
                        args.dump_every, run_dir, 'ours')
    finally:
        play.restore_inis(backup, game, ini_names)
    print('== compare %s, seed %d (screens compared: %s)' % (
        os.path.relpath(script, ROOT), args.seed, ', '.join(screens)))
    counts = {}
    lines, failures, known_hits = compare_checkpoints(
        os.path.join(run_dir, 'original'), os.path.join(run_dir, 'ours'),
        names, screens, known, counts)
    for line in lines:
        print(line)
    for entry in stale_known(known, known_hits):
        stale = ('STALE known difference: %s at %s matched nothing; '
                 'remove it from %s' % (
                     entry['field'], entry['checkpoint'],
                     os.path.relpath(KNOWN_DIFFERENCES, ROOT)))
        lines.append(stale)
        print(stale)
    report = {'scenario': os.path.relpath(script, ROOT), 'seed': args.seed,
              'original_result': original, 'our_result': ours,
              'lines': lines, 'failures': failures,
              'known': [{'checkpoint': h[0], 'field': h[1], 'original': h[2],
                         'ours': h[3]} for h in known_hits]}
    with open(os.path.join(run_dir, 'compare.json'), 'w') as f:
        json.dump(report, f, indent=2)
        f.write('\n')
    for line in compare_verdict(failures, original, ours, known_hits,
                                counts):
        print(line)
    print('compare: %s' % os.path.relpath(run_dir, ROOT))
    return 0 if failures == 0 and original == 0 and ours == 0 else 1


def compare_verdict(failures, original, ours, known_hits, counts):
    """The result lines of replay compare: failures and known_hits from
    compare_checkpoints, original and ours the two runs' exit codes, and
    counts the dict compare_checkpoints filled. A match counts only the
    checkpoints on enabled screens; a second line says how many others
    differed."""
    runs_ok = original == 0 and ours == 0
    if failures == 0 and runs_ok:
        verdict = 'MATCH at %d of %d checkpoints' % (counts['enabled'],
                                                     counts['enabled'])
        if known_hits:
            verdict += ', with %d known difference%s' % (
                len(known_hits), '' if len(known_hits) == 1 else 's')
    else:
        verdict = 'DIFFERENT: %d difference%s' % (
            failures, '' if failures == 1 else 's')
        if not runs_ok:
            verdict += '; a run failed (original %d, ours %d)' % (original,
                                                                  ours)
    lines = ['result: %s' % verdict]
    if counts['outside']:
        lines.append('note: %d checkpoint%s differed outside the enabled '
                     'screens' % (counts['outside'],
                                  '' if counts['outside'] == 1 else 's'))
    return lines


def positive_int(text):
    value = int(text) if re.fullmatch(r'\d+', text) else 0
    if value <= 0:
        raise argparse.ArgumentTypeError('expected a positive integer: %r'
                                         % text)
    return value


def non_negative_int(text):
    if not re.fullmatch(r'\d+', text):
        raise argparse.ArgumentTypeError('expected an integer >= 0: %r'
                                         % text)
    return int(text)


def seed_arg(text):
    """--seed: the original srand takes an unsigned 32-bit seed."""
    value = non_negative_int(text)
    if value > 0xFFFFFFFF:
        raise argparse.ArgumentTypeError('expected a seed below 2^32: %r'
                                         % text)
    return value


HOST_HELP = ('run the original on the host\'s desktop instead of in Windows '
             'Sandbox (for debugging: it takes the focus, and a stray click '
             'lands in the recording)')


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='command', required=True)
    ev = sub.add_parser('evidence', help='draft docs/evidence/<ADDR>.md '
                        'for one or more functions')
    ev.add_argument('function', nargs='+', help='address (0x5845D0) or full '
                    'name (IcewindMisc::Roll); several are drafted in one '
                    'Ghidra run')
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
    run.add_argument('--seed', type=seed_arg, default=1,
                     help='seed for the original rand and the generators '
                     '(default %(default)s)')
    run.add_argument('--iterations', type=positive_int, default=10000,
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
    record.add_argument('--seed', type=seed_arg, default=1,
                        help='seed for the original srand (default '
                        '%(default)s)')
    record.add_argument('--timeout', type=positive_int,
                        default=REPLAY_TIMEOUT,
                        help='seconds before the game is killed (default '
                        '%(default)s)')
    record.add_argument('--dump-every', type=non_negative_int,
                        default=REPLAY_DUMP_EVERY,
                        help='a dump every N frames, 0 for none (default '
                        '%(default)s)')
    record.add_argument('--test-steal-focus', type=positive_int, metavar='N',
                        help='test: a window of the recorder takes the '
                        'foreground at frame N')
    record.add_argument('--test-no-focus-guard', action='store_true',
                        help='test: let activation messages reach the game')
    record.add_argument('--test-slow-reads', type=positive_int, metavar='MS',
                        help='test: from the first frame on, every '
                        'ReadFile off the main thread sleeps MS ms first (a '
                        'slow disk)')
    record.add_argument('--host', action='store_true', help=HOST_HELP)
    record.set_defaults(run=cmd_replay_record)
    compare = replay_sub.add_parser(
        'compare', help='run a scenario in the original and in our engine '
        'with the same seed and compare every checkpoint')
    compare.add_argument('scenario', help='scenarios/<name>.txt by name, '
                         'or a path')
    compare.add_argument('--seed', type=seed_arg, default=1,
                         help='seed for both sides\' main-thread srand '
                         '(default %(default)s)')
    compare.add_argument('--timeout', type=positive_int,
                         default=REPLAY_TIMEOUT,
                         help='seconds before either side is killed '
                         '(default %(default)s)')
    compare.add_argument('--dump-every', type=non_negative_int,
                         default=REPLAY_DUMP_EVERY,
                         help='a dump every N frames on both sides, 0 for '
                         'none (default %(default)s)')
    compare.add_argument('--no-build', action='store_true',
                         help='use the existing build/linux-gcc/iwd2r')
    compare.add_argument('--host', action='store_true', help=HOST_HELP)
    compare.set_defaults(run=cmd_replay_compare)
    args = ap.parse_args(argv)
    return args.run(args)


if __name__ == '__main__':
    sys.exit(main())
