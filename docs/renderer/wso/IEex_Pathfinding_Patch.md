# IEex_Pathfinding_Patch.lua — GemRB-inspired pathfinding: constants pack + DLL policy seams

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Pathfinding_Patch.lua`.
Pathfinding behaviour upgrades in two registers: a phase-1 "constants
pack" of byte-verified in-place patches (retry counts, throttles, two
branch NOPs), and detour seams that route individual engine DECISIONS
through DLL-side policy functions which re-implement the decision and
return the exact vanilla answer whenever the feature is off. Master gate
`[IEex Options] "Improved Pathfinding"` (Icewind2.ini, default 1)
boot-gates only the raw byte patches; the detour seams install
UNCONDITIONALLY (gated only on the helper export existing) and self-gate
at runtime, which keeps the DLL's "Path Log" walk-timing instrumentation
usable in vanilla mode for A/B travel-time runs and makes the in-game
toggle live in both directions.

## Intent

1. **Never blind-poke.** Every byte patch goes through a local
   `IEex_PF_VerifyBytes` (expected-byte compare via `IEex_ReadByte`,
   skip + log on mismatch — exe drift / foreign-mod protection), and the
   boot line prints `[IEex_Pathfinding] loaded (%d/%d sites patched)`.
2. **Give-up retries 4 → 8** (`CGameSprite::MoveToPoint`): the
   `m_curAction.m_specificID2 > N → ACTION_ERROR` counter — GemRB uses
   MAX_PATH_TRIES = 8. Patched as an instruction (`mov ecx,8`), never as
   a data write, because the .rdata word it reads (0x85BBA6, vanilla 4)
   is SHARED with `CGameSprite::Turn` (reader 0x75B15B).
3. **Pursuit re-path throttle, tunable** (`CGameSprite::MoveToObject`):
   the "new path every eighth call" counter (`.rdata` word 0x85BBA4,
   vanilla 8) becomes `mov ecx,N` with N = `[IEex Options] "IP Pursuit
   Repath"` clamped to 2..16; 8 means the vanilla bytes stay. The file's
   STUTTER WARNING is the design centrepiece: SetTarget on the re-search
   THROWS the current path away (`delete m_pPath`) and the sprite stands
   ~2 AI ticks per re-path, so this throttle IS the chase duty cycle —
   at 8 it walks ~6 and stands ~2 (the classic IE pursuit shuffle);
   lowering it doubles the stops AND the search-thread load for everyone.
4. **Collision re-searches keep path smoothing** (off by default): the
   `jne` at 0x549480 in `SearchThreadMain` forces `m_pathSmooth` FALSE
   for collision re-searches (jagged panic paths after every bump); NOP
   it only for `[IEex Options] "IP Collision Smoothing"`, because every
   smoothed collision search costs the SINGLE shared search thread an
   extra LOS pass.
5. **Idle non-hostile NPCs become shovable** (`[IEex Options] "IP Bump
   Idle NPCs"`, default 1): the engine's CanAnimate whitelist
   ({RANDOMWALK, RANDOMWALKCONTINUOUS, WAIT, FACE} + ranged ATTACK*) does
   not contain NO_ACTION, so an idle neutral stamps the search map as
   non-bumpable (0x70 — a hard A* wall that also aborts ClearBumpPath's
   crowd gate) and the party cannot get past "the cat in the doorway".
   NOPping the `ja 0x6FB4E0` (0x6FB4C5) gives idle non-hostiles the same
   deal as idle PCs (EA ≤ cutoff skips the whitelist). Enemies (rejected
   earlier at 0x6FB4A1) and immobile creatures stay non-bumpable.
6. **Collision stand-still backoff halved**: both SetTarget overloads'
   `mov ecx,0xF` (rand()%15 dead time before the re-search) become 8,
   keeping the jitter de-sync purpose with half the dead time (GemRB's
   backoff is RAND(8,16) on a denser tick).
7. **Pursuit keep-path (phase 7)**: `MoveToObject`'s single SetTarget
   call (0x73F519) is retargeted to `IEex_Helper_PF_PursuitSetTarget`,
   which keeps the path across the re-search (same request, same FRONT
   list, same priority); `PF_ArrivalPolicy` swaps the fresh route in
   mid-stride when it lands — the sprite never stops chasing.
   Self-gating: with the toggle off it forwards bit-for-bit to the
   engine SetTarget.
8. **AIUpdateWalk decision seams (phases 2+3)**: two caves inside
   `CGameSprite::AIUpdateWalk` turn the waypoint-arrival test (0x6F9651:
   route 0 = continue, 1 = arrival, 2 = return) and the next-cell
   mobile-cost check (0x6F9CF7: route 0 = engine ClearBumpPath+revert,
   1 = cell usable, 2 = policy-handled visibility/exit, 3 = revert
   without engine ClearBumpPath) into calls to `PF_ArrivalPolicy` /
   `PF_CollisionPolicy`. Policies return the exact vanilla decision when
   "IP Behavior Flags" = 0 or in multiplayer; the rest of the 0x6F9040
   function stays engine bytes. (The file header's "phase 2+: whole-
   function port of AIUpdateWalk installed below" is stale — no
   whole-function port exists in this drop; only these two seams.)
9. **Enemy unstick (phase 4)**: inside `CGameSprite::ClearBumpPath`, the
   gate seam (0x6FA929) lets EA > 30 sprites bump too
   (`[IEex Options] "IP Enemy Bumping"`, DLL-side), while the obstacle
   seam (0x6FACBB, `PF_BumpObstaclePolicy`) forbids shoving EA ≤ 30
   obstacles — enemies displace their own side only, never PCs.
10. **Combat slide (phase 6, EXPERIMENTAL, default off)**:
    `PF_BumpSlidePolicy` lets a same-side ally mid-melee-attack slide
    onto a cell rejected only for personal-space ring overlap
    (re-validating statics via GetLOSCost and refusing any cell that is
    the CENTER of a nearby live sprite); `PF_CanAnimatePolicy` keeps
    party melee bumpable during ATTACK actions (vanilla keeps only
    RANGED attackers, so the front fighter was ungatherable); the two
    ClearBumpPath JumpToPoint call sites AND AIUpdate's bump-return are
    retargeted to `PF_SlideJump` which GLIDES eligible short party hops
    over a few AI ticks (advanced by `PF_GlideTick` from a seam at the
    top of `CBaldurChitin::AsynchronousUpdate`) instead of teleporting;
    and the crowd gate (0x6FAB7B, `PF_BumpCrowdPolicy`) no longer aborts
    melee shoves just because the enemy being fought adds 8 to the
    dynByte>>1 > 7 count — the vanilla store to [esp+0x13] is kept.
11. **Search-side soft-block (phase 4b, default off)**: `jbe → jmp` at
    0x54961E keeps m_bBump for EA > 30 search requests (enemies soft-cost
    through bumpable allies instead of hard-blocking); the file itself
    warns enemies may then path through the party line and grind, since
    phase 4 refuses to shove PCs.
12. **Directed destination adjust (phase 5)**: whole-replace of
    `CGameArea::SnapshotAdjustTarget` (worker thread, ret 0x14) — exact
    port of the vanilla line-probe, then an expanding ring search
    (r = 1..6) around the goal picking the nearest passable snapshot
    cell, tie-broken toward the approach side ("stop just before" an
    occupied destination). Gated by `[IEex Options] "IP Directed Adjust"`
    (default 1) inside the DLL; vanilla-exact when off.

## Hooks table

"verify" = `IEex_PF_VerifyBytes` expected bytes; every site is skipped
and logged on mismatch. Seam rows install when the named
`IEex_Helper_PF_*` export exists (`IEex_LabelDefault` probe) regardless
of the master toggle.

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x73F706 | inside `CGameSprite::MoveToPoint` (entry 0x73F560) | src/CGameSprite.cpp:20291 (marker at 20290; hook site itself unresolved in our tree) | — | master-gated: verify `0F BF 0D A6 BB 85 00` (movsx ecx,[0x85BBA6]) → `B9 08 00 00 00 90 90` (mov ecx,8; nops). Give-up retries 4 → 8. |
| 0x73F321 | inside `CGameSprite::MoveToObject` (entry 0x73EDD0) | src/CGameSprite.cpp:20137 (marker at 20136; hook site itself unresolved in our tree) | — | master-gated AND "IP Pursuit Repath" ≠ 8: verify `0F BF 0D A4 BB 85 00` (movsx ecx,[0x85BBA4]) → `B9 NN 00 00 00 90 90`. Repath throttle 8 → N (clamped 2..16). The 2nd throttle arm (idiv @0x73F40B) reuses ecx — single site suffices. |
| 0x549480 | inside `SearchThreadMain` (entry 0x5492E0) | src/CSearchBitmap.cpp:829 (marker at 828; hook site itself unresolved in our tree) | — | master-gated AND "IP Collision Smoothing" (default 0): verify `75 19` (jne) → `90 90`. Collision re-searches keep path smoothing (the request carries GetPathSmooth(); the jne forced m_pathSmooth=FALSE for them). |
| 0x6FB4C5 | inside `CGameSprite::sub_6FB440` (fork name "CanAnimate"; entry 0x6FB440 — sub_6FB440 is our tree's unresolved-name placeholder) | src/CGameSprite.cpp:3383 (marker at 3382; hook site itself unresolved in our tree) | — | master-gated AND "IP Bump Idle NPCs" (default 1): verify `77 19` (ja 0x6FB4E0 — the EA > EA_GOODCUTOFF whitelist branch) → `90 90`. Idle non-hostiles become bumpable. |
| 0x707B05 | inside `CGameSprite::SetTarget(const CPoint&, BOOL)` (entry 0x707980) | src/CGameSprite.cpp:6547 (marker at 6546; hook site itself unresolved in our tree) | — | master-gated: verify @0x707B04 `B9 0F 00 00 00` → write 0x707B05 = `08`. rand()%15 → rand()%8 stand-still delay. |
| 0x707E2A | inside `CGameSprite::SetTarget(CSearchRequest*, BOOL, BYTE)` (entry 0x707D40) | src/CGameSprite.cpp:6604 (exact marker at 6603; decl follows; hook site itself unresolved in our tree) | — | master-gated: verify @0x707E29 `B9 0F 00 00 00` → write 0x707E2A = `08`. Same backoff, other overload. |
| 0x73F519 | inside `CGameSprite::MoveToObject` — the SetTarget call site | src/CGameSprite.cpp:20137 (marker at 20136; hook site itself unresolved in our tree) | `IEex_Helper_PF_PursuitSetTarget` | export-gated: verify `E8 22 88 FC FF` → `!call >IEex_Helper_PF_PursuitSetTarget`. thiscall == fastcall with dummy edx (ecx = this, 3 stack dwords, callee cleans), so the export replaces the call target outright. Phase 7. |
| 0x6F9651 | inside `CGameSprite::AIUpdateWalk` (entry 0x6F9040) — seam 1, waypoint-arrival test | src/CGameSprite.cpp:2730 (marker at 2729; hook site itself unresolved in our tree) | `IEex_Helper_PF_ArrivalPolicy` | export-gated: verify `3B CF 0F 8C 5A 04 00 00` (cmp ecx,edi; jl 0x6F9AB3) → cave (push ecx/edx/ebx/ebp/esi/edi; push edi/ecx/esi; call policy; pops; cmp eax,1 → 0x6F9659, cmp eax,2 → 0x6F9AB3, else SEH slot [esp+0x60] = -1 and jmp 0x6FA7E7) via `!jmp_dword` + 3 nops. ebx is LIVE into both targets (0x6F972C / 0x6F9B48 read [ebx]) — cave keeps it. Phases 2+3. |
| 0x6F9CF7 | inside `CGameSprite::AIUpdateWalk` — seam 2, next-cell mobile-cost check | src/CGameSprite.cpp:2730 (marker at 2729; hook site itself unresolved in our tree) | `IEex_Helper_PF_CollisionPolicy` | export-gated (same probe as ArrivalPolicy): verify `3A 05 A3 D6 84 00 0F 85 1F 02 00 00` (cmp al,[0x84D6A3] COST_IMPASSABLE; jne 0x6F9F22) → cave (movzx eax,al; push eax/esi; call policy; routes: 1 → 0x6F9F22, 2 → 0x6FA665, 3 → 0x6F9D1C, else 0x6F9D03) via `!jmp_dword` + 7 nops. |
| 0x6FA929 | inside `CGameSprite::ClearBumpPath(const CPoint&, const CPoint&)` (entry 0x6FA900) — bump gate seam | src/CGameSprite.cpp:3140 (marker at 3139; hook site itself unresolved in our tree) | `IEex_Helper_PF_BumpGatePolicy` | export-gated: verify `3A 05 3B 7C 84 00 76 07` (cmp al,[0x847C3B] EA_GOODCUTOFF; jbe 0x6FA938) → cave (movzx/push eax; push ebx; call policy; test eax,eax; jne 0x6FA938; else xor eax,eax; jmp 0x6FB417) via `!jmp_dword` + 3 nops. ebx = bumper(this), esi = obstacle; both callee-saved by the stdcall policies; eax/ecx/edx dead at all four targets. Phase 4. |
| 0x6FACBB | inside `CGameSprite::ClearBumpPath` — obstacle skip seam | src/CGameSprite.cpp:3140 (marker at 3139; hook site itself unresolved in our tree) | `IEex_Helper_PF_BumpObstaclePolicy` | export-gated (BumpGatePolicy probe): verify the 28 bytes `8B 86 A8 54 00 00 85 C0 0F 84 C4 06 00 00` (mov eax,[esi+0x54A8]; test; je 0x6FB38D) + `8B 86 B8 54 00 00 85 C0 0F 85 B6 06 00 00` (mov eax,[esi+0x54B8]; test; jne 0x6FACD7) → cave (push esi/ebx; call policy; je 0x6FB38D; else jmp 0x6FACD7) via `!jmp_dword` + 23 nops. Replaces the m_bumpable/field_54B8 skip pair. Phase 4. |
| 0x6FAFF5 | inside `CGameSprite::ClearBumpPath` — shove-destination candidate validation | src/CGameSprite.cpp:3140 (marker at 3139; hook site itself unresolved in our tree) | `IEex_Helper_PF_BumpSlidePolicy` | export-gated: verify `3A 05 A3 D6 84 00 75 17` (cmp al,[COST_IMPASSABLE]; jne 0x6FB014) → cave keeps the vanilla compare + jne, then (policy consulted only for vanilla-REJECTED candidates): pushes regs, reads candidate cell [esp+0x64]/[esp+0x64], pObstacle [esp+0x34], push ebx; call policy; accept → 0x6FB014 (AddObject + JumpToPoint), reject → 0x6FAFFD (next candidate) via `!jmp_dword` + 3 nops. ebx = bumper; esi/edi = jump destination preserved. Phase 6, "IP Combat Slide" (default 0) is the DLL-side gate. |
| 0x6FB5D7 | inside `CGameSprite::sub_6FB440` ("CanAnimate") — weapon ability-type branch | src/CGameSprite.cpp:3383 (marker at 3382; hook site itself unresolved in our tree) | `IEex_Helper_PF_CanAnimatePolicy` | export-gated (BumpSlidePolicy probe): verify `66 83 FB 04 74 2B 66 83 FB 02 74 25` (cmp bx,4; je 0x6FB608; cmp bx,2; je 0x6FB608) → cave keeps both compares, then pushes; movzx eax,bx; push eax; push esi; call policy; jne 0x6FB608 else jmp 0x6FB5E3 via `!jmp_dword` + 7 nops. Party melee uses the attack whitelist too (combat slide gated). Phase 6. |
| 0x6FB0A8, 0x6FB22B | inside `CGameSprite::ClearBumpPath` — the two JumpToPoint call sites (loop 1 / recursive shove; E8 rel32 → 0x745950) | src/CGameSprite.cpp:3140 (marker at 3139; hook sites themselves unresolved in our tree) | `IEex_Helper_PF_SlideJump` | export-gated (nested `IEex_LabelDefault("IEex_Helper_PF_SlideJump")` probe inside the BumpSlide block): verify `E8 A3 A8 04 00` / `E8 20 A7 04 00` → `!call >IEex_Helper_PF_SlideJump`. Glide instead of teleport; the shim falls through to the real JumpToPoint otherwise. Phase 6. |
| 0x6F7207 | inside `CGameSprite::AIUpdate` (entry 0x6F5FF0) — bump-return (the m_bBumped block that jumps the sprite back to m_ptBumpedFrom once the cell frees up) | src/CGameSprite.cpp:1684 (marker at 1683; hook site itself unresolved in our tree) | `IEex_Helper_PF_SlideJump` | same nested export gate: verify `E8 44 E7 04 00` → `!call >IEex_Helper_PF_SlideJump`. "The last combat teleport left." Phase 6. |
| 0x4237A0 | `CBaldurChitin::AsynchronousUpdate(UINT, UINT, DWORD, DWORD, DWORD)` | src/CBaldurChitin.cpp:859 (exact marker; decl follows) | `IEex_Helper_PF_GlideTick` | same nested export gate: verify `56 8B F1 8A 86 E0 00 00 00` (push esi; mov esi,ecx; mov al,[esi+0xE0]) → cave (push eax/ecx/edx; call GlideTick; pops; re-run the 9 bytes; `!jmp_dword :4237A9`) via `!jmp_dword` + 4 nops. Advances the glides once per async tick. Phase 6. |
| 0x6FAB7B | inside `CGameSprite::ClearBumpPath` — crowd gate ("count = dynByte>>1; if (count > 7) return FALSE") | src/CGameSprite.cpp:3140 (marker at 3139; hook site itself unresolved in our tree) | `IEex_Helper_PF_BumpCrowdPolicy` | export-gated (BumpSlidePolicy probe): verify `D0 E8 3C 07 88 44 24 13 0F 87 EB FE FF FF` (shr al,1; cmp al,7; mov [esp+0x13],al; ja 0x6FAA74) → cave keeps the vanilla store to [esp+0x13] (the raw count still feeds the end-of-function checks) and overrides only the abort: pushes; movzx/push eax; push ebx; call policy; jne 0x6FAB89 else jmp 0x6FAA74 via `!jmp_dword` + 9 nops. Any adjacent NON-bumpable stamp (the enemy being fought) adds 8 — melee shoves aborted here before ever reaching the destination loop. Phase 6. |
| 0x54961E | inside `SearchThreadMain` (entry 0x5492E0) | src/CSearchBitmap.cpp:829 (marker at 828; hook site itself unresolved in our tree) | — | master-gated AND "IP Enemy Soft Block" (default 0): verify `76 0F` (jbe) → `EB` (jmp). Skips the bBump=FALSE forcing for EA > 30 requests. Phase 4b. |
| 0x46A630 | `CGameArea::SnapshotAdjustTarget(CPoint, POINT*, BOOL, SHORT)` | src/CGameArea.cpp:267 (exact marker; decl follows) | `IEex_Helper_PF_SnapshotAdjustTarget` | export-gated: verify `83 EC 14 8B 44 24 20` → `!jmp_dword >IEex_Helper_PF_SnapshotAdjustTarget !nop !nop`. Whole-function replace (worker thread, ret 0x14); the DLL runs the vanilla line-probe port plus the ring search. Phase 5. |

Cited but not hooked — routing targets quoted by the caves (all sites
themselves unresolved in our tree): 0x6F9659 / 0x6F9AB3 / 0x6F972C /
0x6F9B48 / 0x6FA7DF / 0x6FA7E7 / 0x6F9F22 / 0x6FA665 / 0x6F9D03 /
0x6F9D1C (inside `AIUpdateWalk`), 0x6FA938 / 0x6FB417 / 0x6FB38D /
0x6FACD7 / 0x6FB014 / 0x6FAFFD / 0x6FAB89 / 0x6FAA74 (inside
`ClearBumpPath`), 0x6FB608 / 0x6FB5E3 / 0x6FB4A1 / 0x6FB4BC–0x6FB4E0
(inside `sub_6FB440`), 0x4237A9 (inside `AsynchronousUpdate`),
0x73F40B (MoveToObject's second throttle arm), 0x75B15B (the Turn-side
reader of the shared .rdata word — `CGameSprite::Turn` is entry
0x75A3F0, marker at src/CGameSprite.cpp:23745, decl 23746). Real jump
target `CGameSprite::JumpToPoint(CPoint, BOOL)` = 0x745950,
src/CGameSprite.cpp:21355 (exact marker at 21354). Data words:
0x85BBA4 (repath throttle, vanilla 8 — corroborated by our tree's own
comments "The path search counter (the WORD at 0x85BBA4 is 8)" at
src/CGameSprite.cpp:16107 inside `SetCurrAction` (entry 0x7338E0,
marker at 15921, decl 15922) and "A new path every eighth call (the
WORD at 0x85BBA4)" at src/CGameSprite.cpp:20254 inside MoveToObject);
0x85BBA6 (give-up retries, vanilla 4 — no marker in our tree; the fork
insists on patching the instruction because this word is SHARED with
Turn); 0x847C3B = `CAIObjectType::EA_GOODCUTOFF` (= 30, marker
src/CAIObjectType.cpp:32 decl 33, value define src/CAIObjectType.h:89);
0x847784 = `CAIAction::NO_ACTION` (marker src/CAIAction.cpp:6 decl 7);
0x84D6A3 = `CPathSearch::COST_IMPASSABLE` (= 255, marker
src/CPathSearch.cpp:23 decl 24). Creature fields named by the seam
comments: +0x54A8 `field_54A8` (fork "m_bumpable" — unnamed in our
tree, src/CGameSprite.h:909; set from CanAnimate() each AI tick), +0x54B8
`field_54B8` (src/CGameSprite.h:912), +0x54AC/+0x54B0
`m_bBumped`/`m_ptBumpedFrom` (src/CGameSprite.h:910-911), the search
request's +0x24 `m_pathSmooth` (src/CSearchBitmap.h:89),
`m_curAction.m_specificID2` (src/CAIAction.h:88). `GetMoveScale` and
the EA whitelist internals are fork-comment prose only (the function
bodies are stubs in our tree — sites unresolved).

## Data flow

No Lua-side runtime state: the IIFE's only state is the boot counter
pair `IEex_PF_Patched` / `IEex_PF_Total` for the load log. No bridges,
no per-frame Lua, no engine memory reads after boot. Everything runtime
lives in the DLL policies:

- The DLL re-reads `[IEex Options] "Improved Pathfinding"` at runtime
  and returns exact vanilla decisions when 0 (or in multiplayer); the
  detours stay installed so "Path Log" instrumentation works in vanilla
  mode and the toggle is live both ways. Other DLL-side keys named by
  the seam comments: "IP Behavior Flags", "IP Enemy Bumping",
  "IP Combat Slide", "IP Directed Adjust".
- Lua-side ini keys (boot only): the master gate, "IP Pursuit Repath"
  (clamped 2..16, 8 = leave vanilla bytes), "IP Collision Smoothing"
  (default 0), "IP Bump Idle NPCs" (default 1), "IP Enemy Soft Block"
  (default 0).
- Per-frame writers: the AsynchronousUpdate seam (glide queue advance,
  once per async tick); the policies themselves are called only from
  the cave stubs.
- Policy call conventions asserted in the file: the policy exports are
  stdcall (caves push the args in reverse); the PursuitSetTarget export
  replaces a thiscall site directly (fastcall-with-dummy-edx); the
  SnapshotAdjustTarget export replaces a ret-0x14 worker-thread
  function. Caves preserve every register the fork marked live (ebx
  into the arrival/continue targets, esi/edi into the slide
  destinations); all routing targets re-establish their own flags.
- Cross-file seam: `IEex_Vanilla` (set by the loader elsewhere) no-ops
  the whole file. The export probes (`IEex_LabelDefault(name, nil)`)
  make the seams degrade gracefully to the constants pack when the DLL
  predates the policies, printing "helper policies not exported by
  IEexHelper.dll - detours skipped (constants pack still active)".

## IEex_Helper_* entry points

- `IEex_Helper_PF_PursuitSetTarget` — keep-path SetTarget replacement
  (0x73F519 call retarget).
- `IEex_Helper_PF_ArrivalPolicy` — waypoint-arrival decision (seam
  0x6F9651); swaps the fresh route in mid-stride for phase 7.
- `IEex_Helper_PF_CollisionPolicy` — next-cell mobile-cost decision
  (seam 0x6F9CF7).
- `IEex_Helper_PF_BumpGatePolicy` — EA > 30 may-bump gate (seam
  0x6FA929; also the probe name gating the phase-4 pair).
- `IEex_Helper_PF_BumpObstaclePolicy` — EA ≤ 30 obstacles unshovable
  (seam 0x6FACBB).
- `IEex_Helper_PF_BumpSlidePolicy` — combat-slide candidate validator
  (seam 0x6FAFF5; also the probe name gating the phase-6 block).
- `IEex_Helper_PF_CanAnimatePolicy` — party-melee bumpability (seam
  0x6FB5D7).
- `IEex_Helper_PF_SlideJump` — glided JumpToPoint shim (0x6FB0A8,
  0x6FB22B, 0x6F7207 call retargets; also a nested probe name).
- `IEex_Helper_PF_GlideTick` — glide queue advance (0x4237A0 seam).
- `IEex_Helper_PF_BumpCrowdPolicy` — crowd-gate override (seam
  0x6FAB7B).
- `IEex_Helper_PF_SnapshotAdjustTarget` — whole-replace of
  `CGameArea::SnapshotAdjustTarget` (0x46A630).

(`IEex_Vanilla`, `IEex_GetPrivateProfileInt`, `IEex_ReadByte`,
`IEex_WriteAssembly`, `IEex_WriteAssemblyAuto`, `IEex_FlattenTable`,
`IEex_LabelDefault`, `IEex_DisableCodeProtection`/
`IEex_EnableCodeProtection` and the `!call`/`!jmp_dword`/`!repeat`/
`!je_dword`/`!jne_dword`/`!jbe_dword` assembler forms are core-DLL API,
not `IEex_Helper_*`.)
