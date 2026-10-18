# IEex_UIScale_Patch.lua — HD UI scale: canvas-scale UI render, systemic cursor remap, selective 2x de-double

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_UIScale_Patch.lua`. The
UI half of the GL scaling story: the DLL renders the full-screen UI
engines into a scaled MODELVIEW canvas, every cursor capture and draw is
remapped once and systemically, the three 512x512 FX-scratch composite
consumers get filter/inset fixes, and — behind the install-time HD-UI
gate — the engine's "2x asset" arguments are selectively forced back to 0
so exactly the right subset of art is drawn at 1x.

Render-gate coupling: 12 of the hooks below install ONLY when the ini key
`[Program Options] "3D Acceleration"` is 1 — the same gate that decides
whether `IEex_Render_Patch.lua` installs. Under the software renderer the
UI stays stock (only the save-thumbnail hook runs).

## Intent

1. **HD-UI is an install-time contract, not a toggle.** `IEEX_HD_UI =
   false` (as shipped) pushes `IEex_Helper_SetHDUI(0)` at startup. The
   WeiDU 2x-UI component — when the player installed it — copies the 2x
   assets, pre-scales the CHU files, and flips this flag; the flag must
   match how the CHU files on disk were built, because a 2x CHU rendered
   on a 1x canvas is broken and a 1x CHU on a 2x canvas is tiny. A
   runtime toggle is therefore impossible; the DLL only needs to know
   which world it is in.
2. **Menu torch gate.** `IEEX_MENU_TORCH = true` pushes
   `IEex_Helper_SetMenuTorch(1)` — the DLL-side switch behind the
   torch-render duplication fix (item 10).
3. **Save-thumbnail scale, both renderers.** `CInfGame::SynchronousUpdate`
   writes the save-game portrait thumbnail via `PrintSurfaceToBmp` with a
   hardcoded box-downsample factor of 2; with the HD 84x84 (`_S`) portrait
   art that writes a 42x42 BMP (blurry half-scale) instead of 21x21. The
   unguarded hook picks the divisor from the live surface size: 4 when
   both dimensions are 0x54 (84), else 2.
4. **STAGE 1 — the UI canvas.** A bracket around `CUIManager::Render`
   lets the DLL wrap the full-screen UI engines in a MODELVIEW transform
   that fills the window with a letterboxed 4:3 UI canvas. Everything the
   manager draws — panels, buttons, labels — scales with it; nothing in
   the engine changes.
5. **One capture source for the cursor.** Vanilla captures the raw
   hardware pointer in `CChitin::AsynchronousUpdate` and multiple UI
   layers re-map it per-screen. Instead of six per-screen `CUIManager`
   MapPoint hooks plus a Stage-2 map inside `IEex_IsUIBlockingViewport`
   (the old approach), the ONE capture site is hooked and the client-space
   `CPoint` is rewritten UI-logical in place — every downstream consumer
   (hit tests, `m_ptPointer`, dispatch) sees already-correct coordinates.
6. **Cursor draw bracket.** `CVidInf::RenderPointer3d` is bracketed about
   its hotspot so the GL cursor is drawn at UI-logical size/position; the
   epilogue hook resets MODELVIEW so the non-zoomed world frame drawn
   after the HUD composite does not inherit the cursor scale.
7. **FX-scratch composite quality.** `CVidCell::RenderTexture` is
   whole-body swapped for a filter-aware version: when a composite is
   MINIFIED (world tooltip below 4K — the pointer-101 draw eats the raw
   Stage-2 scale of 0.667 at 1440p) it must sample LINEAR, not the
   engine's nearest. The same helper is what arms the IEex_HDSprites
   half-scale draw. The other two 512x512-scratch consumers
   (`CVidMosaic::RenderTexture`, `CVidBitmap::RenderTexture`) get a
   half-texel inset instead: their quad UVs run to W/512, sampling one
   texel into the previous composite's border (ini key "FX Texel Inset").
8. **Show-FPS stays on screen.** `CVidInf::Flip` brackets the
   `DisplayFrameRate` call because under the Stretch-UI MODELVIEW the
   vanilla counter's fixed coordinates would land off-window.
9. **GL glyph-atlas cell padding.** `CVidFont::LoadGlyphs` packs glyph
   quads at exactly the font height; neighbouring atlas cells bleed
   (dashes pick up neighbours, descenders crop). +4 to the node's atlas
   cell size fixes both; line spacing is unaffected — multi-line layout
   recomputes `GetFontHeight` (= 26) separately.
10. **Menu-torch single render.** Under Wine/Mesa the engine's post-Flip
    torch re-draw lands as a "parasite frame" (a second present with
    stale state). `CScreenConnection::RenderTorch` is collapsed into a
    begin-marker and the torch content is re-issued pre-Flip from the
    `CUIManager::Render` epilogue hook instead, so exactly one torch draw
    per frame reaches the swap.
11. **The HD-UI block: selective 2x de-double (NOT installed as shipped).**
    With `IEEX_HD_UI` true, the 2x assets are in place and
    `m_bUseNewGui` is globally set — but a curated subset of art must be
    drawn at 1x anyway: fonts (the 2x INFOFONT is wrong for world-space
    text and the atlas), 92 button/BAM resrefs, ~160 MOS screen layouts,
    and 3 BMP dimension classes. Rather than shipping 1x art for those,
    the block hooks the `bDoubleSize` ARGUMENT at every entry point that
    consumes it (`CResCell::GetFrame`/`GetFrameData`,
    `CResMosaic::Get*`, `CResBitmap::GetImageData`/
    `GetImageDimensions`) and forces it to 0 when a resref or dimension
    gate matches, so the doubler never runs for exactly that subset.
    Plus the 2x-only cursor-constant rescale, the chargen paperdoll
    splice, the atlas nScale NOP, the IEEXFLT font repoint, the STATES
    portrait fix and the `RenderPortrait` force described below.

## Hooks table

Unguarded + GL-gated hooks first; the `IEEX_HD_UI`-gated block follows
after the marker row. "Gated" means the hook only installs when
`IEex_GetPrivateProfileInt("Program Options", "3D Acceleration", 1,
".\\Icewind2.ini") ~= 0`; when the key is 0 the file calls
`IEex_Helper_SetHDUI(0)` and returns.

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x5BF141 | inside `CInfGame::SynchronousUpdate` (entry 0x5BE900) | src/CInfGame.cpp:6758 (marker at 6757; hook site itself unresolved in our tree) | — | `IEex_AttemptHook` (unguarded — both renderers): save-thumbnail divisor. 6 expected bytes `6A 02 8D 4C 24 74` (push 2; lea ecx,[esp+0x74]) become a stub: push eax/edx, read portraitSize.cy @[esp+0x80] and .cx @[esp+0x7C] (esp == frame base; the same slots read at 0x5BF0F8), push 4 when both == 0x54 (84 = HD `_S` art) else push 2, pop, `!jmp_dword >savescale_done`; expected-bytes resume `!jmp_dword :5BF147`. eax (pVidMode, live @0x5BF157) and edx (vtable, live @0x5BF159) are preserved; ecx is dead. The OTHER push 2 @0x5BF150 is the nSurface argument (CVIDINF_SURFACE_2) and must not be touched. Written thumbnail becomes 21x21. |
| 0x4D4540 | `CUIManager::Render` | src/CUIManager.cpp:410 (exact marker; decl follows) | `IEex_Helper_UIScaleRenderBegin` | `IEex_HookRestore(0x4D4540, 0, 7)`: push_all, call helper, pop_all; displaced `6A FF 68 28 48 81 00` (push 0xFFFFFFFF; push 0x814828 — the SEH scope entry) re-run; resume 0x4D4547. STAGE 1 begin. Gated GL. |
| 0x4D45C3 | inside `CUIManager::Render` — the single epilogue before ret @0x4D45D3 | src/CUIManager.cpp:410 (marker at 409; hook site itself unresolved in our tree) | `IEex_Helper_UIScaleRenderEndUI` | `IEex_HookRestore(0x4D45C3, 0, 5)`: displaced `8B 4C 24 14 5F` (mov ecx,[esp+0x14]; pop edi) re-run; resume 0x4D45C8. STAGE 1 end, and the re-issue point for the menu-torch draw (item 10). Gated GL. |
| 0x78F489 | inside `CChitin::AsynchronousUpdate` (entry 0x78F0E0), m_bFullscreen branch | src/CChitin.cpp:1262 (marker at 1261; hook site itself unresolved in our tree) | `IEex_Helper_UIScaleCaptureMap` | `IEex_HookRestore(0x78F489, 0, 10)` with `!mark_esp`: lea eax,[esp+0x1C], push, call helper — the helper rewrites the client CPoint ([esp+0x1C]/[esp+0x20]) to UI-logical IN PLACE. Sits after the ScreenToClient @0x78F41F and before the pt==m_ptPointer compare, the store @0x78F4E2 and the dispatch. Displaced `8B 4C 24 1C` (mov ecx,[esp+0x1C]) + `8B 86 06 19 00 00` (mov eax,[esi+0x1906]) re-run; resume 0x78F493. The windowed branch would need a second hook — deliberately absent, the deploy target is borderless-fullscreen. Gated GL. |
| 0x7BE300 | `CVidInf::RenderPointer3d` | src/CVideo3d.cpp:2293 (exact marker; decl follows) | `IEex_Helper_UIScaleCursorBegin` | `IEex_HookRestore(0x7BE300, 0, 7)`: displaced `6A FF 68 30 5B 84 00` (push 0xFFFFFFFF; push 0x845B30 SEH) re-run; resume 0x7BE307. Cursor draw begin (size bracket about the hotspot). Gated GL. |
| 0x7BE4C1 | inside `CVidInf::RenderPointer3d` — epilogue | src/CVideo3d.cpp:2293 (marker at 2292; hook site itself unresolved in our tree) | `IEex_Helper_UIScaleRenderEnd` | `IEex_HookRestore(0x7BE4C1, 0, 10)`: displaced `5F 5E 5D 64 89 0D 00 00 00 00` (pop edi/esi/ebp; mov fs:[0],ecx) re-run; resume 0x7BE4CB. Cursor draw end — the MODELVIEW reset is what keeps the world frame from inheriting cursor scale. Gated GL. |
| 0x7C4240 | `CVidCell::RenderTexture` (STATIC, __cdecl) | src/CVidCell.cpp:2855 (exact marker; decl follows) | `IEex_Helper_CVidCell_RenderTextureHD` | `IEex_HookReplaceFunctionMaintainOriginal(0x7C4240, 6, "CVidCell::RenderTextureOriginal", {!jmp_dword >IEex_Helper_CVidCell_RenderTextureHD})` + `IEex_Helper_DefineAddress`. Whole-body swap: the helper decides filter (LINEAR when the composite is minified) and either draws half-scale (IEex_HDSprites armed) or falls through to the trampoline. Unarmed cost: one branch. Gated GL. |
| 0x7C56C0 | `CVidMosaic::RenderTexture` (__thiscall) | src/CVidMosaic.cpp:347 (exact marker; decl follows) | `IEex_Helper_CVidMosaic_RenderTextureInset` | `IEex_HookReplaceFunctionMaintainOriginal(0x7C56C0, 7, "CVidMosaic::RenderTextureOriginal", ...)` + DefineAddress; helper takes the fastcall pThis with an edx-dummy shim. Displaced `83 EC 38` / `DB 44 24 3C` verified — the `fild` reads an ARGUMENT through esp, correct only because MaintainOriginal keeps the real stack layout ("Do not pad or reorder the stub"); resume 0x7C56C7. Half-texel inset. Gated GL. |
| 0x7C6800 | `CVidBitmap::RenderTexture` | src/CVidBitmap.cpp:721 (exact marker; decl follows) | `IEex_Helper_CVidBitmap_RenderTextureInset` | Same generator: MaintainOriginal(7, "CVidBitmap::RenderTextureOriginal", ...); displaced `83 EC 44` / `DB 44 24 48`; resume 0x7C6807. Half-texel inset. Gated GL. |
| 0x79C1F8 | inside `CVidInf::Flip` (entry 0x79C1E0), before the DisplayFrameRate call | src/CVidInf.cpp:653 (marker at 652; hook site itself unresolved in our tree) | `IEex_Helper_FrameRateBegin` | `IEex_HookRestore(0x79C1F8, 0, 6)`: displaced `8B 16 6A 00 8B CE` (mov edx,[esi]; push 0; mov ecx,esi) re-run; resume 0x79C1FE. Show-FPS begin. Gated GL. |
| 0x79C201 | inside `CVidInf::Flip`, after the DisplayFrameRate call | src/CVidInf.cpp:653 (marker at 652; hook site itself unresolved in our tree) | `IEex_Helper_FrameRateEnd` | `IEex_HookRestore(0x79C201, 0, 5)`: displaced `A1 D8 F6 8C 00` (mov eax, ds:0x8CF6D8 — the CVidMode global; our tree's Flip issues the virtual `DisplayFrameRate(0)` at src/CVidInf.cpp:660, vtable slot +0x78 per src/CVidMode.h:127) re-run; resume 0x79C206. Show-FPS end. Gated GL. |
| 0x7A0BDF | inside `CVidFont::LoadGlyphs` (entry 0x7A0B20), after the node->m_nFontHeight store @0x7A0BD4 | src/CVidFont.cpp:736 (marker at 735; hook site itself unresolved in our tree) | — | `IEex_HookRestore(0x7A0BDF, 0, 5, {"83 46 40 04"})`: the replacement IS the fix — `add dword [esi+0x40], 4` on the font-registry node's atlas cell size; displaced `89 4E 44 6A 01` (mov [esi+0x44],ecx; push 1) re-run; resume 0x7A0BE4. Fixes atlas row overlap (dash bleed, descender crop). Gated GL. |
| 0x5FB020 | `CScreenConnection::RenderTorch` | src/CScreenConnection.cpp:432 (exact marker; decl follows) | `IEex_Helper_UIScaleTorchBegin` | `IEex_HookRestore(0x5FB020, 0, 5)`: displaced `A1 DC F6 8C 00` (mov eax, ds:0x8CF6DC = g_pBaldurChitin) re-run; resume 0x5FB025. Collapses the post-Flip torch re-draw (the Wine/Mesa parasite frame); the content is re-issued pre-Flip by the 0x4D45C3 hook. Gated GL, and only when `IEEX_MENU_TORCH` pushed SetMenuTorch(1). |

**Gated on `IEEX_HD_UI` — false as shipped, the whole block below does
NOT install** (after a re-`IEex_DisableCodeProtection()`):

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x79FE95, 0x79FEB6, 0x79FF62, 0x79FE98/0x79FE9C, 0x79FEB9/0x79FEBD | inside `CVidInf::RenderPointerImage` (entry 0x79FBA0) | src/CVidInf.cpp:2321 (marker at 2320; hook sites themselves unresolved in our tree) | — | `IEex_WriteByte`/`IEex_WriteDword` cursor-constant rescale for the 2x pointer set: hotspot `-7` → `-14` (lea [ecx-0x07] → [ecx-0x0E] @0x79FE95; add eax,-7 → -14 @0x79FEB6), stack-number `5` → `10` (sub edi,5 → 10 @0x79FF62), X/Y floor constants → 18 (imm8 0x12 @0x79FE98 with dword 18 @0x79FE9C; imm8 0x12 @0x79FEB9 with dword 18 @0x79FEBD). The GL rClip is a copy of m_rPointerStorage, i.e. a HARD clip — at 2x the vanilla constants clip the enlarged pointer. |
| 0x77F520 | `CResCell::GetFrame(WORD, WORD, BOOL)` | src/CResCell.cpp:43 (exact marker; decl follows) | — | `IEex_AttemptHook`: bDoubleSize arg @[esp+0x0C] (→ +0x10 after the gate's push) forced 0 when `hd_match` hits. Expected `8B 51 64 56 85 D2` (mov edx,[ecx+0x64]; push esi; test edx,edx — the quick-out when the frame is not doubled) resume `!jmp_dword :77F526`. |
| 0x77F5F0 | `CResCell::GetFrameData(FRAMEENTRY*, BOOL)` | src/CResCell.cpp:89 (exact marker; decl follows) | — | Same gate, bDoubleSize @[esp+0x08] (→ +0x0C). Expected `83 EC 10 53 8B D9` resume `:77F5F6`. |
| 0x612885 | inside `CScreenCreateChar::UpdateCharacterAppearance` (entry 0x612800) | src/CScreenCreateChar.cpp:4598 (marker at 4597; hook site itself unresolved in our tree) | — | `IEex_AttemptHook`: splices the three instructions `CScreenInventory::UpdateAppearance` carries at 0x62EF95/0x62EF9B/0x62EFA1 — mov ecx,[0x8CF6DC]; mov ecx,[ecx+0x4A28] (`m_bUseNewGui`); mov [eax+0x1444],ecx — into chargen. `CGameAnimationTypeCharacter::field_1444` (src/CGameAnimationTypeCharacter.h:103) is what the animation passes as bDoubleSize to every `CVidCell::SetResRef` it issues; without the splice the paperdoll renders "tiny character in a big disc". Expected `8B 44 24 1C 8B 8E 9A 01 00 00` resume `:61288F`. |
| 0x7A1687 | inside `CVidFont::RenderCharacters` (entry 0x7A1660) | src/CVidFont.cpp:1023 (marker at 1022; hook site itself unresolved in our tree) | — | `IEex_WriteAssembly {"!repeat(5,!nop)"}` over `mov byte [esp+0x14], 2` (nScale = 2): fonts that were de-doubled BAKE at native size into the atlas, so the draw must not scale 2x again (= 4x). |
| 0x4CB3C7 (operand @0x4CB3C6) | inside `CGameText::CGameText` (entry 0x4CB340) | src/CGameText.cpp:14 (marker at 13; hook site itself unresolved in our tree) | — | `IEex_WriteDword(0x4CB3C7, IEex_WriteStringAuto("IEEXFLT"))`: the ctor's CResRef operand (push "INFOFONT" @0x4CB3C6) is repointed to IEEXFLT — the player's STOCK 1x INFOFONT, COPY_EXISTING-captured by the WeiDU 2x component — because floating world text is 1x by design. Loading-screen parchment and worldmap labels keep the 2x INFOFONT. |
| 0x6EFFCB | inside `CGameSprite::CGameSprite` (entry 0x6EF990) | src/CGameSprite.cpp:697 (marker at 696; hook site itself unresolved in our tree) | — | `IEex_WriteDword(0x6EFFCB, 0x9090C031)` + `IEex_WriteWord(0x6EFFCF, 0x9090)`: `xor eax,eax` + 4 NOPs over `mov eax,[ecx+0x4A28]` (6 bytes) — the single m_bUseNewGui read that feeds the STATES portrait cell's bDoubleSize. The cell ctor is inlined here (SetResRef 0x58FC70 is never called; the draw goes through the inlined GetResFrame → header doubler 0x77FDA0, which corrupts other UI if hooked directly), hence the write instead of a hook. |
| 0x780310, 0x780340, 0x780370 | `CResMosaic::GetMosaicWidth` / `GetMosaicHeight` / `GetTileSize` | src/CResMosaic.cpp:21 and 31 (exact markers; decls follow); GetTileSize at src/CResMosaic.cpp:41 — our tree's marker line 40 mislabels it `0x780340` (duplicate of GetMosaicHeight's marker); 0x780370 is the fork's address | — | `IEex_AttemptHook` × 3 with `mos_match`: bDoubleSize @[esp+4] forced 0 when the MOS resref is in the list. Expected `8B 44 24 04 85 C0` (mov eax,[esp+4]; test eax,eax — quick-out when already 0) resume `:780316` / `:780346` / `:780376`. |
| 0x7803A0 | `CResMosaic::GetTileData(UINT, BOOL)` | src/CResMosaic.cpp:51 (exact marker; decl follows) | — | Same gate, bDoubleSize @[esp+8] (→ +0x0C). Expected `83 EC 08 33 D2 53` resume `:7803A6`. |
| 0x77ECF0 | `CResBitmap::GetImageData(BOOL)` | src/CResBitmap.cpp:68 (exact marker; decl follows) | — | `IEex_AttemptHook` with `bmp_match`: bDoubleSize @[esp+4] (→ +8) forced 0 when the BITMAP dimensions gate hits. Expected `83 EC 14 53 56` resume `:77ECF5`. |
| 0x77EF70 | `CResBitmap::GetImageDimensions(CSize&, BOOL)` | src/CResBitmap.cpp:169 (exact marker; decl follows) | — | Same gate, bDoubleSize @[esp+8] (→ +0x0C). Expected `8B 41 58 85 C0` (mov eax,[ecx+0x58] = bParsed; test eax,eax) resume `:77EF75`. |
| 0x704D40 | `CGameSprite::RenderPortrait` | src/CGameSprite.cpp:5597 (exact marker; decl follows) | — | `if not IEex_PortraitGridEnabled then IEex_HookRestore(0x704D40, 0, 7, {"C7 44 24 1C 01 00 00 00"})`: forces the bDoubleSize argument @[esp+0x1C] = 1 at the entry (nScale=2 image rect + HP bar). Skipped when the portrait grid is on: `IEex_PortraitGridEnabled` (IEex_Gui_Patch.lua, unresolved in our tree) means the REFONTE jmp owns this prologue, and the DLL reimplementation (`Export_RenderPortraitRect`) applies the forcing itself. |

`hd_match` (CResCell gates) pushes eax, reads
`m_pDimmKeyTableEntry` @[ecx+0x10] (src/CRes.h:48, `#guess`), then
matches the resref as two dwords (@[eax], @[eax+4]) — FULL 8-char matches
because short prefixes collide ("STON" would catch STONESML and
STONEBIG alike; "TOOL" catches TOOLFONT). Gates: NORM, TOOLFONT,
STONESML, NUMFONT, REALMS, INITIALS (the six fonts, dword-exact); a
92-entry button/BAM list (MMTRCHB, USGBTNQL, CBUT, CGEAR, CLIK2CON,
CONTBACK, FLAG1, the GBTN* family, GCOMMBTN/GCOMMSB, GUIBTACT/GUIBTBUT/
GUICTRL/GUIMAPWC/GUIRSPOR/GUIRZPOR/GUISLDR/GUISTBBC/GUISTMSC, INVBUT2/3,
ROOM* ×4, SPLBUT, the STON* family ×20, TOGGLE, NUMBER, STONEBIG,
STONESM3, HIGHLGHT, STORTINT/4, IHUM, IMISC84, IMOON, FIST, TEMP,
USPLAT15, PINCERS, WPAW/PPAW/DPAW, FORM0–B, GUIHITPT); an SP* prefix
gate (`and eax,0xFFFF; cmp 0x5053`); a US* prefix gate (cmp 0x5355); an
INFOFONT exemption placed BEFORE the I* prefix gate (cmp 0x49 after
`and eax,0xFF`). `mos_match` has the same shape against a ~160-entry MOS
list (GACTN*, GCOMM*, GMP*, GOP*, GPROGBAR, GTRB*/GTRC*/GTRS*,
GUCONTB*, GUDEATH*, the ~70 GUI** layouts, LOAD* ×15, REST/START/STARTN,
STON10*/STONEOPT, WMAP1–3, plus the minimal World-HUD variants
GCOMM11M/GCOMM12M/GACTN11M/GMWDMMBM/GMWDSLBM/GUCONTBM/GUDEATHM/GUWBTP3M/
B3QKLOOM — the same resrefs the IEex_Gui_Patch GetResObject redirect
substitutes — and IEEXOPTS/IEEXSLDR). `bmp_match` gates on
`bParsed` @[ecx+0x58] (src/CResBitmap.h:26) and `pBitmapInfoHeader`
@[ecx+0x64] (src/CResBitmap.h:29), then biWidth @[+4] / biHeight @[+8]:
420x660 (`_L`), 84x84 (`_S`) and 108x168 (`_B` bust) hit; an UNNAMED
42x42 hits too (`m_pDimmKeyTableEntry == NULL` →
`CDimm::ServiceFromFile` — the save-screen PORTRTn.BMP copy), while a
named 42x42 is left doubled.

Cited but not hooked — the removed DirectDraw-path block (commented in
the staged file): 0x79B7C6 inside `CVidInf::CreateSurfaces` (0x79B4A0,
src/CVidInf.cpp:208) — unreachable under GL because `ActivateVideoMode`
(0x79B100, src/CVidInf.cpp:64) returns `ActivateVideoMode3d` whenever
`m_bIs3dAccelerated` (src/CVidInf.cpp:66-67), so the SURFACE_4 cursor
storage is never allocated; 0x79C59A inside `CVidInf::GetCursorSurfaceSize`
(0x79C580, src/CVidInf.cpp:796) — dead and uncalled; 0x79C282 inside
`CVidInf::FullScreenFlip` (0x79C230, src/CVidInf.cpp:670) — the
DirectDraw body behind `je 0x79C25B` (@0x79C245); the 3D branch
(WindowedFlip3d) returns at 0x79C258; 0x7AE46E inside the 7-arg
`CVidCell::StoreBackground` (0x7AE3F0, src/CVidCell.cpp:619, vftable
+0x14) — its only callers are `CVidInf::RenderPointer()` (0x79F6A0,
src/CVidInf.cpp:2162, stubbed to `mov eax,0; ret` by IEex_Gui_Patch.lua)
and the DirectDraw `RenderPointer(UINT)` (0x79F950,
src/CVidInf.cpp:2268); `RenderPointer3d` calls the FIVE-arg overload at
vftable +0x10 = 0x7C4120 (src/CVidCell.cpp:1582), which only computes
the save rect and does no save-under blit. These were authored
2026-06-21 (fork commit 9983810f) when installs ran with
3D Acceleration = 0, and orphaned 2026-07-02 (8200faf8) when the
installer normalised the key to 1.

Other cited-but-not-hooked: SetResRef 0x58FC70 (src/CVidCell.cpp:3408,
exact marker at 3407) and the header doubler 0x77FDA0
(`CResCellHeader::GetFrame`, src/CResCellHeader.cpp:40, exact marker at
39) — the draw path that makes hooking the STATES cell impossible; the
splice SOURCE `CScreenInventory::UpdateAppearance` (entry 0x62EEA0,
src/CScreenInventory.cpp:4672, marker at 4671; splice instructions at
0x62EF95/0x62EF9B/0x62EFA1, sites unresolved in our tree); the removed
per-screen m_bUseNewGui forces 0x686DE0 (inside `CScreenWorld::
EngineGameInit`, src/CScreenWorld.cpp:480, marker at 479) and 0x4D3B80
(`CUIManager::fInit`, src/CUIManager.cpp:54, marker at 53); globals
g_pBaldurChitin 0x8CF6DC and the CVidMode global 0x8CF6D8 and the SEH
scope entry 0x814828 (none carries a marker in our tree — quoted only as
displaced bytes); `CVidMode::DisplayFrameRate` vtable slot +0x78
(src/CVidMode.h:127).

## Data flow

No Lua-side state at all — no bridges, no tables, no closures. The file
is a one-shot installer: read two config gates, push two DLL booleans,
install stubs, done. Everything runtime lives DLL-side:

- `IEex_Helper_SetHDUI` / `SetMenuTorch` — DLL booleans set ONCE at
  startup. SetHDUI gates the Stage-1 UI canvas, the cursor transforms
  and the composite filters; the WeiDU 2x component flips the Lua-side
  `IEEX_HD_UI` at install time (with the pre-scaled CHUs), never at
  runtime.
- The DLL owns the scale state itself: Stage-1 UI MODELVIEW (between
  UIScaleRenderBegin/UIScaleRenderEndUI), Stage-2 world scale (read live
  by the composite filter), the UI-logical transform applied by
  UIScaleCaptureMap, and the cursor bracket state. The Lua hooks only
  bracket engine frames; B1 pins the exact state layout.
- Per-frame readers/writers: `CChitin::AsynchronousUpdate` (writer —
  rewrites the captured client point), `CUIManager::Render` (bracket),
  `RenderPointer3d` (bracket), `CVidInf::Flip` (FPS overlay bracket),
  `RenderTorch` (collapse), the three RenderTexture hooks (read-only
  filter/inset decisions per draw).
- ini keys read DLL-side at draw time: "FX Texel Inset" (the
  Mosaic/Bitmap inset amount). The GL gate key `[Program Options]
  "3D Acceleration"` is read once by the Lua installer.
- Cross-file seams: `IEex_PortraitGridEnabled` and
  `IEex_Extern_InitHighResolutionPaddingPanels` are defined outside this
  file (IEex_Gui_Patch.lua resp. the DLL extern — both unresolved in our
  tree); `IEex_Extern_InitHighResolutionPaddingPanels` sets `m_bUseNewGui`
  (fork name for CBaldurChitin +0x4A28, src/CBaldurChitin.h:200) and
  +0x4A2C (src/CBaldurChitin.h:201) GLOBALLY, which is why this file's
  old per-screen forces at 0x686DE0/0x4D3B80 are removed, and why
  `UIMult()` reads 0x4A28. The 0x7A12C6 NOP (IEex_Gui_Patch.lua) routes
  m_bDoubleSize fonts through the glyph-atlas fast path that the
  0x7A0BDF +4 fix repairs. The RenderTextureHD filter arms only when
  IEex_HDSprites registered the cell's resref.

## IEex_Helper_* entry points

- `IEex_Helper_SetHDUI(0/1)` — HD-UI master gate (install-time).
- `IEex_Helper_SetMenuTorch(1)` — menu-torch feature gate
  (install-time).
- `IEex_Helper_UIScaleRenderBegin` — STAGE 1 begin at 0x4D4540.
- `IEex_Helper_UIScaleRenderEndUI` — STAGE 1 end + torch re-issue at
  0x4D45C3.
- `IEex_Helper_UIScaleCaptureMap` — systemic cursor→UI-logical remap at
  0x78F489.
- `IEex_Helper_UIScaleCursorBegin` — cursor draw begin at 0x7BE300.
- `IEex_Helper_UIScaleRenderEnd` — cursor draw end (MODELVIEW reset) at
  0x7BE4C1.
- `IEex_Helper_CVidCell_RenderTextureHD` — whole-body RenderTexture
  replacement (filter swap + HDSprites half-scale) at 0x7C4240.
- `IEex_Helper_CVidMosaic_RenderTextureInset` — half-texel inset at
  0x7C56C0.
- `IEex_Helper_CVidBitmap_RenderTextureInset` — half-texel inset at
  0x7C6800.
- `IEex_Helper_FrameRateBegin` / `IEex_Helper_FrameRateEnd` — Show-FPS
  overlay bracket at 0x79C1F8 / 0x79C201.
- `IEex_Helper_UIScaleTorchBegin` — RenderTorch collapse at 0x5FB020.
- `IEex_Helper_DefineAddress(name, address)` — publishes the three
  `...RenderTextureOriginal` trampolines to the DLL by name.

(`IEex_HookRestore`, `IEex_HookReplaceFunctionMaintainOriginal`,
`IEex_AttemptHook`, `IEex_WriteAssembly`, `IEex_WriteAssemblyAuto`,
`IEex_WriteStringAuto`, `IEex_WriteByte`, `IEex_WriteWord`,
`IEex_WriteDword`, `IEex_GetPrivateProfileInt`, `IEex_Label`,
`IEex_DisableCodeProtection`/`IEex_EnableCodeProtection` and the
`!mark_esp`/`!repeat`/`!mov`/`!jmp_dword` assembler forms are core-DLL
API, not `IEex_Helper_*`.)
