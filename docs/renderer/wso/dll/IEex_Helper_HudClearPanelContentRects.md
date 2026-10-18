# IEex_Helper_HudClearPanelContentRects

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011fb0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011fb0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011fb0(void)

{
  DAT_104df4dc = 0;
  return 0;
}
```

## Notes

### Shared hud-layer machinery (all helpers in this group)

The DLL retains the rendered UI in its own OpenGL texture (the "HUD
layer") so panels repaint only when their state changes, and composites
retained texels over the world framebuffer. Staged-patch rationale
(`IEex_Gui_Patch.lua`, hook at 0x68DFB6): "renders the UI into a
persistent game-res FBO (panels repaint only when really dirtied) and
composites it over the world with one blended quad. Falls back to a plain
CUIManager::Render call when the layer is unavailable (software / no FBO
ext / [IEex Options] 'HUD Layer'=0)".

#### Layer resources and canvas

- Layer texture `DAT_103deff4` and layer FBO `DAT_104df3d0`; the texture
  is `GL_TEXTURE_2D` / `GL_RGBA8` (0x8058) / `GL_LINEAR` (0x2601) min+mag /
  `GL_CLAMP_TO_EDGE` (0x812F) wrap, attached to the FBO as
  `GL_COLOR_ATTACHMENT0` (0x8CE0). Completeness gate:
  `glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE`
  (0x8CD5).
- Canvas size `DAT_104df564` (w) / `DAT_103deda4` (h). Normally 1:1 with
  the engine screen words at `0x8ba31c`/`0x8ba31e` (read as unsigned
  16-bit words). When the rect table is non-empty and `FUN_100295a0()`
  (the stretched-UI width, ui-scale group) differs from the engine width,
  the canvas becomes a virtual canvas: width = that UI width, height =
  ceil(engineH * uiW / engineW). `DAT_104df506` records which: 0 = 1:1
  with the screen, 1 = scaled virtual canvas (UV math in the composite
  branches on it; the one-shot skip path only diverts to the layer when
  it is 1).
- HD mode (`DAT_104df1fc`, hd-sprite group) keeps a second, engine-side
  frame FBO in `DAT_104df58c`; every layer bind/creation restores that
  FBO afterwards, or 0 (default framebuffer) when HD is off.

#### Ensure step (inner helper FUN_10031ca0, force-decompiled)

Called at the top of every HudLayerRender; returns 1 = layer usable.

1. Gates: ini `[IEex Options] HUD Layer` (default 1, cached in
   `DAT_10092b6c`, negative = not yet read), engine UI manager present
   (`*(*DAT_100944b8 + 0x91c) != 0`), permanent-disable flag
   `DAT_103d4757` clear.
2. Once (guarded by `DAT_100945f7`): `FUN_1002fff0()` resolves the bulk
   GL proc table (0 -> permanent disable with log "HUD Layer: GL/FBO
   procs missing, disabled"); then opengl32 `GetProcAddress` for
   glIsEnabled/glScissor/glGetIntegerv/glDeleteTextures and
   `wglGetProcAddress("glDeleteFramebuffersEXT")`. Any needed EXT proc
   missing -> return 0.
3. Fast path: layer active (`DAT_104df537`), texture still valid
   (`glIsTexture`), canvas unchanged -> 1.
4. Canvas-size change: delete the FBO and texture, reset their slots and
   `DAT_104df537`.
5. Create texture + FBO as above; on `GL_FRAMEBUFFER_COMPLETE`: clear the
   layer to transparent, restore the previous FBO (HD FBO or 0), store
   canvas + `DAT_104df506`, reset the panel hash `DAT_104df060` (full
   invalidate next), log once - "HUD Layer: active (UI composited from
   its own FBO)" or "HUD Layer: active (virtual canvas %dx%d -> screen
   %dx%d)" - set `DAT_104df537` and return 1. Incomplete: restore the
   previous FBO, release the resources, log "HUD Layer: FBO incomplete,
   disabled", set the permanent-disable flag when no logging sink is
   installed, return 0.

#### Frame flow (HudLayerRender wraps CUIManager::Render @0x4D4540)

1. One-shot skip (`DAT_104df08f`, armed by HudLayerSkipFrame): run the
   ensure step; on success with a VIRTUAL canvas, reset the panel hash
   and set the fallback-frame flag `DAT_103d4755`; otherwise (ensure
   failed, or 1:1 canvas) fall through to a plain `0x4d4540` call -
   exactly the pre-layer render path the staged Lua wants for dialog
   frames.
2. Ensure step; then if `DAT_10094507` is set (armed by
   SetMarkerStyle): clear it, reset the panel hash, log "HUD Layer:
   focus regain -> content restore (clear + full invalidate)".
3. Hash the panel state: FNV-1a (offset 0x811c9dc5, prime 0x1000193) over
   up to 64 panels of the CUIManager `+0xb2` list; per panel (only
   entries with non-null `+0x2` pointer) it mixes id `+0x20`, x `+0x24`,
   y `+0x28`, w `+0x34`, h `+0x38` and visibility (`+0xf4 != 0`).
4. Bind the layer FBO, `glViewport(0, 0, canvasW, canvasH)`; when the
   hash CHANGED since last frame: `glClearColor(0,0,0,0)` +
   `glClear(GL_COLOR_BUFFER_BIT)` plus the engine full-invalidate call
   `0x4d45e0(&RECT{0,0,0x7fff,0x7fff})` so every panel repaints into the
   fresh layer.
5. Clear the content-rendered flag `DAT_104df534`, set the
   inside-layer-render marker `DAT_103d4754` and the current-manager
   slot `DAT_103ded68`, then call `0x4d4540` (CUIManager::Render). The
   engine hooks installed at init (SPELLICON @0x66D5A0 / @0x66DC30,
   DOLLTHR throttle @0x626352) repaint changed controls INTO the bound
   FBO and raise `DAT_104df534`.
6. If nothing rendered (`DAT_104df534 == 0`), run the clean-frame
   composite FUN_10035600 (below). Clear `DAT_103d4754`,
   `DAT_103ded68`, `DAT_104df534`.

#### Clean-frame composite (inner helper FUN_10035600)

Runs only when no control repainted this frame and a current CUIManager
is recorded; draws the RETAINED layer texture onto the world target (HD
frame FBO or 0). Saves scissor state (`glIsEnabled(GL_SCISSOR_TEST)`
0xC11, disabled for the pass, restored at the end), disables depth test
(0xB71), builds a `glOrtho(0, w, h, 0, ...)` pixel projection and fetches
the UI scale factor via FUN_100296c0 (ui-scale group; see the decompiler
artifact on the float slot overlap). Then:

- Two passes over the CUIManager `+0xb2` list: pass 0 = NAMED panels
  (any of the 8 class-name bytes at `+0xe6` non-NUL) drawn opaque;
  pass 1 = UNNAMED panels drawn with `GL_BLEND` +
  `glBlendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`. Per panel with
  `+0xf4` or `+0x10a` set: if the rect table holds an entry whose mode
  equals the panel id, draw only that rect from the layer texture (UVs
  per canvas mode); otherwise draw the panel's full rect
  (`+0x24/+0x28/+0x34/+0x38`), inset left/right by a 112 px border
  width (224 px when engine `+0x4a28` is set) when ini
  `[IEex Options] UI Borders` is 0 (cached in `DAT_10092b64`, TRUE =
  borders disabled), logging once per panel id (flags
  `DAT_104df6c8[id & 7]`): "[HUDLAYER] minimal panel id=%d size=%dx%d
  insetL_permille=%d R_permille=%d".
- When NOT a fallback frame (`DAT_103d4755 == 0`) and the table holds
  the special modes: mode -3 rects (command-row bezel) are drawn as
  translucent black (0, 0, 0, 0.85) quads, or with texture
  `DAT_103defe0` when that slot is non-zero; mode -2 rects (log box) as
  translucent black (0, 0, 0, 0.6) quads, or via the border-art path -
  four candidate border textures `DAT_104df238/23c/240/244` with widths
  `DAT_104df360..36c` and heights `DAT_104df540..54c`, chosen by best
  aspect match within 2% relative error, else top/bottom strips of
  texture `DAT_104df5a0` (v ranges 0..0.116 and 0.888..1, strip height
  scaled by `DAT_104df5ac * 0.11214953` and the rect-to-log-width ratio
  against `DAT_104df0d4`; these trios are filled by inner helper
  FUN_10032a50, outside the seed list).
- Finally the -2/-3 rect CONTENT is drawn from the layer texture,
  blended.

Open cross-reference: on frames where controls DID render
(`DAT_104df534 != 0`) FUN_10035600 is skipped - that composite happens
elsewhere, in present-group territory (the staged DialogIdlePresent hook
at 0x68DF53 re-presents the retained FBO when the engine skips the whole
render block). Cross-check the present-group notes.

#### Rect-table contract

`DAT_103dec28` base, `DAT_104df4dc` count, 5-dword entries
`{mode, x, y, w, h}`, hard cap 16 entries (HudAddPanelContentRect
refuses beyond; composite loops all read exactly `count` entries).
`mode >= 0` = panel id, matched against CUIPanel `+0x20`;
`-2` = log box (content counts as panel id 0; dedicated bezel/border-art
passes); `-3` = command-row bezel (content counts as panel id 0 or 1).
The staged Lua (`IEex_Refonte_RegisterRects`, IEex_Gui_State.lua) clears
the table, registers portrait slots + action bar as id 1, the command
row as id 0 plus a `{0,0,0,0,0}` sentinel and a `{-3, ...}` bezel rect,
and the log box as `{-2, ...}` ("id -2, bezel pass").

#### GL entry points used by this group

All GL calls go through proc slots resolved once at init: the bulk
inline resolver inside FUN_1002fff0 (~0x1002fff3..0x10030200; names from
the DLL's string table, verified by objdump against call-site usage),
FUN_1002ff50's five (glClear/glClearColor/glDisable/glEnable/glIsEnabled,
one-shot `DAT_104df710`), and the direct GetProcAddress/wglGetProcAddress
set listed under the ensure step. Slot -> name for this group:

| slot | proc |
| --- | --- |
| `DAT_104df0e4` | glBindFramebufferEXT |
| `DAT_104df3d8` | glFramebufferTexture2DEXT |
| `DAT_104df5b0` | glCheckFramebufferStatusEXT |
| `DAT_104df3a0` | glGenFramebuffersEXT |
| `DAT_104df014` | glDeleteFramebuffersEXT (wglGetProcAddress) |
| `DAT_104df3a4` | glGenTextures |
| `DAT_104df630` | glDeleteTextures |
| `DAT_103dec04` | glBindTexture |
| `DAT_103ded88` | glTexImage2D |
| `DAT_104df0e0` | glTexParameteri |
| `DAT_104df390` | glIsTexture |
| `DAT_104df584` | glClearColor |
| `DAT_104df610` | glClear |
| `DAT_104df4d8` | glViewport |
| `DAT_104df000` | glScissor |
| `DAT_104df568` | glGetIntegerv |
| `DAT_104df5bc` | glIsEnabled |
| `DAT_104df4e4` | glDisable |
| `DAT_104df024` | glEnable |
| `DAT_104df60c` | glMatrixMode |
| `DAT_104df3e8` | glLoadIdentity |
| `DAT_104df3c0` | glPushMatrix |
| `DAT_104df620` | glPopMatrix |
| `DAT_103debf8` | glOrtho |
| `DAT_104df5c0` | glTexEnvf |
| `DAT_103deffc` | glColor4f |
| `DAT_103defe4` | glBlendFunc |
| `DAT_104df5f0` | glTexCoord2f |
| `DAT_104df4f0` | glVertex2f |
| `DAT_104df220` | glBegin |
| `DAT_104df3fc` | glEnd |

Resolver artifact (matters when re-deriving slot names): the inline
resolver stores each GetProcAddress/wglGetProcAddress result with a
ONE-CALL LAG - the `mov [slot], eax` after a `push <name>` stores the
PREVIOUS call's return value. A naive pattern scan mis-assigns every
slot by one; the table above is lag-corrected and cross-checked against
how each slot is used (e.g. only `DAT_104df3a0` is followed by
glBindFramebufferEXT + glFramebufferTexture2DEXT on the name it filled,
pinning it as glGenFramebuffersEXT).

Constants seen: GL_FRAMEBUFFER 0x8D40, GL_COLOR_ATTACHMENT0 0x8CE0,
GL_FRAMEBUFFER_COMPLETE 0x8CD5, GL_COLOR_BUFFER_BIT 0x4000,
GL_TEXTURE_2D 0xDE1, GL_RGBA8 0x8058, GL_RGBA 0x1908,
GL_UNSIGNED_BYTE 0x1401, GL_LINEAR 0x2601, GL_CLAMP_TO_EDGE 0x812F,
GL_BLEND 0x0BE2, GL_DEPTH_TEST 0x0B71, GL_SCISSOR_TEST 0x0C11,
GL_PROJECTION 0x1701, GL_MODELVIEW 0x1700, GL_QUADS 0x0007,
GL_SRC_ALPHA 0x0302, GL_ONE_MINUS_SRC_ALPHA 0x0303.

#### Engine targets called directly (absolute VAs into IWD2.exe)

- `0x4d4540` CUIManager::Render (named by the staged hook comment
  "wraps CUIManager::Render @0x4D4540"); the plain-render fallback calls
  it with no arguments.
- `0x4d45e0` engine invalidate call taking `&RECT`; HudLayerRender uses
  it with `{0, 0, 0x7fff, 0x7fff}` (whole screen) right after clearing
  the layer - the "blanket invalidate" of the staged rationale. Routine
  not identified beyond this use.
- `0x4d3610` CUIPanel's disabled-panel dim call, taking `&RECT`; the
  call REPLACED at 0x4D34ED by HudPanelRenderDither per the staged
  rationale ("CUIPanel::Render paints a 50% black quad over its whole
  dirty rect when the panel is disabled").

#### Threads and synchronization

All layer state is plain bytes/dwords driven from the render thread
inside the CUIManager::Render call chain; no lock is taken (contrast the
key-state registries). The four ExposeFunctions shims below run on the
Lua thread and write the rect table / skip flag unsynchronized - safe in
practice because the staged Lua calls them at load/reset time
(IEex_Refonte_RegisterRects) or from the same thread that later consumes
them, never concurrently with a frame.

#### Registration

HudLayerRender and HudPanelRenderDither are registered in the Lua
raw-address table built by FUN_1000e2a0 (bridge group) and entered from
machine-code hooks patched into the engine. IsHudLayerActive,
HudLayerSkipFrame, HudClearPanelContentRects and HudAddPanelContentRect
are ExposeFunctions shims (lua_CFunction with the hidden fastcall
lua_State in ECX). InitHelperDLL / logging contract as documented in the
bridge notes: `DAT_100944fc` = log function, `DAT_100944f8` = LogVariadic
sink; every hud log line above goes through `DAT_100944fc` and is
silently dropped when it is null (except that missing procs then flip
the permanent-disable flag instead).
`IEex_Helper_HudClearPanelContentRects` - 0x10011fb0 - ExposeFunctions
shim; no Lua arguments.

Body: `DAT_104df4dc = 0; return 0;` - drops every registered content
rect (the count; the 16 x 5 dwords of storage are left stale, which is
safe because every consumer loops exactly `count` times).

Purpose (staged Lua): first call of `IEex_Refonte_RegisterRects()`
(`IEex_Gui_State.lua` ~4930-4965) before re-registering the static rect
set.

##### Failure fallbacks

None needed.

##### Decompiler artifact

None.

