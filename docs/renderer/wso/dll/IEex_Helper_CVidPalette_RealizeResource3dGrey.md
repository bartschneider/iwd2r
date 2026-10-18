# IEex_Helper_CVidPalette_RealizeResource3dGrey

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x1002b0e0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_1002b0e0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Render_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_1002b0e0(undefined4 param_1,uint param_2,undefined4 param_3,undefined4 param_4)

{
  undefined4 *puVar1;
  char *pcVar2;
  int iVar3;
  uint uVar4;
  char cVar5;
  char cVar6;
  char cVar7;
  int in_ECX;
  int iVar8;
  char cVar9;
  char cVar10;
  char *pcVar11;
  undefined4 *puVar12;
  bool bVar13;

  uVar4 = param_2;
  pcVar2 = *(char **)(in_ECX + 8);
  if (((param_2 & 0x2080000) == 0) || (pcVar2 == (char *)0x0)) {
    (*DAT_100942c0)(param_1,param_2,param_3,param_4);
    return;
  }
  iVar3 = *(int *)((int)ThreadLocalStoragePointer + _tls_index * 4);
  puVar1 = (undefined4 *)(iVar3 + 0xe0);
  pcVar11 = pcVar2;
  puVar12 = puVar1;
  for (iVar8 = 0x100; iVar8 != 0; iVar8 = iVar8 + -1) {
    *puVar12 = *(undefined4 *)pcVar11;
    pcVar11 = pcVar11 + 4;
    puVar12 = puVar12 + 1;
  }
  if ((param_2 & 0x400000) == 0) {
    if ((param_2 & 0x800000) != 0) {
      iVar8 = 0;
      goto LAB_1002b182;
    }
    if ((((pcVar2[4] == '\0') && (pcVar2[5] == '\0')) && (pcVar2[6] == '\0')) &&
       (iVar8 = 2, pcVar2[7] == '\0')) goto LAB_1002b182;
  }
  else if (((*pcVar2 != '\0') || (pcVar2[1] != -1)) || ((pcVar2[2] != '\0' || (pcVar2[3] != '\0'))))
  {
    iVar8 = 0;
    goto LAB_1002b182;
  }
  iVar8 = 1;
LAB_1002b182:
  do {
    param_2 = *(uint *)(pcVar2 + iVar8 * 4);
    cVar5 = (char)param_2;
    cVar10 = (char)(param_2 >> 8);
    cVar6 = (char)(param_2 >> 0x10);
    cVar7 = (char)(param_2 >> 0x18);
    if (iVar8 == 0) {
      if (cVar5 != '\0') goto LAB_1002b1bf;
      bVar13 = cVar10 == -1;
LAB_1002b1a7:
      if (((!bVar13) || (cVar6 != '\0')) || (cVar7 != '\0')) goto LAB_1002b1bf;
    }
    else {
      if ((iVar8 == 1) && (cVar5 == '\0')) {
        bVar13 = cVar10 == '\0';
        goto LAB_1002b1a7;
      }
LAB_1002b1bf:
      cVar9 = (char)((param_2 >> 0x10 & 0xff) + (param_2 >> 8 & 0xff) + (param_2 & 0xff) >> 2);
      if ((uVar4 & 0x80000) != 0) {
        param_2 = CONCAT13(cVar7,CONCAT12(cVar9,CONCAT11(cVar9,cVar9)));
        cVar5 = cVar9;
        cVar10 = cVar9;
        cVar6 = cVar9;
      }
      if ((uVar4 & 0x2000000) != 0) {
        cVar6 = cVar9 + '\x15';
        param_2 = CONCAT31(CONCAT21(CONCAT11(param_2._3_1_,cVar6),cVar9),cVar9);
        cVar5 = cVar9;
        cVar10 = cVar9;
      }
      if (((iVar8 != 1) || (cVar5 != '\0')) ||
         ((cVar10 != '\0' || ((cVar6 != '\0' || (param_2._3_1_ != '\0')))))) {
        *(uint *)(iVar3 + 0xe0 + iVar8 * 4) = param_2;
      }
    }
    iVar8 = iVar8 + 1;
    if (0xff < iVar8) {
      *(undefined4 **)(in_ECX + 8) = puVar1;
      (*DAT_100942c0)(param_1,uVar4,param_3,param_4);
      *(char **)(in_ECX + 8) = pcVar2;
      return;
    }
  } while( true );
}
```

## Notes

### Shared 3D-overlay machinery (all helpers in this group)

The staged `IEex_Render_Patch.lua` restores four engine visuals that
the software renderer draws but the 3D/GL path lost: the sprite
highlight fill, the floating-text readability panel, the lightning
bolt, and the greyed-out palette drop. The pattern is always the same:
a Lua stub forwards the engine call site's own stack arguments to an
IEex_Helper export, which either draws in immediate-mode GL or edits
the engine's staging data, and the engine's original code then runs
(or has already run) unchanged. The patch installs nothing when
`[Program Options] "3D Acceleration"` in `.\Icewind2.ini` is 0
(software renderer).

#### GL entry points (16-proc resolver, inner FUN_10027020)

All GL calls in this group go through the resolver FUN_10027020:
GetModuleHandleA("opengl32.dll") + 16 GetProcAddress calls; the gate
returns success only when all sixteen slots are non-null (its result
is checked at every helper's top). Slot table:

| Slot | Function |
|---|---|
| DAT_104df5a4 | glEnable |
| DAT_104df040 | glDisable |
| DAT_103ded84 | glBlendFunc |
| DAT_104df0e8 | glBindTexture |
| DAT_104df5fc | glTexImage2D |
| DAT_104df064 | glTexSubImage2D |
| DAT_103dec00 | glTexParameteri |
| DAT_104df0cc | glTexParameterfv |
| DAT_104df0c8 | glTexEnvf |
| DAT_104df570 | glColor4f |
| DAT_104df5c8 | glBegin |
| DAT_104df208 | glEnd |
| DAT_104df018 | glTexCoord2f |
| DAT_103defe8 | glVertex3f |
| DAT_103defd4 | glGenTextures |
| DAT_104df004 | glIsTexture |

These addresses are the resolver's own table and intentionally do not
match the six-slot (ui-scale group) or 17-slot (tile-atlas group)
tables -- each fork subsystem resolves its own.

#### Engine addresses and the registered original

`DAT_100942c0` holds the engine's original
`CVidPalette::RealizeResource3d` (0x7D6240 per the patch comments). It
is registered at runtime by the Lua patch itself:
`IEex_Helper_DefineAddress("CVidPalette::RealizeResource3dOriginal",
IEex_Label(...))` after the
`IEex_HookReplaceFunctionMaintainOriginal` call -- this is the
loader's public address-list mechanism (Bubb13/IEexLoader,
IEexDLL/IEexLua.asm: IEex_Init / IEex_AddressList; the fork reads
registered engine addresses out of its BSS slots at runtime). All
other engine addresses named below (hook sites, OutlinePoly, PolyLine,
RenderLightning, CVideo3d::texImageData) come from the staged patch's
comments -- untrusted, attributed, never verified against the engine
binary in this stream.

#### Threads and synchronization

Everything in this group runs on the engine's render thread inside the
frame (hook sites in the render call stack). No locks, no atomics, no
worker threads. RealizeResource3dGrey's palette scratch lives in TLS,
so even re-entrant realizes cannot collide.

#### Failure model

Every helper degrades silently: gate failure (missing GL procs, null
pointer, degenerate input) means "do nothing" and the engine's own
code path continues. The Lua stubs do not check results either -- the
helpers are fire-and-forget visual additions on top of an engine that
already drew (or will draw) its own version of the effect.

`IEex_Helper_CVidPalette_RealizeResource3dGrey` - 0x1002b0e0
(FUN_1002b0e0) - forwarded __thiscall (ECX = CVidPalette* this), 4
stack args (the original RealizeResource3d's arguments, forwarded
verbatim); entered from the IEex_Render_Patch.lua replacement hook at
CVidPalette::RealizeResource3d 0x7D6240.

##### Behavior

Restores the greyed-out (disabled) palette drop that the 3D texture
path was missing: buttons/icons realized while a grey flag is set
come out desaturated (and optionally blue-tinted) instead of full
colour.

1. Palette data = `*(char **)(this + 8)`, 256 RGBA8 entries. Gate:
   when `(flags & 0x2080000) == 0` (neither the desaturate 0x80000
   nor the tint 0x2000000 bit) or the palette pointer is null,
   tail-call `DAT_100942c0(param_1..4)` -- the registered original
   `CVidPalette::RealizeResource3d` (see COMMON for the registration)
   -- and return; the stock 3D path runs unchanged.
2. Copy all 256 entries to TLS scratch at (TLS base + 0xe0)
   (via `_tls_index`), leaving `this + 8` untouched for now.
3. Start index policy: flag 0x400000 set -> start 0 unless entry 0 is
   the shadow key (R=0, G=0xFF, B=0, A=0), then 1; flag 0x800000 set
   -> start 0; neither -> start 2 if entry 0 is transparent black
   (0,0,0,0), else 1. Inside the loop indexes 0 and 1 are skipped
   when they hold those key values (they stay as the engine wrote
   them).
4. Grey pass over indexes start..255: grey = (R + G + B) >> 2 (max
   191). Flag 0x80000: R = G = B = grey. Flag 0x2000000: R = G =
   grey, B = grey + 0x15 (+21 -- the Lua patch calls this bit the
   "sepia twin"; the fork's 3D version shifts blue rather than
   multiplying). Entries that would grey to fully transparent black
   are left as-is.
5. Swap `this + 8` to the TLS copy, tail-call `DAT_100942c0` so the
   original realizes the modified palette through the normal 3D
   texture path, restore `this + 8`, return (the original's return
   value is the helper's).

The net effect on the flag vocabulary (patch comments, attributed):
0x80000 = desaturate, 0x2000000 = sepia twin, 0xA0000 = both,
0x20000 = tint multiply, 0x400000/0x800000 = shadow/transparency
start-index selectors. Only 0x80000, 0x2000000 and the two selectors
are implemented in this helper; 0x20000's multiply is not present
here (the software palette code at 0x7C04B0/0x7BF870 handles it).

##### Failure fallbacks

No grey flags set or null palette: original path, byte-identical
behaviour. The swap covers only the registered call, and the TLS
scratch is per-thread, so re-entrant or parallel realizes cannot see
a half-greyed palette.

##### Decompiler artifact

The CONCAT13/CONCAT12 byte-shuffle expressions are the compiler's
in-place dword edits of one RGBA entry -- read them as byte writes
R/G/B/A. `pcVar2[4]`-style byte probes are entry 1 (offset 4 bytes).

##### Lua wiring

`IEex_HookReplaceFunctionMaintainOriginal(0x7D6240, 6,
"CVidPalette::RealizeResource3dOriginal",
[[!jmp_dword >IEex_Helper_CVidPalette_RealizeResource3dGrey]])` --
displaced 6 bytes `55 8B EC 83 EC 58`, resume 0x7D6246 ("verified
with objdump against the live exe" per the patch comment), followed
by `IEex_Helper_DefineAddress("CVidPalette::RealizeResource3dOriginal",
IEex_Label(...))`, which is how `DAT_100942c0` receives the original
engine function (loader address-list mechanism, see COMMON).
Patch-comment callers of the realized flags: CInfButtonArray::
RenderButton 0x5950F0, CIcon::RenderIcon (flag 2), CIcon::
RenderButtonOverlay 0x5957C0, weapon-set ring 0x5951A5,
CUIControlButton 0x80000/0x80001, CInfCursor::SetGreyScale 0x597610.
Note the software grey math (v = (R+G+B) >> 2 then * 180 >> 8 =
0.527*L) differs from this GL version's plain >> 2 (0.333*L): the
fork did not match the software curve exactly.

