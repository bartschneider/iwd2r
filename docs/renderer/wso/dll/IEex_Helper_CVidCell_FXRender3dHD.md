# IEex_Helper_CVidCell_FXRender3dHD

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002a860` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002a860.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_HDSprites_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1002a860(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined4 param_7)

{
  int *piVar1;
  undefined8 local_10;
  undefined1 local_8 [4];

  if (DAT_103d45d4 != 0) {
    local_10 = FUN_1002a740();
    piVar1 = (int *)FUN_1003bd90(local_8,&local_10);
    if (*piVar1 != DAT_103d45d0) {
      DAT_104df08d = 1;
    }
  }
  (*DAT_100942e0)(param_1,param_2,param_3,param_4,param_5,param_6,param_7);
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
  FogTexDraw note). CGameText_RenderHD reuses four of its slots:
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

`IEex_Helper_CVidCell_FXRender3dHD` - 0x1002a860 (FUN_1002a860) -
__cdecl with seven argument words; entered from the IEex_HDSprites_
Patch.lua IEex_HookReplaceFunctionMaintainOriginal at 0x7C5330
(CVidCell::FXRender3d, 6-byte displaced prologue, trampoline registered
as "CVidCell::FXRender3dOriginal").

##### Behavior

Pre-hook: if the HD sprite registry is non-empty (`DAT_103d45d4 != 0`),
derive the key from the cell's resref (FUN_1002a740 with ECX = the
hooked CVidCell, whose offset-0 resref supplies the 8 bytes) and look
it up (FUN_1003bd90); on a hit (`!= DAT_103d45d0`) raise the one-shot
flag `DAT_104df08d`. Then the stock FXRender3d trampoline
(`DAT_100942e0`) runs unconditionally with all seven arguments. The
flag survives until the FXBltToBackHD stage of the same frame consumes
it (see COMMON).

##### Failure fallbacks

Empty registry, unregistered resref, or failed lookup: the flag stays
clear and the frame's FX blit runs at stock size - stock rendering.

##### Decompiler artifact

FUN_1002a740's hidden ECX (the CVidCell this) and the returned qword
are elided by the decompiler; the local receiving the key is shown as
undefined8.

