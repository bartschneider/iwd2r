# IEex_Sound_Patch.lua — sound engine fixes and zoom-aware audibility

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Sound_Patch.lua`. Two
launch-crash fixes, then three zoom/resolution-scaling audible-range
features (positional SFX, elliptical falloff, PC voices), a cross-language
soundset-folder remap — five DLL helpers carry the runtime logic.

Note on names: the fork's DLL-side classes say `CSoundImp`; our tree's
class is `CSound` (e.g. the destruct hook target). `CSoundMixer` matches
directly.

## Intent

1. **Destruct cleanup crash**: the sound system never properly cleared
   invalid sounds from various internal lists when destructed → crash. The
   destruct head calls `IEex_Helper_CSoundImp_DestructHook`, which does the
   bookkeeping DLL-side.
2. **Uninitialised `CSoundMixer::m_pDirectSound` (+0x4)**: the engine never
   assigns it in the constructor, so it starts as whatever was in that
   memory — launch crashes if that happened to be non-zero. Forced to
   nullptr.
3. **`m_nMaxChannels` (+0xD8) starts at -1** so the DLL's
   `CSoundImp::Export_DestructHook` asserts properly (instead of crashing)
   when icewind2.ini's `[Alias]` fields aren't updated — "The game still
   crashes when this happens, but displays the proper assertion."
4. **Zoom + resolution-scaled positional-SFX radius**: `CSound::CSound`
   hardcodes `m_nRange` (+0x20) = 768; `CSound::Play` (positional) and
   `CSoundMixer::ResetVolume` mute a world sound past `m_nRange`
   world-units from the viewport centre — a fixed 768 mutes edge casts at
   hi-res and (once widened) would let you hear the whole map when zoomed
   in. The store is replaced with `IEex_Helper_ScaleSoundRange`, which sets
   `m_nRange` to SFX-Audible-Percent of the visible world width (render
   width / g_fCameraZoom), per construction so casts pick up the live
   zoom; the software renderer keeps zoom = 1.0. Ambient sounds override
   via `CSound::SetRange` after construction. The paired m_nPanRange
   resolution patch stays in `IEex_Extern_InitResolution`; that file must
   NOT also patch 0x7A8C5A (this region is now the jmp).
5. **Elliptical falloff**: Play + ResetVolume attenuate by CIRCULAR world
   distance dx²+dy², which over-covers vertically on a wide screen. Both
   inline dx²+dy² computations are replaced with
   `IEex_Helper_EllipticalSoundDist(dx, dy)`, stretching dy by the live
   viewport aspect (reads live SCREENWIDTH/HEIGHT) so the audible region
   matches the screen rectangle at any aspect (16:9 / 21:9 / 32:9 / 16:10
   / 4:3).
6. **PC voices attenuate with viewport distance**: vanilla plays your own
   party's barks at full volume everywhere (`CSound::Play(BOOL)` = GLOBAL)
   while NPC/enemy voices already attenuate positionally. The PC branches
   of `CGameSprite::VerbalConstant` and of the soundset dispatcher
   `CGameSprite::PlaySound` are routed through positional
   `CSound::Play(int,int,int,BOOL)` with a WIDER voice range
   (`IEex_Helper_ScaleVoiceRange`, "Voice Audible Percent" default 120 vs
   SFX 60%, clamped to 4600 — see Data flow) so a PC at the screen edge
   stays ~83% audible.
7. **Cross-language soundset folder remap**: saves/.CHRs store each PC's
   voice set as the (localized) SOUND FOLDER NAME; loading a save from
   another language makes `CDimm::AddToDirectoryList` (non-recursive) miss
   the folder, so every soundset line of that PC fails silently (no audio,
   no m_talkingCounter/white talking frame, and the CHARSTR.2DA subtitle
   row misses too). `IEex_Helper_FixSoundSetFolder` rescans `sounds\*` for
   the folder whose files match `<prefix>*.wav` (the prefix
   `m_secondarySounds` IS language-independent) and rewrites
   `CGameSprite::field_725A` (char[32] @ +0x725A) in place BEFORE the path
   build; no-op when the stored folder exists.

The file carries a Lua-side warning repeated at each block: never put
`--` comments INSIDE a `[[ ]]` asm block — the IEex assembler turns the
comment text into 0x00 padding bytes (stray `add [eax],al`), corrupting
the stub.

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x7A8E20 | `CSound::~CSound` | src/CSound.cpp:72 (exact marker; decl follows) | `IEex_Helper_CSoundImp_DestructHook` | `IEex_HookRestore(0x7A8E20, 0, 7)`: push_all_registers_iwd2, call helper, pop_all_registers_iwd2. Destructor bookkeeping moves into the DLL. |
| 0x7AAD80 | `CSoundMixer::CSoundMixer` | src/CSoundMixer.cpp:20 (exact marker; decl follows) | — | `IEex_HookRestore(0x7AAD80, 0, 6, {!mov([ecx+0x4],0x0)})` — the 6 displaced bytes are replaced with `m_pDirectSound` (+0x4, src/CSoundMixer.h:64, `#guess`) = nullptr before the original ctor body runs. |
| 0x7AAE62 | inside `CSoundMixer::CSoundMixer` (entry 0x7AAD80) | src/CSoundMixer.cpp:20 (marker at 19; hook site itself unresolved in our tree) | — | `IEex_HookReturnNOPs(0x7AAE62, 1, {!mov([esi+0xD8],-1)})` — forces `m_nMaxChannels` (+0xD8, src/CSoundMixer.h:83; fork name nMaxChannelSlot) to -1 at the end of construction. |
| 0x7A8C57 | inside `CSound::CSound` (entry 0x7A8BB0) | src/CSound.cpp:14 (marker at 13; hook site itself unresolved in our tree) | `IEex_Helper_ScaleSoundRange` | The 7-byte store `mov [esi+0x20],0x300` (`m_nRange` = 768) is replaced with a jmp into an `IEex_WriteAssemblyAuto` cave: push_all, `push esi` (CSound* this, __stdcall arg), call helper, pop_all, `!jmp_dword :7A8C5E`, + 2 NOP padding. Helper sets m_nRange = SFX-Audible-Percent × visible world width. |
| 0x7A9ED5 | inside `CSound::Play(int,int,int,BOOL)` (entry 0x7A9DB0) | src/CSound.cpp:655 (marker at 654; hook site itself unresolved in our tree) | `IEex_Helper_EllipticalSoundDist` | 10 displaced bytes (`mov esi,edx; imul eax,eax; imul edx,esi; add eax,edx` — dx²+dy²). Cave: `8B F2` (mov esi,edx — dx survives the call for the pan divide), `51` (save m_nRange² in ecx), `50` (arg2 dy), `56` (arg1 dx), call helper (eax = dist), `59` (restore), `!jmp_dword :7A9EDF` (the cmp against m_nRange²); site gets jmp + 5 NOPs. |
| 0x7AA1FA | inside `CSoundMixer::ResetVolume` (entry 0x7AA110) | src/CSound.cpp:425 (marker at 424; hook site itself unresolved in our tree) | `IEex_Helper_EllipticalSoundDist` | 10 displaced bytes (same dx²+dy² shape, dx in edi). Cave: `50` (dy), `57` (dx), call helper, `!jmp_dword :7AA204` (the cdq); nothing to save — no m_nRange² live in a register here. jmp + 5 NOPs at the site. |
| 0x702C37 | inside `CGameSprite::VerbalConstant` (entry 0x702900), EA_PC branch | src/CGameSprite.cpp:4604 (marker at 4603; hook site itself unresolved in our tree) | `IEex_Helper_ScaleVoiceRange` | 11 displaced bytes (`push 0` bReplay; `lea ecx,[esp+0x34]` = &strRes.cSound; `call 0x7A9B10` global Play). Cave: `8D 44 24 30` (eax = &cSound), save esi, push &cSound, call ScaleVoiceRange, pop esi, then rebuild the NPC branch verbatim (`mov edx,[esi+0xA]` y, `mov ecx,[esi+6]` x, `push 0/0/50/51`), `lea ecx,[esp+0x40]` (this after the 4 pushes), `push 0x702C42` as return address, `!jmp_dword :7A9DB0` into positional Play — its `ret 0x10` lands on the following `test al,al` with esp balanced. jmp + 6 NOPs at the site. |
| 0x70238F | inside `CGameSprite::PlaySound` (entry 0x7011E0), PC-voice site 1 | src/CGameSprite.cpp:4126 (marker at 4125; hook site itself unresolved in our tree) | `IEex_Helper_ScaleVoiceRange` | `IEex_HookPCVoicePositional(0x70238F, 0x702398)`: cave writes `m_pArea` ([esi+0x12], src/CGameObject.h:78) into `cSound.m_nArea` ([edi+0x60], src/CSound.h:64) — positional Play bails when m_nArea is 0 or ≠ mixer m_nActiveArea (+0xF0), and PlaySound's PC branch SetChannels with nArea = 0, so the fixup is what keeps PC voices audible; then ScaleVoiceRange(&cSound), rebuild Play(x=[esi+6], y=[esi+0xA], 0, 0), `mov ecx,edi`, push the site return address, jmp 0x7A9DB0. 9-byte site → jmp + 4 NOPs. |
| 0x70256B | inside `CGameSprite::PlaySound`, PC-voice site 2 | src/CGameSprite.cpp:4126 | `IEex_Helper_ScaleVoiceRange` | Same generator, return address 0x702574. Sites 2/3 keep &cSound in edi (the sprite's embedded CSound at [esi+0x4F2C]); site 1's edi is a stack strRes. |
| 0x7027BE | inside `CGameSprite::PlaySound`, PC-voice site 3 | src/CGameSprite.cpp:4126 | `IEex_Helper_ScaleVoiceRange` | Same generator, return address 0x7027C7. |
| 0x70CB48 | inside `CGameSprite::Unmarshal(CSavedGamePartyCreature*, BOOLEAN, BOOLEAN)` (entry 0x70C600) — savegame party member | src/CGameSprite.cpp:8338 (marker `// 0x70C600` at 8337; hook site itself unresolved in our tree) | `IEex_Helper_FixSoundSetFolder` | 6 displaced bytes `mov ecx,[0x8CF6DC]` re-run in the cave after push_all / `push ebp` (CGameSprite*) / call helper / pop_all; `!jmp_dword :70CB4E` + 1 NOP. Sits right after the rep-movs that fill field_725A from `CSavedGamePartyCreature.field_25E` (char[32], src/FileFormat.h:132) and before the GetDirSounds path build. |
| 0x5A17E5 | inside `CInfGame::ImportCharacter` (entry 0x5A0F50) — .CHR import | src/CInfGame.cpp:1405 (marker `// 0x5A0F50` at 1404; hook site itself unresolved in our tree) | `IEex_Helper_FixSoundSetFolder` | 5 displaced bytes `mov eax,[0x8CF6DC]` (A1 DC F6 8C 00) re-run in the cave after the helper call; `!jmp_dword :5A17EA`. Exactly the 5-byte jmp — and this address is the target of the `jae` at 0x5A17DB, which lands cleanly on it. Sits right after the memcpy from pChrData+0x184 (+ NUL-cap). |

Cited but not hooked: `CSound::Play(BOOL)` (GLOBAL) 0x7A9B10
(src/CSound.cpp:556, marker at 555); positional `CSound::Play`
0x7A9DB0 internals 0x7A9DCF / 0x7A9DDA (m_nArea guards), 0x7A9EDF (cmp),
0x7AA204 (cdq), `m_nRangeVolume` formula site; `ResetVolume` internals;
`CDimm::AddToDirectoryList` 0x782240 (src/CDimm.cpp:58, exact marker at
57) — left stock, the helper fixes the folder name it is given;
g_pBaldurChitin 0x8CF6DC (no marker in our tree) — quoted as displaced
bytes at both FixSoundSetFolder sites; VerbalConstant internals 0x702C3D
(GLOBAL Play call), 0x702C42 (`test al,al`), 0x702D25 (NPC positional
Play), 0x70255B (the `push 0` nArea SetChannel in PlaySound's PC branch),
0x7027D0; PlaySound's return-landing `test al,al` sites 0x702398 /
0x702574 / 0x7027C7; `CSound::SetRange` (ambient override, unnamed);
`CSound::m_nRange` +0x20 (src/CSound.h:48, `#guess`), `m_nRangeVolume`
+0x24 (src/CSound.h:49), `m_nArea` +0x60 (src/CSound.h:64);
`CGameObject::m_pos` +0x6 (src/CGameObject.h:76).

## Data flow

All logic DLL-side; the Lua side only installs the stubs once inside
`IEex_DisableCodeProtection()` / `IEex_EnableCodeProtection()`.

- Per-sound-construction: `IEex_Helper_ScaleSoundRange` reads the ini
  percent, the render width and `g_fCameraZoom` (software renderer keeps
  zoom 1.0) and writes `CSound::m_nRange`. Live-tunable; ambient sounds
  may still override via `CSound::SetRange`.
- Per-attenuation-tick: Play/ResetVolume compute the listener distance
  through `IEex_Helper_EllipticalSoundDist(dx, dy)`, which reads live
  SCREENWIDTH/HEIGHT for the aspect stretch; the rest of the volume
  formula is stock (`m_nRangeVolume` math untouched).
- Per-PC-voice-play: `IEex_Helper_ScaleVoiceRange` mirrors the SFX helper
  with its own ini key (default 120%) and clamps the radius to 4600: the
  engine's `m_nRangeVolume = 100 × (m_nRange² − distance) / m_nRange²` is
  signed 32-bit, so 100×m_nRange² wraps NEGATIVE once m_nRange > ~4633 —
  muting the sound at ALL distances (which is why an earlier fixed 7680
  range silenced voices). At 4K/zoom 1 the clamp binds (edge ~83%
  audibility is the ceiling); zooming in drops below it.
- Load/import time: `IEex_Helper_FixSoundSetFolder` scans `sounds\*`,
  matches `<prefix>*.wav` against the language-independent prefix, and
  rewrites `field_725A` in place before any path build; the stock
  `CDimm::AddToDirectoryList` then registers the right folder. No bridge
  or Lua-side state anywhere in the file.

## IEex_Helper_* entry points

- `IEex_Helper_CSoundImp_DestructHook` — installed at 0x7A8E20 (our
  `CSound::~CSound`); clears invalid sounds from the mixer's internal
  lists.
- `IEex_Helper_ScaleSoundRange` — called from the 0x7A8C57 cave; sets
  m_nRange = SFX-Audible-Percent × (render width / g_fCameraZoom).
- `IEex_Helper_EllipticalSoundDist(dx, dy)` — called from the 0x7A9ED5 and
  0x7AA1FA caves; aspect-stretched distance.
- `IEex_Helper_ScaleVoiceRange(&cSound)` — called from the four PC-voice
  caves; sets m_nRange from "Voice Audible Percent" (default 120),
  clamped to 4600.
- `IEex_Helper_FixSoundSetFolder(CGameSprite*)` — called from the
  0x70CB48 and 0x5A17E5 caves; cross-language soundset folder remap.

(`IEex_WriteAssembly`, `IEex_WriteAssemblyAuto`, `IEex_HookRestore`,
`IEex_HookReturnNOPs`, `IEex_FlattenTable`,
`IEex_DisableCodeProtection`/`IEex_EnableCodeProtection` are core-DLL
API, not `IEex_Helper_*`.)
