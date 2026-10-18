#!/usr/bin/env python3
"""Trace a scenario's GL stream in the IWD2EE build with apitrace.

    python3 tools/gltrace.py <scenario> [--game DIR] [--apitrace DIR]
        [--timeout S] [--lead S] [--dump TRACE] [--dry-run]

One Windows Sandbox launch per run (the house pattern): the game folder
is mapped at its own host path (its icewind2.ini [Alias] paths name it),
the unpacked apitrace is mapped read-only, and the sandbox runs

    apitrace.exe trace --api gl -m -o <results>/<scenario>.trace IWD2EE.exe

through tools/sandbox/gltrace-driver.ps1. The driver drives the
scenario's input actions with realinput's coordinates; waits are
wall-clock, because the original has none of iwd2r's control channel.
The run stops after the scenario's last checkpoint op (shot/quit ops
would need the control channel) and closes the game; apitrace flushes
the trace when the game exits. -m makes apitrace hook with mhook
instead of IAT patching: IWD2EE.exe imports only kernel32, user32 and
version, and loads OpenGL32.dll dynamically, so there is no IAT to
patch.

Host-side the tool then streams `apitrace dump` into summarize(), keeps
the trace at runs/<stamp>/<scenario>.trace (never committed) and writes
docs/renderer/wso/gltrace/<scenario>.md. --dump summarizes an existing
trace without a sandbox launch.

IWD2EE boots through its intro movies, which the iwd2r build skips, so
the driver gets --lead seconds of lead time with two centre clicks
(a click skips a movie; the menu has nothing clickable at the centre)
before the scenario's first action.
"""

import argparse
import collections
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import filelock
import realinput
import winhome
import winsandbox

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNS = os.path.join(ROOT, 'runs')
SCENARIOS = os.path.join(ROOT, 'scenarios')
DOCS = os.path.join(ROOT, 'docs', 'renderer', 'wso', 'gltrace')
DRIVER = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                      'sandbox', 'gltrace-driver.ps1')

DEFAULT_GAME = winhome.WIN_HOME + '/iwd2r-ieex/game'
DEFAULT_APITRACE = winhome.WIN_HOME + '/iwd2r-apitrace'
BOX_APITRACE = r'C:\iwd2r\apitrace'
DEFAULT_TIMEOUT = 600  # when the scenario header sets none
# Extra seconds for the original's slower loads (movies, world entry) on
# top of the scenario's header timeout and the lead.
LOAD_HEADROOM = 300
LEAD_SECONDS = 50
# Seconds apitrace gets to flush the trace after the game exits, and the
# per-frame rows written into the .md (the whole table goes into the run
# folder as <scenario>-frames.json).
TRACE_WAIT = 300
FRAME_TABLE_MAX = 40

# The calls a frame's "draw calls" count. glDrawBuffers names a different
# thing (render targets) and is not here.
DRAW_CALLS = frozenset((
    'glDrawArrays', 'glDrawArraysInstanced',
    'glDrawArraysInstancedBaseInstance', 'glDrawArraysIndirect',
    'glDrawElements', 'glDrawElementsBaseVertex', 'glDrawElementsInstanced',
    'glDrawElementsInstancedBaseInstance',
    'glDrawElementsInstancedBaseVertex',
    'glDrawElementsInstancedBaseVertexBaseInstance',
    'glDrawElementsIndirect', 'glDrawRangeElements',
    'glDrawRangeElementsBaseVertex', 'glMultiDrawArrays',
    'glMultiDrawArraysIndirect', 'glMultiDrawArraysIndirectCount',
    'glMultiDrawElements', 'glMultiDrawElementsBaseVertex',
    'glMultiDrawElementsIndirect', 'glMultiDrawElementsIndirectCount',
))
# A frame ends at a buffer swap; calls after the last swap are no frame.
SWAP_CALLS = frozenset(('wglSwapBuffers', 'glXSwapBuffers', 'eglSwapBuffers'))
CALL_RE = re.compile(r'^(?:\d+ )?([A-Za-z0-9_]+)\((.*)\)\s*(?:=.*)?$')
NAMED_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$', re.S)


def log(msg):
    print(msg, flush=True)


def split_args(text):
    """An argument list split on its top-level commas."""
    parts, depth, current = [], 0, []
    for ch in text:
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if ch == ',' and depth <= 0:
            parts.append(''.join(current).strip())
            current = []
        else:
            current.append(ch)
    tail = ''.join(current).strip()
    if tail:
        parts.append(tail)
    return parts


def arg_value(part):
    """The value of one dumped argument: 'name = value' to value, or the
    part itself when apitrace printed no argument names."""
    m = NAMED_RE.match(part)
    return m.group(2).strip() if m else part


def tex_upload(args_text):
    """The {'size', 'format'} of one glTexImage2D argument list. The
    format is the texture's internal format (its third argument); size is
    'WxH', or the raw numbers when they are not whole."""
    named = {}
    positional = []
    for part in split_args(args_text):
        value = arg_value(part)
        positional.append(value)
        m = NAMED_RE.match(part)
        if m:
            named[m.group(1)] = value
    width = named.get('width', positional[3] if len(positional) > 3 else '')
    height = named.get('height', positional[4] if len(positional) > 4 else '')
    fmt = named.get('internalformat',
                    named.get('format', positional[2] if len(positional) > 2
                              else ''))
    return {'size': '%sx%s' % (width, height), 'format': fmt}


def summarize(lines):
    """Per-frame counts of an `apitrace dump`: {'frames': [{'draws',
    'fbo_binds', 'texture_uploads': [{'size', 'format'}]}], ...},
    'texture_dimensions': [...]} — every frame from after the previous
    buffer swap through the swap; draw calls are DRAW_CALLS; texture
    uploads are glTexImage2D (size x internal format); FBO binds are
    glBindFramebuffer calls; the distinct texture dimensions seen."""
    frames = []
    dimensions = set()
    frame = None

    def current():
        nonlocal frame
        if frame is None:
            frame = {'draws': 0, 'fbo_binds': 0, 'texture_uploads': []}
        return frame

    for line in lines:
        m = CALL_RE.match(line.strip())
        if not m:
            continue
        name, args_text = m.group(1), m.group(2)
        if name in SWAP_CALLS:
            if frame is not None:
                frames.append(frame)
            frame = None
        elif name == 'glTexImage2D':
            upload = tex_upload(args_text)
            current()['texture_uploads'].append(upload)
            if upload['size'].replace('x', '').isdigit():
                dimensions.add(upload['size'])
        elif name == 'glBindFramebuffer':
            current()['fbo_binds'] += 1
        elif name in DRAW_CALLS:
            current()['draws'] += 1
    return {'frames': frames,
            'texture_dimensions': sorted(dimensions)}


def trim_after_last_checkpoint(actions):
    """The actions up to and including the last checkpoint op: shot and
    quit ops need iwd2r's control channel, which the original lacks."""
    last = None
    for index, action in enumerate(actions):
        if action['op'] == 'checkpoint':
            last = index
    if last is None:
        raise ValueError('scenario has no checkpoint op')
    return actions[:last + 1]


def scenario_path(name):
    if os.path.isfile(name):
        return name
    base = name[:-4] if name.endswith('.txt') else name
    path = os.path.join(SCENARIOS, base + '.txt')
    if not os.path.isfile(path):
        raise SystemExit('gltrace: no scenario %s' % path)
    return path


def find_apitrace(root):
    """The bin/apitrace.exe under the unpacked apitrace folder (a WSL
    path, executable through the Windows interop)."""
    hits = []
    for base, dirs, files in os.walk(root):
        if 'apitrace.exe' in files and os.path.basename(base) == 'bin':
            dirs[:] = []
            hits.append(os.path.join(base, 'apitrace.exe'))
    if not hits:
        raise SystemExit('gltrace: no bin/apitrace.exe under %s' % root)
    return sorted(hits)[0]


def apitrace_version(apitrace):
    m = re.search(r'apitrace-([0-9.]+)-win32', apitrace)
    return m.group(1) if m else os.path.basename(os.path.dirname(
        os.path.dirname(apitrace)))


def make_run_dir(now):
    """runs/<stamp> (with -2, -3... if taken), like play.make_run_folder."""
    base = os.path.join(RUNS, now.strftime('%Y%m%d-%H%M%S'))
    path = base
    n = 2
    while os.path.exists(path):
        path = '%s-%d' % (base, n)
        n += 1
    os.makedirs(path)
    return path


def lead_actions(seconds):
    """The boot lead before the scenario's first action."""
    actions = [{'op': 'wait', 'frames': int(seconds * realinput.FRAME_RATE),
                'line': 0}]
    for frames in (300, 300):
        actions.append({'op': 'click', 'x': 400, 'y': 300, 'line': 0})
        actions.append({'op': 'wait', 'frames': frames, 'line': 0})
    return actions


def dump_trace(apitrace, trace, progress):
    """`apitrace dump` of trace streamed into summarize(). trace is a
    path the Windows apitrace understands."""
    cmd = [apitrace, 'dump', '--call-nos=true', '--arg-names=true',
           '--multiline=false', trace]
    log('dump: %s' % ' '.join(cmd))
    started = time.time()
    with tempfile.TemporaryFile() as err:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=err)
        seen = [0]

        def lines():
            for raw in proc.stdout:
                seen[0] += 1
                if seen[0] % 200000 == 0:
                    progress('  %d dump lines, %.0f s'
                             % (seen[0], time.time() - started))
                yield raw.decode('utf-8', 'replace')
        summary = summarize(lines())
        proc.stdout.close()
        proc.wait()
        err.seek(0)
        message = err.read().decode('utf-8', 'replace').strip()
    if proc.returncode != 0:
        raise RuntimeError('apitrace dump failed (%d): %s'
                           % (proc.returncode, message))
    return summary


def write_md(path, name, summary, meta):
    """The committed summary: shape of the stream, not a per-frame dump."""
    frames = summary['frames']
    draws = [f['draws'] for f in frames]
    uploads = [u for f in frames for u in f['texture_uploads']]
    fbo = sum(f['fbo_binds'] for f in frames)
    groups = sorted(collections.Counter(
        (u['size'], u['format']) for u in uploads).items())
    mean = sum(draws) / len(draws) if draws else 0.0
    lines = ['# GL trace: %s' % name, '']
    lines.append('Captured %s by tools/gltrace.py (%s), summary of '
                 '`apitrace %s dump`.' % (meta['stamp'][:10],
                                          meta['commit'], meta['version']))
    lines.append('')
    lines.append('- game: %s (IWD2EE.exe sha256 %s)'
                 % (meta['game'], meta['exe_sha']))
    lines.append('- apitrace: %s (trace --api gl -m: mhook, the game loads '
                 'OpenGL32.dll dynamically)' % meta['version'])
    lines.append('- sandbox GPU: %s' % meta['gpu'])
    lines.append('- scenario: scenarios/%s.txt, driven to its last '
                 'checkpoint (waits are wall-clock; %s s lead for the intro '
                 'movies)' % (name, meta['lead']))
    lines.append('- frames: %d' % len(frames))
    lines.append('- draw calls: %d total (mean %.1f/frame, max %d)'
                 % (sum(draws), mean, max(draws) if draws else 0))
    lines.append('- texture uploads (glTexImage2D): %d total' % len(uploads))
    for (size, fmt), count in groups:
        lines.append('  - %s %s: %d' % (size, fmt, count))
    lines.append('- FBO binds (glBindFramebuffer): %d' % fbo)
    lines.append('- distinct texture dimensions: %s'
                 % (', '.join(summary['texture_dimensions']) or 'none'))
    lines.append('')
    lines.append('A frame is the calls from after the previous buffer swap '
                 'through the swap; calls after the last swap are no frame.')
    lines.append('')
    if frames:
        lines.append('| frame | draws | fbo binds | uploads |')
        lines.append('|---|---|---|---|')
        for index, frame in enumerate(frames[:FRAME_TABLE_MAX]):
            lines.append('| %d | %d | %d | %d |' % (
                index + 1, frame['draws'], frame['fbo_binds'],
                len(frame['texture_uploads'])))
        if len(frames) > FRAME_TABLE_MAX:
            lines.append('')
            lines.append('%d frames total; the full per-frame table is in '
                         'runs/%s/%s-frames.json (not committed).'
                         % (len(frames), meta['stamp'], name))
    lines.append('')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    return path


def git_commit():
    try:
        out = subprocess.run(
            ['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, check=True).stdout.strip()
        return out or 'unknown'
    except OSError:
        return 'unknown'


def exe_sha(game):
    try:
        with open(os.path.join(game, 'IWD2EE.exe'), 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()[:16]
    except OSError:
        return 'unknown'


def copy_home(results, run_dir, name):
    """Brings the driver's files and the trace home into run_dir."""
    keep = ('driver.log', 'exitcode.txt', 'error.txt', 'log.txt',
            'stderr.txt', 'trace-stdout.txt', 'trace-stderr.txt')
    for entry in sorted(os.listdir(results)):
        source = os.path.join(results, entry)
        if entry in keep or (entry.endswith('-screen.png')):
            shutil.copy2(source, os.path.join(run_dir, entry))
    trace = os.path.join(results, name + '.trace')
    if os.path.isfile(trace):
        shutil.copy2(trace, os.path.join(run_dir, name + '.trace'))
        return os.path.getsize(os.path.join(run_dir, name + '.trace'))
    return 0


def sandbox_run(name, game, apitrace, sandbox_actions, timeout, run_dir,
                host_run):
    """One sandbox launch: maps the game at its own host path, the
    unpacked apitrace read-only, results read-write and the tools (the
    driver, config.json, actions.json, DirectPlay) read-only. Returns
    (exitcode, error, trace_bytes)."""
    results = os.path.join(host_run, 'results')
    tools = os.path.join(host_run, 'tools')
    os.makedirs(results)
    os.makedirs(tools)
    game_win = winsandbox.wslpath_w(game)
    config = {
        'scenario': name,
        'exe': game_win + '\\IWD2EE.exe',
        'data': game_win,
        'apitrace': box_apitrace(apitrace),
        'trace': winsandbox.BOX_RESULTS + '\\%s.trace' % name,
        'results': winsandbox.BOX_RESULTS,
        'actions': winsandbox.BOX_TOOLS + '\\actions.json',
        'frame_rate': realinput.FRAME_RATE,
        'timeout': timeout,
        'trace_wait': TRACE_WAIT,
        'directplay': winsandbox.BOX_TOOLS + '\\directplay',
        'exclusions': [BOX_APITRACE, game_win, winsandbox.BOX_RESULTS],
        'shutdown': True,
    }
    with open(os.path.join(tools, 'config.json'), 'w') as f:
        json.dump(config, f, indent=1)
    with open(os.path.join(tools, 'actions.json'), 'w') as f:
        json.dump(sandbox_actions, f, indent=1)
    shutil.copy2(DRIVER, os.path.join(tools, 'gltrace-driver.ps1'))
    winsandbox.stage_directplay(os.path.join(tools, 'directplay'))
    wsb = os.path.join(host_run, 'run.wsb')
    with open(wsb, 'w', newline='\r\n') as f:
        f.write(winsandbox.make_wsb(
            [(game_win, game_win, False),
             (winsandbox.wslpath_w(apitrace_root(apitrace)), BOX_APITRACE,
              True),
             (winsandbox.wslpath_w(results), winsandbox.BOX_RESULTS, False),
             (winsandbox.wslpath_w(tools), winsandbox.BOX_TOOLS, True)],
            'powershell.exe -NoProfile -ExecutionPolicy Bypass -File '
            '%s\\gltrace-driver.ps1 -Config %s\\config.json'
            % (winsandbox.BOX_TOOLS, winsandbox.BOX_TOOLS)))
    shutil.copy2(wsb, os.path.join(run_dir, 'run.wsb'))
    exitcode, error = None, None
    try:
        with filelock.hold(winsandbox.SANDBOX_LOCK, 'sandbox'):
            winsandbox.start_sandbox(host_run, wsb, log)
            winsandbox.wait_done(
                results, timeout + TRACE_WAIT + winsandbox.SANDBOX_ALLOWANCE)
        code = winsandbox.read_text(os.path.join(results, 'exitcode.txt'))
        if code.lstrip('-').isdigit():
            exitcode = int(code)
        error = winsandbox.read_text(os.path.join(results, 'error.txt'))
    except RuntimeError as e:
        error = str(e)
    finally:
        winsandbox.close_sandbox(log)
        trace_bytes = copy_home(results, run_dir, name)
        log('trace: %s (%d bytes)' % (os.path.join(run_dir, name + '.trace'),
                                      trace_bytes))
    return exitcode, error, trace_bytes


def apitrace_root(apitrace):
    """The unpacked apitrace folder to map (three levels up bin/)."""
    return os.path.dirname(os.path.dirname(os.path.dirname(apitrace)))


def box_apitrace(apitrace):
    """apitrace.exe's path inside the sandbox."""
    return BOX_APITRACE + '\\' + os.path.relpath(
        apitrace, apitrace_root(apitrace)).replace('/', '\\')


def run(args):
    apitrace = find_apitrace(args.apitrace)
    version = apitrace_version(apitrace)
    log('apitrace: %s (%s)' % (apitrace, version))
    if args.dump:
        summary = dump_trace(apitrace, winsandbox.wslpath_w(args.dump), log)
        log_totals(summary)
        return 0
    game = args.game
    if not os.path.isfile(os.path.join(game, 'IWD2EE.exe')):
        raise SystemExit('gltrace: no IWD2EE.exe in %s' % game)
    path = scenario_path(args.scenario)
    name = os.path.splitext(os.path.basename(path))[0]
    with open(path, encoding='utf-8') as f:
        header = realinput.header_timeout(f.read())
    trimmed = trim_after_last_checkpoint(realinput.load_scenario(path))
    lead = max(0.0, args.lead)
    lead_frames = int(lead * realinput.FRAME_RATE)
    timeout = args.timeout or (
        (header or DEFAULT_TIMEOUT) + lead_frames / realinput.FRAME_RATE
        + LOAD_HEADROOM)
    actions = realinput.windows_actions(
        lead_actions(lead) + trimmed, None)
    log('scenario %s: %d actions after the last checkpoint %s, '
        'timeout %d s' % (name, len(actions), [
            a['name'] for a in trimmed if a['op'] == 'checkpoint'][-1],
        timeout))
    if args.dry_run:
        for action in actions:
            log('  %s' % realinput.describe(action))
        log('dry-run: nothing launched')
        return 0
    now = datetime.datetime.now()
    run_dir = make_run_dir(now)
    host_run = os.path.join(winsandbox.VM_ROOT, 'runs',
                            os.path.basename(run_dir), name)
    ini = os.path.join(game, 'icewind2.ini')
    ini_before = os.path.join(run_dir, 'icewind2.ini.before')
    had_ini = os.path.isfile(ini)
    if had_ini:
        shutil.copy2(ini, ini_before)
    started = time.time()
    try:
        exitcode, error, trace_bytes = sandbox_run(
            name, game, apitrace, actions, timeout, run_dir, host_run)
    finally:
        if had_ini and os.path.isfile(ini):
            shutil.copy2(ini, os.path.join(run_dir, 'icewind2.ini.after'))
            shutil.copy2(ini_before, ini)
            log('restored icewind2.ini')
    seconds = time.time() - started
    if error:
        raise RuntimeError('driver: %s' % error)
    trace = os.path.join(run_dir, name + '.trace')
    if not trace_bytes:
        raise RuntimeError('no trace came home: %s' % trace)
    summary = dump_trace(apitrace, winsandbox.wslpath_w(trace), log)
    frames_json = os.path.join(run_dir, name + '-frames.json')
    with open(frames_json, 'w') as f:
        json.dump(summary, f, indent=1)
    meta = {
        'stamp': os.path.basename(run_dir),
        'commit': git_commit(),
        'game': game,
        'exe_sha': exe_sha(game),
        'version': version,
        'gpu': sandbox_gpu(run_dir),
        'lead': '%g' % lead,
        'timeout': timeout,
        'exitcode': exitcode,
        'seconds': seconds,
        'trace_bytes': trace_bytes,
    }
    os.makedirs(DOCS, exist_ok=True)
    md = write_md(os.path.join(DOCS, name + '.md'), name, summary, meta)
    log_totals(summary)
    log('summary: %s' % md)
    if exitcode:
        log('warning: apitrace exit code %s (see runs/%s/driver.log)'
            % (exitcode, os.path.basename(run_dir)))
    return 0


def log_totals(summary):
    """The one-line shape of a summarized trace."""
    draws = [f['draws'] for f in summary['frames']]
    log('frames %d, draws %d (max %d), dimensions %s'
        % (len(draws), sum(draws), max(draws) if draws else 0,
           ', '.join(summary['texture_dimensions']) or 'none'))


def sandbox_gpu(run_dir):
    try:
        with open(os.path.join(run_dir, 'driver.log'), encoding='utf-8') as f:
            for line in f:
                if 'video adapter: ' in line:
                    return line.split('video adapter: ', 1)[1].strip()
    except OSError:
        pass
    return 'unknown'


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description='Trace a scenario GL stream with apitrace in '
                    'Windows Sandbox')
    parser.add_argument('scenario',
                        help='scenario name under scenarios/, or a path')
    parser.add_argument('--game', default=DEFAULT_GAME,
                        help='the IWD2EE game folder (default %(default)s)')
    parser.add_argument('--apitrace', default=DEFAULT_APITRACE,
                        help='the unpacked apitrace folder '
                             '(default %(default)s)')
    parser.add_argument('--timeout', type=int,
                        help='driver deadline override (default the '
                             'scenario header + lead + %d s)'
                             % LOAD_HEADROOM)
    parser.add_argument('--lead', type=float, default=LEAD_SECONDS,
                        help='boot lead seconds before the first action '
                             '(default %(default)s)')
    parser.add_argument('--dump', metavar='TRACE',
                        help='summarize an existing trace, no launch')
    parser.add_argument('--dry-run', action='store_true',
                        help='print the plan, launch nothing')
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    return run(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
