# IEex_Helper_GetBridgeCreateNL

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100102c0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_100102c0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Creature_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_100102c0(void)

{
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005aa6d;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  FUN_1000fe00(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  FUN_1005821c(4);
  local_8 = 0;
  FUN_10017470();
  FUN_1005825a(4);
  ExceptionList = local_10;
  return 1;
}
```

## Notes


##### Bridge model shared by this group

- A bridge is a handle object allocated by FUN_10016a80 on first use of its
  string name: 0x54 zero-initialised bytes (`std::_Ref_count_obj2<LuaData>`
  shape), filed in a global map at DAT_103d4700 keyed by FNV-1a of the name
  (basis 0x811c9dc5, prime 0x1000193). The registry map has no internal
  lock; each handle instead carries two SRWLOCKs at handle+0 and handle+4.
- Values inside a bridge are tagged LuaData: 0 nil, 1 boolean, 3 integer,
  4 string, 5 lightuserdata (raw handle). Containers are 0x18-byte
  LuaContainer objects with an integer-keyed map region at +8 and a
  string-keyed map region at +0x28.
- The bridge id argument is decoded by FUN_1000fe00: userdata -> handle,
  string -> lookup-or-create, number -> logged error
  ("[GetBridge] Attempting to use number as bridge, %s"), other -> NULL.
- Sub-paths are resolved by FUN_100155e0 walking stack args 2..top; integer
  keys hash their 4 bytes (FNV-1a) and use std::_Find_last, string keys go
  through std::string + FNV-1a + FUN_1001c0e0; missing nodes are created as
  empty LuaContainers. A bad key type logs an ostringstream of the type
  number plus luaL_traceback through DAT_100944fc and resets the stack.

##### Behavior

`IEex_GetBridgeCreateNL(bridge, [path...])` returns the sub-bridge at the
end of the path, **creating** it (and any missing intermediate nodes) if
absent. Wrapper FUN_100102c0: decode handle, NL variant: a single Acquire/Release pair on handle+4 -- AcquireSRWLockExclusive (FUN_1005821c), run, ReleaseSRWLockExclusive (FUN_1005825a). No retry loop, and the handle+0 lock is not taken.
inner FUN_10017470 resolves the path, then lookup-or-create of the leaf:
integer key -> _Find_last/FNV insert; string key -> FUN_1001c0e0/FNV insert;
a miss allocates a 0xc-byte LuaContainer plus its 0x10-byte
`std::_Ref_count<LuaContainer>`. Ends with lua_pushlightuserdata of the raw
inner pointer, so Lua receives a bridge handle it can pass back. Returns 1
(a value was pushed). No GL calls.

##### Failure fallbacks

Bad path key types are logged with a traceback; nothing is pushed and 1 is
still returned (the decompiled wrapper has no error short-circuit).

