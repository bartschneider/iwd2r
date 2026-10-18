"""Create (or reuse) the Ghidra project for one DLL.

    python3 tools/ghidra/make_dll_project.py <name> <dll>

e.g.  python3 tools/ghidra/make_dll_project.py ieexhelper-fork \
          $IWD2R_HOME/wso/dll/IEexHelper.fork.dll

Imports <dll> into $IWD2R_HOME/ghidra-projects/<name> and analyses it. A
project whose <name>.gpr already exists is reused untouched; delete the
directory to rebuild it. The import runs through make_project.headless, so
it holds the ghidra lock while headless runs (one analysis at a time across
every worktree) and gets the same JDK/Ghidra environment.
"""

import argparse
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import make_project  # noqa: E402

PROJECTS_DIR = os.path.join(make_project.HOME, 'ghidra-projects')
NAME_RE = re.compile(r'[A-Za-z0-9][A-Za-z0-9_-]*')


def project_dir(name):
    return os.path.join(PROJECTS_DIR, name)


def make(name, dll):
    """Import dll into its own analysed project; returns the project dir."""
    if not NAME_RE.fullmatch(name):
        raise ValueError(f'project name must match {NAME_RE.pattern}: '
                         f'{name!r}')
    if not os.path.isfile(dll):
        raise FileNotFoundError(dll)
    target = project_dir(name)
    if os.path.exists(os.path.join(target, name + '.gpr')):
        return target
    os.makedirs(target, exist_ok=True)
    log = os.path.join(target, 'headless-import.log')
    args = ['-import', os.path.abspath(dll),
            '-max-cpu', str(os.cpu_count() or 4)]
    print(f'import: {dll} -> {target}', flush=True)
    start = time.monotonic()
    r = make_project.headless(args, log, project=target, project_name=name)
    with open(os.path.join(target, 'headless-import.out'), 'w') as f:
        f.write(r.stdout + r.stderr)
    if r.returncode != 0 or 'ERROR REPORT' in r.stdout:
        sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
        sys.exit(f'make_dll_project: analyzeHeadless failed ({r.returncode})')
    print(f'import finished in {time.monotonic() - start:.1f} s')
    return target


def run_script(name, program, script, *script_args):
    """Run one Ghidra script against the program of project <name>.

    One file per script argument, so Ghidra compiles them at run time from
    -scriptPath HERE (see DllScope.java, DllDelta.java,
    DecompileFunctions.java).
    """
    target = project_dir(name)
    if not os.path.exists(os.path.join(target, name + '.gpr')):
        sys.exit(f'make_dll_project: no project {target}; make it first')
    log = os.path.join(target, f'headless-{script.split(".")[0].lower()}.log')
    args = ['-process', program, '-noanalysis',
            '-postScript', script, *script_args]
    r = make_project.headless(args, log, project=target, project_name=name)
    with open(log.replace('.log', '.out'), 'w') as f:
        f.write(r.stdout + r.stderr)
    if r.returncode != 0 or 'ERROR REPORT' in r.stdout:
        sys.stderr.write(r.stdout[-4000:] + r.stderr[-4000:])
        sys.exit(f'make_dll_project: {script} failed ({r.returncode})')
    return r


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('name', help='project name (ghidra-projects/<name>)')
    p.add_argument('dll', help='the DLL to import')
    a = p.parse_args(argv)
    if not os.access(make_project.HEADLESS, os.X_OK):
        sys.exit(f'make_dll_project: {make_project.HEADLESS} missing; '
                 'run tools/ghidra/setup.sh')
    print(make(a.name, a.dll))
    return 0


if __name__ == '__main__':
    sys.exit(main())
