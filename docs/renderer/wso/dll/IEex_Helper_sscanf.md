# IEex_Helper_sscanf

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1000e170` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1000e170.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_LoadTimes_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1000e170(undefined4 param_1,undefined4 param_2)

{
  undefined4 *puVar1;

  puVar1 = (undefined4 *)FUN_1000e160(param_1,0xffffffff,param_2,0,&stack0x0000000c);
  __stdio_common_vsscanf(*puVar1,puVar1[1]);
  return;
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

`sscanf(const char *buffer, const char *format, ...)` -- the plain UCRT
`sscanf`, registered at the init site only as a raw address (the
`IEex_Helper_sscanf` table entry holds the function's code address, so IEex
`!call` raw-call macros can invoke the CRT directly; it is never a callable
`lua_CFunction`). The body (`FUN_1000e170`) is the standard UCRT inline
shim: it validates arguments and delegates to `FUN_1000e160` ->
`__stdio_common_vsscanf`. No fork logic whatsoever.

Returns the number of fields assigned, or EOF on input failure -- as with
any cdecl varargs call through `!call`, the Lua side marshals the pointer
arguments itself.

Caller: `IEex_LoadTimes_Patch.lua` (parses formatted strings at load time).

##### Failure fallbacks

None; CRT semantics only.

##### Decompiler artifact

The decompile is the UCRT inline shim plus stack-cookie noise; the real
entry is cdecl with varargs after the first two pointer arguments, which is
why the decompiled parameter list looks truncated.
