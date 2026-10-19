# IEex_Helper_FakeKeyEvent

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10012050` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10012050.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Key_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10012050(undefined4 param_1)

{
  lua_toboolean(param_1,2);
  lua_tointeger(param_1,1);
  FUN_10024800();
  return 0;
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

`IEex_Helper_FakeKeyEvent(vk: integer, pressed: boolean)` -> returns 0
(nothing). Injects a synthetic key press or release into the fork's
key-state view -- the same state the GetAsyncKeyState helpers below read.

- Shim `FUN_10012050`: `pressed = lua_toboolean(L, 2)`, `vk =
  lua_tointeger(L, 1)`, then calls the dispatcher `FUN_10024800` with ECX =
  vk, EDX = pressed (fastcall; invisible in the decompile).
- Dispatcher `FUN_10024800` (runs/dll-decomp-inner/FUN_10024800.c) computes
  a direction mask `mask = (pressed & 0xff) ^ 1` -- **mask bit0: 0 = press,
  1 = release** -- and routes by VK class (byte table at `0x1002497c`,
  indexed `vk - 1`; jumptable at `0x10024958`):
  - `VK_LBUTTON` (0x01) -> slot 1, direction in mask bit0.
  - `VK_RBUTTON` (0x02) -> mask `mask*4 + 4`, so bit2 = press / bit3 =
    release -> slot 2.
  - `VK_MBUTTON` (0x04) -> mask `(mask + 1) * 0x10`, so bit4 = press / bit5
    = release -> slot 4.
  - `VK_LSHIFT`/`VK_RSHIFT` (0xa0/0xa1) -> modifier helper with param 0x10
    (VK_SHIFT); the L/R choice inside comes from the scancode.
  - `VK_LCONTROL`/`VK_RCONTROL` (0xa2/0xa3) -> param 0x11 (VK_CONTROL),
    R-flag in mask bit1.
  - `VK_LMENU`/`VK_RMENU` (0xa4/0xa5) -> param 0x12 (VK_MENU), R-flag in
    mask bit1.
  - everything else -> generic: `MapVirtualKeyExA(vk, MAPVK_VK_TO_VSC, NULL)`
    scancode in ECX, the VK pushed as a stack arg, direction in EDX.
- Modifier/entry helper `FUN_10023eb0` (runs/dll-decomp-inner/FUN_10023eb0.c)
  masks EDX to bit0 immediately (the R-flag travels separately in BL) and
  reloads ECX from param_1 -- **the slot applied is the VK itself, never the
  scancode**:
  - **Generic**: press -> press-applier(VK); release -> release-applier(VK).
  - **Shift** (param 0x10; own slot `VK_LSHIFT` 0xa0 or `VK_RSHIFT` 0xa1,
    chosen by `scancode == 0x36`): press -> press(own) + press(VK_SHIFT
    0x10). Release -> release(own); then read the OTHER side's state word
    (R-release checks `DAT_100966a8`, L-release `DAT_100966dc`): bit15
    still set (other shift held) -> re-press(VK_SHIFT), else
    release(VK_SHIFT).
  - **Control** (param 0x11; own slot 0xa2/0xa3): same, companion words
    R-release `DAT_10096710` / L-release `DAT_10096744`, combined slot 0x11.
  - **Menu** (param 0x12; own slot 0xa4/0xa5): same, companion words
    R-release `DAT_10096778` / L-release `DAT_100967ac`, combined slot 0x12.
- Press applier `FUN_10023c40(slot)` and release applier
  `FUN_10023d80(slot)` (runs/dll-decomp-inner/FUN_10023c40.c,
  FUN_10023d80.c): take the raw-input mutex, then the slot mutex (throw
  5/6 protocol); press sets the state word to `0xffff` (down + every
  pressed-since bit) and inserts the slot into the pressed-keys list if
  absent; release clears bit15 and unlinks the slot from the list (operator
  delete, 0xc bytes). Release checks the list under the raw-input mutex;
  both unlock in reverse order.

##### Failure fallbacks

None. Lock failures and the counter guard throw C++ exceptions
(`_Throw_Cpp_error(5)`/`(6)`, never Lua errors); list overflow
(`DAT_103d45c8 == 0x15555555`) is `std::_Xlength_error("list too long")`.
There is **no bounds check on vk**: the VK becomes the slot index raw
(`slot*0x34` into the arrays), so a nonsense vk >= 0x100 indexes far past
the real slot blocks and corrupts unrelated BSS. Any Lua value works for
`pressed` (`lua_toboolean`: non-nil/non-false = press).

##### Decompiler artifact

- `FUN_10012050` shows `FUN_10024800()` with no args: hidden fastcall
  ECX/EDX.
- `FUN_10024800` carries two jumptable WARNINGs; its switch operand is
  Ghidra's `PTR_caseD_8_10024978 + vk + 3`, i.e. the class byte at
  `0x1002497c + (vk - 1)`.
- `FUN_10023eb0` is decompiled as `void (short param_1)` with `in_CX` (the
  entry scancode) and `in_DL` (direction) implicit; all
  `FUN_10023c40()`/`FUN_10023d80()` calls show no args (slot passed in
  ECX). The `param_1 != 0x10 / != 0x11 / == 0x12` chain is the
  shift/control/menu/generic dispatch.
- Both appliers show `_Mtx_lock(&DAT_1008f260, cookie)` -- the second
  argument is the stack-cookie artifact, not a real parameter; slot-array
  accesses render as int arithmetic (`* 0xd` = byte stride 0x34).

