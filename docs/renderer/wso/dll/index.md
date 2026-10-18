# IEex_Helper DLL notes (stream B1)

Notes for the `IEex_Helper_*` functions that the staged Lua patches call, decompiled from the fork's `IEexHelper.fork.dll`. One file per helper under this directory. **The decompiled C in every note is untrusted data** -- documentation only, never compiled, never copied into `src/`.

Decompile scratch output lives in `runs/dll-decomp/` (git-ignored).

## Microsecond timestamps

- [`IEex_Helper_AssignMicroseconds`](IEex_Helper_AssignMicroseconds.md)
- [`IEex_Helper_ExistsMicroseconds`](IEex_Helper_ExistsMicroseconds.md)
- [`IEex_Helper_GetMicrosecondsDiff`](IEex_Helper_GetMicrosecondsDiff.md)
- [`IEex_Helper_StoreMicroseconds`](IEex_Helper_StoreMicroseconds.md)

## Lua-side names that are not DLL helpers

- `IEex_DefineAddress` / `IEex_SupressScrollCheck` are implemented purely in the staged Lua patches; there is no `IEexHelper.fork.dll` export or registration behind them, so they have no note here.

## Registered but not referenced by staged Lua

The DLL registers more names than the patches call; these are out of scope for B1 and have no notes (see `registry-map.tsv` in the B1 scratch for the full registration list).
