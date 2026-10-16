---
address: 0x7CFBC0
name: CVidCell::Blt8To32
oracle_case: vid/cell_blt8to32
ghidra_version: 12.1.4
---

# 0x7CFBC0 CVidCell::Blt8To32

## Decompiled (Ghidra)

```c
undefined8 CVidCell::Blt8To32(undefined4 *param_1,int param_2,undefined4 param_3,undefined4 param_4)

{
  byte bVar1;
  undefined4 uVar2;
  byte *pbVar3;
  uint uVar4;
  int in_ECX;
  uint uVar5;
  undefined4 in_EDX;
  int iVar6;
  uint uVar7;
  undefined4 *puVar8;
  bool bVar9;
  undefined8 uVar10;
  uint local_8;
  
  local_8 = (uint)(*(ushort **)(in_ECX + 0xd0))[1];
  uVar7 = (uint)**(ushort **)(in_ECX + 0xd0);
  if ((local_8 != 0) && (uVar7 != 0)) {
    if (*(int *)(in_ECX + 0xcc) == 0) {
      CVidPalette::SetPalette(*(undefined4 *)(*(int *)(in_ECX + 0xa8) + 0x6a),0x100,0);
    }
    CVidPalette::Realize(&DAT_009064f8,0x20,param_3,in_ECX + 0x2c,param_4);
    if (*(char *)(in_ECX + 0xd4) == '\0') {
      if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
        iVar6 = 0;
      }
      else {
        iVar6 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
      }
      DAT_009064fc = *(undefined4 *)(iVar6 + 0x24);
    }
    if ((*(int *)(DAT_008cf6d8 + 0x174) != 0) && (*(short *)(in_ECX + 0xc4) == 0)) {
      puVar8 = &DAT_00906500;
      iVar6 = 0xfe;
      do {
        uVar2 = CVidMode::ConvertToSurfaceRGB(0xff);
        *puVar8 = uVar2;
        puVar8 = puVar8 + 1;
        iVar6 = iVar6 + -1;
      } while (iVar6 != 0);
    }
    pbVar3 = (byte *)CResCell::GetFrameData
                               (*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    iVar6 = *(int *)(in_ECX + 0xa8);
    if (*(int *)(iVar6 + 0x7e) == 0) {
      iVar6 = *(int *)(iVar6 + 0x54);
    }
    else {
      iVar6 = *(int *)(iVar6 + 0x58);
    }
    bVar1 = *(byte *)(iVar6 + 0xb);
    param_2 = param_2 + uVar7 * -4;
    uVar10 = CResCell::GetCompressed(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    in_EDX = (undefined4)((ulonglong)uVar10 >> 0x20);
    uVar5 = uVar7;
    if ((int)uVar10 != 0) {
      uVar2 = 0;
      do {
        while (*pbVar3 != bVar1) {
          uVar2 = (&DAT_009064f8)[*pbVar3];
          pbVar3 = pbVar3 + 1;
          *param_1 = uVar2;
          param_1 = param_1 + 1;
          uVar5 = uVar5 - 1;
          if (uVar5 == 0) {
            param_1 = (undefined4 *)((int)param_1 + param_2);
            local_8 = local_8 - 1;
            uVar5 = uVar7;
            if (local_8 == 0) {
LAB_007cfd9a:
              return CONCAT44(uVar2,1);
            }
          }
        }
        uVar4 = (uint)pbVar3[1];
        pbVar3 = pbVar3 + 2;
        param_1 = param_1 + uVar4 + 1;
        uVar5 = uVar5 - 1;
        if ((uVar5 == 0) || (bVar9 = uVar5 <= uVar4, uVar4 = uVar4 - uVar5, uVar5 = local_8, bVar9))
        {
          do {
            param_1 = (undefined4 *)((int)param_1 + param_2);
            if (local_8 - 1 == 0) goto LAB_007cfd9a;
            bVar9 = uVar4 < uVar7;
            uVar4 = uVar4 - uVar7;
            uVar5 = local_8 - 1;
            if (bVar9) break;
            param_1 = (undefined4 *)((int)param_1 + param_2);
            if (local_8 - 2 == 0) goto LAB_007cfd9a;
            bVar9 = uVar4 < uVar7;
            uVar4 = uVar4 - uVar7;
            uVar5 = local_8 - 2;
            if (bVar9) break;
            param_1 = (undefined4 *)((int)param_1 + param_2);
            local_8 = local_8 - 3;
            if (local_8 == 0) goto LAB_007cfd9a;
            bVar9 = uVar7 <= uVar4;
            uVar4 = uVar4 - uVar7;
            uVar5 = local_8;
          } while (bVar9);
        }
        local_8 = uVar5;
        uVar5 = -uVar4;
      } while( true );
    }
    do {
      do {
        if (*pbVar3 != bVar1) {
          in_EDX = (&DAT_009064f8)[*pbVar3];
          *param_1 = in_EDX;
        }
        pbVar3 = pbVar3 + 1;
        param_1 = param_1 + 1;
        uVar5 = uVar5 - 1;
      } while (uVar5 != 0);
      param_1 = (undefined4 *)((int)param_1 + param_2);
      local_8 = local_8 - 1;
      uVar5 = uVar7;
    } while (local_8 != 0);
  }
  return CONCAT44(in_EDX,1);
}
```

## Callers

- 0x7AEAD0 CVidCell::Render (call at 0x7AEDC5)
- 0x7C4A90 CVidCell::Render3d (call at 0x7C4CD1)
- 0x7AE5D0 CVidCell::Render (call at 0x7AEA4A)

## Callees

- 0x77F5C0 CResCell::GetCompressed
- 0x77F5F0 CResCell::GetFrameData
- 0x795D50 CVidMode::ConvertToSurfaceRGB
- 0x7BF150 CVidPalette::Realize
- 0x7BF390 CVidPalette::SetPalette

## Notes

Checked by the oracle case vid/cell_blt8to32 (the FX form: the whole frame at the surface pointer; the cell blend fixture, raw and RLE frames, shadow on or off, four flag sets, random nTransVal): 900 inputs, 0 failures. No change.
