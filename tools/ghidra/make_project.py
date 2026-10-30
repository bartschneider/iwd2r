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
sys.path.insert(0, os.path.dirname(HERE))
import filelock  # noqa: E402
import labels  # noqa: E402

HOME = os.environ.get('IWD2R_HOME',
                      os.path.expanduser('~/.local/share/iwd2r'))
GHIDRA = os.path.join(HOME, 'ghidra')
HEADLESS = os.path.join(GHIDRA, 'support', 'analyzeHeadless')
DEBIAN_JDK = '/usr/lib/jvm/java-21-openjdk-amd64'
PROJECT_DIR = os.path.join(HOME, 'ghidra-projects', 'iwd2')
PROJECT_NAME = 'iwd2'
# One headless Ghidra at a time across every worktree (filelock.hold).
LOCK_PATH = os.path.join(PROJECT_DIR, '..', 'iwd2.lock')
PROGRAM = 'iwd2.exe'
# Read-only runs (evidence drafts, queries) use a copy of the project per
# worktree, each with its own lock, so parallel worktrees do not queue behind
# one Ghidra. GENERATION_PATH changes whenever make_project writes the main
# project; a copy made from an older generation is made again.
CLONES_DIR = os.path.join(HOME, 'ghidra-projects', 'clones')
GENERATION_PATH = os.path.join(PROJECT_DIR, 'generation')
VANILLA = os.path.join(HOME, 'vanilla', PROGRAM)
INSTALL_EXE = '/mnt/c/Games/Icewind Dale II/iwd2ee/backup/1000/iwd2.exe'
EXTRA_LABELS = os.path.join(HERE, 'extra_labels.txt')
IEEX_DB = '/mnt/c/Games/Icewind Dale II/function_names.db'
VANILLA_MD5 = '200f038bec3ed50a964d68b3f0f1f0e7'
MAXMEM = '8G'


def jdk_path():
    """The JDK that runs Ghidra: $JAVA_HOME when it is a directory, else the
    Debian apt path when that exists, else the java on PATH (`.../bin/java`,
    so two levels up). Distros disagree on the path; Ghidra itself only needs
    a Java 21 to start."""
    home = os.environ.get('JAVA_HOME', '')
    if home and os.path.isdir(home):
        return home
    if os.path.isdir(DEBIAN_JDK):
        return DEBIAN_JDK
    java = shutil.which('java')
    if java:
        return os.path.dirname(os.path.dirname(os.path.realpath(java)))
    return DEBIAN_JDK


JDK = jdk_path()


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


def generation():
    """The main project's generation stamp ('0' before the first bump)."""
    try:
        with open(GENERATION_PATH) as f:
            return f.read().strip() or '0'
    except FileNotFoundError:
        return '0'


def bump_generation():
    with open(GENERATION_PATH + '.tmp', 'w') as f:
        f.write(f'{time.time_ns()}\n')
    os.replace(GENERATION_PATH + '.tmp', GENERATION_PATH)


def _ignore_locks(directory, names):
    # Ghidra's own lock files (iwd2.lock, iwd2.lock~) and our run logs stay
    # with the main project.
    return [n for n in names if '.lock' in n or n.startswith('headless-')]


def worktree_clone(worktree=None):
    """The read-only copy of the project for this worktree, made or remade
    from the main project when missing or from an older generation.
    Returns (project_dir, lock_path)."""
    name = os.path.basename(os.path.normpath(worktree or ROOT))
    base = os.path.join(CLONES_DIR, name)
    clone = os.path.join(base, PROJECT_NAME)
    lock = os.path.join(base, PROJECT_NAME + '.lock')
    stamp = os.path.join(base, 'generation')
    os.makedirs(base, exist_ok=True)
    with filelock.hold(lock, 'ghidra clone ' + name):
        want = generation()
        try:
            with open(stamp) as f:
                have = f.read().strip()
        except FileNotFoundError:
            have = None
        if have != want or not os.path.exists(
                os.path.join(clone, PROJECT_NAME + '.gpr')):
            fresh = clone + '.new'
            shutil.rmtree(fresh, ignore_errors=True)
            # Copy under the main lock so a refresh cannot change it midway.
            with filelock.hold(LOCK_PATH, 'ghidra'):
                want = generation()
                shutil.copytree(PROJECT_DIR, fresh, ignore=_ignore_locks)
            shutil.rmtree(clone, ignore_errors=True)
            os.replace(fresh, clone)
            with open(stamp, 'w') as f:
                f.write(want + '\n')
    return clone, lock


def headless(args, log, read_only=False, project=None, project_name=None):
    """Run analyzeHeadless on the project; returns the CompletedProcess.

    Ghidra refuses project paths with an element starting with '.', such as
    ~/.local, so the project is reached through a temporary symlink. A
    read-only run uses this worktree's copy and holds only its lock; any
    other run uses the main project and holds LOCK_PATH, so a second
    worktree's run waits for this one. Another project (its directory and
    name, e.g. a DLL project of make_dll_project) can be passed instead; it
    still runs under LOCK_PATH."""
    lock = LOCK_PATH
    if project is None or project_name is None:
        project, project_name = PROJECT_DIR, PROJECT_NAME
    if read_only:
        project, lock = worktree_clone()
        log = os.path.join(os.path.dirname(project), os.path.basename(log))
    env = dict(os.environ, JAVA_HOME=JDK, GHIDRA_HEADLESS_MAXMEM=MAXMEM)
    env['PATH'] = os.path.join(JDK, 'bin') + os.pathsep + env.get('PATH', '')
    with tempfile.TemporaryDirectory(prefix='iwd2r-ghidra-') as tmp:
        if any(part.startswith('.') for part in tmp.split(os.sep)):
            sys.exit(f'make_project: temporary directory {tmp} has a '
                     "path element starting with '.'; set TMPDIR")
        link = os.path.join(tmp, project_name)
        os.symlink(project, link)
        cmd = [HEADLESS, link, project_name, *args,
               '-scriptPath', HERE, '-log', log]
        with filelock.hold(lock, 'ghidra'):
            return subprocess.run(cmd, env=env, capture_output=True,
                                  text=True)


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
    p.add_argument('--ieex', default=IEEX_DB,
                   help='IEex function_names.db (default: %(default)s); a '
                        'missing file means no IEex names')
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
    bump_generation()  # worktree copies remake themselves from this one
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
