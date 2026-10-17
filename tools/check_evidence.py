"""Require decompiler evidence for every engine function a change touches.

Usage: python3 -I tools/check_evidence.py [--base REV] [--repo DIR]

A touched function is a labelled function (a `// 0xADDR` comment directly
above a definition, the rule of tools/ghidra/labels.py) in a src/**/*.cpp file
whose lines changed in `git diff <base>...HEAD`. A function spans from its
address comment to the first line after its definition that starts with `}`.
Changes outside every span (includes, globals, comments between functions),
changes in headers, and deleted functions or files are ignored; files new in
the branch count in full. Only committed changes are compared.

Each touched function needs docs/evidence/<ADDR>.md at HEAD (ADDR in upper-case
hex without 0x, docs/evidence/README.md has the format) whose front matter has
a non-empty `oracle_case` or `exemption`. Prints each offending function and
exits 1, else exits 0. A base that is not a commit or shares no history with
HEAD (a force push, or a `before` missing from the clone) is reported on
stderr and skipped with exit 0.

For the same touched functions the checker also reports how many evidence
files carry each `kind` (`oracle`, `replay` or `read-only`), and warns about
the files whose `kind` is missing or not one of those three. The kind is not
enforced: a missing one is only a warning.
"""

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                'ghidra'))
import labels  # noqa: E402

EVIDENCE_DIR = 'docs/evidence'
HUNK = re.compile(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@')
KINDS = ('oracle', 'replay', 'read-only')


def git(repo, *args, check=True):
    return subprocess.run(['git', '-c', 'core.quotePath=false', *args],
                          cwd=repo, check=check, capture_output=True,
                          text=True)


def diffable(repo, base):
    """Whether base is a commit sharing history with HEAD."""
    r = git(repo, 'rev-parse', '--verify', '--quiet', f'{base}^{{commit}}',
            check=False)
    if r.returncode != 0:
        return False
    return git(repo, 'merge-base', base, 'HEAD', check=False).returncode == 0


def front_matter(text):
    """The key/value pairs of a leading `---` block ({} if there is none).
    Values lose surrounding whitespace and one pair of matching quotes."""
    lines = text.splitlines()
    if not lines or lines[0].strip() != '---':
        return {}
    result = {}
    for line in lines[1:]:
        if line.strip() == '---':
            return result
        key, sep, value = line.partition(':')
        if not sep or not key.strip():
            continue
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in '"\'':
            value = value[1:-1]
        result[key.strip()] = value
    return {}


def evidence_name(address):
    return f'{address:X}.md'


def spans(lines):
    """(first, last, address, name) line indexes of each labelled function."""
    defs = labels.definitions(lines)
    result = []
    for k, (start, def_line, address, name) in enumerate(defs):
        limit = defs[k + 1][0] if k + 1 < len(defs) else len(lines)
        end = limit - 1
        for j in range(def_line + 1, limit):
            if lines[j].startswith('}'):
                end = j
                break
        result.append((start, end, address, name))
    return result


def changed_lines(diff):
    """{path: [(first, last, deletion)]} of 0-based new-file line ranges.
    A pure deletion after new line c becomes (c-1, c, True): the two lines
    around the gap, which touch a function only when both lie inside it (so
    deleting a whole function touches nothing)."""
    result = {}
    path = None
    for line in diff.splitlines():
        if line.startswith('+++ '):
            target = line[4:].rstrip('\t')
            path = target[2:] if target.startswith('b/') else None
            if path is not None:
                result.setdefault(path, [])
            continue
        m = HUNK.match(line)
        if not m or path is None:
            continue
        start, count = int(m.group(1)), int(m.group(2) or '1')
        if count:
            result[path].append((start - 1, start + count - 2, False))
        elif start > 0:
            result[path].append((start - 1, start, True))
    return result


def touched(repo, base):
    """(path, address, name) of each labelled function changed since base."""
    # --minimal: git's default diff gives up on optimality in large files
    # with big insertions and reports untouched neighbouring functions.
    diff = git(repo, 'diff', '--unified=0', '--no-color', '--no-ext-diff',
               '--minimal', '-M', f'{base}...HEAD', '--', 'src/').stdout
    result = []
    for path, ranges in sorted(changed_lines(diff).items()):
        if not path.endswith('.cpp') or not ranges:
            continue
        lines = git(repo, 'show', f'HEAD:{path}').stdout.splitlines()
        for first, last, address, name in spans(lines):
            if any(first <= lo and hi <= last if deletion
                   else lo <= last and first <= hi
                   for lo, hi, deletion in ranges):
                result.append((path, address, name))
    return result


def problem(repo, address):
    """(why, fields): why the evidence for address is insufficient, else
    None with the file's front matter."""
    rel = f'{EVIDENCE_DIR}/{evidence_name(address)}'
    r = git(repo, 'show', f'HEAD:{rel}', check=False)
    if r.returncode != 0:
        return f'no {rel}', None
    fields = front_matter(r.stdout)
    recorded = fields.get('address', '')
    try:
        if recorded and int(recorded, 16) != address:
            return f'{rel} records address {recorded}', fields
    except ValueError:
        return f'{rel} has a malformed address {recorded!r}', fields
    if not fields.get('oracle_case') and not fields.get('exemption'):
        return f'{rel} has neither oracle_case nor exemption', fields
    return None, fields


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--base', default='origin/main')
    ap.add_argument('--repo', default='.')
    args = ap.parse_args(argv)

    if not diffable(args.repo, args.base):
        print(f'check_evidence: cannot diff against {args.base}; skipping',
              file=sys.stderr)
        return 0
    errors = []
    kinds = {}
    untagged = []
    found = touched(args.repo, args.base)
    for path, address, name in found:
        why, fields = problem(args.repo, address)
        if why:
            errors.append(f'{path}: 0x{address:X} {name}: {why}')
            continue
        kind = fields.get('kind')
        if kind in KINDS:
            kinds[kind] = kinds.get(kind, 0) + 1
            continue
        rel = f'{EVIDENCE_DIR}/{evidence_name(address)}'
        if kind:
            untagged.append(f'warning: {rel} has kind: {kind!r}; expected '
                            f'one of {", ".join(KINDS)}')
        else:
            untagged.append(f'warning: {rel} has no kind: '
                            f'({", ".join(KINDS)})')
    for line in errors:
        print(line)
    if errors:
        print(f'{len(errors)} touched function(s) lack evidence; '
              'see docs/evidence/README.md')
    else:
        print(f'{len(found)} touched function(s), all with evidence')
    counted = sum(kinds.values())
    if counted:
        print('evidence kinds: '
              + ', '.join(f'{k} {kinds.get(k, 0)}' for k in KINDS)
              + f' ({counted} of {len(found)} touched function(s))')
    for line in untagged:
        print(line)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
