# IEex_Helper_EraseBridgeKey

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10010070` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10010070.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Creature_State.lua`, `IEex_IWD2_State.lua`, `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x1001010d) */
/* WARNING: Removing unreachable block (ram,0x1001011e) */

undefined4 FUN_10010070(void)

{
  char cVar1;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005aa05;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  FUN_1000fe00(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  local_8 = 1;
  do {
    cVar1 = FUN_10013e90();
    if (cVar1 == '\0') break;
    cVar1 = FUN_10013e90();
  } while (cVar1 != '\0');
  FUN_10016fc0();
  ExceptionList = local_10;
  return 0;
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

`IEex_EraseBridgeKey(bridge, [path...,] key)` deletes a single key.
Wrapper FUN_10010070: decode handle, Plain (no suffix) variant: FUN_10013e90 two-lock step, exclusive:
blocking-acquire handle+0, then TryAcquireExclusive handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0. then
FUN_10016fc0: resolve the path (args up to gettop-1), then erase the final
key -- integer keys via FUN_1001a760, string keys via hash-bucket unlink
(FUN_1001c8b0) and a single node free (0x28 bytes); any other key type takes
the error path (ostringstream of the type number, luaL_traceback,
log call through the host log pointer DAT_100944fc, lua_settop(L,-2)). Returns 0 to Lua. No GL
calls.

##### Failure fallbacks

Erasing a missing key is a no-op; a bad key type is logged and the call
returns without touching the bridge.

