# IEex_UncapFPS.lua — tick-independent scrolling (uncapped-FPS companion)

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_UncapFPS.lua`. Companion to
the FPS uncap: once rendering is no longer tied to the engine's fixed logic
tick, everything the engine paced "per tick" has to be re-paced by real
time. This file re-implements the world-screen scroll paths that way.

## Intent

Decouple camera scrolling from the engine's fixed tick. Three pieces, all
written in Lua against engine state (no code bytes patched):

1. Keyboard / edge scroll: listeners take over the world screen's hardcoded
   scroll keys (`IEex_Scroll_RejectHardcodedWorldScreenKeybindingListener`
   makes the engine ignore them) and write the resolved scroll state
   (the engine's 1-8 direction encoding, diagonals merged) directly into
   `CGameArea::m_nKeyScrollState` whenever input changes.
2. Middle-drag pan: on MMB-down the anchor cursor position is captured into
   the `IEex_Scroll_MiddleMouseState` bridge; per frame the delta re-grabs
   the map point under the cursor. Two fork-comment subtleties: the cursor
   must be the *physical* (fill-remapped) cursor, not the capture-mapped
   `m_ptPointer` (whose HUD-logicalised coordinates spike when crossing the
   inflated bottom band at sub-native resolutions — "camera teleport at
   sub-4K"), and the delta must be divided by the GL camera zoom so the
   drag is a true grab at any zoom.
3. Per-frame scroll application (`IEex_Extern_CheckScroll`, thread Both):
   applies the pan delta, then applies edge/keyboard auto-scroll scaled by
   real elapsed microseconds (diff/25000 — normalised to 25 ms, i.e. 40 fps
   reference), instead of the engine's tick-count-paced
   `CInfinity::AdjustViewPosition`.

## Hooks table

None — the file patches no code. Every engine touch goes through Lua reads/
writes and two `IEex_Call`s made by the shared Key_State helpers it uses:

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| — | (no hook sites; engine state accessed from Lua only) | — | — | — |

Named engine references (all resolved in our tree):

- `pVisibleArea + 0x238` / `+ 0x23C` = `CGameArea::m_nScrollState` /
  `m_nKeyScrollState` — src/CGameArea.h:258-259. `m_nKeyScrollState` is
  *written* per input event (`IEex_WriteDword`); both are *read* per frame.
- `gameData + 0x43F2` / `+ 0x443E` = the scroll speeds, read per frame
  (keyboard one divided by 3). In our layout both land inside
  `CGameOptions m_cOptions` embedded in `CInfGame` at 0x43EA
  (src/CInfGame.h:443 — embedding offset marked `#guess`): 0x43F2 =
  m_cOptions+0x8 `m_nMouseScrollSpeed` (src/CGameOptions.h:15), 0x443E =
  m_cOptions+0x54 `m_nKeyboardScrollSpeed` (src/CGameOptions.h:34).
- `CInfinity + 0x164/0x168` = `m_ptCurrentPosExact` (src/CInfinity.h:274)
  and `+ 0x18E/0x192` = `m_ptScrollDest` (src/CInfinity.h:277) — reached
  through the shared Key_State helpers `IEex_AdjustViewPosition`
  (IEex_Key_State.lua:327) and `IEex_IsGameAutoScrolling`
  (IEex_Key_State.lua:366). `IEex_AdjustViewPosition` adds delta×10000 to
  the fixed-point position and calls `IEex_Call(0x5D11F0, ...)` —
  `CInfinity::SetViewPosition` (src/CInfinity.cpp:3058, exact marker).
- Not hooked, but the code path being replaced: the engine's own
  `CInfinity::AdjustViewPosition(BYTE nScrollState)`
  (src/CInfinity.cpp:3355, decl src/CInfinity.h:199), which paces scroll by
  `GetScrollSpeed()` and the tick count — unhooked, simply no longer fed.

## Data flow

State lives in two bridges and one engine field:

- `IEex_Scroll_MiddleMouseState` `{isDown, oldX, oldY}` — created here with
  `IEex_Helper_InitBridgeFromTable`. Writers: the key listeners flip
  `isDown` and capture the pan anchor on MMBEDGE transitions, always inside
  `IEex_Helper_SynchronizedBridgeOperation`; the per-frame reader updates
  `oldX/oldY` after applying each delta.
- `IEex_Helper_SupressScrollCheck` (spelling verbatim) `"value"` — read
  only, via `IEex_Helper_GetBridge`; the bridge is created DLL-side (this
  file never inits it). It is the DLL's escape hatch to suppress the whole
  scroll check (non-zero → return immediately).
- `CGameArea::m_nKeyScrollState` — written by
  `IEex_Scroll_CheckKeyboardInput` when the world screen is active and
  accepting input; if not, `IEex_Scroll_CheckScheduled` defers the write to
  the next input-state event (`IEex_Scroll_InputStateListener`).

Per-frame reader: `IEex_Extern_CheckScroll`, asserted on
`IEex_Thread.Both`. Honest gap: nothing in the staged Lua calls, registers,
or otherwise references it — the DLL must invoke it by name from its
per-frame path. Neither its call site nor the function appears in
`dll-delta.tsv`; B1 should pin down where the DLL calls
`IEex_Extern_CheckScroll` (and what creates the
`IEex_Helper_SupressScrollCheck` bridge).

Scroll-state resolution: the pressed-keys stack comes from the `IEex_Keys`
bridge maintained by Key_State (`IEex_GetPressedKeysStack`,
IEex_Key_State.lua:166); each key is classified through the hotkey
machinery in Gui_State (`IEex_Hotkeys_GetBoundCustomMapIndex`,
IEex_Gui_State.lua:6809, and the `IEex_Hotkeys_IsScroll*` predicates) into
the 1-8 direction grid with diagonal merging. The `IEex_Scroll_DefaultKeys`
table (arrow keys + numpad) doubles as the reject-list that keeps the
engine's own hardcoded scroll handling out of the way.

Coordinate space (the file's longest comments): the pan anchor and cursor
both come from `IEex_Scroll_GetPanCursor()` —
`IEex_Helper_GetCursorPhysicalXY` when the DLL export exists (guarded
`if ... then`, so an older DLL degrades to `IEex_GetCursorXY`). Rationale
per the comments: `m_ptPointer` is capture-mapped and logicalised over the
scaled world HUD, so frame deltas spike across the bottom band; raw
`ScreenToClient` desktop pixels mismatch the anchor's game-resolution space
("camera teleport on middle-click" at 1080p-on-4K with the fill blit). The
delta is then divided by `IEex_Helper_GetCameraZoom()` (guarded existence,
default 1; "no-op at zoom 1.0") so screen-pixel drag maps to world-pixel
motion.

Frame pacing: `IEex_Scroll_CalculateDeltaFactor` stores `"curTick"`
microseconds, diffs against `"lastTick"` (first frame → factor 1), divides
by 25000, and reassigns. Auto-scroll applies
`m_nScrollState × mouseSpeed` and `m_nKeyScrollState × keyboardSpeed/3`
per frame, skipped while middle-drag is down, while the world screen is not
accepting input, or while the engine is auto-scrolling
(`m_ptScrollDest` set).

Reload survival: `IEex_AbsoluteOnce("IEex_Scroll_InitListeners", ...)`
registers the four listeners once and chains
`IEex_AddReloadListener`/`IEex_ReaddReloadListener("IEex_Scroll_ReloadListener")`
to re-register after `IEex_Reload()`.

## IEex_Helper_* entry points

- `IEex_Helper_InitBridgeFromTable` — creates
  `IEex_Scroll_MiddleMouseState` `{isDown=false, oldX=0, oldY=0}`.
- `IEex_Helper_SynchronizedBridgeOperation` — mutual exclusion around every
  read-modify-write of the pan state.
- `IEex_Helper_SetBridge` / `IEex_Helper_SetBridgeNL` — bridge field
  writes; NL (no-lock) variants used inside the synchronised operation.
- `IEex_Helper_GetBridge` / `IEex_Helper_GetBridgeNL` — bridge field
  reads; also the read of the DLL-created
  `IEex_Helper_SupressScrollCheck`.`value` suppress flag.
- `IEex_Helper_StoreMicroseconds` / `IEex_Helper_ExistsMicroseconds` /
  `IEex_Helper_GetMicrosecondsDiff` / `IEex_Helper_AssignMicroseconds` —
  DLL-side high-resolution timer keyed by string (`"curTick"`/`"lastTick"`);
  source of the per-frame delta factor.
- `IEex_Helper_GetCursorPhysicalXY` — physical, fill-remapped cursor
  position; optional export (guarded call, fallback `IEex_GetCursorXY`).
- `IEex_Helper_GetCameraZoom` — current GL camera zoom factor; optional
  export (guarded, default 1). The same guard pattern recurs in
  IEex_Key_State.lua:1875/1898, so the export is shared machinery, not
  specific to this file.
