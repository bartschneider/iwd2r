---
address: 0x7CECD0
name: CVidCell::Blt8To24
oracle_case: vid/cell_blt8to24_fast
kind: read-only
ghidra_version: 12.1.4
parity_waiver: "lints 2026-10-29 blt8to24: the only finding in the function's span is RE004 on its own address-derived name (the definition line every stub carries): the export has no name for 0x7CECD0, so the port keeps the stub's and a real rename (report-T50.md proposes Blt8To24) needs a Ghidra-side rename of the project, a follow-up that also removes the base's RE005 this port already did. The base stub was red for both RE004 and RE005, so the mechanical `red reproduces at base` comparison cannot match the now-smaller finding set. Nothing else in the span trips a lint; asm is green (the four callees are named in src)."
---

# 0x7CECD0 CVidCell::Blt8To24

## Decompiled (Ghidra)

```c
undefined8 CVidCell::sub_7CECD0(undefined2 *param_1,int param_2,undefined4 param_3)

{
  byte bVar1;
  int iVar2;
  byte *pbVar3;
  uint uVar4;
  int in_ECX;
  uint uVar5;
  uint in_EDX;
  uint uVar6;
  uint uVar7;
  bool bVar8;
  undefined8 uVar9;
  uint local_8;
  
  local_8 = (uint)(*(ushort **)(in_ECX + 0xd0))[1];
  uVar7 = (uint)**(ushort **)(in_ECX + 0xd0);
  if ((local_8 != 0) && (uVar7 != 0)) {
    if (*(int *)(in_ECX + 0xcc) == 0) {
      CVidPalette::SetPalette(*(undefined4 *)(*(int *)(in_ECX + 0xa8) + 0x6a),0x100,0);
    }
    CVidPalette::Realize(&DAT_009064f8,0x18,param_3,in_ECX + 0x2c,0xff);
    if (*(char *)(in_ECX + 0xd4) == '\0') {
      if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
        iVar2 = 0;
      }
      else {
        iVar2 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
      }
      DAT_009064fc = *(undefined4 *)(iVar2 + 0x24);
    }
    pbVar3 = (byte *)CResCell::GetFrameData
                               (*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    iVar2 = *(int *)(in_ECX + 0xa8);
    if (*(int *)(iVar2 + 0x7e) == 0) {
      iVar2 = *(int *)(iVar2 + 0x54);
    }
    else {
      iVar2 = *(int *)(iVar2 + 0x58);
    }
    bVar1 = *(byte *)(iVar2 + 0xb);
    param_2 = param_2 + uVar7 * -3;
    uVar9 = CResCell::GetCompressed(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    in_EDX = (uint)((ulonglong)uVar9 >> 0x20);
    uVar5 = uVar7;
    if ((int)uVar9 != 0) {
      uVar6 = 0;
      do {
        while (*pbVar3 != bVar1) {
          uVar4 = (&DAT_009064f8)[*pbVar3];
          pbVar3 = pbVar3 + 1;
          *param_1 = (short)uVar4;
          uVar6 = uVar4 >> 0x10;
          *(char *)(param_1 + 1) = (char)(uVar4 >> 0x10);
          param_1 = (undefined2 *)((int)param_1 + 3);
          uVar5 = uVar5 - 1;
          if (uVar5 == 0) {
            param_1 = (undefined2 *)((int)param_1 + param_2);
            local_8 = local_8 - 1;
            uVar5 = uVar7;
            if (local_8 == 0) {
LAB_007cee51:
              return CONCAT44(uVar6,1);
            }
          }
        }
        uVar4 = (uint)pbVar3[1];
        pbVar3 = pbVar3 + 2;
        param_1 = (undefined2 *)((int)param_1 + uVar4 * 3 + 3);
        uVar5 = uVar5 - 1;
        if ((uVar5 == 0) || (bVar8 = uVar5 <= uVar4, uVar4 = uVar4 - uVar5, uVar5 = local_8, bVar8))
        {
          do {
            param_1 = (undefined2 *)((int)param_1 + param_2);
            if (local_8 - 1 == 0) goto LAB_007cee51;
            bVar8 = uVar4 < uVar7;
            uVar4 = uVar4 - uVar7;
            uVar5 = local_8 - 1;
            if (bVar8) break;
            param_1 = (undefined2 *)((int)param_1 + param_2);
            if (local_8 - 2 == 0) goto LAB_007cee51;
            bVar8 = uVar4 < uVar7;
            uVar4 = uVar4 - uVar7;
            uVar5 = local_8 - 2;
            if (bVar8) break;
            param_1 = (undefined2 *)((int)param_1 + param_2);
            local_8 = local_8 - 3;
            if (local_8 == 0) goto LAB_007cee51;
            bVar8 = uVar7 <= uVar4;
            uVar4 = uVar4 - uVar7;
            uVar5 = local_8;
          } while (bVar8);
        }
        local_8 = uVar5;
        uVar5 = -uVar4;
      } while( true );
    }
    do {
      do {
        if (*pbVar3 != bVar1) {
          uVar6 = (&DAT_009064f8)[*pbVar3];
          *param_1 = (short)uVar6;
          in_EDX = uVar6 >> 0x10;
          *(char *)(param_1 + 1) = (char)(uVar6 >> 0x10);
        }
        pbVar3 = pbVar3 + 1;
        param_1 = (undefined2 *)((int)param_1 + 3);
        uVar5 = uVar5 - 1;
      } while (uVar5 != 0);
      param_1 = (undefined2 *)((int)param_1 + param_2);
      local_8 = local_8 - 1;
      uVar5 = uVar7;
    } while (local_8 != 0);
  }
  return CONCAT44(in_EDX,1);
}
```

## Callers

- 0x7AEAD0 CVidCell::Render (call at 0x7AEDD4)
- 0x7AE5D0 CVidCell::Render (call at 0x7AEA72)

## Callees

- 0x77F5C0 CResCell::GetCompressed
- 0x77F5F0 CResCell::GetFrameData
- 0x7BF150 CVidPalette::Realize
- 0x7BF390 CVidPalette::SetPalette

## Notes

The plain 24-bit cell blit, called by both `CVidCell::Render` overloads for the
`else` column of the dispatcher (no translucent, brightest or clear-shadow flag)
at 24 bpp. Ghidra has no name for it, hence `sub_7CECD0` in the listing above;
the survey (report-T50.md) names it `Blt8To24` after its role in the
dispatcher, but the export calls it `sub_7CECD0` and the code keeps that name:
renaming it rewrites the two `CVidCell::Render` call sites, which makes both
Render overloads touched functions whose pre-existing `sub_` placeholders turn
the gate's lints red and whose asm row then misses the old name. The 6-argument
`CVidCell::Blt8To24` at 0x7CEEB0 (already in src) is the clipped overload,
exactly as `Blt8To32` has a fast and a clipped form. `ret 0xc`: `(BYTE* pSurface, LONG lPitch, DWORD dwFlags)`.

The listing is the fast `Blt8To32` (0x7CFBC0) with the 24-bit differences:

- the palette is realized for 24 bits with a fixed 255 (`Realize(&rgbTempPal,
  0x18, dwFlags, this+0x2C, 0xFF)`; the plain form has no `nTransVal`
  parameter, unlike the 32-bit one);
- `!m_bShadowOn` writes `rgbTempPal[SHADOW_ENTRY]` (index 1, the byte at
  0x85E854) from the mode's `field_24`, the same as the 32-bit sibling;
- there is **no** `g_pChitin->field_174` block (the 32-bit twin's "brighten
  entries 2..255 at frame 0"); the bytes at 0x7CED46-0x7CED79 go straight from
  the shadow write to `GetFrameData`;
- each pixel writes three bytes: `*(WORD*)p = low word of rgbTempPal[i]`,
  `p[2] = rgbTempPal[i] >> 16` (B, G, R - the same write the clipped
  `Blt8To24` makes);
- the pitch stays in bytes: the original computes `lPitch - width * 3` once
  (0x7CEDAD-0x7CEDB8) and adds it after each row's pixels; the compressed walk's
  run handling jumps whole rows at once when a run spans them, the same walk the
  32-bit sibling takes.

Nothing in it draws `rand()`. The unit test (`tests/test_vid.cpp`,
"blt8to24_fast") pins the walk, the row step and the shadow entry on a plain
canvas; the oracle case `vid/cell_blt8to24_fast` (oracle/cases/vid.cpp) compares
the whole 96x96 24-bit canvas against 0x7CECD0 for random raw and RLE frames,
the palette flags and both shadow settings.

`kind: read-only` for now: the case is written but has not been run - the rig is
unreachable. It becomes `kind: oracle` in the follow-up commit once the case has
passed there (the same route `docs/evidence/7B37A0.md` took).
