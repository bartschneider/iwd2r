"""Ownership claims: which stream is editing which source files.

    python3 -I tools/claims.py claim <stream> <holder> <file>...
    python3 -I tools/claims.py release <stream>
    python3 -I tools/claims.py list

docs/wso/claims.json maps a stream name to {holder, since, files}.  A
stream claims the files it is about to edit, so the batch driver never cuts
a batch over them (wave.next_batch skips them), and releases them at merge.
A file also counts as held by every batch with status "assigned" in
docs/wso/wave-state.json, which this module only reads.
"""

import argparse
import datetime
import json
import os
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLAIMS_PATH = os.path.join(ROOT, 'docs', 'wso', 'claims.json')
STATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'wave-state.json')


class ClaimError(Exception):
    pass


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


def owned(path, claims_path=CLAIMS_PATH, state_path=STATE_PATH):
    """The stream name or batch id holding path, or None."""
    for stream, c in sorted(_load(claims_path).items()):
        if path in c.get('files', []):
            return stream
    return _batch_holders(state_path).get(path)


def claim(stream, holder, files, claims_path=CLAIMS_PATH,
          state_path=STATE_PATH):
    """Claim files for stream; ClaimError names the conflicting holder."""
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
            'holder': holder, 'files': list(dict.fromkeys(files)),
            'since': datetime.datetime.now(datetime.timezone.utc)
            .strftime('%Y-%m-%dT%H:%M:%SZ')}
    _save(claims_path, claims)


def release(stream, claims_path=CLAIMS_PATH, state_path=STATE_PATH):
    claims = _load(claims_path)
    if claims.pop(stream, None) is not None:
        _save(claims_path, claims)


def main(argv=None):
    ap = argparse.ArgumentParser(prog='claims.py')
    ap.add_argument('--claims', default=CLAIMS_PATH)
    ap.add_argument('--state', default=STATE_PATH)
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('claim')
    c.add_argument('stream')
    c.add_argument('holder')
    c.add_argument('files', nargs='+')
    r = sub.add_parser('release')
    r.add_argument('stream')
    sub.add_parser('list')
    a = ap.parse_args(argv)
    kw = dict(claims_path=a.claims, state_path=a.state)
    try:
        if a.cmd == 'claim':
            claim(a.stream, a.holder, a.files, **kw)
        elif a.cmd == 'release':
            release(a.stream, **kw)
        else:
            for s, e in sorted(_load(a.claims).items()):
                print(f'{s}\t{e.get("holder", "")}\t{e.get("since", "")}\t'
                      + ' '.join(e.get('files', [])))
    except ClaimError as e:
        print(f'claims: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
