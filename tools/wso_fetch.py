#!/usr/bin/env python3
"""Fetch the WSO fork's renderer artifacts by pinned sha256.

Reads a pins file ({"fork_commit", "upstream_commit", "files":
[{"url", "dest", "sha256"}]}), downloads each file into
${IWD2R_HOME:-~/.local/share/iwd2r}/wso/<dest> and writes only after the
downloaded bytes hash to the pinned sha256; nothing is written anywhere
outside that wso directory.  A pin with "unzip": true is additionally
extracted beside its archive -- but only after the hash matched, member
names are sanitized (no absolute paths, no '..', no symlink members),
and member count and total size are capped.  Extracted files are data:
nothing from an archive is ever executed.  --check verifies the files
already there (and, for unzipped pins, the extracted members) and
downloads nothing.  urllib only, stdlib only.
"""

import argparse
import hashlib
import json
import os
import shutil
import stat
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

PINS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wso_pins.json')

# Archive extraction caps: blunt zip bombs. The real pins (BG2 PDB dumps,
# a Ghidra project) are 10 members / ~240 MB extracted, far below these.
UNZIP_MEMBER_CAP = 4096
UNZIP_BYTES_CAP = 2 << 30  # 2 GiB


def wso_root(home=None):
    """The wso artifact directory under IWD2R_HOME (~/.local/share/iwd2r)."""
    if home is None:
        home = os.environ.get('IWD2R_HOME') or os.path.join(
            os.path.expanduser('~'), '.local', 'share', 'iwd2r')
    return Path(home) / 'wso'


def resolve_dest(dest, root):
    """Resolve dest under root; refuse anything that could land outside it."""
    text = str(dest)
    parts = [p for p in text.replace('\\', '/').split('/') if p not in ('', '.')]
    if not parts or os.path.isabs(text) or '..' in parts:
        raise ValueError(f'unsafe dest: {dest!r}')
    target = Path(root).joinpath(*parts)
    if not target.resolve().is_relative_to(Path(root).resolve()):
        raise ValueError(f'dest escapes the wso root: {dest!r}')
    return target


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def _member_rel(name, dest_dir):
    """A safe relative path for a zip member (its filename, or that of a
    ZipInfo), or None if the name would escape the extraction directory
    (absolute, drive letter, '..', symlink member)."""
    info = name if isinstance(name, zipfile.ZipInfo) else None
    if info is not None and stat.S_ISLNK(info.external_attr >> 16):
        return None
    text = (info.filename if info is not None else str(name)).replace('\\', '/')
    parts = [p for p in text.split('/') if p not in ('', '.')]
    if (not parts or os.path.isabs(text) or '..' in parts
            or (len(text) > 1 and text[1] == ':')):
        return None
    target = dest_dir.joinpath(*parts)
    if not target.resolve().is_relative_to(dest_dir.resolve()):
        return None
    return target.relative_to(dest_dir)


def extract_one(entry, root):
    """Extract a fetched, hash-verified zip beside itself (its dest's
    parent).  Members are written through a staging directory and each
    replaces its target atomically; a manifest next to the archive names
    what was extracted so a re-extract can drop members a new archive no
    longer carries.  Data only: nothing is executed."""
    target = resolve_dest(entry['dest'], root)
    dest_dir = target.parent
    with zipfile.ZipFile(target) as z:
        infos = z.infolist()
        total = sum(i.file_size for i in infos)
        if len(infos) > UNZIP_MEMBER_CAP:
            raise ValueError(f'{len(infos)} members exceeds the cap '
                             f'{UNZIP_MEMBER_CAP}')
        if total > UNZIP_BYTES_CAP:
            raise ValueError(f'{total} extracted bytes exceeds the cap '
                             f'{UNZIP_BYTES_CAP}')
        rels = {}
        for info in infos:
            if info.is_dir():
                continue
            rel = _member_rel(info, dest_dir)
            if rel is None:
                raise ValueError(f'unsafe archive member: {info.filename!r}')
            rels[info] = rel

        manifest = dest_dir / (target.name + '.extracted.json')
        previous = []
        if manifest.is_file():
            try:
                with open(manifest) as f:
                    previous = json.load(f)
            except (OSError, ValueError):
                previous = []

        staging = dest_dir / f'.{target.name}.part'
        shutil.rmtree(staging, ignore_errors=True)
        staging.mkdir(parents=True)
        try:
            for info, rel in rels.items():
                out = staging / rel
                out.parent.mkdir(parents=True, exist_ok=True)
                with z.open(info) as src, open(out, 'wb') as dst:
                    shutil.copyfileobj(src, dst)
            for info, rel in rels.items():
                final = dest_dir / rel
                final.parent.mkdir(parents=True, exist_ok=True)
                os.replace(staging / rel, final)
            shutil.rmtree(staging)
        except BaseException:
            shutil.rmtree(staging, ignore_errors=True)
            raise

        # Drop members the new archive no longer carries.
        keep = {str(r) for r in rels.values()}
        for stale in previous:
            if stale in keep:
                continue
            prefix = dest_dir.relative_to(Path(root))
            victim = resolve_dest(str(prefix / stale), root)
            victim.unlink(missing_ok=True)
        parent = final.parent
        while parent != dest_dir and not any(parent.iterdir()):
            parent.rmdir()
            parent = parent.parent

        tmp = manifest.with_name(manifest.name + '.part')
        with open(tmp, 'w') as f:
            json.dump(sorted(keep), f)
        os.replace(tmp, manifest)
    return sorted(keep)


def fetch_one(entry, root):
    """Fetch one pinned file: download, hash, and only then write (atomic
    replace), so a hash mismatch leaves the destination untouched."""
    target = resolve_dest(entry['dest'], root)
    with urllib.request.urlopen(entry['url'], timeout=60) as resp:
        data = resp.read()
    got = hashlib.sha256(data).hexdigest()
    if got != entry['sha256']:
        raise ValueError(f'sha256 {got} != pinned {entry["sha256"]}')
    target.parent.mkdir(parents=True, exist_ok=True)
    tmp = target.with_name(target.name + '.part')
    try:
        with open(tmp, 'wb') as f:
            f.write(data)
        os.replace(tmp, target)
    except BaseException:
        tmp.unlink(missing_ok=True)
        raise


def fetch(pins, root):
    """Download every pinned file under root (extracting the ones pinned
    with "unzip": true, only after their hash matched); returns problem
    strings."""
    problems = []
    for entry in pins['files']:
        try:
            fetch_one(entry, root)
            if entry.get('unzip'):
                extract_one(entry, root)
        except (ValueError, OSError, urllib.error.URLError,
                zipfile.BadZipFile) as e:
            problems.append(f'{entry["dest"]}: {e}')
    return problems


def check(pins, root):
    """Verify the pinned files already under root; downloads nothing.  An
    unzipped pin's extracted members are also verified against the
    archive's own listing (present, at the recorded size)."""
    problems = []
    for entry in pins['files']:
        dest = entry['dest']
        try:
            target = resolve_dest(dest, root)
        except ValueError as e:
            problems.append(str(e))
            continue
        if not target.is_file():
            problems.append(f'{dest}: missing (run without --check to fetch it)')
            continue
        got = sha256_file(target)
        if got != entry['sha256']:
            problems.append(f'{dest}: sha256 {got} != pinned {entry["sha256"]}')
            continue
        if not entry.get('unzip'):
            continue
        dest_dir = target.parent
        with zipfile.ZipFile(target) as z:
            for info in z.infolist():
                if info.is_dir():
                    continue
                rel = _member_rel(info, dest_dir)
                if rel is None:
                    problems.append(f'{dest}: unsafe member {info.filename!r}')
                    continue
                extracted = dest_dir / rel
                if not extracted.is_file():
                    problems.append(f'{dest}/{rel}: extracted file missing '
                                    '(run without --check to re-extract)')
                elif extracted.stat().st_size != info.file_size:
                    problems.append(f'{dest}/{rel}: extracted size changed '
                                    '(run without --check to re-extract)')
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Fetch or verify the WSO fork renderer artifacts by pinned sha256.')
    parser.add_argument('--check', action='store_true',
                        help='verify the pinned files only; download nothing')
    parser.add_argument('--pins', default=PINS,
                        help='pins JSON (default: tools/wso_pins.json)')
    args = parser.parse_args(argv)
    try:
        with open(args.pins) as f:
            pins = json.load(f)
    except (OSError, ValueError) as e:  # JSONDecodeError is a ValueError
        print(f'wso_fetch: {args.pins}: {e}', file=sys.stderr)
        return 1
    if not isinstance(pins, dict) or not isinstance(pins.get('files'), list):
        print(f'wso_fetch: {args.pins}: "files" must be a list of pins',
              file=sys.stderr)
        return 1
    root = wso_root()
    problems = check(pins, root) if args.check else fetch(pins, root)
    for p in problems:
        print(f'wso_fetch: {p}', file=sys.stderr)
    print(f'wso_fetch: {len(pins["files"])} pinned files, {len(problems)} '
          f'problem(s) under {root}')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
