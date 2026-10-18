---
status: "UNSIGNED — hand-written diagnosis (no oracle draft); scoped reviewer to sign, preferably by re-running the exact case below"
---

# first_fight real-input divergence: a live-sampling diagnosis (2026-10-17)

## Verdict

The four m3-end real-input suite failures of `scenarios/first_fight.txt` are
expectation-side artifacts, not engine divergence. The live checkpoints sample
each pinned state hundreds of AI-updates (au) after the replay frames the
expectations were pinned from, and two of the pinned windows are transient:

1. `dialog_end`'s `world.enemy_creatures[5..8]` block pins a **render-sort
   order the original itself leaves ~21 frames later** (its own periodic dumps
   show the re-sort). Every live run samples past the transient and shows the
   original's own next order.
2. `spell`/`fight_done`'s `world.party[0].hp`/`.morale` pin **mid-combat
   beats**: a goblin counterattack that landed at one specific au in replay,
   and a kill-triggered morale spike sampled 1 dump before it finished
   decaying. Which beats have landed at a checkpoint is input-timing-chaotic;
   our own engine's four live runs disagree with each other.

No engine change is proposed. The engine-side suspects (dialog
StartDialog/StopTime same-frame tick ordering) are exonerated below.

## The failure signature

Real-input suite runs of `scenarios/first_fight.txt`, all of them **our
engine** (the original has no live-input mode; it is only ever driven by the
fixed-frame replay recorder):

| run | build | mismatched fields (excl. the `party_count` dump artifact) |
|---|---|---|
| `~/iwd2r-runs/m3-end-suite-linux-20261017-184619` | linux-gcc | 16 |
| `~/iwd2r-runs/m3-end-suite-linux2-20261017-185926` | linux-gcc | 66 |
| `~/iwd2r-runs/m3-end-suite-linux3-20261017-190902` | linux-gcc | 17 |
| `~/iwd2r-runs/m3-end-suite-windows-20261017-191841` | MSVC (Windows) | 20 |

Every mismatching field falls into exactly two classes (full itemization
recomputed read-only from the folders above):

- **Class A — index-in-sort rotations.** All `world.enemy_creatures[i]`
  mismatches. Order-insensitively, the enemy multisets
  `(hp, max_hp, equiped_effects, timed_effects)` are **equal to the pins at
  every checkpoint in every run**, except one creature at `melee` (below).
  The counts (35), every `max_hp`, every `equiped_effects` and every
  `timed_effects` match everywhere.
- **Class B — fight-beat transients.** `world.party[0].hp` at
  melee/spell/fight_done, `world.party[0].morale` at spell, and (linux2 only)
  `game.members[0].xp` at melee.

The one order-insensitive Class-A residual: at `melee` the pin holds a
`(hp=1, max_hp=4)` goblin; live holds it still at `(4, 4)` (linux2: already
dead). That is wandering AI-vs-AI combat landing at a shifted au — by
`spell` the multiset is equal again in all four runs.

## Chain step 1: live pays real time; replay does not

Replay pins the whole action stream to the virtual 30 fps clock
(`platform/replay_clock.h`): real startup/load/driver latency costs no au.
Live's AI thread ticks at 30 Hz real time, so the per-action driver glue
(xdotool process spawns, control round-trips, 0.1 s settle sleeps), the
startup and the world load all count as game time. The skew
(live au − replay au at the same checkpoint) is monotone and near-identical
in all four runs, on both OSes:

| checkpoint | replay au | C1 | linux2 | linux3 | C2-win |
|---|---:|---:|---:|---:|---:|
| menu | 61 | +45 | +39 | +45 | +53 |
| world | 1943 | +355 | +348 | +355 | +374 |
| moved | 2808 | +429 | +425 | +430 | +442 |
| dialog_end | 3039 | +472 | +470 | +450 | +467 |
| reig_dialog | 3564 | +516 | +514 | +513 | +522 |
| goblin_sighted | 5139 | +682 | +682 | +660 | +682 |
| engaged | 5223 | +695 | +696 | +698 | +697 |
| melee | 5354 | +718 | +717 | +720 | +726 |
| spell | 5579 | +772 | +769 | +751 | +760 |
| fight_done | 5729 | +783 | +780 | +762 | +771 |

(~95 ms of glue per action; ~783 au by fight_done. Live frame counts track
au 1:1, e.g. 6512/6512 in C1.)

## Chain step 2: the dialog_end enemy[5] pin is a transient the original itself leaves

The original's replay periodic dumps
(`~/iwd2r-runs/20261017-174428/original/frame-*.json`):

- frame **3030** (pin moment 3039): `..., 1114129, 851981, 1048592, 983055, 917518, ...`
  — the pin: enemy[5] = 851981 (hp 4, max 4, eq 0).
- frame **3060**: `..., 1114129, 1048592, 983055, 851981, 917518, ...`
  — the original's own re-sort three dumps later: 1048592 (hp 37, eq 6) is now
  at index 5, 983055 at 6, 851981 at 7.

All four live runs sample dialog_end at au 3489–3511 (+450..472) and show
**exactly the 3060 order**: enemy[5] = 1048592 (hp 37, eq 6). Order
insensitively the state is identical to the pin (multisets equal). A pin
taken 21 frames later — or a live sample 21 frames earlier — would agree.
This is a sampling-phase artifact, in both directions.

The same rotation explains every other enemy mismatch (e.g. `melee`
enemy[4].hp 1 vs 4 in C1/linux3/win; the large linux2 `melee` block is
adjacent transpositions of the same wandering train, sampled mid-sort because
linux2's dump landed ~30 au earlier relative to its fight — its morale 15 at
melee catches the kill bump at its peak).

## Chain step 3: spell/fight_done party pins are chaotic combat beats

The original's periodic dumps around the fight:

| frame | Probe hp | morale | goblin 1179666 hp |
|---:|---:|---:|---:|
| 5280 | 4 | 10 | 4 |
| 5310 | 4 | 10 | 1 (Probe's hit) |
| 5340 | 3 | 10 | 1 (the goblin's counterattack) |
| 5460 | 3 | **15** | 0 (the kill: missile) |
| 5490→5610 | 3 | 14, 13, 12, 11, **10** | 0 |

The pins: `spell` (frame 5579) hp 3, morale **11** — one 30-frame dump before
the morale returns to its baseline 10 at 5610. `fight_done` (5729) hp 3,
morale 10.

Which of these beats have landed at a live checkpoint depends on when the
attack/kill clicks landed relative to the AI clock — and our own engine's
four live runs split:

| run | Probe hp at fight_done | morale at spell | the goblin |
|---|---|---|---|
| C1-linux | 4 (never countered) | 10 (bump decayed) | killed by missile |
| linux2 | 4 | 10 (15 caught at melee) | killed by missile |
| linux3 | **3** (counter landed) | 10 | killed by missile |
| C2-win | 4 | 10 | killed by missile |

Same engine, same scenario, same input script: three distinct interleavings
and both outcomes of the counterattack. A live run cannot reproduce the
replay's exact beats, so pinning `party[0].hp` 3 / morale 11 for live fails
for **any** faithful engine.

## Chain step 4: engine equivalence holds

- Replay is bit-for-bit at this head (ddda21ab): `scenarios/first_fight.txt`
  replay MATCH at 39/39 checkpoints and 190/190 periodic dumps
  (`~/iwd2r-runs/post-effects-a5-20261017-200003`).
- Live threading invariants are clean: `ai_stalled_frames` =
  `ai_overlap_frames` = 0 at every checkpoint of all four live runs.
- Both OS builds agree with each other on all deterministic parts (the
  mismatching fields are the same two classes; linux2-vs-others differ only
  in which side of a beat their dump landed).
- The live-vs-live disagreements above are between runs of **our** engine:
  the noise is in the harness timing, not in engine state.

## Why the engine-side suspects are exonerated

The prime suspects — dialog StartDialog/StopTime same-frame tick ordering,
and in-frame dialog-initiation position — would produce engine-state
differences. The evidence rules them out:

- dialog_end state is **multiset-identical** to the pin in all four live
  runs; only list positions differ, and those positions are the original's
  own state 21 frames later (step 2). A tick-ordering defect cannot relocate
  creatures in a render sort while preserving every hp/effect value.
- The party hp/morale differences (step 3) flip **between runs of the same
  binary** (linux3 vs C1 on the same linux-gcc build). No deterministic
  engine defect flips with harness timing.
- If StopTime/UpdateTime ordering were wrong, replay would diverge too — it
  matches bit-for-bit at 190/190 periodic dumps, which sample every 30 frames
  through the same dialog.

## Exact runnable case

Replay oracle (should be, and is, green):

```
cd /home/thier/iwd2-re-worktrees/p3-vis-probe
python3 -I tools/play.py --vm windows scenarios/first_fight.txt   # replay
# last green run at ddda21ab: ~/iwd2r-runs/post-effects-a5-20261017-200003
# (MATCH 39/39 + 190/190)
```

Live (fails in the two pinned windows):

```
python3 -I tools/play.py --vm linux --suite --jobs 3    # or --vm windows
# suite folder: ~/iwd2r-runs/m3-end-suite-linux-20261017-184619/suite.txt
```

All numbers above are recomputed from the run folders by read-only scripts
against `scenarios/first_fight.expect.json` (expectations were generated from
the original's replay dumps only: `~/iwd2r-runs/20261017-174428/original`).

## Remedy (a validation-design decision, not an engine fix)

The live expectation field set pins two things its own design rule excludes
(`tools/realinput.py`: expectations name "the fields input decides … not
those that depend on real time"): index-bound positions in a list the engine
re-sorts every frame, and transient mid-combat values. Options, for the
coordinator:

1. **Order-insensitive enemy pins for live**: compare enemy creatures as an
   id-keyed map instead of an index-bound list (the live dumps already carry
   `id`; the expect format does not pin it). This alone clears every Class-A
   mismatch.
2. **Settled-state party pins**: drop `party[0].hp` at melee/spell (chaotic)
   and morale at spell (mid-decay); pin morale at fight_done+settle (baseline
   10 is reached 150 au after the kill), or add a settle `wait` before the
   fight_done checkpoint. An exact Probe hp can never be pinned under live
   timing — either drop it or pin only "alive and undamaged-beyond-X".
3. **Keep the replay path as the engine-equivalence oracle** (bit-for-bit,
   39/39 + 190/190): the live path validates the real-input pipeline (OS
   input, windowing, timing), which the current field set conflates with
   engine state.

Recommended: 1 + 2 for the live path, 3 unchanged. No engine change; the
Milestone 3 verdict should record the four suite failures as expectation
artifacts with this document as evidence.
