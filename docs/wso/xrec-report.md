# xrec report — first cross-recovery comparison (2026-10-17)

Run from the repo root:

    python3 -I tools/xrec.py 4fa36bcd wso/main HEAD --json docs/wso/disagreements.json

201 shared src/*.cpp files (14 theirs-only, 20 ours-only); 8,661 functions
classified.

| category | functions |
|---|---|
| same | 4,424 |
| same_stub | 2,005 |
| only_theirs | 413 |
| only_ours | 277 |
| theirs_real_ours_stub | 33 |
| ours_real_theirs_stub | 88 |
| theirs_changed_ours_base | 592 |
| ours_changed_theirs_base | 287 |
| both_changed_differ | 529 |
| both_stub_diff | 13 |

Theirs-only files (whole-file imports, wave 5; 159 real functions across
them): AutoLoad.cpp, AutoUI.cpp, CGameFireball3d.cpp, CGameTemporal.cpp,
CMessageDoorOpen.cpp, CProjectileSPBOOM.cpp, CScreenMultiPlayer.cpp,
CSparkleCluster.cpp, CVisualEffect.cpp, DebugLog.cpp,
IcewindCGameAnimationTypeEffect.cpp, IcewindCProjectileTargetMap.cpp,
MfcCString.cpp, TString.cpp.

Against the pre-task script (the spec's table): matching by `// 0xADDR`
before name collapses 682 one-sided rows (746/609 into 413/277) into 356
pairs — the trees' name drift hid real disagreements. both_changed_differ
grows 406 to 529, theirs_changed_ours_base 528 to 592, identical real bodies
4,337 to 4,424.

The rider 22960d3b (base looked up by address in the name loop too) changes
nothing in the recorded run: against base 4fa36bcd, rerun with the pre-rider
tool at ten ours refs (HEAD, the base itself, and eight branch and side
refs), the JSON is byte-identical. Base
stamps 0x4B0050 on two functions (`CGameEffectResistElectricity` and
`CGameEffectResistFire::ApplyEffect`) and 0x5ED130 on two
(`...CharacterScriptScripts` and `...CharacterExportCharacters::OnItemSelected`),
so neither address is a unique key and the lookup falls back to the by-name
one the rider replaced; base's single 0x44DC10 stamp is the `ExecuteAction`
stub, which puts `CGameAIBase::StartCutScene` and `IncrementChapter` in
`both_changed_differ` under either lookup (base has neither function by
name). The same four rows do move `theirs_changed_ours_base` →
`both_changed_differ` if the comparison is run against base 8ce297d7
(iwd2r/main): there 0x4B0050 and 0x5ED130 carry a single stamp each, the
address lookup engages and returns bodies matching neither tree
(`CGameEffectResistFire::ApplyEffect`, `CGameAIBase::StartCutScene`,
`CGameAIBase::IncrementChapter`,
`CUIControlTextDisplayCharacterScriptScripts::OnItemSelected`). That base is
this branch's own src state — against it no row is
`ours_changed_theirs_base` or `both_changed_differ` at all — so it is not
the comparison recorded here; the table keeps base 4fa36bcd, the base this
report documents.

## Spot check (20 rows, mandated by the spec)

Method: an independent script (kept in /tmp, not the repo) extracted each
sampled function from `git show` of both trees by its `// 0xADDR` comment
line — the tool itself finds functions by signature regex — brace-matched
with string literals skipped, stripped comments with a separate state
machine, reclassified with the ten rules, and compared against the row.
Result: 20/20 agree, every body length reproduced exactly. The check also
caught the address match doing its job on a real rename:
`GetSpellAbilityValue` (theirs) is `GetSaveAdjustmentForSourceSprite` (ours)
at 0x547040, paired as both_changed_differ instead of two one-sided rows.
The largest row, `CUIControlBase::CreateControl` (95,790 / 90,825 chars), is
a real ~3,300-line function with ~2,000 case labels in both trees, not a
brace-matching runaway.

The check caught one misclassification. In src/CScreenMap.cpp their tree
stamps `// 0x643CC0` on two functions — `Render` (real) and `RenderFogOfWar`
(TODO stub) — ours only on `Render`; the tool paired their stub with our
real body under `Render`'s name. Such duplication is common: 120
(file, address) pairs are stamped on more than one function of a file in at
least one tree (placeholder stamps: 0x49FC40 on small getters across the
screen files, 0x40A0E0 on 104 CMessage functions). Fix: an address two
functions of one tree carry is not used as a key; those functions match by
name. Regression test `test_xrec_ambiguous_address_pairs_by_name`; the
affected row now reads `Render` same 1452/1452 and `RenderFogOfWar`
ours_real_theirs_stub 2/2710. Counts above and the 20/20 result are from the
fixed tool.

## JSON

docs/wso/disagreements.json: one row per function,
`{"file", "name", "address", "category", "their_len", "our_len"}`, 8,661
rows, sorted by category then total body length descending (file, name
tiebreak); 7,847 rows carry an address. Category counts match the table
above; the sort order was verified programmatically.

## Scorecard

Per-side tally across every batch in docs/wso/wave-state.json, split by deciding evidence kind (regenerate with `python3 tools/wave.py scorecard`):

Decided for the fork: 10 (losing kind: oracle 1, replay 1, read-only 8) -- deciding evidence: dump reading 10
Decided for ours: 182 (losing kind: trace 182) -- deciding evidence: replay checkpoint 21, dump reading 161
Decided for neither: 8 -- deciding evidence: dump reading 8
Deciding evidence across all 200 verdicts: oracle probe 0, replay checkpoint 21, dump reading 179, trace 0

Batches: b001 closed (20 rows), b002 closed (20 rows), b003 closed (20 rows), b004 closed (20 rows), b005 closed (20 rows), b006 closed (20 rows), b007 closed (20 rows), b008 closed (20 rows), b009 closed (20 rows), b010 closed (20 rows), b011 assigned (20 rows), b012 assigned (20 rows)
