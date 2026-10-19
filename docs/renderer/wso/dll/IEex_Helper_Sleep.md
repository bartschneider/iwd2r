# IEex_Helper_Sleep

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100119e0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_100119e0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`, `IEex_IWD2_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_100119e0(undefined4 param_1)

{
  DWORD dwMilliseconds;

  dwMilliseconds = lua_tointeger(param_1,1);
  Sleep(dwMilliseconds);
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

`IEex_Helper_Sleep(milliseconds:int)` -- registered at `ExposeFunctions`
(plain Lua call), returns 0 values:

```c
Sleep(lua_tointeger(param_1,1));
return 0;
```

kernel32 `Sleep` through the IAT; blocks the calling thread (in practice the
thread running the Lua state -- usually the game thread) for the requested
milliseconds. **Dual registration**: the init-site table entry is the IAT
import thunk for `Sleep` (`Sleep_exref`) for `!call` macros; the
`ExposeFunctions` shim `FUN_100119e0` is the only wrapper.

Callers: `IEex_Core_State.lua`, `IEex_IWD2_State.lua`.

##### Failure fallbacks

None; `Sleep` has no failure mode worth reporting and the wrapper adds
nothing. Note only that a large argument freezes whatever thread calls it.

##### Decompiler artifact

None; the helper is a single call.
