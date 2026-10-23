"""The per-attempt launch limit shared by the launchers (ruling R16).

A launcher creates one folder per launch under a runs root; the counter
lives in that root as attempt.json: {launches, limit, history}.  A launch
past the limit is refused until the caller says why a new attempt starts
(--new-attempt REASON), which zeroes the count and records the reason.

    launch(folder, tool, limit=2) -> None to go ahead, or the refusal text
    reset(folder, reason)         -> the new attempt.json contents
"""
import datetime
import json
import os
import tempfile

LIMIT = 2
NAME = 'attempt.json'


def _now():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        '%Y-%m-%dT%H:%M:%SZ')


def _path(folder):
    return os.path.join(folder, NAME)


def _load(folder, limit):
    try:
        with open(_path(folder), encoding='utf-8') as f:
            data = json.load(f)
        if not isinstance(data, dict):
            raise ValueError('not an object')
    except (OSError, ValueError):
        data = {}
    if not isinstance(data.get('launches'), int):
        data['launches'] = 0
    if not isinstance(data.get('history'), list):
        data['history'] = []
    data['limit'] = limit if not isinstance(data.get('limit'), int) \
        else data['limit']
    return data


def _save(folder, data):
    os.makedirs(folder, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=folder, prefix='.attempt-', suffix='.tmp')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
            f.write('\n')
        os.replace(tmp, _path(folder))
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def launch(folder, tool, limit=LIMIT):
    """Count one launch of `tool` in `folder`.  None when it may go ahead,
    else the refusal; a refused call launches nothing and counts nothing."""
    data = _load(folder, limit)
    data['limit'] = limit
    if data['launches'] >= limit:
        return (f'launch limit reached ({data["launches"]} of {limit}) for '
                f'this attempt; rerun with --new-attempt "<why>"')
    data['launches'] += 1
    data['last'] = {'at': _now(), 'tool': tool}
    _save(folder, data)
    return None


def reset(folder, reason):
    """Start a new attempt: zero the count, record why."""
    if not (reason or '').strip():
        raise ValueError('--new-attempt needs a reason')
    data = _load(folder, LIMIT)
    data['launches'] = 0
    data['history'].append({'at': _now(), 'reason': reason})
    _save(folder, data)
    return data


def gate(folder, tool, new_attempt=None, limit=LIMIT):
    """What a launcher calls before it launches: with a --new-attempt
    reason the count is reset first.  None to go ahead, else the refusal."""
    if new_attempt is not None:
        reset(folder, new_attempt)
    return launch(folder, tool, limit)
