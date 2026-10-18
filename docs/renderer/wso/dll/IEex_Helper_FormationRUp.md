# IEex_Helper_FormationRUp

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x10033130` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_10033130.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_10033130(undefined4 param_1,undefined4 param_2)

{
  if (DAT_100945f5 != '\0') {
    DAT_100945f5 = 0;
    return;
  }
  FUN_10032f80();
  (*(code *)0x4767e0)(param_2);
  return;
}
```

## Notes

### Shared ui-input machinery (all helpers in this group)

- **Hook shape.** IEex_Gui_Patch.lua installs ten wrappers, only when the
  fork is active (`if not IEex_Vanilla`). Each site is the engine's 5-byte
  `call` in a `CScreenWorld::On*` handler; the patch overwrites it with a
  jump to a DLL-side stub (`IEex_WriteAssemblyAuto`):
  `push ecx` (the `this` register live at that site) / `!call
  >IEex_Helper_<Name>` / jump to the resume address (site + 5). The helper
  is stdcall over the stub's three dwords - the saved `this` plus the
  original call's argument dwords - keeps ECX live, and forwards to a
  hardcoded absolute vanilla address as a thiscall (callee-cleans its
  stack), so the engine path is identical whether or not the helper
  intervened. All engine/method names below come from the staged patch's
  comments; the DLL itself only holds raw addresses.

| Helper (address) | Hook site | Resume | Forwards to (per patch comments) |
|---|---|---|---|
| FormationRDown (0x10032ff0) | 0x68C2C0 | 0x68C2C5 | CGameArea::OnFormationButtonDown 0x476680 |
| FormationRUp (0x10033130) | 0x68C320 | 0x68C325 | CGameArea::OnFormationButtonUp 0x4767e0 |
| ActionDownPhys (0x10033160) | 0x68C0D4 | 0x68C0D9 | CGameArea::OnLButtonDown 0x475440 |
| ActionUpPhys (0x10033180) | 0x68C17D | 0x68C182 | CGameArea::OnLButtonUp 0x475dc0 |
| ActionDblClkPhys (0x100331a0) | 0x68C060 | 0x68C065 | CGameArea::OnLButtonDblClk 0x4765c0 |
| MouseMovePhys (0x100331c0) | 0x68C244 | 0x68C249 | CGameArea::OnMouseMove 0x476970 |
| UIMouseMoveGated (0x100331e0) | 0x68C20F | 0x68C214 | CUIManager::OnMouseMove 0x4d40b0 |
| UILDownGated (0x10033220) | 0x68C0A1 | 0x68C0A6 | CUIManager::OnLButtonDown 0x4d41d0 |
| UIDblClkGated (0x10033260) | 0x68C02D | 0x68C032 | CUIManager::OnLButtonDblClk 0x4d4310 |
| UIRDownGated (0x100332a0) | 0x68C28D | 0x68C292 | CUIManager::OnRButtonDown 0x4d43d0 |

  (`CUIManager::OnLButtonUp` 0x4d42b0 is deliberately not wrapped - the
  patch comments note it is focused-branch only, so a captured control
  always gets its release.)

- **The two cursor spaces.** With UI scaling active at s < 1 the cursor is
  stored LOGICAL over the inflated HUD panels (so engine control hit-tests
  still find relocated buttons) but PHYSICAL everywhere else (so world
  picking stays exact). The transform happens once per mouse move inside
  `CChitin::AsynchronousUpdate`: IEex_UIScale_Patch.lua hooks 0x78F489
  (m_bFullscreen branch, after ScreenToClient at 0x78F41F, before the
  pt-vs-m_ptPointer compare / store at 0x78F4E2) and calls
  **UIScaleCaptureMap** (0x1002c990, ui-scale group) with &pt; the helper
  converts the client point, publishes it to snapshot pair A
  {x `DAT_103ded7c`, y `DAT_104df038`} under seqlock `DAT_104df4e8`
  (validity byte `DAT_104df075`), rewrites *pt only when the mapped point
  lands on an active HUD panel (check `FUN_1002c910`, flag
  `DAT_103dec27`), and arms the phantom gate `DAT_104df576` when the point
  misses every panel while s < 0.999.
- **WorldPtPhysicalize (`FUN_10032f80`)** is what the five world-side
  wrappers call: ECX = &pt; if `pt != 0` and `DAT_103dec27` (cursor over
  scaled HUD) and the engine's current area (`*(*DAT_100944b8 + 0x3c4)`)
  matches the captured area (`*(*DAT_10094384 + 0x1c88)`) and
  `DAT_104df075`, it polls snapshot pair A (see the artifact note) and
  overwrites the point with {snapshot x, snapshot y}. Every gate failing
  means a no-op - the point the engine pushed stays.
- **The phantom UI gate (`DAT_104df576`)** is what the four *Gated
  wrappers test: when armed, the physical cursor must stay out of UI math,
  because a panel's logical rect is 1/s bigger than its drawn image and
  the band between them is a phantom hit zone. The helpers substitute the
  offscreen point **(-32000, -32000)** (0xffff8300 twice) instead of the
  real one unless a control holds mouse capture (see UIMouseMoveGated).
- **Overlay-rect table.** `DAT_103dec28` holds 5-dword entries
  {mode, x, y, w, h}; count at `DAT_104df4dc`, maintained by the ui-scale
  render path. `FUN_10032980` (ECX = x, EDX = y) is the hit test: only
  when the phantom gate is disarmed and the count is non-zero, it requires
  some entry with mode == 1 to exist (the "refonte bars registered"
  marker) and the active area to match the captured area, then walks the
  entries skipping sentinel modes {0, 1, -1, -2, -3} and returns 1 when
  `x0 <= x < x0+w && y0 <= y < y0+h` for any remaining entry.
- **Volatile double-read poll (decompiler artifact).** Ghidra renders the
  snapshot read as `while ((DAT_104df4e8 & 1) != 0) { if (7 < ++i) return; }`
  - a constant bit test with a dead counter. Disassembly
  (0x10032fc0-0x10032fe8, likewise at 0x10033385 in WorldRejectMapCursor
  and FUN_1002a3b0) shows the real loop: read the guard, test bit 0, read
  the snapshot pair, RE-READ the guard, and use the pair only when the two
  guard reads are equal - up to 8 tries, then give up. It is a lock-free
  stable-snapshot poll against UIScaleCaptureMap's `guard += 2` publish,
  not a flag spin.
- **No Lua, no GL.** The ten native hooks touch no lua_State and issue no
  OpenGL calls; they only inspect the flags above, maybe rewrite the
  point, and forward. Threads: they run on whatever engine thread runs the
  `CScreenWorld::On*` handlers (the patch's Lua-side externs that pair
  with them assert `IEex_Thread.Async`); the only cross-thread data is the
  snapshot pair A / seqlock published by the AsynchronousUpdate hook, and
  the swallow flag `DAT_100945f5` - all unsynchronized except for the
  volatile poll.
- **Registration.** The ten native hooks are reached only through raw
  addresses in the Lua registration table (site `FUN_1000e2a0`), used by
  the `!call >IEex_Helper_*` macros; WorldRejectMapCursor is registered as
  a callable Lua shim through `ExposeFunctions`. Logging flows through the
  InitHelperDLL-provided sink; these helpers do not log.

##### Behavior

Native hook. stdcall `(this, pt*)`. If the swallow flag `DAT_100945f5` is
set, clear it and return WITHOUT forwarding - this is the second half of
the pair FormationRDown armed. Otherwise WorldPtPhysicalize the point and
forward to **0x4767e0** (`CGameArea::OnFormationButtonUp` per the patch
comments). Installed at site 0x68C320 (resume 0x68C325) by
`IEex_Gui_Patch.lua`.

##### Failure fallbacks

If the up arrives without a swallowed down (flag clear) the vanilla runs
on the physicalized point.

##### Decompiler artifact

Same as FormationRDown (unused-looking `this`, hidden register moves).

