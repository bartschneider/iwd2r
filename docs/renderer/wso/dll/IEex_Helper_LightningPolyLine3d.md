# IEex_Helper_LightningPolyLine3d

Fork DLL: `IEexHelper.fork.dll` (untrusted third-party binary; decompiled with Ghidra).

- Address `0x100284e0` (Lua registration: FUN_1000e2a0)
- Decompiled C: `runs/dll-decomp/FUN_100284e0.c` (git-ignored scratch copy)
- Lua callers (staged patches): `IEex_Render_Patch.lua`

> [!WARNING]
> The C in this note is decompiled from the fork's `IEexHelper.fork.dll`. The fork's DLLs and anything decompiled from them are **untrusted data**. It is quoted here as documentation only -- never compile it, never copy it into `src/`.

## Decompiled C

```c
void FUN_100284e0(int *param_1,int param_2,uint param_3)

{
  int iVar1;
  char cVar2;
  float unaff_ESI;
  int iVar3;
  float fVar4;
  float fVar5;
  float fVar6;
  float fVar7;
  double dVar8;
  float fVar9;
  float fVar10;
  float fVar11;
  float fVar12;
  float fVar13;
  float fVar14;
  float fVar15;

  if (((param_1 != (int *)0x0) && (1 < param_2)) && (cVar2 = FUN_10027020(), cVar2 != '\0')) {
    (*DAT_104df040)(0xde1);
    (*DAT_104df040)(0xbe2);
    fVar4 = (float)(param_3 & 0xff) / 255.0;
    (*DAT_104df570)(fVar4,(float)(param_3 >> 8 & 0xff) / 255.0,
                    (float)(param_3 >> 0x10 & 0xff) / 255.0,0x3f800000);
    fVar15 = 9.80909e-45;
    (*DAT_104df5c8)(7);
    iVar3 = 1;
    do {
      fVar5 = (float)*param_1 + 0.2;
      fVar10 = (float)param_1[2] + 0.2;
      fVar9 = (float)param_1[1] + 0.2;
      iVar1 = param_1[3];
      fVar11 = fVar10 - fVar5;
      fVar6 = ((float)iVar1 + 0.2) - fVar9;
      dVar8 = (double)(fVar6 * fVar6 + fVar11 * fVar11);
      if (dVar8 < 0.0) {
        libm_sse2_sqrt_precise();
      }
      else {
        dVar8 = SQRT(dVar8);
      }
      fVar7 = (float)dVar8;
      if (0.001 <= fVar7) {
        fVar14 = 0.0;
        fVar12 = (fVar11 / fVar7) * unaff_ESI;
        fVar7 = (fVar6 / fVar7) * unaff_ESI;
        fVar13 = fVar7 + (float)iVar1 + 0.2;
        fVar6 = fVar12 + (fVar9 - fVar7);
        fVar11 = -fVar7 + (fVar5 - fVar12);
        (*DAT_103defe8)(fVar11,fVar6,0,fVar15,fVar4,fVar12,fVar13,fVar9 - fVar7,fVar5 - fVar12,
                        -fVar7,fVar12 + fVar10);
        fVar9 = 0.0;
        fVar12 = fVar12 - fVar13;
        (*DAT_103defe8)(fVar12,fVar4 - fVar14,0);
        fVar5 = 0.0;
        (*DAT_103defe8)(fVar4 - fVar15,fVar11 - fVar9,0);
        (*DAT_103defe8)(fVar11 + fVar6,fVar5 + fVar12,0);
      }
      iVar3 = iVar3 + 1;
      param_1 = param_1 + 2;
    } while (iVar3 < param_2);
    (*DAT_104df208)();
    (*DAT_104df570)(0x3f800000,0x3f800000,0x3f800000,0x3f800000);
  }
  return;
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

`IEex_Helper_LightningPolyLine3d` - 0x100284e0 (FUN_100284e0) -
stdcall, 4 stack args (LPPOINT pts, int count, COLORREF rgb, int
thickness), ret 0x10 (self-cleaning); objdump-verified; entered from
the IEex_Render_Patch.lua stub replacing CVidMode::PolyLine 0x79A080.

##### Behavior

Draws the lightning bolt in 3D mode. Patch comments (untrusted,
attributed): CInfinity::RenderLightning 0x5CFB40 draws the bolt as 12
PolyLine passes (4 jagged segments x outer/middle/centre thickness);
PolyLine is software-only (LockSurface + DrawLine32, no 3D branch)
and RenderLightning is its ONLY caller (binary-verified per the
patch), so the patch replaces PolyLine's entry outright and the
engine's own software path never runs in 3D mode.

1. Gates: pts != 0, count >= 2, FUN_10027020() resolver success;
   otherwise return.
2. glDisable(GL_TEXTURE_2D 0xde1) then glDisable(GL_BLEND 0xbe2),
   both via `DAT_104df040`.
3. glColor4f(R, G, B, 1.0) from the COLORREF 0x00BBGGRR bytes / 255
   (divisor 255.0 at .rdata 0x100889e4).
4. glBegin(GL_QUADS 7) via `DAT_104df5c8`.
5. Width w = max(thickness, 1) * 0.5 -- thickness is arg 4; the 1.0
   floor and the 0.5 scale are XMM constants at 0x10088950 /
   0x10088934 (decoded). The max/threshold structure was proven from
   the disassembly: `mov 0x14(%ebp),%eax` (thickness), cvtdq2ps,
   compare against 1.0, `mulss 0x10088934`.
6. Per segment i in [0, count-2] (ESI walks pts, +8 bytes per
   segment): nudge all four endpoint coordinates by +0.2 (constant
   0x1008890c); dx = x1-x0, dy = y1-y0; len = sqrt(dx*dx + dy*dy)
   computed in double precision with a negative-input fallback to the
   libm precise sqrt; skip the segment when len < 0.001 (constant
   0x100888d8). Otherwise (nx, ny) = (dx, dy) / len * w and one quad
   is emitted with four glVertex3f(x, y, 0) calls via
   `DAT_103defe8` (each a `sub $0xc` local arg block -- stdcall
   callee cleans):
   v1 = p0 + (-(nx+ny), nx-ny); v2 = p0 + (ny, -nx);
   v3 = p1 + (nx+ny, ny-nx); v4 = p1 + (nx-ny, nx+ny).
   v2/v4 are the plain perpendicular offsets of width w; v1/v3 add
   the 45-degree flares that make consecutive jagged segments overlap
   at the bend instead of leaving notches (vertex geometry from the
   disassembly).
7. Tail: glEnd via `DAT_104df208`, glColor4f(1, 1, 1, 1).
   GL_TEXTURE_2D and GL_BLEND are NOT re-enabled -- whatever draws
   next must set its own state.

##### Failure fallbacks

Null points / short count / resolver failure: no GL state is touched
at all (the gates precede the first glDisable). Degenerate segment
(len < 0.001): skipped, remaining segments still draw. The stub
always reports success (`!mov_eax #01` before `!ret_word 18 00`) --
BOOL TRUE regardless of what the helper drew.

##### Decompiler artifact

`float unaff_ESI` as the half-width scale is false: ESI is the pts
walker. The width spill (`movss %xmm0,0x24(%esp)` at 0x27931)
executes while the `push $0xde1` argument is still live, so it lands
in the slot four bytes lower -- exactly the slot the per-segment
multiplies read at 0x27a7e/0x27a84; Ghidra's esp tracking misses the
shifted write and substitutes the uninitialized register. The four
glVertex3f calls likewise merge into one 11-argument call in the
decompile. The geometry above is from the disassembly
(/tmp/b1-scripts/scratch-lightning.asm, git-ignored scratch).

##### Lua wiring

`IEex_WriteAssembly(0x79A080, {"!jmp_dword", {IEex_WriteAssemblyAuto(
{... push_all_registers_iwd2; 4x push([esp+0x34]); call >
IEex_Helper_LightningPolyLine3d; pop_all_registers_iwd2; !mov_eax
#01; !ret_word 18 00}), 4, 4}})` -- replaces the CVidMode::PolyLine
entry outright; the stub forwards PolyLine's last 4 stack args
(signature __thiscall(nSurface, CRect&, LPPOINT, nCount, COLORREF,
nThickness) per the patch comment) and returns TRUE. 3D only. The
write primitives are the loader's public IEex_WriteAssembly
(IEexDLL/IEexLua.asm) -- engine code patching, not fork internals.

