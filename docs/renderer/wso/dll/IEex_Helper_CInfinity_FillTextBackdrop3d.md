# IEex_Helper_CInfinity_FillTextBackdrop3d

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10028110` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10028110.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Render_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_10028110(undefined4 param_1,int *param_2)

{
  uint uVar1;
  uint uVar2;
  uint *puVar3;
  uint uVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  int iVar8;
  int iVar9;
  uint uVar10;
  uint uVar11;
  uint uVar12;
  uint uVar13;
  int iVar14;
  int iVar15;
  int iVar16;
  uint uVar17;
  uint uVar18;
  uint uVar19;
  uint uVar20;
  uint uVar21;
  uint uVar22;
  uint uVar23;
  uint uVar24;
  uint uVar25;
  uint uVar26;
  uint uStack_90;
  uint uStack_8c;
  uint uStack_88;
  uint uStack_84;
  uint uStack_80;
  uint uStack_7c;
  uint uStack_78;
  uint uStack_74;
  uint uStack_70;
  uint uStack_6c;
  uint uStack_68;
  uint uStack_64;
  uint uStack_60;
  uint uStack_5c;
  uint uStack_58;
  uint uStack_54;
  uint uStack_50;
  uint uStack_4c;
  uint uStack_48;
  uint uStack_44;
  int iStack_2c;
  uint *puStack_28;
  uint uStack_24;
  uint uStack_18;
  uint uStack_14;

  if (((param_2 != (int *)0x0) && (uVar4 = param_2[2] - *param_2, 0 < (int)uVar4)) &&
     (uStack_14 = param_2[3] - param_2[1], 0 < (int)uStack_14)) {
    uVar6 = uVar4;
    if (*(int *)(*DAT_100944b8 + 0x924) != 0) {
      uVar6 = 0x200;
    }
    if ((int)uVar6 < (int)uVar4) {
      uVar4 = uVar6;
    }
    uVar7 = 0xffffffff;
    uStack_18 = 0xffffffff;
    if (0x200 < (int)uStack_14) {
      uStack_14 = 0x200;
    }
    uVar1 = uStack_14;
    uStack_24 = 0;
    iStack_2c = 0x241ec8;
    puStack_28 = (uint *)&DAT_00907b20;
    uStack_70 = uVar4;
    do {
      uVar2 = 0;
      if (0 < (int)uVar4) {
        if ((7 < uVar4) && (1 < DAT_10094134)) {
          uVar5 = uVar4 & 0x80000007;
          if ((int)uVar5 < 0) {
            uVar5 = (uVar5 - 1 | 0xfffffff8) + 1;
          }
          uStack_80 = uStack_14;
          uStack_7c = uStack_14;
          uStack_78 = uStack_14;
          uStack_74 = uStack_14;
          uStack_90 = uStack_18;
          uStack_8c = uStack_18;
          uStack_88 = uStack_18;
          uStack_84 = uStack_18;
          uVar19 = uVar2;
          puVar3 = puStack_28;
          uVar20 = uStack_18;
          uVar13 = uStack_18;
          uVar18 = uStack_18;
          uVar21 = uVar7;
          uVar22 = uVar7;
          uVar23 = uVar7;
          uVar24 = uStack_14;
          uVar25 = uStack_14;
          uVar26 = uStack_14;
          uStack_6c = uStack_70;
          uStack_68 = uStack_70;
          uStack_64 = uStack_70;
          uStack_60 = uStack_70;
          uStack_5c = uStack_70;
          uStack_58 = uStack_70;
          uStack_54 = uStack_70;
          uStack_50 = uVar7;
          uStack_4c = uVar7;
          uStack_48 = uVar7;
          uStack_44 = uVar7;
          do {
            iVar9 = uVar19 + 4;
            uVar2 = *puVar3 >> 0x18;
            uVar10 = puVar3[1] >> 0x18;
            uVar11 = puVar3[2] >> 0x18;
            uVar12 = puVar3[3] >> 0x18;
            uVar17 = -(uint)(((uint)(uVar2 < 0x18) * 0x18 | (uVar2 >= 0x18) * uVar2) == uVar2);
            uVar10 = -(uint)(((uint)(uVar10 < 0x18) * 0x18 | (uVar10 >= 0x18) * uVar10) == uVar10);
            uVar11 = -(uint)(((uint)(uVar11 < 0x18) * 0x18 | (uVar11 >= 0x18) * uVar11) == uVar11);
            uVar12 = -(uint)(((uint)(uVar12 < 0x18) * 0x18 | (uVar12 >= 0x18) * uVar12) == uVar12);
            uVar2 = uVar19 + 8;
            iVar14 = uVar19 + 1;
            iVar15 = uVar19 + 2;
            iVar16 = uVar19 + 3;
            uStack_60 = (((int)uVar19 < (int)uStack_60) * uVar19 |
                        ((int)uVar19 >= (int)uStack_60) * uStack_60) & uVar17 | ~uVar17 & uStack_60;
            uStack_5c = ((uint)(iVar14 < (int)uStack_5c) * iVar14 |
                        (iVar14 >= (int)uStack_5c) * uStack_5c) & uVar10 | ~uVar10 & uStack_5c;
            uStack_58 = ((uint)(iVar15 < (int)uStack_58) * iVar15 |
                        (iVar15 >= (int)uStack_58) * uStack_58) & uVar11 | ~uVar11 & uStack_58;
            uStack_54 = ((uint)(iVar16 < (int)uStack_54) * iVar16 |
                        (iVar16 >= (int)uStack_54) * uStack_54) & uVar12 | ~uVar12 & uStack_54;
            uVar7 = (((int)uVar7 < (int)uVar19) * uVar19 | ((int)uVar7 >= (int)uVar19) * uVar7) &
                    uVar17 | ~uVar17 & uVar7;
            uVar21 = ((uint)((int)uVar21 < iVar14) * iVar14 | ((int)uVar21 >= iVar14) * uVar21) &
                     uVar10 | ~uVar10 & uVar21;
            uVar22 = ((uint)((int)uVar22 < iVar15) * iVar15 | ((int)uVar22 >= iVar15) * uVar22) &
                     uVar11 | ~uVar11 & uVar22;
            uVar23 = ((uint)((int)uVar23 < iVar16) * iVar16 | ((int)uVar23 >= iVar16) * uVar23) &
                     uVar12 | ~uVar12 & uVar23;
            uStack_14 = (((int)uStack_24 < (int)uStack_14) * uStack_24 |
                        ((int)uStack_24 >= (int)uStack_14) * uStack_14) & uVar17 |
                        ~uVar17 & uStack_14;
            uVar24 = (((int)uStack_24 < (int)uVar24) * uStack_24 |
                     ((int)uStack_24 >= (int)uVar24) * uVar24) & uVar10 | ~uVar10 & uVar24;
            uVar25 = (((int)uStack_24 < (int)uVar25) * uStack_24 |
                     ((int)uStack_24 >= (int)uVar25) * uVar25) & uVar11 | ~uVar11 & uVar25;
            uVar26 = (((int)uStack_24 < (int)uVar26) * uStack_24 |
                     ((int)uStack_24 >= (int)uVar26) * uVar26) & uVar12 | ~uVar12 & uVar26;
            uStack_18 = (((int)uStack_18 < (int)uStack_24) * uStack_24 |
                        ((int)uStack_18 >= (int)uStack_24) * uStack_18) & uVar17 |
                        ~uVar17 & uStack_18;
            uVar20 = (((int)uVar20 < (int)uStack_24) * uStack_24 |
                     ((int)uVar20 >= (int)uStack_24) * uVar20) & uVar10 | ~uVar10 & uVar20;
            uVar13 = (((int)uVar13 < (int)uStack_24) * uStack_24 |
                     ((int)uVar13 >= (int)uStack_24) * uVar13) & uVar11 | ~uVar11 & uVar13;
            uVar18 = (((int)uVar18 < (int)uStack_24) * uStack_24 |
                     ((int)uVar18 >= (int)uStack_24) * uVar18) & uVar12 | ~uVar12 & uVar18;
            uVar10 = puVar3[4] >> 0x18;
            uVar11 = puVar3[5] >> 0x18;
            uVar12 = puVar3[6] >> 0x18;
            uVar17 = puVar3[7] >> 0x18;
            uVar10 = -(uint)(((uint)(uVar10 < 0x18) * 0x18 | (uVar10 >= 0x18) * uVar10) == uVar10);
            uVar11 = -(uint)(((uint)(uVar11 < 0x18) * 0x18 | (uVar11 >= 0x18) * uVar11) == uVar11);
            uVar12 = -(uint)(((uint)(uVar12 < 0x18) * 0x18 | (uVar12 >= 0x18) * uVar12) == uVar12);
            uVar17 = -(uint)(((uint)(uVar17 < 0x18) * 0x18 | (uVar17 >= 0x18) * uVar17) == uVar17);
            iVar14 = uVar19 + 5;
            iVar15 = uVar19 + 6;
            iVar16 = uVar19 + 7;
            uStack_70 = ((uint)(iVar9 < (int)uStack_70) * iVar9 |
                        (iVar9 >= (int)uStack_70) * uStack_70) & uVar10 | ~uVar10 & uStack_70;
            uStack_6c = ((uint)(iVar14 < (int)uStack_6c) * iVar14 |
                        (iVar14 >= (int)uStack_6c) * uStack_6c) & uVar11 | ~uVar11 & uStack_6c;
            uStack_68 = ((uint)(iVar15 < (int)uStack_68) * iVar15 |
                        (iVar15 >= (int)uStack_68) * uStack_68) & uVar12 | ~uVar12 & uStack_68;
            uStack_64 = ((uint)(iVar16 < (int)uStack_64) * iVar16 |
                        (iVar16 >= (int)uStack_64) * uStack_64) & uVar17 | ~uVar17 & uStack_64;
            uStack_50 = ((uint)((int)uStack_50 < iVar9) * iVar9 |
                        ((int)uStack_50 >= iVar9) * uStack_50) & uVar10 | ~uVar10 & uStack_50;
            uStack_4c = ((uint)((int)uStack_4c < iVar14) * iVar14 |
                        ((int)uStack_4c >= iVar14) * uStack_4c) & uVar11 | ~uVar11 & uStack_4c;
            uStack_48 = ((uint)((int)uStack_48 < iVar15) * iVar15 |
                        ((int)uStack_48 >= iVar15) * uStack_48) & uVar12 | ~uVar12 & uStack_48;
            uStack_44 = ((uint)((int)uStack_44 < iVar16) * iVar16 |
                        ((int)uStack_44 >= iVar16) * uStack_44) & uVar17 | ~uVar17 & uStack_44;
            uStack_80 = (((int)uStack_24 < (int)uStack_80) * uStack_24 |
                        ((int)uStack_24 >= (int)uStack_80) * uStack_80) & uVar10 |
                        ~uVar10 & uStack_80;
            uStack_7c = (((int)uStack_24 < (int)uStack_7c) * uStack_24 |
                        ((int)uStack_24 >= (int)uStack_7c) * uStack_7c) & uVar11 |
                        ~uVar11 & uStack_7c;
            uStack_78 = (((int)uStack_24 < (int)uStack_78) * uStack_24 |
                        ((int)uStack_24 >= (int)uStack_78) * uStack_78) & uVar12 |
                        ~uVar12 & uStack_78;
            uStack_74 = (((int)uStack_24 < (int)uStack_74) * uStack_24 |
                        ((int)uStack_24 >= (int)uStack_74) * uStack_74) & uVar17 |
                        ~uVar17 & uStack_74;
            uStack_90 = (((int)uStack_90 < (int)uStack_24) * uStack_24 |
                        ((int)uStack_90 >= (int)uStack_24) * uStack_90) & uVar10 |
                        ~uVar10 & uStack_90;
            uStack_8c = (((int)uStack_8c < (int)uStack_24) * uStack_24 |
                        ((int)uStack_8c >= (int)uStack_24) * uStack_8c) & uVar11 |
                        ~uVar11 & uStack_8c;
            uStack_88 = (((int)uStack_88 < (int)uStack_24) * uStack_24 |
                        ((int)uStack_88 >= (int)uStack_24) * uStack_88) & uVar12 |
                        ~uVar12 & uStack_88;
            uStack_84 = (((int)uStack_84 < (int)uStack_24) * uStack_24 |
                        ((int)uStack_84 >= (int)uStack_24) * uStack_84) & uVar17 |
                        ~uVar17 & uStack_84;
            uVar19 = uVar2;
            puVar3 = puVar3 + 8;
          } while ((int)uVar2 < (int)(uVar4 - uVar5));
          uVar10 = ((int)uVar7 < (int)uStack_50) * uStack_50 |
                   ((int)uVar7 >= (int)uStack_50) * uVar7;
          uVar21 = ((int)uVar21 < (int)uStack_4c) * uStack_4c |
                   ((int)uVar21 >= (int)uStack_4c) * uVar21;
          uVar22 = ((int)uVar22 < (int)uStack_48) * uStack_48 |
                   ((int)uVar22 >= (int)uStack_48) * uVar22;
          uVar23 = ((int)uVar23 < (int)uStack_44) * uStack_44 |
                   ((int)uVar23 >= (int)uStack_44) * uVar23;
          uVar7 = ((int)uStack_18 < (int)uStack_90) * uStack_90 |
                  ((int)uStack_18 >= (int)uStack_90) * uStack_18;
          uVar5 = ((int)uVar20 < (int)uStack_8c) * uStack_8c |
                  ((int)uVar20 >= (int)uStack_8c) * uVar20;
          uVar19 = ((int)uVar13 < (int)uStack_88) * uStack_88 |
                   ((int)uVar13 >= (int)uStack_88) * uVar13;
          uVar20 = ((int)uVar18 < (int)uStack_84) * uStack_84 |
                   ((int)uVar18 >= (int)uStack_84) * uVar18;
          uVar7 = ((int)uVar7 < (int)uVar19) * uVar19 | ((int)uVar7 >= (int)uVar19) * uVar7;
          uVar18 = ((int)uVar5 < (int)uVar20) * uVar20 | ((int)uVar5 >= (int)uVar20) * uVar5;
          uVar5 = ((int)uStack_70 < (int)uStack_60) * uStack_70 |
                  ((int)uStack_70 >= (int)uStack_60) * uStack_60;
          uVar19 = ((int)uStack_6c < (int)uStack_5c) * uStack_6c |
                   ((int)uStack_6c >= (int)uStack_5c) * uStack_5c;
          uVar20 = ((int)uStack_68 < (int)uStack_58) * uStack_68 |
                   ((int)uStack_68 >= (int)uStack_58) * uStack_58;
          uVar13 = ((int)uStack_64 < (int)uStack_54) * uStack_64 |
                   ((int)uStack_64 >= (int)uStack_54) * uStack_54;
          uStack_18 = ((int)uVar7 < (int)uVar18) * uVar18 | ((int)uVar7 >= (int)uVar18) * uVar7;
          uVar7 = ((int)uVar10 < (int)uVar22) * uVar22 | ((int)uVar10 >= (int)uVar22) * uVar10;
          uVar18 = ((int)uVar21 < (int)uVar23) * uVar23 | ((int)uVar21 >= (int)uVar23) * uVar21;
          uVar7 = ((int)uVar7 < (int)uVar18) * uVar18 | ((int)uVar7 >= (int)uVar18) * uVar7;
          uVar18 = ((int)uStack_80 < (int)uStack_14) * uStack_80 |
                   ((int)uStack_80 >= (int)uStack_14) * uStack_14;
          uVar21 = ((int)uStack_7c < (int)uVar24) * uStack_7c |
                   ((int)uStack_7c >= (int)uVar24) * uVar24;
          uVar22 = ((int)uStack_78 < (int)uVar25) * uStack_78 |
                   ((int)uStack_78 >= (int)uVar25) * uVar25;
          uVar23 = ((int)uStack_74 < (int)uVar26) * uStack_74 |
                   ((int)uStack_74 >= (int)uVar26) * uVar26;
          uVar18 = ((int)uVar22 < (int)uVar18) * uVar22 | ((int)uVar22 >= (int)uVar18) * uVar18;
          uVar21 = ((int)uVar23 < (int)uVar21) * uVar23 | ((int)uVar23 >= (int)uVar21) * uVar21;
          uStack_14 = ((int)uVar21 < (int)uVar18) * uVar21 | ((int)uVar21 >= (int)uVar18) * uVar18;
          uVar5 = ((int)uVar20 < (int)uVar5) * uVar20 | ((int)uVar20 >= (int)uVar5) * uVar5;
          uVar19 = ((int)uVar13 < (int)uVar19) * uVar13 | ((int)uVar13 >= (int)uVar19) * uVar19;
          uStack_70 = ((int)uVar19 < (int)uVar5) * uVar19 | ((int)uVar19 >= (int)uVar5) * uVar5;
          if ((int)uVar4 <= (int)uVar2) goto LAB_1002843c;
        }
        do {
          if (0x17ffffff < (*(uint *)((iStack_2c + uVar2) * 4) & 0xff000000)) {
            uVar5 = uVar2;
            if ((int)uStack_70 <= (int)uVar2) {
              uVar5 = uStack_70;
            }
            uVar19 = uVar2;
            if ((int)uVar2 <= (int)uVar7) {
              uVar19 = uVar7;
            }
            if ((int)uStack_24 < (int)uStack_14) {
              uStack_14 = uStack_24;
            }
            uVar7 = uVar19;
            uStack_70 = uVar5;
            if ((int)uStack_18 < (int)uStack_24) {
              uStack_18 = uStack_24;
            }
          }
          uVar2 = uVar2 + 1;
        } while ((int)uVar2 < (int)uVar4);
      }
LAB_1002843c:
      uStack_24 = uStack_24 + 1;
      puStack_28 = puStack_28 + uVar6;
      iStack_2c = iStack_2c + uVar6;
    } while ((int)uStack_24 < (int)uVar1);
    if (-1 < (int)uVar7) {
      iVar9 = 0;
      if (-1 < (int)(uStack_70 - 3)) {
        iVar9 = uStack_70 - 3;
      }
      iVar14 = uVar4 - 1;
      if ((int)(uVar7 + 3) <= (int)(uVar4 - 1)) {
        iVar14 = uVar7 + 3;
      }
      iVar15 = uVar1 - 1;
      if ((int)(uStack_18 + 3) <= iVar15) {
        iVar15 = uStack_18 + 3;
      }
      iVar16 = 0;
      if (-1 < (int)(uStack_14 - 3)) {
        iVar16 = uStack_14 - 3;
      }
      if (iVar16 <= iVar15) {
        iVar8 = (iVar15 - iVar16) + 1;
        iVar15 = iVar16 * uVar6 + iVar9;
        do {
          if (iVar9 <= iVar14) {
            puVar3 = (uint *)(&DAT_00907b20 + iVar15 * 4);
            iVar16 = (iVar14 - iVar9) + 1;
            do {
              if (*puVar3 < 0x1000000) {
                *puVar3 = 0x60000000;
              }
              puVar3 = puVar3 + 1;
              iVar16 = iVar16 + -1;
            } while (iVar16 != 0);
          }
          iVar15 = uVar6 + iVar15;
          iVar8 = iVar8 + -1;
        } while (iVar8 != 0);
      }
    }
  }
  return;
}
```

## Notes

### Shared 3D-overlay machinery (all helpers in this group)

The staged `IEex_Render_Patch.lua` restores four engine visuals that
the software renderer draws but the 3D/GL path lost: the sprite
highlight fill, the floating-text readability panel, the lightning
bolt, and the greyed-out palette drop. The pattern is always the same:
a Lua stub forwards the engine call site's own stack arguments to an
IEex_Helper export, which either draws in immediate-mode GL or edits
the engine's staging data, and the engine's original code then runs
(or has already run) unchanged. The patch installs nothing when
`[Program Options] "3D Acceleration"` in `.\Icewind2.ini` is 0
(software renderer).

#### GL entry points (16-proc resolver, inner FUN_10027020)

All GL calls in this group go through the resolver FUN_10027020:
GetModuleHandleA("opengl32.dll") + 16 GetProcAddress calls; the gate
returns success only when all sixteen slots are non-null (its result
is checked at every helper's top). Slot table:

| Slot | Function |
|---|---|
| DAT_104df5a4 | glEnable |
| DAT_104df040 | glDisable |
| DAT_103ded84 | glBlendFunc |
| DAT_104df0e8 | glBindTexture |
| DAT_104df5fc | glTexImage2D |
| DAT_104df064 | glTexSubImage2D |
| DAT_103dec00 | glTexParameteri |
| DAT_104df0cc | glTexParameterfv |
| DAT_104df0c8 | glTexEnvf |
| DAT_104df570 | glColor4f |
| DAT_104df5c8 | glBegin |
| DAT_104df208 | glEnd |
| DAT_104df018 | glTexCoord2f |
| DAT_103defe8 | glVertex3f |
| DAT_103defd4 | glGenTextures |
| DAT_104df004 | glIsTexture |

These addresses are the resolver's own table and intentionally do not
match the six-slot (ui-scale group) or 17-slot (tile-atlas group)
tables -- each fork subsystem resolves its own.

#### Engine addresses and the registered original

`DAT_100942c0` holds the engine's original
`CVidPalette::RealizeResource3d` (0x7D6240 per the patch comments). It
is registered at runtime by the Lua patch itself:
`IEex_Helper_DefineAddress("CVidPalette::RealizeResource3dOriginal",
IEex_Label(...))` after the
`IEex_HookReplaceFunctionMaintainOriginal` call -- this is the
loader's public address-list mechanism (Bubb13/IEexLoader,
IEexDLL/IEexLua.asm: IEex_Init / IEex_AddressList; the fork reads
registered engine addresses out of its BSS slots at runtime). All
other engine addresses named below (hook sites, OutlinePoly, PolyLine,
RenderLightning, CVideo3d::texImageData) come from the staged patch's
comments -- untrusted, attributed, never verified against the engine
binary in this stream.

#### Threads and synchronization

Everything in this group runs on the engine's render thread inside the
frame (hook sites in the render call stack). No locks, no atomics, no
worker threads. RealizeResource3dGrey's palette scratch lives in TLS,
so even re-entrant realizes cannot collide.

#### Failure model

Every helper degrades silently: gate failure (missing GL procs, null
pointer, degenerate input) means "do nothing" and the engine's own
code path continues. The Lua stubs do not check results either -- the
helpers are fire-and-forget visual additions on top of an engine that
already drew (or will draw) its own version of the effect.

`IEex_Helper_CInfinity_FillTextBackdrop3d` - 0x10028110
(FUN_10028110) - stdcall, 7 stack args (the forwarded FXBltFrom
argument list; export rets 0x1C); only arg 2 = `int* rect`
{x0, y0, x1, y1} is read, arg 1 and args 3-7 are unused; ECX in the
stub is preserved for the engine's own FXBltFrom, which still runs
after; entered from the IEex_Render_Patch.lua `backdropBlock` stub at
the single CGameText::Render FXBltFrom call site 0x4CC0AF.

##### Behavior

Puts the semi-transparent readability panel behind floating combat /
selection text in 3D mode. The patch comments (untrusted,
attributed): software mode composites float text through the FX
scratch surface with FXPREP_COPYFROMBACK, producing a translucent
backdrop; 3D mode lost that, so the helper edits the FX staging
buffer -- `CVideo3d::texImageData`, the engine static RGBA8 buffer at
0x907B20 (address from the patch comment) -- directly, before the
engine's own FXBltFrom blits it to the back buffer. It makes no GL
calls at all.

1. Gates: rect != 0, width = rect[2]-rect[0] > 0, height =
   rect[3]-rect[1] > 0; otherwise return without touching the buffer.
2. Scan caps: width capped to 0x200 (512) only when
   `*(int *)(*DAT_100944b8 + 0x924) != 0` (a field of the UI/input
   root observed only here; see COMMON for the root pointer), height
   always capped to 0x200. Row pitch is the clamped width (the scan
   assumes the staging rect is tightly packed).
3. Bounding-box pass over the rect's pixels: a pixel is "glyph" when
   its alpha byte (dword >> 24) is >= 0x18 (24). Trackers init to
   min-x = clamped width, max-x = -1, min-y = clamped height,
   max-y = -1. When width > 7 and `DAT_10094134 > 1` an unrolled
   8-pixel x2 fast path runs; the scalar tail tests
   `0x17ffffff < (dword & 0xff000000)` -- the same alpha >= 24
   criterion (the fast path's expression is max(alpha, 0x18) == alpha).
4. Post-pass: when max-x >= 0 (anything marked), expand the box by
   3 pixels per side (clamped to the scan area) and overwrite every
   pixel in it whose value is < 0x1000000 (alpha byte 0) with
   0x60000000 -- black at alpha 0x60 (96 = 37.5%): the panel.
5. Return; the engine's FXBltFrom (whose arguments the stub forwarded)
   then composites the modified staging buffer as usual.

##### Failure fallbacks

Null/empty rect: no-op, glyphs draw with no panel (stock 3D look).
Fully transparent staging data (no glyphs yet): max-x stays -1 and
the post-pass is skipped, so the buffer is left untouched.

##### Decompiler artifact

The staging buffer is addressed twice -- `puStack_28 = &DAT_00907b20`
(uint* walk) and `iStack_2c = 0x241ec8` scaled by 4 (0x241EC8 * 4 =
0x907B20) -- one buffer, two bases; do not read them as two surfaces.
The unrolled fast path's tracker updates fan out into a dozen shadow
variables that are max-reduced afterwards; the scalar tail shows the
plain semantics. `DAT_10094134` has no writer anywhere in the seed
set (grep-verified) -- a CRT/CPU-capability heuristic flag; both
paths compute the identical box.

##### Lua wiring

`backdropBlock` = push_all_registers_iwd2, 7x `push([esp+0x34])`,
`call >IEex_Helper_CInfinity_FillTextBackdrop3d`,
pop_all_registers_iwd2 -- forwarding FXBltFrom's 7 stack args
__thiscall (ECX preserved for the engine's own call), export rets
0x1C. Single hook: `IEex_HookBeforeCall(0x4CC0AF, backdropBlock)` --
the CGameText::Render FXBltFrom site, after FXTextOut has rasterised
the glyphs. 3D only.

