# IEex_IWD2_State.lua — IWD2 rule-system state: spell lists, actor API, feat gates, opcode 500/502 Lua effects, creature size, effect replay

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_IWD2_State.lua` (34958 lines;
fork commit `138495fde69b6092b7318fad462d571864e9d9b8`, ref
`opengl-hd-beta7`; see `docs/renderer/wso/scope.md` and `index.md` for
provenance). The largest file in the fork and the only one with no upstream
baseline: 243 `function IEex_*` definitions across twelve banner-delimited
sections (Options 1, Specific States 61, Common Engine Structures Util 99,
Spell List 110, Actor Manipulation 424, Actor Details 569, Game State 1363,
ActorID Fetching 1916, "Functions which are called to determine if certain
feats can be taken" 2160, "Functions which can be used by Opcode 500
(Invoke Lua)" 3037, "Functions which can be used by Opcode 502 (Screen
Effects)" 27992, "Console test helpers (weather / lightning QA)" 34926). It
installs no engine hooks itself — no `IEex_HookJump`/`IEex_HookBeforeCall`/
`IEex_WriteAssembly` anywhere — it is the Lua side that the Patch files'
hooks and the DLL call into, and it is also the loader hub: its "Specific
States" section `dofile()`s most of the other State files (IEex_Bridge,
IEex_IWD2_Common_State, IEex_Core_State, IEex_Debug_State, IEex_Action_State,
IEex_Creature_State, IEex_Opcode_State, IEex_Key_State, IEex_Gui_State,
IEex_Render_State, IEex_Resource_State, IEex_Projectile_State,
IEex_StartupFixes_State, IEex_Dev_State, IEex_FakeInputRoutine,
IEex_UncapFPS, plus IEex_TRA/IEex_WEIDU/IEex_INI and the `IEex_Modules`
set). Only one `IEex_Extern_*` lives here (`IEex_Extern_SyncTick`); the
DLL also calls this file's 57 global `Feats_*` functions and the
opcode-500/502 resource-named globals by name.

## Intent

1. **A single kill-switch for the whole fork.** `IEex_Vanilla = false`
   heads the file; the fork's own comment: "This option disables most of
   IWD2:EE's functionality / engine patches, only leaving the widescreen
   and FPS uncap code enabled, as well as some very minor convenience
   patches. This is intended for debugging the vanilla game with IEex."
   It gates the INI module, the `IEex_Modules` dofiles, and individual
   feature registrations elsewhere.
2. **Hot reload of the Lua rule system.** `IEex_Reload` (11) runs on the
   ASYNC thread, `dofile`s this same file, then raises bridge
   `IEex_NeedSyncTick` and spins (`IEex_Helper_Sleep(1)`) until the game
   thread calls `IEex_Extern_SyncTick` (34), which re-`dofile`s the file
   on the synced thread and lowers the flag; the reload then re-runs every
   registered listener function name under
   `IEex_Helper_SynchronizedBridgeOperation("IEex_ReloadListeners", ...)`.
   `IEex_AddReloadListener`/`IEex_ReaddReloadListener` (39/45) register
   names into that bridge via the unprefixed DLL helper
   `IEex_AppendBridgeNL` (see the entry-points section).
3. **Lua wrappers over engine primitives** ("Common Engine Structures
   Util"): strref fetch (`IEex_FetchString`), CString assignment
   (`IEex_CString_Set`), hand-built C2DArray loading (`IEex_2DALoad`,
   `IEex_2DADemand`, `IEex_2DAFindColumn`, `IEex_2DAGetAtRelated`),
   variable/global access (`IEex_GetCVariable`/`SetCVariable`,
   `IEex_GetGlobal`/`SetGlobal`), script-action evaluation (`IEex_Eval`),
   token setting (`IEex_SetToken`), display string helpers
   (`IEex_DisplayString`/`IEex_DS`/`IEex_TF`).
4. **Per-actor spell-list construction** ("Spell List"):
   `IEex_SetSpellInfo`/`IEex_AlterSpellInfo`/`IEex_FetchSpellInfo`
   (152/172/337) maintain per-actor, per-casterType (1..11), per-level
   memorized/castable spell counts, backed by
   `CGameSpriteSpellList::Add` and read back by `IEex_GetSpellNameRef`.
5. **Actor manipulation**: apply raw or decoded effects
   (`IEex_ApplyEffectToSprite`/`IEex_ApplyEffectToActor`/
   `IEex_ApplyResref` via `CGameEffect::DecodeEffect`), rename/retooltip
   sprites, create/move creatures and statics, teleport
   (`IEex_JumpActorToPoint`), swap scripts (`IEex_SetActorScript`), poke
   the actionbar (`IEex_SetActionbarState`), grant party XP
   (`IEex_AddPartyXP`).
6. **Actor details — the read-only query API**: base/full stats via
   `CDerivedStats::GetAtOffset`, states and spell states (bitset at
   derived stats +0xEC), current action ID, equipped weapon/launcher
   headers, backstab validity, effect iterators (all/timed/equipped),
   distances, LOS (`CGameArea::CheckLOS`), allegiances, scroll learning,
   class spell levels.
7. **ActorID fetching and game-object iteration**: share-counted actor
   access (`IEex_GetActorIDShare`/`IEex_GetActorShare`/
   `IEex_UndoActorShare` over `CGameObjectArray::GetShare`/`ReleaseShare`),
   `IEex_IterateIDs` with object-type filters, and iterators over
   containers, doors, triggers, projectiles, fireballs, temporals and
   casting glows.
8. **Feat eligibility gates**: 57 global `Feats_*` functions
   (`Feats_True`, `Feats_ArcaneArcher`, `Feats_Assassin`, ...) that the
   DLL calls by name to decide whether a feat can be taken. They read the
   raw creature data directly — e.g. `Feats_ArcaneArcher` (2167) requires
   race (+0x26) eligibility, +0x626 < 2, +0x630/+0x631 > 0, +0x774 >= 1 —
   and consult the shared feats bitmap mirrored into Lua through the
   `IEex_DerivedStatsData` bridge, keyed by derived-stats pointer with the
   `"feats"` key. `IEex_Feats_DefaultMaxPips[featID]` caps pips (checked
   for featID <= 74 at 33519).
9. **Opcode 500 (Invoke Lua) dispatch**: `IEex_ApplyStatScaling` (4527)
   iterates an actor's effects; for every effect with opcode 500 it reads
   the RESREF at effect +0x30 and calls `_G[theresource](eData + 0x4,
   creatureData, true)` — i.e. functions in this file (or registered by
   other files) are named after the invoking effect's resource. Two
   registration tables gate the calls:
   `ex_noncumulative_opcode_functions` (called once per resource per
   scaling pass) and `ex_stat_scaling_functions` (called for every
   matching effect); both are consulted here but populated elsewhere — not
   in this file (honest gap). `IEex_ApplyStatSpell` (4547) maps a stat
   value to the highest-threshold spell in `ex_stat_spells`.
10. **Opcode 502 (Screen Effects) library** (from 27992): the same
    resource-named convention for screen-effect opcodes.
11. **Gameplay systems implemented in Lua**: creature size
    (`IEex_ResetSize` 7429 / `IEex_Enlarge` 7557 over the huge
    `ex_animation_size` table at 7360 — animationID → {circleSize,
    personalSpace, movementScale} — with bridge `IEex_EnlargedAnimation`
    and table `ex_modified_default_animations`); repeating-effect
    bookkeeping (`IEex_CheckForEffectRepeat` 11681/11712 reads effect
    fields +0x3C (& 0x4000), +0x18/+0x1C params, +0x44 special, +0x24
    time, +0x10C sourceID, +0x2C funcName, +0x90 parent_resource into
    `repeat_record[actorID][funcName]`; `IEex_CheckForInfiniteLoop` 11746;
    `IEex_EvaluatePermanentRepeatingEffects` 11824;
    `IEex_EvaluatePersistentEffects` 18715); monk attack/AC bonuses
    (18952/18987); attack animation frames
    (`IEex_IncrementAnimationFrame` 19023, get/set 19059/19107);
    `IEex_ExtraAttacks` (19808); flight/terrain (`IEex_IsFlying` 22636,
    `IEex_IsIncorporeal` 22644, `IEex_CanFly` 22649, `IEex_HeightMod`
    22874, `IEex_ModifyTerrainTable` 23604,
    `IEex_PreventMovingAttacksOfOpportunity` 26112); duels via a global
    effect flag bitmask (`ex_global_effect_timers` 30258, 31 slots;
    `IEex_GetGlobalEffectFlags`/`IEex_SetGlobalEffectFlags` 30260/30271
    compare bits 0-30 against the game tick; `IEex_CheckGlobalEffect`
    30282, `IEex_DuelHide` 30343); thievery (`IEex_PickPocket` 30916,
    `IEex_DisarmTrap` 31107, `IEex_PickLock` 31276, `IEex_BashLock`
    31463); BCS object matching (`IEex_GetBCSObjectID` 33696/33735,
    `IEex_MatchAOE`/`IEex_MatchAOE_Self`/`IEex_CheckTarget`/
    `IEex_CheckHeightBelow` 33780-34056); rule helpers
    (`IEex_IsOutdoors`, `IEex_HasDroppableItems`,
    `IEex_TransferInventoryAR5202`, `IEex_RechargeItemInSlot`,
    `IEex_ReleaseSoulFromInventory`, `IEex_HasMeleeWeaponEquipped`,
    `IEex_HasEffectRES`, `IEex_IsImmuneToOpcode`, `IEex_See`,
    `IEex_CheckIsPartyMember`, `IEex_KnowsSpell`,
    `IEex_CheckSearchMapColor`, `IEex_HaveSummonerCast`,
    `IEex_CastStoredTrapSpell`, `IEex_SetKillCountToken`, `IEex_SetBanter`,
    `IEex_IfValidForPartyDialogue`, `IEex_StealthTimerExpired`).
12. **Developer/QA tooling**: memory search-and-diff
    (`IEex_Search_Change` 3097 / `IEex_Search` 3157 / `IEex_PrintData`
    3213 — compare-dump over an address range against
    `ex_search_previous`/`ex_search_exclude`; a debugging aid, not a BCS
    evaluator), bitmap printing (`IEex_BitmapPrint` 20909,
    `IEex_GetBitmapPixelColor` 20922), and console weather QA
    (`IEex_TestStorm` 34940 / `IEex_TestBolt` 34947 /
    `IEex_TestWeatherInfo` 34953 driving `CWeather::SetCurrentWeather`
    and `CInfinity::CallLightning`).

## Hooks table

The file installs no engine hooks: there is no `IEex_HookJump`,
`IEex_HookBeforeCall` or `IEex_WriteAssembly` anywhere in it. Everything
below is either an address it calls into the engine with (`IEex_Call`), an
engine global/struct offset it reads or writes, or a Lua-side name the DLL
or another file invokes.

### Engine addresses invoked via `IEex_Call`

Resolved against our tree (`grep "0x<addr>" src/` in the worktree, decl
read after the `// 0x<addr>` marker). Fork line = the line in the staged
`IEex_IWD2_State.lua` where the address appears.

| Address | Engine function (our name) | iwd2r file:line | Fork line | What changes / why called |
|---|---|---|---|---|
| 0x427B60 | `CString CBaldurEngine::FetchString(DWORD dwStrId)` | src/CBaldurEngine.cpp:589 | 52 | `IEex_FetchString`: strref → Lua string; result CString ptr read back then freed |
| 0x7FCC1A | unresolved in our tree (inside the CRT/MFC blob at src/music/audio.cpp:477); no-argument call with the result CString pointer as `this` — destructor shape | — | 55 | frees the temp CString after `IEex_FetchString` copies the text |
| 0x7FCD57 | unresolved in our tree (same blob); the fork comment at the call site names it `CString_operator_equ_<char_const_ptr>` | — | 106 | `IEex_CString_Set`: assign a raw char* into an existing CString |
| 0x725950 | `BOOLEAN CGameSpriteSpellList::Add(const UINT& nID, const unsigned int&, const unsigned int&, const unsigned int&)` | src/CGameSpriteSpells.cpp:22 | 266 | spell-list mutation inside `IEex_AlterSpellInfo` |
| 0x48C800 | `CGameEffect* CGameEffect::DecodeEffect(ITEM_EFFECT*, const CPoint&, LONG, const CPoint&)` | src/CGameEffect.cpp:259 | 508 | build a real CGameEffect from raw effect data (`IEex_ApplyEffectToSprite`) |
| 0x5B9D20 | `BOOL CInfGame::CheckIfSpriteCanUseItem(CGameSprite*, CItem*, STRREF*, BOOL)` | src/CInfGame.cpp:12518 | 582 | `IEex_CanSpriteUseItem` |
| 0x46A820 | `BOOL CGameArea::CheckLOS(const CPoint&, const CPoint&, const BYTE* terrainTable, BOOLEAN)` | src/CGameArea.cpp:329 | 601 | `IEex_CheckActorLOS` |
| 0x71F760 | `const CString& CGameSprite::GetName()` | src/CGameSprite.cpp:12837 | 615, 622 | `IEex_GetSpriteName` / `IEex_GetActorName` |
| 0x446DD0 | `LONG CDerivedStats::GetAtOffset(SHORT offset)` | src/CDerivedStats.cpp:934 | 846 | `IEex_GetActorStat` family: read any derived-stats field by raw offset |
| 0x4531A0 | `CDerivedStats* CGameSprite::GetDerivedStats()` | src/CGameSprite.cpp:26831 | 944 | locate the derived stats whose +0xEC bitset holds spell states |
| 0x45E380 | unresolved in our tree (inside the blob at src/CGameAIBase.cpp:3942); called as `(spellStateID, bitsetStruct)` on the derived-stats `this` — bitset-set shape | — | 945 | `IEex_SetActorSpellState`: set a spell-state bit |
| 0x45E390 | unresolved in our tree (same blob); no-argument call on the bitset struct — bitset read/test shape | — | 946 | `IEex_SetActorSpellState`: read the spell-state byte back |
| 0x589110 | `BOOL CInfButtonArray::SetState(INT nState, BYTE bPush)` | src/CInfButtonArray.cpp:680 | 1389 | `IEex_SetActionbarState` |
| 0x5ADAE0 | `void CInfGame::SelectToolbar()` | src/CInfGame.cpp:4936 | 1400 | `IEex_Eval`: refresh toolbar after an action change |
| 0x452BD0 | `CVariable::CVariable()` | src/CVariableHash.cpp:48 | 1446 | construct a CVariable for `IEex_SetCVariable` |
| 0x410120 | `void CAIScriptFile::ParseResponseString(CString)` | src/CAIScriptFile.cpp:113 | 1511 | `IEex_Eval`: parse and run an action string through the BCS response machinery |
| 0x78AA30 | `CString CResRef::GetResRefStr()` | src/CResRef.cpp:53 | 1561 | `IEex_CreateCreature`: resref of the created creature |
| 0x4EC390 | unresolved in our tree (inside the blob at src/CItem.cpp:1416); one-argument call on a CString — free/copy-cleanup shape | — | 1563 | release the temp CString after `IEex_CreateCreature` |
| 0x4CA530 | `CGameStatic::CGameStatic(CGameArea*, CAreaFileStaticObject*)` | src/CGameStatic.cpp:8 | 1589 | `IEex_CreateStatic` |
| 0x5E0B20 | `void CScreenCharacter::UpdatePopupPanel(DWORD, CGameSprite*)` | src/CScreenCharacter.cpp:4486 | 1638 | `IEex_EngineCharacterUpdatePopupPanel` |
| 0x60CEB0 | `void CScreenCreateChar::UpdatePopupPanel(DWORD, CGameSprite*)` | src/CScreenCreateChar.cpp:2466 | 1653 | `IEex_EngineCreateCharUpdatePopupPanel` |
| 0x5C0BB0 | `LONG CInfGame::AddExperienceParty(DWORD nExperience, BOOLEAN a2, BOOLEAN bChallengeRating)` | src/CInfGame.cpp:8498 | 1713 | `IEex_AddPartyXP` (share XP among living party members as 104 effects) |
| 0x7FCC88 | no decl in our tree; our src/IcewindMisc.cpp:1025 comment describes the same callee: "The original builds a CString from the resref as a C string (0x7FCC88)" | (comment only) src/IcewindMisc.cpp:1025 | 1723 | CString ctor from char* for `IEex_DisplayString` |
| 0x4EC1C0 | unresolved in our tree (src/CItem.cpp:1416 blob); one-argument free shape | — | 1726 | free the CString built for `IEex_DisplayString` |
| 0x7FC1A5 | unresolved in our tree (src/music/audio.cpp:477 blob); fork comment names it "GetTokenCString"; `this` = `g_pBaldurChitin + 0x1CC8` | — | 1793 | `IEex_SetToken`: write the token CString in the chitin object |
| 0x402B70 | `void C2DArray::Load(const CResRef&)` | src/C2DArray.cpp:65 | 1815 | `IEex_2DALoad`: load a 2DA into a hand-built C2DArray (layout below) |
| 0x599A50 | `BYTE CGameObjectArray::GetShare(LONG, BYTE, CGameObject**, DWORD)` | src/CGameObjectArray.cpp:96 | 2120 | `IEex_GetActorShare` |
| 0x599E70 | `BYTE CGameObjectArray::ReleaseShare(LONG, BYTE, DWORD)` | src/CGameObjectArray.cpp:211 | 2129 | `IEex_UndoActorShare` |
| 0x745950 | `SHORT CGameSprite::JumpToPoint(CPoint dest, BOOL spriteUpdate)` | src/CGameSprite.cpp:21354 | 21202 | `IEex_JumpActorToPoint` |
| 0x556230 | `void CWeather::SetCurrentWeather(CGameArea*, ULONG, BYTE)` | src/CWeather.cpp:432 | 34944 | console weather QA; `this` = the CWeather embedded at `engineWorld + 0xF46` |
| 0x5D1340 | `void CInfinity::CallLightning(INT, INT)` | src/CInfinity.cpp:3105 | 34950 | console weather QA (lightning strike test) |

Also referenced without being called: the global `0x8E6820` resolves as
`CGameButtonList* CInfButtonArray::dword_8E6820;` (src/CInfButtonArray.cpp:24)
— the button array whose `SetState` the actionbar helper drives.

### Engine globals / struct offsets

| Global / struct | Offset(s) | Use in this file (fork line) |
|---|---|---|
| g_pBaldurChitin (0x8CF6DC — unresolved in our tree) | +0x1CC8 | the token CString `IEex_SetToken` writes through 0x7FC1A5 (1793) |
| 0x8C1758 (unresolved in our tree) | — | fork comment at 1808: "defaultCString (afxPchNil)"; read into every hand-built C2DArray +0x1C |
| CInfGame | +0x37E0, +0x37E2 | `IEex_GetCInfinity` (1606): visible-area index and the area array entry → `IEex_GetCInfinityFromArea` (1602) reads the `CInfinity` at `CGameArea + 0x4CC` |
| CGameArea | +0x4CC | the area's `CInfinity` (1602) |
| engineWorld | +0xF46 | the embedded `CWeather` passed as `this` to `SetCurrentWeather` (34944) |
| CGameEffect (raw effect data) | +0x10 opcode, +0x30 resource, +0x4 effect-data base passed to Lua functions | opcode-500 dispatch (4533-4543); repeat bookkeeping reads +0x3C (& 0x4000), +0x18/+0x1C, +0x44, +0x24, +0x10C, +0x2C, +0x90 (11681+) |
| creatureData (raw actor share) | +0x26 race; +0x626, +0x630/+0x631, +0x774 | `Feats_ArcaneArcher` eligibility reads (2167+) |
| CDerivedStats | +0xEC | spell-state bitset (`IEex_SetActorSpellState` 944-946) |
| C2DArray (hand-built, 0x24 bytes) | +0x0 lock, +0x4 res, +0x8 resref, +0x10 m_pNamesX, +0x14 m_pNamesY, +0x18 m_pArray, +0x1C defaultCString, +0x20 m_nSizeX, +0x22 m_nSizeY | `IEex_2DALoad` (1800) |

### Lua-side callbacks invoked by name

- `IEex_Extern_SyncTick` (34) — the one extern here; the DLL calls it on
  the synced thread to complete a hot reload (re-`dofile` + lower
  `IEex_NeedSyncTick`). B1 must-verify the exact call site.
- 57 global `function Feats_*` gate functions (2160 section) — the DLL
  calls them by name when checking whether a feat can be taken. B1
  must-verify the call sites and the exact invocation contract.
- Opcode-500/502 resource-named globals — invoked as
  `_G[theresource](eData + 0x4, creatureData, true)` from
  `IEex_ApplyStatScaling` (4538/4542); e.g. `B3FEAT` (3041) writes
  effectData +0x110 = 1, `EXBUTTON` (3047) rewrites the actionbar buttons.
  The same naming convention backs the 502 screen-effect library (27992+).
  Which resources are actually shipped as effect RESREFs is data, not
  code — out of scope for this note.

## Data flow

Load: the game bootstrap runs `IEex_IWD2_Patch.lua`, which ends up
`dofile`ing this file; its head defines `IEex_Vanilla`, then "Specific
States" `dofile`s the rest of the Lua system in a fixed order (gated
modules skipped under `IEex_Vanilla`). From then on the file is a library:
the Patch files' hooks call its functions, the DLL calls its extern,
`Feats_*` gates and resource-named effect functions by name.

Hot reload: `IEex_Reload` re-executes the file on the ASYNC thread, sets
bridge `IEex_NeedSyncTick` = 1 and sleeps in a loop so the bridge stays
unlocked; the game thread, at its next sync tick, calls
`IEex_Extern_SyncTick`, which re-executes the file on the synced thread
and sets the flag to 0, releasing the spinner; the reload then re-runs all
registered listener function names inside
`SynchronizedBridgeOperation("IEex_ReloadListeners", ...)`, re-reading the
listener list from the bridge and clearing it. Reload listeners are
therefore names, not closures — anything registered must be a global
function that survives the re-`dofile`.

Stat scaling (opcode 500): `IEex_ApplyStatScaling(targetID)` grabs the
actor's share, iterates its effects with `IEex_IterateActorEffects`, and
for each opcode-500 effect calls the Lua function named by the effect's
RESREF with (effectData + 0x4, creatureData, true). Those functions write
derived stats back through the raw creature data or through
`IEex_Helper_SetBridge("IEex_DerivedStatsData", ...)`. The noncumulative
table ensures once-per-resource semantics per scaling pass; the stat
scaling table means "call for every such effect". Both tables are
populated by other files — not in this one (honest gap).

Feat gating: the engine asks "can this sprite take feat N?" by calling the
`Feats_*` global for N. The gates read the raw creature share directly
(offsets above) plus the shared feats bitmap that the DLL mirrors into the
`IEex_DerivedStatsData` bridge under the key `"feats"`, fetched with
`IEex_Helper_GetBridgeNL("IEex_DerivedStatsData",
IEex_GetSpriteDerivedStats(creatureData), "feats")`. Pip caps come from
`IEex_Feats_DefaultMaxPips`.

Effect replay: repeating/permanent effects are tracked in Lua.
`IEex_CheckForEffectRepeat` fingerprints each effect (params, special,
time, sourceID, funcName at +0x2C, parent resource at +0x90, and the
+0x3C & 0x4000 flag) into `repeat_record[actorID][funcName]`;
`IEex_CheckForInfiniteLoop` guards runaway recursion;
`IEex_EvaluatePermanentRepeatingEffects` and
`IEex_EvaluatePersistentEffects` drive the periodic re-application.
Global effects (duels) use a 31-slot bit table `ex_global_effect_timers`
whose bits are compared against the game tick (`IEex_GetGameTick`).

Engine access style: nearly every engine touch is an `IEex_Call` against a
hard address with hand-rolled argument/this register setup (`IEex_Call(addr,
{args}, this, thisOff)`), malloc/free for temporary CStrings and
CResRefs, and raw offset reads/writes — the Lua is effectively a second
client of the same ABI the C++ tree implements, which is why the address
table above doubles as a cross-check for our own declarations.

Console QA: `IEex_TestStorm`/`IEex_TestBolt`/`IEex_TestWeatherInfo` build
a weather value and call `CWeather::SetCurrentWeather` on the
`engineWorld + 0xF46` object, or `CInfinity::CallLightning` directly, so
weather/lightning can be exercised from the console without playing.

## IEex_Helper_* entry points

DLL helpers this file calls (counts are call sites in the staged file; all
absent from `ghidra-projects/ieexhelper-fork/dll-delta.tsv` — every name is
a B1 must-verify):

| Helper | Calls | Used for |
|---|---|---|
| `IEex_Helper_GetBridge` | 20 | read bridge values (`IEex_NeedSyncTick`, `IEex_DerivedStatsData`, `IEex_EnlargedAnimation`) |
| `IEex_Helper_SetBridge` | 19 | write bridge values (same keys) |
| `IEex_Helper_GetBridgeNL` | 12 | no-lock reads on the game thread (feats bitmap, listener data) |
| `IEex_Helper_SynchronizedBridgeOperation` | 2 | `IEex_ReloadListeners` rebuild (reload) and registration (AddReloadListener) |
| `IEex_Helper_Sleep` | 1 | the reload spinner's yield |
| `IEex_Helper_ReadDataFromBridgeNL` | 1 | pull the listener list out of the bridge |
| `IEex_Helper_EraseBridgeKey` | 1 | bridge key cleanup |
| `IEex_Helper_ClearBridgeNL` | 1 | clear `IEex_ReloadListeners` before re-filling |

Plus one helper invoked UNPREFIXED: `IEex_AppendBridgeNL` (calls at 41 and
46, in `IEex_AddReloadListener`/`IEex_ReaddReloadListener`). It is defined
nowhere in the staged Lua — like the unprefixed bridge helpers documented
in IEex_Key_State.md, it is a DLL-registered C function; per that note's
convention it is listed here as `IEex_Helper_AppendBridgeNL` invoked
unprefixed. B1 must-verify.

Bridge names in use: `IEex_NeedSyncTick`, `IEex_ReloadListeners`,
`IEex_DerivedStatsData` (keyed by derived-stats pointer, `"feats"` key),
`IEex_EnlargedAnimation`.
