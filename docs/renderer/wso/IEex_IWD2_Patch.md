# IEex_IWD2_Patch.lua — patch loader / bootstrap

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_IWD2_Patch.lua` (fork commit
`138495fde69b6092b7318fad462d571864e9d9b8`, ref `opengl-hd-beta7`; see
`docs/renderer/wso/scope.md` and `index.md` for provenance).

## Intent

The entry point the game's IEex bootstrap runs first. It does not hook
anything itself; it `dofile()`s every other patch file in a fixed order,
with a reduced list when the global `IEex_Vanilla` is set (the "vanilla
debugging" mode from the top of `IEex_IWD2_State.lua` leaves only
widescreen + FPS-uncap + minor conveniences). Its last act defines the two
allocator labels every other patch's assembly relies on.

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x7FC95B | `unresolved in our tree` (named `_malloc` by the patch; nearest marker below is `src/music/audio.cpp:477`, which is not it — our tree has no marker in the 0x7FC9xx region) | — | — | `IEex_DefineAssemblyLabel("_malloc", 0x7FC95B)` — gives the engine's `operator new` an assembly label so injected stubs can allocate engine-side memory. No bytes written. |
| 0x7FC984 | `unresolved in our tree` (named `_free` by the patch; same caveat — no marker in the 0x7FC9xx region) | — | — | `IEex_DefineAssemblyLabel("_free", 0x7FC984)` — same, for the engine's `operator delete`. No bytes written. |

The patch itself says these two must be "THE LAST LINES EXECUTED DURING
INITIAL STARTUP".

## Data flow

No state of its own. Two globals matter downstream:

- `IEex_Normal_Patch_Files` — the 20-file load order (Core, Trigger, Action,
  Creature, Opcode, Gui, Render, HDTiles, CameraZoom, Markers, UIScale,
  HDSprites, Key, Projectile, Sound, LoadTimes, UncapFPS, Pathfinding,
  MiscFixes, Debug).
- `IEex_Vanilla_Patch_Files` — the 11-file reduced list (Core, Gui, Render,
  HDTiles, CameraZoom, Markers, UIScale, HDSprites, Key, LoadTimes,
  UncapFPS). Note the staged set we were given is exactly
  `IEex_Normal_Patch_Files` minus the four files that do not exist in this
  drop (IEex_Trigger/Action/Opcode/Projectile/Debug patches are absent from
  `~/.local/share/iwd2r/wso/lua/`), plus the two `_State` files that are
  `dofile()`d from other patches rather than listed here.

Note the five staged `_State` files (Core, Creature, Gui, IWD2, Key) are not
in either list — they are pulled in by `dofile` from the patch files and/or
the DLL's Lua state bootstrap, not by this loader.

## IEex_Helper_* entry points

None called, none installed. (`IEex_DefineAssemblyLabel` and the
`IEex_Vanilla` global are core-DLL API, not `IEex_Helper_*`.)
