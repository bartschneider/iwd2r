#!/usr/bin/env python3
"""Run one side of a differential Frida trace in its own Windows Sandbox.

    probe.py run --target original --probe hooks.json --scenario world_entry
    probe.py run --target ours     --probe hooks.json --scenario world_entry \
        --loaded-rva 0xABCDEF

Both sides drive the SAME scenario (a run-folder-local copy; the repo's
scenarios/ tree is never written) inside a fresh sandbox -- the same machine
winsandbox.run_replay uses for a recording -- and the trace plus verdict file
come home under runs/<stamp>/replay/.  The exit code is orig.py's verdict for
the side that ran (CLEAN 0, CRASH 1, ran-but-proved-nothing 2); comparing the
two traces is trace_diff.py compare (docs/frida-tracing.md).

  original  the oracle copy's IWD2.exe, injected by the oracle build's
            injector.exe + recorder.dll exactly as replay record drives it;
            the driver's logon command is prefixed with a background Frida
            sidecar (orig.py --attach IWD2.exe) that watches the game.
  ours      our build's iwd2r.exe spawned under Frida by orig.py --exe
            (RunScenario drives the same scenario in-engine); done.txt and
            the shutdown are chained after the sidecar with cmd's & so a
            dead sidecar cannot hang the host.

On the original side the replay driver's shutdown is off (its done.txt comes
when the game exits, before the sidecar's verdict) and the host holds the box
open itself until the status file has RESULT:, within scenario timeout + 60 s.

The Frida venv (%USERPROFILE%\\iwd2r-frida\\venv -- 64-bit python, which
attaches to the 32-bit games fine) and its base interpreter C:\\Python314
are mapped into every sandbox read-only at their host paths, so the sidecar
runs the interpreter the host already vetted.

Locks nest as in replay record (REPLAY_LOCK outside SANDBOX_LOCK), staging
lives on /mnt/c under iwd2r-vm\\replays (the box cannot map WSL paths), and
everything else stays under runs/.

    probe.py plan ...   stages a run (scenario copy, tools, config.json,
                        run.wsb) and prints the sandbox plan -- folders and
                        logon command -- without starting a sandbox.
"""
import argparse
import datetime
import contextlib
import json
import os
import shutil
import sys
import time
from pathlib import Path

TOOLS = os.path.dirname(os.path.abspath(__file__))          # tools/frida
sys.path.insert(0, os.path.dirname(TOOLS))                  # tools/
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import filelock  # noqa: E402
import oracle  # noqa: E402
import play  # noqa: E402
import realinput  # noqa: E402
import winsandbox  # noqa: E402
import winhome  # noqa: E402

VENV_REL = 'iwd2r-frida/venv'        # under %USERPROFILE% (winhome.WIN_HOME)
HOST_PYTHON314 = '/mnt/c/Python314'  # the venv's base interpreter (pyvenv.cfg)
BOX_PYTHON314 = r'C:\Python314'
BOX_OURS = r'C:\iwd2r\ours'          # our RelWithDebInfo build, in the box
OURS_EXE_REL = 'build/windows-msvc-x86/RelWithDebInfo'
FRIDA_FILES = ('orig.py', 'hooks.py')  # staged into the box's tools folder
PROBE_TABLE = 'probe.json'           # the hook table, staged verbatim
ORIGINAL_ATTACH = 'IWD2.exe'
TRACE_GRACE = 90                     # orig.py's extra seconds over the timeout
CLICK_SPOT = (400, 300)              # where --clicks places its clicks
CLICK_WAIT = 60                      # frames between a click and the next
RESULT_GRACE = 60                    # box wait for RESULT: past the timeout
POST_LOAD_FALLBACK = 180             # --post-load when the scenario has no header


def log(message):
    try:
        print('sandbox: %s' % message, flush=True)
    except (BrokenPipeError, OSError):
        pass      # the parent is gone: the cleanup that logs must still run


def venv_python():
    """(host path of the venv's python.exe, the venv's Windows path, the
    interpreter's Windows path -- the logon command runs in the box, where
    only C:\\... forms resolve)."""
    venv = os.path.join(winhome.WIN_HOME, VENV_REL)
    host = os.path.join(venv, 'Scripts', 'python.exe')
    if not os.path.isfile(host):
        sys.exit('probe: no %s; create the Frida venv first '
                 '(docs/frida-tracing.md)' % host)
    return host, winsandbox.wslpath_w(venv), winsandbox.wslpath_w(host)


def load_probe(path):
    """The hook table, checked before any sandbox is started."""
    try:
        spec = json.loads(Path(path).read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        sys.exit('probe: %s: %s' % (path, e))
    if not isinstance(spec, dict) or not spec.get('hooks'):
        sys.exit('probe: %s has no hooks' % path)
    return spec


def prepare_scenario(script, run_dir, clicks):
    """The scenario copy this run drives: a verbatim copy in run_dir, with
    --clicks N click actions (each followed by a wait) inserted before the
    trailing quit so a click-driven hook (CGameSprite::MoveToPoint) fires
    over a look-only scenario.  Returns the copy's path; the repo's
    scenarios/ tree is never written."""
    target = os.path.join(run_dir, 'scenario.txt')
    shutil.copyfile(script, target)
    if not clicks:
        return target
    with open(target, encoding='utf-8') as f:
        lines = f.read().splitlines()
    extra = []
    for _ in range(clicks):
        extra += ['click %d %d' % CLICK_SPOT, 'wait %d' % CLICK_WAIT]
    end = len(lines)
    while end > 0 and not lines[end - 1].strip():
        end -= 1
    if end > 0 and lines[end - 1].strip().startswith('quit'):
        lines[end - 1:end - 1] = extra
    else:
        lines += extra + ['quit']
    with open(target, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    return target


def stage_frida_tools(tools_dir, probe):
    """orig.py + hooks.py + the hook table, verbatim, into the box's tools."""
    for name in FRIDA_FILES:
        shutil.copy2(os.path.join(TOOLS, name), os.path.join(tools_dir, name))
    shutil.copy2(probe, os.path.join(tools_dir, PROBE_TABLE))


def sidecar_args(ns, out, status):
    """orig.py's arguments, as sandbox paths, shared by both sides."""
    args = ['--hooks', winsandbox.BOX_TOOLS + '\\' + PROBE_TABLE,
            '--out', winsandbox.BOX_REPLAY_DIR + '\\' + out,
            '--status', winsandbox.BOX_REPLAY_DIR + '\\' + status,
            '--hit-min', str(ns.hit_min)]
    if ns.hit:
        args += ['--hit', ns.hit]
    if ns.attach_timeout is not None:
        args += ['--attach-timeout', str(ns.attach_timeout)]
    if ns.loaded_addr is not None:
        args += ['--loaded-addr', ns.loaded_addr]
    if ns.loaded_rva is not None:
        args += ['--loaded-rva', ns.loaded_rva]
    if ns.module is not None:
        args += ['--module', ns.module]
    if ns.no_loaded:
        args += ['--no-loaded']
    post_load = getattr(ns, 'post_load', None)
    if post_load is not None:
        args += ['--post-load', str(post_load)]
    return args


def resolve_post_load(arg, script):
    """--post-load SECONDS: the argument when given, else the scenario's
    "# timeout: S" header, else POST_LOAD_FALLBACK -- the sidecar's own 20 s
    default closes the window long before the scenario's trailing clicks."""
    if arg is not None:
        return arg
    import realinput
    try:
        return realinput.load_header_timeout(script) or POST_LOAD_FALLBACK
    except realinput.ScenarioError as e:
        sys.exit('probe: %s' % e)


def _cmdline(argv):
    """One cmd.exe command line; anything with spaces goes quoted."""
    return ' '.join(a if a and ' ' not in a else '"%s"' % a for a in argv)


def sidecar_argv(target, python_win, env, timeout, args):
    """The Frida sidecar as argv: [python, orig.py, <orig.py's options>].
    original attaches to the injected IWD2.exe; ours spawns our build under
    Frida with one --env K=V flag per pair (the option is argparse
    "append", one value each)."""
    argv = [python_win, winsandbox.BOX_TOOLS + '\\orig.py']
    if target == 'original':
        argv += ['--attach', ORIGINAL_ATTACH]
    else:
        argv += ['--exe', BOX_OURS + '\\iwd2r.exe',
                 '--workdir', env['IWD2_DATA']]
    argv += ['--timeout', str(timeout + TRACE_GRACE)] + list(args)
    if target != 'original':
        for key, value in sorted(env.items()):
            argv += ['--env', '%s=%s' % (key, value)]
    return argv


def wait_for_result_cmd(status_win, deadline_s):
    """A powershell one-liner for cmd /c: returns once the sidecar's status
    file has a line starting RESULT: , or after deadline_s seconds.  No
    cmd metacharacters outside the double quotes, so it chains with &."""
    return ('powershell -NoProfile -Command "$e=(Get-Date).AddSeconds(%d); '
            "while((Get-Date) -lt $e){ if((Test-Path '%s') -and "
            "(Select-String -Path '%s' -Pattern '^RESULT:' -Quiet)){ break }; "
            'Start-Sleep -Seconds 1 }"' % (deadline_s, status_win, status_win))


def _status_path(args):
    return args[args.index('--status') + 1]


def _finish_cmd(args, timeout):
    """Wait for the verdict, then done.txt, then the shutdown."""
    return '%s & echo done > %s\\done.txt & shutdown /s /t 0' % (
        wait_for_result_cmd(_status_path(args), timeout + RESULT_GRACE),
        winsandbox.BOX_RESULTS)


def original_logon(python_win, timeout, args):
    """The box's logon command: the Frida sidecar in the background (start
    /b returns at once), the replay driver in front -- the driver injects,
    drives and quits the game exactly as in a recording (its config has
    shutdown off for a probe run, see run_side) -- then the wait for the
    sidecar's RESULT: line, done.txt and the shutdown.  The whole line is
    wrapped in cmd /c: the sandbox executes LogonCommand without a shell,
    where start, & and redirection do not exist."""
    sidecar = ['start', '""', '/b'] + sidecar_argv(
        'original', python_win, None, timeout, args)
    return 'cmd.exe /c %s & %s & %s' % (
        _cmdline(sidecar), winsandbox.replay_logon_command(),
        _finish_cmd(args, timeout))


def ours_env(game_win, seed, dump_every=None):
    """Our build's replay environment, sandbox paths: what oracle.our_env
    sets for a headless replay.  IWD2R_STATE_DUMP (into the run's replay
    folder, which comes home) switches platform/replay.cpp's deterministic
    mode on -- the virtual clock and frame-locked AI the original runs under
    in the recorder -- and IWD2R_SEED is read only then; without it ours ran
    on the real clock.  No IWD2R_INI_OVERRIDE: both sides read the copy's
    prepared icewind2.ini."""
    env = {
        'IWD2_DATA': game_win,
        'IWD2R_SCRIPT': winsandbox.BOX_REPLAY_DIR + '\\scenario.txt',
        'IWD2R_STATE_DUMP': winsandbox.BOX_REPLAY_DIR,
        'IWD2R_SEED': str(seed),
        'IWD2R_SHOT_DIR': winsandbox.BOX_REPLAY_DIR,
        'IWD2R_AUDIO': 'null',
    }
    if dump_every is not None:
        env['IWD2R_DUMP_EVERY'] = str(dump_every)
    return env


def ours_logon(python_win, game_win, env, timeout, args):
    """The box's logon command: orig.py spawns our build under Frida in the
    foreground (its verdict is the run), then the RESULT: wait (immediate
    once the sidecar returned), done.txt, and the shutdown that closes the
    sandbox -- cmd's & runs on regardless of exit codes.  Wrapped in
    cmd /c like original_logon (no shell at LogonCommand)."""
    env = dict(env, IWD2_DATA=game_win)
    return 'cmd.exe /c %s & %s' % (
        _cmdline(sidecar_argv('ours', python_win, env, timeout, args)),
        _finish_cmd(args, timeout))


def plan_folders(target, venv_win, build_win, game_win, results_win,
                 tools_win):
    """The .wsb mapped folders: the recording set for the original (the
    oracle build, the copy's game, results, tools), our build at BOX_OURS
    for ours, plus the Frida venv and its base interpreter at their host
    paths in both cases."""
    if target == 'original':
        folders = winsandbox.replay_folders(build_win, game_win, results_win,
                                            tools_win)
    else:
        folders = [(build_win, BOX_OURS, True),
                   (game_win, game_win, False),
                   (results_win, winsandbox.BOX_RESULTS, False),
                   (tools_win, winsandbox.BOX_TOOLS, True)]
    return folders + [(venv_win, venv_win, True),
                      (winsandbox.wslpath_w(HOST_PYTHON314),
                       BOX_PYTHON314, True)]


def wait_for_result_file(path, seconds):
    """Polls path for a RESULT: line; True once there.  False at once when
    the file does not exist (the sidecar creates it before it attaches, so a
    missing file means no verdict is coming) and False after seconds."""
    if not os.path.exists(path):
        return False
    end = time.time() + seconds
    while True:
        try:
            with open(path, encoding='utf-8', errors='replace') as f:
                if any(line.startswith('RESULT:') for line in f):
                    return True
        except OSError:
            pass
        if time.time() >= end:
            return False
        time.sleep(2)


@contextlib.contextmanager
def saves_of_its_own(game, out_dir, dry):
    """Every probe run starts with the empty save folder a replay record
    gives (oracle.prepared_saves, mode empty, the copy's own saves aside and
    put back afterwards, the run's saves moved into out_dir/saves): a
    scenario that saves (save_load_screens) sees the same state on both
    sides and every run, and no player save is touched.  A plan-only run
    changes nothing.  Inside the replay and sandbox locks, as record_original
    holds them."""
    if dry:
        yield {}
        return
    with oracle.prepared_saves(oracle.mpsave_dir(game),
                               {'mode': 'empty', 'load': None}, out_dir,
                               oracle.save_aside(game)) as report:
        yield report


def run_side(target, script, folders, logon_cmd, timeout, host_run, out_dir,
             probe, seed, dump_every, dry, status_name='frida-trace.jsonl.status'):
    """One sandbox for one side -- winsandbox.run_replay's shape with the
    Frida staging and logon command swapped in.  Everything under results/
    (the driver's logs; results/replay/ also carries the scenario copy, the
    trace and the verdict file) comes home under out_dir.  Returns
    {'error', 'seconds'}; the caller reads the verdict file."""
    started = time.time()
    result = {'error': None, 'seconds': 0.0}
    results = os.path.join(host_run, 'results')
    tools = os.path.join(host_run, 'tools')
    os.makedirs(os.path.join(results, 'replay'))
    os.makedirs(tools)
    shutil.copyfile(script, os.path.join(results, 'replay', 'scenario.txt'))
    if target == 'original':
        config = winsandbox.replay_driver_config(
            winsandbox.wslpath_w(oracle.GAME_DIR),
            oracle.recorder_env(
                winsandbox.BOX_REPLAY_DIR + '\\scenario.txt',
                winsandbox.BOX_REPLAY_DIR, seed, dump_every),
            timeout)
        # The driver must not shut the box down when the game exits: the
        # logon command waits for the sidecar's RESULT: and shuts down.
        config['shutdown'] = False
        with open(os.path.join(tools, 'config.json'), 'w') as f:
            json.dump(config, f, indent=1)
        shutil.copy2(winsandbox.REPLAY_DRIVER,
                     os.path.join(tools, 'replay-driver.ps1'))
        winsandbox.stage_directplay(os.path.join(tools, 'directplay'))
    stage_frida_tools(tools, probe)
    wsb = os.path.join(host_run, 'run.wsb')
    with open(wsb, 'w', newline='\r\n') as f:
        f.write(winsandbox.make_wsb(folders, logon_cmd))
    if not dry:
        try:
            winsandbox.start_sandbox(host_run, wsb, log)
            winsandbox.wait_done(
                results, timeout + winsandbox.REPLAY_BACKSTOP
                + winsandbox.REPLAY_BOOT_ALLOWANCE)
            # The replay driver writes done.txt when the game exits, which
            # on the original side is before the sidecar's verdict: keep
            # the box open until the status file has its RESULT: line,
            # within timeout + RESULT_GRACE (the in-box wait's own bound).
            # Ours needs none: its done.txt already follows the in-box wait.
            if target == 'original' and not wait_for_result_file(
                    os.path.join(results, 'replay', status_name),
                    timeout + RESULT_GRACE):
                log('no RESULT: line in %s within %d s; closing the box'
                    % (status_name, timeout + RESULT_GRACE))
            error = winsandbox.read_text(os.path.join(results, 'error.txt'))
            if error:
                result['error'] = 'driver: %s' % error
        except RuntimeError as e:
            result['error'] = str(e)
        finally:
            winsandbox.close_sandbox(log, winsandbox.REPLAY_CLOSE_TIMEOUT)
        os.makedirs(os.path.join(out_dir, 'sandbox'), exist_ok=True)
        for src, dst in winsandbox.replay_copy_plan(
                results, out_dir, winsandbox.files_in(results),
                winsandbox.files_in(os.path.join(results, 'replay'))):
            shutil.copy2(src, dst)
        shutil.copy2(wsb, os.path.join(out_dir, 'sandbox', 'run.wsb'))
    result['seconds'] = time.time() - started
    return result


def report(out_dir, status_name):
    """The verdict file's tail and its exit code (orig.py's verdict:
    CLEAN 0, CRASH 1, every ran-but-proved-nothing verdict 2; a missing
    file is 2 too)."""
    path = os.path.join(out_dir, status_name)
    try:
        lines = Path(path).read_text(encoding='utf-8',
                                     errors='replace').splitlines()
    except OSError:
        print('probe: no %s came home' % status_name)
        return 2
    for line in lines[-8:]:
        print('  %s' % line)
    for line in reversed(lines):
        if line.startswith('RESULT:'):
            verdict = line[len('RESULT:'):].split()[0]
            return 0 if verdict == 'CLEAN' else 1 if verdict == 'CRASH' else 2
    print('probe: %s has no RESULT line' % status_name)
    return 2


def ours_build(target, build_checkout=None):
    """The build folder a side runs from, in build_checkout (the checkout
    that built the bodies under test, e.g. a batch worktree's) else this
    checkout's: the original's injector and recorder from its oracle build,
    ours from its x86 RelWithDebInfo build.  One checkout for both sides, so
    the recorder writing the original's dumps and the dumper writing ours
    come from one tree and follow one schema.json (a recorder.dll older than
    the schema in the driver's checkout made every b014/i001 dump guard
    fail)."""
    if target == 'original':
        return os.path.join(build_checkout or play.WIN_CHECKOUT, oracle.REPLAY_DIR_REL)
    return os.path.join(build_checkout or play.WIN_CHECKOUT, OURS_EXE_REL)


def run(ns):
    dry = ns.dry or ns.command == 'plan'
    load_probe(ns.probe)
    host_python, venv_win, box_python = venv_python()
    script = oracle.scenario_path(ns.scenario)
    cheats = realinput.load_header_cheats(script)
    fixtures = realinput.load_header_fixtures(script)
    timeout = oracle.replay_timeout(ns.timeout, script)
    ns.post_load = resolve_post_load(ns.post_load, script)
    if timeout <= TRACE_GRACE:
        sys.exit('probe: timeout %d s leaves no %d s trace window'
                 % (timeout, TRACE_GRACE))
    if not dry:
        # The same oracle state a replay record sets up (and nothing more).
        if not winsandbox.available():
            sys.exit('probe: %s' % winsandbox.ENABLE_HINT.replace('play: ', '', 1))
        game = oracle.ensure_vanilla()
        oracle.prepare_replay_ini(game)
    else:
        game = oracle.GAME_DIR
        if not os.path.isdir(game):
            sys.exit('probe: plan: no %s' % game)
    game_win = winsandbox.wslpath_w(game)

    build = ours_build(ns.target, ns.build_checkout)
    exe = os.path.join(build, oracle.INJECTOR if ns.target == 'original'
                       else 'iwd2r.exe')
    if not os.path.isfile(exe):
        if ns.target == 'original':
            sys.exit('probe: no %s; build the oracle there (tools/oracle.py build for '
                     'this checkout; driver.frida_build for a batch checkout)' % exe)
        sys.exit('probe: no %s; build windows-msvc-x86 first' % exe)
    if not dry:
        winsandbox.deploy_runtime(exe, log, arch='x86')

    run_dir = play.make_run_folder(oracle.RUNS_DIR, datetime.datetime.now())
    out_dir = os.path.join(run_dir, 'replay')
    status_name = ns.out + '.status'
    script = prepare_scenario(script, run_dir, ns.clicks)
    actions = oracle.load_actions(script)
    print('probe %s: %s (%d actions), seed %d, timeout %d s, probe %s, %s'
          % (ns.target, os.path.relpath(script, ROOT), len(actions), ns.seed,
             timeout, os.path.relpath(ns.probe, ROOT),
             'PLAN ONLY' if dry else 'in Windows Sandbox'))
    print('probe %s: exe %s' % (ns.target, exe))

    stamp = os.path.basename(run_dir)
    host_run = os.path.join(winsandbox.VM_ROOT, 'replays',
                            '%s-%s' % (stamp, ns.target))
    args = sidecar_args(ns, ns.out, status_name)
    if ns.target == 'original':
        logon_cmd = original_logon(box_python, timeout, args)
    else:
        logon_cmd = ours_logon(box_python, game_win,
                               ours_env(game_win, ns.seed, ns.dump_every), timeout, args)
    with open(os.path.join(run_dir, 'plan.txt'), 'w') as f:
        f.write(logon_cmd + '\n')
    folders = plan_folders(ns.target, venv_win, winsandbox.wslpath_w(build),
                           game_win,
                           winsandbox.wslpath_w(os.path.join(host_run,
                                                             'results')),
                           winsandbox.wslpath_w(os.path.join(host_run,
                                                             'tools')))
    if dry:
        print('plan: sandbox staging in %s' % host_run)
        print('plan: logon command (also %s):'
              % os.path.relpath(os.path.join(run_dir, 'plan.txt'), ROOT))
        print('  %s' % logon_cmd)
        print('plan: mapped folders:')
        for src, dst, ro in folders:
            print('  %s -> %s%s' % (src, dst, ' (ro)' if ro else ''))
    rc = 2
    prior_cheats = None
    prior_console = None
    prior_fixtures = []
    try:
        if not dry and cheats:
            prior_cheats = oracle.prepare_cheats_ini(game, cheats)
        if not dry and ns.target == 'original':
            # The original preloads the console edit box with the ini's
            # [Lua Edit] String0 (the last submitted line, any session's),
            # so a console run's typed command would append to it.
            prior_console = oracle.prepare_console_ini(game)
        if not dry and fixtures:
            # Both probe targets read the copy's Characters folder (ours
            # through IWD2_DATA), so the scenarios-root fixtures land there
            # for the run and come out after it.
            prior_fixtures = oracle.prepare_character_fixtures(game, fixtures)
        with filelock.hold(oracle.REPLAY_LOCK, 'replay'), \
                filelock.hold(winsandbox.SANDBOX_LOCK, 'sandbox'), \
                saves_of_its_own(game, out_dir, dry):
            result = run_side(ns.target, script, folders, logon_cmd, timeout,
                              host_run, out_dir, ns.probe, ns.seed,
                              ns.dump_every, dry, status_name)
    except (RuntimeError, TimeoutError) as e:
        print('probe: %s' % e, file=sys.stderr)
        result = {'error': str(e), 'seconds': 0.0}
    finally:
        if not dry and cheats:
            oracle.restore_cheats_ini(game, prior_cheats)
        if not dry and prior_fixtures:
            oracle.restore_character_fixtures(game, prior_fixtures)
        if not dry and ns.target == 'original':
            oracle.restore_console_ini(game, prior_console)
        if not dry:
            shutil.rmtree(host_run, ignore_errors=True)
    if result['error']:
        print('probe: %s' % result['error'], file=sys.stderr)
    if not dry:
        print('sandbox: %.1f s from start to results' % result['seconds'])
        rc = report(out_dir, status_name)
    print('probe: %s' % os.path.relpath(run_dir, ROOT))
    return rc


def add_side_args(p):
    p.add_argument('--target', required=True, choices=['original', 'ours'],
                   help='which side to trace')
    p.add_argument('--probe', required=True, metavar='HOOKS.json',
                   help='hook table (schema: tools/frida/hooks.py)')
    p.add_argument('--scenario', required=True,
                   help='scenarios/<name>.txt by name, or a path')
    p.add_argument('--out', default='frida-trace.jsonl', metavar='NAME',
                   help='trace file name under runs/<stamp>/replay/ '
                        '(default %(default)s; verdict file: <NAME>.status)')
    p.add_argument('--seed', type=oracle.seed_arg, default=1,
                   help="both sides' main-thread srand seed "
                        '(default %(default)s)')
    p.add_argument('--timeout', type=oracle.positive_int,
                   help='seconds before the run is killed (default: the '
                        'scenario header\'s "# timeout: S", else %d)'
                        % oracle.REPLAY_TIMEOUT)
    p.add_argument('--dump-every', type=oracle.non_negative_int,
                   default=oracle.REPLAY_DUMP_EVERY,
                   help='a dump every N frames, both sides (ours gets '
                        'IWD2R_DUMP_EVERY too) '
                        '(default %(default)s)')
    p.add_argument('--clicks', type=int, default=0, metavar='N',
                   help='insert N click actions before the trailing quit, '
                        'on a copy of the scenario, so a click-driven hook '
                        'fires (default: none)')
    p.add_argument('--post-load', type=oracle.positive_int, metavar='SECONDS',
                   help='seconds the sidecar keeps tracing once the world '
                        'screen is up (default: the scenario header\'s '
                        '"# timeout: S", else %d)' % POST_LOAD_FALLBACK)
    p.add_argument('--hit', help='hook name that must fire, else '
                                 'NOT-EXERCISED (default: any hit)')
    p.add_argument('--hit-min', type=int, default=1)
    p.add_argument('--attach-timeout', type=float,
                   help='original side: seconds to keep polling for '
                        'IWD2.exe (default 90)')
    p.add_argument('--loaded-addr',
                   help='loaded observation site, absolute -- the original '
                        'wants 0x6869C0 (its default)')
    p.add_argument('--loaded-rva',
                   help="ours side: loaded observation site as an RVA off "
                        '--module')
    p.add_argument('--build-checkout', metavar='CHECKOUT',
                   help='the Windows checkout (a WSL path) a side runs from, '
                        "e.g. a batch worktree's: ours its "
                        + OURS_EXE_REL + '/iwd2r.exe, the original its '
                        + oracle.REPLAY_DIR_REL + ' injector and recorder '
                        "(default: this checkout's, play.WIN_CHECKOUT); the "
                        'tools are staged from this checkout either way')
    p.add_argument('--module', default=None,
                   help='module --loaded-rva resolves against '
                        '(default iwd2r.exe)')
    p.add_argument('--no-loaded', action='store_true',
                   help='drop the loaded observation')
    p.add_argument('--new-attempt', metavar='REASON',
                   help='start a new attempt: reset the sandbox launch '
                        'count and record why')
    p.add_argument('--dry', action='store_true',
                   help='stage and print the plan without starting a '
                        'sandbox (same as the plan subcommand)')


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest='command', required=True)
    for name, help_text in (
            ('run', 'run one side in its own sandbox (default trace flow)'),
            ('plan', 'stage and print the sandbox plan, no sandbox')):
        p = sub.add_parser(name, help=help_text)
        add_side_args(p)
        p.set_defaults(fn=run, dry=name == 'plan')
    ns = ap.parse_args(argv)
    if ns.new_attempt is not None:
        import attempt
        bad = attempt.reason_error(ns.new_attempt)
        if bad:
            ap.error(bad)
        winsandbox.set_new_attempt(ns.new_attempt)
    return ns.fn(ns)


if __name__ == '__main__':
    raise SystemExit(main())
