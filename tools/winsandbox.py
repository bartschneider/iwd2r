# iwd2r. Distributed under LICENSE.md.
"""Windows Sandbox runs for `play.py --vm windows`.

Each scenario runs in a fresh, disposable Windows Sandbox: play.py builds
iwd2r.exe with MSVC (deploying the MSVC runtime DLLs next to it), keeps a copy
of the install in a host cache (the real install is never mapped), writes a
.wsb file that maps four folders and runs tools/sandbox/driver.ps1 at logon,
starts the sandbox, waits for the driver's done.txt and closes it.

  host                                   sandbox
  build output (read-only)               C:\\iwd2r\\bin
  C:\\Users\\thier\\iwd2r-vm\\game (rw)       C:\\iwd2r\\game
  <run>\\results (rw)                     C:\\iwd2r\\results
  <run>\\tools (read-only)                C:\\iwd2r\\tools

Standard library only.
"""

import glob
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


def cache_needs_refresh(source, cache):
    """True when the cache is missing or its CHITIN.KEY differs from the
    source's in mtime or size."""
    want = key_stat(source)
    if want is None:
        raise FileNotFoundError('%s has no CHITIN.KEY' % source)
    return key_stat(cache) != want


def refresh_cache(source, cache, log):
    """Copies the install into the cache when needed (rsync keeps mtimes, so
    the next comparison matches). Returns True when it copied."""
    if not cache_needs_refresh(source, cache):
        log('game cache %s is current' % cache)
        return False
    os.makedirs(cache, exist_ok=True)
    log('copying the install to the game cache %s (about 1.9 GB, once)'
        % cache)
    started = time.time()
    r = subprocess.run(['rsync', '-rt', '--delete', source.rstrip('/') + '/',
                        cache.rstrip('/') + '/'])
    if r.returncode != 0:
        raise RuntimeError('rsync to the game cache failed')
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


def find_redist(root=REDIST_ROOT):
    """The newest Microsoft.VC*.CRT x64 folder under the VS redist root, or
    None."""
    found = glob.glob(os.path.join(root, '*', 'x64', 'Microsoft.VC*.CRT'))

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


def deploy_runtime(exe, log, redist_root=REDIST_ROOT):
    """Copies the MSVC runtime DLLs exe imports next to it, so the sandbox
    needs no installer. Returns the names copied or already current."""
    imports = pe_imports(exe)
    if not any(n.lower().startswith(('vcruntime', 'msvcp', 'concrt'))
               for n in imports):
        log('%s links the MSVC runtime statically' % os.path.basename(exe))
        return []
    redist = find_redist(redist_root)
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


def close_sandbox(log):
    """Waits for the sandbox to close itself (the driver shuts it down),
    then kills it."""
    end = time.time() + CLOSE_TIMEOUT
    while time.time() < end:
        if not sandbox_running():
            return
        time.sleep(1)
    log('sandbox still open: taskkill')
    for name in TASKKILL_NAMES:
        subprocess.run([CMD_EXE, '/c', 'taskkill', '/IM', name, '/F'],
                       capture_output=True, cwd='/mnt/c')


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
        if sandbox_running():
            raise RuntimeError('a Windows Sandbox is already running (only '
                               'one can run at a time)')
        log('starting Windows Sandbox with %s' % wslpath_w(wsb))
        # A batch file keeps start's empty title argument intact (WSL would
        # escape the quotes of a command-line argument with backslashes,
        # which cmd.exe does not understand).
        bat = os.path.join(host_run, 'start.bat')
        with open(bat, 'w', newline='\r\n') as f:
            f.write(start_batch(wslpath_w(wsb)))
        subprocess.run([CMD_EXE, '/c', wslpath_w(bat)], cwd='/mnt/c',
                       check=True)
        done = os.path.join(results, 'done.txt')
        end = time.time() + DONE_TIMEOUT
        while not os.path.exists(done):
            if time.time() > end:
                raise RuntimeError('sandbox timeout: no done.txt after %d s'
                                   % DONE_TIMEOUT)
            time.sleep(2)
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
