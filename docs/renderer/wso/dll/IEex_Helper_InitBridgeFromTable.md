# IEex_Helper_InitBridgeFromTable

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100107d0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_100107d0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_Key_State.lua`, `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x10010873) */
/* WARNING: Removing unreachable block (ram,0x10010884) */

undefined4 FUN_100107d0(void)

{
  undefined1 uVar1;
  char cVar2;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005ab15;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  uVar1 = FUN_1000fe00(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  local_8 = 1;
  do {
    cVar2 = FUN_10013e90();
    if (cVar2 == '\0') break;
    cVar2 = FUN_10013e90();
  } while (cVar2 != '\0');
  FUN_10018880(uVar1);
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

`IEex_InitBridgeFromTable(bridge, value)` fills a bridge from a Lua value.
Wrapper FUN_100107d0: decode handle, Plain (no suffix) variant: FUN_10013e90 two-lock step, exclusive:
blocking-acquire handle+0, then TryAcquireExclusive handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0. then inner
FUN_10018880, which consumes the stack value:

- flag from the decoder == 0 -> lua_createtable, then run the LuaData
  serializer FUN_10016dc0 over it and lua_setfield it onto the bridge.
- stack value is a table (type 5) -> lua_pushvalue + lua_setfield (stored by
  reference), then FUN_10016440(2) trims the stack.
- stack value is a function (type 6) -> store it, lua_pcall it (error ->
  lua_tolstring + log "%s" via the host log pointer DAT_100944fc), lua_getfield the
  result back and convert with FUN_10016440, then lua_settop.

Other types fall through and do nothing. Returns 0 to Lua. No GL calls.

##### Decompiler artifact

FUN_10018880's Lua-stack arguments are register-passed and invisible in the
decompile; the branches above are recovered from the lua_type/lua_pcall call
shape, so treat the exact stack discipline as approximate.

