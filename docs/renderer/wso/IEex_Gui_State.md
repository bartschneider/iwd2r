# IEex_Gui_State.lua — Lua UI framework: descriptor-driven options registry, HUD-layer repaint, Modern HUD refonte, world-screen popups

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Gui_State.lua` (8473 lines).
Upstream baseline: `IEex_Gui_State.upstream.lua` (5751 lines, IEex upstream commit 60de54611b249d3bcca3f6522ef9db5e7ec27339).
Fork commit 138495fde69b6092b7318fad462d571864e9d9b8, ref `opengl-hd-beta7`.

## Intent

1. Ship the entire Lua side of the UI layer as one State file: it runs at State load (INI normalization/seeding), at every CHU init (panel surgery), every frame (BeforeWorldRender / AsynchronousUpdate externs), and on every UI input (button/label/slider externs).
2. INI head (fork lines 44–175): define `IEex_GetPrivateProfileInt` / `IEex_WritePrivateProfileString` / `IEex_WritePrivateProfileInt` wrappers over engine function-pointer globals (0x847310 / 0x847308 / 0x84730C) at the very top of the file, so the load-time block can normalize `Icewind2.ini` before any patch gate reads it: force `[Program Options] 3D Acceleration=1` every launch (opt-out `[IEex Options] Software Renderer=1` forces 0, which coherently self-disables every GL gate), one-time `Gamma Correction` normalization behind a `Gamma Normalized` marker key, then seed the 42-entry `ex_ini_option_defaults` table (read-with-default, write back the read value — never clobber a player's setting).
3. Gates: `IEEX_HD_UI` (HD-UI master, flipped false→true by the WeiDU 2x UI component, ships OFF = stock 1x UI), `IEEX_FLOAT_TEXT_HD` (GLOBAL: supersampled floating world text, tied to the 2x-UI "non-pixelated fonts" sub-option that ships the native-2x IEEXFLT atlas), `IEEX_INI_ABSENT = -2147483647` (sentinel no real key can hold, because GetPrivateProfileInt can only return a default), `IEEX_GL_ACTIVE`, `IEEX_PORTRAIT_FRAMES_AVAILABLE` (load-time snapshot, fork line 1630).
4. Replace upstream's hardcoded per-option options-menu code with a descriptor registry: `ieexOptionRows` (fork 305–620, 38 rows keyed by the LABEL control id; each row = kind toggle/slider, ini key, bridge name, default, value list, display callback, onEnable), group/order tables (`IEEX_OPTION_ROW_GROUPS` 221, `IEEX_OPTION_ROW_ORDER` 235, GL-only 243 / refonte-only 250 visibility filters), and balanced pagination (`IEEX_OPTION_PAGE` 740, `optionLayout` 754, `PageCount`/`RowPage`/`RowY`, page arrows only when `PageCount() > 1`). The button/label/slider externs become generic row handlers instead of per-option code.
5. HUD layer: when the GL renderer is active the DLL composites the 2D UI from a persistent FBO; Lua decides when panels are dirty and repaints them into it on the `IEex_Extern_BeforeWorldRender` event. Panel 1 (bottom HUD) repaints on a game-tick edge (CInfGame +0x1B7C/+0x1B78 pair) OR a `PortraitContentHash` change (measured in fork comments: ~5 ms repaint, 93% of frames are portraits-only; ~0.62 ms/frame steady state); panel 100 (quickloot) on the ActionIndicators `updateCounter`; dialog-family panels {6,7,8,22} signal frame-bypass via `IEex_Helper_HudLayerSkipFrame`. The DLL owns panel-1 freshness itself via `HudLayerSparseClear` / `HudLayerContentPanel1CleanSkip` (blanket invalidate measured ~1.5 ms/frame at 4K; paused ~2.3 ms/frame, 240→210 fps on a 238-fps-cap budget).
6. Modern HUD ("refonte", `InstallPortraitGrid` fork 4600): the six portraits stay the stock panel-1 controls 0–5, moved IN PLACE — the engine hardcodes panel 1 for portrait hover→spell-target (CScreenWorld::AsynchronousUpdate), so reparenting them would break that path. A virtual canvas width (`IEex_Refonte_CanvasWidth`, authored "Modern HUD Ref Width" 3840 scaled by "Modern HUD Size") plus the stage-2 GL transform (which scales about the bottom-LEFT corner, per `Export_UIScaleRenderBegin` cx=0) let dialog panels 6/7/8/9/17 re-centre on `centerW` under refonte.
7. Action indicators (`InstallActionIndicators` fork 5075, `AI_Update` 1941): per-portrait overlays on panel 1 with a per-portrait dirty-check signature (settle ticks, `LastGameTime`/`LastIndicatorsCounter`); HD-UI builds add a `div` pre-division and size the panel rect to the controls' real span (indMinX/indMaxX) to avoid a black band over quickloot.
8. Quickloot (`InstallQuickloot` fork 5159, now takes `chuResref`): refonte branch installs chu `B3QKLOOM` 505x51 (gBtn 38, pitch 41, inset 8, slotY 12) vs stock `B3KLOOT` measured from the live panel-1 controls 7–16 plus arrows 6/17 with a content-clip +4 pad; borders toggle via the IEex_Gui_Patch CDimm::GetResObject redirect (hardcoded B3KLOOM + borders ON = solid black block under REPLACE). Bridge-synced scrolling (`SynchronizedBridgeOperation`, `GetBridgeNumIntsNL`).
9. Item-info popup: new world-screen panel 51, built entirely in Lua (`GIITMH08` 513x481): ctrl 0 STONEBIG (strref 15120), ctrl 1 yellow 0xC8C8, ctrl 2 override "InventoryHistoryIcon" 64x64 ("its Render adds the item BAM's centre-point offset"), ctrl 3 TEXT_AREA 445x286, ctrl 4 SCROLL_BAR GBTNSCRL, ctrl 5 Done GBTNMED strref 11973; no def x/y — centred per-frame via SetPanelXY. Launched from container-slot/record clicks (`IEex_LaunchWorldScreenItemInfo` 2305); `IEex_StopWorldScreenSpellInfo` extended to close both panels 50 and 51.
10. Hotkeys (6539–6878) + keybinding-screen integration (`RegisterKeysScreenHardcodedMapDefaults` 6883 / `RegisterKeysScreenCustomBindings` 6884), quickloot toggle button at Refonte_CmdSlots[10], log-resize controls 16 (IEEXNULB invisible strip) + 17 (IEEXRSZB chevron) — with an ORDER MATTERS comment: CUIPanel walks controls TAIL-first for input (0x4D2D80) and HEAD-first for render (0x4D3100).
11. Options persistence: `IEex_LoadOptions` 6287 / `IEex_WriteOptions` 6309 / `IEex_InitOptionButtons` 6328 become loops over `IEex_OptionRows()`; GL forces `transparentFogOfWar` false via `IEex_FogTypePtr` (so the FoW assembly hooks stay inert) and skips the ini write; `IEex_InjectOptionIniComments` 6368 is insert-only raw-file comment injection (~50-key table, sentinel line, CRLF detect, Wine cache reload via a post-write `IEex_GetPrivateProfileInt`), invoked `pcall(IEex_InjectOptionIniComments)` at 6491. Options Done/Cancel additionally call `IEex_Helper_SetMarkerStyle()` and `IEex_Helper_ReloadFloatTextScale()` (guarded) so helper first-use caches re-read the rewritten ini.
12. Record-screen extern `IEex_Extern_OnUpdateRecordDescription` (7158 → end): feat-ID nil guards (7662–7671), Improved Whirlwind second-rank threshold stat-103 raised 14→15 (7891) — the upstream unconditional totalAttacks recompute + imptwf>2 third-rank grant (upstream 5192–5195) is DELETED, the recompute survives only inside the `GetActorSpellState(targetID, 138)` branch; damage-line rewriting (opcode 288/param2 195 critical multiplier, immunity strings ex_tra_55601/55602, `IEex_CString_Set(lineEntry+0x4, line)`); bard/sorcerer spontaneous-cast spell-slot clamps (MXSPLBRD creatureData +0x628/+0x4288/+0x428C at 8439+, MXSPLSOR +0x630/+0x4788/+0x478C at 8456+; level blocks stride 0x1C, entries stride 0x10, clamp `ReadDword(offset+0x8) > baseNumSlots → WriteDword(offset+0x8, base)`).

## Hooks table

This State file installs no engine hooks of its own; it is the Lua side that the DLL/assembly hooks call into and that calls engine addresses directly. Adapted below: (a) upstream Lua functions modified by the fork, (b) engine addresses invoked from Lua, (c) engine globals/struct offsets read or written from Lua.

### (a) Fork-modified upstream functions

| upstream function | fork line | what changes |
|---|---|---|
| `IEex_WritePrivateProfileInt` | 44 | Moved to file top, reimplemented as forward to `IEex_WritePrivateProfileString(tostring(nInt))`; needed before anything else runs (INI head). |
| `IEex_WritePrivateProfileString` | 48 | Moved to file top; wrapper over `IEex_Call(IEex_ReadDword(0x847308), …)` with cdecl-reversed stack args via `IEex_RunWithStackManager`. |
| `IEex_GetSpellIconResref` | 982 | Adds `SpellIconResrefCache` (981): resref → SPL icon resref (wrapper:getData()+0x3A), fixing repeated header reads. |
| `IEex_ActionIndicators_GetPanel` | 1711 | Null-world guard: on the main menu `GetEngineWorld()` is 0; return early instead of indexing. |
| `IEex_ActionIndicators_Update` | 1941 | Feature gate + per-portrait signature dirty check + settleTicks + `updateCounter` bump (HUD repaint trigger). |
| `IEex_StopWorldScreenSpellInfo` | 2335 | Also closes item-info panel 51, not just spell-info panel 50. |
| `IEex_Extern_BeforeWorldRender` | 2586 | HUD-layer gates and repaint scheduling; refonte anchoring; world-popup centring with `UIScaleMapWorldCursor` inverse mapping (2623); guarded `WorldRejectMapCursor` calls (3049–3050/3065 live in `MouseInAreaViewport`'s helpers). |
| `IEex_Extern_InitResolution` | 2887 | `AskResolution` (2895) then `IEex_Helper_SetHDUI(0)` (2908): resolution changes drop back to the 1x UI tier. |
| `IEex_Extern_InitHighResolutionPaddingPanels` | 2974 | Native-2x tier handling; `Memset(IEex_ReadDword(0x8CF6DC)+0x49B4, 0, 4*0x1C)` (3008) clears the padding-panel array the upstream path would double-initialize; comment at 2985 explains the 0x8CF6DC not-yet-assigned ordering trap. |
| `IEex_Extern_MouseInAreaViewport` | 3033 | `WorldRejectMapCursor` mapping so HUD-owned screen regions stop eating world cursor moves. |
| `IEex_Extern_OnActionbarUnhandledRButtonClick` | 3071 | Refonte-aware target cycle (SoundRange mention is comment-only, 2932). |
| `IEex_Extern_UI_ButtonRender` | 3697 | `ActionIndicators_RenderMap`/`ButtonRender` lookup (`DecrementButtonDoRender` 3714, `RenderButtonIcon` 3727); HD 2x icon scale. |
| `IEex_Extern_UI_ButtonLClick` | 3744 | Generic options-row tail (`if resref == "GUIOPT" and panelID == 14 then labelId = IEex_OptionRowOfControl(controlID) …`) replacing upstream's hardcoded per-option table; click-map additions: [31]/[32] = `StepOptionPage(∓1)`, [16]/[17] = `Refonte_CycleLogHeight`, [51][5] = `StopWorldScreenSpellInfo`, [15] = openIEexOptions(2)/(13); CPtrList AddTail/RemoveTail (0x7FBE4E/0x7FB343) on screenOptions+0x434. |
| `IEex_Extern_UI_LabelLDown` | 4206 | Reduced to guard + `SetOptionDescription` (4200): description text now comes from the row descriptor. |
| `IEex_InstallActionIndicators` | 5075 | HD-UI `div` pre-division; panel rect = controls' real span (indMinX/indMaxX) to avoid a black band over quickloot. |
| `IEex_InstallQuickloot` | 5159 | New `chuResref` parameter; refonte vs stock geometry branches; HD div. |
| `IEex_InstallIEexOptions` | 5304 | Descriptor-driven build: labels 0x51, sliders IEEXSLDR/GUISLDR with `knobJumpWidth = IEex_OptionSliderJumpWidth(stops)`, value readouts 0x45, toggles GBTNOPT3; custom `IEex_UI_Slider` control via `IEex_DefineCustomControl`; main-menu panel-13 duplicate button id 15 (Return id 11 shifted to 612,338); page arrows only when `PageCount() > 1`. |
| `IEex_OnCHUInitialized` | 5570 | Refonte-aware centring of panels 6/7/8/9/17 on `centerW = IEex_Refonte_CanvasWidth(resW, mult)` (gate: not Vanilla + PortraitGridEnabled + GUIW10 + `IEex_ReadDword(0x8CF6D8)+0x91C`); console panel 6 width 800*mult fixes the left-half paste bug; panel 21 never re-centred; item-info panel 51 construction; log-resize strip; quickloot button placement; keybinding dividers built at 0,0 then `IEex_SetControlArea` writes m_ptOrigin directly (HD-UI ctor-doubling / "cut text" fix). |
| `IEex_LoadOptions` | 6287 | Loop over `IEex_OptionRows()`; GL forces `transparentFogOfWar=false` via `IEex_FogTypePtr` (3897/6282 — `IEex_Helper_GetBridgePtr`) so FoW asm hooks stay inert. |
| `IEex_WriteOptions` | 6309 | Loop over `IEex_OptionRows()`; ini write skipped in GL. |
| `IEex_InitOptionButtons` | 6328 | Loop-driven widget painting; `pcall(IEex_InjectOptionIniComments)` at 6491. |
| `IEex_Extern_OnUpdateRecordDescription` | 7158 | Feat-ID nil guards; ITWF stat-103 thresholds (<9 / >=16 kept at 7887, second-rank 14→15 at 7891); upstream's unconditional totalAttacks recompute + imptwf>2 third-rank grant deleted; critical-multiplier/immunity damage-line rewriting; MXSPLBRD/MXSPLSOR spell-slot clamps. |

### (b) Engine addresses invoked from Lua

| address | engine function (our name for the hook target) | iwd2r file:line | invoked from | what changes |
|---|---|---|---|---|
| 0x4D3810 | `CUIPanel::InvalidateRect` | src/CUIPanel.cpp:463 | `IEex_PanelInvalidate` wrapper | Dirty-mark panels for the HUD/popup paths. |
| 0x4D3980 | `CUIPanel::SetActive` | src/CUIPanel.cpp:497 | `IEex_SetPanelActive` | Activate/deactivate panels from Lua (popup open/close, refonte moves). |
| 0x4D29D0 | `CUIPanel::SetEnabled` | src/CUIPanel.cpp:69 | `IEex_SetPanelEnabled` | Enable/disable panels (options rows, popup lifecycle). |
| 0x4D2D80 | `CUIPanel::OnLButtonDown` | src/CUIPanel.cpp:222 | comment reference only | Fork comment cites it for the tail-first input walk that dictates log-resize control order. |
| 0x4D3100 | `CUIPanel::Render` | src/CUIPanel.cpp:313 | comment reference only | Fork comment cites it for the head-first render walk (same ORDER MATTERS comment). |
| 0x4802B0 | `CGameContainer::GetItem(SHORT nSlotNum)` | src/CGameContainer.cpp:659 | `IEex_GetContainerIDItem` (1470) | Container-slot handlers resolve the clicked item. |
| 0x4E9B10 | `CItem::GetGenericName` | src/CItem.cpp:440 | `IEex_LaunchWorldScreenItemInfo` (2305) | Item-info popup title. |
| 0x4EA580 | `CItem::FormatItemDescription(CUIControlTextDisplay*, COLORREF)` (fork comment name; ours `sub_4EA580`, marked "TODO: Incomplete") | src/CItem.cpp:861 | `IEex_LaunchWorldScreenItemInfo` | Item-info popup body text. |
| 0x633EA0 | `CUIControlButtonInventoryHistoryIcon::SetItem` | src/CScreenInventory.cpp:5074 | `IEex_LaunchWorldScreenItemInfo` | Drives the popup's item-icon control (ctrl 2). |
| 0x4E2B50 | `CUIControlTextDisplay::RemoveAll` | src/CUIControlTextDisplay.cpp:437 | `IEex_LaunchWorldScreenItemInfo` | Clear the popup text area before refill. |
| 0x4E3D60 | `CUIControlTextDisplay::ScrollToBottom` | src/CUIControlTextDisplay.cpp:967 | refonte `ApplyLogHeight` (4961) | Keep the chat log pinned to the bottom after reflow. |
| 0x5F8610 | `CUIControlButtonCharacterSkillsPlusMinus::AdjustValue` | src/CScreenCharacter.cpp:7320 | `SkillsPlusMinus_OnRButtonClick` (4366) | R-click decrements: loop up to 128 AdjustValue calls until the points pool stops moving. |
| 0x61A980 | `CUIControlButtonCharGenSkillsPlusMinus::AdjustValue` | src/CScreenCreateChar.cpp:6954 | `SkillsPlusMinus_OnRButtonClick` | Same loop, char-gen screen (points offsets +0x798 GUIREC / +0x4F2 GUICG). |
| 0x452FE0 | `CInfGame::GetCharacterId(SHORT nPortrait)` | src/CInfGame.cpp:7282 | `IEex_PortraitContentHash` (2551) | Portrait identity for the dirty-hash. |
| 0x68C3D0 | `CScreenWorld::AsynchronousUpdate(BOOL bActiveEngine)` | src/CScreenWorld.cpp:723 | comment reference only | Fork comment cites it as the reason portraits must stay panel-1 controls (hover→spell-target). |
| 0x77B160 | `CUIControlPortraitWorld::OnLButtonUp(CPoint pt)` | src/CUIControlFactory.cpp:4073 | comment reference only | Fork comment cites it for portrait input ordering under the moved-in-place scheme. |
| 0x8BA31C | `CVideo::SCREENWIDTH` | src/CVideo.cpp:18 | `IEex_GetResolution` (973) | Current resolution reads. |
| 0x8BA31E | `CVideo::SCREENHEIGHT` | src/CVideo.cpp:21 | `IEex_GetResolution` | Current resolution reads. |
| 0x7FBE4E | fork comment names `CPtrList::AddTail` — unresolved in our tree (address falls inside the blob at src/music/audio.cpp:477) | — | `IEex_Extern_UI_ButtonLClick` (3744) | Options open/close bookkeeping on screenOptions+0x434. |
| 0x7FB343 | fork comment names `CPtrList::RemoveTail` — unresolved in our tree (same blob) | — | `IEex_Extern_UI_ButtonLClick` | Mirror of the above. |
| 0x847310 | function-pointer global; fork calls it via `IEex_Call` with the argument shape of kernel32 `GetPrivateProfileInt` — unresolved in our tree (no hit in src/) | — | `IEex_GetPrivateProfileInt` (34) | INI reads at load time. |
| 0x84730C | function-pointer global; same technique, `GetPrivateProfileString` argument shape — unresolved in our tree | — | `IEex_GetPrivateProfileString` (950, call at 960) | INI string reads (options, hotkey persistence). |
| 0x847308 | function-pointer global; same technique, `WritePrivateProfileString` argument shape — unresolved in our tree | — | `IEex_WritePrivateProfileString` (48, call at 55) | INI writes (head seeding, options persistence). |
| 0x8CF6D8 | pointer global (fork locals name it `chitin` / `refonteChitin`); byte +0x91C = GL renderer flag — unresolved in our tree | — | `InstallPortraitGrid` (4610), `InstallQuickloot` (5180), `IEex_OnCHUInitialized` (5631) | GL-active gate for refonte/HUD decisions. |
| 0x8CF6DC | pointer global (fork names it `g_pBaldurChitin`); +0x1906/+0x190A cursor x/y, byte +0x4A28 = UI 2x tier (fork comment: "m_bUseNewGui @0x8CF6DC+0x4A28, == the DLL's UIMult"), +0x49B4 = native-2x padding-panel array (Memset 0x1C dwords) — unresolved in our tree | — | `IEex_GetCursorXY` (21), `MoveHighResolutionPaddingPanels` (2455), `InitHighResolutionPaddingPanels` (2943/2985/3008), `IEex_OnCHUInitialized` (5626–5645) | Cursor reads, UI-tier (1x/2x) decisions, padding-panel reset. |

### (c) Engine globals / struct offsets read or written from Lua

| struct / global | offset | our name (where resolved) | fork usage |
|---|---|---|---|
| CInfGame | +0x1B78 / +0x1B7C | `m_worldTime` (`CTimerWorld`), src/CInfGame.h:387 — fork reads the pair as a tick value | Game-tick edge for panel-1 HUD repaint. |
| CInfGame | +0x1BA2 | `m_iContainer`, src/CInfGame.h:403 | Container-slot R-click, ground side. |
| CInfGame | +0x1BA6 | `m_iContainerSprite`, src/CInfGame.h:404 | Container-slot R-click, personal side. |
| CScreenWorld | +0xF3C | `m_nTopContainerRow`, src/CScreenWorld.h:174 | Ground-container scroll row (5 columns). |
| CScreenWorld | +0xF40 | `m_nTopGroupRow`, src/CScreenWorld.h:175 | Personal-container scroll row. |
| personal-container share | +0x4AD8 | unresolved in our tree | Slot base for the personal side of container R-click. |
| screenOptions | +0x434 | unresolved in our tree | CPtrList AddTail/RemoveTail target in `UI_ButtonLClick`. |
| creatureData | +0x628 / +0x4288 / +0x428C | unresolved (fork names MXSPLBRD) | Bard spontaneous-cast slot clamp (`IEex_Extern_OnUpdateRecordDescription` tail, 8439+). |
| creatureData | +0x630 / +0x4788 / +0x478C | unresolved (fork names MXSPLSOR) | Sorcerer spontaneous-cast slot clamp (8456+). |
| spell-slot entries | stride 0x10, blocks 0x1C, field +0x8 | — | Clamp: `ReadDword(offset+0x8) > baseNumSlots → WriteDword(offset+0x8, base)`. |
| creatureData | +0x5BC / +0x5EC / +0x75C / +0x764 / +0x744 / +0x920 | unresolved (fork does not name them) | Feat/kit/class reads inside `OnUpdateRecordDescription`. |

## Data flow

Load time: the file head runs before any patch gate. It wraps the three INI entry points over engine function-pointer globals, then normalizes `Icewind2.ini`: 3D Acceleration is forced on every launch (with the Software Renderer opt-out flipping it and every GL gate coherently off), gamma is normalized once behind a marker key, and the 42 `[IEex Options]` defaults are seeded read-with-default/write-back. `IEEX_HD_UI`, `IEEX_FLOAT_TEXT_HD`, `IEEX_GL_ACTIVE` and `IEEX_PORTRAIT_FRAMES_AVAILABLE` are fixed here and read by everything downstream.

CHU init: every time the engine finishes loading a CHU, `IEex_OnCHUInitialized` runs panel surgery — refonte centring of the dialog family, the item-info panel build, log-resize controls, quickloot button placement, and the keybinding-divider workaround. `IEex_InstallIEexOptions` / `InstallQuickloot` / `InstallActionIndicators` / `InstallPortraitGrid` are the per-screen installers, all idempotent guards first.

Per frame: `IEex_Extern_BeforeWorldRender` is the HUD-layer heartbeat. If the HUD layer is active (`IEex_Helper_IsHudLayerActive`), panel 1 is repainted only when the game tick advanced or `PortraitContentHash` changed (fingerprint of each portrait's character id + state), panel 100 when the ActionIndicators counter moved; dialog-family panels ask for a frame skip via `IEex_Helper_HudLayerSkipFrame` instead of being composited. World popups (panels 50/51) are re-centred every frame, and map cursor coordinates are mapped back through `UIScaleMapWorldCursor` so the refonte virtual canvas stays transparent to picking (`WorldRejectMapCursor` / `MouseInAreaViewport`).

Input: the `UI_ButtonLClick` / `UI_LabelLDown` / `UI_SliderThumb*` externs are the single funnel. The options rows are never matched by hardcoded control ids beyond the LABEL id lookup (`IEex_OptionRowOfControl`); a click toggles the row's ini key through its bridge, a slider drag lands in `UpdateOptionSlider`, and the description pane shows the row's `display` text. The custom `IEex_UI_Slider` control reports thumb movement through GenLuaCall caves installed by `IEex_DefineCustomControl` (bare `!ret` — the externs are no-arg __thiscall, not the `!ret_word 08 00` the CPoint slots need).

Persistence: `LoadOptions`/`WriteOptions`/`InitOptionButtons` iterate the same registry, so a new option is one descriptor row plus (optionally) one ini-comments entry. Under GL the fog type is forced non-transparent via `IEex_FogTypePtr` and the ini write is skipped. `IEex_InjectOptionIniComments` rewrites the raw ini file to interleave comment lines above each `[IEex Options]` key (insert-only, sentinel-marked, CRLF-aware, with a Wine cache reload read), and the options Done/Cancel path drops helper first-use caches (`SetMarkerStyle` / `ReloadFloatTextScale`) so marker style and float-text scale live-apply and pick up ini-only keys.

Record screen: `OnUpdateRecordDescription` rewrites the description text after the engine builds it — feature nil-guards, ITWF rank thresholds, critical-hit multiplier damage lines and immunity annotations — and clamps bard/sorcerer spontaneous-cast spell slots so converting a spell cannot push the castable count past the 2DA base.

## IEex_Helper_* entry points

Every DLL helper this file calls or installs into (occurrence counts from the staged fork file). All names are B1 must-verify: grepping the staged `dll-delta.tsv` for them returned nothing, so each needs a cross-check against the DLL helper table.

Called from Lua (34 distinct):

| helper | occurrences | notes |
|---|---|---|
| `IEex_Helper_GetBridge` | 30 | |
| `IEex_Helper_SetBridge` | 16 | |
| `IEex_Helper_SetBridgeNL` | 14 | |
| `IEex_Helper_GetBridgeNL` | 14 | |
| `IEex_Helper_WorldRejectMapCursor` | 5 | real guarded call. |
| `IEex_Helper_SynchronizedBridgeOperation` | 5 | |
| `IEex_Helper_InitBridgeFromTable` | 4 | |
| `IEex_Helper_GetBridgeNumIntsNL` | 3 | |
| `IEex_Helper_UIScaleMapWorldCursor` | 2 | real call at 2623; a second mention at 2432 is a comment ("old per-site … removed"). |
| `IEex_Helper_SetMarkerStyle` | 2 | guarded call (options Done). |
| `IEex_Helper_ReloadFloatTextScale` | 2 | guarded call (options Done). |
| `IEex_Helper_HudLayerSkipFrame` | 2 | |
| `IEex_Helper_HudClearPanelContentRects` | 2 | guarded (Refonte_RegisterRects 4946–4947). |
| `IEex_Helper_HudAddPanelContentRect` | 2 | guarded (4949/4953). |
| `IEex_Helper_GetBridgePtr` | 2 | `fogTypePtr` (3897/6282). |
| `IEex_Helper_UnlockGlobal` | 1 | |
| `IEex_Helper_SetPanelMosaicResref` | 1 | 1191. |
| `IEex_Helper_SetHDUI` | 1 | 2908. |
| `IEex_Helper_ScaleSoundRange` | 1 | COMMENT-ONLY mention (2932): "see IEex_Sound_Patch.lua -- do NOT also patch". Not a call site. |
| `IEex_Helper_RenderButtonIcon` | 1 | |
| `IEex_Helper_RegisterKeysScreenHardcodedMapDefaults` | 1 | |
| `IEex_Helper_RegisterKeysScreenCustomBindings` | 1 | |
| `IEex_Helper_ReadDataFromBridge` | 1 | |
| `IEex_Helper_Memset` | 1 | 3008. |
| `IEex_Helper_LockGlobal` | 1 | |
| `IEex_Helper_IterateBridge` | 1 | |
| `IEex_Helper_IsHudLayerActive` | 1 | |
| `IEex_Helper_GetUseItemFields` | 1 | 1775. |
| `IEex_Helper_GetBridgeCreateNL` | 1 | 2122. |
| `IEex_Helper_GetAttackItemFields` | 1 | 1799. |
| `IEex_Helper_DeepCopy` | 1 | 2046. |
| `IEex_Helper_DecrementButtonDoRender` | 1 | |
| `IEex_Helper_ClearBridgeNL` | 1 | |
| `IEex_Helper_AskResolution` | 1 | 2895. |

DLL exports referenced in comments without the `IEex_Helper_` prefix (cross-check on the B1 side too): `PointerOverRefonteContent`, `Export_WheelShouldZoom`, `Export_UIScaleCaptureMap`, `Export_UIScaleRenderBegin`.

Lua-side callbacks the DLL/assembly caves invoke by name (`IEex_Extern_*`, 39 distinct): AttemptSpontaneousCast, BeforeWorldRender, CScreenWorld_AsynchronousUpdate, CScreenWorld_OnInventoryButtonRClick, CUIControlBase_CreateControl, CUIControlButtonWorldContainerSlot_GetActiveContainerID, CUIControlButtonWorldContainerSlot_GetActiveContainerSpriteID, CUIControlButtonWorldContainerSlot_GetContainerItemIndex, CUIControlButtonWorldContainerSlot_OnLButtonClick_Done, CUIControlButtonWorldContainerSlot_OnLButtonClick_GetOnlyUpdateSlot, CUIControlButtonWorldContainerSlot_OnRButtonClick, CUIManager_fInit_CHUInitialized, CheckBitDepth, GetHighlightContainerID, GetSpontaneousCastColumnAndRow, GiveEngineKeysScreenLayout, InitGUIConstants, InitHighResolutionPaddingPanels, InitResolution, KeysScreenOnCustomBindingsChanged, MouseInAreaViewport, OnActionbarUnhandledRButtonClick, OnOptionsScreenESC, OnSetActionbarState, OnUpdateRecordDescription, OverrideWorldScreenScrollbarFocus, Quickloot_ScrollLeft, Quickloot_ScrollRight, RejectGetWorldCoordinates, RejectWorldScreenEsc, ShouldTooltipRefreshInsteadOfDying, SkillsPlusMinus_OnRButtonClick, StartDebugConsole, StopDebugConsole, UI_ButtonLClick, UI_ButtonRender, UI_LabelLDown, UI_SliderThumbChange, UI_SliderThumbFinalChange.
