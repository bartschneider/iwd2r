# iwd2r. Distributed under LICENSE.md.
"""Verify the iwd2r notice lines.

Usage: python3 -I tools/check_notices.py [--base 4fa36bc] [--repo DIR]

Compares the working tree against the base commit (default 4fa36bc, the
upstream fork point) with `git diff --name-status <base>` and untracked files
from `git ls-files --others --exclude-standard`. Only the first line of each
file is read.

  * A source file that differs from the base must start with
    `// Modified for iwd2r; see NOTICE.md` (`# ` form in CMake and Python).
  * A source file absent from the base must start with
    `// iwd2r. Distributed under LICENSE.md.` (`# ` form in CMake and Python).
  * A file that differs from the base AND already carries the new-file header
    is an error: it exists upstream, so it needs the modified notice.
  * Deleted files are ignored.

Exempt: third_party/, docs/, .github/ and non-source files. Source files are
*.cpp, *.h, *.c, *.py, CMakeLists.txt and *.cmake.

--repo selects the repository directory (default: the current directory).
Prints each offending path with the reason and exits 1, else exits 0.
"""

import argparse
import os
import subprocess
import sys

EXEMPT_DIRS = ('third_party/', 'docs/', '.github/')
SLASH_EXT = ('.cpp', '.h', '.c')
HASH_EXT = ('.py', '.cmake')


def comment_prefix(path):
    """Return '//' or '#' for a source file, None for an exempt one."""
    if path.startswith(EXEMPT_DIRS):
        return None
    name = os.path.basename(path)
    if name == 'CMakeLists.txt' or name.endswith(HASH_EXT):
        return '#'
    if name.endswith(SLASH_EXT):
        return '//'
    return None


def git(repo, *args):
    return subprocess.run(['git', *args], cwd=repo, check=True,
                          capture_output=True, text=True).stdout


def first_line(repo, path):
    with open(os.path.join(repo, path), 'r', encoding='utf-8',
              errors='replace') as f:
        return f.readline().rstrip('\r\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--base', default='4fa36bc')
    ap.add_argument('--repo', default='.')
    args = ap.parse_args()

    modified, added = set(), set()
    for line in git(args.repo, 'diff', '--name-status', '--no-renames',
                    args.base).splitlines():
        status, path = line.split('\t', 1)
        if status.startswith('D'):
            continue
        (added if status.startswith('A') else modified).add(path)
    for path in git(args.repo, 'ls-files', '--others',
                    '--exclude-standard').splitlines():
        added.add(path)

    errors = []
    for path in sorted(modified | added):
        prefix = comment_prefix(path)
        if prefix is None or not os.path.isfile(os.path.join(args.repo, path)):
            continue
        first = first_line(args.repo, path)
        mod_line = prefix + ' Modified for iwd2r; see NOTICE.md'
        new_line = prefix + ' iwd2r. Distributed under LICENSE.md.'
        if path in modified:
            if first == new_line:
                errors.append((path, 'exists in base but carries the new-file header; use "%s"' % mod_line))
            elif first != mod_line:
                errors.append((path, 'differs from base; first line must be "%s"' % mod_line))
        elif first != new_line:
            errors.append((path, 'new file; first line must be "%s"' % new_line))

    for path, reason in errors:
        print('%s: %s' % (path, reason))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
