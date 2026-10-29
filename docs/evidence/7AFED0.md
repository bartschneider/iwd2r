---
address: 0x7AFED0
name: CVidCell::sub_7AFED0
oracle_case: vid/cell_translucent16
kind: read-only
ghidra_version: 12.1.4
---

# 0x7AFED0 CVidCell::sub_7AFED0

## Decompiled (Ghidra)

```c
undefined4 CVidCell::sub_7AFED0(ushort *param_1,int param_2,undefined4 param_3,int param_4)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  byte bVar4;
  uint uVar5;
  uint uVar6;
  uint uVar7;
  uint uVar8;
  int iVar9;
  uint uVar10;
  int in_ECX;
  int iVar11;
  uint uVar12;
  ushort extraout_var;
  uint uVar13;
  int iVar14;
  int iVar15;
  int iStack_40;
  uint uStack_3c;
  uint uStack_34;
  undefined2 local_30;
  uint local_28;

  iVar15 = param_4;
  uVar13 = (uint)**(ushort **)(in_ECX + 0xd0);
  local_28 = (uint)(*(ushort **)(in_ECX + 0xd0))[1];
  iVar9 = *(int *)(DAT_008cf6d8 + 0x3c4);
  if (iVar9 == 0) {
    iVar11 = 0;
  }
  else {
    iVar11 = *(int *)(iVar9 + 4);
  }
  uVar5 = *(uint *)(iVar11 + 0x24);
  if (iVar9 == 0) {
    iVar11 = 0;
  }
  else {
    iVar11 = *(int *)(iVar9 + 4);
  }
  uVar6 = *(uint *)(iVar11 + 0x18);
  if (iVar9 == 0) {
    iVar11 = 0;
  }
  else {
    iVar11 = *(int *)(iVar9 + 4);
  }
  uVar7 = *(uint *)(iVar11 + 0x1c);
  if (iVar9 == 0) {
    iVar11 = 0;
  }
  else {
    iVar11 = *(int *)(iVar9 + 4);
  }
  uVar8 = *(uint *)(iVar11 + 0x20);
  if (iVar9 == 0) {
    iVar11 = 0;
  }
  else {
    iVar11 = *(int *)(iVar9 + 4);
  }
  bVar1 = *(byte *)(iVar11 + 0xaa);
  if (iVar9 == 0) {
    iVar11 = 0;
  }
  else {
    iVar11 = *(int *)(iVar9 + 4);
  }
  bVar2 = *(byte *)(iVar11 + 0xae);
  if (iVar9 == 0) {
    iVar9 = 0;
  }
  else {
    iVar9 = *(int *)(iVar9 + 4);
  }
  bVar3 = *(byte *)(iVar9 + 0xb2);
  if (0x100 < param_4) {
    CUtil::UtilAssert(0x107d,s_C__Projects_Icewind2_src_chitin__008bac90,s_nTransVal_<__256_008baee4
                      ,0);
  }
  if (DAT_009060f0 == 0) {
    DAT_009060f0 = FUN_007fc95b(0x20000);
    if (DAT_009060f0 == 0) {
      return 0;
    }
    iStack_40 = 0x40;
    uStack_3c = 0;
    do {
      param_4 = 0;
      uStack_34 = 0x40;
      uVar12 = uStack_3c;
      do {
        iVar14 = 0x40;
        iVar9 = param_4;
        iVar11 = uVar12;
        do {
          *(char *)(iVar11 + DAT_009060f0) = (char)(iVar9 >> 6);
          iVar11 = iVar11 + 0x800;
          iVar9 = iVar9 + uStack_3c * 2;
          iVar14 = iVar14 + -1;
        } while (iVar14 != 0);
        param_4 = param_4 + iStack_40;
        uVar12 = uVar12 + 0x20;
        uStack_34 = uStack_34 + -1;
      } while (uStack_34 != 0);
      iStack_40 = iStack_40 + -2;
      uStack_3c = uStack_3c + 1;
    } while (0 < iStack_40);
  }
  uVar12 = 8;
  do {
    uVar12 = (int)uVar12 >> 1;
    iVar15 = iVar15 >> 1;
  } while (1 < uVar12);
  iVar9 = (int)(param_2 + uVar13 * -2) >> 1;
  iVar11 = CResCell::GetFrameData(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
  if (*(int *)(in_ECX + 0xcc) == 0) {
    CVidPalette::SetPalette
              (*(undefined4 *)(*(int *)(in_ECX + 0xa8) + 0x6a),0x100,(uint)extraout_var << 0x10);
  }
  CVidPalette::Realize(&DAT_009064f8,0x10,param_3,in_ECX + 0x2c,0xff);
  if (*(char *)(in_ECX + 0xd4) == '\0') {
    local_30 = (undefined2)uVar5;
    DAT_009064fc._0_2_ = local_30;
  }
  iVar14 = *(int *)(in_ECX + 0xa8);
  if (*(int *)(iVar14 + 0x7e) == 0) {
    iVar14 = *(int *)(iVar14 + 0x54);
  }
  else {
    iVar14 = *(int *)(iVar14 + 0x58);
  }
  bVar4 = *(byte *)(iVar14 + 0xb);
  uVar12 = 0;
  uStack_3c = 0;
  iVar14 = CResCell::GetCompressed(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
  if (iVar14 == 0) {
    param_2 = 0;
    uStack_34 = local_28 + 1;
    do {
      if (uVar13 != 0) {
        local_28 = uVar13;
        do {
          uVar12 = (uint)*(ushort *)(&DAT_009064f8 + *(byte *)(param_2 + iVar11));
          if (uVar12 != (uVar5 & 0xffff)) {
            uVar10 = (uint)*param_1;
            *param_1 = (ushort)*(byte *)((((uVar10 & uVar6) >> (bVar1 & 0x1f)) * 0x40 +
                                         ((uVar12 & uVar6) >> (bVar1 & 0x1f))) * 0x20 + DAT_009060f0
                                        + iVar15) << (bVar1 & 0x1f) |
                       (ushort)*(byte *)((((uVar10 & uVar7) >> (bVar2 & 0x1f)) * 0x40 +
                                         ((uVar12 & uVar7) >> (bVar2 & 0x1f))) * 0x20 + DAT_009060f0
                                        + iVar15) << (bVar2 & 0x1f) |
                       (ushort)*(byte *)((((uVar10 & uVar8) >> (bVar3 & 0x1f)) * 0x40 +
                                         ((uVar12 & uVar8) >> (bVar3 & 0x1f))) * 0x20 + DAT_009060f0
                                        + iVar15) << (bVar3 & 0x1f);
          }
          param_1 = param_1 + 1;
          param_2 = param_2 + 1;
          local_28 = local_28 - 1;
        } while (local_28 != 0);
      }
      param_1 = param_1 + iVar9;
      uStack_34 = uStack_34 + -1;
    } while (uStack_34 != 0);
  }
  else {
    param_4 = 0;
    if (local_28 != 0) {
      do {
        uStack_34 = uVar13;
        if (uVar13 != 0) {
          do {
            if (*(byte *)(param_4 + iVar11) == bVar4) {
              if (uVar12 == 0) {
                uVar12 = *(byte *)(param_4 + 1 + iVar11) + 1;
              }
              if (uStack_34 == uVar12) {
                uVar12 = 0;
                param_4 = param_4 + 2;
                param_1 = param_1 + uStack_34;
                uStack_3c = 0;
                break;
              }
              if ((int)uStack_34 <= (int)uVar12) {
                uVar12 = uVar12 - uStack_34;
                param_1 = param_1 + uStack_34;
                uStack_3c = uVar12;
                break;
              }
              uStack_34 = uStack_34 - uVar12;
              param_4 = param_4 + 2;
              param_1 = param_1 + uVar12;
              uVar12 = 0;
              uStack_3c = 0;
            }
            else {
              uVar10 = (uint)*(ushort *)(&DAT_009064f8 + *(byte *)(param_4 + iVar11));
              if (uVar10 != (uVar5 & 0xffff)) {
                uVar12 = (uint)*param_1;
                *param_1 = (ushort)*(byte *)((((uVar12 & uVar6) >> (bVar1 & 0x1f)) * 0x40 +
                                             ((uVar10 & uVar6) >> (bVar1 & 0x1f))) * 0x20 +
                                             DAT_009060f0 + iVar15) << (bVar1 & 0x1f) |
                           (ushort)*(byte *)(iVar15 + (((uVar12 & uVar7) >> (bVar2 & 0x1f)) * 0x40 +
                                                      ((uVar10 & uVar7) >> (bVar2 & 0x1f))) * 0x20 +
                                                      DAT_009060f0) << (bVar2 & 0x1f) |
                           (ushort)*(byte *)((((uVar12 & uVar8) >> (bVar3 & 0x1f)) * 0x40 +
                                             ((uVar10 & uVar8) >> (bVar3 & 0x1f))) * 0x20 +
                                             DAT_009060f0 + iVar15) << (bVar3 & 0x1f);
                uVar12 = uStack_3c;
              }
              param_1 = param_1 + 1;
              param_4 = param_4 + 1;
              uStack_34 = uStack_34 - 1;
            }
          } while (0 < (int)uStack_34);
        }
        param_1 = param_1 + iVar9;
        local_28 = local_28 - 1;
        if (local_28 == 0) {
          return 1;
        }
      } while( true );
    }
  }
  return 1;
}
```

## Callers

- 0x7AEAD0 CVidCell::Render (call at 0x7AEF65)
- 0x7AE5D0 CVidCell::Render (call at 0x7AE87C)

## Callees

- 0x77F5C0 CResCell::GetCompressed
- 0x77F5F0 CResCell::GetFrameData
- 0x780C00 CUtil::UtilAssert
- 0x7BF150 CVidPalette::Realize
- 0x7BF390 CVidPalette::SetPalette
- 0x7FC95B FUN_007fc95b

## Notes

Ported from the Ghidra listing and the disassembly (0x7AFED0-0x7B0481, `ret 0x10`): the translucent
blit of a whole frame - the 8-bit frame's indices through the realized 16-bit palette
(`CVidImage::rgbTempPal`, one WORD a pixel), rows bottom-up by `lPitch` in bytes, each drawn pixel
blended through the 64x64x32 table at `CVidImage::m_pBlendTable` (0x9060F0) with the mode's masks
and shifts. It is the 16-bit twin of `sub_7D0950` (`Blt8To32Translucent`) and the last of the
16/24-bpp family: same palette setup (`SetPalette` unless `m_bPaletteChanged`,
`Realize(rgbTempPal, 16, dwFlags, &m_paletteAffects, 0xFF)`), same `UTIL_ASSERT(nTransVal <= 256)`,
and with `!m_bShadowOn` only the low word of `rgbTempPal[SHADOW_ENTRY]` takes the mode's `field_24`
(0x7B00F1). Called by both `CVidCell::Render` overloads for the `CVIDIMG_TRANSLUCENT` column. NOTE:
the survey (report-T50.md) names it `Blt8To16Translucent`; the export has only the address, so the
code keeps the stub's `sub_7AFED0` and both Render call sites stay untouched.

The blend:

- the table is built lazily if null (0x7AFFAD, `operator new(0x20000)`, the same build
  `CVidImage`'s constructor does; a failed allocation returns FALSE);
- `nTransVal` is shifted right three times (0x7B0057-0x7B0063), so the weight is `nTransVal >> 3`
  (0..32 for the asserted range 0..256);
- the index is `((destChannel * 64 + paletteChannel) * 32) + weight` (0x7B01E5-0x7B023C): the
  table's row is the **destination** channel and its column the palette's, so weight 0 gives the
  palette and weight 32 the destination. The brief's sketch had the two swapped; the bytes decide.
  At `nTransVal` 256 the weight 32 lands on the next column's weight 0 - an original edge case,
  kept by the plain arithmetic;
- each channel's table byte is shifted by the mode's shift and ORed (0x7B023C/0x7B0269/0x7B0298);
- a palette word equal to `field_24 & 0xFFFF` is **skipped** - neither blended nor written - in both
  walks (0x7B01D4-0x7B01DF and 0x7B0343-0x7B0367). This is how the shadow entry becomes transparent
  with `m_bShadowOn == 0`, unlike the 32-bit sibling, which draws it opaque.

Two quirks of the raw walk (0x7B0312):

- it draws **height + 1 rows** (`inc eax` before the loop), reading one row past the frame's data;
- it does **not** skip the transparent index: every pixel is looked up in the palette and blended
  unless its value is the shadow colour. The compressed walk (height rows) does skip transparent
  tokens.

The row step is `(lPitch - width * 2) >> 1` **pixels**, added to a WORD pointer (0x7B007B). Checked
against T59's findings: neither 16-bit-only quirk of `sub_7AF1C0` is here - the walks read `[esi]`
(no one-byte lead) and their continuations are width-based, not the `ja`-after-`dec` carry.

Nothing in the function draws `rand()`.

## Oracle

`vid/cell_translucent16` (oracle/cases/vid.cpp): a synthetic BAM (zeroed `CResCell` with the parsed
flag, header, palette and one frame entry; data raw or RLE), random frames up to 8x4 with the
transparent index mixed in and index 1 present, the palette flags, the cell's shadow on and off, the
mode's shadow colour and `nTransVal` (including 0 and 256), drawn at (8, 8) of a 64x64 16-bit canvas
of random bytes, on 5-6-5 and 5-5-5 in turn. The frame is sized so the raw walk's extra row stays
inside the canvas, and the BAM's zero bytes there are the palette's index 0, as the original reads
them. The whole canvas and the result are compared. The unit test (`tests/test_vid.cpp`,
"translucent16") pins the walk, the blend at weight 0/16/32, the shadow skip, the raw extra row and
the raw walk's treatment of the transparent index on both mask layouts.
