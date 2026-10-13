#!/usr/bin/env python3
"""Compiles the presenter's shaders (platform/shaders/*.vert, *.frag, GLSL
450) to SPIR-V beside them (<name>.<stage>.spv) with glslangValidator -V.

    python3 tools/shaders.py            # rewrite the .spv files
    python3 tools/shaders.py --check    # recompile to a temp dir, compare bytes

The .spv files are committed, so builds never need glslang. The compiler is
the glslang release pinned in tools/shaders_versions.json: a glslangValidator
on PATH is used when it reports that version, otherwise the release is
downloaded (SHA-256 checked) into $IWD2R_HOME/glslang (default
~/.local/share/iwd2r/glslang), the way tools/ghidra/setup.sh installs Ghidra.
--check exits 1 when a blob is stale, missing or has no source.
"""

import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHADERS = os.path.join(ROOT, 'platform', 'shaders')
VERSIONS = os.path.join(ROOT, 'tools', 'shaders_versions.json')
STAGES = ('.vert', '.frag')


def log(msg):
    print('shaders: %s' % msg, flush=True)


def pinned():
    with open(VERSIONS) as f:
        return json.load(f)['glslang']


def host_key():
    machine = platform.machine().lower()
    if machine not in ('x86_64', 'amd64'):
        return None
    if sys.platform.startswith('linux'):
        return 'linux-x86_64'
    if sys.platform == 'win32':
        return 'windows-x86_64'
    return None


def install_root(env):
    home = env.get('IWD2R_HOME') or os.path.join(
        os.path.expanduser('~'), '.local', 'share', 'iwd2r')
    return os.path.join(home, 'glslang')


def reports_version(exe, version):
    """True when exe --version names the pinned release."""
    try:
        out = subprocess.run([exe, '--version'], capture_output=True,
                             text=True, timeout=30).stdout
    except (OSError, subprocess.SubprocessError):
        return False
    return any(line.startswith('Glslang Version:')
               and line.rstrip().endswith(':' + version)
               for line in out.splitlines())


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def extract(archive, dest):
    if archive.endswith('.zip'):
        with zipfile.ZipFile(archive) as z:
            for name in z.namelist():
                target = os.path.realpath(os.path.join(dest, name))
                if not target.startswith(os.path.realpath(dest) + os.sep):
                    raise SystemExit('shaders: unsafe path in %s: %s' % (archive, name))
            z.extractall(dest)
    else:
        with tarfile.open(archive) as t:
            t.extractall(dest, filter='data')


def download(pin, key, root):
    """Installs the pinned release into root and returns its compiler."""
    entry = pin[key]
    os.makedirs(os.path.dirname(root), exist_ok=True)
    tmp = tempfile.mkdtemp(prefix='.glslang-dl.', dir=os.path.dirname(root))
    try:
        archive = os.path.join(tmp, os.path.basename(entry['url']))
        log('downloading glslang %s' % pin['version'])
        with urllib.request.urlopen(entry['url'], timeout=300) as r, \
                open(archive, 'wb') as f:
            shutil.copyfileobj(r, f)
        got = sha256(archive)
        if got != entry['sha256']:
            raise SystemExit('shaders: SHA-256 mismatch for %s: expected %s, got %s'
                             % (entry['url'], entry['sha256'], got))
        unpacked = os.path.join(tmp, 'x')
        os.mkdir(unpacked)
        extract(archive, unpacked)
        with open(os.path.join(unpacked, '.iwd2r-version'), 'w') as f:
            f.write(pin['version'] + '\n')
        if os.path.exists(root):
            shutil.rmtree(root)
        os.rename(unpacked, root)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return os.path.join(root, entry['exe'])


def find_compiler(env=None):
    """The pinned glslangValidator: on PATH at that version, the user-local
    install, or a fresh download."""
    env = os.environ if env is None else env
    pin = pinned()
    on_path = shutil.which('glslangValidator', path=env.get('PATH'))
    if on_path and reports_version(on_path, pin['version']):
        return on_path
    key = host_key()
    if key is None or key not in pin:
        raise SystemExit('shaders: no pinned glslang for %s %s; install glslang %s '
                         'as glslangValidator on PATH'
                         % (sys.platform, platform.machine(), pin['version']))
    root = install_root(env)
    exe = os.path.join(root, pin[key]['exe'])
    try:
        with open(os.path.join(root, '.iwd2r-version')) as f:
            have = f.read().strip()
    except OSError:
        have = None
    if have == pin['version'] and os.path.isfile(exe):
        return exe
    return download(pin, key, root)


def sources(folder):
    return sorted(n for n in os.listdir(folder)
                  if os.path.splitext(n)[1] in STAGES)


def blob_name(source):
    return source + '.spv'


def compile_all(compiler, folder, out_dir):
    for name in sources(folder):
        r = subprocess.run([compiler, '-V', '-o',
                            os.path.join(out_dir, blob_name(name)),
                            os.path.join(folder, name)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            sys.stdout.write(r.stdout + r.stderr)
            raise SystemExit('shaders: %s does not compile' % name)


def check(compiler, folder):
    """The stale, missing and orphaned blobs, as messages."""
    problems = []
    with tempfile.TemporaryDirectory() as tmp:
        compile_all(compiler, folder, tmp)
        for name in sorted(os.listdir(tmp)):
            have = os.path.join(folder, name)
            if not os.path.exists(have):
                problems.append('%s is missing' % name)
                continue
            with open(have, 'rb') as a, open(os.path.join(tmp, name), 'rb') as b:
                if a.read() != b.read():
                    problems.append('%s does not match its source' % name)
    wanted = {blob_name(n) for n in sources(folder)}
    for name in sorted(os.listdir(folder)):
        if name.endswith('.spv') and name not in wanted:
            problems.append('%s has no source' % name)
    return problems


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--check', action='store_true',
                    help='recompile to a temp dir and compare with the committed blobs')
    ap.add_argument('--dir', default=SHADERS,
                    help='the shader folder (default platform/shaders)')
    args = ap.parse_args(argv)
    compiler = find_compiler()
    if args.check:
        problems = check(compiler, args.dir)
        for p in problems:
            log(p)
        if problems:
            log('stale: run python3 tools/shaders.py')
            return 1
        log('%d blobs match their sources' % len(sources(args.dir)))
        return 0
    compile_all(compiler, args.dir, args.dir)
    log('compiled %d shaders' % len(sources(args.dir)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
