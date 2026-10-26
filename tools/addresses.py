"""addresses -- give every row of disagreements.json an address.

xrec keys a function by the `// 0xADDR` comment on its definition; a row
whose address is null is one whose definition the fork never commented,
so no tool can hook, trace or measure it and every queue skips it.
`wave.py addresses` resolves those rows, four sources in order of trust:

    sub-name     a decompiler name carries its own address (sub_71C0C0,
                 CGameSprite::sub_71C0C0)
    ghidra       the exact Class::Method name in the Ghidra project's
                 symbol table, one headless QuerySymbol run for every
                 name at once
    ours         our `// 0xADDR` comment on a definition of the same name
                 in src/ at `first`
    fork-header  the fork's `// 0xADDR` comment on a definition of the
                 same name anywhere in its tree at the pin

The first source that names exactly one address wins and is recorded on
the row as `address` plus `address_source`; a name several addresses
point at (two symbols, an overload) stays unresolved, its candidates
listed in the output.  Resolved rows are written back into the file --
the one write the queues leave to this tool.  Both trees are read by
pinned sha through git show, never a checkout.  Standard library only.

Usage: python3 -I tools/wave.py addresses [--dry]
"""

import collections
import importlib.util
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

SOURCES = ('sub-name', 'ghidra', 'ours', 'fork-header')

SUB_NAME = re.compile(r'(?:\w+::)*sub_([0-9A-Fa-f]+)$')

_modules = {}       # tools/<name>.py and tools/ghidra/<name>.py, loaded by path


def _tool(name, ghidra=False):
    """tools/<name>.py (or tools/ghidra/<name>.py), loaded by path: under
    -I tools/ is not on sys.path.  Cached per process."""
    key = ('ghidra/' if ghidra else '') + name
    if key not in _modules:
        spec = importlib.util.spec_from_file_location(
            'addresses_' + key.replace('/', '_'), os.path.join(HERE, key + '.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _modules[key] = module
    return _modules[key]


def sub_name_address(name):
    """'0x<ADDR>' for a decompiler name that carries its own address
    (sub_71C0C0, CGameSprite::sub_71C0C0) inside .text, else None."""
    m = SUB_NAME.fullmatch(name or '')
    if not m:
        return None
    cov = _tool('coverage')
    address = int(m.group(1), 16)
    return ('0x%X' % address
            if cov.TEXT_START <= address < cov.TEXT_END else None)


def parse_query_output(text):
    """QuerySymbol's stdout -> name -> {address}: one `QUERY <name>\t
    0xADDR\t<full name>\tfunction=yes` line per match.  A data symbol
    (function=no) is no function to hook and `not found` names nothing,
    so neither yields an entry; several lines for one name keep every
    address (the name is ambiguous)."""
    out = {}
    for line in text.splitlines():
        if 'QUERY ' not in line:
            continue
        fields = (line.split('QUERY ', 1)[1].rstrip()
                  .removesuffix('(GhidraScript)').rstrip().split('\t'))
        if len(fields) != 4 or fields[3] != 'function=yes':
            continue
        # %X drops Ghidra's zero padding (004AD250) for the row format
        out.setdefault(fields[0], set()).add('0x%X' % int(fields[1], 16))
    return out


def ghidra_addresses(names):
    """name -> {address} for the names the Ghidra project's symbol table
    holds: one headless QuerySymbol run for every name at once (the
    project opens read-only, on this worktree's own copy).  Returns
    (mapping, note): the note says why the mapping is empty when it is
    (no project, a failed run) and '' when the run went through."""
    if not names:
        return {}, ''
    mp = _tool('make_project', ghidra=True)
    if not mp.project_exists():
        return {}, 'ghidra: no project; run tools/ghidra/make_project.py'
    log = os.path.join(mp.PROJECT_DIR, 'headless-addresses.log')
    r = mp.headless(['-process', mp.PROGRAM, '-noanalysis', '-readOnly',
                     '-postScript', 'QuerySymbol.java', *names], log,
                    read_only=True)
    if r.returncode != 0:
        return {}, ('ghidra: analyzeHeadless failed (%d); log: %s'
                    % (r.returncode, log))
    return parse_query_output(r.stdout), ''


_names_cache = {}       # (repo, sha) -> {name: {address}} over all src/*.cpp


def name_addresses(repo, ref):
    """name -> {address} for every `// 0xADDR`-commented definition in
    repo's ref src/*.cpp.  A name defined twice in one file (an overload)
    is left out even when one definition carries the comment; a name two
    files each define once keeps both addresses, so a lookup can come
    back ambiguous.  Read with git show only; cached per (repo, sha); {}
    when the ref does not resolve."""
    wave = _tool('wave')
    sha = wave._resolve_tip(repo, ref) if ref else None
    if sha is None:
        return {}
    if (repo, sha) not in _names_cache:
        lab = wave._labels()
        names = {}
        r = subprocess.run(['git', '-C', repo, 'ls-tree', '-r', '--name-only',
                            sha, '--', 'src/'], capture_output=True, text=True)
        for rel in r.stdout.splitlines():
            if not rel.endswith('.cpp'):
                continue
            text = wave._git_show(repo, sha, rel)
            if not text:
                continue
            lines = text.splitlines()
            per = collections.Counter(n for n in map(lab._definition_name, lines)
                                      if n)
            for _above, _at, address, name in lab.definitions(lines):
                if per[name] == 1:
                    names.setdefault(name, set()).add('0x%X' % address)
        _names_cache[(repo, sha)] = names
    return _names_cache[(repo, sha)]


def resolve_addresses(rows, ghidra=None, ours=None, fork=None):
    """Set `address` and `address_source` in place on every row without an
    address: the sub-name first, then the three name -> {address} indexes.
    The first source that names exactly one address wins; a name several
    candidates point at stays unresolved, all its candidates kept for the
    caller.  Returns (hits, misses): (row, source) pairs and (row,
    {source: [address, ...]}) pairs."""
    ghidra, ours, fork = ghidra or {}, ours or {}, fork or {}
    hits, misses = [], []
    for row in rows:
        if row.get('address'):
            continue
        name = row.get('name') or ''
        address = sub_name_address(name)
        if address:
            row['address'], row['address_source'] = address, 'sub-name'
            hits.append((row, 'sub-name'))
            continue
        candidates = {source: sorted(index.get(name, ()))
                      for source, index in zip(SOURCES[1:], (ghidra, ours, fork))}
        for source, addresses in candidates.items():
            if len(addresses) == 1:
                row['address'], row['address_source'] = addresses[0], source
                hits.append((row, source))
                break
        else:
            misses.append((row, {s: a for s, a in candidates.items() if a}))
    return hits, misses


def _source_counts(hits):
    """'(sub-name 1, ghidra 2, ours 0, fork-header 0)': every source in
    trust order with its hit count."""
    per = collections.Counter(source for _row, source in hits)
    return ', '.join('%s %d' % (s, per.get(s, 0)) for s in SOURCES)


def addresses(disagreements_path=None, repo=None, ours_ref='first',
              fork_pin=None, dry=False, ghidra_of=None, our_names=None,
              fork_names=None):
    """Resolve every address-less row of disagreements_path (the module
    docstring has the sources), print the summary and write the resolved
    rows back into the file -- the one write the queues leave to this
    tool -- unless dry.  ghidra_of/our_names/fork_names inject the three
    indexes so tests never launch Ghidra or git.  Returns (rc, text):
    rc 2 when the file cannot be read, else 0 -- unresolved rows are an
    honest result, listed in the text with their candidates."""
    wave = _tool('wave')
    disagreements_path = disagreements_path or wave.DISAGREEMENTS_PATH
    repo = repo or wave.ROOT
    fork_pin = fork_pin or wave.DEFAULT_FORK_BASE
    try:
        with open(disagreements_path, encoding='utf-8') as f:
            rows = json.load(f)
    except (OSError, ValueError) as e:
        return 2, 'addresses: %s: %s\n' % (disagreements_path, e)
    pending = [r for r in rows if not r.get('address')]
    if not pending:
        return 0, 'addresses: every row already has an address\n'
    names = sorted({r['name'] for r in pending if r.get('name')})
    if ghidra_of is None:
        want = [n for n in names if not sub_name_address(n)]
        ghidra, note = ghidra_addresses(want)
    else:
        ghidra, note = ghidra_of(names), ''
    if our_names is None:
        our_names = name_addresses(repo, ours_ref)
    if fork_names is None:
        fork_names = name_addresses(repo, fork_pin)
    hits, misses = resolve_addresses(rows, ghidra=ghidra, ours=our_names,
                                     fork=fork_names)

    out = ['addresses: %d row(s) without an address: %d resolved (%s), '
           '%d unresolved'
           % (len(pending), len(hits), _source_counts(hits),
              len(pending) - len(hits))]
    theirs = [r for r in pending if r['category'] == 'only_theirs']
    if theirs:
        ot_hits = [(r, s) for r, s in hits if r['category'] == 'only_theirs']
        out.append('addresses: only_theirs %d of them: %d resolved (%s), '
                   '%d unresolved'
                   % (len(theirs), len(ot_hits), _source_counts(ot_hits),
                      len(theirs) - len(ot_hits)))
    if note:
        out.append('addresses: ' + note)
    listed = [(r, c) for r, c in misses if c]
    if listed:
        out.append('addresses: unresolved with candidates:')
        for r, c in sorted(listed, key=lambda p: (p[0]['file'], p[0]['name'])):
            why = '; '.join('%s: %s' % (s, ', '.join(c[s]))
                            for s in SOURCES if s in c)
            out.append('  %s  %s  %s' % (r['name'], r['file'], why))
    out.append('addresses: %d unresolved without candidates (no source '
               'named them)' % (len(misses) - len(listed)))
    if dry:
        out.append('addresses: dry run: nothing written')
    elif hits:
        wave._write_rows(disagreements_path, rows)
        out.append('addresses: wrote %d resolved row(s) to %s'
                   % (len(hits), disagreements_path))
    return 0, '\n'.join(out) + '\n'


def main(argv=None):
    """`wave.py addresses [--dry]`: --dry reports the resolutions but
    writes nothing back."""
    argv = list(sys.argv[1:] if argv is None else argv)
    dry = False
    for a in argv:
        if a == '--dry':
            dry = True
        else:
            print('addresses: unknown option %s' % a, file=sys.stderr)
            print('usage: wave.py addresses [--dry]', file=sys.stderr)
            return 2
    rc, out = addresses(dry=dry)
    sys.stdout.write(out)
    return rc


if __name__ == '__main__':
    sys.exit(main())
