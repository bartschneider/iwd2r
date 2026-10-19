# IEex_Helper_GetBridgePtr

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10010660` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10010660.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x100106fd) */
/* WARNING: Removing unreachable block (ram,0x1001070e) */

undefined4 FUN_10010660(void)

{
  char cVar1;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005aaa5;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  FUN_1000fe00(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  local_8 = 1;
  do {
    cVar1 = FUN_10013f50();
    if (cVar1 == '\0') break;
    cVar1 = FUN_10013f50();
  } while (cVar1 != '\0');
  FUN_100183b0();
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

`IEex_GetBridgePtr(bridge, [path...,] key)` returns the **raw pointer value**
stored at the key as a Lua integer. Wrapper FUN_10010660: decode handle,
Plain (no suffix) variant: FUN_10013f50 two-lock step, shared:
blocking-acquire-shared handle+0, then TryAcquireShared handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0. inner FUN_100183b0 resolves the path, does an
integer- or string-key lookup (other key types -> error path: ostringstream
of the type number + traceback + LogVariadic), and ends with
lua_pushinteger of the stored pointer. The impl carries a "Type propagation
algorithm not settling" warning (recovered around the switch, same shape as
GetBridge). Returns 1. No GL calls.

##### Failure fallbacks

Missing key or unknown tag -> error path; the value pushed is 0/nil-ish (the
decompile is mangled by the type-propagation warning -- flag to the
implementer: treat the not-found return as unverified).

