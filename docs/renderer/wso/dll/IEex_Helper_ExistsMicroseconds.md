# IEex_Helper_ExistsMicroseconds

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011540` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011540.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011540(undefined4 param_1)

{
  byte *pbVar1;
  char cVar2;
  undefined4 ***pppuVar3;
  char *pcVar4;
  int iVar5;
  char *pcVar6;
  uint uVar7;
  uint uVar8;
  undefined4 ****ppppuVar9;
  undefined4 ***pppuStack_40;
  undefined4 uStack_3c;
  undefined4 uStack_38;
  undefined4 uStack_34;
  uint uStack_30;
  uint uStack_2c;
  undefined1 auStack_18 [20];

  pcVar4 = (char *)lua_tolstring(param_1,1,0);
  uStack_30 = 0;
  uStack_2c = 0;
  pppuStack_40 = (undefined4 ****)0x0;
  uStack_3c = 0;
  uStack_38 = 0;
  uStack_34 = 0;
  pcVar6 = pcVar4;
  do {
    cVar2 = *pcVar6;
    pcVar6 = pcVar6 + 1;
  } while (cVar2 != '\0');
  FUN_100035c0(pcVar4,(int)pcVar6 - (int)(pcVar4 + 1));
  pppuVar3 = pppuStack_40;
  uVar8 = 0x811c9dc5;
  ppppuVar9 = &pppuStack_40;
  if (0xf < uStack_2c) {
    ppppuVar9 = (undefined4 ****)pppuStack_40;
  }
  uVar7 = 0;
  if (uStack_30 != 0) {
    do {
      pbVar1 = (byte *)((int)ppppuVar9 + uVar7);
      uVar7 = uVar7 + 1;
      uVar8 = (*pbVar1 ^ uVar8) * 0x1000193;
    } while (uVar7 < uStack_30);
  }
  iVar5 = FUN_10013bb0(auStack_18,&pppuStack_40,uVar8);
  lua_pushboolean(param_1,*(int *)(iVar5 + 4) != 0);
  if (0xf < uStack_2c) {
    uVar8 = uStack_2c + 1;
    ppppuVar9 = (undefined4 ****)pppuVar3;
    if (0xfff < uVar8) {
      ppppuVar9 = (undefined4 ****)pppuVar3[-1];
      uVar8 = uStack_2c + 0x24;
      if (0x1f < (uint)((int)pppuVar3 + (-4 - (int)ppppuVar9))) {
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(ppppuVar9,uVar8);
  }
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

`FUN_10011540`, registered at `ExposeFunctions` (plain Lua call),
`IEex_Helper_ExistsMicroseconds(name)`:

- Find-only: builds the name string, hashes it (FNV-1a inlined here rather
  than calling `FUN_100134e0`), calls `FUN_10013bb0`, and pushes
  `lua_pushboolean(L, found)`. Returns 1 value.
- The only member of the group that never mutates the registry -- a miss
  creates nothing and simply pushes `false`.

Caller: `IEex_UncapFPS.lua` (guards first-use before Store/Diff).

##### Failure fallbacks

None. The unchecked `lua_tolstring` dereferences NULL on a non-string
argument 1 (the string build runs before the hash).

##### Decompiler artifact

- The `pppuVar3`/`ppppuVar9` pointer noise and the `if (0xf < uStack_2c)`
  checks are the `std::string` SBO internals, not logic.
- `*(int *)(iVar5 + 4)` is the `found` byte of the `{node, found}`
  out-struct (`iVar5` points at the out-struct, +4 is the flag).
