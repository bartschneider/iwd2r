---
address: 0x7CCBF0
name: CVidBitmap::BltBmp24To24
oracle_case: vid/blt_bmp24to24
ghidra_version: 12.1.4
---

# 0x7CCBF0 CVidBitmap::BltBmp24To24

## Decompiled (Ghidra)

```c
undefined8
CVidBitmap::BltBmp24To24
          (uint *param_1,uint param_2,byte *param_3,int *param_4,int param_5,uint param_6)

{
  byte bVar1;
  byte bVar2;
  byte bVar3;
  byte bVar4;
  byte bVar5;
  int iVar6;
  byte bVar7;
  uint uVar8;
  int iVar9;
  int iVar10;
  byte *pbVar11;
  uint *puVar12;
  int local_18;
  int local_14;
  uint local_c;
  
  local_18 = param_4[1];
  iVar6 = *param_4;
  iVar10 = 0;
  if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
    iVar9 = 0;
  }
  else {
    iVar9 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
  }
  bVar1 = *(byte *)(iVar9 + 0xaa);
  bVar2 = *(byte *)(iVar9 + 0xae);
  bVar3 = *(byte *)(iVar9 + 0xb2);
  if ((param_6 & 1) == 0) {
    param_6 = iVar6 + -1;
    do {
      do {
        puVar12 = param_1;
        pbVar11 = param_3;
        *puVar12 = (uint)pbVar11[2] << (bVar1 & 0x1f) | (uint)pbVar11[1] << (bVar2 & 0x1f) |
                   (uint)*pbVar11 << (bVar3 & 0x1f);
        param_6 = param_6 + -1;
        param_3 = pbVar11 + 3;
        param_1 = (uint *)((int)puVar12 + 3);
      } while (param_6 != 0);
      uVar8 = (uint)pbVar11[5] << (bVar1 & 0x1f) | (uint)pbVar11[4] << (bVar2 & 0x1f) |
              (uint)pbVar11[3] << (bVar3 & 0x1f);
      *(ushort *)((int)puVar12 + 3) = (ushort)uVar8;
      *(char *)((int)puVar12 + 5) = (char)(uVar8 >> 0x10);
      iVar9 = 0;
      local_18 = local_18 + -1;
      param_3 = pbVar11 + param_5 + 6;
      param_1 = (uint *)((int)puVar12 + (6 - (param_2 + iVar6 * 3)));
      param_6 = iVar6 + -1;
    } while (local_18 != 0);
  }
  else {
    iVar9 = param_2 + iVar6 * 2;
    if (0 < local_18) {
      puVar12 = param_1;
      do {
        local_14 = iVar6 + -1;
        if (0 < iVar6 + -1) {
          do {
            bVar4 = param_3[iVar10 + 2];
            bVar5 = param_3[iVar10 + 1];
            bVar7 = param_3[iVar10];
            local_c = (uint)bVar4;
            param_1 = (uint *)(uint)bVar5;
            param_2 = (uint)bVar7;
            if (((bVar4 != 0) || (bVar5 != 0xff)) || (bVar7 != 0)) {
              if ((((param_6 & 4) != 0) && (bVar4 == 0)) && ((bVar5 == 0 && (bVar7 == 0)))) {
                uVar8 = *puVar12;
                local_c = (uint)((byte)((uVar8 >> (bVar1 & 0x1f)) >> 1) & 0x7f);
                param_1 = (uint *)(uint)((byte)((uVar8 >> (bVar2 & 0x1f)) >> 1) & 0x7f);
                param_2 = (uint)((byte)((uVar8 >> (bVar3 & 0x1f)) >> 1) & 0x7f);
              }
              *puVar12 = param_2 << (bVar3 & 0x1f) | (int)param_1 << (bVar2 & 0x1f) |
                         local_c << (bVar1 & 0x1f);
            }
            puVar12 = (uint *)((int)puVar12 + 3);
            iVar10 = iVar10 + 3;
            local_14 = local_14 + -1;
          } while (local_14 != 0);
        }
        bVar4 = param_3[iVar10 + 2];
        bVar5 = param_3[iVar10 + 1];
        bVar7 = param_3[iVar10];
        local_c = (uint)bVar4;
        param_1 = (uint *)(uint)bVar5;
        if (((bVar4 != 0) || (bVar5 != 0xff)) || (bVar7 != 0)) {
          if (((((param_6 & 4) != 0) && (bVar4 == 0)) && (bVar5 == 0)) && (bVar7 == 0)) {
            local_c = (uint)((byte)((ushort)((ushort)*puVar12 >> (bVar1 & 0x1f)) >> 1) & 0x7f);
            param_1 = (uint *)(uint)((byte)((ushort)((ushort)*puVar12 >> (bVar2 & 0x1f)) >> 1) &
                                    0x7f);
            bVar7 = *(byte *)((int)puVar12 + 2) >> 1;
          }
          uVar8 = (uint)bVar7 << (bVar3 & 0x1f) | (int)param_1 << (bVar2 & 0x1f) |
                  local_c << (bVar1 & 0x1f);
          *(ushort *)puVar12 = (ushort)uVar8;
          *(char *)((int)puVar12 + 2) = (char)(uVar8 >> 0x10);
        }
        iVar10 = iVar10 + 3 + param_5;
        puVar12 = (uint *)((int)puVar12 + (3 - (iVar6 + iVar9)));
        local_18 = local_18 + -1;
      } while (local_18 != 0);
      return CONCAT44(iVar6 + iVar9,1);
    }
  }
  return CONCAT44(iVar9,1);
}
```

## Callers

- 0x7B3190 CVidBitmap::RenderDirect (call at 0x7B3507)

## Callees

None.

## Notes

Ported from the decompile and disassembly; was a stub returning FALSE. Unreachable in iwd2r: the DirectDraw shim lists only 32-bit modes, so CVideo never selects 24 bpp; ported for the record. Each pixel's red, green and blue (bytes 2, 1, 0 of the data) shifted into place; unkeyed, written as in BltBmp8To24 (DWORDs at three-byte steps, a row's last as a WORD and a BYTE; the original's unkeyed loop is inline assembly). Keyed (0x1): pure green skipped; black with 0x4 halves the destination's channels (each shifted down, halved and masked to 7 bits); NOTE (kept): a drawn pixel's fourth byte lands on the next pixel's first even when that pixel is skipped, and the row's last pixel takes the shadow's red and green from its first WORD and its blue from its third byte (0x7CCD6E). Oracle case vid/blt_bmp24to24: bitmaps 2 to 32 wide drawn upward on a 64x40 canvas, plain, keyed and keyed with shadows, the data mixing green, black and random pixels, under shifts 16/8/0 and 0/8/16: 1600 inputs, 0 failures. tests/test_vid.cpp blt_bmp8to24_and_24to24 checks a keyed row by hand.
