"""Windows Sandbox runs for `play.py --vm windows`.

Each scenario runs in a fresh, disposable Windows Sandbox: play.py builds
iwd2r.exe with MSVC (deploying the MSVC runtime DLLs next to it), keeps a copy
of the game data (by default the vanilla copy, never the IWD2EE install) in a
host cache, rebuilt when its source changes (refresh_cache), writes a
.wsb file that maps four folders and runs tools/sandbox/driver.ps1 at logon,
starts the sandbox, waits for the driver's done.txt and closes it.

  host                                   sandbox
  build output (read-only)               C:\\iwd2r\\bin
  C:\\Users\\thier\\iwd2r-vm\\game (rw)       C:\\iwd2r\\game
  <run>\\results (rw)                     C:\\iwd2r\\results
  <run>\\tools (read-only)                C:\\iwd2r\\tools

Standard library only.
"""

import contextlib
import glob
import hashlib
import json
import os
import shutil
import struct
import subprocess
import time
import xml.etree.ElementTree as ET

SANDBOX_EXE = '/mnt/c/Windows/System32/WindowsSandbox.exe'
SANDBOX_EXE_WIN = r'C:\Windows\System32\WindowsSandbox.exe'
ENABLE_HINT = ('play: Windows Sandbox is not enabled (no %s); in an admin '
               'PowerShell run "Enable-WindowsOptionalFeature -Online '
               '-FeatureName Containers-DisposableClientVM -All", then reboot'
               % SANDBOX_EXE_WIN)
CMD_EXE = '/mnt/c/Windows/System32/cmd.exe'
TASKKILL_NAMES = ('WindowsSandbox.exe', 'WindowsSandboxClient.exe',
                  'WindowsSandboxRemoteSession.exe')
VM_ROOT = '/mnt/c/Users/thier/iwd2r-vm'
GAME_CACHE = VM_ROOT + '/game'
# One sandbox at a time across worktrees (filelock.hold in play.py).
SANDBOX_LOCK = VM_ROOT + '/sandbox.lock'
REDIST_ROOT = ('/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/'
               'VC/Redist/MSVC')
DRIVER = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                      'sandbox', 'driver.ps1')

# Where the mapped folders appear inside the sandbox.
BOX_BIN = r'C:\iwd2r\bin'
BOX_GAME = r'C:\iwd2r\game'
BOX_RESULTS = r'C:\iwd2r\results'
BOX_TOOLS = r'C:\iwd2r\tools'

MEMORY_MB = 4096
DONE_TIMEOUT = 300
CLOSE_TIMEOUT = 30


def available(exe=SANDBOX_EXE):
    return os.path.isfile(exe)


# ---------------------------------------------------------------------------
# Game cache


def key_stat(directory):
    """(mtime in whole seconds, size) of the directory's CHITIN.KEY (any
    case), or None when it has none."""
    try:
        names = os.listdir(directory)
    except OSError:
        return None
    for name in names:
        if name.lower() == 'chitin.key':
            st = os.stat(os.path.join(directory, name))
            return int(st.st_mtime), st.st_size
    return None


def source_record_path(cache):
    """The file beside the cache that records what it was copied from."""
    return cache.rstrip('/') + '.source.json'


def source_record(source, marker='vanilla.ok'):
    """What a cache copied from source records: the source's absolute path
    and the SHA-256 of its vanilla marker (tools/oracle.py, None when the
    source has none)."""
    try:
        with open(os.path.join(source, marker), 'rb') as f:
            digest = hashlib.sha256(f.read()).hexdigest()
    except OSError:
        digest = None
    return {'source': os.path.abspath(source), 'marker_sha256': digest}


def read_source_record(cache):
    try:
        with open(source_record_path(cache)) as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    return data if isinstance(data, dict) else None


def write_source_record(source, cache):
    path = source_record_path(cache)
    tmp = path + '.tmp'
    with open(tmp, 'w') as f:
        json.dump(source_record(source), f, indent=2)
        f.write('\n')
    os.replace(tmp, path)


def cache_state(source, cache):
    """'current'; 'stale' when the cache was copied from this source but its
    CHITIN.KEY differs in mtime or size (an update: rsync it); 'foreign'
    when it was copied from another source, another build of the vanilla
    copy, or nothing records its source (rebuild it from scratch)."""
    want = key_stat(source)
    if want is None:
        raise FileNotFoundError('%s has no CHITIN.KEY' % source)
    if read_source_record(cache) != source_record(source):
        return 'foreign'
    return 'current' if key_stat(cache) == want else 'stale'


def cache_needs_refresh(source, cache):
    return cache_state(source, cache) != 'current'


def refresh_cache(source, cache, log, lock=None):
    """Copies the source into the cache when needed (rsync keeps mtimes, so
    the next comparison matches); a cache from another source is deleted
    first, so nothing of it is reused. lock, when given, returns a context
    manager held around the copy, and the state is checked again under it.
    Returns True when it copied."""
    if os.path.realpath(source) == os.path.realpath(cache):
        raise ValueError('the game cache %s is its own source' % cache)
    if cache_state(source, cache) == 'current':
        log('game cache %s is current (from %s)' % (cache, source))
        return False
    with (lock() if lock else contextlib.nullcontext()):
        state = cache_state(source, cache)
        if state == 'current':
            log('game cache %s is current (from %s)' % (cache, source))
            return False
        record = source_record_path(cache)
        if os.path.exists(record):
            os.remove(record)
        if state == 'foreign' and os.path.isdir(cache):
            log('game cache %s was not copied from %s: deleting it'
                % (cache, source))
            # Never remove the root or a folder that holds the source itself.
            assert cache and os.path.realpath(cache) != os.sep and not (os.path.realpath(source) + os.sep).startswith(os.path.realpath(cache) + os.sep), cache
            shutil.rmtree(cache)
        os.makedirs(cache, exist_ok=True)
        log('rebuilding the game cache %s from %s (about 1.9 GB)'
            % (cache, source))
        started = time.time()
        r = subprocess.run(['rsync', '-rt', '--delete',
                            source.rstrip('/') + '/', cache.rstrip('/') + '/'])
        if r.returncode != 0:
            raise RuntimeError('rsync to the game cache failed')
        write_source_record(source, cache)
        log('game cache copied in %.0f s' % (time.time() - started))
    return True


def reset_cache_inis(source, cache):
    """Puts the install's *.ini files back into the cache, so every run
    starts from the same options whatever the previous one saved. Reads the
    install only."""
    names = [n for n in os.listdir(source) if n.lower().endswith('.ini')
             and os.path.isfile(os.path.join(source, n))]
    for name in names:
        shutil.copy2(os.path.join(source, name), os.path.join(cache, name))
    return sorted(names)


# ---------------------------------------------------------------------------
# MSVC runtime (app-local deployment)


def pe_imports(path):
    """The DLL names in a PE file's import directory."""
    with open(path, 'rb') as f:
        data = f.read()
    if data[:2] != b'MZ':
        raise ValueError('%s is not a PE file' % path)
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('%s has no PE header' % path)
    sections, opt_size = struct.unpack_from('<H12xH', data, pe + 6)
    opt = pe + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    dirs = opt + (112 if magic == 0x20B else 96)
    import_rva, _ = struct.unpack_from('<II', data, dirs + 8)
    table = opt + opt_size
    spans = []
    for i in range(sections):
        vsize, vaddr, rsize, rptr = struct.unpack_from(
            '<IIII', data, table + 40 * i + 8)
        spans.append((vaddr, max(vsize, rsize), rptr))

    def offset(rva):
        for vaddr, size, rptr in spans:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + rptr
        raise ValueError('RVA 0x%x is outside every section' % rva)

    names = []
    if import_rva == 0:
        return names
    at = offset(import_rva)
    while True:
        desc = struct.unpack_from('<IIIII', data, at)
        if not any(desc):
            break
        name_at = offset(desc[3])
        end = data.index(b'\0', name_at)
        names.append(data[name_at:end].decode('ascii'))
        at += 20
    return names


def find_redist(root=REDIST_ROOT, arch='x64'):
    """The newest Microsoft.VC*.CRT folder for arch (x64 or x86) under the
    VS redist root, or None."""
    found = glob.glob(os.path.join(root, '*', arch, 'Microsoft.VC*.CRT'))

    def version(path):
        parts = os.path.basename(os.path.dirname(os.path.dirname(path)))
        try:
            return tuple(int(p) for p in parts.split('.'))
        except ValueError:
            return ()
    found.sort(key=version)
    return found[-1] if found else None


def runtime_dlls(imports, redist_dir):
    """The redist DLL file names (as they are in redist_dir) that imports
    names, compared case-insensitively."""
    wanted = {n.lower() for n in imports}
    return sorted(n for n in os.listdir(redist_dir) if n.lower() in wanted)


def deploy_runtime(exe, log, redist_root=REDIST_ROOT, arch='x64'):
    """Copies the MSVC runtime DLLs (of arch) exe imports next to it, so the
    sandbox needs no installer. Returns the names copied or already
    current."""
    imports = pe_imports(exe)
    if not any(n.lower().startswith(('vcruntime', 'msvcp', 'concrt'))
               for n in imports):
        log('%s links the MSVC runtime statically' % os.path.basename(exe))
        return []
    redist = find_redist(redist_root, arch)
    if not redist:
        raise RuntimeError('no MSVC redist folder under %s' % redist_root)
    names = runtime_dlls(imports, redist)
    target = os.path.dirname(exe)
    for name in names:
        src = os.path.join(redist, name)
        dst = os.path.join(target, name)
        if not os.path.exists(dst) or os.path.getsize(dst) != \
                os.path.getsize(src) or os.path.getmtime(dst) < \
                os.path.getmtime(src):
            shutil.copy2(src, dst)
    log('app-local MSVC runtime: %s' % ', '.join(names))
    return names


# ---------------------------------------------------------------------------
# The .wsb file and the driver's inputs


def make_wsb(folders, logon_command, memory_mb=MEMORY_MB):
    """The .wsb XML text. folders: (host_path, sandbox_path, read_only)."""
    root = ET.Element('Configuration')
    ET.SubElement(root, 'vGPU').text = 'Enable'
    ET.SubElement(root, 'Networking').text = 'Disable'
    ET.SubElement(root, 'MemoryInMB').text = str(memory_mb)
    ET.SubElement(root, 'ClipboardRedirection').text = 'Disable'
    ET.SubElement(root, 'PrinterRedirection').text = 'Disable'
    ET.SubElement(root, 'AudioInput').text = 'Disable'
    ET.SubElement(root, 'VideoInput').text = 'Disable'
    mapped = ET.SubElement(root, 'MappedFolders')
    for host, box, read_only in folders:
        mf = ET.SubElement(mapped, 'MappedFolder')
        ET.SubElement(mf, 'HostFolder').text = host
        ET.SubElement(mf, 'SandboxFolder').text = box
        ET.SubElement(mf, 'ReadOnly').text = 'true' if read_only else 'false'
    logon = ET.SubElement(root, 'LogonCommand')
    ET.SubElement(logon, 'Command').text = logon_command
    ET.indent(root)
    return ET.tostring(root, encoding='unicode') + '\n'


def logon_command():
    return ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File '
            '%s\\driver.ps1 -Config %s\\config.json' % (BOX_TOOLS, BOX_TOOLS))


def driver_config(overrides, timeout, frame_rate, scenario):
    """config.json for driver.ps1 (paths are the sandbox's)."""
    return {
        'scenario': scenario,
        'exe': BOX_BIN + '\\iwd2r.exe',
        'data': BOX_GAME,
        'results': BOX_RESULTS,
        'control': BOX_RESULTS + '\\control',
        'ini_override': ';'.join(overrides),
        'actions': BOX_TOOLS + '\\actions.json',
        'frame_rate': frame_rate,
        'timeout': timeout,
        'shutdown': True,
    }


def write_inputs(tools_dir, config, actions):
    os.makedirs(tools_dir, exist_ok=True)
    with open(os.path.join(tools_dir, 'config.json'), 'w') as f:
        json.dump(config, f, indent=1)
    with open(os.path.join(tools_dir, 'actions.json'), 'w') as f:
        json.dump(actions, f, indent=1)
    shutil.copy2(DRIVER, os.path.join(tools_dir, 'driver.ps1'))


# ---------------------------------------------------------------------------
# Launching


def wslpath_w(path):
    return subprocess.run(['wslpath', '-w', path], check=True,
                          capture_output=True, text=True).stdout.strip()


def start_batch(wsb_windows_path):
    """The batch file that starts the sandbox without waiting for it."""
    return '@echo off\nstart "" "%s" "%s"\n' % (SANDBOX_EXE_WIN,
                                                wsb_windows_path)


def read_text(path):
    try:
        with open(path, encoding='utf-8-sig') as f:
            return f.read().strip()
    except OSError:
        return ''


def sandbox_running():
    r = subprocess.run([CMD_EXE, '/c', 'tasklist', '/FO', 'CSV', '/NH'],
                       capture_output=True, text=True, cwd='/mnt/c')
    out = r.stdout.lower()
    return any(n.lower() in out for n in TASKKILL_NAMES)


def close_sandbox(log, timeout=CLOSE_TIMEOUT):
    """Waits up to timeout seconds for the sandbox to close itself (the
    driver shuts it down), then kills it."""
    end = time.time() + timeout
    while time.time() < end:
        if not sandbox_running():
            return
        time.sleep(1)
    log('sandbox still open: taskkill')
    for name in TASKKILL_NAMES:
        subprocess.run([CMD_EXE, '/c', 'taskkill', '/IM', name, '/F'],
                       capture_output=True, cwd='/mnt/c')


def start_sandbox(host_run, wsb, log):
    """Starts Windows Sandbox with the .wsb file wsb (under host_run) without
    waiting for it. Raises RuntimeError when a sandbox is already running."""
    if sandbox_running():
        raise RuntimeError('a Windows Sandbox is already running (only '
                           'one can run at a time)')
    log('starting Windows Sandbox with %s' % wslpath_w(wsb))
    # A batch file keeps start's empty title argument intact (WSL would
    # escape the quotes of a command-line argument with backslashes, which
    # cmd.exe does not understand).
    bat = os.path.join(host_run, 'start.bat')
    with open(bat, 'w', newline='\r\n') as f:
        f.write(start_batch(wslpath_w(wsb)))
    subprocess.run([CMD_EXE, '/c', wslpath_w(bat)], cwd='/mnt/c', check=True)


def wait_done(results, timeout):
    """Waits for the driver's results/done.txt; RuntimeError (with
    "timeout") after timeout seconds."""
    done = os.path.join(results, 'done.txt')
    end = time.time() + timeout
    while not os.path.exists(done):
        if time.time() > end:
            raise RuntimeError('sandbox timeout: no done.txt after %d s'
                               % timeout)
        time.sleep(2)


def run_scenario(exe, game_cache, actions, overrides, scenario, run_dir,
                 host_run, timeout, frame_rate, log):
    """Runs one scenario in a fresh sandbox. host_run is a folder under
    VM_ROOT (on the Windows drive) for this run; the results are copied into
    run_dir. Returns a dict like realinput.run_scenario's."""
    started = time.time()
    result = {'code': None, 'error': None, 'screen_shots': 0,
              'engine_shots': 0, 'seconds': 0.0}
    results = os.path.join(host_run, 'results')
    tools = os.path.join(host_run, 'tools')
    os.makedirs(os.path.join(results, 'control'), exist_ok=True)
    write_inputs(tools, driver_config(overrides, timeout, frame_rate,
                                      scenario), actions)
    folders = [
        (wslpath_w(os.path.dirname(exe)), BOX_BIN, True),
        (wslpath_w(game_cache), BOX_GAME, False),
        (wslpath_w(results), BOX_RESULTS, False),
        (wslpath_w(tools), BOX_TOOLS, True),
    ]
    wsb = os.path.join(host_run, 'run.wsb')
    with open(wsb, 'w', newline='\r\n') as f:
        f.write(make_wsb(folders, logon_command()))
    shutil.copy2(wsb, os.path.join(run_dir, 'run.wsb'))
    try:
        start_sandbox(host_run, wsb, log)
        wait_done(results, DONE_TIMEOUT)
        code_text = read_text(os.path.join(results, 'exitcode.txt'))
        if code_text.lstrip('-').isdigit():
            result['code'] = int(code_text)
        error = read_text(os.path.join(results, 'error.txt'))
        if error:
            result['error'] = 'driver: %s' % error
        elif result['code'] is None:
            result['error'] = 'driver: no exit code'
    except RuntimeError as e:
        result['error'] = str(e)
    except KeyboardInterrupt:
        result['error'] = 'interrupted'
        raise
    finally:
        close_sandbox(log)
        for name in os.listdir(results):
            src = os.path.join(results, name)
            if os.path.isfile(src):
                shutil.copy2(src, os.path.join(run_dir, name))
        names = os.listdir(run_dir)
        result['screen_shots'] = sum(n.endswith('-screen.png') for n in names)
        result['engine_shots'] = sum(n.endswith('.png')
                                     and not n.endswith('-screen.png')
                                     for n in names)
        if result['error']:
            if 'timeout' in result['error']:
                result['code'] = 124
            elif not result['code']:
                result['code'] = 1
        result['seconds'] = time.time() - started
    return result


# ---------------------------------------------------------------------------
# Replays of the original (oracle.py replay record/compare)
#
#   host                                       sandbox
#   oracle build output (read-only)            C:\iwd2r\oracle
#   C:\Users\thier\iwd2r-oracle\game (rw)       the same path
#   <run>\results (rw)                         C:\iwd2r\results
#   <run>\tools (read-only)                    C:\iwd2r\tools
#
# The game folder keeps its own path inside the sandbox: its icewind2.ini's
# [Alias] paths name it.

REPLAY_DRIVER = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             'sandbox', 'replay-driver.ps1')
BOX_ORACLE = r'C:\iwd2r\oracle'
BOX_REPLAY_DIR = BOX_RESULTS + r'\replay'
# The driver kills the injector this long after the injector's own timeout
# should have ended the run, as run_injector does on the host.
REPLAY_BACKSTOP = 60
# Seconds the host allows on top of the driver's backstop for the sandbox to
# boot, run the driver and shut down, before it gives up on done.txt.
REPLAY_BOOT_ALLOWANCE = 180
# driver.ps1 stops the sandbox's audio services because a muted device can
# still be heard on the host. A recording keeps them: without a device the
# original skips a periodic 40 ms multimedia timer it starts on the host
# when the world loads (recorder.log); the device is muted and the copy's
# icewind2.ini has every volume at 0 (oracle.replay_ini), so nothing is
# heard. True stops them as driver.ps1 does.
REPLAY_STOP_AUDIO = False
# The driver's shutdown rarely closes the sandbox within CLOSE_TIMEOUT; a
# replay's results are complete once done.txt exists (the driver writes it
# last), so the sandbox is killed after this many seconds.
REPLAY_CLOSE_TIMEOUT = 5
# The host's 32-bit DirectPlay (the optional feature the original needs:
# iwd2.exe imports DPLAYX.dll), which the sandbox lacks and cannot enable;
# copied into the tools folder for replay-driver.ps1 to install.
HOST_SYSWOW64 = '/mnt/c/Windows/SysWOW64'
DIRECTPLAY_FILES = ('dplayx.dll', 'dpmodemx.dll', 'dpwsockx.dll',
                    'dplaysvr.exe', 'dpnet.dll', 'dpnaddr.dll',
                    'dpnhpast.dll', 'dpnhupnp.dll', 'dpnlobby.dll',
                    'dpnsvr.exe')


def replay_folders(build_win, game_win, results_win, tools_win):
    """The .wsb mapped folders of a replay: (host, sandbox, read_only)."""
    return [(build_win, BOX_ORACLE, True),
            (game_win, game_win, False),
            (results_win, BOX_RESULTS, False),
            (tools_win, BOX_TOOLS, True)]


def replay_logon_command():
    return ('powershell.exe -NoProfile -ExecutionPolicy Bypass -File '
            '%s\\replay-driver.ps1 -Config %s\\config.json'
            % (BOX_TOOLS, BOX_TOOLS))


def replay_driver_config(game_win, env, timeout):
    """config.json for replay-driver.ps1 (paths are the sandbox's). env: the
    recorder's environment (oracle.recorder_env). The injector gets the
    arguments the host mode gives it."""
    return {
        'injector': BOX_ORACLE + '\\injector.exe',
        'recorder': BOX_ORACLE + '\\recorder.dll',
        'exe': game_win + '\\iwd2.exe',
        'workdir': game_win,
        'replay_dir': BOX_REPLAY_DIR,
        'results': BOX_RESULTS,
        'env': env,
        'timeout': timeout,
        'backstop': timeout + REPLAY_BACKSTOP,
        'exclusions': [BOX_ORACLE, game_win, BOX_RESULTS],
        'stop_audio': REPLAY_STOP_AUDIO,
        'directplay': BOX_TOOLS + '\\directplay',
        'shutdown': True,
    }


def replay_copy_plan(results, out_dir, names, replay_names):
    """(source, destination) pairs that bring a replay's results home:
    results/replay/<name> (the dumps, recorder.log, status.txt,
    scenario.txt) to out_dir/<name>, where the host mode puts them, and the
    driver's own files (results/<name>) to out_dir/sandbox/<name>."""
    plan = [(os.path.join(results, 'replay', n), os.path.join(out_dir, n))
            for n in sorted(replay_names)]
    plan += [(os.path.join(results, n), os.path.join(out_dir, 'sandbox', n))
             for n in sorted(names)]
    return plan


def stage_directplay(target, source=HOST_SYSWOW64, names=DIRECTPLAY_FILES):
    """Copies the host's DirectPlay files into target; RuntimeError naming
    the ones the host lacks (DirectPlay is then off on the host too: enable
    it under Windows Features, Legacy Components)."""
    have = {n.lower(): n for n in os.listdir(source)}
    missing = [n for n in names if n.lower() not in have]
    if missing:
        raise RuntimeError('the host has no %s in %s; enable DirectPlay '
                           '(Windows Features, Legacy Components)'
                           % (', '.join(missing), source))
    os.makedirs(target, exist_ok=True)
    for name in names:
        shutil.copyfile(os.path.join(source, have[name.lower()]),
                        os.path.join(target, name))
    return list(names)


def files_in(directory):
    try:
        return [n for n in os.listdir(directory)
                if os.path.isfile(os.path.join(directory, n))]
    except OSError:
        return []


def run_replay(build_dir, game, script, env, timeout, host_run, out_dir,
               log):
    """Runs one recording of the original in a fresh sandbox: the injector
    and recorder from build_dir, the game folder game (a WSL path), the
    scenario file script, the recorder's environment env (sandbox paths).
    host_run is a new folder under VM_ROOT; the results are copied into
    out_dir (replay_copy_plan). Returns {'code': the injector's exit code
    or None, 'stdout', 'stderr', 'error': the driver's or the host's error
    or None, 'seconds'}."""
    started = time.time()
    result = {'code': None, 'stdout': '', 'stderr': '', 'error': None,
              'seconds': 0.0}
    results = os.path.join(host_run, 'results')
    tools = os.path.join(host_run, 'tools')
    os.makedirs(os.path.join(results, 'replay'))
    os.makedirs(tools)
    shutil.copyfile(script, os.path.join(results, 'replay', 'scenario.txt'))
    game_win = wslpath_w(game)
    with open(os.path.join(tools, 'config.json'), 'w') as f:
        json.dump(replay_driver_config(game_win, env, timeout), f, indent=1)
    shutil.copy2(REPLAY_DRIVER, os.path.join(tools, 'replay-driver.ps1'))
    stage_directplay(os.path.join(tools, 'directplay'))
    wsb = os.path.join(host_run, 'run.wsb')
    with open(wsb, 'w', newline='\r\n') as f:
        f.write(make_wsb(replay_folders(wslpath_w(build_dir), game_win,
                                        wslpath_w(results),
                                        wslpath_w(tools)),
                         replay_logon_command()))
    try:
        start_sandbox(host_run, wsb, log)
        wait_done(results, timeout + REPLAY_BACKSTOP + REPLAY_BOOT_ALLOWANCE)
        code_text = read_text(os.path.join(results, 'exitcode.txt'))
        if code_text.lstrip('-').isdigit():
            result['code'] = int(code_text)
        error = read_text(os.path.join(results, 'error.txt'))
        if error:
            result['error'] = 'driver: %s' % error
    except RuntimeError as e:
        result['error'] = str(e)
    finally:
        close_sandbox(log, REPLAY_CLOSE_TIMEOUT)
        for name, key in (('injector-stdout.txt', 'stdout'),
                          ('injector-stderr.txt', 'stderr')):
            result[key] = read_text(os.path.join(results, name))
        os.makedirs(os.path.join(out_dir, 'sandbox'), exist_ok=True)
        for src, dst in replay_copy_plan(
                results, out_dir, files_in(results),
                files_in(os.path.join(results, 'replay'))):
            shutil.copy2(src, dst)
        shutil.copy2(wsb, os.path.join(out_dir, 'sandbox', 'run.wsb'))
        result['seconds'] = time.time() - started
    return result
