# IEex_Helper_WriteDump

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011c00` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011c00.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Core_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011c00(undefined4 param_1)

{
  undefined4 uVar1;
  undefined4 uVar2;
  LPCSTR lpFileName;
  DWORD DVar3;
  HANDLE hObject;
  HANDLE pvVar4;
  undefined8 local_14;
  undefined4 local_c;
  uint local_8;

  local_8 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  uVar1 = lua_tointeger(param_1,1);
  uVar2 = lua_tointeger(param_1,2);
  lpFileName = (LPCSTR)lua_tolstring(param_1,3,0);
  local_c = 0;
  local_14 = 0;
  DVar3 = GetCurrentThreadId();
  local_14 = CONCAT44(uVar2,DVar3);
  local_c = 0;
  hObject = CreateFileA(lpFileName,0x40000000,0,(LPSECURITY_ATTRIBUTES)0x0,2,0x80,(HANDLE)0x0);
  DVar3 = GetCurrentProcessId();
  pvVar4 = GetCurrentProcess();
  MiniDumpWriteDump(pvVar4,DVar3,hObject,uVar1,&local_14,0,0);
  CloseHandle(hObject);
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

`IEex_Helper_WriteDump(dumpType:int, exceptionPointersPtr:int,
filename:string)` -- registered at `ExposeFunctions` (plain Lua call),
returns 0 values. Writes a minidump (dbghelp `MiniDumpWriteDump` through the
IAT):

- Fills `MINIDUMP_EXCEPTION_INFORMATION` on the stack:
  `{ThreadId = GetCurrentThreadId(), ExceptionPointers =
  (PEXCEPTION_POINTERS)exceptionPointersPtr, ClientPointers = 0}` -- the
  exception-pointers argument is a raw integer the caller obtained
  elsewhere, and the dump is written in the helper's own process context.
- `CreateFileA(filename, GENERIC_WRITE, 0 /*no sharing*/, NULL,
  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)`.
- `MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
  dumpType, &info, NULL, NULL)`.
- `CloseHandle(hFile)`.

Caller: `IEex_Core_State.lua` (crash/dump plumbing).

##### Failure fallbacks

Nothing is checked: a `CreateFileA` failure hands `INVALID_HANDLE_VALUE`
straight into `MiniDumpWriteDump` (which fails silently), and the helper
returns 0 regardless of whether a file was produced. No logging, no
"disabled" path.

##### Decompiler artifact

None worth flagging -- Ghidra recovered the Win32 types from the IAT, so
this is one of the few notes whose decompile reads as written.
