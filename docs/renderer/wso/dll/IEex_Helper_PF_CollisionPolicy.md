# IEex_Helper_PF_CollisionPolicy

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10021390` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10021390.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Pathfinding_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_10021390(int param_1,uint param_2)

{
  char cVar1;
  undefined4 *puVar2;
  int iVar3;
  code *pcVar4;
  int iVar5;
  int iVar6;
  uint uVar7;
  undefined4 uVar8;
  uint uVar9;
  code *pcStack_78;
  undefined4 uStack_74;
  code *pcStack_70;
  int local_6c;
  code *pcStack_68;
  uint uStack_64;
  undefined4 local_60;
  undefined4 uStack_5c;
  undefined4 uStack_58;
  undefined4 uStack_54;
  code *pcStack_50;
  int iStack_4c;
  int iStack_48;
  uint uStack_44;
  int iStack_40;
  int iStack_3c;
  uint *puStack_38;
  uint uStack_34;
  undefined1 local_30 [4];
  int local_2c;
  int local_28;
  int iStack_24;
  int iStack_20;
  int iStack_1c;
  int iStack_18;
  uint local_14;

  local_14 = DAT_1008f040 ^ (uint)&pcStack_78;
  local_6c = param_1;
  LOCK();
  DAT_103d475c = DAT_103d475c + 1;
  UNLOCK();
  uStack_74 = CONCAT13(DAT_0084d6a3,(undefined3)uStack_74);
  puVar2 = (undefined4 *)FUN_100201c0(&local_60);
  iVar5 = _DAT_008cf6d8;
  local_60 = *puVar2;
  uStack_5c = puVar2[1];
  uStack_58 = puVar2[2];
  uStack_54 = puVar2[3];
  if (param_2 == uStack_74 >> 0x18) {
    LOCK();
    DAT_103d4760 = DAT_103d4760 + 1;
    UNLOCK();
    if (((char)local_60 == '\0') ||
       (((_DAT_008cf6d8 != 0 && (iVar3 = (*(code *)0x452b50)(), iVar3 != 0)) &&
        (1 < *(int *)(iVar5 + 0x1084))))) {
      return 0;
    }
    iVar5 = FUN_100204a0();
    iStack_1c = (int)((*(int *)(param_1 + 0x5362) >> 0x1f & 0xfU) + *(int *)(param_1 + 0x5362)) >> 4
    ;
    iStack_18 = *(int *)(param_1 + 0x5366) / 0xc;
    pcStack_70 = (code *)((int)(*(int *)(param_1 + 6) + (*(int *)(param_1 + 6) >> 0x1f & 0xfU)) >> 4
                         );
    uStack_34 = *(int *)(param_1 + 10) / 0xc;
    puStack_38 = (uint *)pcStack_70;
    if ((((uStack_5c & 2) != 0) || ((char)uStack_54 != '\0')) &&
       ((0 < *(int *)(iVar5 + 4) &&
        (((code *)*(uint **)(iVar5 + 0x1c) == pcStack_70 && (*(uint *)(iVar5 + 0x20) == uStack_34)))
        ))) {
      *(int *)(iVar5 + 4) = *(int *)(iVar5 + 4) + -1;
      FUN_100207e0();
      return 2;
    }
    iVar3 = (*(code *)0x6fa900)(&iStack_1c,&puStack_38);
    if (iVar3 != 0) {
      LOCK();
      DAT_103d4764 = DAT_103d4764 + 1;
      UNLOCK();
      *(undefined4 *)(iVar5 + 4) = 0;
      *(undefined4 *)(iVar5 + 8) = 0;
      *(undefined4 *)(iVar5 + 0x14) = 0;
      *(undefined4 *)(iVar5 + 0x18) = 0;
      return 1;
    }
    if ((*(int *)(iVar5 + 0x1c) != iStack_40) || (*(int *)(iVar5 + 0x20) != iStack_3c)) {
      *(int *)(iVar5 + 0x1c) = iStack_40;
      *(int *)(iVar5 + 0x20) = iStack_3c;
      *(undefined4 *)(iVar5 + 8) = 0;
      *(undefined4 *)(iVar5 + 4) = 0;
    }
    if ((char)uStack_5c != '\0') {
      if ((_DAT_008cf6d8 != 0) && (iVar3 = *(int *)(_DAT_008cf6d8 + 0x1c54), iVar3 != 0)) {
        iStack_4c = *(int *)(param_1 + 0x12);
        puStack_38 = (uint *)(iVar3 + 0x372c);
        pcStack_70 = (code *)(iVar3 + 0x3816);
        uVar7 = *(uint *)(param_1 + 0x5c);
        local_6c = 0;
        pcStack_50 = (code *)0x599a50;
        pcStack_78 = (code *)0x599e70;
        uStack_74 = uVar7;
        do {
          uStack_44 = *(uint *)pcStack_70;
          if ((uStack_44 != 0xffffffff) && (uStack_44 != uVar7)) {
            iStack_48 = 0;
            cVar1 = (*pcStack_50)(uStack_44,0,&iStack_48,0);
            uVar7 = uStack_74;
            if ((cVar1 == '\0') && (iStack_48 != 0)) {
              local_2c = (int)(*(int *)(iStack_48 + 6) + (*(int *)(iStack_48 + 6) >> 0x1f & 0xfU))
                         >> 4;
              uStack_34 = *(int *)(iStack_48 + 10) / 0xc;
              if (((((*(int *)(iStack_48 + 0x12) == iStack_4c) && (iStack_40 + -1 <= local_2c)) &&
                   (local_2c <= iStack_40 + 1)) &&
                  (((iStack_3c + -1 <= (int)uStack_34 && ((int)uStack_34 <= iStack_3c + 1)) &&
                   (*(int *)(iStack_48 + 0x5382) != 0)))) && (0 < *(short *)(iStack_48 + 0x5386))) {
                (*pcStack_78)(uStack_44,0,0xffffffff);
                uStack_34 = uStack_44;
                if (uStack_44 != 0) {
                  pcStack_78 = *(code **)(param_1 + 0x5c);
                  iVar6 = FUN_10023680(&local_2c,&uStack_34,
                                       ((((uStack_44 & 0xff ^ 0x811c9dc5) * 0x1000193 ^
                                         uStack_44 >> 8 & 0xff) * 0x1000193 ^
                                        uStack_44 >> 0x10 & 0xff) * 0x1000193 ^ uStack_44 >> 0x18) *
                                       0x1000193);
                  iVar3 = DAT_103d47b0;
                  if (*(int *)(iVar6 + 4) != 0) {
                    iVar3 = *(int *)(iVar6 + 4);
                  }
                  if ((iVar3 == DAT_103d47b0) || (*(code **)(iVar3 + 0x24) != pcStack_78)) {
                    uVar8 = 0;
LAB_100219ae:
                    if (*(int *)(iVar5 + 0x14) < 0x28) {
                      if ((*(int *)(iVar5 + 0x14) == 0) && (uStack_5c._1_1_ != '\0')) {
                        FUN_100203b0("ally-queue id=%d behind=%d cell=%d,%d",pcStack_78,uStack_44,
                                     iStack_40,iStack_3c);
                      }
                      *(int *)(iVar5 + 0x14) = *(int *)(iVar5 + 0x14) + 1;
                      *(uint *)(iVar5 + 0x18) = uStack_44;
                      *(undefined4 *)(iVar5 + 4) = 2;
                      LOCK();
                      DAT_103d47a4 = DAT_103d47a4 + 1;
                      UNLOCK();
                      FUN_100207e0();
                      return 2;
                    }
                  }
                  else {
                    uVar8 = 1;
                    if ((int)pcStack_78 <= (int)uStack_44) goto LAB_100219ae;
                  }
                  *(undefined4 *)(iVar5 + 0x18) = 0;
                  LOCK();
                  DAT_103d47a8 = DAT_103d47a8 + 1;
                  UNLOCK();
                  if (uStack_5c._1_1_ != '\0') {
                    FUN_100203b0("ally-queue-yield id=%d behind=%d mutual=%d holds=%d",pcStack_78,
                                 uStack_44,uVar8,*(undefined4 *)(iVar5 + 0x14));
                  }
                  goto LAB_10021a23;
                }
                break;
              }
              (*pcStack_78)(uStack_44,0,0xffffffff);
              uVar7 = uStack_74;
            }
          }
          pcStack_70 = (code *)((int)pcStack_70 + 4);
          local_6c = local_6c + 1;
        } while (local_6c < 6);
      }
      if (*(int *)(iVar5 + 0x18) != 0) {
        *(undefined4 *)(iVar5 + 0x18) = 0;
        *(undefined4 *)(iVar5 + 0x14) = 0;
      }
    }
LAB_10021a23:
    if ((((uStack_64 & 4) == 0) || (*(short *)(param_1 + 0x53c0) < *(short *)(param_1 + 0x5386))) ||
       (iVar3 = *(int *)(param_1 + 0x5366) - *(int *)(param_1 + 0x535e),
       iVar6 = *(int *)(param_1 + 0x5362) - *(int *)(param_1 + 0x535a),
       400 < iVar3 * iVar3 + iVar6 * iVar6)) {
      if (((uStack_64 & 2) == 0) || (1 < *(int *)(iVar5 + 8))) {
        *(undefined4 *)(iVar5 + 4) = 0;
        *(undefined4 *)(iVar5 + 8) = 0;
        LOCK();
        DAT_103d476c = DAT_103d476c + 1;
        UNLOCK();
        if (uStack_5c._1_1_ != '\0') {
          FUN_100203b0("revert-repath id=%d cell=%d,%d",*(undefined4 *)(param_1 + 0x5c),iStack_40,
                       iStack_3c);
        }
        return 3;
      }
      LOCK();
      DAT_103d4768 = DAT_103d4768 + 1;
      UNLOCK();
      iVar3 = rand();
      *(int *)(iVar5 + 8) = *(int *)(iVar5 + 8) + 1;
      *(int *)(iVar5 + 4) = iVar3 % 7 + 4;
      FUN_100207e0();
      if (uStack_5c._1_1_ != '\0') {
        FUN_100203b0("wait id=%d round=%d ticks=%d cell=%d,%d",*(undefined4 *)(param_1 + 0x5c),
                     *(undefined4 *)(iVar5 + 8),*(undefined4 *)(iVar5 + 4),iStack_40,iStack_3c);
      }
    }
    else {
      LOCK();
      DAT_103d4778 = DAT_103d4778 + 1;
      UNLOCK();
      FUN_100207e0();
      (*(code *)0x6ff2f0)();
      (*(code *)0x6ff320)();
      (*(code *)0x707170)();
      *(undefined4 *)(iVar5 + 4) = 0;
      *(undefined4 *)(iVar5 + 8) = 0;
      if (uStack_5c._1_1_ != '\0') {
        FUN_100203b0("arrived-stop id=%d",*(undefined4 *)(param_1 + 0x5c));
        return 2;
      }
    }
    return 2;
  }
  if (((char)local_60 != '\0') &&
     (((_DAT_008cf6d8 == 0 || (iVar3 = (*(code *)0x452b50)(), iVar3 == 0)) ||
      (*(int *)(iVar5 + 0x1084) < 2)))) {
    local_2c = (int)((*(int *)(param_1 + 6) >> 0x1f & 0xfU) + *(int *)(param_1 + 6)) >> 4;
    local_28 = *(int *)(param_1 + 10) / 0xc;
    pcStack_70 = (code *)0x547a00;
    cVar1 = (*(code *)0x547a00)(&local_2c,param_1 + 0x50bb,local_30,0);
    if (cVar1 == uStack_74._3_1_) {
      uStack_74 = CONCAT13(DAT_0084d6a3,(undefined3)uStack_74);
      iStack_24 = (int)((*(int *)(param_1 + 0x5362) >> 0x1f & 0xfU) + *(int *)(param_1 + 0x5362)) >>
                  4;
      iStack_20 = *(int *)(param_1 + 0x5366) / 0xc;
      cVar1 = (*pcStack_70)(&iStack_24,param_1 + 0x50bb,&iStack_3c,0);
      if (cVar1 != uStack_74._3_1_) {
        uStack_64 = *(uint *)(param_1 + 0x5382);
        if ((uStack_64 != 0) && (0 < *(short *)(param_1 + 0x5386))) {
          pcStack_70 = (code *)(int)*(short *)(param_1 + 0x53c0);
          pcVar4 = (code *)(*(short *)(param_1 + 0x5386) + -1);
          pcStack_68 = pcStack_70 + -2;
          if ((int)pcVar4 <= (int)pcStack_70) {
            pcStack_70 = pcVar4;
          }
          if ((int)pcStack_68 < 1) {
            pcStack_68 = (code *)0x0;
          }
          for (; (int)pcStack_68 <= (int)pcStack_70; pcStack_68 = pcStack_68 + 1) {
            iVar5 = *(int *)(uStack_64 + (int)pcStack_68 * 4);
            uVar9 = iVar5 % 0x140 - local_2c;
            uVar7 = (int)uVar9 >> 0x1f;
            if (((int)((uVar9 ^ uVar7) - uVar7) < 2) &&
               (uVar7 = (0x13f - iVar5 / 0x140) - local_28, uVar9 = (int)uVar7 >> 0x1f,
               (int)((uVar7 ^ uVar9) - uVar9) < 2)) goto LAB_100215f1;
            param_1 = local_6c;
          }
        }
        pcStack_70 = (code *)((int)(*(int *)(param_1 + 0x5362) +
                                   (*(int *)(param_1 + 0x5362) >> 0x1f & 0xfU)) >> 4);
        iVar5 = *(int *)(param_1 + 0x5366);
        LOCK();
        DAT_103d478c = DAT_103d478c + 1;
        UNLOCK();
        iVar3 = FUN_100204a0();
        *(undefined4 *)(iVar3 + 4) = 0;
        *(undefined4 *)(iVar3 + 8) = 0;
        if (uStack_54._1_1_ != '\0') {
          FUN_100203b0("static-block id=%d cell=%d,%d from=%d,%d",*(undefined4 *)(param_1 + 0x5c),
                       local_2c,local_28,pcStack_70,iVar5 / 0xc);
        }
        return 3;
      }
    }
LAB_100215f1:
    iVar5 = FUN_100204a0();
    if (((*(int *)(iVar5 + 4) != 0) || (*(int *)(iVar5 + 8) != 0)) || (*(int *)(iVar5 + 0x14) != 0))
    {
      *(undefined4 *)(iVar5 + 4) = 0;
      *(undefined4 *)(iVar5 + 8) = 0;
      *(undefined4 *)(iVar5 + 0x14) = 0;
      *(undefined4 *)(iVar5 + 0x18) = 0;
    }
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

`IEex_Helper_PF_CollisionPolicy` - 0x10021390 (FUN_10021390) -
stdcall, 2 stack args (sprite, the cell cost byte just read), callee
cleans 0x8; entered from the IEex_Pathfinding_Patch.lua cave at
engine 0x6F9CF7, which REPLACES the vanilla `cmp al,[0x84d6a3];
jne` entirely -- the helper sees the cost byte on both paths.

##### Behavior

The blocked-cell brain of the walk loop. Counted, logged, stateful;
returns one of four routes (0 blocked / 1 usable / 2 handled / 3
revert-skip):

1. `lock incl DAT_103d475c` (calls); COST_IMPASSABLE staged into a
   stack byte; config.
2. **Impassable cell** (param_2 == DAT_0084d6a3): count blocked
   (DAT_103d4760); master off OR multiplayer -> return 0 (vanilla
   blocked). `state = FUN_100204a0()`; decode the sprite's
   destination cell from +0x5362/+0x5366 and current cell from
   +6/+10 (>> 4 with the sign fix, / 0xc). Ally-queue wait consume
   (byte +12 "IP Ally Queue" OR flags & 2): if state+4 > 0 and the
   remembered blocked cell (+0x1c/+0x20) equals the CURRENT cell,
   decrement state+4, FUN_100207e0() (un-apply any re-aim), return
   2 (handled -- keep waiting in place). Engine clear-bump query
   `(*0x6fa900)(&point, &id)` nonzero -> count bumpClear
   (DAT_103d4764), zero state+4/+8/+0x14/+0x18, return 1 (usable --
   the engine itself will clear the bump). Otherwise update the
   remembered blocked cell and zero state+8/+4 when the cell
   changed.
3. **Ally queue** (byte +12): walk the area's six-slot object list
   at area +0x3816 (skip -1 and self), resolve each with
   `(*0x599a50)(id, 0, &sprite, 0)` / release `(*0x599e70)`;
   for each ally in the same area whose cell is within +-1 of ours
   and which has a live path (+0x5382 != 0, count +0x5386 > 0):
   FUN_10023680 state lookup, mutual test (their state's
   queued-other id +0x18 == our id == sprite +0x5c). Lower id
   queues, higher id yields: YIELD -> zero state+0x18, count
   allyQueueYield (DAT_103d47a8), log `"ally-queue-yield id=%d
   behind=%d mutual=%d holds=%d"`, fall to the decision; QUEUE ->
   while state+0x14 < 0x28 (40): log `"ally-queue id=%d behind=%d
   cell=%d,%d"` on the first iteration, state+0x14++, state+0x18 =
   ally id, state+4 = 2, count allyQueue (DAT_103d47a4),
   FUN_100207e0(), return 2. Queue full -> the yield path. After
   the loop, a stale state+0x18 != 0 clears +0x18/+0x14.
4. **Decision** (LAB_10021a23): en route (flags & 4 == 0, or
   waypoint < count, or the destination is still > 20 px away):
   if flags & 2 is clear OR state+8 > 1 -> zero state+4/+8, count
   revert (DAT_103d476c), log `"revert-repath id=%d cell=%d,%d"`,
   return 3 (revert-skip -- the walk aborts and repaths); else
   count wait (DAT_103d4768), state+8++, state+4 = rand() % 7 + 4,
   FUN_100207e0(), log `"wait id=%d round=%d ticks=%d cell=%d,%d"`,
   return 2. Near the target: count arrivedStop (DAT_103d4778),
   FUN_100207e0(), the engine stop trio, zero state+4/+8, log
   `"arrived-stop id=%d"`, return 2.
5. **Passable cell** (param_2 != COST_IMPASSABLE), master on and
   not multiplayer: read the cost of the CURRENT cell via 0x547a00;
   if the sprite is standing ON an impassable cell, scan its own
   path nodes from max(waypoint - 2, 0) -- if any own node decodes
   to a cell within +-1 of the current cell, the sprite is on its
   own route (LAB_100215f1: clear state+4/+8/+0x14/+0x18) and
   return 1 (usable); none -> count staticBlock (DAT_103d478c),
   zero state+4/+8, log `"static-block id=%d cell=%d,%d
   from=%d,%d"`, return 3.
6. Default (master off, passable cell) -> return 1 (usable --
   vanilla).

The cave routes eax: 0 -> 0x6F9D03 (vanilla blocked), 1 ->
0x6F9F22 (usable), 2 -> 0x6FA665 (handled), 3 -> 0x6F9D1C
(revert-skip).

##### Failure fallbacks

Master off / multiplayer: impassable -> 0 (vanilla), passable -> 1
(vanilla) -- the seam sits where the vanilla compare sat, so the
two answers reproduce it exactly. Ally resolution failures just
shrink the queue scan; the queue caps at 40 entries. No
allocation.

##### Decompiler artifact

Five, all resolved: (a) the `uStack_64` flags tests are stale
slot-reuse -- the real tests are the config flags dword (asm esp
+0x24 band); (b) the cell pair `iStack_40/iStack_3c` is consumed
with no visible store (an alias of the entry-computed blocked/dest
cells); (c) the final `return 2` inside the verbose `if` -- both
paths of that arm return 2; (d) the log gate shows as
`uStack_5c._1_1_` (asm byte +13, six sites 0x209a3..0x20f05 plus
the static stats block); (e) the ally-scan gate shows as
`(char)uStack_5c` (asm `cmpb $0x0,0x2c(%esp)` at 0x20ae2/0x20b95 =
byte +12). A `movups` self-copy at entry is the same duplicated
config spill as everywhere in the group.

##### Lua wiring

Lines 270-290, the second half of the ArrivalPolicy LabelDefault
block: VerifyBytes checks `3A 05 A3 D6 84 00 0F 85 1F 02 00 00` at
0x6F9CF7 (the vanilla compare + long jne); the cave pushes the
register-preserving set, `0F B6 C0` (movzx eax,al of the
GetMobileCost result) / push eax (cost) / push esi (this), `!call
>IEex_Helper_PF_CollisionPolicy`, pops, then the four-way dispatch
`cmp eax,1 / je :6F9F22 / cmp eax,2 / je :6FA665 / cmp eax,3 / je
:6F9D1C / jmp :6F9D03`; seam patched with `!jmp_dword` +
`repeat(7,!nop)`. Comment: "all four targets set their own flags at
entry."

