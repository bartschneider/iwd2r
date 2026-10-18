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


def log(message):
    print('sandbox: %s' % message, flush=True)


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
    return args


def _cmdline(argv):
    """One cmd.exe command line; anything with spaces goes quoted."""
    return ' '.join(a if a and ' ' not in a else '"%s"' % a for a in argv)


def original_logon(python_win, timeout, args):
    """The box's logon command: the Frida sidecar in the background (start
    /b returns at once), the replay driver in front -- the driver injects,
    drives and quits the game exactly as in a recording, and writes
    done.txt, so the run fails exactly as a recording would.  The whole
    line is wrapped in cmd /c: the sandbox executes LogonCommand without a
    shell, where start, & and redirection do not exist."""
    sidecar = ['start', '""', '/b', python_win,
               winsandbox.BOX_TOOLS + '\\orig.py',
               '--attach', ORIGINAL_ATTACH,
               '--timeout', str(timeout + TRACE_GRACE)] + args
    return 'cmd.exe /c %s & %s' % (_cmdline(sidecar),
                                   winsandbox.replay_logon_command())


def ours_env(game_win, seed):
    """Our build's replay environment, sandbox paths (platform/replay.cpp
    reads IWD2R_SCRIPT and IWD2R_SEED the way the recorder does).  No
    IWD2R_INI_OVERRIDE: both sides read the copy's prepared icewind2.ini."""
    return {
        'IWD2_DATA': game_win,
        'IWD2R_SCRIPT': winsandbox.BOX_REPLAY_DIR + '\\scenario.txt',
        'IWD2R_SEED': str(seed),
        'IWD2R_SHOT_DIR': winsandbox.BOX_REPLAY_DIR,
        'IWD2R_AUDIO': 'null',
    }


def ours_logon(python_win, game_win, env, timeout, args):
    """The box's logon command: orig.py spawns our build under Frida in the
    foreground (its verdict is the run), then done.txt, then the shutdown
    that closes the sandbox -- cmd's & runs on regardless of exit codes.
    Wrapped in cmd /c like original_logon (no shell at LogonCommand)."""
    sidecar = [python_win, winsandbox.BOX_TOOLS + '\\orig.py',
               '--exe', BOX_OURS + '\\iwd2r.exe', '--workdir', game_win,
               '--timeout', str(timeout + TRACE_GRACE)] + args
    sidecar += ['--env', *('%s=%s' % kv for kv in sorted(env.items()))]
    return 'cmd.exe /c %s & echo done > %s\\done.txt & shutdown /s /t 0' % (
        _cmdline(sidecar), winsandbox.BOX_RESULTS)


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


def run_side(target, script, folders, logon_cmd, timeout, host_run, out_dir,
             probe, seed, dump_every, dry):
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
        with open(os.path.join(tools, 'config.json'), 'w') as f:
            json.dump(winsandbox.replay_driver_config(
                winsandbox.wslpath_w(oracle.GAME_DIR),
                oracle.recorder_env(
                    winsandbox.BOX_REPLAY_DIR + '\\scenario.txt',
                    winsandbox.BOX_REPLAY_DIR, seed, dump_every),
                timeout), f, indent=1)
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


def run(ns):
    dry = ns.dry or ns.command == 'plan'
    load_probe(ns.probe)
    host_python, venv_win, box_python = venv_python()
    script = oracle.scenario_path(ns.scenario)
    timeout = oracle.replay_timeout(ns.timeout, script)
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

    build = os.path.join(play.WIN_CHECKOUT,
                         oracle.REPLAY_DIR_REL if ns.target == 'original'
                         else OURS_EXE_REL)
    exe = os.path.join(build, oracle.INJECTOR if ns.target == 'original'
                       else 'iwd2r.exe')
    if not os.path.isfile(exe):
        if ns.target == 'original':
            sys.exit('probe: no %s; run tools/oracle.py build' % exe)
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

    stamp = os.path.basename(run_dir)
    host_run = os.path.join(winsandbox.VM_ROOT, 'replays',
                            '%s-%s' % (stamp, ns.target))
    args = sidecar_args(ns, ns.out, status_name)
    if ns.target == 'original':
        logon_cmd = original_logon(box_python, timeout, args)
    else:
        logon_cmd = ours_logon(box_python, game_win,
                               ours_env(game_win, ns.seed), timeout, args)
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
    try:
        with filelock.hold(oracle.REPLAY_LOCK, 'replay'), \
                filelock.hold(winsandbox.SANDBOX_LOCK, 'sandbox'):
            result = run_side(ns.target, script, folders, logon_cmd, timeout,
                              host_run, out_dir, ns.probe, ns.seed,
                              ns.dump_every, dry)
    except (RuntimeError, TimeoutError) as e:
        print('probe: %s' % e, file=sys.stderr)
        result = {'error': str(e), 'seconds': 0.0}
    finally:
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
                   help='original side only: a dump every N frames '
                        '(default %(default)s)')
    p.add_argument('--clicks', type=int, default=0, metavar='N',
                   help='insert N click actions before the trailing quit, '
                        'on a copy of the scenario, so a click-driven hook '
                        'fires (default: none)')
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
    p.add_argument('--module', default=None,
                   help='module --loaded-rva resolves against '
                        '(default iwd2r.exe)')
    p.add_argument('--no-loaded', action='store_true',
                   help='drop the loaded observation')
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
    return ns.fn(ns)


if __name__ == '__main__':
    raise SystemExit(main())
