# IEex_Helper_RunWithRawInputLock

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10012170` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10012170.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10012170(void)

{
  undefined **local_30;
  undefined1 *local_2c;
  undefined1 *local_c;

  local_c = (undefined1 *)&local_30;
  local_2c = &stack0x00000004;
  local_30 = std::
             _Func_impl_no_alloc<`int___cdecl_Lua_RunWithRawInputLock(lua_State*)'::`2'::<lambda_1>,void>
             ::vftable;
  FUN_10023b80();
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

`IEex_Helper_RunWithRawInputLock(fn)` -- runs the Lua function `fn` while
the fork's raw-input mutex (`DAT_1008f260`) is held: a race-free window for
reading/writing the key-state view (the same mutex the FakeKeyEvent
press/release appliers take for every list mutation).

- Shim `FUN_10012170` (runs/dll-decomp/FUN_10012170.c) builds a stack
  `std::_Func_impl_no_alloc<Lua_RunWithRawInputLock(lua_State*)::lambda_1,
  void>` -- the RTTI name survives -- with vftable `0x1006430c` (`_Do_call`
  at vtable+8 = `0x10014210`), capturing `&arg1` (the address of the
  shim's `lua_State*` argument slot, `lea 0x8(%ebp)`), then calls the
  driver `FUN_10023b80` and returns 1.
- Driver `FUN_10023b80` (runs/dll-decomp-inner/FUN_10023b80.c):
  `_Mtx_lock(&DAT_1008f260)` (throw 5 / counter 6 protocol); `_Xbad_function_call()`
  if the function object is null (unreachable via Lua); calls `_Do_call`
  through the vtable; `_Mtx_unlock`; destroying dtor.
- Lambda `_Do_call` (raw disassembly at `0x10014210`): `lua_getfield(L,
  LUA_GLOBALSINDEX (-10002), "debug")`, `lua_getfield(L, -1, "traceback")`,
  `lua_pushvalue(L, 1)` (the user function), `lua_pcall(L, 0, 0, -2)` --
  traceback as error handler, **no results kept**. On error:
  `lua_tolstring(L, -1, NULL)` and the host log `%s` through
  `DAT_100944fc` (the InitHelperDLL-registered logging pointer) with the
  literal at `0x100642cc`; cleanup `lua_settop(L, -5)` (error) /
  `lua_settop(L, -4)` (success) -- indices exactly as decompiled, not
  reconciled against a stack model. Literals: `"debug"` `0x100642b8`,
  `"traceback"` `0x100642c0`.
- Net contract: `fn` runs to completion inside the lock; its return values
  are discarded; **Lua errors inside `fn` do not propagate** -- they are
  logged to the host log and swallowed. Treat the helper as
  side-effect-only: the shim declares 1 return but nothing is pushed.

##### Failure fallbacks

Lock failures throw C++ exceptions (`_Throw_Cpp_error(5)`/`(6)`); callback
errors are caught at the `lua_pcall` boundary and logged, never rethrown.

##### Decompiler artifact

- The shim decompile is almost entirely elided: it shows only the vftable
  store and `local_2c = &stack0x00000004` (the `&arg1` capture); the call
  wiring is register-only.
- `FUN_10023b80`'s decompile shows the function object as
  `in_stack_00000028` and the deleting-dtor call with a nonsensical
  argument (that argument is the destroyed-state bool; artifact).
- `_Mtx_lock` appears with two extra arguments (stack cookie + the mutex
  again) -- spurious.

