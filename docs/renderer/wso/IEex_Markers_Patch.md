# IEex_Markers_Patch.lua — per-character selection-marker colour

Staged file: `~/.local/share/iwd2r/wso/lua/IEex_Markers_Patch.lua`. One
hook, heavily documented in the file itself.

## Intent

Tint each controlled PC's selection circle / move reticle with that
character's own clothing colour instead of the shared green, plus a
configurable stroke thickness. Settings live under `[IEex Options]` in
`Icewind2.ini` (read lazily by the DLL helper, not by this file):
`Colored Selection Circles`, `Selection Circle Thickness`, `Colored Portrait
Frames`, `Portrait Frame Thickness`, `Selection Circle Color Slot`,
`Selection Circle Min Brightness`.

## Hooks table

| address | engine function (our name) | iwd2r file:line | helper called | what changes |
|---|---|---|---|---|
| 0x766C06 | inside `CMarker::AsynchronousUpdate` (function entry 0x766900) | src/CMarker.cpp:69 (marker `// 0x766900` at 68) | `IEex_Helper_MarkerTintPC` | `IEex_HookRestore(0x766C06, 0, 6, ...)`: saves all registers (`!push_all_registers_iwd2` / `!pop_all_registers_iwd2`), pushes `edi` (CGameSprite*) and `esi` (CMarker*), calls `IEex_Helper_MarkerTintPC`, restores. The helper rewrites `m_rgbColor` (`[esi+6]`) after all vanilla colour branches have stored their base colour and before the picked/hover pulse ramp at 0x766C43 runs. The 6 displaced bytes are `mov eax,[edi+0x712A]` = `m_talkingCounter` (our field layout confirms: `/* 712A */ int m_talkingCounter;` in src/CGameSprite.h:1012), resume at 0x766C0C. |

The file also *cites* (no patch): `CMarker::AsynchronousUpdate` entry
0x766900, pulse ramp 0x766C43, and the DrawEllipse3d / DrawRecticle3d
overrides installed by `IEex_CameraZoom_Patch.lua` as the renderer-gated
half (thickness) of the feature.

## Data flow

- Per-frame read/write: `CMarker::AsynchronousUpdate` runs on the async
  (AI/render-adjacent) tick for both `m_marker` and `m_destMarker` of every
  `CGameSprite`; the hook is at the branch join so the helper sees the final
  base colour in `m_rgbColor` and the engine animates (pulses) whatever the
  helper left there.
- The helper recognises the two controlled-PC greens (red and blue both 0 —
  talking white, morale yellow, enemy red, neutral cyan fail that test and
  pass through untouched) and substitutes the character's clothing colour
  from the configured palette slot, clamped by the configured minimum
  brightness. That recognition logic and the ini options live in the DLL;
  the Lua side ships none of it.
- Deliberately not renderer-gated (the file says so): the colour decision is
  cheap and renderer-independent; only stroke *thickness* is OpenGL-only and
  that part lives in the camera-zoom patch's ellipse/recticle overrides.

## IEex_Helper_* entry points

- `IEex_Helper_MarkerTintPC(CMarker* esi, CGameSprite* edi)` — called with
  the marker and sprite pushed; mutates `m_rgbColor` in place. Also owns the
  `[IEex Options]` keys listed under Intent (portrait-frame tint shares the
  same helper per the ini key names).
