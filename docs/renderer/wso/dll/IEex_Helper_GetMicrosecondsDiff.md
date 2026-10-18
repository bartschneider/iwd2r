# IEex_Helper_GetMicrosecondsDiff

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011690` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011690.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Key_State.lua`, `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011690(undefined4 param_1)

{
  char cVar1;
  uint uVar2;
  char *pcVar3;
  int *piVar4;
  int *piVar5;
  char *pcVar6;
  uint uVar7;
  int iVar8;
  int local_44 [12];
  char *local_14;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005ac75;
  local_10 = ExceptionList;
  uVar2 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  ExceptionList = &local_10;
  pcVar3 = (char *)lua_tolstring(param_1,1,0,uVar2);
  local_44[4] = 0;
  local_44[5] = 0;
  local_44[0] = 0;
  local_44[1] = 0;
  local_44[2] = 0;
  local_44[3] = 0;
  local_14 = pcVar3 + 1;
  pcVar6 = pcVar3;
  do {
    cVar1 = *pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(pcVar3,(int)pcVar6 - (int)local_14);
  local_8 = 0;
  pcVar3 = (char *)lua_tolstring(param_1,2,0,uVar2);
  local_44[10] = 0;
  local_44[0xb] = 0;
  local_44[6] = 0;
  local_44[7] = 0;
  local_44[8] = 0;
  local_44[9] = 0;
  pcVar6 = pcVar3;
  do {
    cVar1 = *pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(pcVar3,(int)pcVar6 - (int)(pcVar3 + 1));
  local_8 = CONCAT31(local_8._1_3_,1);
  piVar4 = (int *)FUN_100134e0(local_44 + 6);
  piVar5 = (int *)FUN_100134e0(local_44);
  lua_pushinteger(param_1,*piVar5 - *piVar4);
  if (0xf < (uint)local_44[0xb]) {
    uVar7 = local_44[0xb] + 1;
    uVar2 = local_44[6];
    if (0xfff < uVar7) {
      uVar2 = *(uint *)(local_44[6] - 4);
      uVar7 = local_44[0xb] + 0x24;
      if (0x1f < (local_44[6] - uVar2) - 4) goto LAB_100117d3;
    }
    FUN_100585f9(uVar2,uVar7);
  }
  local_44[10] = 0;
  local_44[0xb] = 0xf;
  local_44[6] = local_44[6] & 0xffffff00;
  if (0xf < (uint)local_44[5]) {
    uVar2 = local_44[5] + 1;
    iVar8 = local_44[0];
    if (0xfff < uVar2) {
      iVar8 = *(int *)(local_44[0] + -4);
      uVar2 = local_44[5] + 0x24;
      if (0x1f < (local_44[0] - iVar8) - 4U) {
LAB_100117d3:
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(iVar8,uVar2);
  }
  ExceptionList = local_10;
  return 1;
}
```

## Notes

### Shared machinery (all four helpers)

**Timestamp registry.** A third FNV-1a-keyed `std::unordered_map`, sibling of
the bridge registry (`DAT_103d4700`) and the lock registry (`DAT_103d4620`)
(decompiled from the fork's `IEexHelper.fork.dll`; helpers listed in
`runs/dll-decomp-inner/FUN_10013bb0.c`, `FUN_10013c50.c`,
`FUN_100134e0.c`):

- Map rooted at `DAT_103d46a0` (element count `DAT_103d46a4`, bucket array
  `DAT_103d46a8`, bucket mask `DAT_103d46b4`, bucket count `DAT_103d46b8`,
  max-load factor `DAT_103d469c`); key is an `std::string` hashed with 32-bit
  FNV-1a (basis `0x811c9dc5`, prime `0x01000193`); length cap `0x6666666`
  -> `_Xlength_error("unordered_map/set too long")`.
- Node = `operator_new(0x28)`: `std::string` name at node+0x08, **int64 value
  slot at node+0x20..0x27**, zeroed at creation.
- `FUN_10013bb0` = find (out-struct `{node-or-before-begin, found}`; a full
  miss returns `{DAT_103d46a0, 0}`); `FUN_10013c50` = lookup-or-create;
  `FUN_100134e0` wraps the latter and returns `node + 0x20` -- the value-slot
  address (decompile reads `return *piVar1 + 0x20;` because `piVar1` points
  at the out-struct).
- The registry map itself is unsynchronized, like the other two registries.

**32-bit wrap semantics.** StoreMicroseconds computes `now - epoch` in 32
bits only and stores it sign-extended into the 64-bit slot (raw asm at
`0x10011a3b`: `sub 0x100944f0,%eax` then `cltd` -- the high half of the
division result is discarded), and GetMicrosecondsDiff subtracts the low
dwords only. Net effect: **every stored value is (microseconds since DLL
init) mod 2^32, signed**, wrapping every ~35.8 minutes. A diff between two
stores is exact whenever the true difference fits in int32
(+-2^31 us ~ +-2147 s ~ +-35.8 min) -- including across a wrap, because
subtraction is mod-2^32. Differences larger than that come out wrapped.

**Epoch base.** `DAT_100944f0`/`DAT_100944f4` is written by **InitHelperDLL**
(the DLL's ordinal-1 export at `0x1000e260`, called by the host at load): it
captures `_Xtime_get_ticks() / 10` (MSVCP140.dll `_Xtime_get_ticks`,
100 ns ticks since 1601-01-01 UTC, divided via `__alldiv`) into the global,
stores its second argument into `DAT_100944fc` (the `LogVariadic` function
pointer the fork logs through) and its third into `DAT_100944f8`; the first
argument is ignored. Stored values are therefore **microseconds since DLL
initialization** -- not wall clock, not game ticks.

None of these four helpers touch GL state, threads, or locks; the group is
pure named-value timing.

##### Behavior

`FUN_10011690`, registered at `ExposeFunctions` (plain Lua call),
`IEex_Helper_GetMicrosecondsDiff(nameA, nameB)`:

- Pushes `value(nameA) - value(nameB)` (one integer, returns 1). The
  subtraction is 32-bit only: each slot's low dword is read as a signed
  int, high dwords are not consulted.
- Both lookups go through `FUN_100134e0` (lookup-or-create), so diffing two
  never-stored names creates both entries with value 0 and returns 0.

Callers: `IEex_Key_State.lua`, `IEex_UncapFPS.lua` (elapsed-time checks; see
the wrap window in the shared-machinery section for the +-35.8-minute
validity bound).

##### Failure fallbacks

None. Both `lua_tolstring` calls are unchecked: a non-string argument (1 or
2) dereferences NULL. Allocator exceptions are unhandled.

##### Decompiler artifact

- `lua_tolstring` shows the spurious 4th stack-cookie argument (real
  signature `(L, index, NULL)`); the `strlen` loops and the
  `local_44[...]`/`_invoke_watson` dances are MSVC `std::string`
  construction/destruction, not logic.
- `*piVar5 - *piVar4` really is the whole computation -- the missing `[1]`
  reads are the semantics (high dwords ignored), not a decompiler
  omission.
