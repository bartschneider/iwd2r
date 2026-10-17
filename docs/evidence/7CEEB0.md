---
address: 0x7CEEB0
name: CVidCell::Blt8To24
oracle_case: vid/cell_blt8to24
kind: oracle
ghidra_version: 12.1.4
---

# 0x7CEEB0 CVidCell::Blt8To24

## Decompiled (Ghidra)

```c
undefined4
CVidCell::Blt8To24(undefined2 *param_1,int param_2,int *param_3,int *param_4,undefined4 param_5,
                  int *param_6)

{
  int iVar1;
  int iVar2;
  int iVar3;
  short sVar4;
  int in_ECX;
  int iVar5;
  int iVar6;
  uint uVar7;
  int iVar8;
  int iVar9;
  uint uVar10;
  int iVar11;
  int iVar12;
  int iStack_10;
  int local_c;
  
  iVar12 = 0;
  uVar7 = (uint)**(ushort **)(in_ECX + 0xd0);
  iVar11 = (uint)(*(ushort **)(in_ECX + 0xd0))[1] - param_6[1];
  iVar9 = uVar7 - *param_6;
  if ((*param_6 < 0) || (param_6[1] < 0)) {
    CUtil::UtilAssert(0xc01,s_C__Projects_Icewind2_src_chitin__008bddfc,
                      s_ptSource_x_>__0____ptSource_y_>__008baec0,0);
  }
  if ((((param_4[1] < 0) || (param_4[3] < param_4[1])) || (*param_4 < 0)) || (param_4[2] < *param_4)
     ) {
    CUtil::UtilAssert(0xc02,s_C__Projects_Icewind2_src_chitin__008bddfc,
                      s_rClip_top_>__0____rClip_top_<__r_008bae64,0);
  }
  if (((param_3[1] <= param_4[3]) && (*param_3 <= param_4[2])) &&
     ((param_4[1] <= param_3[1] + iVar11 && (*param_4 <= *param_3 + iVar9)))) {
    iVar1 = CResCell::GetFrameData(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    iVar2 = *(int *)(in_ECX + 0xa8);
    if (*(int *)(iVar2 + 0x7e) == 0) {
      iVar2 = *(int *)(iVar2 + 0x54);
    }
    else {
      iVar2 = *(int *)(iVar2 + 0x58);
    }
    uVar10 = (uint)*(byte *)(iVar2 + 0xb);
    param_1 = (undefined2 *)((int)param_1 + *param_3 * 3 + param_3[1] * param_2);
    iVar2 = CResCell::GetCompressed(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    if (iVar2 == 0) {
      iVar12 = *param_6;
      iVar2 = param_3[1];
      iVar5 = param_4[1];
      iVar3 = (uint)**(ushort **)(in_ECX + 0xd0) * param_6[1] + iVar12;
      iVar6 = iVar11;
      if (iVar2 < iVar5) {
        iVar3 = iVar3 + (iVar5 - iVar2) * (uint)**(ushort **)(in_ECX + 0xd0);
        param_1 = (undefined2 *)((int)param_1 + (iVar5 - iVar2) * param_2);
        iVar6 = (iVar2 - iVar5) + iVar11;
      }
      if (param_4[3] < iVar11 + iVar2) {
        iVar6 = iVar6 + ((param_4[3] - iVar2) - iVar11);
      }
      iVar11 = *param_3;
      iVar2 = *param_4;
      iVar5 = iVar9;
      param_6 = (int *)iVar12;
      if (iVar11 < iVar2) {
        iVar8 = iVar2 - iVar11;
        param_6 = (int *)(iVar12 + iVar8);
        iVar5 = (iVar11 - iVar2) + iVar9;
        param_1 = (undefined2 *)(iVar8 * 3 + (int)param_1);
        iVar3 = iVar3 + iVar8;
      }
      iVar12 = param_4[2];
      if (iVar12 < iVar9 + iVar11) {
        param_6 = (int *)((int)param_6 + (iVar11 - iVar12) + iVar9);
        iVar5 = iVar5 + ((iVar12 - iVar11) - iVar9);
      }
      param_3 = (int *)iVar6;
      if (0 < iVar6) {
        do {
          iVar12 = iVar5;
          if (0 < iVar5) {
            do {
              if (*(byte *)(iVar1 + iVar3) != uVar10) {
                *param_1 = *(undefined2 *)(&DAT_009064f8 + *(byte *)(iVar1 + iVar3));
                *(char *)(param_1 + 1) =
                     (char)((uint)(&DAT_009064f8)[*(byte *)(iVar1 + iVar3)] >> 0x10);
              }
              param_1 = (undefined2 *)((int)param_1 + 3);
              iVar3 = iVar3 + 1;
              iVar12 = iVar12 + -1;
            } while (iVar12 != 0);
          }
          param_1 = (undefined2 *)((int)param_1 + param_2 + iVar5 * -3);
          iVar3 = iVar3 + (int)param_6;
          param_3 = (int *)((int)param_3 + -1);
        } while (param_3 != (int *)0x0);
      }
    }
    else {
      iStack_10 = param_6[1];
      iVar2 = 0;
      if (0 < iStack_10) {
        do {
          iVar5 = uVar7 * 3;
          if (uVar7 * 3 != 0) {
            do {
              if (*(byte *)(iVar1 + iVar12) == uVar10) {
                if (iVar2 == 0) {
                  iVar2 = (*(byte *)(iVar1 + 1 + iVar12) + 1) * 3;
                }
                if (iVar5 == iVar2) {
                  iVar2 = 0;
                  iVar12 = iVar12 + 2;
                  break;
                }
                if (iVar5 <= iVar2) {
                  iVar2 = iVar2 - iVar5;
                  break;
                }
                iVar12 = iVar12 + 2;
                iVar6 = -iVar2;
                iVar2 = 0;
              }
              else {
                iVar12 = iVar12 + 1;
                iVar6 = -3;
              }
              iVar5 = iVar5 + iVar6;
            } while (0 < iVar5);
          }
          iStack_10 = iStack_10 + -1;
        } while (iStack_10 != 0);
      }
      iVar5 = param_3[1];
      iVar6 = param_4[1];
      if (iVar5 < iVar6) {
        iStack_10 = iVar6 - iVar5;
      }
      else {
        iStack_10 = 0;
      }
      if ((iVar5 < iVar6) && (0 < iStack_10)) {
        local_c = iStack_10;
        do {
          iVar6 = uVar7 * 3;
          if (uVar7 * 3 != 0) {
            do {
              if (*(byte *)(iVar1 + iVar12) == uVar10) {
                if (iVar2 == 0) {
                  iVar2 = (*(byte *)(iVar1 + 1 + iVar12) + 1) * 3;
                }
                if (iVar6 == iVar2) {
                  param_1 = (undefined2 *)((int)param_1 + iVar6);
                  iVar2 = 0;
                  iVar12 = iVar12 + 2;
                  break;
                }
                if (iVar6 <= iVar2) {
                  param_1 = (undefined2 *)((int)param_1 + iVar6);
                  iVar2 = iVar2 - iVar6;
                  break;
                }
                iVar12 = iVar12 + 2;
                iVar3 = -iVar2;
                param_1 = (undefined2 *)((int)param_1 + iVar2);
                iVar2 = 0;
              }
              else {
                param_1 = (undefined2 *)((int)param_1 + 3);
                iVar12 = iVar12 + 1;
                iVar3 = -3;
              }
              iVar6 = iVar6 + iVar3;
            } while (0 < iVar6);
          }
          param_1 = (undefined2 *)((int)param_1 + param_2 + uVar7 * -3);
          local_c = local_c + -1;
        } while (local_c != 0);
      }
      sVar4 = (short)*param_4 - (short)*param_3;
      if (sVar4 < 0) {
        iVar6 = 0;
      }
      else {
        iVar6 = (int)sVar4;
      }
      iVar6 = (iVar9 - iVar6) * 3;
      if (iVar6 != 0) {
        iVar3 = iVar9;
        if (param_4[2] - *param_3 <= iVar9) {
          iVar3 = param_4[2] - *param_3;
        }
        uVar7 = iVar11 - iStack_10;
        param_3 = (int *)(param_4[3] - iVar5);
        if ((int)(((int)uVar7 < 0) - 1 & uVar7) <= (int)param_3) {
          param_3 = (int *)(((int)uVar7 < 0) - 1 & uVar7);
        }
        iVar11 = iVar9 * 3;
        if (0 < (int)param_3) {
LAB_007cf1b6:
          for (iVar5 = *param_6 * 3; iVar8 = iVar11, 0 < iVar5; iVar5 = iVar5 + iVar8) {
            if (*(byte *)(iVar1 + iVar12) == uVar10) {
              if (iVar2 == 0) {
                iVar2 = (*(byte *)(iVar1 + 1 + iVar12) + 1) * 3;
              }
              if (iVar5 == iVar2) {
                iVar2 = 0;
                iVar12 = iVar12 + 2;
                break;
              }
              if (iVar5 <= iVar2) {
                iVar2 = iVar2 - iVar5;
                break;
              }
              iVar12 = iVar12 + 2;
              iVar8 = -iVar2;
              iVar2 = 0;
            }
            else {
              iVar12 = iVar12 + 1;
              iVar8 = -3;
            }
          }
          do {
            if (iVar8 < 1) goto LAB_007cf270;
            if (*(byte *)(iVar1 + iVar12) == uVar10) {
              if (iVar2 == 0) {
                iVar2 = (*(byte *)(iVar1 + 1 + iVar12) + 1) * 3;
              }
              if (iVar8 == iVar2) {
                param_1 = (undefined2 *)((int)param_1 + iVar8);
                iVar2 = 0;
                iVar12 = iVar12 + 2;
                goto LAB_007cf270;
              }
              if (iVar8 <= iVar2) {
                param_1 = (undefined2 *)((int)param_1 + iVar8);
                iVar2 = iVar2 - iVar8;
                goto LAB_007cf270;
              }
              iVar12 = iVar12 + 2;
              iVar5 = -iVar2;
              param_1 = (undefined2 *)((int)param_1 + iVar2);
              iVar2 = 0;
            }
            else {
              if ((iVar8 <= iVar6) && ((iVar9 - iVar3) * 3 < iVar8)) {
                *param_1 = *(undefined2 *)(&DAT_009064f8 + *(byte *)(iVar1 + iVar12));
                *(char *)(param_1 + 1) =
                     (char)((uint)(&DAT_009064f8)[*(byte *)(iVar1 + iVar12)] >> 0x10);
              }
              param_1 = (undefined2 *)((int)param_1 + 3);
              iVar12 = iVar12 + 1;
              iVar5 = -3;
            }
            iVar8 = iVar8 + iVar5;
          } while( true );
        }
      }
    }
  }
  return 1;
LAB_007cf270:
  param_1 = (undefined2 *)((int)param_1 + param_2 + iVar9 * -3);
  param_3 = (int *)((int)param_3 - 1);
  if (param_3 == (int *)0x0) {
    return 1;
  }
  goto LAB_007cf1b6;
}
```

## Callers

- 0x7AF060 CVidCell::Render (call at 0x7AF145)

## Callees

- 0x77F5C0 CResCell::GetCompressed
- 0x77F5F0 CResCell::GetFrameData
- 0x780C00 CUtil::UtilAssert

## Notes

Ported from the decompile; was a stub returning TRUE. Unreachable in iwd2r: the DirectDraw shim lists only 32-bit modes (platform/ddraw_shim.cpp FillModeDesc) and GetDeviceCaps reports 32 bits, so CVideo never selects 16 or 24 bpp; ported for the record.

As Blt8To16 (7AF3C0.md) with three bytes a pixel: the low word of the rgbTempPal entry, then its third byte. The RLE walk counts in destination bytes (runs times 3). The same rows-below-the-clip behaviour; kept.

Oracle case vid/cell_blt8to24: as vid/cell_blt8to16 on a 24-bit canvas, 1600 inputs, 0 failures. tests/test_vid.cpp blt8to24_point checks the 4x3 frame raw and RLE.
