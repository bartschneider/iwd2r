# IEex_MiscFixes_Patch.lua — nine small engine fixes

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_MiscFixes_Patch.lua`.
Fork-authored bug-fix collection (store-identify crash, two chargen spell
panel bugs, action-bar count digit, cutscene log flicker). Three of the nine
fixes only surface at the engine's 2x UI tier, which is why they ride along
with the HD work.

## Intent

Fix five distinct engine bugs, each with a root-cause comment in the file:
(1) buying an unidentified item from a store crashes because the engine
flags it Identified mid-transaction; (2) the chargen MEMORISE spell panels
print the spell level one too high because `CScreenCreateChar::m_nSpellLevel`
(`BYTE` @ this+0x14A2) doubles as a loop counter the level walk leaves at 2;
(3) the chargen memorise panels pass the wrong `bDoubleSize` argument to
`CIcon::RenderIcon`, so at 2x the spell icons draw 16px off their slot and
get clipped; (4) the action bar's uses-remaining digit lands centred in the
button at 2x because `RenderIcon`'s `bForceCount` branch never scales
`LAST_DIGIT_OFFSET` (25,25) @0x8D7E68/0x8D7E6C; (5) the combat log / message
window flickers during single-player cutscenes because three
`CheckPanelInputMode` rules alias to panel 0 and fight each other every tick.
Fixes 2-5 are original Black Isle bugs (the pristine pre-mod exe is
byte-identical at the patched sites, per the comments); fix 3 and 4 are
invisible at 1x.

The whole file runs once inside `IEex_DisableCodeProtection()` /
`IEex_EnableCodeProtection()`.

## Hooks table

All `IEex_AttemptHook` sites are guarded writes — the fork's byte-pattern-
verified patch primitive; see the note under Data flow. Byte patterns are
quoted verbatim.

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x54C2A5 | inside `CStore::RemoveItemFromStore` (entry 0x54C230) | src/CStore.cpp:514 (marker `// 0x54C230` at 513) | — | `IEex_WriteAssembly(0x54C2A5, {"!repeat(3,!nop)"})` — NOPs a 3-byte instruction on the bought-item identify path so the flag flip that both mislabels the item and breaks the store's stock lookup (crash) is gone. |
| 0x60A920 | `CScreenCreateChar::ResetMemorizeArcaneSpellsPanel` | src/CScreenCreateChar.cpp:1691 (exact marker; decl follows) | — | `IEex_AttemptHook`: expected `C6 81 A2 14 00 00 01` = `mov byte ptr [ecx+0x14A2], 1`; replacement bytes `6A FF 68 08 9F 82 00` = the SEH prologue prefix `push -1 / push 0x829F08`, resume `:60A927`. Effect (per the file's root-cause comment): force `m_nSpellLevel` (this+0x14A2) = 1 on entry, replacing the first 7 prologue bytes (same length — "ecx = this at the entry, before the SEH prologue runs"). |
| 0x60AF60 | `CScreenCreateChar::ResetMemorizeDivineSpellsPanel` | src/CScreenCreateChar.cpp:1789 (exact marker; decl follows) | — | Same fix, handler `push 0x829F38`, resume `:60AF67`. |
| 0x60B610 | `CScreenCreateChar::ResetMemorizeDomainSpellsPanel` | src/CScreenCreateChar.cpp:1882 (exact marker; decl follows) | — | Same fix, handler `push 0x829F68`, resume `:60B617`. |
| 0x620AAE | inside `CUIControlButtonCharGenMemorizedArcaneSpellSelection::Render` (entry 0x620970) | src/CScreenCreateChar.cpp:10194 (exact marker `// 0x620970`; decl follows; hook site itself unresolved in our tree) | — | `IEex_AttemptHook`: expected `6A 00 6A 00 8B 56 06 8B 12 8B 8A AA 00 00 00 51`, replacement `6A 00 6A 00 50` with `!jmp_dword :620AB3`. The file: "Replaces `6A 00 6A 00 50` outright (3 pushes in, 3 pushes out — esp is unchanged at the jump back)" — the wrong `push eax` (disabled-flag pushed into `RenderIcon`'s `bDoubleSize` slot at 620AB2) is removed so the unmodified bytes that follow load the real `m_pPanel->m_pManager->m_bDoubleSize` (m_pPanel @ this+0x6, m_bDoubleSize @ m_pManager+0xAA) and push it; `dwFlags` stays 0. |
| 0x62140E | inside `CUIControlButtonCharGenMemorizedDivineSpellSelection::Render` (entry 0x6212D0; serves divine + domain) | src/CScreenCreateChar.cpp:10365 (exact marker `// 0x6212D0`; decl follows; hook site itself unresolved in our tree) | — | Identical pattern and fix, resume `:621413`. |
| 0x4E6A67 | inside `CIcon::RenderIcon`, `bForceCount` branch (entry 0x4E66E0) | src/CIcon.cpp:20 (marker at 19; hook site itself unresolved in our tree) | — | `IEex_AttemptHook`: expected `8B 0D 6C 7E 8D 00 0F AF CB A1 68 7E 8D 00 0F AF C3` (mov+imul pairs — the fixed form), replacement `8B 0D 6C 7E 8D` (the unscaled `mov ecx,[0x8D7E6C]`), new asm `!jmp_dword :4E6A72`. Per the file: "mov ecx,[8D7E6C] ; imul ecx,ebx ; mov eax,[8D7E68] ; imul eax,ebx / Both movs are replicated: the 5-byte jmp only covers the first, so resume past the second at 4E6A72" — mirrors the sibling `wCount` branch (0x4E693F/0x4E698A) and scales both `LAST_DIGIT_OFFSET` (25,25) @0x8D7E68/0x8D7E6C components by `ebx`, the scale register. Deliberately ungated by renderer: at 1x imul by 1 is a no-op. Of `RenderIcon`'s fourteen call sites exactly one passes `bForceCount` TRUE (0x595790, action-bar spell/ability slot with STONSPEL/STONSPEC art), so inventory (0x62E30E), stores, ground containers (0x6963F8) and chargen are untouched. |
| 0x68C6B5 | inside `CScreenWorld::AsynchronousUpdate` (entry 0x68C3D0) | src/CScreenWorld.cpp:1663 (marker at 1662; hook site itself unresolved in our tree) | — | Guarded `IEex_WriteAssembly(0x68C6B5, {"!jz_byte", {0x68C730, 1, 1}})` — `je 0x68C6D1` → `je 0x68C730`: the first of the two single-player branches of the inlined `GetPanel_22_0` (mask 0x8) is retargeted past the "panel 0" result at 0x68C6D1 to the next rule at 0x68C730 ("both fit in rel8, so no length change"). Kills the intra-tick disable/re-enable flap on the message window in cutscenes/dialogues — each edge runs the full `CUIPanel::SetEnabled` (0x4D29D0) body, which unloads and re-registers the combat-log `CVidFont`, and `CUIPanel::Render` (0x4D3100) draws `RenderDither` over frames sampling the FALSE half. |
| 0x68C6C8 | inside `CScreenWorld::AsynchronousUpdate` | src/CScreenWorld.cpp:1663 | — | Second single-player branch of `GetPanel_22_0`, same retarget `je 0x68C6D1` → `je 0x68C730`. Multiplayer untouched: there `GetPanel_22_0/19_0/21_0` return 22/19/21 and panel 22 keeps its own 0x8 rule. |

The file also cites (no patch): `sub_617D80` — the spell-level walk whose
`for (m_nSpellLevel = 1; m_nSpellLevel <= field_14A3; ++m_nSpellLevel)` loop
keeps its counter in the member (increment sites 0x618B54/0x618B5A/0x618B60)
— unresolved in our tree; the ctor store `field_14A3 = 1` at 0x606B3E (inside
`CScreenCreateChar::CScreenCreateChar`, src/CScreenCreateChar.cpp:145);
`ResetArcaneSpellsPanel` 0x60A5C0 (src/CScreenCreateChar.cpp:1628, immune
because it re-inits the member at its top); the learn panel's correct arg
pushes 0x61B893-0x61B89C inside
`CUIControlButtonCharGenKnownArcaneSpellSelection::Render` 0x61B750
(src/CScreenCreateChar.cpp:8070); `CInfButtonArray` action-bar geometry
0x5950F0 (our name `sub_5950F0`, src/CInfButtonArray.cpp:4685) and its
spell/ability call site 0x595790; inventory count call site 0x62E30E (inside
`CUIControlButtonInventorySlot::Render`, src/CScreenInventory.cpp:6051);
ground-container call site 0x6963F8 (inside
`CUIControlButtonWorldContainerSlot::Render`, src/CScreenWorld.cpp:4917);
dragged-item cursor `CVidInf::RenderPointerImage` 0x79FBA0
(src/CVidInf.cpp:2320); `CIcon::RenderIcon` internals 0x4E693F, 0x4E698A,
0x4E69C7, 0x4E69E4, 0x4E69ED, 0x4E6A6D, 0x4E6A72; `AsynchronousUpdate`
branch targets 0x68C6D1, 0x68C730; `CUIPanel::SetEnabled` 0x4D29D0
(src/CUIPanel.cpp:69) and `CUIPanel::Render` 0x4D3100 (src/CUIPanel.cpp:313).

## Data flow

No persistent state and no per-frame readers/writers added. All nine fixes
are static machine-code surgery applied once at patch time; after that the
engine runs its own code paths with the patched instructions in place. The
one runtime behaviour change is structural: `AsynchronousUpdate` no longer
emits the contradictory `SetEnabled` edges for panel 0, so end-of-tick panel
state is unchanged in every mode ("within a tick the two 0x100 rules fire
exactly when the removed 0x8 rule would have, and — running last — they
already decided the final value").

Guarding is the theme of this file: every `IEex_AttemptHook` verifies code
bytes before writing (the "attempt": on mismatch the fix is skipped instead
of corrupting the process), and the one bare `IEex_WriteAssembly` that is
not length-NOPs explicitly reads the bytes first
(`IEex_ReadWord(0x68C6B5, 0) == 0x1A74 and IEex_ReadWord(0x68C6C8, 0) ==
0x0774`) and prints "[?] Unexpected bytes at 0x68C6B5 / 0x68C6C8 - cutscene
panel-0 flap fix not applied." otherwise. `IEex_AttemptHook`'s exact
argument protocol (verified-original bytes, replacement bytes, displaced-
instruction stub with resume target, already-applied pattern) is core-DLL
API and not recoverable from the Lua side alone — B1 should pin the
primitive down in the DLL; the per-fix semantics above come from the file's
own comments, which are unambiguous.

## IEex_Helper_* entry points

None called, none installed. (`IEex_ReadWord`, `IEex_WriteAssembly`,
`IEex_AttemptHook`, `IEex_DisableCodeProtection`/`IEex_EnableCodeProtection`
are core-DLL API.)
