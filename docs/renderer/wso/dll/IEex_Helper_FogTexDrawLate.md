# IEex_Helper_FogTexDrawLate

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10027680` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10027680.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_HDTiles_Patch.lua`, `IEex_Render_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_10027680(undefined1 *param_1)

{
  int iVar1;
  int iVar2;
  char cVar3;
  undefined1 *puVar4;
  int iVar5;
  byte *pbVar6;
  byte *pbVar7;
  int iVar8;
  int iVar9;
  float fVar10;
  float fVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  float fVar15;
  float fVar16;
  float fVar17;
  int local_8;

  if (DAT_1009278c != '\0') {
    return;
  }
  if (param_1 == (undefined1 *)0x0) {
    return;
  }
  cVar3 = FUN_10027020();
  if (cVar3 == '\0') {
    return;
  }
  iVar5 = (int)*(short *)((int)param_1 + 0x868);
  pbVar6 = *(byte **)((int)param_1 + 0x860);
  iVar9 = (int)*(short *)((int)param_1 + 0x866);
  if (pbVar6 == (byte *)0x0) {
    return;
  }
  if (iVar9 < 2) {
    return;
  }
  if (iVar5 < 2) {
    return;
  }
  if (0x40000 < iVar5 * iVar9) {
    return;
  }
  iVar1 = *(int *)((int)param_1 + 0x50c);
  iVar2 = *(int *)((int)param_1 + 0x510);
  param_1 = &DAT_103df002;
  puVar4 = param_1;
  iVar8 = iVar9;
  local_8 = iVar5;
  pbVar7 = pbVar6;
  do {
    do {
      if ((*pbVar6 & 0x3f) == 0) {
        cVar3 = ((char)*pbVar6 >> 7 & 0x81U) - 1;
      }
      else {
        cVar3 = '\0';
      }
      *(undefined2 *)(puVar4 + -2) = 0;
      pbVar6 = pbVar6 + 1;
      *puVar4 = 0;
      puVar4[1] = cVar3;
      iVar8 = iVar8 + -1;
      puVar4 = puVar4 + 4;
    } while (iVar8 != 0);
    puVar4 = param_1 + iVar9 * 4;
    pbVar6 = pbVar7 + iVar9;
    local_8 = local_8 + -1;
    iVar8 = iVar9;
    param_1 = puVar4;
    pbVar7 = pbVar6;
  } while (local_8 != 0);
  if (DAT_104df0d8 != 0) {
    cVar3 = (*DAT_104df004)(DAT_104df0d8);
    if (cVar3 == '\0') {
      DAT_104df0d8 = 0;
      DAT_104df55c = 0;
    }
    else if (DAT_104df0d8 != 0) goto LAB_1002778d;
  }
  (*DAT_103defd4)(1,&DAT_104df0d8);
LAB_1002778d:
  (*DAT_104df5a4)(0xde1);
  (*DAT_104df0e8)(0xde1,DAT_104df0d8);
  if ((iVar9 == DAT_104df55c) && (iVar5 == DAT_104df3b4)) {
    (*DAT_104df064)(0xde1,0,0,0,iVar9,iVar5,0x1908,0x1401,&DAT_103df000);
  }
  else {
    (*DAT_104df5fc)(0xde1,0,0x1908,iVar9,iVar5,0,0x1908,0x1401,&DAT_103df000);
    DAT_104df3b4 = iVar5;
    DAT_104df55c = iVar9;
    (*DAT_103dec00)(0xde1,0x2800,0x2601);
    (*DAT_103dec00)(0xde1,0x2801,0x2601);
    (*DAT_103dec00)(0xde1,0x2802,0x812f);
    (*DAT_103dec00)(0xde1,0x2803,0x812f);
  }
  (*DAT_104df0c8)(0x2300,0x2200,0x46040000);
  (*DAT_104df5a4)(0xbe2);
  (*DAT_103ded84)(0x302,0x303);
  (*DAT_104df570)(0x3f800000,0x3f800000,0x3f800000,0x3f800000);
  fVar14 = 0.2 - (float)iVar1;
  fVar15 = 0.2 - (float)iVar2;
  fVar10 = (float)((iVar9 * 0x20 - iVar1) + -0x20) + 0.2;
  fVar11 = (float)((iVar5 * 0x20 - iVar2) + -0x20) + 0.2;
  fVar12 = (float)iVar9;
  fVar16 = (fVar12 - 0.5) / fVar12;
  fVar13 = (float)iVar5;
  fVar17 = (fVar13 - 0.5) / fVar13;
  (*DAT_104df5c8)(7);
  (*DAT_104df018)(0.5 / fVar12,0.5 / fVar13);
  (*DAT_103defe8)(fVar14,fVar15,0);
  (*DAT_104df018)(0.5 / fVar12,fVar17);
  (*DAT_103defe8)(fVar14,fVar11,0);
  (*DAT_104df018)(fVar16,fVar17);
  (*DAT_103defe8)(fVar10,fVar11,0);
  (*DAT_104df018)(fVar16,0.5 / fVar13);
  (*DAT_103defe8)(fVar10,fVar15,0);
  (*DAT_104df208)();
  if (DAT_104df0ed != '\0') {
    DAT_104df228 = DAT_104df228 + 1;
    DAT_104df3f0 = DAT_104df3f0 + iVar5 * iVar9 * 4;
  }
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

`IEex_Helper_FogTexDrawLate` - 0x10027680 (FUN_10027680) - __cdecl with
one argument; entered from the IEex_Render_Patch.lua hook at 0x477B61
(the CInfinity::PostRender call in CGameArea::Render), pushed before
the RenderFoW dispatch in the same injected stub.

##### Behavior

param_1 = CGameArea* (esi at the hook). Early-outs, in order:
`DAT_1009278c != 0` (FogTexDraw already handled this frame - stock
width or a failure there), null area, FUN_10027020() failure, fog map
null, width < 2, height < 2, width*height > 0x40000. Any of these
returns without touching GL.

Main path:

1. Expand the visibility map (+0x860, width w = +0x866, height h =
   +0x868) into the RGBA scratch `DAT_103df000` (w*h*4 bytes): RGB =
   0; alpha = when `(byte & 0x3f) == 0`: bit7 set -> 0x80 (explored,
   not visible), else 0xFF (never seen); when any of the low 6 bits is
   set -> 0 (visible).
2. Texture management on `DAT_104df0d8`: if a name is held, verify
   with glIsTexture - dead (context loss) resets the name and the
   cached dimensions; then glGenTextures if needed. Same dimensions as
   cached (`DAT_104df55c`/`DAT_104df3b4`) -> glTexSubImage2D the whole
   scratch; otherwise glTexImage2D (full respecify), record the new
   dimensions, and set the filters: GL_TEXTURE_MIN_FILTER (0x2800) and
   GL_TEXTURE_MAG_FILTER (0x2801) = GL_LINEAR (0x2601); GL_TEXTURE_
   WRAP_S (0x2802) and GL_TEXTURE_WRAP_T (0x2803) = GL_CLAMP_TO_EDGE
   (0x812f).
3. Draw state: glEnable(GL_TEXTURE_2D), glBindTexture(fog texture),
   glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE),
   glEnable(GL_BLEND), glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_
   ALPHA), glColor4f(1, 1, 1, 1).
4. One GL_QUADS quad over the fog rectangle (the same left/top/right/
   bottom formulas as FogTexDraw's bands: 0.2 - scroll .. w*32 -
   scroll - 32 + 0.2 per axis), with half-texel UV insets: u from
   0.5/w to (w - 0.5)/w, v from 0.5/h to (h - 0.5)/h (1:1 texel-to-
   pixel mapping, LINEAR filtering). z = 0 throughout; glTexCoord2f
   precedes each glVertex3f.
5. Under the perf-log flag: DAT_104df228++ (fog_draws) and
   DAT_104df3f0 += w*h*4 (fog_upload bytes).

The whole texture is re-uploaded every frame it draws (no dirty-rect
tracking) - the fog_upload_kib CSV column is exactly this cost.

##### Failure fallbacks

All early-outs are silent; the most interesting is `DAT_1009278c`:
whenever FogTexDraw took its stock-width or failure exit, Late skips
by design. Context loss (glIsTexture false) is absorbed by the rename
+ respecify path - the next frame rebuilds the texture from the
scratch, which is rebuilt from the live visibility map every frame, so
there is no staleness window beyond one frame.

##### Decompiler artifact

The alpha computation is decompiled as the single expression
`((char)*pbVar6 >> 7 & 0x81U) - 1` - arithmetic right shift of the
signed char, mask 0x81, minus 1: 0x80 for bit7-set bytes, 0xFF
(reported as char -1) for bit7-clear bytes. The scratch pointer is
written through an overlapping-symbol offset (`&DAT_103df002` with -2
byte offsets) - it is plain sequential RGBA8 output from
`DAT_103df000`. The decompiler also elides that this helper never
calls glGetError and never logs.

