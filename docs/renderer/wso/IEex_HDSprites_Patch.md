# IEex_HDSprites_Patch.lua — HD creature sprites (native-2x BAMs)

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_HDSprites_Patch.lua`.
Registered creature resrefs ship native-2x BAMs; the GL renderer composites
their layers into the FX scratch as stock, then the final composite quad is
drawn at HALF scale (a MODELVIEW wrap inside the `RenderTexture`
trampoline) so the logical world size is unchanged and camera zoom samples
2x texels.

## Intent

Ship 2x creature art for registered resrefs without touching the engine's
animation or compositing logic: draw stock-shaped but sample from 2x
texels, and let the existing camera zoom turn that into real detail. Two
`IEex_HookReplaceFunctionMaintainOriginal` trampolines do the arming and
the size bookkeeping (bodies in IEexHelper.dll render.cpp); the third link
of the chain — the half-scale MODELVIEW wrap — is NOT installed here.

Staged state matters: the file is shipped with `local IEEX_HD_SPRITES =
false`, i.e. the feature is OFF in this drop. The off path runs
`IEex_Helper_DefineAddress("CVidCell::FXRender3dOriginal", -1)` and
`("CVidInf::FXBltToBackOriginal", -1)` — the DLL's "feature disabled"
sentinel — and returns before any hooking. The WeiDU component "HD
Creature Sprites" flips the local on at install (like IEEX_HD_UI); the 2x
BAMs only exist in override/ when installed. A second gate,
`[Program Options] 3D Acceleration` = 0 (software renderer), takes the same
off path: the half-scale draw is a GL-path concept, and unhooked the engine
runs stock bytes while 1x BAMs from the biffs keep working.

Cross-file coupling (spelled out in the header): `0x7C4240
CVidCell::RenderTexture` — which wraps the original draw in `scale(0.5)`
about the anchor — is hooked UNCONDITIONALLY by `IEex_UIScale_Patch.lua`
(it also serves the sub-4K world tooltip), so this file must NOT re-hook it
or redefine the `CVidCell::RenderTextureOriginal` trampoline; UIScale loads
first (per the `IEex_IWD2_Patch.lua` file list).

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x7C5330 | `CVidCell::FXRender3d` | src/CVidCell.cpp:3320 (exact marker; decl follows) | `IEex_Helper_CVidCell_FXRender3dHD` | `IEex_HookReplaceFunctionMaintainOriginal(0x7C5330, 6, "CVidCell::FXRender3dOriginal", ...)`: 6-byte prologue → `!jmp_dword >IEex_Helper_CVidCell_FXRender3dHD`; trampoline published via `IEex_Helper_DefineAddress`. Arms only when the cell's BAM resref is registered (checked inside the helper); unregistered creatures fall through untouched. Installed only when IEEX_HD_SPRITES is on and the renderer is GL. |
| 0x79CC90 | `CVidInf::FXBltToBack` | src/CVidInf.cpp:1083 (exact) | `IEex_Helper_CVidInf_FXBltToBackHD` | Same primitive, 6-byte prologue → `IEex_Helper_CVidInf_FXBltToBackHD`; trampoline `CVidInf::FXBltToBackOriginal`. Halves the ref-point subtraction (mirror-aware) so the 2x-texel composite lands at the stock logical position. Same on/off conditions. |
| 0x7C4240 | `CVidCell::RenderTexture` — NOT hooked here; owned by `IEex_UIScale_Patch.lua` | src/CVidCell.cpp:2854 (exact) | — | The half-scale MODELVIEW wrap about the anchor; this file only documents the dependency (must not re-hook or redefine `CVidCell::RenderTextureOriginal`). |

## Data flow

No persistent Lua state and no per-frame Lua readers. The feature's state is
the DLL-side registry of registered resrefs, populated once at patch time:
181 resrefs — goblin MGO2 (including the E mirror variants), dwarf male
CDMB1, and human male+female CHMB/CHFB armour tiers 1-3, full anim sets
("a single action anim alone is invisible on an idle/walking creature").
The header notes this is a GENERATED POC roster (from `hd_sprites_wip/poc/`)
that Phase E replaces with a generated `IEex_HDSprites_List.lua`. Every
resref must have a native-2x BAM in override/.

Per frame: `CVidCell::FXRender3d` renders the (2x-texel) layers into the FX
scratch as stock; `CVidCell::RenderTexture`'s trampoline wrap scales the
final composite quad by 0.5 about the anchor; `CVidInf::FXBltToBack`'s
halved ref-point math keeps screen placement identical to stock. The
camera zoom then samples the 2x texels — the "HD" is entirely in what the
zoom picks up.

## IEex_Helper_* entry points

- `IEex_Helper_DefineAddress(name, address)` — publishes trampoline
  addresses to the DLL by name; called here with `-1` for both names when
  the feature is off (the DLL's disabled sentinel) and with
  `IEex_Label(...)` values when on.
- `IEex_Helper_RegisterHDSprite(resref)` — registers one resref for the HD
  path; called 181 times with the POC roster.
- `IEex_Helper_CVidCell_FXRender3dHD` — DLL implementation installed at
  0x7C5330.
- `IEex_Helper_CVidInf_FXBltToBackHD` — DLL implementation installed at
  0x79CC90.

(`IEex_HookReplaceFunctionMaintainOriginal`, `IEex_Label`,
`IEex_GetPrivateProfileInt`, `IEex_DisableCodeProtection`/
`IEex_EnableCodeProtection` are core-DLL API, not `IEex_Helper_*`.)
