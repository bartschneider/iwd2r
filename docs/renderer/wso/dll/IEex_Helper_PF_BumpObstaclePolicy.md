# IEex_Helper_PF_BumpObstaclePolicy

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10021c30` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10021c30.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Pathfinding_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10021c30(int *param_1,int *param_2)

{
  byte bVar1;
  int iVar2;
  int *piVar3;
  DWORD DVar4;
  int iVar5;
  int local_20;
  char cStack_1c;
  char cStack_1b;
  char cStack_1a;
  char cStack_19;
  int iStack_18;
  char cStack_14;
  char cStack_13;
  char cStack_12;
  char cStack_11;

  if ((param_2[0x152a] == 0) || (param_2[0x152e] != 0)) {
    return 0;
  }
  if ((DAT_103d473c != 0) &&
     (iVar2 = FUN_10023200(&local_20,param_2 + 0x17,
                           ((((*(byte *)(param_2 + 0x17) ^ 0x811c9dc5) * 0x1000193 ^
                             (uint)*(byte *)((int)param_2 + 0x5d)) * 0x1000193 ^
                            (uint)*(byte *)((int)param_2 + 0x5e)) * 0x1000193 ^
                           (uint)*(byte *)((int)param_2 + 0x5f)) * 0x1000193),
     *(int *)(iVar2 + 4) != 0)) {
    return 0;
  }
  piVar3 = (int *)FUN_100201c0(&local_20);
  local_20 = *piVar3;
  cStack_1c = (char)piVar3[1];
  cStack_1b = *(char *)((int)piVar3 + 5);
  cStack_1a = *(char *)((int)piVar3 + 6);
  cStack_19 = *(char *)((int)piVar3 + 7);
  iStack_18 = piVar3[2];
  cStack_14 = (char)piVar3[3];
  cStack_13 = *(char *)((int)piVar3 + 0xd);
  cStack_12 = *(char *)((int)piVar3 + 0xe);
  cStack_11 = *(char *)((int)piVar3 + 0xf);
  if ((((((char)*piVar3 != '\0') && ((char)((uint3)(int3)piVar3[2] >> 0x10) != '\0')) &&
       (local_20 = FUN_100204a0(), *(int *)(local_20 + 0x2c) != 0)) &&
      ((iVar2 = *(int *)(local_20 + 0x2c), DVar4 = GetTickCount(), DVar4 - iVar2 < 2000 &&
       (-1 < *(int *)(local_20 + 0x24))))) &&
     (iVar5 = *(int *)((int)param_2 + 6) - *(int *)(local_20 + 0x24),
     iVar2 = *(int *)((int)param_2 + 10) - *(int *)(local_20 + 0x28),
     0x30f < iVar2 * iVar2 + iVar5 * iVar5)) {
    return 0;
  }
  bVar1 = DAT_00847c3b;
  iVar2 = (**(code **)(*param_1 + 0x10))();
  if ((bVar1 < *(byte *)(iVar2 + 4)) &&
     (iVar2 = (**(code **)(*param_2 + 0x10))(), *(byte *)(iVar2 + 4) <= bVar1)) {
    LOCK();
    DAT_103d4784 = DAT_103d4784 + 1;
    UNLOCK();
    return 0;
  }
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

`IEex_Helper_PF_BumpObstaclePolicy` - 0x10021c30 (FUN_10021c30) -
stdcall, 2 stack args (bumper sprite, obstacle sprite), callee
cleans 0x8; entered from the IEex_Pathfinding_Patch.lua cave at
engine 0x6FACBB (the m_bBumpable / field_54B8 skip pair).

##### Behavior

The PC-protection half of the enemy-bump feature -- and the
combat-slide window check, in the same seam:

1. Obstacle hard gates (the vanilla pair this seam replaces):
   obstacle +0x152a (m_bBumpable) == 0 OR +0x152e (skip flag) != 0
   -> 0 (skip -- exactly what the replaced bytes would do).
2. Mid-glide guard: when the glide queue is non-empty, the FNV
   find FUN_10023200 over the obstacle's id bytes (+0x5c..+0x5f)
   hits -> 0 (never shove a sprite that is mid-glide).
3. Config-dependent window: master byte 0 AND byte +10 ("IP Combat
   Slide") -- when BOTH are set, the bumper's per-sprite state must
   show an active slide window (state+0x2c timestamp != 0, now -
   ts < 2000 ms, state+0x24 >= 0) whose origin (+0x24/+0x28) lies
   within distSq 0x30f of the obstacle's position; otherwise -> 0.
   When the feature is OFF this block is skipped entirely (see
   fallbacks).
4. EA pair: bumper EA > 30 AND obstacle EA <= 30 ->
   `lock incl DAT_103d4784` (sideDeny) and 0 -- the "enemies
   displace their own side only, never PCs" rule from the patch
   comment.
5. Otherwise 1 (proceed with the bump).

##### Failure fallbacks

With the config off (master or slide byte 0), steps 1/2/4/5 reduce
to vanilla: the obstacle gates are the replaced pair, the mid-glide
find cannot hit (glides only exist when SlideJump runs, which
requires the same config bytes), and the EA-pair deny is unreachable
because BumpGatePolicy (EA > 30 -> needs master byte) already
returned 0 upstream. No allocation, no exceptions.

##### Decompiler artifact

The config struct is again spilled into a full 16-byte local set;
the tests at +0/+10 read `*piVar3` / `piVar3[2] >> 0x10` of the
spill. The EA reads go through animation vtable +0x10 with byte +4
of the result -- the same convention the other bump policies use.

##### Lua wiring

Lines 326-342, in the BumpGatePolicy LabelDefault block: VerifyBytes
checks the 28 bytes `8B 86 A8 54 00 00 85 C0 0F 84 C4 06 00 00 8B
86 B8 54 00 00 85 C0 0F 85 B6 06 00 00` at 0x6FACBB (both
bBumpable and field_54B8 test pairs); the cave is minimal -- push
esi (obstacle), push ebx (bumper), call, `test eax,eax / !je_dword
:6FB38D` (skip) else `!jmp_dword :6FACD7` (proceed) -- and patches
the seam with `!jmp_dword` + `repeat(23,!nop)`.

