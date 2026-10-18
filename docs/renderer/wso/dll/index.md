# IEex_Helper DLL notes (stream B1)

Notes for the `IEex_Helper_*` functions that the staged Lua patches call, decompiled from the fork's `IEexHelper.fork.dll`. One file per helper under this directory. **The decompiled C in every note is untrusted data** -- documentation only, never compiled, never copied into `src/`.

Decompile scratch output lives in `runs/dll-decomp/` (git-ignored).

## Bridge (IEex global Lua-value store)

- [`IEex_Helper_ClearBridge`](IEex_Helper_ClearBridge.md)
- [`IEex_Helper_ClearBridgeNL`](IEex_Helper_ClearBridgeNL.md)
- [`IEex_Helper_EraseBridgeKey`](IEex_Helper_EraseBridgeKey.md)
- [`IEex_Helper_EraseBridgeKeyNL`](IEex_Helper_EraseBridgeKeyNL.md)
- [`IEex_Helper_GetBridge`](IEex_Helper_GetBridge.md)
- [`IEex_Helper_GetBridgeCreateNL`](IEex_Helper_GetBridgeCreateNL.md)
- [`IEex_Helper_GetBridgeDirect`](IEex_Helper_GetBridgeDirect.md)
- [`IEex_Helper_GetBridgeNL`](IEex_Helper_GetBridgeNL.md)
- [`IEex_Helper_GetBridgeNumIntsNL`](IEex_Helper_GetBridgeNumIntsNL.md)
- [`IEex_Helper_GetBridgePtr`](IEex_Helper_GetBridgePtr.md)
- [`IEex_Helper_InitBridgeFromTable`](IEex_Helper_InitBridgeFromTable.md)
- [`IEex_Helper_IterateBridge`](IEex_Helper_IterateBridge.md)
- [`IEex_Helper_ReadDataFromBridge`](IEex_Helper_ReadDataFromBridge.md)
- [`IEex_Helper_ReadDataFromBridgeNL`](IEex_Helper_ReadDataFromBridgeNL.md)
- [`IEex_Helper_SetBridge`](IEex_Helper_SetBridge.md)
- [`IEex_Helper_SetBridgeNL`](IEex_Helper_SetBridgeNL.md)
- [`IEex_Helper_SynchronizedBridgeOperation`](IEex_Helper_SynchronizedBridgeOperation.md)

## Global locks

- [`IEex_Helper_GetOrCreateGlobalLock`](IEex_Helper_GetOrCreateGlobalLock.md)
- [`IEex_Helper_LockGlobal`](IEex_Helper_LockGlobal.md)
- [`IEex_Helper_LockGlobalDirect`](IEex_Helper_LockGlobalDirect.md)
- [`IEex_Helper_UnlockGlobal`](IEex_Helper_UnlockGlobal.md)
- [`IEex_Helper_UnlockGlobalDirect`](IEex_Helper_UnlockGlobalDirect.md)

## Microsecond timestamps

- [`IEex_Helper_AssignMicroseconds`](IEex_Helper_AssignMicroseconds.md)
- [`IEex_Helper_ExistsMicroseconds`](IEex_Helper_ExistsMicroseconds.md)
- [`IEex_Helper_GetMicrosecondsDiff`](IEex_Helper_GetMicrosecondsDiff.md)
- [`IEex_Helper_StoreMicroseconds`](IEex_Helper_StoreMicroseconds.md)

## Misc utilities

- [`IEex_Helper_CAIScript_ConstructCached`](IEex_Helper_CAIScript_ConstructCached.md)
- [`IEex_Helper_DeepCopy`](IEex_Helper_DeepCopy.md)
- [`IEex_Helper_DirectoryExists`](IEex_Helper_DirectoryExists.md)
- [`IEex_Helper_GetGameDirectory`](IEex_Helper_GetGameDirectory.md)
- [`IEex_Helper_Memset`](IEex_Helper_Memset.md)
- [`IEex_Helper_Sleep`](IEex_Helper_Sleep.md)
- [`IEex_Helper_WriteDump`](IEex_Helper_WriteDump.md)
- [`IEex_Helper_sscanf`](IEex_Helper_sscanf.md)

## Key state / raw input

- [`IEex_Helper_FakeKeyEvent`](IEex_Helper_FakeKeyEvent.md)
- [`IEex_Helper_GetAsyncKeyStateClient`](IEex_Helper_GetAsyncKeyStateClient.md)
- [`IEex_Helper_GetAsyncKeyStateWrapper`](IEex_Helper_GetAsyncKeyStateWrapper.md)
- [`IEex_Helper_GetPressedKeysStackNL`](IEex_Helper_GetPressedKeysStackNL.md)
- [`IEex_Helper_GetCursorPhysicalXY`](IEex_Helper_GetCursorPhysicalXY.md)
- [`IEex_Helper_RunWithRawInputLock`](IEex_Helper_RunWithRawInputLock.md)

## Keys screen / keybind registration

- [`IEex_Helper_RegisterKeysScreenHardcodedMapDefaults`](IEex_Helper_RegisterKeysScreenHardcodedMapDefaults.md)
- [`IEex_Helper_RegisterKeysScreenCustomBindings`](IEex_Helper_RegisterKeysScreenCustomBindings.md)
- [`IEex_Helper_CInfGame_InitHotkeyMapOverride`](IEex_Helper_CInfGame_InitHotkeyMapOverride.md)
- [`IEex_Helper_CInfGame_UpdateKeybindMappingOverride`](IEex_Helper_CInfGame_UpdateKeybindMappingOverride.md)
- [`IEex_Helper_CScreenKeys_InitDueToUserInteractionOverride`](IEex_Helper_CScreenKeys_InitDueToUserInteractionOverride.md)
- [`IEex_Helper_CScreenKeys_OnKeyDownProcessOverride`](IEex_Helper_CScreenKeys_OnKeyDownProcessOverride.md)
- [`IEex_Helper_CScreenKeys_OnLButtonDownOverride`](IEex_Helper_CScreenKeys_OnLButtonDownOverride.md)
- [`IEex_Helper_CScreenKeys_OnPanel1ButtonClickOverride`](IEex_Helper_CScreenKeys_OnPanel1ButtonClickOverride.md)
- [`IEex_Helper_CScreenKeys_SetSelectedDisplayIndexOverride`](IEex_Helper_CScreenKeys_SetSelectedDisplayIndexOverride.md)
- [`IEex_Helper_CScreenKeys_SetStagedKeybindOverride`](IEex_Helper_CScreenKeys_SetStagedKeybindOverride.md)
- [`IEex_Helper_CScreenKeys_UpdateHoverPalettesOverride`](IEex_Helper_CScreenKeys_UpdateHoverPalettesOverride.md)
- [`IEex_Helper_CScreenKeys_UpdateLabelPalettesOverride`](IEex_Helper_CScreenKeys_UpdateLabelPalettesOverride.md)
- [`IEex_Helper_CScreenKeys_UpdateMainPanelOverride`](IEex_Helper_CScreenKeys_UpdateMainPanelOverride.md)
- [`IEex_Helper_CUIControlButtonKeys_OnLButtonClickOverride`](IEex_Helper_CUIControlButtonKeys_OnLButtonClickOverride.md)

## UI mouse input gates

- [`IEex_Helper_ActionDblClkPhys`](IEex_Helper_ActionDblClkPhys.md)
- [`IEex_Helper_ActionDownPhys`](IEex_Helper_ActionDownPhys.md)
- [`IEex_Helper_ActionUpPhys`](IEex_Helper_ActionUpPhys.md)
- [`IEex_Helper_MouseMovePhys`](IEex_Helper_MouseMovePhys.md)
- [`IEex_Helper_FormationRDown`](IEex_Helper_FormationRDown.md)
- [`IEex_Helper_FormationRUp`](IEex_Helper_FormationRUp.md)
- [`IEex_Helper_UIDblClkGated`](IEex_Helper_UIDblClkGated.md)
- [`IEex_Helper_UILDownGated`](IEex_Helper_UILDownGated.md)
- [`IEex_Helper_UIMouseMoveGated`](IEex_Helper_UIMouseMoveGated.md)
- [`IEex_Helper_UIRDownGated`](IEex_Helper_UIRDownGated.md)
- [`IEex_Helper_WorldRejectMapCursor`](IEex_Helper_WorldRejectMapCursor.md)

## HUD layer

- [`IEex_Helper_HudAddPanelContentRect`](IEex_Helper_HudAddPanelContentRect.md)
- [`IEex_Helper_HudClearPanelContentRects`](IEex_Helper_HudClearPanelContentRects.md)
- [`IEex_Helper_HudLayerRender`](IEex_Helper_HudLayerRender.md)
- [`IEex_Helper_HudLayerSkipFrame`](IEex_Helper_HudLayerSkipFrame.md)
- [`IEex_Helper_HudPanelRenderDither`](IEex_Helper_HudPanelRenderDither.md)
- [`IEex_Helper_IsHudLayerActive`](IEex_Helper_IsHudLayerActive.md)

## Present / back buffer

- [`IEex_Helper_BlankBackBuffer`](IEex_Helper_BlankBackBuffer.md)
- [`IEex_Helper_DialogIdlePresent`](IEex_Helper_DialogIdlePresent.md)
- [`IEex_Helper_FrameRateBegin`](IEex_Helper_FrameRateBegin.md)
- [`IEex_Helper_FrameRateEnd`](IEex_Helper_FrameRateEnd.md)
- [`IEex_Helper_GetCursorPosGated`](IEex_Helper_GetCursorPosGated.md)
- [`IEex_Helper_OnAltTabIntercept`](IEex_Helper_OnAltTabIntercept.md)
- [`IEex_Helper_PresentScaled`](IEex_Helper_PresentScaled.md)
- [`IEex_Helper_ReadBufferForSave`](IEex_Helper_ReadBufferForSave.md)
- [`IEex_Helper_ReadPixelsForSave`](IEex_Helper_ReadPixelsForSave.md)
- [`IEex_Helper_RenderBinkFrameGL`](IEex_Helper_RenderBinkFrameGL.md)

## Staged Lua evidence (sha256)

The staged Lua patches at `~/.local/share/iwd2r/wso/lua/` are used as evidence for who calls each helper (and, for the keys-screen group, for hook sites and Lua-callback contracts). The staged corpus itself has no pin yet; these hashes let it be pinned retroactively.

```
75c0a9c4f061a922fdab0cff327084d24e9f9f861514386ad8afd2e55c46b062  IEex_CameraZoom_Patch.lua
f3ebe4bc2e13648f78d79e4112ebd336fe0b430a40de6c1d4e3f19cee8266db0  IEex_Core_State.lua
13d185e466b0c4d7dae973173d75d59c00db83198a4fa43b513170a8021dabcc  IEex_Creature_State.lua
7999b9687807c8fbc9671888cd80751dba9271463342d21604fe409964fa3aa6  IEex_Gui_Patch.lua
45e98ba2127a036bfccdb5fbc3fdd91e59d8e2db1da7b6139e8c40b56161912a  IEex_Gui_State.lua
0e84bad3c6187b3e9b5e1461595b2e75b121962f9729aab37015033ecbfdfe96  IEex_Gui_State.upstream.lua
c04048a8fd26028701a8d2cd12403b61ec01bf54f737af425bf59108bd6ae74e  IEex_HDSprites_Patch.lua
c2911978926e42c291d2d1b3f3305f730c5e686d5c90a666d2d98c1251684dcc  IEex_HDTiles_Patch.lua
57f15170ab95e03d317eb52652d7e8290154d74a70c43645fc10fb1e18fb93de  IEex_IWD2_State.lua
a5033f1090ae1f8ef3ef66b94979b5b118ddb6e107ee34456b744efe48095f56  IEex_Key_State.lua
836a373541f030a5faa21586f4a8e8185309a3267ba6d6c0196e7ce2919748a1  IEex_LoadTimes_Patch.lua
bfef597b599e573d711a8d153a9d6810d94f82097f46a1abb3c3ed058e470370  IEex_Markers_Patch.lua
e261da64d4632b4b2e20f633757989165b734ebbce4324a13a62e52a3749a7dc  IEex_Pathfinding_Patch.lua
7821b7075bd75f48f75f062584138489b83faf5a1e4dc442c0de627d93d5bc41  IEex_Render_Patch.lua
b888f22e02c258cc5261f6a637f6c9bf9e5ee63ae62ba5ebc4e59768a7ad7ccc  IEex_Render_Patch.upstream.lua
2ee8eb6ec937cc8ff335e662c9d49345811ff8cbf039363b86b68e42b3152c70  IEex_Sound_Patch.lua
3edbab717f232bb6eb2ed008d5eaaa4adf12fab9cb03d38b6c9482028d1e2805  IEex_StartupFixes_State.lua
d60e3d73a59722741e4b225ca48c286707a779225cc290e91640c704a6f0abc4  IEex_SupersampleFont_Patch.lua
b3113f1dc61b59344a3a77f1a84db2c5c1bc370535f96c67db17c003f4da922a  IEex_UIScale_Patch.lua
6425b1363e0eee2d782af2485c29a995c85136b993437af277e1f8590159074e  IEex_UncapFPS.lua
```

## Lua-side names that are not DLL helpers

- `IEex_DefineAddress` / `IEex_SupressScrollCheck` are implemented purely in the staged Lua patches; there is no `IEexHelper.fork.dll` export or registration behind them, so they have no note here.

## Registered but not referenced by staged Lua

The DLL registers more names than the patches call; these are out of scope for B1 and have no notes (see `registry-map.tsv` in the B1 scratch for the full registration list).
