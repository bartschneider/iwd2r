# IEex_Helper_ClearBridge

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1000fed0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_1000fed0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Creature_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_IWD2_State.lua`, `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x1000ff8f) */
/* WARNING: Removing unreachable block (ram,0x1000ffa0) */

undefined4 FUN_1000fed0(undefined4 param_1)

{
  char cVar1;
  undefined4 uVar2;
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
  uVar2 = lua_gettop(param_1);
  FUN_100155e0(uVar2);
  FUN_1001a700();
  FUN_1001a560();
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

`IEex_ClearBridge(bridge, [path...])` removes **every key** from the bridge
(or sub-bridge at the end of the path). Wrapper FUN_1000fed0: decode the
handle (FUN_1000fe00), Plain (no suffix) variant: FUN_10013e90 two-lock step, exclusive:
blocking-acquire handle+0, then TryAcquireExclusive handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0. then resolve the path
(FUN_100155e0 over args 2..top), clear the string-keyed map at container+0x28
with FUN_1001a700 and the integer-keyed map at container+8 with FUN_1001a560.
Both clear helpers follow the MSVC `unordered_map::clear` shape: below a
load-factor threshold they free the node list in one pass (FUN_1001bd30 /
FUN_1001bb80), otherwise they unlink bucket by bucket (FUN_1001c8b0 + free
of 0x28-byte nodes), reset the bucket array to its empty sentinel and the
count to 0. Returns 0 to Lua. No GL calls.

##### Failure fallbacks

Decode failure leaves the handle NULL and the resolution simply runs on it
(the decompiled wrapper shows no guard; the "Removing unreachable block"
comments suggest dead error paths were cut). Bad path key types are logged
with a traceback by FUN_100155e0 and abort the path walk.

