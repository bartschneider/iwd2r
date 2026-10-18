# IEex_Creature_State.lua — creature Lua-state lifecycle and per-tick externs

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Creature_State.lua`. The
DLL↔Lua seam for per-creature state. Not one engine byte is patched here:
the file DEFINES 14 `IEex_Extern_*` functions that the DLL invokes BY NAME
(the same mechanism as the engine's `IEex_Extern_CheckScroll`), five
bridges of shared state, and a small public Lua API
(`IEex_RegisterLuaStat` / `IEex_Access*LuaStats` / `IEex_Ensure*EffectListProcessed`).

## Intent

1. **Per-object Lua state with engine-mirrored lifetime.** The
   `IEex_GameObjectData` bridge (keyed by actor ID) is created per creature
   on add, torn down on delete, and cleared wholesale when the game's
   object lists are cleaned. Its one stock field, `bEffectsHandled`,
   records whether the engine has already run the creature's effect list —
   the basis for deferred processing.
2. **A Lua extension of `CDerivedStats`.** Any Lua file may call
   `IEex_RegisterLuaStat{init, reload, copy, cleanup}`; the registry is a
   bridge (`IEex_RegisteredLuaStats`), and per-`CDerivedStats` storage
   lives in the `IEex_DerivedStatsData` bridge keyed by the CDerivedStats
   POINTER. The four lifecycle externs (construct / reload / operator= /
   destruct) run every registered stat's corresponding function, so
   Lua-side stats copy and reset exactly when the engine's own stats do.
   The accessors mirror the engine's gate `m_bAllowEffectListCall`
   (src/CGameSprite.cpp:12327 selects `m_derivedStats` when it is set,
   `m_tempStats` otherwise): +0x920 `m_derivedStats` vs +0x1778
   `m_tempStats`, and the fork tests the same flag at +0x72A4 to choose.
3. **Deferred effect processing.** `IEex_EnsureSpriteEffectListProcessed`
   forces the engine's `CGameSprite::ProcessEffectList` to run NOW — write
   1 to the "effects changed" flag +0x562C (`field_562C`, which
   ProcessEffectList itself consumes at src/CGameSprite.cpp:15176) then
   call it — so Lua can read post-effect state at moments the engine has
   not reached yet (sprite construction, async access from other threads).
4. **Per-tick creature fixups** (`IEex_Extern_OnPostCreatureProcessEffectList`,
   thread Async, the file's centre of gravity, ~285 lines): replacement
   animations (with a paired USPOLYMO opcode-53 effect so the change
   survives), uncapped ability-score words (with the barbarian STR-cap
   carve-out), Courteous Magocracy charisma-bonus cleanup, first-PC
   per-party init (teleport-room-string dedup, dead-PC equipment record,
   reform-party button, reloading the 31 `EX_GLOBEF` global effect timers
   into `ex_global_effect_timers`), sanitisation of the extra-flags dword,
   constant-ID issuance plus back-fill of old saves' effect source IDs,
   and the opcode-500 on-tick machinery: effects whose RESOURCE names a
   Lua function fire that function each tick (with `IEex_IterateActorEffects`
   state flags driving opcode-288 MEINCAPA and opcode-254/288 USINVSNA
   maintenance, and a flag-0x80 "used this tick" handshake with an
   early-out for fully-static creatures).
5. **Item usability override** (`IEex_Extern_CheckItemUsable`): replaces
   the engine's usability verdict when armor-restriction-during-combat or
   the Use Magic Device feature is active — armor item types are refused
   with a strref while combat is on, UMD below the item's DC is refused
   with the DC tokenised into USIUVAL1, and everything else falls through
   to the original `CInfGame::CheckItemUsable` by label.

## Hooks table

None — the file patches no code bytes. Every engine touch is a Lua
read/write of creature memory, an `IEex_Call`, or a helper invocation the
DLL performs:

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| — | (no hook sites; DLL calls the `IEex_Extern_*` functions by name) | — | — | — |

Named engine references (all resolved in our tree unless stated):

- `IEex_Call(0x72DE60, {}, sprite, 0x0)` → `CGameSprite::ProcessEffectList()`
  — src/CGameSprite.cpp:15154 (exact marker; decl follows). Called from
  both `IEex_Ensure*` helpers after `IEex_WriteDword(sprite + 0x562C, 1)`;
  `field_562C` is declared at src/CGameSprite.h:964 and consumed inside
  ProcessEffectList at src/CGameSprite.cpp:15176 / cleared at 15218.
- `IEex_Label("CInfGame::CheckItemUsable")` → 0x5BA080 —
  src/CInfGame.cpp:6330 (exact marker at 6329). The extern's original-
  behaviour tail: `IEex_Call(label, {0, strDesc, item, sprite},
  IEex_GetGameData(), 0x0) ~= 0`.
- 0x7FB3E3 — commented `CPtrList_RemoveAt` in the commented-out effect-
  node removal path (IEex_Creature_State.lua:340): `IEex_Call(0x7FB3E3,
  {node}, list, 0x0)`. Unresolved in our tree at that exact address
  (nearest preceding marker is inside src/music/audio.cpp:477); cited
  only from dead code, so it never executes as staged.
- Creature share offsets (all on `CGameSprite` unless noted):
  +0x4 `m_objectType` (src/CGameObject.h:75; 0x31 = sprite — the
  `IEex_IsSprite` guard), +0x5C `m_id` (src/CGameObject.h:83 — the actor
  ID the bridges are keyed by), +0x12 `m_pArea` (src/CGameObject.h:78;
  read in CheckItemUsable, +0xB16 into the area = in-combat byte),
  +0xE `m_posZ` (src/CGameObject.h:77 — the fork's visual-height
  early-out test), +0x5A4 `m_baseStats` (src/CGameSprite.h:788,
  `CCreatureFileHeader`, src/FileFormat.h:436 — animation +0x5C4,
  constantID +0x700, speed +0x722, extra-flags +0x740 and the +0x8A0
  spell-state byte all live INSIDE it), +0x920 `m_derivedStats`
  (src/CGameSprite.h:789 — ability words +0x974, Courteous Magocracy
  word +0x9FA inside it), +0x1778 `m_tempStats` (src/CGameSprite.h:790),
  +0x5622 `m_attackFrame` (src/CGameSprite.h:961), +0x562C `field_562C`
  (src/CGameSprite.h:964), +0x72A4 `m_bAllowEffectListCall`
  (src/CGameSprite.h:1044).
- The `CDerivedStats` lifecycle the derived-stats externs hang off:
  ctor 0x443B30 (src/CDerivedStats.cpp:11), dtor 0x5D6650
  (src/CDerivedStats.cpp:66), `Reload` 0x4440F0 (src/CDerivedStats.cpp:71),
  `operator=` 0x445330 (src/CDerivedStats.cpp:411). The DLL must invoke
  `IEex_Extern_OnConstructDerivedStats` / `...OnDestructDerivedStats` /
  `...OnReloadDerivedStats` / `...OnDerivedStatsOperatorEqu` from these
  four bodies; the Lua side pins only the names and thread contracts.
- Actionbar/creature-data offsets quoted by the small predicates:
  creatureData+0x24 (actionbar button type, RestrictCreatureActionbar),
  +0x740 bit 0x400000 (actionbar-restriction override flag).

## Data flow

No Lua-side closures own state; everything lives in bridges keyed by
engine objects, plus file-scope `ex_*` tables. Per-bridge:

- `IEex_RegisteredLuaStats` — array of `{init, reload, copy, cleanup}`
  function-NAME records. Writers: `IEex_RegisterLuaStat` (inside
  `IEex_Helper_SynchronizedBridgeOperation`, appended with
  `IEex_AppendBridgeTableNL`). Readers: the four derived-stats externs.
- `IEex_DerivedStatsData` — keyed by the `CDerivedStats` pointer
  (`stats` argument the DLL passes straight from its callsite inside the
  ctor/dtor/Reload/operator= bodies). Created on construct
  (`IEex_Helper_GetBridgeCreateNL`), every registered stat's init/reload/
  copy/cleanup runs against it; erased on destruct. Access path for
  gameplay code: `IEex_AccessSpriteLuaStats(sprite)` /
  `IEex_AccessActorLuaStats(actorID)` — derived-stats entry when
  `ReadDword(sprite+0x72A4)` (`m_bAllowEffectListCall`) == 1 and the
  sprite is a sprite, else the temp-stats entry.
- `IEex_GameObjectData` — keyed by actor ID
  (`IEex_GetActorIDShare`). `bEffectsHandled` written true by
  `IEex_Extern_OnPostCreatureHandleEffects`; read by the Ensure helpers;
  erased per-actor on delete; the whole bridge cleared on
  `IEex_Extern_OnGameObjectsBeingCleaned` (thread Both).
- `IEex_ConstantID` — constantID → actor ID, issued from
  `IEex_Extern_OnPostCreatureProcessEffectList` (constantID == -1 → take
  the `EX_CONSTANT_ID` global, +1, store back), erased on object delete.
  Purpose: effects carry the ISSUER's ID in +0x68; the back-fill loop
  copies `IEex_ConstantID[eData+0x68]` into `eData+0x110` (sourceID) for
  effects whose sourceID is ≤ 0, so per-creature Lua can attribute them
  after a save/load.
- `IEex_EnlargedAnimation` — erased on object delete (defined/written
  elsewhere in the fork's Lua, not in this file).
- `IEex_Options` / the options bridge — READ only
  (`IEex_Helper_GetBridge("IEex_Options", "options")`, then
  `GetBridge(options, "preventEquippingArmorDuringCombat")`); created
  elsewhere (B1: the DLL options machinery).

Per-frame reader: `IEex_Extern_OnPostCreatureProcessEffectList`
(thread Async — the DLL calls it after each creature's effects are
handled; the matching engine callsite is B1's to pin). Tick counter via
`IEex_GetGameTick` (MEINCAPA / USINVSNA maintenance runs on
`tick % 3 == 0`); `ex_global_effect_timers[bt+1]` reloads from the
`EX_GLOBEF0..30` game globals on first-PC init; opcode-500 on-tick
functions are looked up by effect RESOURCE string in
`ex_on_tick_functions` (falling back to `ex_on_tick_default_functions`),
called as `_G[resource](effectData, creatureData, true)` where effectData
is the effect node base — the function returns true to mark the effect
"used" (flag 0x80 in internal_flags at eData+0xD8).

Honest gaps (staged-drop evidence): `ex_on_tick_functions`,
`ex_on_tick_default_functions` and `ex_replacement_animations` are never
ASSIGNED in the staged Lua (only read here and at
IEex_Key_State.lua:651-652); `ex_tra_55821` / `ex_tra_55816` (the two
usability strrefs) and `ex_enable_use_magic_device_item_usability` appear
NOWHERE else in the staged drop; `ex_full_ability_score_cap` /
`ex_barbarian_exceed_strength_cap_level` /
`ex_courteous_magocracy_charisma_bonus` are likewise only read (their
value-setting code sits in data/state files not staged). `ex_no_teleport_room_strings_printed`
is created at IEex_IWD2_State.lua:22169 and `ex_global_effect_timers`
(zeros, 31 entries) at IEex_IWD2_State.lua:30258;
`ex_record_projectile_position` / `ex_record_temporal_position`
(IEex_IWD2_State.lua:11770-11771) are only CLEARED here on object delete.
These tables being nil as staged is a real behavioural gap of the drop,
not a documentation guess.

Thread contracts as asserted IN the file (relevant to B1's call-site
placement): Async — OnGameObjectAdded, OnGameObjectBeingDeleted,
OnUpdateTempStats, OnPostCreatureHandleEffects, RestrictCreatureActionbar,
ShouldForceDefaultButtons, OnReloadDerivedStats,
OnPostCreatureProcessEffectList. Both — OnConstructDerivedStats,
OnDerivedStatsOperatorEqu, OnDestructDerivedStats,
OnGameObjectsBeingCleaned, OnAfterConstructSprite. CheckItemUsable
asserts no thread. The OnDerivedStatsOperatorEqu comment (lines 184-190)
explains why construct/equ/destruct are Both, not Async: six of
~CDerivedStats' seven callsites live in CScreenCharacter (dtor,
OnCancelButtonClick, LevelUp) on the Sync thread; only
`CGameSprite::~CGameSprite` is Async, and synchronized access is required
because Async constructs `CDerivedStats` (inserting into
IEex_DerivedStatsData) while the record screen is open — an unlocked read
could walk the map mid-rehash.

## IEex_Helper_* entry points

- `IEex_Helper_SynchronizedBridgeOperation(bridge, fn)` — mutual exclusion
  around every multi-step bridge transaction in the file.
- `IEex_Helper_AppendBridgeTableNL` — appends a registry row (invoked
  unprefixed as `IEex_AppendBridgeTableNL` at line 10 — the bridge
  primitives are exported under both spellings; B1's export table must
  cover both).
- `IEex_Helper_SetBridge` / `IEex_Helper_SetBridgeNL` — field writes
  (registry entries, `bEffectsHandled`).
- `IEex_Helper_GetBridge` / `IEex_Helper_GetBridgeNL` — field and key
  reads (options, registry, per-stats data, GameObjectData).
- `IEex_Helper_GetBridgeCreateNL` — get-or-create the per-CDerivedStats
  entry on construct.
- `IEex_Helper_GetBridgeNumIntsNL` — registry length for the extern loops.
- `IEex_Helper_EraseBridgeKey` — per-key teardown on object delete
  (GameObjectData, ConstantID, EnlargedAnimation).
- `IEex_Helper_ClearBridge` — wholesale teardown of `IEex_GameObjectData`
  on `IEex_Extern_OnGameObjectsBeingCleaned`.

Non-helper Lua API defined here that other files (and the DLL, by name)
consume: `IEex_RegisterLuaStat`, `IEex_AccessSpriteLuaStats`,
`IEex_AccessActorLuaStats`, `IEex_EnsureActorEffectListProcessed`,
`IEex_EnsureSpriteEffectListProcessed`, and the 14
`IEex_Extern_OnGameObjectAdded`, `IEex_Extern_OnGameObjectBeingDeleted`,
`IEex_Extern_OnUpdateTempStats`, `IEex_Extern_OnPostCreatureHandleEffects`,
`IEex_Extern_RestrictCreatureActionbar`,
`IEex_Extern_ShouldForceDefaultButtons`,
`IEex_Extern_OnConstructDerivedStats`, `IEex_Extern_OnReloadDerivedStats`,
`IEex_Extern_OnDerivedStatsOperatorEqu`,
`IEex_Extern_OnDestructDerivedStats`,
`IEex_Extern_OnPostCreatureProcessEffectList`,
`IEex_Extern_OnGameObjectsBeingCleaned`,
`IEex_Extern_OnAfterConstructSprite`, `IEex_Extern_CheckItemUsable`.

(`IEex_AssertThread`, `IEex_Call`, `IEex_Label`, `IEex_ReadDword`,
`IEex_WriteDword`, `IEex_IterateActorEffects`,
`IEex_IterateActorTimedEffects`, `IEex_ApplyEffectToActor`,
`IEex_GetActorIDShare`, `IEex_IsSprite`, `IEex_IsPartyMember`,
`IEex_GetGlobal`/`IEex_SetGlobal`, `IEex_GetGameTick`,
`IEex_GetActorState`, `IEex_GetActorSpellState`, `IEex_CheckGlobalEffect`,
`IEex_DemandCItem`/`IEex_UndemandCItem`, `IEex_GetActorStat`,
`IEex_GetActorFullStat`, `IEex_SetToken` are core-DLL API, not
`IEex_Helper_*`.)
