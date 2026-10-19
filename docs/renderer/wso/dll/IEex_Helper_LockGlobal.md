# IEex_Helper_LockGlobal

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1000b640` (Lua registration: FUN_1000e2a0)  |  Address `0x10011980` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_1000b640.c`, `runs/dll-decomp/FUN_10011980.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`, `IEex_Gui_State.lua`, `IEex_Gui_State.upstream.lua`, `IEex_Key_State.lua`, `IEex_Render_Patch.lua`, `IEex_Render_Patch.upstream.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C — `FUN_1000b640.c`

```c
void FUN_1000b640(char *param_1)

{
  char cVar1;
  uint uVar2;
  int iVar3;
  int iVar4;
  char *pcVar5;
  uint uVar6;
  uint uVar7;
  uint local_28 [6];
  void *local_10;
  undefined1 *puStack_c;
  undefined4 local_8;

  local_8 = 0xffffffff;
  puStack_c = &LAB_1005a5ad;
  local_10 = ExceptionList;
  uVar2 = DAT_1008f040 ^ (uint)&stack0xfffffffc;
  ExceptionList = &local_10;
  local_28[4] = 0;
  local_28[0] = 0;
  local_28[1] = 0;
  local_28[2] = 0;
  local_28[3] = 0;
  local_28[5] = 0;
  pcVar5 = param_1;
  do {
    cVar1 = *pcVar5;
    pcVar5 = pcVar5 + 1;
  } while (cVar1 != '\0');
  FUN_100035c0(param_1,(int)pcVar5 - (int)(param_1 + 1));
  local_8 = 0;
  iVar3 = FUN_1000bfe0(local_28);
  local_8 = 0xffffffff;
  if (0xf < local_28[5]) {
    uVar6 = local_28[5] + 1;
    uVar7 = local_28[0];
    if (0xfff < uVar6) {
      uVar7 = *(uint *)(local_28[0] - 4);
      uVar6 = local_28[5] + 0x24;
      if (0x1f < (local_28[0] - uVar7) - 4) {
                    /* WARNING: Subroutine does not return */
        _invoke_watson((wchar_t *)0x0,(wchar_t *)0x0,(wchar_t *)0x0,0,0);
      }
    }
    FUN_100585f9(uVar7,uVar6,uVar2);
  }
  local_28[4] = 0;
  local_28[5] = 0xf;
  local_28[0] = local_28[0] & 0xffffff00;
  iVar4 = _Mtx_lock(iVar3);
  if (iVar4 != 0) {
    std::_Throw_Cpp_error(5);
  }
  if (*(int *)(iVar3 + 0x2c) == 0x7fffffff) {
    *(undefined4 *)(iVar3 + 0x2c) = 0x7ffffffe;
    std::_Throw_Cpp_error(6);
  }
  ExceptionList = local_10;
  return;
}
```

## Decompiled C — `FUN_10011980.c`

```c
undefined4 FUN_10011980(undefined4 param_1)

{
  undefined4 uVar1;

  uVar1 = lua_tolstring(param_1,1,0);
  FUN_1000b640(uVar1);
  return 0;
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

Two addresses carry this name:

- `0x10011980` (`ExposeFunctions`) -- `FUN_10011980`, a thin callable shim:
  `lua_tolstring(L, 1)` -> `FUN_1000b640(<char*>)` -> returns 0 (no results).
- `0x1000b640` (init-site table) -- `FUN_1000b640` itself, the core
  `void lock_by_name(const char*)`, stored as a raw integer address.

The core builds an `std::string`, resolves the lock object via
`FUN_1000bfe0`, and acquires it with `_Mtx_lock` plus the tripwire shown
under Shared machinery. It blocks until the lock is available; there is no
timeout or try-acquire variant anywhere in this group.

The staged patches use the name-based pair for short critical sections
across six patch files (`IEex_Gui_Patch.lua`, `IEex_Gui_State.lua`,
`IEex_Gui_State.upstream.lua`, `IEex_Key_State.lua`,
`IEex_Render_Patch.lua`, `IEex_Render_Patch.upstream.lua`), e.g. around
bridge access; the DLL keeps one lock object per name, so differently named
sections do not exclude each other.

##### Failure fallbacks

- `_Mtx_lock` failing throws `_Throw_Cpp_error(5)`.
- The `obj+0x2c == 0x7fffffff` tripwire writes `0x7ffffffe` and throws
  `_Throw_Cpp_error(6)`.
- Non-string argument 1: NULL dereference in the shim (no type check).
- Nothing is caught here; a C++ exception unwinds out of the lua_CFunction.

##### Decompiler artifact

The shim's `lua_tolstring` shows the same spurious 4th stack-cookie argument
as GetOrCreateGlobalLock.
