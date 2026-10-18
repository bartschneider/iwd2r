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

## Lua-side names that are not DLL helpers

- `IEex_DefineAddress` / `IEex_SupressScrollCheck` are implemented purely in the staged Lua patches; there is no `IEexHelper.fork.dll` export or registration behind them, so they have no note here.

## Registered but not referenced by staged Lua

The DLL registers more names than the patches call; these are out of scope for B1 and have no notes (see `registry-map.tsv` in the B1 scratch for the full registration list).
