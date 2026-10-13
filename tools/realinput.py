"""Scenario files driven with genuine operating-system input.

The scenario format is the one platform/script_core.cpp reads (see
platform/script.h); parse_scenario() is its Python port and turns a file into
an action list. Two executors consume that list:

- the X driver here (XDriver), used by `play.py --vm linux`: the game runs on
  its own Xvfb display and every command becomes real X input through
  xdotool (XTEST), so events travel X server -> SDL -> the window shim ->
  engine exactly as a human's would;
- tools/sandbox/driver.ps1 inside Windows Sandbox (`play.py --vm windows`),
  which reads windows_actions() as JSON and uses SendInput.

The in-engine script (IWD2R_SCRIPT) stays off in these runs. Engine
framebuffer screenshots are requested through the IWD2R_CONTROL_DIR channel
(platform::script::PollControl), which injects no input.

Standard library only.
"""

import ctypes
import ctypes.util
import os
import re
import shutil
import signal
import subprocess
import threading
import time

import dataview

# The engine updates (and the in-engine script advances) 30 times a second;
# `wait N` is N of those frames.
FRAME_RATE = 30

# Frames a click or key is held and settled, as platform/script_core.cpp does.
MOVE_SETTLE_FRAMES = 1
HOLD_FRAMES = 2
SETTLE_FRAMES = 2

# Seconds to wait for the engine to answer a framebuffer shot request.
SHOT_TIMEOUT = 60.0
# Seconds to wait for the game window to appear (loading the install from a
# Windows drive takes about half a minute under WSL), and the 0.2 s polls it
# must stay the same window before the scenario starts.
WINDOW_TIMEOUT = 120.0
WINDOW_STABLE_POLLS = 5
# Seconds quit waits for the close confirmation before asking again.
QUIT_RETRY = 10.0
# Default per-scenario timeout in seconds.
DEFAULT_TIMEOUT = 180

# The virtual-key names platform/script_core.cpp accepts (kNamedKeys), with
# their Win32 VK codes. Letters and digits are handled in vk_for_name.
KEY_VK = {
    'BACK': 0x08, 'TAB': 0x09, 'RETURN': 0x0D, 'SHIFT': 0x10,
    'CONTROL': 0x11, 'MENU': 0x12, 'ESCAPE': 0x1B, 'SPACE': 0x20,
    'PRIOR': 0x21, 'NEXT': 0x22, 'END': 0x23, 'HOME': 0x24, 'LEFT': 0x25,
    'UP': 0x26, 'RIGHT': 0x27, 'DOWN': 0x28, 'SNAPSHOT': 0x2C,
    'INSERT': 0x2D, 'DELETE': 0x2E,
    'NUMPAD0': 0x60, 'NUMPAD1': 0x61, 'NUMPAD2': 0x62, 'NUMPAD3': 0x63,
    'NUMPAD4': 0x64, 'NUMPAD5': 0x65, 'NUMPAD6': 0x66, 'NUMPAD7': 0x67,
    'NUMPAD8': 0x68, 'NUMPAD9': 0x69, 'MULTIPLY': 0x6A, 'ADD': 0x6B,
    'SEPARATOR': 0x6C, 'SUBTRACT': 0x6D, 'DECIMAL': 0x6E, 'DIVIDE': 0x6F,
    'F1': 0x70, 'F2': 0x71, 'F3': 0x72, 'F4': 0x73, 'F5': 0x74, 'F6': 0x75,
    'F7': 0x76, 'F8': 0x77, 'F9': 0x78, 'F10': 0x79, 'F11': 0x7A,
    'F12': 0x7B, 'NUMLOCK': 0x90, 'LSHIFT': 0xA0, 'RSHIFT': 0xA1,
    'LCONTROL': 0xA2, 'RCONTROL': 0xA3, 'LMENU': 0xA4, 'RMENU': 0xA5,
    'OEM_1': 0xBA, 'OEM_PLUS': 0xBB, 'OEM_COMMA': 0xBC, 'OEM_MINUS': 0xBD,
    'OEM_PERIOD': 0xBE, 'OEM_2': 0xBF, 'OEM_3': 0xC0, 'OEM_4': 0xDB,
    'OEM_5': 0xDC, 'OEM_6': 0xDD, 'OEM_7': 0xDE, 'OEM_8': 0xDF,
}

# The X keysym for each name, matching the key the window shim maps the VK
# to (platform/window_keys.cpp) on a US layout.
X_KEYSYM = {
    'BACK': 'BackSpace', 'TAB': 'Tab', 'RETURN': 'Return',
    'SHIFT': 'Shift_L', 'CONTROL': 'Control_L', 'MENU': 'Alt_L',
    'ESCAPE': 'Escape', 'SPACE': 'space', 'PRIOR': 'Prior', 'NEXT': 'Next',
    'END': 'End', 'HOME': 'Home', 'LEFT': 'Left', 'UP': 'Up',
    'RIGHT': 'Right', 'DOWN': 'Down', 'SNAPSHOT': 'Print',
    'INSERT': 'Insert', 'DELETE': 'Delete',
    'NUMPAD0': 'KP_0', 'NUMPAD1': 'KP_1', 'NUMPAD2': 'KP_2',
    'NUMPAD3': 'KP_3', 'NUMPAD4': 'KP_4', 'NUMPAD5': 'KP_5',
    'NUMPAD6': 'KP_6', 'NUMPAD7': 'KP_7', 'NUMPAD8': 'KP_8',
    'NUMPAD9': 'KP_9', 'MULTIPLY': 'KP_Multiply', 'ADD': 'KP_Add',
    'SEPARATOR': 'KP_Separator', 'SUBTRACT': 'KP_Subtract',
    'DECIMAL': 'KP_Decimal', 'DIVIDE': 'KP_Divide',
    'F1': 'F1', 'F2': 'F2', 'F3': 'F3', 'F4': 'F4', 'F5': 'F5', 'F6': 'F6',
    'F7': 'F7', 'F8': 'F8', 'F9': 'F9', 'F10': 'F10', 'F11': 'F11',
    'F12': 'F12', 'NUMLOCK': 'Num_Lock', 'LSHIFT': 'Shift_L',
    'RSHIFT': 'Shift_R', 'LCONTROL': 'Control_L', 'RCONTROL': 'Control_R',
    'LMENU': 'Alt_L', 'RMENU': 'Alt_R', 'OEM_1': 'semicolon',
    'OEM_PLUS': 'equal', 'OEM_COMMA': 'comma', 'OEM_MINUS': 'minus',
    'OEM_PERIOD': 'period', 'OEM_2': 'slash', 'OEM_3': 'grave',
    'OEM_4': 'bracketleft', 'OEM_5': 'backslash', 'OEM_6': 'bracketright',
    'OEM_7': 'apostrophe',
    # The shim maps VK_OEM_8 to SDL_SCANCODE_NONUSHASH, the ISO key left of
    # Return; X has no US keysym for it, so the closest is the one evdev
    # puts there on a UK layout.
    'OEM_8': 'numbersign',
}

# Keys Win32 sends with KEYEVENTF_EXTENDEDKEY (the navigation block, the
# arrows, right Ctrl/Alt, keypad divide and Num Lock).
EXTENDED_KEYS = frozenset((
    'PRIOR', 'NEXT', 'END', 'HOME', 'LEFT', 'UP', 'RIGHT', 'DOWN',
    'INSERT', 'DELETE', 'DIVIDE', 'NUMLOCK', 'RCONTROL', 'RMENU',
    'SNAPSHOT'))


class ScenarioError(ValueError):
    """A malformed scenario line; str() is "line N: <reason>"."""


def vk_for_name(name):
    """The VK code for a key name (case-insensitive), or 0 when unknown."""
    upper = name.upper()
    if len(upper) == 1:
        if 'A' <= upper <= 'Z' or '0' <= upper <= '9':
            return ord(upper)
        return 0
    return KEY_VK.get(upper, 0)


def keysym_for_name(name):
    """The X keysym for a key name, or None when unknown."""
    upper = name.upper()
    if len(upper) == 1:
        if 'A' <= upper <= 'Z':
            return upper.lower()
        if '0' <= upper <= '9':
            return upper
        return None
    return X_KEYSYM.get(upper)


def _int(token):
    if not token or not token.isascii() or not token.isdigit():
        return None
    value = int(token)
    return value if value <= 2 ** 31 - 1 else None


def parse_scenario(text):
    """Parses scenario text into a list of actions (dicts with 'op' and
    'line'), accepting exactly what platform::script::Parse accepts.

    Actions: wait {frames}, move/click/rclick {x, y, ui}, key {key}, type
    {text}, shot {name}, checkpoint {name}, quit, origin {ui}. A checkpoint
    is where the replay recorder (oracle/replay) dumps state; the drivers
    here pass over it. ui is True when X Y are in the
    original's 800x600 UI coordinates (the default, or after `origin ui`),
    False for screen pixels (after `origin screen`). Raises ScenarioError on
    the first bad line.
    """
    actions = []
    ui = True
    checkpoints = {}  # upper-cased name -> the line it first appeared on
    for number, raw in enumerate(text.split('\n'), 1):
        if raw.endswith('\r'):
            raw = raw[:-1]
        hash_at = raw.find('#')
        if hash_at >= 0:
            raw = raw[:hash_at]
        words = raw.split()
        if not words:
            continue
        verb, args = words[0], words[1:]
        v = verb.upper()

        def fail(why):
            raise ScenarioError('line %d: %s' % (number, why))

        if v == 'WAIT':
            frames = _int(args[0]) if len(args) == 1 else None
            if frames is None:
                fail('wait needs one frame count')
            actions.append({'op': 'wait', 'frames': frames, 'line': number})
        elif v in ('CLICK', 'RCLICK', 'MOVE'):
            xy = [_int(a) for a in args] if len(args) == 2 else [None]
            if None in xy:
                fail('%s needs X Y (non-negative integers)' % verb.lower())
            actions.append({'op': v.lower(), 'x': xy[0], 'y': xy[1],
                            'ui': ui, 'line': number})
        elif v == 'KEY':
            if len(args) != 1:
                fail('key needs one key name')
            if vk_for_name(args[0]) == 0:
                fail("unknown key '%s'" % args[0])
            actions.append({'op': 'key', 'key': args[0].upper(),
                            'line': number})
        elif v == 'TYPE':
            body = raw.lstrip(' \t')[len(verb):].lstrip(' \t').rstrip()
            if not body:
                fail('type needs text')
            for ch in body:
                if not (ch.isascii() and ch.isalnum()) and ch != ' ':
                    fail("type supports letters, digits and spaces, not '%s'"
                         % ch)
            actions.append({'op': 'type', 'text': body, 'line': number})
        elif v == 'SHOT':
            if len(args) != 1:
                fail('shot needs one name')
            if any(c in args[0] for c in '/\\:'):
                fail('shot name must not contain a path')
            actions.append({'op': 'shot', 'name': args[0], 'line': number})
        elif v == 'CHECKPOINT':
            if len(args) != 1:
                fail('checkpoint needs one name')
            if not re.fullmatch(r'[A-Za-z0-9_-]+', args[0]):
                fail('checkpoint name must be letters, digits, _ or -')
            first = checkpoints.setdefault(args[0].upper(), number)
            if first != number:
                fail("duplicate checkpoint '%s' (first on line %d)"
                     % (args[0], first))
            actions.append({'op': 'checkpoint', 'name': args[0],
                            'line': number})
        elif v == 'ORIGIN':
            mode = args[0].upper() if len(args) == 1 else ''
            if mode not in ('UI', 'SCREEN'):
                fail('origin needs ui or screen')
            ui = mode == 'UI'
            actions.append({'op': 'origin', 'ui': ui, 'line': number})
        elif v == 'QUIT':
            if args:
                fail('quit takes no arguments')
            actions.append({'op': 'quit', 'line': number})
        else:
            fail("unknown command '%s'" % verb)
    return actions


def load_scenario(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    try:
        return parse_scenario(text)
    except ScenarioError as e:
        raise ScenarioError('%s: %s' % (path, e)) from None


def windows_actions(actions, geometry=None):
    """The action list for tools/sandbox/driver.ps1: keys carry their VK code
    and extended flag, `type` is expanded into single key presses (with
    shift for capitals), so PowerShell never interprets scenario text, and
    pointer coordinates are window pixels (window_point)."""
    out = []
    for a in actions:
        if a['op'] in ('origin', 'checkpoint'):
            continue
        if a['op'] in ('move', 'click', 'rclick'):
            x, y = window_point(a, geometry)
            out.append({'op': a['op'], 'x': x, 'y': y, 'line': a['line']})
            continue
        if a['op'] == 'key':
            out.append({'op': 'key', 'line': a['line'], 'key': a['key'],
                        'vk': vk_for_name(a['key']),
                        'extended': a['key'] in EXTENDED_KEYS,
                        'shift': False})
        elif a['op'] == 'type':
            for ch in a['text']:
                name = 'SPACE' if ch == ' ' else ch.upper()
                out.append({'op': 'key', 'line': a['line'], 'key': name,
                            'vk': vk_for_name(name), 'extended': False,
                            'shift': ch.isupper()})
        else:
            out.append(dict(a))
    return out


def describe(action):
    """One action as scenario text (for logs)."""
    op = action['op']
    if op == 'wait':
        return 'wait %d' % action['frames']
    if op in ('move', 'click', 'rclick'):
        return '%s %d %d' % (op, action['x'], action['y'])
    if op == 'key':
        return 'key %s' % action['key']
    if op == 'type':
        return 'type %s' % action['text']
    if op == 'shot':
        return 'shot %s' % action['name']
    if op == 'checkpoint':
        return 'checkpoint %s' % action['name']
    if op == 'origin':
        return 'origin %s' % ('ui' if action['ui'] else 'screen')
    return op


# The original resolutions and their 4:3 heights (src/ScreenLayout.cpp).
ORIGINAL_HEIGHTS = {800: 600, 1024: 768, 1600: 1200, 2048: 1536}
MAX_SCALE = 4


def resolve_screen(width, height, scale, desktop):
    """Python port of iwd2r::ResolveScreenSize (src/ScreenLayout.cpp).

    width: an int or 'desktop'; height: an int or None; scale: 1-4 or
    'auto'; desktop: (w, h). Returns a dict with output_width,
    output_height, scale, width, height (the engine's internal screen).
    Raises ValueError below the 800x600 minimum."""
    if width == 'desktop':
        width = desktop[0]
        if not height:
            height = desktop[1]
    elif not height:
        height = ORIGINAL_HEIGHTS.get(width)
        if height is None:
            height = (width * desktop[1] + desktop[0] // 2) // desktop[0]
    if scale == 'auto':
        scale = 1
        for s in range(MAX_SCALE, 1, -1):
            if height // s >= 720 and width // s >= 800 and height // s >= 600:
                scale = s
                break
    scale = int(scale)
    w, h = width // scale, height // scale
    if w < 800 or h < 600:
        raise ValueError('%dx%d at scale %d is %dx%d inside; the minimum is '
                         '800x600' % (width, height, scale, w, h))
    return {'output_width': w * scale, 'output_height': h * scale,
            'scale': scale, 'width': w, 'height': h}


def ui_to_screen(x, y, width, height):
    """A point in the 800x600 UI on an engine screen of width x height:
    platform::script::UiToScreen (centred, doubled at 1600x1200 and
    2048x1536)."""
    k = 2 if (width, height) in ((1600, 1200), (2048, 1536)) else 1
    return ((width - 800 * k) // 2 + x * k, (height - 600 * k) // 2 + y * k)


def window_point(action, geometry):
    """The window (client area) pixel for a move/click action: UI
    coordinates placed on the engine's screen, then times the scale."""
    if geometry is None:
        geometry = {'width': 800, 'height': 600, 'scale': 1}
    x, y = action['x'], action['y']
    if action.get('ui', True):
        x, y = ui_to_screen(x, y, geometry['width'], geometry['height'])
    return x * geometry['scale'], y * geometry['scale']


def abstract_x_sockets(proc_net_unix='/proc/net/unix'):
    """The display numbers with an abstract X socket (@/tmp/.X11-unix/XN).
    Under WSLg /tmp/.X11-unix is read-only, so Xvfb listens only there."""
    found = set()
    try:
        with open(proc_net_unix) as f:
            for line in f:
                m = re.search(r'@/tmp/\.X11-unix/X(\d+)\s*$', line)
                if m:
                    found.add(int(m.group(1)))
    except OSError:
        pass
    return found


def pick_display(start=90, x11_dir='/tmp/.X11-unix', lock_dir='/tmp',
                 taken=(), limit=200, abstract=None):
    """The first display number from start whose socket and lock file are
    absent, with no abstract socket (abstract: a set of numbers, read from
    /proc/net/unix when None) and not in taken."""
    if abstract is None:
        abstract = abstract_x_sockets()
    for n in range(start, start + limit):
        if n in taken or n in abstract:
            continue
        if os.path.exists(os.path.join(x11_dir, 'X%d' % n)):
            continue
        if os.path.exists(os.path.join(lock_dir, '.X%d-lock' % n)):
            continue
        return n
    raise RuntimeError('no free X display number in %d..%d'
                       % (start, start + limit - 1))


def make_install_view(install, view):
    """A private view of install for one run (tools/dataview.py)."""
    return dataview.make_view(install, view)


def remove_install_view(view):
    """Deletes a view made by make_install_view without following its
    links into the install."""
    dataview.remove_view(view)


def missing_tools():
    """The Linux real-input tools that are not installed."""
    return [t for t in ('Xvfb', 'xdotool', 'import') if not shutil.which(t)]


# ---------------------------------------------------------------------------
# Processes


def kill_group(proc, grace=3.0):
    """Terminates proc's process group (it was started in a new session),
    then kills it if it lingers."""
    if proc is None or proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        proc.wait(timeout=grace)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.wait()


_display_lock = threading.Lock()


def start_xvfb(width, height, taken, log_file):
    """Starts Xvfb on a free display; returns (number, process). taken is a
    set of numbers this process already uses (updated; shared by parallel
    runs)."""
    for _ in range(5):
        with _display_lock:
            n = pick_display(taken=taken)
            taken.add(n)
        proc = subprocess.Popen(
            ['Xvfb', ':%d' % n, '-screen', '0', '%dx%dx24' % (width, height),
             '-nolisten', 'tcp'],
            stdout=log_file, stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.time() + 10
        while time.time() < deadline:
            if proc.poll() is not None:
                break
            if x_display_ready(':%d' % n):
                return n, proc
            time.sleep(0.1)
        kill_group(proc)
    raise RuntimeError('Xvfb did not start')


def pin_threads(pid, cpu):
    """Moves every thread of pid onto cpu. The engine pins itself to CPU 0
    like the original; parallel runs would all share that core. Returns the
    number of threads moved."""
    moved = 0
    try:
        tids = os.listdir('/proc/%d/task' % pid)
    except OSError:
        return 0
    for tid in tids:
        try:
            os.sched_setaffinity(int(tid), {cpu})
            moved += 1
        except OSError:
            pass
    return moved


def job_cpu(slot, cpus):
    """The core for parallel job slot (0-based) from the usable cpus."""
    cpus = sorted(cpus)
    return cpus[slot % len(cpus)]


# Xlib, for the one thing xdotool 3.2016 cannot do: ask a window to close the
# way a window manager's close button does (WM_DELETE_WINDOW).
class _XClientMessageEvent(ctypes.Structure):
    _fields_ = [('type', ctypes.c_int), ('serial', ctypes.c_ulong),
                ('send_event', ctypes.c_int), ('display', ctypes.c_void_p),
                ('window', ctypes.c_ulong), ('message_type', ctypes.c_ulong),
                ('format', ctypes.c_int), ('data', ctypes.c_long * 5)]


class _XEvent(ctypes.Union):
    _fields_ = [('xclient', _XClientMessageEvent),
                ('pad', ctypes.c_long * 24)]


_X_ERROR_HANDLER_TYPE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p,
                                         ctypes.c_void_p)
_x_state = {'lib': None, 'errors': 0}


def _x_error(display, event):
    # Xlib's default handler exits the process (and play.py with it, before
    # it restores the INI files); count the error instead.
    _x_state['errors'] += 1
    return 0


_x_error_handler = _X_ERROR_HANDLER_TYPE(_x_error)


def _xlib():
    if _x_state['lib'] is None:
        x11 = ctypes.CDLL(ctypes.util.find_library('X11') or 'libX11.so.6')
        x11.XOpenDisplay.restype = ctypes.c_void_p
        x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
        x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
        x11.XSetErrorHandler.argtypes = [_X_ERROR_HANDLER_TYPE]
        x11.XSetErrorHandler(_x_error_handler)
        _x_state['lib'] = x11
    return _x_state['lib']


def x_display_ready(display):
    """True once an X server accepts connections on display."""
    x11 = _xlib()
    dpy = x11.XOpenDisplay(display.encode())
    if not dpy:
        return False
    x11.XCloseDisplay(dpy)
    return True


def send_wm_delete(display, window):
    """Sends WM_DELETE_WINDOW to window on display (":N")."""
    x11 = _xlib()
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                               ctypes.c_long, ctypes.c_void_p]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    dpy = x11.XOpenDisplay(display.encode())
    if not dpy:
        raise RuntimeError('cannot open display %s' % display)
    try:
        ev = _XEvent()
        ev.xclient.type = 33  # ClientMessage
        ev.xclient.window = window
        ev.xclient.message_type = x11.XInternAtom(dpy, b'WM_PROTOCOLS', 0)
        ev.xclient.format = 32
        ev.xclient.data[0] = x11.XInternAtom(dpy, b'WM_DELETE_WINDOW', 0)
        ev.xclient.data[1] = 0  # CurrentTime
        errors = _x_state['errors']
        sent = x11.XSendEvent(dpy, window, 0, 0, ctypes.byref(ev))
        x11.XSync(dpy, 0)
        if not sent or _x_state['errors'] != errors:
            raise DriverError('WM_DELETE_WINDOW to 0x%x failed' % window)
    finally:
        x11.XCloseDisplay(dpy)


class DriverError(RuntimeError):
    pass


class GameExited(Exception):
    pass


class XDriver:
    """Executes an action list against a game on an X display with real
    input. sleep and run are injectable for tests."""

    def __init__(self, display, game, run_dir, control_dir, log, deadline,
                 run=None, sleep=time.sleep, abort=None, baseline=(),
                 cpu=None, geometry=None):
        self.display = ':%d' % display if isinstance(display, int) else display
        self.game = game
        self.run_dir = run_dir
        self.control_dir = control_dir
        self.log = log
        self.deadline = deadline
        self._run = run or self._subprocess_run
        self._sleep = sleep
        self.abort = abort
        self.window = None
        self.baseline = set(baseline)
        self.cpu = cpu
        self.geometry = geometry
        self.screen_shots = 0
        self.engine_shots = 0

    def _subprocess_run(self, cmd, timeout=30):
        env = dict(os.environ, DISPLAY=self.display)
        r = subprocess.run(cmd, env=env, capture_output=True, text=True,
                           timeout=timeout)
        return r.returncode, r.stdout

    def xdo(self, *args, check=True, timeout=30):
        code, out = self._run(['xdotool', *[str(a) for a in args]],
                              timeout=timeout)
        if check and code != 0:
            raise DriverError('xdotool %s failed (%d)'
                              % (' '.join(str(a) for a in args), code))
        return out

    def alive(self):
        return self.game.poll() is None

    def check(self):
        if not self.alive():
            raise GameExited()
        if self.abort is not None and self.abort.is_set():
            raise DriverError('interrupted')
        if time.time() > self.deadline:
            raise DriverError('scenario timeout')

    def sleep_frames(self, frames):
        end = time.time() + frames / FRAME_RATE
        while True:
            self.check()
            left = end - time.time()
            if left <= 0:
                return
            self._sleep(min(left, 0.1))

    def request(self, name, timeout):
        """Creates control/<name> and waits for the engine to delete it,
        which it does on its next presented frame."""
        path = os.path.join(self.control_dir, name)
        with open(path, 'w') as f:
            f.write('\n')
        end = time.time() + timeout
        while os.path.exists(path):
            self.check()
            if time.time() > end:
                raise DriverError('engine did not answer %s in %.0f s'
                                  % (name, timeout))
            self._sleep(0.01)

    def engine_frames(self):
        """The engine's presented frame count."""
        self.request('frames.req', self.deadline - time.time())
        with open(os.path.join(self.control_dir, 'frames.txt')) as f:
            return int(f.read().strip())

    def wait(self, frames):
        """`wait N`: N frames of wall-clock time (N / FRAME_RATE s), and
        until the engine has presented N frames, so a wait spans the same
        engine work as the in-engine script's even while the engine is
        busy loading."""
        started = time.time()
        first = self.engine_frames()
        self.sleep_frames(frames)
        done = self.engine_frames() - first
        while done < frames:
            self.sleep_frames(min(frames - done, FRAME_RATE))
            done = self.engine_frames() - first
        extra = time.time() - started - frames / FRAME_RATE
        if extra > 0.5:
            self.log('  the engine needed %.1f s more for %d frames'
                     % (extra, frames))

    def visible_windows(self):
        # --class: SDL's message box window has a class but no WM_NAME.
        out = self.xdo('search', '--onlyvisible', '--class', '', check=False,
                       timeout=10)
        return [int(w) for w in out.split()]

    def game_windows(self):
        """The game's mapped windows: those carrying its pid, else any not
        there before it started."""
        out = self.xdo('search', '--onlyvisible', '--pid', self.game.pid,
                       check=False, timeout=10)
        ids = [int(w) for w in out.split()]
        if not ids:
            ids = [w for w in self.visible_windows()
                   if w not in self.baseline]
        return ids

    def find_window(self):
        """Waits for the game's mapped window to appear and stay (the engine
        may recreate it while starting); returns its id."""
        end = time.time() + WINDOW_TIMEOUT
        last = None
        stable = 0
        while time.time() < end:
            self.check()
            ids = self.game_windows()
            current = ids[0] if ids else None
            if current is not None and current == last:
                stable += 1
                if stable >= WINDOW_STABLE_POLLS:
                    self.window = current
                    self.log('game window 0x%x' % current)
                    return current
            else:
                stable = 0
            last = current
            self._sleep(0.2)
        raise DriverError('no game window after %.0f s' % WINDOW_TIMEOUT)

    def refresh_window(self):
        """Follows the game window if the engine recreated it."""
        ids = self.game_windows()
        if ids and self.window not in ids:
            self.window = ids[0]
            self.log('  game window is now 0x%x' % self.window)
            self.focus(self.window)

    def focus(self, window):
        # No window manager: XSetInputFocus directly. Failure is harmless
        # (the default focus follows the pointer).
        self.xdo('windowfocus', '--sync', window, check=False, timeout=5)

    def pointer(self, x, y):
        self.refresh_window()
        self.xdo('mousemove', '--window', self.window, x, y)

    def button(self, button):
        self.xdo('mousedown', button)
        self.sleep_frames(HOLD_FRAMES)
        self.xdo('mouseup', button)
        self.sleep_frames(SETTLE_FRAMES)

    def key(self, keysym):
        self.xdo('keydown', keysym)
        self.sleep_frames(HOLD_FRAMES)
        self.xdo('keyup', keysym)
        self.sleep_frames(SETTLE_FRAMES)

    def screen_shot(self, name):
        path = os.path.join(self.run_dir, name + '-screen.png')
        code, _ = self._run(['import', '-display', self.display, '-window',
                             'root', path], timeout=30)
        if code != 0:
            raise DriverError('import failed for %s' % path)
        self.screen_shots += 1
        self.log('  wrote %s' % os.path.basename(path))

    def engine_shot(self, name):
        self.request('shot-%s.req' % name, SHOT_TIMEOUT)
        if not os.path.exists(os.path.join(self.run_dir, name + '.png')):
            raise DriverError('engine wrote no %s.png' % name)
        self.engine_shots += 1
        self.log('  wrote %s.png' % name)

    def quit(self):
        """Asks the window to close as a window manager would; the engine
        then asks for confirmation in a message box, which gets Return (its
        default button, Yes) on the keyboard."""
        # The engine ignores a close request while its progress screen is
        # up, so ask again (as a person would) until the dialog shows.
        while True:
            self.refresh_window()
            send_wm_delete(self.display, self.window)
            self.log('  sent WM_DELETE_WINDOW to 0x%x' % self.window)
            end = time.time() + QUIT_RETRY
            while time.time() < end:
                self.check()
                dialogs = [w for w in self.visible_windows()
                           if w != self.window and w not in self.baseline]
                if dialogs:
                    self.log('  confirmation window 0x%x: Return'
                             % dialogs[0])
                    self.focus(dialogs[0])
                    self.sleep_frames(SETTLE_FRAMES)
                    self.xdo('key', '--clearmodifiers', 'Return')
                    return
                self._sleep(0.1)
            self.log('  no confirmation window after %.0f s' % QUIT_RETRY)

    def do(self, action):
        op = action['op']
        if op == 'wait':
            self.wait(action['frames'])
        elif op in ('move', 'click', 'rclick'):
            self.pointer(*window_point(action, self.geometry))
            self.sleep_frames(MOVE_SETTLE_FRAMES)
            if op != 'move':
                self.button(1 if op == 'click' else 3)
        elif op == 'key':
            self.key(keysym_for_name(action['key']))
        elif op == 'type':
            self.xdo('type', '--delay', int(1000 * (HOLD_FRAMES + SETTLE_FRAMES)
                                            / FRAME_RATE), action['text'])
            self.sleep_frames(SETTLE_FRAMES)
        elif op == 'shot':
            self.screen_shot(action['name'])
            self.engine_shot(action['name'])
        elif op == 'quit':
            self.quit()

    def run(self, actions):
        """Runs the actions. Returns 'finished' or 'game exited'; raises
        DriverError on failure."""
        try:
            self.find_window()
            if self.cpu is not None:
                self.log('moved %d game threads to CPU %d'
                         % (pin_threads(self.game.pid, self.cpu), self.cpu))
            self.focus(self.window)
            for action in actions:
                self.log('line %d: %s' % (action['line'], describe(action)))
                self.do(action)
            return 'finished'
        except GameExited:
            self.log('game exited')
            return 'game exited'
        except DriverError:
            # A command that failed because the game exited under it.
            if not self.alive():
                self.log('game exited')
                return 'game exited'
            raise


def run_scenario(exe, base_env, actions, run_dir, geometry, timeout,
                 taken, cwd=None, abort=None, cpu=None):
    """Runs the game on its own Xvfb display and drives it with actions.

    geometry: resolve_screen's result; Xvfb takes the output size.

    Returns a dict: code (the game's exit code, or non-zero when the driver
    failed or the run timed out), error, screen_shots, engine_shots,
    seconds. Xvfb and the game are killed on every exit path.
    """
    started = time.time()
    control = os.path.join(run_dir, 'control')
    os.makedirs(control, exist_ok=True)
    width, height = geometry['output_width'], geometry['output_height']
    result = {'code': None, 'error': None, 'screen_shots': 0,
              'engine_shots': 0, 'seconds': 0.0, 'display': None}
    xvfb = game = None
    with open(os.path.join(run_dir, 'driver.log'), 'w') as dlog, \
            open(os.path.join(run_dir, 'log.txt'), 'w') as glog, \
            open(os.path.join(run_dir, 'xvfb.log'), 'w') as xlog:

        def log(msg):
            stamp = time.strftime('%H:%M:%S')
            dlog.write('%s.%03d +%7.3f %s\n' % (
                stamp, int(time.time() * 1000) % 1000,
                time.time() - started, msg))
            dlog.flush()

        try:
            n, xvfb = start_xvfb(width, height, taken, xlog)
            result['display'] = n
            log('Xvfb :%d %dx%dx24' % (n, width, height))
            env = dict(base_env)
            env.pop('WAYLAND_DISPLAY', None)
            env.pop('IWD2R_SCRIPT', None)
            env.update({'DISPLAY': ':%d' % n, 'SDL_VIDEO_DRIVER': 'x11',
                        'SDL_AUDIO_DRIVER': 'dummy',
                        'IWD2R_CONTROL_DIR': control,
                        'IWD2R_SHOT_DIR': run_dir})
            # Windows the bare server already shows (Xvfb maps one).
            probe = XDriver(n, None, run_dir, control, log, 0)
            baseline = probe.visible_windows()
            game = subprocess.Popen([exe], env=env, cwd=cwd, stdout=glog,
                                    stderr=subprocess.STDOUT,
                                    start_new_session=True)
            log('game pid %d' % game.pid)
            driver = XDriver(n, game, run_dir, control, log,
                             started + timeout, abort=abort,
                             baseline=baseline, cpu=cpu, geometry=geometry)
            try:
                outcome = driver.run(actions)
                log('scenario %s; waiting for the game to exit' % outcome)
                while game.poll() is None:
                    if abort is not None and abort.is_set():
                        raise DriverError('interrupted')
                    if time.time() > started + timeout:
                        raise DriverError('scenario timeout (%d s)' % timeout)
                    time.sleep(0.1)
                result['code'] = game.returncode
            finally:
                result['screen_shots'] = driver.screen_shots
                result['engine_shots'] = driver.engine_shots
        except (DriverError, RuntimeError, OSError,
                subprocess.SubprocessError) as e:
            result['error'] = str(e) or e.__class__.__name__
            log('driver error: %s' % result['error'])
        except KeyboardInterrupt:
            result['error'] = 'interrupted'
            log('interrupted')
            raise
        finally:
            kill_group(game)
            kill_group(xvfb)
            if result['code'] is None and game is not None \
                    and game.returncode is not None and not result['error']:
                result['code'] = game.returncode
            if result['error']:
                if 'timeout' in result['error']:
                    result['code'] = 124
                elif result['error'] == 'interrupted':
                    result['code'] = 130
                elif not result['code']:
                    result['code'] = 1
            result['seconds'] = time.time() - started
            log('exit code %s' % result['code'])
            for name in ('frames.txt', 'frames.tmp'):
                try:
                    os.unlink(os.path.join(control, name))
                except OSError:
                    pass
            try:
                os.rmdir(control)  # kept when a request was left over
            except OSError:
                pass
    return result
