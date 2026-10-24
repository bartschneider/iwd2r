#!/usr/bin/env python3
"""A coverage map of the ORIGINAL exe per Frida scenario.

The Frida phase (tools/truth.py frida_run) can only measure a row a scenario
exercises, and finds out after six sandbox launches.  This pass runs each
scenario once per chunk of candidate functions on the original alone, every
function hooked for hit counts only (hooks.py's table-level "count_only"),
and writes docs/wso/coverage.json:

  {"exe": sha256 of the original iwd2.exe, "scenarios_sha": sha256 of the
   scenarios/*.txt set (information only), "at": UTC ISO, "scenarios": {name:
   {"sha": sha256 of scenarios/<name>.txt and its .expect.json when present,
   "hooks": N, "refused": [fn names Frida could not hook], "failed_chunks":
   [chunk numbers whose status had no RESULT twice], "hits": {"0xADDR": n}}}}

Only non-zero hits are stored.  A chunk is complete only with `RESULT: CLEAN`
and `HITS-FINAL yes` on a zero exit (after one retry); per scenario "measured"
counts, and "hooked" lists, the addresses of complete chunks less the refused
ones.  A pass with any failed chunk writes coverage.failed.json instead.  A
pass over some scenarios (--scenarios) is merged into the existing map: the
others keep their entries.  Freshness is per entry: one whose "sha" differs
from the scenario's files now (or is absent) counts as NOT measured for every
row ("unknown"), so editing one scenario never makes another's entry stale.
A map with failed chunks, or with no fresh complete phase scenario, is never
`usable`.  Consumers: `wave.py next --queue import` (covered rows first), the
driver's frida phase (runs a minimal cover of the scenarios that hit its
rows, plus those that have not measured a row nothing hit) and the scorecard line.  The pass itself is run by hand:

  python3 -I tools/wave.py coverage [--scenarios a,b,c] [--chunk N]
                                    [--addresses FILE] [--dry]
  python3 -I tools/wave.py coverage --pending [--chunk N] [--dry]
      (only the candidates `wave.py recompare` recorded in
      docs/wso/coverage.pending.json; merged into the map, list emptied,
      both committed)

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
PENDING_REL = os.path.join('docs', 'wso', 'coverage.pending.json')
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


TEXT_START, TEXT_END = 0x401000, 0x8468F1    # the exe's .text: [start, end)


def split_candidates(disagreements_path):
    """(in .text, outside .text): sorted unique addresses of the rows in a
    candidate category; the second list holds the non-code addresses."""
    with open(disagreements_path, encoding='utf-8') as f:
        rows = json.load(f)
    found = {int(norm(r['address']), 16) for r in rows
             if r.get('address') and r.get('category') in CATEGORIES}
    code = ['0x%X' % a for a in sorted(found) if TEXT_START <= a < TEXT_END]
    other = ['0x%X' % a for a in sorted(found) if not TEXT_START <= a < TEXT_END]
    return code, other


def candidates(disagreements_path):
    """Sorted unique addresses of the rows in a candidate category that lie
    in the exe's .text range."""
    return split_candidates(disagreements_path)[0]


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
    # What each scenario's files are before the first launch: a scenario
    # edited mid-pass must not be stamped fresh.
    shas = {s: entry_sha(s, root) for s in scenarios}
    whole = scenarios_sha(root)
    with tempfile.TemporaryDirectory() as tmp:
        paths = []
        for i, part in enumerate(parts, 1):
            p = os.path.join(tmp, 'coverage-hooks-%d.json' % i)
            with open(p, 'w', encoding='utf-8', newline='\n') as f:
                json.dump(dict(table(part)['original'], count_only=True), f, indent=1)
            paths.append(p)
        for s in scenarios:
            res = {'hooks': 0, 'refused': [], 'failed_chunks': [], 'hits': {}, 'hooked': [],
                   'sha': shas[s], 'scenarios_sha': whole}
            for i, part in enumerate(parts, 1):
                out = 'coverage-%s-%d.jsonl' % (s, i)
                reason = 'coverage %s chunk %d/%d: %d hooks' % (s, i, len(parts), len(part))
                cmd = [sys.executable, '-I', probe, 'run', '--target', 'original',
                       '--probe', paths[i - 1], '--scenario', s, '--out', out,
                       '--clicks', str(CLICKS), '--timeout',
                       str(truth._scenario_timeout(root, s)), '--new-attempt', reason]
                if s in truth.FRIDA_UI_SCENARIOS:
                    # a UI-only scenario never reaches the world screen: the
                    # observer must not read that as NOT-LOADED (biography's
                    # four chunks failed that way on 2026-10-23)
                    cmd.append('--no-loaded')
                if log:
                    log(reason)
                complete, hits, status = False, {}, None
                for attempt in (0, 1):
                    if attempt:
                        cmd = list(cmd)
                        cmd[cmd.index('--new-attempt') + 1] = reason + ' retry: incomplete'
                    done = run(cmd, capture_output=True, text=True, cwd=root)
                    folder, out_path = truth._home_of(done, root, out)
                    status = out_path + '.status' if folder else None
                    verdict, hits, final = truth._read_status(status) if status else (None, None, False)
                    complete = (verdict == 'CLEAN' and final
                                and getattr(done, 'returncode', 0) == 0)
                    if complete:
                        break
                res['hooks'] += len(part)
                refused = _unhooked(status) if status else []
                res['refused'] += refused
                for name, n in (hits or {}).items():
                    if n and name.startswith('fn_'):
                        a = _addr_of(name)
                        res['hits'][a] = res['hits'].get(a, 0) + n
                if complete:
                    gone = {_addr_of(n) for n in refused}
                    res['hooked'] += [a for a in part if a not in gone]
                else:
                    res['failed_chunks'].append(i)
            res['refused'] = sorted(set(res['refused']))
            res['hooked'] = sorted(set(res['hooked']), key=lambda a: int(a, 16))
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


def default_scenarios():
    """The scenarios a pass measures by default: the phase's world scenarios
    and the UI-only ones."""
    t = _load('truth')
    return list(t.FRIDA_SCENARIOS) + list(t.FRIDA_UI_SCENARIOS)


def kept_scenarios(path, scenarios):
    """The scenarios of the existing map at path that a pass over
    `scenarios` leaves as they are (a merge keeps them)."""
    old = read(path)
    return [s for s in sorted(old['scenarios']) if s not in scenarios] if old else []


def entry_sha(name, root=ROOT):
    """sha256 of scenarios/<name>.txt plus scenarios/<name>.expect.json when
    present (what makes one scenario's map entry current)."""
    h = hashlib.sha256()
    folder = os.path.join(root, 'scenarios')
    for ext in ('.txt', '.expect.json'):
        try:
            with open(os.path.join(folder, name + ext), 'rb') as f:
                h.update(ext.encode('utf-8') + b'\0' + f.read() + b'\0')
        except OSError:
            pass
    return h.hexdigest()


def stale_entries(cov, root=ROOT, names=None):
    """Names of the map's entries (limited to names) whose recorded sha is not
    the current files' (an entry without a sha is stale)."""
    return [s for s, r in sorted(cov['scenarios'].items())
            if (names is None or s in names)
            and (not isinstance(r, dict) or r.get('sha') != entry_sha(s, root))]


def stale_note(cov, root=ROOT, names=None):
    """The warning line for stale entries, or None."""
    st = stale_entries(cov, root, names)
    return ('coverage: stale entr%s (scenario files changed since; read as unknown until '
            're-run): %s' % ('y' if len(st) == 1 else 'ies', ', '.join(st))) if st else None


def restamp(path, root=ROOT):
    """(rc, text): stamp the entries of an old-format map (no `sha`) with the
    scenario files' current sha, but only when the map's recorded global
    scenarios_sha is the current one (every scenario file is byte-identical to
    what the pass measured); else refuse.  Entries with a sha are left alone."""
    cov = read(path)
    if cov is None:
        return 1, 'coverage --restamp: no map at %s\n' % path
    todo = [s for s, r in sorted(cov['scenarios'].items())
            if isinstance(r, dict) and not r.get('sha')]
    if not todo:
        return 0, 'coverage --restamp: every entry already carries a sha; nothing to stamp\n'
    now = scenarios_sha(root)
    if cov.get('scenarios_sha') != now:
        return 1, ('coverage --restamp: the map recorded scenarios_sha %s but the scenario files '
                   'are %s now (a scenario changed since the pass); re-run the pass\n'
                   % (cov.get('scenarios_sha'), now))
    for s in todo:
        cov['scenarios'][s]['sha'] = entry_sha(s, root)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(cov, f, indent=1)
        f.write('\n')
    return 0, 'coverage --restamp: stamped %s\n' % ', '.join(todo)


def write(path, results, root=ROOT, exe=None, merge=True):
    """Write the map.  An existing map at path is merged (merge=True): the
    scenarios in results replace their entries, the others are kept as they
    are (whatever sha they were measured under), the exe is kept, and `at` and
    `scenarios_sha` are recomputed."""
    old = read(path) if merge else None
    data = {'exe': exe if exe is not None else (old.get('exe') if old else exe_sha()),
            'scenarios_sha': next((r['scenarios_sha'] for r in results.values()
                                   if r.get('scenarios_sha')), None) or scenarios_sha(root),
            'at': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
            'scenarios': {s: {'sha': r.get('sha') or entry_sha(s, root), 'hooks': r['hooks'], 'refused': r['refused'],
                              'failed_chunks': r['failed_chunks'],
                              'measured': len(r.get('hooked', ())),
                              'hooked': list(r.get('hooked', ())),
                              'hits': {a: n for a, n in sorted(
                                  r['hits'].items(), key=lambda kv: int(kv[0], 16)) if n}}
                          for s, r in results.items()}}
    if old:
        kept = {s: r for s, r in old['scenarios'].items() if s not in results}
        data['scenarios'] = dict(sorted({**kept, **data['scenarios']}.items()))
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, indent=1)
        f.write('\n')
    return data


def known_addresses(cov):
    """The set of normalised addresses a map has an opinion on: hooked,
    refused (by fn_ name) or hit in any scenario."""
    found = set()
    for r in (cov or {}).get('scenarios', {}).values():
        for a in list(r.get('hooked', ())) + list(r.get('hits', {})):
            try:
                found.add(norm(a))
            except (TypeError, ValueError):
                pass
        for n in r.get('refused', ()):
            try:
                found.add(norm(_addr_of(n)))
            except (TypeError, ValueError):
                pass
    return found


def read_pending(path):
    """The pending candidates ([{address, name, file, category, at}]) at
    path; [] when the file is missing or unreadable."""
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
    except (OSError, ValueError):
        return []
    if not isinstance(data, list):
        return []
    return [e for e in data if isinstance(e, dict) and e.get('address')]


def merge_pending_results(path, results, root=ROOT):
    """Write `results` (a pass over new candidates) into the map at path
    ADDING to each scenario's entry instead of replacing it: hooks add up,
    hooked/refused unite, hits are added.  An entry keeps its stamp (a stale
    entry stays stale; the new hits are only as fresh as the old stamp)."""
    old = read(path)
    merged = {}
    for s, r in results.items():
        o = (old or {}).get('scenarios', {}).get(s)
        if not o:
            merged[s] = r
            continue
        hits = dict(o.get('hits', {}))
        hits.update(r['hits'])
        merged[s] = {
            'hooks': o.get('hooks', 0) + r['hooks'],
            'refused': sorted(set(o.get('refused', ())) | set(r['refused'])),
            'failed_chunks': list(o.get('failed_chunks', ())),
            'hooked': sorted(set(o.get('hooked', ())) | set(r.get('hooked', ())),
                             key=lambda a: int(a, 16)),
            'hits': hits,
            'sha': o.get('sha') or r.get('sha'),
            'scenarios_sha': r.get('scenarios_sha')}
    return write(path, merged, root, merge=True)


def commit_paths(root, rels, message):
    """Stage and commit `rels` alone in the repo at root; None or why not."""
    def git(*a):
        return subprocess.run(['git', '-C', root, *a], capture_output=True, text=True)
    r = git('add', '--', *rels)
    if r.returncode != 0:
        return 'git add failed: %s' % (r.stderr or r.stdout).strip()
    if git('diff', '--cached', '--quiet', '--', *rels).returncode == 0:
        return None
    r = git('commit', '-q', '-m', message, '--', *rels)
    if r.returncode != 0:
        return 'git commit failed: %s' % (r.stderr or r.stdout).strip()
    return None


def read(path):
    try:
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    return data if isinstance(data, dict) and isinstance(data.get('scenarios'), dict) else None


def covered(cov, address, scenarios=None, include_stale=False):
    """Names of the scenarios whose hits include the address (limited to
    `scenarios` when given).  A stale entry's hits rank and join but never
    decide: they count only with include_stale."""
    a = norm(address)
    stale = () if include_stale else cov.get('stale', ())
    return [s for s, r in sorted(cov['scenarios'].items())
            if (scenarios is None or s in scenarios) and s not in stale
            and a in r.get('hits', {})]


def measured(cov, address, scenarios=None):
    """Names of the scenarios that measured the address (hooked in a
    complete chunk and not refused)."""
    a = norm(address)
    stale = cov.get('stale', ())
    return [s for s, r in sorted(cov['scenarios'].items())
            if (scenarios is None or s in scenarios) and s not in stale
            and a in r.get('hooked', ())]


def refused_in(cov, address, scenarios=None):
    """Names of the (fresh) scenarios whose entry lists the address as a hook
    Frida refused (the map names them fn_<HEX>)."""
    a = norm(address)
    stale = cov.get('stale', ())
    out = []
    for s, r in sorted(cov['scenarios'].items()):
        if (scenarios is not None and s not in scenarios) or s in stale or not isinstance(r, dict):
            continue
        for x in r.get('refused', ()):
            x = str(x)
            try:
                if norm(x[3:] if x.lower().startswith('fn_') else x) == a:
                    out.append(s)
                    break
            except ValueError:
                pass
    return out


def status_of(cov, address, scenarios):
    """'hit' (some phase scenario hits it), 'unhookable' (every scenario is
    fresh and refused the hook: Frida cannot hook it in the original), 'dead'
    (every phase scenario measured it, or refused it, and none hit it) or
    'unknown' (not measured everywhere)."""
    if covered(cov, address, scenarios):
        return 'hit'
    ref = refused_in(cov, address, scenarios)
    if scenarios and len(ref) == len(scenarios):
        return 'unhookable'
    if len(measured(cov, address, scenarios)) + len(ref) == len(scenarios):
        return 'dead'
    return 'unknown'


def cover_order(cands):
    """The tie order of the cover: world_entry, first_fight, walk_and_talk,
    the other phase scenarios in tuple order, the UI scenarios; anything else
    after."""
    order = default_scenarios()
    return sorted(cands, key=lambda s: order.index(s) if s in order else len(order))


def cover_plan(cov, rows, cands):
    """The Frida phase's scenario choice for rows (dicts with 'address') among
    cands (a list): 'run' (cands order), 'left', 'hit' (rows a scenario hits),
    'unknown' (addresses some candidate neither measured nor refused),
    'unhookable' (addresses every fresh scenario refused), 'covered' and
    'total' (distinct rows).  Rows hit by a fresh scenario are covered by a
    greedy set cover (most uncovered rows first, ties in cover_order); a
    candidate that neither measured nor refused a row no fresh scenario hits
    (a stale or missing entry, or a partial `hooked`) runs.  Row states are
    status_of's."""
    hit, unknown, unhookable, forced = {}, [], [], set()
    addrs = []
    for r in rows:
        a = norm(r['address'])
        if a in addrs:
            continue
        addrs.append(a)
        state = status_of(cov, a, cands)
        if state == 'hit':
            hit[a] = set(covered(cov, a, cands))
        elif state == 'unhookable':
            unhookable.append(a)
        elif state == 'unknown':
            unknown.append(a)
            done = set(measured(cov, a, cands)) | set(refused_in(cov, a, cands))
            forced.update(s for s in cands if s not in done)
    chosen = set()
    todo = dict(hit)
    ordered = cover_order(cands)
    while todo:
        best = max(ordered, key=lambda s: sum(1 for by in todo.values() if s in by))
        if not any(best in by for by in todo.values()):
            break
        chosen.add(best)
        todo = {a: by for a, by in todo.items() if best not in by}
    run = [s for s in cands if s in chosen or s in forced]
    return {'run': run, 'left': [s for s in cands if s not in run], 'hit': sorted(hit),
            'unknown': unknown, 'unhookable': unhookable,
            'covered': sum(1 for by in hit.values() if by & set(run)), 'total': len(addrs)}


def row_scenarios(cov, rows):
    """The Frida phase's candidate scenarios for rows (dicts with 'address'
    and 'file'): truth.scenarios_for, plus every UI scenario the map says
    hits one of the rows (a stale entry's hits count: it then runs as unknown) (a sprite function only the biography screen
    reaches)."""
    t = _load('truth')
    cands = list(t.scenarios_for(rows, [r.get('file') for r in rows]))
    for sc in t.FRIDA_UI_SCENARIOS:
        if sc not in cands and any(covered(cov, r['address'], [sc], include_stale=True) for r in rows):
            cands.append(sc)
    return cands


def phase_scenarios():
    return tuple(_load('truth').FRIDA_SCENARIOS)


def check(path, root=ROOT):
    """(map, None) when the map at path is usable, else (None, why):
    present and well formed, no failed chunk anywhere, and at least one phase
    scenario fresh and complete.  The map's 'stale' lists the entries whose
    sha is not the scenario files' now: they read as not measured."""
    cov = read(path)
    if cov is None:
        return None, 'no map (%s)' % COVERAGE_REL
    try:
        for s, r in sorted(cov['scenarios'].items()):
            if r.get('failed_chunks'):
                return None, 'map has failed chunk(s) in %s' % s
            if not (isinstance(r.get('hits', {}), dict) and isinstance(r.get('hooked', []), list)):
                return None, 'map is malformed (%s)' % s
        cov['stale'] = stale_entries(cov, root)
        fresh = [s for s in phase_scenarios() if s in cov['scenarios'] and s not in cov['stale']]
        if not fresh:
            return None, ('map has no fresh phase scenario (missing or changed: %s); re-run '
                          '`wave.py coverage`' % ', '.join(phase_scenarios()))
    except (TypeError, AttributeError, ValueError, KeyError) as e:
        return None, 'map is malformed (%r)' % (e,)
    return cov, None


def usable(path, root=ROOT):
    """The map at path when it is fresh and complete, else None."""
    return check(path, root)[0]


def main(argv, root=ROOT, run_=subprocess.run, out=sys.stdout):
    scenarios, chunk, addrs, dry, restamp_ = None, DEFAULT_CHUNK, None, False, False
    pending_ = False
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
        elif a == '--restamp':
            restamp_ = True
        elif a == '--pending':
            pending_ = True
        else:
            print('wave coverage: unknown option %s' % a, file=sys.stderr)
            return 2
    if restamp_:
        rc, text = restamp(os.path.join(root, COVERAGE_REL), root)
        (out if rc == 0 else sys.stderr).write(text)
        return rc
    if scenarios is None:
        scenarios = default_scenarios()
    if pending_:
        return _main_pending(scenarios, chunk, dry, root, run_, out)
    try:
        addresses = (read_addresses(addrs) if addrs else
                     candidates(os.path.join(root, 'docs', 'wso', 'disagreements.json')))
    except (OSError, ValueError) as e:
        print('wave coverage: %s' % e, file=sys.stderr)
        return 2
    launches = plan(scenarios, addresses, chunk)
    if dry:
        out.write('coverage: %d candidate function(s), %d launch(es)\n' % (len(addresses), len(launches)))
        if not addrs:
            _, other = split_candidates(os.path.join(root, 'docs', 'wso', 'disagreements.json'))
            if other:
                out.write('coverage: dropped %d address(es) outside .text [0x%X, 0x%X): %s\n'
                          % (len(other), TEXT_START, TEXT_END, ', '.join(other)))
        out.write(_tooling_line() + '\n')
        kept = kept_scenarios(os.path.join(root, COVERAGE_REL), scenarios)
        if kept:
            out.write('coverage: would keep from the existing map: %s\n' % ', '.join(kept))
            old = read(os.path.join(root, COVERAGE_REL))
            note = stale_note(old, root, kept)
            if note:
                out.write(note + '\n')
        for s, i, n, k in launches:
            out.write('coverage %s chunk %d/%d: %d hooks\n' % (s, i, n, k))
        return 0
    results = run(scenarios, chunk, run=run_, root=root, addresses=addresses,
                  log=lambda m: out.write(m + '\n'))
    failed = {s: r['failed_chunks'] for s, r in results.items() if r['failed_chunks']}
    path = os.path.join(root, COVERAGE_REL)
    if failed:
        path = os.path.join(os.path.dirname(path), 'coverage.failed.json')
    data = write(path, results, root, merge=not failed)
    for s, r in data['scenarios'].items():
        if s not in results:
            continue
        out.write('%s: %d of %d hooks hit, %d refused, %d failed chunk(s)\n'
                  % (s, len(r['hits']), r['hooks'], len(r['refused']), len(r['failed_chunks'])))
    if failed:
        out.write('coverage: failed chunk(s): %s; wrote %s, NOT %s\n' % (
            '; '.join('%s %s' % (s, ','.join(map(str, c))) for s, c in sorted(failed.items())),
            path, COVERAGE_REL))
        return 1
    note = stale_note(data, root, [s for s in data['scenarios'] if s not in results])
    if note:
        out.write(note + '\n')
    out.write('wrote %s\n' % path)
    return 0


def _main_pending(scenarios, chunk, dry, root, run_, out):
    """`wave.py coverage --pending`: the count-only pass over the pending
    candidates only; the hits join coverage.json, the pending list empties
    and both files are committed."""
    ppath = os.path.join(root, PENDING_REL)
    mpath = os.path.join(root, COVERAGE_REL)
    pending = read_pending(ppath)
    addresses = sorted({int(norm(e['address']), 16) for e in pending})
    addresses = ['0x%X' % a for a in addresses if TEXT_START <= a < TEXT_END]
    if not pending:
        out.write('coverage: nothing pending\n')
        return 0
    launches = plan(scenarios, addresses, chunk)
    if dry:
        out.write('coverage: %d pending candidate(s), %d launch(es)\n'
                  % (len(addresses), len(launches)))
        out.write(_tooling_line() + '\n')
        for s, i, n, k in launches:
            out.write('coverage %s chunk %d/%d: %d hooks\n' % (s, i, n, k))
        return 0
    results = run(scenarios, chunk, run=run_, root=root, addresses=addresses,
                  log=lambda m: out.write(m + '\n'))
    failed = {s: r['failed_chunks'] for s, r in results.items() if r['failed_chunks']}
    if failed:
        out.write('coverage: failed chunk(s): %s; nothing merged, the pending list stays\n'
                  % '; '.join('%s %s' % (s, ','.join(map(str, c))) for s, c in sorted(failed.items())))
        return 1
    data = merge_pending_results(mpath, results, root)
    for s in results:
        r = data['scenarios'][s]
        out.write('%s: %d hits recorded over %d hooks\n' % (s, len(r['hits']), r['hooks']))
    with open(ppath, 'w', encoding='utf-8', newline='\n') as f:
        f.write('[]\n')
    err = commit_paths(root, [COVERAGE_REL, PENDING_REL],
                       'coverage: %d pending candidates measured' % len(pending))
    if err:
        out.write('coverage: merged, but %s\n' % err)
        return 1
    out.write('coverage: %d pending candidates measured; merged into %s and committed\n'
              % (len(pending), COVERAGE_REL))
    return 0


def _tooling_line():
    """Which Windows checkout supplies the injector and recorder the probe
    runs, and whether both are there."""
    try:
        play, oracle = _load('play'), _load('oracle')
        base = os.path.join(play.WIN_CHECKOUT, oracle.REPLAY_DIR_REL)
        have = {n: os.path.isfile(os.path.join(base, n)) for n in (oracle.INJECTOR, oracle.RECORDER)}
        return 'coverage: injector and recorder from %s: %s' % (
            base, ', '.join('%s %s' % (n, 'present' if ok else 'MISSING')
                            for n, ok in have.items()))
    except Exception as e:      # reporting only
        return 'coverage: cannot tell which checkout supplies the injector and recorder (%r)' % (e,)
