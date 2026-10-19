# IEex_Helper_DirectoryExists

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011eb0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011eb0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_StartupFixes_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011eb0(undefined4 param_1)

{
  LPCSTR pszPath;
  BOOL BVar1;

  pszPath = (LPCSTR)lua_tolstring(param_1,1,0);
  BVar1 = PathIsDirectoryA(pszPath);
  lua_pushboolean(param_1,BVar1);
  return 1;
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

`IEex_Helper_DirectoryExists(path)` -- registered at `ExposeFunctions`
(plain Lua call), returns 1 boolean:

```c
pszPath = lua_tolstring(param_1,1,0);
BVar1 = PathIsDirectoryA(pszPath);
lua_pushboolean(param_1,BVar1);
return 1;
```

`PathIsDirectoryA` is shell32 (through the IAT). True for directories,
false for regular files and missing paths.

Caller: `IEex_StartupFixes_State.lua`.

##### Failure fallbacks

No failure paths and no logging. The `lua_tolstring` is unchecked: a
non-string first argument hands NULL to `PathIsDirectoryA` (documented to
return FALSE, so in practice the helper answers `false` rather than
crashing -- but that is the CRT's behavior, not the helper's design).

##### Decompiler artifact

None worth flagging; the helper is four calls long and decompiles cleanly.
