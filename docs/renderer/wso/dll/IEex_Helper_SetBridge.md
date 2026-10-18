# IEex_Helper_SetBridge

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10010f40` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10010f40.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Creature_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_IWD2_State.lua`, `IEex_Key_State.lua`, `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Removing unreachable block (ram,0x10010fdd) */
/* WARNING: Removing unreachable block (ram,0x10010fee) */

undefined4 FUN_10010f40(void)

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
  FUN_10019930();
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

`IEex_SetBridge(bridge, [path...,] key, value)` stores a Lua value.
Wrapper FUN_10010f40: decode handle, Plain (no suffix) variant: FUN_10013e90 two-lock step, exclusive:
blocking-acquire handle+0, then TryAcquireExclusive handle+4; contention
releases handle+0, clears the slot flag, _Thrd_yield()s and retries.
Release order after the work is +4 then +0. inner
FUN_10019930: FUN_100155e0 over args 2..gettop-2 resolves the **parent**
container, then the key argument decides the slot -- type 3 (number) ->
lua_tointeger, FNV-1a over the 4 bytes, std::_Find_last; type 4 (string) ->
std::string + FNV-1a + FUN_1001c0e0. A missing slot allocates a 0xc-byte
LuaContainer + 0x10-byte `std::_Ref_count<LuaContainer>` and links it; the
value argument is then converted into the slot by FUN_10015d60 (tagged
LuaData: boolean/integer/string/bridge-handle, ref-counted under LOCK).
Any other key type: ostringstream of the type number + luaL_traceback +
log call + stack reset. Returns 0 to Lua. The middle of the decompile is
obscured by shared_ptr bookkeeping and five "Removing unreachable block"
warnings -- treat the replace-vs-reuse detail as approximate. No GL calls.

##### Failure fallbacks

Bad key type -> logged, bridge untouched. Values of unsupported Lua types
are converted by the tag rules above or dropped.

