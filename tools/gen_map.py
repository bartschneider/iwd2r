# iwd2r. Distributed under LICENSE.md.
"""Generate docs/MAP.md, a per-family map of the source tree.

Usage: python3 -I tools/gen_map.py [--repo DIR] [--check]

Every file under src/ is assigned to one family by its basename, first match
wins, in this order: AI (CAI*), Game (CGame*, CInfGame*, CDungeonMaster*,
CSpawn*, CTimer*, CWeather*), Screens (CScreen*), UI (CUI*, CButton*), Video
(CVid*, CVideo*, CInfinity*, CChitin3d*, CBlood*, CColor*, CParticle*),
Resources (CRes*, CDimm*, CCache*, C2DArray*, CCreatureFile*, FileFormat*),
Sound (CSound*, CVoice*, anything under src/music/), Messaging (CMessage*,
CNetwork*, CGameSpy*, CChatBuffer*, CMultiplayerSettings*), Engine core
(CChitin*, CBaldur*, main.*) and Other. Two overrides are tested first so
that CGameSpy* lands in Messaging (not Game, via CGame*) and CChitin3d* in
Video (not Engine core, via CChitin*). Two more rows, compat and
platform, cover every file in those directories.

Per family it reports file count, line count, lines containing
`TODO: Incomplete`, address comments (`// 0x<hex>` at the start of a comment
line) and the distinct `platform/<name>.h` headers included.

The commit short hash sits on its own line ("Generated from commit ...").
--check regenerates the map in memory and compares it with docs/MAP.md,
ignoring only that line, so committing does not invalidate the check. It
prints a one-line reason and exits 1 on a difference or a missing file, else 0.
"""

import argparse
import os
import re
import subprocess
import sys

FAMILIES = [
    ('AI', ('CAI',)),
    ('Game', ('CGame', 'CInfGame', 'CDungeonMaster', 'CSpawn', 'CTimer', 'CWeather')),
    ('Screens', ('CScreen',)),
    ('UI', ('CUI', 'CButton')),
    ('Video', ('CVid', 'CVideo', 'CInfinity', 'CChitin3d', 'CBlood', 'CColor', 'CParticle')),
    ('Resources', ('CRes', 'CDimm', 'CCache', 'C2DArray', 'CCreatureFile', 'FileFormat')),
    ('Sound', ('CSound', 'CVoice')),
    ('Messaging', ('CMessage', 'CNetwork', 'CGameSpy', 'CChatBuffer', 'CMultiplayerSettings')),
    ('Engine core', ('CChitin', 'CBaldur', 'main')),
]
# Longer prefixes that must win over a shorter prefix of an earlier family.
OVERRIDES = [('CGameSpy', 'Messaging'), ('CChitin3d', 'Video')]
ORDER = [n for n, _ in FAMILIES] + ['Other', 'compat', 'platform']
HASH_PREFIX = 'Generated from commit '
INCLUDE = re.compile(r'^\s*#\s*include\s+"platform/([^"/]+\.h)"')
ADDRESS = re.compile(r'^\s*// 0x[0-9A-Fa-f]+')


def family_of(rel):
    """rel is a path relative to src/, with forward slashes."""
    if rel.startswith('music/'):
        return 'Sound'
    base = rel.rsplit('/', 1)[-1]
    for prefix, fam in OVERRIDES:
        if base.startswith(prefix):
            return fam
    for fam, prefixes in FAMILIES:
        if any(base.startswith(p) for p in prefixes):
            return fam
    return 'Other'


def walk(root):
    for d, dirs, files in os.walk(root):
        dirs.sort()
        for f in sorted(files):
            full = os.path.join(d, f)
            yield os.path.relpath(full, root).replace(os.sep, '/'), full


def tally(stats, fam, path):
    s = stats[fam]
    with open(path, 'rb') as f:
        text = f.read().decode('latin-1')
    s['files'] += 1
    for line in text.splitlines():
        s['lines'] += 1
        if 'TODO: Incomplete' in line:
            s['todo'] += 1
        if ADDRESS.match(line):
            s['addr'] += 1
        m = INCLUDE.match(line)
        if m:
            s['hdrs'].add(m.group(1))


def commit_hash(repo):
    try:
        r = subprocess.run(['git', 'rev-parse', '--short', 'HEAD'], cwd=repo,
                           capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip():
            return r.stdout.strip()
    except OSError:
        pass
    return 'unknown'


def generate(repo):
    stats = {n: {'files': 0, 'lines': 0, 'todo': 0, 'addr': 0, 'hdrs': set()} for n in ORDER}
    for rel, full in walk(os.path.join(repo, 'src')):
        tally(stats, family_of(rel), full)
    for fam in ('compat', 'platform'):
        for _, full in walk(os.path.join(repo, fam)):
            tally(stats, fam, full)
    tot = [sum(stats[n][k] for n in ORDER) for k in ('files', 'lines', 'todo', 'addr')]
    out = [
        '# Subsystem map', '',
        HASH_PREFIX + '`' + commit_hash(repo) + '` by `tools/gen_map.py`. Do not edit; '
        'CI runs `python3 -I tools/gen_map.py --check`.', '',
        'Totals: %d files, %d lines, %d `TODO: Incomplete`, %d address comments.' % tuple(tot), '',
        '| Family | Files | Lines | `TODO: Incomplete` | Address comments | Platform headers included |',
        '|---|---:|---:|---:|---:|---|',
    ]
    for n in ORDER:
        s = stats[n]
        out.append('| %s | %d | %d | %d | %d | %s |' % (
            n, s['files'], s['lines'], s['todo'], s['addr'],
            ', '.join(sorted(s['hdrs'])) or '—'))
    out.append('| **Total** | %d | %d | %d | %d | |' % tuple(tot))
    return '\n'.join(out) + '\n'


def body(text):
    return [l for l in text.splitlines() if not l.startswith(HASH_PREFIX)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default='.')
    ap.add_argument('--check', action='store_true')
    args = ap.parse_args()
    repo = os.path.abspath(args.repo)
    path = os.path.join(repo, 'docs', 'MAP.md')
    text = generate(repo)
    if args.check:
        try:
            with open(path, encoding='utf-8') as f:
                old = f.read()
        except OSError:
            print('docs/MAP.md is missing; run python3 -I tools/gen_map.py')
            return 1
        if body(old) != body(text):
            print('docs/MAP.md is out of date; run python3 -I tools/gen_map.py')
            return 1
        return 0
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
