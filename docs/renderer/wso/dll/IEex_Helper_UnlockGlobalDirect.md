# IEex_Helper_UnlockGlobalDirect

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1000b870` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1000b870.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1000b870(undefined4 param_1)

{
  _Mtx_unlock(param_1);
  return;
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

`FUN_1000b870`, registered only in the init-site table as a raw address;
the staged patches invoke it via `!call >IEex_Helper_UnlockGlobalDirect`.
The entire body:

```c
void FUN_1000b870(undefined4 obj)
{
  _Mtx_unlock(obj);
}
```

The barest of the five: no name lookup, no handle validation, no error
handling.

##### Failure fallbacks

None. As with LockGlobalDirect, a bogus handle crashes inside `MSVCP140`.

##### Decompiler artifact

None beyond the shared ones.
