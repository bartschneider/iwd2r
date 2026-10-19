# IEex_Helper_DialogZoomTick

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10029460` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10029460.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_CameraZoom_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

void FUN_10029460(void)

{
  uint uVar1;
  int iVar2;

  if (((*(int *)(*DAT_100944b8 + 0x91c) != 0) && (*DAT_10094384 != 0)) &&
     (iVar2 = *(int *)(*DAT_10094384 + 0x1c54), iVar2 != 0)) {
    if ((*(int *)(iVar2 + 0x43e2) != 0x182) && (*(int *)(iVar2 + 0x43e2) != 0x502)) {
      DAT_104df40c = 0;
      DAT_103d47ef = 0;
      return;
    }
    if ((((DAT_103dec25 == '\0') && (DAT_104df40c != '\0')) &&
        ((-1 < (int)DAT_100927a4 &&
         (((uVar1 = (uint)*(byte *)(iVar2 + 0x37e0), uVar1 == DAT_100927a4 && (uVar1 < 6)) &&
          (iVar2 = *(int *)(iVar2 + 0x37e2 + uVar1 * 4), iVar2 != 0)))))) &&
       (*(char *)(iVar2 + 0x1ef) != '\0')) {
      if (DAT_10092798 == _DAT_104df3cc) {
        if (DAT_103d47ef == '\0') {
          return;
        }
      }
      else {
        _DAT_104df3cc = DAT_10092798;
        DAT_103d47ef = '\x01';
      }
      if (DAT_1008f298 == DAT_10092798) {
        DAT_103d47ef = '\0';
        iVar2 = DAT_104df380;
        if (1.0 < DAT_1008f298) {
          iVar2 = DAT_103dedc8 - (int)(DAT_103ded94 + 0.5);
        }
        (*(code *)0x68c340)(DAT_104df554,iVar2,0x10);
      }
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

`IEex_Helper_DialogZoomTick` - 0x10029460 (FUN_10029460) - stdcall, no
args; entered from the IEex_CameraZoom_Patch.lua IEex_HookRestore at
CGameDialogSprite::AsynchronousUpdate entry 0x483F00 (called
unconditionally every tick by CScreenWorld::AsynchronousUpdate -- the
same async thread that runs the engine's dialog scroll, so nothing
writes the scroll target concurrently; patch comments).

##### Behavior

Re-issues the cached dialog scroll line once the zoom ease settles (the
line's target is zoom-dependent twice over: the pivot-row branch and the
HUD-strip offset AdjustAutoScrollY adds inside StartScroll, so a wheel
notch after the line starts leaves the speaker drifting, or scaled clean
off the screen when the target was clamped at a map edge):

1. Gates: GL-mode flag, state root, viewport; then the current screen id
   `*(vp+0x43e2)` must be 0x182 or 0x502 (the two dialog-capable world
   screens) -- otherwise disarm (`DAT_104df40c = 0`, `DAT_103d47ef = 0`)
   and return.
2. Active test: script zoom inactive (`DAT_103dec25 == 0`), armed
   (`DAT_104df40c != 0`), snapshot portrait index >= 0, current index
   matches and is < 6, that portrait non-null, and `*(char*)(port+0x1ef)`
   set. Any miss -> return.
3. Track target-zoom changes: when `DAT_10092798 == _DAT_104df3cc`
   (target unchanged since the line was cached) and `DAT_103d47ef == 0`,
   nothing to do -> return; when the target changed, refresh the
   snapshot and set `DAT_103d47ef = 1`.
4. When the ease has settled (`DAT_1008f298 == DAT_10092798`): clear
   `DAT_103d47ef` and re-issue the line through engine call 0x68C340
   with (ptScrollX = `DAT_104df554`, y = zoom > 1 ?
   `DAT_103dedc8 - (int)(DAT_103ded94 + 0.5)` : `DAT_104df380`, 0x10).

Net effect: exactly one re-pan per zoom change, on the tick the ease
lands. The armed latch stays set while the dialog screen is up, so
subsequent zoom changes re-trigger.

##### Failure fallbacks

Every gate miss is a silent no-op; disarming on non-dialog screens
prevents stale re-pans. If the ease never settles the helper keeps
waiting (no timeout), which matches the ease design (0.22 lerp with snap
always converges).

##### Decompiler artifact

None significant; the 0x68C340 call decompiles as a direct engine call
with three args.

