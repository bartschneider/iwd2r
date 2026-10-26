"""Build and run iwd2r in one command, optionally driven by an input script.

Usage (from WSL, in the repository):

  python3 tools/play.py                 # Windows build, game window on the desktop
  python3 tools/play.py --linux --headless --script scenarios/options.txt
  python3 tools/play.py --vm linux --suite --jobs 3

--vm runs scenarios with genuine operating-system input in an isolated
display instead of the in-engine script: `linux` on an Xvfb display driven
with xdotool (tools/realinput.py), `windows` in Windows Sandbox driven with
SendInput (tools/winsandbox.py, tools/sandbox/driver.ps1), `all` both.
--suite runs every scenarios/*.txt, not the subfolders: scenarios/pending/
holds scenarios our engine cannot finish yet (a milestone's exit scenario),
which move up to scenarios/ when they pass.

Windows runs (the default) copy the working tree (tracked files plus modified
and untracked non-ignored ones) into a persistent checkout, build it there
with MSVC, and launch the exe. Each worktree has its own checkout:
%USERPROFILE%\\iwd2r-build for the `first` worktree,
%USERPROFILE%\\iwd2r-build-<worktree folder name> for any other, or
$IWD2R_WIN_CHECKOUT (a WSL path) when set. %USERPROFILE% is the folder
tools/winhome.py resolves ($IWD2R_WIN_HOME, default /mnt/c/Users/<you>). --linux builds and runs
build/linux-gcc instead.

Windows Sandbox runs one sandbox at a time, so every --vm windows scenario
holds %USERPROFILE%\\iwd2r-vm\\sandbox.lock (tools/filelock.py) while it
prepares the shared game cache and runs; another worktree's run waits.

The game data is --data, else IWD2_DATA, else a vanilla copy: for Linux runs
${IWD2R_HOME:-~/.local/share/iwd2r}/game (as tools/gates.sh), for Windows runs
(the host build and --vm windows) %USERPROFILE%\\iwd2r-oracle\\game, the
verified copy `oracle.py vanilla` builds. When the copy is missing the run
stops and says how to make it; the IWD2EE install is never the default.

Game options never touch the INI files: they reach the engine through the
IWD2R_INI_OVERRIDE environment variable. Every *.ini in the install root is
still backed up before the run and restored byte-identical afterwards (also
on Ctrl-C), because the engine rewrites icewind2.ini on exit as the original
does.

Every run starts the engine on a private view of the install
(tools/dataview.py): links to the install plus real, private temp/, cache/,
INI files and the other folders the engine writes, in the run folder's data/
(on the Windows drive for Windows runs). Runs at once do not collide, and
the install is never written. The view is removed after the run unless
--keep-data-view is given.

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
import attempt  # noqa: E402
import dataview  # noqa: E402
import filelock  # noqa: E402
import oracle  # noqa: E402
import realinput  # noqa: E402
import winhome  # noqa: E402
import winsandbox  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# The default game data is a vanilla copy, never the IWD2EE install under
# /mnt/c/Games: Linux runs read a Linux-local copy (as tools/gates.sh does),
# Windows runs the verified copy oracle.py vanilla builds. IWD2_DATA and
# --data override both.
WINDOWS_DATA = oracle.GAME_DIR
LINUX_COPY_HINT = ('rsync -a --exclude temp --exclude cache --exclude '
                   'tempsave --exclude mpsave --exclude ScrnShot '
                   + oracle.GAME_DIR + '/ ~/.local/share/iwd2r/game/')
WIN_BUILD_BASE = winhome.WIN_HOME + '/iwd2r-build'


def win_checkout(root, env):
    """The Windows build folder of the worktree at root: IWD2R_WIN_CHECKOUT
    when set; the original folder for the `first` worktree, so it keeps its
    build; iwd2r-build-<worktree name> for any other."""
    if env.get('IWD2R_WIN_CHECKOUT'):
        return env['IWD2R_WIN_CHECKOUT']
    name = os.path.basename(os.path.normpath(root))
    if name == 'first':
        return WIN_BUILD_BASE
    return WIN_BUILD_BASE + '-' + name


WIN_CHECKOUT = win_checkout(ROOT, os.environ)
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
# The desktop --vm runs give Resolution=desktop (the Xvfb screen).
VM_DESKTOP = (1920, 1080)


def log(msg):
    print('play: ' + msg, flush=True)


# ---------------------------------------------------------------------------
# Pure parts (unit-tested in tests/test_tools.py)

def resolution_arg(value):
    """--resolution: a width in pixels or 'desktop'."""
    if value.lower() == 'desktop':
        return 'desktop'
    if value.isdigit() and int(value) > 0:
        return int(value)
    raise argparse.ArgumentTypeError('expected a width or "desktop": %r'
                                     % value)


def scale_arg(value):
    """--scale: 1 to 4 or 'auto'."""
    if value.lower() == 'auto':
        return 'auto'
    if value in ('1', '2', '3', '4'):
        return int(value)
    raise argparse.ArgumentTypeError('expected 1, 2, 3, 4 or auto: %r'
                                     % value)


def set_value(sets, key):
    """The last --set value for 'Section/Key' (case-insensitive), or
    None."""
    found = None
    for entry in sets:
        name, _, value = entry.partition('=')
        if name.lower() == key.lower():
            found = value
    return found


def vm_height(resolution, sets):
    """The requested (output) height of a --vm run: --set "Program
    Options/Height=H", else the desktop's for Resolution=desktop, else the
    original 4:3 heights, else 16:9 (the Xvfb desktop is the output size,
    so the engine derives the same)."""
    height = set_value(sets, 'Program Options/Height')
    if height and height.isdigit() and int(height) > 0:
        return int(height)
    if resolution == 'desktop':
        return VM_DESKTOP[1]
    if resolution in realinput.ORIGINAL_HEIGHTS:
        return realinput.ORIGINAL_HEIGHTS[resolution]
    return (resolution * 9 + 8) // 16


def vm_geometry(resolution, scale, sets):
    """The screen of a --vm run (realinput.resolve_screen), with the Scale
    and World Scale the --set entries give winning over scale. Raises
    ValueError below 800x600 inside or for a bad scale."""
    scale_set = set_value(sets, 'Program Options/Scale')
    if scale_set is not None:
        scale = scale_arg(scale_set.strip())
    return realinput.resolve_screen(resolution, vm_height(resolution, sets),
                                    scale, VM_DESKTOP,
                                    set_value(sets,
                                              'Program Options/World Scale'))


def parse_args(argv):
    ap = argparse.ArgumentParser(
        prog='play.py', description='Build and run iwd2r.')
    ap.add_argument('--linux', action='store_true',
                    help='build and run build/linux-gcc instead of Windows')
    ap.add_argument('--sound', action='store_true',
                    help='let the game play sound (runs are silent otherwise)')
    ap.add_argument('--headless', action='store_true',
                    help='SDL dummy video and audio drivers (no window)')
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('--windowed', dest='fullscreen', action='store_false',
                      help='Program Options/Full Screen=0 (default)')
    mode.add_argument('--fullscreen', dest='fullscreen', action='store_true',
                      help='Program Options/Full Screen=1')
    ap.set_defaults(fullscreen=False)
    ap.add_argument('--resolution', type=resolution_arg, default=800,
                    help='Program Options/Resolution: a width or desktop '
                    '(default 800); the height comes from --set '
                    '"Program Options/Height=H" (else 4:3 for 800, 1024, '
                    '1600 and 2048, else the desktop\'s aspect, 16:9 with '
                    '--vm)')
    ap.add_argument('--scale', type=scale_arg,
                    help='Program Options/Scale: 1-4 or auto (with --vm the '
                    'default is 1)')
    ap.add_argument('--set', dest='sets', action='append', default=[],
                    metavar='Section/Key=Value',
                    help='any other INI override (repeatable)')
    ap.add_argument('--frames', type=int, metavar='N',
                    help='exit after N presented frames (IWD2R_SMOKE_FRAMES)')
    ap.add_argument('--script', metavar='FILE',
                    help='input script (see platform/script.h)')
    ap.add_argument('--shot-presented', action='store_true',
                    help='each screenshot also writes <name>-presented.png, '
                    'the frame as the presenter drew it (IWD2R_SHOT_PRESENTED)')
    ap.add_argument('--data', metavar='PATH',
                    help='install directory (default: IWD2_DATA, else the '
                    'vanilla copy: $IWD2R_HOME/game or '
                    '~/.local/share/iwd2r/game for Linux runs, '
                    + WINDOWS_DATA + ' for Windows runs)')
    ap.add_argument('--no-build', action='store_true',
                    help='run the existing build without syncing or building')
    ap.add_argument('--keep-data-view', action='store_true',
                    help='keep the run\'s private data/ view of the install '
                    '(removed after the run otherwise)')
    ap.add_argument('--vm', choices=VM_MODES,
                    help='real OS input in an isolated display: linux (Xvfb '
                    '+ xdotool), windows (Windows Sandbox) or all')
    ap.add_argument('--suite', action='store_true',
                    help='with --vm: run every scenarios/*.txt '
                    '(not scenarios/pending/)')
    ap.add_argument('--jobs', type=int, default=1, metavar='N',
                    help='with --vm linux --suite: scenarios run at once '
                    '(default 1)')
    ap.add_argument('--timeout', type=int, metavar='S',
                    help='with --vm: per-scenario timeout in seconds, for '
                    'every scenario (default: the scenario header\'s '
                    '"# timeout: S", else %d)' % realinput.DEFAULT_TIMEOUT)
    ap.add_argument('--new-attempt', metavar='REASON',
                    help='start a new attempt: reset the launch count '
                    '(%d launches per attempt) and record why'
                    % attempt.LIMIT)
    args = ap.parse_args(argv)
    bad = attempt.reason_error(args.new_attempt)
    if bad:
        ap.error(bad)
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
    if args.timeout is not None and args.timeout <= 0:
        ap.error('--timeout must be positive')
    for entry in args.sets:
        if not SET_RE.match(entry):
            ap.error('--set expects "Section/Key=Value" without ";": %r' % entry)
    if args.frames is not None and args.frames <= 0:
        ap.error('--frames must be positive')
    if args.vm:
        try:
            vm_geometry(args.resolution, args.scale or 1, args.sets)
        except (ValueError, argparse.ArgumentTypeError) as e:
            ap.error(str(e))
    return args


def build_overrides(resolution, fullscreen, sets, scale=None, height=None):
    """The IWD2R_INI_OVERRIDE entries, in order (a later entry wins)."""
    entries = [
        'Program Options/Resolution=%s' % resolution,
        'Program Options/Full Screen=%d' % (1 if fullscreen else 0),
    ]
    if scale is not None:
        entries.append('Program Options/Scale=%s' % scale)
    if height is not None:
        entries.append('Program Options/Height=%d' % height)
    entries.extend(sets)
    return entries


def vm_overrides(args):
    """build_overrides for a --vm run and its geometry. Scale and Height are
    always explicit, so the INI's own values cannot disagree with the
    driver's."""
    scale = args.scale or 1
    sets = [e for e in args.sets
            if e.partition('=')[0].lower() != 'program options/height']
    overrides = build_overrides(args.resolution, False, sets, scale,
                                vm_height(args.resolution, args.sets))
    return overrides, vm_geometry(args.resolution, scale, args.sets)


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


def child_env(base, data, overrides, headless, frames, script, shot_dir,
              sound=False, shot_presented=False):
    """The variables the game gets on top of base (a dict)."""
    env = dict(base)
    env['IWD2_DATA'] = data
    env['IWD2R_INI_OVERRIDE'] = ';'.join(overrides)
    env['IWD2R_SHOT_DIR'] = shot_dir
    # Silent unless --sound: on WSL with WSLg the game's audio reaches the
    # Windows speakers.
    if not sound:
        env['IWD2R_AUDIO'] = 'null'
    if headless:
        env['SDL_VIDEO_DRIVER'] = 'dummy'
        env['SDL_AUDIO_DRIVER'] = 'dummy'
    if frames:
        env['IWD2R_SMOKE_FRAMES'] = str(frames)
    if script:
        env['IWD2R_SCRIPT'] = script
    if shot_presented:
        env['IWD2R_SHOT_PRESENTED'] = '1'
    return env


def batch_set(name, value):
    """A cmd.exe `set` line that keeps value verbatim."""
    return 'set "%s=%s"' % (name, value.replace('%', '%%'))


def sync_list(paths):
    """The files to copy to the Windows checkout: drops build/ and runs/."""
    return [p for p in paths if p and not p.startswith(SYNC_EXCLUDE)]


# ---------------------------------------------------------------------------
# Running

def stream(cmd, log_file=None, echo=True, **kw):
    """Runs cmd, echoing its output (unless echo is False) and appending it
    to log_file."""
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, **kw)
    try:
        for raw in proc.stdout:
            line = raw.decode('utf-8', errors='replace')
            if echo:
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


def linux_data(env):
    """The Linux-local vanilla copy: ${IWD2R_HOME:-$HOME/.local/share/iwd2r}
    /game, as in tools/gates.sh."""
    home = env.get('IWD2R_HOME') or os.path.join(
        env.get('HOME') or os.path.expanduser('~'), '.local', 'share',
        'iwd2r')
    return os.path.join(home, 'game')


def has_key(directory, isfile=os.path.isfile):
    return isfile(os.path.join(directory, 'CHITIN.KEY')) or \
        isfile(os.path.join(directory, 'chitin.key'))


def default_data(platform, env, isfile=os.path.isfile,
                 marker_ok=oracle.marker_ok):
    """The vanilla copy a run on platform ('linux' or 'windows') uses when
    neither --data nor IWD2_DATA is given. Raises SystemExit naming how to
    make it when it is missing: the install is never the fallback."""
    if platform == 'linux':
        data = linux_data(env)
        if has_key(data, isfile):
            return data
        sys.exit('play: no vanilla copy of the game for Linux runs at %s '
                 '(no CHITIN.KEY); build the verified copy with "python3 '
                 'tools/oracle.py vanilla", then copy it as tools/gates.sh '
                 'says: %s; or pass --data / set IWD2_DATA'
                 % (data, LINUX_COPY_HINT))
    data = WINDOWS_DATA
    if has_key(data, isfile) and marker_ok(data):
        return data
    sys.exit('play: no verified vanilla copy of the game for Windows runs '
             'at %s (CHITIN.KEY and %s); build it with "python3 '
             'tools/oracle.py vanilla" (the Linux copy is made from it with: '
             '%s); or pass --data / set IWD2_DATA'
             % (data, oracle.MARKER, LINUX_COPY_HINT))


def resolve_data(arg, platform, env=None):
    """--data, else IWD2_DATA, else the platform's vanilla copy
    (default_data)."""
    env = os.environ if env is None else env
    data = arg or env.get('IWD2_DATA') or default_data(platform, env)
    if re.match(r'^[A-Za-z]:[\\/]', data):
        data = wslpath('-u', data)
    data = os.path.abspath(data)
    if not has_key(data):
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


def sync_windows(root=None, checkout=None):
    """Copies the worktree at root (default this one) into its Windows
    checkout (default WIN_CHECKOUT): tracked files plus modified and
    untracked non-ignored ones."""
    root = root or ROOT
    checkout = checkout or WIN_CHECKOUT
    os.makedirs(checkout, exist_ok=True)
    out = subprocess.run(['git', 'ls-files', '-z', '--cached', '--others',
                          '--exclude-standard'], cwd=root, check=True,
                         capture_output=True).stdout.decode('utf-8')
    files = [p for p in sync_list(out.split('\0'))
             if os.path.lexists(os.path.join(root, p))]
    log('syncing %d files to %s' % (len(files), checkout))
    proc = subprocess.run(['rsync', '-rlt', '--from0', '--files-from=-',
                           root + '/', checkout + '/'],
                          input='\0'.join(files).encode('utf-8'))
    if proc.returncode != 0:
        sys.exit('play: rsync failed')


def build_windows(preset=WIN_PRESET, target='iwd2r', exe_rel=WIN_EXE_REL,
                  bat_name='play-build.bat', checkout=None, log_path=None):
    """Builds target with the Windows CMake preset in checkout (default
    WIN_CHECKOUT); returns the WSL path of exe_rel. Defaults build the game
    (play.py's use). With log_path the compiler output goes to that file
    instead of the terminal. A failure exits (SystemExit)."""
    checkout = checkout or WIN_CHECKOUT
    for path in (VCVARS, CMAKE_WIN):
        if not os.path.exists(wslpath('-u', path)):
            sys.exit('play: missing %s' % path)
    win_root = wslpath('-w', checkout)
    configured = os.path.isfile(os.path.join(
        checkout, 'build', preset, 'CMakeCache.txt'))
    lines = [
        '@echo off',
        'call "%s" >nul || exit /b 1' % VCVARS,
        'cd /d "%s" || exit /b 1' % win_root,
    ]
    if not configured:
        lines.append('"%s" --preset %s || exit /b 1' % (CMAKE_WIN, preset))
    lines.append('"%s" --build --preset %s --target %s || exit /b 1'
                 % (CMAKE_WIN, preset, target))
    bat = os.path.join(checkout, bat_name)
    with open(bat, 'w', newline='\r\n') as f:
        f.write('\n'.join(lines) + '\n')
    if not configured:
        log('first Windows build: configuring and compiling SDL3 takes several '
            'minutes')
    log('building %s with MSVC (target %s) in %s' % (preset, target, checkout))
    started = time.time()
    if log_path:
        with open(log_path, 'a', encoding='utf-8') as out:
            rc = stream([CMD_EXE, '/c', wslpath('-w', bat)], log_file=out,
                        echo=False, cwd=checkout)
    else:
        rc = stream([CMD_EXE, '/c', wslpath('-w', bat)], cwd=checkout)
    if rc != 0:
        sys.exit('play: Windows build failed%s' % (
            ' (log: %s)' % log_path if log_path else ''))
    log('build done in %.0f s' % (time.time() - started))
    exe = os.path.join(checkout, exe_rel)
    if not os.path.isfile(exe):
        sys.exit('play: build produced no %s' % exe)
    return exe


X86_EXE_REL = 'build/windows-msvc-x86/RelWithDebInfo/iwd2r.exe'


def build_windows_x86(bat_name='play-build-x86.bat', build=build_windows,
                      checkout=None, log_path=None, target='iwd2r'):
    """Builds the x86 RelWithDebInfo exe (with its linker map, iwd2r.map
    beside it) that tools/frida/probe.py runs in the sandbox, in checkout
    (default WIN_CHECKOUT); returns the WSL path of the exe. target (a
    space-separated CMake target list) may add targets of the same preset,
    e.g. the oracle's iwd2r_injector and iwd2r_recorder. build is
    injectable for tests."""
    kw = dict(preset='windows-msvc-x86', target=target, exe_rel=X86_EXE_REL,
              bat_name=bat_name)
    if checkout:
        kw['checkout'] = checkout
    if log_path:
        kw['log_path'] = log_path
    return build(**kw)


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


def scenario_timeout(arg, path):
    """A --vm scenario's timeout in seconds: --timeout (arg) when given,
    else the scenario header's "# timeout: S" (realinput.header_timeout),
    else realinput.DEFAULT_TIMEOUT."""
    if arg is not None:
        return arg
    return realinput.load_header_timeout(path) or realinput.DEFAULT_TIMEOUT


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


def missing_shots(run_dir, actions, presented=False):
    """The `shot NAME` lines of the scenario whose NAME.png (and, with
    presented, NAME-presented.png) is not in run_dir, in script order."""
    missing = []
    for action in actions:
        if action['op'] != 'shot' or action['name'] in missing:
            continue
        files = [action['name'] + '.png']
        if presented:
            files.append(action['name'] + '-presented.png')
        if not all(os.path.isfile(os.path.join(run_dir, f)) for f in files):
            missing.append(action['name'])
    return missing


def check_shots(result, run_dir, actions, presented=False):
    """A scenario passes only when the game exited 0 and every shot it
    asks for was written: an engine that quits early with 0 fails here with
    code 1 and "missing shot NAME"."""
    if result['code'] != 0:
        return result
    missing = missing_shots(run_dir, actions, presented)
    if missing:
        result['code'] = 1
        error = 'missing shot ' + ', '.join(missing)
        result['error'] = error if not result.get('error') \
            else result['error'] + '; ' + error
    return result


def check_checkpoints(result, run_dir, actions, scenario):
    """A scenario that exited 0 with its shots passes only when the engine
    dumped its state at every checkpoint and each dump has what the
    scenario's expectations (realinput.load_expectations: what the original
    reached there) name: a lost click that leaves the run on another screen
    or with other choices fails here with code 1 and the first field."""
    if result['code'] != 0:
        return result
    problems = realinput.check_checkpoints(
        actions, run_dir, realinput.load_expectations(scenario))
    if problems:
        result['code'] = 1
        error = problems[0] if len(problems) == 1 else '%s (and %d more)' % (
            problems[0], len(problems) - 1)
        result['error'] = error if not result.get('error') \
            else result['error'] + '; ' + error
        with open(os.path.join(run_dir, 'checkpoints.txt'), 'w') as f:
            f.write('\n'.join(problems) + '\n')
    return result


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


def run_linux_vm(args, argv, scenarios, data, overrides, base_dir, nested,
                 geometry):
    """Runs the scenarios on Xvfb displays; returns the table rows."""
    missing = realinput.missing_tools()
    if missing:
        log('missing %s: sudo apt-get install -y xvfb xdotool imagemagick'
            % ', '.join(missing))
        return None
    exe = os.path.join(ROOT, 'build', LINUX_PRESET, 'iwd2r') \
        if args.no_build else build_linux()
    count_launch(args)
    backup = os.path.join(base_dir, 'ini-backup')
    names = backup_inis(data, backup)
    log('backed up %d INI files to %s' % (len(names), backup))
    base_env = child_env(os.environ, data, overrides, False, None, None,
                         base_dir, shot_presented=args.shot_presented)
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
        env = base_env
        # Games at once would share the install's cache and temp folders
        # and INI files: each gets a private view of it.
        view = dataview.make_view(data, os.path.join(run_dir, 'data'))
        env = dict(base_env, IWD2_DATA=view)
        if realinput.load_header_cheats(path):
            oracle.set_cheats_ini(
                os.path.join(view, 'icewind2.ini'), True)
        try:
            result = realinput.run_scenario(
                exe, env, actions, run_dir, geometry,
                scenario_timeout(args.timeout, path),
                taken, cwd=ROOT, abort=abort,
                cpu=realinput.job_cpu(slot, cpus) if args.jobs > 1 else None)
        finally:
            if not args.keep_data_view:
                dataview.remove_view(view)
        check_shots(result, run_dir, actions, args.shot_presented)
        check_checkpoints(result, run_dir, actions, path)
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


def run_windows_vm(args, argv, scenarios, data, overrides, base_dir, nested,
                   geometry):
    """Runs the scenarios one after another in Windows Sandbox (one sandbox
    can run at a time); returns the table rows."""
    if not winsandbox.available():
        print(winsandbox.ENABLE_HINT, flush=True)
        return None
    exe = build_windows_vm(args)
    count_launch(args)
    rows = []
    stamp = os.path.relpath(base_dir, os.path.join(ROOT, 'runs'))
    for path, actions in scenarios:
        name = scenario_name(path)
        run_dir = os.path.join(base_dir, name) if nested else base_dir
        prepare_scenario_dir(run_dir, path, actions)
        host_run = os.path.join(winsandbox.VM_ROOT, 'runs', stamp, name)
        os.makedirs(host_run, exist_ok=True)
        with filelock.hold(winsandbox.SANDBOX_LOCK, 'sandbox'):
            # Checked for every scenario: another worktree's run may have
            # refilled the shared cache from another source in between. The
            # copy holds the vanilla copy's lock (never held while waiting
            # for another lock, so taking it here cannot deadlock); no
            # sandbox maps the copy while we hold the sandbox lock.
            winsandbox.refresh_cache(
                data, winsandbox.GAME_CACHE, log,
                lock=lambda: filelock.hold(oracle.VANILLA_LOCK, 'vanilla'))
            with filelock.hold(oracle.VANILLA_LOCK, 'vanilla'):
                winsandbox.reset_cache_inis(data, winsandbox.GAME_CACHE)
            if realinput.load_header_cheats(path):
                oracle.set_cheats_ini(
                    os.path.join(winsandbox.GAME_CACHE, 'icewind2.ini'),
                    True)
            log('windows: %s in Windows Sandbox (real input)' % name)
            result = winsandbox.run_scenario(
                exe, winsandbox.GAME_CACHE,
                realinput.windows_actions(actions, geometry),
                overrides, name, run_dir, host_run,
                scenario_timeout(args.timeout, path), realinput.FRAME_RATE,
                log)
        # --shot-presented is not passed into the sandbox, so only the
        # engine's NAME.png is required there.
        check_shots(result, run_dir, actions)
        check_checkpoints(result, run_dir, actions, path)
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
            scenario_timeout(args.timeout, path)
        except realinput.ScenarioError as e:
            sys.exit('play: %s' % e)
    platforms = ['linux', 'windows'] if args.vm == 'all' else [args.vm]
    datas = {p: resolve_data(args.data, p) for p in platforms}
    for platform in platforms:
        log('%s data: %s' % (platform, datas[platform]))
    overrides, geometry = vm_overrides(args)
    log('screen: %dx%d inside, scale %d (UI %d), output %dx%d' % (
        geometry['width'], geometry['height'], geometry['scale'],
        geometry['ui_scale'], geometry['output_width'],
        geometry['output_height']))
    run_root = make_run_folder(os.path.join(ROOT, 'runs'),
                               datetime.datetime.now())
    tables = []
    codes = []
    try:
        for platform in platforms:
            base = os.path.join(run_root, platform) if len(platforms) > 1 \
                else run_root
            os.makedirs(base, exist_ok=True)
            runner = run_linux_vm if platform == 'linux' else run_windows_vm
            rows = runner(args, argv, scenarios, datas[platform], overrides,
                          base, args.suite, geometry)
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


def launch_refusal(args, runs_dir=None):
    """Why this launch is refused (the attempt's launch limit, ruling R16),
    or None.  One play.py invocation is one launch, a suite included; the
    count lives in the runs root the run folders are created under."""
    return attempt.gate(runs_dir or os.path.join(ROOT, 'runs'), 'play',
                        args.new_attempt, name=ATTEMPT_FILE)


_LAUNCH_COUNTED = False


def count_launch(args):
    """Count this process's launch once, just before the first launch;
    exits with the refusal."""
    global _LAUNCH_COUNTED
    if _LAUNCH_COUNTED:
        return
    refusal = launch_refusal(args)
    if refusal:
        sys.exit('play: ' + refusal)
    _LAUNCH_COUNTED = True


ATTEMPT_FILE = 'attempt-play.json'
DESKTOP_OK = 'IWD2R_DESKTOP_OK'


def desktop_refusal(headless, env):
    """Why a run that would open a window on the desktop is refused, or None.

    Agents drive the game headless, under Xvfb (--vm linux) or in Windows
    Sandbox (--vm windows); a window on the user's desktop steals focus and
    a stray click lands in the run. A person who wants the window sets
    IWD2R_DESKTOP_OK=1."""
    if headless or env.get(DESKTOP_OK) == '1':
        return None
    return ('play: this run would open a window on the desktop; desktop runs '
            'need a person at the PC (%s is set only by them); use '
            '--headless, --vm linux or --vm windows' % DESKTOP_OK)


def main(argv):
    args = parse_args(argv)
    if args.vm:
        return main_vm(args, argv)
    headless = args.headless
    if args.linux and not headless and not os.environ.get('DISPLAY') \
            and not os.environ.get('WAYLAND_DISPLAY'):
        log('no DISPLAY or WAYLAND_DISPLAY: running headless')
        headless = True
    refusal = desktop_refusal(headless, os.environ)
    if refusal:
        sys.exit(refusal)
    data = resolve_data(args.data, 'linux' if args.linux else 'windows')
    log('data: %s' % data)
    script = os.path.abspath(args.script) if args.script else None
    if script and not os.path.isfile(script):
        sys.exit('play: no script %s' % script)
    overrides = build_overrides(args.resolution, args.fullscreen, args.sets,
                                args.scale)

    if args.linux:
        exe = os.path.join(ROOT, 'build', LINUX_PRESET, 'iwd2r') \
            if args.no_build else build_linux()
    else:
        if not args.no_build:
            sync_windows()
        exe = build_windows() if not args.no_build else \
            os.path.join(WIN_CHECKOUT, WIN_EXE_REL)

    count_launch(args)
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
                view = dataview.make_view(data, os.path.join(run_dir, 'data'))
                env = child_env(os.environ, view, overrides, headless,
                                args.frames, script, run_dir, args.sound,
                                args.shot_presented)
                try:
                    code = run_linux(exe, env, log_file)
                finally:
                    if not args.keep_data_view:
                        dataview.remove_view(view)
            else:
                win_run = os.path.join(WIN_CHECKOUT, 'runs',
                                       os.path.basename(run_dir))
                os.makedirs(win_run)
                win_script = None
                if script:
                    shutil.copy2(script, os.path.join(win_run, 'script.txt'))
                    win_script = wslpath('-w', os.path.join(win_run, 'script.txt'))
                # The view is on the Windows drive: junctions and hard links.
                view = dataview.make_view(data, os.path.join(win_run, 'data'))
                env_vars = child_env({}, wslpath('-w', view), overrides,
                                     headless, args.frames, win_script,
                                     wslpath('-w', win_run), args.sound,
                                     args.shot_presented)
                try:
                    code = run_windows(exe, env_vars, win_run, log_file)
                finally:
                    if not args.keep_data_view:
                        dataview.remove_view(view)
                    for name in os.listdir(win_run):
                        if name not in ('run.bat', 'script.txt', 'data'):
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
