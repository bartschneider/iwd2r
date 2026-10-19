# IEex_Helper_GetCursorPhysicalXY

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002a5e0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_1002a5e0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_1002a5e0(undefined4 param_1)

{
  undefined4 local_c;
  undefined4 local_8;

  FUN_1002a3b0();
  lua_pushnumber(param_1,(double)local_8);
  lua_pushnumber(param_1,(double)local_c);
  return 2;
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

Caller: the staged `IEex_UncapFPS.lua` patch.

`IEex_Helper_GetCursorPhysicalXY()` -> two numbers `(x, y)`: the engine
cursor's logical position converted to physical screen pixels under the
current zoom/panel transform. This defines the logical->physical mapping a
reimplementation of cursor hit-testing must reproduce.

- Shim `FUN_1002a5e0` (runs/dll-decomp/FUN_1002a5e0.c): two int locals;
  calls `FUN_1002a3b0` with EDX = `&x`, stack arg = `&y`, CL = 1; then
  `lua_pushnumber(L, (double)x)`, `lua_pushnumber(L, (double)y)`; returns
  2. **(x, y) order is asm-verified**: the first `movd`/convert reads
  `[ebp-0x4]` (the EDX out) and is pushed first.
- Impl `FUN_1002a3b0(&y, EDX = &x, CL = enable-checks)`
  (runs/dll-decomp-inner/FUN_1002a3b0.c): `g = *DAT_100944b8` (engine state
  pointer); logical cursor at `g+0x1906` (x) / `g+0x190a` (y); UI-mode byte
  `DAT_103dec27`.
  - **Retry loops** (active because CL = 1): while byte `DAT_100945f6` is
    non-zero, up to 8 attempts -- when bit0 of `DAT_104df5e4` is observed
    clear, snapshot the fallback cursor (`DAT_104df0ac`, `DAT_103ded80`) +
    mode byte (`DAT_104df218`), re-check the flag is unchanged, and proceed
    with those values through the normal transform; after 8 attempts
    proceed with the values read at entry. Then while byte `DAT_104df075`
    is non-zero, the same 8-attempt pattern on bit0 of `DAT_104df4e8` --
    but its success path **returns the last-known-good pair directly**
    (`DAT_103ded7c` -> x, `DAT_104df038` -> y), bypassing the transform.
    These are concurrent-update guards (the cursor being rewritten on
    another thread); the fallback globals are the last stable snapshots.
  - **Main path**: if the mode byte is non-zero and the engine's panel
    field `*(g+0x3c4)` is non-zero:
    - `*(g+0x3c4) == *(*DAT_10094384 + 0x1c88)` (cursor over the current
      panel): `FUN_100296c0` yields the zoom float (via a hidden ECX
      out-arg); zoom must be > 0; centerX = (`DAT_104df4dc == 0` ?
      `(float)(ushort)*DAT_10094318 * 0.5f` : `0.0f`); centerY =
      `(float)(ushort)*DAT_10094328`;
      `x = (logicalX - centerX)*zoom + centerX + 0.5f`,
      `y = (logicalY - centerY)*zoom + centerY + 0.5f` -- the panel center
      stays fixed under zoom.
    - otherwise: `FUN_100298b0` yields zoom (ECX out), centerX (EDX out)
      and three stack outs (originX, centerY, originY);
      `x = (logicalX - centerX)*zoom + originX + 0.5f`,
      `y = (logicalY - centerY)*zoom + originY + 0.5f` -- anchored to a
      scroll origin instead.
  - **Fallback**: mode byte zero, panel field zero, zoom <= 0, or the
    helper returning false -> raw logical coords out unchanged.

##### Failure fallbacks

Enumerated above: every failure collapses to the raw logical position (or
the last-known-good fallback globals inside the retry loops, capped at 8
attempts). No exceptions are thrown on this path.

##### Decompiler artifact

- **Stack-slot dual role**: `-0x10(%ebp)` first holds `g` (the engine
  pointer) and is then OVERWRITTEN by the zoom float returned through the
  hidden ECX out-arg of `FUN_100296c0`/`FUN_100298b0` (`lea -0x10(%ebp),%ecx`
  before each call). Ghidra merged the two uses into one `local_14`, so the
  decompile reads `(cVar3 != '\0') && (fVar6 = 0.0, 0.0 < local_14)` --
  that comparison tests the zoom float, not the pointer.
- Both panel helpers show one argument in the decompile; the ECX/EDX/stack
  out-args are dropped (fastcall/hidden-register artifacts).
- The shim decompile's `local_8`/`local_c` names do not line up with the
  raw `[ebp-0x8]`/`[ebp-0x4]` offsets -- trust the asm for the (x, y)
  order.
- `(float)*DAT_10094318 * 0.5` operates on a `ushort` load -- the
  decompile's implicit conversions around `DAT_104df4dc` are literal.

