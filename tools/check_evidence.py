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
the files whose `kind` is missing or not one of those three. Outside the
rules-math set the kind is not enforced: a missing one is only a warning.
Functions in RULES_MATH below — the rules-math set, whose correctness is the
D&D 3e rules themselves (attack rolls, AC, saving throws, damage
resolution, effect application; triaged in docs/evidence/rules-math-triage.md)
— must have `kind: oracle`: a replay or a read-only exemption fails, and an
untagged document counts as not-oracle instead of a warning. The report
also prints the per-kind split of the touched rules-math functions.
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

# The rules-math set: functions whose correctness IS a D&D 3e rule — the
# to-hit and AC math, saving throws, damage resolution, and the
# effect-application paths that roll saves and apply damage. Keyed by
# function address (the `// 0xADDR` comment, the same address the evidence
# file name uses), so the set survives renames. Hand-maintained and
# conservative: an address enters only when the function computes or applies
# a rule; callers, orchestration and display stay out. Each entry says what
# the function computes and lists the ones considered and left out with the
# reasons. A diff touching one of these addresses requires evidence with
# `kind: oracle` — an exemption or a replay does not pass.
RULES_MATH = frozenset({
    # Attack rolls, to-hit modifiers and AC.
    0x723CC0,  # CGameSprite::GetAC: base 10, armor-capped DEX, blind penalty,
               # monk WIS bonus
    0x723DB0,  # touch AC: GetAC minus the armor and shield components
    0x723EA0,  # DEX-only AC bonus (touch attacks)
    0x723F60,  # monk WIS AC bonus (unarmored, no shield)
    0x73B760,  # CGameSprite::Hit: the d20 to-hit, nat 20/1, flat-footed,
               # iteratives, crit confirm, concealment
    0x73C490,  # concealment / miss-chance roll against an attack
    0x73CD00,  # bane weapon attack bonus
    0x73CF10,  # weapon proficiency attack bonus/penalty
    0x73D080,  # two-weapon fighting attack penalties
    0x73D1B0,  # situational attack modifiers (flanking, prone, charging...)
    0x547020,  # CRuleTables::GetIterativeAttackBonus (src/CRuleTables.cpp):
               # max(0, bonus - step*attack)
    0x4E9840,  # CItem critical threat range (src/CItem.cpp): base 20, item
               # types, Keen
    # Saving throws.
    0x4A42F0,  # CGameEffect::CheckSave (src/CGameEffect.cpp): the save d20
               # vs DC 10 + level + saveMod, racial/class save bonuses
    0x4A9E30,  # CGameEffectDamage::CheckSave (src/CGameEffect.cpp): save
               # (for half) against a damage effect
    0x547040,  # CRuleTables::GetSaveAdjustmentForSourceSprite
               # (src/CRuleTables.cpp)
    0x547620,  # CRuleTables::sub_547620 (src/CRuleTables.cpp): SPIN108
               # animal-empathy save adjustment
    0x4A3310,  # CGameEffect::CheckAdd (src/CGameEffect.cpp): the
               # pre-application gate — immunity, probability, spell
               # resistance penetration
    # Damage resolution.
    0x73D560,  # CGameSprite::Damage: weapon dice, STR (×1.5 two-handed,
               # half off-hand), sneak attack and uncanny dodge, crit
               # multiplier, damage-type mapping
    0x4A7900,  # CGameEffectDamage::ApplyEffect (src/CGameEffect.cpp): dice
               # roll, resistance subtraction, damage reduction, HP floor
    0x5845D0,  # IcewindMisc::Roll (src/IcewindMisc.cpp): sum of
               # rand()%sides+1 — the dice primitive
    0x447C20,  # CDamageReduction::Apply (src/CDerivedStats.cpp)
    0x447E30,  # CDamageReduction::GetEffectiveAmount
               # (src/CDerivedStats.cpp)
    0x448130,  # CDamageReductionList::GetBestGeneric
               # (src/CDerivedStats.cpp)
    0x4481A0,  # CDamageReductionList::GetBest (src/CDerivedStats.cpp)
    0x448250,  # CDamageReductionList::Apply (src/CDerivedStats.cpp): the
               # DR X/enchantment rule
    # Effect application paths that carry rules math.
    0x72DE60,  # CGameSprite::ProcessEffectList (src/CGameSprite.cpp): the
               # per-tick effect pass; rolls the four save d20s and the d100
    0x4A3030,  # CGameEffect::ResolveEffect (src/CGameEffect.cpp): effect
               # duration units (rounds ×15 ticks) and expiry
    0x733200,  # CGameSprite::HandlePersistantEffects
               # (src/CGameSprite.cpp): persistent-effect timing
})

RULES_MATH_NOTE = ' (rules math: requires kind: oracle)'


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
    rules_kinds = {}
    rules_found = 0
    found = touched(args.repo, args.base)
    for path, address, name in found:
        rules = address in RULES_MATH
        why, fields = problem(args.repo, address)
        if rules:
            rules_found += 1
        if why:
            if rules:
                why += RULES_MATH_NOTE
            errors.append(f'{path}: 0x{address:X} {name}: {why}')
            continue
        kind = fields.get('kind')
        if kind in KINDS:
            kinds[kind] = kinds.get(kind, 0) + 1
            if rules:
                rules_kinds[kind] = rules_kinds.get(kind, 0) + 1
        elif not rules:
            # Inside the set an untagged document is the rules-math error
            # below; outside it stays a warning.
            rel = f'{EVIDENCE_DIR}/{evidence_name(address)}'
            if kind:
                untagged.append(f'warning: {rel} has kind: {kind!r}; expected '
                                f'one of {", ".join(KINDS)}')
            else:
                untagged.append(f'warning: {rel} has no kind: '
                                f'({", ".join(KINDS)})')
        if rules and kind != 'oracle':
            current = f'kind: {kind}' if kind else 'no kind'
            errors.append(f'{path}: 0x{address:X} {name}: rules math: '
                          f'evidence has {current}; requires kind: oracle '
                          f'(an exemption or a replay does not pass)')
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
    if rules_found:
        print('rules math: '
              + ', '.join(f'{k} {rules_kinds.get(k, 0)}' for k in KINDS)
              + f' ({rules_found} of {len(found)} touched function(s); '
              'set and triage in docs/evidence/rules-math-triage.md)')
    for line in untagged:
        print(line)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
