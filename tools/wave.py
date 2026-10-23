#!/usr/bin/env python3
"""wave.py -- drive the disagreement batches and the wave closes.

The coordinator's wave loop, one command each::

    python3 tools/wave.py next                 # pick, cut and triage the next batch
    python3 tools/wave.py brief b001           # the text to dispatch verbatim
    python3 tools/wave.py verdict b001 0x643CC0 ours probe-9
    python3 tools/wave.py close b001           # gates, then ff `first`
    python3 tools/wave.py close b002 --kind import   # waves 2-5
    python3 tools/wave.py scorecard            # the tally into xrec-report.md
    python3 tools/wave.py status
    python3 tools/wave.py run [--workers N] [--budget C] [--seed S]   # the driver
    python3 tools/wave.py run --dry-run --state COPY.json   # one batch, fake worker
    python3 tools/wave.py run --clear b011      # the coordinator's retry of an exception

Rows come from docs/wso/disagreements.json (tools/xrec.py); batch state
lives in docs/wso/wave-state.json, created by the first `next`.  Nothing
is written by hand: `next` picks the rows, cuts the batch's branch and
worktree from `first` and applies docs/wso/triage.json (auto verdicts for
the classes calibration enables, ours-stub rows to
docs/wso/import-list.json), `close` checks the batch's worktree itself
(verdicts, evidence lines, clean tree, an unmodified gate, gates), then
fast-forwards `first` to the batch branch.

Exit codes: 0 ok, 1 refusal (close lists every reason), 2 usage error or
nothing left to assign.  `run` (tools/driver.py) exits 0 when every row
is assigned or decided or at the budget stop, 1 when an exception that is
not a batch's own (cut, push) stopped it, it halted (a launch failure, an
auth or billing API error, the breaker), the status file cannot be read
or another driver runs on it, 3 when parked, 4 when stalled (the rows
left wait on files owned by assigned batches).  Every state write is atomic (temp file in the
same directory, then os.replace), so a killed `next` cannot corrupt it.
"""
from __future__ import annotations

import datetime
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'wave-state.json')
DISAGREEMENTS_PATH = os.path.join(ROOT, 'docs', 'wso', 'disagreements.json')
TEMPLATE_PATH = os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')
REPORT_PATH = os.path.join(ROOT, 'docs', 'wso', 'xrec-report.md')
TRIAGE_PATH = os.path.join(ROOT, 'docs', 'wso', 'triage.json')
# Relative to the repo `next` is given: where ours-stub rows go when they
# leave wave 1.
IMPORT_LIST_REL = os.path.join('docs', 'wso', 'import-list.json')
# The fork is read by pinned sha, never by branch name (wso/main moves).
DEFAULT_FORK_BASE = 'e8dd16a750966af53f24daaf49dcb4fb13594c13'
WSO_DELTA_HEAD = '## WSO delta'

DEFAULT_SIZE = 20
DEFAULT_CATEGORY = 'both_changed_differ'
SIDES = ('fork', 'ours', 'neither')
# Scorecard display order, strongest verification first (the same kinds
# check_evidence.py knows).
DISPLAY_ORDER = ('oracle', 'replay', 'read-only', 'trace')
# `close --kind import` accepts trace or better; wave-1 closes read the
# body phrases instead (EVIDENCE_PHRASES).
IMPORT_KINDS = ('trace', 'replay', 'oracle')
EVIDENCE_PHRASES = ('disagreed with WSO', 'corroborated by WSO')
EVIDENCE_DIR = 'docs/evidence'
KIND_RE = re.compile(r'^kind:\s*(\S+)\s*$', re.M)
# A batch branch must not change the gate that certifies it; `close`
# refuses when the branch's diff against the base touches either path.
GATE_PATHS = ('tools/gates.sh', 'tools/parity')


def _now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec='seconds')


def _addr_key(address):
    """0x643CC0 / 643cc0 -> 643cc0: one spelling per address."""
    return (address or '').removeprefix('0x').lower()


def _evidence_rel(address):
    return f'{EVIDENCE_DIR}/{_addr_key(address).upper()}.md'


def load_state(path=STATE_PATH):
    try:
        with open(path, encoding='utf-8') as f:
            state = json.load(f)
    except FileNotFoundError:
        return {'batches': {}}
    if not isinstance(state, dict) or not isinstance(state.get('batches', {}), dict):
        raise ValueError(f'{path}: not a wave-state file')
    state.setdefault('batches', {})
    return state


def _save_json(path, data):
    """Write JSON atomically: a temp file in the same directory, then
    os.replace."""
    d = os.path.dirname(os.path.abspath(path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.wave-', suffix='.json')
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


def save_state(path, state):
    _save_json(path, state)


def _load_list(path, what, missing_ok=False):
    """(entries, None) for a JSON list of objects, or (None, error).  A
    missing file is an empty list when missing_ok."""
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
    except FileNotFoundError:
        if missing_ok:
            return [], None
        return None, f'no {what} {path}\n'
    except (OSError, ValueError) as e:
        return None, f'{what} {path}: {e}\n'
    if not isinstance(data, list) or not all(
            isinstance(e, dict) and 'address' in e for e in data):
        return None, f'{what} {path}: not a list of rows with an address\n'
    return data, None


_triage_module = None


def _triage():
    """tools/triage.py, loaded by path: under -I tools/ is not on sys.path."""
    global _triage_module
    if _triage_module is None:
        spec = importlib.util.spec_from_file_location(
            'triage', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   'triage.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _triage_module = module
    return _triage_module


_driver_module = None


def _driver():
    """tools/driver.py, loaded by path: under -I tools/ is not on sys.path."""
    global _driver_module
    if _driver_module is None:
        spec = importlib.util.spec_from_file_location(
            'driver', os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   'driver.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _driver_module = module
    return _driver_module


_assemble_module = None


def _assemble_bundle(address):
    """assemble_context.bundle, loaded by path (tools/ is not on sys.path
    under -I)."""
    global _assemble_module
    if _assemble_module is None:
        spec = importlib.util.spec_from_file_location(
            'assemble_context',
            os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         'assemble_context.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _assemble_module = module
    return _assemble_module.bundle(address)


# -- next ---------------------------------------------------------------

def default_worktree_root(repo=ROOT):
    """Where batch worktrees live: the main worktree's path (the first one
    `git worktree list` names) plus '-worktrees'.  None outside a repo."""
    r = subprocess.run(['git', '-C', repo, 'worktree', 'list', '--porcelain'],
                       capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if line.startswith('worktree '):
            return line[len('worktree '):] + '-worktrees'
    return None


def _cut_worktree(repo, branch, base, worktree_root=None):
    """The batch branch's worktree: the one it already has, else a new one
    at <worktree_root>/<branch, / as -> with the branch cut from `base` (or
    checked out as it is, when the branch exists without a worktree).
    (path, how) or (None, error)."""
    wt = worktree_for(repo, branch)
    if wt:
        return wt, 'its existing worktree'
    root = worktree_root or default_worktree_root(repo)
    if root is None:
        return None, f'no main worktree found for {repo}'
    path = os.path.join(root, branch.replace('/', '-'))
    def rev(ref):
        r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify',
                            '--quiet', ref], capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else None

    exists = rev(f'refs/heads/{branch}') is not None
    if exists:
        cmd = ['git', '-C', repo, 'worktree', 'add', '-q', path, branch]
    else:
        start = rev(f'{base}^{{commit}}')
        if start is None:
            return None, f'{base} not found in {repo}'
        cmd = ['git', '-C', repo, 'worktree', 'add', '-q', '-b', branch, path,
               start]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        if not exists:
            # git creates the branch before it fails on the path; drop it,
            # only while it still points where this call put it.
            subprocess.run(['git', '-C', repo, 'update-ref', '-d',
                            f'refs/heads/{branch}', start],
                           capture_output=True, text=True)
        return None, (f'git worktree add {path} {branch} failed: '
                      f'{r.stderr.strip()}')
    return path, ('the existing branch, new worktree' if exists
                  else f'new branch from {base}')


def next_batch(disagreements_path=DISAGREEMENTS_PATH, state_path=STATE_PATH,
               size=DEFAULT_SIZE, category=DEFAULT_CATEGORY, *,
               triage_path=None, repo=None, fork_base=None,
               worktree_root=None, import_path=None, base='first'):
    """Rank the unclaimed rows, take the biggest `size`, record the batch.

    With triage_path (the CLI passes docs/wso/triage.json and ROOT) it
    also skips rows already on the import list, cuts the batch's branch
    p3/wso-<id> from `base` and its worktree under worktree_root (default
    default_worktree_root(repo)) unless the branch has one, and then runs
    apply_triage with fork_base (default: the state's).  Nothing is
    recorded when the worktree cannot be cut.  Without triage_path only
    the state file is written."""
    with open(disagreements_path, encoding='utf-8') as f:
        rows = json.load(f)
    state = load_state(state_path)
    decided = set()
    owned = set()
    if triage_path is not None:
        repo = repo or ROOT
        _, err = _load_list(triage_path, 'triage file')
        if err:
            return 2, err
        if import_path is None:
            import_path = os.path.join(repo, IMPORT_LIST_REL)
        imported, err = _load_list(import_path, 'import list', missing_ok=True)
        if err:
            return 2, err
        # Rows on the import list have left wave 1.
        decided.update(_addr_key(e['address']) for e in imported)
    for b in state['batches'].values():
        for r in b['rows']:
            if r.get('address'):
                decided.add(_addr_key(r['address']))
        if b['status'] == 'assigned':
            owned.update(b['files'])

    def in_category(r):
        return r.get('category') == category

    no_addr = sum(1 for r in rows if in_category(r) and not r.get('address'))
    candidates = [r for r in rows
                  if in_category(r) and r.get('address')
                  and _addr_key(r['address']) not in decided]
    eligible = [r for r in candidates if r['file'] not in owned]
    if not eligible:
        if candidates:
            return 2, ('nothing left to assign: %d row%s wait%s on files '
                       'owned by assigned batches\n'
                       % (len(candidates), 's' if len(candidates) != 1 else '',
                          '' if len(candidates) != 1 else 's'))
        return 2, f'nothing left: every {category} row is assigned or decided\n'
    eligible.sort(key=lambda r: (-(r['their_len'] + r['our_len']),
                                 r['file'], r['name']))
    chosen = eligible[:size]
    nid = 1 + max((int(m.group(1)) for bid in state['batches']
                   for m in [re.fullmatch(r'b(\d+)', bid)] if m), default=0)
    batch_id = f'b{nid:03d}'
    files = _files_of(chosen)
    branch = f'p3/wso-{batch_id}'
    if triage_path is not None:
        wt, how = _cut_worktree(repo, branch, base, worktree_root)
        if wt is None:
            return 1, f'{batch_id} not assigned: {how}\n'
    state['batches'][batch_id] = {
        'rows': chosen,
        'files': files,
        'branch': branch,
        'status': 'assigned',
        'assigned_at': _now(),
        'closed_at': None,
        'verdicts': {},
    }
    save_state(state_path, state)
    lines = [f'{batch_id} assigned: {len(chosen)} rows, {len(files)} files, '
             f'branch {branch}',
             f'files: {", ".join(files)}']
    for i, r in enumerate(chosen, 1):
        lines.append(f'{i:2}. {r["address"]}  {r["name"]}  {r["file"]}  '
                     f'theirs {r["their_len"]} / ours {r["our_len"]}')
    if no_addr:
        lines.append(f'(skipped {no_addr} row'
                     f'{"s" if no_addr != 1 else ""} without an address)')
    if triage_path is None:
        return 0, '\n'.join(lines) + '\n'
    lines.append(f'worktree {wt} ({how})')
    rc, out = apply_triage(
        state_path, repo, batch_id, triage_path,
        fork_base or state.get('fork_base') or DEFAULT_FORK_BASE,
        import_path=import_path)
    return rc, '\n'.join(lines) + '\n' + out


def _files_of(rows):
    """The rows' files, first-seen order."""
    files = []
    for r in rows:
        if r['file'] not in files:
            files.append(r['file'])
    return files


def _under_heading(text, heading, line):
    """text with `line` added at the end of the `heading` section (before
    the next # or ## heading outside a code fence), or with the section
    appended when the file has none."""
    lines = text.split('\n')
    fence = False
    start = end = None
    for i, l in enumerate(lines):
        if l.lstrip().startswith('```'):
            fence = not fence
            continue
        if fence:
            continue
        if start is None:
            if l.rstrip() == heading:
                start = i
        elif re.match(r'#{1,2} ', l):
            end = i
            break
    if start is None:
        body = text.rstrip('\n')
        return (body + '\n\n' if body else '') + heading + '\n\n' + line + '\n'
    if end is None:
        end = len(lines)
    section = lines[start + 1:end]
    while section and not section[-1].strip():
        section.pop()
    rest = lines[end:]
    out = '\n'.join(lines[:start + 1] + section + ['', line]
                    + ([''] + rest if rest else []))
    return out if out.endswith('\n') else out + '\n'


def write_triage_evidence(worktree, address, cls, fork_base):
    """Add the auto verdict's line, `disagreed with WSO at <fork_base>:
    <cls>; case <CASE_IDS[cls]> decides`, under '## WSO delta' in the
    row's evidence file in `worktree`, and commit that file alone on the
    worktree's branch.  The front matter (kind included) is untouched.  A
    file that already carries the line, committed, is left as it is.
    Returns None, or why it failed."""
    rel = _evidence_rel(address)
    path = os.path.join(worktree, rel)
    line = (f'{EVIDENCE_PHRASES[0]} at {fork_base}: {cls}; '
            f'case {_triage().CASE_IDS[cls]} decides')
    try:
        with open(path, encoding='utf-8') as f:
            text = f.read()
        if line not in text:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(_under_heading(text, WSO_DELTA_HEAD, line))
    except OSError as e:
        return f'cannot edit {rel}: {e}'

    def git(*args):
        return subprocess.run(['git', '-C', worktree, *args],
                              capture_output=True, text=True)

    r = git('add', '--', rel)
    if r.returncode != 0:
        return f'git add {rel} failed: {r.stderr.strip()}'
    if git('diff', '--cached', '--quiet', '--', rel).returncode == 0:
        return None
    r = git('commit', '-q', '-m', f'triage: {cls} verdict for {address}',
            '--', rel)
    if r.returncode != 0:
        return f'git commit of {rel} failed: {(r.stderr or r.stdout).strip()}'
    return None


def apply_triage(state_path, repo, batch_id, triage_path, fork_base,
                 import_path=None):
    """Triage one assigned batch from triage.json.

    A row's class is triage.json's auto_class when present, else its
    class.  The enabled classes come from triage.calibrate over the same
    entries and the state's closed batches, so the gate re-evaluates as
    batches close.  A row of an enabled class whose evidence file exists
    in the batch worktree gets write_triage_evidence (committed first),
    then an `ours` verdict with case CASE_IDS[class], and keeps its class.
    An ours-stub row leaves the batch (its files are recomputed) and is
    appended to the import list (default <repo>/docs/wso/import-list.json,
    created when missing).  Every other row stays for a worker, classed
    'ambiguous'; the message says why for each row and ends with
    `triage: n auto, n import, n ambiguous`.  Exit 0, 1 when an evidence
    commit failed or the batch is not assigned, 2 on bad input."""
    tri = _triage()
    entries, err = _load_list(triage_path, 'triage file')
    if err:
        return 2, err
    if not all('class' in e for e in entries):
        return 2, f'triage file {triage_path}: a row has no class\n'
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch['status'] != 'assigned':
        return 1, (f'batch {batch_id} is {batch["status"]}; triage is for '
                   f'assigned batches\n')
    if import_path is None:
        import_path = os.path.join(repo, IMPORT_LIST_REL)
    imported, err = _load_list(import_path, 'import list', missing_ok=True)
    if err:
        return 2, err
    enabled = tri.enabled_classes(tri.calibrate(state, entries))
    by_addr = {_addr_key(e['address']): e for e in entries}
    branch = batch['branch']
    wt = worktree_for(repo, branch)
    lines = [f'triage {batch_id} (calibration enables: '
             f'{", ".join(sorted(enabled)) or "none"}):']
    classes = {}
    moved = []
    failed = False
    for row in batch['rows']:
        addr = row['address']
        if addr in batch['verdicts']:
            classes[addr] = row.get('class', 'ambiguous')
            lines.append(f'  {addr}: already has a verdict')
            continue
        entry = by_addr.get(_addr_key(addr))
        if entry is None:
            classes[addr] = 'ambiguous'
            lines.append(f'  {addr}: no triage entry; left for a worker')
            continue
        cls = entry.get('auto_class', entry['class'])
        rel = _evidence_rel(addr)
        if cls == 'ours-stub':
            moved.append(row)
            lines.append(f'  {addr} ours-stub: moved to the import list')
            continue
        if cls not in tri.AUTO_OURS:
            why = 'left for a worker'
        elif cls not in enabled:
            why = 'class not enabled by calibration; left for a worker'
        elif wt is None:
            why = f'no worktree for {branch}; left for a worker'
        elif not os.path.isfile(os.path.join(wt, rel)):
            why = f'no evidence file {rel}; left for a worker'
        else:
            case = tri.CASE_IDS[cls]
            err = write_triage_evidence(wt, addr, cls, fork_base)
            if err is None:
                rc, out = record_verdict(state_path, repo, batch_id, addr,
                                         'ours', case)
                if rc != 0:
                    err = out.strip()
            if err is None:
                classes[addr] = cls
                lines.append(f'  {addr} {cls}: auto verdict ours, case {case}')
                continue
            failed = True
            why = f'{err}; left for a worker'
        classes[addr] = 'ambiguous'
        lines.append(f'  {addr} {cls}: {why}')
    # The import list first: a crash between the two writes leaves the row
    # in both places (the next run de-duplicates), never in neither.
    if moved:
        have = {_addr_key(e['address']) for e in imported}
        imported += [{'address': r['address'], 'file': r['file'],
                      'name': r['name'], 'source': 'triage'}
                     for r in moved if _addr_key(r['address']) not in have]
        _save_json(import_path, imported)
    # record_verdict wrote the state; re-read it before the class update.
    state = load_state(state_path)
    batch = state['batches'][batch_id]
    gone = {r['address'] for r in moved}
    batch['rows'] = [r for r in batch['rows'] if r['address'] not in gone]
    for r in batch['rows']:
        r['class'] = classes.get(r['address'], r.get('class', 'ambiguous'))
    batch['files'] = _files_of(batch['rows'])
    save_state(state_path, state)
    auto = sum(1 for r in batch['rows'] if r['class'] in tri.AUTO_OURS)
    lines.append(f'triage: {auto} auto, {len(moved)} import, '
                 f'{len(batch["rows"]) - auto} ambiguous')
    return (1 if failed else 0), '\n'.join(lines) + '\n'


# -- brief --------------------------------------------------------------

def render_brief(template_path=TEMPLATE_PATH, state_path=STATE_PATH,
                 batch_id=None, bundle_dir=None, bundle_fn=_assemble_bundle):
    """Fill brief-template.md for one batch; the coordinator dispatches
    the result verbatim.  With bundle_dir, a context bundle is written
    per row the worker must decide (class ambiguous, or no class and no
    verdict yet) and {{bundle}} lists the paths."""
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    try:
        with open(template_path, encoding='utf-8') as f:
            template = f.read()
    except OSError as e:
        return 2, f'no template {template_path}: {e}\n'
    rows = '\n'.join(
        f'{i}. `{r["address"]}` **{r["name"]}** ({r["file"]}, '
        f'theirs {r["their_len"]} / ours {r["our_len"]} chars)'
        for i, r in enumerate(batch['rows'], 1))
    files = '\n'.join(f'- `{f}`' for f in batch['files'])
    fork_base = str(state.get('fork_base') or DEFAULT_FORK_BASE)
    bundle = '(no bundles)'
    if bundle_dir:
        decided = batch.get('verdicts', {})
        paths = []
        for r in batch['rows']:
            cls = r.get('class')
            if cls != 'ambiguous' and not (
                    cls is None and r['address'] not in decided):
                continue
            os.makedirs(bundle_dir, exist_ok=True)
            path = os.path.join(bundle_dir, f'{r["address"]}.md')
            with open(path, 'w', encoding='utf-8') as f:
                f.write(bundle_fn(int(r['address'], 16)))
            paths.append(f'- `{path}`')
        bundle = '\n'.join(paths) or '(no bundles)'
    brief = (template
             .replace('{{id}}', batch_id)
             .replace('{{branch}}', batch['branch'])
             .replace('{{fork_base}}', fork_base)
             .replace('{{size}}', str(len(batch['rows'])))
             .replace('{{files}}', files)
             .replace('{{rows}}', rows)
             .replace('{{bundle}}', bundle))
    return 0, brief


# -- verdict ------------------------------------------------------------

def worktree_for(repo, branch):
    """The checked-out path of `branch`, or None (git worktree list)."""
    r = subprocess.run(['git', '-C', repo, 'worktree', 'list', '--porcelain'],
                       capture_output=True, text=True)
    want = f'refs/heads/{branch}'
    current = found = None
    for line in r.stdout.splitlines():
        if line.startswith('worktree '):
            current = line[len('worktree '):]
        elif line.startswith('branch ') and line[len('branch '):] == want:
            found = current
    return found


def _read_evidence(repo, branch, address):
    """The evidence file's text: the branch's worktree copy when one is
    checked out (it may hold uncommitted work), else the branch tip."""
    rel = _evidence_rel(address)
    wt = worktree_for(repo, branch)
    if wt:
        path = os.path.join(wt, rel)
        if os.path.isfile(path):
            with open(path, encoding='utf-8') as f:
                return f.read(), None
    r = subprocess.run(['git', '-C', repo, 'show', f'{branch}:{rel}'],
                       capture_output=True, text=True)
    if r.returncode == 0:
        return r.stdout, None
    return None, (f'no evidence file {rel} on {branch}: write it '
                  f'({EVIDENCE_DIR}/<ADDRESS>.md), commit it, then re-run '
                  f'the verdict')


def _evidence_kind(repo, branch, address):
    text, err = _read_evidence(repo, branch, address)
    if text is None:
        return None, err
    m = KIND_RE.search(text)
    if not m:
        return None, (f'{_evidence_rel(address)} on {branch}: no kind: in '
                      f'the front matter')
    return m.group(1), ''


def record_verdict(state_path=STATE_PATH, repo=ROOT, batch_id=None,
                   address=None, side=None, case=None):
    """Record one row's outcome.  `side` says who won: fork (his body is
    adopted, ours' verification is a trace), ours (his verification wins,
    ours' evidence kind is the losing kind), neither (parser noise)."""
    if side not in SIDES:
        return 2, f'side must be one of {"|".join(SIDES)}\n'
    if not case:
        return 2, 'the deciding case id is required (probe-N, replay checkpoint, ...)\n'
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch['status'] != 'assigned':
        return 1, (f'batch {batch_id} is {batch["status"]}; verdicts are '
                   f'for assigned batches\n')
    row = next((r for r in batch['rows']
                if _addr_key(r.get('address')) == _addr_key(address)), None)
    if row is None:
        return 1, f'{address} is not one of batch {batch_id}\'s rows\n'
    losing = None
    if side == 'ours':
        losing = 'trace'   # his evidence is always a trace of the exe
    elif side == 'fork':
        losing, err = _evidence_kind(repo, batch['branch'], row['address'])
        if losing is None:
            return 1, err + '\n'
    batch['verdicts'][row['address']] = {
        'side': side, 'case': case, 'losing_kind': losing}
    save_state(state_path, state)
    kind = f', losing kind: {losing}' if losing else ''
    return 0, f'verdict {batch_id} {row["address"]} {side} (case {case}{kind})\n'


# -- close ---------------------------------------------------------------

def _tally(batch):
    sides = {s: 0 for s in SIDES}
    kinds = {}
    for v in batch.get('verdicts', {}).values():
        sides[v['side']] += 1
        k = v.get('losing_kind')
        if k:
            kinds[k] = kinds.get(k, 0) + 1
    return sides, kinds


def _tally_line(batch):
    sides, kinds = _tally(batch)
    if not any(sides.values()):
        return ''   # an import wave: no verdicts to tally
    ks = ', '.join(f'{k} {kinds[k]}' for k in DISPLAY_ORDER if k in kinds)
    line = (f' -- fork {sides["fork"]}, ours {sides["ours"]}, '
            f'neither {sides["neither"]}')
    if ks:
        line += f'; losing kinds: {ks}'
    return line


def _fast_forward(repo, base, branch):
    """Move `base` to `branch`, only as a fast-forward, atomically."""
    def rev(ref):
        r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify', ref],
                           capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else None
    old, new = rev(base), rev(branch)
    if old is None:
        return f'{base} not found in {repo}'
    if new is None:
        return f'{branch} not found in {repo}'
    if old == new:
        return None
    r = subprocess.run(['git', '-C', repo, 'merge-base', '--is-ancestor',
                        base, branch], capture_output=True, text=True)
    if r.returncode != 0:
        return f'not a fast-forward: {base} is not an ancestor of {branch}'
    r = subprocess.run(['git', '-C', repo, 'update-ref',
                        f'refs/heads/{base}', new, old],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return f'update-ref failed: {r.stderr.strip()}'
    return None


def close_batch(state_path=STATE_PATH, repo=ROOT, batch_id=None,
                kind='verdicts', base='first', gate=True):
    """Close a batch: every reason checked, one refusal naming them all;
    on success fast-forward `base` to the batch branch.  kind='verdicts'
    (wave 1) requires a verdict and a phrase-bearing evidence file per
    row; kind='import' (waves 2-5) requires evidence kind trace or
    better instead.  A branch that modified tools/gates.sh or tools/parity
    is refused outright -- it would be certified by its own gate.  The
    gate runs only when everything else is clean: it costs minutes.
    gate=False skips tools/gates.sh (tests only)."""
    state = load_state(state_path)
    batch = state['batches'].get(batch_id or '')
    if batch is None:
        return 2, f'no batch {batch_id}\n'
    if batch['status'] == 'closed':
        return 1, (f'batch {batch_id} is already closed '
                   f'({batch.get("closed_at")})\n')
    branch = batch['branch']
    problems = []
    wt = worktree_for(repo, branch)
    if wt is None:
        r = subprocess.run(['git', '-C', repo, 'rev-parse', '--verify', branch],
                           capture_output=True, text=True)
        if r.returncode == 0:
            problems.append(f'{branch} exists but has no worktree '
                            f'(git worktree add <path> {branch})')
        else:
            problems.append(f'branch {branch} does not exist yet')
    if kind == 'verdicts':
        missing = [r['address'] for r in batch['rows']
                   if r['address'] not in batch.get('verdicts', {})]
        problems += [f'no verdict for {a}' for a in missing]
    if wt is not None:
        st = subprocess.run(['git', '-C', wt, 'status', '--porcelain', '-uall'],
                            capture_output=True, text=True)
        dirty = [l for l in st.stdout.splitlines() if l.strip()]
        if dirty:
            problems.append(f'uncommitted paths in {wt}:')
            problems += ['  ' + l for l in dirty[:20]]
        d = subprocess.run(
            ['git', '-C', repo, 'diff', '--name-only', f'{base}..{branch}',
             '--', *GATE_PATHS], capture_output=True, text=True)
        changed = [l for l in d.stdout.splitlines() if l.strip()]
        if changed:
            problems.append(f'these paths differ from {base}; a batch must '
                            f'not change the gate that certifies it:')
            problems += ['  ' + p for p in changed]
        for row in batch['rows']:
            rel = _evidence_rel(row['address'])
            path = os.path.join(wt, rel)
            text = None
            if os.path.isfile(path):
                with open(path, encoding='utf-8') as f:
                    text = f.read()
            if text is None:
                problems.append(f'no evidence file {rel}')
                continue
            if kind == 'verdicts':
                if not any(p in text for p in EVIDENCE_PHRASES):
                    problems.append(
                        f'{rel}: neither {" nor ".join(EVIDENCE_PHRASES)} '
                        f'in the body')
            else:
                m = KIND_RE.search(text)
                have = m.group(1) if m else 'none'
                if have not in IMPORT_KINDS:
                    problems.append(f'{rel}: kind {have} is not trace or '
                                    f'better')
    if gate and not problems and wt is not None:
        g = subprocess.run(['bash', 'tools/gates.sh', '--base', base],
                           cwd=wt, capture_output=True, text=True)
        if g.returncode != 0:
            problems.append(f'tools/gates.sh --base {base} failed in {wt} '
                            f'(exit {g.returncode}):')
            problems += ['  ' + l
                         for l in g.stdout.splitlines()[-10:]]
    if problems:
        return 1, (f'batch {batch_id} not closed:\n'
                   + '\n'.join('  - ' + p for p in problems) + '\n')
    err = _fast_forward(repo, base, branch)
    if err:
        return 1, f'batch {batch_id} not closed:\n  - {err}\n'
    batch['status'] = 'closed'
    batch['closed_at'] = _now()
    save_state(state_path, state)
    new = subprocess.run(['git', '-C', repo, 'rev-parse', base],
                         capture_output=True, text=True).stdout.strip()
    return 0, (f'{batch_id} closed: {len(batch["rows"])} rows'
               f'{_tally_line(batch)}\n{base} fast-forwarded to {new} '
               f'({branch})\n')


# -- scorecard -----------------------------------------------------------

SCORECARD_HEAD = '## Scorecard'
# How a verdict was measured (verdict['measured'], default none).
MEASURED_ORDER = ('probe', 'frida', 'replay', 'none')
# Deciding-evidence buckets, in display order -- strongest verification
# first, the way DISPLAY_ORDER orders the losing kinds; triage (an auto
# verdict from the two bodies' text) comes last.  A verdict's free-form
# case id picks exactly one, by deciding_kind(); `losing_kind` is a
# different thing (the loser's kind, not what decided).
DECIDING_ORDER = ('oracle probe', 'replay checkpoint', 'dump reading',
                  'trace', 'triage')
# Bare case ids that name a replay scenario: b005 decided rows with the
# scenario name alone; later replay cases are scenario/checkpoint, which
# the / rule already catches.  Membership is exact, and the classifier
# stays a pure function -- no probing scenarios/ on disk.
LEGACY_REPLAY_CASES = frozenset({'world_entry'})


def deciding_kind(case):
    """The evidence kind that decided a verdict, from its case id:
    triage-* (apply_triage's auto verdicts) triage, probe-N an oracle
    probe, replay-*, any scenario/checkpoint id (a / in it) or a bare
    legacy scenario name a replay checkpoint, dump-N / readout-ADDR a
    dump reading, anything else (frida-* included) a trace."""
    c = case or ''
    if c.startswith('triage-'):
        return 'triage'
    if c.startswith('probe'):
        return 'oracle probe'
    if (c.startswith('replay') or '/' in c
            or c in LEGACY_REPLAY_CASES):
        return 'replay checkpoint'
    if c.startswith('dump-') or c.startswith('readout-'):
        return 'dump reading'
    return 'trace'


def scorecard_text(state):
    """The '## Scorecard' section: per-side tally, split by losing kind
    and by deciding evidence kind, plus one line per batch."""
    sides = {s: 0 for s in SIDES}
    kinds = {s: {} for s in SIDES}
    deciding = {s: {} for s in SIDES}
    every = {}
    measured = {s: {} for s in SIDES}
    total = 0
    batches = []
    for bid, b in sorted(state['batches'].items()):
        for v in b.get('verdicts', {}).values():
            total += 1
            sides[v['side']] += 1
            k = v.get('losing_kind')
            if k:
                kinds[v['side']][k] = kinds[v['side']].get(k, 0) + 1
            d = deciding_kind(v.get('case', ''))
            deciding[v['side']][d] = deciding[v['side']].get(d, 0) + 1
            every[d] = every.get(d, 0) + 1
            m = v.get('measured', 'none')
            measured[v['side']][m] = measured[v['side']].get(m, 0) + 1
        batches.append(f'{bid} {b["status"]} ({len(b["rows"])} rows)')

    def split(side):
        ks = kinds[side]
        if not ks:
            return ''
        return (' (losing kind: '
                + ', '.join(f'{k} {ks[k]}' for k in DISPLAY_ORDER if k in ks)
                + ')')

    def by_deciding(side):
        ks = deciding[side]
        if not sides[side] or not ks:
            return ''
        return (' -- deciding evidence: '
                + ', '.join(f'{k} {ks[k]}' for k in DECIDING_ORDER if k in ks))

    def by_measured(side):
        if not sides[side]:
            return ''
        return ('; measured: '
                + ', '.join(f'{k} {measured[side].get(k, 0)}'
                            for k in MEASURED_ORDER))

    lines = [
        SCORECARD_HEAD,
        '',
        'Per-side tally across every batch in docs/wso/wave-state.json, '
        'split by deciding evidence kind (regenerate with '
        '`python3 tools/wave.py scorecard`):',
        '',
        f'Decided for the fork: {sides["fork"]}{split("fork")}'
        f'{by_deciding("fork")}{by_measured("fork")}',
        f'Decided for ours: {sides["ours"]}{split("ours")}'
        f'{by_deciding("ours")}{by_measured("ours")}',
        f'Decided for neither: {sides["neither"]}{split("neither")}'
        f'{by_deciding("neither")}{by_measured("neither")}',
        f'Deciding evidence across all {total} verdicts: '
        + ', '.join(f'{k} {every.get(k, 0)}' for k in DECIDING_ORDER),
    ]
    if batches:
        lines += ['', 'Batches: ' + ', '.join(batches)]
    return '\n'.join(lines) + '\n'


def write_scorecard(state_path=STATE_PATH, report_path=REPORT_PATH,
                    triage_path=TRIAGE_PATH):
    """Replace (or append) the '## Scorecard' section of the report.
    Idempotent: the same state writes the same bytes.  When triage.json
    exists the section ends with a labelled calibration block (the
    separate '## Triage calibration' section stays triage.py's)."""
    state = load_state(state_path)
    section = scorecard_text(state)
    if os.path.exists(triage_path):
        rows, err = _load_list(triage_path, 'triage')
        if err:
            return 2, err
        tri = _triage()
        section += ('\nCalibration, computed from triage.json classes over '
                    'closed verdicts:\n\n'
                    + tri.calibration_text(tri.calibrate(state, rows)))
        if not section.endswith('\n'):
            section += '\n'
    try:
        with open(report_path, encoding='utf-8') as f:
            text = f.read()
    except FileNotFoundError:
        text = ''
    head = text.find(SCORECARD_HEAD)
    if head != -1:
        nxt = text.find('\n## ', head + 1)
        body = (text[:head] + section if nxt == -1
                else text[:head] + section + '\n' + text[nxt + 1:])
    elif text.strip():
        body = text.rstrip('\n') + '\n\n' + section
    else:
        body = section
    d = os.path.dirname(os.path.abspath(report_path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.wave-', suffix='.md')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            f.write(body)
        os.replace(tmp, report_path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise
    n = sum(len(b.get('verdicts', {})) for b in state['batches'].values())
    return 0, f'wrote {report_path}: {SCORECARD_HEAD} over {n} verdicts\n'


# -- status / main -------------------------------------------------------

def status(state_path=STATE_PATH):
    state = load_state(state_path)
    if not state['batches']:
        return 0, 'no batches\n'
    lines = []
    for bid, b in sorted(state['batches'].items()):
        v = len(b.get('verdicts', {}))
        n = len(b['rows'])
        extra = f', {v}/{n} verdicts' if b['status'] == 'assigned' else ''
        lines.append(f'{bid} {b["status"]} {b["branch"]} {n} rows{extra}')
    return 0, '\n'.join(lines) + '\n'


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__.strip())
        return 0 if argv else 2
    cmd, rest = argv[0], argv[1:]
    if cmd == 'next':
        size, category = DEFAULT_SIZE, DEFAULT_CATEGORY
        it = iter(rest)
        for a in it:
            if a == '--size':
                try:
                    size = int(next(it))
                except (StopIteration, ValueError):
                    print('wave next: --size needs a number', file=sys.stderr)
                    return 2
            elif a == '--category':
                try:
                    category = next(it)
                except StopIteration:
                    print('wave next: --category needs a value',
                          file=sys.stderr)
                    return 2
            else:
                print(f'wave next: unknown option {a}', file=sys.stderr)
                return 2
        rc, out = next_batch(size=size, category=category,
                             triage_path=TRIAGE_PATH, repo=ROOT)
    elif cmd == 'brief':
        if len(rest) != 1:
            print('usage: wave.py brief <batch-id>', file=sys.stderr)
            return 2
        rc, out = render_brief(batch_id=rest[0])
    elif cmd == 'verdict':
        if len(rest) != 4:
            print('usage: wave.py verdict <batch-id> <address> '
                  'fork|ours|neither <case>', file=sys.stderr)
            return 2
        rc, out = record_verdict(batch_id=rest[0], address=rest[1],
                                 side=rest[2], case=rest[3])
    elif cmd == 'close':
        kind, args = 'verdicts', []
        it = iter(rest)
        for a in it:
            if a == '--kind':
                kind = next(it, 'import')
            else:
                args.append(a)
        if len(args) != 1:
            print('usage: wave.py close <batch-id> [--kind import]',
                  file=sys.stderr)
            return 2
        rc, out = close_batch(batch_id=args[0], kind=kind)
    elif cmd == 'scorecard':
        rc, out = write_scorecard()
    elif cmd == 'status':
        rc, out = status()
    elif cmd == 'run':
        return _driver().main(rest)
    else:
        print(f'wave.py: unknown command {cmd}', file=sys.stderr)
        return 2
    sys.stdout.write(out)
    return rc


if __name__ == '__main__':
    sys.exit(main())
