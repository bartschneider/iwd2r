# IEex_Helper_CVidMode_DrawRecticle3dOverride

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002e5b0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002e5b0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_CameraZoom_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Type propagation algorithm not settling */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_1002e5b0(int *param_1,undefined4 *param_2)

{
  byte *pbVar1;
  char cVar2;
  int iVar3;
  size_t _Size;
  UINT UVar4;
  uint uVar5;
  int iVar6;
  short sVar7;
  undefined4 in_EDX;
  undefined2 uVar8;
  undefined2 extraout_var;
  short sVar9;
  float fVar10;
  undefined1 auStack_138 [11];
  char local_12d;
  int local_12c;
  byte *pbStack_128;
  uint local_124;
  float local_120;
  int local_11c;
  uint local_118;
  uint local_114;
  int iStack_110;
  int iStack_10c;
  undefined4 uStack_108;
  undefined4 uStack_104;
  int iStack_100;
  byte *pbStack_fc;
  int local_e4;
  int iStack_e0;
  int iStack_dc;
  undefined4 uStack_d8;
  undefined4 uStack_d4;
  int iStack_d0;
  byte *pbStack_cc;
  int local_c0;
  int iStack_bc;
  undefined4 uStack_b8;
  uint uStack_b4;
  undefined8 local_b0;
  undefined4 local_94;
  int iStack_90;
  float *pfStack_8c;
  int *piStack_88;
  float *pfStack_84;
  int *piStack_80;
  float *pfStack_7c;
  uint *puStack_78;
  char *pcStack_74;
  int *piStack_70;
  undefined4 *puStack_6c;
  undefined4 *puStack_68;
  undefined4 uStack_64;
  undefined4 *puStack_60;
  undefined1 *puStack_5c;
  undefined4 *puStack_58;
  undefined4 *puStack_54;
  undefined4 local_50;
  undefined4 local_4c;
  undefined4 local_48;
  char cStack_41;
  uint uStack_40;
  int iStack_3c;
  int iStack_38;
  float fStack_34;
  float afStack_30 [2];
  int iStack_28;
  undefined4 local_24;
  undefined4 local_20;
  int local_1c;
  int local_18;
  uint local_14;

  uVar8 = (undefined2)((uint)in_EDX >> 0x10);
  local_14 = DAT_1008f040 ^ (uint)auStack_138;
  local_50 = 0x7bcbb0;
  local_c0 = *param_1;
  iStack_bc = param_1[1];
  uStack_b8 = param_1[2];
  uStack_b4 = param_1[3];
  local_4c = 0x7bce20;
  local_48 = 0x7bc250;
  local_b0 = *(undefined8 *)(param_1 + 4);
  local_120 = 1.0;
  if (((DAT_104df0ee == '\0') || (DAT_1008f298 <= 1.0)) ||
     (cVar2 = FUN_100257e0(), uVar8 = extraout_var, cVar2 == '\0')) {
    local_114 = (uint)local_b0._2_2_;
    local_12c = CONCAT22(uVar8,(undefined2)uStack_b8);
    local_118 = (uint)(ushort)local_b0;
    local_124 = uStack_b4 >> 0x10;
    local_e4 = iStack_bc;
    local_12d = '\0';
    local_11c = local_c0;
    sVar9 = uStack_b8._2_2_;
  }
  else {
    local_12d = '\x01';
    local_120 = DAT_1008f298;
    local_11c = (int)(((float)*param_1 - _DAT_104df388) * DAT_1008f298 + _DAT_104df388 + 0.5);
    local_e4 = (int)(((float)param_1[1] - _DAT_104df39c) * DAT_1008f298 + _DAT_104df39c + 0.5);
    local_12c = (int)((float)(int)(short)param_1[2] * DAT_1008f298 + 0.5);
    fVar10 = (float)(int)*(short *)((int)param_1 + 10) * DAT_1008f298;
    local_124 = (uint)((float)(int)*(short *)((int)param_1 + 0xe) * DAT_1008f298 + 0.5);
    local_118 = (uint)((float)(int)(short)param_1[4] * DAT_1008f298 + 0.5);
    local_114 = (uint)((float)(int)*(short *)((int)param_1 + 0x12) * DAT_1008f298 + 0.5);
    (*DAT_104df52c)();
    (*DAT_103debfc)();
    (*DAT_104df50c)();
    sVar9 = (short)(int)(fVar10 + 0.5);
  }
  local_24 = *param_2;
  local_20 = param_2[1];
  local_1c = param_2[2] + -1;
  local_18 = param_2[3] + -1;
  sVar7 = sVar9;
  if (sVar9 < (short)local_12c) {
    sVar7 = (short)local_12c;
  }
  iVar3 = (int)sVar7;
  if (iVar3 < 1) {
    iVar3 = 1;
  }
  _Size = iVar3 + 2;
  if (SCARRY4(iVar3,2)) {
    _Size = 0xffffffff;
  }
  pbStack_128 = malloc(_Size);
  if (pbStack_128 == (byte *)0x0) goto LAB_1002ee4f;
  if (DAT_10092770 == 0xffffffff) {
    DAT_10092770 = GetPrivateProfileIntA
                             ("IEex Options","Destination Marker Thickness",2,".\\Icewind2.ini");
  }
  if ((int)DAT_10092770 < 1) {
    UVar4 = FUN_1002cfa0();
  }
  else {
    UVar4 = DAT_10092770;
    if (6 < (int)DAT_10092770) {
      UVar4 = 6;
    }
  }
  uStack_40 = (uint)((float)(int)UVar4 * local_120 * 0.5 + 0.5);
  if ((int)uStack_40 < 1) {
    uStack_40 = 1;
  }
  uVar5 = (int)((int)(short)local_124 + ((int)(short)local_124 >> 0x1f & 3U)) >> 2;
  if ((int)uVar5 < 2) {
    uVar5 = 2;
  }
  if (uVar5 < uStack_40) {
    uStack_40 = uVar5;
  }
  sVar7 = sVar9;
  if ((short)local_12c < sVar9) {
    sVar7 = (short)local_12c;
  }
  if ((int)sVar7 < (int)uStack_40) {
    uStack_40 = (int)sVar7;
  }
  cStack_41 = FUN_10027020();
  if (cStack_41 == '\0') {
LAB_1002e90b:
    iStack_28 = 0;
  }
  else {
    (*DAT_104df040)();
    (*DAT_104df040)(0xbe2);
    if (cStack_41 == '\0') goto LAB_1002e90b;
    iStack_28 = (int)(uStack_40 - 1) / 2;
  }
  local_120 = (float)(int)(short)local_12c;
  local_12c = (int)(short)local_118 + (int)local_120;
  local_118 = (uint)sVar9;
  local_124 = (int)(short)local_114 + local_118;
  iStack_3c = local_11c;
  uVar5 = 1;
  if (0 < sVar9) {
    uVar5 = local_118;
  }
  iStack_38 = local_e4;
  afStack_30[1] = 1.0;
  uStack_64 = local_94;
  afStack_30[0] = (float)(int)(uStack_40 - 1) * 0.5 - (float)iStack_28;
  pfStack_8c = afStack_30;
  piStack_88 = &iStack_3c;
  pfStack_84 = afStack_30 + 1;
  piStack_80 = &iStack_38;
  pfStack_7c = &fStack_34;
  puStack_78 = &uStack_40;
  pcStack_74 = &cStack_41;
  piStack_70 = &iStack_28;
  puStack_6c = &local_50;
  fStack_34 = (float)(int)local_120 / (float)(int)uVar5;
  puStack_68 = &local_4c;
  puStack_60 = param_2;
  puStack_5c = &stack0x0000000c;
  puStack_58 = &local_48;
  puStack_54 = &local_24;
  if ((int)uStack_40 < 1) {
    local_114 = 0;
  }
  else {
    iVar3 = (*(code *)0x799aa0)(local_120,local_118,pbStack_128);
    local_114 = iVar3 - 1;
  }
  local_11c = iStack_3c;
  local_e4 = iStack_38;
  iStack_100 = FUN_1002ee80(0x3f800000,local_114);
  local_c0 = local_11c + 1;
  iVar3 = iStack_100 + -1;
  iStack_bc = (*pbStack_128 - local_124) + local_e4;
  uStack_b8 = 1;
  iStack_10c = local_e4 - local_124;
  iStack_110 = local_11c;
  uStack_b4 = 1;
  local_b0 = CONCAT44(pbStack_128 + 1,iVar3);
  uStack_108 = 0xffffffff;
  uStack_104 = 1;
  pbStack_fc = pbStack_128;
  iStack_90 = iStack_100;
  FUN_1002efe0(1,1,local_11c,iStack_10c,0xffffffff,1,CONCAT44(pbStack_128,iStack_100),local_c0,
               iStack_bc,1,1,local_b0);
  pbVar1 = pbStack_128;
  uStack_108 = 1;
  uStack_104 = 0xffffffff;
  iStack_110 = iStack_3c + 1;
  iStack_e0 = iStack_3c;
  iStack_10c = (iStack_38 - (uint)*pbStack_128) + local_124;
  iStack_dc = local_124 + iStack_38;
  iStack_d0 = iStack_90;
  pbStack_fc = (byte *)local_b0._4_4_;
  uStack_d8 = 0xffffffff;
  uStack_d4 = 0xffffffff;
  pbStack_cc = pbStack_128;
  iStack_100 = iVar3;
  FUN_1002efe0(1,0xffffffff,iStack_3c,iStack_dc,0xffffffff,0xffffffff,
               CONCAT44(pbStack_128,iStack_90),iStack_110,iStack_10c,1,0xffffffff,
               CONCAT44(local_b0._4_4_,iVar3));
  uVar5 = local_114;
  if (0 < (int)uStack_40) {
    uVar5 = (*(code *)0x799aa0)(local_118,local_120,pbVar1);
  }
  iVar6 = FUN_1002ee80(fStack_34,uVar5);
  iStack_e0 = ((uint)*pbVar1 - local_12c) + iStack_3c;
  iStack_110 = iStack_3c - local_12c;
  iVar3 = iVar6 + -1;
  iStack_dc = iStack_38 + 1;
  pbStack_cc = (byte *)local_b0._4_4_;
  pbStack_fc = pbStack_128;
  uStack_d8 = 1;
  uStack_d4 = 1;
  iStack_10c = iStack_38;
  uStack_108 = 1;
  uStack_104 = 0xffffffff;
  iStack_100 = iVar6;
  iStack_d0 = iVar3;
  FUN_1002efe0(0,1,iStack_110,iStack_38,1,0xffffffff,CONCAT44(pbStack_128,iVar6),iStack_e0,iStack_dc
               ,1,1,CONCAT44(local_b0._4_4_,iVar3));
  pbVar1 = pbStack_128;
  uStack_d8 = 0xffffffff;
  uStack_d4 = 1;
  iStack_e0 = (iStack_3c - (uint)*pbStack_128) + local_12c;
  iStack_10c = iStack_38;
  uStack_108 = 0xffffffff;
  uStack_104 = 0xffffffff;
  iStack_dc = iStack_38 + 1;
  pbStack_cc = (byte *)local_b0._4_4_;
  iStack_110 = local_12c + iStack_3c;
  pbStack_fc = pbStack_128;
  iStack_100 = iVar6;
  iStack_d0 = iVar3;
  FUN_1002efe0(0,0xffffffff,iStack_110,iStack_38,0xffffffff,0xffffffff,CONCAT44(pbStack_128,iVar6),
               iStack_e0,iStack_dc,0xffffffff,1,CONCAT44(local_b0._4_4_,iVar3));
  free(pbVar1);
LAB_1002ee4f:
  if (local_12d != '\0') {
    (*DAT_104df608)();
  }
  return 1;
}
```

## Notes

### Shared camera-zoom machinery (all helpers in this group)

The staged `IEex_CameraZoom_Patch.lua` installs a GL MODELVIEW camera zoom
for the world viewport. Its own comments state the design (untrusted,
attributed): bracket `CGameArea::Render` (0x477740) with a MODELVIEW scale
about the viewport centre; the world (tiles, sorted sprites, FoW, selection
highlight) draws inside the bracket and UI panels draw after it with
MODELVIEW back at identity, so only the world scales. The whole patch
returns without installing anything when `[Program Options] "3D
Acceleration"` in `.\Icewind2.ini` is 0 (software renderer).

#### Zoom state (fork BSS)

- `DAT_10092798` (float): target zoom, recomputed by inner FUN_10025930
  (below) under the recompute flag `DAT_10094548`.
- `DAT_1008f298` (float): animated current zoom. ZoomWorldBegin lerps it
  toward the target at 0.22 per tick and snaps within 0.01. This is the
  value the GL bracket scales by and the value `GetCameraZoom` returns.
- `DAT_1009275c` (float): previous-frame zoom, used by the
  zoom-toward-cursor compensation.
- `DAT_104df0ee` (byte): "MODELVIEW bracket installed this frame" flag;
  set by ZoomWorldBegin after the GL calls, cleared at its own entry and
  by ZoomWorldEnd.
- `DAT_10094548` (int): recompute-target flag, the only LOCK-protected
  word in the group; cleared by ZoomWorldBegin and MapWheelPump, set by
  the wheel-tick path in the window proc (per the patch comments).
- `DAT_103d4756` (int): `[IEex Options] "Integer Zoom"` flag; when set,
  zoom steps snap to whole numbers and the centre math floors instead of
  rounding.
- `DAT_103dec27` (byte): zoom-mode flag on the world screen.
- `DAT_103dec25` (byte) + `DAT_1009276c` (float): script-zoom active flag
  and script zoom value (scripted camera framing can pin its own zoom;
  ClampScriptViewDest prefers it).
- `DAT_103ded94` (float): half viewport height stored while zoomed; the
  dialog pivot-row math subtracts it.
- `_DAT_104df388` / `_DAT_104df39c` (floats): zoom anchor = viewport
  centre, read by the draw overrides and the screen-to-world mapping.
- `DAT_104df4dc` (byte): ui-scale full-viewport flag consulted by
  WheelShouldZoom (ui-scale group).

#### INI knobs ("IEex Options" section of .\Icewind2.ini)

- "Integer Zoom" (default 0): whole-number zoom steps.
- "Camera Overscroll" (default 50, clamped [0,50], re-read at most every
  300 ms -- inner FUN_100258a0 caches by microsecond timestamp): how far
  past the map edges the camera may pan, as a percent of half the zoomed
  viewport.
- "Selection Circle Thickness" (default 2, clamped [1,6]; below 1 the
  engine-scaled 4 or 6 is used depending on viewport height -- inner
  FUN_1002cfa0): ellipse override line thickness.
- "Destination Marker Thickness" (default 2, clamped [1,6]; below 1 falls
  back to FUN_1002cfa0): recticle override line thickness.

#### Target zoom (inner FUN_10025930)

Cap = min(max(3.0, (viewportWidth-1080)/1080*1.5+3.0), 5.0): 3.0 at
viewports up to 1080 px wide, 5.0 at 2160 and above, linear between
(constants at .rdata 0x10088980 / 0x100889a0 / 0x100889f4 / 0x10088958).
Step handling: Integer Zoom computes max(1.0, min(round(current)+step,
floor(cap))) via the CRT ceil/floor helpers at 0x10059ad0/0x10059ad6;
the smooth path computes max(1.0, min(current + step*0.15, cap)) with
0.15 at .rdata 0x10088908. The decompiler prints this function as void;
the clamped result is returned in XMM0 (objdump-verified) and callers
store it to `DAT_10092798`.

#### GL entry points

All GL calls in the group go through the six-slot table resolved one-shot
by inner FUN_100257e0 (documented in the ui-scale group): `DAT_104df52c`
glMatrixMode, `DAT_104df50c` glLoadIdentity, `DAT_104df524` glTranslatef,
`DAT_104df56c` glScalef, `DAT_103debfc` glPushMatrix, `DAT_104df608`
glPopMatrix. The world bracket itself (objdump-verified at
0x10028f60-0x10029030) is:

    glMatrixMode(GL_MODELVIEW 0x1700)
    glLoadIdentity()
    glTranslatef(cx, cy, 0)
    glScalef(z, z, 1.0f)
    glTranslatef(-cx, -cy, 0)     ; negation via xorps 0x10088590
    DAT_104df0ee = 1

There is no glPushMatrix here on purpose: identity loads make the bracket
stateless, so an engine early-return inside the bracket can never
imbalance the matrix stack (the patch comment calls this out). The draw
overrides (ellipse, recticle) do push/pop around their own identity load,
guarded by a byte set only when they actually bracketed.

#### Engine addresses used by the group

The engine binary is out of scope for this stream; the names below come
from the staged patch's comments (attributed, unverified against the
engine), the offsets from the decompiles:

| Address | Name per patch comments |
|---|---|
| 0x477740 / 0x477E55 | CGameArea::Render entry / epilogue |
| 0x5CDFC0 | CInfinity::GetWorldCoordinates (replaced) |
| 0x5D11F0 | CInfinity::SetViewPosition (replaced; DLL callers of this address therefore run the override) |
| 0x7BC580 | CVidMode::DrawEllipse3d (replaced) |
| 0x7BC7A0 | CVidMode::DrawRecticle3d (replaced) |
| 0x4289C0 | CBaldurEngine::OnMouseWheel |
| 0x476970 | CGameArea::OnMouseMove |
| 0x644B00 / 0x645100 / 0x6437A0 / 0x642DA0 | CUIControlButtonMapAreaMap::RenderViewRect / TimerAsynchronousUpdate / CenterViewPort / OnLButtonDown |
| 0x484B88 / 0x483F00 | CGameDialogEntry::Handle site / CGameDialogSprite::AsynchronousUpdate entry |
| 0x45F2A0 / 0x45F5D0 | CGameAIBase::MoveView / MoveViewPoint |
| 0x70CC2F | CGameSprite::Unmarshal party-branch camera restore site |

Unnamed engine calls made from the helpers: 0x5D11F0 (SetViewPosition,
see above), 0x644A50 (area-map repaint), 0x68C340 (dialog re-pan, called
as (ref, y, 0x10)), 0x799AA0 (midpoint-ellipse span-count setup, returns
the span count), 0x7BCBB0 / 0x7BCE20 / 0x7BC250 (horizontal / vertical /
third span rasterisers used by the ellipse and recticle overrides).

#### Engine pointers (runtime-registered BSS)

The `DAT_10094xxx` objects are engine pointers filled at runtime through
the loader's public address-list mechanism (public source: see
IEexDLL/IEexLua.asm; not reversed here). As used by this group:
`DAT_10094384` state root (+0x1c54 = shared viewport/state object with
the current screen id at +0x43e2, portrait index at +0x37e0 and the
party-portrait pointer array at +0x37e2; +0x1c74 / +0x1c88 = world and
area-map screen ids); `DAT_100944b8` UI/input root (+0x91c = GL-mode
flag, +0x3c4 = current screen id, +0x1906 / +0x190a = cursor x/y);
`DAT_10094318` / `DAT_10094328` screen width/height ints; `DAT_1009436c`
object-resolve slot; `DAT_100944d0` object-release slot.

#### Threads and synchronization

ZoomWorldBegin/End and the two draw overrides run on the render thread
inside the frame. MapWheelPump and DialogZoomTick run on the async update
thread (the patch hooks TimerAsynchronousUpdate / AsynchronousUpdate
entries for exactly that reason and notes the async thread is the same
one that runs the engine's dialog scroll, so nothing writes the dialog
scroll target concurrently). WheelShouldZoom and MapGrabHitTest run on
the input/window path. GetCameraZoom is callable from any Lua context and
is called from the async tick and the scroll code. The shared zoom floats
are plain BSS words with no synchronisation; only `DAT_10094548` is
LOCK-protected. A Lua reader can in principle observe a torn
DAT_1008f298 mid-lerp; the Lua callers only divide by it, so this is
benign there.

#### Patch wiring (all sites from IEex_CameraZoom_Patch.lua)

| Helper | Site | Marshal |
|---|---|---|
| ZoomWorldBegin | 0x477740 entry, IEex_HookRestore 8 bytes (`sub esp,0x14; mov eax,ds:0x8CF6DC`), resume 0x477748 | push_all, push ecx (CGameArea*), call, pop_all |
| ZoomWorldEnd | 0x477E55 epilogue, IEex_HookRestore 7 bytes (`pop edi/ebp/ebx/esi; add esp,0x14`), resume 0x477E5C | push_all, call, pop_all |
| CInfinity_GetWorldCoordinatesOverride | 0x5CDFC0, !jmp_dword -- whole body replaced, original dead | native: ecx=this, stack [sret][pt], ret 8 |
| BandSelectGrow | 0x476C02 (resume 0x476DD6) and 0x476C94 (resume 0x476D12) in OnMouseMove | push_all, push ebx (CPoint*), push esi (CGameArea*), call, pop_all; site 2 stub re-creates edi/eax/ecx from +0x3e4/+0x3e8 |
| CInfinity_SetViewPositionOverride | 0x5D11F0, !jmp_dword -- whole body replaced | native: ecx=this, (x, y, flag), ret 0xC |
| CVidMode_DrawEllipse3dOverride | 0x7BC580, !jmp_dword | native thiscall, ret 0x10 |
| CVidMode_DrawRecticle3dOverride | 0x7BC7A0, !jmp_dword | native thiscall |
| WheelShouldZoom | 0x4289C0 via IEex_WriteAssemblyAuto stub | push_all, call, test eax: true -> pop_all + ret 0x10 (original skipped); false -> pop_all, displaced 6 bytes (`push esi; push edi; mov edi,[esp+0x14]`), jmp 0x4289C6 |
| MapViewRectZoom | 0x644B00 entry, IEex_HookRestore 5 bytes (`sub esp,0x24; push ebx; push ebp`), resume 0x644B05 | lea eax,[esp+0xC] (&a3 stack slot) before push_all; push eax, call, pop_all |
| MapWheelPump | 0x645100 entry, IEex_HookRestore 5 bytes (`sub esp,0xC; push ebx; push esi`), resume 0x645105 | push_all, push ecx (control this), call, pop_all |
| MapCenterViewPort | 0x6437A0 replaced by stub | push [esp+4] (&pt), push ecx (this), call (stdcall), ret 4 |
| MapGrabHitTest | 0x642EFA hit-test block in OnLButtonDown via stub | mov ebp,eax; lea eax,[esp+0x20] (&r); push ebp (y), push edi (x), push eax (&r), call; jnz -> 0x642F0D inside, else 0x642F2E |
| DialogPanCompensateY | 0x484B88 in CGameDialogEntry::Handle via stub | push ebp (ptScrollX), push edi (pSprite), push esi (ptScrollY), call (stdcall ret 12); stub re-runs displaced 8 bytes, resume 0x484B90 |
| DialogZoomTick | 0x483F00 entry, IEex_HookRestore 6 bytes (`mov eax,fs:0x0`, re-run by stub) | push_all, call, pop_all |
| ClampScriptViewDest | 0x45F305 clamp block in MoveView, resume 0x45F36F; MoveViewPoint's own clamp skipped (0x45F614 -> jmp 0x45F63D) | lea eax,[esp+0x38] (&destY); push eax, push ebp (destX), call; mov ebp,eax (X returned in eax) |

A 17th site routes through the group without a dedicated helper: the
load-game camera recentre stub at 0x70CC2F (CGameSprite::Unmarshal party
branch) ignores the saved viewport words entirely and calls :5D11F0
directly -- i.e. the SetViewPosition override -- to centre on the
character's own position.

`IEex_Helper_CVidMode_DrawRecticle3dOverride` - 0x1002e5b0 (FUN_1002e5b0)
- __thiscall (param_1 = CRect-like int* [ints +0/+4, shorts
+8/+10/+0xe/+0x10/+0x12], param_2 = 4-int param block), ret 1;
entered from the IEex_CameraZoom_Patch.lua !jmp_dword replacing
CVidMode::DrawRecticle3d 0x7BC7A0 (whole body).

##### Behavior

Crisp move-destination / formation chevrons under zoom (patch comments):
same GL_POINTS problem as the ellipse override. ~250 lines; the decompiler
warns "Type propagation algorithm not settling" but the structure is
clear:

1. Zoom path: scale all six rect values by the zoom around the anchor
   `_DAT_104df388` / `_DAT_104df39c`, keep the zoom in a local, set the
   bracket byte `local_12d`.
2. Matrix bracket: glMatrixMode(GL_MODELVIEW), glPushMatrix
   (`DAT_103debfc`), glLoadIdentity; pop guarded by `local_12d`.
3. Line thickness: lazy INI read `GetPrivateProfileIntA("IEex Options",
   "Destination Marker Thickness", 2, ".\\Icewind2.ini")` cached in
   `DAT_10092770`; below 1 falls back to FUN_1002cfa0() (Selection
   Circle Thickness getter); clamped to [1,6]. Pixel thickness =
   `(int)(t * zoom * 0.5 + 0.5)` clamped to the half-extents.
4. Blend state: FUN_10027020() gate (16-proc GL resolver), then
   glDisable(GL_BLEND) / glEnable(GL_BLEND) through `DAT_104df040` /
   `DAT_104df5a4` bracketing the spans (the decompiler drops the args;
   the second call's literal 0xbe2 = GL_BLEND is in the disassembly).
5. Span plumbing: local function pointers local_50 = 0x7BCBB0
   (horizontal), local_4c = 0x7BCE20 (vertical), local_48 = 0x7BC250
   (third span fn) are passed by address into a stack context struct
   together with the row count, colour (0xffffffff), a destination
   pointer and the rect edges. Four FUN_1002efe0 calls draw the four
   quadrant edge pairs (selector 1/0 x sign +1/-1), each with a row
   count from FUN_1002ee80 -- the first pair uses step 1.0f and count
   from `FUN_1002ee80(1.0f, 0x799AA0(...) - 1)`, the second pair a
   thickness-adjusted step (`fStack_34`) and its own count.
6. Inner FUN_1002ee80(step, maxCount) computes one edge's row count from
   an angular thickness: angle = clamp(acos-clamped-ratio-derived
   angle, 8, 45) degrees, count = (int)(sin(angle) * radius + 0.5)
   clamped to [2, maxCount] (degrees conversion 57.29578, radians
   0.01745329; register-arg mangled: radius/step arrive in XMM regs).
7. Inner FUN_1002efe0(selector, sign, ...) paints one quadrant edge pair
   through the context struct: per row it projects and offsets the two
   endpoints (FUN_1002db80 / FUN_1002dcb0) and calls the selected span
   function (context slots [8]/[9] indexed by `selector ^ 1`) with the
   row length and colour; when the tip flag (context slot [6]) is clear
   it additionally fills the chevron tip via three FUN_1002dd60 triangle
   fills between the re-projected corners and finishes through
   FUN_1002d780 plus a span call through context slot [0xd] and one
   virtual call through the engine object's +0x34 vtable slot. The
   DLL-side projection helpers (FUN_1002db80 / FUN_1002dcb0 /
   FUN_1002dd60 / FUN_1002d780) are not in the seed set and were not
   decompiled.
8. free; pop when bracketed; return 1.

##### Failure fallbacks

FUN_10027020() failing (GL slots unresolved) skips the blend bracket; a
zero span count skips the edges. The guarded pop keeps the matrix stack
balanced on every exit path.

##### Decompiler artifact

Heavy: the "Type propagation algorithm not settling" warning, the three
span-function pointers passed by address through puStack_* aliases
(Ghidra garbage), 12-arg FUN_1002efe0 calls with two values double-packed
into undefined8 locals, and the dropped args on the glDisable/glEnable
pair. The functional summary above follows the disassembly where the
decompile is unreliable.

