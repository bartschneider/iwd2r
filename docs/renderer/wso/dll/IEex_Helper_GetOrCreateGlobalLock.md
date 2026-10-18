# IEex_Helper_GetOrCreateGlobalLock

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10011840` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_10011840.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_10011840(undefined4 param_1)

{
  char cVar1;
  char *pcVar2;
  undefined4 uVar3;
  char *pcVar4;
  uint uVar5;
  int iVar6;
  int local_28 [6];
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005acad;
  local_10 = ExceptionList;
  ExceptionList = &local_10;
  pcVar2 = (char *)lua_tolstring(param_1,1,0,DAT_1008f040 ^ (uint)&stack0xfffffffc);
  local_28[4] = 0;
  local_28[5] = 0;
  local_28[0] = 0;
  local_28[1] = 0;
  local_28[2] = 0;
  local_28[3] = 0;
  pcVar4 = pcVar2;
  do {
    cVar1 = *pcVar4;
    pcVar4 = pcVar4 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(pcVar2,(int)pcVar4 - (int)(pcVar2 + 1));
  local_8 = 0;
  uVar3 = FUN_1000bfe0(local_28);
  lua_pushinteger(param_1,uVar3);
  if (0xf < (uint)local_28[5]) {
    uVar5 = local_28[5] + 1;
    iVar6 = local_28[0];
    if (0xfff < uVar5) {
      iVar6 = *(int *)(local_28[0] + -4);
      uVar5 = local_28[5] + 0x24;
      if (0x1f < (local_28[0] - iVar6) - 4U) {
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(iVar6,uVar5);
  }
  ExceptionList = local_10;
  return 1;
}
```

## Notes

### Shared machinery (all five helpers)

**Named-lock registry.** `FUN_1000bfe0` resolves a lock name to a lock object,
creating the entry on first use (decompiled from the fork's
`IEexHelper.fork.dll`; helper itself listed in
`runs/dll-decomp-inner/FUN_1000bfe0.c` / `FUN_1000c550.c`):

- The `std::string` key is hashed with 32-bit FNV-1a (basis `0x811c9dc5`,
  prime `0x01000193` -- the same constants the bridge registry uses) and
  probed in an `std::unordered_map` rooted at `DAT_103d4620` (element count
  `DAT_103d4624`, bucket array `DAT_103d4628`).
- On a miss the code allocates a `0x50`-byte node: an `std::string` copy of
  the name lives at node+0x08 and the lock object sits inline at node+0x20
  (0x30 bytes). At creation the object is zeroed except `obj+0x00 = 2`,
  `obj+0x28 = 0xffffffff` and `obj+0x2c = 0`.
- `FUN_1000bfe0` returns `node + 0x20` -- the lock-object address. In the
  decompile this reads as `return *piVar1 + 0x20;` because `piVar1` points at
  the `{node, created}` out-struct, so `*piVar1` is the node pointer.
- The registry map itself is not guarded by any lock (insert and lookup run
  unsynchronized, exactly like the bridge registry at `DAT_103d4700`).

**Acquire path.** `_Mtx_lock` and `_Mtx_unlock` are imports from
`MSVCP140.dll`. Both fork lock paths wrap `_Mtx_lock` with one custom check:

```c
if (_Mtx_lock(obj) != 0) {
    std::_Throw_Cpp_error(5);
}
if (*(int *)(obj + 0x2c) == 0x7fffffff) {
    *(int *)(obj + 0x2c) = 0x7ffffffe;
    std::_Throw_Cpp_error(6);
}
```

`obj+0x2c` is inside the `MSVCP140` lock object (0 at creation, only touched
by the runtime afterwards). The `0x7fffffff` tripwire turns a would-be
overflow of that counter into an exception on the *next* acquire. The
integer codes 5 and 6 are `std::_Throw_Cpp_error`'s -- i.e. MSVC
`std::system_error`-family failures -- **thrown as C++ exceptions**, never
returned to Lua as errors. Surface that deliberately in a reimplementation;
the DLL does not catch or translate them.

**Two exposure paths.** The fork init (`FUN_1000e2a0`) fills a Lua table with
`"IEex_Helper_<Name>" -> <raw code address as integer>` entries
(`lua_pushinteger` + `lua_settable`), while `ExposeFunctions` registers
callable `lua_CFunction` shims under the same name strings. The staged Lua
patches call the shims with plain Lua calls and consume the integer entries
through IEex's `!call >IEex_Helper_<Name>` raw-call macro, which reads the
integer and performs a machine-level call. LockGlobal/UnlockGlobal exist in
both tables (callable shim *and* raw address); the *Direct variants and
GetOrCreateGlobalLock exist in only one.

##### Behavior

`FUN_10011840` (registered at `ExposeFunctions`, called from plain Lua):
`lua_tolstring(L, 1)` gives the lock name, the code builds an `std::string`
from it, `FUN_1000bfe0` resolves-or-creates the lock object, and the object's
address is pushed with `lua_pushinteger(L, <address>)`. Returns 1 value.

- Creating an entry does **not** acquire the lock; it only materializes the
  0x30-byte object in the registry node.
- The returned integer is a raw heap address that stays stable for the life
  of the process; the intended usage is
  `IEex_Helper_LockGlobalDirect(IEex_Helper_GetOrCreateGlobalLock("name"))`
  (see `IEex_Gui_Patch.lua`, `local lock_... = IEex_Helper_GetOrCreateGlobalLock("IEex_CScreenSpell_m_lPopupStack")`).
- Because the registry is global to the DLL (not per Lua state), two Lua
  states asking for the same name get the same lock object.

##### Failure fallbacks

None. Argument 1 is passed to `lua_tolstring` unchecked: a non-string/number
yields NULL and the subsequent string copy dereferences it. Registry
allocation failure is unhandled (operator new throws).

##### Decompiler artifact

`lua_tolstring` appears with a spurious 4th argument (the stack-cookie XOR);
the real signature is `(L, index, NULL)`. The `strlen` loop and the
`local_28[...]` dance after the call are ordinary MSVC `std::string`
construction/destruction, not logic.
