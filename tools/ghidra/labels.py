# iwd2r. Distributed under LICENSE.md.
"""Function labels for the vanilla iwd2.exe Ghidra project.

ours(src_dir) reads the `// 0xADDR` comments above function definitions in
src/**/*.cpp, ieex(db_path) reads IEex's function_names.db, and merge()
combines them with our names taking precedence. definitions(lines) gives the
line positions of the labelled definitions; tools/check_evidence.py uses it
so both tools agree on what a labelled function is.
"""

import os
import re

ADDRESS = re.compile(r'^// 0x([0-9A-Fa-f]+)\s*$')
OPERATOR = re.compile(r'((?:[\w~]+::)*operator\s*(?:\(\)|[^\s(]+))\s*\(')
PLAIN = re.compile(r'((?:\w+::)*~?\w+)\s*$')
LOOKAHEAD = 3


def _definition_name(line):
    """The function name defined on line, or None if it is not a definition."""
    if not line or line[0].isspace() or line.startswith(('//', '/*', '#', '{', '}')):
        return None
    text = line.rstrip()
    if text.endswith((';', ',')) or '(' not in text:
        return None
    m = OPERATOR.search(text)
    if m:
        return m.group(1).replace(' ', '')
    head = text[:text.index('(')]
    if '=' in head:
        return None
    m = PLAIN.search(head)
    return m.group(1) if m else None


def definitions(lines):
    """(address_line, definition_line, address, name) for each `// 0xADDR`
    line directly above a function definition (0-based line indexes)."""
    found = []
    for i, line in enumerate(lines):
        m = ADDRESS.match(line)
        if not m:
            continue
        for j in range(i + 1, min(i + 1 + LOOKAHEAD, len(lines))):
            follow = lines[j]
            if ADDRESS.match(follow) or follow.startswith(('{', '}')):
                break
            name = _definition_name(follow)
            if name:
                found.append((i, j, int(m.group(1), 16), name))
                break
            if follow.strip() and not follow.lstrip().startswith('//'):
                break
    return found


def _scan(lines):
    return [(addr, name) for _, _, addr, name in definitions(lines)]


def ours(src_dir):
    """(address, name) for each `// 0xADDR` line directly above a function
    definition in the .cpp files under src_dir, in file order."""
    result = []
    for root, dirs, files in os.walk(src_dir):
        dirs.sort()
        for name in sorted(files):
            if name.endswith('.cpp'):
                with open(os.path.join(root, name), encoding='utf-8',
                          errors='replace') as f:
                    result.extend(_scan(f.read().splitlines()))
    return result


def ieex(db_path):
    """(address, name) from IEex's `0xADDR NAME` lines, without FUN_ names."""
    result = []
    with open(db_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            parts = line.split()
            if len(parts) != 2 or parts[1].startswith('FUN_'):
                continue
            result.append((int(parts[0], 16), parts[1]))
    return result


def merge(ours, ieex):
    """One name per address, sorted by address. Our names win; an IEex name
    equal to one of our names elsewhere gets the prefix ieex_."""
    merged = {}
    for addr, name in ours:
        merged.setdefault(addr, name)
    taken = set(merged.values())
    for addr, name in ieex:
        if addr in merged:
            continue
        merged[addr] = 'ieex_' + name if name in taken else name
    return sorted(merged.items())
