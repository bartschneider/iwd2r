# IEex_Helper_IterateBridge

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10010ab0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10010ab0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x10010b4d) */
/* WARNING: Removing unreachable block (ram,0x10010b5e) */

undefined4 FUN_10010ab0(void)

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
  FUN_100189d0();
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

`IEex_IterateBridge(bridge, [path...], callback)` invokes `callback(key,
value)` for every entry of the container at the path. Wrapper FUN_10010ab0:
decode handle, Plain (no suffix) variant: FUN_10013f50 two-lock step, shared:
blocking-acquire-shared handle+0, then TryAcquireShared handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0. inner FUN_100189d0 resolves
the path, then walks the integer-keyed map (keys pushed with
lua_pushinteger) followed by the string-keyed map (keys pushed with
lua_pushstring). Each entry: the callback from the stack is pushed, then the
key, then the value through the standard tag switch (0 nil / 1 boolean /
3 integer / 4 string / 5 lightuserdata; ref-count bump under LOCK while
pushing; unknown tag -> ostringstream of the tag + luaL_traceback +
log call + lua_pushnil). lua_pcall(2, 1, 0) runs the callback; a pcall
error is logged ("%s" via DAT_100944fc) and iteration continues; a **truthy
callback return stops the iteration early**. The impl carries a "Type
propagation algorithm not settling" warning. Returns 0 to Lua. No GL calls.

##### Failure fallbacks

Per-entry errors are logged and skipped; the callback is the only failure
surface.

