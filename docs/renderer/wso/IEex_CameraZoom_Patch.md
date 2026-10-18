# IEex_CameraZoom_Patch.lua — camera zoom: GL world scale, zoom-aware picking/scroll, map-screen and dialog integration

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_CameraZoom_Patch.lua`
(367 lines). One IIFE, GL-only (returns immediately when
`[Program Options] "3D Acceleration"` is 0 — "camera zoom is a GL
MODELVIEW feature; install none of it under the software renderer. See
IEex_Render_Patch.lua"). The zoom itself lives in the DLL
(`Export_ZoomWorldBegin`/`Export_ZoomWorldEnd` scale MODELVIEW about the
viewport centre between the world render and the UI); this file installs
the 13 hooks/replacements that make the REST of the engine agree with a
scaled world.

## Intent

1. **Stage 1 — scale the world, not the UI.** Bracket `CGameArea::Render`
   (entry 0x477740, exit epilogue 0x477E55): entry calls
   `IEex_Helper_ZoomWorldBegin` with the CGameArea*, exit resets
   MODELVIEW to identity. UI panels draw after Render with the matrix
   back at identity, so only the world scales. `glLoadIdentity` keeps the
   bracket stateless — the engine's `m_bAreaLoaded` early-return can
   never imbalance the stack.
2. **Stage 2 — zoom-aware picking.** Replace
   `CInfinity::GetWorldCoordinates` (0x5CDFC0) outright (direct jmp; the
   __thiscall struct-return ABI matches, original body dead) so mouse
   picking / selection / move-orders align when zoomed.
3. **Stage 2b — zoom-aware band-select growth.** `CGameArea::OnMouseMove`
   (0x476970) grows `m_selectSquare` with INLINE vanilla 1:1 arithmetic
   while the anchor goes through the zoom-aware override — at z>1 the two
   edges live in different spaces and the rect gains phantom size
   `((dist from viewport centre)*(1-1/z))`, so a plain click misreads as
   a band-select. Both growth sites (0x476C02 group-protect/state-3, and
   0x476C94 normal) are routed through `IEex_Helper_BandSelectGrow`
   ("identical to the vanilla arithmetic at z==1"), jumping to the
   original resume points (0x476DD6 / 0x476D12) with the expected
   registers recreated.
4. **Stage 3a — overscroll-aware `CInfinity::SetViewPosition`**
   (0x5D11F0): replaced with a reimplementation that allows panning ~half
   a viewport past every map edge at EVERY zoom (z==1 included) so
   content the bottom UI occludes can be repositioned. Vanilla small-map
   centring is kept only for absolute framing requests (`bSetExactScale`).
   The same override is the landing target of the map-screen and
   load-recentre patches below (they `call :5D11F0`).
5. **Crisp circles and chevrons.** Selection/move-dest circles
   (`CVidMode::DrawEllipse3d` 0x7BC580) and the formation reticle arcs
   (`CVidMode::DrawRecticle3d` 0x7BC7A0) are GL_POINTS at glPointSize 1.0
   — px size is immune to the MODELVIEW scale, so zoomed-in they are
   sparse dots. Both are replaced with zoom-aware reimplementations that
   scale centre+axes by z and rasterise at identity MODELVIEW.
6. **Wheel gate.** `CBaldurEngine::OnMouseWheel` (0x4289C0) scrolls the
   message-log scrollbar on every notch; when the cursor is over the
   world (`IEex_Helper_WheelShouldZoom` = world screen + GL + not over a
   UI panel) the original is skipped entirely (`ret 0x10`) so only the
   zoom happens. The zoom tick itself is recorded in the DLL's
   window_proc under the same predicate.
7. **Area Map screen** (four patches on `CUIControlButtonMapAreaMap`):
   the green "you are here" rect is inset to rViewPort/zoom at DRAW time
   only (field_72A untouched so click-to-recentre math stays vanilla),
   wheel ticks are pumped into the zoom target from the map's
   TimerAsynchronousUpdate, `CenterViewPort`'s UNZOOMED pre-clamp is
   replaced (the small rect used to "move as if not zoomed"), and the
   drag-grab hit-test runs against the VISIBLE zoomed rect while the
   grab OFFSET stays 1:1.
8. **Load-game camera recentre.** On load, `CGameSprite::Unmarshal`
   restores the camera from the GAM party record's stored viewport
   top-left — which the overscroll-legal SetViewPosition makes negative /
   past-map, wrapping in the WORD cast, and which is anyway a stale
   top-left captured under a different zoom. The stored view is ignored:
   the stub centres on the character's own position, OnPortraitLDblClick
   math, and the SetViewPosition override does the clamp. "Last party
   member processed wins = party on screen."
9. **Dialog auto-scroll zoom compensation.**
   `CGameDialogEntry::Handle`'s extra vertical `ptReference.y/2` nudge
   lands the speaker OFF the zoom pivot row, so the centre scale
   amplifies it ("Bubb: bottom speaker pans the wrong way");
   `IEex_Helper_DialogPanCompensateY` retargets the scroll to
   `speakerWorldY - vpHeight/2` (dead-centre on the pivot, identity at
   z<=1) and CACHES the line's whole target; `IEex_Helper_DialogZoomTick`
   (hooked into `CGameDialogSprite::AsynchronousUpdate`, the async tick)
   re-issues the cached line once a later zoom ease settles.
10. **Scripted camera framing.** `MoveViewPoint`/`MoveView` clamp the
    authored viewport top-left twice over — to `[0, nAreaX]` (throws away
    authored X for any point in the left half of the map; "at 4K that is
    anything under 1920") and to `[0, nAreaX - W]` with the RAW viewport
    ("COLLAPSES in any area narrower than the screen: measured in AR2000
    (3328 wide) at 3840x2160, maxX = -512, so every shot of the Torak
    cutscene framed 602px left of its authored centre"). MoveViewPoint's
    clamp is skipped outright; MoveView's is replaced by
    `IEex_Helper_ClampScriptViewDest` using the same range as the
    SetViewPosition override, narrowed to the visible rect so z==1 keeps
    vanilla framing.

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x477740 | `CGameArea::Render` | src/CGameArea.cpp:5394 (exact marker; decl follows) | `IEex_Helper_ZoomWorldBegin` | `IEex_HookRestore(0x477740, 0, 8, ...)`: push_all, `push ecx` (CGameArea*), call begin, pop_all. 8 displaced bytes (`sub esp,0x14; mov eax,ds:0x8CF6DC`) re-run, continue 0x477748. |
| 0x477E55 | inside `CGameArea::Render` (entry 0x477740) — epilogue before `ret 0x8` @0x477E5C | src/CGameArea.cpp:5394 (hook site itself unresolved in our tree) | `IEex_Helper_ZoomWorldEnd` | `IEex_HookRestore(0x477E55, 0, 7, ...)`: push_all, call end, pop_all. 7 displaced bytes (`pop edi/ebp/ebx/esi; add esp,0x14`) re-run, continue 0x477E5C. |
| 0x5CDFC0 | `CInfinity::GetWorldCoordinates(const CPoint&)` | src/CInfinity.cpp:1596 (exact marker; decl follows) | `IEex_Helper_CInfinity_GetWorldCoordinatesOverride` | `IEex_WriteAssembly`: head → `!jmp_dword` to the override (direct replacement; __thiscall struct-return ABI, ret 0x8). Original body dead. |
| 0x476C02 | inside `CGameArea::OnMouseMove(const CPoint&)` (entry 0x476970) — growth site 1 | src/CGameArea.cpp:5061 (marker at 5060; hook site itself unresolved in our tree) | `IEex_Helper_BandSelectGrow` | `IEex_WriteAssembly`: push_all, `push ebx` (CPoint* pt), `push esi` (CGameArea*), call helper, pop_all, `!jmp_dword :476DD6`. Replaces the inline 1:1 growth (0x476C02..0x476C77). |
| 0x476C94 | inside `CGameArea::OnMouseMove` — growth site 2 | src/CGameArea.cpp:5061 (hook site itself unresolved in our tree) | `IEex_Helper_BandSelectGrow` | Same shape, resume 0x476D12 — after the call the stub recreates the width-test registers the replaced range would have left (`edi=right`, `ebp=left`, `eax=bottom`, `ecx/flags = right-left`: `mov edi,[esi+0x3E4]; mov eax,[esi+0x3E8]; mov ecx,edi; sub ecx,ebp`). |
| 0x5D11F0 | `CInfinity::SetViewPosition(int, int, BOOLEAN)` | src/CInfinity.cpp:3058 (exact marker; decl follows) | `IEex_Helper_CInfinity_SetViewPositionOverride` | `IEex_WriteAssembly`: head → `!jmp_dword` (direct replacement; __thiscall(int,int,uint), ret 0xC). Overscroll-aware clamping; absolute (`bSetExactScale`) requests keep small-map centring. |
| 0x7BC580 | `CVidMode::DrawEllipse3d(const CPoint&, const CSize&, const CRect&, COLORREF)` | src/CVideo3d.cpp:1487 (exact marker; decl follows) | `IEex_Helper_CVidMode_DrawEllipse3dOverride` | `IEex_WriteAssembly`: head → `!jmp_dword` (direct replacement, ret 0x10). Zoom-aware solid-circle rasteriser. |
| 0x7BC7A0 | `CVidMode::DrawRecticle3d(const CVIDMODE_RECTICLE_DESCRIPTION&, const CRect&, COLORREF)` | src/CVideo3d.cpp:1580 (exact marker; decl follows) | `IEex_Helper_CVidMode_DrawRecticle3dOverride` | Same — chevron arcs. |
| 0x4289C0 | `CBaldurEngine::OnMouseWheel(BOOL, LONG, DWORD, WORD)` | src/CBaldurEngine.cpp:827 (exact marker; decl follows) | `IEex_Helper_WheelShouldZoom` | Auto cave: push_all, call predicate, `test eax,eax`; zr → re-run the 6 displaced bytes (`push esi; push edi; mov edi,[esp+0x14]`) and `!jmp_dword :4289C6` (original runs); nz → pop_all + `ret 0x10` (original skipped, only the zoom happens). Site: 5-byte jmp + nop. |
| 0x644B00 | `CUIControlButtonMapAreaMap::RenderViewRect(CVidInf*, const CRect&, const CRect&)` | src/CScreenMap.cpp:1927 (exact marker; decl follows) | `IEex_Helper_MapViewRectZoom` | `IEex_HookRestore(0x644B00, 0, 5, ...)`: `lea eax,[esp+0xC]` (&a3 slot, absolute — preserved by push_all), push_all, `push eax`, call helper, pop_all. Helper repoints the on-stack &a3 at a static rViewPort/zoom copy; 5 displaced bytes (`sub esp,0x24; push ebx; push ebp`) re-run, continue 0x644B05. |
| 0x645100 | `CUIControlButtonMapAreaMap::TimerAsynchronousUpdate(BOOLEAN)` | src/CScreenMap.cpp:2067 (exact marker; decl follows) | `IEex_Helper_MapWheelPump` | `IEex_HookRestore(0x645100, 0, 5, ...)`: push_all, `push ecx` (this), call helper, pop_all; 5 displaced bytes (`sub esp,0xC; push ebx; push esi`) re-run, continue 0x645105. Drains recorded wheel ticks into g_fZoomTarget and repaints. |
| 0x6437A0 | `CUIControlButtonMapAreaMap::CenterViewPort(const CPoint&)` | src/CScreenMap.cpp:1501 (exact marker; decl follows) | `IEex_Helper_MapCenterViewPort` | Head → auto stub: `push [esp+4]` (&pt), `push ecx` (this), call helper, `ret 4`. The UNZOOMED `[0, nArea - rViewPort]` pre-clamp is gone; the helper passes the raw point to the SetViewPosition override and reloads the clamped nNewX/nNewY into field_72A. |
| 0x642EFA | inside `CUIControlButtonMapAreaMap::OnLButtonDown(CPoint)` (entry 0x642DA0) — the PtInRect hit-test | src/CScreenMap.cpp:1322 (marker at 1321; hook site itself unresolved in our tree) | `IEex_Helper_MapGrabHitTest` | Auto grab-stub: `mov ebp,edi` (recreate a displaced byte), `lea eax,[esp+0x20]` (&r), `push ebp` (y), `push edi` (x), `push eax`, call helper, `test eax,eax`; nz → `!jmp_dword :642F0D` (inside = drag-grab), else `!jmp_dword :642F2E` (outside = click-to-recentre). Site: 5-byte jmp. The grab OFFSET at 0x642F0D still reads the unzoomed field_72A rect, so drags stay 1:1. |
| 0x70CC2F | inside `CGameSprite::Unmarshal(CSavedGamePartyCreature*, BOOLEAN, BOOLEAN)` (entry 0x70C600) — party camera restore | src/CGameSprite.cpp:8337 (marker `// 0x70C600` at 8336; hook site itself unresolved in our tree) | — | 27-byte range (0x70CC2F..0x70CC4A) replaced by jmp into an auto stub: reloads `m_posX/m_posY` from `pCreature` (+0x20/+0x22, WORD), recomputes centred view via `rViewPort` (`[esi+0x51C]` minus half extents: `mov edx,[esi+0x51C]; sub edx,[esi+0x514]; sar edx,1; sub ecx,edx` …), `push 1` (bSetExactScale), pushes x/y, `lea ecx,[esi+0x4CC]`, `!call :5D11F0` (the override), `!jmp_dword :70CC75`. Site gets the jmp + 22 NOPs. ebx = pCreature, esi = pArea. |
| 0x484B88 | inside `CGameDialogEntry::Handle(CGameSprite*, COLORREF, int)` (entry 0x484900) — after the GetViewPosition call | src/CGameDialog.cpp:904 (marker `// 0x484900` at 903; hook site itself unresolved in our tree) | `IEex_Helper_DialogPanCompensateY` | Auto stub at 0x484B88: `push ebp` (arg3 ptScrollX), `push edi` (arg2 pSprite), `push esi` (arg1 ptScrollY), call helper, then re-run the 8 displaced bytes (`mov esi,eax`-shaped: `8B F0`; `mov ecx,[esp+0x18]`; `mov eax,[esp+0x1C]`) and `!jmp_dword :484B90`. Retargets the scroll to the pivot row; caches the line target. |
| 0x483F00 | `CGameDialogSprite::AsynchronousUpdate()` | src/CGameDialog.cpp:636 (exact marker; decl follows) | `IEex_Helper_DialogZoomTick` | `IEex_HookRestore(0x483F00, 0, 6, ...)`: push_all, call helper, pop_all; 6 displaced bytes (`mov eax,fs:0x0`) re-run by the stub before the SEH frame. Re-issues the cached dialog line once the zoom ease settles. Deliberately NOT hooked on `CScreenWorld::AsynchronousUpdate` (0x68C3D0) — IEex baseline already HookRestores its first 7 bytes there for `IEex_Extern_CScreenWorld_AsynchronousUpdate` (IEex_Gui_Patch.lua); a second HookRestore would capture the first hook's jmp as its restore bytes and loop. |
| 0x45F305 | inside `CGameAIBase::MoveView(CPoint, int)` (entry 0x45F2A0) — the clamp block 0x45F305..0x45F36F | src/CGameAIBase.cpp:1879 (marker at 1878; hook site itself unresolved in our tree) | `IEex_Helper_ClampScriptViewDest` | `IEex_WriteAssembly`: `lea eax,[esp+0x38]` (&dest.y, BEFORE the pushes), `push eax`, `push ebp` (dest.x), call helper, `mov ebp,eax` (clamped X back), `!jmp_dword :45F36F`. No push_all (helper is __stdcall; eax/ecx/edx dead here). The whole clamp block including its register loads is skipped. |
| 0x45F614 | inside `CGameAIBase::MoveViewPoint()` (entry 0x45F5D0) — its own clamp 0x45F614..0x45F63D | src/CGameAIBase.cpp:1943 (marker at 1942; hook site itself unresolved in our tree) | — | `IEex_WriteAssembly(0x45F614, "!jmp_dword :45F63D")` — the `[0, nAreaX]` clamp is skipped outright; edi/ebx carry dest straight into the MoveView call and ecx reloads at 0x45F646. |

Cited but not hooked: `CGameSprite::Marshal` (save side, "@0x70F2xx") —
stuffs `GetViewPosition()` into the WORDs the Unmarshal patch ignores;
`CScreenWorld::AsynchronousUpdate` 0x68C3D0 (src/CScreenWorld.cpp:1663,
exact) — the natural DialogZoomTick site left to IEex_Gui_Patch.lua's
existing hook; g_pBaldurChitin 0x8CF6DC (no marker in our tree) — quoted
in the displaced bytes at the Render entry; resume/internal targets
0x477748, 0x477E5C, 0x476C77, 0x476DD6, 0x476D0C, 0x476D12, 0x4289C6,
0x644B05, 0x645105, 0x642F0D, 0x642F2E, 0x70CC4A, 0x70CC75, 0x484B90,
0x45F36F, 0x45F63D, 0x45F646.

## Data flow

All zoom STATE is DLL-side: `g_fCameraZoom` (live) and `g_fZoomTarget`
(wheel ticks), eased per tick by the DLL; the Lua side installs the
stubs once inside `IEex_DisableCodeProtection()` /
`IEex_EnableCodeProtection()` and owns nothing at runtime. The one
cross-helper cache: `DialogPanCompensateY` stores the current dialog
line's scroll target so `DialogZoomTick` can re-issue it after a zoom
change. `WheelShouldZoom` gates BOTH `OnMouseWheel` and the DLL
window_proc's tick recording, so the map screen's pump
(`MapWheelPump`) drains the same recorded ticks.

Structural reads the helpers perform (per the comments): the world
viewport rect `CInfinity::rViewPort` (+0x48, src/CInfinity.h:229;
`CGameArea`+0x4CC+0x48 = `[esi+0x514]`/`[esi+0x51C]` in the
load-recentre stub), map edges from `nAreaX/nAreaY` (+0x80/+0x84,
src/CInfinity.h:237-238), the speaker's world Y from
`CGameSprite::m_pos.y` (+0x0A, src/CGameObject.h:76), the map screen's
`field_72A` (CRect @ +0x72A, src/CScreenMap.h:142 — the unzoomed
viewport rect that stays vanilla on disk), `field_722` (+0x722, :140 —
the 1:1 grab offset) and `field_71E` (+0x71E, :137).

Ini gate read at patch time: `[Program Options] "3D Acceleration"`
(default 1).

Stock paths removed: GetWorldCoordinates, DrawEllipse3d, DrawRecticle3d,
SetViewPosition bodies (dead — full replacements); MoveView's clamp
block and MoveViewPoint's clamp (skipped); OnMouseMove's inline growth
arithmetic (both sites). Everything else trampolines or resumes through
the original code.

## IEex_Helper_* entry points

- `IEex_Helper_ZoomWorldBegin(CGameArea*)` — installed at 0x477740;
  scales MODELVIEW about the viewport centre.
- `IEex_Helper_ZoomWorldEnd()` — installed at 0x477E55; identity reset.
- `IEex_Helper_CInfinity_GetWorldCoordinatesOverride` — installed at
  0x5CDFC0; zoom-aware screen→world transform.
- `IEex_Helper_BandSelectGrow(CGameArea*, CPoint*)` — called from the
  two OnMouseMove caves.
- `IEex_Helper_CInfinity_SetViewPositionOverride` — installed at
  0x5D11F0; the overscroll-aware clamp everything else calls into.
- `IEex_Helper_CVidMode_DrawEllipse3dOverride` — installed at 0x7BC580.
- `IEex_Helper_CVidMode_DrawRecticle3dOverride` — installed at
  0x7BC7A0.
- `IEex_Helper_WheelShouldZoom()` — gates 0x4289C0 and the DLL
  window_proc's zoom-tick recording.
- `IEex_Helper_MapViewRectZoom(&a3Slot)` — called from the 0x644B00
  hook; draw-time rect inset.
- `IEex_Helper_MapWheelPump(CUIControlButtonMapAreaMap*)` — called from
  the 0x645100 hook.
- `IEex_Helper_MapCenterViewPort(CUIControlButtonMapAreaMap*,
  const CPoint&)` — installed at 0x6437A0.
- `IEex_Helper_MapGrabHitTest(x, y, &r)` — called from the 0x642EFA
  stub; visible-rect hit test.
- `IEex_Helper_DialogPanCompensateY(ptScrollY, pSprite, ptScrollX)` —
  called from the 0x484B88 stub; pivot-row retarget + line cache.
- `IEex_Helper_DialogZoomTick()` — called from the 0x483F00 hook.
- `IEex_Helper_ClampScriptViewDest(&destY, destX)` — called from the
  0x45F305 stub; returns clamped X in eax.

(`IEex_HookRestore`, `IEex_WriteAssembly`, `IEex_WriteAssemblyAuto`,
`IEex_FlattenTable`, `IEex_GetPrivateProfileInt`,
`IEex_DisableCodeProtection`/`IEex_EnableCodeProtection` are core-DLL
API, not `IEex_Helper_*`.)
