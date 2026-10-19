# IEex_Helper_MarkerTintPC

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002e0a0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002e0a0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Markers_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1002e0a0(int param_1,int param_2)

{
  uint uVar1;
  int iVar2;
  undefined4 uVar3;
  uint uVar4;

  if ((param_1 != 0) && (param_2 != 0)) {
    if (DAT_10092788 == 0xffffffff) {
      DAT_10092788 = GetPrivateProfileIntA
                               ("IEex Options","Colored Selection Circles",0,".\\Icewind2.ini");
    }
    if (DAT_10092788 != 0) {
      uVar1 = *(uint *)(param_1 + 6);
      if (((uVar1 & 0xff00ff) == 0) && ((uVar1 & 0xff00) != 0)) {
        if (DAT_100927b4 == 0xffffffff) {
          DAT_100927b4 = GetPrivateProfileIntA
                                   ("IEex Options","Colored Portrait Frames",0,".\\Icewind2.ini");
        }
        iVar2 = 0;
        if (DAT_100927b4 == 0) {
          do {
            if ((&DAT_104df260)[iVar2 * 2] == param_2) {
              (&DAT_104df264)[iVar2 * 2] = uVar1;
              goto LAB_1002e166;
            }
            iVar2 = iVar2 + 1;
          } while (iVar2 < 0x20);
          uVar4 = DAT_104df5dc & 0x1f;
          DAT_104df5dc = DAT_104df5dc + 1;
          (&DAT_104df260)[uVar4 * 2] = param_2;
          (&DAT_104df264)[uVar4 * 2] = uVar1;
        }
LAB_1002e166:
        uVar3 = FUN_1002d1e0(0);
        *(undefined4 *)(param_1 + 6) = uVar3;
        return;
      }
      iVar2 = 0;
      while ((&DAT_104df260)[iVar2 * 2] != param_2) {
        iVar2 = iVar2 + 1;
        if (0x1f < iVar2) {
          return;
        }
      }
      (&DAT_104df260)[iVar2 * 2] = 0;
    }
  }
  return;
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

`RenderPortraitRect` (gui-misc-group note) reuses steps 1-5 verbatim
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

`IEex_Helper_MarkerTintPC` - 0x1002e0a0 (FUN_1002e0a0) - 2 stack
args, callee does not clean (the hook stub balances the stack);
entered from the IEex_Markers_Patch.lua inline hook at engine
0x766C06 (all registers preserved; the stub's last two pushes make
param_1 = ESI, param_2 = EDI).

##### Behavior

Recolors one PC marker in place on the engine's marker draw path:
param_1 is the engine's marker/draw structure whose `+6` holds the
COLORREF about to be consumed (an unaligned dword), param_2 the
sprite object the marker belongs to.

1. Gates `param_1 != 0 && param_2 != 0`; lazy-reads the
   "Colored Selection Circles" cache (`DAT_10092788`, default 0)
   and returns when it is 0 -- the engine's green passes through
   untouched.
2. Reads `color = *(uint *)(param_1 + 6)`.
3. Pure green (`(color & 0xff00ff) == 0 && (color & 0xff00) != 0`):
   lazy-reads "Colored Portrait Frames" (`DAT_100927b4`, default
   0). When that is OFF, maintains the pair table -- search for
   `key == param_2` and update the stored green, else insert
   `(param_2, color)` at round-robin slot `DAT_104df5dc++ & 0x1f`.
   (When portrait frames are ON the table is skipped:
   RenderPortraitRect then recomputes through FUN_1002d1e0 live and
   needs no remembered green.) Then replaces the marker color:
   `*(param_1 + 6) = FUN_1002d1e0(ECX = param_2, EDX = color,
   stack floor = 0)` -- the shared tint pipeline of the COMMON
   section, so the floor is exactly the "Min Brightness" ini value.
4. Any other color: search the table for `param_2` and clear that
   entry's key to 0 -- deregistration, so the portrait-frame code
   stops treating the sprite as green-circled. The engine's color
   is left exactly as it was (no tint on non-green markers).

Net effect per frame: each PC's marker becomes the configured
palette color (creature color slot, brightness-floored) at the
original green's intensity, and the fork remembers which sprites
are currently green-circled for the portrait-frame code.

##### Failure fallbacks

Null args -> no-op. Feature INI gates default to 0 -> vanilla
colors and no table traffic. Table full -> oldest entry overwritten
round-robin (the counter never pauses). Non-green color for an
unregistered sprite -> immediate return. Green byte 0 inside the
tint pipeline -> the input color returns unchanged. No allocation,
no exceptions, no GL.

##### Decompiler artifact

The decompile shows `uVar3 = FUN_1002d1e0(0)` -- one stack
argument. The register pair is real: objdump at raw 0x2d566 shows
`push $0x0 / mov %edi,%edx / mov %esi,%ecx / call 0x2c5e0` with
ESI = param_2 and EDI = the green (COMMON pipeline). The
`param_1 + 6` COLORREF is genuinely unaligned (x86 unaligned dword
read/write, no struct packedness issue at machine level).

##### Lua wiring

`IEex_HookRestore(0x766C06, 0, 6, [[ !push_all_registers_iwd2
!push_edi !push_esi !call >IEex_Helper_MarkerTintPC
!pop_all_registers_iwd2 ]])` -- the loader's public hook primitive
(IEexDLL/IEexLua.asm; engine code patching, not fork internals).
The patch comment attributes 0x766C06 to the marker color decision
site, notes the 6 original bytes are restored around it, and
stresses the hook is deliberately NOT renderer-gated: the tint
applies under the software renderer too; only stroke thickness is
OpenGL-only (the DrawEllipse3d / DrawRecticle3d overrides in the
zoom-group note).

