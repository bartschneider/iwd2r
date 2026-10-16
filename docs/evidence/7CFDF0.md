---
address: 0x7CFDF0
name: CVidCell::Blt8To32
oracle_case: vid/cell_blt8to32_point
ghidra_version: 12.1.4
---

# 0x7CFDF0 CVidCell::Blt8To32

## Decompiled (Ghidra)

```c
undefined4
CVidCell::Blt8To32(uint param_1,uint param_2,int *param_3,int *param_4,undefined4 param_5,
                  uint *param_6,int param_7)

{
  int iVar1;
  int iVar2;
  int iVar3;
  uint uVar4;
  short sVar5;
  int in_ECX;
  uint uVar6;
  int iVar7;
  uint uVar8;
  int iVar9;
  uint uVar10;
  uint uVar11;
  uint uVar12;
  int iVar13;
  uint *puVar14;
  int iVar15;
  int local_20;
  
  uVar12 = (uint)**(ushort **)(in_ECX + 0xd0);
  iVar15 = 0;
  iVar13 = (uint)(*(ushort **)(in_ECX + 0xd0))[1] - param_6[1];
  uVar10 = uVar12 - *param_6;
  local_20 = 0;
  if (((int)*param_6 < 0) || ((int)param_6[1] < 0)) {
    CUtil::UtilAssert(0x13a5,s_C__Projects_Icewind2_src_chitin__008bddfc,
                      s_ptSource_x_>__0____ptSource_y_>__008baec0,0);
  }
  if ((((param_4[1] < 0) || (param_4[3] < param_4[1])) || (*param_4 < 0)) || (param_4[2] < *param_4)
     ) {
    CUtil::UtilAssert(0x13a6,s_C__Projects_Icewind2_src_chitin__008bddfc,
                      s_rClip_top_>__0____rClip_top_<__r_008bae64,0);
  }
  if (((param_3[1] <= param_4[3]) && (*param_3 <= param_4[2])) &&
     ((param_4[1] <= param_3[1] + iVar13 && (*param_4 <= (int)(*param_3 + uVar10))))) {
    iVar1 = CResCell::GetFrameData(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    iVar2 = *(int *)(in_ECX + 0xa8);
    if (*(int *)(iVar2 + 0x7e) == 0) {
      iVar2 = *(int *)(iVar2 + 0x54);
    }
    else {
      iVar2 = *(int *)(iVar2 + 0x58);
    }
    uVar11 = (uint)*(byte *)(iVar2 + 0xb);
    iVar2 = (int)param_2 >> 2;
    puVar14 = (uint *)(param_1 + (iVar2 * param_3[1] + *param_3) * 4);
    iVar3 = CResCell::GetCompressed(*(undefined4 *)(in_ECX + 0xd0),*(undefined4 *)(in_ECX + 0xd6));
    if (iVar3 == 0) {
      uVar12 = *param_6;
      iVar15 = param_3[1];
      iVar3 = param_4[1];
      iVar9 = (uint)**(ushort **)(in_ECX + 0xd0) * param_6[1] + uVar12;
      iVar7 = iVar13;
      if (iVar15 < iVar3) {
        puVar14 = puVar14 + (iVar3 - iVar15) * iVar2;
        iVar9 = iVar9 + (iVar3 - iVar15) * (uint)**(ushort **)(in_ECX + 0xd0);
        iVar7 = (iVar15 - iVar3) + iVar13;
      }
      if (param_4[3] < iVar13 + iVar15) {
        iVar7 = iVar7 + ((param_4[3] - iVar15) - iVar13);
      }
      iVar13 = *param_3;
      iVar15 = *param_4;
      uVar4 = uVar10;
      param_6 = (uint *)uVar12;
      if (iVar13 < iVar15) {
        iVar3 = iVar15 - iVar13;
        param_6 = (uint *)(uVar12 + iVar3);
        uVar4 = (iVar13 - iVar15) + uVar10;
        iVar9 = iVar9 + iVar3;
        puVar14 = puVar14 + iVar3;
      }
      iVar15 = param_4[2];
      if (iVar15 < (int)(uVar10 + iVar13)) {
        param_6 = (uint *)((int)param_6 + (iVar13 - iVar15) + uVar10);
        uVar4 = uVar4 + ((iVar15 - iVar13) - uVar10);
      }
      if (param_7 == 0) {
        if (0 < iVar7) {
          do {
            uVar12 = uVar4;
            if (0 < (int)uVar4) {
              do {
                if (*(byte *)(iVar1 + iVar9) != uVar11) {
                  *puVar14 = (&DAT_009064f8)[*(byte *)(iVar1 + iVar9)];
                }
                puVar14 = puVar14 + 1;
                iVar9 = iVar9 + 1;
                uVar12 = uVar12 - 1;
              } while (uVar12 != 0);
            }
            iVar9 = iVar9 + (int)param_6;
            iVar7 = iVar7 + -1;
            puVar14 = puVar14 + (iVar2 - uVar4);
          } while (iVar7 != 0);
        }
      }
      else {
        param_7 = iVar7;
        if (0 < iVar7) {
          do {
            param_3 = (int *)uVar4;
            if (0 < (int)uVar4) {
              do {
                uVar12 = (uint)*(byte *)(iVar1 + iVar9);
                if (uVar12 == uVar11) {
                  *puVar14 = *puVar14 & 0xffffff;
                }
                else {
                  uVar10 = *puVar14 >> 8;
                  *puVar14 = ((uVar10 & 0xffff0000) * uVar12 ^ *puVar14) & 0xffffff ^
                             (uVar10 & 0xff0000) * uVar12;
                }
                puVar14 = puVar14 + 1;
                iVar9 = iVar9 + 1;
                param_3 = (int *)((int)param_3 - 1);
              } while (param_3 != (int *)0x0);
            }
            puVar14 = puVar14 + (iVar2 - uVar4);
            iVar9 = iVar9 + (int)param_6;
            param_7 = param_7 + -1;
          } while (param_7 != 0);
          return 1;
        }
      }
    }
    else {
      param_2 = param_6[1];
      uVar4 = 0;
      if (0 < (int)param_2) {
        do {
          uVar6 = uVar12;
          if (uVar12 != 0) {
            do {
              if (*(byte *)(iVar15 + iVar1) == uVar11) {
                if (uVar4 == 0) {
                  uVar4 = *(byte *)(iVar15 + 1 + iVar1) + 1;
                }
                if (uVar6 == uVar4) {
                  uVar4 = 0;
                  iVar15 = iVar15 + 2;
                  break;
                }
                if ((int)uVar6 <= (int)uVar4) {
                  uVar4 = uVar4 - uVar6;
                  break;
                }
                iVar15 = iVar15 + 2;
                iVar3 = -uVar4;
                uVar4 = 0;
              }
              else {
                iVar15 = iVar15 + 1;
                iVar3 = -1;
              }
              uVar6 = uVar6 + iVar3;
            } while (0 < (int)uVar6);
          }
          param_2 = param_2 - 1;
          local_20 = iVar15;
        } while (param_2 != 0);
      }
      iVar15 = param_3[1];
      iVar3 = param_4[1];
      if (iVar15 < iVar3) {
        param_1 = iVar3 - iVar15;
      }
      else {
        param_1 = 0;
      }
      if ((iVar15 < iVar3) && (0 < (int)param_1)) {
        param_2 = param_1;
        do {
          uVar6 = uVar12;
          if (uVar12 != 0) {
            do {
              if (*(byte *)(iVar1 + local_20) == uVar11) {
                if (uVar4 == 0) {
                  uVar4 = *(byte *)(iVar1 + 1 + local_20) + 1;
                }
                if (uVar6 == uVar4) {
                  uVar4 = 0;
                  puVar14 = puVar14 + uVar6;
                  local_20 = local_20 + 2;
                  break;
                }
                if ((int)uVar6 <= (int)uVar4) {
                  puVar14 = puVar14 + uVar6;
                  uVar4 = uVar4 - uVar6;
                  break;
                }
                local_20 = local_20 + 2;
                iVar3 = -uVar4;
                puVar14 = puVar14 + uVar4;
                uVar4 = 0;
              }
              else {
                puVar14 = puVar14 + 1;
                local_20 = local_20 + 1;
                iVar3 = -1;
              }
              uVar6 = uVar6 + iVar3;
            } while (0 < (int)uVar6);
          }
          puVar14 = puVar14 + (iVar2 - uVar12);
          param_2 = param_2 + -1;
        } while (param_2 != 0);
      }
      sVar5 = (short)*param_4 - (short)*param_3;
      if (sVar5 < 0) {
        uVar12 = 0;
      }
      else {
        uVar12 = (uint)sVar5;
      }
      iVar3 = uVar10 - uVar12;
      if (uVar10 != uVar12) {
        uVar12 = uVar10;
        if (param_4[2] - *param_3 <= (int)uVar10) {
          uVar12 = param_4[2] - *param_3;
        }
        iVar7 = uVar10 - uVar12;
        uVar12 = iVar13 - param_1;
        param_1 = param_4[3] - iVar15;
        if ((int)(((int)uVar12 < 0) - 1 & uVar12) <= (int)param_1) {
          param_1 = ((int)uVar12 < 0) - 1 & uVar12;
        }
        iVar13 = local_20;
        if (0 < (int)param_1) {
          do {
            for (uVar12 = *param_6; 0 < (int)uVar12; uVar12 = uVar12 + iVar15) {
              if (*(byte *)(iVar13 + iVar1) == uVar11) {
                if (uVar4 == 0) {
                  uVar4 = *(byte *)(iVar13 + 1 + iVar1) + 1;
                }
                if (uVar12 == uVar4) {
                  uVar4 = 0;
                  iVar13 = iVar13 + 2;
                  local_20 = iVar13;
                  break;
                }
                if ((int)uVar12 <= (int)uVar4) {
                  uVar4 = uVar4 - uVar12;
                  local_20 = iVar13;
                  break;
                }
                iVar13 = iVar13 + 2;
                iVar15 = -uVar4;
                uVar4 = 0;
              }
              else {
                iVar13 = iVar13 + 1;
                iVar15 = -1;
              }
              local_20 = iVar13;
            }
            uVar12 = uVar10;
            uVar6 = uVar4;
            if (param_7 != 0) {
joined_r0x007d0127:
              while (uVar4 = uVar6, 0 < (int)uVar12) {
                uVar6 = (uint)*(byte *)(iVar13 + iVar1);
                if (uVar6 == uVar11) {
                  if (uVar4 == 0) {
                    uVar4 = *(byte *)(iVar13 + 1 + iVar1) + 1;
                  }
                  if (uVar12 == uVar4) goto LAB_007d014f;
                  if ((int)uVar12 <= (int)uVar4) {
                    uVar4 = uVar4 - uVar12;
                    for (; 0 < (int)uVar12; uVar12 = uVar12 - 1) {
                      if (((int)uVar12 <= iVar3) && (iVar7 < (int)uVar12)) {
                        *(undefined1 *)((int)puVar14 + 3) = 0;
                      }
                      puVar14 = puVar14 + 1;
                    }
                    break;
                  }
                  iVar13 = iVar13 + 2;
                  uVar6 = uVar4;
                  local_20 = iVar13;
                  if (0 < (int)uVar4) {
                    do {
                      if (((int)uVar12 <= iVar3) && (iVar7 < (int)uVar12)) {
                        *puVar14 = *puVar14 & 0xffffff;
                      }
                      puVar14 = puVar14 + 1;
                      uVar12 = uVar12 - 1;
                      uVar4 = uVar4 - 1;
                      uVar6 = 0;
                    } while (uVar4 != 0);
                  }
                }
                else {
                  if (((int)uVar12 <= iVar3) && (iVar7 < (int)uVar12)) {
                    uVar8 = *puVar14 >> 8;
                    *puVar14 = ((uVar8 & 0xffff0000) * uVar6 ^ *puVar14) & 0xffffff ^
                               (uVar8 & 0xff0000) * uVar6;
                    iVar13 = local_20;
                  }
                  puVar14 = puVar14 + 1;
                  iVar13 = iVar13 + 1;
                  uVar12 = uVar12 - 1;
                  uVar6 = uVar4;
                  local_20 = iVar13;
                }
              }
              goto LAB_007d0275;
            }
            for (; 0 < (int)uVar12; uVar12 = uVar12 + iVar15) {
              if (*(byte *)(iVar13 + iVar1) == uVar11) {
                if (uVar4 == 0) {
                  uVar4 = *(byte *)(iVar13 + 1 + iVar1) + 1;
                }
                if (uVar12 == uVar4) {
                  uVar4 = 0;
                  puVar14 = puVar14 + uVar12;
                  iVar13 = iVar13 + 2;
                  local_20 = iVar13;
                  break;
                }
                if ((int)uVar12 <= (int)uVar4) {
                  puVar14 = puVar14 + uVar12;
                  uVar4 = uVar4 - uVar12;
                  local_20 = iVar13;
                  break;
                }
                iVar13 = iVar13 + 2;
                iVar15 = -uVar4;
                puVar14 = puVar14 + uVar4;
                uVar4 = 0;
              }
              else {
                if (((int)uVar12 <= iVar3) && (iVar7 < (int)uVar12)) {
                  *puVar14 = (&DAT_009064f8)[*(byte *)(iVar13 + iVar1)];
                }
                puVar14 = puVar14 + 1;
                iVar13 = iVar13 + 1;
                iVar15 = -1;
              }
              local_20 = iVar13;
            }
LAB_007d0275:
            puVar14 = puVar14 + (iVar2 - uVar10);
            param_1 = param_1 - 1;
            if (param_1 == 0) {
              return 1;
            }
          } while( true );
        }
      }
    }
  }
  return 1;
LAB_007d014f:
  do {
    if (((int)uVar12 <= iVar3) && (iVar7 < (int)uVar12)) {
      *puVar14 = *puVar14 & 0xffffff;
    }
    puVar14 = puVar14 + 1;
    uVar12 = uVar12 - 1;
  } while (0 < (int)uVar12);
  iVar13 = iVar13 + 2;
  uVar6 = 0;
  local_20 = iVar13;
  goto joined_r0x007d0127;
}
```

## Callers

- 0x7A0B20 CVidFont::LoadToTexture (call at 0x7A0DFA)
- 0x7C4E20 CVidCell::Render3d (call at 0x7C5091)
- 0x7C5330 CVidCell::FXRender3d (call at 0x7C55BF)
- 0x7AF060 CVidCell::Render (call at 0x7AF11E)

## Callees

- 0x77F5C0 CResCell::GetCompressed
- 0x77F5F0 CResCell::GetFrameData
- 0x780C00 CUtil::UtilAssert

## Notes

Re-ported from the decompile (the point form, which CVidCell::Render uses for the cells drawn straight to a surface: the UI). Ours was a rewrite ('original code is different, but does the same thing') that did not: for RLE frames clipped on the right it drew different pixels, and its alpha form (nTransVal) for RLE frames was missing. Now the structure of Blt8To16 (7AF3C0.md) on DWORDs: raw frames clipped row by row and column by column; RLE frames skip ptSource.y rows, then the rows above the clip, then per row ptSource.x pixels, and walk the frame width drawing the pixels between the clip's left (computed in shorts) and right. With nTransVal the frame's indices scale the surface's alpha byte ((alpha * index) << 16, top byte kept) and transparent pixels clear it, in the raw and the RLE walks. NOTE: as in the original, the RLE row count is counted from pt.y, so a frame starting above the clip draws as many rows below its bottom as it skipped. The asserts are in ChVidImage24.cpp (lines 5029, 5030), not ChVidImage.cpp.

Oracle case vid/cell_blt8to32_point (the 16- and 24-bit case on a 32-bit canvas, nTransVal 0 or random half the time): 1600 inputs, 0 failures (70 failures before, all RLE frames clipped on the right or with nTransVal). scenarios/text_screens.txt's five shots are unchanged.
