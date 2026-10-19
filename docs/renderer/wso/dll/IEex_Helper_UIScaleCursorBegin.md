# IEex_Helper_UIScaleCursorBegin

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002bdf0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002bdf0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_UIScale_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_1002bdf0(void)

{
  ushort uVar1;
  int iVar2;
  HWND hWnd;
  char cVar3;
  UINT UVar4;
  BOOL BVar5;
  int iVar6;
  int *piVar7;
  undefined4 unaff_EDI;
  int iVar8;
  bool bVar9;
  bool bVar10;
  float fVar11;
  undefined4 uVar12;
  uint uVar13;
  float local_50;
  float local_4c;
  float local_48;
  undefined4 uStack_44;
  float local_40;
  int *local_3c;
  int local_38;
  int iStack_34;
  int iStack_30;
  tagPOINT local_2c;
  int local_24;
  int local_20;
  int local_1c;
  int iStack_18;
  int iStack_14;
  int iStack_10;
  uint local_c;

  local_c = DAT_1008f040 ^ (uint)&local_50;
  FUN_10035600();
  DAT_103dedb6 = 0;
  DAT_104df076 = 0;
  iVar2 = *(int *)(*DAT_100944b8 + 0x3c4);
  if (iVar2 == 0) {
    DAT_103dedb6 = 0;
    DAT_104df076 = 0;
    return;
  }
  cVar3 = FUN_100257e0();
  if (cVar3 == '\0') {
    return;
  }
  if ((int)DAT_10092b88 < 0) {
    UVar4 = GetPrivateProfileIntA("IEex Options","Smooth Cursor",1,".\\Icewind2.ini");
    DAT_10092b88 = (uint)(UVar4 != 0);
  }
  if (((DAT_10092b88 != 0) && (DAT_100945b3 != '\0')) && (*(int *)(*DAT_100944b8 + 0x91c) != 0)) {
    if ((DAT_10094554 == (HWND)0x0) || (BVar5 = IsWindow(DAT_10094554), BVar5 == 0)) {
      if (*(HDC *)(_DAT_008cf6d8 + 0x914) == (HDC)0x0) {
        DAT_10094554 = (HWND)0x0;
      }
      else {
        DAT_10094554 = WindowFromDC(*(HDC *)(_DAT_008cf6d8 + 0x914));
      }
    }
    hWnd = DAT_10094554;
    if (((DAT_10094554 != (HWND)0x0) && (iVar6 = FUN_10037cd0(&local_2c), iVar6 != 0)) &&
       (BVar5 = ScreenToClient(hWnd,&local_2c), BVar5 != 0)) {
      local_3c = (int *)local_2c.x;
      local_38 = local_2c.y;
      DAT_103ded6e = 1;
      FUN_1002c990(&local_3c);
      DAT_103ded6e = 0;
      DAT_104df0ac = local_3c;
      DAT_103ded80 = local_38;
      DAT_104df218 = (uint)(DAT_103dec27 != '\0');
      DAT_104df5e4 = DAT_104df5e4 + 2;
      DAT_100945f6 = 1;
      iVar6 = *DAT_100944b8;
      if (DAT_104df40e == '\0') {
        DAT_104df5e0 = *(undefined4 *)(iVar6 + 0x1906);
        DAT_104df010 = *(undefined4 *)(iVar6 + 0x190a);
        DAT_104df40e = '\x01';
      }
      *(int **)(iVar6 + 0x1906) = local_3c;
      *(int *)(iVar6 + 0x190a) = local_38;
    }
  }
  iVar6 = *DAT_100944b8;
  piVar7 = (int *)(iVar6 + 0x1906);
  local_3c = piVar7;
  if ((((DAT_104df6d1 != '\0') && (*piVar7 == DAT_104df72c)) &&
      (*(int *)(iVar6 + 0x190a) == DAT_104df750)) && (iVar2 == DAT_104df6dc)) goto LAB_1002c29f;
  if (DAT_104df4dc == 0) {
LAB_1002c030:
    uStack_44 = uStack_44 & 0xffff;
  }
  else {
    if ((int)DAT_10092b6c < 0) {
      UVar4 = GetPrivateProfileIntA("IEex Options","HUD Layer",1,".\\Icewind2.ini");
      DAT_10092b6c = (uint)(UVar4 != 0);
    }
    if (DAT_10092b6c != 1) goto LAB_1002c030;
    uStack_44._0_3_ = CONCAT12(1,(undefined2)uStack_44);
    uStack_44 = (uint)(uint3)uStack_44;
    if (*(int *)(*DAT_100944b8 + 0x91c) == 0) goto LAB_1002c030;
  }
  uStack_44 = CONCAT13(DAT_103dec27,(uint3)uStack_44);
  cVar3 = DAT_103dec27;
  if ((DAT_103dec27 != '\0') && (DAT_104df4dc != 0)) {
    cVar3 = FUN_10031b90(*piVar7,*(undefined4 *)(iVar6 + 0x190a));
    uStack_44 = CONCAT13(cVar3,(uint3)uStack_44);
  }
  if (iVar2 == *(int *)(*DAT_10094384 + 0x1c88)) {
    if ((uStack_44._2_1_ == '\0') &&
       (((cVar3 != '\0' || (DAT_104df6d8 != '\0')) ||
        ((iVar2 != DAT_104df6dc || (DAT_104df6d1 == '\0')))))) {
      local_2c.x = *piVar7 + -0x60;
      local_2c.y = *(int *)(iVar6 + 0x190a) + -0x60;
      local_24 = *piVar7 + 0x60;
      local_20 = *(int *)(iVar6 + 0x190a) + 0x60;
      (*(code *)0x4d45e0)(&local_2c);
      if (DAT_104df6d1 != '\0') {
        local_1c = DAT_104df72c + -0x60;
        iStack_18 = DAT_104df750 + -0x60;
        iStack_14 = DAT_104df72c + 0x60;
        iStack_10 = DAT_104df750 + 0x60;
        (*(code *)0x4d45e0)(&local_1c);
        piVar7 = local_3c;
      }
    }
  }
  else {
    cVar3 = FUN_100375b0();
    if (cVar3 == '\0') {
      cVar3 = FUN_100298b0(&local_3c,&local_4c,&local_2c);
      if (cVar3 == '\0') {
        local_1c = 0;
        iStack_18 = 0;
        iStack_14 = 0x7fff;
        iStack_10 = 0x7fff;
        (*(code *)0x4d45e0)(&local_1c);
      }
      else {
        local_50 = (float)(2 - (int)(local_48 * -96.0));
        iStack_14 = (int)(((float)*piVar7 - local_4c) * local_48 + local_40);
        local_1c = iStack_14 - (int)local_50;
        iStack_10 = (int)(((float)*(int *)(iVar6 + 0x190a) - (float)local_2c.x) * local_48 +
                         (float)local_3c);
        iStack_18 = iStack_10 - (int)local_50;
        iStack_14 = (int)local_50 + iStack_14;
        iStack_10 = (int)local_50 + iStack_10;
        (*(code *)0x4d45e0)(&local_1c);
        if (DAT_104df6d1 != '\0') {
          iVar6 = (int)(((float)DAT_104df72c - local_4c) * local_48 + local_40);
          iStack_30 = (int)(((float)DAT_104df750 - (float)local_2c.x) * local_48 + (float)local_3c);
          local_38 = iStack_30 - (int)local_50;
          iStack_34 = (int)local_50 + iVar6;
          iStack_30 = (int)local_50 + iStack_30;
          local_3c = (int *)(iVar6 - (int)local_50);
          (*(code *)0x4d45e0)(&local_3c);
        }
      }
    }
  }
  DAT_104df72c = *piVar7;
  DAT_104df750 = piVar7[1];
  DAT_104df6d1 = '\x01';
  DAT_104df6d8 = uStack_44._3_1_;
  DAT_104df6dc = iVar2;
LAB_1002c29f:
  if ((DAT_100945f1 == '\0') &&
     (((iVar2 != *(int *)(*DAT_10094384 + 0x1c88) && (cVar3 = FUN_100375b0(), cVar3 != '\0')) ||
      ((iVar2 == *(int *)(*DAT_10094384 + 0x1c88) && (cVar3 = FUN_10037a20(), cVar3 != '\0')))))) {
    (*DAT_104df52c)(0x1700);
    (*DAT_104df50c)();
    (*DAT_104df524)(0xc9742400,0xc9742400,0);
    (*DAT_104df56c)(0,0,0x3f800000);
    DAT_104df627 = 1;
  }
  else {
    if (DAT_103dec27 == '\0') {
      cVar3 = FUN_100296c0(&local_2c);
      if (cVar3 == '\0') {
        return;
      }
      uVar13 = 0x1700;
      local_40 = (float)*(int *)(*DAT_100944b8 + 0x1906);
      local_2c.x = (LONG)(float)*(int *)(*DAT_100944b8 + 0x190a);
      (*DAT_104df52c)(0x1700);
      (*DAT_104df50c)();
      local_4c = local_40;
      local_50 = 1.0;
      if (DAT_100945f1 != '\0') {
        if (0 < DAT_104df580) {
          local_50 = (float)DAT_104df4f8 / (float)DAT_104df580;
        }
        (*DAT_104df524)((float)DAT_104df230,(float)DAT_104df0c4,0);
        (*DAT_104df56c)(uVar13,uVar13,0x3f800000);
        local_4c = local_4c * local_50;
      }
      (*DAT_104df524)(uStack_44,iStack_30,0);
      (*DAT_104df56c)(local_4c,local_4c,0x3f800000);
      (*DAT_104df524)(uVar13 ^ 0x80000000,-local_48,0);
      bVar10 = local_48 == 0.999;
      bVar9 = 0.999 < local_48;
    }
    else if (iVar2 == *(int *)(*DAT_10094384 + 0x1c88)) {
      cVar3 = FUN_100296c0(&local_3c);
      if (cVar3 == '\0') {
        return;
      }
      fVar11 = 0.0;
      if (DAT_104df4dc == 0) {
        fVar11 = (float)*DAT_10094318 * 0.5;
      }
      DAT_103dec24 = 1;
      uVar1 = *DAT_10094328;
      iVar2 = *DAT_100944b8;
      DAT_103deff0 = *(int *)(iVar2 + 0x1906);
      DAT_104df5d0 = *(int *)(iVar2 + 0x190a);
      iVar6 = (int)(((float)DAT_103deff0 - fVar11) * local_48 + fVar11 + 0.5);
      fVar11 = (float)DAT_104df5d0;
      *(int *)(iVar2 + 0x1906) = iVar6;
      iVar8 = (int)((fVar11 - (float)uVar1) * local_48 + (float)uVar1 + 0.5);
      *(int *)(iVar2 + 0x190a) = iVar8;
      (*DAT_104df52c)(0x1700);
      (*DAT_104df50c)();
      local_50 = local_4c;
      fVar11 = 1.0;
      if (DAT_100945f1 != '\0') {
        if (0 < DAT_104df580) {
          fVar11 = (float)DAT_104df4f8 / (float)DAT_104df580;
        }
        uVar12 = 0;
        (*DAT_104df524)((float)DAT_104df230,(float)DAT_104df0c4,0);
        (*DAT_104df56c)(uVar12,uVar12,0x3f800000);
        local_50 = local_50 * fVar11;
      }
      (*DAT_104df524)((float)iVar6,(float)iVar8,0);
      (*DAT_104df56c)(unaff_EDI,unaff_EDI,0x3f800000);
      (*DAT_104df524)((float)-iVar6,(float)-iVar8,0);
      bVar10 = local_4c == 0.999;
      bVar9 = 0.999 < local_4c;
    }
    else {
      cVar3 = FUN_100298b0(&local_4c,&local_3c,&local_40);
      if (cVar3 == '\0') {
        return;
      }
      uVar12 = 0x1700;
      DAT_103dec24 = 1;
      iVar2 = *DAT_100944b8;
      DAT_103deff0 = *(int *)(iVar2 + 0x1906);
      DAT_104df5d0 = *(int *)(iVar2 + 0x190a);
      iVar6 = (int)(((float)DAT_103deff0 - (float)local_3c) * local_48 + (float)local_2c.x + 0.5);
      fVar11 = (float)DAT_104df5d0;
      *(int *)(iVar2 + 0x1906) = iVar6;
      iVar8 = (int)((fVar11 - local_40) * local_48 + local_4c + 0.5);
      *(int *)(iVar2 + 0x190a) = iVar8;
      (*DAT_104df52c)(0x1700);
      (*DAT_104df50c)();
      local_50 = 1.0;
      if (DAT_100945f1 != '\0') {
        if (0 < DAT_104df580) {
          local_50 = (float)DAT_104df4f8 / (float)DAT_104df580;
        }
        (*DAT_104df524)((float)DAT_104df230,(float)DAT_104df0c4,0);
        (*DAT_104df56c)(uVar12,uVar12,0x3f800000);
      }
      (*DAT_104df524)((float)iVar6,(float)iVar8,0);
      (*DAT_104df56c)(unaff_EDI,unaff_EDI,0x3f800000);
      (*DAT_104df524)((float)-iVar6,(float)-iVar8,0);
      bVar10 = local_50 == 0.999;
      bVar9 = 0.999 < local_50;
    }
    DAT_104df076 = !bVar9 && !bVar10;
  }
  DAT_103dedb6 = 1;
  return;
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
`IEex_Helper_UIScaleCursorBegin` - 0x1002bdf0 (FUN_1002bdf0) - no
arguments; entered from the IEex_UIScale_Patch.lua
IEex_HookRestore(0x7BE300, 7) stub at CVidInf::RenderPointer3d entry
(displaced SEH prologue bytes `6A FF 68 30 5B 84 00`).

##### Behavior

The largest helper in the group; runs the "Stage 2" world-HUD scale
plus cursor sizing at RenderPointer3d entry. Order of operations:

1. FUN_10035600() first - the HUD-layer overlay painter (borders,
   dim/portrait overlays; documented in COMMON's painter summary).
   Then clears `DAT_103dedb6` and `DAT_104df076`; surface gate
   `*(*DAT_100944b8 + 0x3c4) == 0` -> both stay clear, return.
2. FUN_100257e0() required. Lazy INI "Smooth Cursor" default 1
   (`DAT_10092b88`).
3. Smooth-cursor tracking: when smooth && `DAT_100945b3` &&
   render-active, resolve the render HWND (`WindowFromDC` over the HDC
   at g_pBaldurChitin+0x914, cached in `DAT_10094554`), read the OS
   cursor via FUN_10037cd0 (GetCursorPos with sticky edge clamping) +
   ScreenToClient, then run the capture transform re-entrantly:
   `DAT_103ded6e = 1; FUN_1002c990(&pt); DAT_103ded6e = 0`. Stores the
   converted x/y in `DAT_104df0ac`/`DAT_103ded80`, records whether the
   point landed on UI (`DAT_104df218 = DAT_103dec27 != 0`), bumps the
   perf counters, and damage-invalidates.
4. Mouse-field redirect: one-shot snapshot pair 2 (`DAT_104df5e0`/
   `DAT_104df010` = engine mouse fields at g_pBaldurChitin+0x1906/
   +0x190a, flag `DAT_104df40e`), then OVERWRITES those engine fields
   with the scale-corrected position - so every engine consumer this
   frame reads logical coordinates. UIScaleRenderEnd restores the
   snapshot later.
5. Damage tracking: skip invalidation when the position and surface are
   unchanged and `DAT_104df6d1` is set; otherwise invalidate a rect
   around the new + previous cursor position (world: +/-0x60 = 96 px
   through engine 0x4D45E0; UI: FUN_100298b0-converted rect with pad
   `2 + (int)(96*s)`, or the full screen `{0,0,0x7fff,0x7fff}` when the
   panel query fails).
6. Matrix work (LAB_1002c29f): with the HUD-layer/dialog-idle
   predicates FUN_100375b0/FUN_10037a20 deciding overlay state, either
   park the MODELVIEW off-screen (glTranslatef(-1000000.0f, -1000000.0f,
   0) + glScalef(0, 0, 1), flag `DAT_104df627`) or apply the scale:
   world branch B (cursor over UI, `DAT_103dec27`) additionally
   snapshots pair 1 (`DAT_103deff0`/`DAT_104df5d0`, flag
   `DAT_103dec24`), re-redirects the mouse fields with the anchor-based
   rounding, and scales about the floating-HUD anchor (anchor x = 0
   when `DAT_104df4dc` else W/2, anchor y = H).
7. Set `DAT_104df076 = (scale > 0.999) && (scale != 0.999)` - true only
   when the UI actually minified; this arms the LINEAR filter swap in
   CVidCell_RenderTextureHD for this frame. Finally sets
   `DAT_103dedb6 = 1`.

##### Failure fallbacks

No render flag, no matrix slots, or surface 0: returns with both stage
flags clear - RenderPointer3d then draws the cursor through the stock
identity matrix. The mouse fields are only redirected after the capture
transform succeeded, so a failed frame never leaves logical garbage in
the engine state.

##### Decompiler artifact

Register allocation leaks into GL arguments (`unaff_EDI` as glScalef
args, `uVar13 = 0x1700` used where a float argument belongs): read the
constants, not the register names. The function is > 1000 lines
decompiled because the three matrix branches are inlined copies of the
same scale-about-anchor pattern.


