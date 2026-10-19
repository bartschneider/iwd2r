# IEex_Helper_GetPressedKeysStackNL

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10012080` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10012080.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10012080(undefined4 param_1)

{
  int iVar1;
  int *piVar2;
  int iVar3;

  iVar1 = DAT_103d45c8;
  lua_createtable(param_1,DAT_103d45c8,0);
  if (iVar1 != 0) {
    iVar3 = 1;
    piVar2 = (int *)*DAT_103d45c4;
    lua_pushinteger(param_1,piVar2[2]);
    lua_rawseti(param_1,0xfffffffe,1);
    if (iVar1 != 1) {
      do {
        piVar2 = (int *)*piVar2;
        iVar3 = iVar3 + 1;
        lua_pushinteger(param_1,piVar2[2]);
        lua_rawseti(param_1,0xfffffffe,iVar3);
      } while (iVar3 != iVar1);
    }
    lua_pushinteger(param_1,iVar3);
    lua_rawseti(param_1,0xfffffffe,0);
  }
  return 1;
}
```

## Notes

### Shared key-state machinery (all helpers in this group)

- **Slot model.** A "slot" is a virtual-key code. Per-slot block, byte
  stride 0x34: a `_Mtx_t` (0x30 bytes) at `0x100945f8 + slot*0x34`, a
  recursion count at `0x10094624 + slot*0x34`, and a 16-bit state word at
  `0x10094628 + slot*0x34` (decompiler: `&DAT_100945f8 + slot*0xd`
  int-pointer arithmetic, `(&DAT_10094628)[slot*0x1a]`). State bit15 =
  currently held down; bits 0-14 = per-vk "pressed since last query" flags
  (`1 << (vk & 0x1f)`), consumed by the GetAsyncKeyState helpers.
- **Raw-input mutex.** A global `_Mtx_t` at `DAT_1008f260` with a recursion
  counter at `DAT_1008f28c`. Every mutating helper takes it (plus the
  per-slot mutex); `RunWithRawInputLock` hands it to Lua callbacks. Lock
  protocol everywhere in this group: `_Mtx_lock` failure ->
  `std::_Throw_Cpp_error(5)`; counter `== 0x7fffffff` -> set `0x7ffffffe`
  and `std::_Throw_Cpp_error(6)`. These are thrown C++ exceptions, never
  Lua errors -- they will not surface as a pcall-able error.
- **Pressed-keys list.** A doubly linked list of currently-pressed slots:
  head sentinel `DAT_103d45c4`, element count `DAT_103d45c8`, 0xc-byte
  nodes (+0 next, +4 prev, +8 slot value), newest insertion first.
- **Runtime-populated state.** All of the above (slot blocks, counts, state
  words, companion modifier words, pressed list) is BSS -- zero in the DLL
  file, filled once the host registers the fork into the engine's input
  path. The raw-input mutex at `DAT_1008f260` sits in initialized `.data`
  and needs no registration.
- **Modifier companion words** `DAT_100966a8`/`DAT_100966dc` (shift),
  `DAT_10096710`/`DAT_10096744` (control), `DAT_10096778`/`DAT_100967ac`
  (menu) are not part of the 0x34-stride slot array (their offsets from it
  are not multiples of 0x34) -- a separate BSS array, same bit15-down
  encoding, one word per side of each L/R modifier pair.

### Fork load-time context (all helpers)

- **InitHelperDLL** (the DLL's ordinal-1 export at `0x1000e260`, stdcall,
  `ret 0xc`) is called by the host at load: its first argument is ignored,
  its second is stored into `DAT_100944fc` -- a host-provided logging
  function pointer the fork calls through all over its code -- and its
  third into `DAT_100944f8`, the sink the DLL's exported `LogVariadic`
  forwards to. It also captures the epoch base used by the time helpers.
- **LogVariadic** (ordinal-4 export at `0x1000e1b0`): variadic
  `(format, ...)`. If `DAT_100944f8` is still NULL the call is a no-op;
  otherwise it takes the global recursive mutex (mtx `0x1008f138`,
  recursion count `0x1008f164`; `_Mtx_lock` failure -> `_Throw_Cpp_error(5)`,
  count `0x7fffffff` -> set `0x7ffffffe` and throw 6) and calls
  `sink(format, &va_list)`.
- **Dual registration.** The fork's init function fills a Lua table with
  `"IEex_Helper_<Name>" -> <raw code address>` entries (for IEex `!call`
  raw-call macros), and exported `ExposeFunctions` registers callable
  `lua_CFunction` shims under the same names.

##### Behavior

Caller: the staged `IEex_Key_State.lua` patch.

`IEex_Helper_GetPressedKeysStackNL()` -> one table: the currently-pressed
slots. "NL" = **no locks**: it reads `DAT_103d45c8`, the list head, and the
node values without taking the raw-input mutex, so it can race a concurrent
FakeKeyEvent (or engine) update -- a just-pressed slot may be missed or a
torn read possible.

`FUN_10012080` (runs/dll-decomp/FUN_10012080.c): `count = DAT_103d45c8`;
`lua_createtable(L, count, 0)` (preallocated to the live count); walks the
pressed list from `*DAT_103d45c4` pushing each node's slot value (`node+8`)
with `lua_pushinteger` + `lua_rawseti(L, -2 /* the table */, i)` for
`i = 1..count`; then `lua_pushinteger(count)` +
`lua_rawseti(L, -2, 0)` -- **index 0 holds the count**. Returns 1.

Values are slot codes, i.e. VKs (mouse 0x01/0x02/0x04, keyboard the VK,
modifiers 0xa0-0xa5 and combined 0x10/0x11/0x12 -- see FakeKeyEvent).
Newest-pressed first (list head insertion order).

Quirk: when `count == 0` the `rawseti(..., 0)` is skipped (the whole body
is inside `if (count != 0)`), so the empty result has **no `[0]` entry** --
Lua callers must treat a missing `[0]` as 0.

##### Failure fallbacks

None -- no locks (nothing to throw on) and no explicit allocation-failure
path beyond `lua_createtable` itself.

##### Decompiler artifact

`0xfffffffe` in the decompile is `-2`, the table below the pushed integer
(not `LUA_REGISTRYINDEX`). The first list iteration is peeled out of the
`do/while` by Ghidra; the logic is a plain walk.

