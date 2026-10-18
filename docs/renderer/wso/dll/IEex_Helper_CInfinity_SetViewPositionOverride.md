# IEex_Helper_CInfinity_SetViewPositionOverride

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002fd90` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002fd90.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_CameraZoom_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_1002fd90(int param_1,int param_2,int param_3)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int in_ECX;
  int iVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int iVar8;
  float fVar9;
  int local_18;
  int local_14;

  fVar9 = DAT_1008f298;
  if (DAT_1008f298 <= 1.0) {
    fVar9 = 1.0;
  }
  iVar4 = *(int *)(in_ECX + 0x50) - *(int *)(in_ECX + 0x48);
  iVar1 = *(int *)(in_ECX + 0x54) - *(int *)(in_ECX + 0x4c);
  iVar2 = FUN_100258a0();
  iVar6 = iVar4 / 2 - ((0x32 - iVar2) * (int)((float)iVar4 / fVar9)) / 100;
  iVar3 = iVar1 / 2 - ((0x32 - iVar2) * (int)((float)iVar1 / fVar9)) / 100;
  iVar5 = *(int *)(in_ECX + 0x80) - iVar4;
  iVar2 = iVar5 + iVar6;
  iVar7 = *(int *)(in_ECX + 0x84) - iVar1;
  iVar8 = iVar7 + iVar3;
  local_14 = -iVar6;
  if (-iVar2 != iVar6 && iVar2 <= -iVar6) {
    iVar2 = iVar5 / 2;
    local_14 = iVar2;
  }
  local_18 = -iVar3;
  if (-iVar8 != iVar3 && iVar8 <= -iVar3) {
    iVar8 = iVar7 / 2;
    local_18 = iVar8;
  }
  if ((param_3 != 0) && (*(int *)(in_ECX + 0x28c) == 0)) {
    if (*(int *)(in_ECX + 0x80) <= (int)((float)iVar4 / fVar9)) {
      param_1 = iVar5 / 2;
    }
    if (*(int *)(in_ECX + 0x84) <= (int)((float)iVar1 / fVar9)) {
      param_2 = (*(int *)(in_ECX + 0x84) - iVar1) / 2;
    }
  }
  if ((param_1 < local_14) || (local_14 = iVar2, iVar2 < param_1)) {
    *(int *)(in_ECX + 0x164) = local_14 * 10000;
    param_1 = local_14;
  }
  if ((param_2 < local_18) || (local_18 = iVar8, iVar8 < param_2)) {
    *(int *)(in_ECX + 0x168) = local_18 * 10000;
    param_2 = local_18;
  }
  *(int *)(in_ECX + 0x40) = param_1;
  *(int *)(in_ECX + 0x44) = param_2;
  *(undefined4 *)(in_ECX + 0x1be) = 1;
  if (param_3 != 0) {
    *(int *)(in_ECX + 0x164) = param_1 * 10000;
    *(int *)(in_ECX + 0x168) = param_2 * 10000;
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

`IEex_Helper_CInfinity_SetViewPositionOverride` - 0x1002fd90
(FUN_1002fd90) - __thiscall (ecx = CInfinity* this, stack int x, int y,
uint flag, ret 0xC); entered from the IEex_CameraZoom_Patch.lua
!jmp_dword replacing CInfinity::SetViewPosition 0x5D11F0, and re-entered
by ZoomWorldBegin, MapCenterViewPort and the load-game recentre stub,
which all call the engine address 0x5D11F0 and therefore land here.

##### Behavior

Overscroll-aware camera positioning: allows panning about half a
viewport past every map edge at EVERY zoom (z == 1 included) so content
the bottom UI occludes can be repositioned (patch comment). Mechanics:

1. effectiveZoom = max(zoom, 1.0) with zoom = the animated
   `DAT_1008f298`.
2. Pull-to-centre margins from inner FUN_100258a0 (Camera Overscroll
   percent, 300 ms cache): margin = `(50 - overscroll) *
   (viewportWidth/zoom) / 100` per axis -- at the default 50 the margin
   vanishes; at 0 it is the full half-viewport/zoom.
3. Clamp the destination against the area size (`+0x80` / `+0x84`)
   extended by those margins. Destinations fully outside the extended
   range centre instead (small-map handling), with extra centring when
   `flag != 0` and `+0x28c == 0` and the area fits inside the zoomed
   viewport.
4. On rejection at an edge, store the clamped destination as the scroll
   targets `+0x164` / `+0x168` = `dest*10000` (the engine's fixed-point
   scroll targets).
5. Set the view position `+0x40` / `+0x44` = the (possibly clamped)
   position and the dirty byte `+0x1be = 1`.
6. When `flag` is set (absolute framing request) also store `+0x164` /
   `+0x168` = `pos*10000` so the glide converges to the same point.
7. Return 1.

Vanilla's small-map centring is kept only for absolute framing requests
(-- as a clamp it would forbid the pan outright at z == 1 on any map not
larger than the screen, patch comment). Glides arrive through the
incremental path and are not centred; the Lua AutoScroll code states the
centring itself in that case (see GetCameraZoom entry).

##### Failure fallbacks

Cannot fail; every input lands on a legal position. The function always
returns 1.

##### Decompiler artifact

None significant.

