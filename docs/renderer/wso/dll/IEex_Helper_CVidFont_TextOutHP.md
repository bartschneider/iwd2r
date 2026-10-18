# IEex_Helper_CVidFont_TextOutHP

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10033c20` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10033c20.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_SupersampleFont_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4
FUN_10033c20(int *param_1,undefined4 param_2,undefined4 param_3,undefined4 *param_4,
            undefined4 param_5)

{
  int iVar1;
  char cVar2;
  undefined4 uVar3;
  int in_ECX;
  int iVar4;
  undefined4 local_14;
  undefined4 uStack_10;
  undefined4 uStack_c;
  undefined4 uStack_8;

  if ((((param_1 != (int *)0x0) && (in_ECX != 0)) && (*(int *)(in_ECX + 0xa8) != 0)) &&
     (iVar1 = *(int *)(*(int *)(in_ECX + 0xa8) + 0x10), iVar1 != 0)) {
    iVar4 = 0;
    do {
      if (*(char *)(iVar4 + iVar1) != "NUMFONT"[iVar4]) goto LAB_10033cbb;
      iVar4 = iVar4 + 1;
    } while (iVar4 < 8);
    if (*param_1 != 0) {
      local_14 = 0xffff0000;
      uStack_10 = 0xffff0000;
      uStack_c = 0x10000;
      uStack_8 = 0x10000;
      if (param_4 != (undefined4 *)0x0) {
        local_14 = *param_4;
        uStack_10 = param_4[1];
        uStack_c = param_4[2];
        uStack_8 = param_4[3];
      }
      cVar2 = FUN_10033750(param_3,*(undefined4 *)(in_ECX + 0x4ec),&local_14);
      if (cVar2 != '\0') {
        return 1;
      }
    }
  }
LAB_10033cbb:
  uVar3 = (*DAT_10094330)(param_1,param_2,param_3,param_4,param_5);
  return uVar3;
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

`IEex_Helper_CVidFont_TextOutHP` - 0x10033c20 (FUN_10033c20) -
__cdecl with five argument words (thiscall this in ECX); entered from
the IEex_SupersampleFont_Patch.lua hook at 0x793720 (CVidFont::TextOut,
8-byte displaced prologue `sub esp,0x10 / push ebp / mov ebp,[esp+
0x18]`).

##### Behavior

Special-cased for the portrait HP digits. Gates: non-null first
argument (the string), non-null font this, `*(this+0xa8)` and
`*(*(this+0xa8)+0x10)` non-null (the engine's bitmap-carrier chain).
The first 8 bytes of the font object are compared against "NUMFONT" -
the helper is scoped to the NUMFONT resref and falls through to stock
for every other font. For NUMFONT with a non-empty string it builds a
fixed-point 16.16 clip rect (either the full-range default
{0xFFFF0000, 0xFFFF0000, 0x00010000, 0x00010000} or from argument 4's
four words) and calls the atlas renderer FUN_10033750 with (string,
color dword from `*(this+0x4ec)`, rect); on success it returns 1 and
nothing else is drawn.

FUN_10033750 (inner) renders digits from the 8-bit coverage atlas
Override/IEEXHPF.BMP: glBindTexture + glTexEnvf(GL_DECAL) +
glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA), then three glBegin(4)
passes - two pre-baked shadow/outline passes positioned from the
`DAT_10064c18`/`DAT_10064c1c`/`DAT_10064c20` tables, then the color
pass tinting the coverage with the engine color (RGB/255) - with 16-
texel glyph cells, per-digit advance from the `DAT_10064be0` width
table, UVs computed against the atlas width `DAT_104df550`, per-glyph
clipping against the rect (adjusting UVs, not just rejecting), and the
stock restores (GL_MODULATE 0x45F00800 as a GLfloat bit pattern,
glColor4f(1,1,1,1)). It returns 0 when its own gates fail (mode flag
`DAT_10092b80` != 1 via FUN_1002fff0, atlas texture `DAT_103dedcc` =
0, or atlas width < 1) and the helper falls back to the stock TextOut
trampoline `DAT_10094330`.

##### Failure fallbacks

Any gate failure, non-NUMFONT font, or FUN_10033750 returning 0: the
stock TextOut trampoline draws the digits with the engine's 1-bit
alpha path (the dark-fringed rendering the patch exists to replace).

##### Decompiler artifact

FUN_10033750 receives two arguments in registers (ECX = string pointer,
EDX = start x) that the decompiler surfaces only as `in_ECX`/`in_EDX`
inside the callee; the fixed-point 16.16 rect values in the caller are
shown as signed ints. The three-pass loop's pass-2 special case is the
color pass, not a third shadow.

