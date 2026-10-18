# IEex_Helper_GetGameDirectory

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011d90` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011d90.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_StartupFixes_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011d90(undefined4 param_1)

{
  undefined4 ****ppppuVar1;
  undefined **ppuStack_a4;
  undefined1 *puStack_a0;
  undefined1 *puStack_80;
  undefined **ppuStack_7c;
  undefined1 *puStack_78;
  undefined4 ***pppuStack_74;
  undefined4 uStack_6c;
  undefined4 ***pppuStack_44;
  undefined4 uStack_40;
  undefined4 uStack_3c;
  undefined4 uStack_38;
  undefined4 uStack_34;
  uint uStack_30;
  undefined4 uStack_2c;
  undefined1 auStack_28 [4];
  uint uStack_24;
  undefined1 *puStack_20;
  void *pvStack_1c;
  undefined1 *puStack_18;
  undefined4 uStack_14;

  puStack_20 = &stack0xfffffffc;
  puStack_18 = &LAB_1005ad6e;
  pvStack_1c = ExceptionList;
  uStack_24 = DAT_1008f040 ^ (uint)&stack0xfffffff0;
  ExceptionList = &pvStack_1c;
  uStack_14 = 0;
  puStack_a0 = auStack_28;
  uStack_34 = 0;
  pppuStack_74 = &pppuStack_44;
  uStack_30 = 0xf;
  uStack_2c = 1;
  puStack_80 = (undefined1 *)&ppuStack_a4;
  uStack_40 = 0;
  uStack_3c = 0;
  uStack_38 = 0;
  pppuStack_44 = (undefined4 ****)0x0;
  ppuStack_7c = std::
                _Func_impl_no_alloc<`class_std::basic_string<char,std::char_traits<char>,std::allocator<char>_>___cdecl_GetGameDirectory(void)'::`2'::<lambda_2>,void,char*,unsigned_int>
                ::vftable;
  ppuStack_a4 = std::
                _Func_impl_no_alloc<`class_std::basic_string<char,std::char_traits<char>,std::allocator<char>_>___cdecl_GetGameDirectory(void)'::`2'::<lambda_1>,TryFillBufferResult,char*,unsigned_int>
                ::vftable;
  puStack_78 = puStack_a0;
  FUN_1000bbc0();
  ppppuVar1 = &pppuStack_44;
  if (0xf < uStack_30) {
    ppppuVar1 = (undefined4 ****)pppuStack_44;
  }
  lua_pushstring(param_1,ppppuVar1);
  if (0xf < uStack_30) {
    if (0xfff < uStack_30 + 1) {
      if (0x1f < (uint)((int)pppuStack_44 + (-4 - (int)pppuStack_44[-1]))) {
                    /* WARNING: Subroutine does not return */
        uStack_6c = 0x10011e7a;
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9();
  }
  ExceptionList = pvStack_1c;
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

Two faces of one helper:

- The `ExposeFunctions` shim `FUN_10011d90` (registry raw-symbol
  `&LAB_10011d90`) takes no Lua arguments, computes the directory, pushes it
  with `lua_pushstring`, and returns 1.
- DLL export ordinal 3 at `0x10011cc0` is the native form:
  `void GetGameDirectory(std::string* out)` (initializes an empty
  `std::string` at `out`, then runs the same machinery).

**Result = the directory portion of the host process executable path** --
i.e. the game root, wherever Icewind2.exe actually runs from. It is not
read from any INI and does not consult a host callback. Machinery (shim
`FUN_10011d90` -> driver `FUN_1000bbc0` -> grow-buffer loop `FUN_1000c080`):

- Two `std::function` objects are built on the shim's stack; their RTTI
  names survive in the decompile:
  `_Func_impl_no_alloc<GetGameDirectory(void)::lambda_1, TryFillBufferResult,
  char*, unsigned int>` (filler) and `...lambda_2, void, char*, unsigned int>`
  (converter).
- The driver starts with a `0x400`-byte stack buffer and loops: the filler's
  `_Do_call` (`0x10014410`) is `GetModuleFileNameA(NULL, buffer, size)`
  (KERNEL32 through IAT `0x1005e070`) and returns whether the copied length
  differs from the buffer size; "fits" -> the converter's `_Do_call`
  (`0x10014320`) runs once; "possibly truncated" -> next size =
  `size * 2` (`FUN_10001a00`), `operator new`, retry; filler result 2 ends
  the loop without converting; any other nonzero filler result is reported
  through `LogVariadic` (format string `0x10062398`).
- The converter scans the filled buffer backwards from the copied length for
  the last `'\\'`; found -> the directory prefix is produced via
  `FUN_100035c0` and moved into the out string (`FUN_10013520`); no
  backslash at all -> the whole buffer is appended as-is (`FUN_10014900`).

Caller: `IEex_StartupFixes_State.lua`.

##### Failure fallbacks

- `GetModuleFileNameA` returning 0 (API failure) still reads as "fits"
  (`0 != size`), and the copied length 0 skips the backslash scan -- so the
  helper **silently produces an empty or garbage directory** (whatever the
  uninitialized stack buffer holds) rather than an error. No logging, no
  "disabled" path.
- Truncation is the one handled case: the buffer doubles and retries.

##### Decompiler artifact

- `FUN_1000bbc0`'s decompile is a garbled template driver with in-stack
  arguments -- decode `FUN_1000c080` plus the two `_Do_call` bodies
  instead.
- The `std::function` vtables are slot +0x8 = `_Do_call`,
  +0x00/+0x04 = `_Copy` thunks, +0x0c = `_Get`, +0x10 = deleting dtor; the
  force-decompiled `FUN_1000cbe0`/`FUN_1000cb60` are that deleting dtor
  (`if (flag) operator delete`), not worker logic.
- The shim's `_invoke_watson`/`FUN_100585f9` tail is just the result
  string's destruction.
