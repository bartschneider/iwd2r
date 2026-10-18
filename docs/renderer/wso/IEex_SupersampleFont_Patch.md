# IEex_SupersampleFont_Patch.lua — native-2x AA font atlases (non-pixelated fonts)

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_SupersampleFont_Patch.lua`.
The 2x-UI "non-pixelated fonts" option. Six function-head replacements, all
jumping into DLL-side render helpers, with the original bodies kept as named
trampolines.

## Intent

Replace the engine's 1-bit-alpha font draws with sharp GL draws from
native-2x anti-aliased atlases. Two sub-features, both OpenGL-only (the file
returns immediately when `[Program Options] 3D Acceleration` is 0):

1. Floating world text (`CGameText`): the engine CPU-blits it
   (`Blt8To32`) into the world surface, so glyphs are pixelated and — with
   1-bit alpha — any AA at the stock ~8px cap is too coarse to look smooth.
   The option ships `IEEXFLT.BAM` as a native-2x AA atlas (Play Bold) and
   `IEex_Helper_CGameText_RenderHD` takes over `CGameText::Render` entirely:
   each line is redrawn as GL quads from the cached atlas plus a
   translucent readability panel. The block FOLLOWS the world (its anchor
   is mapped through the camera zoom) but its SIZE does not: one atlas
   texel per screen pixel at every zoom, snapped to whole pixels — the old
   draw was half scale UNDER the zoom matrix, which minified the 2x atlas
   at zoom 1-2 and gave back the mush the option exists to remove.
   `[IEex Options] Floating Text Size` (percent) scales it further.
2. Portrait HP digits (`CVidFont::TextOut`, scoped to the NUMFONT resref
   inside the helper): redrawn from an 8-bit coverage atlas
   (`Override/IEEXHPF.BMP`). Rationale per the comment: the engine font
   path has 1-BIT alpha and `CVidFont::RealizePalette` (0x793570) rebuilds
   the palette as a ramp from index1 = BACKGROUND to index255 = text
   colour, so every AA edge pixel composites over BLACK rather than over
   the portrait behind it — dark fringe, hard 1-bit shadow. Hooked on the
   font draw rather than the portrait render because the latter is the
   Modern HUD's reimplementation of `CGameText::RenderPortrait`; hooking it
   would have left every 2x-UI-without-Modern-HUD install on the old look.

The hook primitive throughout is
`IEex_HookReplaceFunctionMaintainOriginal(address, prologueLen, "Name", asm)`:
overwrite the function's `prologueLen`-byte prologue with a jump into the
named `IEex_Helper_*` implementation, keeping the original body reachable
at a generated trampoline label `"Name"`; `IEex_Label("Name")` resolves the
label and `IEex_Helper_DefineAddress("Name", ...)` publishes the trampoline
address to the DLL by name (the DLL bodies call the trampolines — "the
overlay itself calls the TRAMPOLINE (no re-entry)").

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x793720 | `CVidFont::TextOut` | src/CVidFont.cpp:329 (exact marker; decl follows) | `IEex_Helper_CVidFont_TextOutHP` | 8-byte prologue (`sub esp,0x10 / push ebp / mov ebp,[esp+0x18]`) → `!jmp_dword >IEex_Helper_CVidFont_TextOutHP`; trampoline `CVidFont::TextOutOriginal` published. HP digits from the coverage atlas; helper-scoped to NUMFONT, everything else trampolines through. |
| 0x7A1210 | `CVidFont::TextOut3d` | src/CVidFont.cpp:942 (exact) | `IEex_Helper_CVidFont_TextOut3dHD` | 7-byte prologue (`push -1 / push 0x845028`) → `IEex_Helper_CVidFont_TextOut3dHD`; trampoline `CVidFont::TextOut3dOriginal`. Wraps a registered font's draw in a MODELVIEW scale about the anchor; inert unless "Floating Text Size" ≠ 100; protects other cached-path consumers of a registered font. |
| 0x7A0F00 | `CVidFont::CreateTexture` | src/CVidFont.cpp:876 (exact) | `IEex_Helper_CVidFont_CreateTextureHD` | 5-byte prologue (`lea eax,[esp+8] / push esi`) → `IEex_Helper_CVidFont_CreateTextureHD`; trampoline `CVidFont::CreateTextureOriginal`. Forces LINEAR filtering on a registered font's 2x atlas — NEAREST would drop or duplicate texels at any scale ≠ 1.0 (i.e. "Floating Text Size" > 100). |
| 0x4CBB20 | `unresolved in our tree` (patch names `CGameText::Render`; our tree has no such function — src/CGameText.h declares none; nearest preceding marker is `CGameText::IsOnScreen` entry 0x4CB9B0 at src/CGameText.cpp:156, a short function the hook address lies 0x170 past) | — | `IEex_Helper_CGameText_RenderHD` | 7-byte prologue (`push -1 / push 0x813e88`) → `IEex_Helper_CGameText_RenderHD`; trampoline `CGameText::RenderOriginal`. OWNS the floating-text render for registered fonts (sharp GL overlay + readability panel); stock path skipped. |
| 0x793050 | `CVidFont::GetFontHeight` | src/CVidFont.cpp:156 (exact) | `IEex_Helper_CVidFont_GetFontHeightHD` | 5-byte prologue (`mov eax,ds:0x8cf6d8`) → `IEex_Helper_CVidFont_GetFontHeightHD`; trampoline `CVidFont::GetFontHeightOriginal`. Reports HALF the native-2x atlas height: the engine word-wraps at `SetText` time from the FONT's metrics against a box it sizes itself (`GetFXSize/2`), a budget in STOCK 1x terms (~40 chars/line, ~16 lines); fed the full atlas, lines break at ~20 chars and `m_nMaxLines` comes out ~8, so `SplitString` runs out of lines and dumps the remainder onto one very long last line. |
| 0x792D40 | `CVidFont::GetStringLength` | src/CVidFont.cpp:42 (exact) | `IEex_Helper_CVidFont_GetStringLengthHD` | Same 5-byte prologue and halving rationale → `IEex_Helper_CVidFont_GetStringLengthHD`; trampoline `CVidFont::GetStringLengthOriginal`. |

Cited but not hooked: `CVidFont::RealizePalette` 0x793570
(src/CVidFont.cpp:298, exact) — named in the TextOut rationale; the global
`ds:0x8cf6d8` quoted as prologue bytes (no marker in our tree); the wrap
machinery the metric hooks feed: `CGameText::SetText`
(src/CGameText.cpp:42) calls `GetFXSize` at src/CGameText.cpp:51 and
`CUtil::SplitString` at src/CGameText.cpp:71, with `m_nMaxLines` (0x650),
`m_szLine`, `m_nLines` in src/CGameText.h; assets `IEEXFLT.BAM` and
`IEEXHPF.BAM` (override/).

## Data flow

Static one-time surgery inside `IEex_DisableCodeProtection()` /
`IEex_EnableCodeProtection()`; all per-frame behaviour lives in the DLL
helpers. The registry is the state: `IEex_Helper_RegisterSupersampleFont`
is called for exactly one resref, `"IEEXFLT"` — per the closing comment, a
registered resref WITHOUT a 2x BAM would draw at half its intended size, so
registration must match the atlases actually shipped. Every other font
falls through to its trampoline (stock rendering). The DLL resolves the six
`...Original` trampolines through the `IEex_Helper_DefineAddress` name→
address table built at patch time — that table is also how the DLL bodies
reach engine code without hard-coded addresses (relevant to B1: these six
names are the DLL's handles on the original bodies).

Per-frame readers/writers: the render helpers read the
"Floating Text Size" percentage (ini, read DLL-side), the font registry,
and the cached GL atlas textures the helper suite maintains; the engine's
own `SetText` word-wrap keeps running unmodified on the halved metrics.

## IEex_Helper_* entry points

- `IEex_Helper_DefineAddress(name, address)` — publishes a named address
  (here: each `...Original` trampoline) to the DLL; called six times, and
  with `-1` by other patch files as a "feature off" sentinel.
- `IEex_Helper_RegisterSupersampleFont(resref)` — registers a font resref
  whose draws the HD font path may take over (only `"IEEXFLT"`).
- `IEex_Helper_CVidFont_TextOutHP` — DLL implementation installed at
  0x793720 (coverage-atlas HP digits).
- `IEex_Helper_CVidFont_TextOut3dHD` — installed at 0x7A1210 (MODELVIEW
  scale wrap).
- `IEex_Helper_CVidFont_CreateTextureHD` — installed at 0x7A0F00 (LINEAR
  filtering on 2x atlases).
- `IEex_Helper_CGameText_RenderHD` — installed at 0x4CBB20 (floating-text
  overlay + readability panel).
- `IEex_Helper_CVidFont_GetFontHeightHD` / `IEex_Helper_CVidFont_GetStringLengthHD`
  — installed at 0x793050 / 0x792D40 (halved metrics for the engine wrap).

(`IEex_HookReplaceFunctionMaintainOriginal`, `IEex_Label`,
`IEex_GetPrivateProfileInt`, `IEex_DisableCodeProtection`/
`IEex_EnableCodeProtection` are core-DLL API, not `IEex_Helper_*`.)
