# IEex_Helper_CGameText_RenderHD

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002b6a0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002b6a0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_SupersampleFont_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1002b6a0(undefined4 param_1,undefined4 param_2,undefined4 param_3)

{
  undefined8 uVar1;
  char cVar2;
  short sVar3;
  int *piVar4;
  uint uVar5;
  int iVar6;
  UINT UVar7;
  int in_ECX;
  uint uVar8;
  float fVar9;
  double dVar10;
  undefined4 uVar11;
  undefined4 uVar12;
  undefined4 uVar13;
  undefined4 uVar14;
  undefined4 uVar15;
  undefined1 auStack_b4 [4];
  undefined4 uStack_b0;
  float fStack_a8;
  float fStack_a4;
  undefined4 uStack_a0;
  int *local_9c;
  int iStack_98;
  float fStack_94;
  float local_90;
  float local_8c;
  float local_88;
  undefined4 *puStack_84;
  undefined1 local_80 [8];
  undefined8 local_78;
  int local_70;
  undefined8 uStack_6c;
  int iStack_64;
  int local_60;
  int local_5c;
  int *local_58;
  undefined4 uStack_54;
  undefined4 uStack_50;
  undefined4 uStack_4c;
  undefined4 uStack_48;
  uint local_44;

  local_44 = DAT_1008f040 ^ (uint)auStack_b4;
  uStack_6c._4_4_ = param_2;
  fVar9 = *(float *)(in_ECX + 0x12);
  local_70 = in_ECX + 0x148;
  local_90 = fVar9;
  local_78 = CONCAT44(local_78._4_4_,(float)local_78);
  if (((fVar9 != 0.0) && (local_78 = CONCAT44(local_78._4_4_,(float)local_78), DAT_103d45f8 != 0))
     && (local_78 = CONCAT44(local_78._4_4_,(float)local_78), local_70 != 0)) {
    local_80._0_4_ = in_ECX;
    local_78 = FUN_1002a740();
    piVar4 = (int *)FUN_1003bd90(&local_60,&local_78);
    if (*piVar4 != DAT_103d45f4) {
      cVar2 = FUN_1002a6d0();
      if (cVar2 != '\0') {
        cVar2 = FUN_10027020();
        if (cVar2 != '\0') {
          local_88 = *(float *)(in_ECX + 0x1c);
          uVar8 = (uint)*(byte *)(in_ECX + 0x656);
          if ((local_88 != 0.0) && (*(int *)(*DAT_10094384 + 0x1c54) != 0)) {
            local_8c = (float)(*(int *)(*DAT_10094384 + 0x1c54) + 0x372c);
            local_58 = (int *)0x0;
            cVar2 = (*DAT_1009436c)(local_88,0,&local_58);
            fVar9 = local_90;
            if ((cVar2 == '\0') && (local_58 != (int *)0x0)) {
              uStack_a0 = (float)CONCAT13((char)local_58[1],(undefined3)uStack_a0);
              puStack_84 = (undefined4 *)(**(code **)(*local_58 + 0x1c))();
              if (puStack_84 != (undefined4 *)0x0) {
                if (uStack_a0._3_1_ == '1') {
                  piVar4 = (int *)local_58[0x143c];
                  if (piVar4 != (int *)0x0) {
                    if ((short)piVar4[1] == -0x1fb8) {
                      iVar6 = 0x14;
                    }
                    else {
                      uVar5 = (**(code **)(*piVar4 + 0x5c))();
                      iVar6 = (uVar5 & 0xff) * 7;
                    }
                    *(undefined4 *)(in_ECX + 6) = *puStack_84;
                    *(uint *)(in_ECX + 10) = (uVar8 - iVar6) * 5 + puStack_84[1];
                  }
                }
                else if (uStack_a0._3_1_ != 'A') {
                  *(undefined4 *)(in_ECX + 6) = *puStack_84;
                  *(uint *)(in_ECX + 10) = puStack_84[1] + (uVar8 + 3) * -5;
                }
              }
              (*DAT_100944d0)(local_88,0);
            }
          }
          local_60 = (*(int *)((int)fVar9 + 0x514) - *(int *)((int)fVar9 + 0x55c)) +
                     *(int *)(in_ECX + 6);
          local_5c = ((*(int *)((int)fVar9 + 0x518) - *(int *)((int)fVar9 + 0x560)) -
                     *(int *)(in_ECX + 0xe)) + *(int *)(in_ECX + 10);
          piVar4 = *(int **)(in_ECX + 0x652);
          uVar5 = (uint)*(byte *)(in_ECX + 0x650);
          if ((uVar5 != 0) && (uVar5 < uVar8)) {
            uVar8 = uVar5;
          }
          if (0x20 < uVar8) {
            uVar8 = 0x20;
          }
          if (piVar4 == (int *)0x0) {
            return;
          }
          if (uVar8 == 0) {
            return;
          }
          local_9c = piVar4;
          sVar3 = (*(code *)0x792fb0)();
          local_70 = (int)sVar3;
          sVar3 = (*DAT_1009431c)(0);
          puStack_84 = (undefined4 *)(int)sVar3;
          if ((int)puStack_84 < 1) {
            return;
          }
          iStack_98 = 0;
          fStack_a4 = (float)uVar8;
          uVar1 = local_78;
          do {
            if (*piVar4 != 0) {
              local_78 = uVar1;
              iVar6 = (*DAT_10094478)(piVar4);
              uVar1 = local_78;
              if (iStack_98 < iVar6) {
                iStack_98 = iVar6;
              }
            }
            local_78._4_4_ = (undefined4)((ulonglong)uVar1 >> 0x20);
            piVar4 = piVar4 + 1;
            fStack_a4 = (float)((int)fStack_a4 - 1);
          } while (fStack_a4 != 0.0);
          local_88 = DAT_100927b8;
          if (DAT_100927b8 < 0.0) {
            local_78 = uVar1;
            UVar7 = GetPrivateProfileIntA("IEex Options","Floating Text Size",100,".\\Icewind2.ini")
            ;
            if ((int)UVar7 < 0x32) {
              UVar7 = 0x32;
            }
            else if (200 < (int)UVar7) {
              UVar7 = 200;
            }
            DAT_100927b8 = (float)(int)UVar7 / 100.0;
          }
          iVar6 = iStack_98;
          piVar4 = local_9c;
          fStack_94 = (float)iStack_98;
          local_8c = DAT_1008f298;
          uStack_6c = (double)CONCAT44(fStack_94 * DAT_100927b8,(float)uStack_6c);
          local_58 = (int *)(float)(int)((int)puStack_84 * uVar8);
          local_78._0_4_ = (float)local_58 * DAT_100927b8;
          fVar9 = (fStack_94 * DAT_100927b8) / (DAT_1008f298 + DAT_1008f298);
          if (*(int *)(local_80._0_4_ + 6) + (int)fVar9 < *(int *)((int)local_90 + 0x55c)) {
            return;
          }
          if ((*(int *)((int)local_90 + 0x51c) - *(int *)((int)local_90 + 0x514)) +
              *(int *)((int)local_90 + 0x55c) < *(int *)(local_80._0_4_ + 6) - (int)fVar9) {
            return;
          }
          local_80._0_4_ = *(int *)(local_80._0_4_ + 10) - *(int *)(local_80._0_4_ + 0xe);
          fVar9 = (float)(int)((float)local_78 / (DAT_1008f298 + DAT_1008f298));
          if (local_80._0_4_ + (int)fVar9 < *(int *)((int)local_90 + 0x560)) {
            return;
          }
          if ((*(int *)((int)local_90 + 0x520) - *(int *)((int)local_90 + 0x518)) +
              *(int *)((int)local_90 + 0x560) < local_80._0_4_ - (int)fVar9) {
            return;
          }
          uStack_54 = 0xffff8300;
          uStack_50 = 0xffff8300;
          uStack_4c = 32000;
          uStack_48 = 32000;
          local_9c = (int *)((float)(*(int *)((int)local_90 + 0x51c) +
                                    *(int *)((int)local_90 + 0x514)) * 0.5);
          fStack_a4 = (float)(*(int *)((int)local_90 + 0x520) + *(int *)((int)local_90 + 0x518)) *
                      0.5;
          local_90 = fVar9;
          local_88 = DAT_100927b8;
          if (DAT_103d4756 != '\0') {
            local_80 = (undefined1  [8])(double)(float)local_9c;
            dVar10 = floor((double)local_80);
            local_9c = (int *)(float)dVar10;
            local_80 = (undefined1  [8])(double)fStack_a4;
            dVar10 = floor((double)local_80);
            fStack_a4 = (float)dVar10;
            local_80._0_4_ = fStack_a4;
          }
          (*DAT_104df52c)();
          (*DAT_103debfc)();
          (*DAT_104df50c)();
          uVar15 = 0;
          register0x00001200 =
               (double)(((((float)local_60 - fStack_a8) * local_90 + fStack_a8) -
                        (float)local_80._4_4_ * 0.5) + 0.5);
          dVar10 = floor(register0x00001200);
          fVar9 = (float)dVar10;
          uStack_6c = (double)(((((float)iStack_64 - uStack_a0) * local_90 + uStack_a0) -
                               (float)uStack_6c * 0.5) + 0.5);
          local_80._4_4_ = fVar9;
          dVar10 = floor(uStack_6c);
          uStack_6c = (double)CONCAT44(uStack_6c._4_4_,(float)dVar10);
          (*DAT_104df524)((float)dVar10,fVar9);
          uVar14 = 0x3f800000;
          (*DAT_104df56c)(iStack_98,iStack_98,0x3f800000);
          fVar9 = uStack_a0;
          if (0 < iVar6) {
            uVar13 = 0xde1;
            (*DAT_104df040)(0xde1);
            (*DAT_104df5a4)(0xbe2);
            uVar12 = 0x302;
            (*DAT_103ded84)(0x302,0x303);
            (*DAT_104df570)(0,0,0,0x3ec28f5c);
            fStack_94 = fStack_94 + 6.0;
            (*DAT_104df5c8)(4);
            uVar11 = 0;
            (*DAT_103defe8)(0xc0c00000,0xc0c00000,0);
            (*DAT_103defe8)(uVar13,0xc0c00000,0);
            (*DAT_103defe8)(uVar12,uStack_b0,0);
            (*DAT_103defe8)(0xc0c00000,0xc0c00000,0);
            (*DAT_103defe8)(uVar11,uVar15,0);
            (*DAT_103defe8)(0xc0c00000,uVar14,0);
            (*DAT_104df208)();
            (*DAT_104df5a4)(0xde1);
            fVar9 = uStack_a0;
          }
          do {
            if (*piVar4 != 0) {
              (*DAT_10094364)(piVar4,0,local_88,&local_70,0,0);
            }
            local_88 = (float)((int)local_88 + (int)fVar9);
            piVar4 = piVar4 + 1;
            uVar8 = uVar8 - 1;
          } while (uVar8 != 0);
          (*DAT_104df608)();
          return;
        }
      }
    }
  }
  (*DAT_1009428c)(param_1,uStack_6c._4_4_,param_3);
  return;
}
```

## Notes

### The two resref registries

Two identical hash sets of normalized 8-byte resrefs live in the fork's
BSS:

| Registry | Object | Count (+8) | Miss-default key (+4) | Populated by |
|---|---|---|---|---|
| HD sprite resrefs | `DAT_103d45cc` | `DAT_103d45d4` | `DAT_103d45d0` | `IEex_Helper_RegisterHDSprite` |
| Supersample font resrefs | `DAT_103d45f0` | `DAT_103d45f8` | `DAT_103d45f4` | `IEex_Helper_RegisterSupersampleFont` |

Mechanics (inner FUN_1002a740 / FUN_1003bd90 / FUN_1003bee0):

1. The caller supplies an 8-byte buffer (zero-padded copy of the Lua
   string, truncated at 8 chars). FUN_1002a740 is a thiscall on that
   buffer: it uppercases a-z in all 8 bytes and returns them as a qword
   key in eax:edx.
2. Lookup FUN_1003bd90 hashes the 8 bytes with FNV-1a (basis
   0x811C9DC5, prime 0x01000193), finds the node through the bucket
   array, and returns the node's key dword 0 - or the registry's
   miss-default at object+4 when the bucket is empty. Every draw-side
   membership test in this group is `lookup(...) != <miss-default>`.
3. Insert FUN_1003bee0 uses the same hash, allocates a 0x10-byte node
   with operator_new, links it into the bucket chain, and increments
   the count at object+8. At the hard capacity it throws
   std::length_error("unordered_map/set too long") - a C++ exception,
   not a Lua error (the helper is entered from Lua through the same
   __cdecl marshal as every other helper).

The draw helpers additionally pass the hooked engine object itself as
the FUN_1002a740 `this` (the decompiler hides the register argument):
CVidCell and CVidFont both start with their resref, so the lookup key
is the object's own resref uppercased. Registration is done from Lua at
patch-load time; lookups happen on the render thread. Nothing
synchronizes the two - same model as the other fork registries
(documented in the bridge group): single-threaded use or nothing.

### GL resolvers this group depends on

- **Matrix six** - inner FUN_100257e0 (one-shot flag `DAT_104df740`):
  GetModuleHandleA("opengl32.dll") + GetProcAddress into
  `DAT_104df52c` glMatrixMode, `DAT_104df50c` glLoadIdentity,
  `DAT_104df524` glTranslatef, `DAT_104df56c` glScalef,
  `DAT_103debfc` glPushMatrix, `DAT_104df608` glPopMatrix; returns 1
  only if all six resolved.
- **Filter pair** - inner FUN_1002a6d0 (one-shot flag `DAT_104df754`):
  resolves `DAT_104df61c` glTexParameteri and `DAT_104df054`
  glTexSubImage2D, then requires the matrix six (calls FUN_100257e0);
  returns 1 only if all eight are live.
- **Fog sixteen** - the fog group's FUN_10027020 (documented in the
  FogTexDraw note): 16 procs from opengl32.dll, gate passes only when
  all sixteen are live. CGameText_RenderHD reuses five of its slots:
  `DAT_104df040` glDisable, `DAT_104df5a4` glEnable, `DAT_104df570`
  glColor4f, `DAT_104df5c8` glBegin, `DAT_104df208` glEnd.

A helper that needs a resolver calls it every time; the one-shot flags
make the repeat calls cheap, and any missing entry point simply fails
the gate and drops the helper to its stock fallback.

### The half-scale FX pipeline (HD creature sprites)

HD sprites ship as native-2x BAMs under registered resrefs. One frame
of a creature render then flows through three helpers:

1. `CVidCell_FXRender3dHD` (hook over CVidCell::FXRender3d at 0x7C5330)
   checks the cell's resref against the HD sprite registry and, on a
   hit, raises the one-shot flag `DAT_104df08d`; the stock FXRender3d
   runs unchanged either way, compositing the cell's effects into the
   FX scratch surface at full 2x size.
2. `CVidInf_FXBltToBackHD` (hook over CVidInf::FXBltToBack at 0x79CC90)
   consumes `DAT_104df08d`: while it is set it clears the flag,
   converts the destination width/height into the mirrored coordinate
   space when the blit flags say so (bit 0x10 adds the source-rect
   width, bit 0x20 its height), halves both (integer division), and
   raises `DAT_103dedb4` around the stock blit - which now transfers
   only the lower-left 2x quadrant of the scratch into the back buffer,
   i.e. the half-size image.
3. `IEex_Helper_CVidCell_RenderTextureHD` (ui-scale group, 0x1002aaa0,
   hook over CVidCell::RenderTexture at 0x7C4240) sees `DAT_103dedb4`
   set inside the same stock call and switches the cell's texture
   filtering to LINEAR + CLAMP_TO_EDGE, applies its texel-inset fixups,
   and wraps the draw in `glScalef(0.5, 0.5, 1.0)` about the sprite
   anchor - the half-size image is drawn into a half-size screen area,
   with one HD texel per screen pixel. Afterwards it restores NEAREST
   and clears the texture matrix. The flag is cleared again when
   FXBltToBackHD returns, so the bracket covers exactly one blit.

Because the 0x7C4240 hook belongs to the UI-scale patch, this pipeline
only works with both patches installed; the HDSprites Lua patch does
not install it and would leave the second half of the bracket missing.

### The supersample-font draw pipeline (world floating text)

Stock floating text (damage numbers, sayings) is drawn by CPU-blitting
1-bit glyph masks - aliased at any resolution. The supersample patch
redraws it from a native-2x glyph atlas through GL:

- `CVidFont_CreateTextureHD` forces the registered font's texture to
  LINEAR + CLAMP_TO_EDGE after the stock upload, so the 2x atlas is
  filtered when sampled.
- `CGameText_RenderHD` owns the whole CGameText::Render: it measures
  the text, computes the block position from the viewport/area camera
  (coordinates halved by the 0.5 factor to land in the HD world's
  half-scale modelview), draws a translucent black readability panel
  (alpha 0.38, GL_QUADS, +-1.5 world units), then draws each line
  through the TextOut3d trampoline. Stock render is skipped entirely on
  success and used as the failure fallback.
- `CVidFont_TextOut3dHD` scales the stock draw about the text anchor by
  the Floating Text Size factor (below), so the percent setting still
  works inside the GL path.
- The block follows the world through camera zoom but the glyph size is
  zoom-independent: the metrics hooks (`GetFontHeightHD`,
  `GetStringLengthHD`) halve the registered font's reported height and
  string length, which keeps CGameText's SetText word-wrap budget at
  the stock character/line counts while the GL draw produces crisp 2x
  glyphs.
- `CVidFont_TextOutHP` is a separate special case: portrait HP numbers
  are redrawn from the 8-bit coverage atlas `Override/IEEXHPF.BMP`
  (three passes: two pre-baked shadow/outline passes from
  `DAT_10064c18`-family tables, then the color pass tinted by the
  engine color dword), with per-glyph clipping against the panel rect.
  The engine's own font path is 1-bit alpha, which produced dark
  fringes; this path samples real coverage.

### Floating Text Size (the shared scale factor)

`DAT_100927b8` caches the user's Floating Text Size percent as a float
multiplier. It is lazily loaded on first use with
`GetPrivateProfileIntA("IEex Options", "Floating Text Size", 100,
".\\Icewind2.ini")`, clamped to 50..200 and divided by 100.0; it
starts at -1.0f so the first read always loads.
`IEex_Helper_ReloadFloatTextScale` just writes -1.0f back - the IEex
Options Apply button uses it as a cache drop; nothing is rebuilt (the
atlas does not change, only the draw scale).

### Lua hook topology

- `IEex_HDSprites_Patch.lua` (gated on the WeiDU flag IEEX_HD_SPRITES
  and `[Program Options] "3D Acceleration"` != 0 in .\Icewind2.ini):
  two `IEex_HookReplaceFunctionMaintainOriginal` hooks - 0x7C5330
  (6-byte prologue, name "CVidCell::FXRender3dOriginal") and 0x79CC90
  (6-byte prologue, name "CVidInf::FXBltToBackOriginal") - each
  jumping to the corresponding helper with the trampoline registered
  through `IEex_Helper_DefineAddress` (first -1 placeholder, then the
  real label after the hook); then ~150 `IEex_Helper_RegisterHDSprite`
  calls listing the shipped 2x BAM resrefs.
- `IEex_SupersampleFont_Patch.lua` (gated on 3D Acceleration and the
  2x-UI "non-pixelated fonts" sub-option): hooks 0x4CBB20
  CGameText::Render (7-byte prologue) -> `CGameText_RenderHD`; 0x793720
  CVidFont::TextOut (8-byte prologue) -> `CVidFont_TextOutHP`; 0x7A1210
  CVidFont::TextOut3d (7-byte prologue) -> `CVidFont_TextOut3dHD`;
  0x7A0F00 CVidFont::CreateTexture (5-byte prologue) ->
  `CVidFont_CreateTextureHD`; 0x793050 CVidFont::GetFontHeight and
  0x792D40 CVidFont::GetStringLength (5-byte prologues) ->
  `CVidFont_GetFontHeightHD` / `CVidFont_GetStringLengthHD`; one
  `IEex_Helper_RegisterSupersampleFont("IEEXFLT")` - IEEXFLT is the
  only font that ships a 2x atlas. All hooks are
  MaintainOriginal-style: the original body stays reachable as a
  trampoline, which is what the CGameText_RenderHD per-line draw and
  the TextOut3dHD fallback call.
- `IEex_Gui_State.lua` (IEex Options dialog): the Apply button calls
  `IEex_Helper_ReloadFloatTextScale` (next to
  `IEex_Helper_SetMarkerStyle`, markers group).

`IEex_Helper_CGameText_RenderHD` - 0x1002b6a0 (FUN_1002b6a0) - __cdecl
with three argument words (thiscall this in ECX, `ret 0xC`); entered
from the IEex_SupersampleFont_Patch.lua hook at 0x4CBB20 (CGameText::
Render, 7-byte displaced prologue `push -1 / push 0x813E88`).

##### Behavior

The most involved helper in the group; it owns the whole floating-text
render. Gates, in order (objdump-verified): the pointer at `this+0x12`
non-null (the area/viewport object; its +0x514/+0x518/+0x55C/+0x560
fields supply the camera math below), the font registry non-empty
(`DAT_103d45f8`), a near-dead check on the *computed address*
`this+0x148` (lea + test - cannot fail for a live object), the font
resref registered - the 8 bytes at `this+0x1F4` (= `this+0x148+0xAC`,
the text object's cached copy of its font resref; uppercased by
FUN_1002a740, looked up by FUN_1003bd90 against `DAT_103d45f4`; for a
registered font this is the same resref the CVidFont carries at offset
0, which is why the font-object lookups in the other helpers hit the
same entries) - then FUN_1002a6d0 (filter pair + matrix six) and
FUN_10027020 (fog sixteen) both returning 1, and a `DAT_103d4756` gate
(the HD world-scale enable). Any failure jumps straight to the stock
tail.

On success it re-implements the render:

1. Reads engine state through runtime-registered slots: the active
   area from `*DAT_10094384` (fields +0x1C54 then +0x372C, both
   checked non-null), then a thiscall through `DAT_1009436C` on the
   area+0x1C54+0x372C object with args (string at `this+0x1C`, 0,
   &out, 0) - success in al, failure skips the block. The out object
   is classed by its byte at +0x4: '1' refreshes `this+0x6` (x) and
   `this+0xA` (y) with digit layout (via its +0x50F0 object and a
   WORD tag 0xE048 check: `(font_height - offset) * 5 + [metrics+4]`,
   offset = a vtable-derived value * 7 or a fixed 0x14); 'A' leaves
   the position untouched; any other letter uses letter layout
   (`[metrics+4] - (font_height + 3) * 5`), font_height being the
   byte at `this+0x656`. The out object is released through
   `DAT_100944D0`.
2. Computes the block position from the viewport fields at area
   +0x514/+0x518 minus +0x55C/+0x560 plus the just-refreshed
   `this+6`/`this+0xA`, multiplies by 0.5 (.rdata 0x10088934) to land
   in the HD world's half-scale space, and floor-snaps the anchor when
   `DAT_103d4756` asks for it.
3. Splits `this+0x652` (line array of dwords) into per-line maxima:
   the line budget is `min(byte this+0x656, byte this+0x650, 0x20)`,
   each line's pixel width comes from `DAT_10094478` (GetStringLength
   trampoline), and the running maximum sizes the panel.
4. Loads the Floating-Text-Size multiplier from `DAT_100927b8`
   (lazy GetPrivateProfileIntA "IEex Options"/"Floating Text Size",
   default 100, clamp 50..200, /100.0; -1.0f sentinel), culls the
   block against the viewport, then emits the GL sequence confirmed at
   the instruction level: glMatrixMode(GL_MODELVIEW 0x1700),
   glPushMatrix, glLoadIdentity, glTranslatef(floor-snapped anchor,
   0.5-scaled), glScalef(s, s, 1.0), then the readability panel -
   glDisable(GL_TEXTURE_2D), glEnable(GL_BLEND),
   glBlendFunc(GL_SRC_ALPHA 0x302, GL_ONE_MINUS_SRC_ALPHA 0x303),
   glColor4f(0, 0, 0, 0.38) (0x3EC28F5C), glBegin(GL_QUADS = 4) with
   four vertices at the panel corners (+-1.5 world units about the
   anchor), glEnd, glEnable(GL_TEXTURE_2D) - then one trampoline call
   `DAT_10094364` (TextOut3d original) per line, advancing x by the
   line height, and glPopMatrix.
5. Returns without calling the stock render - the overlay replaces it
   entirely.

##### Failure fallbacks

Every gate failure funnels into one tail: push the three arguments,
ECX = the CGameText this, call the stock CGameText::Render through
`DAT_1009428C`, return its path. The stock render is therefore both
the unregistered-font fallback and the no-GL fallback - the hook can
never leave floating text undrawn.

##### Decompiler artifact

The float soup around the anchor (mulss by 0.5 applied to stack
copies, repeated floor() calls) resolves to "scale coordinates by 0.5
and snap"; the objdump-confirmed call order above is authoritative.
`DAT_1008F298` (1.0f) is loaded early as a default multiplier and is
not the font height. The '1' vs 'A' first-char classification is a
digit-vs-text heuristic inherited from the stock measure code.

