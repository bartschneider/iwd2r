# IEex_Helper_CAIScript_ConstructCached

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1000d380` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1000d380.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_LoadTimes_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 * FUN_1000d380(undefined4 *param_1,undefined4 *param_2)

{
  byte *pbVar1;
  char cVar2;
  int iVar3;
  undefined4 *puVar4;
  undefined4 *puVar5;
  char *pcVar6;
  uint uVar7;
  int iVar8;
  uint uVar9;
  undefined4 ****ppppuVar10;
  undefined4 *puVar11;
  char local_3c [12];
  undefined4 *local_30;
  undefined4 ***local_2c;
  undefined4 uStack_28;
  undefined4 uStack_24;
  undefined4 uStack_20;
  uint local_1c;
  uint local_18;
  uint local_14;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005a88d;
  local_10 = ExceptionList;
  local_14 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  ExceptionList = &local_10;
  local_30 = param_1;
  (*DAT_100943b4)(10,local_14);
  param_1[3] = DAT_1009418c;
  *param_1 = *param_2;
  param_1[1] = param_2[1];
  iVar8 = 0;
  do {
    iVar3 = iVar8;
    cVar2 = *(char *)((int)param_2 + iVar3);
    local_3c[iVar3] = cVar2;
    if (cVar2 == '\0') break;
    iVar8 = iVar3 + 1;
  } while (iVar3 + 1 < 8);
  local_3c[iVar3 + 1] = '\0';
  pcVar6 = local_3c;
  local_1c = 0;
  local_2c = (undefined4 ****)0x0;
  uStack_28 = 0;
  uStack_24 = 0;
  uStack_20 = 0;
  local_18 = 0;
  do {
    cVar2 = *pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (cVar2 != '\0');
  FUN_100035c0(local_3c,(int)pcVar6 - (int)(local_3c + 1));
  local_8 = 0;
  uVar9 = 0x811c9dc5;
  ppppuVar10 = &local_2c;
  if (0xf < local_18) {
    ppppuVar10 = (undefined4 ****)local_2c;
  }
  uVar7 = 0;
  if (local_1c != 0) {
    do {
      pbVar1 = (byte *)((int)ppppuVar10 + uVar7);
      uVar7 = uVar7 + 1;
      uVar9 = (*pbVar1 ^ uVar9) * 0x1000193;
    } while (uVar7 < local_1c);
  }
  iVar3 = FUN_1000d770(local_3c,&local_2c,uVar9);
  iVar8 = DAT_103d4660;
  if (*(int *)(iVar3 + 4) != 0) {
    iVar8 = *(int *)(iVar3 + 4);
  }
  if (iVar8 == DAT_103d4660) {
    (*DAT_10094334)(*param_2,param_2[1],0);
    puVar4 = (undefined4 *)(*DAT_10094224)(0x28);
    (*DAT_100943b4)(10);
    puVar11 = local_30;
    puVar4[3] = DAT_1009418c;
    *puVar4 = *param_2;
    puVar4[1] = param_2[1];
    FUN_1000d2f0();
    puVar5 = (undefined4 *)FUN_1000d570(&local_2c);
    *puVar5 = puVar4;
  }
  else {
    FUN_1000d2f0();
    puVar11 = local_30;
  }
  if (0xf < local_18) {
    uVar9 = local_18 + 1;
    ppppuVar10 = (undefined4 ****)local_2c;
    if (0xfff < uVar9) {
      ppppuVar10 = (undefined4 ****)local_2c[-1];
      uVar9 = local_18 + 0x24;
      if (0x1f < (uint)((int)local_2c + (-4 - (int)ppppuVar10))) {
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(ppppuVar10,uVar9);
  }
  ExceptionList = local_10;
  return puVar11;
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

`CAIScript *FUN_1000d380(CAIScript *out, void *resrefPair)` -- registered at
the init site only (raw address in the `IEex_Helper_CAIScript_ConstructCached`
table entry; the Lua patch reaches it through `!call` raw-call macros, not as
a normal Lua call). Not a `lua_CFunction`: `out` is a caller-provided
CAIScript, `resrefPair` is the 8-byte Resref as two dwords.

- `out` is constructed unconditionally: `(*DAT_100943b4)(out+0xc, 10)` (note:
  this runs BEFORE the vtable store, on the not-yet-initialized object; its
  exact role is unverified -- do not read it as a lock on a live object),
  then vtable `DAT_1009418c` -> out+0xc, and the two resref dwords ->
  out+0x00/out+0x04.
- **Cache key**: the resref bytes are copied into an up-to-8-byte
  NUL-terminated buffer, wrapped in an `std::string`, and hashed with inline
  FNV-1a (`0x811c9dc5`/`0x01000193`); `FUN_1000d770` finds it in the
  script-cache registry (sentinel `DAT_103d4660`, found flag at out-struct+4).
- **Hit**: `FUN_1000d2f0` (ECX = cached `CAIScript*` at node+0x20, EDX =
  out+0xc) clones the cached script's internals into `out`.
- **Miss**: the engine constructor behind `DAT_10094334` runs as
  `__thiscall(out, resref0, resref1, 0)`; a fresh node value is allocated
  (`operator new(0x28)` via `DAT_10094224`), initialized the same way
  (`DAT_100943b4`, vtable, resref dwords), then `FUN_1000d2f0` (ECX = out,
  EDX = newobj+0xc) clones `out`'s internals INTO the new object;
  `FUN_1000d570(&key)` returns the value slot (node+0x20) and the new object
  is stored there.
- **Returns `out` in both branches**; the cache always keeps its own copy.
- `FUN_1000d2f0(src in ECX, dst in EDX)` tears down dst's existing lists
  (dst+4 list node-by-node via `DAT_100941ac`/`DAT_10094408`, then
  `DAT_1009442c`) and deep-copies src's src+0x10 list (`operator new(0x3c)`
  per node, per-node copy via `DAT_10094418`/`DAT_10094178`).

Caller: `IEex_LoadTimes_Patch.lua` (warms the script cache at load time).

##### Failure fallbacks

None: no logging or "disabled" strings, no GL, no locks. The script-cache
registry is the fourth unsynchronized FNV-1a registry, so concurrent first
loads of the same resref could race (the staged caller uses it during
single-threaded load). An unregistered engine pointer would simply crash.

##### Decompiler artifact

- `(*DAT_10094334)(*param_2,param_2[1],0)` hides the hidden-thiscall `this`
  (= `out`, set in ECX at asm `0x1000d486`-`0x1000d4a0`); likewise the first
  `(*DAT_100943b4)(10,local_14)` call's second "argument" is the ECX this
  pointer plus cookie noise.
- Both `FUN_1000d2f0()` calls show no arguments -- src/dst travel in
  ECX/EDX.
- The `do {... while (cVar2 != 0)}` loop is `strlen`; the
  `_invoke_watson`/`FUN_100585f9` tail is just the key string's
  destruction.
