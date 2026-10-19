# IEex_Helper_CInfinity_FillHighlightPoly3d

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10027ac0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10027ac0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Render_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_10027ac0(int param_1,float param_2,undefined4 param_3,uint param_4)

{
  float fVar1;
  float fVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  bool bVar7;
  char cVar8;
  int iVar9;
  int iVar10;
  int in_ECX;
  int iVar11;
  int iVar12;
  int iVar13;
  float fVar14;
  int *piVar15;
  int *piVar16;
  bool bVar17;
  float fVar18;
  float fVar19;
  float fVar20;
  float fVar21;
  float fVar22;
  float fVar23;
  int iVar24;
  float local_338 [12];
  float afStack_308 [49];
  float afStack_244 [15];
  float afStack_208 [55];
  int aiStack_12c [74];

  if (param_1 == 0) {
    return;
  }
  if ((int)param_2 < 3) {
    return;
  }
  cVar8 = FUN_10027020();
  if (cVar8 == '\0') {
    return;
  }
  iVar11 = *(int *)(in_ECX + 0x90);
  iVar10 = *(int *)(in_ECX + 0x48);
  iVar24 = *(int *)(in_ECX + 0x94);
  iVar12 = *(int *)(in_ECX + 0x4c);
  fVar14 = 8.96831e-44;
  if ((int)param_2 < 0x41) {
    fVar14 = param_2;
  }
  iVar9 = 0;
  if (fVar14 != 0.0) {
    do {
      afStack_208[iVar9] = ((float)*(int *)(param_1 + iVar9 * 8) - (float)(iVar11 - iVar10)) + 0.2;
      afStack_308[iVar9] =
           ((float)*(int *)(param_1 + 4 + iVar9 * 8) - (float)(iVar24 - iVar12)) + 0.2;
      iVar9 = iVar9 + 1;
    } while (iVar9 < (int)fVar14);
  }
  local_338[0] = fVar14;
  (*DAT_104df040)(0xde1);
  (*DAT_104df5a4)(0xbe2);
  fVar23 = 1.079e-42;
  (*DAT_103ded84)(0x302,0x303);
  fVar18 = ((float)(param_4 >> 0x10 & 0xff) + 0.0) / 255.0;
  (*DAT_104df570)(((float)(param_4 & 0xff) + 0.0) / 255.0,
                  ((float)(param_4 >> 8 & 0xff) + 0.0) / 255.0,fVar18,0x3e99999a);
  iVar11 = 0;
  if (fVar14 == 0.0) {
LAB_10027cd6:
    iVar11 = 0;
    if (fVar14 == 0.0) goto LAB_10027d35;
  }
  else {
    if (0xf < (uint)fVar14) {
      iVar10 = 0;
      do {
        *(int *)((int)aiStack_12c + iVar10 + 4) = iVar11;
        *(int *)((int)aiStack_12c + iVar10 + 8) = iVar11 + 1;
        *(int *)((int)aiStack_12c + iVar10 + 0xc) = iVar11 + 2;
        *(int *)((int)aiStack_12c + iVar10 + 0x10) = iVar11 + 3;
        aiStack_12c[iVar11 + 5] = iVar11 + 4;
        aiStack_12c[iVar11 + 6] = iVar11 + 5;
        aiStack_12c[iVar11 + 7] = iVar11 + 6;
        aiStack_12c[iVar11 + 8] = iVar11 + 7;
        aiStack_12c[iVar11 + 9] = iVar11 + 8;
        aiStack_12c[iVar11 + 10] = iVar11 + 9;
        aiStack_12c[iVar11 + 0xb] = iVar11 + 10;
        aiStack_12c[iVar11 + 0xc] = iVar11 + 0xb;
        aiStack_12c[iVar11 + 0xd] = iVar11 + 0xc;
        aiStack_12c[iVar11 + 0xe] = iVar11 + 0xd;
        aiStack_12c[iVar11 + 0xf] = iVar11 + 0xe;
        aiStack_12c[iVar11 + 0x10] = iVar11 + 0xf;
        iVar11 = iVar11 + 0x10;
        iVar10 = iVar10 + 0x40;
      } while (iVar11 < (int)((uint)fVar14 & 0xfffffff0));
      fVar14 = fVar23;
      if ((int)fVar23 <= iVar11) goto LAB_10027cd6;
    }
    do {
      aiStack_12c[iVar11 + 1] = iVar11;
      iVar11 = iVar11 + 1;
    } while (iVar11 < (int)fVar14);
    iVar11 = 0;
  }
  do {
    iVar11 = iVar11 + 1;
  } while (iVar11 < (int)fVar14);
LAB_10027d35:
  (*DAT_104df5c8)(4);
  if (2 < (uint)fVar14) {
    iVar11 = 0x1000;
    iVar10 = (int)fVar14 - 1;
    while (0 < iVar11) {
      iVar9 = 0;
      iVar12 = iVar10 + -1;
      iVar24 = 1;
      while( true ) {
        piVar16 = aiStack_12c + iVar9;
        iVar3 = *piVar16;
        iVar4 = aiStack_12c[(iVar24 + iVar12) % (int)fVar14];
        fVar23 = afStack_244[iVar4 + 6];
        local_338[2] = local_338[iVar4 + 3];
        iVar5 = aiStack_12c[iVar24 % (int)fVar14];
        local_338[1] = local_338[iVar3 + 3];
        local_338[0] = afStack_244[iVar5 + 6] - fVar23;
        fVar1 = afStack_244[iVar3 + 6];
        fVar19 = (fVar1 - fVar23) * (local_338[iVar5 + 3] - local_338[2]) -
                 (local_338[1] - local_338[2]) * local_338[0];
        if ((char)((uint)fVar18 >> 0x18) == '\0') {
          bVar17 = fVar19 < 0.0;
        }
        else {
          bVar17 = 0.0 < fVar19;
        }
        if (bVar17) break;
LAB_1002801b:
        iVar9 = iVar9 + 1;
        iVar24 = iVar24 + 1;
        if ((int)fVar14 <= iVar9) {
          piVar16 = aiStack_12c + 2;
          iVar11 = 2;
          fVar18 = local_338[aiStack_12c[0] + 3];
          fVar23 = afStack_244[aiStack_12c[0] + 6];
          do {
            (*DAT_103defe8)(fVar23,fVar18,0);
            (*DAT_103defe8)(afStack_244[piVar16[-1] + 3],local_338[piVar16[-1]],0);
            (*DAT_103defe8)(afStack_244[*piVar16],local_338[*piVar16 + -3],0);
            piVar16 = piVar16 + 1;
            iVar11 = iVar11 + 1;
          } while (iVar11 < (int)fVar14);
          goto LAB_100280ef;
        }
      }
      iVar13 = 0;
      do {
        iVar6 = aiStack_12c[iVar13];
        fVar19 = fVar23;
        if (((iVar6 != iVar4) && (iVar6 != iVar3)) && (iVar6 != iVar5)) {
          fVar20 = afStack_244[iVar6 + 6];
          fVar2 = local_338[iVar6 + 3];
          fVar22 = (local_338[2] - local_338[1]) * (fVar20 - fVar1) -
                   (fVar2 - local_338[1]) * (afStack_244[iVar4 + 6] - fVar1);
          fVar19 = afStack_244[iVar4 + 6];
          fVar21 = (fVar20 - afStack_244[iVar5 + 6]) * (local_338[iVar3 + 3] - local_338[iVar5 + 3])
                   - (fVar2 - local_338[iVar5 + 3]) * (fVar1 - afStack_244[iVar5 + 6]);
          fVar20 = (fVar20 - fVar19) * (local_338[iVar5 + 3] - local_338[2]) -
                   (fVar2 - local_338[iVar4 + 3]) * local_338[0];
          if (((fVar22 < 0.0) || (fVar21 < 0.0)) || (fVar20 < 0.0)) {
            bVar17 = true;
          }
          else {
            bVar17 = false;
          }
          if (((0.0 < fVar22) || (0.0 < fVar21)) || (0.0 < fVar20)) {
            bVar7 = true;
          }
          else {
            bVar7 = false;
          }
          if ((!bVar17) || (!bVar7)) goto LAB_1002801b;
        }
        iVar13 = iVar13 + 1;
      } while (iVar13 < (int)fVar14);
      (*DAT_103defe8)(fVar19,local_338[2],0);
      (*DAT_103defe8)(afStack_244[(int)piVar16 + 3],local_338[(int)piVar16],0);
      (*DAT_103defe8)(afStack_244[(int)fVar18],local_338[(int)fVar18 + -3],0);
      if (iVar9 < iVar10) {
        piVar15 = aiStack_12c + iVar9 + 1;
        for (iVar10 = iVar10 - iVar9; iVar10 != 0; iVar10 = iVar10 + -1) {
          *piVar16 = *piVar15;
          piVar15 = piVar15 + 1;
          piVar16 = piVar16 + 1;
        }
      }
      fVar14 = (float)((int)fVar14 - 1);
      iVar11 = iVar11 + -1;
      iVar10 = iVar12;
      if ((int)fVar14 < 3) break;
    }
  }
LAB_100280ef:
  (*DAT_104df208)();
  (*DAT_104df040)(0xbe2);
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

`IEex_Helper_CInfinity_FillHighlightPoly3d` - 0x10027ac0
(FUN_10027ac0) - forwarded __thiscall (ECX = CInfinity* preserved by
the stub), 4 stack args (LPPOINT pts, int count, unused word,
COLORREF colour), export cleans its own args (ret 0x10 per the patch
stub design); entered from the IEex_Render_Patch.lua `fillBlock`
stubs before every CInfinity::OutlinePoly call.

##### Behavior

Draws the translucent polygon fill behind sprite/door/trigger
outlines. The patch comments (untrusted, attributed): the software
renderer's fill is `CGame*::RenderClippedPoly`, compiled out in 3D
because `CVidInf::FXPrep`'s 3D branch cannot COPYFROMBACK; the helper
re-creates it in immediate-mode GL. Hooking immediately BEFORE each
OutlinePoly means the fill reuses the same polygon and colour,
including the empty-container gray override.

1. Gates: pts != 0, count >= 3, FUN_10027020() resolver success;
   otherwise return with nothing drawn.
2. Reads the viewport transform from the CInfinity `this`: fields
   +0x90 / +0x48 (x) and +0x94 / +0x4c (y). Per vertex
   `x' = x - (v0x90 - v0x48) + 0.2`, same for y (the +0.2 nudge is the
   same idea as LightningPolyLine3d's).
3. Vertex cap 64: when `(int)count > 64` only the first 64 points are
   used (the constant appears in the decompile as the float-bits leak
   8.96831e-44 = 64).
4. GL state: glDisable(GL_TEXTURE_2D 0xde1) via `DAT_104df040`;
   glEnable(GL_BLEND 0xbe2) via `DAT_104df5a4`; glBlendFunc(GL_SRC_ALPHA
   0x302, GL_ONE_MINUS_SRC_ALPHA 0x303) via `DAT_103ded84`;
   glColor4f(R, G, B, 0.3) with the COLORREF bytes (0x00BBGGRR) divided
   by 255 and a fixed 0.3 alpha; glBegin(GL_TRIANGLES 4) via
   `DAT_104df5c8`.
5. Ear-clipping triangulation over the point ring: for each ring
   triple (prev, cur, next) compute the cross product; if the turn is
   convex (sign test -- see artifacts), test every other vertex for
   containment in that triangle with three signed-area comparisons; an
   empty convex ear is emitted as a triangle (three glVertex3f(x, y, 0)
   via `DAT_103defe8`) and removed from the ring (shift-down);
   iteration guard 0x1000. If the guard exhausts or no ear is found,
   the remaining polygon is emitted as a fan from ring[0].
6. Tail: glEnd via `DAT_104df208`; glDisable(GL_BLEND) via
   `DAT_104df040`. GL_TEXTURE_2D is NOT re-enabled and the blend
   function is not restored -- the engine's OutlinePoly that follows
   sets whatever state it needs; anything drawn after must do the same.

##### Failure fallbacks

Null points, fewer than 3 vertices, or a failed GL resolver: the
helper returns before touching GL state, and the engine's OutlinePoly
still runs (the hook is before it) -- the visual degrades to outline
only, exactly like stock 3D mode.

##### Decompiler artifact

The triangulation data structures are heavily aliased in the
decompile: three arrays are one scratch block (`local_338[0]` holds
the count, `local_338[i+3]` the x_i, `afStack_244[i+6]` the y_i), and
the identity index-ring fill in blocks of 16 is a dual alias of the
same memory. The convexity sign selector reads as
`(char)((uint)fVar18 >> 0x18)` where fVar18 is the blue channel float
-- a stack-slot reuse alias, not real logic (the compiled code reuses
the slot; treat both windings as supported). `fVar23 = 1.079e-42` is
int 770 = 0x302 (GL_SRC_ALPHA) leaked into a float slot. `count` is
printed as `float param_2` because of the float comparisons.

##### Lua wiring

`fillBlock` = push_all_registers_iwd2, 4x `push([esp+0x28])`,
`call >IEex_Helper_CInfinity_FillHighlightPoly3d`,
pop_all_registers_iwd2 -- duplicating OutlinePoly's 4 stack args
(pPoly, nVerts, rClip, colour) with ECX (pInfinity) left standing;
the export cleans them. Gate
`IEex_GetPrivateProfileInt("Program Options", "3D Acceleration", 1,
".\\Icewind2.ini") ~= 0`. Hook sites (IEex_HookBeforeCall, all
immediately before an OutlinePoly call): 0x47FB76 (mouse hover
outline), 0x47FD52 (alt-key down outline), 0x47FDF2 (script-flash
red), 0x488CA3 / 0x488D34 / 0x488E76 / 0x488F1C / 0x48929D (door
highlights), 0x4CFC10 (trigger/info-point). The software-path fill
sites 0x47FB41 / 0x47FC2A / 0x47FD1C receive only the colour override
(HookRestore), not the fill.

