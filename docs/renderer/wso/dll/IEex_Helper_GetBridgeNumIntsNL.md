# IEex_Helper_GetBridgeNumIntsNL

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100105c0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_100105c0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Creature_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_100105c0(undefined4 param_1)

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
  lua_pushinteger(param_1,uRam00000010);
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

`IEex_GetBridgeNumIntsNL(bridge, [path...])` pushes the entry count of the
**integer-keyed** map of the container at the path. Wrapper FUN_100105c0:
decode handle, NL variant: a single Acquire/Release pair on handle+4 -- AcquireSRWLockShared (FUN_1005822a), run, ReleaseSRWLockShared (FUN_10058268). No retry loop, and the handle+0 lock is not taken. resolve the path (FUN_100155e0),
push the count with lua_pushinteger, return 1.

##### Decompiler artifact

The pushed value decompiles as `lua_pushinteger(param_1, uRam00000010)` --
uRam00000010 is the stack slot where FUN_100155e0 left the resolved
container pointer; Ghidra failed to bind the trailing field read. The count
itself is the `+8` size field of the integer-keyed map region (compare
FUN_1001a560). Treat the exact read as unverified. No GL calls.

##### Failure fallbacks

None visible; a bad path key type logs a traceback from FUN_100155e0.

