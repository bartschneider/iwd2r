# IEex_Helper_GetAsyncKeyStateWrapper

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10024710` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10024710.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
ushort FUN_10024710(int param_1)

{
  ushort uVar1;
  int iVar2;

  iVar2 = _Mtx_lock(&DAT_100945f8 + param_1 * 0xd);
  if (iVar2 != 0) {
    std::_Throw_Cpp_error(5);
  }
  if ((&DAT_10094624)[param_1 * 0xd] == 0x7fffffff) {
    (&DAT_10094624)[param_1 * 0xd] = 0x7ffffffe;
    std::_Throw_Cpp_error(6);
  }
  uVar1 = (&DAT_10094628)[param_1 * 0x1a];
  (&DAT_10094628)[param_1 * 0x1a] = uVar1 & 0xfffe;
  _Mtx_unlock(&DAT_100945f8 + param_1 * 0xd);
  return uVar1 & 0x8001;
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

Registered at the **init site only** (raw address `0x10024710` in the
`IEex_Helper_GetAsyncKeyStateWrapper` table entry): the Lua patch reaches it
through `!call` raw-call macros, not as a normal Lua call. It is not a
`lua_CFunction` -- single stack argument `slot`, returns a `ushort`.

`FUN_10024710(slot)` (runs/dll-decomp/FUN_10024710.c) is the fixed-bit
sibling of GetAsyncKeyStateClient: locks the slot mutex only (throw 5 /
counter 6), reads the state word, clears **bit0** unconditionally, unlocks,
returns the pre-clear `state & 0x8001`:

- **bit0** = the slot was faked-pressed since the previous call (consumed).
- **bit15** = currently held down (non-consuming).

##### Failure fallbacks

Same as GetAsyncKeyStateClient: `_Throw_Cpp_error(5)`/`(6)` lock protocol,
no bounds checks on `slot`.

##### Decompiler artifact

None of note -- `FUN_10024710` decompiles clean.

