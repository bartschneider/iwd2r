# IEex_Helper_SynchronizedBridgeOperation

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011af0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011af0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Creature_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_IWD2_State.lua`, `IEex_Key_State.lua`, `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011af0(undefined4 param_1)

{
  code *pcVar1;
  int iVar2;
  undefined4 uVar3;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005aa6d;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  FUN_1000fe00(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  FUN_1005821c(0);
  local_8 = 0;
  lua_getfield(param_1,0xffffd8ee,"debug");
  lua_getfield(param_1,0xffffffff,"traceback");
  lua_pushvalue(param_1,2);
  lua_pushlightuserdata(param_1,0);
  iVar2 = lua_pcall(param_1,1,0,0xfffffffd);
  pcVar1 = DAT_100944fc;
  if (iVar2 == 0) {
    lua_settop(param_1,0xfffffffd);
  }
  else {
    uVar3 = lua_tolstring(param_1,0xffffffff,0);
    (*pcVar1)(&DAT_100642cc,1,uVar3);
    lua_settop(param_1,0xfffffffc);
  }
  FUN_1005825a(0);
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

`IEex_SynchronizedBridgeOperation(bridge, func)` runs `func` **while holding
the bridge's exclusive lock**. Wrapper FUN_10011af0: decode handle
(FUN_1000fe00), NL variant: a single Acquire/Release pair on handle+4 -- AcquireSRWLockExclusive (FUN_1005821c), run, ReleaseSRWLockExclusive (FUN_1005825a). No retry loop, and the handle+0 lock is not taken. then inside the lock:
lua_getfield of the "debug" table (literals at 0x100642ba/0x100642c1) and
its "traceback" function, lua_pushvalue of the Lua function argument,
lua_pushlightuserdata(0), lua_pcall(1, 0, traceback). On success the stack
is trimmed with lua_settop(-3); on error the message is read with
lua_tolstring(-1) and logged as "%s" through DAT_100944fc, then lua_settop
(-4). Returns 0 to Lua. This is the escape hatch the staged patches use for
multi-step read-modify-write sequences that the single Set/Get calls cannot
express atomically. No GL calls.

##### Decompiler artifact

The stack index for the debug-table getfield decompiles as 0xffffd8ee (an
upvalue/registry offset); the pcall discipline above is read from the call
shape, not clean C.

