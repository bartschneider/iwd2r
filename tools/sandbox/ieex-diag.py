#!/usr/bin/env python3
"""One untraced sandbox launch of IWD2EE.exe (the IEex loader) with its
diagnostics captured: the bisect between "apitrace's child injection breaks
the loader" and "the sandbox cannot run IWD2EE at all".

Companion to tools/gltrace.py (the traced path) and
tools/sandbox/ieex-diag-driver.ps1 (what runs inside the sandbox). Reuses
the same house pattern: the game mapped at its own host path, results
read-write under VM_ROOT, tools read-only, done.txt as the completion
signal, icewind2.ini backed up and restored around the launch.

    python3 -I tools/sandbox/ieex-diag.py \
        --game /mnt/c/Users/thier/iwd2r-ieex/game --timeout 150
"""

import argparse
import datetime
import json
import os
import shutil
import sys

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)

import filelock  # noqa: E402
import winsandbox  # noqa: E402

DRIVER = os.path.join(TOOLS, 'sandbox', 'ieex-diag-driver.ps1')
KEEP = ('diag.log', 'loader-console.log', 'loader-console.err',
        'processes.txt', 'windows.txt', 'game-before.txt', 'game-after.txt',
        'game-diff.txt', 'exitcode.txt', 'error.txt', 'done.txt')


def log(message):
    print(datetime.datetime.now().strftime('%H:%M:%S'), message, flush=True)


def copy_home(results, run_dir):
    for entry in sorted(os.listdir(results)):
        if entry in KEEP or entry.endswith('-screen.png'):
            shutil.copy2(os.path.join(results, entry),
                         os.path.join(run_dir, entry))


def run(args):
    game = args.game
    if not os.path.isfile(os.path.join(game, 'IWD2EE.exe')):
        raise SystemExit('ieex-diag: no IWD2EE.exe in %s' % game)
    stamp = args.stamp or datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    run_dir = os.path.join(args.runs, stamp)
    os.makedirs(run_dir, exist_ok=True)
    host_run = os.path.join(winsandbox.VM_ROOT, 'runs', stamp)
    results = os.path.join(host_run, 'results')
    tools = os.path.join(host_run, 'tools')
    os.makedirs(results)
    os.makedirs(tools)
    game_win = winsandbox.wslpath_w(game)
    config = {
        'scenario': 'ieex-diag',
        'exe': game_win + '\\IWD2EE.exe',
        'data': game_win,
        'results': winsandbox.BOX_RESULTS,
        'directplay': winsandbox.BOX_TOOLS + '\\directplay',
        'exclusions': [game_win, winsandbox.BOX_RESULTS],
        'timeout': args.timeout,
    }
    with open(os.path.join(tools, 'config.json'), 'w') as f:
        json.dump(config, f, indent=1)
    shutil.copy2(DRIVER, os.path.join(tools, 'ieex-diag-driver.ps1'))
    winsandbox.stage_directplay(os.path.join(tools, 'directplay'))
    wsb = os.path.join(host_run, 'diag.wsb')
    with open(wsb, 'w', newline='\r\n') as f:
        f.write(winsandbox.make_wsb(
            [(game_win, game_win, False),
             (winsandbox.wslpath_w(results), winsandbox.BOX_RESULTS, False),
             (winsandbox.wslpath_w(tools), winsandbox.BOX_TOOLS, True)],
            'powershell.exe -NoProfile -ExecutionPolicy Bypass -File '
            '%s\\ieex-diag-driver.ps1 -Config %s\\config.json'
            % (winsandbox.BOX_TOOLS, winsandbox.BOX_TOOLS)))
    shutil.copy2(wsb, os.path.join(run_dir, 'diag.wsb'))
    if args.stage_only:
        with open(wsb) as f:
            sys.stdout.write(f.read())
        log('staged %s (stage-only: nothing launched)' % host_run)
        return 0

    ini = os.path.join(game, 'icewind2.ini')
    ini_before = os.path.join(run_dir, 'icewind2.ini.before')
    had_ini = os.path.isfile(ini)
    if had_ini:
        shutil.copy2(ini, ini_before)
    exitcode, error = None, None
    try:
        with filelock.hold(winsandbox.SANDBOX_LOCK, 'sandbox'):
            log('starting the sandbox (untraced loader bisect)')
            winsandbox.start_sandbox(host_run, wsb, log)
            winsandbox.wait_done(
                results, args.timeout + winsandbox.SANDBOX_ALLOWANCE)
        code = winsandbox.read_text(os.path.join(results, 'exitcode.txt'))
        if code.lstrip('-').isdigit():
            exitcode = int(code)
        error = winsandbox.read_text(os.path.join(results, 'error.txt'))
    except RuntimeError as e:
        error = str(e)
    finally:
        winsandbox.close_sandbox(log)
        copy_home(results, run_dir)
        if had_ini and os.path.isfile(ini):
            shutil.copy2(ini, os.path.join(run_dir, 'icewind2.ini.after'))
            shutil.copy2(ini_before, ini)
            log('restored icewind2.ini')
    log('exitcode %s, error %r' % (exitcode, error))
    log('evidence in %s' % run_dir)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--game', default='/mnt/c/Users/thier/iwd2r-ieex/game')
    parser.add_argument('--timeout', type=int, default=150,
                        help='sandbox-side seconds to wait for a game window')
    parser.add_argument('--stamp', default=None,
                        help='run-directory name (default: a new timestamp)')
    parser.add_argument('--runs', default=os.path.join(TOOLS, os.pardir,
                                                       'runs'))
    parser.add_argument('--stage-only', action='store_true',
                        help='build config.json, the driver folder and the '
                             '.wsb, then stop (nothing launched)')
    return run(parser.parse_args())


if __name__ == '__main__':
    sys.exit(main())
