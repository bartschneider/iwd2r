"""addresses -- give every row of disagreements.json an address.

xrec keys a function by the `// 0xADDR` comment on its definition; a row
whose address is null is one whose definition the fork never commented,
so no tool can hook, trace or measure it and every queue skips it.
`wave.py addresses` resolves those rows, five sources in order of trust:

    sub-name     a decompiler name carries its own address (sub_71C0C0,
                 CGameSprite::sub_71C0C0)
    ghidra       the exact Class::Method name in the Ghidra project's
                 symbol table, one headless QuerySymbol run for every
                 name at once
    ours         our `// 0xADDR` comment on a definition of the same name
                 in src/ at `first`
    fork-header  the fork's `// 0xADDR` comment on a definition of the
                 same name anywhere in its tree at the pin
    fork-decl    the fork's trailing `; // 0xADDR` comment on a
                 declaration of the same name in its headers at the pin

The first source that names exactly one address wins and is recorded on
the row as `address` plus `address_source`.  A name a source splits over
several addresses (two symbols, an overload) is settled by signature --
the row's own when it carries one, else the fork's .cpp definition of
the name -- and stays unresolved, its candidates listed in the output.
Resolved rows are written back into the file -- the one write the
queues leave to this tool.  Both trees are read by pinned sha through
git show, never a checkout.  Standard library only.

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

SOURCES = ('sub-name', 'ghidra', 'ours', 'fork-header', 'fork-decl')

SUB_NAME = re.compile(r'(?:\w+::)*sub_([0-9A-Fa-f]+)$')

DECL_TRAILING = re.compile(r'^(.+?)\s*//(.*)$')     # code // trailing comment
DECL_HEX = re.compile(r'0x([0-9A-Fa-f]{6,8})')      # addresses a comment names
DECL_CLASS = re.compile(r'\b(?:class|struct|union)\s+(\w+)')
DECL_NAMESPACE = re.compile(r'\bnamespace(?:\s+\w+)?\s*\{')

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
        # headless(read_only=True) files the log next to this worktree's
        # own project clone, not under PROJECT_DIR -- say where it is.
        return {}, ('ghidra: analyzeHeadless failed (%d); log: %s'
                    % (r.returncode,
                       os.path.join(mp.CLONES_DIR,
                                    os.path.basename(os.path.normpath(mp.ROOT)),
                                    os.path.basename(log))))
    return parse_query_output(r.stdout), ''


_names_cache = {}       # (repo, sha) -> {name: {address: params}} over src/*.cpp


def name_addresses(repo, ref):
    """name -> {address: params} for every `// 0xADDR`-commented definition
    in repo's ref src/*.cpp.  Every definition counts, an in-file overload
    included: it keeps both addresses with their parameter lists, so the
    signature match in resolve_addresses can settle which overload a row
    is instead of dropping the name.  Read with git show only; cached per
    (repo, sha); {} when the ref does not resolve."""
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
            for _above, at, address, name in lab.definitions(lines):
                names.setdefault(name, {}).setdefault(
                    '0x%X' % address, _def_params_of(lines, at))
        _names_cache[(repo, sha)] = names
    return _names_cache[(repo, sha)]


def _paren_span(text):
    """(start, end) of the first balanced (...) in text -- the parameter
    list of a declaration or a definition head -- or None when there is
    none or the one it opens never closes (a head split over lines)."""
    start = text.find('(')
    if start < 0:
        return None
    depth = 0
    for i in range(start, len(text)):
        if text[i] == '(':
            depth += 1
        elif text[i] == ')':
            depth -= 1
            if depth == 0:
                return start, i
    return None


def _def_params_of(lines, at):
    """The parameter list of the definition starting on line `at`, joined
    over its continuation lines; '' when it has none."""
    joined, i = lines[at], at
    while _paren_span(joined) is None and i + 1 < len(lines) and i - at < 8:
        i += 1
        joined += ' ' + lines[i].strip()
    span = _paren_span(joined)
    return joined[span[0] + 1:span[1]] if span else ''


def _norm_params(params):
    """A parameter list as one canonical string: whitespace collapsed,
    split on top-level commas (not those inside (), [] or <>), each
    parameter cut at its top-level default `= ...` -- the fork's headers
    spell defaults its .cpp definitions leave out.  `int a, T b = 0`
    becomes 'int a,T b'."""
    parts, depth, cur, cut = [], 0, '', False
    for ch in params:
        if ch in '([<':
            depth += 1
        elif ch in ')]>':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(cur)
            cur, cut = '', False
            continue
        if ch == '=' and depth == 0:
            cut = True          # the default value, not the type
        if not cut:
            cur += ch
    parts.append(cur)
    return ','.join(' '.join(p.split()) for p in parts)


_decls_cache = {}        # (repo, sha) -> {name: {address: params}} over src/*.h


def decl_addresses(repo, ref):
    """name -> {address: params} for every declaration repo's headers at
    ref label with a trailing `; // 0xADDR` comment (`CProjectileArrow();
    // 0x5324A0`, `void Fire(...) override;   // 0x52D9F0 (slot 27)`): a
    declaration split over lines is joined, the class it is declared in
    qualifies the name, the address is the comment's first .text hex (the
    fork writes the declaration's own address first, its vtable or
    deleting thunk after), and the parameters are the declaration's own,
    for overload matching.  Read with git show only; cached per (repo,
    sha); {} when the ref does not resolve."""
    wave = _tool('wave')
    sha = wave._resolve_tip(repo, ref) if ref else None
    if sha is None:
        return {}
    if (repo, sha) not in _decls_cache:
        decls = {}
        r = subprocess.run(['git', '-C', repo, 'ls-tree', '-r', '--name-only',
                            sha, '--', 'src/'], capture_output=True, text=True)
        for rel in r.stdout.splitlines():
            if not rel.endswith('.h'):
                continue
            text = wave._git_show(repo, sha, rel)
            if not text:
                continue
            for name, address, params in _header_decls(text.splitlines()):
                decls.setdefault(name, {}).setdefault(address, params)
        _decls_cache[(repo, sha)] = decls
    return _decls_cache[(repo, sha)]


def _code_and_comment(line):
    """(code, comment) when the line ends in a // comment that labels code
    before it, else (line, None): a comment-only line labels nothing, so a
    declaration's trailing comment never gets mistaken for the
    comment-above-definition form, and braces inside comments do not open
    scopes."""
    m = DECL_TRAILING.match(line)
    if (m and m.group(1).strip()
            and not m.group(1).strip().startswith(('*', '/*', '//'))):
        return m.group(1), m.group(2)
    return line, None


def _first_text_address(comment):
    """'0x<ADDR>' for the first .text hex the comment names, None when it
    names none: the declaration's own address comes first, what follows
    names its vtable (outside .text), its deleting thunk or an ICF twin
    (`0x52CCE0 (deleting thunk 0x52D7F0, vtable slot 0)`)."""
    cov = _tool('coverage')
    for h in DECL_HEX.findall(comment):
        address = int(h, 16)
        if cov.TEXT_START <= address < cov.TEXT_END:
            return '0x%X' % address
    return None


def _header_decls(lines):
    """(qualified name, '0x<ADDR>', params) for each trailing-commented
    function declaration of one header."""
    stack, pending = [], None       # classes we are inside; a class head
    for i, line in enumerate(lines):    # waiting for its {
        code, comment = _code_and_comment(line)
        if comment is not None and code.rstrip().endswith(';'):
            entry = _decl_entry(lines, i, code.rstrip()[:-1], comment, stack)
            if entry:
                yield entry
        ns = DECL_NAMESPACE.search(code)
        if pending is None:
            m = DECL_CLASS.search(code)
            if m and ';' not in code[:m.start()]:
                pending = m.group(1)
        for ch in code:
            if ch == '{':
                stack.append(None if ns is not None else pending)
                ns = pending = None
            elif ch == '}' and stack:
                stack.pop()
        if pending is not None and ';' in code:
            pending = None      # a forward declaration, not a class head


def _decl_entry(lines, i, decl, comment, stack):
    """(qualified name, '0x<ADDR>', params) for the declaration ending on
    line i with `decl` as its text and `comment` as its trailing comment:
    joined back over its continuation lines, qualified by the innermost
    enclosing class (a namespace qualifies nothing -- rows and .cpp
    definitions spell those names bare), addressed with the comment's
    first .text hex.  None when it is not a function (no parameter list)
    or the comment names no .text address."""
    parts, j = [decl.strip()], i - 1
    while j >= 0 and len(parts) < 8:
        prev = _code_and_comment(lines[j])[0].strip()
        if (not prev or prev.startswith(('*', '/*', '//', '#'))
                or prev.rstrip().endswith((';', '{', '}'))):
            break
        parts.insert(0, prev)       # a declaration split over lines
        j -= 1
    joined = ' '.join(parts)
    span = _paren_span(joined)
    if span is None:
        return None                 # not a function: nothing to overload
    m = _tool('wave')._labels().PLAIN.search(joined[:span[0]])
    address = _first_text_address(comment)
    if m is None or address is None:
        return None
    cls = next((c for c in reversed(stack) if c), None)
    return ((cls + '::' if cls else '') + m.group(1), address,
            joined[span[0] + 1:span[1]])


_def_params_cache = {}    # (repo, sha) -> {name: {normalised params}} over src/*.cpp


def definition_params(repo, ref):
    """name -> {normalised parameter list} for every function definition
    in repo's ref src/*.cpp -- the fork's own signatures, which settle
    which overload a name split over several addresses is when the row
    carries no signature of its own.  Read with git show only; cached per
    (repo, sha); {} when the ref does not resolve."""
    wave = _tool('wave')
    sha = wave._resolve_tip(repo, ref) if ref else None
    if sha is None:
        return {}
    if (repo, sha) not in _def_params_cache:
        lab = wave._labels()
        params = {}
        r = subprocess.run(['git', '-C', repo, 'ls-tree', '-r', '--name-only',
                            sha, '--', 'src/'], capture_output=True, text=True)
        for rel in r.stdout.splitlines():
            if not rel.endswith('.cpp'):
                continue
            text = wave._git_show(repo, sha, rel)
            if not text:
                continue
            lines = text.splitlines()
            for i, line in enumerate(lines):
                name = lab._definition_name(line)
                if name:
                    params.setdefault(name, set()).add(
                        _norm_params(_def_params_of(lines, i)))
        _def_params_cache[(repo, sha)] = params
    return _def_params_cache[(repo, sha)]


def _row_signatures(row, def_params):
    """The normalised signatures that can settle which overload a row is:
    its own `signature` when it carries one (the empty string included --
    that is a parameterless overload), else every parameter list the
    fork's .cpp definitions give that name."""
    if row.get('signature') is not None:
        return {_norm_params(row['signature'])}
    return set(def_params.get(row.get('name') or '', ()))


def resolve_addresses(rows, ghidra=None, ours=None, fork=None,
                      fork_decls=None, def_params=None):
    """Set `address` and `address_source` in place on every row without an
    address: the sub-name first, then the four name -> {address: params}
    indexes in trust order (ghidra's carries no params).  The first
    source that names exactly one address wins; a name a source splits
    over several addresses is settled by signature -- the row's own when
    it carries one, else the fork's .cpp definitions of the name -- and
    stays unresolved when no signature picks exactly one, every candidate
    kept for the caller.  Returns (hits, misses): (row, source) pairs and
    (row, {source: [address, ...]}) pairs."""
    ghidra, ours, fork = ghidra or {}, ours or {}, fork or {}
    fork_decls, def_params = fork_decls or {}, def_params or {}
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
        # ghidra names addresses only (no parameter lists to match a
        # signature against), the file sources name params too.
        candidates = {}
        for source, entries in zip(
                SOURCES[1:],
                ({a: None for a in ghidra.get(name, ())},
                 ours.get(name) or {}, fork.get(name) or {},
                 fork_decls.get(name) or {})):
            if len(entries) == 1:
                row['address'], row['address_source'] = next(iter(entries)), source
                hits.append((row, source))
                break
            if len(entries) > 1:
                sigs = _row_signatures(row, def_params)
                picks = {a for a, p in entries.items()
                         if p is not None and _norm_params(p) in sigs}
                if len(picks) == 1:
                    row['address'], row['address_source'] = picks.pop(), source
                    hits.append((row, source))
                    break
                candidates[source] = sorted(entries)
        else:
            misses.append((row, candidates))
    return hits, misses


def _source_counts(hits):
    """'(sub-name 1, ghidra 2, ours 0, fork-header 0, fork-decl 0)': every
    source in trust order with its hit count."""
    per = collections.Counter(source for _row, source in hits)
    return ', '.join('%s %d' % (s, per.get(s, 0)) for s in SOURCES)


def addresses(disagreements_path=None, repo=None, ours_ref='first',
              fork_pin=None, dry=False, ghidra_of=None, our_names=None,
              fork_names=None, fork_decls=None, def_params=None):
    """Resolve every address-less row of disagreements_path (the module
    docstring has the sources), print the summary and write the resolved
    rows back into the file -- the one write the queues leave to this
    tool -- unless dry.  ghidra_of/our_names/fork_names/fork_decls/
    def_params inject the five indexes so tests never launch Ghidra or
    git.  Returns (rc, text): rc 2 when the file cannot be read, else 0
    -- unresolved rows are an honest result, listed in the text with
    their candidates."""
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
    if fork_decls is None:
        fork_decls = decl_addresses(repo, fork_pin)
    if def_params is None:
        def_params = definition_params(repo, fork_pin)
    hits, misses = resolve_addresses(rows, ghidra=ghidra, ours=our_names,
                                     fork=fork_names, fork_decls=fork_decls,
                                     def_params=def_params)

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
