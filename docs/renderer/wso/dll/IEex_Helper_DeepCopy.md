# IEex_Helper_DeepCopy

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10012de0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10012de0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10012de0(void)

{
  FUN_10012c00();
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

`IEex_Helper_DeepCopy(value)` -- exactly one Lua argument, returns 1 (the
copy). The shim `FUN_10012de0` is three instructions: `ECX = [esp+8]` (the
lua_State reaches the impl as a hidden fastcall register argument), call
`FUN_10012c00`, `return 1`.

`FUN_10012c00` (decompiled at
`runs/dll-decomp-inner/FUN_10012c00.c`):

- `lua_type(L, 1) != LUA_TTABLE` -> `lua_pushvalue(L, 1)` and return: any
  non-table input is passed through unchanged.
- Tables: two fresh tables are created (`lua_createtable` twice) -- the copy
  and a visited map -- then walked **iteratively**: the Lua stack itself is
  the work stack (bookkeeping index starts at 4, +3 per nesting level; the
  `iVar1 == 4` check means "stack empty, done"). Each source table is
  iterated with `lua_next`; scalar key/value pairs are copied into the
  parent copy via `lua_pushvalue`/`lua_insert`/`lua_rawset`; a nested table
  gets a fresh copy table, is recorded in the visited map (identity-keyed
  via `lua_pushvalue` + `lua_rawseti`), and is pushed onto the work stack.
  The visited map makes this **cycle-safe** (self-referencing and shared
  subtables terminate; shared subtables are re-copied per occurrence but do
  not loop).

No GL calls, no locks, no registry involvement.

Callers: `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua` (clones GUI
state tables).

##### Failure fallbacks

None. Non-table input is returned as-is; there are no error paths, no
logging, no allocation-failure handling beyond Lua's own longjmp.

##### Decompiler artifact

- The shim loses its argument list entirely (hidden ECX "fastcall"), which
  is why `FUN_10012de0(void)` shows no parameters and returns a constant 1.
- Every Lua API call inside `FUN_10012c00` decompiles with **empty argument
  lists** for the same reason -- read the API names and the rawseti index
  arithmetic, not the parens; the `lua_rawgeti`/`lua_rawseti` dance is the
  explicit work stack, not redundant logic.
