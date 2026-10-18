# IEex_Core_State.lua — startup stages, feat/skill system growth, async Lua state, crash handler

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Core_State.lua`. The core of
the DLL↔Lua seam's engine side: the three startup stages (Stage0 before
`CBaldurChitin` construction, Stage1 before `CInfGame` construction, Stage2
after `CBaldurChitin::Init`), creation of the ASYNC Lua state and the
per-thread Lua-state selector, the feat/skill system re-implementation
(function trampolines marshalling to Lua externs, extended count storage in
the creature-stats tail, chargen/levelup snapshot arrays, immediate-bound
rewrites), master spell-list and resource indexing, and a crash handler
that dumps diagnostics and force-saves the game. 17 active
`IEex_Extern_*` functions (plus one commented out); the DLL invokes every
one BY NAME — no other staged file calls them.

## Intent

1. **Staged startup.** `IEex_Extern_Stage0Startup` (no thread assert —
   "Earliest possible hook into engine. Directly before
   CBaldurChitin_Construct()") records the current thread into the
   `IEex_ThreadBridge` under "Sync". `IEex_Extern_Stage1Startup` (Sync,
   "CBaldurChitin = partially initialized … Directly before
   CInfGame_Construct()") runs resource indexing + spell→scroll mapping,
   loads the initial 2DAs (which derives `IEex_NEW_FEATS_SIZE` /
   `IEex_NEW_SKILLS_SIZE` from FEATS.2DA / SKILLS.2DA and stores
   `NEW_FEATS_MAXID` in the `IEex_Feats` bridge), then applies all delayed
   code patches. `IEex_Extern_Stage2Startup` (Sync, after
   `CBaldurChitin_Init()`) indexes the four master spell lists and — only
   when Icewind2.ini `[IEex Options] "UI Canvas Scale x10" <= 10` — calls
   `IEex_MoveHighResolutionPaddingPanels` (defined in IEex_Gui_State.lua).
2. **The async Lua state.** `IEex_AbsoluteOnce("IEex_GetLuaState")` creates
   a SECOND Lua state (`_luaL_newstate` / `_luaL_openlibs` by label, kept
   as `IEex_AsyncState`, defined to assembly as `_g_lua_async`) and emits
   an `IEex_WriteAssemblyAuto` cave that dispatches on the current thread:
   `IEex_Helper_GetBridgeDirect("IEex_ThreadBridge", "Sync")` → `*_g_lua`,
   `...("Async")` → `*_g_lua_async`, else 0. Published DLL-side as
   `"IEex_JIT_GetLuaState"` via `IEex_Helper_DefineAddress`. Header
   comment: "This is special and is needed in some IEex_Gui_State calls;
   it SHOULD be in a Patch file, but some major refactoring would have to
   take place to make that happen."
3. **The feat system, re-implemented in Lua.** The engine's six
   feat-entry-points (set/get count, max pips, requirements met,
   incrementable, display-string count) are trampolined to
   `IEex_Extern_*Hook` externs; the Lua side answers from FEATS.2DA /
   B3FEATS.2DA / B3FEATEX.2DA and from the creature's stats memory.
   Storage grows past the stock `m_feats[3]` bitmask: counts for feats
   75-111 live in the `field_1EA` tail and 112+ in `field_300` (see Data
   flow); every hard-coded array bound inside the touched functions is
   rewritten to `IEex_NEW_FEATS_SIZE` / `IEex_NEW_SKILLS_SIZE`.
4. **Chargen / levelup snapshot arrays.** The character screens keep
   before/after copies of the feat array in FIXED static buffers (3
   dwords) — with a grown feat list those writes run off the end. Eight
   in-function sites re-point those buffers at `IEex_Malloc`'d,
   `IEex_Helper_Memset`-zeroed arrays of `IEex_NEW_FEATS_SIZE * 4` bytes:
   `chargenBeforeFeatCounts` (3 sites) and `levelupBeforeFeatCounts`
   (5 sites).
5. **Crash handler.** `IEex_Extern_Crashing(excCode, EXCEPTION_POINTERS)`
   is once-only (bridge `IEex_Extern_Crashing` / "alreadyCrashed"): copy
   IEex.log into `crash\IEex_<timestamp>.log`, write a small and a
   full-memory minidump via `IEex_Helper_WriteDump`, message-box the paths,
   then rename the save target (`CString` at `CInfGame`+0x4220 =
   `m_sSaveGame`, and `CScreenCharacter::SAVE_NAME` at 0x8F3338) to
   `crash_<timestamp>` and call `CInfGame::SaveGame(1, 0, 0)` — "Your game
   will attempt to save under the following name after you press OK,
   though it may be corrupted".
6. **Rules/usability externs.** `IEex_Extern_CSpell_UsableBySprite`
   (level-24 spell gate reading the indexed master lists),
   `IEex_Extern_CRuleTables_GetRaceName` /
   `IEex_Extern_CGameSprite_GetRacialFavoredClass`, the feat Lua API
   (`IEex_GetSpriteFeatCount`, `IEex_SetSpriteFeatCount`,
   `IEex_GetFeatMaxPips`, … all reading/writing the extended storage), and
   the global `B3FEAT(pEffect, pSprite)` — the opcode-500 FeatStat effect
   handler the DLL calls by name (Async thread).

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x762890 | `CGameSprite::SetFeatValue` (fork: "CGameSprite::SetFeatCount") | src/CGameSprite.cpp:25459 (exact marker; decl follows) | `IEex_Extern_SetSpriteFeatCountHook` | `IEex_HookReplaceFunctionMaintainOriginal(0x762890, 7, "CGameSprite::SetFeatCount", ...)`: 7-byte prologue → `IEex_GenLuaCall` marshalling (sprite=ecx, featID, count), no return, `!ret 8`; trampoline label keeps the original body reachable (an `IEex_Call` target). Also `IEex_WriteByte(0x762897 + 2, IEex_NEW_FEATS_SIZE)` — the array-bound immediate inside the original body. |
| 0x762923 … 0x762D3D | 26 sites inside `CGameSprite::SetFeatValue` (entry 0x762890) | src/CGameSprite.cpp:25459 (marker; the sites themselves unresolved in our tree) | — | `IEex_WriteAssembly(addr, {"!jmp_byte"})` at 0x762923, 0x76294D, 0x762977, 0x7629A1, 0x7629CB, 0x7629F5, 0x762A1F, 0x762A49, 0x762A73, 0x762A9D, 0x762AC7, 0x762AF1, 0x762B1B, 0x762B45, 0x762B6F, 0x762B99, 0x762BC3, 0x762BED, 0x762C17, 0x762C41, 0x762C6B, 0x762C95, 0x762CBF, 0x762CE9, 0x762D13, 0x762D3D — "Disable default maximum-feat-count assertions": each 2-byte jmp replaces a short conditional jump over the assert path. |
| 0x762E20 | `CGameSprite::GetFeatValue` (fork: "CGameSprite::GetFeatCount") | src/CGameSprite.cpp:25658 (exact marker; decl follows) | `IEex_Extern_GetSpriteFeatCountHook` | `IEex_HookReplaceFunctionMaintainOriginal(0x762E20, 6, "CGameSprite::GetFeatCount", ...)`; GenLuaCall returns `IEex_LuaCallReturnType.Number`, `!ret 4`. `IEex_WriteByte(0x762E26 + 2, IEex_NEW_FEATS_SIZE)`. |
| 0x7630A0 | `CGameSprite::GetMaxFeatValue` (fork: FeatGetNumMaxPips) | src/CGameSprite.cpp:25726 (exact marker; decl follows) | `IEex_Extern_GetFeatMaxPipsHook` | `IEex_HookJumpNoReturn` — "Complete function reimplementation", the original body is NOT retained: GenLuaCall Number, `!ret 4`, call-error path `!mov_eax #1` (max pips 1 on Lua failure). `IEex_WriteByte(0x7630A5 + 2, IEex_NEW_FEATS_SIZE)`. |
| 0x763200 | `CGameSprite::sub_763200` (fork: MeetsFeatRequirements) | src/CGameSprite.cpp:25819 (exact marker; decl follows) | `IEex_Extern_GetSpriteMeetsFeatRequirementsHook` | `IEex_HookReplaceFunctionMaintainOriginal(0x763200, 6, "CGameSprite::MeetsFeatRequirements", ...)`; Boolean, `!ret 8`. `IEex_WriteByte(0x763206 + 2, IEex_NEW_FEATS_SIZE)`. |
| 0x763A40 | `CGameSprite::sub_763A40` (fork: FeatIncrementable) | src/CGameSprite.cpp:26054 (exact marker; decl follows) | `IEex_Extern_GetFeatIncrementableHook` | `IEex_HookJumpNoReturn` (full reimplementation); Boolean, `!ret 8`. `IEex_WriteByte(0x763A46 + 2, IEex_NEW_FEATS_SIZE)`. |
| 0x765D24 | inside `CGameSprite::DisplayFeats` (entry 0x765C50; fork: UpdateSkillsAndFeatsTab) | src/CGameSprite.cpp:26684 (`// 0x765C50` marker; hook site itself unresolved in our tree) | `IEex_Extern_GetFeatStringHasCountHook` | `IEex_HookJumpNoReturn` after `push esi`: GenLuaCall Boolean, then `!test_eax_eax; !jz_dword :765D7E; !jmp_dword :765D3B` — branches back INTO the original body at both outcomes (the reimplementation is only the has-count test feeding the original's two print paths). Bounds rewrites `IEex_WriteByte(0x765CE8 + 2, ...)` and `IEex_WriteByte(0x765DC8 + 2, ...)` in the same body. |
| 0x60CD6C | inside `CScreenCreateChar::ResetFeatsPanel` (entry 0x60CA10; fork: Chargen_Init_Feats) | src/CScreenCreateChar.cpp:2348 (INSIDE marker `// 0x60CA10`; decl 2349; hook site itself unresolved in our tree) | — | `IEex_WriteAssembly(0x60CD6C, {"mov_[edx*4+dword]_eax", {chargenBeforeFeatCounts, 4}})`: the feat-array base store is re-emitted with the Malloc'd `chargenBeforeFeatCounts` (sized `IEex_NEW_FEATS_SIZE * 4`, `IEex_Helper_Memset`-zeroed) as the imm32 base. |
| 0x60E689 | inside `CScreenCreateChar::UpdateFeatsPanel` (entry 0x60E3E0; fork: Chargen_Update_FeatList) | src/CScreenCreateChar.cpp:2933 (INSIDE marker; decl 2934; hook site itself unresolved in our tree) | — | `cmp_[ecx*4+dword]_eax` against the same `chargenBeforeFeatCounts` base. |
| 0x623447 | inside `CUIControlButtonCharGenFeatsPlusMinus::AdjustValue` (entry 0x6232A0; fork: Chargen_OnFeatCountChange) | src/CScreenCreateChar.cpp:11224 (INSIDE marker; decl 11225; hook site itself unresolved in our tree) | — | `cmp_[esi*4+dword]_eax` against `chargenBeforeFeatCounts`. |
| 0x5E4311 | inside `CScreenCharacter::OnCancelButtonClick` (entry 0x5E3B80; fork: OnBackButtonClick) | src/CScreenCharacter.cpp:5577 (INSIDE marker; decl 5578; hook site itself unresolved in our tree) | — | The engine's fixed-buffer load becomes `mov_edi {levelupBeforeFeatCounts}` + `!nop`. |
| 0x5F89D5 | inside `CScreenCharacter::sub_5F89B0` (entry 0x5F89B0; fork: OnFeatPipTakenBack) | src/CScreenCharacter.cpp:8467 (INSIDE marker; decl 8468; hook site itself unresolved in our tree) | — | Same shape: `mov_ebp {levelupBeforeFeatCounts}` + `!nop`. |
| 0x5DA2DE | inside `CScreenCharacter::ResetFeatsPanel` (entry 0x5DA200) | src/CScreenCharacter.cpp:1632 (INSIDE marker; decl 1633; hook site itself unresolved in our tree) | — | `mov_[ecx*4+dword]_eax` re-based on `levelupBeforeFeatCounts`. |
| 0x5E8AD7 | inside `CScreenCharacter::UpdateFeatsPanel` (entry 0x5E87E0) | src/CScreenCharacter.cpp:7538 (INSIDE marker; decl 7539; hook site itself unresolved in our tree) | — | `cmp_[ecx*4+dword]_eax` against `levelupBeforeFeatCounts`. |
| 0x5F7156 | inside `CUIControlButtonCharacterFeatsPlusMinus::AdjustValue` (entry 0x5F6FD0) | src/CScreenCharacter.cpp:12132 (INSIDE marker; decl 12133; hook site itself unresolved in our tree) | — | `cmp_[esi*4+dword]_eax` against `levelupBeforeFeatCounts`. |
| — (label-defined) | `IEex_GetLuaState` — assembly Lua-state selector | — | `IEex_Helper_GetBridgeDirect` | `IEex_WriteAssemblyAuto` cave (see Intent 2): `!call >IEex_GetCurrentThread`, two `!call >IEex_Helper_GetBridgeDirect("IEex_ThreadBridge", …)` dispatches, `mov_eax *_g_lua` / `*_g_lua_async` / `xor_eax_eax`; published as `IEex_JIT_GetLuaState`. Called from assembly everywhere the DLL needs the right Lua state — B1 must export `GetBridgeDirect`. |
| 0x763E9D (+2), 0x76429A (+2), 0x7642D6 (+2), 0x764313 (+4), 0x7644C6 (+2) | skill-size bounds: inside `CGameSprite::sub_763E40(UINT nSkill)` (decl src/CGameSprite.cpp:26229), `CGameSprite::SetSkillValue` (26386), `CGameSprite::GetSkillValue` (26400), `CGameSprite::GetSkillCost` (26410, two sites) | the decls listed (hook sites themselves unresolved in our tree) | — | `IEex_WriteByte(addr + 2 or 4, IEex_NEW_SKILLS_SIZE)` — "prevents the game from crashing when there are more than 16 skills in SKILLS.2DA", with the caveat "the additional skills still aren't displayed on the skills screen". |
| 0x763156 (+2) | inside `CGameSprite::sub_763150` (entry 0x763150; fork: HasFeatAndMeetsRequirements) | src/CGameSprite.cpp:25791 (exact marker at entry; site itself unresolved) | — | `IEex_WriteByte(0x763156 + 2, IEex_NEW_FEATS_SIZE)`. |
| 0x5E1251 (+2) | inside `CScreenCharacter::CheckSummonPopup(DWORD, CGameSprite*)` (entry 0x5E0FD0) | src/CScreenCharacter.cpp:4672 (INSIDE marker; decl 4673; site itself unresolved) | — | `IEex_WriteByte(0x5E1251 + 2, IEex_NEW_FEATS_SIZE)`. |
| 0x84EA66 | inside the data blob at `dword_84EA50` (name at +0x16 unresolved in our tree; fork comment "nNumItems") | src/CGameSprite.cpp:28397 (marker `// 0x84EA50`; decl 28398 `static const INT dword_84EA50 = 1;`) | — | `IEex_WriteWord(0x84EA66, IEex_NEW_FEATS_SIZE)` — a table-size word in read-only data, widened to match the new feat count. |

Cited but not hooked: `CGameSprite::sub_763150` 0x763150
(src/CGameSprite.cpp:25791, exact) — `IEex_Call`ed by
`IEex_GetSpriteHasFeatAndMeetsRequirements` with the "abuse the function's
simple indexing" trick of passing `baseStats - 0x5A4` so the `this`-relative
field loads land on plain `m_baseStats`; `CInfGame::SaveGame(BOOLEAN,
BOOLEAN, BOOLEAN)` 0x5AC430 (src/CInfGame.cpp:1614, exact) — the crash
handler's last act, called as `{1, 0, 0}` with `IEex_GetGameData()` as
`this`; `CString CScreenCharacter::SAVE_NAME` 0x8F3338
(src/CScreenCharacter.cpp:54, exact) and `CInfGame::m_sSaveGame`
(+0x4220, src/CInfGame.h, ahead of `m_sSaveDir` at +0x4224) — both
`IEex_CString_Set` to `crash_<timestamp>`; the master lists
`CSpellResRefList m_spells` / `m_innateSpells` / `m_songs` /
`m_shapeshifts` at CInfGame +0x4BF8 / +0x4C00 / +0x4C08 / +0x4C10
(src/CInfGame.h:503-506; the class is `{ CResRef* m_pList; UINT m_nCount; }`
at src/CInfGame.h:55 — the indexer reads the pointer at [address], the
count at [address+4], 8-byte resref stride); the feat storage
`DWORD m_feats[3]` at baseStats+0x1B8 (src/FileFormat.h:513), counts
75-111 at baseStats+0x1EB inside `BYTE field_1EA[38]` (src/FileFormat.h:541),
counts 112+ at baseStats+0x300 inside `field_300[124]` (src/FileFormat.h:584);
the sprite-relative forms sprite+0x75C (= `m_baseStats` +0x5A4
(src/CGameSprite.h:788) + 0x1B8, fork comment "sprite.m_baseStats.m_feats"),
sprite+0x78F and sprite+0x8A4 (the two extended-count tiers), and
sprite+0x62B (= `m_baseStats` + 0x87 = `BYTE m_fighterLevel`,
src/FileFormat.h:497 — the fork's comment
"sprite.m_baseStats.m_fighterLevel" matches our field name exactly);
`CSpell_UsableBySprite`'s spells-known probe sprite+0x4888 — inside
`CGameSpriteSpells m_spells` (src/CGameSprite.h, before
`m_domainSpells` at +0x4984): the layout resolves as
`m_spellsByClass[7]` (CSPELLLIST_NUM_CLASSES) × 0x100 of
`CGameSpriteGroupedSpellList` (`m_lists[9]` × 0x1C of
`std::vector`-backed `CGameSpriteSpellList`, 16-byte entries), putting
0x4888 in `m_spellsByClass[6]` — the fork's
`(ReadDword(off+4) - ReadDword(off)) / 16` is vector end−begin = size;
which class/level element that is intended to read is unresolved in our
tree (off-by-4 from a clean `m_lists` base); `_luaL_newstate` /
`_luaL_openlibs` `IEex_Label`s — DLL assembly labels, unresolved in our
tree.

## Data flow

Bridges (DLL-side maps; all access through the helpers):

- `IEex_Feats` / "NEW_FEATS_MAXID" — written by `IEex_LoadInitial2DAs`
  (B3FEATS.2DA's last ID) and read by `IEex_Extern_FeatHook`;
  `IEex_NEW_FEATS_SIZE` (= maxID + 1) and `IEex_NEW_SKILLS_SIZE` are
  file-scope Lua globals set in the same function, nil until Stage1 (both
  gated `if IEex_Vanilla then return end` — IEex_Vanilla is
  `IEex_IWD2_State.lua`'s, line 6).
- `IEex_Extern_Crashing` / "alreadyCrashed" — once-only guard inside
  `IEex_Helper_SynchronizedBridgeOperation`.
- `IEex_ThreadBridge` / "Sync", "Async" — Stage0 writes "Sync" =
  `IEex_GetCurrentThread()`; the `IEex_JIT_GetLuaState` cave reads both
  keys per call.
- `IEex_DerivedStatsData` / <CDerivedStats ptr> / "feats" — the FeatStat
  Lua-stat registered via `IEex_RegisterLuaStat{init =
  IEex_FeatStat_Init, reload = IEex_FeatStat_Reload, copy =
  IEex_FeatStat_Copy, ...}` (lifecycle driven by Creature_State's four
  derived-stats externs). `B3FEAT` writes per-feat entries
  ({pEffect, featID, count} + a "feat_index_<id>" reverse index) under
  `IEex_Helper_SynchronizedBridgeOperation` — the same sub-bridge
  Creature_State's lifecycle externs create/erase.
- `IEex_FakeCursorPosMem` / "value" — a 0xC-byte `IEex_Malloc` block parked
  in a bridge at `IEex_Once("IEex_CoreInitializeVariables")` time (shared
  cursor-position scratch between the threads; consumers are not in this
  file).

File-scope Lua state: the `ex_` tables built in `IEex_Once` —
`IEex_LISTSPLL`/`LISTINNT`/`LISTSONG`/`LISTSHAP` (+ `_Reverse` mirrors),
`IEex_IndexedResources`, `IEex_SpellToScroll`, `IEex_Loaded2DAs` — filled
by Stage1/Stage2 indexing, read by the externs
(`IEex_Extern_CSpell_UsableBySprite` resolves a spell's scroll through
`IEex_SpellToScroll` and the LIST* tables).

Per-frame readers: every `IEex_Extern_*Hook` (the six feat trampolines) is
called on the engine's hot paths — get-count during rendering of the
records screen and HUD, meets-requirements/incrementable during pip
clicks, FeatHook (Async) after stat recalcs. `IEex_Extern_FeatHook`
reconciles old/new base+derived stats (diffing the extended count tiers)
after the DLL copies stats.

Honest gaps (staged-drop evidence, same class as the Creature_State
note): `ex_listspll` is READ here (the CSpell_UsableBySprite level gate)
and in IEex_IWD2_State.lua, but NEVER ASSIGNED anywhere in the drop —
Core_State fills the differently-named `IEex_LISTSPLL`; as staged the
level-24 gate indexes nil. `ex_tra_5000` (GetRaceName's fallback strref)
is read once (line 204) and never assigned. `IEex_Debug_WriteTracePatches`
is called at the very end (line 1052 — "needs to come after all other
patches, even the delayed ones") and defined NOWHERE in the drop. The
comment "Rest of special async globals in IEex_Core_Patch" references
`IEex_Core_Patch.lua`, which is not in the staged drop (22 files staged:
the 20 noted here + 2 upstream baselines).

## IEex_Helper_* entry points

- `IEex_Helper_SynchronizedBridgeOperation(bridge, fn)` — wraps the crash
  guard and every B3FEAT bridge transaction.
- `IEex_Helper_GetBridgeDirect(key, field)` — the bridge read called FROM
  ASSEMBLY (the `IEex_JIT_GetLuaState` cave, twice) — must be exported
  with a plain `__stdcall`-style signature the cave can call.
- `IEex_Helper_GetBridge` / `IEex_Helper_GetBridgeNL` — reads
  (NEW_FEATS_MAXID, bridge values).
- `IEex_Helper_SetBridge` / `IEex_Helper_SetBridgeNL` — writes (thread
  registration, crash flag, FeatStat entries).
- `IEex_Helper_GetBridgeCreateNL` — create-on-read (`IEex_FeatStat_Init`).
- `IEex_Helper_ClearBridgeNL` — FeatStat reload/copy teardown (new use vs
  Creature_State: per-sub-bridge clear, not per-key erase).
- `IEex_Helper_GetBridgeNumIntsNL` — FeatStat_Copy's entry count.
- `IEex_AppendBridgeTableNL` (unprefixed spelling, inside `B3FEAT`) —
  appends a new per-feat entry; the bridge primitives are exported under
  both spellings (B1's export table must cover both).
- `IEex_Helper_DefineAddress(name, address)` — publishes
  `"IEex_JIT_GetLuaState"` to the DLL.
- `IEex_Helper_Memset` — zeroes the two feat-snapshot arrays.
- `IEex_Helper_WriteDump(flags, EXCEPTION_POINTERS, path)` — the two
  crash minidumps.
- `IEex_Helper_Sleep` — present only in commented-out code (line 72).

(`IEex_AssertThread`, `IEex_Once`, `IEex_AbsoluteOnce`, `IEex_Call`,
`IEex_Label`, `IEex_ReadDword`, `IEex_WriteByte`/`Word`/`Dword`,
`IEex_WriteAssembly`, `IEex_WriteAssemblyAuto`,
`IEex_HookReplaceFunctionMaintainOriginal`, `IEex_HookJumpNoReturn`,
`IEex_GenLuaCall`, `IEex_DefineAssemblyLabel`, `IEex_WriteStringAuto`,
`IEex_GetCurrentThread`, `IEex_GetGameData`, `IEex_CString_Set`,
`IEex_Malloc`, `IEex_GetPrivateProfileInt`, `IEex_2DADemand` /
`IEex_2DAGetAt` / `IEex_2DAGetAtRelated`, `IEex_Flags`,
`IEex_DumpCrashThreadStack`, `IEex_MessageBox`, `IEex_TracebackPrint`,
`IEex_RegisterLuaStat`, `IEex_TracebackMessage`,
`IEex_GetSpriteDerivedStats`, `IEex_GetActorIDShare` are core-DLL API,
not `IEex_Helper_*`.)
