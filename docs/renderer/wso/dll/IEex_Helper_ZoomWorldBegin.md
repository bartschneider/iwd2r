# IEex_Helper_ZoomWorldBegin

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100288d0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_100288d0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_CameraZoom_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_100288d0(int param_1)

{
  uint uVar1;
  char cVar2;
  int iVar3;
  int iVar4;
  undefined4 uVar5;
  int iVar6;
  int iVar7;
  uint uVar8;
  int *piVar9;
  bool bVar10;
  float in_XMM0_Da;
  float fVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  double dVar15;
  undefined1 auStack_48 [2];
  char local_46;
  char local_45;
  float local_44;
  float local_40;
  float local_3c;
  double local_38;
  uint local_30;
  undefined4 uStack_2c;
  int local_28;
  int local_24;
  uint local_20;
  int local_1c;
  ulonglong local_18;
  uint local_c;

  iVar6 = DAT_10094548;
  local_c = DAT_1008f040 ^ (uint)auStack_48;
  DAT_104df0ee = 0;
  if ((param_1 != 0) &&
     (local_44 = (float)(param_1 + 0x4cc),
     *(int *)(param_1 + 0x51c) - *(int *)(param_1 + 0x514) == 0x200)) {
    local_3c = *(float *)(*DAT_10094384 + 0x1c54);
    if (local_3c == 0.0) {
      DAT_104df0ee = 0;
      return;
    }
    piVar9 = (int *)((int)local_3c + 0x3816);
    local_18 = 0;
    local_24 = 0;
    local_20 = 0;
    local_28 = 0;
    local_1c = 0;
    local_38 = (double)CONCAT44(local_38._4_4_,piVar9);
    local_40 = 8.40779e-45;
    local_30 = 0;
    uStack_2c = 0;
    do {
      iVar6 = *piVar9;
      if (iVar6 != -1) {
        local_18 = local_18 & 0xffffffff00000000;
        cVar2 = (*DAT_1009436c)(iVar6,0,&local_18,0);
        if ((cVar2 == '\0') && ((float)local_18 != 0.0)) {
          if ((*(int *)((int)(float)local_18 + 0x12) == param_1) &&
             ((uVar8 = *(uint *)((int)(float)local_18 + 6), -1 < (int)uVar8 &&
              (uVar1 = *(uint *)((int)(float)local_18 + 10), -1 < (int)uVar1)))) {
            bVar10 = CARRY4(local_20,uVar8);
            local_20 = local_20 + uVar8;
            local_24 = local_24 + ((int)uVar8 >> 0x1f) + (uint)bVar10;
            bVar10 = CARRY4(local_30,uVar1);
            local_30 = local_30 + uVar1;
            local_1c = local_1c + ((int)uVar1 >> 0x1f) + (uint)bVar10;
            local_28 = local_28 + 1;
          }
          (*DAT_100944d0)(iVar6);
        }
      }
      piVar9 = (int *)(local_38._0_4_ + 4);
      local_40 = (float)((int)local_40 + -1);
      local_38 = (double)CONCAT44(local_38._4_4_,piVar9);
    } while (local_40 != 0.0);
    if (local_28 < 1) {
      return;
    }
    iVar7 = local_28 >> 0x1f;
    local_38._0_4_ = local_28;
    iVar6 = *(int *)((int)local_44 + 0x48);
    iVar4 = *(int *)((int)local_44 + 0x50);
    iVar3 = __alldiv(local_20,local_24,local_28,iVar7);
    local_18 = CONCAT44(local_18._4_4_,iVar3 - (iVar4 - iVar6) / 2);
    iVar6 = *(int *)((int)local_44 + 0x54);
    iVar4 = __alldiv(local_30,local_1c,local_38._0_4_,iVar7);
    iVar4 = iVar4 - (iVar6 - *(int *)((int)local_44 + 0x4c)) / 2;
    local_38 = (double)CONCAT44(local_38._4_4_,iVar4);
    iVar7 = (*(int *)((int)local_44 + 0x80) - *(int *)((int)local_44 + 0x50)) +
            *(int *)((int)local_44 + 0x48);
    iVar3 = 0;
    if (-1 < iVar7) {
      iVar3 = iVar7;
    }
    iVar7 = (*(int *)((int)local_44 + 0x84) - iVar6) + *(int *)((int)local_44 + 0x4c);
    iVar6 = 0;
    if (-1 < iVar7) {
      iVar6 = iVar7;
    }
    if ((int)(float)local_18 < 0) {
      iVar7 = 0;
    }
    else {
      iVar7 = (int)(float)local_18;
      if (iVar3 < (int)(float)local_18) {
        iVar7 = iVar3;
      }
    }
    if (iVar4 < 0) {
      (*(code *)0x5d11f0)(iVar7,0,1);
      return;
    }
    if (iVar6 < iVar4) {
      iVar4 = iVar6;
    }
    (*(code *)0x5d11f0)(iVar7,iVar4,1);
    return;
  }
  if (*(int *)(*DAT_100944b8 + 0x91c) == 0) {
    DAT_104df0ee = 0;
    return;
  }
  LOCK();
  DAT_10094548 = 0;
  UNLOCK();
  if (iVar6 != 0) {
    FUN_10025930();
    DAT_10092798 = in_XMM0_Da;
  }
  fVar11 = DAT_1008f298;
  if ((DAT_1008f298 != DAT_10092798) &&
     ((fVar13 = DAT_10092798 - DAT_1008f298, 0.01 <= fVar13 ||
      (fVar11 = DAT_10092798, fVar13 <= -0.01)))) {
    fVar11 = DAT_1008f298 + fVar13 * 0.22;
  }
  DAT_1008f298 = fVar11;
  local_44 = DAT_1008f298;
  if (((param_1 == 0) || (*(char *)(param_1 + 0x1ef) == '\0')) || (*(int *)(param_1 + 0x238) != 9))
  goto LAB_10028c3c;
  iVar6 = *(int *)(param_1 + 0x25a);
  iVar4 = *(int *)(param_1 + 0x256);
  iVar3 = *DAT_10094318 - 1;
  if (iVar6 < 1) {
    if (iVar4 < 1) {
      uVar5 = 8;
    }
    else {
      if (iVar4 < iVar3) goto LAB_10028bf2;
      uVar5 = 2;
    }
  }
  else {
LAB_10028bf2:
    if (iVar6 < (int)(*DAT_10094328 - 1)) {
LAB_10028c0c:
      if (iVar6 < 1) {
        uVar5 = 1;
      }
      else if (iVar6 < (int)(*DAT_10094328 - 1)) {
        if (iVar4 < 1) {
          uVar5 = 7;
        }
        else {
          if (iVar4 < iVar3) goto LAB_10028c3c;
          uVar5 = 3;
        }
      }
      else {
        uVar5 = 5;
      }
    }
    else if (iVar4 < 1) {
      uVar5 = 6;
    }
    else {
      if (iVar4 < iVar3) goto LAB_10028c0c;
      uVar5 = 4;
    }
  }
  *(undefined4 *)(param_1 + 0x238) = uVar5;
LAB_10028c3c:
  local_46 = '\0';
  if (local_44 == 1.0) {
    local_45 = '\0';
    if (DAT_104df1fd != '\0') {
      local_46 = '\x01';
      DAT_104df1fd = '\0';
      DAT_1009275c = 1.0;
    }
  }
  else {
    local_45 = '\x01';
  }
  if ((param_1 != 0) && (*(char *)(param_1 + 0x1ef) != '\0')) {
    local_3c = (float)(*(int *)(param_1 + 0x51c) + *(int *)(param_1 + 0x514)) * 0.5;
    local_40 = (float)(*(int *)(param_1 + 0x520) + *(int *)(param_1 + 0x518)) * 0.5;
    if (DAT_103d4756 != '\0') {
      local_38 = (double)local_3c;
      dVar15 = floor(local_38);
      local_38 = (double)local_40;
      local_18 = CONCAT44(local_18._4_4_,(float)dVar15);
      dVar15 = floor(local_38);
      local_40 = (float)dVar15;
      local_38 = (double)CONCAT44(local_38._4_4_,local_40);
      local_3c = (float)local_18;
    }
    iVar6 = FUN_100258a0();
    if ((DAT_10092b7c != iVar6) || ((DAT_10092b7c = iVar6, local_46 != '\0' && (iVar6 < 0x32)))) {
      DAT_10092b7c = iVar6;
      *(int *)(param_1 + 0x630) = *(int *)(param_1 + 0x50c) * 10000;
      *(int *)(param_1 + 0x634) = *(int *)(param_1 + 0x510) * 10000;
      (*(code *)0x5d11f0)(*(int *)(param_1 + 0x50c),*(int *)(param_1 + 0x510),0);
    }
    if (local_45 != '\0') {
      iVar6 = *(int *)(param_1 + 0x518);
      DAT_104df1fd = '\x01';
      DAT_103ded94 = (float)(*(int *)(param_1 + 0x520) - iVar6) * 0.5;
      _DAT_104df388 = local_3c;
      _DAT_104df39c = local_40;
      if (local_44 != DAT_1009275c) {
        iVar4 = *(int *)(param_1 + 0x25a);
        iVar3 = *(int *)(param_1 + 0x514);
        if (*(int *)(param_1 + 0x514) <= *(int *)(param_1 + 0x256)) {
          iVar3 = *(int *)(param_1 + 0x256);
          if (*(int *)(param_1 + 0x51c) < *(int *)(param_1 + 0x256)) {
            iVar3 = *(int *)(param_1 + 0x51c);
          }
        }
        if ((iVar6 <= iVar4) && (iVar6 = iVar4, *(int *)(param_1 + 0x520) < iVar4)) {
          iVar6 = *(int *)(param_1 + 0x520);
        }
        fVar11 = 1.0 / DAT_1009275c - 1.0 / local_44;
        fVar12 = ((float)iVar3 - local_3c) * fVar11;
        fVar11 = ((float)iVar6 - local_40) * fVar11;
        fVar13 = -0.5;
        if (fVar12 < 0.0) {
          fVar14 = -0.5;
        }
        else {
          fVar14 = 0.5;
        }
        if (0.0 <= fVar11) {
          fVar13 = 0.5;
        }
        iVar4 = *(int *)(param_1 + 0x50c) + (int)(fVar14 + fVar12);
        *(int *)(param_1 + 0x630) = iVar4 * 10000;
        iVar6 = (int)(fVar13 + fVar11) + *(int *)(param_1 + 0x510);
        *(int *)(param_1 + 0x634) = iVar6 * 10000;
        (*(code *)0x5d11f0)(iVar4,iVar6,0);
        DAT_1009275c = local_44;
      }
    }
    if (*(int *)(param_1 + 0x3dc) != -1) {
      FUN_1002a3b0();
      iVar6 = *(int *)(param_1 + 0x514);
      if ((iVar6 <= local_38._0_4_) &&
         (iVar4 = *(int *)(param_1 + 0x51c) + -1, iVar6 = local_38._0_4_, iVar4 < local_38._0_4_)) {
        iVar6 = iVar4;
      }
      uVar8 = *(int *)(param_1 + 0x518);
      if ((*(int *)(param_1 + 0x518) <= (int)local_30) &&
         (iVar4 = *(int *)(param_1 + 0x520) + -1, uVar8 = local_30, iVar4 < (int)local_30)) {
        uVar8 = iVar4;
      }
      *(int *)(param_1 + 0x3e4) =
           ((int)(((float)iVar6 - local_3c) / local_44 + local_3c + 0.5) + *(int *)(param_1 + 0x50c)
           ) - *(int *)(param_1 + 0x514);
      *(int *)(param_1 + 1000) =
           ((int)(((float)(int)uVar8 - local_40) / local_44 + local_40 + 0.5) -
           *(int *)(param_1 + 0x518)) + *(int *)(param_1 + 0x510);
    }
    if ((local_45 != '\0') && (cVar2 = FUN_100257e0(), cVar2 != '\0')) {
      uVar5 = 0x1700;
      (*DAT_104df52c)();
      (*DAT_104df50c)();
      uVar8 = 0;
      fVar11 = local_44;
      (*DAT_104df524)(local_40,local_44,0);
      (*DAT_104df56c)(uVar5,uVar5,0x3f800000);
      (*DAT_104df524)(uVar8 ^ 0x80000000,-fVar11,0);
      DAT_104df0ee = 1;
    }
  }
  return;
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

`IEex_Helper_ZoomWorldBegin` - 0x100288d0 (FUN_100288d0) - stdcall, 1
stack arg (CGameArea* this, ret 4); entered from the IEex_CameraZoom_Patch.lua
IEex_HookRestore at the CGameArea::Render entry 0x477740.

##### Behavior

Runs at the top of every world-frame render, before the engine body. Two
paths:

Early path -- when the viewport width (`+0x51c` minus `+0x514` of the
CGameArea's viewport rect) is exactly 0x200 (512 px): iterate 64 ints at
`*(areaPtr+0x3816)` (areaPtr = `*(*DAT_10094384+0x1c54)`), resolve each
id through the `DAT_1009436c` slot into an object, and for objects whose
`+0x12` field equals the passed CGameArea accumulate `*(obj+6)` / `*(obj+10)`
into 64-bit sums (object release through the `DAT_100944d0` slot each
iteration). Average, clamp, and call 0x5D11F0(x, y, 1) -- i.e. the
SetViewPosition override -- to centre on the party average. Returns
without touching the GL state. This reads like a cinematic/letterbox
special case (512 px wide viewport).

Main path, in order:

1. Clear the recompute flag `DAT_10094548` under LOCK; if it was set,
   refresh the target zoom `DAT_10092798` through inner FUN_10025930
   (result in XMM0; the store is invisible in the decompile).
2. Lerp the animated zoom `DAT_1008f298` toward the target by 0.22 per
   tick; snap onto the target when within 0.01. Keep
   `DAT_104df1fd` (was-zoomed latch) in step.
3. Re-encode the keyboard-scroll mode-9 state at `this+0x238` into
   8-direction codes derived from `+0x256` / `+0x25a` against the screen
   dimensions (`*DAT_10094318-1` / `*DAT_10094328-1`).
4. Re-read the Camera Overscroll cache via inner FUN_100258a0; when the
   cached value (kept in `DAT_10092b7c`) changed, rewrite the scroll
   targets `+0x630` / `+0x634` as `view*10000` and call 0x5D11F0 to
   re-apply the position under the new margins.
5. Zoom-toward-cursor: shift the view position by the mouse delta scaled
   with `1/DAT_1009275c - 1/zoom` (truncating rounding), so the point
   under the cursor stays under the cursor while the ease runs.
6. Refresh the followed-sprite screen offsets `+0x3e4` / `+0x3e8`
   through FUN_1002a3b0 (the world-to-zoomed-screen mapper shared with
   BandSelectGrow).
7. GL bracket (see COMMON): glMatrixMode(GL_MODELVIEW), glLoadIdentity,
   glTranslatef(cx, cy, 0), glScalef(z, z, 1.0f), glTranslatef(-cx,
   -cy, 0), then set `DAT_104df0ee = 1`. The anchor (cx, cy) is the
   viewport centre; z is the animated zoom.
8. Security-cookie epilogue; `ret 4`.

##### Failure fallbacks

The GL bracket is gated on the FUN_100257e0 one-shot resolver; if the
slots are unavailable the bracket is skipped and `DAT_104df0ee` stays
clear, so ZoomWorldEnd no-ops too and the frame renders unzoomed. The
helper cannot fail otherwise -- the Lua patch installed it inside
CGameArea::Render, and it assumes the class invariants of its argument.
If the whole patch is not installed (software renderer) this function is
never called.

##### Decompiler artifact

The GL bracket arg marshalling is mangled: the decompiler shows the
scale call taking the modelview constant and the translate pair taking
overlapping locals. The objdump sequence in COMMON is authoritative
(scale takes the same zoom float in both x and y plus literal 0x3f800000;
the second translate negates exactly the two floats the first translate
used, via xorps with 0x80000000 at 0x10088590).

