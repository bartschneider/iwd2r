# IEex_Helper_GetBridge

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10010350` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10010350.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Creature_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_IWD2_State.lua`, `IEex_Key_State.lua`, `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x100103ed) */
/* WARNING: Removing unreachable block (ram,0x100103fe) */

undefined4 FUN_10010350(void)

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
  FUN_10017b50();
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

`IEex_GetBridge(bridge, [path...])` pushes the value stored at the path.
Wrapper FUN_10010350: decode handle, Plain (no suffix) variant: FUN_10013f50 two-lock step, shared:
blocking-acquire-shared handle+0, then TryAcquireShared handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0.
inner FUN_10017b50 resolves the path and dispatches on the LuaData tag:
0 -> lua_pushnil; 1 -> lua_pushboolean; 3 -> lua_pushinteger; 4 ->
lua_pushstring (SBO string, inline up to 0xf chars); 5 ->
lua_pushlightuserdata (a sub-bridge handle). Ref-counted values are
bumped/dropped under LOCK()-ed sections while pushing. A missing leaf pushes
nil; an unknown tag runs the error path and logs "[GetBridge] Pushing nil"
before lua_pushnil. Returns 1 (one value pushed). The impl carries a
"Type propagation algorithm not settling" decompiler warning -- the tag
switch above is read from the recovered dispatch, not from clean C. No GL
calls.

##### Failure fallbacks

Missing key -> nil; unknown tag -> logged + nil. A numeric bridge id is
rejected at decode with the "[GetBridge] Attempting to use number as bridge"
log.

