# IEex_Helper_GetBridgeDirect

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1001a170` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1001a170.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_1001a170(void)

{
  undefined4 uVar1;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005aadd;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  FUN_10016a80(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  FUN_1005822a(4);
  local_8 = 0;
  uVar1 = FUN_1001a030();
  FUN_10058268(4);
  ExceptionList = local_10;
  return uVar1;
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

`IEex_GetBridgeDirect(name)` is the native-side fast path: it takes the
bridge **name string** (ECX/in_ECX -- there is no lua_State), runs
FUN_10016a80 (registry lookup-or-create, unlocked), acquires
AcquireSRWLockShared on handle+4 (FUN_1005822a), and FUN_1001a030 does a
single string-key lookup: std::string + FNV-1a + FUN_1001c0e0 over the
string-keyed map at +0x2c; if the node's LuaData (node+0x20) has tag 3
(integer) its value is returned (with a LOCK()-ed ref-count bump of the
stored object); otherwise (or on a miss) it returns 0. The wrapper
FUN_1001a170 returns the integer directly -- when exposed to Lua this comes
back as a number without ever touching the Lua stack. Used by native
render-side code that needs one integer out of a bridge without building a
path. No GL calls.

##### Failure fallbacks

Missing key, or a stored value that is not an integer, returns 0 -- callers
cannot distinguish "absent" from "zero".

