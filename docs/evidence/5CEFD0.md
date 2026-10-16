---
address: 0x5CEFD0
name: CInfinity::Render
exemption: "renders tiles through the video mode and the area's tilesets; checked against the decompilation"
ghidra_version: 12.1.4
---

# 0x5CEFD0 CInfinity::Render

## Decompiled (Ghidra)

```c
undefined4 CInfinity::Render(undefined *param_1,undefined4 param_2,int param_3,int param_4)

{
  byte *pbVar1;
  byte bVar2;
  byte bVar3;
  undefined4 uVar4;
  uint uVar5;
  int iVar6;
  ushort *puVar7;
  int iVar8;
  int *in_ECX;
  int *piVar9;
  int iVar10;
  int iVar11;
  ushort *puVar12;
  uint uVar13;
  int iVar14;
  bool bVar15;
  undefined4 uVar16;
  undefined4 *puVar17;
  undefined4 uVar18;
  undefined4 local_50;
  short local_4c;
  uint local_48;
  char *pcStack_44;
  undefined4 local_40;
  ushort *apuStack_3c [2];
  ushort *apuStack_34 [2];
  undefined1 auStack_2c [4];
  undefined4 uStack_28;
  uint uStack_24;
  uint uStack_20;
  int iStack_1c;
  int iStack_18;
  void *local_c;
  undefined1 *puStack_8;
  int iStack_4;
  
  iStack_4 = 0xffffffff;
  puStack_8 = &LAB_00825100;
  local_c = ExceptionList;
  local_48 = *(uint *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x1b78);
  bVar3 = *(byte *)((int)in_ECX + 0x15a);
  local_40 = 0;
  if (*(int *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x4b40) != 0) {
    local_48 = 0;
  }
  ExceptionList = &local_c;
  if (param_4 == 0) {
    ExceptionList = &local_c;
    CUtil::UtilAssert(0x1058,s_C__Projects_Icewind2_src_Baldur__008b28bc,
                      s_pVisibilityMap____NULL_008b2988,0);
  }
  in_ECX[7] = (int)param_1;
  if (param_1 == (undefined *)0x0) {
    ExceptionList = local_c;
    return 0;
  }
  if ((char)in_ECX[0xa1] != '\0') {
    uVar4 = RenderMessageScreen(param_1,param_2);
    ExceptionList = local_c;
    return uVar4;
  }
  uStack_28 = *(undefined4 *)((int)param_1 + 0x24);
  if ((param_3 == 0) &&
     ((*(int *)((int)in_ECX + 0x18e) != -1 || (*(int *)((int)in_ECX + 0x192) != -1)))) {
    Scroll(*(undefined4 *)((int)in_ECX + 0x18e),*(undefined4 *)((int)in_ECX + 0x192),
           (short)in_ECX[99]);
  }
  else {
    AdjustViewPosition(param_3);
  }
  if (in_ECX[10] == 0) {
    ExceptionList = local_c;
    return 0;
  }
  bVar2 = *(byte *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x1b7c);
  iStack_1c = in_ECX[0x10];
  param_1 = (undefined *)CONCAT31(param_1._1_3_,bVar2);
  uStack_24 = (uint)bVar2;
  uStack_20 = (uint)(*(int *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x4b40) == 0);
  iStack_18 = in_ECX[0x11];
  if (((in_ECX[0xa3] != 0) && (bVar2 != 0)) && (uStack_20 != 0)) {
    SetViewPosition((in_ECX[0xa4] >> 10) + iStack_1c,(in_ECX[0xa5] >> 10) + iStack_18,1);
    iVar6 = in_ECX[0xa4];
    if ((iVar6 < 1) || (3 < ((byte)local_48 & 7))) {
      in_ECX[0xa4] = -iVar6;
    }
    else {
      iVar10 = in_ECX[0xa6];
      if (iVar6 <= in_ECX[0xa6]) {
        iVar10 = iVar6;
      }
      in_ECX[0xa4] = iVar10 - iVar6;
      uVar5 = _rand();
      param_1 = (undefined *)(uVar5 & 0x80000001);
      if ((int)param_1 < 0) {
        param_1 = (undefined *)(((uint)(param_1 + -1) | 0xfffffffe) + 1);
      }
      if (param_1 == (undefined *)0x0) {
        in_ECX[0xa4] = -in_ECX[0xa4];
      }
    }
    iVar6 = in_ECX[0xa5];
    if ((0 < iVar6) && (((byte)local_48 & 7) < 4)) {
      iVar10 = in_ECX[0xa7];
      if (iVar6 <= in_ECX[0xa7]) {
        iVar10 = iVar6;
      }
      in_ECX[0xa5] = iVar10 - iVar6;
      uVar5 = _rand();
      param_1 = (undefined *)(uVar5 & 0x80000001);
      if ((int)param_1 < 0) {
        param_1 = (undefined *)(((uint)(param_1 + -1) | 0xfffffffe) + 1);
      }
      if (param_1 != (undefined *)0x0) goto LAB_005cf225;
      iVar6 = in_ECX[0xa5];
    }
    in_ECX[0xa5] = -iVar6;
  }
LAB_005cf225:
  if (bVar3 != *(byte *)((int)in_ECX + 0x15b)) {
    if ((bVar3 & 2) == 0) {
      param_1 = PTR_DAT_008c1758;
      puVar17 = &param_1;
      iStack_4 = 0;
      (**(code **)(*(int *)in_ECX[5] + 0x1c))(apuStack_34,puVar17);
      CResRef::CopyToString(puVar17);
      apuStack_34[0] = (ushort *)CONCAT31(apuStack_34[0]._1_3_,param_1[*(int *)(param_1 + -8) + -1])
      ;
      CString_Construct_FromRepeatedChar(apuStack_34[0],1);
      iVar6 = strcmp(pcStack_44,DAT_008e7a0c);
      CString_Destruct();
      if (iVar6 == 0) {
        uVar4 = CString_Left(apuStack_34,*(int *)(param_1 + -8) + -1);
        iStack_4._0_1_ = 1;
        CString_operator_equ(uVar4);
        iStack_4 = (uint)iStack_4._1_3_ << 8;
        CString_Destruct();
      }
      uVar18 = 1;
      uVar16 = 0x3e9;
      uVar4 = CResRef::CResRef(&param_1);
      iVar6 = CDimm::GetResObject(uVar4,uVar16,uVar18);
      iStack_4 = 0xffffffff;
      CString_Destruct();
    }
    else {
      (**(code **)(*(int *)in_ECX[5] + 0x1c))(apuStack_34);
      CResRef::operator+=(apuStack_3c,&DAT_008e7a0c);
      iVar6 = CDimm::GetResObject(apuStack_34,0x3e9,1);
    }
    if (iVar6 != 0) {
      FreeWED();
      CDimm::ReleaseResObject(in_ECX[5]);
      in_ECX[5] = iVar6;
      CRes::Request();
      AttachWED(in_ECX[5],(short)in_ECX[0x56],bVar3);
      iVar6 = *(int *)(DAT_008cf6dc + 0x1c54) + 0x3662;
      if (iVar6 != 0) {
        in_ECX[6] = iVar6;
        iVar10 = 5;
        piVar9 = in_ECX;
        do {
          iVar11 = *piVar9;
          piVar9 = piVar9 + 1;
          iVar10 = iVar10 + -1;
          *(int *)(iVar11 + 0xb0) = iVar6;
        } while (iVar10 != 0);
      }
      CGameArea::SetNewResWED(in_ECX[5]);
      piVar9 = (int *)in_ECX[6];
      if ((piVar9 != (int *)0x0) && (iVar6 = 0, 0 < *piVar9)) {
        iVar10 = 0;
        do {
          iVar11 = *(int *)(piVar9[2] + 8 + iVar10);
          if (iVar11 != 0) {
            pbVar1 = (byte *)(*(int *)(*(int *)(iVar11 + 0xb4) + *(int *)(piVar9[2] + iVar10) * 4) +
                             0x70);
            *pbVar1 = *pbVar1 & 0xfd;
          }
          iVar6 = iVar6 + 1;
          iVar10 = iVar10 + 0xc;
        } while (iVar6 < *piVar9);
      }
    }
    in_ECX[9] = 1;
    in_ECX[10] = 1;
    *(byte *)((int)in_ECX + 0x15b) = bVar3;
  }
  puVar12 = (ushort *)in_ECX[0x10];
  uVar5 = in_ECX[0x11];
  apuStack_34[0] = puVar12;
  iVar6 = GetGlobalLighting();
  if (iVar6 == *(int *)((int)in_ECX + 0x1b2)) {
    *(int *)(in_ECX[7] + 0xa4) = *(int *)((int)in_ECX + 0x1b2);
  }
  else {
    if (((*(int *)(DAT_008cf6d8 + 0x91c) == 0) && (piVar9 = (int *)in_ECX[6], piVar9 != (int *)0x0))
       && (iVar10 = 0, 0 < *piVar9)) {
      iVar11 = 0;
      do {
        iVar14 = *(int *)(piVar9[2] + 8 + iVar11);
        if (iVar14 != 0) {
          pbVar1 = (byte *)(*(int *)(*(int *)(iVar14 + 0xb4) + *(int *)(piVar9[2] + iVar11) * 4) +
                           0x70);
          *pbVar1 = *pbVar1 & 0xfd;
        }
        iVar10 = iVar10 + 1;
        iVar11 = iVar11 + 0xc;
        puVar12 = apuStack_34[0];
      } while (iVar10 < *piVar9);
    }
    *(int *)((int)in_ECX + 0x1b2) = iVar6;
    *(int *)(in_ECX[7] + 0xa4) = iVar6;
  }
  if ((*(byte *)(in_ECX + 0x56) & 4) != 0) {
    iVar6 = in_ECX[0x4a];
    iVar10 = in_ECX[0x4b];
    if ((iVar6 != iVar10) && ((in_ECX[0x4a] = iVar10, iVar6 == 0 || (iVar10 == 0)))) {
      iVar6 = 4;
      piVar9 = in_ECX;
      do {
        piVar9 = piVar9 + 1;
        iVar10 = *piVar9;
        if ((*(int *)(iVar10 + 0xb8) != 0) && (iVar11 = 0, 0 < *(int *)(iVar10 + 0xb8))) {
          do {
            iVar14 = *(int *)(*(int *)(iVar10 + 0xb4) + iVar11 * 4);
            if ((iVar14 != 0) && (*(int *)(iVar10 + 0xb0) != 0)) {
              pbVar1 = (byte *)(iVar14 + 0x70);
              *pbVar1 = *pbVar1 & 0xfd;
            }
            iVar11 = iVar11 + 1;
          } while (iVar11 < *(int *)(iVar10 + 0xb8));
        }
        iVar6 = iVar6 + -1;
      } while (iVar6 != 0);
    }
    if (in_ECX[0x4a] != 0) {
      local_40 = 4;
    }
  }
  puVar7 = (ushort *)in_ECX[0x24];
  if ((((puVar12 == puVar7) && (uVar13 = in_ECX[0x25], uVar5 == uVar13)) && (in_ECX[9] == 0)) &&
     (*(char *)((int)in_ECX + 0x15f) == '\0')) {
    if (*(int *)((int)in_ECX + 0x196) != 0) {
      if ((int)puVar7 < 0) {
        puVar7 = (ushort *)((int)puVar7 + -0x3f);
      }
      in_ECX[0x22] = (int)((int)puVar7 + ((int)puVar7 >> 0x1f & 0x3fU)) >> 6;
      uVar5 = uVar13 - 0x3f;
      if (-1 < (int)uVar13) {
        uVar5 = uVar13;
      }
      iVar6 = (int)(uVar5 + ((int)uVar5 >> 0x1f & 0x3fU)) >> 6;
      in_ECX[0x23] = iVar6;
      RequestRect(in_ECX[0x22] + -3,iVar6 + -3,in_ECX[0x1e] + 2 + in_ECX[0x22],
                  in_ECX[0x1f] + 2 + iVar6);
    }
  }
  else {
    iVar6 = in_ECX[9];
    if (iVar6 != 0) {
      in_ECX[9] = 0;
      CancelRequestRect(CONCAT31((int3)((uint)iVar6 >> 8),*(undefined1 *)((int)in_ECX + 0x15e)));
      DetachVRamRect();
    }
    if (*(char *)((int)in_ECX + 0x15f) != '\0') {
      CVidMode::Set3dClipRect(in_ECX + 0x12);
    }
    *(undefined1 *)((int)in_ECX + 0x15f) = 0;
    in_ECX[0x24] = (int)puVar12;
    in_ECX[0x25] = uVar5;
    if ((int)puVar12 < 0) {
      uVar13 = -(int)puVar12 & 0x8000003f;
      in_ECX[0x22] = (int)((int)puVar12 + -0x3f + ((int)puVar12 + -0x3f >> 0x1f & 0x3fU)) >> 6;
      if ((int)uVar13 < 0) {
        uVar13 = (uVar13 - 1 | 0xffffffc0) + 1;
      }
      in_ECX[0xc] = uVar13;
    }
    else {
      uVar13 = (uint)puVar12 & 0x8000003f;
      in_ECX[0x22] = (int)((int)puVar12 + ((int)puVar12 >> 0x1f & 0x3fU)) >> 6;
      if ((int)uVar13 < 0) {
        uVar13 = (uVar13 - 1 | 0xffffffc0) + 1;
      }
      in_ECX[0xc] = 0x40 - uVar13;
    }
    if ((int)uVar5 < 0) {
      uVar13 = -uVar5 & 0x8000003f;
      in_ECX[0x23] = (int)((uVar5 - 0x3f) + ((int)(uVar5 - 0x3f) >> 0x1f & 0x3fU)) >> 6;
      if ((int)uVar13 < 0) {
        uVar13 = (uVar13 - 1 | 0xffffffc0) + 1;
      }
      in_ECX[0xd] = uVar13;
    }
    else {
      uVar13 = uVar5 & 0x8000003f;
      in_ECX[0x23] = (int)(uVar5 + ((int)uVar5 >> 0x1f & 0x3fU)) >> 6;
      if ((int)uVar13 < 0) {
        uVar13 = (uVar13 - 1 | 0xffffffc0) + 1;
      }
      in_ECX[0xd] = 0x40 - uVar13;
    }
    iVar6 = in_ECX[0xc];
    if (iVar6 == 0) {
      iVar11 = in_ECX[0x14] - in_ECX[0x12];
      iVar10 = ((int)(iVar11 + (iVar11 >> 0x1f & 0x3fU)) >> 6) + 1;
    }
    else {
      iVar11 = in_ECX[0x14] - in_ECX[0x12];
      iVar10 = ((int)((iVar11 - iVar6) + (iVar11 - iVar6 >> 0x1f & 0x3fU)) >> 6) + 2;
    }
    iVar14 = in_ECX[0xd];
    in_ECX[0x1e] = iVar10;
    if (iVar14 == 0) {
      iVar8 = in_ECX[0x15] - in_ECX[0x13];
      iVar10 = ((int)(iVar8 + (iVar8 >> 0x1f & 0x3fU)) >> 6) + 1;
    }
    else {
      iVar8 = in_ECX[0x15] - in_ECX[0x13];
      iVar10 = ((int)((iVar8 - iVar14) + (iVar8 - iVar14 >> 0x1f & 0x3fU)) >> 6) + 2;
    }
    uVar5 = iVar11 - iVar6 & 0x8000003f;
    bVar15 = uVar5 == 0;
    in_ECX[0x1f] = iVar10;
    if ((int)uVar5 < 0) {
      bVar15 = (uVar5 - 1 | 0xffffffc0) == 0xffffffff;
    }
    if (!bVar15) {
      in_ECX[0x1e] = in_ECX[0x1e] + 1;
    }
    uVar5 = iVar8 - iVar14 & 0x8000003f;
    bVar15 = uVar5 == 0;
    if ((int)uVar5 < 0) {
      bVar15 = (uVar5 - 1 | 0xffffffc0) == 0xffffffff;
    }
    if (!bVar15) {
      in_ECX[0x1f] = in_ECX[0x1f] + 1;
    }
    if ((param_3 == 0) || (param_3 == 9)) {
      RequestRect(in_ECX[0x22] + -3,in_ECX[0x23] + -3,in_ECX[0x1e] + 2 + in_ECX[0x22],
                  in_ECX[0x1f] + 2 + in_ECX[0x23]);
    }
    else {
      ScrollingRequestRect
                (in_ECX[0x22],in_ECX[0x23],in_ECX[0x1e] + -1 + in_ECX[0x22],
                 in_ECX[0x1f] + -1 + in_ECX[0x23],param_3);
    }
    AttachVRamRect(in_ECX[0x22],in_ECX[0x23],in_ECX[0x1e] + -1 + in_ECX[0x22],
                   in_ECX[0x1f] + -1 + in_ECX[0x23]);
  }
  iVar6 = in_ECX[0x23];
  if (iVar6 < in_ECX[0x1f] + iVar6) {
    do {
      iVar10 = in_ECX[0x22];
      param_3 = iVar10;
      if (iVar10 < in_ECX[0x1e] + iVar10) {
        do {
          if (in_ECX[0xc] == 0) {
            iVar10 = (param_3 - iVar10) * 0x40;
          }
          else {
            iVar10 = ((param_3 - iVar10) + -1) * 0x40 + in_ECX[0xc];
          }
          iVar10 = iVar10 + in_ECX[0x12];
          iVar11 = in_ECX[0xd];
          if (iVar11 == 0) {
            iVar11 = in_ECX[0x13];
            iVar14 = (iVar6 - in_ECX[0x23]) * 0x40;
          }
          else {
            iVar14 = ((iVar6 - in_ECX[0x23]) + -1) * 0x40 + in_ECX[0x13];
          }
          iVar14 = iVar14 + iVar11;
          apuStack_34[0] = (ushort *)CResWED::GetTileData(0,param_3,iVar6);
          if (apuStack_34[0] == (ushort *)0x0) {
            iVar11 = in_ECX[7];
LAB_005cfa67:
            CInfTileSet::RenderBlack(iVar11,param_2,in_ECX + 0x12,iVar10,iVar14);
          }
          else {
            local_50 = 0;
            if (((byte)apuStack_34[0][3] & 0x1e) != 0) {
              bVar3 = (byte)apuStack_34[0][3] & 0x1e;
              if (bVar3 == 4) {
                iVar11 = 2;
              }
              else if (bVar3 == 8) {
                iVar11 = 3;
              }
              else {
                iVar11 = ((bVar3 == 0x10) - 1 & 0xfffffffd) + 4;
              }
              iVar8 = CResWED::GetLayerHeader(iVar11);
              if (((iVar8 != 0) &&
                  (apuStack_3c[0] = (ushort *)CResWED::GetTileData(iVar11,0,0),
                  apuStack_3c[0] != (ushort *)0x0)) &&
                 (iVar8 = CResWED::GetTileList(iVar11), iVar8 != 0)) {
                *(undefined4 *)(in_ECX[iVar11] + 4) = *(undefined4 *)((int)in_ECX + 0x1b2);
                CInfTileSet::Render(in_ECX[7],param_2,
                                    *(undefined2 *)
                                     (iVar8 + ((local_48 >> 1) % (uint)apuStack_3c[0][1] +
                                              (uint)*apuStack_3c[0]) * 2),0xffffffff,in_ECX + 0x12,
                                    iVar10,iVar14,&DAT_0084eddc,0,0,local_40,0);
                local_50 = 3;
              }
            }
            puVar12 = apuStack_34[0];
            if ((apuStack_34[0][3] & 1) == 0) {
              iVar11 = CResWED::GetTileList(0);
              if (iVar11 != 0) {
                uVar5 = (uint)*(byte *)((int)puVar12 + 7);
                if (uVar5 == 0) {
                  uVar5 = 1;
                }
                *(undefined4 *)(*in_ECX + 4) = *(undefined4 *)((int)in_ECX + 0x1b2);
                if (((puVar12[4] & 2) == 0) || (puVar12[2] == 0xffff)) {
                  uVar5 = (uint)*(ushort *)
                                 (iVar11 + ((int)(((ulonglong)local_48 / (ulonglong)uVar5) %
                                                 (ulonglong)puVar12[1]) + (uint)*puVar12) * 2);
                }
                else {
                  uVar5 = (uint)(short)puVar12[2];
                }
                local_4c = (short)iVar6;
                CVisibilityMap::GetTileCode
                          ((uint)(ushort)((short)in_ECX[0xe] * local_4c) + param_3,auStack_2c);
                if ((puVar12[3] & 0x1e) == 0) {
                  CInfTileSet::Render(in_ECX[7],param_2,uVar5,0xffffffff,in_ECX + 0x12,iVar10,iVar14
                                      ,auStack_2c,local_50,0,0,0);
                }
                else {
                  CInfTileSet::Render(in_ECX[7],param_2,uVar5,(int)(short)puVar12[2],in_ECX + 0x12,
                                      iVar10,iVar14,auStack_2c,local_50,uStack_28,0,0);
                }
              }
            }
            else if ((apuStack_34[0][3] & 0x1e) == 0) {
              iVar11 = in_ECX[7];
              goto LAB_005cfa67;
            }
          }
          iVar10 = in_ECX[0x22];
          param_3 = param_3 + 1;
        } while (param_3 < in_ECX[0x1e] + iVar10);
      }
      iVar6 = iVar6 + 1;
    } while (iVar6 < in_ECX[0x23] + in_ECX[0x1f]);
  }
  if (((((in_ECX[0xa3] != 0) && (uStack_24 != 0)) && (uStack_20 != 0)) &&
      ((iStack_1c != -1 && (SetViewPosition(iStack_1c,iStack_18,1), in_ECX[0xa4] == 0)))) &&
     (in_ECX[0xa5] == 0)) {
    in_ECX[0xa3] = 0;
  }
  if (*(int *)(DAT_008cf6d8 + 0x91c) != 0) {
    CVidMode::RenderTint3d(*(undefined4 *)((int)in_ECX + 0x1b2),in_ECX + 0x12);
  }
  ExceptionList = local_c;
  return 1;
}
```

## Callers

- 0x477740 CGameArea::Render (call at 0x477813)
- 0x477740 CGameArea::Render (call at 0x477837)
- 0x477740 CGameArea::Render (call at 0x477856)

## Callees

- 0x401190 CResWED::GetLayerHeader
- 0x4011C0 CResWED::GetTileData
- 0x4012E0 CResWED::GetTileList
- 0x4790D0 CGameArea::SetNewResWED
- 0x5526E0 CVisibilityMap::GetTileCode
- 0x5CB4A0 CInfTileSet::Render
- 0x5CB630 CInfTileSet::RenderBlack
- 0x5CC7F0 CInfinity::AttachWED
- 0x5CCE90 CInfinity::AttachVRamRect
- 0x5CD2E0 CInfinity::CancelRequestRect
- 0x5CDA70 CInfinity::DetachVRamRect
- 0x5CE020 CInfinity::FreeWED
- 0x5CE960 CInfinity::GetGlobalLighting
- 0x5CFE80 CInfinity::RequestRect
- 0x5D00C0 CInfinity::ScrollingRequestRect
- 0x5D11F0 CInfinity::SetViewPosition
- 0x5D1380 CInfinity::Scroll
- 0x5D1C30 CInfinity::AdjustViewPosition
- 0x5D2530 CInfinity::RenderMessageScreen
- 0x77E610 CRes::Request
- 0x780C00 CUtil::UtilAssert
- 0x786DF0 CDimm::GetResObject
- 0x787CE0 CDimm::ReleaseResObject
- 0x78A970 CResRef::CResRef
- 0x78A9F0 CResRef::CopyToString
- 0x78ADD0 CResRef::operator+=
- 0x7BC110 CVidMode::Set3dClipRect
- 0x7BD270 CVidMode::RenderTint3d
- 0x7E7A54 strcmp
- 0x7E8160 _rand
- 0x7FA7C1 CString_Construct_FromRepeatedChar
- 0x7FAA72 CString_Left
- 0x7FCC1A CString_Destruct
- 0x7FCD07 CString_operator_equ

## Notes

vis-probe round 9 (from render round 5): the tile code passed to CInfinityTileSet::Render (a TILE_CODE, 4 bytes) is one stack slot for the whole tile pass. GetTileCode (0x5526E0) writes NE only when the tile is not in the last column and SW/SE only when it is not in the last row, so those corners keep the previous tile's values. The port declared it inside the loop without an initialiser (a fresh, indeterminate value per tile, visible with odd map heights on the last row). Now declared once before the loops; ours starts at 0 where the original's slot holds whatever its stack held before the first tile.
