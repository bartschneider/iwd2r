# IEex_Helper_SetMenuTorch

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002a670` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_1002a670.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_UIScale_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_1002a670(undefined4 param_1)

{
  float10 fVar1;

  fVar1 = (float10)lua_tonumber(param_1,1);
  DAT_1009278d = (int)fVar1 != 0;
  return 0;
}
```

## Notes

### Shared UI-scale machinery (all helpers in this group)

The staged IEex_UIScale_Patch.lua brackets four engine render paths and
re-routes three FX-composite draw functions so the whole UI (menus,
world HUD, cursor, torch, FX quads) renders through GL MODELVIEW scales
instead of the stock 1x canvas. Everything below is gated on
`[Program Options] "3D Acceleration"` = 1 in `Icewind2.ini`: when that
key is 0 the Lua patch calls `IEex_Helper_SetHDUI(0)` and installs
NONE of the hooks (stock software renderer). The helpers themselves
additionally self-gate on their own render-active flag, so they are
all no-ops without a live GL context.

#### Frame bracket model

One frame looks like this (hook addresses from the Lua patch):

| Stage | Engine site | Helper |
|---|---|---|
| UI pass entry | CUIManager::Render 0x4D4540 | UIScaleRenderBegin |
| UI pass exit | CUIManager::Render epilogue 0x4D45C3 | UIScaleRenderEndUI |
| Show-FPS overlay | CVidInf::Flip 0x79C1F8 / 0x79C201 | FrameRateBegin / FrameRateEnd (documented with the present group) |
| Cursor pass entry | CVidInf::RenderPointer3d 0x7BE300 | UIScaleCursorBegin |
| Cursor pass exit | RenderPointer3d epilogue 0x7BE4C1 | UIScaleRenderEnd |
| Main-menu torch | CScreenConnection::RenderTorch 0x5FB020 | UIScaleTorchBegin |
| Input capture | CChitin::AsynchronousUpdate 0x78F489 | UIScaleCaptureMap |
| FX composites | CVidCell::RenderTexture 0x7C4240, CVidMosaic::RenderTexture 0x7C56C0, CVidBitmap::RenderTexture 0x7C6800 | CVidCell_RenderTextureHD / CVidMosaic_RenderTextureInset / CVidBitmap_RenderTextureInset |

The CUIManager::Render brackets are stateless with respect to the GL
matrix stack (UIScaleRenderBegin ends in glLoadIdentity, so an early
return inside the engine function can never leave the stack unbalanced).

#### Virtual canvas and scale queries

The scale model: a virtual canvas of `factor * 800 x factor * 600`
pixels (factor = 2 when the engine's wide-area/HUD-2x flag
`*(*DAT_10094384 + 0x4a28)` is set, else 1), multiplied by an HD
multiplier of 2.0 when HD UI is on and the wide flag is clear. UI
surfaces render at `scale = min(W / (f*800*hd), H / (f*600*hd))`,
capped at 1.0 unless `[IEex Options] "Stretch UI to Screen"` is on
(then the cap is dropped so the canvas fills the screen, letterboxed).
World surfaces render at `scale = W / FUN_100295a0()` where
FUN_100295a0 is the floating-HUD reference width (INI
"Modern HUD Ref Width" / fallback "Floating HUD Ref Width", default
3840, clamped [2560, 7680]; "Modern HUD Size" / "Floating HUD Size",
default 100, clamped [50, 200]; result = max(800, width*100/size) with
width widened to the ref width on wide areas). All three queries clamp
the final scale to at least 0.1.

FUN_100296c0 is the central scale query (real convention: ECX -> out
scale, EDX -> out virtual width, stack -> out virtual height; returns
bool). It requires the render-active flag `*(*DAT_100944b8 + 0x91c)`
and non-zero screen dimensions *DAT_10094318 / *DAT_10094328, and
re-reads "Stretch UI to Screen" from INI on EVERY call (uncached).
FUN_100298b0 adds the panel centering offsets: given a non-world
surface it outputs the scaled top-left offsets `(W - scale*virtualW)/2,
(H - scale*virtualH)/2` plus the same offsets at scale 1.0.

#### Fork state (BSS flags all helpers share)

- `DAT_103dedb6` (byte): "scale transform active" - set 1 by
  UIScaleRenderBegin / UIScaleCursorBegin on success, cleared by
  UIScaleRenderEnd / UIScaleCursorBegin entry and by UIScaleRenderEndUI
  when it resets the world matrix. Gates the matrix-reset branches.
- `DAT_104df076` (byte): "cursor minified this frame" - set by
  UIScaleCursorBegin only when the UI scale actually minifies
  (scale > 0.999 and not == 0.999); cleared unconditionally at
  UIScaleRenderEnd entry. Gates the LINEAR-filter swap in
  CVidCell_RenderTextureHD (cross-link: hd-sprite group).
- `DAT_103dec27` (byte): "cursor is over blocking UI" - set by
  UIScaleCaptureMap / UIScaleCursorBegin hit-tests, refined by
  FUN_10031b90.
- `DAT_103dec24` / `DAT_104df40e` (bytes) + snapshot pairs
  `DAT_103deff0`/`DAT_104df5d0` (pair 1) and
  `DAT_104df5e0`/`DAT_104df010` (pair 2): the two one-shot snapshots of
  the engine mouse position fields. UIScaleRenderEnd restores pair 1
  FIRST and pair 2 SECOND, so the pre-CursorBegin original (pair 2)
  wins when both stages ran.
- `DAT_104df3e0`: surface id saved by UIScaleTorchBegin, compared by
  UIScaleRenderEndUI to decide the torch re-render.
- `DAT_104df536` (byte): HD-UI flag, written only by SetHDUI (a RAM
  flag, NOT an INI key). Feeds the 2.0 HD multiplier and the
  UIScaleRenderEndUI torch gate.
- `DAT_1009278d` (byte): menu-torch flag, written only by SetMenuTorch
  (RAM flag, NOT an INI key). Gates the torch re-render.
- `DAT_104df575` (byte): torch re-entrancy guard around the
  `(*(code*)0x5fb020)()` re-render.
- `DAT_103ded6e` (byte): capture-mode re-entrancy guard - UIScaleCursorBegin
  sets it 1 around its own `FUN_1002c990(&pt)` call so the capture hook
  (UIScaleCaptureMap) does not recurse; while set, UIScaleCaptureMap
  leaves `DAT_104df576` alone.
- `DAT_104df4dc` (int): count of 5-dword records (id, x, y, w, h) in
  the unified blocklist table at `DAT_103dec28`; ids -2 and -3 are
  special (dim overlay / portrait-close strip, drawn by the HUD-layer
  painter FUN_10035600). Also doubles as the "floating HUD" selector:
  when it is non-zero the world-surface anchor moves from the screen
  center to the left edge.
- `DAT_104df1fc` / `DAT_104df1ff` (bytes): UI-scale subsystem init
  flags set once by the first helpers to run (lazy INI reads and GL
  slot resolution happen under them).
- `DAT_104df506` (byte): sub-viewport mode (bottom-strip HUD);
  `DAT_104df564` sub-viewport width, `DAT_103deda4` strip height.
- `DAT_103d4754` (byte): cleared by FUN_10035600 at entry so re-entrant
  engine draws during the HUD-layer composite skip the scale-aware
  forwarding of FUN_10032120.
- Perf stamps: UIScaleRenderBegin accumulates into `DAT_104df0f0/f4`
  and `DAT_104df538/53c` under the perf-log flag `DAT_104df0ed`;
  UIScaleRenderEndUI accumulates into `DAT_104df200/204` - that pair
  feeds the `ui_ms` column of the perf CSV (documented with the fog
  group).

#### Engine state the helpers read

- `*DAT_100944b8`: the fork-registered engine game-state pointer (same
  object as the engine global g_pBaldurChitin at 0x8CF6D8, which the
  helpers also read directly as `_DAT_008cf6d8`). Fields used: +0x3c4
  current surface id, +0x91c render-active flag, +0x914 HDC,
  +0x1906/+0x190a mouse x/y (the fields the cursor stage redirects),
  +0xaa memset region (0x30 bytes cleared when the UI-scale cache goes
  stale), +0xf04 dialog-idle byte (read via FUN_10037a20).
- `*DAT_10094318` / `*DAT_10094328`: screen width / height.
- `*DAT_10094384`: world-surface/area object; +0x1c88 world surface id,
  +0x4a28 wide-area / HUD-2x flag (the Lua calls it
  g_pBaldurChitin->field_4A2C).
- `_DAT_008ba31c` / `_DAT_008ba31e`: engine word globals with screen
  width / height (used by the HUD-layer painter and FUN_10032120).

#### Engine-address calls (open questions for the renderer design)

- `0x4D45E0` - CUIManager rect-invalidate thiscall (named by the staged
  Lua wrapper IEex_InvalidateUIManagerRect, which builds a CRect and
  calls it). The helpers call it with `{0, 0, 0x7fff, 0x7fff}` (full
  screen) after a full matrix change, and with a damage rect around the
  cursor on the world surface.
- `0x5FB020` - CScreenConnection::RenderTorch (the Lua hooks its entry
  for UIScaleTorchBegin; UIScaleRenderEndUI calls it naked to re-issue
  the torch pre-Flip). No arguments.
- `0x4D2CC0` - point-in-control test, called thiscall-style with a
  CPoint* on the stack from FUN_10031b90 and FUN_1002c910. Not named in
  the staged Lua; treat the exact semantics as an open question.

#### INI keys (all `[IEex Options]` in `.\Icewind2.ini` unless noted)

"Stretch UI to Screen" (default 0, re-read every scale query),
"Smooth Cursor" (1), "HUD Layer" (1), "Cursor Overlay" (1),
"Dialog Idle Present" (1), "FX Texel Inset" (1), "FX Scratch Wipe" (1),
"Vsync" (1), "UI Borders" (1, read inverted by the HUD painter),
"Modern HUD Ref Width"/"Floating HUD Ref Width" (3840, clamp
[2560,7680]), "Modern HUD Size"/"Floating HUD Size" (100, clamp
[50,200]), plus `[Options] "Perf Log"` in `.\IEex.ini` (perf CSV) and
`[Program Options] "3D Acceleration"` (Lua-side master gate). HD UI and
menu torch are RAM flags set through SetHDUI / SetMenuTorch, not INI
keys.

#### GL entry points

All GL calls go through function-pointer slots resolved once by lazy
GetProcAddress-style init (slot map documented with the hd-sprite and
tile-atlas groups). Slots used by this group: DAT_104df52c glMatrixMode,
DAT_104df50c glLoadIdentity, DAT_103debfc glPushMatrix, DAT_104df608
glPopMatrix, DAT_104df524 glTranslatef, DAT_104df56c glScalef,
DAT_103debf8 glOrtho, DAT_104df4d8 glViewport, DAT_104df61c
glTexParameteri, DAT_104df054 glTexSubImage2D, DAT_100941d4 /
DAT_100941f8 / DAT_100943a0 (the three stock engine draw slots the FX
helpers forward to), plus the FUN_10024f40 set glClear / glClearColor /
glDisable / glEnable / glIsEnabled and the FUN_10025040 slot
glwSwapIntervalEXT. The vectors of GL state changes are short and
always paired (push/pop, enable/disable, filter set/restore).

#### Threads and synchronization

Everything in this group runs on the engine's GL/render thread inside
the frame; the Lua hooks replace call sites in the render loop and the
input hook (UIScaleCaptureMap) runs on CChitin::AsynchronousUpdate's
thread but only touches plain BSS flags and ints shared with the render
thread. There are no locks, atomics, or TLS interactions in the group
(the only TLS use in the wider DLL is inside the vsync machine
FUN_10025040, documented with the present group because
UIScaleRenderBegin calls it). The mouse snapshot/redirect pairs are the
one cross-stage contract: AsynchronousUpdate writes the physical mouse
position, the cursor stage snapshots and replaces it, and the epilogue
restores the snapshot in fixed order (pair 2 last = wins).

#### Cross-links

CVidCell_RenderTextureHD, the texel-inset pair and the scratch wipe
consume the same 512x512 FX scratch and lazy-INI machinery as the
hd-sprite group (see the hd-sprite COMMON). FUN_10025040 (vsync),
FUN_10025a80 (perf CSV) and FUN_10024f40 (clear-behind-UI) are shared
services documented here because this group is their only caller chain
relevant to rendering order.
`IEex_Helper_SetMenuTorch` - 0x1002a670 (FUN_1002a670) - Lua-registered
helper (lua_State* argument); entered from a direct Lua call,
IEex_UIScale_Patch.lua line 16 (`SetMenuTorch(IEEX_MENU_TORCH and 1 or
0)`).

##### Behavior

`DAT_1009278d = (int)lua_tonumber(L, 1) != 0; return 0;` - identical
shape to SetHDUI, writing the menu-torch RAM flag that gates the
UIScaleRenderEndUI torch re-render (the single-render fix for the
post-Flip "parasite frame" flicker on Wine/Mesa).

##### Failure fallbacks

None; missing argument clears the flag.

##### Decompiler artifact

Same float10 promotion as SetHDUI.


