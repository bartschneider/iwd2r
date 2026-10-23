#!/usr/bin/env python3
"""Trace a scenario's GL stream in the IWD2EE build with apitrace.

    python3 tools/gltrace.py <scenario> [--game DIR] [--apitrace DIR]
        [--timeout S] [--lead S] [--dump TRACE] [--dry-run]

One desktop launch per run, and only when IWD2R_DESKTOP_OK=1 (play.py's
gate: the game window opens on the user's desktop, who is there and
expects it; Windows Sandbox breaks the IEex DLL injection, so the trace
cannot be taken there). The tool writes config.json, actions.json and
its driver into a staging folder next to the game copy and runs them
through cmd.exe the way play.py launches the Windows build.

Both IWD2EE.exe and iwd2.exe are packed (empty import tables), so
`apitrace trace` cannot hook either. The capture instead uses apitrace's
wrapper: the tool copies apitrace's 32-bit lib/wrappers/opengl32.dll into
the game folder for the run and the driver launches the exact IEex chain
(IWD2EE.exe, which spawns iwd2.exe with IEex.dll injected and exits). The
game loads OpenGL32.dll by bare name, the DLL search finds the wrapper in
the application directory first, and the wrapper records the GL stream to
the path in TRACE_FILE while forwarding every call to the real system
DLL. The wrapper is removed after the run and the copy is re-hashed
against its pre-run state.

The driver answers the game's modal "IWD2:EE Resolution Select" dialog
(WM_SETTEXT 800/600 into the Custom edits, then 'Custom:' and 'SELECT',
then the 2x-UI warning's confirm; startup blocks until it is answered)
and drives
the scenario's input actions with realinput's coordinates; waits are
wall-clock, because the original has none of iwd2r's control channel.
The run stops after the scenario's last checkpoint op (shot/quit ops
would need the control channel) and closes the game; the wrapper's trace
is complete when the game exits.

Before the launch the copy's icewind2.ini is rewritten for the run and
restored after: its [Alias] paths name the source install, which the
game must not touch, and the volume keys go to 0 (there is no
system-wide muting on the desktop). The backup lives in the run folder.

Host-side the tool then streams `apitrace dump` into summarize(), keeps
the trace at runs/<stamp>/<scenario>.trace (never committed) and writes
docs/renderer/wso/gltrace/<scenario>.md. --dump summarizes an existing
trace without a launch.

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

import attempt
import realinput
import winhome
import winsandbox

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNS = os.path.join(ROOT, 'runs')
SCENARIOS = os.path.join(ROOT, 'scenarios')
DOCS = os.path.join(ROOT, 'docs', 'renderer', 'wso', 'gltrace')

DEFAULT_GAME = winhome.WIN_HOME + '/iwd2r-ieex/game'
DEFAULT_APITRACE = winhome.WIN_HOME + '/iwd2r-apitrace'
CMD_EXE = '/mnt/c/Windows/System32/cmd.exe'
DESKTOP_OK = 'IWD2R_DESKTOP_OK'
DRIVER_PS1 = 'gltrace-desktop-driver.ps1'
DEFAULT_TIMEOUT = 600  # when the scenario header sets none
# Extra seconds for the original's slower loads (movies, world entry) on
# top of the scenario's header timeout and the lead.
LOAD_HEADROOM = 300
LEAD_SECONDS = 50
# Per-frame rows written into the .md (the whole table goes into the run
# folder as <scenario>-frames.json).
FRAME_TABLE_MAX = 40

# The capture wrapper: apitrace's lib/wrappers/opengl32.dll goes beside
# iwd2.exe for the run (the game loads OpenGL32.dll by bare name; the
# application directory wins the DLL search), records the GL stream to
# TRACE_FILE and forwards to the real system DLL.
WRAPPER_SUBPATH = os.path.join('lib', 'wrappers', 'opengl32.dll')
WRAPPER_NAME = 'opengl32.dll'
TRACE_ENV = 'TRACE_FILE'
# Top-level folders of the game copy the game freely rewrites at runtime;
# the post-run tree comparison skips them.
MANIFEST_EXCLUDES = frozenset(('cache', 'temp', 'tempsave', 'mpsave',
                               'crash'))

# The calls a frame's "draw calls" count. glDrawBuffers names a different
# thing (render targets) and is not here. glBegin is here because the
# engine's renderer is immediate mode (the 203206 trace holds 166204
# glBegin blocks and not one vertex-array draw): a glBegin/glEnd block
# is one draw.
DRAW_CALLS = frozenset((
    'glBegin',
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
# The FBO binds an "fbo binds" count (the engine uses the EXT names on
# Windows: 130824 glBindFramebufferEXT in the 203206 trace, and no core
# glBindFramebuffer at all).
FBO_BIND_CALLS = frozenset(('glBindFramebuffer', 'glBindFramebufferEXT'))
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


def parsed_args(args_text):
    """One dumped argument list as {'named': {name: value}, 'pos':
    [value, ...]} — every value in call order, plus the named form when
    apitrace printed argument names."""
    named, pos = {}, []
    for part in split_args(args_text):
        value = arg_value(part)
        pos.append(value)
        m = NAMED_RE.match(part)
        if m:
            named[m.group(1)] = value
    return {'named': named, 'pos': pos}


def _upload_size(args, width_at, height_at):
    """The 'WxH' of an upload's width/height ('%sx%s' of the raw values
    when they are not whole numbers)."""
    named, pos = args['named'], args['pos']
    width = named.get('width', pos[width_at] if len(pos) > width_at else '')
    height = named.get('height', pos[height_at] if len(pos) > height_at
                       else '')
    return '%sx%s' % (width, height)


def tex_upload(args_text):
    """The {'size', 'format'} of one glTexImage2D argument list. The
    format is the texture's internal format (its third argument); size is
    'WxH', or the raw numbers when they are not whole."""
    args = parsed_args(args_text)
    fmt = args['named'].get(
        'internalformat',
        args['named'].get('format', args['pos'][2] if len(args['pos']) > 2
                          else ''))
    return {'size': _upload_size(args, 3, 4), 'format': fmt}


def tex_sub_upload(args_text):
    """The {'size', 'format'} of one glTexSubImage2D argument list. A
    sub-image replaces pixels of an existing texture: its format argument
    (the seventh) is the client-side pixel layout, not the texture's
    internal format, which this call does not carry — so the format is
    left empty. Size is width x height (the fifth and sixth arguments)."""
    return {'size': _upload_size(parsed_args(args_text), 4, 5),
            'format': ''}


def summarize(lines):
    """Per-frame counts of an `apitrace dump`: {'frames': [{'draws',
    'fbo_binds', 'texture_uploads': [{'size', 'format'}]}], ...},
    'texture_dimensions': [...]} — every buffer swap ends a frame (the
    calls from after the previous swap through the swap; an empty swap
    still presented a frame); draw calls are DRAW_CALLS (the engine's
    immediate-mode glBegin blocks included); texture uploads are
    glTexImage2D (size x internal format) and glTexSubImage2D (size,
    empty format); FBO binds are FBO_BIND_CALLS; the distinct texture
    dimensions seen."""
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
            frames.append(frame if frame is not None
                          else {'draws': 0, 'fbo_binds': 0,
                                'texture_uploads': []})
            frame = None
        elif name in ('glTexImage2D', 'glTexSubImage2D'):
            upload = (tex_upload if name == 'glTexImage2D'
                      else tex_sub_upload)(args_text)
            current()['texture_uploads'].append(upload)
            if upload['size'].replace('x', '').isdigit():
                dimensions.add(upload['size'])
        elif name in FBO_BIND_CALLS:
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


def find_wrapper(root):
    """The lib/wrappers/opengl32.dll under the unpacked apitrace folder
    (a WSL path; it is copied into the game folder for the run)."""
    wanted = os.path.normpath(WRAPPER_SUBPATH)
    hits = []
    for base, dirs, files in os.walk(root):
        rel = os.path.normpath(os.path.relpath(base, root))
        # Match <root>/.../lib/wrappers (the unpacked tree has a versioned
        # folder such as apitrace-14.0-win32 above lib/).
        parts = rel.split(os.sep)
        tail = os.path.join(*parts[-2:]) if len(parts) >= 2 else ''
        if tail == os.path.dirname(wanted) and WRAPPER_NAME in files:
            dirs[:] = []
            hits.append(os.path.join(base, WRAPPER_NAME))
    if not hits:
        raise SystemExit('gltrace: no %s under %s' % (WRAPPER_SUBPATH, root))
    return sorted(hits)[0]


def pe_machine(path):
    """The machine type of a PE image (0x014c x86, 0x8664 x64), or None
    when the file is not a PE image."""
    try:
        with open(path, 'rb') as f:
            data = f.read(4096)
    except OSError:
        return None
    if len(data) < 0x40 or data[:2] != b'MZ':
        return None
    pe = int.from_bytes(data[0x3c:0x40], 'little')
    if pe < 0 or pe + 6 > len(data) or data[pe:pe + 4] != b'PE\x00\x00':
        return None
    return int.from_bytes(data[pe + 4:pe + 6], 'little')


def snapshot_manifest(root, excludes=MANIFEST_EXCLUDES):
    """{relpath: {'size', 'sha256'}} of every file under root, skipping
    the top-level folders in excludes (the game rewrites them at
    runtime). Relpaths use '/'."""
    manifest = {}
    for base, dirs, files in os.walk(root):
        top = os.path.relpath(base, root).split(os.sep)[0]
        if top in excludes:
            dirs[:] = []
            continue
        for entry in files:
            path = os.path.join(base, entry)
            key = os.path.relpath(path, root).replace(os.sep, '/')
            digest = hashlib.sha256()
            size = 0
            with open(path, 'rb') as f:
                while True:
                    chunk = f.read(1 << 20)
                    if not chunk:
                        break
                    size += len(chunk)
                    digest.update(chunk)
            manifest[key] = {'size': size, 'sha256': digest.hexdigest()}
    return manifest


def manifest_delta(before, after):
    """The file-level difference of two snapshot_manifest() results:
    sorted added, removed and changed relpaths."""
    return {
        'added': sorted(set(after) - set(before)),
        'removed': sorted(set(before) - set(after)),
        'changed': sorted(key for key in set(before) & set(after)
                          if before[key] != after[key]),
    }


def place_wrapper(wrapper, game):
    """Copies the apitrace wrapper beside iwd2.exe for the run. Refuses
    to overwrite an opengl32.dll that is already there (a real wrapper
    left over, or something else entirely)."""
    dest = os.path.join(game, WRAPPER_NAME)
    if os.path.exists(dest):
        raise SystemExit('gltrace: %s already exists in the game folder; '
                         'not overwriting it' % dest)
    shutil.copy2(wrapper, dest)
    return dest


def remove_wrapper(game):
    """Undoes place_wrapper(); returns whether a wrapper was removed."""
    try:
        os.unlink(os.path.join(game, WRAPPER_NAME))
    except FileNotFoundError:
        return False
    return True


def remove_stray_traces(game):
    """Deletes any .trace under the game folder (a trace belongs under
    runs/, never in the copy) and returns their relpaths."""
    strays = []
    for base, dirs, files in os.walk(game):
        for entry in files:
            if entry.lower().endswith('.trace'):
                path = os.path.join(base, entry)
                os.unlink(path)
                strays.append(os.path.relpath(path, game).replace(os.sep,
                                                                  '/'))
    return sorted(strays)


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


# ---------------------------------------------------------------------------
# The desktop leg (pure parts, unit-tested in tests/test_tools.py)

def desktop_allowed(env):
    """play.py's gate: a launch that opens the game window on the user's
    desktop happens only when IWD2R_DESKTOP_OK=1 (the user is at the PC
    and expects the window)."""
    return env.get(DESKTOP_OK) == '1'


ALIAS_VALUES = (('hd0:', 'HD0:=', '\\'), ('cd1:', 'CD1:=', '\\Data\\'),
                ('cd2:', 'CD2:=', '\\CD2\\'))
VOLUME_KEY_RE = re.compile(r'(?i)^volume\s+\w+$')


def ini_overrides(text, game):
    """The icewind2.ini text for one run: [Alias] points at the game
    folder (the copy ships pointing at the source install, which the game
    would read and write) and the game's volume keys are 0 (there is no
    system-wide muting on the desktop). Everything else is kept. Raises
    ValueError when an alias key is missing, rather than leave the game
    pointing at the source install."""
    game = game.rstrip('\\')
    alias = {old: head + game + tail for old, head, tail in ALIAS_VALUES}
    seen = set()
    out = []
    for line in text.split('\n'):
        key, eq, _ = line.partition('=')
        low = key.strip().lower()
        if eq and low in alias:
            seen.add(low)
            out.append(alias[low])
        elif eq and VOLUME_KEY_RE.match(key.strip()):
            out.append(key.rstrip() + '=0')
        else:
            out.append(line)
    missing = sorted(set(alias) - seen)
    if missing:
        raise ValueError('icewind2.ini has no %s alias' % ', '.join(
            sorted(old.rstrip(':').upper() for old in missing)))
    return '\n'.join(out)


def desktop_config(name, game, results, timeout):
    """The desktop driver's config: everything on the Windows side, no
    sandbox mappings. The trace path doubles as the wrapper's TRACE_FILE
    value."""
    return {
        'scenario': name,
        'exe': game + '\\IWD2EE.exe',
        'data': game,
        'trace': results + '\\' + name + '.trace',
        'results': results,
        'actions': results + '\\actions.json',
        'frame_rate': realinput.FRAME_RATE,
        'timeout': timeout,
    }


def desktop_bat(results_win):
    """The run.bat that runs the driver, for cmd.exe (play.py's launch
    pattern); it blocks until the driver is done."""
    return '\n'.join((
        '@echo off',
        'cd /d ' + results_win,
        'powershell.exe -NoProfile -ExecutionPolicy Bypass -File '
        + DRIVER_PS1 + ' -Config config.json',
        'exit /b %ERRORLEVEL%',
    ))


# The desktop driver, written next to config.json in the staging folder.
# Ported from tools/sandbox/gltrace-driver.ps1 with the sandbox-only parts
# gone: this script changes nothing on the machine (no service, scheduled
# task, antivirus or system-file change, no shutdown); its only effects are
# its own processes, the synthetic input it sends, and the game folder's
# icewind2.ini, which gltrace.py rewrites for the run and restores, and
# the opengl32.dll wrapper, which gltrace.py places and removes.
DESKTOP_DRIVER = r'''<#
.SYNOPSIS
Traces one scenario of the IWD2EE build on the host desktop.

.DESCRIPTION
tools/gltrace.py writes config.json, actions.json and this script into a
staging folder under %USERPROFILE%\iwd2r-ieex\runs\<stamp>\<scenario>\ and
launches it through cmd.exe, the way play.py launches the Windows build.
gltrace.py starts it only with IWD2R_DESKTOP_OK=1 in the environment: the
user's explicit opt-in to a game window on their desktop for a few minutes
(the desktop is the only place the game runs - Windows Sandbox breaks the
IEex DLL injection). One launch per scenario; there is no retry.

The capture is apitrace's opengl32.dll wrapper: gltrace.py copies it into
the game folder for the run (beside iwd2.exe; the game loads OpenGL32.dll
by bare name and the application directory wins the DLL search), and this
script starts the exact IEex chain

  IWD2EE.exe   (with TRACE_FILE=<results>\<scenario>.trace in the env)

IWD2EE.exe spawns iwd2.exe with IEex.dll injected and exits, so the script
looks for the game window among the visible top-level windows of the
loader's descendants (three levels deep by parent process id, which
survives IWD2EE.exe exiting), skips dialog-class windows, and drives the
scenario's input actions with SendInput. Waits are wall-clock only: the
original has none of iwd2r's control channel, so frames cannot be polled,
and shot/checkpoint ops (gltrace.py drops them) are logged and skipped.

The game's first window is the modal "IWD2:EE Resolution Select" box and
startup blocks until it is answered: the script polls for it for ~20 s,
writes 800/600 into the Custom width/height edits (WM_SETTEXT under the
send deadline; the read-back is logged before anything is clicked),
BM_CLICKs 'Custom:' (id 1200) and 'SELECT' (id 1), and answers the
2x-UI minimum warning at its confirm button. When no dialog appears the
choice is persisted and the script moves on.

After the last action the script closes the game (WM_CLOSE, its
confirmation dialog answered with Return, then a force kill). The wrapper
finishes the trace when the game exits; the script then checks the trace
is there and non-empty.

Foreground losses are logged, not fatal: this is a capture tool, and the
trace of what did run is still worth keeping.

The results folder receives driver.log, <scenario>.trace, exitcode.txt
(the game's exit code), error.txt (only when the driver failed) and
finally done.txt; gltrace.py copies them home.
#>
param(
    [Parameter(Mandatory = $true)][string]$Config
)

$ErrorActionPreference = 'Stop'
$started = Get-Date
$cfg = Get-Content -Raw -LiteralPath $Config | ConvertFrom-Json
# Windows PowerShell 5.1 emits a JSON array as one object; the second
# pipeline enumerates it.
$actions = @((Get-Content -Raw -LiteralPath $cfg.actions | ConvertFrom-Json) | ForEach-Object { $_ })
$frameRate = [double]$cfg.frame_rate
$deadline = $started.AddSeconds([double]$cfg.timeout)
$logPath = Join-Path $cfg.results 'driver.log'

function Log([string]$message) {
    $elapsed = ((Get-Date) - $started).TotalSeconds
    $line = '{0} +{1,7:0.000} {2}' -f (Get-Date -Format 'HH:mm:ss.fff'), $elapsed, $message
    Write-Host $line
    Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
}

$source = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class IwdInput {
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X; public int Y; }

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    struct MOUSEINPUT {
        public int dx; public int dy; public uint mouseData; public uint dwFlags;
        public uint time; public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct KEYBDINPUT {
        public ushort wVk; public ushort wScan; public uint dwFlags;
        public uint time; public IntPtr dwExtraInfo;
    }

    [StructLayout(LayoutKind.Explicit)]
    struct INPUTUNION {
        [FieldOffset(0)] public MOUSEINPUT mi;
        [FieldOffset(0)] public KEYBDINPUT ki;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct INPUT { public uint type; public INPUTUNION u; }

    delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    public class ForegroundChange {
        public string Time;
        public uint Pid;
        public string Window;
        // The game window (GameWindow) still existed and was visible: the
        // window took the foreground from it, rather than after it closed.
        public bool GameShown;
    }

    // The game window, for the watcher; the driver sets it.
    public static volatile IntPtr GameWindow = IntPtr.Zero;

    [DllImport("user32.dll", SetLastError = true)]
    static extern uint SendInput(uint count, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int size);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc proc, IntPtr lParam);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int GetClassName(IntPtr hWnd, StringBuilder name, int size);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr hWnd, EnumWindowsProc proc, IntPtr lParam);
    [DllImport("user32.dll", SetLastError = true)]
    static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint msg, IntPtr w, IntPtr l, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint msg, IntPtr w, StringBuilder l, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint msg, IntPtr w, string l, uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr hWnd);

    const uint INPUT_MOUSE = 0, INPUT_KEYBOARD = 1;
    const uint MOUSEEVENTF_LEFTDOWN = 0x02, MOUSEEVENTF_LEFTUP = 0x04;
    const uint MOUSEEVENTF_RIGHTDOWN = 0x08, MOUSEEVENTF_RIGHTUP = 0x10;
    const uint KEYEVENTF_EXTENDEDKEY = 0x1, KEYEVENTF_KEYUP = 0x2;
    public const uint WM_CLOSE = 0x10;
    public const uint WM_SETTEXT = 0x000C;
    public const uint WM_GETTEXT = 0x000D, WM_GETTEXTLENGTH = 0x000E;
    public const uint BM_CLICK = 0x00F5;

    static void Send(INPUT input) {
        INPUT[] inputs = new INPUT[] { input };
        if (SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT))) != 1)
            throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
    }

    public static void Button(bool right, bool down) {
        INPUT input = new INPUT();
        input.type = INPUT_MOUSE;
        input.u.mi.dwFlags = right ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                                   : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        Send(input);
    }

    public static void Key(ushort vk, bool down, bool extended) {
        INPUT input = new INPUT();
        input.type = INPUT_KEYBOARD;
        input.u.ki.wVk = vk;
        input.u.ki.wScan = (ushort)MapVirtualKey(vk, 0);
        input.u.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
        Send(input);
    }

    public static POINT ToScreen(IntPtr hWnd, int x, int y) {
        POINT p = new POINT();
        p.X = x;
        p.Y = y;
        if (!ClientToScreen(hWnd, ref p))
            throw new InvalidOperationException("ClientToScreen failed");
        return p;
    }

    // The window's pixel area, or 0.
    public static int Area(IntPtr hWnd) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) return 0;
        return Math.Max(0, r.Right - r.Left) * Math.Max(0, r.Bottom - r.Top);
    }

    // The process owning a window.
    public static uint PidOf(IntPtr hWnd) {
        uint pid;
        GetWindowThreadProcessId(hWnd, out pid);
        return pid;
    }

    // "0xHWND CLASS 'TITLE' pid N", or "none".
    public static string Describe(IntPtr hWnd) {
        if (hWnd == IntPtr.Zero)
            return "none";
        uint pid;
        GetWindowThreadProcessId(hWnd, out pid);
        StringBuilder name = new StringBuilder(256);
        GetClassName(hWnd, name, name.Capacity);
        StringBuilder title = new StringBuilder(256);
        GetWindowText(hWnd, title, title.Capacity);
        return string.Format("0x{0:x} {1} '{2}' pid {3}", hWnd.ToInt64(), name, title, pid);
    }

    public static string ClassOf(IntPtr hWnd) {
        StringBuilder name = new StringBuilder(256);
        GetClassName(hWnd, name, name.Capacity);
        return name.ToString();
    }

    // Every change of the foreground window, polled every 2 ms on a thread
    // of its own, so a window that holds the foreground for a few
    // milliseconds is seen too.
    static System.Collections.Concurrent.ConcurrentQueue<ForegroundChange> changes =
        new System.Collections.Concurrent.ConcurrentQueue<ForegroundChange>();
    static System.Threading.Thread watcher;

    public static void WatchForeground() {
        if (watcher != null)
            return;
        watcher = new System.Threading.Thread(delegate() {
            IntPtr last = IntPtr.Zero;
            while (true) {
                IntPtr now = GetForegroundWindow();
                if (now != last) {
                    last = now;
                    ForegroundChange change = new ForegroundChange();
                    change.Time = DateTime.Now.ToString("HH:mm:ss.fff");
                    uint pid = 0;
                    if (now != IntPtr.Zero)
                        GetWindowThreadProcessId(now, out pid);
                    change.Pid = pid;
                    change.Window = Describe(now);
                    IntPtr game = GameWindow;
                    change.GameShown = game != IntPtr.Zero && IsWindow(game) && IsWindowVisible(game);
                    changes.Enqueue(change);
                }
                System.Threading.Thread.Sleep(2);
            }
        });
        watcher.IsBackground = true;
        watcher.Start();
    }

    public static ForegroundChange[] ForegroundChanges() {
        List<ForegroundChange> list = new List<ForegroundChange>();
        ForegroundChange change;
        while (changes.TryDequeue(out change))
            list.Add(change);
        return list.ToArray();
    }

    // The visible top-level windows of a process, optionally of one class.
    // The callback is a static method behind a static delegate field: an
    // inline anonymous delegate passed straight to EnumWindows was observed
    // to silently enumerate nothing (a reversed delegate must stay rooted
    // for the whole native call; a static field guarantees that).
    static IntPtr[] windowsArg = new IntPtr[0];
    static List<IntPtr> windowsFound;
    static int windowsPid;
    static string windowsClass;
    static bool WindowsCallback(IntPtr hWnd, IntPtr lParam) {
        uint owner;
        GetWindowThreadProcessId(hWnd, out owner);
        if (owner != (uint)windowsPid || !IsWindowVisible(hWnd))
            return true;
        if (!string.IsNullOrEmpty(windowsClass)) {
            StringBuilder name = new StringBuilder(256);
            GetClassName(hWnd, name, name.Capacity);
            if (name.ToString() != windowsClass)
                return true;
        }
        windowsFound.Add(hWnd);
        return true;
    }
    static EnumWindowsProc windowsProc = new EnumWindowsProc(WindowsCallback);
    public static IntPtr[] Windows(int pid, string className) {
        windowsFound = new List<IntPtr>();
        windowsPid = pid;
        windowsClass = className;
        EnumWindows(windowsProc, IntPtr.Zero);
        return windowsFound.ToArray();
    }

    // Every visible top-level window on the desktop (any process). Same
    // static-delegate pattern as Windows(): the callback must stay rooted
    // for the whole native call.
    static List<IntPtr> allFound;
    static bool AllCallback(IntPtr hWnd, IntPtr lParam) {
        if (IsWindowVisible(hWnd))
            allFound.Add(hWnd);
        return true;
    }
    static EnumWindowsProc allProc = new EnumWindowsProc(AllCallback);
    public static IntPtr[] AllWindows() {
        allFound = new List<IntPtr>();
        EnumWindows(allProc, IntPtr.Zero);
        return allFound.ToArray();
    }

    // The child windows of a dialog (its buttons, lists, ...).
    static List<IntPtr> childrenFound;
    static bool ChildrenCallback(IntPtr hWnd, IntPtr lParam) {
        childrenFound.Add(hWnd);
        return true;
    }
    static EnumWindowsProc childrenProc = new EnumWindowsProc(ChildrenCallback);
    public static IntPtr[] ChildWindows(IntPtr hWnd) {
        childrenFound = new List<IntPtr>();
        EnumChildWindows(hWnd, childrenProc, IntPtr.Zero);
        return childrenFound.ToArray();
    }

    // Sends a window message with a deadline. Every cross-process send in
    // this driver goes through here: a game UI thread that stopped pumping
    // must not block the driver on one SendMessage forever (run
    // 20261018-185556 sat 48 minutes inside such a send until the game was
    // killed by hand). SMTO_ABORTIFHUNG aborts once Windows marks the
    // target hung, the 10 s timeout catches the rest; a healthy window
    // answers in milliseconds either way. On abort the call returns 0,
    // which the callers already treat as "no answer / empty text".
    static IntPtr SendDeadline(IntPtr hWnd, uint msg, IntPtr w, IntPtr l) {
        IntPtr res;
        SendMessageTimeout(hWnd, msg, w, l, 0x2 /* SMTO_ABORTIFHUNG */, 10000, out res);
        return res;
    }
    static IntPtr SendDeadline(IntPtr hWnd, uint msg, IntPtr w, StringBuilder l) {
        IntPtr res;
        SendMessageTimeout(hWnd, msg, w, l, 0x2 /* SMTO_ABORTIFHUNG */, 10000, out res);
        return res;
    }

    // A window's title text.
    public static string TextOf(IntPtr hWnd) {
        int len = (int)SendDeadline(hWnd, WM_GETTEXTLENGTH, IntPtr.Zero, IntPtr.Zero);
        if (len <= 0)
            return "";
        StringBuilder text = new StringBuilder(len + 1);
        SendDeadline(hWnd, WM_GETTEXT, (IntPtr)text.Capacity, text);
        return text.ToString();
    }

    // Sets a window's text with a deadline: WM_SETTEXT into the resolution
    // dialog's Custom width/height edits is the one cross-process write
    // that works here (LB_GETTEXT reads nothing from the owner-drawn list,
    // runs 185556 and 200445). Returns whether the target answered.
    public static bool SendText(IntPtr hWnd, string text) {
        IntPtr res;
        SendMessageTimeout(hWnd, WM_SETTEXT, IntPtr.Zero, text,
                           0x2 /* SMTO_ABORTIFHUNG */, 10000, out res);
        return res != IntPtr.Zero;
    }

    // Sends a message and returns its result as int (the driver's
    // BM_CLICK calls go through this).
    public static int Send(uint msg, IntPtr hWnd, IntPtr w, IntPtr l) {
        return (int)SendDeadline(hWnd, msg, w, l);
    }

    // A child dialog control's resource id (-1 when it has none).
    public static int CtrlId(IntPtr hWnd) {
        return GetDlgCtrlID(hWnd);
    }
}
'@
Add-Type -TypeDefinition $source -Language CSharp
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$script:loader = $null   # IWD2EE.exe; it exits once iwd2.exe is going
$script:game = $null
$script:gamePid = [uint32]0
$script:window = [IntPtr]::Zero
# The scenario line being executed, and whether foreground changes count:
# from the first action until the last one.
$script:line = 0
$script:watching = $false
$script:focusLosses = New-Object System.Collections.Generic.List[string]
$script:lastTraceLog = [DateTime]::MinValue
$script:lastTraceSize = -1

# Logs the foreground changes the watcher saw since the last call. While the
# scenario runs, a window of another process taking the foreground from the
# game window is a focus loss: input sent meanwhile may not have reached the
# game. Logged, not fatal (see the header comment).
function Watch-Foreground {
    foreach ($c in [IwdInput]::ForegroundChanges()) {
        Log "  foreground at $($c.Time): $($c.Window)"
        if ($script:watching -and $c.GameShown -and $c.Pid -ne 0 -and $c.Pid -ne $script:gamePid) {
            $script:focusLosses.Add("line $($script:line): $($c.Window) at $($c.Time)")
        }
    }
}

# Throws when the run cannot go on: the game died early, or the deadline
# passed.
function Check {
    if ($script:game -ne $null -and $script:game.HasExited) {
        throw [System.OperationCanceledException]::new('game exited')
    }
    Watch-Foreground
    Trace-Progress
    if ((Get-Date) -gt $deadline) { throw "scenario timeout ($($cfg.timeout) s)" }
}

# Logs the trace's growth once in a while: a wrapper that never got loaded
# shows up as a size stuck at 0.
function Trace-Progress {
    if ((Get-Date) -lt $script:lastTraceLog.AddSeconds(30)) { return }
    $script:lastTraceLog = Get-Date
    $size = 0
    if ([System.IO.File]::Exists($cfg.trace)) {
        $size = (Get-Item -LiteralPath $cfg.trace).Length
    }
    if ($size -ne $script:lastTraceSize) {
        $script:lastTraceSize = $size
        Log ('  trace {0} bytes' -f $size)
    }
}

# Wall-clock only: the original has none of iwd2r's control channel, so
# frames cannot be polled; a frame is 1/$frameRate seconds.
function Sleep-Frames([double]$frames) {
    $end = (Get-Date).AddSeconds($frames / $frameRate)
    while ($true) {
        Check
        $left = ($end - (Get-Date)).TotalMilliseconds
        if ($left -le 0) { return }
        Start-Sleep -Milliseconds ([Math]::Min(100, [Math]::Ceiling($left)))
    }
}

# The pids of the loader's descendants, three levels deep (IWD2EE.exe ->
# iwd2.exe -> any child it spawns). The walk is by recorded parent process
# id, so it survives IWD2EE.exe exiting.
function Get-DescendantIds([uint32]$root) {
    $all = New-Object System.Collections.Generic.List[uint32]
    $frontier = New-Object System.Collections.Generic.List[uint32]
    $frontier.Add($root)
    for ($depth = 0; $depth -lt 3 -and $frontier.Count -gt 0; $depth++) {
        $next = New-Object System.Collections.Generic.List[uint32]
        foreach ($parent in $frontier) {
            $children = @(Get-CimInstance -Query "SELECT ProcessId FROM Win32_Process WHERE ParentProcessId = $parent" -ErrorAction SilentlyContinue)
            foreach ($c in $children) {
                $id = [uint32]$c.ProcessId
                if (-not $all.Contains($id)) {
                    $all.Add($id)
                    $next.Add($id)
                }
            }
        }
        $frontier = $next
    }
    return ,$all
}

# The game process for the error paths: the iwd2.exe among the loader's
# descendants, resolved to an exact pid, which the finally then kills by
# pid (never by image name - a name-wide kill could hit a process this run
# did not start). This is what a run ends without Find-GameWindow having
# identified the game: the window-wait expiry, and every other throw via
# the finally. The stuck 20261018-185556 run left iwd2.exe sitting at the
# resolution dialog for 48 minutes because $script:game stayed null on
# that path and the finally only ever killed what it knew.
function Find-GameProcess {
    if ($script:loader -eq $null -or $script:game -ne $null) { return }
    foreach ($procId in (Get-DescendantIds $script:loader.Id)) {
        $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
        if ($p -ne $null -and $p.ProcessName -eq 'iwd2') {
            $script:gamePid = [uint32]$p.Id
            $script:game = $p
            $null = $script:game.Handle # keeps ExitCode readable after exit
            Log ('  game process by parent walk, no window: pid {0}' -f $p.Id)
            return
        }
    }
}

# The game's first window is the modal "IWD2:EE Resolution Select" box
# (#32770): IEex_Gui_State.lua:2895 calls IEex_Helper_AskResolution
# unconditionally, so the dialog shows on every launch (the ini's
# AskResolution string is the helper function's name, not a key) and
# startup blocks until it is answered. Its ListBox (id 1004) is
# owner-drawn: cross-process LB_GETTEXT returns empty strings for every
# entry (runs 20261018-185556 and -200445), so no text match can ever
# pick 800x600 from it. The Custom fields are plain Edit controls and
# WM_SETTEXT works across processes: write 800 and 600 into the width
# (id 1201) and height (id 1203) edits, BM_CLICK 'Custom:' (id 1200),
# then BM_CLICK 'SELECT' (id 1). 800x600 because the scenario
# coordinates are written for the copy's Resolution=800; 1366x768 would
# leave every click misaligned. A pick below IEex's 2x UI minimum brings
# up a warning box: if one appears, BM_CLICK its confirm button (id 1
# or 6, else the first button). Confirming with nothing selected ends
# the game (the 20261018-180428 run) - the read-back of both edits is
# logged before anything is clicked. When the dialog never shows, the
# choice is persisted - log and move on, that is not a failure.
function Answer-ResolutionDialog {
    $end = (Get-Date).AddSeconds(20)
    while ((Get-Date) -lt $end) {
        Check
        foreach ($h in [IwdInput]::AllWindows()) {
            if ([IwdInput]::ClassOf($h) -ne '#32770') { continue }
            $title = [IwdInput]::TextOf($h)
            if ($title -notmatch 'Resolution') { continue }
            $ownerPid = [IwdInput]::PidOf($h)
            $owner = try { (Get-Process -Id $ownerPid -ErrorAction Stop).ProcessName } catch { '?' }
            foreach ($c in [IwdInput]::ChildWindows($h)) {
                Log ("  dialog child 0x{0:x} class '{1}' id {2} text '{3}'" -f $c.ToInt64(), [IwdInput]::ClassOf($c), [IwdInput]::CtrlId($c), [IwdInput]::TextOf($c))
            }
            $width = [IntPtr]::Zero
            $height = [IntPtr]::Zero
            $custom = [IntPtr]::Zero
            $button = [IntPtr]::Zero
            foreach ($c in [IwdInput]::ChildWindows($h)) {
                $id = [IwdInput]::CtrlId($c)
                $class = [IwdInput]::ClassOf($c)
                if ($id -eq 1201 -and $class -eq 'Edit') { $width = $c }
                if ($id -eq 1203 -and $class -eq 'Edit') { $height = $c }
                if ($id -eq 1200 -and $class -eq 'Button') { $custom = $c }
                if ($id -eq 1 -and $class -eq 'Button') { $button = $c }
            }
            if ($width -eq [IntPtr]::Zero -or $height -eq [IntPtr]::Zero -or $custom -eq [IntPtr]::Zero -or $button -eq [IntPtr]::Zero) {
                throw ('resolution dialog is missing a control (width 0x{0:x}, height 0x{1:x}, custom 0x{2:x}, select 0x{3:x})' -f $width.ToInt64(), $height.ToInt64(), $custom.ToInt64(), $button.ToInt64())
            }
            # WM_SETTEXT with a deadline on both edits; the read-back in the
            # log proves what was written before anything is clicked.
            $wOk = [IwdInput]::SendText($width, '800')
            $hOk = [IwdInput]::SendText($height, '600')
            Log ('  custom width now ''{0}'' (sent {1}), height now ''{2}'' (sent {3})' -f [IwdInput]::TextOf($width), $wOk, [IwdInput]::TextOf($height), $hOk)
            [void][IwdInput]::Send([IwdInput]::BM_CLICK, $custom, [IntPtr]::Zero, [IntPtr]::Zero)
            Log ('  clicked ''Custom:'' 0x{0:x}' -f $custom.ToInt64())
            [void][IwdInput]::Send([IwdInput]::BM_CLICK, $button, [IntPtr]::Zero, [IntPtr]::Zero)
            Log ('  clicked ''SELECT'' 0x{0:x} (pid {1}, {2})' -f $button.ToInt64(), $ownerPid, $owner)
            # A pick below IEex's 2x UI minimum brings up a warning box:
            # answer its confirm button, once per popup handle.
            $answered = @{}
            $warnEnd = (Get-Date).AddSeconds(15)
            while ((Get-Date) -lt $warnEnd -and [IwdInput]::IsWindow($h)) {
                Check
                foreach ($w in [IwdInput]::AllWindows()) {
                    if ($w -eq $h -or $answered.ContainsKey($w)) { continue }
                    if ([IwdInput]::ClassOf($w) -ne '#32770') { continue }
                    if ([IwdInput]::PidOf($w) -ne $ownerPid) { continue }
                    $answered[$w] = $true
                    Log ('  popup 0x{0:x} ''{1}'' (pid {2})' -f $w.ToInt64(), [IwdInput]::TextOf($w), [IwdInput]::PidOf($w))
                    foreach ($c in [IwdInput]::ChildWindows($w)) {
                        Log ("    popup child 0x{0:x} class '{1}' id {2} text '{3}'" -f $c.ToInt64(), [IwdInput]::ClassOf($c), [IwdInput]::CtrlId($c), [IwdInput]::TextOf($c))
                    }
                    $confirm = [IntPtr]::Zero
                    foreach ($c in [IwdInput]::ChildWindows($w)) {
                        if ([IwdInput]::ClassOf($c) -ne 'Button') { continue }
                        $id = [IwdInput]::CtrlId($c)
                        if ($id -eq 1 -or $id -eq 6) { $confirm = $c; break }
                        if ($confirm -eq [IntPtr]::Zero) { $confirm = $c }
                    }
                    if ($confirm -ne [IntPtr]::Zero) {
                        [void][IwdInput]::Send([IwdInput]::BM_CLICK, $confirm, [IntPtr]::Zero, [IntPtr]::Zero)
                        Log ('    BM_CLICK on 0x{0:x} (id {1} ''{2}'')' -f $confirm.ToInt64(), [IwdInput]::CtrlId($confirm), [IwdInput]::TextOf($confirm))
                    } else {
                        Log '    no button child; leaving the popup alone'
                    }
                }
                Start-Sleep -Milliseconds 250
            }
            if ([IwdInput]::IsWindow($h)) {
                # Still up after the warning window: dump again so the log
                # shows what the one-shot run was looking at.
                Log '  resolution dialog still up after SELECT; children now:'
                foreach ($c in [IwdInput]::ChildWindows($h)) {
                    Log ("    dialog child 0x{0:x} class '{1}' id {2} text '{3}'" -f $c.ToInt64(), [IwdInput]::ClassOf($c), [IwdInput]::CtrlId($c), [IwdInput]::TextOf($c))
                }
            }
            Log ('resolution dialog 0x{0:x} (pid {1}, {2}): custom 800x600 written and confirmed' -f $h.ToInt64(), $ownerPid, $owner)
            return
        }
        Start-Sleep -Milliseconds 250
    }
    Log 'no resolution dialog within 20 s (the choice may be persisted); proceeding'
}

# The game window: the largest visible top-level window of the loader's
# descendants, stable over five 200 ms polls, within the scenario timeout
# (the same deadline the Check calls race; a stuck launch cannot outlive
# the run's own budget). Dialog-class windows are skipped: the resolution
# box and IWD2EE.exe's "IEex loader" box are ones, and the game is not.
# On expiry the game process is resolved by exact pid (Find-GameProcess)
# before the throw, so the finally's kill reaches it.
function Find-GameWindow {
    $end = $deadline
    $last = [IntPtr]::Zero
    $stable = 0
    $lastIdle = (Get-Date)
    while ((Get-Date) -lt $end) {
        Check
        $best = [IntPtr]::Zero
        $bestArea = 0
        foreach ($procId in (Get-DescendantIds $script:loader.Id)) {
            foreach ($h in [IwdInput]::Windows([int]$procId, $null)) {
                if ([IwdInput]::ClassOf($h) -eq '#32770') { continue }
                $a = [IwdInput]::Area($h)
                if ($a -gt $bestArea) { $bestArea = $a; $best = $h }
            }
        }
        # Run 200445 waited its full 650 s in silence: log what the wait
        # sees every 15 s so a stuck launch leaves evidence behind.
        if (((Get-Date) - $lastIdle).TotalSeconds -ge 15) {
            $lastIdle = (Get-Date)
            if ($best -eq [IntPtr]::Zero) {
                Log '  still no game window (dialog-class windows are skipped)'
            } else {
                Log ('  game window not stable yet: best 0x{0:x} {1}' -f $best.ToInt64(), [IwdInput]::Describe($best))
            }
        }
        if ($best -ne [IntPtr]::Zero -and $best -eq $last) {
            $stable++
            if ($stable -ge 5) {
                $script:window = $best
                [IwdInput]::GameWindow = $best
                $script:gamePid = [IwdInput]::PidOf($best)
                $script:game = Get-Process -Id $script:gamePid -ErrorAction Stop
                $null = $script:game.Handle # keeps ExitCode readable after exit
                Log ('game window 0x{0:x}, {1} px area, pid {2}' -f $best.ToInt64(), $bestArea, $script:gamePid)
                return
            }
        } else {
            $stable = 0
        }
        $last = $best
        Start-Sleep -Milliseconds 200
    }
    Find-GameProcess
    throw ('no game window within the {0} s scenario timeout' -f $cfg.timeout)
}

# Follows the game window if the process recreated it, and brings it to the
# front (SendInput goes to the foreground window).
function Refresh-Window {
    if ($script:game -eq $null -or $script:game.HasExited) { return }
    $best = [IntPtr]::Zero
    $bestArea = 0
    foreach ($h in [IwdInput]::Windows([int]$script:gamePid, $null)) {
        if ([IwdInput]::ClassOf($h) -eq '#32770') { continue }
        $a = [IwdInput]::Area($h)
        if ($a -gt $bestArea) { $bestArea = $a; $best = $h }
    }
    if ($best -ne [IntPtr]::Zero -and $best -ne $script:window) {
        $script:window = $best
        [IwdInput]::GameWindow = $best
        Log ('  game window is now 0x{0:x}' -f $best.ToInt64())
    }
    if ($script:window -ne [IntPtr]::Zero -and -not [IwdInput]::SetForegroundWindow($script:window)) {
        Log ('  SetForegroundWindow refused; the foreground is ' + [IwdInput]::Describe([IwdInput]::GetForegroundWindow()))
    }
}

# A click or key phase is N frames of wall-clock time.
function Press-Key($action) {
    $vk = [uint16]$action.vk
    $extended = [bool]$action.extended
    if ($action.shift) { [IwdInput]::Key(0x10, $true, $false); Sleep-Frames 1 }
    [IwdInput]::Key($vk, $true, $extended)
    Sleep-Frames 2
    [IwdInput]::Key($vk, $false, $extended)
    if ($action.shift) { Sleep-Frames 1; [IwdInput]::Key(0x10, $false, $false) }
    Sleep-Frames 2
}

function Screen-Shot([string]$name) {
    $path = Join-Path $cfg.results "$name-screen.png"
    $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bitmap.Size)
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Log "  wrote $name-screen.png"
}

function Describe($a) {
    switch ($a.op) {
        'wait' { "wait $($a.frames)" }
        'key' { if ($a.shift) { "key SHIFT+$($a.key)" } else { "key $($a.key)" } }
        'shot' { "shot $($a.name)" }
        'checkpoint' { "checkpoint $($a.name)" }
        'quit' { 'quit' }
        default { "$($a.op) $($a.x) $($a.y)" }
    }
}

# Returns $true when the run should stop (the quit op).
function Do-Action($a) {
    switch ($a.op) {
        'wait' { Sleep-Frames ([int]$a.frames) }
        { $_ -in 'move', 'click', 'rclick' } {
            Refresh-Window
            $p = [IwdInput]::ToScreen($script:window, [int]$a.x, [int]$a.y)
            [void][IwdInput]::SetCursorPos($p.X, $p.Y)
            Sleep-Frames 1
            if ($a.op -ne 'move') {
                $right = $a.op -eq 'rclick'
                [IwdInput]::Button($right, $true)
                Sleep-Frames 2
                [IwdInput]::Button($right, $false)
                Sleep-Frames 2
            }
        }
        'key' { Refresh-Window; Press-Key $a }
        'shot' { Screen-Shot $a.name }
        'checkpoint' { Log "  checkpoint $($a.name)" }
        'quit' { return $true }
        default { throw "unknown action $($a.op)" }
    }
    return $false
}

# One trace: start the IEex chain with TRACE_FILE set (the opengl32.dll
# wrapper in the game folder records the GL stream), answer the resolution
# dialog, find the game window, run the actions, close the game. Throws
# when the game never came up.
function Run-Trace {
    if (-not [System.IO.File]::Exists((Join-Path $cfg.data 'opengl32.dll'))) {
        throw 'no opengl32.dll wrapper in the game folder (gltrace.py places it before the run)'
    }
    Log "starting $($cfg.exe) in $($cfg.data)"
    Log ('  TRACE_FILE={0} (the wrapper opengl32.dll records the GL stream there)' -f $cfg.trace)
    $env:TRACE_FILE = $cfg.trace
    $script:loader = Start-Process -FilePath $cfg.exe -WorkingDirectory $cfg.data -PassThru
    $null = $script:loader.Handle # keeps ExitCode readable after exit
    Log "IWD2EE.exe pid $($script:loader.Id) (its descendants carry the game)"
    Answer-ResolutionDialog
    Find-GameWindow
    Refresh-Window
    Screen-Shot 'ready'
    try {
        $script:watching = $true
        foreach ($a in $actions) {
            $script:line = $a.line
            Log "line $($a.line): $(Describe $a)"
            if (Do-Action $a) { break }
        }
        Log 'scenario finished'
    } catch [System.OperationCanceledException] {
        Log 'game exited'
    } finally {
        Watch-Foreground
        $script:watching = $false
    }
    End-Run
}

# Closes the game (the wrapper finishes the trace when it exits) and
# verifies the trace is there and non-empty.
function End-Run {
    Screen-Shot 'end'
    if ($script:game -ne $null -and -not $script:game.HasExited) {
        Refresh-Window
        if ($script:window -ne [IntPtr]::Zero) {
            [void][IwdInput]::PostMessage($script:window, [IwdInput]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            Log ('  posted WM_CLOSE to 0x{0:x}' -f $script:window.ToInt64())
        }
        $end = (Get-Date).AddSeconds(15)
        while ((Get-Date) -lt $end -and -not $script:game.HasExited) {
            $dialogs = @([IwdInput]::Windows([int]$script:gamePid, '#32770'))
            if ($dialogs.Count -gt 0) {
                Log ('  confirmation window 0x{0:x}: Return' -f $dialogs[0].ToInt64())
                [void][IwdInput]::SetForegroundWindow($dialogs[0])
                Start-Sleep -Milliseconds 200
                [IwdInput]::Key(0x0D, $true, $false)
                Start-Sleep -Milliseconds 50
                [IwdInput]::Key(0x0D, $false, $false)
            }
            Start-Sleep -Milliseconds 200
        }
        if (-not $script:game.HasExited) {
            Log '  the game is still running after WM_CLOSE; killing it'
            Stop-Process -Id $script:gamePid -Force -ErrorAction SilentlyContinue
            $script:game.WaitForExit(10000) | Out-Null
        }
    }
    if (-not [System.IO.File]::Exists($cfg.trace)) { throw "no trace at $($cfg.trace)" }
    $size = (Get-Item -LiteralPath $cfg.trace).Length
    if ($size -le 0) { throw "the trace at $($cfg.trace) is empty" }
    Log "trace: $size bytes"
}

$errorText = $null
$exitCode = $null
try {
    Log "scenario $($cfg.scenario): $($actions.Count) actions, timeout $($cfg.timeout) s"
    $gpu = (Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1).Name
    Log "video adapter: $gpu"
    Log "desktop user $([Environment]::UserName) (the run was allowed: IWD2R_DESKTOP_OK=1)"
    [IwdInput]::WatchForeground()
    Run-Trace
    if ($script:focusLosses.Count -gt 0) {
        Log ("the game lost the foreground {0} time(s) while the scenario ran, so input may have missed it; first {1}" -f $script:focusLosses.Count, $script:focusLosses[0])
    }
} catch {
    $errorText = "$_"
    Log "driver error: $errorText"
    Log "  exception $($_.Exception.GetType().FullName); inner $($_.Exception.InnerException)"
    Log "  at $($_.InvocationInfo.ScriptLineNumber): $($_.InvocationInfo.Line.Trim())"
    Log "  stack $($_.ScriptStackTrace -replace '\r?\n', ' <- ')"
    try { Screen-Shot 'stuck' } catch { Log "  no screen shot: $_" }
} finally {
    # Every run ends by killing what it started, by exact pid: resolve the
    # game from the loader's descendants when no window was ever found
    # (before the loader kill, while the chain is still intact).
    Find-GameProcess
    if ($script:loader -ne $null -and -not $script:loader.HasExited) {
        Stop-Process -Id $script:loader.Id -Force -ErrorAction SilentlyContinue
    }
    if ($script:game -ne $null -and -not $script:game.HasExited) {
        Stop-Process -Id $script:gamePid -Force -ErrorAction SilentlyContinue
    }
    if ($script:game -ne $null) {
        try { $exitCode = $script:game.ExitCode } catch { $exitCode = $null }
    }
    Log "exit code $exitCode"
    Set-Content -LiteralPath (Join-Path $cfg.results 'exitcode.txt') -Value "$exitCode" -Encoding ASCII
    if ($errorText) {
        Set-Content -LiteralPath (Join-Path $cfg.results 'error.txt') -Value $errorText -Encoding UTF8
    }
    Set-Content -LiteralPath (Join-Path $cfg.results 'done.txt') -Value 'done' -Encoding ASCII
}
if ($errorText) { exit 1 }
'''


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
                 '`apitrace %s dump`.' % (meta['stamp'][:8],
                                          meta['commit'], meta['version']))
    lines.append('')
    lines.append('- game: %s (IWD2EE.exe sha256 %s)'
                 % (meta['game'], meta['exe_sha']))
    lines.append('- apitrace: %s (captured by its lib/wrappers/opengl32.dll '
                 'proxy in the game folder, writing $TRACE_FILE)' % meta['version'])
    lines.append('- GPU: %s' % meta['gpu'])
    lines.append('- scenario: scenarios/%s.txt, driven to its last '
                 'checkpoint (waits are wall-clock; %s s lead for the intro '
                 'movies)' % (name, meta['lead']))
    lines.append('- frames: %d' % len(frames))
    lines.append('- draw calls: %d total (mean %.1f/frame, max %d)'
                 % (sum(draws), mean, max(draws) if draws else 0))
    lines.append('- texture uploads (glTexImage2D + glTexSubImage2D): '
                 '%d total' % len(uploads))
    for (size, fmt), count in groups:
        lines.append('  - %s %s: %d'
                     % (size, fmt or '(subimage: internal format n/a)',
                        count))
    lines.append('- FBO binds (glBindFramebuffer + -EXT): %d' % fbo)
    lines.append('- distinct texture dimensions: %s'
                 % (', '.join(summary['texture_dimensions']) or 'none'))
    lines.append('')
    lines.append('A frame is the calls from after the previous buffer swap '
                 'through the swap; calls after the last swap are no frame. '
                 'Draw calls are vertex-array draws and immediate-mode '
                 'glBegin blocks; FBO binds are glBindFramebuffer(-EXT); '
                 'glTexSubImage2D carries no internal format, so its '
                 'uploads have none.')
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
    keep = ('driver.log', 'exitcode.txt', 'error.txt', 'done.txt',
            'log.txt', 'stderr.txt')
    for entry in sorted(os.listdir(results)):
        source = os.path.join(results, entry)
        if entry in keep or (entry.endswith('-screen.png')):
            shutil.copy2(source, os.path.join(run_dir, entry))
    trace = os.path.join(results, name + '.trace')
    if os.path.isfile(trace):
        shutil.copy2(trace, os.path.join(run_dir, name + '.trace'))
        return os.path.getsize(os.path.join(run_dir, name + '.trace'))
    return 0


def stream(cmd, log_file):
    """Runs cmd, echoing its output into log_file (play.py's pattern)."""
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    try:
        for raw in proc.stdout:
            line = raw.decode('utf-8', 'replace')
            sys.stdout.write(line)
            sys.stdout.flush()
            log_file.write(line)
            log_file.flush()
        return proc.wait()
    except BaseException:
        proc.terminate()
        proc.wait()
        raise


def desktop_run(name, game, actions, timeout, run_dir):
    """One desktop launch: config.json, actions.json and the driver go
    into a staging folder next to the game copy (on the Windows side),
    and cmd.exe runs the driver the way play.py launches the Windows
    build. One launch, no retries: a failure is reported, not retried.
    The opengl32.dll wrapper must already sit in the game folder
    (place_wrapper); the driver starts IWD2EE.exe with TRACE_FILE pointing
    into the staging folder. Returns (exitcode, error, trace_bytes)."""
    results = os.path.join(winhome.WIN_HOME, 'iwd2r-ieex', 'runs',
                           os.path.basename(run_dir), name)
    os.makedirs(results)
    results_win = winsandbox.wslpath_w(results)
    config = desktop_config(name, winsandbox.wslpath_w(game), results_win,
                            timeout)
    with open(os.path.join(results, 'config.json'), 'w') as f:
        json.dump(config, f, indent=1)
    with open(os.path.join(results, 'actions.json'), 'w') as f:
        json.dump(actions, f, indent=1)
    with open(os.path.join(results, DRIVER_PS1), 'w', newline='\r\n') as f:
        f.write(DESKTOP_DRIVER)
    bat = os.path.join(results, 'run.bat')
    with open(bat, 'w', newline='\r\n') as f:
        f.write(desktop_bat(results_win))
    exitcode, error = None, None
    with open(os.path.join(run_dir, 'driver-output.log'), 'w') as log_file:
        code = stream([CMD_EXE, '/c', winsandbox.wslpath_w(bat)], log_file)
    # The driver's own error.txt is the real diagnosis; the generic
    # 'no driver exit code' line is only the fallback when the driver
    # died before writing anything (empty exitcode.txt drops the real
    # error otherwise).
    error = winsandbox.read_text(os.path.join(results, 'error.txt'))
    text = winsandbox.read_text(os.path.join(results, 'exitcode.txt'))
    if text.lstrip('-').isdigit():
        exitcode = int(text)
    elif not error:
        error = ('no driver exit code (cmd exit %s); see %s'
                 % (code, os.path.join(run_dir, 'driver-output.log')))
    trace_bytes = copy_home(results, run_dir, name)
    log('trace: %s (%d bytes)' % (os.path.join(run_dir, name + '.trace'),
                                  trace_bytes))
    return exitcode, error, trace_bytes


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
    if not desktop_allowed(os.environ):
        raise SystemExit('gltrace: this run opens the game on the desktop; '
                         'set %s=1 to allow it (the user must be at the PC)'
                         % DESKTOP_OK)
    refusal = attempt.gate(RUNS, 'gltrace', args.new_attempt)
    if refusal:
        raise SystemExit('gltrace: ' + refusal)
    now = datetime.datetime.now()
    run_dir = make_run_dir(now)
    wrapper = find_wrapper(args.apitrace)
    for label, path in (('the apitrace wrapper', wrapper),
                        ('iwd2.exe', os.path.join(game, 'iwd2.exe'))):
        machine = pe_machine(path)
        if machine != 0x014c:
            raise SystemExit(
                'gltrace: %s must be 32-bit x86 (found %s); the wrapper and '
                'the game must match bitness'
                % (label, 'not a PE image' if machine is None
                   else hex(machine)))
    log('apitrace wrapper: %s (x86; the game loads OpenGL32.dll by bare '
        'name, the application directory wins the DLL search)' % wrapper)
    ini = os.path.join(game, 'icewind2.ini')
    if not os.path.isfile(ini):
        raise SystemExit('gltrace: no icewind2.ini in %s (the run rewrites '
                         'its aliases, so there must be one to rewrite)'
                         % game)
    ini_before = os.path.join(run_dir, 'icewind2.ini.before')
    shutil.copy2(ini, ini_before)
    with open(ini, 'rb') as f:
        overridden = ini_overrides(
            f.read().decode('latin-1').replace('\r\n', '\n'), game)
    with open(ini, 'w', encoding='latin-1', newline='\r\n') as f:
        f.write(overridden)
    log('icewind2.ini: aliases pointed at %s, volumes 0 (the original is '
        'backed up and restored)' % game)
    manifest_before = snapshot_manifest(game)
    place_wrapper(wrapper, game)
    log('wrapper placed in %s for the run' % game)
    started = time.time()
    try:
        exitcode, error, trace_bytes = desktop_run(
            name, game, actions, timeout, run_dir)
    finally:
        if remove_wrapper(game):
            log('wrapper removed from %s' % game)
        strays = remove_stray_traces(game)
        if strays:
            log('stray traces deleted from the game folder: %s'
                % ', '.join(strays))
        shutil.copy2(ini, os.path.join(run_dir, 'icewind2.ini.after'))
        shutil.copy2(ini_before, ini)
        log('restored icewind2.ini')
        manifest_after = snapshot_manifest(game)
        delta = manifest_delta(manifest_before, manifest_after)
        for entry, obj in (('manifest-before.json', manifest_before),
                           ('manifest-after.json', manifest_after),
                           ('manifest-delta.json', delta)):
            with open(os.path.join(run_dir, entry), 'w') as f:
                json.dump(obj, f, indent=1, sort_keys=True)
        counts = (len(delta['added']), len(delta['removed']),
                  len(delta['changed']))
        if any(counts):
            log('game copy delta: +%d added, -%d removed, %d changed '
                '(manifest-*.json in the run folder)' % counts)
            for key in delta['added'] + delta['removed'] + delta['changed']:
                log('  %s' % key)
        else:
            log('game copy delta: none (all %d files byte-identical)'
                % len(manifest_after))
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
        'gpu': log_gpu(run_dir),
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
        log('warning: game exit code %s (see runs/%s/driver.log)'
            % (exitcode, os.path.basename(run_dir)))
    return 0


def log_totals(summary):
    """The one-line shape of a summarized trace."""
    draws = [f['draws'] for f in summary['frames']]
    log('frames %d, draws %d (max %d), dimensions %s'
        % (len(draws), sum(draws), max(draws) if draws else 0,
           ', '.join(summary['texture_dimensions']) or 'none'))


def log_gpu(run_dir):
    """The 'video adapter:' line the driver logs, for the summary's meta."""
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
        description='Trace a scenario GL stream with apitrace on the '
                    'desktop (one launch per scenario)')
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
    parser.add_argument('--new-attempt', metavar='REASON',
                        help='start a new attempt: reset the launch count '
                             '(%d launches per attempt) and record why'
                             % attempt.LIMIT)
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv)
    return run(args)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
