#!/usr/bin/env python3
"""Run GLM workers headless over wave batches.

This part: the worker protocol, a fake worker, the report parser and the
credit estimate. The driver itself uses no model."""
import collections
import json
import os
import re
import subprocess

Result = collections.namedtuple(
    'Result', 'exit_code report transcript credits rate_limited')

# (input, cached, output) credits per 10000 tokens.
FLASH_RATES = (2.3, 0.56, 8.0)
GLM53_RATES = (6.9, 1.7, 24.0)

VERDICT_RE = re.compile(r'^verdict (0x[0-9a-fA-F]+) (ours|fork|neither) case (\S+)$')


class ReportError(ValueError):
    pass


def parse_report(text):
    """Parse the '## Verdicts' section into {addr: (side, case)}."""
    out = {}
    in_section = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith('#'):
            in_section = line.lstrip('#').strip().lower() == 'verdicts'
            continue
        if not in_section or not line:
            continue
        if not line.startswith('verdict'):
            continue
        m = VERDICT_RE.match(line)
        if not m:
            raise ReportError('malformed verdict line: %r' % raw)
        addr, side, case = m.groups()
        if addr in out:
            raise ReportError('duplicate verdict for %s' % addr)
        out[addr] = (side, case)
    return out


def _rates(model):
    model = re.sub(r'\[1m\]$', '', model or '')
    return FLASH_RATES if 'flash' in model else GLM53_RATES


def credits_of(path):
    """Sum per-turn usage in a transcript. Lines are data, never executed."""
    total = 0.0
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
            r_in, r_cache, r_out = _rates(msg.get('model'))
            inp = (usage.get('input_tokens') or 0) + \
                (usage.get('cache_creation_input_tokens') or 0)
            total += (inp * r_in
                      + (usage.get('cache_read_input_tokens') or 0) * r_cache
                      + (usage.get('output_tokens') or 0) * r_out) / 10000
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
        try:
            data = json.loads(proc.stdout)
            if isinstance(data, dict):
                report = data.get('result') or ''
                session_id = data.get('session_id')
        except ValueError:
            report = proc.stdout or ''
        transcript = credits = None
        if isinstance(session_id, str) and re.fullmatch(r'[A-Za-z0-9_-]+', session_id):
            path = self.transcript_path(cwd, session_id)
            if os.path.isfile(path):
                transcript = path
                credits = credits_of(path)
        return Result(proc.returncode, report, transcript, credits, False)
