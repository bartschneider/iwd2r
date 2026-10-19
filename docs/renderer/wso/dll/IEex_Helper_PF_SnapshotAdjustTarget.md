# IEex_Helper_PF_SnapshotAdjustTarget

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10022310` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10022310.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Pathfinding_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_10022310(int param_1,int param_2,int *param_3,undefined4 param_4,int param_5)

{
  int iVar1;
  bool bVar2;
  char cVar3;
  char cVar4;
  uint uVar5;
  int iVar6;
  char *pcVar7;
  int iVar8;
  int in_ECX;
  uint uVar9;
  int iVar10;
  int iVar11;
  int iVar12;
  int iVar13;
  int iStack_60;
  int iStack_5c;
  int iStack_58;
  int iStack_54;
  int iStack_50;
  int local_4c;
  int local_48;
  int local_40;
  int local_3c;
  int local_34;
  int iStack_24;
  char cStack_20;
  char cStack_1f;
  char cStack_1e;
  char cStack_1d;
  char cStack_1c;
  char cStack_1b;
  char cStack_1a;
  char cStack_19;
  undefined4 uStack_18;
  char cStack_14;
  char cStack_13;
  char cStack_12;
  char cStack_11;

  cVar3 = DAT_0084d6a3;
  iVar11 = *param_3;
  if ((param_1 == iVar11) && (param_2 == param_3[1])) {
    return 0;
  }
  iVar13 = param_3[1];
  uVar9 = param_1 - iVar11;
  uVar5 = param_2 - iVar13;
  local_3c = (uVar5 ^ (int)uVar5 >> 0x1f) - ((int)uVar5 >> 0x1f);
  iVar6 = (uVar9 ^ (int)uVar9 >> 0x1f) - ((int)uVar9 >> 0x1f);
  if (local_3c < iVar6) {
    local_3c = -0x400;
    if (iVar11 < param_1) {
      local_3c = 0x400;
    }
    iVar6 = (int)(uVar5 * 0x400) / iVar6;
  }
  else {
    local_3c = (int)(uVar9 * 0x400) / local_3c;
    iVar6 = -0x400;
    if (iVar13 < param_2) {
      iVar6 = 0x400;
    }
  }
  local_40 = iVar11 * 0x400 + 0x200;
  local_4c = iVar13 * 0x400 + 0x200;
  for (; ((iVar11 != param_1 || (iVar13 != param_2)) && (0 < param_5)); param_5 = param_5 + -1) {
    cVar4 = (*(code *)0x5485a0)(iVar11,iVar13,param_4);
    if (cVar4 != cVar3) {
      if ((iVar11 == param_1) && (iVar13 == param_2)) {
        return 0;
      }
      *param_3 = iVar11;
      param_3[1] = iVar13;
      return 1;
    }
    local_40 = local_40 + local_3c;
    local_4c = local_4c + iVar6;
    iVar11 = local_40 >> 10;
    iVar13 = local_4c >> 10;
  }
  pcVar7 = (char *)FUN_100200b0(&cStack_20);
  iVar11 = _DAT_008cf6d8;
  cStack_20 = *pcVar7;
  cStack_1f = pcVar7[1];
  cStack_1e = pcVar7[2];
  cStack_1d = pcVar7[3];
  cStack_1c = pcVar7[4];
  cStack_1b = pcVar7[5];
  cStack_1a = pcVar7[6];
  cStack_19 = pcVar7[7];
  uStack_18 = *(undefined4 *)(pcVar7 + 8);
  cStack_14 = pcVar7[0xc];
  cStack_13 = pcVar7[0xd];
  cStack_12 = pcVar7[0xe];
  cStack_11 = pcVar7[0xf];
  if (cStack_20 == '\0') {
    return 0;
  }
  if ((char)((uint3)*(undefined3 *)(pcVar7 + 8) >> 8) == '\0') {
    return 0;
  }
  if (((_DAT_008cf6d8 != 0) && (iVar13 = (*(code *)0x452b50)(), iVar13 != 0)) &&
     (1 < *(int *)(iVar11 + 0x1084))) {
    return 0;
  }
  iStack_60 = 0;
  iStack_5c = 0;
  bVar2 = false;
  iVar11 = *(int *)(in_ECX + 0x852);
  iVar13 = *(int *)(in_ECX + 0x856);
  local_48 = -1;
  iVar6 = *param_3;
  iVar1 = param_3[1];
  iStack_58 = 1;
  do {
    if (bVar2) goto LAB_1002269c;
    iStack_54 = 0x7fffffff;
    iStack_50 = 0x7fffffff;
    if (local_48 <= iStack_58) {
      iStack_24 = iVar6 - param_1;
      local_3c = iVar6 + local_48;
      iVar12 = local_48;
      local_34 = local_48;
      while( true ) {
        do {
          if ((((iVar12 <= local_48) || (iStack_58 <= iVar12)) ||
              ((local_34 <= local_48 || (iStack_58 <= local_34)))) &&
             ((((((-1 < local_3c && (iVar8 = iVar1 + local_34, -1 < iVar8)) &&
                 (iVar10 = iVar6 + iVar12, iVar10 < iVar11)) && (iVar8 < iVar13)) &&
               ((iVar10 != param_1 || (iVar8 != param_2)))) &&
              (cVar4 = (*(code *)0x5485a0)(iVar10,iVar8,param_4), cVar4 != cVar3)))) {
            iVar10 = iVar12 * iVar12 + local_34 * local_34;
            iVar8 = (iVar1 - param_2) + local_34;
            iVar8 = (iStack_24 + iVar12) * (iStack_24 + iVar12) + iVar8 * iVar8;
            if ((iVar10 < iStack_54) || ((iVar10 == iStack_54 && (iVar8 < iStack_50)))) {
              iStack_60 = iVar6 + iVar12;
              iStack_5c = iVar1 + local_34;
              bVar2 = true;
              iStack_54 = iVar10;
              iStack_50 = iVar8;
            }
          }
          local_3c = local_3c + 1;
          iVar12 = iVar12 + 1;
        } while (iVar12 <= iStack_58);
        local_34 = local_34 + 1;
        if (iStack_58 < local_34) break;
        iStack_24 = iVar6 - param_1;
        local_3c = iVar6 + local_48;
        iVar12 = local_48;
      }
    }
    local_48 = local_48 + -1;
    iStack_58 = iStack_58 + 1;
  } while (-7 < local_48);
  if (!bVar2) {
    return 0;
  }
LAB_1002269c:
  LOCK();
  DAT_103d4788 = DAT_103d4788 + 1;
  UNLOCK();
  *param_3 = iStack_60;
  param_3[1] = iStack_5c;
  return 1;
}
```

## Notes

### The pathfinding policy subsystem (all 11 PF_* helpers)

The staged `IEex_Pathfinding_Patch.lua` reworks the engine's movement
AI around one idea: the engine's hard-coded stop/bump/teleport
decisions become calls into DLL export policies that read a small
config block, consult per-sprite bookkeeping, and return the route
the engine should take -- with every policy returning the exact
vanilla answer whenever the subsystem is off. All eleven exports live
in the fork's pathfinding blob FUN_10020d20..FUN_10022950 and share
the machinery below (decompiled from the fork's
`IEexHelper.fork.dll`).

#### Two install layers (what runs when)

The patch separates raw byte patches from DLL seams, and gates them
differently:

- **Boot-gated byte patches** (phase 1 constants, lines 66-204, and
  phase 4b, lines 501-505) install only when the Lua-side master
  toggle `IEex_GetPrivateProfileInt("IEex Options", "Improved
  Pathfinding", 1, ".\\Icewind2.ini") ~= 0` (line 42) -- the patch
  comment: only these have no runtime gate, so they honor the toggle
  at boot. Phase 4b additionally requires its own key "IP Enemy Soft
  Block" (default 0).
- **DLL policy seams** (`IEex_LabelDefault("IEex_Helper_PF_*")`
  blocks at lines 206 / 236 / 307 / 364 / 518, with SlideJump's
  retargets + GlideTick nested inside the line-364 block at 428)
  install whenever the export exists, regardless of the master
  toggle -- each policy then self-gates at runtime on the config
  master byte, which is re-read from the ini every 2 seconds. The
  comment: this "makes the in-game toggle live in both directions".
- Every site goes through the patch's `IEex_PF_VerifyBytes` (lines
  47-63): each expected byte is read with `IEex_ReadByte` first; on
  the first mismatch it prints
  `"[IEex_Pathfinding] SKIP patch @0x%X: byte %d is 0x%02X, expected 0x%02X"`
  and skips that site. The footer (line 526) prints
  `"[IEex_Pathfinding] loaded (%d/%d sites patched)"`.
- All writes use the loader's public primitives `IEex_WriteAssembly`
  / `IEex_WriteAssemblyAuto` / `IEex_ReadByte` inside
  `IEex_DisableCodeProtection()` .. `IEex_EnableCodeProtection()`
  (Bubb13/IEexLoader, IEexDLL/IEexLua.asm -- engine code patching,
  not fork internals).

#### The config struct and its INI keys

FUN_100200b0(out) fills a 16-byte struct with
`GetPrivateProfileIntA` reads, refreshed only when the caller's tick
cache is 0 or stale by more than 2000 ms. Each thread has its own
instance: the game-thread helpers go through FUN_100201c0 (global
instance DAT_10092b20, cache DAT_104df698), while the worker-thread
SnapshotAdjustTarget calls FUN_100200b0 directly (instance
DAT_10092b10, cache DAT_104df69c). Identical values, independent
refresh -- no cross-thread sharing.

| Byte | Key | File | Default |
|---|---|---|---|
| +0 | IEex Options / Improved Pathfinding (master) | .\Icewind2.ini | 1 |
| +4 (dword) | IP Behavior Flags | .\Icewind2.ini | 0xf |
| +8 | IP Enemy Bumping | .\Icewind2.ini | 1 |
| +9 | IP Directed Adjust | .\Icewind2.ini | 1 |
| +10 | IP Combat Slide | .\Icewind2.ini | 0 |
| +11 | IP Pursuit Keep Path | .\Icewind2.ini | 1 |
| +12 | IP Ally Queue | .\Icewind2.ini | 1 |
| +13 | Options / Path Log | .\IEex.ini | 0 |

FUN_100201c0's first call also prints
`"path-log enabled: pathfinding=%d flags=0x%X enemyBump=%d
directedAdjust=%d combatSlide=%d"`. The flags dword gates the
AIUpdateWalk-seam branches: bit 0 = the stall/re-aim branch, bit 1 =
wait rounds and the revert-vs-wait choice, bit 2 = the
waypoint/arrived-stop path (clear = always revert), bit 3 = the
lookahead scan. Four further keys are read only by the Lua patch
(phase-1 constants, not by these exports): "IP Pursuit Repath"
(default 8, clamped to [2, 16], verify 0x73F321), "IP Collision
Smoothing" (default 0, verify 0x549480), "IP Bump Idle NPCs"
(default 1, verify 0x6FB4C5), "IP Enemy Soft Block" (default 0,
verify 0x54961E).

#### The verbose log gate and `.\IEex_Path.log`

Every log line in the group is gated on config byte +13 ("Path Log",
default 0) and goes through FUN_100203b0: a one-time
`InitializeCriticalSection`, a lazy `_fsopen(".\IEex_Path.log",
"a", _SH_DENYNO)` handle (DAT_104df688), a GetTickCount timestamp
prefix, `__stdio_common_vfprintf`, newline, `fflush`, leave the
critical section. So all trace lines below land in
`.\IEex_Path.log` prefixed with the tick. Decompiler caveat: the
gate is misnamed per function -- `uStack_5c._1_1_` (ArrivalPolicy /
CollisionPolicy), `local_20._1_1_` (PursuitSetTarget), `cStack_33`
(BumpSlidePolicy), `cStack_23` (SlideJump) -- in every case the
disassembly shows `cmpb $0x0` on the config-copy byte at offset
+13. Only two gates in the group are genuinely byte +12 ("IP Ally
Queue"): the CollisionPolicy ally-queue scan and the wait-consume OR.

#### The 21 performance counters

FUN_100201c0 prints one stats line, gated on byte +13, throttled to
once per 10 seconds, and only when the 21-counter sum changed since
the last print:

`"stats calls=%ld/%ld blocked=%ld bumpClear=%ld wait=%ld revert=%ld
reaim=%ld stallRepath=%ld arrivedStop=%ld lookahead=%ld enemyBump=%ld
sideDeny=%ld ring=%ld staticBlock=%ld slide=%ld crowdRelax=%ld
pursuitKeep=%ld pursuitSwap=%ld pursuitDrift=%ld allyQueue=%ld
allyQueueYield=%ld"` (the `calls` conversion consumes two counters).

| Counter | Address | Incremented by |
|---|---|---|
| calls | DAT_103d4758 | ArrivalPolicy entry |
| calls (2nd) | DAT_103d475c | CollisionPolicy entry |
| blocked | DAT_103d4760 | CollisionPolicy, impassable entry |
| bumpClear | DAT_103d4764 | CollisionPolicy, engine clear-bump nonzero |
| wait | DAT_103d4768 | CollisionPolicy wait branch |
| revert | DAT_103d476c | CollisionPolicy revert branch |
| reaim | DAT_103d4770 | ArrivalPolicy stall re-aim |
| stallRepath | DAT_103d4774 | ArrivalPolicy stall repath |
| arrivedStop | DAT_103d4778 | CollisionPolicy arrived-stop |
| lookahead | DAT_103d477c | ArrivalPolicy lookahead repath |
| enemyBump | DAT_103d4780 | BumpGatePolicy allow |
| sideDeny | DAT_103d4784 | BumpObstaclePolicy enemy->PC deny |
| ring | DAT_103d4788 | SnapshotAdjustTarget ring hit |
| staticBlock | DAT_103d478c | CollisionPolicy static block |
| slide | DAT_103d4790 | BumpSlidePolicy accept |
| crowdRelax | DAT_103d4794 | BumpCrowdPolicy allow |
| pursuitKeep | DAT_103d4798 | PursuitSetTarget keep path |
| pursuitSwap | DAT_103d479c | FUN_10020ad0 swap |
| pursuitDrift | DAT_103d47a0 | FUN_10020ad0 drift |
| allyQueue | DAT_103d47a4 | CollisionPolicy queue |
| allyQueueYield | DAT_103d47a8 | CollisionPolicy yield |

Every increment is `lock`-prefixed -- the group's only atomic
operations.

#### Per-sprite state and the shared registries

- FUN_100204a0 acquires a 40-byte hash entry keyed by the sprite's
  id dword (`+0x5c`), FNV-1a (0x811c9dc5 / 0x1000193) over the four
  id bytes, and stamps `entry+0x3c = GetTickCount()`. It returns
  `entry + 0xc`, i.e. the caller sees: +0 re-aim counter, +4 wait
  ticks (slot +0x10 of the entry is reused by PursuitSetTarget as
  the remembered search request), +8 wait round, +0xc repath
  counter, +0x14 ally-queue count, +0x18 queued-other id,
  +0x1c/+0x20 remembered blocked cell, +0x24/+0x28 slide origin,
  +0x2c slide timestamp. A sweep every 0x1000 acquires frees
  entries idle for more than 30001 ms.
- FUN_100205d0 is the walk tracker: a second hash (entries 0x28
  bytes) recording walk begin-tick/position per sprite; entries
  older than 60001 ms are removed and walks idle for more than
  1200 ms log `"walk-end id=%d dur=%lu begin=%lu pos=%d,%d"`.
  ArrivalPolicy calls it on entry with the sprite in ECX.
- FUN_10023270 manages the **glide queue** (0x30-byte nodes in an
  intrusive list + MSVC `std::unordered_map` buckets: +8 id, +0xc/
  +0x10 from x/y, +0x14/+0x18 to x/y, +0x1c/+0x20 last pos, +0x24
  tick n, +0x28 total ticks, +0x2c miss counter; size
  DAT_103d473c, sentinel head DAT_103d4738), FUN_10022bf0 erases.
  FUN_10023200 is a bucket FIND over the same sentinel -- so
  "FNV-marked" anywhere in the group means *this sprite currently
  has a glide node*. FUN_10023680 is the same shape over the
  per-sprite-state registry.
- Inner motion helpers used by the policies: FUN_100207e0 (un-apply
  a stall re-aim: subtract the +0x5352/+0x5356 render offset,
  restore the true position from +0x5362/+0x5366, rebuild the
  search-map entry), FUN_10020a30 (glide/walk to the last path
  node via engine vtable call +0xb8), FUN_10020ad0 (the
  pursuit-swap/drift helper, called only from ArrivalPolicy with
  ECX = sprite, EDX = per-sprite state), FUN_100208b0 (a stepped
  line-of-sight check over cost queries 0x547b10).

#### The multiplayer gate

Every policy refuses to deviate under multiplayer:
`(*0x452b50)(chitin + 0x952)() != 0 && 1 < *(int *)(chitin +
0x1084)` (session check plus player count > 1), chitin =
`DAT_008cf6d8`. The Lua comment confirms the design: all DLL seams
install in single-player and multiplayer alike; the policies
themselves go vanilla under multiplayer.

#### Seam map and return conventions

| Helper | Engine site | Patch shape | Return route |
|---|---|---|---|
| PF_PursuitSetTarget | 0x73F519 (MoveToObject's SetTarget call) | 5-byte `!call` retarget | eax ignored by the seam; keep-path eax is the policy's own state/logger path, not SetTarget's return |
| PF_ArrivalPolicy | 0x6F9651 (AIUpdateWalk arrival seam) | register-preserving cave | 0 -> 0x6F9AB3 continue, 1 -> 0x6F9659 arrival, 2 -> SEH slot -1 + jmp 0x6FA7E7 abort |
| PF_CollisionPolicy | 0x6F9CF7 (replaces the vanilla cost compare) | register-preserving cave | 0 -> 0x6F9D03 blocked, 1 -> 0x6F9F22 usable, 2 -> 0x6FA665 handled, 3 -> 0x6F9D1C revert-skip |
| PF_BumpGatePolicy | 0x6FA929 (ClearBumpPath EA gate) | cave, eax = EA byte | nonzero -> 0x6FA938 allow, 0 -> xor eax + jmp 0x6FB417 deny |
| PF_BumpObstaclePolicy | 0x6FACBB (m_bBumpable/field_54B8 pair) | cave, ebx = bumper, esi = obstacle | 0 -> 0x6FB38D skip, nonzero -> 0x6FACD7 |
| PF_BumpSlidePolicy | 0x6FAFF5 (candidate-cost compare) | cave re-runs the compare, then calls | nonzero -> 0x6FB014 accept, 0 -> 0x6FAFFD reject |
| PF_BumpCrowdPolicy | 0x6FAB7B (crowd-count gate) | cave re-runs `shr al,1` + store, then calls | nonzero -> 0x6FAB89 continue, 0 -> 0x6FAA74 abort |
| PF_CanAnimatePolicy | 0x6FB5D7 (weapon-ability branch) | cave keeps the RANGED tests, then calls | nonzero -> 0x6FB608 bumpable, 0 -> 0x6FB5E3 vanilla melee |
| PF_SlideJump | 0x6FB0A8 / 0x6FB22B / 0x6F7207 (JumpToPoint call sites) | 5-byte `!call` retargets | eax = 0x745950's return on the pass-through path, 0 on the glide path |
| PF_GlideTick | 0x4237A0 (AsynchronousUpdate entry) | cave preserves eax/ecx/edx, re-runs the 9 displaced bytes | void |
| PF_SnapshotAdjustTarget | 0x46A630 (whole-replace) | `!jmp_dword` + 2 nops | 0 = keep the incoming cell, 1 = the cell was rewritten |

Gate grouping in the Lua: the line-236 block installs BOTH the
Arrival and Collision caves; line 307 installs BOTH bump caves
(BumpGate + BumpObstacle); line 364 installs BumpSlide + CanAnimate +
BumpCrowd and contains the nested line-428 SlideJump block
(SlideJump's three retargets + GlideTick). Each block's else-branch
prints `"[IEex_Pathfinding] helper policies not exported by
IEexHelper.dll - detours skipped (constants pack still active)"`.

#### Threads and synchronization

Arrival/Collision run inside the game thread's AIUpdateWalk;
PursuitSetTarget and the bump/slide/can-animate policies on the
game thread's movement paths; SlideJump wherever JumpToPoint was
called; GlideTick once per asynchronous tick from
CBaldurChitin::AsynchronousUpdate (the cave preserves eax/ecx/edx
around the call); SnapshotAdjustTarget on the search **worker
thread** -- the only worker-thread helper in the group, and it
touches only its own config instance (objdump raw 0x21850) plus the
state/counter blobs. The lock-prefixed counter increments are the
group's only atomics; the hash registries, the glide queue and the
walk tracker have no locks and rely on single-threaded callers
(the same stance as every other registry in this DLL).

#### Failure model and engine-address attribution

Every path degrades to exact vanilla: master byte 0 or a feature
byte 0 returns the vanilla route from each table row above;
multiplayer does the same; `IEex_PF_VerifyBytes` skips any site
whose bytes do not match the pinned engine build; the config cache
means an ini edit lands within 2 seconds without restart. No C++
exceptions, no allocation on policy paths (the hash registries
allocate only on first sight of a sprite). All engine addresses
named in these notes (seam sites, engine calls 0x707d40 / 0x745950
/ 0x547a00 family, statics DAT_00847c3b = EA_GOODCUTOFF 30 and
DAT_0084d6a3 = COST_IMPASSABLE, chitin global DAT_008cf6d8, the
sprite field offsets) are attributed to the staged patch comments
and this DLL's disassembly -- never verified against the engine
binary in this stream. The call gates (`*0x452b50`, `*0x599a50`
resolve / `*0x599e70` release, cost queries) are runtime-registered
engine pointers consumed through the loader's public address-list
mechanism (Bubb13/IEexLoader, IEexDLL/IEexLua.asm: IEex_Init /
IEex_AddressList); `IEex_WriteAssembly` itself is the loader's
public IEex_WriteByte / IEex_WriteByte_Count pair.

`IEex_Helper_PF_SnapshotAdjustTarget` - 0x10022310 (FUN_10022310) -
stdcall, 5 stack args (goal x, goal y, CPoint *in/out snapshot
cell, context id, probe step budget), callee cleans 0x14 (ret $0x14
at raw 0x2184d), ECX = the area object, no /GS cookie; entered from
the IEex_Pathfinding_Patch.lua whole-replace of engine 0x46A630
(CGameArea::SnapshotAdjustTarget).

##### Behavior

Runs on the search WORKER thread. Patch comment (phase 5, lines
507-521): "Whole-replace of CGameArea::SnapshotAdjustTarget @0x46A630
(worker thread, ret 0x14). Vanilla line-probe first (exact port);
on failure, expanding ring search (r=1..6) around the goal picks
the nearest passable snapshot cell, tie-broken toward the approach
side ('stop just before' an occupied destination). Gated by 'IP
Directed Adjust' (default 1) inside the DLL; vanilla-exact when
off."

1. Early out: the incoming cell (*param_3/param_3[1]) already
   equals the goal -> 0 (nothing to do).
2. **Vanilla line probe** (always, regardless of gates): step
   toward the goal in cell units along the dominant axis (steps of
   0x400 fixed-point with half-cell centers, +0x200), decrementing
   the budget param_5, asking cost query `(*0x5485a0)(x, y,
   param_4)` per cell. First passable cell: if it is the goal
   itself -> 0 (keep); otherwise write it to *param_3 and return 1.
   Budget exhausted while blocked -> the ring search.
3. **Ring search** (the new part, gated): refreshes config through
   its OWN worker-thread instance (FUN_100200b0 direct -- no
   stats/first-call print); master byte 0 -> 0; byte +9 ("IP
   Directed Adjust") 0 -> 0; multiplayer -> 0. Area bounds from
   in_ECX +0x852/+0x856. Rings r = 1..6 around the INCOMING cell:
   ring r scans ONLY cells at exact Chebyshev distance r (the
   perimeter -- offsets with off_x <= -r or r <= off_x or off_y <=
   -r or r <= off_y), skipping out-of-bounds cells, the exact goal
   cell, and COST_IMPASSABLE cells. Score = distSq from the ring
   center; tiebreak = distSq from the GOAL (approach side). The
   first ring containing a valid candidate decides (the found flag
   latches; later rings never run). Hit -> `lock incl
   DAT_103d4788` (ring), write the best cell to *param_3, return 1.
   No candidate in any ring -> 0 (keep the incoming cell).

##### Failure fallbacks

The probe is the exact vanilla port and always runs first, so with
"IP Directed Adjust" = 0 (or master 0 / multiplayer) the function
IS vanilla SnapshotAdjustTarget. Ring failure -> 0, the vanilla
"keep the snapshot cell" answer. Worker-thread safety: it touches
only its own config instance DAT_10092b10 / cache DAT_104df69c
(objdump raw 0x21850) and the lock-prefixed ring counter.

##### Decompiler artifact

The ring predicate LOOKS inverted in the decompile
(`if ((iVar12 <= local_48) || (iStack_58 <= iVar12) || ...)`) --
it is the process-condition, and it means perimeter-only scanning
(exact Chebyshev rings), not an interior fill. The earlier scratch
note in this stream ("strictly interior only, benign because the
found-flag latches") mis-stated the polarity; the coverage is
Chebyshev 1..6 exactly.

##### Lua wiring

Lines 518-520: `IEex_LabelDefault("IEex_Helper_PF_SnapshotAdjustTarget")`,
VerifyBytes checks `83 EC 14 8B 44 24 20` at 0x46A630 (the prologue
`sub esp,0x14 / mov eax,[esp+0x20]`), then
`IEex_WriteAssembly(0x46A630, {"!jmp_dword
>IEex_Helper_PF_SnapshotAdjustTarget !nop !nop"})` -- a whole-replace
jmp; the stdcall callee cleans the caller's five stack args via its
`ret 0x14`.

