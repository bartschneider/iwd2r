# Rules-math triage

Round-1 triage of the rules-math set (`RULES_MATH` in
`tools/check_evidence.py`), 2026-10-17. The set is the list of functions
whose correctness **is** a D&D 3e rule: get one of these wrong and the game
computes the wrong numbers. The checker requires `kind: oracle` evidence for
every address in the set that a diff touches; this doc says what is in the
set, what evidence each entry has today, and what it would take to convert
the rest.

## What is in the set

An address enters only when the function computes or applies a rule:
attack rolls and the AC the roll is made against, saving throws and their
DCs, damage and its resistance/damage-reduction pipeline, the dice
primitive, and the effect-application paths that roll saves and apply
damage. Keyed by function address (the `// 0xADDR` comment, the same
address the evidence file name uses), so the set survives renames;
hand-maintained; deliberately conservative, because a wrongly-included
function creates gate friction for streams that only meant to refactor it.

Out of scope, on purpose: callers and orchestration (the Attack loop's
timing and number of swings — the rules it invokes are in the set
themselves), effect-list plumbing (dispatch and insertion), display
(damage feedback text), and pure data tables (HP/XP progressions —
candidates for a later round; see "Considered and left out").

## Enforcement

For a touched address in the set, the checker (`python3 -I
tools/check_evidence.py --base <rev>`) fails unless the evidence doc has
`kind: oracle`:

- read-only or replay kind →
  `src/CGameEffect.cpp: 0x4A9E30 CGameEffect_opcode_12_6: rules math: evidence has kind: read-only; requires kind: oracle (an exemption or a replay does not pass)`
- no doc → `no docs/evidence/4A9E30.md (rules math: requires kind: oracle)`
- doc without a `kind:` → counted as not-oracle (an error here, while
  outside the set it stays a warning)

The report adds a `rules math: oracle N, replay N, read-only N (...)`
summary line next to the all-function kinds line. Outside the set nothing
changes: warn-only, as before.

`kind: oracle` with a pending case satisfies the checker (README
convention), but a pending case without a body compares nothing. Converting
a target below should mean a **running** comparison, not a placeholder —
the "evidence today" column distinguishes the two.

## Counts

28 functions: **16 oracle** (6 with running comparisons, 10 pointing at
body-less pending cases), **11 read-only** (all exempted), **1 replay**.

## Triage

### Attack rolls, to-hit modifiers and AC — 12

| Address | Function | Src | Kind | Evidence today |
|---|---|---|---|---|
| 0x723CC0 | CGameSprite::GetAC | CGameSprite.cpp | oracle | runs: combat/armor_class |
| 0x723DB0 | CGameSprite_sub_723DB0 (touch AC) | CGameSprite.cpp | oracle | runs: combat/armor_class |
| 0x723EA0 | CGameSprite_sub_723EA0 (DEX-only AC) | CGameSprite.cpp | oracle | runs: combat/dex_armor_class |
| 0x723F60 | CGameSprite::sub_723F60 (monk WIS AC) | CGameSprite.cpp | oracle | runs: combat/armor_class |
| 0x73B760 | CGameSprite::Hit | CGameSprite.cpp | oracle | pending/hit (no body) |
| 0x73C490 | concealment/miss-chance roll | CGameSprite.cpp | oracle | pending/hit (no body) |
| 0x73CD00 | bane weapon attack bonus | CGameSprite.cpp | oracle | pending/hit (no body) |
| 0x73CF10 | weapon proficiency attack bonus | CGameSprite.cpp | oracle | pending/hit (no body) |
| 0x73D080 | two-weapon fighting penalties | CGameSprite.cpp | oracle | pending/hit (no body) |
| 0x73D1B0 | situational attack modifiers | CGameSprite.cpp | oracle | pending/hit (no body) |
| 0x547020 | CRuleTables::GetIterativeAttackBonus | CRuleTables.cpp | oracle | runs: rules/iterative_attack_bonus |
| 0x4E9840 | CItem critical threat range | CItem.cpp | oracle | pending/hit (no body) |

### Saving throws — 5

| Address | Function | Src | Kind | Evidence today |
|---|---|---|---|---|
| 0x4A42F0 | CGameEffect::CheckSave | CGameEffect.cpp | oracle | pending/check_save (no body) |
| 0x4A9E30 | CGameEffect_opcode_12_6 (damage save) | CGameEffect.cpp | read-only | exemption (below) |
| 0x547040 | CRuleTables::GetSaveAdjustmentForSourceSprite | CRuleTables.cpp | read-only | exemption (below) |
| 0x547620 | CRuleTables::sub_547620 (SPIN108) | CRuleTables.cpp | read-only | exemption (below) |
| 0x4A3310 | CGameEffect_CheckAdd (immunity/SR gate) | CGameEffect.cpp | read-only | exemption (below) |

### Damage resolution — 8

| Address | Function | Src | Kind | Evidence today |
|---|---|---|---|---|
| 0x73D560 | CGameSprite::Damage | CGameSprite.cpp | oracle | pending/damage (no body) |
| 0x4A7900 | CGameEffect_opcode_12_2 (ApplyEffect) | CGameEffect.cpp | read-only | exemption (below) |
| 0x5845D0 | IcewindMisc::Roll | IcewindMisc.cpp | oracle | runs: rand/roll |
| 0x447C20 | CDamageReduction::Apply | CDerivedStats.cpp | read-only | exemption (below) |
| 0x447E30 | CDamageReduction::GetEffectiveAmount | CDerivedStats.cpp | read-only | exemption (below) |
| 0x448130 | CDamageReductionList::GetBestGeneric | CDerivedStats.cpp | read-only | exemption (below) |
| 0x4481A0 | CDamageReductionList::GetBest | CDerivedStats.cpp | read-only | exemption (below) |
| 0x448250 | CDamageReductionList::Apply | CDerivedStats.cpp | read-only | exemption (below) |

### Effect application paths — 3

| Address | Function | Src | Kind | Evidence today |
|---|---|---|---|---|
| 0x72DE60 | CGameSprite::sub_72DE60 (ProcessEffectList) | CGameSprite.cpp | oracle | pending/save_rolls (no body) |
| 0x4A3030 | CGameEffect::ResolveEffect | CGameEffect.cpp | read-only | exemption (below) |
| 0x733200 | CGameSprite::HandlePersistantEffects | CGameSprite.cpp | replay | replay compare (first_fight) + tests/test_combat.cpp |

## Conversion targets — the 11 read-only

Each entry records the standing exemption and what an oracle case needs.
Common theme: the original must run against original-constructed engine
objects (ruling R10a fixtures), which none of these build today.

| Address | Function | Exemption (abridged) | An oracle case needs |
|---|---|---|---|
| 0x447C20 | CDamageReduction::Apply | original's std::set trees (shared nil node 0x8D13F4) built by its own ctors (R10a); Catch2 [derived] tests | fixture building the original's DR set via its constructors; compare damage applied |
| 0x447E30 | CDamageReduction::GetEffectiveAmount | same | same fixture |
| 0x448130 | CDamageReductionList::GetBestGeneric | same | same fixture |
| 0x4481A0 | CDamageReductionList::GetBest | same | same fixture |
| 0x448250 | CDamageReductionList::Apply | same | same fixture; composes the four above, so one case family covers all five |
| 0x4A3030 | CGameEffect::ResolveEffect | needs a sprite and the world timer | sprite + world timer fixture; compare duration/expiry |
| 0x4A3310 | CGameEffect_CheckAdd | needs a sprite, the game and the effect's source object | sprite + game + source object; compare add/deny per immunity, probability, SR |
| 0x4A7900 | CGameEffect_opcode_12_2 | needs a CGameSprite and the engine; tests/test_effects.cpp [effects][data] | engine + sprite; compare damage dealt incl. resistance and DR |
| 0x4A9E30 | CGameEffect_opcode_12_6 | virtual calls on an original-constructed sprite, its effect lists and the message handler (R10a); Catch2 [checksave] tests | original-constructed sprite (R10a); compare save-for-half rolls |
| 0x547040 | GetSaveAdjustmentForSourceSprite | calls the sprite's virtual GetAIType (R10a); Catch2 [spell] test | original sprite with derived stats; compare adjustment |
| 0x547620 | CRuleTables::sub_547620 | needs an original sprite with its derived stats; tests/test_effects.cpp | original sprite + derived stats; compare SPIN108 adjustment |

Cheapest first: the five CDerivedStats functions share one fixture and
0x448250 composes the rest; 0x5845D0-style seeding (deterministic srand)
carries over to the save/damage rolls.

## Known debt: 643CC0

`docs/evidence/643CC0.md` covers `CUIControlButtonMapAreaMap::Render` —
the area-map button's rendering (IntersectRect/OffsetRect clipping,
`CVidMosaic::Render`, fog and character blits). It has an empty
`oracle_case` and **no exemption text**: the only doc in the repo with
neither, so the base check fails the moment a diff touches that span.
That is the debt signal working as intended, but it is mislabeled as a
debt candidate for this set — the function is pure UI, not rules math,
and is **not** added to the set. No exemption was written for it this
round (per instructions); whoever next touches the world map should draft
its exemption or give it a case.

## Watchlist — rules math without evidence docs

Labelled in `src/`, look like rules math, but have no evidence doc yet.
Not in the set this round: admitting a doc-less function would make every
touch fail with "no docs/evidence/<ADDR>.md", which is correct but noisy,
and the base check already fails on a touch. Round 2 should write their
docs and cases, then admit them.

| Address | Function | Why it looks like rules math |
|---|---|---|
| 0x73B740 | CGameSprite::GetCriticalHitBonus | crit bonus math feeding the confirm roll |
| 0x48C800 | CGameEffect::DecodeEffect | effect decode incl. save/dice parameters |
| 0x542610 | CRuleTables::FindSavingThrow | save lookup by name |
| 0x5429B0 | CRuleTables::GetSavingThrow | save value assembly |
| 0x542510 | CRuleTables::GetAbilityScoreModifier | the ability modifier table |
| 0x780C80 | CUtil::UtilRandInt | weighted random primitive |
| 0x724540 | CGameSprite::CheckDivineFailure | divine spell failure gate |
| 0x5418E0 | CRuleTables::GetTrapSenseBonus | trap-sense save bonus |
| 0x4C3F60 | CGameEffect::AdjustResistMagic | magic resistance adjustment |

Not yet enumerated: the per-opcode save-vs-effect classes (the opcode
ApplyEffect families with their own save handling).

## Considered and left out

- 0x737EF0 CGameSprite::Attack, 0x738E70 CGameSprite::AttackReevaluate,
  0x739B60 CGameSprite::Swing, 0x74FC50 CGameSprite::GroupAttack —
  orchestration (round timing, action queue, number of swings). The rules
  they call are in the set (Hit, Damage, AC).
- 0x733050 CGameSprite::AddEffect, 0x734550 CGameSprite::HandleEffects —
  effect-list plumbing; no rule of their own.
- Heal / HitPoints / RollHitPoints / lay-on-hands tables, XP tables —
  data tables more than math; candidates for a later round if streams
  start touching them.
- Damage feedback text and combat-log formatting — display.
- Effect constructors and Copy — plumbing.

## Data-quality notes

- **0x78E6E0 has four identities.** `docs/evidence/78E6E0.md` documents
  `CBaldurEngine::StopMusic` (`return 0;`, exemption "returns FALSE"),
  but the same address is labelled on `CRes::OnResourceFreed`
  (src/CRes.cpp), `CRes::GetFixedResourceDataOffset` (src/CRes.cpp) and
  `CGameAnimationTypeMonsterAnkheg::IsFalseColor`
  (src/CGameAnimationTypeMonsterAnkheg.cpp). All are constant-return
  stubs, so the behavior risk is nil, but at most one of the four claims
  can be right. The checker cannot see this (it validates the recorded
  address field, not the name). Flagged for the label/evidence owners;
  not fixed this round.
- Earlier suspicion that the 78E6E0 body actually documented
  CGameEffect::UsesDice was wrong — the doc is internally consistent;
  the conflict is with the src labels only.

## Maintenance

To admit a function: add its address with a one-line what-it-computes
comment to `RULES_MATH` in `tools/check_evidence.py`, add a triage row
here, and give it an evidence doc with a running comparison.
`tests/test_tools.py` keeps the set honest: every address needs an
evidence doc and a row here.
