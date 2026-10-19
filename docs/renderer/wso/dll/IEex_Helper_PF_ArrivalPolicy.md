# IEex_Helper_PF_ArrivalPolicy

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10020e50` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10020e50.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Pathfinding_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

bool FUN_10020e50(int param_1,int param_2,int param_3)

{
  char cVar1;
  undefined1 uVar2;
  int *piVar3;
  int iVar4;
  int iVar5;
  byte *pbVar6;
  uint uVar7;
  int iVar8;
  int iVar9;
  double dVar10;
  undefined1 auStack_68 [12];
  int iStack_5c;
  uint uStack_58;
  int iStack_54;
  int local_50;
  uint uStack_4c;
  undefined4 uStack_48;
  undefined4 uStack_44;
  int local_38;
  int iStack_34;
  int local_30;
  uint uStack_2c;
  int iStack_28;
  int iStack_24;
  uint local_14;

  local_14 = DAT_1008f040 ^ (uint)auStack_68;
  local_38 = param_1;
  LOCK();
  DAT_103d4758 = DAT_103d4758 + 1;
  UNLOCK();
  piVar3 = (int *)FUN_100201c0(&local_50);
  local_50 = *piVar3;
  uStack_4c = piVar3[1];
  uStack_48 = piVar3[2];
  uStack_44 = piVar3[3];
  local_30 = local_50;
  uStack_2c = uStack_4c;
  iStack_28 = uStack_48;
  iStack_24 = uStack_44;
  FUN_100205d0();
  iVar8 = _DAT_008cf6d8;
  if (((char)local_30 == '\0') ||
     (((_DAT_008cf6d8 != 0 && (iVar4 = (*(code *)0x452b50)(), iVar4 != 0)) &&
      (1 < *(int *)(iVar8 + 0x1084))))) {
    return param_3 <= param_2;
  }
  piVar3 = (int *)FUN_100204a0();
  if ((uStack_48._3_1_ == '\0') || (cVar1 = FUN_10020ad0(&local_50), cVar1 == '\0')) {
    if (((uStack_4c & 2) != 0) && (0 < piVar3[1])) {
      return false;
    }
    if (param_2 < param_3) {
      *piVar3 = 0;
      piVar3[3] = 0;
      return false;
    }
    if (param_2 < 0x241) {
      *piVar3 = 0;
      if ((((uStack_4c & 8) != 0) && (*(int *)(param_1 + 0x5382) != 0)) &&
         (*(short *)(param_1 + 0x53c0) < *(short *)(param_1 + 0x5386))) {
        iVar8 = *(int *)(*(int *)(param_1 + 0x5382) + *(short *)(param_1 + 0x53c0) * 4);
        local_30 = iVar8 % 0x140;
        uStack_2c = 0x13f - iVar8 / 0x140;
        uVar2 = (**(code **)(**(int **)(param_1 + 0x50f0) + 0x5c))();
        uStack_58 = CONCAT31(uStack_58._1_3_,uVar2);
        cVar1 = (*(code *)0x547d30)(&local_30,param_1 + 0x50bb,uStack_58,0);
        iVar8 = local_38;
        if ((cVar1 == DAT_0084d6a3) &&
           (iStack_54 = *(int *)(param_1 + 0x12), *(int *)(iStack_54 + 0x826) != 0)) {
          iVar4 = (int)((uStack_58 & 0xff) - 2) / 2;
          uStack_58 = local_30 - iVar4;
          if ((int)uStack_58 < 1) {
            uStack_58 = 0;
          }
          iVar5 = *(int *)(*(int *)(param_1 + 0x12) + 0x852) + -1;
          iVar9 = iVar4 + local_30;
          if (iVar5 <= iVar4 + local_30) {
            iVar9 = iVar5;
          }
          iStack_5c = uStack_2c - iVar4;
          iVar5 = *(int *)(*(int *)(param_1 + 0x12) + 0x856) + -1;
          iStack_34 = iVar4 + uStack_2c;
          if (iVar5 <= (int)(iVar4 + uStack_2c)) {
            iStack_34 = iVar5;
          }
          if (iStack_5c < 1) {
            iStack_5c = 0;
          }
          if (iStack_5c <= iStack_34) {
            iVar4 = iStack_5c * *(int *)(*(int *)(param_1 + 0x12) + 0x852);
            do {
              if ((int)uStack_58 <= iVar9) {
                pbVar6 = (byte *)(*(int *)(iStack_54 + 0x826) + uStack_58 + iVar4);
                iVar5 = uStack_58;
                do {
                  if ((*pbVar6 & 1) != 0) {
                    cVar1 = FUN_10020a30();
                    if (cVar1 == '\0') {
                      return true;
                    }
                    LOCK();
                    DAT_103d477c = DAT_103d477c + 1;
                    UNLOCK();
                    if (uStack_44._1_1_ == '\0') {
                      return (bool)2;
                    }
                    FUN_100203b0("lookahead-repath id=%d cell=%d,%d",*(undefined4 *)(iVar8 + 0x5c),
                                 local_30,uStack_2c);
                    return (bool)2;
                  }
                  iVar5 = iVar5 + 1;
                  pbVar6 = pbVar6 + 1;
                  param_1 = local_38;
                } while (iVar5 <= iVar9);
              }
              iStack_5c = iStack_5c + 1;
              iVar4 = iVar4 + *(int *)(*(int *)(param_1 + 0x12) + 0x852);
              if (iStack_34 < iStack_5c) {
                return true;
              }
            } while( true );
          }
        }
      }
    }
    else if ((param_2 < 0x901) && ((uStack_4c & 1) != 0)) {
      cVar1 = FUN_100208b0();
      if (cVar1 == '\0') {
        if (uStack_44._1_1_ != '\0') {
          FUN_100203b0("stall-line-blocked id=%d distSq=%d",*(undefined4 *)(param_1 + 0x5c),param_2)
          ;
        }
      }
      else {
        *piVar3 = *piVar3 + 1;
        if (*piVar3 < 4) {
          iStack_54 = *(int *)(param_1 + 0x535a) - *(int *)(param_1 + 6);
          iStack_5c = ((*(int *)(param_1 + 0x535e) / 0xc) * 0x10 - (*(int *)(param_1 + 10) << 2) / 3
                      ) + 8;
          dVar10 = (double)iStack_54 * (double)iStack_54 + (double)iStack_5c * (double)iStack_5c;
          if (dVar10 < 0.0) {
            libm_sse2_sqrt_precise();
          }
          else {
            dVar10 = SQRT(dVar10);
          }
          uStack_58 = (uint)dVar10;
          if (uStack_58 == 0) {
            *piVar3 = 0;
            return true;
          }
          uVar7 = (**(code **)(**(int **)(param_1 + 0x50f0) + 0x40))();
          uVar7 = uVar7 & 0xff;
          if (uVar7 < 9) {
            iStack_5c = (int)(uVar7 * iStack_5c * 0x400) / (int)uStack_58;
            iVar8 = (int)(uVar7 * iStack_54 * 0x400) / (int)uStack_58;
          }
          else {
            if ((int)uStack_58 < (int)uVar7) {
              iVar8 = 1;
            }
            else {
              iVar8 = (int)((uVar7 >> 1) + uStack_58) / (int)uVar7;
            }
            iStack_5c = (iStack_5c << 10) / iVar8;
            iVar8 = (iStack_54 << 10) / iVar8;
          }
          *(int *)(param_1 + 0x5352) = iVar8;
          *(int *)(param_1 + 0x5356) = iStack_5c;
          LOCK();
          DAT_103d4770 = DAT_103d4770 + 1;
          UNLOCK();
          if (uStack_44._1_1_ == '\0') {
            return false;
          }
          FUN_100203b0("stall-reaim id=%d n=%d distSq=%d",*(undefined4 *)(param_1 + 0x5c),*piVar3,
                       param_2);
          return false;
        }
      }
      iVar8 = piVar3[3];
      *piVar3 = 0;
      piVar3[3] = iVar8 + 1;
      if (iVar8 + 1 < 5) {
        cVar1 = FUN_10020a30();
        if (cVar1 == '\0') {
          return true;
        }
        LOCK();
        DAT_103d4774 = DAT_103d4774 + 1;
        UNLOCK();
        if (uStack_44._1_1_ != '\0') {
          FUN_100203b0("stall-repath id=%d distSq=%d",*(undefined4 *)(param_1 + 0x5c),param_2);
        }
      }
      else {
        piVar3[3] = 0;
        (*(code *)0x6ff2f0)();
        (*(code *)0x6ff320)();
        (*(code *)0x707170)();
        if (uStack_44._1_1_ != '\0') {
          FUN_100203b0("stall-giveup id=%d distSq=%d",*(undefined4 *)(param_1 + 0x5c),param_2);
          return (bool)2;
        }
      }
      return (bool)2;
    }
  }
  return true;
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

`IEex_Helper_PF_ArrivalPolicy` - 0x10020e50 (FUN_10020e50) -
stdcall, 3 stack args (sprite, distNowSq, distPrevSq), callee cleans
0xC; entered from the IEex_Pathfinding_Patch.lua cave at engine
0x6F9651 (AIUpdateWalk's arrival check, which the vanilla build
answers with `cmp ecx,edi; jl 0x6F9AB3`).

##### Behavior

Decides what a sprite should do when it looks like it has arrived
(or is stuck short of) its goal. Counted, logged, stateful:

1. `lock incl DAT_103d4758` (calls); config; FUN_100205d0 walk
   tracker (ECX = sprite).
2. Master byte 0 OR multiplayer -> `return param_3 <= param_2`,
   the vanilla comparison this seam replaced.
3. `state = FUN_100204a0()`.
4. Byte +11 ("IP Pursuit Keep Path") 0 -> return 1 (the vanilla
   arrival outcome -- this seam only fires where vanilla's `jl`
   fell through, i.e. arrival-suspect). Byte +11 on: run the
   pursuit-swap helper FUN_10020ad0 (ECX = sprite, EDX = state,
   stack = config). It acts only while the sprite's current request
   pointer +0x53d6 still matches the request PursuitSetTarget
   remembered in state+0x10: it splices the fresh route into the
   target's path (truncate via 0x6fa810 at the best node, clear the
   target id bytes, re-derive the destination, engine calls 0x707d40
   / 0x707230 or the stop pair 0x6ff320), clears the remembered
   request, counts pursuitSwap or pursuitDrift, and returns 1 (swapped
   -> this policy returns 1 = arrival handled) or 0 (drift ->
   continue below).
5. `(flags & 2) && state+4 > 0` -> return 0 (a wait round is
   consuming ticks; keep the sprite where it is). If
   `param_2 < param_3` (drifting AWAY), zero state+0 and state+0xc
   and return 0.
6. **Lookahead** (only when `param_2 < 0x241`, i.e. within ~24 px,
   and `flags & 8`): zero state+0; requires a live path (+0x5382)
   and waypoint short +0x53c0 < count short +0x5386; takes the
   waypoint node, decodes its cell `(node % 0x140, 0x13f -
   node / 0x140)`, reads the sprite's radius from animation vtable
   +0x5c, and asks cost query 0x547d30 about the node's cell. If
   that cell is COST_IMPASSABLE (and the area's block bitmap at
   `*(*(sprite+0x12)+0x826)` is non-null), it scans the radius box
   around the node (r = (radiusByte - 2) / 2, area-bounded): any
   cell with bit 0 set (blocked) means the goal is walled off -- if
   FUN_10020a30() (glide/walk to the last node) returns 0, return 1
   (arrive where we are); otherwise count lookahead
   (DAT_103d477c), log `"lookahead-repath id=%d cell=%d,%d"` and
   return 2 (abort this walk tick for a repath). Scan finds nothing
   blocked -> return 1 (arrive).
7. **Stall** (only when `param_2 < 0x901`, within ~48 px, and
   `flags & 1`): LOS check FUN_100208b0 (ECX = sprite) -- 0 means
   the straight line home is blocked; log
   `"stall-line-blocked id=%d distSq=%d"` and fall to the repath
   arm. Otherwise increment state+0 (the re-aim counter): below 4,
   compute a sidestep from the re-aim target +0x535a/+0x535e toward
   the sprite's current true position (+6/+10), scale it by the
   animation radius (vtable +0x40), store it as the render offset
   +0x5352/+0x5356 (FUN_100207e0 undoes it later), count reaim,
   log `"stall-reaim id=%d n=%d distSq=%d"`, return 0 (keep
   walking). At 4 or more: increment state+0xc (the repath
   counter); below 5 -> FUN_10020a30(); 0 -> return 1 (arrive),
   else count stallRepath, log `"stall-repath id=%d distSq=%d"`,
   return 2. At 5 -> reset both counters, stop the sprite with the
   engine trio `(*0x6ff2f0)(); (*0x6ff320)(); (*0x707170)();`, log
   `"stall-giveup id=%d distSq=%d"`, return 2.
8. Fall-through -> return 1 (arrive; the vanilla answer for a
   non-suspect arrival).

The cave routes eax: 0 -> 0x6F9AB3 (continue walking), 1 ->
0x6F9659 (arrival), 2 -> SEH slot -1 (`mov dword [esp+0x60],-1`,
the same unwind slot the vanilla abort at 0x6FA7DF writes) + jmp
0x6FA7E7 (abort this AIUpdateWalk so the repath can run).

##### Failure fallbacks

Master off / multiplayer -> the vanilla comparison, byte for byte.
Every threshold branch ends in either the vanilla arrival (1), the
vanilla continue (0), or the vanilla-shaped abort (2); the give-up
arm stops the sprite with the engine's own stop calls. No
allocation; the state counters simply reset when exhausted.

##### Decompiler artifact

FUN_100201c0's 16-byte result is copied into TWO local sets
(`local_50` and the `uStack_48` family) -- one copy feeds the flag
tests, the other the byte tests; they are the same values. The log
gates show as `uStack_5c._1_1_` / `local_20._1_1_`: disassembly
(config copy base esp+0x20, `cmpb $0x0` at +13 of the copy at
0x204e1/0x2065b/0x206aa/0x206f8/0x2073e) proves all of them are
config byte +13. FUN_10020a30's result is compared as `== '\0'`
then counter++/return 2 -- polarity: 0 -> arrive (return 1), nonzero
-> repath (return 2).

##### Lua wiring

Lines 236-265, inside the `IEex_LabelDefault("IEex_Helper_PF_ArrivalPolicy")`
block that also installs the CollisionPolicy cave: VerifyBytes
checks `3B CF 0F 8C 5A 04 00 00` at 0x6F9651 (the vanilla
`cmp edi,ecx; jl` long-forward); the cave pushes edi (distPrevSq),
ecx (distNowSq), esi (this) -- stdcall args -- around
`!call >IEex_Helper_PF_ArrivalPolicy`, pops them, then
`cmp eax,1 / je :6F9659 / cmp eax,2 / jne :6F9AB3 / mov dword
[esp+0x60],-1 / jmp :6FA7E7`, and patches the seam with `!jmp_dword`
+ `repeat(3,!nop)`. Comment: "ebx is LIVE into both arrival and
continue paths" (hence ebx untouched); "All three routing targets
re-establish their own flags."

