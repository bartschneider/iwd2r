"""Ownership claims: which stream is editing which source files.

    python3 -I tools/claims.py claim <stream> <holder> <file>...
    python3 -I tools/claims.py release <stream>
    python3 -I tools/claims.py list

docs/wso/claims.json maps a stream name to {holder, since, files}.  A
stream claims the files it is about to edit, so the batch driver never cuts
a batch over them (wave.next_batch skips them), and releases them at merge.
A file also counts as held by every batch with status "assigned" in
docs/wso/wave-state.json, which this module only reads.

Claims and state always live in the checkout that has branch `first`
(found with `git worktree list`), whichever worktree runs this tool, so the
driver and every stream see the same file.  Paths are stored repo-relative
with forward slashes; relative arguments are taken from this checkout's root.
"""

import argparse
import datetime
import importlib.util
import json
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLAIMS_REL = os.path.join('docs', 'wso', 'claims.json')
STATE_REL = os.path.join('docs', 'wso', 'wave-state.json')


class ClaimError(Exception):
    pass


class NoFirstCheckout(ClaimError):
    pass


def _filelock():
    spec = importlib.util.spec_from_file_location(
        'filelock', os.path.join(ROOT, 'tools', 'filelock.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def first_checkout(repo=ROOT):
    """The worktree directory that has branch `first` checked out."""
    try:
        out = subprocess.run(['git', '-C', repo, 'worktree', 'list',
                              '--porcelain'], capture_output=True, text=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError) as e:
        raise NoFirstCheckout(f'cannot list worktrees: {e}')
    path = None
    for line in out.splitlines():
        if line.startswith('worktree '):
            path = line[len('worktree '):]
        elif line == 'branch refs/heads/first' and path:
            return path
    raise NoFirstCheckout('no worktree has branch first checked out; '
                          'claims live in that checkout')


def _paths(claims_path, state_path):
    if claims_path is None or state_path is None:
        top = first_checkout()
        claims_path = claims_path or os.path.join(top, CLAIMS_REL)
        state_path = state_path or os.path.join(top, STATE_REL)
    return claims_path, state_path


def normalize(path):
    """Repo-relative POSIX form of path; ClaimError when outside the repo."""
    p = os.path.normpath(os.path.join(ROOT, path))
    rel = os.path.relpath(p, ROOT)
    if rel == '.' or rel == '..' or rel.startswith('..' + os.sep):
        raise ClaimError(f'{path} is outside the repository')
    return rel.replace(os.sep, '/')


def _load(path):
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
    except FileNotFoundError:
        return {}
    if not isinstance(data, dict):
        raise ClaimError(f'{path}: not a JSON object')
    return data


def _save(path, data):
    """Atomic: a temp file in the same directory, then os.replace."""
    d = os.path.dirname(os.path.abspath(path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.claims-', suffix='.json')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
            f.write('\n')
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def _batch_holders(state_path):
    """{file: batch id} for the assigned batches (state is read only)."""
    try:
        with open(state_path, encoding='utf-8') as f:
            state = json.load(f)
    except FileNotFoundError:
        return {}
    out = {}
    for bid, b in state.get('batches', {}).items():
        if b.get('status') == 'assigned':
            for p in b.get('files', []):
                out.setdefault(p, bid)
    return out


def owned(path, claims_path=None, state_path=None):
    """The stream name or batch id holding path, or None."""
    claims_path, state_path = _paths(claims_path, state_path)
    path = normalize(path)
    for stream, c in sorted(_load(claims_path).items()):
        if path in c.get('files', []):
            return stream
    return _batch_holders(state_path).get(path)


def claim(stream, holder, files, claims_path=None, state_path=None):
    """Claim files for stream; ClaimError names the conflicting holder."""
    claims_path, state_path = _paths(claims_path, state_path)
    files = list(dict.fromkeys(normalize(p) for p in files))
    with _filelock().hold(claims_path + '.lock', 'claims'):
        claims = _load(claims_path)
        batches = _batch_holders(state_path)
        for p in files:
            for other, c in sorted(claims.items()):
                if other != stream and p in c.get('files', []):
                    raise ClaimError(f'{p} is held by stream {other} '
                                     f'({c.get("holder", "?")})')
            if p in batches:
                raise ClaimError(f'{p} is held by assigned batch {batches[p]}')
        cur = claims.get(stream)
        if cur:
            merged = list(cur.get('files', []))
            merged += [p for p in files if p not in merged]
            claims[stream] = dict(cur, holder=holder, files=merged)
        else:
            claims[stream] = {
                'holder': holder, 'files': files,
                'since': datetime.datetime.now(datetime.timezone.utc)
                .strftime('%Y-%m-%dT%H:%M:%SZ')}
        _save(claims_path, claims)


def release(stream, claims_path=None, state_path=None):
    claims_path, state_path = _paths(claims_path, state_path)
    with _filelock().hold(claims_path + '.lock', 'claims'):
        claims = _load(claims_path)
        if claims.pop(stream, None) is not None:
            _save(claims_path, claims)


def main(argv=None):
    ap = argparse.ArgumentParser(prog='claims.py')
    ap.add_argument('--claims', default=None)
    ap.add_argument('--state', default=None)
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('claim')
    c.add_argument('stream')
    c.add_argument('holder')
    c.add_argument('files', nargs='+')
    r = sub.add_parser('release')
    r.add_argument('stream')
    sub.add_parser('list')
    a = ap.parse_args(argv)
    try:
        kw = dict(zip(('claims_path', 'state_path'),
                      _paths(a.claims, a.state)))
        if a.cmd == 'claim':
            claim(a.stream, a.holder, a.files, **kw)
        elif a.cmd == 'release':
            release(a.stream, **kw)
        else:
            for s, e in sorted(_load(kw['claims_path']).items()):
                print(f'{s}\t{e.get("holder", "")}\t{e.get("since", "")}\t'
                      + ' '.join(e.get('files', [])))
    except NoFirstCheckout as e:
        print(f'claims: {e}', file=sys.stderr)
        return 2
    except ClaimError as e:
        print(f'claims: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
