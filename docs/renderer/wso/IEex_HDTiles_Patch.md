# IEex_HDTiles_Patch.lua — HD tile cache, fog-to-texture, tile atlas, UI census

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_HDTiles_Patch.lua`. Five GL
renderer blocks: enlarge the tile-texture cache for high resolutions, turn
fog-of-war rendering into one textured quad, stop a 4K-only crash from the
now-dead fog tile list, batch tile draws into an atlas, and a UI-panel
render-time census. Loads after `IEex_Render_Patch.lua` (IWD2_Patch file
list) and depends on its PostRender hook — see Data flow.

## Intent

All five blocks are OpenGL-only: the file returns immediately when
`[Program Options] 3D Acceleration` is 0 — "the GL tile-texture cache
(CreateSurfaces3d / Render3d) is unused under the software renderer — skip
both patches. See IEex_Render_Patch.lua".

1. **HD tiles (up to 4K).** `CVidInf::CreateSurfaces3d` hardcodes the tile
   texture pool to `m_nVRamSurfaces = 780`. When more tiles are visible
   than the pool holds (1440p ~984, 4K ~2135), overflow tiles fall to the
   slow path in `CInfTileSet::Render3d` and are re-uploaded with
   `glTexImage2D` EVERY frame. The pool bump alone is unsafe: tile GL
   texture ids are computed as `m_nVRamTile + 9`, and the engine's fonts/UI
   use DYNAMIC ids (`glGenTextures`) that pack just above the old 780 pool
   — a grown pool would extend the fixed tile range into those dynamic ids
   and overwrite font textures (garbled text). So the base moves to +0x2000
   (8192), "well clear of the low dynamic font/UI ids".
2. **Fog-of-war → texture.** Stock fog draws ~8000 per-cell quads/fans via
   `FillRect3d`/`RenderFan` (only callers: CVisibility's fog/exploration
   blits). Both heads are killed and the helper reads
   `CVisibilityMap::m_pMap` directly, decoding it to a tiny W×H corner
   texture drawn as ONE bilinear quad, with hard-black margins drawn
   outside the map. The file's estimate: "~47.7% of frame → a few GL
   calls".
3. **Dead fog tile list (4K exit crash).** `CInfTileSet::RenderTexture`
   appends one TEXTURE record per visible 64px tile to the FIXED array
   `CInfTileSet::m_aTextures[2700]` through the UNBOUNDED counter
   `CInfTileSet::m_nTextures` (our tree carries the warning "the number of
   elements is not validated"). At 4K, ~3000+ visible tiles spill past the
   array (`m_aTextures + 2700×16 = 0x8F2700`, exactly the counter) into
   other globals; element 3096 lands on
   `CScreenInventory::OPTION_PAUSE_WARNING.m_pchData`, so the CRT static
   ~CString runs `InterlockedDecrement(0 − 12)` at exit → write AV at
   0xFFFFFFF4 — the 4K-only IEex crash dumps in `<game>\crash`. ≤1440p
   (~1350 tiles) stays under. In GL mode the array's ONLY consumer,
   `CInfTileSet::RenderFogOfWar`, draws through the FillRect3d/RenderFan
   chain block 2 already killed, so the records are dead weight: the append
   is skipped outright (`m_nTextures` stays 0).
4. **Tile atlas** (gated `[IEex Options] "Tile Atlas"`, default 1). Pack
   every per-tile texture into one 4096×4096 atlas and draw all visible
   tiles in ~2 batched `glDrawArrays` instead of ~3295 per-tile
   bind + immediate-mode quad. Measured bottleneck (warm cache, ~0
   uploads/frame) is the bind+draw, not the upload.
5. **UI census.** Time each `CUIPanel::Render` call, bucket by panel
   `m_nID`, and emit ranked "[UIcensus]" lines in IEex.log every 512
   frames while `[Options] "Perf Log"` is on. The helper is self-gated in
   C++ — ~free when logging is off.

## Hooks table

Byte patterns quoted verbatim from the file.

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x5D2A2F | inside `CInfTileSet::Render3d` (entry 0x5D29A0) | src/CInfinity.cpp:604 (marker `// 0x5D29A0` at 603; hook site itself unresolved in our tree) | — | `IEex_AttemptHook(0x5D2A2F, argA, argB, argC)`: argA (already-applied marker) `8D 99 00 20 00 00` = `lea ebx,[ecx+0x2000]`; argC (expected original prefix) `{0x8D, 0x59, 0x09, 0xF6, 0x40}` = `lea ebx,[ecx+9]` (ecx = m_nVRamTile) + first bytes of `test [eax+0x70],2`; argB (new code) `F6 40 70 02 !jmp_dword :5D2A36` — re-runs the displaced test and resumes at 0x5D2A36. Tile ids become `m_nVRamTile + 0x2000` (8192..8192+pool); the detour exists because disp8 (+9) cannot hold the new base in place. |
| 0x7BE029 | inside `CVidInf::CreateSurfaces3d` (entry 0x7BDF90) | src/CVideo3d.cpp:2191 (marker `// 0x7BDF90` at 2190; write site itself unresolved in our tree) | — | `IEex_WriteDword(0x7BE029, 4096)` — the imm32 of `mov [esi+0x67C], 780`, i.e. `CVidInf::m_nVRamSurfaces` (src/CVidInf.h:97), raised to 4096 so the pool holds the HD tile counts. Per the file: "In-memory (survives verify+repair)" — a plain exe patch, no hook. |
| 0x7BD140 | `CVidMode::FillRect3d` | src/CVideo3d.cpp:1873 (exact marker; decl follows) | `IEex_Helper_FogSkip` | `IEex_WriteAssembly`: head → `!jmp_dword >IEex_Helper_FogSkip`; the helper is `__cdecl` `ret 0` (both callers are cdecl calls). The per-cell fog/exploration quad draw stops existing. |
| 0x7BD740 | `CVidMode::RenderFan` | src/CVideo3d.cpp:1989 (exact marker; decl follows) | `IEex_Helper_FogSkip` | Same head overwrite. These two heads are the whole stock fog draw chain; only CVisibility calls them. |
| 0x47785B | inside `CGameArea::Render` (entry 0x477740) — tiles→sprites boundary | src/CGameArea.cpp:5395 (marker `// 0x477740` at 5394; hook site itself unresolved in our tree) | `IEex_Helper_FogTexDraw` | `IEex_HookRestore(0x47785B, 0, 6, {push_all_registers_iwd2 / !push_esi / !call >IEex_Helper_FogTexDraw / pop_all_registers_iwd2})` — esi = CGameArea*. Flushes the tile atlas, decodes the fog map and paints the margins BEFORE the sprite pass. The 6 displaced bytes are the RENDER_MESSAGESCREEN `cmp`, whose eax the pushad/popad preserves; GL_TEXTURE_ENV_MODE=MODULATE is restored after. |
| 0x5D2D75 | inside `CInfTileSet::RenderTexture` (entry 0x5D2D20) | src/CInfinity.cpp:710 (marker `// 0x5D2D20` at 709; hook site itself unresolved in our tree) | — | `IEex_WriteAssembly(0x5D2D75, {"!jmp_dword :5D2DCC", "!nop"})` — the 6-byte `mov ecx,[0x8F2700]` (8B 0D 00 27 8F 00, load of the append counter m_nTextures) becomes `jmp rel32` (E9 52 00 00 00) + nop to the epilogue at 0x5D2DCC (pop edi/esi/ebx; ret 0x18). The tile itself still draws — `CVidTile::RenderTexture` runs at 0x5D2D70, before the append — but `m_aTextures` never receives records and `m_nTextures` stays 0. |
| 0x7C64F0 | `CVidTile::RenderTexture` | src/CVidImage.cpp:1122 (exact marker; decl follows) | `IEex_Helper_TileAtlasDraw` | Head overwrite marshalling `__thiscall(this, nTextureId, rDest&, x, y, dwFlags)` to the __stdcall export: `!mark_esp`, four `!marked_esp !push([esp+…])` (0x14/0x10/0x0C/0x04), `!call >IEex_Helper_TileAtlasDraw`, `!ret_word 14 00` (ret 0x14). Draws the tile's quad from the shared atlas instead of bind + immediate-mode quad. Atlas feature only. |
| 0x7C6187 | inside `CVidTile::ReadyTexture` (first overload, decl src/CVidImage.h:58) | src/CVidImage.cpp:976 (marker `// 0x7C6187` not present; decl at 976) | `IEex_Helper_TileAtlasUpload` | `IEex_HookRestore(0x7C6187, 0, 6, {push_all / !call >IEex_Helper_TileAtlasUpload / pop_all})` after the engine's per-tile `glTexImage2D`; the 6 displaced bytes are `mov edx,[0x8CF6D8]` (g_pBaldurChitin — no marker in our tree). Copies `CVidTile::m_pPixels` (0xA09FC8) into the tile's atlas cell. Fires only on cold/lighting re-uploads (~0/frame warm). |
| 0x4D45A4 | inside `CUIManager::Render` (entry 0x4D4540) — the `CUIPanel::Render` call slot | src/CUIManager.cpp:410 (marker `// 0x4D4540` at 409; write site itself unresolved in our tree) | `IEex_Helper_UIPanelRenderTimed` | `IEex_WriteAssemblyAuto` allocates a cave: push_all, `push ecx` (CUIPanel*), `!call >IEex_Helper_UIPanelRenderTimed`, pop_all, `!jmp_dword :4D45A9`; then `IEex_WriteAssembly(0x4D45A4, jmp {cave,4,4})` replaces the 5-byte `e8` call to `CUIPanel::Render` (0x4D3100) — exactly the call slot, falling through to 0x4D45A9. Loop registers esi/eax preserved by push_all. The helper calls the original render, then times it. |

Cited but not hooked: `CInfTileSet::RenderFogOfWar` 0x5D2DE0 — our tree
names it `sub_5D2DE0` (src/CInfinity.cpp:736, marker at 735) — the only
consumer of `m_aTextures`, whose draw chain
(BltFogOWar3d → BltVisibility3d/BltExploration3d → FillRect3d/RenderFan)
block 2 already killed; the globals `CInfTileSet::m_aTextures[2700]`
0x8E7E40 (src/CInfinity.cpp:44), `CInfTileSet::m_nTextures` 0x8F2700
(src/CInfinity.cpp:47), `CScreenInventory::OPTION_PAUSE_WARNING` 0x8F3FC0
(src/CScreenInventory.cpp:89), `CVidTile::m_pPixels[16384]` 0xA09FC8
(src/CVidImage.cpp:16), all exact in our tree; `CUIPanel::Render` 0x4D3100
(src/CUIPanel.cpp:313, exact); `CVidInf::m_nVRamSurfaces` @+0x67C
(src/CVidInf.h:97); the append-site internals 0x5D2D70 (the
CVidTile::RenderTexture call) and epilogue 0x5D2DCC; the resume target
0x5D2A36; `CGameArea::Render`'s PostRender call at 0x477B61 — installed by
`IEex_Render_Patch.lua`, from which `IEex_Helper_FogTexDrawLate` draws the
decoded fog quad AFTER the object passes ("else corpses/statics/doors/
piles escape fog"). Overflow arithmetic: spill starts at
`m_aTextures + 2700×16 = 0x8F2700`; element 3096 = 0x8F3FC0.

## Data flow

Static one-time surgery inside `IEex_DisableCodeProtection()` /
`IEex_EnableCodeProtection()` (IIFE); no persistent Lua state. After patch
time the engine and the DLL helpers carry everything:

- Fog: the helpers read `CVisibilityMap::m_pMap` directly (bypassing the
  killed FillRect3d/RenderFan chain), decode to the corner texture, and
  draw twice — margins + flush at the 0x47785B hook (before sprites), the
  full quad from Render_Patch's PostRender hook at 0x477B61 (after
  objects). That cross-file dependency is why this file must load after
  IEex_Render_Patch.lua.
- Tile atlas: `IEex_Helper_TileAtlasUpload` fills cells from
  `CVidTile::m_pPixels` on the (rare) ReadyTexture uploads;
  `IEex_Helper_TileAtlasDraw` replaces every per-tile bind/draw; the
  Export_FogTexDraw flush point drives atlas invalidation before fog.
- UI census: state is the per-panel timing buckets DLL-side, flushed every
  512 frames; gated by `[Options] "Perf Log"` in IEex.ini.
- Ini gates read at patch time: `[Program Options] "3D Acceleration"`
  (whole file) and `[IEex Options] "Tile Atlas"` (atlas block; default 1,
  checked `~= 0`).

Stock paths removed: the m_aTextures append (dead in GL mode) and both fog
draw heads. Everything else trampolines or resumes through the original
code.

## IEex_Helper_* entry points

- `IEex_Helper_FogSkip` — installed at 0x7BD140 and 0x7BD740 (cdecl,
  `ret 0`); kills the per-cell fog draw.
- `IEex_Helper_FogTexDraw` — called from the 0x47785B hook; decode +
  margins + atlas flush before sprites.
- `IEex_Helper_FogTexDrawLate` — called from Render_Patch's PostRender
  hook at 0x477B61 (referenced here, installed there; B1 cross-check both
  notes list it).
- `IEex_Helper_TileAtlasUpload` — called from the 0x7C6187 hook; copies
  m_pPixels into the atlas.
- `IEex_Helper_TileAtlasDraw` — installed at 0x7C64F0; the batched
  per-tile draw.
- `IEex_Helper_UIPanelRenderTimed` — called from the 0x4D45A4 cave; times
  each `CUIPanel::Render` and emits "[UIcensus]" lines.

(`IEex_WriteAssembly`, `IEex_WriteAssemblyAuto`, `IEex_WriteDword`,
`IEex_HookRestore`, `IEex_AttemptHook`, `IEex_GetPrivateProfileInt`,
`IEex_DisableCodeProtection`/`IEex_EnableCodeProtection`,
`IEex_FlattenTable` are core-DLL API, not `IEex_Helper_*`.)
