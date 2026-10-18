# IEex_LoadTimes_Patch.lua — load-time shortening

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_LoadTimes_Patch.lua`.
Fork-authored (its header comment: "speeds up loading by caching non-player
scripts and by removing some SleepEx() calls that seem to have no effect
other than slowing down the load process"). Not renderer work, but it
documents several load-path functions our tree only partially implements.

## Intent

Cut area/game load wall time. Two mechanisms: (1) route the engine's
`sscanf` through the DLL's modern CRT implementation and cache non-player
`CAIScript`s instead of re-reading them; (2) NOP out the `SleepEx`-style
pacing calls sprinkled through the load path. Gated by
`[IEex Options] Fast Loading` in `Icewind2.ini` (default 1; 0 restores the
engine's own pacing — the file's comment calls it "a diagnostic lever, not a
tuning knob: it does not affect WHETHER the loading screen appears, only for
how long").

## Hooks table

Function names in the left column come from the patch's own comments where
the exact hook address has no marker of its own; the enclosing function is
identified by our tree's nearest preceding address marker — the mid-function
hook sites themselves are `unresolved in our tree` (our sources carry
function-start markers only).

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x7E7B99 | `unresolved in our tree` (no marker anywhere near; patch comment: "Modern version of sscanf() is slightly faster") | — | `IEex_Helper_sscanf` | `!jmp_dword` — redirects a call target into the DLL's `sscanf` replacement. |
| 0x45FE00 | `CAIScript::Construct` equivalent — the patch replaces the whole prologue; our marker is exactly `CAIScript.cpp:41` (next line is the function body start) | src/CAIScript.cpp:41 | `IEex_Helper_CAIScript_ConstructCached` | Function head overwritten with `lea/push/push/call helper/ret 8`: non-player script construction now goes through the DLL's script cache (reads `CAIScript::Read`'s own arguments off the stack). |
| 0x5A8338 | inside `CInfGame::Unmarshal` | src/CInfGame.cpp:2406 (function marker `// 0x5A7E40` at 2405) | — | `!add(esp,8)` + 3×NOP — removes a stack-arg call (pacing/cleanup) in the unmarshal loop. |
| 0x5A89C4 | inside `CInfGame::Unmarshal` | src/CInfGame.cpp:2406 | — | same removal, second site. |
| 0x5A921D | inside `CInfGame::Unmarshal` | src/CInfGame.cpp:2406 | — | same removal, third site. |
| 0x54FD8F | inside `CVariableHash::AddKey` | src/CVariableHash.cpp:137 (marker 136) | — | same removal (patch labels it "CVariableHash_Unknown()"). |
| 0x4408A6 | inside `CCacheStatus::Update` | src/CCacheStatus.cpp:227 (marker 226) | — | 5×NOP. |
| 0x4413AC | inside `CCacheStatus::Update` | src/CCacheStatus.cpp:227 | — | 5×NOP. |
| 0x6F242F | inside `CGameSprite::CGameSprite` (creature ctor) | src/CGameSprite.cpp:697 (marker 696) | — | `IEex_HookRestore(0x6F242F, 2, 7, {"!add(esp,8)"})` — rewrites 7 displaced bytes, new code 2 bytes: pops 8 bytes of args after an inner call. |
| 0x6F2458 | inside `CGameSprite::CGameSprite` | src/CGameSprite.cpp:697 | — | 5×NOP. |
| 0x6F2464 | inside `CGameSprite::CGameSprite` | src/CGameSprite.cpp:697 | — | 5×NOP. |
| 0x5A25C8 | inside `CInfGame::LoadArea` | src/CInfGame.cpp:3145 (marker 3144) | — | `!add(esp,8)` + 3×NOP. |
| 0x5A287A | inside `CInfGame::LoadArea` | src/CInfGame.cpp:3145 | — | 12×NOP. |
| 0x5A30AE | inside `CInfGame::LoadArea` | src/CInfGame.cpp:3145 | — | `!add(esp,8)` + 3×NOP. |
| 0x5AB2B2 | inside `CInfGame::LoadGame` | src/CInfGame.cpp:4347 (marker 4346) | — | `!add(esp,8)` + 3×NOP. |
| 0x5AB89F | inside `CInfGame::LoadGame` | src/CInfGame.cpp:4347 | — | `!add(esp,8)` + 3×NOP. |
| 0x5A9761 | inside `CInfGame::ProgressBarCallback` | src/CInfGame.cpp:3581 (marker 3580) | — | `!add(esp,8)` + 3×NOP. |
| 0x5CB040 | inside `CInfTileSet::~CInfTileSet` | src/CInfinity.cpp:170 (marker 169) | — | 9×NOP (patch label: "CInfTileSet_Destruct()"). |
| 0x40F7BD | inside `CAIScript::Read` | src/CAIScript.cpp:121 (marker 120) | — | 5×NOP. |
| 0x46F3CE | inside `CGameArea::AIUpdate` | src/CGameArea.cpp:1891 (marker 1890) | — | 12×NOP — patch comment: "Long hang when entering an area". |
| 0x474EF0 | inside `CGameArea::ProgressBarCallback` | src/CGameArea.cpp:4424 (marker 4423) | — | `!add(esp,4)` + 3×NOP. |
| 0x5BEC07 | inside `CInfGame::SynchronousUpdate` (autosave path) | src/CInfGame.cpp:6758 (marker 6757) | — | 8×NOP. |

## Data flow

No persistent state. The only state is the DLL-side `CAIScript` cache that
`IEex_Helper_CAIScript_ConstructCached` maintains (transparent to Lua) and
the `Fast Loading` ini switch read once at patch time. Everything else is
static machine-code surgery applied inside `IEex_DisableCodeProtection()` /
`IEex_EnableCodeProtection()`.

## IEex_Helper_* entry points

- `IEex_Helper_sscanf` — DLL export; drop-in replacement for the engine's
  CRT `sscanf`, jumped to from 0x7E7B99.
- `IEex_Helper_CAIScript_ConstructCached` — DLL export; `__fastcall`-style
  replacement for the non-player `CAIScript` constructor: `lea eax,[esp+4];
  push eax; push ecx; call helper; ret 8` — caches the parsed script in the
  DLL so repeated constructions of the same non-player script resref stop
  hitting disk/parse.
