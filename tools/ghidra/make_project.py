"""Create (or refresh) the Ghidra project for the vanilla iwd2.exe.

    python3 tools/ghidra/make_project.py [--src DIR] [--extra FILE] [--ieex DB]
                                         [--exe EXE]

The project lives at ~/.local/share/iwd2r/ghidra-projects/iwd2/ ($IWD2R_HOME
overrides ~/.local/share/iwd2r). On the first run the vanilla exe is copied to
$IWD2R_HOME/vanilla/iwd2.exe (MD5 checked; the install is only read), imported,
labelled by ImportLabels.java as a pre-script (names from src/, then
tools/ghidra/extra_labels.txt, then IEex's), fully analysed, and labelled
again as a post-script. Later runs re-apply the labels to the existing project
without re-importing or re-analysing it.
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import labels  # noqa: E402

HOME = os.environ.get('IWD2R_HOME',
                      os.path.expanduser('~/.local/share/iwd2r'))
GHIDRA = os.path.join(HOME, 'ghidra')
HEADLESS = os.path.join(GHIDRA, 'support', 'analyzeHeadless')
JDK = '/usr/lib/jvm/java-21-openjdk-amd64'
PROJECT_DIR = os.path.join(HOME, 'ghidra-projects', 'iwd2')
PROJECT_NAME = 'iwd2'
PROGRAM = 'iwd2.exe'
VANILLA = os.path.join(HOME, 'vanilla', PROGRAM)
INSTALL_EXE = '/mnt/c/Games/Icewind Dale II/iwd2ee/backup/1000/iwd2.exe'
EXTRA_LABELS = os.path.join(HERE, 'extra_labels.txt')
IEEX_DB = '/mnt/c/Games/Icewind Dale II/function_names.db'
VANILLA_MD5 = '200f038bec3ed50a964d68b3f0f1f0e7'
MAXMEM = '8G'


def md5(path):
    h = hashlib.md5()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def vanilla_exe(install_exe):
    """The scratch copy of the vanilla exe, copied from the install once."""
    if not os.path.exists(VANILLA):
        if md5(install_exe) != VANILLA_MD5:
            sys.exit(f'make_project: {install_exe} is not the vanilla exe '
                     f'(MD5 {VANILLA_MD5})')
        os.makedirs(os.path.dirname(VANILLA), exist_ok=True)
        shutil.copyfile(install_exe, VANILLA)
    if md5(VANILLA) != VANILLA_MD5:
        sys.exit(f'make_project: {VANILLA} MD5 mismatch; delete it and rerun')
    return VANILLA


def project_exists():
    return os.path.exists(os.path.join(PROJECT_DIR, PROJECT_NAME + '.gpr'))


def headless(args, log):
    """Run analyzeHeadless on the project; returns the CompletedProcess.

    Ghidra refuses project paths with an element starting with '.', such as
    ~/.local, so the project is reached through a temporary symlink."""
    env = dict(os.environ, JAVA_HOME=JDK, GHIDRA_HEADLESS_MAXMEM=MAXMEM)
    env['PATH'] = os.path.join(JDK, 'bin') + os.pathsep + env.get('PATH', '')
    with tempfile.TemporaryDirectory(prefix='iwd2r-ghidra-') as tmp:
        if any(part.startswith('.') for part in tmp.split(os.sep)):
            sys.exit(f'make_project: temporary directory {tmp} has a '
                     "path element starting with '.'; set TMPDIR")
        link = os.path.join(tmp, 'iwd2')
        os.symlink(PROJECT_DIR, link)
        cmd = [HEADLESS, link, PROJECT_NAME, *args,
               '-scriptPath', HERE, '-log', log]
        return subprocess.run(cmd, env=env, capture_output=True, text=True)


def write_labels(path, src, db, extra_path=EXTRA_LABELS):
    """labels.tsv: ADDR, source (ours, extra or ieex) and name per line."""
    ours = labels.ours(src)
    extra = labels.extra(extra_path)
    ieex = labels.ieex(db)
    merged = labels.merge(ours, ieex, extra)
    ours_addrs = {a for a, _ in ours}
    extra_addrs = {a for a, _ in extra} - ours_addrs
    with open(path, 'w') as f:
        for addr, name in merged:
            source = ('ours' if addr in ours_addrs else
                      'extra' if addr in extra_addrs else 'ieex')
            f.write(f'{addr:X}\t{source}\t{name}\n')
    named = ours_addrs | extra_addrs
    prefixed = sum(1 for a, n in merged
                   if a not in named and n.startswith('ieex_'))
    return {'ours_lines': len(ours), 'ours': len(ours_addrs),
            'extra_lines': len(extra), 'extra': len(extra_addrs),
            'ieex_lines': len(ieex),
            'ieex': len(merged) - len(named), 'prefixed': prefixed}


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--src', default=os.path.join(ROOT, 'src'))
    p.add_argument('--extra', default=EXTRA_LABELS)
    p.add_argument('--ieex', default=IEEX_DB)
    p.add_argument('--exe', default=INSTALL_EXE)
    a = p.parse_args(argv)

    if not os.access(HEADLESS, os.X_OK):
        sys.exit(f'make_project: {HEADLESS} missing; run tools/ghidra/setup.sh')
    exe = vanilla_exe(a.exe)
    os.makedirs(PROJECT_DIR, exist_ok=True)
    tsv = os.path.join(PROJECT_DIR, 'labels.tsv')
    summary = os.path.join(PROJECT_DIR, 'labels-summary.txt')
    counts = write_labels(tsv, a.src, a.ieex, a.extra)
    print(f"labels: ours {counts['ours']} addresses "
          f"({counts['ours_lines']} definitions), "
          f"extra {counts['extra']} of {counts['extra_lines']}, "
          f"ieex {counts['ieex']} of {counts['ieex_lines']} named "
          f"({counts['prefixed']} prefixed ieex_)")

    script = ['ImportLabels.java', tsv, summary]
    if project_exists():
        mode = 'refresh'
        args = ['-process', PROGRAM, '-noanalysis', '-postScript', *script]
    else:
        mode = 'create'
        args = ['-import', exe, '-max-cpu', str(os.cpu_count() or 4),
                '-preScript', *script, '-postScript', *script]
    log = os.path.join(PROJECT_DIR, f'headless-{mode}.log')
    print(f'{mode}: {PROJECT_DIR} (log {log})', flush=True)
    start = time.monotonic()
    r = headless(args, log)
    elapsed = time.monotonic() - start
    with open(os.path.join(PROJECT_DIR, f'headless-{mode}.out'), 'w') as f:
        f.write(r.stdout + r.stderr)
    if r.returncode != 0 or 'ERROR REPORT' in r.stdout:
        sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
        sys.exit(f'make_project: analyzeHeadless failed ({r.returncode})')
    print(f'{mode} finished in {elapsed / 60:.1f} min')
    with open(summary) as f:
        lines = f.read().splitlines()
    failures = [l for l in lines if l.startswith('failure\t')]
    for line in lines:
        if not line.startswith(('failure\t', 'renamed\t')):
            print('applied ' + line.replace('\t', ': '))
    print(f'failures: {len(failures)} (listed in {summary})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
