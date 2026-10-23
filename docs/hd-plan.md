# The HD plan

How iwd2r goes from a faithful port to a remaster, in what order, and why the order is
fixed. Status: plan, 2026-10-23. Owner: project 6 in the [README](../README.md#status).
The render seam it builds on is project 4
([design](superpowers/specs/2026-10-13-iwd2r-render-api-design.md), done); the scalable
text path is [its own design](superpowers/specs/2026-10-14-iwd2r-scalable-text-design.md).

## 1. The one rule: vanilla first

Nothing in this plan starts before project 3 is complete: the engine runs the game end to
end, every function checked against the original, with the verification harness
(oracle cases, replay checkpoints, differential traces) green. Three reasons:

- **A remaster of a wrong engine remasters the bugs.** Every HD feature draws what the
  simulation says; if the simulation diverges from the original, the pretty frame is a
  pretty lie, and we lose the ability to tell a renderer bug from an engine bug.
- **The original look is the test.** At scale 1 with the classic presenter the pixels must
  equal the original's, frame for frame, in the replay captures. That comparison only means
  something while the engine underneath is the vanilla one.
- **Options stay options.** Every HD feature is a switch in the INI with the vanilla value
  as its default, so the port can always be run as the original for comparison, for mods,
  and for people who want the 2002 game on a 2026 machine and nothing else.

Until then the only renderer work that lands is base-engine recovery: the gaps the audits
found in the vanilla path (the night light map `m_pbmLumNight` is never allocated; the
24→16-bit and tinted 24-bit blits in `CVidBitmap` are stubs). Those are recovery batches
like any other, verified against the original.

## 2. What already exists

The graphics API is SDL3 GPU, full stop. There is no OpenGL anywhere in this plan: the
original's OpenGL 3D mode stays inert in the engine, and IWD2EE's OpenGL/HD renderer is not
ported. What we take from that work is ideas (zoom, HD packs, UI scale, viewport sound),
re-implemented on our own presenter.

| piece | state |
|---|---|
| Render API seam (project 4) | done: the engine draws into one CPU back buffer; a presenter puts it on screen |
| Classic presenter | done, default: SDL3 software blit, pixel-identical to the original at scale 1 |
| SDL3 GPU presenter | done, opt-in (`Presenter=gpu`): Vulkan backend on Linux and Windows, scale filters `nearest`, `sharp`, `smooth`, `fsr`, `nis`; runtime hotkey toggle; falls back to classic when no device exists |
| Region composition | done: each UI panel's 1× rectangle is composited at the UI scale, the world at its own scale, so the UI can be 2× at desktop resolution while the world stays 1× |
| Scalable text | done, opt-in: TrueType text rendering (`Text Renderer=scalable`) |
| Desktop runs | `IWD2R_DESKTOP_OK=1 python3 tools/play.py --sound --resolution desktop --scale 2 ...` |
| Ideas borrowed from IWD2EE's unfinished OpenGL/HD work (not ported) | notes and traces under [renderer/wso/](renderer/wso/index.md): camera zoom, HD tiles, HD sprites, UI scale, supersampled fonts, viewport-relative sound, pathfinding, uncapped FPS, markers, load times. Its author never finished it; these are ideas, not a specification |

Vulkan is reached through SDL3 GPU, never written directly: SDL3 GPU owns the device, the
swapchain and the shader loading, and its other backends (D3D12, Metal) become available by
adding their shader blobs. We write shaders once in GLSL, compile them with
`glslangValidator` and commit the SPIR-V. No raw Vulkan code lives in this repository.

## 3. The phases, in order

Each phase is its own spec and plan under `docs/superpowers/` when its turn comes, runs on
the same batch driver and review process as the recovery, and ships behind a switch. The
order is by dependency: each phase is the thing the next one is verified against.

### Phase 1: the GPU presenter draws the frame from parts, not from one bitmap

Today the GPU presenter uploads the finished CPU back buffer every frame. Phase 1 makes the
engine hand it the parts instead: the world as an atlas of tiles plus the sprite list with
positions and palettes, the fog of war as a texture, the UI panels as their own layers.
The classic presenter keeps compositing on the CPU exactly as now.

- Deliverable: the same pixels as phase 0 at scale 1 (the replay shot comparison proves
  it), with the world, fog and UI as separate GPU resources.
- Why first: every later feature (zoom, HD assets, lighting) needs the world as texture
  data the GPU can sample, not as a finished bitmap.
- Switch: `Presenter=gpu` (unchanged); the parts path is internal.

### Phase 2: camera zoom and smooth scrolling

With the world sampled from an atlas, zoom is a camera transform: the world draws at any
scale between the original 1× and about 0.5× (zoomed out) or 2× (zoomed in), the UI keeps
its own scale, and scrolling becomes sub-pixel. The simulation is untouched: pathing, fog
reveal, picking and triggers still happen in world coordinates at the original resolution.

- Deliverable: mouse-wheel zoom with sane limits, a zoom-to-cursor option, scrolling that
  does not snap to 1× pixels, and picking that stays exact at every zoom.
- Verification: at zoom 1 the frame equals phase 1's; at other zooms the picked object for
  a click equals what the engine picks at 1× for the same world point (a replay with
  `IWD2R_SHOT_PRESENTED` shots plus the state dump).
- Switch: `Zoom=on|off`, default off; `Zoom Min`, `Zoom Max`.
- Idea source: IWD2EE's CameraZoom patch, for what felt right to its players.

### Phase 3: HD UI from packs

The UI panels draw from the original 1× art scaled by the region compositor today. Phase 3
lets a pack in `override/` supply higher-resolution versions of UI art (panels, buttons,
cursors, portraits) and fonts, chosen by the UI scale, with a clean fallback to the 1× art
per asset. Text already has its scalable path.

- Deliverable: a pack format (plain files under `override/hd/` keyed by the original
  resource name and scale; nothing bundled with the engine), per-asset fallback, and a
  layout that uses the extra pixels rather than blowing the 1× panel up.
- Verification: with no pack installed, the frame equals phase 2's at every scale.
- Switch: `UI Scale=1|2|auto` (exists), `HD Packs=on|off`.
- Not in scope: new UI layouts (a Pillars-style HUD is an idea from IWD2EE's notes for a
  later option, not part of HD UI).

### Phase 4: HD sprites and tiles from packs

The same pack mechanism for the world: higher-resolution tiles (the pre-rendered area art)
and creature animations, sampled by the atlas renderer at the zoom level in use, with the
original palette effects (tinting, pseudo-transparency, the blended and shadowed
sequences) reproduced in the shader rather than on the CPU. Assets come from packs players
install; the engine ships none and the original data stays the fallback for every missing
asset.

- Deliverable: tile and sprite packs, palette-effect shaders that match the CPU effects at
  1× (compared pixel for pixel in the replay shots before any HD asset is used), mipmapped
  sampling at zoom.
- Verification: no pack installed → identical frames; with a pack, the sprite's position,
  size and hit box equal the 1× ones.
- Switch: `HD Packs=on|off` (shared with phase 3).

### Phase 5: lighting and time of day

The original tints the scene by time of day through its light maps and the global tint
(`CGameArea::GetTintColor` and the night map). Phase 5 keeps that exact behaviour as the
default and adds, as an option, GPU lighting over the pre-rendered scenes: per-pixel use of
the light map, light sources from spell and item effects, and a normal-map pack format for
areas whose authors supply one. 2.5D in the Pillars of Eternity sense: lighting the painting,
not rebuilding it in 3D.

- Prerequisite: the vanilla night-map path recovered (the engine gap above), so there is a
  correct baseline to compare against.
- Verification: `Lighting=classic` equals the vanilla frames; `Lighting=hd` is judged by eye
  and by screenshots in the ledger, never by parity.
- Switch: `Lighting=classic|hd`, default classic.

### Phase 6: quality of life around the picture

Small, independent options, each its own switch, each verified by parity when off:
uncapped frame rate with the simulation still ticking at the original rate (IWD2EE's
UncapFPS idea), viewport-relative sound (its Sound patch), supersampled fonts for the 1×
text renderer, marker and selection-circle styles, faster load times by caching decoded
resources. None of these changes the simulation.

## 4. How each phase is verified

- **Parity when off.** With every HD switch at its default, the presented frame is compared
  to the classic presenter's frame at scale 1 in the replay captures (`world_entry`,
  `first_fight`, `walk_and_talk`, and the scenarios recorded since). Zero differing pixels
  is the gate.
- **Picking and state when on.** A replay runs with the feature on; the state dumps must
  stay identical to the vanilla run at every checkpoint, since no HD feature may touch the
  simulation. The presented shots go to the ledger for a human look.
- **Packs are data.** A pack is tested by installing it into `override/` in a sandbox run;
  the engine's own test suite never contains HD assets.

## 5. What this plan does not do

- No 3D, no new game systems, no changes to the rules. Those are project 5 (native IEex
  layer and Lua API) and the later options project, after this one.
- No bundled assets: packs are the player's, like WeiDU mods are.
- No raw Vulkan, D3D12 or Metal code; SDL3 GPU is the only graphics API we program.
- No OpenGL, and no reconstruction of IWD2EE's unfinished OpenGL renderer; its notes are read
  for ideas and credited where an idea is taken.

## 6. Open questions, to settle when phase 1 is specified

- Atlas layout for areas larger than the maximum texture size on older GPUs (tiling the
  atlas versus streaming).
- Whether the fog of war stays a per-cell texture or becomes a mask at world resolution.
- The pack manifest format: one file per pack naming the assets it provides and the scale,
  or discovery by file name alone.
