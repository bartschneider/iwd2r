#!/usr/bin/env python3
"""Fetch the WSO fork's renderer artifacts by pinned sha256.

Reads a pins file ({"fork_commit", "upstream_commit", "files":
[{"url", "dest", "sha256"}]}), downloads each file into
${IWD2R_HOME:-~/.local/share/iwd2r}/wso/<dest> and writes only after the
downloaded bytes hash to the pinned sha256; nothing is written anywhere
outside that wso directory.  --check verifies the files already there
and downloads nothing.  urllib only, stdlib only.
"""

import argparse
import hashlib
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

PINS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'wso_pins.json')


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
    """Download every pinned file under root; returns problem strings."""
    problems = []
    for entry in pins['files']:
        try:
            fetch_one(entry, root)
        except (ValueError, OSError, urllib.error.URLError) as e:
            problems.append(f'{entry["dest"]}: {e}')
    return problems


def check(pins, root):
    """Verify the pinned files already under root; downloads nothing."""
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
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Fetch or verify the WSO fork renderer artifacts by pinned sha256.')
    parser.add_argument('--check', action='store_true',
                        help='verify the pinned files only; download nothing')
    parser.add_argument('--pins', default=PINS,
                        help='pins JSON (default: tools/wso_pins.json)')
    args = parser.parse_args(argv)
    with open(args.pins) as f:
        pins = json.load(f)
    root = wso_root()
    problems = check(pins, root) if args.check else fetch(pins, root)
    for p in problems:
        print(f'wso_fetch: {p}', file=sys.stderr)
    print(f'wso_fetch: {len(pins["files"])} pinned files, {len(problems)} '
          f'problem(s) under {root}')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
