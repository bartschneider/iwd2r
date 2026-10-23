#!/usr/bin/env python3
"""A coverage map of the ORIGINAL exe per Frida scenario.

The Frida phase (tools/truth.py frida_run) can only measure a row a scenario
exercises, and finds out after six sandbox launches.  This pass runs each
scenario once per chunk of candidate functions on the original alone, every
function hooked for hit counts only (hooks.py's table-level "count_only"),
and writes docs/wso/coverage.json:

  {"exe": sha256 of the original iwd2.exe, "scenarios_sha": sha256 of the
   scenarios/*.txt set, "at": UTC ISO, "scenarios": {name: {"hooks": N,
   "refused": [fn names Frida could not hook], "failed_chunks": [chunk
   numbers whose status had no RESULT twice], "hits": {"0xADDR": n}}}}

Only non-zero hits are stored.  Consumers: `wave.py next --queue import`
(covered rows first), the driver's frida phase (skips the launches when no
row is covered) and the scorecard line.  The pass itself is run by hand:

  python3 -I tools/wave.py coverage [--scenarios a,b,c] [--chunk N]
                                    [--addresses FILE] [--dry]

Standard library only; `run` is injectable and the tests never launch.
"""
import datetime
import hashlib
import importlib.util
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
COVERAGE_REL = os.path.join('docs', 'wso', 'coverage.json')
COVERAGE_PATH = os.path.join(ROOT, COVERAGE_REL)
CATEGORIES = ('theirs_real_ours_stub', 'theirs_changed_ours_base', 'only_theirs',
              'both_changed_differ', 'both_stub_diff')
DEFAULT_CHUNK = 400
CLICKS = 3          # as frida_run: the phase's scenarios run with 3 clicks

_MODS = {}


def _load(name):
    if name not in _MODS:
        spec = importlib.util.spec_from_file_location(
            'coverage_' + name, os.path.join(HERE, name + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _MODS[name] = module
    return _MODS[name]


def norm(address):
    """'0x6869c0' / '6869C0' -> '0x6869C0'."""
    return '0x%X' % int(str(address), 16)


def candidates(disagreements_path):
    """Sorted unique addresses of the rows in a candidate category."""
    with open(disagreements_path, encoding='utf-8') as f:
        rows = json.load(f)
    found = {int(norm(r['address']), 16) for r in rows
             if r.get('address') and r.get('category') in CATEGORIES}
    return ['0x%X' % a for a in sorted(found)]


def read_addresses(path):
    """One address per line (blank lines and # comments skipped)."""
    with open(path, encoding='utf-8') as f:
        found = {int(norm(l.split('#')[0].strip()), 16) for l in f
                 if l.split('#')[0].strip()}
    return ['0x%X' % a for a in sorted(found)]


def table(addresses):
    """The count-only hook table for the original side."""
    truth = _load('truth')
    return {'original': {'hooks': [{'name': truth.hook_name(a), 'addr': norm(a)}
                                   for a in addresses]},
            'count_only': True}


def chunks(addresses, chunk=DEFAULT_CHUNK):
    chunk = max(1, int(chunk))
    return [addresses[i:i + chunk] for i in range(0, len(addresses), chunk)]


def plan(scenarios, addresses, chunk=DEFAULT_CHUNK):
    """[(scenario, i, n, k)]: the launches a pass makes (1-based i of n)."""
    parts = chunks(addresses, chunk)
    return [(s, i, len(parts), len(p)) for s in scenarios
            for i, p in enumerate(parts, 1)]


def _status_lines(path):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            return f.read().splitlines()
    except OSError:
        return []


def _unhooked(path):
    out = []
    for line in _status_lines(path):
        parts = line.split()
        if len(parts) >= 2 and parts[0] == 'UNHOOKED':
            out.append(parts[1])
    return out


def _addr_of(name):
    return '0x' + name[3:] if name.startswith('fn_') else name


def run(scenarios, chunk=DEFAULT_CHUNK, run=subprocess.run, root=ROOT,
        addresses=None, disagreements_path=None, log=None):
    """{scenario: {'hooks', 'refused', 'failed_chunks', 'hits'}}: per
    scenario and chunk one `probe.py run --target original` over a
    count-only table; hits come from the status file's `HITS <name> <n>`
    lines, `UNHOOKED <name>` lines are the refused hooks.  A chunk with no
    RESULT line is launched once more, then recorded failed."""
    import tempfile
    truth = _load('truth')
    if addresses is None:
        addresses = candidates(disagreements_path or os.path.join(
            root, 'docs', 'wso', 'disagreements.json'))
    probe = os.path.join(truth.FRIDA_DIR, 'probe.py')
    parts = chunks(addresses, chunk)
    results = {}
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for i, part in enumerate(parts, 1):
            p = os.path.join(tmp, 'coverage-hooks-%d.json' % i)
            with open(p, 'w', encoding='utf-8', newline='\n') as f:
                json.dump(table(part), f, indent=1)
            paths.append(p)
        for s in scenarios:
            res = {'hooks': 0, 'refused': [], 'failed_chunks': [], 'hits': {}}
            for i, part in enumerate(parts, 1):
                out = 'coverage-%s-%d.jsonl' % (s, i)
                reason = 'coverage %s chunk %d/%d: %d hooks' % (s, i, len(parts), len(part))
                cmd = [sys.executable, '-I', probe, 'run', '--target', 'original',
                       '--probe', paths[i - 1], '--scenario', s, '--out', out,
                       '--clicks', str(CLICKS), '--timeout',
                       str(truth._scenario_timeout(root, s)), '--new-attempt', reason]
                if log:
                    log(reason)
                verdict, hits, status = None, {}, None
                for attempt in (0, 1):
                    if attempt:
                        cmd = list(cmd)
                        cmd[cmd.index('--new-attempt') + 1] = reason + ' retry: no status'
                    done = run(cmd, capture_output=True, text=True, cwd=root)
                    folder, out_path = truth._home_of(done, root, out)
                    status = out_path + '.status' if folder else None
                    verdict, hits, _final = truth._read_status(status) if status else (None, None, False)
                    if verdict is not None:
                        break
                res['hooks'] += len(part)
                if verdict is None:
                    res['failed_chunks'].append(i)
                    continue
                res['refused'] += _unhooked(status)
                for name, n in (hits or {}).items():
                    if n and name.startswith('fn_'):
                        a = _addr_of(name)
                        res['hits'][a] = res['hits'].get(a, 0) + n
            res['refused'] = sorted(set(res['refused']))
            results[s] = res
    return results


def _sha(path):
    h = hashlib.sha256()
    try:
        with open(path, 'rb') as f:
            for block in iter(lambda: f.read(1 << 20), b''):
                h.update(block)
    except OSError:
        return None
    return h.hexdigest()


def scenarios_sha(root=ROOT):
    """sha256 over the sorted scenarios/*.txt (name and contents)."""
    folder = os.path.join(root, 'scenarios')
    h = hashlib.sha256()
    try:
        names = sorted(n for n in os.listdir(folder) if n.endswith('.txt'))
    except OSError:
        names = []
    for n in names:
        h.update(n.encode('utf-8') + b'\0')
        with open(os.path.join(folder, n), 'rb') as f:
            h.update(f.read() + b'\0')
    return h.hexdigest()


def exe_sha():
    """sha256 of the original iwd2.exe the probe launches (oracle.py's
    VANILLA_EXE), None when it is not there."""
    return _sha(_load('oracle').VANILLA_EXE)


def write(path, results, root=ROOT, exe=None):
    data = {'exe': exe if exe is not None else exe_sha(),
            'scenarios_sha': scenarios_sha(root),
            'at': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
            'scenarios': {s: {'hooks': r['hooks'], 'refused': r['refused'],
                              'failed_chunks': r['failed_chunks'],
                              'hits': {a: n for a, n in sorted(
                                  r['hits'].items(), key=lambda kv: int(kv[0], 16)) if n}}
                          for s, r in results.items()}}
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, indent=1)
        f.write('\n')
    return data


def read(path):
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    return data if isinstance(data, dict) and isinstance(data.get('scenarios'), dict) else None


def covered(cov, address, scenarios=None):
    """Names of the scenarios whose hits include the address (limited to
    `scenarios` when given)."""
    a = norm(address)
    return [s for s, r in sorted(cov['scenarios'].items())
            if (scenarios is None or s in scenarios)
            and a in r.get('hits', {})]


def stale(cov, root=ROOT):
    return cov.get('scenarios_sha') != scenarios_sha(root)


def usable(path, root=ROOT):
    """The map at path when it exists and is fresh, else None."""
    cov = read(path)
    return cov if cov is not None and not stale(cov, root) else None


def main(argv, root=ROOT, run_=subprocess.run, out=sys.stdout):
    scenarios, chunk, addrs, dry = None, DEFAULT_CHUNK, None, False
    it = iter(argv)
    for a in it:
        if a == '--scenarios':
            scenarios = [s for s in next(it, '').split(',') if s]
        elif a == '--chunk':
            try:
                chunk = int(next(it))
            except (StopIteration, ValueError):
                print('wave coverage: --chunk needs a number', file=sys.stderr)
                return 2
        elif a == '--addresses':
            addrs = next(it, None)
        elif a == '--dry':
            dry = True
        else:
            print('wave coverage: unknown option %s' % a, file=sys.stderr)
            return 2
    if scenarios is None:
        scenarios = list(_load('truth').FRIDA_SCENARIOS)
    try:
        addresses = (read_addresses(addrs) if addrs else
                     candidates(os.path.join(root, 'docs', 'wso', 'disagreements.json')))
    except (OSError, ValueError) as e:
        print('wave coverage: %s' % e, file=sys.stderr)
        return 2
    launches = plan(scenarios, addresses, chunk)
    if dry:
        out.write('coverage: %d candidate function(s), %d launch(es)\n' % (len(addresses), len(launches)))
        for s, i, n, k in launches:
            out.write('coverage %s chunk %d/%d: %d hooks\n' % (s, i, n, k))
        return 0
    results = run(scenarios, chunk, run=run_, root=root, addresses=addresses,
                  log=lambda m: out.write(m + '\n'))
    path = os.path.join(root, COVERAGE_REL)
    data = write(path, results, root)
    for s, r in data['scenarios'].items():
        out.write('%s: %d of %d hooks hit, %d refused, %d failed chunk(s)\n'
                  % (s, len(r['hits']), r['hooks'], len(r['refused']), len(r['failed_chunks'])))
    out.write('wrote %s\n' % path)
    return 1 if any(r['failed_chunks'] for r in data['scenarios'].values()) else 0
