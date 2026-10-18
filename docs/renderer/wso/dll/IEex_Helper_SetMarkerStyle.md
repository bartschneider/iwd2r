# IEex_Helper_SetMarkerStyle

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002e1b0` (Lua registration: ExposeFunctions)
- Decompiled C: `runs/dll-decomp/FUN_1002e1b0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Gui_State.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
undefined4 FUN_1002e1b0(void)

{
  undefined4 *puVar1;

  DAT_10092770 = 0xffffffff;
  puVar1 = &DAT_104df264;
  DAT_100927b0 = 0xffffffff;
  DAT_100927b4 = 0xffffffff;
  DAT_10092774 = 0xffffffff;
  DAT_1009277c = 0xffffffff;
  DAT_10092760 = 0xffffffff;
  DAT_10092788 = 0xffffffff;
  do {
    puVar1[-1] = 0;
    *puVar1 = 0;
    puVar1 = puVar1 + 2;
  } while ((int)puVar1 < 0x104df364);
  DAT_10094507 = 1;
  return 0;
}
```

## Notes

### Shared marker machinery (both helpers)

One pipeline: the engine draws PC selection markers, destination
markers and portrait frames in per-PC shades of green, and the fork
recolors them from the creature's own palette colors while keeping
each sprite's original green *intensity* (decompiled from the fork's
`IEexHelper.fork.dll`).

#### The (sprite -> green) pair table

- 32 dword pairs at `DAT_104df260` (keys) / `DAT_104df264` (colors),
  8 bytes per entry, range checked against `0x104df364`.
- Key = the engine sprite object pointer (the value the hook stub
  forwards and the value `RenderPortraitRect` matches its sprites
  against), value = the pure-green COLORREF the engine used for that
  sprite's marker (`0x00GG00` family).
- Insert is round-robin: slot = counter `DAT_104df5dc`, used
  post-increment, masked `& 0x1f`. A search hit updates in place; a
  non-green marker color deregisters (entry key cleared to 0). No
  lock, no allocation.

#### The seven lazy INI caches

All are `GetPrivateProfileIntA("IEex Options", key, default,
".\Icewind2.ini")` on first use; sentinel `0xffffffff` = "not read
yet"; all seven reset by `SetMarkerStyle`:

| Cache | INI key | Default | Clamp / fallback | Read by |
|---|---|---|---|---|
| `DAT_10092788` | Colored Selection Circles | 0 | on/off gate | MarkerTintPC |
| `DAT_100927b4` | Colored Portrait Frames | 0 | on/off gate | MarkerTintPC, RenderPortraitRect |
| `DAT_1009277c` | Selection Circle Color Slot | 1 | unsigned > 6 -> 1 | FUN_1002d1e0 |
| `DAT_10092774` | Selection Circle Min Brightness | 110 | [0, 255], then max(stack floor) | FUN_1002d1e0 |
| `DAT_10092760` | Selection Circle Thickness | 2 | < 1 -> auto (4, or 6 when screen width > 1920); > 6 -> 6 | FUN_1002cfa0 |
| `DAT_10092770` | Destination Marker Thickness | 2 | < 1 -> FUN_1002cfa0(); > 6 -> 6 | `CVidMode_DrawRecticle3dOverride` (zoom-group note) |
| `DAT_100927b0` | Portrait Frame Thickness | 1 | clamped into [1, 4] | RenderPortraitRect |

`FUN_1002cfa0`'s auto values are half-pixels (the patch comment says
"half map pixels"): 4 = 2 px, 6 = 3 px above a 1920-wide screen (the
width comes from the engine pointer `DAT_10094318`, one of the
runtime-registered BSS slots below). The patch comment lists
"Selection Circle Thickness" / "Portrait Frame Thickness" as options
rows and "Destination Marker Thickness", the colour slot and the
brightness floor as ini-only keys picked up on the next panel
open/close.

#### The tint pipeline (inner FUN_1002d1e0 -> FUN_1002d080)

`FUN_1002d1e0(ECX = sprite, EDX = green COLORREF, stack = extra
min-brightness floor)`:

1. green byte = `(EDX >> 8) & 0xff`; 0 -> return EDX unchanged (a
   black or non-green base is never touched); clamp green to 250.
2. slot = the "Selection Circle Color Slot" cache; idx = byte at
   `sprite + 0x5c8 + slot` -- the creature's color-index byte for
   that slot (the patch comment names them: 0 metal, 1 minor cloth
   (default), 2 major, 3 skin, ...); idx clamped to 114.
3. base = `*(uint *)(0x0085e8d8 + idx * 4)` -- an **engine static
   color table** (the displacement sits below the DLL's image base,
   like the tile-atlas group's engine global `_DAT_008cf6d8`). Its
   contents were not decoded in this stream (engine binary out of
   scope); the patch comment identifies it as the creature
   color-slot palette.
4. lift = `FUN_1002d080(ECX = base, EDX = min)`: integer Rec.601
   luma `(299 R + 587 G + 114 B) / 1000`; when a floor is set and
   `0 < luma < min`, scale every channel by `min / luma` (each
   clamped to 255), recompute the luma, and if still short blend
   each channel toward 255 by `t = (min - luma) * 255 /
   (255 - luma)`. Otherwise the base color passes through
   unchanged.
5. Result = per-channel `lift * green / 250` (imul-high magic
   `0x10624dd3` + shift) -- the palette color at the engine's own
   green intensity -- returned as a COLORREF.

`RenderPortraitRect` (hud-group note) reuses steps 1-5 verbatim
with its own inputs: floors of `G * 587 / 1000` (derived from a
green channel, same /1000 magic) or literal 146, and base colors of
the engine call 0x705cc0's result or pure green `0xfa00`.

#### Threads and synchronization

All of it runs on engine threads: MarkerTintPC on the marker draw
path (the patch comment stresses the hook is deliberately NOT
renderer-gated -- a color decision costs nothing under the software
renderer either), the thickness helpers inside their render hooks,
SetMarkerStyle from the Lua GUI handler on the engine/GUI thread.
The pair table and the seven caches have no locks or atomics
anywhere -- the fork relies on single-threaded drawing, the same
stance as every other registry in this DLL.

#### Failure model and engine-address attribution

Every path degrades to vanilla: INI gates default to 0 -> the
engine's own green stands; null args -> no-op; unregistered sprite
on the deregister path -> immediate return; green byte 0 -> the
color comes back unchanged; slot > 6 and idx > 114 clamp. No
allocation, no exceptions, no GL in this group. Hook site 0x766C06,
engine call 0x705cc0 and the color-slot byte offset +0x5c8 are
engine facts attributed to the staged patch comments and this DLL's
disassembly -- never verified against the engine binary in this
stream. The `DAT_10094xxx` engine pointers are runtime-registered
BSS slots filled through the loader's public address-list mechanism
(Bubb13/IEexLoader, IEexDLL/IEexLua.asm: IEex_Init /
IEex_AddressList).

`IEex_Helper_SetMarkerStyle` - 0x1002e1b0 (FUN_1002e1b0) - no
arguments, returns 0; entered from IEex_Gui_State.lua (the IEex
Options OK/Apply handler, line 3910, immediately after
`IEex_WriteOptions()`).

##### Behavior

A full reset of the marker subsystem's caches -- despite the name
it sets no style; it invalidates everything so the next marker or
portrait draw re-reads the ini:

1. Seven lazy caches <- `0xffffffff` (the "not read yet" sentinel):
   `DAT_10092770`, `DAT_100927b0`, `DAT_100927b4`, `DAT_10092774`,
   `DAT_1009277c`, `DAT_10092760`, `DAT_10092788` (the COMMON
   table's first column, all of them).
2. Zeroes the 32-entry pair table (key and color dwords, walked to
   `0x104df364`) -- every sprite forgets its registered green.
3. `DAT_10094507 = 1` -- a one-shot flag consumed by HudLayerRender
   (hud-group note): on its next pass it clears the flag, zeroes
   `DAT_104df060` and runs the full content restore it logs as "HUD
   Layer: focus regain -> content restore (clear + full
   invalidate)".
4. Returns 0.

Called from exactly one place: after the OK button rewrote
Icewind2.ini via `IEex_WriteOptions()`. The patch comment gives the
intent -- "Marker colour/thickness live-apply: the helper caches
its [IEex Options] keys on first use, so drop the cache now that
the ini has been rewritten. Also picks up the ini-only marker keys
(thickness, portrait rings, colour slot, brightness floor) on the
next open/close of this panel."

##### Failure fallbacks

None needed: no gates, no allocation, no table-size assumptions
beyond its own constant walk; safe to call at any time (worst case
one extra ini read per key on the next draw plus one HUD
invalidate).

##### Decompiler artifact

None significant; the function is small and clean. Note only that
the pair-table walk writes each entry as `puVar1[-1] = 0; *puVar1 =
0` starting from `&DAT_104df264` -- key dword first, then color.

##### Lua wiring

`if IEex_Helper_SetMarkerStyle then IEex_Helper_SetMarkerStyle()
end` in the OK handler between `IEex_WriteOptions()` and
`closeIEexOptions()`; the next statement does the analogous cache
drop for floating text (`IEex_Helper_ReloadFloatTextScale`).

