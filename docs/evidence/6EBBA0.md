---
address: 0x6EBBA0
name: CGameAnimationTypeMonsterIcewindNew::EquipWeapon
exemption: "needs a constructed 0xF000 animation and weapon BAMs; reached by the 10GUTHE construction data test"
ghidra_version: 12.1.4
---

# 0x6EBBA0 CGameAnimationTypeMonsterIcewindNew::EquipWeapon

## Decompiled (Ghidra)

```c
void CGameAnimationTypeMonsterIcewindNew::EquipWeapon(void)

{
  int *piVar1;
  int iVar2;
  int *piVar3;
  undefined4 uVar4;
  uint uVar5;
  int *in_ECX;
  byte bVar6;
  undefined1 *puVar7;
  byte unaff_retaddr;
  int in_stack_00000010;
  undefined1 auStack_20 [8];
  void *pvStack_18;
  uint uStack_10;
  void *pvStack_c;
  int *piStack_8;
  undefined1 *puStack_4;
  
  puStack_4 = (undefined1 *)0xffffffff;
  piStack_8 = (int *)&LAB_00837fd0;
  pvStack_c = ExceptionList;
  ExceptionList = &pvStack_c;
  if (in_stack_00000010 == 0) {
    ExceptionList = &pvStack_c;
    CUtil::UtilAssert(0x6cd4,s_C__Projects_Icewind2_src_Baldur__008b6234,
                      s_pAttackProb____NULL_008b69b4,0);
  }
  *(undefined4 *)((int)in_ECX + 0x34e2) = 0;
  (**(code **)(*in_ECX + 0x94))(0x15);
  (**(code **)(*in_ECX + 0x94))(0x14);
  (**(code **)(*in_ECX + 0x94))(0x10);
  piVar1 = piStack_8;
  if (*(int *)(*piStack_8 + -8) == 0) {
    *(undefined1 *)((int)in_ECX + 0x34e6) = 0;
    *(undefined4 *)((int)in_ECX + 0x1c0e) = 0;
    if (*(int *)((int)in_ECX + 0x1cbe) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x1e72) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x2026) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x21da) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x238e) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x2542) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x26f6) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x28aa) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x2a5e) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x2c12) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x2dc6) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x2f7a) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x312e) != 0) {
      CRes::CancelRequest();
    }
    if (*(int *)((int)in_ECX + 0x32e2) != 0) {
      CRes::CancelRequest();
    }
    if (DAT_008f8e14 == 0) {
      if (in_ECX[0x766] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0x7d3] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0x840] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0x8ad] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0x91a] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0x987] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0x9f4] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xa61] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xace] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xb3b] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xba8] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xc15] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xc82] != 0) {
        CRes::CancelRequest();
      }
      if (in_ECX[0xcef] != 0) {
        CRes::CancelRequest();
      }
      *(undefined4 *)((int)in_ECX + 0x1c12) = 0;
    }
    *(undefined4 *)((int)in_ECX + 0x1c0a) = 0;
  }
  else {
    bVar6 = 0;
    do {
      iVar2 = strcmp((char *)in_ECX[bVar6 + 0xd3a],(char *)*piVar1);
      if (iVar2 == 0) {
        if (puStack_4 == (undefined1 *)0x0) {
          CUtil::UtilAssert(0x6d07,s_C__Projects_Icewind2_src_Baldur__008b6234,
                            s_colorRangeValues____NULL_008b6998,0);
        }
        iVar2 = *(int *)((int)in_ECX + 0x406);
        if (iVar2 == (int)in_ECX + 0x40e) {
          *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x1c16;
          piVar3 = in_ECX + 0x73c;
LAB_006ec001:
          *(int **)((int)in_ECX + 0x1c12) = piVar3;
        }
        else {
          if (iVar2 == (int)in_ECX + 0x5c2) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x1dca;
            piVar3 = in_ECX + 0x7a9;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x776) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x1f7e;
            piVar3 = in_ECX + 0x816;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x92a) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x2132;
            piVar3 = in_ECX + 0x883;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0xade) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x22e6;
            piVar3 = in_ECX + 0x8f0;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0xc92) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x249a;
            piVar3 = in_ECX + 0x95d;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0xe46) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x264e;
            piVar3 = in_ECX + 0x9ca;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0xffa) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x2802;
            piVar3 = in_ECX + 0xa37;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x11ae) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x29b6;
            piVar3 = in_ECX + 0xaa4;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x1362) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x2b6a;
            piVar3 = in_ECX + 0xb11;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x1516) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x2d1e;
            piVar3 = in_ECX + 0xb7e;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x16ca) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x2ed2;
            piVar3 = in_ECX + 0xbeb;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x187e) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x3086;
            piVar3 = in_ECX + 0xc58;
            goto LAB_006ec001;
          }
          if (iVar2 == (int)in_ECX + 0x1a32) {
            *(int *)((int)in_ECX + 0x1c0e) = (int)in_ECX + 0x323a;
            piVar3 = in_ECX + 0xcc5;
            goto LAB_006ec001;
          }
        }
        *(byte *)((int)in_ECX + 0x34e6) = bVar6;
        if ((unaff_retaddr & 0x80) != 0) {
          *(undefined4 *)((int)in_ECX + 0x34e2) = 1;
        }
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        iVar2 = (int)in_ECX + 0x3fe;
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6bf4);
        uStack_10 = CONCAT31(uStack_10._1_3_,1);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x1cbe) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x1cbe) + 0x7e) = uVar4;
        }
        in_ECX[0x73b] = 0;
        uStack_10 = uStack_10 & 0xffffff00;
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        if ((*(byte *)(in_ECX + 1) & 8) != 0) {
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 2;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6cfc);
          uStack_10 = CONCAT31(uStack_10._1_3_,3);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (*(int *)((int)in_ECX + 0x1e72) != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(*(int *)((int)in_ECX + 0x1e72) + 0x7e) = uVar4;
          }
          in_ECX[0x7a8] = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,2);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
        }
        if ((*(byte *)(in_ECX + 1) & 4) != 0) {
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 4;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6bf0);
          uStack_10 = CONCAT31(uStack_10._1_3_,5);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (*(int *)((int)in_ECX + 0x2026) != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(*(int *)((int)in_ECX + 0x2026) + 0x7e) = uVar4;
          }
          in_ECX[0x815] = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,4);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
        }
        if ((*(byte *)(in_ECX + 1) & 2) != 0) {
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 6;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6cf8);
          uStack_10 = CONCAT31(uStack_10._1_3_,7);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (*(int *)((int)in_ECX + 0x21da) != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(*(int *)((int)in_ECX + 0x21da) + 0x7e) = uVar4;
          }
          in_ECX[0x882] = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,6);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
        }
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 8;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b7304);
        uStack_10 = CONCAT31(uStack_10._1_3_,9);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x238e) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x238e) + 0x7e) = uVar4;
        }
        in_ECX[0x8ef] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,8);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 10;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f50);
        uStack_10 = CONCAT31(uStack_10._1_3_,0xb);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x2542) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x2542) + 0x7e) = uVar4;
        }
        in_ECX[0x95c] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,10);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0xc;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f58);
        uStack_10 = CONCAT31(uStack_10._1_3_,0xd);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x26f6) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x26f6) + 0x7e) = uVar4;
        }
        in_ECX[0x9c9] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,0xc);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0xe;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f5c);
        uStack_10 = CONCAT31(uStack_10._1_3_,0xf);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x28aa) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x28aa) + 0x7e) = uVar4;
        }
        in_ECX[0xa36] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,0xe);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        if ((*(byte *)(in_ECX + 1) & 1) != 0) {
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x10;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f44);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x11);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (*(int *)((int)in_ECX + 0x2a5e) != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(*(int *)((int)in_ECX + 0x2a5e) + 0x7e) = uVar4;
          }
          in_ECX[0xaa3] = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x10);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x12;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f40);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x13);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (*(int *)((int)in_ECX + 0x2c12) != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(*(int *)((int)in_ECX + 0x2c12) + 0x7e) = uVar4;
          }
          in_ECX[0xb10] = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x12);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
        }
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0x14;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f48);
        uStack_10 = CONCAT31(uStack_10._1_3_,0x15);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x2dc6) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x2dc6) + 0x7e) = uVar4;
        }
        in_ECX[0xb7d] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,0x14);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0x16;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f4c);
        uStack_10 = CONCAT31(uStack_10._1_3_,0x17);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x2f7a) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x2f7a) + 0x7e) = uVar4;
        }
        in_ECX[0xbea] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,0x16);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0x18;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f3c);
        uStack_10 = CONCAT31(uStack_10._1_3_,0x19);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x312e) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x312e) + 0x7e) = uVar4;
        }
        in_ECX[0xc57] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,0x18);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
        uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
        uStack_10 = 0x1a;
        uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f38);
        uStack_10 = CONCAT31(uStack_10._1_3_,0x1b);
        CResRef::CResRef(uVar4);
        CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
        CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
        if (*(int *)((int)in_ECX + 0x32e2) != 0) {
          uVar4 = CResRef::operator==(&DAT_008c5cf8);
          *(undefined4 *)(*(int *)((int)in_ECX + 0x32e2) + 0x7e) = uVar4;
        }
        in_ECX[0xcc4] = 0;
        uStack_10 = CONCAT31(uStack_10._1_3_,0x1a);
        CString_Destruct();
        uStack_10 = 0xffffffff;
        CString_Destruct();
        if (DAT_008f8e14 == 0) {
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x1c;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6e18);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x1d);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0x766] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0x766] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x1dc6) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x1c);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          if ((*(byte *)(in_ECX + 1) & 8) != 0) {
            piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
            uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
            uStack_10 = 0x1e;
            uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6e5c);
            uStack_10 = CONCAT31(uStack_10._1_3_,0x1f);
            CResRef::CResRef(uVar4);
            CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
            CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
            if (in_ECX[0x7d3] != 0) {
              uVar4 = CResRef::operator==(&DAT_008c5cf8);
              *(undefined4 *)(in_ECX[0x7d3] + 0x7e) = uVar4;
            }
            *(undefined4 *)((int)in_ECX + 0x1f7a) = 0;
            uStack_10 = CONCAT31(uStack_10._1_3_,0x1e);
            CString_Destruct();
            uStack_10 = 0xffffffff;
            CString_Destruct();
          }
          if ((*(byte *)(in_ECX + 1) & 4) != 0) {
            piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
            uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
            uStack_10 = 0x20;
            uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6e14);
            uStack_10 = CONCAT31(uStack_10._1_3_,0x21);
            CResRef::CResRef(uVar4);
            CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
            CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
            if (in_ECX[0x840] != 0) {
              uVar4 = CResRef::operator==(&DAT_008c5cf8);
              *(undefined4 *)(in_ECX[0x840] + 0x7e) = uVar4;
            }
            *(undefined4 *)((int)in_ECX + 0x212e) = 0;
            uStack_10 = CONCAT31(uStack_10._1_3_,0x20);
            CString_Destruct();
            uStack_10 = 0xffffffff;
            CString_Destruct();
          }
          if ((*(byte *)(in_ECX + 1) & 2) != 0) {
            piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
            uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
            uStack_10 = 0x22;
            uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6e58);
            uStack_10 = CONCAT31(uStack_10._1_3_,0x23);
            CResRef::CResRef(uVar4);
            CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
            CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
            if (in_ECX[0x8ad] != 0) {
              uVar4 = CResRef::operator==(&DAT_008c5cf8);
              *(undefined4 *)(in_ECX[0x8ad] + 0x7e) = uVar4;
            }
            *(undefined4 *)((int)in_ECX + 0x22e2) = 0;
            uStack_10 = CONCAT31(uStack_10._1_3_,0x22);
            CString_Destruct();
            uStack_10 = 0xffffffff;
            CString_Destruct();
          }
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x24;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f2c);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x25);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0x91a] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0x91a] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x2496) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x24);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x26;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f28);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x27);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0x987] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0x987] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x264a) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x26);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x28;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f30);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x29);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0x9f4] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0x9f4] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x27fe) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x28);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x2a;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f34);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x2b);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0xa61] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0xa61] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x29b2) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x2a);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          if ((*(byte *)(in_ECX + 1) & 1) != 0) {
            piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
            uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
            uStack_10 = 0x2c;
            uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f1c);
            uStack_10 = CONCAT31(uStack_10._1_3_,0x2d);
            CResRef::CResRef(uVar4);
            CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
            CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
            if (in_ECX[0xace] != 0) {
              uVar4 = CResRef::operator==(&DAT_008c5cf8);
              *(undefined4 *)(in_ECX[0xace] + 0x7e) = uVar4;
            }
            *(undefined4 *)((int)in_ECX + 0x2b66) = 0;
            uStack_10 = CONCAT31(uStack_10._1_3_,0x2c);
            CString_Destruct();
            uStack_10 = 0xffffffff;
            CString_Destruct();
            piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
            uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
            uStack_10 = 0x2e;
            uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f18);
            uStack_10 = CONCAT31(uStack_10._1_3_,0x2f);
            CResRef::CResRef(uVar4);
            CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
            CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
            if (in_ECX[0xb3b] != 0) {
              uVar4 = CResRef::operator==(&DAT_008c5cf8);
              *(undefined4 *)(in_ECX[0xb3b] + 0x7e) = uVar4;
            }
            *(undefined4 *)((int)in_ECX + 0x2d1a) = 0;
            uStack_10 = CONCAT31(uStack_10._1_3_,0x2e);
            CString_Destruct();
            uStack_10 = 0xffffffff;
            CString_Destruct();
          }
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x30;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f20);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x31);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0xba8] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0xba8] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x2ece) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x30);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x32;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f24);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x33);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,1);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0xc15] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0xc15] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x3082) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x32);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0x00000000,iVar2,piStack_8);
          uStack_10 = 0x34;
          uVar4 = CString_Concat_With_CString_InPlace(&piStack_8,uVar4,&DAT_008b6f14);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x35);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(&stack0xffffffd8,1,0);
          CResHelper_CResBAH_SetResRef(&stack0xffffffd8,1,0);
          if (in_ECX[0xc82] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0xc82] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x3236) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x34);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
          piStack_8 = (int *)CONCAT31(piStack_8._1_3_,*(undefined1 *)*piVar1);
          uVar4 = CString_operator_plus(&stack0xffffffd4,iVar2,piStack_8);
          uStack_10 = 0x36;
          uVar4 = CString_Concat_With_CString_InPlace(&stack0xffffffd0,uVar4,&DAT_008b6f10);
          uStack_10 = CONCAT31(uStack_10._1_3_,0x37);
          CResRef::CResRef(uVar4);
          CResHelper_CResCell_SetResRef(auStack_20,1,1);
          CResHelper_CResBAH_SetResRef(auStack_20,1,0);
          if (in_ECX[0xcef] != 0) {
            uVar4 = CResRef::operator==(&DAT_008c5cf8);
            *(undefined4 *)(in_ECX[0xcef] + 0x7e) = uVar4;
          }
          *(undefined4 *)((int)in_ECX + 0x33ea) = 0;
          uStack_10 = CONCAT31(uStack_10._1_3_,0x36);
          CString_Destruct();
          uStack_10 = 0xffffffff;
          CString_Destruct();
        }
        break;
      }
      bVar6 = bVar6 + 1;
    } while (bVar6 < 3);
    if (bVar6 == 3) {
      *(undefined1 *)((int)in_ECX + 0x34e6) = 0;
      *(undefined4 *)((int)in_ECX + 0x1c0e) = 0;
      if (*(int *)((int)in_ECX + 0x1cbe) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x1e72) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x2026) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x21da) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x238e) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x2542) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x26f6) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x28aa) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x2a5e) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x2c12) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x2dc6) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x2f7a) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x312e) != 0) {
        CRes::CancelRequest();
      }
      if (*(int *)((int)in_ECX + 0x32e2) != 0) {
        CRes::CancelRequest();
      }
      if (DAT_008f8e14 == 0) {
        *(undefined4 *)((int)in_ECX + 0x1c12) = 0;
        if (in_ECX[0x766] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0x7d3] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0x840] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0x8ad] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0x91a] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0x987] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0x9f4] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xa61] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xace] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xb3b] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xba8] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xc15] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xc82] != 0) {
          CRes::CancelRequest();
        }
        if (in_ECX[0xcef] != 0) {
          CRes::CancelRequest();
        }
      }
      *(undefined4 *)((int)in_ECX + 0x1c0a) = 0;
    }
    else {
      bVar6 = 0;
      puVar7 = puStack_4;
      do {
        CVidPalette::SetRange
                  (CONCAT31((int3)((uint)DAT_008cf6dc >> 8),bVar6) & 0xffff00ff,*puVar7,
                   *(int *)(DAT_008cf6dc + 0x1c54) + 0x3672);
        bVar6 = bVar6 + 1;
        puVar7 = puVar7 + 1;
      } while (bVar6 < 7);
      if ((DAT_008f8e14 == 0) &&
         ((short)(ushort)*(byte *)((int)in_ECX + 0x34e7) < (short)in_ECX[0xd05])) {
        *(undefined4 *)((int)in_ECX + 0x1c0a) = *(undefined4 *)((int)in_ECX + 0x1c12);
      }
      else {
        *(undefined4 *)((int)in_ECX + 0x1c0a) = *(undefined4 *)((int)in_ECX + 0x1c0e);
      }
      if ((DAT_008f8e14 == 0) ||
         ((short)in_ECX[0xd05] <= (short)(ushort)*(byte *)((int)in_ECX + 0x34e7))) {
        iVar2 = (int)(short)in_ECX[0xd05] / 2 +
                (uint)(ushort)(*(short *)((int)in_ECX + 0x3412) << 3);
      }
      else {
        uVar5 = 0x11U - (int)(short)in_ECX[0xd05] & 0x8000000f;
        if ((int)uVar5 < 0) {
          uVar5 = (uVar5 - 1 | 0xfffffff0) + 1;
        }
        iVar2 = (int)uVar5 / 2 + (uint)(ushort)(*(short *)((int)in_ECX + 0x3412) << 3);
      }
      CVidCell::SequenceSet(iVar2);
      CVidCell::FrameSet(*(undefined2 *)(*(int *)((int)in_ECX + 0x402) + 0xc4));
    }
  }
  ExceptionList = pvStack_18;
  return;
}
```

## Callers

- 0x85BA7C (not in a function)

## Callees

- 0x4428A0 CResHelper_CResCell_SetResRef
- 0x442990 CResHelper_CResBAH_SetResRef
- 0x77E370 CRes::CancelRequest
- 0x780C00 CUtil::UtilAssert
- 0x78A970 CResRef::CResRef
- 0x78ABB0 CResRef::operator==
- 0x7ADFF0 CVidCell::FrameSet
- 0x7AE290 CVidCell::SequenceSet
- 0x7BF7D0 CVidPalette::SetRange
- 0x7E7A54 strcmp
- 0x7FA83C CString_operator_plus
- 0x7FCC1A CString_Destruct
- 0x7FCE63 CString_Concat_With_CString_InPlace

## Notes

Changed, against the disassembly: (1) the "SDe" and "DeE" weapon cells are `m_deVidCellWeaponBase`/`Extend` (pRes at +0x238E and +0x2468, i.e. the cells at 0x22E6 and 0x23C0), not the `sd` ones, which the port loaded twice; (2) the "SDe", "Sp", "Ca", "Tw" cells and their "E" twins are loaded with bWarningIfMissing FALSE (`CResHelper_CResCell_SetResRef(...,1,0)`); (3) after the palette loop the current weapon cell is `m_currentVidCellWeaponBase` (+0x1C0E, 0x6ED665), not the body's `m_currentVidCellBase`; (4) the mirrored sequence formula applies when MIRROR_BAM is set and the direction exceeds m_extendDirectionTest (0x6ED671-0x6ED68C), the port had the test inverted. The palette loop writes `m_weaponPalette` (ecx = esi+0x33EE at 0x6ED60B), as before.
