#!/usr/bin/env python3
"""Run GLM workers headless over wave batches.

The worker protocol, a fake worker, the report parser, the credit
estimate, the mechanical checks and the review policy, the ledger
template and the batch loop (`wave.py run`).  The driver itself uses no
model."""
import argparse
import collections
import datetime
import json
import importlib.util
import os
import random
import re
import subprocess
import sys
import time

Result = collections.namedtuple(
    'Result', 'exit_code report transcript credits rate_limited')

# (input, cached, output) credits per 10000 tokens.
FLASH_RATES = (2.3, 0.56, 8.0)
GLM53_RATES = (6.9, 1.7, 24.0)

VERDICT_RE = re.compile(
    r'^verdict (0[xX][0-9a-fA-F]+) (ours|fork|neither) case (\S+)$', re.IGNORECASE)
MARKER_RE = re.compile(r'^(?:[-*]|\d+\.)\s+')


class ReportError(ValueError):
    pass


def parse_report(text):
    """Parse the '## Verdicts' section into {addr: (side, case)}.

    Addresses are normalised to '0x' + uppercase hex without leading zeros."""
    out = {}
    in_section = False
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith('#'):
            in_section = line.lstrip('#').strip().lower() == 'verdicts'
            continue
        if not in_section or not line:
            continue
        if re.fullmatch(r'```[^`]*', line):
            continue
        cand = MARKER_RE.sub('', line, count=1).strip().strip('`').strip()
        m = VERDICT_RE.match(cand)
        if not m:
            raise ReportError('line %d: malformed verdict line: %r' % (n, raw))
        addr = '0x%X' % int(m.group(1), 16)
        side, case = m.group(2).lower(), m.group(3)
        if addr in out:
            raise ReportError('line %d: duplicate verdict for %s' % (n, addr))
        out[addr] = (side, case)
    return out


def _rates(model):
    """Rates for a model name, or None for a non-glm model (skipped)."""
    model = re.sub(r'\[1m\]$', '', model or '')
    if 'flash' in model:
        return FLASH_RATES
    if model.startswith('glm'):
        return GLM53_RATES
    return None


def credits_of(path):
    """Sum usage in a transcript, once per message.id (last line wins).
    Lines are data, never executed."""
    by_id = {}
    anon = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            msg = obj.get('message') if isinstance(obj, dict) else None
            usage = msg.get('usage') if isinstance(msg, dict) else None
            if not isinstance(usage, dict):
                continue
            mid = msg.get('id')
            if isinstance(mid, str) and mid:
                by_id[mid] = (msg.get('model'), usage)
            else:
                anon.append((msg.get('model'), usage))
    total = 0.0
    for model, usage in list(by_id.values()) + anon:
        rates = _rates(model)
        if rates is None:
            continue
        inp = (usage.get('input_tokens') or 0) + \
            (usage.get('cache_creation_input_tokens') or 0)
        total += (inp * rates[0]
                  + (usage.get('cache_read_input_tokens') or 0) * rates[1]
                  + (usage.get('output_tokens') or 0) * rates[2]) / 10000
    return total


def project_slug(cwd):
    return re.sub(r'[^A-Za-z0-9]', '-', os.path.abspath(cwd))


class FakeWorker:
    def __init__(self, report, exit_code=0, rate_limited=False):
        self.report = report
        self.exit_code = exit_code
        self.rate_limited = rate_limited

    def run(self, brief, cwd):
        return Result(self.exit_code, self.report, None, None, self.rate_limited)


class HeadlessWorker:
    def __init__(self, model='glm-5.3-flash[1m]', max_turns=400,
                 wrapper='claude-glm', config_dir=None, run=subprocess.run):
        self.model = model
        self.max_turns = max_turns
        self.wrapper = wrapper
        self._run = run
        if config_dir is None:
            if os.environ.get('CLAUDE_GLM_CONFIG_DIR'):
                config_dir = os.environ['CLAUDE_GLM_CONFIG_DIR']
            elif os.path.basename(wrapper) == 'claude-glm':
                config_dir = os.path.expanduser('~/.claude-glm')
            else:
                config_dir = os.path.expanduser('~/.claude')
        self.config_dir = config_dir

    def command(self, brief):
        argv = [self.wrapper, '-p', brief, '--output-format', 'json',
                '--dangerously-skip-permissions', '--max-turns', str(self.max_turns)]
        env = dict(os.environ)
        env['CLAUDE_GLM_MODEL'] = self.model
        return argv, env

    def transcript_path(self, cwd, session_id):
        return os.path.join(self.config_dir, 'projects', project_slug(cwd),
                            session_id + '.jsonl')

    def run(self, brief, cwd):
        argv, env = self.command(brief)
        proc = self._run(argv, cwd=cwd, env=env, capture_output=True, text=True)
        report, session_id = '', None
        exit_code = proc.returncode
        try:
            data = json.loads(proc.stdout)
            if isinstance(data, dict):
                report = data.get('result') or ''
                session_id = data.get('session_id')
        except ValueError:
            report = proc.stdout or ''
            exit_code = exit_code or 1
        transcript = credits = None
        if isinstance(session_id, str) and re.fullmatch(r'[A-Za-z0-9_-]+', session_id):
            path = self.transcript_path(cwd, session_id)
            if os.path.isfile(path):
                transcript = path
                credits = credits_of(path)
        return Result(exit_code, report, transcript, credits, False)


_TOOLS = {}


def _tool(name):
    """tools/<name>.py loaded by path: under -I tools/ is not on sys.path."""
    if name not in _TOOLS:
        spec = importlib.util.spec_from_file_location(
            'driver_' + name, os.path.join(
                os.path.dirname(os.path.abspath(__file__)), name + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _TOOLS[name] = module
    return _TOOLS[name]


def _vanilla_exe():
    return _tool('oracle').VANILLA_EXE


def _parity_python():
    """As tools/gates.sh: the parity venv when executable, else python3 -I."""
    home = os.environ.get('IWD2R_HOME') or os.path.expanduser('~/.local/share/iwd2r')
    venv = os.path.join(home, 'parity-venv', 'bin', 'python')
    if os.access(venv, os.X_OK):
        return [venv]
    return ['python3', '-I']


def _tail(proc, n=5):
    text = ((proc.stdout or '') + (proc.stderr or '')).strip()
    return ' | '.join(text.splitlines()[-n:])


def mechanical_checks(batch, worktree, verdicts, run=subprocess.run):
    """Problems that stop a batch before any review; [] when clean.

    Each problem says what failed and how to fix it (it is the worker's
    fix-round prompt)."""
    wave = _tool('wave')
    problems = []
    key = wave._addr_key
    rows = batch.get('rows', [])
    row_keys = {key(r['address']) for r in rows}
    given = {key(a): a for a in verdicts}
    for r in rows:
        if r.get('class') == 'ambiguous' or (
                'class' not in r and not r.get('verdict')):
            if key(r['address']) not in given:
                problems.append('%s: no verdict line in the report; add '
                                '"verdict <addr> ours|fork|neither case <id>" '
                                'under ## Verdicts' % r['address'])
    for k, a in given.items():
        if k not in row_keys:
            problems.append('%s: verdict for an address outside this batch; '
                            'remove that line' % a)
    for r in rows:
        k = key(r['address'])
        if k not in given:
            continue
        rel = '%s/%s.md' % (wave.EVIDENCE_DIR, k.upper())
        text, err = wave._read_evidence(worktree, batch['branch'], r['address'])
        if text is None:
            problems.append('%s: missing; write it and commit it' % rel)
        elif not any(p in text for p in wave.EVIDENCE_PHRASES):
            problems.append("%s: no 'disagreed with WSO' or 'corroborated by "
                            "WSO' phrase; add one of the exact phrases" % rel)
    steps = [
        (['python3', '-I', 'tests/test_tools.py'], 'tests/test_tools.py'),
        (['python3', '-I', 'tools/check_evidence.py', '--base', 'first'],
         'tools/check_evidence.py'),
    ]
    if os.path.isfile(_vanilla_exe()):
        steps.append((_parity_python() + ['tools/parity/run.py', '--base', 'first'],
                      'tools/parity/run.py'))
    for argv, label in steps:
        proc = run(argv, cwd=worktree, capture_output=True, text=True)
        if proc.returncode != 0:
            problems.append('%s failed (exit %s); last lines: %s; fix it and '
                            'rerun' % (label, proc.returncode, _tail(proc)))
    return problems


def review_needed(batch, verdicts, seed, sample=0.2):
    """(needed, reason): fork/neither rows always, else a seeded sample."""
    if any(side != 'ours' for side, _case in verdicts.values()):
        return True, 'fork/neither rows'
    batch_id = batch if isinstance(batch, str) else batch.get('id')
    if random.Random('%s:%s' % (seed, batch_id)).random() < sample:
        return True, 'sample'
    return False, ''


# -- the batch loop ------------------------------------------------------

# Relative to the repo root (ruling R29): one ledger per plan, and the
# driver folder is the status file's directory.
LEDGER_REL = os.path.join('.superpowers', 'sdd', '2026-10-17-wso-integration',
                          'progress.md')
STATUS_REL = os.path.join('.superpowers', 'sdd', '2026-10-17-wso-integration',
                          'driver', 'status.json')
DEFAULT_SEED = '2026-10-22'
PUSH_ARGS = ('push', 'iwd2r', 'first:main', 'first:first')
# Exceptions at these stages are not the batch's own: the run stops.
GLOBAL_STAGES = ('cut', 'push')
BLOCKING_RE = re.compile(r'^(?:[-*]|\d+\.)?\s*[*_`]*BLOCKING[*_`]*:')

FIX_PROMPT = """# Fix round for batch {id}

The driver's {stage} found the problems below in your work on `{branch}`
(this worktree). Fix every one, commit, and write your report again: the
full `## Verdicts` section, one line for every row you decide, not only
the rows you fixed.

{problems}

The original brief follows.

---

{brief}"""

REVIEW_BRIEF = """# Review of wave-1 batch {id}

You review batch {id} on branch `{branch}`; your cwd is its worktree.
Do not edit, commit or push anything.

The review package is `git diff {base}...{branch}`, written to:

    {diff}

The batch's verdicts (review reason: {why}):

{verdicts}

Check every fork and neither verdict against its deciding case and its
evidence file (`docs/evidence/<ADDRESS>.md`), and that every adopted fork
body carries its `Imported-From:` trailer. Put each finding that must be
fixed before the batch closes on its own line starting exactly
`BLOCKING:`, followed by the address, the file and what to change.
Anything else is advisory: write it without that prefix. When nothing
blocks, say so in one line.
"""


def _now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec='seconds')


def _one_line(text, limit=300):
    s = ' '.join(str(text).split())
    return s if len(s) <= limit else s[:limit - 3] + '...'


def _side_case(v):
    """(side, case) from a wave-state verdict dict or a report tuple."""
    if isinstance(v, dict):
        return v.get('side'), v.get('case') or ''
    return v[0], v[1]


def ledger_entry(batch, verdicts, classes, review, commits, credits, now=None,
                 ignored=None):
    """The templated progress.md entry for one batch.

    batch carries 'id'; verdicts maps address to a wave-state verdict or a
    (side, case) tuple; classes maps class to row count; review is
    (reviewed, outcome); credits None prints as 'credits unknown';
    ignored maps address to the (side, case) of a worker line set aside
    because the row was triage-decided (a line only when there are any)."""
    wave = _tool('wave')
    known = _tool('triage').CLASSES
    order = [c for c in known if c in classes] + sorted(
        c for c in classes if c not in known)
    sides = {s: 0 for s in wave.SIDES}
    deciding = {}
    for v in verdicts.values():
        side, case = _side_case(v)
        sides[side] = sides.get(side, 0) + 1
        k = wave.deciding_kind(case)
        deciding[k] = deciding.get(k, 0) + 1
    reviewed, outcome = review
    lines = [
        '## driver %s (%s)' % (batch['id'], now or _now()),
        '',
        '- rows by class: ' + (', '.join('%s %d' % (c, classes[c]) for c in order)
                               or 'none'),
        '- verdicts: fork %d, ours %d, neither %d' % (
            sides['fork'], sides['ours'], sides['neither']),
    ] + ([
        '- ignored for triage-decided rows: ' + ', '.join(
            '%s %s' % (a, _side_case(v)[0]) for a, v in sorted(ignored.items())),
    ] if ignored else []) + [
        '- deciding evidence: ' + ', '.join(
            '%s %d' % (k, deciding.get(k, 0)) for k in wave.DECIDING_ORDER),
        '- review: %s, %s' % ('yes' if reviewed else 'no', outcome),
        '- commits %s' % commits,
        '- credits unknown' if credits is None else '- credits %.0f' % credits,
    ]
    return '\n'.join(lines) + '\n'


def dry_report(batch):
    """The dry run's canned report: every row a worker must decide (class
    ambiguous, or no class and no verdict yet), all ours, case dry-<addr>."""
    decided = batch.get('verdicts', {})
    lines = ['# dry run', '', '## Verdicts', '']
    for r in batch.get('rows', []):
        cls = r.get('class')
        if cls == 'ambiguous' or (cls is None and r['address'] not in decided):
            lines.append('verdict %s ours case dry-%s' % (r['address'], r['address']))
    return '\n'.join(lines) + '\n'


def blocking_findings(report):
    """The reviewer's lines that start BLOCKING: (one list marker allowed)."""
    out = []
    for raw in (report or '').splitlines():
        line = raw.strip()
        if BLOCKING_RE.match(line):
            out.append(MARKER_RE.sub('', line, count=1).strip())
    return out


class Driver:
    """The batch loop: cut and triage, brief, worker, mechanical checks
    (one fix round), review when it can matter (one fix round), record,
    close, push, ledger.  A failure the driver cannot settle is an
    exception: written to the status file and the ledger, sent to the AoE
    session, and the loop goes on with the next batch.

    The wave functions are attributes, so tests replace them.  Batches run
    one at a time; `workers` is stored for the pools (ruling R30), and
    `budget` for the budget stop."""

    def __init__(self, repo, state_path, worker, reviewer, ledger_path,
                 status_path, aoe_session=None, seed=DEFAULT_SEED, budget=None,
                 workers=1, *, push=None, send=None, dry_run=False, gate=True,
                 base='first', log=None):
        wave = _tool('wave')
        self.repo = repo
        self.state_path = state_path
        self.worker = worker
        self.reviewer = reviewer
        self.ledger_path = ledger_path
        self.status_path = status_path
        self.driver_dir = os.path.dirname(os.path.abspath(status_path))
        self.aoe_session = aoe_session
        self.seed = seed
        self.budget = budget
        self.workers = workers
        self.dry_run = dry_run
        self.gate = gate
        self.base = base
        self.size = wave.DEFAULT_SIZE
        self.category = wave.DEFAULT_CATEGORY
        docs = os.path.join(repo, 'docs', 'wso')
        self.disagreements_path = os.path.join(docs, 'disagreements.json')
        self.triage_path = os.path.join(docs, 'triage.json')
        self.template_path = os.path.join(docs, 'brief-template.md')
        self.report_path = os.path.join(docs, 'xrec-report.md')
        self.next_batch = wave.next_batch
        self.render_brief = wave.render_brief
        self.record_verdict = wave.record_verdict
        self.close_batch = wave.close_batch
        self.write_scorecard = wave.write_scorecard
        self.worktree_for = wave.worktree_for
        self.load_state = wave.load_state
        self.mechanical_checks = mechanical_checks
        self.review_needed = review_needed
        self.push = self._git_push if push is None else push
        self.send = self._aoe_send if send is None else send
        self.log = (lambda m: print(m, flush=True)) if log is None else log
        self._read_status()   # a corrupt status file refuses here
        self._imported = {}
        self._runs = []
        self._ignored = {}
        self._halt = False

    # -- status, ledger, results ----------------------------------------

    def _read_status(self):
        """The status file as it is on disk now: the coordinator edits it
        (clearing an exception is how it asks for a retry)."""
        for attempt in range(3):
            try:
                with open(self.status_path, encoding='utf-8') as f:
                    status = json.load(f)
                break
            except FileNotFoundError:
                status = {}
                break
            except ValueError:
                if attempt == 2:
                    raise
                time.sleep(0.2)   # a torn read while the coordinator writes
        if not isinstance(status, dict):
            raise ValueError('%s: not a driver status file' % self.status_path)
        for key, empty in (('exceptions', {}), ('batches', {}),
                           ('current', None), ('parked', None),
                           ('stalled', None)):
            status.setdefault(key, empty)
        return status

    def _update_status(self, change):
        """Read-modify-write: re-read the file, apply only this change,
        write it atomically.  What the coordinator removed stays removed."""
        status = self._read_status()
        change(status)
        if self.dry_run:
            status['dry_run'] = True
        status['updated_at'] = _now()
        _tool('wave')._save_json(self.status_path, status)
        return status

    def _stage(self, batch_id, stage):
        current = {'batch': batch_id, 'stage': stage, 'at': _now()}
        self._update_status(lambda s: s.__setitem__('current', current))
        self.log('%s: %s' % (batch_id, stage))

    def _finish(self, batch_id, outcome, change=None, **extra):
        """Record the batch's outcome; `change` edits the status in the
        same write."""
        entry = dict(outcome=outcome, credits=self._credits(), at=_now(), **extra)

        def apply(s):
            s['batches'][batch_id] = entry
            s['current'] = None
            if change:
                change(s)
        self._update_status(apply)

    def _append_ledger(self, text):
        os.makedirs(os.path.dirname(os.path.abspath(self.ledger_path)),
                    exist_ok=True)
        sep = '\n' if os.path.isfile(self.ledger_path) and os.path.getsize(
            self.ledger_path) else ''
        with open(self.ledger_path, 'a', encoding='utf-8') as f:
            f.write(sep + text.rstrip('\n') + '\n')

    def _credits(self):
        if not self._runs or any(c is None for c in self._runs):
            return None
        return sum(self._runs)

    def _run(self, worker, brief, cwd, batch_id, label):
        """One worker run; the brief, report and result land in
        <driver folder>/<batch_id>/<label>.*"""
        res = worker.run(brief, cwd)
        self._runs.append(res.credits)
        folder = os.path.join(self.driver_dir, batch_id)
        os.makedirs(folder, exist_ok=True)
        for name, text in (('brief.md', brief), ('report.md', res.report or '')):
            with open(os.path.join(folder, '%s.%s' % (label, name)), 'w',
                      encoding='utf-8') as f:
                f.write(text)
        with open(os.path.join(folder, label + '.json'), 'w',
                  encoding='utf-8') as f:
            json.dump(res._asdict(), f, indent=2, sort_keys=True)
            f.write('\n')
        self.log('%s: %s exited %s, credits %s' % (
            batch_id, label, res.exit_code,
            'unknown' if res.credits is None else '%.1f' % res.credits))
        return res

    # -- outcomes ---------------------------------------------------------

    def _exception(self, batch_id, stage, problems):
        at = _now()
        problems = [str(p) for p in problems if str(p).strip()] or ['(no detail)']
        exc = {'stage': stage, 'problems': problems, 'at': at}
        self._finish(batch_id, 'exception', stage=stage,
                     change=lambda s: s['exceptions'].__setitem__(batch_id, exc))
        if stage in GLOBAL_STAGES:
            self._halt = True
        n = len(problems)
        line = ('driver %s: exception at %s (%s): %d problem%s, first: %s; '
                'the full list is in %s' % (
                    batch_id, stage, at, n, '' if n == 1 else 's',
                    _one_line(problems[0], 200), self.status_path))
        self._append_ledger(line)
        self.log(line)
        if not self.dry_run:
            try:
                self.send(_one_line('iwd2r driver: %s exception at %s: %s (status %s)'
                                    % (batch_id, stage, problems[0],
                                       self.status_path), 400))
            except Exception as e:   # a failed send must not stop the loop
                self.log('send failed: %r' % (e,))
        return 'exception'

    def _park(self, batch_id, reason):
        parked = {'batch_id': batch_id, 'at': _now(), 'reason': reason}
        self._finish(batch_id, 'parked',
                     change=lambda s: s.__setitem__('parked', parked))
        self.log('%s: parked (%s)' % (batch_id, reason))
        return 'parked'

    def _settle(self, batch_id, res, label):
        """'parked' or 'exception' when a run gave nothing to check."""
        if res.rate_limited:
            return self._park(batch_id, '%s rate limited' % label)
        if not (res.report or '').strip():
            return self._exception(
                batch_id, 'review' if label.startswith('reviewer') else 'worker',
                ['%s exited %s without a report' % (label, res.exit_code)])
        return None

    # -- the stages ---------------------------------------------------------

    def _check(self, batch_id, worktree, report):
        """(verdicts, problems) for a report: the parse, then the
        mechanical checks.  Lines for rows whose recorded verdict is a
        triage-* case are set aside in self._ignored (ruling R33): never
        recorded, never checked, never a reason to review."""
        self._ignored = {}
        try:
            parsed = parse_report(report)
        except ReportError as e:
            return {}, ['report: %s; fix that line under ## Verdicts (one '
                        '"verdict <0xADDR> <fork|ours|neither> case <id>" '
                        'per row)' % e]
        self._stage(batch_id, 'checks')
        batch = self.load_state(self.state_path)['batches'][batch_id]
        key = _tool('wave')._addr_key
        auto = {key(a) for a, v in batch.get('verdicts', {}).items()
                if _side_case(v)[1].startswith('triage-')}
        verdicts = {}
        for addr, sc in parsed.items():
            (self._ignored if key(addr) in auto else verdicts)[addr] = sc
        if self._ignored:
            self.log('%s: ignored for triage-decided rows: %s' % (
                batch_id, ', '.join(sorted(self._ignored))))
        return verdicts, self.mechanical_checks(batch, worktree, verdicts)

    def _merged(self, batch_id, verdicts):
        """The state's verdicts plus the report's, as (side, case)."""
        key = _tool('wave')._addr_key
        batch = self.load_state(self.state_path)['batches'][batch_id]
        out = {key(a): _side_case(v) for a, v in batch.get('verdicts', {}).items()}
        out.update({key(a): sc for a, sc in verdicts.items()})
        return out

    def _fix(self, batch_id, batch, worker, worktree, stage, problems, brief, label):
        prompt = FIX_PROMPT.format(
            id=batch_id, stage=stage, branch=batch['branch'], brief=brief,
            problems='\n'.join('- ' + p for p in problems))
        self._stage(batch_id, label)
        return self._run(worker, prompt, worktree, batch_id, label)

    def _review(self, batch_id, batch, worktree, merged, why, label):
        """(blocking findings, None) or (None, outcome)."""
        self._stage(batch_id, label)
        d = self._git('diff', '%s...%s' % (self.base, batch['branch']))
        if d.returncode != 0:
            return None, self._exception(batch_id, 'review', [
                'git diff %s...%s failed: %s' % (self.base, batch['branch'],
                                                 d.stderr.strip())])
        path = os.path.join(self.driver_dir, batch_id, 'review.diff')
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(d.stdout)
        brief = REVIEW_BRIEF.format(
            id=batch_id, branch=batch['branch'], base=self.base, diff=path,
            why=why, verdicts='\n'.join(
                '- verdict 0x%s %s case %s' % (k.upper(), side, case)
                for k, (side, case) in sorted(merged.items())))
        res = self._run(self.reviewer, brief, worktree, batch_id, label)
        out = self._settle(batch_id, res, label)
        if out:
            return None, out
        return blocking_findings(res.report), None

    def _classes(self, batch_id, batch):
        counts = {}
        for r in batch.get('rows', []):
            c = r.get('class') or 'unclassed'
            counts[c] = counts.get(c, 0) + 1
        moved = self._imported.get(batch_id)
        if moved:
            counts['ours-stub'] = counts.get('ours-stub', 0) + moved
        return counts

    def _git(self, *args):
        return subprocess.run(['git', '-C', self.repo, *args],
                              capture_output=True, text=True)

    def _commits(self, branch):
        """'<merge-base>..<tip> (n commits)' of the batch branch over base."""
        mb = self._git('merge-base', self.base, branch).stdout.strip()
        tip = self._git('rev-parse', '--verify', branch).stdout.strip()
        if not mb or not tip:
            return 'unknown (%s..%s)' % (self.base, branch)
        n = self._git('rev-list', '--count', '%s..%s' % (mb, tip)).stdout.strip()
        return '%s..%s (%s commit%s)' % (mb[:8], tip[:8], n, '' if n == '1' else 's')

    def _ledger_for(self, batch_id, verdicts, review):
        batch = self.load_state(self.state_path)['batches'][batch_id]
        self._append_ledger(ledger_entry(
            dict(batch, id=batch_id), verdicts, self._classes(batch_id, batch),
            review, self._commits(batch['branch']), self._credits(),
            ignored=self._ignored))

    def run_batch(self, batch_id):
        """One assigned batch, start to end: 'closed', 'exception' or
        'parked' ('checked' for a dry run whose checks passed)."""
        self._runs = []
        self._ignored = {}
        state = self.load_state(self.state_path)
        batch = state['batches'].get(batch_id)
        if batch is None:
            return self._exception(batch_id, 'state', [
                'no batch %s in %s' % (batch_id, self.state_path)])
        if batch['status'] != 'assigned':
            self.log('%s is %s; nothing to do' % (batch_id, batch['status']))
            return 'closed'
        worktree = self.worktree_for(self.repo, batch['branch'])
        if worktree is None:
            return self._exception(batch_id, 'worktree', [
                '%s has no worktree; `git worktree add <path> %s`, then clear '
                'this exception' % (batch['branch'], batch['branch'])])
        self._stage(batch_id, 'brief')
        try:
            rc, brief = self.render_brief(
                self.template_path, self.state_path, batch_id,
                bundle_dir=os.path.join(self.driver_dir, 'bundles', batch_id))
        except Exception as e:
            rc, brief = 1, 'render_brief raised %r' % (e,)
        if rc != 0:
            return self._exception(batch_id, 'brief', [brief.strip()])
        worker = FakeWorker(dry_report(batch)) if self.dry_run else self.worker

        # The worker, the mechanical checks, one fix round.
        self._stage(batch_id, 'worker')
        res = self._run(worker, brief, worktree, batch_id, 'worker')
        out = self._settle(batch_id, res, 'worker')
        if out:
            return out
        verdicts, problems = self._check(batch_id, worktree, res.report)
        if problems:
            self.log('%s: %d problem(s); one fix round' % (batch_id, len(problems)))
            res = self._fix(batch_id, batch, worker, worktree, 'mechanical checks',
                            problems, brief, 'worker-fix-checks')
            out = self._settle(batch_id, res, 'worker-fix-checks')
            if out:
                return out
            verdicts, problems = self._check(batch_id, worktree, res.report)
        if self.dry_run:
            self._ledger_for(batch_id, self._merged(batch_id, verdicts), (
                False, 'dry run: no review, nothing recorded, no close'))
            if problems:
                return self._exception(batch_id, 'checks', problems)
            self._finish(batch_id, 'checked')
            return 'checked'
        if problems:
            return self._exception(batch_id, 'checks', problems)

        # The review, when it can matter; one fix round.
        merged = self._merged(batch_id, verdicts)
        needed, why = self.review_needed(batch_id, merged, self.seed)
        if not needed:
            review = (False, 'not sampled (seed %s)' % self.seed)
        elif self.reviewer is None:
            return self._exception(batch_id, 'review', [
                'review needed (%s) but no reviewer is configured' % why])
        else:
            findings, out = self._review(batch_id, batch, worktree, merged, why,
                                         'reviewer')
            if out:
                return out
            fixed = bool(findings)
            if findings:
                res = self._fix(batch_id, batch, worker, worktree, 'review',
                                findings, brief, 'worker-fix-review')
                out = self._settle(batch_id, res, 'worker-fix-review')
                if out:
                    return out
                verdicts, problems = self._check(batch_id, worktree, res.report)
                if problems:
                    return self._exception(batch_id, 'checks', problems)
                merged = self._merged(batch_id, verdicts)
                findings, out = self._review(batch_id, batch, worktree, merged,
                                             why, 'reviewer-2')
                if out:
                    return out
                if findings:
                    return self._exception(batch_id, 'review', findings)
            review = (True, '%s (seed %s): clean%s' % (
                why, self.seed, ' after one fix round' if fixed else ''))

        # Record, close, push, ledger.
        self._stage(batch_id, 'record')
        failed = []
        for addr, (side, case) in sorted(verdicts.items()):
            rc, text = self.record_verdict(self.state_path, self.repo, batch_id,
                                           addr, side, case)
            if rc != 0:
                failed.append(text.strip())
        if failed:
            return self._exception(batch_id, 'record', failed)
        commits = self._commits(batch['branch'])
        self._stage(batch_id, 'close')
        try:
            rc, text = self.close_batch(self.state_path, self.repo, batch_id,
                                        base=self.base, gate=self.gate)
        except Exception as e:
            rc, text = 1, 'close_batch raised %r' % (e,)
        if rc != 0:
            return self._exception(batch_id, 'close', [
                l.strip() for l in text.splitlines() if l.strip()])
        self.log(text.rstrip())
        try:
            rc, text = self.write_scorecard(self.state_path, self.report_path,
                                            self.triage_path)
            self.log(text.rstrip())
        except Exception as e:   # the batch is closed; the scorecard can wait
            self.log('scorecard not written: %r' % (e,))
        err = self.push()
        batch = self.load_state(self.state_path)['batches'][batch_id]
        self._append_ledger(ledger_entry(
            dict(batch, id=batch_id), batch['verdicts'],
            self._classes(batch_id, batch), review, commits, self._credits(),
            ignored=self._ignored))
        self._finish(batch_id, 'closed')
        if err:
            return self._exception(batch_id, 'push', ['git push failed: %s' % err])
        return 'closed'

    # -- the loop -----------------------------------------------------------

    def _cut(self):
        """(batch_id, None) for a batch cut and triaged, else (None, exit)."""
        try:
            rc, text = self.next_batch(
                self.disagreements_path, self.state_path, self.size,
                self.category, triage_path=self.triage_path, repo=self.repo,
                base=self.base)
        except Exception as e:
            rc, text = 1, 'next_batch raised %r\n' % (e,)
        self.log(text.rstrip())
        m = re.match(r'(\S+) assigned:', text)
        if m:
            batch_id = m.group(1)
            t = re.search(r'^triage: \d+ auto, (\d+) import, \d+ ambiguous$',
                          text, re.M)
            if t:
                self._imported[batch_id] = int(t.group(1))
            if rc != 0:
                self.log('%s: triage exited %s; the rows it could not decide '
                         'stay for the worker' % (batch_id, rc))
            self._update_status(lambda s: s.__setitem__('stalled', None))
            return batch_id, None
        if rc == 2 and text.startswith('nothing left to assign'):
            # Rows remain, but only on files that assigned batches own.
            stalled = {'at': _now(), 'message': text.strip()}
            self._update_status(lambda s: s.__setitem__('stalled', stalled))
            return None, 4
        if rc == 2 and text.startswith('nothing left'):
            return None, 0
        m = re.match(r'(\S+) not assigned:', text)
        self._exception(m.group(1) if m else 'next', 'cut',
                        [l for l in text.splitlines() if l.strip()]
                        or ['next_batch exited %s' % rc])
        return None, 1

    def _cleared(self):
        """Batches whose exception the coordinator cleared on disk: retried
        first."""
        state = self.load_state(self.state_path)
        status = self._read_status()
        return [bid for bid, info in sorted(status['batches'].items())
                if isinstance(info, dict) and info.get('outcome') == 'exception'
                and bid not in status['exceptions']
                and state['batches'].get(bid, {}).get('status') == 'assigned']

    def run(self):
        """0 when every row is assigned or decided, 3 when parked, 4 when
        stalled (the rows left wait on files owned by assigned batches), 1
        when an exception that is not the batch's own stopped the run.  A
        dry run processes one batch and exits 0."""
        if self.dry_run:
            batch_id, code = self._cut()
            if batch_id is None:
                return code
            self.run_batch(batch_id)
            self.log('dry run: %s left assigned; ledger and status in %s'
                     % (batch_id, self.driver_dir))
            return 0
        queue = self._cleared()
        while True:
            if queue:
                batch_id = queue.pop(0)
            else:
                batch_id, code = self._cut()
                if batch_id is None:
                    return code
            if self.run_batch(batch_id) == 'parked':
                return 3
            if self._halt:
                return 1

    # -- the defaults for push and send (ruling R28) ------------------------

    def _git_push(self):
        """None, or why `git push iwd2r first:main first:first` failed."""
        try:
            r = subprocess.run(['git', '-C', self.repo, *PUSH_ARGS],
                               capture_output=True, text=True, timeout=600)
        except (OSError, subprocess.SubprocessError) as e:
            return repr(e)
        if r.returncode != 0:
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'exit %s' % r.returncode
        return None

    def _aoe_send(self, line):
        """`aoe send <session> <line>`, only when aoe_session is set."""
        if not self.aoe_session:
            return
        try:
            r = subprocess.run(['aoe', 'send', self.aoe_session, line],
                               capture_output=True, text=True, timeout=60)
        except (OSError, subprocess.SubprocessError) as e:
            self.log('aoe send failed: %r' % (e,))
            return
        if r.returncode != 0:
            self.log('aoe send exited %s: %s' % (
                r.returncode, _one_line((r.stderr or '') + (r.stdout or ''))))


def driver_from_args(argv, repo=None):
    """(Driver, None), or (None, (exit code, message)) for `wave.py run`."""
    wave = _tool('wave')
    repo = repo or wave.ROOT
    p = argparse.ArgumentParser(
        prog='wave.py run',
        description='Run the wave batches: cut, triage, worker, checks, '
                    'review, close, push, ledger.')
    p.add_argument('--workers', type=int, default=1,
                   help='stored; batches run one at a time for now')
    p.add_argument('--budget', type=float, default=None, help='credits')
    p.add_argument('--dry-run', action='store_true',
                   help='one batch with a fake worker; no review, no close, '
                        'no push, no aoe; needs --state')
    p.add_argument('--seed', default=DEFAULT_SEED,
                   help='the review sample seed (default %(default)s)')
    p.add_argument('--state', help='the wave-state file '
                                   '(default docs/wso/wave-state.json)')
    p.add_argument('--aoe-session', help='the AoE session that gets '
                                         'exception lines')
    try:
        args = p.parse_args(argv)
    except SystemExit as e:
        return None, (e.code if isinstance(e.code, int) else 2, '')
    if args.workers < 1:
        return None, (2, 'wave.py run: --workers must be at least 1')
    if args.dry_run and not args.state:
        return None, (2, 'wave.py run --dry-run refuses without an explicit '
                         '--state PATH: a dry run cuts a real batch (branch, '
                         'worktree, triage commits) in that state file. Point '
                         'it at a copy, never at docs/wso/wave-state.json.')
    if args.dry_run:
        live = {os.path.realpath(wave.STATE_PATH), os.path.realpath(
            os.path.join(repo, 'docs', 'wso', 'wave-state.json'))}
        if os.path.realpath(args.state) in live:
            return None, (2, 'wave.py run --dry-run refuses %s: that is the '
                             'live wave-state file. Copy it outside docs/wso '
                             'and pass the copy.' % args.state)
    state = os.path.abspath(args.state) if args.state else wave.STATE_PATH
    ledger = os.path.join(repo, LEDGER_REL)
    status = os.path.join(repo, STATUS_REL)
    if args.dry_run:
        dry = os.path.join(os.path.dirname(status), 'dry-run')
        ledger = os.path.join(dry, 'progress.md')
        status = os.path.join(dry, 'status.json')
        worker = reviewer = None
    else:
        worker, reviewer = HeadlessWorker(), HeadlessWorker()
    try:
        d = Driver(repo, state, worker, reviewer, ledger, status,
                   aoe_session=args.aoe_session, seed=args.seed,
                   budget=args.budget, workers=args.workers,
                   dry_run=args.dry_run)
    except ValueError as e:
        return None, (2, 'wave.py run: %s' % e)
    return d, None


def main(argv=None, repo=None):
    d, err = driver_from_args(sys.argv[1:] if argv is None else argv, repo)
    if err:
        if err[1]:
            print(err[1], file=sys.stderr)
        return err[0]
    return d.run()
