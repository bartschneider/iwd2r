#!/usr/bin/env python3
"""wave.py -- drive the disagreement batches and the wave closes.

The coordinator's wave loop, one command each::

    python3 tools/wave.py next                 # pick the next batch
    python3 tools/wave.py brief b001           # the text to dispatch verbatim
    python3 tools/wave.py verdict b001 0x643CC0 ours probe-9
    python3 tools/wave.py close b001           # gates, then ff `first`
    python3 tools/wave.py close b002 --kind import   # waves 2-5
    python3 tools/wave.py scorecard            # the tally into xrec-report.md
    python3 tools/wave.py status

Rows come from docs/wso/disagreements.json (tools/xrec.py); batch state
lives in docs/wso/wave-state.json, created by the first `next`.  Nothing
is written by hand: `next` picks the rows, `close` checks the batch's
worktree itself (verdicts, evidence lines, clean tree, an unmodified
gate, gates), then fast-forwards `first` to the batch branch.

Exit codes: 0 ok, 1 refusal (close lists every reason), 2 usage error or
nothing left to assign.  Every state write is atomic (temp file in the
same directory, then os.replace), so a killed `next` cannot corrupt it.
"""
from __future__ import annotations

import datetime
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


def save_state(path, state):
    d = os.path.dirname(os.path.abspath(path)) or '.'
    os.makedirs(d, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=d, prefix='.wave-', suffix='.json')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as f:
            json.dump(state, f, indent=2, sort_keys=True)
            f.write('\n')
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


# -- next ---------------------------------------------------------------

def next_batch(disagreements_path=DISAGREEMENTS_PATH, state_path=STATE_PATH,
               size=DEFAULT_SIZE, category=DEFAULT_CATEGORY):
    """Rank the unclaimed rows, take the biggest `size`, record the batch."""
    with open(disagreements_path, encoding='utf-8') as f:
        rows = json.load(f)
    state = load_state(state_path)
    decided = set()
    owned = set()
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
    files = []
    for r in chosen:
        if r['file'] not in files:
            files.append(r['file'])
    state['batches'][batch_id] = {
        'rows': chosen,
        'files': files,
        'branch': f'p3/wso-{batch_id}',
        'status': 'assigned',
        'assigned_at': _now(),
        'closed_at': None,
        'verdicts': {},
    }
    save_state(state_path, state)
    lines = [f'{batch_id} assigned: {len(chosen)} rows, {len(files)} files, '
             f'branch p3/wso-{batch_id}',
             f'files: {", ".join(files)}']
    for i, r in enumerate(chosen, 1):
        lines.append(f'{i:2}. {r["address"]}  {r["name"]}  {r["file"]}  '
                     f'theirs {r["their_len"]} / ours {r["our_len"]}')
    if no_addr:
        lines.append(f'(skipped {no_addr} row'
                     f'{"s" if no_addr != 1 else ""} without an address)')
    return 0, '\n'.join(lines) + '\n'


# -- brief --------------------------------------------------------------

def render_brief(template_path=TEMPLATE_PATH, state_path=STATE_PATH,
                 batch_id=None):
    """Fill brief-template.md for one batch; the coordinator dispatches
    the result verbatim."""
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
    # The fork is read by pinned sha, never by branch name (wso/main moves).
    fork_base = str(state.get('fork_base') or
                    'e8dd16a750966af53f24daaf49dcb4fb13594c13')
    brief = (template
             .replace('{{id}}', batch_id)
             .replace('{{branch}}', batch['branch'])
             .replace('{{fork_base}}', fork_base)
             .replace('{{size}}', str(len(batch['rows'])))
             .replace('{{files}}', files)
             .replace('{{rows}}', rows))
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
                kind='verdicts', base='first'):
    """Close a batch: every reason checked, one refusal naming them all;
    on success fast-forward `base` to the batch branch.  kind='verdicts'
    (wave 1) requires a verdict and a phrase-bearing evidence file per
    row; kind='import' (waves 2-5) requires evidence kind trace or
    better instead.  A branch that modified tools/gates.sh or tools/parity
    is refused outright -- it would be certified by its own gate.  The
    gate runs only when everything else is clean: it costs minutes."""
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
    if not problems and wt is not None:
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
# Deciding-evidence buckets, in display order -- strongest verification
# first, the way DISPLAY_ORDER orders the losing kinds.  A verdict's
# free-form case id picks exactly one, by deciding_kind(); `losing_kind`
# is a different thing (the loser's kind, not what decided).
DECIDING_ORDER = ('oracle probe', 'replay checkpoint', 'dump reading',
                  'trace')
# Bare case ids that name a replay scenario: b005 decided rows with the
# scenario name alone; later replay cases are scenario/checkpoint, which
# the / rule already catches.  Membership is exact, and the classifier
# stays a pure function -- no probing scenarios/ on disk.
LEGACY_REPLAY_CASES = frozenset({'world_entry'})


def deciding_kind(case):
    """The evidence kind that decided a verdict, from its case id:
    probe-N an oracle probe, replay-*, any scenario/checkpoint id
    (a / in it) or a bare legacy scenario name a replay checkpoint,
    dump-N / readout-ADDR a dump reading, anything else a trace."""
    c = case or ''
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

    lines = [
        SCORECARD_HEAD,
        '',
        'Per-side tally across every batch in docs/wso/wave-state.json, '
        'split by deciding evidence kind (regenerate with '
        '`python3 tools/wave.py scorecard`):',
        '',
        f'Decided for the fork: {sides["fork"]}{split("fork")}'
        f'{by_deciding("fork")}',
        f'Decided for ours: {sides["ours"]}{split("ours")}'
        f'{by_deciding("ours")}',
        f'Decided for neither: {sides["neither"]}{split("neither")}'
        f'{by_deciding("neither")}',
        f'Deciding evidence across all {total} verdicts: '
        + ', '.join(f'{k} {every.get(k, 0)}' for k in DECIDING_ORDER),
    ]
    if batches:
        lines += ['', 'Batches: ' + ', '.join(batches)]
    return '\n'.join(lines) + '\n'


def write_scorecard(state_path=STATE_PATH, report_path=REPORT_PATH):
    """Replace (or append) the '## Scorecard' section of the report.
    Idempotent: the same state writes the same bytes."""
    state = load_state(state_path)
    section = scorecard_text(state)
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
        rc, out = next_batch(size=size, category=category)
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
    else:
        print(f'wave.py: unknown command {cmd}', file=sys.stderr)
        return 2
    sys.stdout.write(out)
    return rc


if __name__ == '__main__':
    sys.exit(main())
