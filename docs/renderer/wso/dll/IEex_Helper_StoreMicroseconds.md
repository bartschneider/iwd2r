# IEex_Helper_StoreMicroseconds

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011a00` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011a00.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Key_State.lua`, `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_10011a00(undefined4 param_1)

{
  char cVar1;
  int iVar2;
  char *pcVar3;
  int *piVar4;
  char *pcVar5;
  uint uVar6;
  undefined8 uVar7;
  int local_2c [7];
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005acdd;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  uVar7 = _Xtime_get_ticks(DAT_1008f040 ^ (uint)&stack0xfffffffc);
  iVar2 = __alldiv(uVar7,10,0);
  iVar2 = iVar2 - _DAT_100944f0;
  local_2c[6] = iVar2 >> 0x1f;
  pcVar3 = (char *)lua_tolstring(param_1,1,0);
  local_2c[4] = 0;
  local_2c[5] = 0;
  local_2c[0] = 0;
  local_2c[1] = 0;
  local_2c[2] = 0;
  local_2c[3] = 0;
  pcVar5 = pcVar3;
  do {
    cVar1 = *pcVar5;
    pcVar5 = pcVar5 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(pcVar3,(int)pcVar5 - (int)(pcVar3 + 1));
  local_8 = 0;
  piVar4 = (int *)FUN_100134e0(local_2c);
  *piVar4 = iVar2;
  piVar4[1] = local_2c[6];
  if (0xf < (uint)local_2c[5]) {
    uVar6 = local_2c[5] + 1;
    iVar2 = local_2c[0];
    if (0xfff < uVar6) {
      iVar2 = *(int *)(local_2c[0] + -4);
      uVar6 = local_2c[5] + 0x24;
      if (0x1f < (local_2c[0] - iVar2) - 4U) {
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(iVar2,uVar6);
  }
  ExceptionList = local_10;
  return 0;
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

`FUN_10011a00`, registered at `ExposeFunctions` (plain Lua call),
`IEex_Helper_StoreMicroseconds(name)`:

- `t = _Xtime_get_ticks() / 10` (microseconds since 1601), minus the
  epoch-base low dword, stored (lookup-or-create) into `registry[name]` as
  the 64-bit pair `{lo = diff, hi = sign(diff)}`. Returns 0 values.
- The store overwrites any previous value for the name; absent names are
  created on the spot.

Usage pattern in the staged patches: store under a name, later read the
elapsed time with `IEex_Helper_GetMicrosecondsDiff(name, name2)` (see
`IEex_UncapFPS.lua` for the FPS-loop timestamps and `IEex_Key_State.lua`).

##### Failure fallbacks

No failure paths at all: no "disabled"/log strings, no GL, no locks. The
only hazards are the unchecked `lua_tolstring` (a non-string argument 1
dereferences NULL) and unhandled allocator exceptions.

##### Decompiler artifact

- `_Xtime_get_ticks(DAT_1008f040 ^ (uint)&stack0xfffffffc)` -- the stack
  cookie rendered as an argument; the real signature is `void`.
- `local_2c[6] = iVar2 >> 0x1f` is `cltd` (sign-extend `eax` into `edx`),
  not part of the epoch math -- it is exactly where the 32-bit wrap
  semantics of the shared-machinery section come from.
- Ghidra's "Globals starting with '_' overlap smaller symbols" warning on
  `_DAT_100944f0`: read it as the low dword of the 64-bit base whose high
  dword is `DAT_100944f4` (only the low dword is ever read).
