# iwd2r. Distributed under LICENSE.md.
"""Build and run iwd2r in one command, optionally driven by an input script.

Usage (from WSL, in the repository):

  python3 tools/play.py                 # Windows build, game window on the desktop
  python3 tools/play.py --linux --headless --script scenarios/options.txt
  python3 tools/play.py --vm linux --suite --jobs 3

--vm runs scenarios with genuine operating-system input in an isolated
display instead of the in-engine script: `linux` on an Xvfb display driven
with xdotool (tools/realinput.py), `windows` in Windows Sandbox driven with
SendInput (tools/winsandbox.py, tools/sandbox/driver.ps1), `all` both.
--suite runs every scenarios/*.txt.

Windows runs (the default) copy the working tree (tracked files plus modified
and untracked non-ignored ones) into a persistent checkout at
C:\\Users\\thier\\iwd2r-build, build it there with MSVC, and launch the exe.
--linux builds and runs build/linux-gcc instead.

Game options never touch the INI files: they reach the engine through the
IWD2R_INI_OVERRIDE environment variable. Every *.ini in the install root is
still backed up before the run and restored byte-identical afterwards (also
on Ctrl-C), because the engine rewrites icewind2.ini on exit as the original
does.

Each run gets runs/<YYYYMMDD-HHMMSS>/ in the repository: log.txt (the game's
stdout and stderr), the screenshots, ini-backup/ and summary.txt. The exit
code is the game's.

Standard library only.
"""

import argparse
import concurrent.futures
import datetime
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time

# The sibling modules (python3 -I does not put the script's folder on the
# path).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import realinput  # noqa: E402
import winsandbox  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DATA = '/mnt/c/Games/Icewind Dale II'
WIN_CHECKOUT = '/mnt/c/Users/thier/iwd2r-build'
CMD_EXE = '/mnt/c/Windows/System32/cmd.exe'
VCVARS = r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
CMAKE_WIN = r'C:\Program Files\CMake\bin\cmake.exe'
WIN_PRESET = 'windows-msvc-x64'
# The preset's build configuration (CMakePresets.json).
WIN_EXE_REL = 'build/windows-msvc-x64/RelWithDebInfo/iwd2r.exe'
LINUX_PRESET = 'linux-gcc'
SYNC_EXCLUDE = ('build/', 'runs/')
SCENARIOS = os.path.join(ROOT, 'scenarios')
VM_MODES = ('linux', 'windows', 'all')
SET_RE = re.compile(r'^[^/=;]+/[^=;]+=[^;]*$')


def log(msg):
    print('play: ' + msg, flush=True)


# ---------------------------------------------------------------------------
# Pure parts (unit-tested in tests/test_tools.py)

def parse_args(argv):
    ap = argparse.ArgumentParser(
        prog='play.py', description='Build and run iwd2r.')
    ap.add_argument('--linux', action='store_true',
                    help='build and run build/linux-gcc instead of Windows')
    ap.add_argument('--headless', action='store_true',
                    help='SDL dummy video and audio drivers (no window)')
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('--windowed', dest='fullscreen', action='store_false',
                      help='Program Options/Full Screen=0 (default)')
    mode.add_argument('--fullscreen', dest='fullscreen', action='store_true',
                      help='Program Options/Full Screen=1')
    ap.set_defaults(fullscreen=False)
    ap.add_argument('--resolution', type=int, choices=(800, 1024), default=800,
                    help='Program Options/Resolution (default 800)')
    ap.add_argument('--set', dest='sets', action='append', default=[],
                    metavar='Section/Key=Value',
                    help='any other INI override (repeatable)')
    ap.add_argument('--frames', type=int, metavar='N',
                    help='exit after N presented frames (IWD2R_SMOKE_FRAMES)')
    ap.add_argument('--script', metavar='FILE',
                    help='input script (see platform/script.h)')
    ap.add_argument('--data', metavar='PATH',
                    help='install directory (default: IWD2_DATA, else '
                    + DEFAULT_DATA + ')')
    ap.add_argument('--no-build', action='store_true',
                    help='run the existing build without syncing or building')
    ap.add_argument('--vm', choices=VM_MODES,
                    help='real OS input in an isolated display: linux (Xvfb '
                    '+ xdotool), windows (Windows Sandbox) or all')
    ap.add_argument('--suite', action='store_true',
                    help='with --vm: run every scenarios/*.txt')
    ap.add_argument('--jobs', type=int, default=1, metavar='N',
                    help='with --vm linux --suite: scenarios run at once '
                    '(default 1)')
    ap.add_argument('--timeout', type=int, default=realinput.DEFAULT_TIMEOUT,
                    metavar='S', help='with --vm: per-scenario timeout in '
                    'seconds (default %d)' % realinput.DEFAULT_TIMEOUT)
    args = ap.parse_args(argv)
    if args.vm:
        if not args.suite and not args.script:
            ap.error('--vm needs --script FILE or --suite')
        if args.suite and args.script:
            ap.error('--suite and --script are exclusive')
        for flag, value in (('--linux', args.linux),
                            ('--headless', args.headless),
                            ('--frames', args.frames is not None),
                            ('--fullscreen', args.fullscreen)):
            if value:
                ap.error('%s does not apply to --vm runs' % flag)
    elif args.suite:
        ap.error('--suite needs --vm')
    if args.jobs < 1:
        ap.error('--jobs must be positive')
    if args.timeout <= 0:
        ap.error('--timeout must be positive')
    for entry in args.sets:
        if not SET_RE.match(entry):
            ap.error('--set expects "Section/Key=Value" without ";": %r' % entry)
    if args.frames is not None and args.frames <= 0:
        ap.error('--frames must be positive')
    return args


def build_overrides(resolution, fullscreen, sets):
    """The IWD2R_INI_OVERRIDE entries, in order (a later entry wins)."""
    entries = [
        'Program Options/Resolution=%d' % resolution,
        'Program Options/Full Screen=%d' % (1 if fullscreen else 0),
    ]
    entries.extend(sets)
    return entries


def run_folder_name(now):
    return now.strftime('%Y%m%d-%H%M%S')


def make_run_folder(runs_dir, now):
    """Creates runs_dir/<stamp> (with -2, -3... if taken) and returns it."""
    base = os.path.join(runs_dir, run_folder_name(now))
    path = base
    n = 2
    while os.path.exists(path):
        path = '%s-%d' % (base, n)
        n += 1
    os.makedirs(path)
    return path


def ini_files(install):
    return sorted(name for name in os.listdir(install)
                  if name.lower().endswith('.ini')
                  and os.path.isfile(os.path.join(install, name)))


def backup_inis(install, backup_dir):
    """Copies every *.ini in the install root to backup_dir; returns names."""
    os.makedirs(backup_dir, exist_ok=True)
    names = ini_files(install)
    for name in names:
        shutil.copy2(os.path.join(install, name), os.path.join(backup_dir, name))
    return names


def restore_inis(backup_dir, install, names):
    """Writes each backed-up INI back byte-identical; returns the names whose
    content had changed. Never deletes anything in the install."""
    changed = []
    for name in names:
        src = os.path.join(backup_dir, name)
        dst = os.path.join(install, name)
        with open(src, 'rb') as f:
            want = f.read()
        have = None
        if os.path.exists(dst):
            with open(dst, 'rb') as f:
                have = f.read()
        if have != want:
            changed.append(name)
        shutil.copy2(src, dst)
    return changed


def child_env(base, data, overrides, headless, frames, script, shot_dir):
    """The variables the game gets on top of base (a dict)."""
    env = dict(base)
    env['IWD2_DATA'] = data
    env['IWD2R_INI_OVERRIDE'] = ';'.join(overrides)
    env['IWD2R_SHOT_DIR'] = shot_dir
    if headless:
        env['SDL_VIDEO_DRIVER'] = 'dummy'
        env['SDL_AUDIO_DRIVER'] = 'dummy'
    if frames:
        env['IWD2R_SMOKE_FRAMES'] = str(frames)
    if script:
        env['IWD2R_SCRIPT'] = script
    return env


def batch_set(name, value):
    """A cmd.exe `set` line that keeps value verbatim."""
    return 'set "%s=%s"' % (name, value.replace('%', '%%'))


def sync_list(paths):
    """The files to copy to the Windows checkout: drops build/ and runs/."""
    return [p for p in paths if p and not p.startswith(SYNC_EXCLUDE)]


# ---------------------------------------------------------------------------
# Running

def stream(cmd, log_file=None, **kw):
    """Runs cmd, echoing its output (and appending it to log_file)."""
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, **kw)
    try:
        for raw in proc.stdout:
            line = raw.decode('utf-8', errors='replace')
            sys.stdout.write(line)
            sys.stdout.flush()
            if log_file:
                log_file.write(line)
                log_file.flush()
        return proc.wait()
    except BaseException:
        proc.terminate()
        proc.wait()
        raise


def wslpath(flag, path):
    return subprocess.run(['wslpath', flag, path], check=True,
                          capture_output=True, text=True).stdout.strip()


def resolve_data(arg):
    data = arg or os.environ.get('IWD2_DATA') or DEFAULT_DATA
    if re.match(r'^[A-Za-z]:[\\/]', data):
        data = wslpath('-u', data)
    data = os.path.abspath(data)
    if not os.path.isfile(os.path.join(data, 'CHITIN.KEY')) and \
            not os.path.isfile(os.path.join(data, 'chitin.key')):
        sys.exit('play: %s has no CHITIN.KEY; pass --data' % data)
    return data


def build_linux():
    build_dir = os.path.join(ROOT, 'build', LINUX_PRESET)
    if not os.path.isfile(os.path.join(build_dir, 'CMakeCache.txt')):
        log('configuring %s' % LINUX_PRESET)
        if stream(['cmake', '--preset', LINUX_PRESET], cwd=ROOT) != 0:
            sys.exit('play: configure failed')
    log('building %s (target iwd2r)' % LINUX_PRESET)
    if stream(['cmake', '--build', '--preset', LINUX_PRESET,
               '--target', 'iwd2r'], cwd=ROOT) != 0:
        sys.exit('play: build failed')
    return os.path.join(build_dir, 'iwd2r')


def sync_windows():
    os.makedirs(WIN_CHECKOUT, exist_ok=True)
    out = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others',
                          '--exclude-standard'], cwd=ROOT, check=True,
                         capture_output=True).stdout.decode('utf-8')
    files = [p for p in sync_list(out.split('\0'))
             if os.path.lexists(os.path.join(ROOT, p))]
    log('syncing %d files to %s' % (len(files), WIN_CHECKOUT))
    proc = subprocess.run(['rsync', '-rlt', '--from0', '--files-from=-',
                           ROOT + '/', WIN_CHECKOUT + '/'],
                          input='\0'.join(files).encode('utf-8'))
    if proc.returncode != 0:
        sys.exit('play: rsync failed')


def build_windows():
    for path in (VCVARS, CMAKE_WIN):
        if not os.path.exists(wslpath('-u', path)):
            sys.exit('play: missing %s' % path)
    win_root = wslpath('-w', WIN_CHECKOUT)
    configured = os.path.isfile(os.path.join(
        WIN_CHECKOUT, 'build', WIN_PRESET, 'CMakeCache.txt'))
    lines = [
        '@echo off',
        'call "%s" >nul || exit /b 1' % VCVARS,
        'cd /d "%s" || exit /b 1' % win_root,
    ]
    if not configured:
        lines.append('"%s" --preset %s || exit /b 1' % (CMAKE_WIN, WIN_PRESET))
    lines.append('"%s" --build --preset %s --target iwd2r || exit /b 1'
                 % (CMAKE_WIN, WIN_PRESET))
    bat = os.path.join(WIN_CHECKOUT, 'play-build.bat')
    with open(bat, 'w', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')
    if not configured:
        log('first Windows build: configuring and compiling SDL3 takes several '
            'minutes')
    log('building %s with MSVC (target iwd2r)' % WIN_PRESET)
    started = time.time()
    if stream([CMD_EXE, '/c', wslpath('-w', bat)], cwd=WIN_CHECKOUT) != 0:
        sys.exit('play: Windows build failed')
    log('build done in %.0f s' % (time.time() - started))
    exe = os.path.join(WIN_CHECKOUT, WIN_EXE_REL)
    if not os.path.isfile(exe):
        sys.exit('play: build produced no %s' % exe)
    return exe


def run_linux(exe, env, log_file):
    log('running %s' % exe)
    return stream([exe], log_file=log_file, env=env, cwd=ROOT)


def run_windows(exe, env_vars, win_run, log_file):
    lines = ['@echo off']
    for name in sorted(env_vars):
        lines.append(batch_set(name, env_vars[name]))
    lines.append('"%s"' % wslpath('-w', exe))
    lines.append('exit /b %ERRORLEVEL%')
    bat = os.path.join(win_run, 'run.bat')
    with open(bat, 'w', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')
    log('running %s' % wslpath('-w', exe))
    try:
        return stream([CMD_EXE, '/c', wslpath('-w', bat)], log_file=log_file,
                      cwd=win_run)
    except KeyboardInterrupt:
        subprocess.run([CMD_EXE, '/c', 'taskkill', '/F', '/IM', 'iwd2r.exe'],
                       cwd=WIN_CHECKOUT, capture_output=True)
        raise


def write_summary(run_dir, argv, platform, code, overrides, script, shots,
                  seconds, restored):
    with open(os.path.join(run_dir, 'summary.txt'), 'w') as f:
        f.write('command: python3 tools/play.py %s\n' % ' '.join(argv))
        f.write('platform: %s\n' % platform)
        f.write('exit code: %s\n' % code)
        f.write('duration: %.1f s\n' % seconds)
        f.write('overrides:\n')
        for o in overrides:
            f.write('  %s\n' % o)
        f.write('script: %s\n' % (script or '(none)'))
        f.write('screenshots:\n')
        for s in shots:
            f.write('  %s\n' % s)
        f.write('ini restored: %s\n' % restored)


# ---------------------------------------------------------------------------
# Real-input runs (--vm)

def suite_files(directory=SCENARIOS):
    return sorted(glob.glob(os.path.join(directory, '*.txt')))


def scenario_name(path):
    return os.path.splitext(os.path.basename(path))[0]


def format_table(title, rows):
    """rows: (scenario, code, seconds, screenshots) tuples."""
    lines = [title,
             '%-20s %6s %9s %6s' % ('scenario', 'exit', 'duration', 'shots')]
    for name, code, seconds, shots in rows:
        lines.append('%-20s %6s %8.1fs %6d' % (name, code, seconds, shots))
    passed = sum(1 for r in rows if r[1] == 0)
    lines.append('%d of %d passed' % (passed, len(rows)))
    return '\n'.join(lines) + '\n'


def result_row(name, result):
    return (name, result['code'], result['seconds'],
            result['screen_shots'] + result['engine_shots'])


def write_vm_summary(run_dir, argv, platform, script, overrides, result):
    shots = sorted(n for n in os.listdir(run_dir) if n.endswith('.png'))
    write_summary(run_dir, argv, platform, result['code'], overrides, script,
                  shots, result['seconds'],
                  'n/a (the sandbox runs a copy of the install)'
                  if platform == 'windows-vm'
                  else 'after the run (ini-backup/ in the run folder)')
    if result['error']:
        with open(os.path.join(run_dir, 'summary.txt'), 'a') as f:
            f.write('driver error: %s\n' % result['error'])


def prepare_scenario_dir(run_dir, script, actions):
    os.makedirs(run_dir, exist_ok=True)
    shutil.copy2(script, os.path.join(run_dir, 'script.txt'))
    with open(os.path.join(run_dir, 'actions.json'), 'w') as f:
        json.dump(actions, f, indent=1)


def run_linux_vm(args, argv, scenarios, data, overrides, base_dir, nested):
    """Runs the scenarios on Xvfb displays; returns the table rows."""
    missing = realinput.missing_tools()
    if missing:
        log('missing %s: sudo apt-get install -y xvfb xdotool imagemagick'
            % ', '.join(missing))
        return None
    exe = os.path.join(ROOT, 'build', LINUX_PRESET, 'iwd2r') \
        if args.no_build else build_linux()
    backup = os.path.join(base_dir, 'ini-backup')
    names = backup_inis(data, backup)
    log('backed up %d INI files to %s' % (len(names), backup))
    base_env = child_env(os.environ, data, overrides, False, None, None,
                         base_dir)
    taken = set()
    abort = threading.Event()
    rows = {}
    # Parallel jobs each get a core of their own (the engine pins itself to
    # CPU 0); a single job keeps the engine's own choice.
    slots = list(range(args.jobs))
    slots_lock = threading.Lock()
    cpus = os.sched_getaffinity(0)

    def one(path, actions):
        with slots_lock:
            slot = slots.pop(0)
        try:
            run_one(path, actions, slot)
        finally:
            with slots_lock:
                slots.append(slot)

    def run_one(path, actions, slot):
        name = scenario_name(path)
        run_dir = os.path.join(base_dir, name) if nested else base_dir
        prepare_scenario_dir(run_dir, path, actions)
        log('linux: %s on Xvfb (real input)' % name)
        result = realinput.run_scenario(exe, base_env, actions, run_dir,
                                        args.resolution, args.timeout, taken,
                                        cwd=ROOT, abort=abort,
                                        cpu=realinput.job_cpu(slot, cpus)
                                        if args.jobs > 1 else None)
        write_vm_summary(run_dir, argv, 'linux-vm', path, overrides, result)
        log('linux: %s exit %s in %.1f s%s' % (
            name, result['code'], result['seconds'],
            ' (%s)' % result['error'] if result['error'] else ''))
        rows[path] = result_row(name, result)

    try:
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            futures = [pool.submit(one, p, a) for p, a in scenarios]
            try:
                for f in futures:
                    f.result()
            except KeyboardInterrupt:
                log('interrupted: stopping the running scenarios')
                abort.set()
                for f in futures:
                    f.cancel()
                raise
    finally:
        changed = restore_inis(backup, data, names)
        log('restored %d INI files in %s byte-identical (%d had changed%s)' % (
            len(names), data, len(changed),
            ': ' + ', '.join(changed) if changed else ''))
    return [rows[p] for p, _ in scenarios if p in rows]


def build_windows_vm(args):
    if not args.no_build:
        sync_windows()
        exe = build_windows()
    else:
        exe = os.path.join(WIN_CHECKOUT, WIN_EXE_REL)
    winsandbox.deploy_runtime(exe, log)
    return exe


def run_windows_vm(args, argv, scenarios, data, overrides, base_dir, nested):
    """Runs the scenarios one after another in Windows Sandbox (one sandbox
    can run at a time); returns the table rows."""
    if not winsandbox.available():
        print(winsandbox.ENABLE_HINT, flush=True)
        return None
    exe = build_windows_vm(args)
    winsandbox.refresh_cache(data, winsandbox.GAME_CACHE, log)
    rows = []
    stamp = os.path.relpath(base_dir, os.path.join(ROOT, 'runs'))
    for path, actions in scenarios:
        name = scenario_name(path)
        run_dir = os.path.join(base_dir, name) if nested else base_dir
        prepare_scenario_dir(run_dir, path, actions)
        winsandbox.reset_cache_inis(data, winsandbox.GAME_CACHE)
        host_run = os.path.join(winsandbox.VM_ROOT, 'runs', stamp, name)
        os.makedirs(host_run, exist_ok=True)
        log('windows: %s in Windows Sandbox (real input)' % name)
        result = winsandbox.run_scenario(
            exe, winsandbox.GAME_CACHE, realinput.windows_actions(actions),
            overrides, name, run_dir, host_run, args.timeout,
            realinput.FRAME_RATE, log)
        write_vm_summary(run_dir, argv, 'windows-vm', path, overrides,
                         result)
        log('windows: %s exit %s in %.1f s%s' % (
            name, result['code'], result['seconds'],
            ' (%s)' % result['error'] if result['error'] else ''))
        rows.append(result_row(name, result))
    return rows


def main_vm(args, argv):
    if args.vm == 'windows' and not winsandbox.available():
        print(winsandbox.ENABLE_HINT, flush=True)
        return 2
    paths = suite_files() if args.suite else [os.path.abspath(args.script)]
    if not paths:
        sys.exit('play: no scenarios in %s' % SCENARIOS)
    scenarios = []
    for path in paths:
        if not os.path.isfile(path):
            sys.exit('play: no script %s' % path)
        try:
            scenarios.append((path, realinput.load_scenario(path)))
        except realinput.ScenarioError as e:
            sys.exit('play: %s' % e)
    data = resolve_data(args.data)
    overrides = build_overrides(args.resolution, False, args.sets)
    run_root = make_run_folder(os.path.join(ROOT, 'runs'),
                               datetime.datetime.now())
    platforms = ['linux', 'windows'] if args.vm == 'all' else [args.vm]
    tables = []
    codes = []
    try:
        for platform in platforms:
            base = os.path.join(run_root, platform) if len(platforms) > 1 \
                else run_root
            os.makedirs(base, exist_ok=True)
            runner = run_linux_vm if platform == 'linux' else run_windows_vm
            rows = runner(args, argv, scenarios, data, overrides, base,
                          args.suite)
            if rows is None:
                tables.append('%s: not run (see above)\n' % platform)
                codes.append(2 if args.vm != 'all' or platform == 'linux'
                             else 0)
                continue
            tables.append(format_table(platform, rows))
            codes.extend(r[1] for r in rows)
    except KeyboardInterrupt:
        log('interrupted')
        codes.append(130)
    text = '\n'.join(tables)
    print('\n' + text, end='', flush=True)
    if args.suite:
        with open(os.path.join(run_root, 'suite.txt'), 'w') as f:
            f.write('command: python3 tools/play.py %s\n\n' % ' '.join(argv))
            f.write(text)
    log('run folder %s' % run_root)
    failed = [c for c in codes if c != 0]
    return failed[0] if failed else 0


def main(argv):
    args = parse_args(argv)
    if args.vm:
        return main_vm(args, argv)
    headless = args.headless
    if args.linux and not headless and not os.environ.get('DISPLAY') \
            and not os.environ.get('WAYLAND_DISPLAY'):
        log('no DISPLAY or WAYLAND_DISPLAY: running headless')
        headless = True
    data = resolve_data(args.data)
    script = os.path.abspath(args.script) if args.script else None
    if script and not os.path.isfile(script):
        sys.exit('play: no script %s' % script)
    overrides = build_overrides(args.resolution, args.fullscreen, args.sets)

    if args.linux:
        exe = os.path.join(ROOT, 'build', LINUX_PRESET, 'iwd2r') \
            if args.no_build else build_linux()
    else:
        if not args.no_build:
            sync_windows()
        exe = build_windows() if not args.no_build else \
            os.path.join(WIN_CHECKOUT, WIN_EXE_REL)

    now = datetime.datetime.now()
    run_dir = make_run_folder(os.path.join(ROOT, 'runs'), now)
    if script:
        shutil.copy2(script, os.path.join(run_dir, 'script.txt'))
    backup = os.path.join(run_dir, 'ini-backup')
    names = backup_inis(data, backup)
    log('backed up %d INI files to %s' % (len(names), backup))

    code = None
    restored = 'no'
    started = time.time()
    log_path = os.path.join(run_dir, 'log.txt')
    try:
        with open(log_path, 'w') as log_file:
            if args.linux:
                env = child_env(os.environ, data, overrides, headless,
                                args.frames, script, run_dir)
                code = run_linux(exe, env, log_file)
            else:
                win_run = os.path.join(WIN_CHECKOUT, 'runs',
                                       os.path.basename(run_dir))
                os.makedirs(win_run)
                win_script = None
                if script:
                    shutil.copy2(script, os.path.join(win_run, 'script.txt'))
                    win_script = wslpath('-w', os.path.join(win_run, 'script.txt'))
                env_vars = child_env({}, wslpath('-w', data), overrides,
                                     headless, args.frames, win_script,
                                     wslpath('-w', win_run))
                try:
                    code = run_windows(exe, env_vars, win_run, log_file)
                finally:
                    for name in os.listdir(win_run):
                        if name not in ('run.bat', 'script.txt'):
                            shutil.copy2(os.path.join(win_run, name),
                                         os.path.join(run_dir, name))
    except KeyboardInterrupt:
        log('interrupted')
        code = 130
    finally:
        changed = restore_inis(backup, data, names)
        restored = 'yes (%d files, %d had changed: %s)' % (
            len(names), len(changed), ', '.join(changed) or 'none')
        log('restored %d INI files in %s byte-identical (%d had changed%s)' % (
            len(names), data, len(changed),
            ': ' + ', '.join(changed) if changed else ''))
        shots = sorted(n for n in os.listdir(run_dir) if n.endswith('.png'))
        write_summary(run_dir, argv, 'linux' if args.linux else 'windows',
                      code, overrides, script, shots, time.time() - started,
                      restored)
    log('exit code %s' % code)
    for s in shots:
        log('screenshot %s' % os.path.join(run_dir, s))
    log('run folder %s' % run_dir)
    return code if code is not None else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
