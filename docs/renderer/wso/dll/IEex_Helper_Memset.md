# IEex_Helper_Memset

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100119a0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_100119a0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_100119a0(undefined4 param_1)

{
  size_t _Size;
  int _Val;
  void *_Dst;

  _Size = lua_tointeger(param_1,3);
  _Val = lua_tointeger(param_1,2);
  _Dst = (void *)lua_tointeger(param_1,1);
  memset(_Dst,_Val,_Size);
  return 0;
}
```

## Notes

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
  `sink(format, &va_list)`. Errors inside this group (see GetGameDirectory)
  are reported through it.
- **Dual registration.** The fork's init function fills a Lua table with
  `"IEex_Helper_<Name>" -> <raw code address>` entries (for IEex `!call`
  raw-call macros), and exported `ExposeFunctions` registers callable
  `lua_CFunction` shims under the same names. Memset and Sleep are special:
  their init-site entries are IAT import thunks (CRT `memset` /
  kernel32 `Sleep`), so `!call` reaches the raw import.
- All `DAT_100941xx`-`DAT_100944xx` slots used by these helpers are
  **runtime-registered engine entry points** (BSS; zero in the DLL file,
  resolved by the host at load) (decompiled from the fork's
  `IEexHelper.fork.dll`).

##### Behavior

`IEex_Helper_Memset(address:int, value:int, count:int)` -- registered at
`ExposeFunctions` (plain Lua call), returns 0 values:

```c
memset((void *)lua_tointeger(param_1,1),(int)lua_tointeger(param_1,2),
       (size_t)lua_tointeger(param_1,3));
return 0;
```

A raw memory write from Lua: destination address, byte value and length all
arrive as Lua integers. **Dual registration**: the init-site table entry is
the IAT import thunk for the CRT `memset` (`memset_exref`), so IEex
`!call` macros reach the raw CRT function with the same semantics; the
`ExposeFunctions` shim `FUN_100119a0` is the only wrapper.

Callers: `IEex_Core_State.lua`, `IEex_Gui_State.lua`,
`IEex_Gui_State.upstream.lua`.

##### Failure fallbacks

None by design: a bad address or length simply crashes the process. No
logging, no GL, no locks.

##### Decompiler artifact

None; the helper is a single call.
