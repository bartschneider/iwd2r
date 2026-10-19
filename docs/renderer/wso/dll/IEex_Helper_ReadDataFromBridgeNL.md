# IEex_Helper_ReadDataFromBridgeNL

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10010e90` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10010e90.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_IWD2_State.lua`, `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10010e90(undefined4 param_1)

{
  undefined4 uVar1;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005aadd;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  FUN_1000fe00(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  FUN_1005822a(4);
  local_8 = 0;
  uVar1 = lua_gettop(param_1);
  FUN_100155e0(uVar1);
  lua_createtable(param_1,0,0);
  uVar1 = lua_gettop(param_1);
  FUN_10016dc0(uVar1);
  FUN_10058268(4);
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

Same deep copy as IEex_ReadDataFromBridge (wrapper FUN_10010e90, same
FUN_100155e0 + lua_createtable + FUN_10016dc0 sequence) but NL variant: a single Acquire/Release pair on handle+4 -- AcquireSRWLockShared (FUN_1005822a), run, ReleaseSRWLockShared (FUN_10058268). No retry loop, and the handle+0 lock is not taken. This is the render-thread-safe read: it never
waits on a writer, at the cost of returning a snapshot that may already be
stale. Returns 1. No GL calls.

##### Failure fallbacks

As ReadDataFromBridge.

