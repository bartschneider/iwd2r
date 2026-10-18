# IEex_Gui_Patch.lua — GUI/UX pack: right-click enablement, cooldown removals, HUD layer, widescreen, keybinds, portrait refonte

Source: `~/.local/share/iwd2r/wso/lua/IEex_Gui_Patch.lua` (1807 lines; no upstream baseline — fork-only file). All hooks are installed inside one `IEex_EnableCodeProtection()`-guarded block (`(function() ... end)()` at line 1807).

## Intent

1. Enable right-click everywhere the stock engine only handles left-click: world-container loot slots (open the stock container window or the quickloot bar), the chargen skills +/- fill/empty buttons, the chargen animation preview button, and the world-screen inventory button.
2. Remove artificial click cooldowns: the actionbar button double-click guard, the dialog/reply "one click per frame" guards on all four input paths (number key, Enter, left-click Continue, left-click reply), and double-click redirection for container slots and the dialog Continue button.
3. Add a dedicated HUD Layer: after the world (area) renders, the UI manager's render is replayed into a separate FBO/layer (`IEex_Helper_HudLayerRender`), with a software/no-FBO/opt-out fallback; a "dialog idle present" saver skips the world render when nothing but the dialog is animating.
4. Ship a widescreen/resolution pack (this block is inherited from upstream IWD2EE code and gated on `not IEex_Vanilla` in places): drive `InitResolution`, `CheckBitDepth`, `InitGUIConstants`, and high-resolution panel padding from Lua, and select GUIW08/GUIW10 CHUs by width.
5. Replace world-HUD border MOSpics with minimal-memory variants through a `CDimm::GetResObject` chokepoint detour, enabled by `[IEex Options] "UI Borders"=0` (i.e. the non-default setting enables the redirect).
6. Fully replace the CScreenKeys keybind editor: 13 whole-entry overrides re-implement hotkey mapping, staged keybinds, palette updates, and panel updates in the DLL.
7. Add spontaneous casting for divine casters (SPONCAST.2DA): intercept the action-button press, ask Lua whether to substitute a spell, and remap the spell/level slot selection.
8. Refonte the portrait HUD ("IEex_PortraitGridEnabled" Lua global, set from IEex_Gui_State.lua together with IEEX_PORTRAIT_FRAMES_AVAILABLE): rectangular/portrait-grid rendering via `IEex_Helper_RenderPortraitRect`, panel MOS skip, right-click refonte, portrait cell bitmap drop, and disabled-panel dither.
9. Gate stock-vs-physical cursor handling: formation right-clicks are swallowed for both HUDs, and the physical-cursor world handlers (action down/up/dblclk, mouse move) plus phantom hit-test gates keep the floating IEex bars clickable while the logical cursor is elsewhere.
10. Fix the stuck-yellow portrait highlight by clearing `CUIControlPortraitWorld::m_bHighlighted` on button KillFocus.
11. Suppress tooltip "kill" during effect-list updates so tooltips refresh instead of dying (Bubb's tooltip flicker fix).
12. Misc: doubled-font atlas path enabled GL-only, space bar acts as dialog Continue, blank back/CCache buffers during loads (progress screens), debug console start/stop notifications, spellbook popup stack global lock, auto-scroll adjustment, message-screen 1-second removal removal, negative CHU panel coordinates, learnable-scroll green overlay on all four item-slot renderers, elf store raise, options-screen ESC/restore-assert fixes, world scrollbar focus override, scroll bar encumbrance label updates, CHU-init notification, custom CHU control creation via Lua, and Tab-tooltip conditional jumps.

## Hooks table

Gate key used below: `[NV]` = installed only `if not IEex_Vanilla`; `[—]` = unconditional; `[PG]` = installed only when Lua global `IEex_PortraitGridEnabled` is truthy; `[UB]` = installed only when `[IEex Options] "UI Borders"` (IEex_GetPrivateProfileInt, default 1) == 0.

### A. Right-click enablement and custom CHU controls

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x76D41B HookJump(n=0) | inside `CUIControlBase::CreateControl` (entry 0x76D370) | src/CUIControlFactory.cpp:33 (exact marker; decl follows) | `IEex_Extern_CUIControlBase_CreateControl` (Lua) | [NV] After the stock factory fails to match the control type, call Lua with CHU resref (edx), panel (edi), controlInfo (esi); nonzero return jumps to `:76E93F` (custom-control path) |
| 0x4D3D55 HookBeforeCall | inside `CUIManager::fInit` (entry 0x4D3B80), at the CreatePanelArea/CHU call | src/CUIManager.cpp:53 (exact marker; decl follows) | `IEex_Extern_CUIManager_fInit_CHUInitialized` (Lua) | [—] Notifies Lua once the CHU is loaded (esi = manager, [ecx+10] = resref) |
| 0x695C8E HookBeforeCall | inside `CUIControlButtonWorldContainerSlot::OnLButtonClick` (entry 0x6957E0), at the scroll-bar update call | src/CScreenWorld.cpp:4911 (exact marker; decl follows) | — | [NV] `push [esp+18]`, jnz → run original; else return: suppress ScrollBar UpdateScrollBar when the Lua-driven slot state says not to update |
| 0x696080 HookBeforeCall | inside same function, at `Encumbrance_SetVolume` | src/CScreenWorld.cpp:4911 | — | [NV] `push [esp+20]`, jz → call, else `add esp,8` return: drop volume label update |
| 0x69608D HookBeforeCall | inside same function, at `Encumbrance_SetEncumbrance` | src/CScreenWorld.cpp:4911 | — | [NV] same pattern for the encumbrance label |
| 0x6960EE HookBeforeCall | inside same function, at the label SetText call | src/CScreenWorld.cpp:4911 | — | [NV] `push [esp+1C]`, `add esp,4`: drop the label text update |
| 0x69589C HookRestore(6) | inside `CUIControlButtonWorldContainerSlot::OnLButtonClick` | src/CScreenWorld.cpp:4911 | `IEex_Extern_CUIControlButtonWorldContainerSlot_GetActiveContainerID` (Lua) | [NV] Restore activeContainerID; FF sentinel → return_skip (no container) |
| 0x6958C7 HookRestore(6) | inside same | src/CScreenWorld.cpp:4911 | `IEex_Extern_..._GetActiveContainerSpriteID` (Lua) | [NV] restore activeContainerSpriteID (esi) |
| 0x6959A3 HookRestore(8) | inside same | src/CScreenWorld.cpp:4911 | `IEex_Extern_..._GetContainerItemIndex` (Lua) | [NV] restore m_nTopContainerRow; override row via `mov edi,eax` + `mov [esp+34],edi` |
| 0x6956F6 IEex_WriteAssembly | inside `CUIControlButtonWorldContainerSlot` ctor (entry 0x6956F0) | src/CScreenWorld.cpp:4900 (exact marker; decl follows) | — | [NV] `mov al,3; nop x3`: the button's mouse-mask seed becomes LBUTTON\|RBUTTON so R-clicks reach the control at all |
| 0x85A3E8 IEex_WriteDword | `.rdata` vtable slot 0x70 of CUIControlButtonWorldContainerSlot (data write; nearest marker is CScreenWorld::BORED_TIME) | src/CScreenWorld.cpp:24 (exact marker; data region) | `IEex_Extern_CUIControlButtonWorldContainerSlot_OnRButtonClick` (Lua) | [NV] Slot → cave stub: `push ecx`, GenLuaCall, ret 8; covers the stock container window and the quickloot bar |
| 0x853BE0 IEex_WriteDword | `.rdata` vtable slot (chargen screen region; nearest marker AUTO_PICK_SPECIALIST_SPELLS) — fork comment calls the target "CScreenCreateChar::CheckMouseRButton" | src/CScreenCreateChar.cpp:133 (exact marker; data region) | — | [NV] Slot ← 0x49FC40. Naming discrepancy: 0x49FC40 in our tree is `CBaldurChitin::GetIDSChoosePixelFormat()` (src/CBaldurChitin.cpp:1171, returns IDS_CHOOSE_PIXEL_FORMAT = 1, i.e. the same `mov eax,1; ret` constant-return shape the fork wants: always allow R-button in chargen) |
| 0x852D24, 0x854248 IEex_WriteDword | `.rdata` vtable slots 0x24 (two chargen skill +/- buttons; regions nearest CInfinity::RENDER_MESSAGESCREEN and AUTO_PICK_SPECIALIST_SPELLS) | src/CInfinity.cpp:100, src/CScreenCreateChar.cpp:133 (exact markers; data regions) | — | [NV] Slots → `or byte [ecx+0x138],2` (OR RBUTTON into the button's mouse mask) then `!jmp_dword :4D4F70` = tail-jump into `CUIControlButton::OnRButtonDown` (0x4D4F70 exact, src/CUIControlButton.cpp:215) |
| 0x852D70, 0x854294 IEex_WriteDword | `.rdata` vtable slots 0x70 (same two buttons) | src/CInfinity.cpp:100, src/CScreenCreateChar.cpp:133 | `IEex_Extern_SkillsPlusMinus_OnRButtonClick` (Lua) | [NV] Slots → GenLuaCall stub, ret 8 |
| 0x68C3D0 HookRestore(7) | `CScreenWorld::AsynchronousUpdate` (function entry, exact) | src/CScreenWorld.cpp:1663 (exact marker; decl follows) | `IEex_Extern_CScreenWorld_AsynchronousUpdate` (Lua) | [NV] Lua sees every async world update (no args) |
| 0x85D798 IEex_WriteDword | `.rdata` vtable slot (region nearest CMarker::RECTICLE_XGAP) — world-screen inventory button | src/CMarker.cpp:51 (exact marker; data region) | `IEex_Extern_CScreenWorld_OnInventoryButtonRClick` (Lua) | [NV] Slot → GenLuaCall stub (ret 8) |
| 0x47F954 HookAfterRestore(5) | inside `CGameContainer::Render(CGameArea*, CVidMode*, INT)` | src/CGameContainer.cpp:526 (exact marker; decl follows) | `IEex_Extern_GetHighlightContainerID` (Lua) | [NV] Manual lua-C-API call (IEex_GetLuaState/tonumber/__ftol2_sse); matched container (`cmp edx,[esi+5C]`) forces eax=1: highlight the container under the cursor while Lua drives it |
| 0x85A3E4 IEex_WriteDword | `.rdata` vtable slot OnLButtonDoubleClick of CUIControlButtonWorldContainerSlot (same data region as 0x85A3E8) | src/CScreenWorld.cpp:24 | — | [NV] Slot ← 0x66D760 = `CUIControlButtonSpellbookSpell::OnLButtonDown` (exact marker src/CScreenSpellbook.cpp:2176, decl follows): double-click is downgraded to a plain down, killing the container-slot double-click cooldown |

### B. Click-cooldown removals and dialog pacing

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x5DC792 HookBeforeCall | inside `CScreenCharacter::UpdateMainPanel` (entry 0x5DBDE0), at the record-description update call | src/CScreenCharacter.cpp:2537 (exact marker; decl follows) | `IEex_Extern_OnUpdateRecordDescription` (Lua) | [NV] `!mark_esp(50)`; args esi, [esp-3C] via marked esp, ebx, ecx; Lua may refresh the record text panel |
| 0x85C518 IEex_WriteDword | `.rdata` vtable slot OnLButtonDown of the actionbar buttons | src/CMarker.cpp:51 (data region) | — | [—] Slot ← 0x4D4D70 = `CUIControlButton::OnLButtonDown` (exact, src/CUIControlButton.cpp:150): skips the stock derived override that imposed the click cooldown |
| 0x687606 IEex_WriteAssembly (2 nops) | inside `CScreenWorld::OnKeyDown` (entry 0x6872D0), number-key reply path | src/CScreenWorld.cpp:592 (exact marker; decl follows) | — | [—] Removes the reply cooldown guard |
| 0x68746A IEex_WriteAssembly (6 nops) | inside same, Enter-key reply path | src/CScreenWorld.cpp:592 | — | [—] Removes the reply cooldown guard |
| 0x6968E0 IEex_WriteAssembly (2 nops) | inside `CUIControlButtonDialog::OnLButtonClick` (entry 0x696890), Continue path | src/CScreenWorld.cpp:4938 (exact marker; decl follows) | — | [—] Removes the Continue cooldown guard |
| 0x77BCC8 IEex_WriteAssembly (2 nops) | inside `CUIControlTextDisplayDialog::OnItemSelected(LONG lMarker)` (entry marker src/CUIControlFactory.cpp:4423) | src/CUIControlFactory.cpp:4423 (exact marker; decl follows) | — | [—] Removes the reply-click cooldown guard |
| 0x85A45C IEex_WriteDword | `.rdata` vtable slot OnLButtonDoubleClick of the dialog Continue button | src/CScreenWorld.cpp:24 (data region) | — | [—] Slot ← 0x4D4D70 (`CUIControlButton::OnLButtonDown`): double-click Continue acts as a single click |
| 0x687434 HookJump(5) | inside `CScreenWorld::OnKeyDown` | src/CScreenWorld.cpp:592 | — | [—] Space-to-Continue: the key test becomes `cmp al,20` (space) instead of the original key |
| 0x68785E HookRestore(6) | inside `CScreenWorld::OnKeyDown` | src/CScreenWorld.cpp:592 | `IEex_Extern_RejectWorldScreenEsc` (Lua) | [NV] true → `:689504` (skip ESC handling entirely) |
| 0x654446 HookBeforeCall | inside `CScreenOptions::OnKeyDown(SHORT nKeysFlags)` (entry marker src/CScreenOptions.cpp:360) | src/CScreenOptions.cpp:360 (exact marker; decl follows) | `IEex_Extern_OnOptionsScreenESC` (Lua, Boolean) | [NV] true → return: Lua can veto options-screen ESC |
| 0x6554FB IEex_WriteAssembly (5 nops) | inside `CScreenOptions::UpdatePopupPanel(DWORD, BOOLEAN)` (entry marker src/CScreenOptions.cpp:880) | src/CScreenOptions.cpp:880 (exact marker; decl follows) | — | [NV] NOPs the CVidMode-restore assert |
| 0x68DECB HookAfterRestore(5) | inside `CScreenWorld::TimerSynchronousUpdate` (entry 0x68DEC0) | src/CScreenWorld.cpp:2327 (exact marker; decl follows) | `IEex_Extern_OverrideWorldScreenScrollbarFocus` (Lua, Boolean) | [NV] Prefix `cmp [ebp+4],0` (m_UIManager->m_resLoaded); jz `:68DF04`; Lua true → `:68DF04` — the m_displayStale quickload crash guard |
| 0x68DF87 IEex_WriteAssembly (5 nops) + 0x68DF9F HookBeforeCall | inside `CScreenWorld::TimerSynchronousUpdate` | src/CScreenWorld.cpp:2327 | `IEex_Extern_BeforeWorldRender` (Lua) | [—] World UI is not drawn before the world render any more (nops) and Lua is told when the world is about to render |
| 0x484B94 HookRestore(3) | inside `CGameDialogEntry::Handle` (entry 0x484900) | src/CGameDialog.cpp:904 (exact marker; decl follows) | `IEex_Extern_AdjustAutoScrollY` (Lua, Number) | [NV] `sub [esp],eax`: Lua adjusts the auto-scroll Y delta |
| 0x484BE1 IEex_WriteAssembly (!jmp_byte) | inside `CGameDialogEntry::Handle` | src/CGameDialog.cpp:904 | — | [NV] Removes the 1-second message-screen minimum delay |

### C. HUD Layer FBO, DialogIdlePresent, blanking, debug console

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x68DFB6 HookRestore(5) | inside `CScreenWorld::TimerSynchronousUpdate`, right after the world render / before `CUIManager::Render` (0x4D4540 exact, src/CUIManager.cpp:409) | src/CScreenWorld.cpp:2327 | `IEex_Helper_HudLayerRender` | [—] `mov ecx,ebp; push ecx; call IEex_Helper_HudLayerRender` replaces the stock UI render; helper falls back to the original when software-rendered, no FBO, or `[IEex Options] "HUD Layer"=0`; `IEex_Helper_IsHudLayerActive` is queried by the BeforeWorldRender fallback path |
| 0x68DF53 HookRestore(7) | inside `CScreenWorld::TimerSynchronousUpdate`, the render-block entry right after `CInfGame::SynchronousUpdate` | src/CScreenWorld.cpp:2327 | `IEex_Helper_DialogIdlePresent` (checked; eax=1 → `!jmp_dword :68DFC3`) | [—] When only the dialog is animating, jump to the epilogue — the engine's own skip — instead of re-rendering the world |
| 0x46E45F HookReturnNOPs(0x4A) | inside `CGameArea::AIUpdate` (entry 0x46E3D0) | src/CGameArea.cpp:1890 (exact marker; decl follows) | `IEex_Extern_MouseInAreaViewport` (Lua, Boolean) | [—] Lua decides "mouse in viewport"; error → eax=1 (treat as inside); writes the flag through `mov [esp+1C],eax` |
| 0x790DC4 HookRestore(8) | inside `CChitin::SynchronousUpdate` (entry 0x790B70) | src/CChitin.cpp:2062 (exact marker; decl follows) | `IEex_Helper_BlankBackBuffer` | [—] Blanks the back buffer during load screens |
| 0x4408C2 HookRestore(5); 0x44208D, 0x4422AF, 0x4427FF HookAfterRestore(6) | inside `CCacheStatus::Update` (entry 0x4406B0) | src/CCacheStatus.cpp:226 (exact marker; decl follows) | `IEex_Helper_BlankCCache1`, `IEex_Helper_BlankCCache2` | [—] Blank the cache-status surfaces on the four progress-update paths |
| 0x69126A HookAfterCall | inside `CScreenWorld::StartCommand` (entry 0x691140) | src/CScreenWorld.cpp:3301 (exact marker; decl follows) | `IEex_Extern_StartDebugConsole` (Lua) | [—] Lua opens its console when the engine console starts |
| 0x6913AF HookAfterRestore(10) | inside `CScreenWorld::StopCommand` (entry 0x6912A0) | src/CScreenWorld.cpp:3324 (exact marker; decl follows) | `IEex_Extern_StopDebugConsole` (Lua) | [—] Lua closes its console when the engine console stops |

### D. Cursor, viewport rejection, phantom hit-tests, formation R-click

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x5CDFCE HookJump(3) | inside `CInfinity::GetWorldCoordinates` (entry 0x5CDFC0) | src/CInfinity.cpp:1596 (exact marker; decl follows) | `IEex_Extern_RejectGetWorldCoordinates` (Lua, Boolean) | [—] Args ecx, [eax], [eax+4]; Lua may reject the world-coords result; retest `cmp edi,ebx` |
| 0x4765DC HookJump(0) | inside `CGameArea::OnActionButtonDblClk` (entry 0x4765C0) | src/CGameArea.cpp:4979 (exact marker; decl follows) | `IEex_Extern_RejectGetWorldCoordinates` (Lua) | [—] lea [edi+4CC] (action result coords); Lua reject → retest `cmp ecx,edx` |
| 0x475465 HookJump(0) | inside `CGameArea::OnActionButtonDown` (entry 0x475440) | src/CGameArea.cpp:4593 (exact marker; decl follows) | `IEex_Extern_RejectGetWorldCoordinates` (Lua) | [—] lea [esi+4CC]; reject → retest `cmp eax,[esi+514]` |
| 0x4766BA HookJump(0) | inside `CGameArea::OnFormationButtonDown` (entry 0x476680) | src/CGameArea.cpp:4992 (exact marker; decl follows) | `IEex_Extern_RejectGetWorldCoordinates` (Lua) | [—] Right-click formation path; fork comment: the IEex bars float inside the viewport rect unlike the stock HUD, so rejection must be Lua-driven |
| 0x68C2C0 IEex_WriteAssembly (stub, `!jmp_dword :68C2C5` tail) | inside `CScreenWorld::OnRButtonDown` (entry 0x68C280) | src/CScreenWorld.cpp:1622 (exact marker; decl follows) | `IEex_Helper_FormationRDown` | [NV] Export_FormationRDown goes through the helper, which decides StockHudBarsHit vs RefonteBarsHit and may swallow the click |
| 0x68C320 IEex_WriteAssembly (stub, `:68C325` tail) | inside `CScreenWorld::OnRButtonUp` (entry 0x68C2E0) | src/CScreenWorld.cpp:1637 (exact marker; decl follows) | `IEex_Helper_FormationRUp` | [NV] Matching formation R-up swallow (NOT gated on [PG] — covers the stock HUD too) |
| 0x68C0D4 IEex_WriteAssembly (stub, `:68C0D9`) | inside `CScreenWorld::OnLButtonDown` (entry 0x68C080) | src/CScreenWorld.cpp:1543 (exact marker; decl follows) | `IEex_Helper_ActionDownPhys` | [NV] Physical-cursor action handling before the stock path (WorldPtPhysicalize) |
| 0x68C0A1 IEex_WriteAssembly (stub, `:68C0A6`) | inside `CScreenWorld::OnLButtonDown` | src/CScreenWorld.cpp:1543 | `IEex_Helper_UILDownGated` | [NV] Phantom-band gate on the logical UI down path |
| 0x68C182 IEex_WriteAssembly (stub, `:68C187`) | inside `CScreenWorld::OnLButtonUp` (entry 0x68C100) | src/CScreenWorld.cpp:1560 (exact marker; decl follows) | `IEex_Helper_ActionUpPhys` | [NV] Physical-cursor action up |
| 0x68C060 IEex_WriteAssembly (stub, `:68C065`) | inside `CScreenWorld::OnLButtonDblClk` (entry 0x68C020) | src/CScreenWorld.cpp:1528 (exact marker; decl follows) | `IEex_Helper_ActionDblClkPhys` | [NV] Physical-cursor double-click |
| 0x68C02D IEex_WriteAssembly (stub, `:68C032`) | inside `CScreenWorld::OnLButtonDblClk` | src/CScreenWorld.cpp:1528 | `IEex_Helper_UIDblClkGated` | [NV] Phantom-band gate on the logical double-click path |
| 0x68C244 IEex_WriteAssembly (stub, `:68C249`) | inside `CScreenWorld::OnMouseMove` (entry 0x68C1A0) | src/CScreenWorld.cpp:1586 (exact marker; decl follows) | `IEex_Helper_MouseMovePhys` | [NV] Physical-cursor move tracking |
| 0x68C20F IEex_WriteAssembly (stub, `:68C214`) | inside `CScreenWorld::OnMouseMove` | src/CScreenWorld.cpp:1586 | `IEex_Helper_UIMouseMoveGated` | [NV] Phantom-band gate on the logical move path |
| 0x68C28D IEex_WriteAssembly (stub, `:68C292`) | inside `CScreenWorld::OnRButtonDown` | src/CScreenWorld.cpp:1622 | `IEex_Helper_UIRDownGated` | [NV] Phantom-band gate on the logical R-down path |
| 0x4D3FC7 IEex_WriteAssembly (stub, `:4D3FCC`) | inside `CUIManager::TimerAsynchronousUpdate` (entry 0x4D3F00), before the `CUIPanel::TimerAsynchronousUpdate` call | src/CUIManager.cpp:157 (exact marker; decl follows) | `IEex_Helper_PanelTimerGated` | [NV] The panel timer replays with "over no control" when the physical cursor is outside the phantom band |
| 0x4D4C50 IEex_AttemptHook | `CUIControlButton::KillFocus` (exact) | src/CUIControlButton.cpp:113 (exact marker; decl follows) | `IEex_Helper_ButtonKillFocus` | [NV] Verify bytes `56 8B F1 8B 86 46 06 00 00` then detour `push ecx; push ecx; call IEex_Helper_ButtonKillFocus; pop ecx; !jmp_dword :4D4C59`; helper clears CUIControlPortraitWorld::m_bHighlighted (+0x666) — the stuck-yellow-portrait fix. m_bHighlighted is set at 0x77AFD0 (`CUIControlPortraitWorld::OnMouseMove`, exact src/CUIControlFactory.cpp:3858) and cleared at 0x77B160 (`CUIControlPortraitWorld::OnLButtonUp`, exact :3891); KillFocus is reachable from m_mode 0x142 release-kill, SetCapture, EngineDeactivated, StopCommand, popup close, RemoveControl |

### E. Widescreen / resolution pack (upstream-inherited)

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x42212A HookReturnNOPs(1) | inside `CBaldurChitin::CBaldurChitin()` (entry marker src/CBaldurChitin.cpp:153) | src/CBaldurChitin.cpp:153 (exact marker; decl follows) | `IEex_Extern_InitResolution` (Lua) | [NV] Resolution init is Lua-driven |
| 0x42216E HookJumpNoReturn | inside the same ctor | src/CBaldurChitin.cpp:153 | — | [NV] Width compare (`cmp ecx,320`) and `cmp word [8BA31E],258` pick GUIW08 vs GUIW10; targets `:422198` / `:4224CC` |
| 0x42228C IEex_WriteAssembly (7 nops) | inside the same ctor | src/CBaldurChitin.cpp:153 | — | [NV] Do not write g_resolution[1] on the GUIW10 path |
| 0x422848 HookRestore(6) | inside the same ctor | src/CBaldurChitin.cpp:153 | `IEex_Extern_InitGUIConstants` (Lua) | [NV] GUI constants (panel rects) are Lua-driven |
| 0x4229D5 HookAfterRestore(7) | inside the same ctor | src/CBaldurChitin.cpp:153 | `IEex_Extern_InitHighResolutionPaddingPanels` (Lua; arg push esi) | [NV] Lua pads the high-resolution panels |
| 0x79B3DF HookRestore(5) | inside `CVidInf::ActivateVideoMode` (entry 0x79B100) | src/CVidInf.cpp:63 (exact marker; decl follows) | `IEex_Extern_CheckBitDepth` (Lua, Boolean) | [NV] Non-32-bit modes: false → `mov ecx,[8CF6D8]; !jmp_dword :79B34B` (skip the reject) |

### F. UI-Borders MOS redirect [UB]

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x786DF0 IEex_AttemptHook | `CDimm::GetResObject(const CResRef&, USHORT nResType, BOOL bWarn)` (exact) | src/CDimm.cpp:1626 (exact marker; decl follows) | — (uses core `IEex_Malloc` + `IEex_WriteLString` + Lua-side `leBytes()`) | [UB] Expected bytes `8B 44 24 0C 8B 54 24 04`; detour overwrites &resref ([esp+4]) with a malloc'd minimal resref before `!jmp_dword :786DF8`. MOS nType 1004 = 0x3EC. 9-entry redirect table (session-fixed): GCOMM118→GCOMM11M, GCOMM128→GCOMM12M, GACTN118→GACTN11M, GMWDMMB0→GMWDMMBM, GMWDSLB0→GMWDSLBM, GUCONTB0→GUCONTBM, GUDEATH0→GUDEATHM, GUWBTP30→GUWBTP3M, B3QKLOOT→B3QKLOOM (GCOMM11M-style minimal resrefs) |

### G. CScreenKeys keybinds — 13 whole-entry DLL overrides

All 13 are `IEex_WriteAssembly(addr, "!jmp_dword >IEex_Helper_*Override" + nop padding)` — the engine function is replaced from its first instruction. All resolved EXACT.

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x597740 | `CInfCursor::SetToolTip` | src/CInfCursor.cpp:254 (exact marker; decl follows) | `IEex_Helper_CInfCursor_SetToolTipOverride` | [NV] Fork name: SetToolTipOverride |
| 0x5A98F0 | `CInfGame::LoadKeymap` | src/CInfGame.cpp:3674 (exact marker; decl follows) | `IEex_Helper_CInfGame_InitHotkeyMapOverride` | [NV] Fork name: InitHotkeyMap |
| 0x5A9B00 | `CInfGame::SetKey` | src/CInfGame.cpp:3745 (exact marker; decl follows) | `IEex_Helper_CInfGame_UpdateKeybindMappingOverride` | [NV] Fork name: UpdateKeybindMapping |
| 0x639DC0 | `CScreenKeymaps::StartKeymaps` | src/CScreenKeymaps.cpp:752 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_InitDueToUserInteractionOverride` | [NV] Fork name: InitDueToUserInteraction |
| 0x639160 | `CScreenKeymaps::HandleKeyDown` | src/CScreenKeymaps.cpp:475 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_OnKeyDownProcessOverride` | [NV] Fork name: OnKeyDownProcess |
| 0x639790 | `CScreenKeymaps::OnLButtonDown` | src/CScreenKeymaps.cpp:624 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_OnLButtonDownOverride` | [NV] |
| 0x63A480 | `CScreenKeymaps::OnErrorButtonClick` | src/CScreenKeymaps.cpp:1064 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_OnPanel1ButtonClickOverride` | [NV] Fork name: OnPanel1ButtonClick |
| 0x63AA60 | `CScreenKeymaps::SelectKeymap` | src/CScreenKeymaps.cpp:1229 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_SetSelectedDisplayIndexOverride` | [NV] Fork name: SetSelectedDisplayIndex |
| 0x63A910 | `CScreenKeymaps::SetKeymap` | src/CScreenKeymaps.cpp:1207 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_SetStagedKeybindOverride` | [NV] Fork name: SetStagedKeybind |
| 0x63A7A0 | `CScreenKeymaps::sub_63A7A0` (unnamed in our tree) | src/CScreenKeymaps.cpp:1168 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_UpdateHoverPalettesOverride` | [NV] Fork name: UpdateHoverPalettes |
| 0x63A660 | `CScreenKeymaps::sub_63A660` (unnamed in our tree) | src/CScreenKeymaps.cpp:1133 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_UpdateLabelPalettesOverride` | [NV] Fork name: UpdateLabelPalettes |
| 0x639A50 | `CScreenKeymaps::UpdateMainPanel` | src/CScreenKeymaps.cpp:701 (exact marker; decl follows) | `IEex_Helper_CScreenKeys_UpdateMainPanelOverride` | [NV] |
| 0x63ACA0 | `CUIControlButtonKeymap::OnLButtonClick` | src/CScreenKeymaps.cpp:1260 (exact marker; decl follows) | `IEex_Helper_CUIControlButtonKeys_OnLButtonClickOverride` | [NV] |

### H. Spontaneous casting (SPONCAST.2DA)

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x589110 HookRestore(5) | `CInfButtonArray::SetState(INT nState, BYTE bPush)` (exact; function entry) | src/CInfButtonArray.cpp:680 (exact marker; decl follows) | `IEex_Extern_OnSetActionbarState` (Lua) | [NV] `!marked_esp !push([esp+4])` nState: Lua sees every actionbar state change |
| 0x5947DA HookJumpOnSuccess(0) | inside `CInfButtonArray::OnRButtonPressed(int buttonID)` (entry 0x594720) | src/CInfButtonArray.cpp:4382 (exact marker; decl follows) | `IEex_Extern_OnActionbarUnhandledRButtonClick` (Lua) | [NV] arg push esi = buttonID: right-clicks on buttons with no stock handler go to Lua |
| 0x5915F4 HookReturnNOPs(15) | inside `CInfButtonArray::OnLButtonPressed` (entry 0x58FF20) | src/CInfButtonArray.cpp:2974 (exact marker; decl follows) | `IEex_Extern_AttemptSpontaneousCast` (Lua, Boolean) | [NV] Guard `cmp byte [ebp+3A],0` (m_bDisabled); args: sprite [esp+10], buttonData ebp; true → return (spell substituted), else `:59162B` |
| 0x7167BB, 0x7167D9 IEex_WriteAssembly (!jmp_byte) | inside `CGameSprite::sub_716770(CButtonData*)` (entry 0x716770) | src/CGameSprite.cpp:28986 (exact marker; decl follows) | — | [NV] NOPs the two cleric-spell asserts |
| 0x716841 HookReturnNOPs(24) | inside `CGameSprite::sub_716770(CButtonData*)` | src/CGameSprite.cpp:28986 | `IEex_Extern_GetSpontaneousCastColumnAndRow` (Lua) | [NV] Args: sprite [esp+14], buttonData [esp+90], returnValues &([esp-1C]); results land in ebp = SPONCAST column, ebx = row, then the spell/level slot selection is rewritten |

### I. Modern portrait HUD refonte [PG] and panel rendering

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x704D40 IEex_WriteAssembly | `CGameSprite::RenderPortrait(const CPoint&, const CSize&, BOOL, BOOL, BOOL, const CRect&, BOOL)` (exact; whole-entry) | src/CGameSprite.cpp:5596 (exact marker; decl follows) | `IEex_Helper_RenderPortraitRect` | [PG] `!mark_esp` + 7 pushes [esp+1C..04] + `push ecx` + ret 1C: whole function marshals into the helper. Fork comments: geometry from control size (image sz−4, ring sz−2), rectangular 54×84 is fine, stock 46×46 stays pixel-identical. Mutually exclusive with the UIScale HD-UI block, which writes `mov [esp+1C],1` over the same 7 prologue bytes |
| 0x4D3435 IEex_WriteAssembly (mosStub, `!jmp_dword :4D343A`) | inside `CUIPanel::Render` (entry 0x4D3100), the panel-1 GACTN band MOS draw | src/CUIPanel.cpp:313 (exact marker; decl follows) | `IEex_Helper_PanelMosaicRender` | [PG] Skip the stock background MOS so the refonte grid shows through |
| 0x4D4492 IEex_WriteAssembly (rclickStub, `!jmp_dword :4D4497`) | inside `CUIManager::OnRButtonDown` (entry 0x4D43D0) | src/CUIManager.cpp:349 (exact marker; decl follows) | `IEex_Helper_PanelRClickRefonte` | [PG] Right-click routing for the refonte HUD |
| 0x4D5225 IEex_WriteAssembly (porCellStub, `!jmp_dword :4D522A`) | inside `CUIControlButton::Render` (entry marker src/CUIControlButton.cpp:259, decl follows) | src/CUIControlButton.cpp:259 | `IEex_Helper_PortraitButtonCellRender` | [PG] Drops the GUIRSPOR bitmap; fork comment: fixes the green-L bug (Bubb 2026-07-25); helper skips only CUIControlPortraitWorld buttons |
| 0x4D34ED IEex_WriteAssembly (`!call >IEex_Helper_HudPanelRenderDither`) | inside `CUIPanel::Render` | src/CUIPanel.cpp:313 | `IEex_Helper_HudPanelRenderDither` | [PG] Disabled-panel dimming clipped to the content rects; fork notes a veil bug when log x runs 16..1118 |
| 0x4D3435 HookJumpToAutoReturn | inside `CUIPanel::Render` | src/CUIPanel.cpp:313 | `IEex_Helper__CUIPanel_Render__CVidMosaic_Render` | [NV] The mosaic draw goes through the helper with gamma disabled (same address as the [PG] mosStub — the two installs are mutually exclusive by gate) |
| 0x855AC8, 0x85C770 IEex_WriteDword | `.rdata` vtable slots NeedRender of "Inventory Portraits" and "Portrait Health Bars" panels | (data regions; not resolved to a named marker in our tree — vtable-region data writes) | — | [NV] Slots → `noNeedRenderWhenInventoryPopupOpen` stub: reads g_pBaldurChitin [0x8CF6DC], m_pEngineInventory +0x1C68, pActiveEngine +0x3C4, m_lPopupStack.m_nCount +0x49C; returns no-render while a popup is open (portrait flicker fix); original entry `:4D4C20` |

### J. Tooltip, popup lock, fonts, item-slot overlays, misc UI fixes

| address | engine function (our name for the hook target) | iwd2r file:line | helper called (if any) | what changes |
|---|---|---|---|---|
| 0x734704 HookBeforeAndAfterCall | inside `CGameSprite::HandleEffects` (entry 0x734550), the effect-list processing call | src/CGameSprite.cpp:16216 (exact marker; decl follows) | `IEex_TooltipKillReason` Lua table (EFFECT_LIST_UPDATE / UNKNOWN) | [NV] `IEex_Malloc(4)` flag toggled around the call; `determineTooltipKillReason` stub reports the reason so Lua can suppress the tooltip kill |
| 0x4D4060 HookRestore(7) | `CUIManager::ClearTooltip` (exact; function entry) | src/CUIManager.cpp:222 (exact marker; decl follows) | `IEex_Extern_ShouldTooltipRefreshInsteadOfDying` (Lua, Boolean) | [NV] Tooltip kill suppression decision point |
| 0x4D40A8 HookAfterRestore(7) | inside `CUIManager::ClearTooltip` | src/CUIManager.cpp:222 | — | [NV] `mov [eax+76],1` — sets m_bIsForceToolTip so the tooltip refreshes instead of dying |
| 0x687AFB, 0x68BC16, 0x6873D5 IEex_WriteAssembly (!jmp_byte ×3) | inside `CScreenWorld::OnKeyDown` | src/CScreenWorld.cpp:592 | — | [—] Tab-tooltip conditional jumps (show tooltips while Tab is held) |
| 0x66B480 HookJumpToAutoReturn | inside `CScreenSpellbook::SummonPopup` (entry 0x66B3C0) | src/CScreenSpellbook.cpp:1276 (exact marker; decl follows) | `IEex_Helper_GetOrCreateGlobalLock` / `LockGlobalDirect` / `UnlockGlobalDirect` | [NV] LockGlobalDirect("IEex_CScreenSpell_m_lPopupStack") → call original → UnlockGlobalDirect |
| 0x66B71E HookJumpToAutoReturn | inside `CScreenSpellbook::DismissPopup` (entry 0x66B6F0) | src/CScreenSpellbook.cpp:1317 (exact marker; decl follows) | same lock trio | [NV] Same global lock around popup dismissal |
| 0x66A59C HookRestore(6), 0x66A6F5 HookRestore(5) | inside `CScreenSpellbook::TimerSynchronousUpdate` (entry 0x66A580) | src/CScreenSpellbook.cpp:860 (exact marker; decl follows) | lock trio | [NV] Lock/Unlock on the two popup-stack paths in the timer |
| 0x62E560 HookJumpOnSuccess(0) | inside `CUIControlButtonInventorySlot::Render` (entry 0x62DDE0) | src/CScreenInventory.cpp:6051 (exact marker; decl follows) | `IEex_Helper_PostItemSlotRenderHook` | [NV] Learnable-scroll green overlay (B3TINTG.BAM) after the item slot renders |
| 0x681C3B HookJumpOnSuccess(3) | inside `CUIControlButtonStoreStoreItem::Render` (entry 0x6817A0) | src/CScreenStore.cpp:7306 (exact marker; decl follows) | `IEex_Helper_PostItemSlotRenderHook` | [NV] Same overlay for store buy list |
| 0x68278B HookJumpOnSuccess(3) | inside `CUIControlButtonStoreGroupItem::Render` (entry 0x6822F0) | src/CScreenStore.cpp:7616 (exact marker; decl follows) | `IEex_Helper_PostItemSlotRenderHook` | [NV] Same overlay for store sell/party list |
| 0x696623 HookJumpOnSuccess(3) | inside `CUIControlButtonWorldContainerSlot::Render` (entry 0x696140) | src/CScreenWorld.cpp:4917 (exact marker; decl follows) | `IEex_Helper_PostItemSlotRenderHook` | [NV] Same overlay for world container slots |
| 0x7AEAD0 HookReplaceFunctionMaintainOriginal(5) | `CVidCell::Render(INT nSurface, int x, int y, const CRect&, CVidPoly*, int, DWORD, int)` (exact) | src/CVidCell.cpp:861 (exact marker; decl follows) | `IEex_Helper_CVidCell_RenderIconOverride`, `IEex_Helper_DefineAddress` | [NV] Named "CVidCell::RenderIconOriginal"; the icon render goes through the helper; vanilla else-branch calls `DefineAddress("CVidCell::RenderIconOriginal", -1)` |
| 0x67E7E2 IEex_WriteAssembly (!jmp_byte) | inside `CScreenStore::CheckSpellUsable` (entry 0x67E560) | src/CScreenStore.cpp:5481 (exact marker; decl follows) | — | [NV] Elf store-raise fix |
| 0x61DE34 IEex_WriteAssembly (!jmp_byte) | inside `CUIControlButtonCharGenHairSkinAppearance::Render` (entry 0x61DAC0) | src/CScreenCreateChar.cpp:9076 (exact marker; decl follows) | `IEex_Helper_CUIControlButtonChargenAnimationPreview_TimerAsynchronousUpdateOverride` (installed into a vtable, below) | [NV] Turns the sync-thread advancement off during the chargen animation preview |
| 0x85CDF0 IEex_WriteDword (RData-protection-guarded) | `.rdata` vtable slot TimerAsynchronousUpdateOverride of the chargen animation preview button (region nearest CMarker::RECTICLE_XGAP) | src/CMarker.cpp:51 (data region) | `IEex_Helper_CUIControlButtonChargenAnimationPreview_TimerAsynchronousUpdateOverride` (via `IEex_Label`) | [NV] Slot ← helper label: the preview animates from its own timer override |
| 0x7A12C6 IEex_WriteAssembly (6 nops) | inside `CVidFont::TextOut3d` (entry 0x7A1210) — the `jne 0x7A1601` gate | src/CVidFont.cpp:942 (exact marker; decl follows) | — | [—] Doubled fonts: fork comment — the TextOutEx3d path re-uploads a 512×512 atlas per glyph per frame; `CVidFont::RenderCharacters` (0x7A1660 exact, src/CVidFont.cpp:1022) already doubles via nScale. GL-only effect (TextOut3d early-returns unless Is3dAccelerated) |
| 0x4D2822, 0x4D2828, 0x4D27F4, 0x4D27FA IEex_WriteAssembly (movsx rewrites) | inside `CUIPanel::CUIPanel(CUIManager*, UI_PANELHEADER*)` (entry 0x4D2750) | src/CUIPanel.cpp:13 (exact marker; decl follows) | — | [—] `movsx eax/ecx, word [edi+04/06]`: panel x/y become signed, allowing negative CHU panel coordinates |
| 0x79F6A0 IEex_WriteAssembly (`mov eax,0; ret`) | `CVidInf::RenderPointer` (exact; whole entry) | src/CVidInf.cpp:2161 (exact marker; decl follows) | — | [—] Cursor is not drawn via the non-flip path any more (the physical-cursor path owns it) |

## Cited but not hooked

- 0x76E93F — the custom-control jump target inside `CUIControlBase::CreateControl` (nearest marker src/CUIControlFactory.cpp:33, span 0x76D370..). It is a later label in the same function, not a separate function; we did not match a named decl for it.
- 0x7A1660 — resolved EXACT as `CVidFont::RenderCharacters` (src/CVidFont.cpp:1022); cited by a fork comment rather than hooked.
- The `.rdata` vtable-slot writes (0x85A3E8/0x85A3E4/0x85A45C, 0x853BE0, 0x852D24/0x854248/0x852D70/0x854294, 0x85D798, 0x85CDF0, 0x855AC8/0x85C770, 0x85C518) are data writes; their nearest markers are data-region entries (CScreenWorld::BORED_TIME src/CScreenWorld.cpp:24, AUTO_PICK_SPECIALIST_SPELLS src/CScreenCreateChar.cpp:133, CInfinity::RENDER_MESSAGESCREEN src/CInfinity.cpp:100, CMarker::RECTICLE_XGAP src/CMarker.cpp:51) — the slot's owning class is established by the fork's comments and the stub semantics, not by a marker in our tree.
- 0x4D4C20 (original NeedRender entry used by the flicker stub), 0x4D42B0 (`CUIManager::OnLButtonUp`, exact src/CUIManager.cpp:308 — noted by the fork as deliberately NOT wrapped: focused-branch only), 0x68C2C5/0x68C325/0x68C0D9/0x68C182/0x68C065/0x68C249/0x68C214/0x68C0A6/0x68C032/0x68C292/0x4D3FCC/0x4D343A/0x4D4497/0x4D522A/0x4D4C59/0x68DFC3/0x68DF04/0x79B34B/0x422198/0x4224CC/0x59162B/0x786DF8 — resume/tail-jump targets inside already-listed functions.

## Naming discrepancies (fork name vs our tree)

- 0x49FC40: fork comment "CScreenCreateChar::CheckMouseRButton"; our tree `CBaldurChitin::GetIDSChoosePixelFormat` (src/CBaldurChitin.cpp:1171, returns IDS_CHOOSE_PIXEL_FORMAT = 1 from compat/mfc_resource.h:6). Same constant-return shape; we keep our name and flag the fork's.
- 0x66D760: used by the fork as a generic OnLButtonDown target; our tree names it `CUIControlButtonSpellbookSpell::OnLButtonDown` (src/CScreenSpellbook.cpp:2176, decl follows) — the fork borrows a spellbook control's handler as the cooldown-free base behavior.
- CScreenKeys block: fork names (`InitDueToUserInteraction`, `OnKeyDownProcess`, `OnPanel1ButtonClick`, `SetSelectedDisplayIndex`, `SetStagedKeybind`, `UpdateHoverPalettes`, `UpdateLabelPalettes`, `InitHotkeyMap`, `UpdateKeybindMapping`) do not match our decls (`StartKeymaps`, `HandleKeyDown`, `OnErrorButtonClick`, `SelectKeymap`, `SetKeymap`, `sub_63A7A0`, `sub_63A660`, `LoadKeymap`, `SetKey`) — see table G for both.
- Store/inventory renderers: `CUIControlButtonStoreStoreItem` / `CUIControlButtonStoreGroupItem` / `CUIControlButtonInventorySlot` are our decl names; the fork's comments say "StoreItem" / "StorePartyItem" / "ItemSlot".

## Data flow

- State lives in three places: (1) Lua globals set by IEex_Gui_State.lua — `IEex_PortraitGridEnabled` and `IEEX_PORTRAIT_FRAMES_AVAILABLE` gate the [PG] block at install time; (2) INI keys read at install time — `[IEex Options] "UI Borders"` (default 1; 0 enables the MOS redirect) and `[IEex Options] "HUD Layer"` (read DLL-side by IEex_Helper_HudLayerRender as the fallback trigger); (3) DLL-side state behind the helpers — the HUD FBO/layer, the physical/logical cursor transform (WorldPtPhysicalize), the phantom band (s<1 logical vs physical rects), the session-fixed MOS redirect table, the tooltip-kill flag from IEex_Malloc(4), and the "IEex_CScreenSpell_m_lPopupStack" global lock.
- Per-frame readers: `CScreenWorld::TimerSynchronousUpdate` (BeforeWorldRender, HUD layer, DialogIdlePresent, scrollbar-focus override), `CGameArea::AIUpdate` (MouseInAreaViewport), `CUIManager::TimerAsynchronousUpdate` (panel-timer gate), the four CScreenWorld mouse handlers plus OnRButtonUp (physical + gated stubs), `CInfButtonArray::OnLButtonPressed`/`OnRButtonPressed` (actionbar externs), `CGameSprite::HandleEffects` and `sub_716770` (tooltip + spontaneous cast), `CUIControlButton*::Render` family (item-slot overlays, portrait cell, panel mosaic/dither).
- Writers: the installer itself (all the WriteAssembly/WriteDword slots above); Lua via the IEex_Extern_* callbacks (returns drive control flow — booleans gate, numbers become coordinates/rows, the FF/−1 sentinel convention means "no override, run original"); the DLL helpers write the persistent bits (highlight clear on KillFocus, popup lock counters, HUD layer contents).
- The popup lock is the only cross-screen mutex: spellbook popup open/close and the spellbook timer all serialize on it, so the stack count read by the flicker stub (+0x49C) is consistent.

## IEex_Helper_* entry points

Called from or installed into by this file (complete list; stream B1 seed — all names, including any absent from `ghidra-projects/ieexhelper-fork/dll-delta.tsv`):

- IEex_Helper_HudLayerRender
- IEex_Helper_IsHudLayerActive (comment-referenced; queried by the BeforeWorldRender fallback)
- IEex_Helper_DialogIdlePresent
- IEex_Helper_BlankBackBuffer
- IEex_Helper_BlankCCache1
- IEex_Helper_BlankCCache2
- IEex_Helper__CUIPanel_Render__CVidMosaic_Render
- IEex_Helper_GetOrCreateGlobalLock
- IEex_Helper_LockGlobalDirect
- IEex_Helper_UnlockGlobalDirect
- IEex_Helper_PostItemSlotRenderHook
- IEex_Helper_CVidCell_RenderIconOverride
- IEex_Helper_DefineAddress
- IEex_Helper_CUIControlButtonChargenAnimationPreview_TimerAsynchronousUpdateOverride (installed into a vtable via IEex_Label)
- IEex_Helper_CInfCursor_SetToolTipOverride (whole-entry override target)
- IEex_Helper_CInfGame_InitHotkeyMapOverride
- IEex_Helper_CInfGame_UpdateKeybindMappingOverride
- IEex_Helper_CScreenKeys_InitDueToUserInteractionOverride
- IEex_Helper_CScreenKeys_OnKeyDownProcessOverride
- IEex_Helper_CScreenKeys_OnLButtonDownOverride
- IEex_Helper_CScreenKeys_OnPanel1ButtonClickOverride
- IEex_Helper_CScreenKeys_SetSelectedDisplayIndexOverride
- IEex_Helper_CScreenKeys_SetStagedKeybindOverride
- IEex_Helper_CScreenKeys_UpdateHoverPalettesOverride
- IEex_Helper_CScreenKeys_UpdateLabelPalettesOverride
- IEex_Helper_CScreenKeys_UpdateMainPanelOverride
- IEex_Helper_CUIControlButtonKeys_OnLButtonClickOverride
- IEex_Helper_RenderPortraitRect
- IEex_Helper_PanelMosaicRender
- IEex_Helper_PanelRClickRefonte
- IEex_Helper_PortraitButtonCellRender
- IEex_Helper_HudPanelRenderDither
- IEex_Helper_FormationRDown
- IEex_Helper_FormationRUp
- IEex_Helper_ActionDownPhys
- IEex_Helper_ActionUpPhys
- IEex_Helper_ActionDblClkPhys
- IEex_Helper_MouseMovePhys
- IEex_Helper_UIMouseMoveGated
- IEex_Helper_UILDownGated
- IEex_Helper_UIDblClkGated
- IEex_Helper_UIRDownGated
- IEex_Helper_PanelTimerGated
- IEex_Helper_ButtonKillFocus

Core hook-API (not DLL-feature helpers) also used: `IEex_Label`, `IEex_Malloc`, `IEex_WriteLString`, `IEex_CheckCallError`, `IEex_GetLuaState`, `IEex_TooltipKillReason` (Lua table), plus the hook primitives (IEex_HookJump, HookBeforeCall, HookAfterCall, HookRestore, HookAfterRestore, HookReturnNOPs, HookJumpNoReturn, HookJumpOnSuccess, HookJumpToAutoReturn, HookBeforeAndAfterCall, AttemptHook, WriteAssembly, WriteAssemblyAuto, WriteDword) and the assembler macros (!push/pop_registers_iwd2, !push/pop_state, !push/pop_complete_state, !mark_esp/!marked_esp, !jmp_dword, !repeat, !ret_word).

Lua-side callbacks installed here for the DLL to call (`IEex_Extern_*`, defined in the companion state/extern files): CUIControlBase_CreateControl, CUIManager_fInit_CHUInitialized, CUIControlButtonWorldContainerSlot_GetActiveContainerID, CUIControlButtonWorldContainerSlot_GetActiveContainerSpriteID, CUIControlButtonWorldContainerSlot_GetContainerItemIndex, CUIControlButtonWorldContainerSlot_OnLButtonClick_GetOnlyUpdateSlot, CUIControlButtonWorldContainerSlot_OnLButtonClick_Done, CUIControlButtonWorldContainerSlot_OnRButtonClick, SkillsPlusMinus_OnRButtonClick, CScreenWorld_AsynchronousUpdate, CScreenWorld_OnInventoryButtonRClick, GetHighlightContainerID, OnUpdateRecordDescription, OnOptionsScreenESC, BeforeWorldRender, MouseInAreaViewport, RejectGetWorldCoordinates, OverrideWorldScreenScrollbarFocus, OnSetActionbarState, OnActionbarUnhandledRButtonClick, RejectWorldScreenEsc, InitResolution, CheckBitDepth, InitGUIConstants, InitHighResolutionPaddingPanels, StartDebugConsole, StopDebugConsole, AdjustAutoScrollY, ShouldTooltipRefreshInsteadOfDying, AttemptSpontaneousCast, GetSpontaneousCastColumnAndRow.
