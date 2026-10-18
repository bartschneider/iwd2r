# IEex_Key_State.lua — input listener framework, fake input, zoom-aware camera externs

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Key_State.lua` (2254
lines). Not one engine byte is patched here: the DLL invokes seven
`IEex_Extern_*` functions by name — three on the input path, four for
zoom-aware camera/scroll — while the file's own bulk is a Lua-side input
framework (listener registries, polled key state, scripted fake input)
plus a dozen gameplay key listeners riding it. This is the file that
defines what "a key was pressed" means for the rest of the fork's Lua.

## Intent

1. **A listener framework over polled key state.** Four registries
   (key-pressed, key-released, input-state, reject-hardcoded-binding) are
   bridges of `{function, ...}` records holding function NAMES; the DLL
   calls `IEex_Extern_CChitin_ProcessEvents_CheckKeys` every tick on the
   Async thread, which polls VK 0x1–0xFE via
   `IEex_Helper_GetAsyncKeyStateClient` under
   `IEex_Helper_RunWithRawInputLock`, maintains the `IEex_Keys` bridge
   (`isDown` + `pressedStack`), derives pressed/released events (with a
   `missedPress` correction when a tick is slow enough to see down→up in
   one poll), and dispatches listeners by name.
2. **Fake input.** `IEex_Helper_FakeKeyEvent` injects synthetic
   press/release into the same state (so engine code and listeners cannot
   tell), a fake cursor mode feeds the engine a forged
   `IEex_FakeCursorPosMem` block (allocated in IEex_Core_State.lua:25-30),
   and scripted "input routines" — tables of
   FUNCTION/UP/DOWN/PRESS/SET_MOUSE_POS/CLICK_CONTROL/WAIT events with
   CONTINUE/WAIT/END-style return codes — are interpreted inside
   `IEex_Extern_BeforeCheckKeys` on the Async thread, start/stop managed
   cross-thread through a bridge and optionally bound to hotkeys.
3. **Zoom-aware camera.** Four externs re-implement engine
   viewport/scroll arithmetic with the camera zoom folded in:
   `IEex_Extern_EnforceViewportBottomBound` clamps the view Y above the
   HUD strip, `IEex_Extern_AdjustAutoScrollY` divides the HUD offset by
   the zoom, `IEex_Extern_AutoScroll` recomputes the overscroll "slack"
   bounds from the visible content area and writes the result back into
   `m_ptScrollDest` (plus a stuck-scroll watchdog), and
   `IEex_Extern_AllowMouseScrollDown` tests against the zoom-scaled
   effective view bottom. Zoom comes from the DLL helper
   `IEex_Helper_GetCameraZoom`, guarded (`and ... or 1`) so the software
   renderer (no helper) stays stock — the same guard IEex_UncapFPS.lua:219
   uses.
4. **Gameplay key listeners** (the point of the framework): extra cheat
   toggles (opcode/action printing), Arcane-Sight and Deathwatch HUD
   tokens, cutscene skip, uncapped ability scores, an item-use fix,
   reform-party/reroll buttons, an EXBUFFRC buff-recording/autocast
   system, and two large character-screen overhauls (chargen and level-up
   extra feat/skill points, replacement wizard-spell-learning panels).

## Hooks table

None — the file patches no code bytes (no `IEex_WriteAssembly`,
`IEex_HookRestore`, or `IEex_AttemptHook` anywhere). Every engine touch
is a Lua read/write of engine memory, an `IEex_Call`, or an extern the
DLL invokes:

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| — | (no hook sites; DLL calls the `IEex_Extern_*` functions by name) | — | — | — |

Named engine references (resolved in our tree unless stated):

- `IEex_Call(0x5D11F0, {0, nNewY, nNewX}, pInfinity, 0x0)` →
  `CInfinity::SetViewPosition(int, int, BOOLEAN)` — src/CInfinity.cpp:3058
  (exact marker; decl follows). Fork comment names it
  `CInfinity_SetViewPosition`. Called from `IEex_CheckViewPosition`
  (IEex_Key_State.lua:319) and `IEex_AdjustViewPosition` (:327); note the
  argument order `{0, y, x}` and `bSetExactScale = 0`.
- `IEex_Call(0x5D1380, {speed, targetViewY, targetViewX}, CInfinity,
  0x0)` → `CInfinity::Scroll(CPoint, SHORT)` — src/CInfinity.cpp:3115
  (exact marker; decl follows). Called from `IEex_Extern_AutoScroll` at
  IEex_Key_State.lua:1936.
- 0x7FBE4E — fork comment `CPtrList_AddTail()`, called at
  IEex_Key_State.lua:898 and :975 as
  `IEex_Call(0x7FBE4E, {newWizardSpellsPanel}, screenCharacter + 0x62A,
  0x0)` — appends a replacement spell-learning panel to the
  character screen's popup list. Unresolved in our tree at that exact
  address (nearest preceding marker is src/music/audio.cpp:477;
  no `CPtrList` markers exist). The `this` argument is
  `CScreenCharacter::m_lPopupStack` (+0x62A, src/CScreenCharacter.h:188),
  a `CTypedPtrList<CPtrList, CUIPanel*>`, so the callee is AddTail on
  that list.
- 0x8CF6DC `g_pBaldurChitin` — no marker in our tree; read at
  IEex_Key_State.lua:643 (chargen) and :863 (level-up). `+0x1C64` →
  `CBaldurChitin::m_pEngineCreateChar` (src/CBaldurChitin.h:167,
  `CScreenCreateChar*`), `+0x1C60` →
  `CBaldurChitin::m_pEngineCharacter` (src/CBaldurChitin.h:166,
  `CScreenCharacter*`).
- 0x80140F is NOT an address here — it is the head of the state-mask
  constant `0x80140FED` passed to `IEex_GetActorState` at :1524/:1541
  (ArcaneSightListener). Listed only to keep it out of B1's address
  census.
- `CInfinity` offsets: `nNewX`/`nNewY` +0x40/+0x44
  (src/CInfinity.h:227-228), `nAreaX`/`nAreaY` +0x80/+0x84
  (src/CInfinity.h:237-238; the fork names these nAreaWidth/nAreaHeight
  and uses +0x84 as the viewport bottom in
  EnforceViewportBottomBound/AllowMouseScrollDown),
  `m_ptCurrentPosExact` +0x164 (src/CInfinity.h:274; +0x164 x / +0x168 y,
  stored ×10000), `m_ptScrollDest` +0x18E (src/CInfinity.h:277; +0x18E x,
  +0x192 y, −1 = not scrolling — `IEex_IsGameAutoScrolling` :366).
- `CGameArea` +0x550 — the `AllowMouseScrollDown` threshold
  (IEex_Key_State.lua:1970) is the embedded infinity's
  `CInfinity::nAreaY`: `m_cInfinity` at +0x4CC (src/CGameArea.h:290) +
  0x84 (src/CInfinity.h:238).
- `CInfGame` +0x43E2 — `IsWorldScreenAcceptingInput`'s "input mode" byte
  (IEex_Key_State.lua:342, bits 0/1 tested) is
  `m_gameSave.m_mode`: `m_gameSave` at +0x422C (src/CInfGame.h:442) +
  `m_mode` +0x1B6 (src/CGameSave.h:23, DWORD — the fork reads it as
  bytes; fork comment says m_inputMode). +0x452A — the hotkey byte
  compared against the incoming key (:342; fork comment
  `aHotkeyToVirtualKey[0x16] (Map)`) is `m_pKeymap[0x16]`: `m_pKeymap`
  at +0x4514 (src/CInfGame.h:447, 372 entries).
- `CUIManager` offsets (read in IsWorldScreenAcceptingInput): +0x0
  `m_bHidden` (src/CUIManager.h:45), +0x14 `m_pFocusedControl`
  (src/CUIManager.h:49, `#guess`; fork comment m_controlCaptured), +0x2C
  `m_nCaptureType` (src/CUIManager.h:53; fork comment m_inputCaptured,
  tested `~= 2`). World engine +0x156 — fork name m_bCheatKeysEnabled —
  is bare `field_156` in our tree (src/CScreenWorld.h:152); +0x136 is
  `m_bCtrlKeyDown` (src/CScreenWorld.h:144; fork comment m_bCtrlDown).
- `CScreenCreateChar` (chargen): `m_nGameSprite` +0x4E2
  (src/CScreenCreateChar.h:187), `m_nExtraAbilityPoints` +0x4EA
  (:189), `m_nExtraSkillPoints` +0x4F2 (:191), `m_lPopupStack`
  +0x536 (:216) with +0x53E — the `panelData` source read at :700,
  `IEex_ReadDword(IEex_ReadDword(chargenData + 0x53E) + 0x8)` — falling
  in the undeclared stretch right after the list.
- `CScreenCharacter` (level-up): actorID at +0x136 (undeclared stretch
  in our tree, between the 0x3C and 0x1B0 offset comments), the
  panel-ID override target +0x304 =
  `m_pVirtualKeys[1].m_keyCode` — `m_pVirtualKeys[90]` at +0x300
  (src/CScreenCharacter.h:186), `CKeyInfo::m_keyCode` at +0x4
  (src/CKeyInfo.h) — the fork rebinds virtual key 1's code to 27 or 65
  to force the right panel open, and `m_lPopupStack` +0x62A
  (src/CScreenCharacter.h:188), the AddTail target.
- Creature share (`CGameSprite`) offsets used by the listeners:
  +0x920 `m_derivedStats` (src/CGameSprite.h:789) and +0x1778
  `m_tempStats` (src/CGameSprite.h:790) — the ability-score words at
  +0x974..+0x97E and +0x17CC..+0x17D6 are
  `CDerivedStats::m_nSTR/m_nINT/m_nWIS/m_nDEX/m_nCON/m_nCHR` at +0x54..+0x5E
  (src/CDerivedStats.h:450-460), i.e. share+0x974 = derived `m_nSTR`,
  share+0x17CC = temp `m_nSTR`; +0x5A4 `m_baseStats`
  (src/CGameSprite.h:788, `CCreatureFileHeader` src/FileFormat.h:436)
  with XP +0x5B4 = `m_xp` +0x10 (src/FileFormat.h:440), current HP
  +0x5C0 = `m_hitPoints` +0x1C (src/FileFormat.h:443, SHORT — read via
  `IEex_ReadSignedWord`), animation +0x5C4 = `m_animationType` +0x20
  (src/FileFormat.h:444), total level +0x626 = `m_characterLevel` +0x82
  (src/FileFormat.h:490), extra-flags +0x740 (bits 0x40 feat-applied,
  0x1000000 "Opcode printing", 0x2000000 "Action printing"), derived
  ability words +0x974 (see above); +0x4AD8 `m_equipment`
  (src/CGameSprite.h:816, `CGameSpriteEquipment`) — item slots read as
  `[share + 0x4AD8 + itemSlot*0x4]` in ItemUseFixListener with the
  resref at slotData+0xC, and actionData at share+0x476 (undeclared
  stretch in our tree). Race +0x26 and subrace +0x3E3D (chargen, inside
  the m_baseStats region) are fork-cited offsets with no matching
  declaration in our headers.
- `IEex_Extern_MoveViewUntilDone_Stuck` — read/written at
  IEex_Key_State.lua:1943 and :1962 (`IEex_WriteDword(..., 1)` when the
  AutoScroll target cannot be reached). Defined in NO staged Lua file:
  it is a DLL-published address (B1 must define it; it did not appear in
  our dll-delta.tsv pass either).

## Data flow

Everything lives in bridges; the only file-scope tables are the `ex_*`
config/strref tables. Per bridge:

- `IEex_Keys` — keyed by virtual key 0x1–0xFE, each
  `{isDown = false, pressedStack = {}}`, created once
  (IEex_Key_State.lua:154-160). Writer: the
  `IEex_Extern_CChitin_ProcessEvents_CheckKeys` poll (isDown,
  pressedStack push/pop, missedPress correction). Readers:
  `IEex_IsKeyDown` (:162), `IEex_GetPressedKeysStack(NL)` (:166/:170),
  the fake-input machinery, and every listener that asks "is X held".
- `IEex_KeyPressedListeners` / `IEex_KeyReleasedListeners` /
  `IEex_InputStateListeners` /
  `IEex_RejectHardcodedWorldScreenKeybindingListeners` — arrays of
  function-NAME records appended under
  `IEex_Helper_SynchronizedBridgeOperation` with (unprefixed)
  `IEex_AppendBridgeNL` (:113-143), cleared and rebuilt by
  `Key_ReloadListener` (:137) on Lua reload, registered from
  `Key_RegisterInputListeners` (:1817) inside
  `AbsoluteOnce("IEex_Key_InitInputListeners")` (:1838). Dispatched
  per-tick by the ProcessEvents extern: pressed/released listeners get
  the key, input-state listeners get called per key-state change.
- `IEex_FakeInputRoutine` — `{active, routine, stopKey}`; the
  cross-thread mailbox between `IEex_StartFakeInputRoutine` (:251) /
  `IEex_StopFakeInputRoutine` (:265) (any thread) and
  `IEex_Extern_BeforeCheckKeys` (Async), which owns the routine's event
  cursor. `IEex_FakeInputRoutineStartStopKeys` (:290-316) maps hotkeys
  to registered routines (toggled by
  `IEex_ToggleFakeInputRoutineStartOrStop` :280; entries erased with
  `IEex_Helper_EraseBridgeKeyNL`).
- `IEex_FakeCursorPosMem` — NOT created here: lazy-allocated in
  IEex_Core_State.lua:25-30 (`IEex_Malloc(0xC)`, zero-initialised,
  cached under this bridge's "value" field) and guarded by
  `IEex_Helper_LockGlobal`/`IEex_Helper_UnlockGlobal("IEex_FakeCursorPosMem")`
  here (:197-220). Layout: +0x0 active flag, +0x4 x, +0x8 y; the DLL
  reads it in place of the real cursor position while active.
- `IEex_Feats` — read-only here (chargen/level-up listeners enumerate
  `FE_<featID>_<featLevel>` resrefs up to `NEW_FEATS_MAXID`); defined in
  the feat data files.

Per-frame reader: the DLL's key-poll path. Thread contracts as asserted
in the file: Async — `IEex_Extern_AllowMouseScrollDown` (:1970),
`IEex_Extern_BeforeCheckKeys` (:2106),
`IEex_Extern_CChitin_ProcessEvents_CheckKeys` (:2153),
`IEex_Extern_OnBeforeWorldScreenCheckingHardcodedKeybinding` (:2241;
contract comment: return false = normal handling, true = prevent the
hardcoded world-screen keybinding). Both —
`IEex_Extern_EnforceViewportBottomBound` (:1855),
`IEex_Extern_AdjustAutoScrollY` (:1867),
`IEex_Extern_AutoScroll` (:1880). Microsecond timing for the
missedPress/window logic comes from `IEex_Helper_StoreMicroseconds` /
`IEex_Helper_GetMicrosecondsDiff` (:2093-2098).

The zoom externs all follow the same guard:
`zoom = IEex_Helper_GetCameraZoom and IEex_Helper_GetCameraZoom() or 1`,
clamped to ≥ 1 — when the helper is absent (software renderer) every
computation degenerates to the stock formulas. AutoScroll clamps its
target to the visible-content bounds derived from
`nNewX/nNewY`, `nAreaX/nAreaY` and the zoom, writes the clamped target
back into `m_ptScrollDest`, calls `CInfinity::Scroll`, and if the view
cannot make progress flags `IEex_Extern_MoveViewUntilDone_Stuck` so the
engine's own wait loop gives up.

Gameplay listeners and their state: ExtraCheatKeysListener (:581)
flips share+0x740 bits; ArcaneSightListener (:1524) /
DeathwatchListener (:1487) walk opcode-288 effects
(saving-throw bits 0x10000 / 0x20000 at eData+0x40) and set
`EXHPSTATE1-5` tokens from HP (`m_hitPoints` vs max); CutsceneSkipListener
(:1668); AbilityScoreCapListener (:1674) — when
`ex_full_ability_score_cap > 40`, stats above the cap are written into
BOTH `m_derivedStats` and `m_tempStats` (statID 37→36 remap);
ItemUseFixListener (:1696) rewrites action 34's slot resref;
AddButtonListener (:1720) adds the reform-party / reroll buttons;
BuffRecording (:383-580, opcode-0 EXBUFFRC effects, `EX_Recording_Buffs`
global, casts via `IEex_Eval('SpellRES(...)')` + `SmallWait(1)`) and
Chargen_Reroll registrations are present but COMMENTED OUT in
`Key_RegisterInputListeners`; the chargen listener (:642-860) and
level-up listener (:862-1230) apply `ex_*` extra points, race/subrace
bonuses and replacement spell-learn panels.

Honest gaps (staged-drop evidence):

- Staging bug: the InputStateListeners dispatch inside
  `IEex_Extern_CChitin_ProcessEvents_CheckKeys` calls
  `_G[funcName](key)` where `key` is not in scope at that point —
  harmless for the zero-argument listeners actually registered, but the
  name would resolve to a global nil.
- `ex_replacement_animations` is only read (also by
  IEex_Creature_State.lua:651-652); the chargen/level-up `ex_tra_*`
  strrefs and `ex_item_use_resource` / `ex_full_ability_score_cap` /
  `ex_base_ability_score_cre_offset` are read here but never assigned in
  the staged drop (their setters live in data files not staged).
- `IEex_Vanilla` gates the whole registration (:1817: `if IEex_Vanilla
  then return end`) — set by the DLL, not staged Lua.

## IEex_Helper_* entry points

- `IEex_Helper_SynchronizedBridgeOperation(bridge, fn)` — wraps every
  listener registration and the start/stop-key toggles.
- `IEex_Helper_AppendBridgeNL` — appends listener records. Invoked
  UNPREFIXED as `IEex_AppendBridgeNL` here (:113-143) while other files
  spell it `IEex_Helper_AppendBridgeNL` — the bridge primitives are
  exported under both spellings; B1's export table must cover both.
- `IEex_Helper_ClearBridge` — wipes the four listener bridges on reload
  (:137).
- `IEex_Helper_InitBridgeFromTable` — seeds `IEex_Keys` (:154) and the
  fake-input bridges.
- `IEex_Helper_GetBridge` / `IEex_Helper_GetBridgeNL` — listener-bridge
  iteration and per-key reads.
- `IEex_Helper_SetBridge` / `IEex_Helper_SetBridgeNL` —
  fake-routine active flag, start/stop-key registry entries.
- `IEex_Helper_ReadDataFromBridge` / `IEex_Helper_ReadDataFromBridgeNL`
  — whole-record reads (routine tables, key records).
- `IEex_Helper_GetBridgeNumIntsNL` — listener counts for the dispatch
  loops.
- `IEex_Helper_EraseBridgeKeyNL` — start/stop-key removal (:290-316).
- `IEex_Helper_IterateBridge` — listener dispatch (B1 cross-check:
  also used by other state files).
- `IEex_Helper_FakeKeyEvent` — the DLL-side synthetic key injection
  (:180/:188 wrappers).
- `IEex_Helper_LockGlobal` / `IEex_Helper_UnlockGlobal` — the fake-cursor
  allocation/teardown critical section (:197-220).
- `IEex_Helper_RunWithRawInputLock` — wraps the whole per-tick key poll
  (:2153).
- `IEex_Helper_GetAsyncKeyStateClient(vk)` — the polled key state;
  `IEex_Helper_GetAsyncKeyStateWrapper` is its nClient-0 form (15
  clients exist, client 0 = engine, 1 = this Lua poll).
- `IEex_Helper_GetPressedKeysStackNL` — native accessor paired with the
  Lua `IEex_GetPressedKeysStackNL` (:170).
- `IEex_Helper_GetCameraZoom()` — guarded call (:1875/:1898; same
  pattern IEex_UncapFPS.lua:219). DLL helper, absent from our
  dll-delta.tsv pass — B1 must confirm the export.
- `IEex_Helper_StoreMicroseconds` / `IEex_Helper_GetMicrosecondsDiff` —
  poll-timing bookkeeping (:2093-2098).

Non-helper API consumed (core-DLL, not `IEex_Helper_*`):
`IEex_AssertThread`, `IEex_AbsoluteOnce`, `IEex_AddReloadListener` /
`IEex_ReaddReloadListener`, `IEex_Call`, `IEex_ReadByte` /
`IEex_ReadWord` / `IEex_ReadDword` / `IEex_ReadSignedWord` /
`IEex_WriteByte` / `IEex_WriteWord` / `IEex_WriteDword`,
`IEex_GetActorIDShare`, `IEex_IsValidSprite`, `IEex_GetActorStat`,
`IEex_GetActorFullStat`, `IEex_GetActorState`,
`IEex_GetActorSpellState`, `IEex_IterateActorEffects`, `IEex_Eval`,
`IEex_SetToken`, `IEex_GetGlobal` / `IEex_SetGlobal`,
`IEex_CreateErrorDialog`, plus the `IEex_Extern_*` functions the DLL
invokes by name: `IEex_Extern_EnforceViewportBottomBound`,
`IEex_Extern_AdjustAutoScrollY`, `IEex_Extern_AutoScroll`,
`IEex_Extern_AllowMouseScrollDown`, `IEex_Extern_BeforeCheckKeys`,
`IEex_Extern_CChitin_ProcessEvents_CheckKeys`,
`IEex_Extern_OnBeforeWorldScreenCheckingHardcodedKeybinding` — and the
DLL-published `IEex_Extern_MoveViewUntilDone_Stuck` address.
