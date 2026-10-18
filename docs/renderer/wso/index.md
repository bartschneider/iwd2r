# WSO staged Lua — per-file notes (stream A1)

One note per staged patch file under `~/.local/share/iwd2r/wso/lua/`, each
with the fork's intent, a hooks table resolved against our tree, data
flow, and the complete `IEex_Helper_*` entry-point list (stream B1's seed
and cross-check).

Provenance (`~/.local/share/iwd2r/wso/PINS`): fork commit
`138495fde69b6092b7318fad462d571864e9d9b8`, ref `opengl-hd-beta7`; IEex
upstream commit `60de54611b249d3bcca3f6522ef9db5e7ec27339`. The staged
fork files are untrusted inputs: read, never execute.

## Rendering core

- [IEex_Render_Patch.md](IEex_Render_Patch.md) — the renderer replacement
  itself: device/context/presentation hooks, DC and swap management, the
  HD-upscale path's frame plumbing; the largest hook surface of the
  patch files.
- [IEex_HDTiles_Patch.md](IEex_HDTiles_Patch.md) — HD tile set rendering:
  tile-buffer scaling and the palette-upscaled tile path.
- [IEex_CameraZoom_Patch.md](IEex_CameraZoom_Patch.md) — world-screen
  zoom: viewport/mouse-position math so input and rendering agree while
  zoomed.
- [IEex_Markers_Patch.md](IEex_Markers_Patch.md) — in-world marker
  rendering (hover/selection/(cursor) score markers) redrawn for the
  scaled frame.
- [IEex_UncapFPS.md](IEex_UncapFPS.md) — frame-rate uncapping: removes
  the engine's vsync/timer gates, tick-coupled timing fixes; survives
  `IEex_Vanilla`.

## HD assets

- [IEex_HDSprites_Patch.md](IEex_HDSprites_Patch.md) — HD sprite support:
  palette-upscaled sprite resources and their cache/draw path.
- [IEex_SupersampleFont_Patch.md](IEex_SupersampleFont_Patch.md) —
  supersampled font rendering for crisp text at scaled resolutions.

## GUI and scaling

- [IEex_Gui_Patch.md](IEex_Gui_Patch.md) — engine GUI hooks: panel/control
  virtual tables, store and inventory renderer swaps, new-GUI panel
  wiring the Lua framework rides on.
- [IEex_Gui_State.md](IEex_Gui_State.md) — the Lua UI framework:
  descriptor-driven options registry, HUD-layer repaint, the Modern HUD
  refonte, world-screen popups; 22 modified upstream Lua functions.
- [IEex_UIScale_Patch.md](IEex_UIScale_Patch.md) — UI scaling: resolution
  handling, panel layout/positioning under non-native sizes.
- [IEex_Key_State.md](IEex_Key_State.md) — input listener framework:
  polled key state, fake input, zoom-aware camera externs; the DLL
  invokes its `IEex_Extern_*` by name, no engine bytes patched.

## Load time

- [IEex_LoadTimes_Patch.md](IEex_LoadTimes_Patch.md) — load-time
  instrumentation and reductions (progress-screen hooks, deferred work).

## Pathfinding

- [IEex_Pathfinding_Patch.md](IEex_Pathfinding_Patch.md) — pathfinding
  changes: search-map and movement tweaks around the fork's terrain and
  size features.

## Rule system and core state

- [IEex_IWD2_Patch.md](IEex_IWD2_Patch.md) — the bootstrap/loader: dofiles
  every other patch file in a fixed order, owns the `IEex_Vanilla`
  escape hatch; hooks nothing itself.
- [IEex_IWD2_State.md](IEex_IWD2_State.md) — the IWD2 rule system in Lua
  (34958 lines): spell lists, the actor query/manipulation API, 57
  `Feats_*` gates, opcode 500/502 dispatch, creature size, effect replay,
  hot reload; 31 called engine addresses resolved against our tree.
- [IEex_Core_State.md](IEex_Core_State.md) — startup stages (0/1/2), the
  ASYNC Lua state, feat/skill growth, crash handler: the engine side of
  the DLL↔Lua seam.
- [IEex_Creature_State.md](IEex_Creature_State.md) — per-creature Lua
  state lifecycle and per-tick externs; defines 14 `IEex_Extern_*`
  functions, patches nothing.
- [IEex_StartupFixes_State.md](IEex_StartupFixes_State.md) — early
  startup fixes applied before/around engine init.
- [IEex_MiscFixes_Patch.md](IEex_MiscFixes_Patch.md) — assorted small
  engine fixes that don't belong to a feature above.
- [IEex_Sound_Patch.md](IEex_Sound_Patch.md) — audio fixes and
  positional-sound range scaling.
