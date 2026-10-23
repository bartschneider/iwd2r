#!/usr/bin/env python3
"""Run GLM workers headless over wave batches.

The worker protocol, a fake worker, the report parser, the credit
estimate, the mechanical checks and the review policy, the ledger
template, the batch loop (`wave.py run`) with its budget stop, 429 park
and resume, and `--clear`.  The driver itself uses no model."""
import argparse
import collections
import contextlib
import datetime
import fcntl
import glob
import json
import importlib.util
import os
import random
import shlex
import signal
import re
import queue as queue_mod
import subprocess
import sys
import threading
import time

# api_error: the kind of an API error no retry cures (ruling R46), or None.
Result = collections.namedtuple(
    'Result', 'exit_code report transcript credits rate_limited api_error '
    'timed_out', defaults=(None, False))

# (input, cached, output) credits per 10000 tokens.
FLASH_RATES = (2.3, 0.56, 8.0)
GLM53_RATES = (6.9, 1.7, 24.0)
# Z.ai's rate-limit refusal, in a worker's final JSON result or in the
# transcript's last API-error entry (ruling R41).
RATE_LIMIT_TEXT = 'Request rejected (429)'
# API errors no retry cures: the run halts (ruling R46).
FATAL_API_ERRORS = ('authentication_failed', 'billing_error')
FATAL_API_STATUSES = (401, 402, 403)

VERDICT_RE = re.compile(
    r'^verdict (0[xX][0-9a-fA-F]+) (ours|fork|neither) case (\S+)$', re.IGNORECASE)
UNDECIDED_RE = re.compile(
    r'^undecided\s+(0[xX][0-9a-fA-F]+)\s+(fork|neither)\s+(\S.*)$', re.IGNORECASE)
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


def subagent_transcripts(path):
    """The transcripts of the subagents a session ran:
    <transcript dir>/<session id>/subagents/*.jsonl, sorted; [] when none
    (ruling R71)."""
    if not path:
        return []
    session = os.path.splitext(path)[0]
    return sorted(glob.glob(os.path.join(glob.escape(session), 'subagents',
                                         '*.jsonl')))


def usage_all(path):
    """usage_of summed over the transcript and its subagents'."""
    tokens, calls = usage_of(path)
    for sub in subagent_transcripts(path):
        try:
            t, c = usage_of(sub)
        except OSError:
            continue
        for k in tokens:
            tokens[k] += t[k]
        calls += c
    return tokens, calls


def usage_of(path):
    """(tokens, tool_calls) for a transcript (ruling R61): tokens is
    {input, cached, output} summed over the transcript's assistant
    messages once per message.id (last line wins, as credits_of), input
    counting cache creation as credits_of does; tool_calls counts the
    distinct tool_use blocks.  Any model counts: credits stay GLM-only,
    these figures do not.  Lines are data, never executed."""
    by_id = {}
    anon = []
    calls, anon_calls = set(), 0
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            msg = obj.get('message') if isinstance(obj, dict) else None
            if not isinstance(msg, dict):
                continue
            content = msg.get('content')
            if obj.get('type') == 'assistant' and isinstance(content, list):
                for b in content:
                    if isinstance(b, dict) and b.get('type') == 'tool_use':
                        if isinstance(b.get('id'), str) and b['id']:
                            calls.add(b['id'])
                        else:
                            anon_calls += 1
            usage = msg.get('usage')
            if not isinstance(usage, dict):
                continue
            mid = msg.get('id')
            if isinstance(mid, str) and mid:
                by_id[mid] = usage
            else:
                anon.append(usage)
    tokens = {'input': 0, 'cached': 0, 'output': 0}
    for usage in list(by_id.values()) + anon:
        tokens['input'] += (usage.get('input_tokens') or 0) + \
            (usage.get('cache_creation_input_tokens') or 0)
        tokens['cached'] += usage.get('cache_read_input_tokens') or 0
        tokens['output'] += usage.get('output_tokens') or 0
    return tokens, len(calls) + anon_calls


def _texts(content):
    """The text blocks of a message's content (a string or a block list)."""
    if isinstance(content, str):
        return [content]
    if not isinstance(content, list):
        return []
    return [b['text'] for b in content if isinstance(b, dict)
            and b.get('type') == 'text' and isinstance(b.get('text'), str)]


def fatal_api_error(entry):
    """The kind of an auth or billing error in a dict carrying Claude
    Code's API-error keys (`error`, `apiErrorStatus`), else None."""
    error = entry.get('error')
    if error in FATAL_API_ERRORS:
        return error
    status = entry.get('apiErrorStatus')
    if status in FATAL_API_STATUSES:
        return 'status %d' % status
    return None


def scan_transcript(path):
    """(kind, ran) for a worker transcript.  kind is what the LAST
    API-error entry says when no successful assistant turn follows it:
    'rate_limit' (apiErrorStatus 429 or error rate_limit, RATE_LIMIT_TEXT
    in its own text as the fallback), an auth or billing kind, or None
    (rulings R41, R44, R46).  ran is whether any successful, non-synthetic
    assistant turn exists: the worker really ran (R46).  The API-error
    entry is the synthetic assistant line Claude Code writes with
    isApiErrorMessage; tool_use and tool_result content, file text and
    the worker's prose are never read.  Lines are parsed as JSON data,
    never executed."""
    kind, ran = None, False
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            if not isinstance(obj, dict) or obj.get('type') != 'assistant':
                continue
            msg = obj.get('message')
            msg = msg if isinstance(msg, dict) else {}
            if obj.get('isApiErrorMessage') is True:
                if (obj.get('apiErrorStatus') == 429
                        or obj.get('error') == 'rate_limit'
                        or any(RATE_LIMIT_TEXT in t
                               for t in _texts(msg.get('content')))):
                    kind = 'rate_limit'
                else:
                    kind = fatal_api_error(obj)
            elif msg.get('model') != '<synthetic>':
                kind, ran = None, True   # a successful turn: retried past
    return kind, ran


def worker_ran(res):
    """True when a run's transcript holds a successful, non-synthetic
    assistant turn: the worker really ran (the breaker, ruling R46)."""
    if res.transcript is None:
        return False
    try:
        return scan_transcript(res.transcript)[1]
    except OSError:
        return False


def project_slug(cwd):
    return re.sub(r'[^A-Za-z0-9]', '-', os.path.abspath(cwd))


class FakeWorker:
    def __init__(self, report, exit_code=0, rate_limited=False):
        self.report = report
        self.exit_code = exit_code
        self.rate_limited = rate_limited

    def run(self, brief, cwd):
        return Result(self.exit_code, self.report, None, None, self.rate_limited)


# The worker's environment (ruling R64).  A driver launched from inside a
# Claude Code session must not let its workers attach to that session, and
# a plain-claude worker must not inherit the GLM wrapper's endpoint.
SESSION_VARS = ('CLAUDE_CODE_CHILD_SESSION', 'CLAUDE_CODE_ENTRYPOINT',
                'CLAUDE_CODE_EXECPATH', 'CLAUDE_PID', 'CLAUDE_EFFORT',
                'CLAUDECODE')
SESSION_PREFIXES = ('CLAUDE_CODE_MESSAGING_', 'CLAUDE_CODE_SESSION_')
# The launching shell's AoE identity (ruling R71): the user's hooks call
# $AOE_HOOK_BIN whenever AOE_INSTANCE_ID is set, which would re-bind the
# coordinator's session to the worker.  Dropped for every wrapper.
AOE_PREFIX = 'AOE_'
# What the driver's own `aoe send` must not inherit: the coordinator's
# Claude Code session.  AOE_* stays (aoe finds its session and profile
# through it; `aoe send --help` names no other way than --profile).
SEND_DROP = ('CLAUDECODE', 'CLAUDE_PID', 'CLAUDE_EFFORT')
SEND_DROP_PREFIXES = ('CLAUDE_CODE_',)


def send_env(environ):
    """environ for `aoe send`: without the Claude Code session variables."""
    return {k: v for k, v in environ.items()
            if k not in SEND_DROP and not k.startswith(SEND_DROP_PREFIXES)}

GLM_VARS = ('ANTHROPIC_BASE_URL', 'ANTHROPIC_AUTH_TOKEN',
            'CLAUDE_CODE_AUTO_COMPACT_WINDOW', 'API_TIMEOUT_MS')
GLM_PREFIXES = ('ANTHROPIC_DEFAULT_', 'CLAUDE_GLM_', 'CLAUDE_CODE_SUBAGENT_MODEL')


def _glm_config_dir():
    return os.environ.get('CLAUDE_GLM_CONFIG_DIR') or \
        os.path.expanduser('~/.claude-glm')


def _in_glm_config(path):
    glm = os.path.realpath(_glm_config_dir())
    real = os.path.realpath(path)
    return real == glm or real.startswith(glm + os.sep)


def worker_env(environ, glm):
    """environ without the variables that tie a worker to the launching
    session; for a wrapper other than claude-glm (glm False) also without
    the GLM wrapper's endpoint, token and model settings.  Nothing is
    added but CLAUDE_CODE_DISABLE_AUTO_MEMORY=1."""
    env = dict(environ)
    # Worktrees share the project's auto-memory folder; a worker must not
    # write to it (ruling R72).
    env['CLAUDE_CODE_DISABLE_AUTO_MEMORY'] = '1'
    for k in list(env):
        drop = (k in SESSION_VARS or k.startswith(SESSION_PREFIXES)
                or k.startswith(AOE_PREFIX))
        if not glm:
            drop = (drop or k in GLM_VARS or k.startswith(GLM_PREFIXES)
                    or (k == 'CLAUDE_CONFIG_DIR' and _in_glm_config(env[k])))
        if drop:
            del env[k]
    return env


DEFAULT_TIMEOUT = 10800   # seconds a worker or reviewer run may take (R71)


# Process groups of the live worker and reviewer runs (pgid = child pid), so
# that stopping the driver stops them (ruling R72).
_LIVE = set()
# Re-entrant: the stop handler runs on a thread that may hold it already.
_LIVE_LOCK = threading.RLock()


# Set (under _LIVE_LOCK) once a stop has killed the live groups: a worker
# forked after that kill, before it joined _LIVE, is killed on joining.
_STOPPED = threading.Event()


class _Stop(BaseException):
    """The driver was told to stop (SIGTERM, SIGINT)."""


def kill_live_groups():
    """SIGKILL every live worker process group; the pgids killed."""
    with _LIVE_LOCK:
        groups = sorted(_LIVE)
        _STOPPED.set()
    for pgid in groups:
        try:
            os.killpg(pgid, signal.SIGKILL)
        except OSError:
            pass
    return groups


def _run_group(argv, timeout=None, **kw):
    """subprocess.run in a session of its own, so that a timeout kills the
    whole process group (the worker's children too), then raises
    TimeoutExpired."""
    proc = subprocess.Popen(argv, start_new_session=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            **{k: v for k, v in kw.items()
                               if k not in ('capture_output',)})
    with _LIVE_LOCK:
        _LIVE.add(proc.pid)
        if _STOPPED.is_set():      # forked after the stop's kill: not missed
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except OSError:
                pass
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            proc.kill()
        proc.communicate()
        raise
    except BaseException:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            pass
        proc.wait()
        raise
    finally:
        with _LIVE_LOCK:
            _LIVE.discard(proc.pid)
    return subprocess.CompletedProcess(argv, proc.returncode, out, err)


class HeadlessWorker:
    def __init__(self, model='glm-5.3-flash[1m]', max_turns=400,
                 wrapper='claude-glm', config_dir=None, run=None,
                 timeout=DEFAULT_TIMEOUT):
        self.model = model
        self.max_turns = max_turns
        self.wrapper = wrapper
        self.timeout = timeout
        self._run = _run_group if run is None else run
        if config_dir is None:
            if os.path.basename(wrapper) == 'claude-glm':
                config_dir = (os.environ.get('CLAUDE_GLM_CONFIG_DIR')
                              or os.path.expanduser('~/.claude-glm'))
            else:
                # A plain claude keeps its transcripts in ~/.claude (or
                # CLAUDE_CONFIG_DIR), never in the GLM wrapper's folder.
                cfg = os.environ.get('CLAUDE_CONFIG_DIR')
                if not cfg or _in_glm_config(cfg):
                    cfg = os.path.expanduser('~/.claude')
                config_dir = cfg
        self.config_dir = config_dir

    def command(self, brief):
        argv = [self.wrapper, '-p', brief, '--output-format', 'json',
                '--dangerously-skip-permissions', '--max-turns', str(self.max_turns)]
        glm = os.path.basename(self.wrapper) == 'claude-glm'
        env = worker_env(os.environ, glm)
        if glm:
            env['CLAUDE_GLM_MODEL'] = self.model
        else:
            # A plain `claude` takes the model as a flag, and its
            # transcripts are under ~/.claude/projects (ruling R23).
            argv += ['--model', self.model]
            # Worker isolation (ruling R71): only the project's and the
            # local settings, no user hooks or MCP servers.
            argv += ['--setting-sources', 'project,local', '--strict-mcp-config']
            env['ENABLE_CLAUDEAI_MCP_SERVERS'] = '0'
        return argv, env

    def transcript_path(self, cwd, session_id):
        return os.path.join(self.config_dir, 'projects', project_slug(cwd),
                            session_id + '.jsonl')

    def run(self, brief, cwd):
        argv, env = self.command(brief)
        try:
            proc = self._run(argv, cwd=cwd, env=env, capture_output=True,
                             text=True, stdin=subprocess.DEVNULL,
                             timeout=self.timeout)
        except subprocess.TimeoutExpired:
            return Result(124, '', None, None, False, None, True)
        report, session_id = '', None
        exit_code = proc.returncode
        rate_limited, api_error = False, None
        try:
            data = json.loads(proc.stdout)
            if isinstance(data, dict):
                report = data.get('result') or ''
                session_id = data.get('session_id')
                # The final result says so: an error result (is_error, or
                # text that starts `API Error`) with the 429 text.  A
                # report that quotes the phrase is not one.
                rate_limited = (
                    isinstance(report, str) and RATE_LIMIT_TEXT in report
                    and (data.get('is_error') is True
                         or report.lstrip().startswith('API Error')))
                api_error = fatal_api_error(data)
        except ValueError:
            report = proc.stdout or ''
            exit_code = exit_code or 1
        transcript = credits = None
        if isinstance(session_id, str) and re.fullmatch(r'[A-Za-z0-9_-]+', session_id):
            path = self.transcript_path(cwd, session_id)
            if os.path.isfile(path):
                transcript = path
                credits = credits_of(path)
                kind, _ran = scan_transcript(path)
                if kind == 'rate_limit':
                    rate_limited = True
                elif kind is not None and api_error is None:
                    api_error = kind
        return Result(exit_code, report, transcript, credits, rate_limited,
                      api_error)


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

# -- the worker's transcript (Task 16) --------------------------------------

# The runners a worker may launch the game, a sandbox or a trace with.
HOUSE_RUNNERS = ('tools/play.py', 'tools/gltrace.py')
READ_LIMIT = 200      # lines of a covered source file one read may take
_SEGMENT = re.compile(r'&&|\|\||;|\n')
_WSO_REF = re.compile(r"(?:(?<=[\s'\"=:.^~@])|(?<=remotes/)|(?<=heads/))wso/")
_SED_SPAN = re.compile(r"(\d+),(\d+)p")
_LAUNCH = re.compile(r'\.wsb\b|WindowsSandbox|Start-Process', re.I)
# A command that sets the desktop override: an assignment (leading word,
# export, env), os.environ or putenv in python -c, setenv, `set`.
_DESKTOP_SET = re.compile(
    r'IWD2R_DESKTOP_OK\s*='
    r'|(?:os\.environ|putenv|setenv|export|\$env:|\bset\b)[^;\n]*IWD2R_DESKTOP_OK')
_LAUNCH_SHELLS = ('cmd', 'powershell', 'pwsh', 'explorer', 'wsl', 'bash', 'sh')


def _is_runner(text):
    for r in HOUSE_RUNNERS:
        if re.search(r'\bpython3\s+-I\s+(?:\S*/)?%s\b' % re.escape(r), text):
            return True
    return False


def _words(stage):
    """(command word, its arguments): leading VAR=value words and sudo
    skipped; the command word without its directory or .exe."""
    try:
        toks = shlex.split(stage)
    except ValueError:
        toks = stage.split()
    while toks and (re.match(r'^[A-Za-z_]\w*=', toks[0]) or toks[0] == 'sudo'):
        toks = toks[1:]
    if not toks:
        return '', []
    cw = os.path.basename(toks[0].replace('\\', '/')).lower()
    if cw.endswith('.exe'):
        cw = cw[:-4]
    return cw, toks[1:]


def _bound(cw, args):
    """The most lines a head, tail or sed -n stage passes on, or None."""
    if cw in ('head', 'tail'):
        for i, a in enumerate(args):
            m = re.match(r'^-n(\d+)$', a) or re.match(r'^-(\d+)$', a)
            if m:
                return int(m.group(1))
            if a == '-n' and i + 1 < len(args) and args[i + 1].isdigit():
                return int(args[i + 1])
        return 10
    if cw == 'sed' and '-n' in args:
        for a in args:
            m = _SED_SPAN.search(a)
            if m:
                return int(m.group(2)) - int(m.group(1)) + 1
    return None


def _short(text, n=100):
    text = ' '.join(str(text).split())
    return text if len(text) <= n else text[:n - 3] + '...'


def _tool_calls(path):
    """(name, input) of every tool_use block of the assistant messages in a
    Claude Code JSONL transcript; unparseable lines are skipped."""
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            msg = obj.get('message') if isinstance(obj, dict) else None
            content = msg.get('content') if isinstance(msg, dict) else None
            if not isinstance(content, list):
                continue
            for block in content:
                if isinstance(block, dict) and block.get('type') == 'tool_use' \
                        and isinstance(block.get('input'), dict):
                    yield block.get('name'), block['input']


_HISTORY_VERBS = ('reset', 'rebase', 'revert', 'restore', 'clean', 'stash',
                  'filter-repo', 'push', 'update-ref')


# No real branch is named for these (the batch's own path prefixes).
_BATCH_DIRS = ('src', 'compat', 'docs', 'oracle', 'tools', 'tests')


_HEREDOC = re.compile(r"""<<-?[ \t]*(?:'([^'\n]*)'|"([^"\n]*)"|([^\s;&|<>()]+))""")


# A heredoc fed to one of these runs its body as commands.
_SHELLS = ('bash', 'sh', 'zsh', 'dash', 'ksh', 'wsl', 'pwsh', 'powershell')


def _fallback_split(text, seps):
    return text.split('|') if seps == ('|',) else _SEGMENT.split(text)


def _feeds_shell(stage):
    """True when a word of a pipe stage names a shell (the command word,
    or after env, exec, nohup and the like)."""
    cw, args = _words(stage)
    for w in [cw] + args:
        w = os.path.basename(w.replace('\\', '/')).lower()
        if (w[:-4] if w.endswith('.exe') else w) in _SHELLS:
            return True
    return False


def _split_unquoted(text, seps):
    """text split at any of seps (longest first) outside quotes; a
    backslash escapes the next character, an unquoted # at the start of a
    word is a comment to the end of the line, and the body of a heredoc
    (<<WORD, <<-WORD, <<'WORD') is data up to its delimiter line, unless
    a stage of the pipeline that opened it names a shell (_feeds_shell,
    before or after the <<): that body is commands and is split the same
    way.  Fails closed: a quote or heredoc left open at the end falls
    back to _SEGMENT."""
    parts, cur, quote, i, pending = [], [], None, 0, []
    while i < len(text):
        ch = text[i]
        if quote:
            cur.append(ch)
            if ch == '\\' and quote == '"' and i + 1 < len(text):
                i += 1
                cur.append(text[i])
            elif ch == quote:
                quote = None
        elif ch == '\\' and i + 1 < len(text):
            cur.append(ch + text[i + 1])
            i += 1
        elif ch in '\'"':
            quote = ch
            cur.append(ch)
        elif ch == '#' and (not cur or cur[-1][-1:] in ' \t'):
            j = text.find('\n', i)
            i = len(text) if j < 0 else j
            continue
        elif ch == '\n' and pending:
            parts.append(''.join(cur))
            cur = []
            delims, pending = pending, []
            i += 1
            for d, idx in delims:
                # the whole pipeline of the segment that opened it (slots
                # stay valid, bodies are appended only after this newline);
                # a trailing | continues it past the body: fail closed
                run = parts[idx].rstrip().endswith('|') or any(
                    _feeds_shell(st)
                    for st in _split_unquoted(parts[idx], ('|',)))
                found, body = False, []
                while i < len(text) and not found:
                    j = text.find('\n', i)
                    line = text[i:len(text) if j < 0 else j]
                    i = len(text) if j < 0 else j + 1
                    found = line.strip() == d
                    if not found:
                        body.append(line)
                if not found:           # no delimiter line: fail closed
                    return _fallback_split(text, seps)
                if run:                 # fed to a shell: the body is commands
                    parts += _split_unquoted('\n'.join(body), seps)
            continue
        elif text.startswith('<<', i) and not text.startswith('<<<', i) \
                and _HEREDOC.match(text, i):
            m = _HEREDOC.match(text, i)
            d = next(g for g in m.groups() if g is not None)
            pending.append((re.sub(r'[\\\'"]', '', d) if m.group(3) else d,
                            len(parts)))
            cur.append(text[i:m.end()])
            i = m.end()
            continue
        else:
            sep = next((x for x in seps if text.startswith(x, i)), None)
            if sep:
                parts.append(''.join(cur))
                cur = []
                i += len(sep) - 1
            else:
                cur.append(ch)
        i += 1
    if quote or pending:
        return _fallback_split(text, seps)
    parts.append(''.join(cur))
    return parts


def _history_rewrite(args, covered=(), tracked=None):
    """True when the arguments of a `git` command (after the command word)
    rewrite or discard history or work: reset, rebase, revert, restore,
    clean, stash, filter-repo, push, update-ref, commit --amend, branch
    -d/-D, reflog expire, gc --prune, and a checkout that is not a plain
    branch switch (`--`, -f, -B, `.`, a path after a start point, or one
    argument that names a covered file, looks like a path or starts with a
    batch directory), branch -f/-M/-C and switch -C/--force-create.
    add, commit, switch, checkout -b / <branch> and the read-only
    commands are not.  `tracked(name)`, when given, says whether a name is a
    file tracked in the worktree: one checkout argument naming one is a
    path restore, not a branch switch."""
    rest = list(args)
    while rest and rest[0].startswith('-'):      # git -C dir / -c k=v / --x
        rest = rest[2:] if rest[0] in ('-C', '-c') else rest[1:]
    if not rest:
        return False
    sub, rest = rest[0], rest[1:]
    if sub in _HISTORY_VERBS:
        return True
    if sub == 'commit':
        return any(a == '--amend' for a in rest)
    if sub == 'branch':
        return any(a in ('--delete', '--force')
                   or re.fullmatch(r'-[a-zA-Z]*[dDfMC][a-zA-Z]*', a)
                   for a in rest)
    if sub == 'reflog':
        return 'expire' in rest or 'delete' in rest
    if sub == 'gc':
        return any(a.startswith('--prune') for a in rest)
    if sub == 'switch':
        return any(a in ('--force', '--discard-changes', '--force-create')
                   or re.fullmatch(r'-[a-zA-Z]*[fC][a-zA-Z]*', a)
                   for a in rest)
    if sub == 'checkout':
        if '--' in rest or any(
                a in ('--force', '--patch', '--ours', '--theirs')
                or re.fullmatch(r'-[a-zA-Z]*[fpB][a-zA-Z]*', a) for a in rest):
            return True
        if '-b' in rest:
            return False
        pos = [a for a in rest if not a.startswith('-')]
        if len(pos) != 1:
            return len(pos) > 1
        a = pos[0]
        return (a == '.' or a.startswith(('./', '../'))
                or any(ch in a for ch in '*?[')
                or a.split('/')[0] in _BATCH_DIRS
                or any(a == c or a.endswith('/' + c) for c in covered)
                or bool(tracked and tracked(a)))
    return False


def transcript_findings(transcript_path, covered_files, tracked=None):
    """[(kind, problem, file)] for the forbidden tool calls in a worker's
    transcript.  kind 'wso' (a git command naming a wso/ ref; the pinned
    sha is allowed), 'launch' (a .wsb, WindowsSandbox or Start-Process
    launch outside HOUSE_RUNNERS), 'read' (a whole-file read of a covered
    source file: no Read limit or one over READ_LIMIT; cat, a sed -n span
    or head/tail -n over READ_LIMIT; file is the covered file),
    'history' (a git command that rewrites history or discards work, see
    _history_rewrite; terminal like launch) or 'unreadable'.  cat and the launch words count only as a
    command word, and a cat piped into a bounded head/tail/sed is bounded."""
    covered = [c for c in covered_files if c]
    out = []
    calls = []
    for path in [transcript_path] + subagent_transcripts(transcript_path):
        try:
            calls += list(_tool_calls(path))
        except OSError as e:
            return [('unreadable', 'transcript %s could not be read (%s); '
                     'the rules about tool calls cannot be checked'
                     % (path, e), None)]
    for name, inp in calls:
        if name == 'Bash' and _DESKTOP_SET.search(str(inp.get('command', ''))):
            out.append(('launch', 'sets IWD2R_DESKTOP_OK (%s): only a person '
                        'at the PC sets it' % _short(inp.get('command', '')),
                        None))
        if name == 'Read':
            fp = str(inp.get('file_path', ''))
            hit = [c for c in covered if fp == c or fp.endswith('/' + c)]
            limit = inp.get('limit')
            if hit and (not isinstance(limit, int) or limit > READ_LIMIT):
                out.append(('read', 'Read of %s with %s: read a covered '
                            'source file in slices of at most %d lines '
                            '(offset and limit)' % (
                                hit[0], 'no limit' if not isinstance(limit, int)
                                else 'limit %d' % limit, READ_LIMIT), hit[0]))
        elif name == 'Bash':
            for seg in _split_unquoted(str(inp.get('command', '')),
                                       ('&&', '||', ';', '\n')):
                seg = seg.strip()
                if not seg:
                    continue
                stages = [_words(st) for st in _split_unquoted(seg, ('|',))]
                for i, (cw, args) in enumerate(stages):
                    text = ' '.join(args)
                    if cw == 'git' and _WSO_REF.search(' ' + text):
                        out.append(('wso', 'git command naming a wso/ ref '
                                    '(%s): read the fork only at the pinned '
                                    'sha, `git show <sha>:<path>`'
                                    % _short(seg), None))
                    if cw == 'git' and _history_rewrite(args, covered, tracked):
                        out.append(('history', 'history rewrite in a worker '
                                    '(%s): workers only add and commit'
                                    % _short(seg), None))
                    launch = cw in ('windowssandbox', 'start-process', 'start') \
                        or (cw in _LAUNCH_SHELLS and _LAUNCH.search(text))
                    if launch and not _is_runner(seg):
                        out.append(('launch', 'launch outside a house runner '
                                    '(%s): launch only through `python3 -I %s`'
                                    % (_short(seg), '` or `python3 -I '.join(
                                        HOUSE_RUNNERS)), None))
                    for c in covered:
                        if not any(a == c or a.endswith('/' + c) for a in args):
                            continue
                        why = None
                        if cw == 'cat':
                            later = [_bound(*st) for st in stages[i + 1:]]
                            if not any(b is not None and b <= READ_LIMIT
                                       for b in later):
                                why = 'cat of %s' % c
                        else:
                            b = _bound(cw, args)
                            if b is not None and b > READ_LIMIT:
                                why = '%s of %s over %d lines' % (cw, c, READ_LIMIT)
                        if why:
                            out.append(('read', '%s: read a covered source '
                                        'file in slices of at most %d lines'
                                        % (why, READ_LIMIT), c))
    return out


def transcript_checks(transcript_path, covered_files, tracked=None):
    """One problem per forbidden worker tool call (transcript_findings)."""
    return [text for _kind, text, _f in
            transcript_findings(transcript_path, covered_files, tracked)]


# The paths a batch may change (ruling R71): anything else on the branch is
# a check problem, tools/, tests/, .claude/, .mcp.json and docs/wso/ included
# (but for the import parked rows, one file per row, that the worker writes).
ALLOWED_PATHS = ('src/', 'compat/', 'docs/evidence/', 'docs/MAP.md',
                 'oracle/cases/', 'oracle/CMakeLists.txt',
                 'docs/wso/import-parked/')


def _path_allowed(path):
    return any(path == a or (a.endswith('/') and path.startswith(a))
               for a in ALLOWED_PATHS)


def _git_paths(run, worktree, args, parse):
    """(paths, problem) from a git command run in the worktree."""
    proc = run(['git', *args], cwd=worktree, capture_output=True, text=True)
    if proc.returncode != 0:
        return [], 'git %s failed (exit %s): %s' % (
            ' '.join(args), proc.returncode, _tail(proc))
    return [parse(l) for l in (proc.stdout or '').splitlines() if l.strip()], None


def parse_undecided(text):
    """Parse the '## Undecided' section into {addr: (side, reason)} (ruling
    R73): rows the worker's reading says are fork or neither but that no
    probe, replay or trace case it can run decides.  Same tolerance as
    parse_report; a missing section is {}."""
    out = {}
    in_section = False
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line.startswith('#'):
            in_section = line.lstrip('#').strip().lower() == 'undecided'
            continue
        if not in_section or not line:
            continue
        if re.fullmatch(r'```[^`]*', line):
            continue
        cand = MARKER_RE.sub('', line, count=1).strip().strip('`').strip()
        m = UNDECIDED_RE.match(cand)
        if not m:
            raise ReportError('line %d: malformed undecided line: %r' % (n, raw))
        addr = '0x%X' % int(m.group(1), 16)
        if addr in out:
            raise ReportError('line %d: duplicate undecided line for %s' % (n, addr))
        out[addr] = (m.group(2).lower(), m.group(3).strip())
    return out


FRIDA_SCENARIOS_TEXT = 'world_entry, first_fight, walk_and_talk'
# The history rule's way to undo one function (first's 021a6cb0): the
# function is edited back, never the whole file put back, which would
# discard the other rows' changes in a shared file.  %s is the merge base.
EDIT_BACK = ("edit that function back to the merge base's body (`git show %s:<path>` "
             'shows it)')
SHARED_FILE = "do not replace a file that holds other rows' changes"


def park_text(address, mb, needs):
    """How a worker parks one import row (the import brief's rule): the
    function edited back, its evidence deleted, its own file under
    docs/wso/import-parked/ (one per row, so parallel batches never
    conflict), one commit."""
    stem = address[2:].upper() if address.lower().startswith('0x') else address.upper()
    return ('%s, delete docs/evidence/%s.md, write docs/wso/import-parked/%s.json '
            '{"address", "name", "file", "batch", "needs"} with needs "%s", and commit '
            'the three together; %s' % (EDIT_BACK % mb, stem, stem, needs, SHARED_FILE))


UNMEASURED_NEEDS = FRIDA_SCENARIOS_TEXT + ': not exercised'
UNMEASURED = '%s: exercised by no scenario (' + FRIDA_SCENARIOS_TEXT + '); park it: %s'


def _merge_base(worktree, base):
    """Where the worktree's HEAD left base (`git merge-base base HEAD`), as
    a short sha; base itself when git cannot tell."""
    r = subprocess.run(['git', '-C', worktree, 'merge-base', base, 'HEAD'],
                       capture_output=True, text=True)
    sha = r.stdout.strip() if r.returncode == 0 else ''
    return sha[:12] if sha else base


X86_MAP_REL = 'build/windows-msvc-x86/RelWithDebInfo/iwd2r.map'


def frida_build(worktree, log_path=None):
    """Our x86 build of a batch worktree, for its Frida phase (spec B): the
    worktree synced into its own Windows checkout (play.win_checkout:
    %USERPROFILE%\\iwd2r-build-<worktree folder name>, never the driver's,
    whatever IWD2R_WIN_CHECKOUT says) and built there with the x86 preset,
    the compiler output in log_path.  Returns (checkout, map path): the map
    beside the exe names the RVAs of the bodies under test, and probe.py
    --build-checkout runs that exe.  Raises on any failure (play exits with
    SystemExit)."""
    play = _tool('play')
    checkout = play.win_checkout(worktree, {})
    play.sync_windows(root=worktree, checkout=checkout)
    exe = play.build_windows_x86(checkout=checkout, log_path=log_path)
    map_path = os.path.splitext(exe)[0] + '.map'
    if not os.path.isfile(map_path):
        raise RuntimeError('the x86 build left no %s' % map_path)
    return checkout, map_path


def _map_checkout(map_path):
    """The Windows checkout whose x86 build wrote map_path (the --frida-map
    override measures the exe beside it), or None when the map is not where
    that build puts it."""
    norm = os.path.normpath(os.path.abspath(map_path))
    rel = os.path.normpath(X86_MAP_REL)
    return norm[:-len(rel) - 1] if norm.endswith(os.sep + rel) else None


def _write_trace_evidence(path, probe, orig_path, ours_path):
    """Rewrite one evidence file's front matter to `kind: trace` with the
    `trace_source:` of a measured Frida pair (truth.trace_source_line) and no
    `exemption:`; every other line, and the line endings, stay as they are."""
    truth = _tool('truth')
    with open(path, encoding='utf-8', newline='') as f:
        lines = f.read().splitlines(keepends=True)
    if not lines or lines[0].strip() != '---':
        raise ValueError('%s has no front matter' % path)
    end = next((i for i in range(1, len(lines)) if lines[i].strip() == '---'), None)
    if end is None:
        raise ValueError('%s has an unclosed front matter' % path)
    eol = '\r\n' if lines[0].endswith('\r\n') else '\n'
    source = truth.trace_source_line(probe, orig_path, ours_path)
    head, kind_at, source_at = [], None, None
    for line in lines[1:end]:
        name = line.partition(':')[0].strip()
        if name == 'exemption':
            continue
        if name == 'kind' and kind_at is None:
            kind_at = len(head)
            line = 'kind: trace' + (line[len(line.rstrip('\r\n')):] or eol)
        elif name == 'trace_source' and source_at is None:
            source_at = len(head)
            line = 'trace_source: %s' % source + (line[len(line.rstrip('\r\n')):] or eol)
        head.append(line)
    if kind_at is None:
        kind_at = len(head)
        head.append('kind: trace' + eol)
        if source_at is not None and source_at >= kind_at:
            source_at += 1
    if source_at is None:
        head.insert(kind_at + 1, 'trace_source: %s%s' % (source, eol))
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(''.join([lines[0]] + head + lines[end:]))


def mechanical_checks(batch, worktree, verdicts, run=subprocess.run,
                      kind='verdicts', transcript=None, covered_files=(),
                      probe=(), base='first', undecided=()):
    """Problems that stop a batch before any review; [] when clean.

    Each problem says what failed and how to fix it (it is the worker's
    fix-round prompt).  An import batch (kind 'import', ruling R63) has no
    verdict lines or evidence phrases to check; the tool tests,
    check_evidence and parity still run.  With a transcript, the worker's
    forbidden tool calls (transcript_checks) join the problems.  `probe`
    holds the address keys the driver recorded from an oracle probe pass:
    only those (and a `probe-<batch id>-...` ours case) need the
    'corroborated by WSO' phrase (ruling R71).  Uncommitted paths, and
    paths changed against `base` outside ALLOWED_PATHS, are problems.
    `undecided` holds the addresses of the report's ## Undecided section
    (ruling R73): such a row needs no verdict line, and a verdict line for
    it too is a problem."""
    wave = _tool('wave')
    problems = []
    if transcript:
        problems += transcript_checks(transcript, covered_files)
    key = wave._addr_key
    rows = batch.get('rows', [])
    row_keys = {key(r['address']) for r in rows}
    given = {key(a): a for a in verdicts}
    und = {key(a): a for a in undecided}
    if kind == 'import':
        if und:
            problems.append('Undecided is for wave-1 batches; remove the '
                            '## Undecided section')
        rows, given, und = [], {}, {}
    if kind != 'import':
        for k, a in sorted(given.items()):
            side, case = verdicts[a][0], verdicts[a][1]
            if side in ('fork', 'neither') and \
                    wave.deciding_kind(case) == 'dump reading':
                problems.append(
                    '%s: a %s verdict decided by a dump reading (%s); a fork '
                    'or neither verdict needs a probe, replay or trace case '
                    '(probe-N, a replay checkpoint or a trace id); dump-* and '
                    'readout-* decide only ours' % (a, side, case))
    for r in rows:
        if r.get('class') == 'ambiguous' or (
                'class' not in r and not r.get('verdict')):
            if key(r['address']) not in given and \
                    key(r['address']) not in und:
                problems.append('%s: no verdict line in the report; add '
                                '"verdict <addr> ours|fork|neither case <id>" '
                                'under ## Verdicts (or, if no probe, replay or '
                                'trace case you can run decides a fork or '
                                'neither reading, list it under ## '
                                'Undecided)' % r['address'])
    for k, a in sorted(und.items()):
        if k not in row_keys:
            problems.append('%s: undecided line for an address outside this '
                            'batch; remove that line' % a)
        elif k in given:
            problems.append('%s: both a verdict line and an undecided line; '
                            'keep one (a row you cannot decide gets no '
                            'verdict line, never ours)' % a)
    inside = {k: a for k, a in und.items() if k in row_keys and k not in given}
    if inside:
        # An undecided row leaves the batch with our body: a changed body
        # would reach the base as an import without a probe.
        try:
            changed = {key('0x%X' % addr) for _p, addr, _n in
                       _tool('check_evidence').touched(worktree, base)}
        except Exception as e:
            changed = set()
            problems.append('cannot tell whether the undecided rows kept '
                            'our bodies (%r); fix it and rerun' % (e,))
        mb = _merge_base(worktree, base)
        for k, a in sorted(inside.items()):
            if k in changed:
                problems.append('%s: listed under ## Undecided but its body '
                                'changed against %s; %s and commit; %s.'
                                % (a, base, EDIT_BACK % mb, SHARED_FILE))
    for k, a in given.items():
        if k not in row_keys:
            problems.append('%s: verdict for an address outside this batch; '
                            'remove that line' % a)
    probe_keys = {key(a) for a in probe}
    for r in rows:
        k = key(r['address'])
        if k not in given:
            continue
        rel = '%s/%s.md' % (wave.EVIDENCE_DIR, k.upper())
        text, err = wave._read_evidence(worktree, batch['branch'], r['address'])
        if text is None:
            problems.append('%s: missing; write it and commit it' % rel)
            continue
        side, case = verdicts[given[k]][0], str(verdicts[given[k]][1])
        if side == 'fork' and not wave.KIND_RE.search(text):
            problems.append("%s: no 'kind:' line in the front matter; add "
                            "'kind: oracle' or 'kind: replay'" % rel)
        driver_probe = k in probe_keys or (
            side == 'ours' and batch.get('id') is not None
            and case.startswith('probe-%s-' % batch['id']))
        if driver_probe:
            # A probe pass says ours matches the original (ruling R57).
            if wave.EVIDENCE_PHRASES[1] not in text:
                problems.append("%s: its verdict case is a probe; add the exact "
                                "phrase 'corroborated by WSO at <sha>'" % rel)
        elif not any(p in text for p in wave.EVIDENCE_PHRASES):
            problems.append("%s: no 'disagreed with WSO' or 'corroborated by "
                            "WSO' phrase; add one of the exact phrases" % rel)
    dirty, bad = _git_paths(run, worktree, ('status', '--porcelain', '-uall'),
                            lambda l: l[3:].split(' -> ')[-1].strip('"'))
    if bad:
        problems.append(bad)
    if dirty:
        problems.append('uncommitted paths in the worktree; commit or '
                        'delete: %s' % ', '.join(sorted(dirty)))
    # The base is where the branch left `base`, not `base` itself: commits
    # that landed on first meanwhile (merged into the batch before the close)
    # are not the worker's edits.
    mbs, bad = _git_paths(run, worktree,
                          ('merge-base', base, batch.get('branch') or 'HEAD'),
                          str.strip)
    if bad:
        problems.append(bad)
    changed, bad = _git_paths(run, worktree,
                              ('diff', '--name-only') + (
                                  (mbs[0], 'HEAD') if mbs
                                  else ('%s...HEAD' % base,)),
                              str.strip)
    if bad:
        problems.append(bad)
    outside = sorted(p for p in changed if not _path_allowed(p))
    if outside:
        # These files hold no batch row, so the whole file goes back, from
        # the merge base the diff above was taken against.
        held = mbs[0][:12] if mbs else base
        problems.append('changed outside the paths a batch may touch (%s): '
                        '%s; put back the merge base\'s copy of each (`git show '
                        '%s:<path> > <path>`, or `git rm <path>` when the merge base '
                        'lacks the file; no batch row lives there) and commit'
                        % (', '.join(ALLOWED_PATHS), ', '.join(outside), held))
    steps = [
        (['python3', '-I', 'tests/test_tools.py'], 'tests/test_tools.py'),
        (['python3', '-I', 'tools/check_evidence.py', '--base', 'first'],
         'tools/check_evidence.py'),
    ]
    if os.path.isfile(_vanilla_exe()):
        steps.append((_parity_python() + ['tools/parity/run.py', '--base', 'first'],
                      'tools/parity/run.py'))
    # docs/MAP.md is not the batch's: the driver regenerates it once at close,
    # on the merged tree (_regen_map), so parallel batches never conflict.
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


# -- worker pools (Task 15) -----------------------------------------------

# reviewer_model is resolved by parse_pool; None only on a hand-built Pool.
Pool = collections.namedtuple(
    'Pool', 'name wrapper model workers reviewer_model', defaults=(None,))
DEFAULT_POOL = 'glm=claude-glm:glm-5.3-flash[1m]:2'
# The short form NAME[:WORKERS] of the two known pool names, per queue.
POOL_DEFAULTS = {
    'glm': {'wave1': ('claude-glm', 'glm-5.3-flash[1m]'),
            'import': ('claude-glm', 'glm-5.3-flash[1m]')},
    'claude': {'wave1': ('claude', 'sonnet'), 'import': ('claude', 'haiku')},
}
# A pool's reviewer model unless --reviewer-model says otherwise; any
# other pool reviews with its own worker model.
REVIEWER_DEFAULTS = {'claude': 'sonnet', 'glm': 'glm-5.3-flash[1m]'}
POOL_NAME_RE = re.compile(r'[A-Za-z0-9_-]+')


def _workers_count(text, spec):
    try:
        n = int(text)
    except ValueError:
        n = 0
    if n < 1:
        raise ValueError('pool %r: WORKERS must be a whole number of at '
                         'least 1, not %r' % (spec, text))
    return n


def parse_pool(spec, queue='wave1', reviewer_model=None):
    """A Pool from `NAME=WRAPPER:MODEL[:WORKERS]` (WORKERS defaults to 1),
    or from the short form `NAME[:WORKERS]` of a known name (POOL_DEFAULTS,
    which depend on the queue).  ValueError when malformed.
    reviewer_model overrides REVIEWER_DEFAULTS."""
    if not isinstance(spec, str) or not spec.strip():
        raise ValueError('empty pool spec')
    if '=' in spec:
        name, _, rest = spec.partition('=')
        parts = rest.split(':')
        if len(parts) not in (2, 3) or not parts[0] or not parts[1]:
            raise ValueError(
                'pool %r: expected NAME=WRAPPER:MODEL[:WORKERS]' % spec)
        wrapper, model = parts[0], parts[1]
        workers = _workers_count(parts[2], spec) if len(parts) == 3 else 1
    else:
        name, _, count = spec.partition(':')
        if name not in POOL_DEFAULTS or queue not in POOL_DEFAULTS[name]:
            raise ValueError(
                'pool %r: the short form NAME[:WORKERS] needs a known name '
                '(%s) and a known queue; give NAME=WRAPPER:MODEL[:WORKERS]'
                % (spec, ', '.join(sorted(POOL_DEFAULTS))))
        wrapper, model = POOL_DEFAULTS[name][queue]
        workers = _workers_count(count, spec) if ':' in spec else 1
    if not POOL_NAME_RE.fullmatch(name):
        raise ValueError('pool %r: the name must be letters, digits, - or _'
                         % spec)
    return Pool(name, wrapper, model, workers,
                reviewer_model or REVIEWER_DEFAULTS.get(name, model))


def pools_with_reviewers(pools, reviewer_models):
    """The pools with --reviewer-model NAME=MODEL applied; ValueError for
    a name that is no pool."""
    names = {p.name for p in pools}
    for name in reviewer_models:
        if name not in names:
            raise ValueError('--reviewer-model %s=...: no pool named %s '
                             '(pools: %s)' % (name, name, ', '.join(sorted(names))))
    return [p._replace(reviewer_model=reviewer_models[p.name])
            if p.name in reviewer_models else p for p in pools]


def pool_is_glm(pool):
    """True when Z.ai credits price the pool's turns (ruling R40)."""
    return _rates(pool.model) is not None


def parked_map(value):
    """A pool's `parked` as {batch_id: {at, reason}} (ruling R64).  The
    one-slot form `{batch_id, at, reason}` of the first pool driver, and
    None, read as a map too."""
    if not isinstance(value, dict) or not value:
        return {}
    if isinstance(value.get('batch_id'), str):
        return {value['batch_id']: {'at': value.get('at'),
                                    'reason': value.get('reason')}}
    return {k: v for k, v in value.items() if isinstance(v, dict)}


def _set_parked(holder, mapping):
    holder['parked'] = dict(mapping) or None


def _unpark_everywhere(s, batch_id):
    """Remove batch_id from every pool's parked map (and the legacy
    top-level slot)."""
    for holder in [s] + [p for p in s['pools'].values() if isinstance(p, dict)]:
        m = parked_map(holder.get('parked'))
        if batch_id in m:
            m.pop(batch_id)
            _set_parked(holder, m)


# -- the batch loop ------------------------------------------------------

# Relative to the repo root (ruling R29): one ledger per plan, and the
# driver folder is the status file's directory.
LEDGER_REL = os.path.join('.superpowers', 'sdd', '2026-10-17-wso-integration',
                          'progress.md')
DRIVER_REL = os.path.join('.superpowers', 'sdd', '2026-10-17-wso-integration',
                          'driver')
STATUS_REL = os.path.join(DRIVER_REL, 'status.json')


def status_rel(queue='wave1'):
    """The status file of a queue, relative to the repo root (ruling
    R42): status.json for wave1, status-import.json for import."""
    return os.path.join(DRIVER_REL, _tool('wave').QUEUES[queue]['status'])


DEFAULT_SEED = '2026-10-22'
# The next batch's credits before this run has measured one (ruling R38;
# the plan's ledger example is 1180).
DEFAULT_ESTIMATE = 1200.0
WIP_MESSAGE = 'WIP: parked by driver'
# The breaker (ruling R45): this many batch exceptions in a row whose
# worker never produced a transcript halt the run.
BREAKER_LIMIT = 3
STATE_COMMIT_TRAILER = 'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>'
PUSH_ARGS = ('push', 'iwd2r', 'first:main', 'first:first')
# Exceptions at these stages are not the batch's own: the run stops.
GLOBAL_STAGES = ('cut', 'push')
BLOCKING_RE = re.compile(r'^(?:[-*]|\d+\.)?\s*[*_`]*BLOCKING[*_`]*:')

FIX_PROMPT = """# Fix round for batch {id}

The driver's {stage} found the problems below in your work on `{branch}`
(this worktree). Fix every one, commit, and write your report again: the
full `## Verdicts` section, one line for every row you decide, not only
the rows you fixed. `## Verdicts` holds verdict lines only; put notes under
`## Notes`. A row whose reading is fork or neither, with no probe, replay
or trace case you can run to decide it, gets no verdict line: keep our
body and list it under `## Undecided` as `undecided <0xADDR> <fork|neither>
<one-line reason>`. Never write `ours` for such a row.

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

RESUME_PREAMBLE = """Resume: batch {id} {why}. This run takes the batch up again in
the same worktree, on `{branch}`. The work done so far is on that branch:
`git log {base}..{branch}`, and `git status` for anything not committed
(a `{wip}` commit, if one is still there, holds what a park saved). Build on it: do not redo
finished work, and do not rewrite that history.{reset}

Verdicts already recorded in the wave state:

{verdicts}

Your report still carries the full `## Verdicts` section: one line for
every row you decide, the rows you decided before this resume included.
A row you decided `ours` before this resume although your reading says fork
or neither goes under ## Undecided now (rule 13): keep our body and drop its
verdict line.

The original brief follows.

---

"""


class StatusError(ValueError):
    """The driver status file cannot be read: corrupt, torn, or not a
    status object.  The message names the file."""


class _StageError(Exception):
    """A stage of one batch failed: run_batch turns it into that batch's
    exception, never a traceback."""

    def __init__(self, stage, problem):
        super().__init__(problem)
        self.stage = stage
        self.problem = problem


class _LaunchError(Exception):
    """A worker, a reviewer or a check step could not be launched in an
    existing worktree (ruling R45): not the batch's fault, so the run
    halts with the batch parked."""

    def __init__(self, cwd, error):
        super().__init__(error)
        self.cwd = cwd
        self.error = error


def _launch_failed(label, cwd, e):
    """The error for an OSError raised while launching something in cwd:
    a missing cwd (the batch worktree vanished) is the batch's `worktree`
    exception; anything else (a wrapper not on PATH, ...) halts the run."""
    if not os.path.isdir(cwd):
        return _StageError('worktree', '%s could not run: the worktree %s is '
                           'gone (%s); run `git worktree prune`, then `git '
                           'worktree add <path> <branch>`, then clear this '
                           'exception' % (label, cwd, e))
    return _LaunchError(cwd, '%s: %s' % (label, e))


def read_status(path):
    """The status file as it is on disk now, every key present ({}
    defaults when there is no file).  The coordinator changes it through
    `--clear`, so the driver never keeps a copy."""
    for attempt in range(3):
        try:
            with open(path, encoding='utf-8') as f:
                status = json.load(f)
            break
        except FileNotFoundError:
            status = {}
            break
        except ValueError as e:
            if attempt == 2:
                raise StatusError('%s: not valid JSON (%s)' % (path, e)) from None
            time.sleep(0.2)   # a torn read of a file edited in place
        except OSError as e:
            raise StatusError('%s: unreadable (%s)' % (path, e)) from None
    if not isinstance(status, dict):
        raise StatusError('%s: not a driver status file (not a JSON object)'
                          % path)
    for key, empty in (('exceptions', {}), ('batches', {}), ('pools', {}),
                       ('running', {}),
                       ('current', None), ('parked', None),
                       ('stopped', None), ('stalled', None),
                       ('halted', None)):
        status.setdefault(key, empty)
        if isinstance(empty, dict) and not isinstance(status[key], dict):
            raise StatusError('%s: %r is not a JSON object' % (path, key))
    return status


def status_lock(status_path):
    """The flock on the sidecar `<status_path>.lock` that every
    read-modify-write of the status file holds, the driver's and
    `--clear`'s (ruling R37): the read, the change and the replace only."""
    return _tool('filelock').hold(status_path + '.lock', 'wave.py run')


def clear_exception(status_path, batch_id):
    """Remove status['exceptions'][batch_id] under the status lock:
    (0, the removed entry), or (2, None) when there is no such entry.
    StatusError when the file cannot be read."""
    if not os.path.isfile(status_path):
        return 2, None
    with status_lock(status_path):
        status = read_status(status_path)
        if batch_id not in status['exceptions']:
            return 2, None
        entry = status['exceptions'].pop(batch_id)
        status['updated_at'] = _now()
        _tool('wave')._save_json(status_path, status)
    return 0, entry


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
                 ignored=None, pool=None, wall=None, tokens=None,
                 tool_calls=None, transcript=None, undecided=None):
    """The templated progress.md entry for one batch.

    batch carries 'id'; verdicts maps address to a wave-state verdict or a
    (side, case) tuple; classes maps class to row count; review is
    (reviewed, outcome); credits None prints as 'credits unknown';
    ignored maps address to the (side, case) of a worker line set aside
    because the row was triage-decided (a line only when there are any).
    pool, wall (seconds), tokens ({input, cached, output}) and tool_calls
    each add a line when given (rulings R10, R61); undecided maps address
    to the (side, reason) of a row left for a probe (ruling R73)."""
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
    ] if ignored else []) + ([
        '- undecided (probe needed): ' + ', '.join(
            '%s %s' % (a, _side_case(v)[0]) for a, v in sorted(undecided.items())),
    ] if undecided else []) + [
        '- deciding evidence: ' + ', '.join(
            '%s %d' % (k, deciding.get(k, 0)) for k in wave.DECIDING_ORDER),
        '- review: %s, %s' % ('yes' if reviewed else 'no', outcome),
        '- commits %s' % commits,
        '- credits unknown' if credits is None else '- credits %.0f' % credits,
    ]
    if pool:
        lines.append('- pool %s' % pool)
    if wall is not None:
        lines.append('- wall %ds' % round(wall))
    if tokens is not None:
        lines.append('- tokens input %d, cached %d, output %d' % (
            tokens['input'], tokens['cached'], tokens['output']))
    if tool_calls is not None:
        lines.append('- tool calls %d' % tool_calls)
    lines += ['- ' + t for t in transcript or ()]
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


class _BatchContext(threading.local):
    """The fields of the batch a thread is running (a fresh set per
    thread): the Driver exposes them as properties."""

    def __init__(self):
        self.runs = []          # credits of this batch's worker and reviewer runs
        self.worker_ran = []    # this batch's worker runs: really ran
        self.ignored = {}
        self.undecided = {}     # address -> (side, reason) the worker listed (R73)
        self.probe = {}         # address key -> (side, case) the oracle passed
        self.frida_got = {}     # address key -> (status, detail) of its one Frida run
        self.pool = None        # the pool this batch runs on
        self.t0 = None          # clock() at the cut or resume (ruling R61)
        self.usage = {'input': 0, 'cached': 0, 'output': 0, 'tool_calls': 0}
        self.final = None       # the figures once the ledger has used them
        self.transcript_lines = []   # ledger lines about this batch's transcripts


def _per_thread(name):
    return property(lambda self: getattr(self._ctx, name),
                    lambda self, v: setattr(self._ctx, name, v))


class Driver:
    """The batch loop: cut and triage, brief, worker, mechanical checks
    (one fix round), review when it can matter (one fix round), record,
    close, push, ledger.  A failure the driver cannot settle is an
    exception: written to the status file and the ledger, sent to the AoE
    session, and the loop goes on with the next batch.

    The wave functions are attributes, so tests replace them.  Worker
    pools (Task 15) share the batches: a batch goes to the first pool with
    a free worker (`pick_pool`), each busy worker is one thread, and every
    wave-state mutation and repo-level git operation runs under the one
    driver-wide `_lock` (ruling R62); only the worker and reviewer runs,
    the oracle and Frida phases and the mechanical checks run in parallel.
    The per-batch fields (`_runs`, `_probe`, ...) are thread-local.
    `budget` is in credits over all pools: before each new batch the run
    stops when what it spent plus the next batch's estimate would pass it
    (ruling R38); `budgets` caps single pools.  A worker's 429 parks its
    pool, the WIP committed in the batch worktree; the next run resumes
    that batch on that pool first (ruling R39).  `queue` picks the state
    file's schema twin: wave1 or import (ruling R11)."""

    def __init__(self, repo, state_path, worker, reviewer, ledger_path,
                 status_path, aoe_session=None, seed=DEFAULT_SEED, budget=None,
                 workers=1, *, push=None, send=None, dry_run=False, gate=True,
                 base='first', log=None, frida=False, frida_map=None,
                 oracle_run=None, frida_run=None, frida_build=None, pools=None,
                 workers_by_pool=None, reviewers_by_pool=None, budgets=None,
                 queue='wave1', runner=None, clock=None, max_batches=None):
        wave = _tool('wave')
        if queue not in wave.QUEUES:
            raise ValueError('unknown queue %r (one of %s)' % (
                queue, ', '.join(wave.QUEUES)))
        self.repo = repo
        self.state_path = state_path
        self.queue = queue
        self.kind = wave.QUEUES[queue]['kind']
        # One pool by default, named glm; `worker` and `reviewer` serve
        # every pool that workers_by_pool / reviewers_by_pool leaves out.
        self.pools = list(pools) if pools else [Pool(
            'glm', 'claude-glm', 'glm-5.3-flash[1m]', workers,
            'glm-5.3-flash[1m]')]
        if len({p.name for p in self.pools}) != len(self.pools):
            raise ValueError('two pools have the same name')
        self.workers_by_pool = dict(workers_by_pool or {})
        self.reviewers_by_pool = dict(reviewers_by_pool or {})
        for p in self.pools:
            self.workers_by_pool.setdefault(p.name, worker)
            self.reviewers_by_pool.setdefault(p.name, reviewer)
        self.budgets = dict(budgets or {})
        for name in self.budgets:
            pool = next((p for p in self.pools if p.name == name), None)
            if pool is None:
                raise ValueError('--budget-%s: no pool named %s' % (name, name))
            if not pool_is_glm(pool):
                raise ValueError(
                    '--budget-%s is refused: pool %s runs %s, which Z.ai '
                    'credits do not price (only GLM turns are priced); '
                    'budget a GLM pool, or cap the run with --budget over '
                    'the GLM pools' % (name, name, pool.model))
        self.ledger_path = ledger_path
        self.status_path = status_path
        self.driver_dir = os.path.dirname(os.path.abspath(status_path))
        self.aoe_session = aoe_session
        self.seed = seed
        self.budget = budget
        self.workers = (workers if pools is None
                        else sum(p.workers for p in self.pools))
        self.dry_run = dry_run
        self.max_batches = max_batches
        self.gate = gate
        self.base = base
        self.size = wave.DEFAULT_SIZE
        self.category = wave.QUEUES[queue]['category']
        docs = os.path.join(repo, 'docs', 'wso')
        self.disagreements_path = os.path.join(docs, 'disagreements.json')
        self.triage_path = os.path.join(docs, 'triage.json')
        self.template_path = os.path.join(
            docs, os.path.basename(wave.QUEUES[queue]['template']))
        self.report_path = os.path.join(docs, 'xrec-report.md')
        self.probe_path = os.path.join(docs, 'probe-list.json')
        self.import_list_path = os.path.join(repo, wave.IMPORT_LIST_REL)
        self.calibration_path = os.path.join(docs, 'truth-calibration.json')
        truth = _tool('truth')
        self.frida = frida
        self.frida_map = frida_map
        self.oracle_run = truth.run_oracle if oracle_run is None else oracle_run
        self.frida_run = truth.frida_run if frida_run is None else frida_run
        # The per-batch x86 build the phase measures (spec B); injectable.
        self.frida_build = globals()['frida_build'] if frida_build is None else frida_build
        self.next_batch = wave.next_batch
        self.recompare = wave.recompare
        self._recompared = False
        self.render_brief = wave.render_brief
        self.record_verdict = wave.record_verdict
        self.close_batch = wave.close_batch
        self.sync_base = wave._sync_base
        self.write_scorecard = wave.write_scorecard
        self.cut_worktree = wave._cut_worktree
        self.apply_triage = wave.apply_triage
        self.worktree_for = wave.worktree_for
        self.load_state = wave.load_state
        self.mechanical_checks = mechanical_checks
        self.review_needed = review_needed
        self.push = self._git_push if push is None else push
        self.send = self._aoe_send if send is None else send
        self.log = (lambda m: print(m, flush=True)) if log is None else log
        self._read_status()   # a corrupt status file refuses here
        # Ruling R62: the one lock over every wave-state mutation and
        # repo-level git operation; never taken twice by one thread.
        self._lock = threading.Lock()
        # Git writes in progress (their thread idents), so a stop waits for
        # them: a signal must not land mid commit or fast-forward.
        self._write_cv = threading.Condition(threading.RLock())
        self._writers = {}         # thread ident -> nesting depth
        self._stop_deferred = None    # ident of the thread whose write defers the stop
        self._ctx = _BatchContext()
        self._runner = runner      # runs a callable on its own thread; None: threads
        self._clock = time.monotonic if clock is None else clock
        self._started = {}         # batch id -> clock() at its cut
        self._busy = {p.name: 0 for p in self.pools}
        self._cursor = 0           # pick_pool's round-robin position
        self._parked_now = set()   # pools that ended a batch parked this run
        self._threads = []
        self._pool_batch_credits = {p.name: [] for p in self.pools}
        self._imported = {}
        self._stopping = False     # SIGTERM or SIGINT arrived (ruling R72)
        self._retry_stage = {}     # cleared batch id -> the stage it failed at
        self._halt = False
        self._batch_credits = []   # this run's batches: credits or None
        self._no_transcript = 0    # batch exceptions in a row without one
        self._began = 0            # batches started in this run (--max-batches)

    _runs = _per_thread('runs')
    _worker_ran = _per_thread('worker_ran')
    _ignored = _per_thread('ignored')
    _undecided = _per_thread('undecided')
    _probe = _per_thread('probe')
    _frida_got = _per_thread('frida_got')

    @property
    def worker(self):
        return self.workers_by_pool[self.pools[0].name]

    @worker.setter
    def worker(self, value):
        self.workers_by_pool[self.pools[0].name] = value

    @property
    def reviewer(self):
        return self.reviewers_by_pool[self.pools[0].name]

    @reviewer.setter
    def reviewer(self, value):
        self.reviewers_by_pool[self.pools[0].name] = value

    def _pool_name(self):
        """The pool the current batch runs on."""
        return self._ctx.pool or self.pools[0].name

    # -- status, ledger, results ----------------------------------------

    def _read_status(self):
        """The status file as it is on disk now (the coordinator's
        `--clear` changes it between the driver's writes)."""
        return read_status(self.status_path)

    def _update_status(self, change):
        """Read-modify-write under the status lock: re-read the file,
        apply only this change, write it atomically.  What the
        coordinator removed stays removed."""
        with status_lock(self.status_path):
            status = self._read_status()
            change(status)
            if self.dry_run:
                status['dry_run'] = True
            status['updated_at'] = _now()
            _tool('wave')._save_json(self.status_path, status)
        return status

    def _stage(self, batch_id, stage):
        current = {'batch': batch_id, 'stage': stage, 'at': _now()}

        def apply(s):
            s['current'] = current
            s['running'][batch_id] = dict(current, pool=self._pool_name())
        self._update_status(apply)
        self.log('%s: %s' % (batch_id, stage))

    def _figures(self):
        """{wall, tokens, tool_calls} of the batch this thread runs
        (ruling R61), fixed at the first call so the ledger entry and the
        status agree; None for a batch that was never started."""
        if self._ctx.t0 is None:
            return None
        if self._ctx.final is None:
            u = self._ctx.usage
            self._ctx.final = {
                'wall': max(0.0, self._clock() - self._ctx.t0),
                'tokens': {k: u[k] for k in ('input', 'cached', 'output')},
                'tool_calls': u['tool_calls']}
        return self._ctx.final

    def _finish(self, batch_id, outcome, change=None, **extra):
        """Record the batch's outcome; `change` edits the status in the
        same write.  Any outcome but another park ends the batch's park."""
        entry = dict(outcome=outcome, credits=self._credits(), at=_now(), **extra)
        figures = self._figures()
        if figures:
            entry.update(wall=round(figures['wall'], 1),
                         tokens=dict(figures['tokens']),
                         tool_calls=figures['tool_calls'])

        def apply(s):
            old = s['batches'].get(batch_id)
            if isinstance(old, dict) and old.get('oracle_findings'):
                entry['oracle_findings'] = old['oracle_findings']
            s['batches'][batch_id] = entry
            s['running'].pop(batch_id, None)
            # current: the latest batch still in flight, if any.
            left = sorted(s['running'].items(), key=lambda kv: kv[1].get('at', ''))
            s['current'] = ({'batch': left[-1][0], 'stage': left[-1][1].get('stage'),
                             'at': left[-1][1].get('at')} if left else None)
            if outcome != 'parked':
                _unpark_everywhere(s, batch_id)
            if change:
                change(s)
        self._update_status(apply)

    def _append_ledger(self, text):
        with self._lock:
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
        if self._stopping:     # stopped before this launch: start nothing
            raise _Stop()
        try:
            res = worker.run(brief, cwd)
        except OSError as e:
            raise _launch_failed(label, cwd, e) from None
        if self._stopping:     # killed with its driver: nothing to record
            raise _Stop()
        self._runs.append(res.credits)
        if label.startswith('worker'):
            self._worker_ran.append(worker_ran(res))
        if res.transcript:
            try:
                tokens, calls = usage_all(res.transcript)
            except OSError:
                tokens, calls = None, 0
            if tokens:
                u = self._ctx.usage
                for k in ('input', 'cached', 'output'):
                    u[k] += tokens[k]
                u['tool_calls'] += calls
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
        # Last, after the bookkeeping above: a transcript violation is an
        # exception, but the run it found still counts as a worker run
        # for the breaker and its evidence is saved (ruling R68).
        self._scan_transcript(batch_id, label, res)
        return res

    def _scan_transcript(self, batch_id, label, res):
        """Every run's transcript, worker, fix rounds and reviewer: a wso/
        ref, a launch outside the house runners or a history rewrite is a
        past call nobody can undo, so a `transcript` exception with no fix round; a whole-file
        read is a ledger line only; a missing transcript is a ledger line."""
        lines = self._ctx.transcript_lines
        if not res.transcript:
            lines.append('transcript: none (%s)' % label)
            return
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        covered = sorted({r['file'] for r in batch.get('rows', [])
                          if r.get('file')})
        fatal = []
        for kind, text, f in transcript_findings(res.transcript, covered,
                                              self._is_tracked):
            if kind == 'read':
                line = 'transcript: whole read of %s' % f
                if line not in lines:
                    lines.append(line)
            elif kind == 'unreadable':
                lines.append('transcript: unreadable (%s)' % label)
            else:
                fatal.append('%s (%s)' % (text, label))
        if fatal:
            raise _StageError('transcript', ' | '.join(fatal))

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
                self.send(self._exception_line(batch_id, stage))
            except Exception as e:   # a failed send must not stop the loop
                self.log('send failed: %r' % (e,))
        return 'exception'

    def _exception_line(self, batch_id, stage):
        """The one line sent over aoe (ruling R71): the batch, the stage
        and where to read more; never problem text, which comes from a
        worker's work."""
        return 'driver: %s exception at %s; see %s' % (
            batch_id, stage, self.status_path)

    def _commit_wip(self, worktree, message=WIP_MESSAGE):
        """Commit what the batch worktree holds uncommitted, as
        `git add -A && git commit -m WIP_MESSAGE` (ruling R39): (sha, None),
        (None, None) when it is clean, or (None, why) when nothing was
        committed.  Never in the driver's own worktree."""
        if not worktree:
            return None, 'no batch worktree'
        if os.path.realpath(worktree) == os.path.realpath(self.repo):
            return None, ("%s is the driver's own worktree, not a batch "
                          "worktree" % worktree)

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        st = git('status', '--porcelain')
        if st.returncode != 0:
            return None, 'git status failed: %s' % _one_line(st.stderr)
        if not st.stdout.strip():
            return None, None
        for args in (('add', '-A'), ('commit', '-q', '-m', message)):
            r = git(*args)
            if r.returncode != 0:
                return None, 'git %s failed: %s' % (
                    args[0], _one_line((r.stderr or '') + (r.stdout or '')))
        return git('rev-parse', 'HEAD').stdout.strip() or None, None

    def _park(self, batch_id, reason, worktree):
        """Commit the WIP in the batch worktree, write the pool's
        `parked`; the pool takes no more batches, the run exits 3 once the
        other pools are done, and the next run resumes this batch first."""
        wip, err = self._commit_wip(worktree)
        extra = {}
        if wip:
            extra['wip'] = wip
        if err:
            extra['wip_error'] = err
            self.log('%s: WIP not committed: %s' % (batch_id, err))
        parked = {'at': _now(), 'reason': reason}
        name = self._pool_name()

        def mark(s):
            _unpark_everywhere(s, batch_id)
            info = s['pools'].setdefault(name, {})
            m = parked_map(info.get('parked'))
            m[batch_id] = parked
            _set_parked(info, m)
        self._finish(batch_id, 'parked', change=mark, **extra)
        self.log('%s: parked (%s)%s' % (
            batch_id, reason, '; WIP committed as %s' % wip[:8] if wip else ''))
        return 'parked'

    def _settle(self, batch_id, res, label, worktree):
        """'parked' or 'exception' when a run gave nothing to check.  An
        auth or billing error parks the batch and halts the run (R46)."""
        if res.timed_out:
            return self._park(batch_id, 'timeout', worktree)
        if res.api_error:
            reason = 'api error: %s' % res.api_error
            out = self._park(batch_id, reason, worktree)
            self._halt_run(reason)
            return out
        if res.rate_limited:
            return self._park(batch_id, '%s rate limited' % label, worktree)
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
        self._undecided = {}
        try:
            parsed = parse_report(report)
        except ReportError as e:
            return {}, ['report: %s; fix that line under ## Verdicts (one '
                        '"verdict <0xADDR> <fork|ours|neither> case <id>" '
                        'per row; ## Verdicts holds verdict lines only, put '
                        'notes under ## Notes)' % e]
        try:
            listed = parse_undecided(report)
        except ReportError as e:
            return {}, ['report: %s; fix that line under ## Undecided (one '
                        '"undecided <0xADDR> <fork|neither> <one-line '
                        'reason>" per row)' % e]
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
        # Ruling R48: a probe pass is the verdict, whatever the report says.
        for addr, sc in self._probe_verdicts(batch).items():
            if addr in verdicts and verdicts[addr] != sc:
                self.log('%s: %s decided by the oracle probe (%s); the '
                         'report line is replaced' % (batch_id, addr, sc[1]))
            verdicts[addr] = sc
        # A row the driver's own probe decided, or triage decided, is not
        # undecided whatever the report says.
        for addr in sorted(listed):
            if key(addr) in auto:
                del listed[addr]
        probed = {key(a) for a in self._probe_verdicts(batch)}
        for addr in sorted(listed):
            if key(addr) in probed:
                self.log('%s: %s decided by the oracle probe; its undecided '
                         'line is dropped' % (batch_id, addr))
                del listed[addr]
        self._undecided = {} if self.kind == 'import' else dict(listed)
        try:
            kw = {'kind': 'import'} if self.kind == 'import' else {}
            kw['probe'] = sorted(self._probe)
            kw['undecided'] = sorted(listed)
            return verdicts, self.mechanical_checks(
                dict(batch, id=batch_id), worktree, verdicts, **kw)
        except OSError as e:
            raise _launch_failed('the mechanical checks', worktree, e) from None

    # -- measurement (tools/truth.py) -----------------------------------

    def _open_rows(self, batch):
        """The rows a worker must decide: class ambiguous, or no class and no
        verdict yet (the same test as the mechanical checks)."""
        decided = batch.get('verdicts', {})
        return [r for r in batch.get('rows', [])
                if r.get('class') == 'ambiguous' or (
                    'class' not in r and r['address'] not in decided
                    and not r.get('verdict'))]

    def _probe_verdicts(self, batch):
        """{address as in the batch rows: (side, case)} for the rows the
        oracle probe passed (ruling R48: side ours, case probe-<batch>-<addr>)."""
        out = {}
        for r in batch.get('rows', []):
            hit = self._probe.get(_tool('wave')._addr_key(r['address']))
            if hit:
                out[r['address']] = hit
        return out

    def _measured_for(self, addr, side):
        """The `measured` a verdict is recorded with, or None."""
        key = _tool('wave')._addr_key(addr)
        if key in self._probe:
            return 'probe'
        if side == 'ours' and self._frida_got.get(key, ('',))[0] == 'equal':
            return 'frida'
        return None

    def _oracle_phase(self, batch_id, batch, worktree):
        """Before the worker: draft, build and run an oracle case for the
        open rows that are pure.  Passes become verdicts (self._probe), a fail
        is a finding for the brief, crash and pending leave the row alone.
        Returns the text to put in front of the brief ('' for none).  A batch
        with no pure row skips the build and the run; an oracle that cannot
        run is logged and the batch goes on unmeasured."""
        self._probe = {}
        truth = _tool('truth')
        wave = _tool('wave')
        decided = {wave._addr_key(a) for a in batch.get('verdicts', {})}
        rows = [r for r in self._open_rows(batch)
                if wave._addr_key(r['address']) not in decided]
        carried = self._carry_probe_passes(batch_id, worktree)
        saved = (self._read_status()['batches'].get(batch_id) or {}).get('oracle_findings') or []
        again = ('## Oracle findings from the earlier run\n\n' + '\n'.join(saved) + '\n\n') if saved else ''
        if carried is not None:
            # A resume: the probe file is already committed, so its passes
            # stand and the oracle does not run again (it would rewrite it).
            for k in carried:
                self._probe[k] = ('ours', 'probe-%s-%s' % (batch_id, k.upper()))
            self.log('%s: oracle: %d earlier probe pass(es) carried forward' % (
                batch_id, len(carried)))
            if not carried:
                return again
            return again + ('## Measured before you started\n\nThe oracle probe passed these rows earlier '
                    '(the driver records each as `ours`; cite `corroborated by WSO at <sha>`):\n'
                    + ''.join('- 0x%s: case %s\n' % (k.upper(), self._probe[k][1]) for k in sorted(carried))
                    + '\n')

        def read(address):
            text, _err = wave._read_evidence(self.repo, batch['branch'], address)
            return text
        try:
            cases, _why = truth.oracle_candidates(rows, read)
        except Exception as e:
            self.log('%s: oracle path skipped: %s' % (batch_id, _one_line(repr(e), 300)))
            return again
        if not cases:
            return again
        try:
            self._stage(batch_id, 'oracle')
            got = self.oracle_run(batch_id, worktree, cases)
            got = {wave._addr_key(a): v for a, v in got.items()}
        except Exception as e:   # measurement is best effort
            self.log('%s: oracle path skipped: %s' % (batch_id, _one_line(repr(e), 300)))
            self._settle_oracle_files(batch_id, worktree, cases, ())
            return again
        self._settle_oracle_files(batch_id, worktree, cases, [
            c for c in cases if got.get(wave._addr_key(c['address']), ('',))[0] == 'pass'])
        key = wave._addr_key
        passed, failed = [], []
        for c in cases:
            status, detail = got.get(key(c['address']), ('pending', 'no result'))
            self.log('%s: oracle %s %s (%s)' % (batch_id, c['address'], status, detail))
            if status == 'pass':
                case = 'probe-%s-%s' % (batch_id, key(c['address']).upper())
                self._probe[key(c['address'])] = ('ours', case)
                passed.append((c['address'], case, detail))
            elif status == 'fail':
                failed.append((c['address'], c['name'], detail, c))
        lines = []
        if passed:
            lines.append('The oracle probe passed these rows (ours matches the original over a '
                         'swept domain). The driver records each as `ours`, with the case shown; '
                         'write the evidence file with the exact phrase '
                         '`corroborated by WSO at <sha>` and cite that case:')
            lines += ['- %s: case %s (%s)' % p for p in passed]
        if failed:
            lines.append('The oracle probe FAILED these rows (ours and the original disagree). '
                         'Decide them with that in mind. The failing case is not kept in the '
                         'tree; its source, each integer argument swept over {min, -1, 0, 1, 7, max}:')
            for address, name, detail, c in failed:
                lines.append('- %s %s: case wave/%s/%s, %s' % (
                    address, name, batch_id, key(address)[0:].upper(), detail))
                lines.append('```cpp\n' + truth.draft_case(
                    c['address'], c['name'], c['signature'], batch_id).rstrip() + '\n```')
        if failed:
            texts = [l for l in lines[lines.index(next(x for x in lines if x.startswith('The oracle probe FAILED'))):]]
            self._update_status(lambda s: s['batches'].setdefault(batch_id, {}).__setitem__(
                'oracle_findings', texts))
        text = ('## Measured before you started\n\n' + '\n'.join(lines) + '\n\n') if lines else ''
        return text if failed else again + text

    def _tracked(self, worktree, rel):
        r = subprocess.run(['git', '-C', worktree, 'ls-files', '--error-unmatch', '--', rel],
                           capture_output=True, text=True)
        return r.returncode == 0

    def _carry_probe_passes(self, batch_id, worktree):
        """None when the batch has no committed probe file; else the address
        keys of the passing cases in it."""
        rel = 'oracle/cases/wave-%s.cpp' % batch_id
        if not self._tracked(worktree, rel):
            return None
        try:
            with open(os.path.join(worktree, rel), encoding='utf-8') as f:
                text = f.read()
        except OSError:
            return None
        return set(m.group(1).lower() for m in re.finditer(
            r'ORACLE_CASE\("wave/%s/([0-9A-Fa-f]+)"' % re.escape(batch_id), text))

    def _settle_oracle_files(self, batch_id, worktree, cases, passed):
        """Leave in the batch worktree only what is worth keeping of the
        oracle path's files: the case file with the passing cases (their
        permanent oracle cases) and its oracle/CMakeLists.txt line, committed;
        a failing, crashing or unrun case never stays in the oracle suite, and
        with nothing passing both files are put back."""
        truth = _tool('truth')
        cmake = os.path.join(worktree, 'oracle', 'CMakeLists.txt')
        cases_dir = os.path.join(worktree, 'oracle', 'cases')
        path = os.path.join(cases_dir, 'wave-%s.cpp' % batch_id)

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        try:
            if not os.path.exists(path):
                return
            if not passed:
                # Tracked files (an earlier committed probe) are restored,
                # never deleted; only an untracked case file is unlinked.
                if self._tracked(worktree, 'oracle/cases/wave-%s.cpp' % batch_id):
                    git('checkout', 'HEAD', '--', 'oracle/cases/wave-%s.cpp' % batch_id,
                        'oracle/CMakeLists.txt')
                else:
                    os.unlink(path)
                    git('checkout', 'HEAD', '--', 'oracle/CMakeLists.txt')
                return
            if len(passed) != len(cases):
                truth.write_cases(batch_id, passed, cases_dir)
            names = ['oracle/cases/wave-%s.cpp' % batch_id, 'oracle/CMakeLists.txt']
            if git('status', '--porcelain', '--', *names).stdout.strip():
                git('add', '--', *names)
                r = git('commit', '-q', '-m', 'truth: oracle probes for %s (pure rows; the driver)' % batch_id,
                        '--', *names)
                if r.returncode != 0:
                    self.log('%s: oracle files not committed: %s' % (
                        batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))
        except OSError as e:
            self.log('%s: oracle files left as they are: %r' % (batch_id, e))

    def _frida_phase(self, batch_id, batch, worktree, verdicts):
        """After the worker, with --frida: one batched Frida differential run
        (all three scenarios) over the open rows the oracle did not decide, or
        for an import batch over every row.  Wave 1: equal confirms a verdict
        of ours (measured frida); different where the verdict says ours, or
        equal where it says the fork, is a finding.  Import: an equal row's
        evidence is rewritten to kind: trace (and committed on the batch
        branch), a different row is a finding, a row no scenario exercised is
        a finding that tells the worker to revert and park it.  It runs once
        per batch, never again.  Returns the findings."""
        self._frida_got = {}
        if not self.frida or self.dry_run:
            return []
        truth = _tool('truth')
        wave = _tool('wave')
        key = wave._addr_key
        importing = self.kind == 'import'
        rows = list(batch.get('rows', [])) if importing else [
            r for r in self._open_rows(batch) if key(r['address']) not in self._probe]
        if not rows:
            return []
        checkout, map_path, why = self._frida_target(batch_id, worktree)
        sigs = {}
        for r in rows:
            text, _err = wave._read_evidence(self.repo, batch['branch'], r['address'])
            sig = truth.evidence_signature(text) if text else None
            if sig:
                sigs[r['address']] = sig
        traces = {}

        def run_frida(batch_id, table, scenarios):
            # Ours runs the batch worktree's own build (the map's RVAs).
            traces.update(self.frida_run(batch_id, table, scenarios,
                                         build_checkout=checkout))
            return traces
        if why:
            # A phase error (the build failed): nothing measured, no crash.
            self.log('%s: frida phase error: %s' % (batch_id, why))
            got = {key(r['address']): ('not measured', why) for r in rows}
        else:
            try:
                with open(map_path, encoding='utf-8', errors='replace') as f:
                    map_text = f.read()
                self._stage(batch_id, 'frida')
                got = truth.frida_batch(batch_id, rows, sigs, map_text, run_frida=run_frida,
                                        headers=truth._our_headers(worktree))
                got = {key(a): v for a, v in got.items()}
            except Exception as e:
                self.log('%s: frida path skipped: %s' % (batch_id, _one_line(repr(e), 300)))
                return []
        self._frida_got = {key(r['address']): got.get(key(r['address']), ('skipped', 'no result'))
                           for r in rows}
        sides = {key(a): s for a, (s, _c) in verdicts.items()}
        mb = _merge_base(worktree, self.base) if importing else None
        findings, written = [], []
        for r in rows:
            k = key(r['address'])
            status, detail = got.get(k, ('skipped', 'no result'))
            self.log('%s: frida %s %s (%s)' % (batch_id, r['address'], status, detail))
            if importing:
                # The phase never lowers evidence: a row already at replay
                # or oracle keeps its file in every outcome.
                kept = self._evidence_kind(worktree, r['address']) in wave.IMPORT_KINDS
                if status == 'equal':
                    if not kept:
                        err = self._trace_evidence(batch_id, worktree, r, detail, traces, mb)
                        if err:
                            findings.append(err)
                        else:
                            written.append(r['address'])
                elif status == 'different' and kept:
                    findings.append(
                        '%s: the Frida trace diff shows the imported body different from the '
                        'original (%s); its evidence (kind %s) stands and nothing is undone, '
                        'deleted or parked in this batch; note the difference in your report'
                        % (r['address'], detail, self._evidence_kind(worktree, r['address'])))
                elif status == 'different':
                    findings.append(
                        '%s: the Frida trace diff shows the imported body different from the '
                        'original (%s); the phase does not run again in this batch, so park '
                        'it: %s' % (r['address'], detail, park_text(
                            r['address'], mb, 'frida different: %s' % detail)))
                elif status == 'not-exercised' and not kept:
                    findings.append(UNMEASURED % (r['address'], park_text(
                        r['address'], mb, UNMEASURED_NEEDS)))
                elif status == 'skipped' and not kept:
                    findings.append('%s: no Frida hook (%s); park it: %s' % (
                        r['address'], detail, park_text(r['address'], mb,
                                                        'no Frida hook: %s' % detail)))
                continue
            side = sides.get(k)
            if status == 'equal':
                if side and side != 'ours':
                    findings.append('%s: the Frida trace diff shows ours equal to the original (%s) '
                                    'but the verdict is %s; recheck it' % (r['address'], detail, side))
            elif status == 'different' and side == 'ours':
                findings.append('%s: the Frida trace diff shows ours different from the original '
                                '(%s) but the verdict is ours; recheck it' % (r['address'], detail))
        if written:
            self._commit_trace_evidence(batch_id, worktree, written)
        return findings

    def _frida_target(self, batch_id, worktree):
        """(checkout, map path, phase error or None): what the Frida phase
        measures.  By default the batch worktree is built for x86 in its own
        Windows checkout (self.frida_build), so the bodies under test are the
        batch's and the map's RVAs are theirs; --frida-map overrides that
        with an existing build (the exe beside the map), for hand runs.  A
        failed build is a phase error, never a crash."""
        if self.frida_map:
            checkout = _map_checkout(self.frida_map)
            if not os.path.isfile(self.frida_map):
                return None, None, '--frida-map %s does not exist' % self.frida_map
            if checkout is None:
                return None, None, ('--frida-map %s is not where an x86 build writes its map '
                                    '(<checkout>/%s)' % (self.frida_map, X86_MAP_REL))
            self.log('%s: frida: --frida-map given: measuring the build in %s, not a build '
                     'of %s' % (batch_id, checkout, worktree))
            return checkout, self.frida_map, None
        self._stage(batch_id, 'frida build')
        log_path = os.path.join(self.driver_dir, batch_id, 'frida-build.log')
        try:
            os.makedirs(os.path.dirname(log_path), exist_ok=True)
            checkout, map_path = self.frida_build(worktree, log_path=log_path)
        except (Exception, SystemExit) as e:
            return None, None, 'the x86 build of %s failed: %s (log %s)' % (
                worktree, _one_line(str(e) or repr(e), 300), log_path)
        self.log('%s: frida: x86 build of %s in %s' % (batch_id, worktree, checkout))
        return checkout, map_path, None

    def _evidence_kind(self, worktree, address):
        """The `kind:` of a row's evidence file in the worktree, or None."""
        wave = _tool('wave')
        try:
            with open(os.path.join(worktree, wave._evidence_rel(address)),
                      encoding='utf-8') as f:
                m = wave.KIND_RE.search(f.read())
        except OSError:
            return None
        return m.group(1) if m else None

    def _trace_evidence(self, batch_id, worktree, row, detail, traces, mb):
        """Rewrite one equal import row's evidence to kind: trace; None, or
        the finding that says why it could not be."""
        truth = _tool('truth')
        wave = _tool('wave')
        scenario = detail[len(truth.EQUAL_PREFIX):].split(', ')[0]
        t = traces.get(scenario)
        t = t if isinstance(t, dict) else {}
        orig, ours = t.get('orig_path'), t.get('ours_path')
        if not (orig and ours and os.path.isfile(orig) and os.path.isfile(ours)):
            return ('%s: measured equal in %s but its trace files are missing; the phase does '
                    'not run again, so write its evidence as kind trace (a legal trace_source) or '
                    'better yourself, or park it: %s' % (
                        row['address'], scenario, park_text(
                            row['address'], mb, 'frida trace files missing')))
        path = os.path.join(worktree, wave._evidence_rel(row['address']))
        try:
            _write_trace_evidence(path, '%s-%s' % (batch_id, scenario), orig, ours)
        except (OSError, ValueError) as e:
            return ('%s: its evidence could not be rewritten to kind: trace (%s); the phase does '
                    'not run again, so write the evidence as kind trace (a legal trace_source) or '
                    'better yourself, or park it: %s' % (
                        row['address'], e, park_text(
                            row['address'], mb, 'evidence not rewritable: %s' % e)))
        return None

    def _commit_trace_evidence(self, batch_id, worktree, addresses):
        wave = _tool('wave')
        rels = [wave._evidence_rel(a) for a in addresses]

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        if not git('status', '--porcelain', '--', *rels).stdout.strip():
            return
        git('add', '--', *rels)
        r = git('commit', '-q', '-m', 'frida: trace evidence for %d rows of %s' % (
            len(rels), batch_id), '--', *rels)
        if r.returncode != 0:
            self.log('%s: trace evidence not committed: %s' % (
                batch_id, _one_line((r.stderr or '') + (r.stdout or ''))))

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
                for k, (side, case) in sorted(merged.items()))
            or '(none: an import batch has no verdicts)')
        res = self._run(self._reviewer_of(self._pool_name()), brief, worktree,
                        batch_id, label)
        out = self._settle(batch_id, res, label, worktree)
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

    def _is_tracked(self, name):
        """True when `name` is a file tracked in the driver's worktree."""
        return self._git('ls-files', '--error-unmatch', '--', name
                         ).returncode == 0

    def _commits(self, branch):
        """'<merge-base>..<tip> (n commits)' of the batch branch over base."""
        mb = self._git('merge-base', self.base, branch).stdout.strip()
        tip = self._git('rev-parse', '--verify', branch).stdout.strip()
        if not mb or not tip:
            return 'unknown (%s..%s)' % (self.base, branch)
        n = self._git('rev-list', '--count', '%s..%s' % (mb, tip)).stdout.strip()
        return '%s..%s (%s commit%s)' % (mb[:8], tip[:8], n, '' if n == '1' else 's')

    def _worker_of(self, name):
        return self.workers_by_pool.get(name)

    def _reviewer_of(self, name):
        return self.reviewers_by_pool.get(name)

    def _figure_kw(self):
        """ledger_entry's pool, wall, tokens and tool_calls (R10, R61)."""
        f = self._figures() or {}
        return dict(pool=self._pool_name(), wall=f.get('wall'),
                    tokens=f.get('tokens'), tool_calls=f.get('tool_calls'),
                    transcript=list(self._ctx.transcript_lines))

    def _ledger_for(self, batch_id, verdicts, review):
        batch = self.load_state(self.state_path)['batches'][batch_id]
        self._append_ledger(ledger_entry(
            dict(batch, id=batch_id), verdicts, self._classes(batch_id, batch),
            review, self._commits(batch['branch']), self._credits(),
            ignored=self._ignored, undecided=self._undecided,
            **self._figure_kw()))

    def _record_pool(self, batch_id, name):
        """Write batch['pool'] = name into the state entry (ruling R10)."""
        with self._lock:
            state = self.load_state(self.state_path)
            batch = state['batches'].get(batch_id)
            if batch is not None and batch.get('pool') != name:
                batch['pool'] = name
                _tool('wave').save_state(self.state_path, state)

    UNION_LINE = 'oracle/CMakeLists.txt merge=union'

    def _union_merge_attributes(self):
        """Make oracle/CMakeLists.txt merge as a union (ruling R64): every
        batch with passing probes adds its case line after the same anchor,
        so parallel batches would conflict.  Written once, to the common git
        dir's info/attributes, only when the exact line is missing."""
        r = self._git('rev-parse', '--git-common-dir')
        if r.returncode != 0 or not r.stdout.strip():
            return
        common = r.stdout.strip()
        if not os.path.isabs(common):
            common = os.path.join(self.repo, common)
        path = os.path.join(common, 'info', 'attributes')
        try:
            with open(path, encoding='utf-8') as f:
                text = f.read()
        except FileNotFoundError:
            text = ''
        except OSError as e:
            self.log('git attributes not read: %r' % (e,))
            return
        if self.UNION_LINE in text.splitlines():
            return
        try:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'a', encoding='utf-8') as f:
                f.write(('' if not text or text.endswith('\n') else '\n')
                        + self.UNION_LINE + '\n')
        except OSError as e:
            self.log('git attributes not written: %r' % (e,))

    def _sync_branch(self, branch, worktree):
        """None, or why not: with batches in parallel the base may have
        moved since this branch was cut, and close only fast-forwards, so
        the base is merged into the branch first (the files are disjoint;
        a no-op when the base is already an ancestor).  The base is pulled
        first (ruling R17).  Under the lock."""
        why = self.sync_base(self.repo, self.base)
        if why:
            return why
        if self._git('merge-base', '--is-ancestor', self.base,
                     branch).returncode == 0:
            return None
        self._union_merge_attributes()
        r = subprocess.run(['git', '-C', worktree, 'merge', '--no-edit', '-q',
                            self.base], capture_output=True, text=True)
        if r.returncode != 0 and self._only_map_conflicts(worktree):
            # A worker's own docs/MAP.md edit: the close regenerates the map
            # on the merged tree anyway, so the base's copy stands.
            r = self._git_in(worktree, 'checkout', '--theirs', '--', 'docs/MAP.md')
            if r.returncode == 0:
                r = self._git_in(worktree, 'add', '--', 'docs/MAP.md')
            if r.returncode == 0:
                r = self._git_in(worktree, 'commit', '-q', '--no-edit')
        if r.returncode == 0:
            self.log('%s: %s merged into it before the close' % (branch, self.base))
            return None
        subprocess.run(['git', '-C', worktree, 'merge', '--abort'],
                       capture_output=True, text=True)
        return 'merging %s into %s failed: %s' % (
            self.base, branch, _one_line((r.stderr or '') + (r.stdout or '')))

    STOP_WAIT = 30     # seconds a stop waits for a git write in progress

    @contextlib.contextmanager
    def _writing(self):
        """A git write is in progress on this thread: a stop waits for it
        (bounded by STOP_WAIT), and a stop that lands on this very thread
        waits for the write's end and is raised there."""
        tid = threading.get_ident()
        with self._write_cv:
            self._writers[tid] = self._writers.get(tid, 0) + 1
        try:
            yield
        finally:
            with self._write_cv:
                n = self._writers[tid] - 1
                if n:
                    self._writers[tid] = n
                else:
                    del self._writers[tid]
                self._write_cv.notify_all()
                deferred = not n and self._stop_deferred == tid
            if deferred:
                self._stop_deferred = None
                raise _Stop()

    def _on_stop(self, signum, _frame):
        """SIGTERM/SIGINT: kill the workers, let a git write in progress
        finish (bounded), then raise _Stop.  Only locks that are re-entrant
        are taken here: the signal may land while this thread holds them."""
        self._stopping = True
        kill_live_groups()
        with self._write_cv:
            if threading.get_ident() in self._writers:
                # Raised on this thread when its write ends (the main
                # thread's cut, adoption and recompare are writes too).
                self._stop_deferred = threading.get_ident()
                return
            deadline = time.monotonic() + self.STOP_WAIT
            while self._writers:
                left = deadline - time.monotonic()
                if left <= 0:
                    break
                self._write_cv.wait(left)
        raise _Stop()

    def _git_in(self, worktree, *args):
        return subprocess.run(['git', '-C', worktree, *args],
                              capture_output=True, text=True)

    def _only_map_conflicts(self, worktree):
        r = self._git_in(worktree, 'diff', '--name-only', '--diff-filter=U')
        return r.returncode == 0 and r.stdout.split() == ['docs/MAP.md']

    def _regen_map(self, worktree):
        """None, or why not: regenerate docs/MAP.md on the merged tree
        (batches no longer carry it, so parallel closes cannot conflict)
        and commit it by name when it changed; `gen_map.py --check` passes
        after the fast-forward.  Under the lock."""
        # The worktree's own tool, the one the gate's check runs, so the two
        # cannot disagree.
        gen = os.path.join(worktree, 'tools', 'gen_map.py')
        if not os.path.isfile(os.path.join(worktree, 'docs', 'MAP.md')):
            return None        # no map to keep current
        if subprocess.run([sys.executable, '-I', gen, '--repo', worktree,
                           '--check'], capture_output=True,
                          text=True).returncode == 0:
            return None        # current already (the commit line aside)
        r = subprocess.run([sys.executable, '-I', gen, '--repo', worktree],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'gen_map.py exit %s' % r.returncode
        st = self._git_in(worktree, 'status', '--porcelain', '--', 'docs/MAP.md')
        if st.returncode != 0:
            return _one_line(st.stderr) or 'git status failed'
        if not st.stdout.strip():
            return None
        r = self._git_in(worktree, 'add', '--', 'docs/MAP.md')
        if r.returncode == 0:
            r = self._git_in(
                worktree, 'commit', '-q', '-m',
                'map: regenerate docs/MAP.md on the merged tree\n\n%s\n'
                % STATE_COMMIT_TRAILER, '--', 'docs/MAP.md')
        if r.returncode != 0:
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'exit %s' % r.returncode
        return None

    def _commit_state(self, batch_id):
        """After a close, before the push (rulings R66, R71): commit the
        wave state, the regenerated scorecard and the probe list (ruling
        R73, when it exists), and only those, on the base, in the
        worktree that has it checked out, staged by name.  The import
        list goes with either queue's state, and the import queue
        commits its state file (no scorecard).  Skipped in a dry run, with nothing to
        commit, or when a file lies outside that worktree.  Under the
        lock.  Whatever goes wrong is logged, those paths are unstaged,
        one aoe line is sent, and the push still happens."""
        if self.dry_run:
            return
        rels = []
        try:
            err = self._commit_state_files(batch_id, rels)
        except Exception as e:
            err = 'raised %r' % (e,)
        if err is None:
            return
        self.log('%s: the state commit failed: %s' % (batch_id, err))
        try:
            wt = self.worktree_for(self.repo, self.base)
            if wt and rels:
                subprocess.run(['git', '-C', wt, 'reset', '-q', '--', *rels],
                               capture_output=True, text=True)
            self.send(self._exception_line(batch_id, 'state-commit'))
        except Exception as e:
            self.log('%s: state commit cleanup failed: %r' % (batch_id, e))

    def _commit_state_files(self, batch_id, rels):
        """None, or why not; `rels` is filled with the paths staged."""
        wt = self.worktree_for(self.repo, self.base)
        if wt is None:
            return 'no worktree has %s' % self.base
        top = os.path.realpath(wt)
        if self.kind == 'verdicts':
            # disagreements.json too: add and diff --cached skip it unchanged
            paths = [self.state_path, self.report_path, self.probe_path,
                     self.disagreements_path]
        else:    # the import queue: its state (never a scorecard)
            paths = [self.state_path]
        paths.append(self.import_list_path)   # ours-stub rows leave wave 1 for it
        for path in paths:
            if not os.path.exists(path):
                continue
            rel = os.path.relpath(os.path.realpath(path), top)
            if rel != os.pardir and not rel.startswith(os.pardir + os.sep) \
                    and rel not in rels:
                rels.append(rel)
        if not rels:
            return None

        def git(*args):
            return subprocess.run(['git', '-C', wt, *args],
                                  capture_output=True, text=True)

        def fail(r):
            return _one_line((r.stderr or '') + (r.stdout or '')) or \
                'exit %s' % r.returncode

        r = git('add', '--', *rels)
        if r.returncode != 0:
            return fail(r)
        if git('diff', '--cached', '--quiet', '--', *rels).returncode == 0:
            return None
        sides = [v.get('side') for v in self.load_state(self.state_path)[
            'batches'][batch_id]['verdicts'].values()]
        msg = '%s: close %s (ours %d, fork %d, neither %d)\n\n%s\n' % (
            'wave-state' if self.kind == 'verdicts' else 'import-state',
            batch_id, sides.count('ours'), sides.count('fork'),
            sides.count('neither'), STATE_COMMIT_TRAILER)
        r = git('commit', '-q', '-m', msg, '--', *rels)
        return fail(r) if r.returncode != 0 else None

    def _wip_tip(self, batch_id, branch):
        """The branch tip's sha when it is the driver's own unpushed WIP
        commit: the one this batch's park recorded, with WIP_MESSAGE, not
        in the base and on no remote branch.  Else None."""
        entry = self._read_status()['batches'].get(batch_id)
        wip = entry.get('wip') if isinstance(entry, dict) else None
        tip = self._git('rev-parse', '--verify', branch).stdout.strip()
        if not wip or tip != wip:
            return None
        if self._git('log', '-1', '--format=%s', tip).stdout.strip() != WIP_MESSAGE:
            return None
        if self._git('merge-base', '--is-ancestor', tip, self.base).returncode == 0:
            return None
        if self._git('branch', '-r', '--contains', tip).stdout.strip():
            return None
        return tip

    def _undo_wip(self, worktree, tip):
        """`git reset --soft HEAD~1` of the driver's own WIP commit, made
        in the batch worktree when that worktree's HEAD is `tip`: `tip`, or
        None when nothing was undone."""
        if not worktree or os.path.realpath(worktree) == os.path.realpath(
                self.repo):
            return None

        def git(*args):
            return subprocess.run(['git', '-C', worktree, *args],
                                  capture_output=True, text=True)
        if git('rev-parse', 'HEAD').stdout.strip() != tip:
            return None
        r = git('reset', '--soft', 'HEAD~1')
        if r.returncode != 0:
            self.log('WIP commit not undone: %s' % _one_line(
                (r.stderr or '') + (r.stdout or '')))
            return None
        return tip

    def _resume_preamble(self, batch_id, batch, why, worktree=None):
        key = _tool('wave')._addr_key
        recorded = sorted(batch.get('verdicts', {}).items(),
                          key=lambda kv: key(kv[0]))
        tip = self._wip_tip(batch_id, batch['branch'])
        # Ruling R44: the driver, not the worker, undoes its own WIP commit
        # (a worker's `git reset` is a history exception), so an adopted fork
        # body in it can get its Imported-From: trailer.
        if tip is not None:
            tip = self._undo_wip(worktree, tip)
        reset = '' if tip is None else (
            ' The driver undid its own `%s` commit before resuming: the '
            'previous work is staged in the worktree, no git command needed. '
            'Commit it with proper messages and its `Imported-From:` '
            'trailers.' % WIP_MESSAGE)
        return RESUME_PREAMBLE.format(
            id=batch_id, why=why, branch=batch['branch'], base=self.base,
            wip=WIP_MESSAGE, reset=reset, verdicts='\n'.join(
                '- verdict %s %s case %s' % ((a,) + _side_case(v))
                for a, v in recorded) or '(none yet)')

    def run_batch(self, batch_id, resume=None):
        """One assigned batch, start to end: 'closed', 'exception' or
        'parked' ('checked' for a dry run whose checks passed).  With
        `resume` (why the batch is taken up again), the brief opens with
        a `Resume:` preamble listing the verdicts already recorded.  An
        OSError in a stage is that stage's exception (ruling R44)."""
        try:
            return self._run_batch(batch_id, resume)
        except _StageError as e:
            return self._exception(batch_id, e.stage, [e.problem])
        except _LaunchError as e:
            reason = 'launch failed: %s' % e.error
            out = self._park(batch_id, reason, e.cwd)
            self._halt_run(reason)
            return out

    def _run_batch(self, batch_id, resume):
        self._runs = []
        self._worker_ran = []
        self._ignored = {}
        self._undecided = {}
        self._probe = {}
        self._frida_got = {}
        self._ctx.usage = {'input': 0, 'cached': 0, 'output': 0, 'tool_calls': 0}
        self._ctx.final = None
        self._ctx.transcript_lines = []
        self._ctx.t0 = self._started.pop(batch_id, None)
        if self._ctx.t0 is None:
            self._ctx.t0 = self._clock()
        state = self.load_state(self.state_path)
        batch = state['batches'].get(batch_id)
        stage = self._retry_stage.pop(batch_id, None)
        if batch is None:
            return self._exception(batch_id, 'state', [
                'no batch %s in %s' % (batch_id, self.state_path)])
        if batch['status'] != 'assigned':
            if stage == 'push' and batch['status'] == 'closed':
                # A cleared push exception: the push again, nothing else.
                return ('exception' if self._settle_closed(batch_id)
                        else 'closed')
            self.log('%s is %s; nothing to do' % (batch_id, batch['status']))
            return 'closed'
        # The pool: the one this run assigned, else the one the state
        # names, else the first.
        names = [p.name for p in self.pools]
        name = self._ctx.pool or batch.get('pool')
        if name not in names:
            name = names[0]
        self._ctx.pool = name
        self._record_pool(batch_id, name)
        worktree = self.worktree_for(self.repo, batch['branch'])
        if worktree is None or not os.path.isdir(worktree):
            # A removed directory stays listed (prunable) until a prune.
            # The exception also ends this batch's park and current.
            gone = '' if worktree is None else ' (%s is gone)' % worktree
            return self._exception(batch_id, 'worktree', [
                '%s has no worktree%s; run `git worktree prune`, then `git '
                'worktree add <path> %s`, then clear this exception' % (
                    batch['branch'], gone, batch['branch'])])
        if stage in ('record', 'close') and not self.dry_run:
            # A cleared record or close exception: straight to that stage on
            # the worktree and the verdicts already recorded; the worker is
            # not run again (ruling R71).
            self.log('%s: retried at its %s stage; the worker and the review '
                     'are not run again' % (batch_id, stage))
            if self.kind == 'verdicts':
                self._undecided = self._saved_undecided(batch_id, batch)
            return self._land(
                batch_id, batch, worktree,
                self._saved_verdicts(batch_id, batch) if stage == 'record' else {},
                (False, 'retry at %s: worker and review not run again' % stage))
        self._stage(batch_id, 'brief')
        try:
            rc, brief = self.render_brief(
                self.template_path, self.state_path, batch_id,
                bundle_dir=os.path.join(self.driver_dir, 'bundles', batch_id))
        except Exception as e:
            rc, brief = 1, 'render_brief raised %r' % (e,)
        if rc != 0:
            return self._exception(batch_id, 'brief', [brief.strip()])
        if resume:
            brief = self._resume_preamble(batch_id, batch, resume,
                                          worktree) + brief
            self.log('%s: resumed (%s)' % (batch_id, resume))
        worker = (FakeWorker(dry_report(batch)) if self.dry_run
                  else self._worker_of(name))
        if not self.dry_run and self.kind == 'verdicts':
            brief = self._oracle_phase(batch_id, batch, worktree) + brief

        # The worker, the mechanical checks, one fix round.
        self._stage(batch_id, 'worker')
        res = self._run(worker, brief, worktree, batch_id, 'worker')
        out = self._settle(batch_id, res, 'worker', worktree)
        if out:
            return out
        verdicts, problems = self._check(batch_id, worktree, res.report)
        # Frida runs once, here, clean check or not; what it finds joins the
        # one fix round (ruling R58).
        problems = problems + self._frida_phase(batch_id, batch, worktree, verdicts)
        if problems:
            self.log('%s: %d problem(s); one fix round' % (batch_id, len(problems)))
            res = self._fix(batch_id, batch, worker, worktree, 'mechanical checks',
                            problems, brief, 'worker-fix-checks')
            out = self._settle(batch_id, res, 'worker-fix-checks', worktree)
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
        elif self._reviewer_of(name) is None:
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
                out = self._settle(batch_id, res, 'worker-fix-review', worktree)
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

        return self._land(batch_id, batch, worktree, verdicts, review)

    def _saved_verdicts(self, batch_id, batch):
        """The verdicts of the last saved report that the state does not
        hold yet (the retry of a record exception)."""
        key = _tool('wave')._addr_key
        have = {key(a) for a in batch.get('verdicts', {})}
        for text in self._saved_reports(batch_id):
            try:
                parsed = parse_report(text)
            except ReportError:
                continue
            return {a: sc for a, sc in parsed.items() if key(a) not in have}
        return {}

    def _saved_reports(self, batch_id):
        """The texts of the saved worker reports, the last one first."""
        folder = os.path.join(self.driver_dir, batch_id)
        for label in ('worker-fix-review', 'worker-fix-checks', 'worker'):
            try:
                with open(os.path.join(folder, label + '.report.md'),
                          encoding='utf-8') as f:
                    yield f.read()
            except OSError:
                continue

    def _saved_undecided(self, batch_id, batch):
        """The ## Undecided rows of the last saved report that are still in
        the batch with no recorded verdict (a retry of record or close,
        ruling R74: the move is idempotent)."""
        key = _tool('wave')._addr_key
        open_rows = {key(r['address']) for r in batch.get('rows', [])} - \
            {key(a) for a in batch.get('verdicts', {})}
        for text in self._saved_reports(batch_id):
            try:
                parse_report(text)
                listed = parse_undecided(text)
            except ReportError:
                continue
            return {a: sc for a, sc in listed.items() if key(a) in open_rows}
        return {}

    def _move_undecided(self, batch_id):
        """Ruling R73: the rows the worker listed under ## Undecided leave
        the batch and join the probe list (docs/wso/probe-list.json, one
        entry per address), the same way triage moves an ours-stub row to
        the import list.  The list is written first: a crash between the
        two writes leaves the row in both places, never in neither.  The
        caller holds the driver lock."""
        wave = _tool('wave')
        if not self._undecided:
            return
        key = wave._addr_key
        state = self.load_state(self.state_path)
        batch = state['batches'][batch_id]
        gone = {key(a): sc for a, sc in self._undecided.items()}
        moved = [r for r in batch['rows'] if key(r['address']) in gone]
        listed, err = wave._load_list(self.probe_path, 'probe list',
                                      missing_ok=True)
        if err:
            raise ValueError(err.strip())
        have = {key(e['address']) for e in listed}
        at = _now()
        listed += [{'address': r['address'], 'file': r['file'],
                    'name': r.get('name'), 'reading': gone[key(r['address'])][0],
                    'reason': gone[key(r['address'])][1], 'batch': batch_id,
                    'source': 'worker', 'at': at}
                   for r in moved if key(r['address']) not in have]
        wave._save_json(self.probe_path, listed)
        batch['rows'] = [r for r in batch['rows'] if key(r['address']) not in gone]
        batch['files'] = wave._files_of(batch['rows'])
        wave.save_state(self.state_path, state)
        self.log('%s: %d undecided row(s) moved to the probe list' % (
            batch_id, len(moved)))

    def _land(self, batch_id, batch, worktree, verdicts, review):
        """Record, close, push, ledger -- and the retry of a cleared record,
        close or push exception, which comes straight here (ruling R71)."""
        # Record, close, push, ledger.  Every state write and git
        # operation here holds the driver lock (ruling R62); nothing
        # inside the locked blocks takes it again.
        self._stage(batch_id, 'record')
        failed = []
        with self._lock:
            try:
                self._move_undecided(batch_id)
            except Exception as e:
                failed.append('moving the undecided rows to the probe list '
                              'raised %r' % (e,))
        if failed:
            return self._exception(batch_id, 'record', failed)
        with self._lock:
            for addr, (side, case) in sorted(verdicts.items()):
                measured = self._measured_for(addr, side)
                extra = {} if measured is None else {'measured': measured}
                rc, text = self.record_verdict(self.state_path, self.repo,
                                               batch_id, addr, side, case,
                                               **extra)
                if rc != 0:
                    failed.append(text.strip())
        if failed:
            return self._exception(batch_id, 'record', failed)
        self._stage(batch_id, 'close')
        with self._lock, self._writing():
            commits = self._commits(batch['branch'])
            # Uncommitted paths were a mechanical-check problem already
            # (ruling R71); nothing is committed on the worker's behalf.
            # Another driver may fast-forward the base while this one waits
            # on the gate lock: on that error, and only after the gate
            # passed, sync and gate once more in place (ruling R64).
            for attempt in (0, 1):
                sync = self._sync_branch(batch['branch'], worktree)
                if sync:
                    rc, text = 1, sync
                    break
                try:
                    why = self._regen_map(worktree)
                except Exception as e:
                    why = 'raised %r' % (e,)
                if why:
                    rc, text = 1, 'regenerating docs/MAP.md failed: %s' % why
                    break
                try:
                    rc, text = self.close_batch(
                        self.state_path, self.repo, batch_id, base=self.base,
                        gate=self.gate, kind=self.kind)
                except Exception as e:
                    rc, text = 1, 'close_batch raised %r' % (e,)
                if rc != 0 and attempt == 0 and 'not a fast-forward' in text:
                    self.log('%s: %s moved during the gate; syncing and '
                             'closing again' % (batch_id, self.base))
                    continue
                break
        if rc != 0:
            return self._exception(batch_id, 'close', [
                l.strip() for l in text.splitlines() if l.strip()])
        self.log(text.rstrip())
        with self._lock, self._writing():
            if self.kind == 'verdicts':
                # The import queue has no verdicts: its state must not
                # overwrite wave 1's scorecard.
                try:
                    rc, text = self.write_scorecard(
                        self.state_path, self.report_path, self.triage_path,
                        calibration_path=self.calibration_path)
                    self.log(text.rstrip())
                except Exception as e:   # the batch is closed; the scorecard can wait
                    self.log('scorecard not written: %r' % (e,))
            self._commit_state(batch_id)
            err = self.push()
        batch = self.load_state(self.state_path)['batches'][batch_id]
        entry = ledger_entry(
            dict(batch, id=batch_id), batch['verdicts'],
            self._classes(batch_id, batch), review, commits, self._credits(),
            ignored=self._ignored, undecided=self._undecided,
            **self._figure_kw())
        kept = [(a, d) for a, (side, _c) in sorted(verdicts.items())
                for st, d in [self._frida_got.get(_tool('wave')._addr_key(a), ('', ''))]
                if side == 'ours' and st == 'different']
        entry += ''.join('- frida different, kept ours: %s %s\n' % kd for kd in kept)
        self._append_ledger(entry)
        self._finish(batch_id, 'closed')
        if err:
            return self._exception(batch_id, 'push', ['git push failed: %s' % err])
        return 'closed'

    # -- the loop: cuts and retries -------------------------------------------

    def _recompare(self):
        """Once per run, when the wave-1 list is exhausted: look at Will's
        tip.  Logs `recompare: N new rows at <tip7>`; returns N (0 when
        the comparison failed, which only costs the exit the run had)."""
        if self._recompared:
            return 0
        self._recompared = True
        try:
            with self._lock, self._writing():
                rc, text = self.recompare(
                    self.state_path, self.disagreements_path, repo=self.repo)
        except Exception as e:
            rc, text = 2, 'recompare raised %r\n' % (e,)
        self.log(text.rstrip())
        m = re.match(r'recompare: (\d+) new rows', text)
        return int(m.group(1)) if rc == 0 and m else 0

    def _cut(self):
        """(batch_id, None) for a batch cut and triaged, else (None, exit)."""
        triage = (self.triage_path
                  if _tool('wave').QUEUES[self.queue]['triage'] else None)
        try:
            with self._lock, self._writing():
                rc, text = self.next_batch(
                    self.disagreements_path, self.state_path, self.size,
                    self.category, triage_path=triage, repo=self.repo,
                    base=self.base, queue=self.queue)
        except Exception as e:
            rc, text = 1, 'next_batch raised %r\n' % (e,)
        if (rc == 2 and text.startswith('nothing left:')
                and self.queue == 'wave1' and not self.dry_run):
            n = self._recompare()
            if n:
                return self._cut()
        self.log(text.rstrip())
        m = re.match(r'(\S+) assigned:', text)
        if m:
            batch_id = m.group(1)
            self._started[batch_id] = self._clock()
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

    def _adoptable(self, taken):
        """Ids of the batches to adopt (ruling R69), oldest assigned_at
        first: assigned, no verdict recorded, and claimed by nothing
        (`taken`: this run's resumes and retries; the status file's
        running, parked and exception entries).  Wave 1 only."""
        if self.dry_run or self.queue != 'wave1':
            return []
        status = self._read_status()
        claimed = set(taken) | set(status['running']) | set(status['exceptions'])
        for holder in [status] + [p for p in status['pools'].values()
                                  if isinstance(p, dict)]:
            claimed.update(parked_map(holder.get('parked')))
        if isinstance(status['current'], dict) and status['current'].get('batch'):
            claimed.add(status['current']['batch'])
        batches = self.load_state(self.state_path)['batches']
        free = [(b.get('assigned_at') or '', bid) for bid, b in batches.items()
                if b.get('status') == 'assigned' and not b.get('verdicts')
                and bid not in claimed]
        return [bid for _at, bid in sorted(free)]

    def _adopt(self, batch_id):
        """None when batch_id is cut (branch and worktree, as next_batch
        does under R14) and triaged like a fresh one, else an exit code."""
        wave = _tool('wave')
        batch = self.load_state(self.state_path)['batches'][batch_id]
        claims = os.path.join(os.path.dirname(os.path.abspath(self.state_path)),
                              'claims.json')
        try:
            with self._lock, self._writing(), \
                    wave._filelock().hold(claims + '.lock', 'wave adopt'):
                wt, how = self.cut_worktree(self.repo, batch['branch'],
                                            self.base)
                if wt is None:
                    rc, text = 1, '%s not adopted: %s\n' % (batch_id, how)
                else:
                    state = self.load_state(self.state_path)
                    rc, text = self.apply_triage(
                        self.state_path, self.repo, batch_id, self.triage_path,
                        state.get('fork_base') or wave.DEFAULT_FORK_BASE)
                    text = ('%s adopted: worktree %s (%s)\n' % (
                        batch_id, wt, how)) + text
        except Exception as e:
            rc, text = 1, '%s not adopted: adoption raised %r\n' % (batch_id, e)
        self.log(text.rstrip())
        if text.startswith(batch_id + ' adopted:'):
            self._started[batch_id] = self._clock()
            t = re.search(r'^triage: \d+ auto, (\d+) import, \d+ ambiguous$',
                          text, re.M)
            if t:
                self._imported[batch_id] = int(t.group(1))
            if rc != 0:
                self.log('%s: triage exited %s; the rows it could not decide '
                         'stay for the worker' % (batch_id, rc))
            self._update_status(lambda s: s.__setitem__('stalled', None))
            return None
        self._exception(batch_id, 'cut', [l for l in text.splitlines()
                                          if l.strip()])
        return 1

    def _retry_why(self, batch_id):
        """The resume text for a cleared batch (None: it goes straight to
        its stage, or is closed), or False when it has no worktree and no
        verdict and so is adopted anew (ruling R71)."""
        batch = self.load_state(self.state_path)['batches'].get(batch_id) or {}
        stage = self._retry_stage.get(batch_id)
        if batch.get('status') != 'assigned' or stage in ('record', 'close'):
            return None
        if not batch.get('verdicts') and self.queue == 'wave1' and \
                not self.dry_run and (self.worktree_for(
                    self.repo, batch.get('branch', '')) is None):
            return False
        return 'had its exception at the %s stage cleared: the work so ' \
               'far is on its branch' % (stage or 'earlier')

    def _cleared(self):
        """Batches whose exception the coordinator cleared on disk: retried
        first."""
        state = self.load_state(self.state_path)
        status = self._read_status()
        out = []
        for bid, info in sorted(status['batches'].items()):
            if not (isinstance(info, dict) and info.get('outcome') == 'exception'
                    and bid not in status['exceptions']):
                continue
            st = state['batches'].get(bid, {}).get('status')
            stage = info.get('stage')
            if st == 'assigned' or (st == 'closed' and stage == 'push'):
                out.append(bid)
                self._retry_stage[bid] = stage
        return out

    # -- the budget (ruling R38) ----------------------------------------------

    @property
    def estimate_next(self):
        """The next batch's credits: the mean of this run's batches with
        known credits, DEFAULT_ESTIMATE before there is one."""
        known = [c for c in self._batch_credits if c is not None]
        return sum(known) / len(known) if known else DEFAULT_ESTIMATE

    @property
    def credits_spent(self):
        """This run's credits; a batch with unknown credits counts as the
        estimate (unknown is not free)."""
        est = self.estimate_next
        return sum(est if c is None else c for c in self._batch_credits)

    def _over_budget(self):
        """True, with status['stopped'] written, when the credits spent
        plus the next batch's estimate pass the budget.  Checked before
        each new batch, never mid-batch."""
        if self.budget is None:
            return False
        spent, est = self.credits_spent, self.estimate_next
        if spent + est <= self.budget:
            return False
        stopped = {'at': _now(), 'reason': 'budget', 'spent': spent,
                   'budget': self.budget}
        self._update_status(lambda s: s.__setitem__('stopped', stopped))
        self.log('budget stop: %.0f credits spent + %.0f estimated for the '
                 'next batch > budget %.0f' % (spent, est, self.budget))
        return True

    # -- what the last run left (resume) --------------------------------------

    def _has_ledger_entry(self, batch_id):
        head = '## driver %s (' % batch_id
        try:
            with open(self.ledger_path, encoding='utf-8') as f:
                return any(line.startswith(head) for line in f)
        except FileNotFoundError:
            return False

    def _settle_closed(self, batch_id):
        """A batch the last run closed and then stopped on: its ledger
        entry when it is missing, the push, then the status.  1 when the
        push fails (the push exception), else None."""
        self._runs = []
        self._ignored = {}
        batch = self.load_state(self.state_path)['batches'][batch_id]
        if self._has_ledger_entry(batch_id):
            self.log('%s: closed by the last run, ledger entry present; '
                     'pushing again' % batch_id)
        else:
            folder = os.path.join(self.driver_dir, batch_id)
            reviewed = any(os.path.isfile(os.path.join(folder, l + '.report.md'))
                           for l in ('reviewer', 'reviewer-2'))
            tip = self._git('rev-parse', '--verify', batch['branch']).stdout.strip()
            self._append_ledger(ledger_entry(
                dict(batch, id=batch_id), batch.get('verdicts', {}),
                self._classes(batch_id, batch),
                (reviewed, 'recovered after the last run stopped between '
                           'close and ledger'),
                'unknown..%s (recovered)' % (tip[:8] or batch['branch']), None,
                pool=batch.get('pool')))
            self.log('%s: closed by the last run without its ledger entry; '
                     'entry written' % batch_id)
        with self._lock:
            self._commit_state(batch_id)
            err = self.push()
        self._finish(batch_id, 'closed')
        if err:
            self._exception(batch_id, 'push', ['git push failed: %s' % err])
            return 1
        return None

    def _pool_or_first(self, name):
        return name if name in self._busy else self.pools[0].name

    def _leftovers(self):
        """What the last run left behind: ([(batch_id, why, pool)] to
        resume before anything new, None), or ([], exit code).  First a
        batch it closed but did not finish (current or running names a
        closed batch), then each pool's parked batch (a top-level `parked`
        from before the pools is moved to its batch's pool once), then a
        batch it left in flight (the process died mid-batch)."""
        status = self._read_status()
        batches = self.load_state(self.state_path)['batches']
        old = parked_map(status['parked'])
        if old:
            def migrate(s):
                for bid, info in parked_map(s['parked']).items():
                    owner = self._pool_or_first(
                        (batches.get(bid) or {}).get('pool'))
                    pool = s['pools'].setdefault(owner, {})
                    m = parked_map(pool.get('parked'))
                    m.setdefault(bid, info)
                    _set_parked(pool, m)
                s['parked'] = None
            status = self._update_status(migrate)
        current = status['current'] if isinstance(status['current'], dict) else {}
        flying = {}
        for bid, info in status['running'].items():
            if isinstance(info, dict):
                flying[bid] = dict(info)
        if current.get('batch'):
            flying.setdefault(current['batch'], current)
        for cur in sorted(flying):
            if batches.get(cur, {}).get('status') == 'closed':
                if self._settle_closed(cur):
                    return [], 1
                flying.pop(cur)
        resume = []
        parked_ids = set()
        for name, info in sorted(status['pools'].items()):
            if not isinstance(info, dict):
                continue
            for pid, parked in sorted(parked_map(info.get('parked')).items()):
                if pid in parked_ids:   # named by two pools: resumed once
                    continue
                parked_ids.add(pid)
                st = batches.get(pid, {}).get('status')
                if st == 'assigned':
                    named = (batches[pid].get('pool')
                             if (batches[pid].get('pool') in status['pools']
                                 and pid in parked_map(status['pools'][
                                     batches[pid]['pool']].get('parked')))
                             else name)
                    resume.append((pid, 'was parked by the driver at %s (%s)' % (
                        parked.get('at'), parked.get('reason')),
                        self._pool_or_first(named)))
                else:
                    self._update_status(lambda s, pid=pid: _unpark_everywhere(s, pid))
                    self.log('%s was parked but is %s; parked cleared' % (
                        pid, st or 'not in the wave state'))
        for cur, info in sorted(flying.items()):
            if cur in parked_ids:
                continue
            st = batches.get(cur, {}).get('status')
            if st == 'assigned':
                resume.append((cur, 'was interrupted at its %s stage (%s): '
                                    'the last run stopped mid-batch' % (
                                        info.get('stage'), info.get('at')),
                               self._pool_or_first(
                                   batches[cur].get('pool') or info.get('pool'))))
            else:
                def clear(s, cur=cur):
                    s['running'].pop(cur, None)
                    if isinstance(s['current'], dict) and \
                            s['current'].get('batch') == cur:
                        s['current'] = None
                self._update_status(clear)
                self.log('%s was in flight but is %s; current cleared' % (
                    cur, st or 'not in the wave state'))
        return resume, None

    # -- pools ----------------------------------------------------------------

    def _pool_estimate(self, name):
        known = [c for c in self._pool_batch_credits[name] if c is not None]
        return sum(known) / len(known) if known else DEFAULT_ESTIMATE

    def _pool_spent(self, name):
        est = self._pool_estimate(name)
        return sum(est if c is None else c
                   for c in self._pool_batch_credits[name])

    def _init_pools(self):
        """status['pools'][name]: this run's credits (0), the budget, no
        stop; an earlier park stays until its batch is resumed."""
        def init(s):
            for p in self.pools:
                old = s['pools'].get(p.name)
                parked = (parked_map(old.get('parked')) or None
                          if isinstance(old, dict) else None)
                s['pools'][p.name] = {
                    'credits': 0.0, 'budget': self.budgets.get(p.name),
                    'parked': parked, 'stopped': None}
        self._update_status(init)

    def _pool_over_budget(self, name, status):
        """True, with the pool's `stopped: 'budget'` written, when its
        credits plus the next batch's estimate pass its budget."""
        budget = self.budgets.get(name)
        if budget is None:
            return False
        info = status['pools'].get(name) or {}
        if info.get('stopped') == 'budget':
            return True
        spent, est = self._pool_spent(name), self._pool_estimate(name)
        if spent + est <= budget:
            return False
        self._update_status(lambda s: s['pools'].setdefault(name, {})
                            .__setitem__('stopped', 'budget'))
        self.log('pool %s budget stop: %.0f credits spent + %.0f estimated '
                 'for the next batch > budget %.0f' % (name, spent, est, budget))
        return True

    def pick_pool(self):
        """The pool for the next batch, or None: the first, from a
        round-robin cursor that advances after each assignment, that has a
        free worker, is not parked and is under its budget.  Call under
        the lock."""
        status = self._read_status()
        ok = []
        for i, p in enumerate(self.pools):
            info = status['pools'].get(p.name)
            if self._busy[p.name] >= p.workers:
                continue
            if p.name in self._parked_now or (
                    isinstance(info, dict) and parked_map(info.get('parked'))):
                continue
            if self._pool_over_budget(p.name, status):
                continue
            ok.append(i)
        if not ok:
            return None
        n = len(self.pools)
        for off in range(n):
            i = (self._cursor + off) % n
            if i in ok:
                self._cursor = (i + 1) % n
                return self.pools[i].name

    # -- the loop -------------------------------------------------------------

    def _halt_run(self, reason):
        """Stop the run (exit 1) with status['halted'] = {at, reason}, and
        say so once on the AoE session.  A later run that starts a batch
        clears it."""
        self._halt = True
        halted = {'at': _now(), 'reason': reason}
        self._update_status(lambda s: s.__setitem__('halted', halted))
        self.log('driver halted: %s' % reason)
        if not self.dry_run:
            try:
                self.send(_one_line('iwd2r driver halted: %s (status %s)' % (
                    reason, self.status_path), 400))
            except Exception as e:   # a failed send must not hide the halt
                self.log('send failed: %r' % (e,))

    def _one(self, batch_id, resume=None, pool=None):
        """Run one batch on `pool`, count its
        credits, and feed the breaker; an exit code when the run must
        stop, else None."""
        self._runs = []
        self._worker_ran = []
        self._ctx.pool = pool
        try:
            out = (self.run_batch(batch_id, resume=resume) if resume
                   else self.run_batch(batch_id))
        finally:
            # Nothing of this batch's figures may reach the next exception
            # this thread writes (a cut failure has no batch).
            self._ctx.t0 = None
            self._ctx.final = None
        name = self._pool_name()
        if self._runs:
            credits = self._credits()
            with self._lock:
                self._batch_credits.append(credits)
                if name in self._pool_batch_credits:
                    self._pool_batch_credits[name].append(credits)
                    spent = self._pool_spent(name)
                    self._update_status(lambda s: s['pools'].setdefault(
                        name, {}).__setitem__('credits', spent))
        if self._halt:
            return 1
        if out == 'parked':
            with self._lock:
                self._parked_now.add(name)   # whatever the status file says
            return 3
        # The breaker (rulings R45, R46): a batch exception whose worker
        # never really ran (no transcript, or one with no successful
        # non-synthetic turn) counts; any other batch resets.  Credits are
        # no signal: a Claude pool's read 0.
        tripped = False
        with self._lock:
            if out == 'exception' and not any(self._worker_ran):
                self._no_transcript += 1
                tripped = self._no_transcript >= BREAKER_LIMIT
            else:
                self._no_transcript = 0
        if tripped:
            self._halt_run('breaker: %d batches without a worker transcript'
                           % BREAKER_LIMIT)
            return 1
        return None

    def _take_run_lock(self):
        """(fd, None) holding a non-blocking flock on
        `<status_path>.run.lock` with this pid written into it, or (None,
        the holder's pid) when another driver has it (ruling R42)."""
        path = self.status_path + '.run.lock'
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            holder = os.pread(fd, 32, 0).decode('ascii', 'replace').strip()
            os.close(fd)
            return None, holder or 'unknown'
        except BaseException:
            os.close(fd)
            raise
        os.ftruncate(fd, 0)
        os.pwrite(fd, b'%d\n' % os.getpid(), 0)
        return fd, None

    def run(self):
        """0 when every row is assigned or decided, or at the budget stop;
        3 when a pool ended parked by a rate limit; 4 when stalled (the
        rows left wait on files owned by assigned batches); 1 when an
        exception that is not the batch's own stopped the run, the run
        halted (a launch failure, an auth or billing API error, the
        breaker: status['halted']), the status file cannot be read, or
        another driver runs on the same status file.  What the last run
        left goes first: a closed batch's missing ledger entry and push,
        then each pool's parked batch on that pool, then an interrupted
        one.  A dry run processes one batch and exits 0."""
        fd, holder = self._take_run_lock()
        if fd is None:
            self.log('another driver is running (pid %s) on %s; this one '
                     'stops' % (holder, self.status_path))
            return 1
        self._stopping = False
        self._stop_deferred = None
        _STOPPED.clear()
        previous = {}
        for sig in (signal.SIGTERM, signal.SIGINT):
            try:
                previous[sig] = signal.signal(sig, self._on_stop)
            except ValueError:     # not the main thread: no handler
                pass
        try:
            return self._loop()
        except _Stop:
            self.log('driver stopped by a signal: its workers are killed; '
                     'the batches in flight stay recorded for resume')
            return 1
        except StatusError as e:
            self.log('driver stopped: the status file cannot be read: %s. '
                     'Repair it (one JSON object), then run again.' % e)
            return 1
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
            _STOPPED.clear()
            try:
                os.ftruncate(fd, 0)
            except OSError:
                pass
            os.close(fd)   # releases the run lock

    def _loop(self):
        self._halt = False            # per run, like the counters below
        if self.dry_run:
            batch_id, code = self._cut()
            if batch_id is None:
                return code
            self.run_batch(batch_id)
            self.log('dry run: %s left assigned; ledger and status in %s'
                     % (batch_id, self.driver_dir))
            return 0
        self._batch_credits = []
        self._pool_batch_credits = {p.name: [] for p in self.pools}
        self._no_transcript = 0
        self._busy = {p.name: 0 for p in self.pools}
        self._parked_now = set()
        self._began = 0
        self._init_pools()
        if self._read_status()['halted'] is not None:
            self._update_status(lambda s: s.__setitem__('halted', None))
        if self._read_status()['stopped'] is not None:
            self._update_status(lambda s: s.__setitem__('stopped', None))
        resume, code = self._leftovers()
        if code is not None:
            return code
        return self._schedule(resume, self._cleared())

    def _start(self, fn):
        """Run fn on its own thread (the default), or as the injected
        runner says; the synchronous runner `lambda fn: fn()` keeps a
        test deterministic."""
        if self._stopping:
            raise _Stop()
        if self._runner is not None:
            self._runner(fn)
        elif sum(p.workers for p in self.pools) <= 1:
            fn()   # one worker: no second thread to overlap with
        else:
            t = threading.Thread(target=fn, name='driver-batch')
            self._threads.append(t)
            t.start()

    def _schedule(self, resume, retries):
        """Hand batches to the pools until nothing is left: resumes first
        (each on its own pool), then the cleared exceptions, then new
        cuts.  One thread per busy worker; this thread only assigns, under
        the lock, and collects the finished ones."""
        done = queue_mod.Queue()
        pending = list(resume)
        retries = list(retries)
        running = 0
        exhausted = None      # the exit code once a cut finds nothing
        cut_blocked = False   # stalled while batches run: retry after one ends
        failure = None        # 1 when the run must stop
        adopt = self._adoptable([b for b, _w, _n in pending] + retries)
        capped = False        # --max-batches reached
        crash = None          # an exception that escaped a batch thread

        threads = self._threads = []

        def body(batch_id, why, name):
            err = None
            try:
                code = self._one(batch_id, why, name)
            except BaseException as e:   # reported by the scheduler
                code, err = 1, e
            finally:
                with self._lock:
                    self._busy[name] -= 1
            done.put((code, err))

        def launch(batch_id, why, name):
            nonlocal running
            running += 1
            self._began += 1
            self._start(lambda: body(batch_id, why, name))

        def reap(block):
            nonlocal running, cut_blocked, failure, crash
            try:
                code, err = done.get(block=block)
            except queue_mod.Empty:
                return False
            running -= 1
            cut_blocked = False
            if err is not None and crash is None:
                crash = err
            if code == 1:
                failure = 1
            return True

        def launch_next():
            nonlocal exhausted, cut_blocked, failure, capped
            if self._stopping:
                return False
            if self._halt:
                return False
            if self.max_batches is not None and self._began >= self.max_batches:
                if exhausted is None and not capped:
                    capped = True
                    stopped = {'at': _now(), 'reason': 'max-batches',
                               'n': self.max_batches}
                    self._update_status(lambda s: s.__setitem__('stopped', stopped))
                    self.log('--max-batches %d: %d started; no new batch '
                             'starts' % (self.max_batches, self._began))
                return False
            if pending:
                with self._lock:
                    for i, (batch_id, why, name) in enumerate(pending):
                        pool = next(p for p in self.pools if p.name == name)
                        if self._busy[name] < pool.workers:
                            pending.pop(i)
                            self._busy[name] += 1
                            break
                    else:
                        batch_id = None
                if batch_id is not None:
                    # A resumed batch is in flight again, not parked: off
                    # every pool's map (ruling R64).
                    self._update_status(lambda s: _unpark_everywhere(s, batch_id))
                    launch(batch_id, why, name)
                    return True
                # Its pool is busy: the idle pools go on with new cuts.
            if exhausted is not None or cut_blocked:
                return False
            if self._over_budget():
                exhausted = 0
                return False
            with self._lock:
                name = self.pick_pool()
                if name is not None:
                    self._busy[name] += 1
            if name is None:
                return False
            if retries:
                bid = retries.pop(0)
                why = self._retry_why(bid)
                if why is False:           # a cut that never happened: adopt it
                    code = self._adopt(bid)
                    if code is not None:
                        with self._lock:
                            self._busy[name] -= 1
                        failure = code
                        return False
                    self._record_pool(bid, name)
                    why = None
                launch(bid, why, name)
                return True
            if adopt:
                batch_id = adopt.pop(0)
                code = self._adopt(batch_id)
                if code is not None:
                    with self._lock:
                        self._busy[name] -= 1
                    failure = code
                    return False
                self._record_pool(batch_id, name)
                launch(batch_id, None, name)
                return True
            batch_id, code = self._cut()
            if batch_id is None:
                with self._lock:
                    self._busy[name] -= 1
                if code == 4 and running:
                    cut_blocked = True
                elif code == 1:
                    failure = 1
                else:
                    exhausted = code
                return False
            launch(batch_id, None, name)
            return True

        try:
            while True:
                while reap(False):
                    pass
                if failure is None and crash is None and launch_next():
                    continue
                if running == 0:
                    break
                reap(True)
        finally:
            for t in threads:   # never leave a batch running past the run lock
                t.join()
        if crash is not None:
            raise crash
        if failure:
            return failure
        status = self._read_status()
        if self._parked_now or any(
                isinstance(p, dict) and parked_map(p.get('parked'))
                for p in status['pools'].values()):
            return 3
        return exhausted or 0

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
                               capture_output=True, text=True, timeout=60,
                               env=send_env(os.environ))
        except (OSError, subprocess.SubprocessError) as e:
            self.log('aoe send failed: %r' % (e,))
            return
        if r.returncode != 0:
            self.log('aoe send exited %s: %s' % (
                r.returncode, _one_line((r.stderr or '') + (r.stdout or ''))))


def _parse(argv):
    """(args, None), or (None, (exit code, message)) for `wave.py run`.

    `--budget-NAME C` is parsed here by hand: the pool names are only
    known after `--pool` (ruling R12), so argparse cannot declare them."""
    wave = _tool('wave')
    p = argparse.ArgumentParser(
        prog='wave.py run', allow_abbrev=False,
        description='Run the wave batches: cut, triage, worker, checks, '
                    'review, close, push, ledger.')
    p.add_argument('--queue', choices=sorted(wave.QUEUES), default='wave1',
                   help='wave1 (both_changed_differ, b<NNN>) or import '
                        '(theirs_real_ours_stub and the import list, i<NNN>)')
    p.add_argument('--pool', action='append', default=[], metavar='SPEC',
                   help='NAME=WRAPPER:MODEL[:WORKERS], or the short form '
                        'NAME[:WORKERS] of glm or claude; repeatable '
                        '(default: %s)' % DEFAULT_POOL)
    p.add_argument('--reviewer-model', action='append', default=[],
                   metavar='NAME=MODEL',
                   help="a pool's reviewer model; repeatable (default: "
                        'sonnet for claude, glm-5.3-flash[1m] for glm, else '
                        "the pool's worker model)")
    p.add_argument('--workers', type=int, default=None,
                   help="the default pool's worker count (an error with "
                        '--pool: give each pool its :WORKERS)')
    p.add_argument('--budget', type=float, default=None,
                   help='credits over all pools: stop before a new batch '
                        'would pass it; --budget-NAME C caps one GLM pool')
    p.add_argument('--max-batches', type=int, default=None, metavar='N',
                   help='stop starting batches once N have started in this '
                        'run (adopted, cut or resumed); the ones in flight '
                        'finish, and the run exits 0')
    p.add_argument('--worker-timeout', type=float, default=DEFAULT_TIMEOUT,
                   metavar='SECONDS',
                   help='kill a worker or reviewer run (its whole process '
                        'group) after this long and park the batch with the '
                        'reason timeout (default %(default)s)')
    p.add_argument('--seed', default=DEFAULT_SEED,
                   help='the review sample seed (default %(default)s)')
    p.add_argument('--state', help='the queue\'s state file '
                                   '(default: its docs/wso state file)')
    p.add_argument('--aoe-session', help='the AoE session that gets '
                                         'exception lines')
    p.add_argument('--frida', action=argparse.BooleanOptionalAction, default=None,
                   help='run the batched Frida differential path after the worker '
                        '(default: docs/wso/truth-calibration.json frida_default; '
                        'off when that file is missing)')
    p.add_argument('--frida-map',
                   help='override: measure an existing x86 build (the exe beside this '
                        'iwd2r.map, at <checkout>/' + X86_MAP_REL + ') instead of '
                        "building each batch's worktree (hand runs; the default builds "
                        'the batch so the bodies under test are its own)')
    one = p.add_mutually_exclusive_group()
    one.add_argument('--dry-run', action='store_true',
                     help='one batch with a fake worker; no review, no close, '
                          'no push, no aoe; needs --state')
    one.add_argument('--clear', metavar='BATCH_ID',
                     help="remove that batch's exception from the status "
                          "file, under its lock (the coordinator's retry); "
                          "exit 2 when there is none")
    try:
        args, extra = p.parse_known_args(argv)
    except SystemExit as e:
        return None, (e.code if isinstance(e.code, int) else 2, '')
    args.budgets = {}
    i = 0
    while i < len(extra):
        m = re.fullmatch(r'--budget-([A-Za-z0-9_-]+)(?:=(.*))?', extra[i])
        if not m:
            return None, (2, 'wave.py run: unrecognized argument %s' % extra[i])
        value = m.group(2)
        if value is None:
            i += 1
            value = extra[i] if i < len(extra) else ''
        try:
            args.budgets[m.group(1)] = float(value)
        except ValueError:
            return None, (2, 'wave.py run: --budget-%s needs a number of '
                             'credits, not %r' % (m.group(1), value))
        i += 1
    return args, None


def _pools_of(args):
    """([Pool], None), or (None, message) for the --pool, --workers and
    --reviewer-model flags."""
    if args.pool and args.workers is not None:
        return None, ('--workers sets the default pool\'s worker count and '
                      'cannot be combined with --pool: give each pool its '
                      ':WORKERS')
    if args.workers is not None and args.workers < 1:
        return None, '--workers must be at least 1'
    reviewers = {}
    for spec in args.reviewer_model:
        name, eq, model = spec.partition('=')
        if not eq or not name or not model:
            return None, '--reviewer-model %r: expected NAME=MODEL' % spec
        reviewers[name] = model
    try:
        pools = [parse_pool(spec, args.queue) for spec in (
            args.pool or [DEFAULT_POOL])]
        if args.workers is not None:
            pools[0] = pools[0]._replace(workers=args.workers)
        if len({p.name for p in pools}) != len(pools):
            return None, 'two --pool specs have the same name'
        return pools_with_reviewers(pools, reviewers), None
    except ValueError as e:
        return None, str(e)


def _build(args, repo):
    """(Driver, None), or (None, (exit code, message))."""
    wave = _tool('wave')
    queue = args.queue
    pools, why = _pools_of(args)
    if pools is None:
        return None, (2, 'wave.py run: %s' % why)
    if args.dry_run and not args.state:
        return None, (2, 'wave.py run --dry-run refuses without an explicit '
                         '--state PATH: a dry run cuts a real batch (branch, '
                         'worktree, triage commits) in that state file. Point '
                         'it at a copy, never at docs/wso/wave-state.json.')
    if args.max_batches is not None and args.max_batches < 1:
        return None, (2, 'wave.py run: --max-batches must be at least 1')
    if args.worker_timeout <= 0:
        return None, (2, 'wave.py run: --worker-timeout must be positive')
    live_path = wave.QUEUES[queue]['state']
    if args.dry_run:
        live = {os.path.realpath(live_path), os.path.realpath(
            os.path.join(repo, 'docs', 'wso', os.path.basename(live_path)))}
        if os.path.realpath(args.state) in live:
            return None, (2, 'wave.py run --dry-run refuses %s: that is the '
                             'live %s file. Copy it outside docs/wso '
                             'and pass the copy.' % (
                                 args.state, os.path.basename(live_path)
                                 .replace('.json', '')))
    state = os.path.abspath(args.state) if args.state else live_path
    ledger = os.path.join(repo, LEDGER_REL)
    status = os.path.join(repo, status_rel(queue))
    workers_by_pool = reviewers_by_pool = None
    if args.dry_run:
        dry = os.path.join(os.path.dirname(status), 'dry-run')
        ledger = os.path.join(dry, 'progress.md')
        status = os.path.join(dry, os.path.basename(status))
        worker = reviewer = None
    else:
        worker = reviewer = None
        workers_by_pool = {p.name: HeadlessWorker(model=p.model, wrapper=p.wrapper,
                                           timeout=args.worker_timeout)
                           for p in pools}
        reviewers_by_pool = {p.name: HeadlessWorker(model=p.reviewer_model,
                                                    wrapper=p.wrapper,
                                                    timeout=args.worker_timeout)
                             for p in pools}
    frida = args.frida
    if frida is None:
        frida = _tool('truth').frida_default(
            os.path.join(repo, 'docs', 'wso', 'truth-calibration.json'))
    try:
        d = Driver(repo, state, worker, reviewer, ledger, status,
                   aoe_session=args.aoe_session, seed=args.seed,
                   budget=args.budget, dry_run=args.dry_run, frida=frida,
                   frida_map=args.frida_map, pools=pools,
                   workers_by_pool=workers_by_pool,
                   reviewers_by_pool=reviewers_by_pool,
                   budgets=args.budgets, queue=queue,
                   max_batches=args.max_batches)
    except StatusError as e:
        return None, (1, 'wave.py run: the status file cannot be read: %s' % e)
    except ValueError as e:
        return None, (2, 'wave.py run: %s' % e)
    return d, None


def driver_from_args(argv, repo=None):
    """(Driver, None), or (None, (exit code, message)) for `wave.py run`."""
    args, err = _parse(argv)
    if err:
        return None, err
    return _build(args, repo or _tool('wave').ROOT)


def _clear(batch_id, repo, queue='wave1'):
    """`wave.py run --clear <batch_id>`: 0, 2 when the status file holds
    no exception for it, 1 when the file cannot be read."""
    path = os.path.join(repo, status_rel(queue))
    try:
        rc, entry = clear_exception(path, batch_id)
    except StatusError as e:
        print('wave.py run --clear: the status file cannot be read: %s' % e,
              file=sys.stderr)
        return 1
    if rc != 0:
        print('wave.py run --clear: no exception for %s in %s' % (batch_id, path),
              file=sys.stderr)
        return rc
    print('removed the exception for %s from %s: %s' % (
        batch_id, path, json.dumps(entry, sort_keys=True)))
    return 0


def main(argv=None, repo=None):
    repo = repo or _tool('wave').ROOT
    args, err = _parse(sys.argv[1:] if argv is None else argv)
    if err is None:
        if args.clear is not None:
            return _clear(args.clear, repo, args.queue)
        d, err = _build(args, repo)
    if err:
        if err[1]:
            print(err[1], file=sys.stderr)
        return err[0]
    return d.run()
