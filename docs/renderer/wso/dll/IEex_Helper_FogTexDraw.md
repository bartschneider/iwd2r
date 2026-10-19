# IEex_Helper_FogTexDraw

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100271d0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_100271d0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_HDTiles_Patch.lua`, `IEex_Render_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_100271d0(int param_1)

{
  char cVar1;
  int iVar2;
  int iVar3;
  float fVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  float fVar8;
  float fVar9;
  float fVar10;
  float fVar11;

  DAT_1009278c = 1;
  if (param_1 == 0) {
    FUN_10025a80();
  }
  else if (*(int *)(param_1 + 0x51c) - *(int *)(param_1 + 0x514) != 0x200) {
    FUN_10025a80();
    FUN_10026b50();
    cVar1 = FUN_10027020();
    if (cVar1 == '\0') {
      return;
    }
    (*DAT_104df0c8)(0x2300,0x2200,0x46040000);
    if (*(int *)(param_1 + 0x860) == 0) {
      return;
    }
    iVar2 = (int)*(short *)(param_1 + 0x866);
    if (iVar2 < 2) {
      return;
    }
    iVar3 = (int)*(short *)(param_1 + 0x868);
    if (iVar3 < 2) {
      return;
    }
    if (0x40000 < iVar3 * iVar2) {
      return;
    }
    fVar4 = (float)*(int *)(param_1 + 0x514);
    fVar5 = (float)*(int *)(param_1 + 0x518);
    fVar6 = (float)*(int *)(param_1 + 0x51c);
    fVar7 = (float)*(int *)(param_1 + 0x520);
    fVar10 = 0.2 - (float)*(int *)(param_1 + 0x50c);
    fVar11 = 0.2 - (float)*(int *)(param_1 + 0x510);
    fVar8 = (float)((iVar2 * 0x20 - *(int *)(param_1 + 0x50c)) + -0x20) + 0.2;
    fVar9 = (float)((iVar3 * 0x20 - *(int *)(param_1 + 0x510)) + -0x20) + 0.2;
    (*DAT_104df040)(0xde1);
    (*DAT_104df5a4)(0xbe2);
    (*DAT_103ded84)(0x302,0x303);
    (*DAT_104df570)(0,0,0,0x3f800000);
    if (fVar4 < fVar10) {
      (*DAT_104df5c8)(7);
      (*DAT_103defe8)(fVar4,fVar5,0);
      (*DAT_103defe8)(fVar4,fVar7,0);
      (*DAT_103defe8)(fVar10,fVar7,0);
      (*DAT_103defe8)(fVar10,fVar5,0);
      (*DAT_104df208)();
    }
    if (fVar8 < fVar6) {
      (*DAT_104df5c8)(7);
      (*DAT_103defe8)(fVar8,fVar5,0);
      (*DAT_103defe8)(fVar8,fVar7,0);
      (*DAT_103defe8)(fVar6,fVar7,0);
      (*DAT_103defe8)(fVar6,fVar5,0);
      (*DAT_104df208)();
    }
    if (fVar5 < fVar11) {
      (*DAT_104df5c8)(7);
      (*DAT_103defe8)(fVar10,fVar5,0);
      (*DAT_103defe8)(fVar10,fVar11,0);
      (*DAT_103defe8)(fVar8,fVar11,0);
      (*DAT_103defe8)(fVar8,fVar5,0);
      (*DAT_104df208)();
    }
    if (fVar9 < fVar7) {
      (*DAT_104df5c8)(7);
      (*DAT_103defe8)(fVar10,fVar9,0);
      (*DAT_103defe8)(fVar10,fVar7,0);
      (*DAT_103defe8)(fVar8,fVar7,0);
      (*DAT_103defe8)(fVar8,fVar9,0);
      (*DAT_104df208)();
    }
    (*DAT_104df040)(0xbe2);
    DAT_1009278c = 0;
    return;
  }
  FUN_10026b50();
  return;
}
```

## Notes

### The two fog paths and the ordering flag

The staged patches replace stock fog with two independent mechanisms,
both C++ side (this group):

1. **Texture path (HD, default under the tile-atlas patches).**
   FogTexDraw runs early in the area render (hook 0x47785B, the
   tiles-to-sprites boundary): it flushes the tile-atlas batch, paints
   the four solid-black edge bands outside the fog rectangle with
   immediate-mode GL_QUADS, and clears the ordering flag
   `DAT_1009278c`. FogTexDrawLate runs late (hook 0x477B61, just before
   the CInfinity::PostRender call in CGameArea::Render): when the flag
   is clear it expands the 1-byte-per-cell visibility map into an RGBA
   scratch image (alpha 0x80 explored-not-visible, 0xFF never seen, 0
   visible) and draws the fog interior as one textured GL_QUADS quad.
   Sprites rendered between the two hooks therefore sit *above* fog;
   the Lua comment records that the original single early hook let
   objects drawn after tiles escape the fog (corpses worst -
   STATE_DEAD bypasses the m_canBeSeen gate in CGameSprite::Render).
   FogSkip is the empty stub that kills the stock immediate-mode fog
   draws (CVisibilityMap FillRect3d at 0x7BD140 and RenderFan at
   0x7BD740).
2. **Per-cell CPU path (fog-type gated).** RenderFoW (hook 0x477B61,
   after FogTexDrawLate in the same injected stub) and RenderFoWSolid
   (hook 0x551FF0, replacing the stock solid-cell renderer) darken the
   32-bpp surface pixels of each visible 64x64 fog cell directly,
   quadrant by quadrant, multiplying RGB by a factor (0.6 for the
   non-solid path, 0.0 - pure black - for the solid path), with an
   optional 64x64 dither mask instead of a flat multiply. The Lua stub
   reads the fork's fog-type value first (`IEex_FogTypePtr` in the
   "IEex_Options" bridge - see the lock/bridge groups for
   LockGlobal/UnlockGlobal) and only jumps to the helpers when it is
   non-zero; otherwise the stock code runs.

`DAT_1009278c` (byte, BSS) is the ordering flag: FogTexDraw sets it to
1 at entry and clears it to 0 only when it drew the edge bands; the
stock-width short-circuit leaves it at 1, which makes FogTexDrawLate
skip (see the FogTexDraw note for the consequences).

#### Lua hook topology (IEex_Render_Patch.lua; FogSkip in IEex_HDTiles_Patch.lua)

The whole fog section is gated by `not IEex_Vanilla` and by is3D =
GetPrivateProfileIntA-style read of `[Program Options] "3D
Acceleration"` in `.\Icewind2.ini` being non-zero (software renderer
opt-out; IEex_HDTiles_Patch.lua applies the same gate).

- FogSkip (IEex_HDTiles_Patch.lua): plain `!jmp_dword` to the helper's
  registry address written over the entries of CVisibilityMap::
  FillRect3d (0x7BD140) and CVisibilityMap::RenderFan (0x7BD740) -
  both __cdecl, so the empty `ret` stub is ABI-correct. Only callers
  are the CVisibility* fog loops; the patch comment notes the fog
  records become DEAD in GL mode once both are skipped.
- FogTexDraw (IEex_HDTiles_Patch.lua): IEex_HookRestore(0x47785B, 0,
  6, push_all, push_esi, call FogTexDraw, pop_all) - the displaced 6
  bytes include the stock `cmp` against RENDER_MESSAGESCREEN whose
  flags pushad preserves (eax). esi = CGameArea (CGameArea::Render's
  this) and is the helper's only argument; ecx is saved/restored by
  pushad/popad.
- FogTexDrawLate + RenderFoW: IEex_HookBeforeCall(0x477B61, ...) -
  0x477B61 is the CInfinity::PostRender call inside CGameArea::Render,
  after all four object passes and before weather; the stock
  CInfTileSet::RenderFogOfWar call sits 0x17 bytes later at 0x477B78.
  Injected stub: if is3D, save ecx (`push ecx` - ecx holds the
  CInfinity this for the imminent PostRender call), `push esi` (the
  helper argument), call FogTexDrawLate, restore ecx (`pop ecx`); then
  call the getFogTypePtr stub, `cmp [eax],0`, and if non-zero the same
  save/push esi/call RenderFoW/restore ecx sequence.
- RenderFoWSolid: IEex_HookRestore(0x551FF0, 0, 5, [call
  getFogTypePtr; cmp [eax],0; jnz_dword >IEex_Helper_RenderFoWSolid]) -
  when the fog type is zero, execution falls through into the
  displaced original bytes (the stock solid-cell renderer); when
  non-zero it tails into the helper, which inherits the stock call
  site's stack arguments (see the RenderFoWSolid note).
- getFogTypePtr is an IEex_WriteAssemblyAuto stub: push ecx/edx, push
  "IEex_Options", `!call >IEex_Helper_LockGlobal`, `!push_[dword]
  IEex_FogTypePtr`, push "IEex_Options", `!call
  >IEex_Helper_UnlockGlobal`, pop eax/edx/ecx, ret - i.e. it returns
  the address of the fog-type integer (global `IEex_FogTypePtr` in the
  "IEex_Options" bridge namespace) in eax, holding the bridge's lock
  only while reading the pointer.

#### GL entry-point slots (resolved by inner FUN_10027020)

One-shot (flag `DAT_104df701`), result cached in `DAT_104df0ec`:
GetModuleHandleA("opengl32.dll") + GetProcAddress for all sixteen
names below; the function returns 1 only when every slot is non-null
(glTexParameterfv is required even though no drawn path calls it).
All core GL 1.1.

| Slot | Function |
|---|---|
| DAT_104df5a4 | glEnable |
| DAT_104df040 | glDisable |
| DAT_103ded84 | glBlendFunc |
| DAT_104df0e8 | glBindTexture |
| DAT_104df5fc | glTexImage2D |
| DAT_104df064 | glTexSubImage2D |
| DAT_103dec00 | glTexParameteri |
| DAT_104df0cc | glTexParameterfv (resolved, never called) |
| DAT_104df0c8 | glTexEnvf |
| DAT_104df570 | glColor4f |
| DAT_104df5c8 | glBegin |
| DAT_104df208 | glEnd |
| DAT_104df018 | glTexCoord2f |
| DAT_103defe8 | glVertex3f |
| DAT_103defd4 | glGenTextures |
| DAT_104df004 | glIsTexture |

#### Fork state and CGameArea fields

- `DAT_1009278c` (byte): ordering flag (see above).
- `DAT_104df0d8` (GLuint): fog texture name; `DAT_104df55c` /
  `DAT_104df3b4`: the width/height it was last created with.
- `DAT_103df000`: RGBA scratch, w*h*4 bytes, rebuilt every frame.
- Counters (perf-log flag `DAT_104df0ed`): `DAT_104df228` = fog_draws
  (FogTexDrawLate quads drawn this frame), `DAT_104df3f0` = fog_upload
  bytes (accumulated w*h*4; logged as kib).
- CGameArea fields read (offsets as decompiled; engine side out of
  scope): +0x4fc / +0x500 (column/row start adjustments), +0x504
  (short: visibility-map width in cells), +0x50c / +0x510 (scroll x /
  y), +0x514 / +0x518 / +0x51c / +0x520 (viewport left/top/right/
  bottom; +0x514 is also passed as the clip-rect origin to the CPU
  path), +0x544 / +0x548 (visible column count / row count), +0x554 /
  +0x558 (start column / start row), +0x860 (pointer to the 1-byte-per-
  cell visibility map, CVisibilityMap::m_pMap), +0x866 / +0x868
  (shorts: map width / height in cells), +0x4e8 (object whose +0xa0 is
  dereferenced once to produce the ECX surface object for RenderFoW;
  identity not resolvable from the DLL).
- Engine pointer slots used by RenderFoW (BSS, runtime-registered via
  the loader's public address-list mechanism - cite
  IEexDLL/IEexLua.asm, not the DLL): `DAT_100944b8` (base whose +0x91c
  gate enables the CPU path), `DAT_100941a4` (tile-object lookup:
  fn(0, col, row) -> object pointer), `DAT_100943d0` (per-cell gate,
  called with no args), `DAT_10094210` (fog-state fetch:
  fn(visWidth*row + col, out 4 bytes)).

#### Perf-log CSV subsystem (inner FUN_10025a80 = PerfLogFrameTick)

FogTexDraw calls FUN_10025a80 every time (including its stock-width
and null short-circuits), making it the frame tick. State: open FILE*
`DAT_104df21c`, active flag `DAT_104df0ed`, frame counter
`DAT_104df234`, poll countdown `DAT_104df3c4` (reset to 0x1e).

- Every 30th call it re-reads `GetPrivateProfileIntA("Options",
  "Perf Log", 0, ".\IEex.ini")`. Turning it OFF closes the CSV
  (fclose), clears `DAT_104df0ed`, and logs "Perf Log OFF" through the
  InitHelperDLL logging function `DAT_100944fc`.
- Turning it ON: CreateDirectoryA(".\perf") (existing dir is fine),
  open `.\perf\perf_%04d%02d%02d_%02d%02d%02d.csv` (local time), and
  write the 28-column header
  "frame,ms,fps,tile_binds,tile_draws,tile_blend,fog_draws,fog_upload_kib,r3d_total,r3d_scratch,r3d_cachehit,r3d_upload,pool_vtiles,tilepass_ms,sync_ms,sprites_ms,ui_ms,present_ms,gpu_ms,atlas_up,blit_ms,swap_ms,ptstall_ms,ptswap_ms,presented,dropped,ptfence_ms,ptcycle_ms".
- Each call while ON: if `DAT_104df234 != 0` write one CSV row, then
  reset the per-frame counters. Documented column sources: frame =
  DAT_104df234; ms / fps from the frame timestamp pair
  (`_DAT_104df57c`/`_DAT_104df578`, 64-bit ticks/10, written on every
  call even when OFF); tile_binds = DAT_104df030; tile_draws =
  DAT_104df068; tile_blend = DAT_104df0a4; fog_draws = DAT_104df228;
  fog_upload_kib = (DAT_104df3f0 + 512) >> 10; r3d_total =
  DAT_103deda0; r3d_scratch = DAT_104df5cc; r3d_cachehit =
  DAT_104df594; r3d_upload = DAT_104df4e0; pool_vtiles =
  DAT_100927a0 (engine pool counter, read absolutely); atlas_up =
  DAT_103ded8c. The remaining columns (blit_ms, swap_ms, ptstall_ms,
  ptswap_ms, presented, dropped, ptfence_ms, ptcycle_ms) come from the
  present-group counters (`_DAT_104df410`, `_DAT_104df098`,
  `_DAT_104df378`, `_DAT_104df600`, `_DAT_103dedb8`, `_DAT_104df628`,
  `DAT_104df22c`, `DAT_104df514`); the decompiler elides the trailing
  varargs of the row-write _ftol/fprintf call, so the exact per-column
  arithmetic for those eight is not visible in the decompile - the
  reset list (see below) is the authoritative mapping of which
  counters feed the row.
- fflush every 64th frame (`(DAT_104df234 & 0x3f) == 0`).
- Every 512th frame (`(DAT_104df234 & 0x1ff) == 0`): (a) collect the
  64 panel-time buckets (counts `DAT_104df0f8`, 64-bit tick sums at
  `DAT_103dedd0` low / `DAT_103dedd4` high, stride 8 - filled by
  UIPanelRenderTimed, tile-atlas group) with non-zero counts,
  selection-sort them by total (high dword, then low), and log up to
  8 of them / 400 chars as " id%d=%dus(x%d)" (sum divided by 512 =
  microseconds per frame; count in parens), then memset both tables;
  (b) when `DAT_104df048` is non-zero, log "[P1census] over 512f:
  gate=%u clean=%u blkPanel=%u blkCtrl=%u blkDim=%u" (DAT_104df048 /
  DAT_104df1f8 / DAT_104df058 / DAT_104df0bc / DAT_104df5a8 - the
  UIPanelRenderTimed census) followed by one " | ctrl[%d] id=%d
  type=%d vft=%08X rc=%d hits=%u" per census record (max 8, 415-char
  cap); (c) reset all census counters and the census record count.
- Always (regardless of the flag): store the current Xtime_get_ticks
  (divided by 10) into `_DAT_104df57c`/`_DAT_104df578` as the new
  frame baseline.

The CSV lives under the game directory; the file is created on the GL
thread when the tick notices the ini flip, and nothing else in the DLL
reads or writes it.

#### Per-cell CPU darkening machinery (RenderFoW / RenderFoWSolid)

Shared entry FUN_1003d140 - decompiled as
`(param_1 = y, param_2 = clip-rect origin, param_3 = state bytes,
param_4 = solid flag)` plus two register arguments the decompiler
hides and one stack double it drops; the full convention from objdump:

- ECX (in): the "surface object" - an engine object whose vtable is
  used (see below). RenderFoW computes it as
  `*(*(CGameArea+0x4e8)+0xa0)`; RenderFoWSolid forwards its own first
  stack argument.
- EDX (in): x coordinate of the cell on the surface (RenderFoW: the
  column-adjusted screen x; RenderFoWSolid: 0).
- param_1 (stack): y coordinate (RenderFoW: the row-adjusted screen y;
  RenderFoWSolid: 0).
- param_2 (stack): pointer to the clip rect, 4 ints {x, y, x2, y2}
  (RenderFoW: CGameArea+0x514, the viewport rect; RenderFoWSolid: a
  local copy of the .rdata constant {0,0,0x40,0x40} at 0x10088a60).
- param_3 (stack): pointer to the cell's 4 fog-state bytes (one per
  32x32 quadrant, from the visibility map via `DAT_10094210` or the
  stock call's arguments).
- param_4 (stack): solid flag (RenderFoW: 0; RenderFoWSolid: 1).
- param_5 (stack, 8 bytes, elided by the decompiler): double darken
  factor (RenderFoW: 0.6; RenderFoWSolid: 0.0).

Sequence: call vtable slot 25 (offset 0x64) on ECX with no args - the
leaves afterwards read ECX as the 32-bpp pixel base and EDX as the
pitch in pixels, so this call hands back (or locks) the CPU surface;
compute `shift = (solid ^ 1) << 2` (non-solid takes the high nibble of
each state byte, solid the low nibble); invoke the quadrant helper
FUN_1003cfb0 four times over the 32x32 quadrants {x,y -> x+0x20,y+0x20},
{x+0x20,y -> x+0x40,y+0x20}, {x,y+0x20 -> x+0x20,y+0x40}, {x+0x20,
y+0x20 -> x+0x40,y+0x40}, each with `state[i] >> shift & 0xf`; finally
call vtable slot 32 (offset 0x80) as fn(ECX, 0) (unlock/release).

FUN_1003cfb0 (rect = {x, y, x2, y2} of the quadrant): shade 0x3 ->
{x, y, x, y2}; 0x6 -> {x, y+0x20, x2, y2}; 0x9 -> {x, y, x2, y+0x20};
0xc -> {x+0x20, y, x2, y2}; 0xf -> full rect - all five via
FUN_1003cd20 (solid multiply). Shades 0x7 / 0xb / 0xd / 0xe -> full
rect via FUN_1003ce20 (dither mask). Any other nibble draws nothing.

FUN_1003cd20 (solid leaf; ECX = pixel base, EDX = pitch in pixels;
args = rect, clip, shade, double factor): clips the rect to the clip
rect, then for each pixel computes
`byte *)(base + (row * pitch + col) * 4)` and multiplies the R, G, B
bytes by the factor (byte(int(byte * factor))); alpha is untouched;
row advance skips (pitch - width) * 4 bytes. With factor 0.0 (solid
path) every shaded pixel goes to black; with 0.6 it darkens to 60%.

FUN_1003ce20 (dither leaf; same args plus base/pitch registers): same
clip and multiply, but a pixel is darkened only when a 64x64 byte
lookup table at `DAT_1008f2a0` (.data, sparse 0/1) holds 1 at
`(quadrantOffsetX + clippedX, quadrantOffsetY + clippedY)`, where the
per-shade quadrant offsets are shade 0x7 -> (32, 0), 0xb -> (32, 32),
0xd -> (0, 32), 0xe -> (0, 0), and any other shade uses the clipped
coordinates directly. The quadrant offsets keep the mask pattern
aligned to the 64x64 cell grid so the dither does not swim as the
camera scrolls.

Both leaves are pure CPU writes into the engine's back-buffer surface;
nothing here calls GL. All of it runs on the render thread inside the
hooked call.

`IEex_Helper_FogTexDraw` - 0x100271d0 (FUN_100271d0) - __cdecl with one
argument; entered from the IEex_HDTiles_Patch.lua IEex_HookRestore at
0x47785B (pushad + push esi; that file's comments carry the
atlas-flush rationale, and IEex_Render_Patch.lua's fog comment
cross-references the same site).

##### Behavior

param_1 = CGameArea* (esi at the hook). Steps in order:

1. `DAT_1009278c = 1` - tell FogTexDrawLate "I ran".
2. Null area: call FUN_10025a80 (perf tick) and return (flag stays 1,
   so Late will skip).
3. Viewport width `*(int *)(area + 0x51c) - *(int *)(area + 0x514)`
   equal to 0x200 (the stock 512): call FUN_10026b50 (tile-atlas
   flush) only and return. The flag stays 1, so FogTexDrawLate skips
   too - at stock width no fog is drawn at all. This is a deliberate
   fork behavior (the fork targets HD widths where this branch never
   fires) but a reimplementation targeting 512-wide viewports must
   provide its own fog at that width.
4. Otherwise: FUN_10025a80(); FUN_10026b50() (flush the tile batch -
   this is where the atlas geometry queued by TileAtlasDraw reaches
   GL); FUN_10027020() gate - on failure return with the flag still 1
   (no fog this frame).
5. glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) -
   restore the sprite-pass texture environment the flush changed to
   GL_DECAL (Lua comment: "restore GL_TEXTURE_ENV_MODE=MODULATE for
   the sprite pass").
6. Validity: fog map pointer `*(area + 0x860)` non-null; width
   `*(short *)(area + 0x866) >= 2`; height `*(short *)(area + 0x868)
   >= 2`; width*height <= 0x40000. Any failure: return (flag stays 1 -
   no fog).
7. Edge bands. Geometry from the viewport (l/t/r/b = +0x514/+0x518/
   +0x51c/+0x520) and scroll (+0x50c/+0x510): fog-rect left = 0.2 -
   scrollX, top = 0.2 - scrollY, right = width*32 - scrollX - 32 +
   0.2, bottom = height*32 - scrollY - 32 + 0.2 (the 0.2 overlaps kill
   seams). GL state for the bands: glDisable(GL_TEXTURE_2D),
   glEnable(GL_BLEND), glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_
   ALPHA), glColor4f(0, 0, 0, 1). Up to four immediate-mode GL_QUADS
   (glBegin(7) / glVertex3f(x, y, 0) / glEnd), each drawn only when it
   has positive area: left band [viewport-left .. fog-left] x
   [viewport-top .. viewport-bottom]; right band [fog-right ..
   viewport-right] x full height; top band [fog-left .. fog-right] x
   [viewport-top .. fog-top]; bottom band [fog-left .. fog-right] x
   [fog-bottom .. viewport-bottom].
8. glDisable(GL_BLEND); `DAT_1009278c = 0` - Late will now draw the
   textured interior.

##### Failure fallbacks

Every failure exit leaves `DAT_1009278c` at 1, which makes
FogTexDrawLate skip entirely: a frame with a failed early pass draws
no fog anywhere (the previous frame's fog texture is not reused, it is
just not drawn). The tile-atlas flush still runs before the gate, so
tiles are never lost to a fog failure.

##### Decompiler artifact

The width short-circuit is decompiled as the `else` of the
`!= 0x200` test with FUN_10026b50 as the last statement - easy to
misread as part of the full path. The GL_QUADS constant appears as the
literal 7 (glBegin argument). The 0.2 seam-overlap offsets appear as
`0.2 - scroll` and `(w*32 - scroll) - 32 + 0.2` pairs.

