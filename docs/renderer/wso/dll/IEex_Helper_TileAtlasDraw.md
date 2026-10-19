# IEex_Helper_TileAtlasDraw

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10026e30` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10026e30.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_HDTiles_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_10026e30(int param_1,undefined4 param_2,undefined4 param_3,uint param_4)

{
  int iVar1;
  uint uVar2;

  if ((8 < param_1) && (DAT_104df535 != '\0')) {
    iVar1 = 9;
    if (0x1fff < param_1) {
      iVar1 = 0x2000;
    }
    uVar2 = param_1 - iVar1;
    if ((uVar2 < 0x1000) && (*(char *)(DAT_104df0b0 + uVar2) != '\0')) {
      FUN_10026a10(uVar2);
      if (DAT_104df0ed != '\0') {
        DAT_104df030 = DAT_104df030 + 1;
        DAT_104df068 = DAT_104df068 + 1;
        if ((param_4 & 3) != 0) {
          DAT_104df0a4 = DAT_104df0a4 + 1;
        }
      }
    }
  }
  return;
}
```

## Notes

### Shared HD tile-atlas machinery (all helpers in this group)

The staged IEex_HDTiles_Patch.lua replaces the engine's per-tile GL
texture churn with one 4096x4096 RGBA8 atlas. The Lua patch grows the
engine 3D tile pool from 780 to 4096 (IEex_WriteDword(0x7BE029, 4096)
against the `mov [esi+0x67c], 780` in CVidInf::CreateSurfaces3d),
relocates the tile-id base to 0x2000 for ids that would collide with
regular texture names (detour at 0x5D2A2F: original bytes
`8D 59 09 F6 40 70 02` = `lea ebx,[ecx+9]; test byte [eax+0x2],0x70`
replaced by `8D 99 00 20 00 00 F6 40 70 02`, resume 0x5D2A36), and
clamps CInfTileSet's m_nTextures append (fix at 0x5D2D75, see the
TileAtlasUpload note) so the enlarged pool cannot overflow
m_aTextures[2700] at 0x8E7E40. The C++ side (this group) then:

1. uploads each 64x64 tile into the atlas the first time the engine
   uploads it (TileAtlasUpload, hooked after the engine's own
   glTexImage2D in the ReadyTexture path at 0x7C6187),
2. re-routes each tile bind (CVidTile::RenderTexture at 0x7C64F0) to
   emit two textured triangles into a CPU vertex array (TileAtlasDraw),
3. flushes the whole batch with two glDrawArrays calls at the
   tiles-to-sprites boundary (inner FUN_10026b50, driven from the fog
   group's FogTexDraw helper at hook 0x47785B).

Everything in this group is gated on `[IEex Options] "Tile Atlas"` in
`.\Icewind2.ini` indirectly: the Lua patch only installs the hooks when
that key is on (default on), and additionally skips everything when
`[Program Options] "3D Acceleration"` in `Icewind2.ini` is 0 (software
renderer - the patch returns before installing any hook).

#### Fork state (BSS globals)

- `DAT_104df535` (byte): atlas-initialized flag; set by FUN_100265c0
  once GL slots and buffers are ready. TileAtlasDraw and the flush both
  no-op while it is 0.
- `DAT_104df4f4` (GLuint): the atlas texture name; 0 until created.
- `DAT_104df398`: opaque-batch vertex array, malloc(0x60000) = 393216
  bytes = 24576 vertices = 4096 tiles (6 verts x 4 floats x 4 bytes).
- `DAT_104df3dc`: blend-batch vertex array, same size.
- `DAT_104df0b0`: calloc(0x1000) uploaded-flags, one byte per atlas
  slot (4096 slots); a tile is emitted only when its byte is set.
- `DAT_104df408`: calloc(0x4000000) = 64 MB CPU shadow of the atlas,
  row-major RGBA8; enables one-shot texture restore after context loss.
- `DAT_104df3c8` (dword): opaque-batch float count (vertex-array bytes
  / 4); `DAT_104df5d4`: blend-batch float count. Both reset to 0 by the
  flush.
- `DAT_104df08e` (byte): alt-tab flag consulted by the restore check.
- Perf counters (bumped only while `DAT_104df0ed`, the perf-log flag,
  is set; mapped to CSV columns by the perf-log subsystem documented in
  the fog group): `DAT_104df030` = tile_binds (TileAtlasDraw calls that
  emit), `DAT_104df068` = tile_draws, `DAT_104df0a4` = tile_blend
  (emits whose tile flags have bits 0-1 set), `DAT_103ded8c` = atlas_up
  (glTexSubImage2D uploads).

#### GL entry-point slots (resolved by inner FUN_100265c0)

FUN_100265c0 does GetModuleHandleA("opengl32.dll") + GetProcAddress for
each of these and fails the whole init if any of ten required slots is
null. All are core GL 1.1:

| Slot | Function | Required |
|---|---|---|
| DAT_104df598 | glEnable | yes |
| DAT_104df530 | glDisable | no |
| DAT_104df05c | glEnableClientState | no |
| DAT_104df3ec | glDisableClientState | no |
| DAT_104df5b4 | glBindTexture | yes |
| DAT_104df0dc | glBlendFunc | no |
| DAT_104df3e4 | glGenTextures | yes |
| DAT_104df370 | glTexImage2D | yes |
| DAT_104df5d8 | glTexSubImage2D | yes |
| DAT_103ded90 | glTexParameteri | yes |
| DAT_104df094 | glTexEnvf | no |
| DAT_104df3bc | glColor4f | no |
| DAT_104df03c | glVertexPointer | yes |
| DAT_104df51c | glTexCoordPointer | yes |
| DAT_104df558 | glDrawArrays | yes |
| DAT_104df3b8 | glIsTexture | yes |

Optional slots are called only after the required-ten check passes, so
in practice they always exist on any GL 1.1 implementation.

#### Atlas GL init (inner FUN_100265c0)

Idempotent via `DAT_104df535`; returns 1/0. Resolves the slot table,
allocates the four buffers above, creates the atlas texture
(glGenTextures; glTexImage2D 4096x4096 GL_RGBA8 with data = NULL;
glTexParameteri GL_TEXTURE_MIN_FILTER/MAG_FILTER = GL_NEAREST 0x2600;
GL_TEXTURE_WRAP_S/T = GL_CLAMP_TO_EDGE 0x812f), and logs "Tile Atlas:
GL ready (4096x4096)" through the DLL logging function `DAT_100944fc`.
Any missing required slot or failed allocation leaves the flag clear
and returns 0; callers then no-op (the engine's own texture path keeps
running untouched, since the hooks check the gate first).

#### Context-loss / alt-tab restore (inner FUN_10026870)

Runs at the top of every flush. When the atlas is alive
(`glIsTexture(DAT_104df4f4)` true) and the alt-tab flag is clear it
returns immediately. Otherwise it resolves the current GL context once
(wglGetCurrentContext into `DAT_104df758`, flag `DAT_104df6d0`); with
no current context it returns without touching anything (the restore is
deferred to a later frame). With a dead texture name it takes a fresh
glGenTextures, counts the loss in `DAT_104df75c` and logs
"[ATLASDBG] atlas lost #%d ctx=%p tex=%u" on every 120th occurrence,
then re-runs glTexImage2D 4096x4096 from the 64 MB shadow
`DAT_104df408` in one call and re-applies the same filters. Logs "Tile
Atlas: GL context lost -> restored from shadow" or "Tile Atlas: alt-tab
regain -> restored from shadow" according to which condition fired.
The shadow stays valid because TileAtlasUpload maintains it on every
upload; tiles never uploaded before the loss come back black and are
re-uploaded by the engine path as the game re-references them.

#### Batched flush (inner FUN_10026b50)

Gate: `DAT_104df535` and at least one of the two batch counters
non-zero; then FUN_10026870() to ensure the texture is alive. GL state
per flush: glEnable(GL_TEXTURE_2D 0xde1), glBindTexture(atlas),
glTexEnvf(GL_TEXTURE_ENV 0x2300, GL_TEXTURE_ENV_MODE 0x2200, GL_DECAL),
glColor4f(1,1,1,1), glEnableClientState(GL_VERTEX_ARRAY 0x8074) +
(GL_TEXTURE_COORD_ARRAY 0x8078). Opaque batch (array `DAT_104df398`):
glDisable(GL_BLEND 0xbe2), glVertexPointer(2, GL_FLOAT 0x1406,
stride 16, DAT_104df398), glTexCoordPointer(2, GL_FLOAT, 16,
DAT_104df398+8), glDrawArrays(GL_TRIANGLES 4, 0, count>>2). Blend batch
(array `DAT_104df3dc`): glEnable(GL_BLEND), glBlendFunc(GL_SRC_ALPHA
0x302, GL_ONE_MINUS_SRC_ALPHA 0x303), same pointers on `DAT_104df3dc`,
glDrawArrays, glDisable(GL_BLEND). Then glDisableClientState twice and
both counters reset to 0. The count argument is the dword count >> 2 =
vertex count (4 dwords per vertex). There is no depth/scissor/matrix
manipulation: the flush inherits whatever transform the engine had
current at the tiles-to-sprites boundary.

The flush's tail is `if (flushed && in_ECX != 0)
(*(code *)0x7bd270)(*(undefined4 *)(in_ECX + 0x1b2), in_ECX + 0x48)` -
an engine-address call (0x7BD270) taking two pointers derived from the
`this` the hook chain left in ECX, most likely a CVidInf queue-flush
that presents the modified tile data. The callee was not identified
from the DLL (it lives in the engine binary, which is out of scope for
this stream). Note that when the flush is reached via FogTexDraw's
hook, ECX holds whatever the engine had there - the guard only checks
non-zero. Treat this call as an open question for the renderer design.

#### Vertex emit (inner FUN_10026a10)

Decompiled signature `(in_ECX = vertex buffer base, in_EDX = int*
write cursor, in_XMM2_Da = screen x, in_XMM3_Da = screen y)` - the
decompiler mangled the register convention; functionally it is
`emit(index, x, y)` appending into the batch selected by the caller.
Capacity check: cursor + 0x18 dwords must stay below 0x18001 (6 verts
x 4 floats = 24 dwords per tile inside the 0x60000-byte arrays). Each
64x64 tile becomes 6 vertices, 16 bytes apart: triangle 1
(x,y) - (x,y+64) - (x+64,y+64), triangle 2 (x,y) - (x+64,y+64) -
(x+64,y). UVs from the slot index: u0 = (index % 64) * 0.015625 +
0.00012207031, v0 = (index / 64) * 0.015625 + 0.00012207031,
u1/v1 = +0.015625 - 0.00024414062 (1/64 tile size; half-texel inset
0.5/4096 on the min edge, 1.0/4096 on the max edge - NEAREST filtering
makes the asymmetry cosmetic). Cursor advances 0x18 dwords.

#### Threads and synchronization

All of this group runs on the engine's GL thread inside the frame:
the Lua hooks replace engine call sites in the render loop. No locks,
no atomics, no worker threads are involved anywhere in the group; the
only cross-thread surface is the perf-log reader (fog group), which
reads the counters from the same thread. The 64 MB shadow is written
on the same thread as the uploads.

#### Lua hook topology (IEex_HDTiles_Patch.lua)

- TileAtlasDraw: mark_esp stdcall marshal at 0x7C64F0 (four marked
  pushes [esp+0x14]/[esp+0x10]/[esp+0x0C]/[esp+0x04]) calls
  IEex_Helper_TileAtlasDraw then `!ret_word 14 00` - the stock
  CVidTile::RenderTexture body is replaced entirely (thiscall this +
  4 stack args = ret 0x14). Whatever the helper does not emit is not
  drawn that frame.
- TileAtlasUpload: IEex_HookRestore(0x7C6187, 0, 6, push_all, call
  IEex_Helper_TileAtlasUpload, pop_all); the displaced 6 bytes are
  `mov edx,[0x8CF6D8]` (the engine global the helper itself reads as
  _DAT_008cf6d8), re-executed from the trampoline after the helper
  returns.
- UIPanelRenderTimed: 5-byte call slot at 0x4D45A4 (the CUIPanel::
  Render call inside CUIManager::Render, target 0x4D3100) rewritten
  with !jmp_dword to an IEex_WriteAssemblyAuto stub: push_all, push
  ecx, call IEex_Helper_UIPanelRenderTimed, pop_all, jmp 0x4D45A9.
  ecx at the stub = the CUIPanel this, forwarded as the helper's
  argument.

`IEex_Helper_TileAtlasDraw` - 0x10026e30 (FUN_10026e30) - registered in
the Lua raw-address table (inner FUN_1000e2a0); entered from the
IEex_HDTiles_Patch.lua stdcall marshal replacing CVidTile::
RenderTexture at 0x7C64F0.

##### Behavior

Called with the tile id the engine was about to bind (first marshaled
argument) plus three further stack words, of which only the last is
read: `param_4 & 3` (the tile's blend flags) gates the tile_blend
counter. Steps:

1. Gate: `tileId > 8 && DAT_104df535` - ids 0-8 are reserved engine
   texture names and never atlas slots.
2. Index = tileId - 9 for ids <= 0x1fff, tileId - 0x2000 above (the
   relocated base). Anything with index >= 0x1000 is ignored (outside
   the 4096-slot atlas).
3. If the uploaded flag byte `DAT_104df0b0[index]` is set, call the
   emitter FUN_10026a10 to append the tile's two triangles (6
   vertices) to the current batch; the caller (the marshal stub)
   selects opaque vs blend by tile state, feeding either
   `DAT_104df398`/`DAT_104df3c8` or `DAT_104df3dc`/`DAT_104df5d4`.
4. Under the perf-log flag: DAT_104df030++ (tile_binds),
   DAT_104df068++ (tile_draws), and DAT_104df0a4++ (tile_blend) when
   `(param_4 & 3) != 0`.

The helper only queues geometry; nothing reaches GL until the batched
flush runs later in the frame (see COMMON).

##### Failure fallbacks

Atlas not initialized, tile id in 1-8, index out of range, or tile
never uploaded: the helper returns without emitting; since the Lua
stub replaced the stock RenderTexture with a bare return, that tile is
simply not drawn this frame. There is no fallback to the engine bind -
the Lua patch only enables itself when the whole atlas path is
available, and the flush counters make a silently-empty batch visible
in the perf CSV.

##### Decompiler artifact

The four stack arguments are marshaled by the Lua stub; the decompiler
shows them as (int, undefined4, undefined4, uint). Only arg 0 (tile
id) and arg 3 (flags, masked 3) are read - args 1-2 appear to be the
stock function's screen coordinates and are unused here.

