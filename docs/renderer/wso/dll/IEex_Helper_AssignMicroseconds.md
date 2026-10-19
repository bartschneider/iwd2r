# IEex_Helper_AssignMicroseconds

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100113c0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_100113c0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_UncapFPS.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_100113c0(undefined4 param_1)

{
  char cVar1;
  uint uVar2;
  char *pcVar3;
  undefined4 *puVar4;
  char *pcVar5;
  uint uVar6;
  int iVar7;
  int local_48 [13];
  char *local_14;
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005ac35;
  local_10 = ExceptionList;
  uVar2 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  ExceptionList = &local_10;
  pcVar3 = (char *)lua_tolstring(param_1,2,0,uVar2);
  local_48[4] = 0;
  local_48[5] = 0;
  local_48[0] = 0;
  local_48[1] = 0;
  local_48[2] = 0;
  local_48[3] = 0;
  local_14 = pcVar3 + 1;
  pcVar5 = pcVar3;
  do {
    cVar1 = *pcVar5;
    pcVar5 = pcVar5 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(pcVar3,(int)pcVar5 - (int)local_14);
  local_8 = 0;
  puVar4 = (undefined4 *)FUN_100134e0(local_48);
  local_14 = (char *)*puVar4;
  local_48[0xc] = puVar4[1];
  pcVar3 = (char *)lua_tolstring(param_1,1,0,uVar2);
  local_48[10] = 0;
  local_48[0xb] = 0;
  local_48[6] = 0;
  local_48[7] = 0;
  local_48[8] = 0;
  local_48[9] = 0;
  pcVar5 = pcVar3;
  do {
    cVar1 = *pcVar5;
    pcVar5 = pcVar5 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(pcVar3,(int)pcVar5 - (int)(pcVar3 + 1));
  local_8 = CONCAT31(local_8._1_3_,1);
  puVar4 = (undefined4 *)FUN_100134e0(local_48 + 6);
  *puVar4 = local_14;
  puVar4[1] = local_48[0xc];
  if (0xf < (uint)local_48[0xb]) {
    uVar6 = local_48[0xb] + 1;
    uVar2 = local_48[6];
    if (0xfff < uVar6) {
      uVar2 = *(uint *)(local_48[6] - 4);
      uVar6 = local_48[0xb] + 0x24;
      if (0x1f < (local_48[6] - uVar2) - 4) goto LAB_10011503;
    }
    FUN_100585f9(uVar2,uVar6);
  }
  local_48[10] = 0;
  local_48[0xb] = 0xf;
  local_48[6] = local_48[6] & 0xffffff00;
  if (0xf < (uint)local_48[5]) {
    uVar2 = local_48[5] + 1;
    iVar7 = local_48[0];
    if (0xfff < uVar2) {
      iVar7 = *(int *)(local_48[0] + -4);
      uVar2 = local_48[5] + 0x24;
      if (0x1f < (local_48[0] - iVar7) - 4U) {
LAB_10011503:
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(iVar7,uVar2);
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
stores its second argument into `DAT_100944fc` (a host-provided logging
function pointer the fork logs through all over its code) and its third into
`DAT_100944f8` (the sink the DLL's exported `LogVariadic` forwards to); the first
argument is ignored. Stored values are therefore **microseconds since DLL
initialization** -- not wall clock, not game ticks.

None of these four helpers touch GL state, threads, or locks; the group is
pure named-value timing.

##### Behavior

`FUN_100113c0`, registered at `ExposeFunctions` (plain Lua call),
`IEex_Helper_AssignMicroseconds(dst, src)`:

- **The second argument is the source**: it copies `value(src)` into
  `value(dst)` (arg 2 is read into a temp pair first, then written to arg
  1's slot). Returns 0 values.
- Both slots resolve through `FUN_100134e0` (lookup-or-create): assigning
  from a never-stored name creates that name with value 0 and zeroes the
  destination.
- The copy is nominally 64-bit (`slot[0]` and `slot[1]`), but since stored
  high dwords are always sign extensions (see shared machinery) this is an
  int32 copy in practice.

Caller: `IEex_UncapFPS.lua` (moves a timestamp without losing it across a
phase where the original name would be re-stored).

##### Failure fallbacks

None. Either `lua_tolstring` (arg 1 or arg 2) dereferences NULL on a
non-string. Allocator exceptions are unhandled.

##### Decompiler artifact

Same SBO/`lua_tolstring` noise as the group; `local_48[0xc] = puVar4[1]`
is the int64 high-dword temp copy, not extra logic.
