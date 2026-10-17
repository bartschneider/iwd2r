---
address: 0x5EFFD0
name: FUN_005effd0
exemption: "writes the selected character's text sheet; checked by tests/test_ui_misc.cpp [uimisc][character]"
kind: read-only
ghidra_version: 12.1.4
---

# 0x5EFFD0 FUN_005effd0

## Decompiled (Ghidra)

```c
undefined4 * FUN_005effd0(undefined4 *param_1,undefined1 *param_2)

{
  uint *puVar1;
  bool bVar2;
  int *piVar3;
  char cVar4;
  undefined1 uVar5;
  short sVar6;
  undefined4 uVar7;
  undefined4 *puVar8;
  int iVar9;
  undefined4 uVar10;
  undefined4 *puVar11;
  int iVar12;
  uint uVar13;
  char *pcVar14;
  int iVar15;
  undefined *puVar16;
  LPCSTR lpFileName;
  UINT UVar17;
  int in_ECX;
  int *piVar18;
  int iVar19;
  float fVar20;
  undefined **ppuVar21;
  float *pfVar22;
  undefined4 uVar23;
  undefined *local_114;
  undefined *local_110;
  float fStack_10c;
  float fStack_108;
  int local_104;
  int *local_100;
  undefined *local_fc;
  undefined4 *local_f8;
  undefined *local_f4;
  undefined *local_f0;
  char cStack_e9;
  undefined4 *local_e8;
  undefined *puStack_e4;
  undefined *local_e0;
  int iStack_dc;
  int iStack_d8;
  undefined *puStack_d4;
  int local_d0;
  int *local_cc;
  int local_c8;
  int local_c4;
  int iStack_c0;
  int local_bc;
  undefined4 auStack_b8 [5];
  int local_a4;
  undefined *local_a0;
  float local_9c;
  undefined *puStack_98;
  uint local_94;
  uint uStack_90;
  int iStack_8c;
  undefined *local_88;
  undefined *local_84;
  int local_80;
  undefined1 local_7c [4];
  undefined1 local_78 [4];
  undefined4 local_74;
  undefined *local_70;
  undefined4 local_6c;
  undefined *local_68;
  undefined1 local_60 [4];
  undefined1 local_5c [4];
  undefined1 local_58 [4];
  undefined *local_54;
  undefined1 auStack_4e [2];
  undefined4 local_4c;
  undefined1 uStack_48;
  undefined1 uStack_47;
  undefined1 uStack_46;
  undefined1 uStack_45;
  undefined4 uStack_44;
  undefined1 auStack_3d [5];
  undefined4 uStack_38;
  undefined2 uStack_34;
  short sStack_2e;
  undefined1 auStack_21 [15];
  undefined1 uStack_12;
  undefined1 uStack_11;
  undefined1 auStack_10 [4];
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  puStack_8 = &LAB_00828923;
  local_c = ExceptionList;
  local_94 = 0;
  local_88 = PTR_DAT_008c1758;
  local_84 = PTR_DAT_008c1758;
  local_a0 = PTR_DAT_008c1758;
  local_4._0_1_ = 3;
  local_4._1_3_ = 0;
  if (*(short *)(in_ECX + 0x28) < *(short *)(DAT_008cf6dc[0x715] + 0x3846)) {
    local_74 = *(undefined4 *)(DAT_008cf6dc[0x715] + 0x382e + *(short *)(in_ECX + 0x28) * 4);
    ExceptionList = &local_c;
    uVar10 = local_74;
  }
  else {
    local_74 = 0xffffffff;
    ExceptionList = &local_c;
    uVar10 = local_74;
  }
  do {
    do {
      cVar4 = CGameObjectArray::GetShare(uVar10,0,&local_104,0xffffffff);
      iVar15 = local_104;
    } while (cVar4 == '\x04');
  } while (cVar4 == '\x05');
  if (cVar4 != '\0') {
    *param_2 = 0;
    *param_1 = PTR_DAT_008c1758;
    local_94 = local_94 | 1;
    goto LAB_005f6565;
  }
  local_d0 = DAT_008cf6dc[0x715];
  piVar18 = (int *)(local_104 + 0x5a4);
  local_c4 = local_104 + 0x3e36;
  puVar1 = (uint *)(local_104 + 0x920);
  local_100 = piVar18;
  uVar7 = CGameSprite::GetName();
  CString_Construct(uVar7);
  local_4._0_1_ = 4;
  uVar5 = (undefined1)local_4;
  local_4._0_1_ = 4;
  iVar9 = *piVar18;
  if (iVar9 != -1) {
    uVar7 = CBaldurEngine::FetchString(&local_f8,iVar9);
    local_4._0_1_ = 5;
    CString_operator_equ(uVar7);
    local_4._0_1_ = 4;
    CString_Destruct();
    uVar5 = (undefined1)local_4;
  }
  local_4._0_1_ = uVar5;
  iVar19 = 0;
  iVar9 = local_a4;
  if (0 < *(int *)(local_a4 + -8)) {
    do {
      cVar4 = *(char *)(iVar19 + iVar9);
      if ((((((cVar4 == '\\') || (cVar4 == '/')) || (cVar4 == ':')) ||
           ((cVar4 == '*' || (cVar4 == '?')))) || (cVar4 == '<')) ||
         ((cVar4 == '>' || (cVar4 == '|')))) {
        CString_SetAt(iVar19,0x5f);
        iVar9 = local_a4;
      }
      iVar19 = iVar19 + 1;
    } while (iVar19 < *(int *)(iVar9 + -8));
  }
  CInfGame::GetDirSaveRoot(local_78);
  local_4._0_1_ = 6;
  uVar7 = CString_Concat(&local_f0,local_78,&DAT_008f3338);
  local_4._0_1_ = 7;
  uVar7 = CString_Concat_With_CString_InPlace(&local_110,uVar7,&DAT_008ab8a4);
  local_4._0_1_ = 8;
  uVar7 = CString_Concat(&local_114,uVar7,&local_a4);
  local_4._0_1_ = 9;
  puVar8 = (undefined4 *)CString_Concat_With_CString_InPlace(&local_f8,uVar7,&DAT_008b3330);
  iVar9 = CInfGame_B3AUTO_007e8aab(*puVar8,&DAT_008ab204);
  CString_Destruct();
  local_4._0_1_ = 8;
  CString_Destruct();
  local_4._0_1_ = 7;
  CString_Destruct();
  local_4._0_1_ = 6;
  CString_Destruct();
  uVar7 = CString_Construct(&DAT_008b3328);
  local_4._0_1_ = 10;
  uVar7 = CString_Concat(local_60,uVar7,&DAT_008f3338);
  local_4._0_1_ = 0xb;
  uVar7 = CString_Concat_With_CString_InPlace(local_58,uVar7,&DAT_008ab8a4);
  local_4._0_1_ = 0xc;
  uVar7 = CString_Concat(local_5c,uVar7,&local_a4);
  local_4._0_1_ = 0xd;
  CString_Concat_With_CString_InPlace(local_7c,uVar7,s__txt_008b3320);
  local_4._0_1_ = 0x12;
  CString_Destruct();
  local_4._0_1_ = 0x11;
  CString_Destruct();
  local_4._0_1_ = 0x10;
  CString_Destruct();
  local_4._0_1_ = 0xf;
  CString_Destruct();
  if (iVar9 == 0) {
    *param_2 = 0;
    CGameObjectArray::ReleaseShare(uVar10,0,0xffffffff);
    CString_Construct(local_7c);
    local_94 = local_94 | 1;
  }
  else {
    local_68 = PTR_DAT_008c1758;
    local_54 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x14;
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x417);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x17;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a6a68,local_a4);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x16;
    CString_Destruct();
    local_4._0_1_ = 0x15;
    CString_Destruct();
    local_4._0_1_ = 0x14;
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    switch(*(undefined1 *)(iVar15 + 0x3e4a)) {
    default:
      uVar10 = 0xffffffff;
      break;
    case 1:
      uVar10 = 0x41a;
      break;
    case 2:
      uVar10 = 0x41b;
      break;
    case 3:
      uVar10 = 0x2044;
      break;
    case 4:
      uVar10 = 0x2045;
      break;
    case 5:
      uVar10 = 0x2046;
    }
    local_f8._0_1_ = *(undefined1 *)(iVar15 + 0x3e4a);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,uVar10);
    local_4._0_1_ = 0x18;
    puVar11 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x2f67);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x1b;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar11);
    CString_Format(&local_fc,&DAT_008a6a68,*puVar8);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x1a;
    CString_Destruct();
    local_4._0_1_ = 0x19;
    CString_Destruct();
    local_4._0_1_ = 0x18;
    CString_Destruct();
    local_4._0_1_ = 0x14;
    CString_Destruct();
    local_70 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x1c;
    local_f8 = (undefined4 *)CONCAT31(local_f8._1_3_,*(undefined1 *)(iVar15 + 0x3e3d));
    local_114 = (undefined *)CONCAT31(local_114._1_3_,*(undefined1 *)(iVar15 + 0x3e3c));
    CRuleTables::GetRaceStringMixed(local_114,&local_70,local_f8);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x418);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x1f;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a6a68,local_70);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x1e;
    CString_Destruct();
    local_4._0_1_ = 0x1d;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x1c);
    CString_Destruct();
    local_f8 = (undefined4 *)CONCAT31(local_f8._1_3_,*(undefined1 *)(iVar15 + 0x3e4b));
    switch(*(undefined1 *)(iVar15 + 0x3e4b)) {
    case 0:
      puVar16 = (undefined *)0xffffffff;
      break;
    default:
      CUtil::UtilAssert(0x3cde,s_C__Projects_Icewind2_src_Baldur__008b2afc,s_FALSE_008a636c,0);
      puVar16 = local_68;
      break;
    case 0x11:
      puVar16 = (undefined *)0x44e;
      break;
    case 0x12:
      puVar16 = (undefined *)0x450;
      break;
    case 0x13:
      puVar16 = (undefined *)0x44f;
      break;
    case 0x21:
      puVar16 = (undefined *)0x451;
      break;
    case 0x22:
      puVar16 = (undefined *)0x452;
      break;
    case 0x23:
      puVar16 = (undefined *)0x453;
      break;
    case 0x31:
      puVar16 = (undefined *)0x454;
      break;
    case 0x32:
      puVar16 = (undefined *)0x455;
      break;
    case 0x33:
      puVar16 = (undefined *)0x456;
    }
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,puVar16);
    local_4._0_1_ = 0x20;
    puVar11 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x419);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x23;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar11);
    CString_Format(&local_fc,&DAT_008a6a68,*puVar8);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x22;
    CString_Destruct();
    local_4._0_1_ = 0x21;
    CString_Destruct();
    local_4._0_1_ = 0x20;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x42c0);
    FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x974);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x479);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x26;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x25;
    CString_Destruct();
    local_4._0_1_ = 0x24;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x97a);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x47f);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x29;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x28;
    CString_Destruct();
    local_4._0_1_ = 0x27;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x97c);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x49a);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x2c;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x2b;
    CString_Destruct();
    local_4._0_1_ = 0x2a;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x976);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x49b);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x2f;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x2e;
    CString_Destruct();
    local_4._0_1_ = 0x2d;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x978);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x49c);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x32;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x31;
    CString_Destruct();
    local_4._0_1_ = 0x30;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x97e);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x49d);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x35;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x34;
    CString_Destruct();
    local_4._0_1_ = 0x33;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    uVar10 = CGameSprite::GetAC();
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x2863);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x38;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,uVar10);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x37;
    CString_Destruct();
    local_4._0_1_ = 0x36;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x924);
    iVar19 = local_100[7];
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x7901);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x3b;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,s__d__d_008b3310,(int)(short)iVar19,(int)sVar6);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x3a;
    CString_Destruct();
    local_4._0_1_ = 0x39;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    if (0 < *(int *)(local_104 + 0x713c)) {
      puVar8 = *(undefined4 **)(local_104 + 0x7134);
      local_4._0_1_ = 0x3c;
      local_fc = PTR_DAT_008c1758;
      for (; puVar8 != (undefined4 *)0x0; puVar8 = (undefined4 *)*puVar8) {
        CRuleTables::GetCharacterStateDescription(puVar8[2],&local_fc);
        FUN_007e89c1(iVar9,s__s___s_008b3308,PTR_DAT_008b2ab4,local_fc);
      }
      local_4._0_1_ = 0x1c;
      CString_Destruct();
    }
    if ((*puVar1 & 0x800) != 0) {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x2e35);
      FUN_007e89c1(iVar9,s__s___s_008b3308,PTR_DAT_008b2ab4,*puVar8);
      CString_Destruct();
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    bVar2 = false;
    iVar19 = CGameSprite::sub_763150(5);
    if ((iVar19 != 0) && ((*(uint *)(local_104 + 0xa14) & 0x400000) != 0)) {
      bVar2 = true;
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,0x8bbe);
      local_4._0_1_ = 0x3d;
      puVar11 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x9b8f);
      FUN_007e89c1(iVar9,s__s___s_008b2d74,*puVar11,*puVar8);
      CString_Destruct();
      local_4._0_1_ = 0x1c;
      CString_Destruct();
    }
    iVar19 = CGameSprite::sub_763150(0x13);
    if ((iVar19 != 0) && (iVar19 = CGameSprite::sub_726270(0x13), 0 < iVar19)) {
      bVar2 = true;
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_110,0x8bc9);
      local_4._0_1_ = 0x3e;
      local_f8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,0x9b8f);
      uVar10 = CGameSprite::sub_726270(0x13);
      FUN_007e89c1(iVar9,s__s___s__d_008b2c48,*local_f8,*puVar8,uVar10);
      CString_Destruct();
      local_4._0_1_ = 0x1c;
      CString_Destruct();
    }
    iVar19 = CGameSprite::sub_763150(0x1b);
    if ((iVar19 != 0) && ((*(uint *)(local_104 + 0xa14) & 0x800000) != 0)) {
      bVar2 = true;
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,0x8bcb);
      local_4._0_1_ = 0x3f;
      puVar11 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x9b8f);
      FUN_007e89c1(iVar9,s__s___s_008b2d74,*puVar11,*puVar8);
      CString_Destruct();
      local_4._0_1_ = 0x1c;
      CString_Destruct();
    }
    iVar19 = CGameSprite::sub_763150(0x2f);
    if ((iVar19 != 0) && (iVar19 = CGameSprite::sub_726270(0x2f), 0 < iVar19)) {
      bVar2 = true;
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_110,0x8bd2);
      local_4._0_1_ = 0x40;
      local_f8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,0x9b8f);
      uVar10 = CGameSprite::sub_726270(0x2f);
      FUN_007e89c1(iVar9,s__s___s__d_008b2c48,*local_f8,*puVar8,uVar10);
      CString_Destruct();
      local_4._0_1_ = 0x1c;
      CString_Destruct();
    }
    iVar19 = CGameSprite::sub_763150(0x31);
    if ((iVar19 == 0) || ((*(uint *)(local_104 + 0xa14) & 0x1000000) == 0)) {
      if (bVar2) goto LAB_005f1066;
    }
    else {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_114,0x8bd4);
      local_4._0_1_ = 0x41;
      puVar11 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x9b8f);
      FUN_007e89c1(iVar9,s__s___s_008b2d74,*puVar11,*puVar8);
      CString_Destruct();
      local_4._0_1_ = 0x1c;
      CString_Destruct();
LAB_005f1066:
      FUN_007e8973(&DAT_008a6a50,iVar9);
    }
    local_e0 = (undefined *)0x0;
    iVar19 = 1;
    piVar18 = &local_4c;
    do {
      iVar12 = CDerivedStats::GetClassLevel(iVar19);
      if (iVar12 != 0) {
        local_e0 = local_e0 + 1;
        *piVar18 = iVar19;
        piVar18 = piVar18 + 1;
      }
      iVar19 = iVar19 + 1;
    } while (iVar19 < 0xc);
    if ((int)local_e0 < 2) {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x2f68);
      FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    }
    else {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x4d09);
      FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    }
    CString_Destruct();
    if (0 < (int)local_e0) {
      local_e8 = &local_4c;
      local_114 = local_e0;
      do {
        uVar10 = *local_e8;
        uVar7 = CDerivedStats::GetClassLevel(uVar10);
        CRuleTables::GetClassStringMixed
                  (uVar10,*(undefined4 *)(local_104 + 0x80c),local_100[2],&local_84,
                   *(char *)(local_c4 + 0x14) != '\x02');
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x2f68);
        local_f4 = PTR_DAT_008c1758;
        local_fc = PTR_DAT_008c1758;
        local_4._0_1_ = 0x44;
        CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
        CString_Format(&local_fc,&DAT_008a6a68,local_84);
        uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x43;
        CString_Destruct();
        local_4._0_1_ = 0x42;
        CString_Destruct();
        local_4._0_1_ = 0x1c;
        CString_Destruct();
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_110,0x42c2);
        local_f4 = PTR_DAT_008c1758;
        local_fc = PTR_DAT_008c1758;
        local_4._0_1_ = 0x47;
        CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
        CString_Format(&local_fc,&DAT_008a64c4,uVar7);
        uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x46;
        CString_Destruct();
        local_4._0_1_ = 0x45;
        CString_Destruct();
        local_4._0_1_ = 0x1c;
        CString_Destruct();
        local_e8 = local_e8 + 1;
        local_114 = local_114 + -1;
      } while (local_114 != (undefined *)0x0);
    }
    uVar10 = CDerivedStats::GetXP();
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f8,0x42c1);
    local_f4 = PTR_DAT_008c1758;
    local_fc = PTR_DAT_008c1758;
    local_4._0_1_ = 0x4a;
    CString_Format(&local_f4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_fc,&DAT_008a64c4,uVar10);
    uVar10 = FUN_005ef760(&local_f4,&local_fc,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x49;
    CString_Destruct();
    local_4._0_1_ = 0x48;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x1c);
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    CRuleTables::sub_546B60(local_104,&local_80,&local_9c,&local_6c,1);
    iVar19 = 0;
    local_c8 = 0;
    local_bc = 0;
    if (local_104 == -0x4ad8) {
LAB_005f1471:
      iVar12 = local_c8;
      CUtil::UtilAssert(0x3f0c,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                        s_curWeapon____NULL_008b2df4,0);
    }
    else {
      iVar12 = *(int *)(local_104 + 0x4ad8 + (uint)*(byte *)(local_104 + 0x4ba4) * 4);
      local_c8 = iVar12;
      uVar13 = CGameSprite::sub_726800();
      iVar19 = *(int *)(local_104 + 0x4ad8 + (uVar13 & 0xff) * 4);
      if ((*(char *)(local_104 + 0x4ba4) == '*') ||
         ((local_bc = iVar19, iVar19 != 0 &&
          ((((sVar6 = CItem::GetItemType(), sVar6 == 0x2f ||
             (sVar6 = CItem::GetItemType(), sVar6 == 0x35)) ||
            (sVar6 = CItem::GetItemType(), sVar6 == 0x31)) ||
           (sVar6 = CItem::GetItemType(), sVar6 == 0x29)))))) {
        iVar19 = 0;
        local_bc = 0;
      }
      if (iVar12 == 0) {
        CGameSprite::SelectWeaponAbility(10,0,0,1);
        iVar12 = *(int *)(local_104 + 0x4ad8 + (uint)*(byte *)(local_104 + 0x4ba4) * 4);
        local_c8 = iVar12;
        if (iVar12 == 0) goto LAB_005f1471;
      }
    }
    local_cc = (int *)0x0;
    local_e0 = (undefined *)0x0;
    local_110 = (undefined *)0x0;
    local_f4 = (undefined *)CGameSprite::sub_7240A0();
    local_fc = (undefined *)CGameSprite::sub_724170();
    if (iVar12 != 0) {
      CItem::Demand();
    }
    pcVar14 = (char *)CItem::GetAbility(*(undefined2 *)(local_104 + 0x4ba6));
    if (pcVar14 == (char *)0x0) {
      CUtil::UtilAssert(0x3f20,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                        s_curAttack____NULL_008b2de0,0);
    }
    iStack_8c = CGameSprite::sub_73CA20(local_c8,pcVar14);
    iVar12 = CGameSprite::sub_73CAE0(local_c8,pcVar14);
    iStack_8c = iStack_8c + iVar12;
    iVar12 = CGameSprite::sub_73CC40(local_c8,pcVar14);
    iStack_8c = iStack_8c + iVar12;
    local_e8 = (undefined4 *)CGameSprite_B3AUTO_0073cf10(local_c8,pcVar14);
    iVar12 = CGameSprite_B3AUTO_0073d080(0);
    local_e8 = (undefined4 *)((int)local_e8 + iVar12);
    local_f8 = (undefined4 *)CGameSprite::sub_73CB10(local_c8,pcVar14);
    if (iVar19 != 0) {
      CItem::Demand();
      CResRef::CResRef();
      CResRef::CResRef();
      CAbilityId::CAbilityId();
      uVar10 = CResRef::operator=(&fStack_10c,&DAT_008c5cf8);
      CResRef::operator=(&iStack_dc,uVar10);
      uStack_38 = 0xffffffff;
      uStack_44 = 0xffffffff;
      puVar8 = &local_4c;
      uStack_34 = 0;
      uStack_12 = 0;
      uStack_11 = 1;
      cVar4 = CGameSprite::sub_726800(puVar8);
      CGameSprite::GetQuickWeapon(cVar4 + -0x2b,puVar8);
      puStack_d4 = (undefined *)CItem::GetAbility((int)sStack_2e);
      if (puStack_d4 == (undefined *)0x0) {
        CUtil::UtilAssert(0x3f3b,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                          s_curAttackSecond____NULL_008b2dc8,0);
      }
      local_cc = (int *)CGameSprite::sub_73CA20(iVar19,puStack_d4);
      iVar12 = CGameSprite::sub_73CAE0(iVar19,puStack_d4);
      local_cc = (int *)((int)local_cc + iVar12);
      iVar12 = CGameSprite::sub_73CC40(iVar19,puStack_d4);
      local_cc = (int *)((int)local_cc + iVar12);
      local_e0 = (undefined *)CGameSprite_B3AUTO_0073cf10(iVar19,puStack_d4);
      iVar12 = CGameSprite_B3AUTO_0073d080(1);
      local_e0 = local_e0 + iVar12;
      local_110 = (undefined *)CGameSprite::sub_73CB10(iVar19,puStack_d4);
    }
    if (local_c8 != 0) {
      CItem::Release();
    }
    iVar19 = CGameSprite::sub_73C7E0();
    iVar12 = CGameSprite::sub_73CAD0();
    local_114 = (undefined *)(iVar19 + iVar12);
    puStack_e4 = PTR_DAT_008c1758;
    puStack_98 = PTR_DAT_008c1758;
    local_4 = CONCAT31(local_4._1_3_,0x4c);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f0,0x24f1);
    FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    CString_Destruct();
    cStack_e9 = '\0';
    puStack_d4 = (undefined *)CGameSprite::GetLauncher(pcVar14,auStack_4e);
    if (puStack_d4 == (undefined *)0x0) {
LAB_005f1772:
      if (*pcVar14 == '\x02') goto LAB_005f1777;
    }
    else {
      CItem::Demand();
      iVar19 = CItem::GetAbility(0);
      if (iVar19 == 0) goto LAB_005f1772;
LAB_005f1777:
      cStack_e9 = '\x01';
    }
    if (puStack_d4 != (undefined *)0x0) {
      CItem::Release();
    }
    fStack_10c = local_9c;
    if ((((local_c8 == 0) || (local_bc == 0)) ||
        ((local_c8 == local_bc ||
         ((sVar6 = CItem::GetItemType(), sVar6 == 0x2f ||
          (sVar6 = CItem::GetItemType(), sVar6 == 0x35)))))) ||
       ((sVar6 = CItem::GetItemType(), sVar6 == 0x31 ||
        (sVar6 = CItem::GetItemType(), sVar6 == 0x29)))) {
      local_9c = (float)(int)*(short *)(local_104 + 0x93a);
    }
    else {
      local_9c = (float)((int)*(short *)(local_104 + 0x93a) - 1);
    }
    CString_operator_equ(&DAT_008c5cf8);
    iVar19 = 0;
    if (0 < (int)local_9c) {
      do {
        iVar12 = local_80;
        if ((((*(uint *)(local_104 + 0xa14) & 0x1000000) != 0) && (cStack_e9 == '\x01')) &&
           (iVar19 == 0)) {
          iVar12 = local_80 + -5;
        }
        sVar6 = CRuleTables::GetIterativeAttackBonus(iVar12,local_6c,iVar19);
        sVar6 = sVar6 + (short)local_fc + (short)local_f4 + (short)local_114 + (short)local_f8 +
                (short)local_e8 + (short)iStack_8c;
        if (((*(uint *)(local_104 + 0xa14) & 0x1000000) != 0) && (cStack_e9 == '\x01')) {
          sVar6 = sVar6 + -2;
        }
        CString_Format(&puStack_98,&PTR_DAT_008abd14,(int)sVar6);
        CString_operator_plus_equ(&puStack_98);
        iVar19 = iVar19 + 1;
      } while ((iVar19 < (int)local_9c) &&
              (CString_operator_plus_equ(&DAT_008b2dbc), iVar19 < (int)local_9c));
    }
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&iStack_dc,0x2de);
    local_f0 = PTR_DAT_008c1758;
    puStack_d4 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x4f;
    CString_Format(&local_f0,&DAT_008a6a68,*puVar8);
    CString_Format(&puStack_d4,&DAT_008a6a68,puStack_e4);
    uVar10 = FUN_005ef760(&local_f0,&puStack_d4,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x4e;
    CString_Destruct();
    local_4._0_1_ = 0x4d;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x4c);
    CString_Destruct();
    CString_operator_equ(&DAT_008c5cf8);
    fVar20 = 0.0;
    if (0 < (int)local_9c) {
      do {
        uVar10 = CRuleTables::GetIterativeAttackBonus(local_80,local_6c,fVar20);
        CString_Format(&puStack_98,&PTR_DAT_008abd14,uVar10);
        CString_operator_plus_equ(&puStack_98);
        fVar20 = (float)((int)fVar20 + 1);
        if ((uint)fVar20 < (uint)fStack_10c) {
          CString_operator_plus_equ(&DAT_008b2dbc);
        }
      } while ((int)fVar20 < (int)local_9c);
    }
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x7a79);
    puStack_d4 = PTR_DAT_008c1758;
    local_f0 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x52;
    CString_Format(&puStack_d4,&DAT_008a6a68,*puVar8);
    CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
    uVar10 = FUN_005ef760(&puStack_d4,&local_f0,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x51;
    CString_Destruct();
    local_4._0_1_ = 0x50;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    iVar19 = local_bc;
    if (local_c8 != local_bc) {
      if ((short)iStack_8c != 0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)(short)iStack_8c);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x7f30);
        puStack_d4 = PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x55;
        CString_Format(&puStack_d4,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&puStack_d4,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x54;
        CString_Destruct();
        local_4._0_1_ = 0x53;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if ((short)local_e8 != 0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)(short)local_e8);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x7f31);
        local_e8 = (undefined4 *)PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x58;
        CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x57;
        CString_Destruct();
        local_4._0_1_ = 0x56;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if ((short)local_f8 != 0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)(short)local_f8);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x830b);
        local_e8 = (undefined4 *)PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x5b;
        CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x5a;
        CString_Destruct();
        local_4._0_1_ = 0x59;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if (local_f4 != (undefined *)0x0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,local_f4);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x9b88);
        local_e8 = (undefined4 *)PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x5e;
        CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x5d;
        CString_Destruct();
        local_4._0_1_ = 0x5c;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if (local_fc != (undefined *)0x0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,local_fc);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x9b8e);
        local_e8 = (undefined4 *)PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x61;
        CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x60;
        CString_Destruct();
        local_4._0_1_ = 0x5f;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
    }
    if (iVar19 != 0) {
      if (local_c8 != iVar19) {
        FUN_007e8973(&DAT_008a6a50,iVar9);
      }
      sVar6 = (short)local_cc;
      CString_Format(&puStack_e4,&PTR_DAT_008abd14,
                     (int)(short)((short)local_80 + (short)local_fc + (short)local_f4 +
                                  (short)local_114 + (short)local_110 + (short)local_e0 + sVar6));
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2dd);
      local_e8 = (undefined4 *)PTR_DAT_008c1758;
      local_f0 = PTR_DAT_008c1758;
      local_4._0_1_ = 100;
      CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
      CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
      uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 99;
      CString_Destruct();
      local_4._0_1_ = 0x62;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      CString_operator_equ(&DAT_008c5cf8);
      CString_Format(&puStack_98,&PTR_DAT_008abd14,local_80);
      CString_operator_plus_equ(&puStack_98);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x7a79);
      local_e8 = (undefined4 *)PTR_DAT_008c1758;
      local_f0 = PTR_DAT_008c1758;
      local_4._0_1_ = 0x67;
      CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
      CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
      uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0x66;
      CString_Destruct();
      local_4._0_1_ = 0x65;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      if (sVar6 != 0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)sVar6);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x7f30);
        local_e8 = (undefined4 *)PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x6a;
        CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x69;
        CString_Destruct();
        local_4._0_1_ = 0x68;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if ((short)local_e0 != 0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)(short)local_e0);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x7f31);
        local_e8 = (undefined4 *)PTR_DAT_008c1758;
        local_f0 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x6d;
        CString_Format(&local_e8,&DAT_008a6a68,*puVar8);
        CString_Format(&local_f0,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_e8,&local_f0,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x6c;
        CString_Destruct();
        local_4._0_1_ = 0x6b;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if ((short)local_110 != 0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)(short)local_110);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x830b);
        local_f0 = PTR_DAT_008c1758;
        local_110 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x70;
        CString_Format(&local_f0,&DAT_008a6a68,*puVar8);
        CString_Format(&local_110,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_f0,&local_110,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x6f;
        CString_Destruct();
        local_4._0_1_ = 0x6e;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if (local_f4 != (undefined *)0x0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,local_f4);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x9b88);
        local_f0 = PTR_DAT_008c1758;
        local_110 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x73;
        CString_Format(&local_f0,&DAT_008a6a68,*puVar8);
        CString_Format(&local_110,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_f0,&local_110,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x72;
        CString_Destruct();
        local_4._0_1_ = 0x71;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      if (local_fc != (undefined *)0x0) {
        CString_Format(&puStack_e4,&PTR_DAT_008abd14,local_fc);
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x9b8e);
        local_f0 = PTR_DAT_008c1758;
        local_110 = PTR_DAT_008c1758;
        local_4._0_1_ = 0x76;
        CString_Format(&local_f0,&DAT_008a6a68,*puVar8);
        CString_Format(&local_110,&DAT_008a6a68,puStack_e4);
        uVar10 = FUN_005ef760(&local_f0,&local_110,1,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0x75;
        CString_Destruct();
        local_4._0_1_ = 0x74;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      CItem::Release();
      iVar19 = local_bc;
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    if ((short)local_114 != 0) {
      CString_Format(&puStack_e4,&PTR_DAT_008abd14,(int)(short)local_114);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x830c);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0x79;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a6a68,puStack_e4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0x78;
      CString_Destruct();
      local_4._0_1_ = 0x77;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    if (local_c8 == iVar19) {
      iVar19 = 1;
LAB_005f2624:
      CString_Format(&local_a0,&DAT_008a64c4,iVar19);
    }
    else {
      uVar13 = CGameSprite::sub_726800();
      if (*(int *)(local_104 + 0x4ad8 + (uVar13 & 0xff) * 4) == 0) {
LAB_005f2612:
        iVar19 = (int)*(short *)(iVar15 + 0x93a);
        goto LAB_005f2624;
      }
      CGameSprite::sub_726800();
      sVar6 = CItem::GetItemType();
      if (sVar6 == 0x2f) goto LAB_005f2612;
      CGameSprite::sub_726800();
      sVar6 = CItem::GetItemType();
      if (sVar6 == 0x35) goto LAB_005f2612;
      CGameSprite::sub_726800();
      sVar6 = CItem::GetItemType();
      if (sVar6 == 0x31) goto LAB_005f2612;
      CGameSprite::sub_726800();
      sVar6 = CItem::GetItemType();
      if (sVar6 == 0x29) goto LAB_005f2612;
      CString_Format(&local_a0,s__d__d_008b2dac,*(short *)(iVar15 + 0x93a) + -1,1);
    }
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24f2);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x7c;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a6a68,local_a0);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x7b;
    CString_Destruct();
    local_4._0_1_ = 0x7a;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    uVar10 = CGameSprite::GetAC();
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x8311);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x7f;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,uVar10);
    uVar10 = FUN_005ef760(&local_110,&local_114,0,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x7e;
    CString_Destruct();
    local_4._0_1_ = 0x7d;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x926);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2edd);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x82;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x81;
    CString_Destruct();
    local_4._0_1_ = 0x80;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x928);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x830f);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x85;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x84;
    CString_Destruct();
    local_4._0_1_ = 0x83;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x92a);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2ee6);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x88;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x87;
    CString_Destruct();
    local_4._0_1_ = 0x86;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x92c);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x8310);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x8b;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x8a;
    CString_Destruct();
    local_4._0_1_ = 0x89;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    uVar10 = CGameSprite_sub_723EA0();
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x47f);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x8e;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,uVar10);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x8d;
    CString_Destruct();
    local_4._0_1_ = 0x8c;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    cVar4 = *(char *)(iVar15 + 0xa64);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x919b);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x91;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,(int)cVar4);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x90;
    CString_Destruct();
    local_4._0_1_ = 0x8f;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    cVar4 = *(char *)(iVar15 + 0xa6c);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x92d6);
    local_110 = PTR_DAT_008c1758;
    local_114 = PTR_DAT_008c1758;
    local_4._0_1_ = 0x94;
    CString_Format(&local_110,&DAT_008a6a68,*puVar8);
    CString_Format(&local_114,&DAT_008a64c4,(int)cVar4);
    uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x93;
    CString_Destruct();
    local_4._0_1_ = 0x92;
    CString_Destruct();
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    iVar19 = CAIObjectType::IsClassValid(9);
    if (iVar19 != 0) {
      cVar4 = *(char *)(iVar15 + 0xa6e);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24f4);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0x97;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0x96;
      CString_Destruct();
      local_4._0_1_ = 0x95;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      cVar4 = *(char *)(iVar15 + 0xa6d);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24f5);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0x9a;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0x99;
      CString_Destruct();
      local_4._0_1_ = 0x98;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      cVar4 = *(char *)(iVar15 + 0xa6a);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x8548);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0x9d;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0x9c;
      CString_Destruct();
      local_4._0_1_ = 0x9b;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      cVar4 = *(char *)(iVar15 + 0xa70);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24f6);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xa0;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0x9f;
      CString_Destruct();
      local_4._0_1_ = 0x9e;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      cVar4 = *(char *)(iVar15 + 0xa6f);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24f7);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xa3;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xa2;
      CString_Destruct();
      local_4._0_1_ = 0xa1;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      cVar4 = *(char *)(iVar15 + 0xa72);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x5e31);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xa6;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xa5;
      CString_Destruct();
      local_4._0_1_ = 0xa4;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      local_f0 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xa7;
      uVar10 = IcewindMisc::GetSneakAttackDice();
      uVar10 = IcewindMisc::GetSneakAttackRolls(local_104,uVar10);
      CString_Format(&local_f0,s__dd_d_008b2e60,uVar10);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x6142);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xaa;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a6a68,local_f0);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xa9;
      CString_Destruct();
      local_4._0_1_ = 0xa8;
      CString_Destruct();
      local_4._0_1_ = 0xa7;
      CString_Destruct();
      uVar10 = CVidInf::GetType(local_104);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x6141);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xad;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a64c4,uVar10);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xac;
      CString_Destruct();
      local_4._0_1_ = 0xab;
      CString_Destruct();
      local_4._0_1_ = 0xa7;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    iVar19 = CAIObjectType::IsClassValid(2);
    if (iVar19 != 0) {
      cVar4 = *(char *)(iVar15 + 0xa6f);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24f7);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xb0;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xaf;
      CString_Destruct();
      local_4._0_1_ = 0xae;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      cVar4 = *(char *)(iVar15 + 0xa72);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x5e31);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xb3;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,(int)cVar4);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xb2;
      CString_Destruct();
      local_4._0_1_ = 0xb1;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    iVar19 = CAIObjectType::IsClassValid(8);
    if (iVar19 != 0) {
      uVar10 = Average((int)*(char *)(iVar15 + 0xa6d),(int)*(char *)(iVar15 + 0xa6a));
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x8559);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xb6;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008abd0c,uVar10);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xb5;
      CString_Destruct();
      local_4._0_1_ = 0xb4;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    iVar19 = CAIObjectType::IsClassValid(7);
    if (iVar19 != 0) {
      sVar6 = *(short *)(iVar15 + 0x9b0);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2f5e);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xb9;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xb8;
      CString_Destruct();
      local_4._0_1_ = 0xb7;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
      sVar6 = *(short *)(iVar15 + 0x9b4);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2f5f);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xbc;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xbb;
      CString_Destruct();
      local_4._0_1_ = 0xba;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    iVar19 = CAIObjectType::IsClassValid(3);
    if (iVar19 != 0) {
      sVar6 = *(short *)(iVar15 + 0x9b0);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2f5e);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xbf;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a64c4,(int)sVar6);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xbe;
      CString_Destruct();
      local_4._0_1_ = 0xbd;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    if (*(int *)(local_104 + 0x402) == 0) {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&iStack_dc,0x42c5);
      local_4._0_1_ = 0xc0;
      puVar11 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x81e);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xc3;
      CString_Format(&local_110,&DAT_008a6a68,*puVar11);
      CString_Format(&local_114,&DAT_008a6a68,*puVar8);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xc2;
      CString_Destruct();
      local_4._0_1_ = 0xc1;
      CString_Destruct();
      local_4._0_1_ = 0xc0;
      CString_Destruct();
    }
    else {
      fStack_10c = **(float **)(local_104 + 0x402);
      fStack_108 = (*(float **)(local_104 + 0x402))[1];
      CResRef::CopyToString(&local_88);
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x81e);
      local_110 = PTR_DAT_008c1758;
      local_114 = PTR_DAT_008c1758;
      local_4._0_1_ = 0xc6;
      CString_Format(&local_110,&DAT_008a6a68,*puVar8);
      CString_Format(&local_114,&DAT_008a6a68,local_88);
      uVar10 = FUN_005ef760(&local_110,&local_114,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xc5;
      CString_Destruct();
      local_4._0_1_ = 0xc4;
      CString_Destruct();
    }
    local_4 = CONCAT31(local_4._1_3_,0x4c);
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    iVar19 = CAIObjectType::IsClassValid(8);
    if (iVar19 != 0) {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3e6e);
      FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
      CString_Destruct();
      iVar19 = -0x253 - (int)local_100;
      pcVar14 = (char *)((int)local_100 + 0x253);
      iStack_dc = iVar19;
      do {
        if (*pcVar14 != -1) {
          switch(*pcVar14) {
          case 'q':
            uVar10 = 0x3e67;
            break;
          default:
            uVar10 = 0xffffffff;
            break;
          case 'v':
            uVar10 = 0x92f1;
            break;
          case -0x67:
            uVar10 = 0x3d;
            break;
          case -0x65:
            uVar10 = 0x734e;
            break;
          case -99:
            uVar10 = 0xccb;
            break;
          case -0x60:
            uVar10 = 0xcc7;
            break;
          case -0x5f:
            uVar10 = 0xcc8;
            break;
          case -0x5b:
            uVar10 = 0xccc;
            break;
          case -0x5a:
            uVar10 = 0xccd;
            break;
          case -0x59:
            uVar10 = 0xcce;
            break;
          case -0x58:
            uVar10 = 0x7881;
            break;
          case -0x52:
            uVar10 = 0x92f0;
            break;
          case -0x4c:
            uVar10 = 0x6ece;
            break;
          case -0x4b:
            uVar10 = 0x92e6;
            break;
          case -0x4a:
            uVar10 = 0x92e7;
            break;
          case -0x41:
            uVar10 = 0x92e5;
          }
          puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,uVar10);
          FUN_007e89c1(iVar9,s__s_008b3300,*puVar8);
          CString_Destruct();
          iVar19 = iStack_dc;
        }
        pcVar14 = pcVar14 + 1;
      } while ((int)(pcVar14 + iVar19) < 8);
      FUN_007e8973(&DAT_008a6a50,iVar9);
    }
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x43e3);
    FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x93c);
    uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x43e4);
    local_4._0_1_ = 199;
    FUN_005ef9e0(iVar9,uVar10,(int)sVar6,1);
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x93e);
    uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x43e5);
    local_4._0_1_ = 200;
    FUN_005ef9e0(iVar9,uVar10,(int)sVar6,1);
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x940);
    uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x43e6);
    local_4._0_1_ = 0xc9;
    FUN_005ef9e0(iVar9,uVar10,(int)sVar6,1);
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    iVar19 = CRuleTables::GetTrapSenseBonus(puVar1);
    if (iVar19 != 0) {
      sVar6 = *(short *)(iVar15 + 0x93e);
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x9b7a);
      local_4._0_1_ = 0xca;
      FUN_005ef9e0(iVar9,uVar10,sVar6 + iVar19,1);
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x24fa);
    FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    CString_Destruct();
    fStack_10c = (float)FUN_0075e930(0);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xcb;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x221d);
      local_4._0_1_ = 0xcc;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xcb;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(1);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xcd;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3ea1);
      local_4._0_1_ = 0xce;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xcd;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(2);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xcf;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x24bb);
      local_4._0_1_ = 0xd0;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xcf;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(3);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xd1;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x24ba);
      local_4._0_1_ = 0xd2;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xd1;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(4);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xd3;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e91);
      local_4._0_1_ = 0xd4;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xd3;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(5);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xd5;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e8d);
      local_4._0_1_ = 0xd6;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xd5;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(6);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xd7;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e94);
      local_4._0_1_ = 0xd8;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xd7;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(7);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xd9;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e8f);
      local_4._0_1_ = 0xda;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xd9;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(8);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xdb;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e96);
      local_4._0_1_ = 0xdc;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xdb;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(9);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xdd;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e9f);
      local_4._0_1_ = 0xde;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xdd;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(10);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xdf;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x3e8b);
      local_4._0_1_ = 0xe0;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xdf;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(0xb);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xe1;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x21dc);
      local_4._0_1_ = 0xe2;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xe1;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(0xc);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xe3;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x221c);
      local_4._0_1_ = 0xe4;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xe3;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(0xd);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xe5;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x79a2);
      local_4._0_1_ = 0xe6;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xe5;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(0xe);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xe7;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x799f);
      local_4._0_1_ = 0xe8;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xe7;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(0xf);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xe9;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x79a0);
      local_4._0_1_ = 0xea;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xe9;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    fStack_10c = (float)FUN_0075e930(0x10);
    if ((0 < (int)fStack_10c) && ((int)fStack_10c < 0x14)) {
      puVar8 = auStack_b8;
      for (uVar13 = (uint)fStack_10c >> 2; uVar13 != 0; uVar13 = uVar13 - 1) {
        *puVar8 = 0x2b2b2b2b;
        puVar8 = puVar8 + 1;
      }
      for (uVar13 = (uint)fStack_10c & 3; uVar13 != 0; uVar13 = uVar13 - 1) {
        *(undefined1 *)puVar8 = 0x2b;
        puVar8 = (undefined4 *)((int)puVar8 + 1);
      }
      *(undefined1 *)((int)auStack_b8 + (int)fStack_10c) = 0;
      uVar10 = CString_Construct(auStack_b8);
      local_4._0_1_ = 0xeb;
      uVar7 = CBaldurEngine::FetchString(&fStack_10c,0x79a1);
      local_4._0_1_ = 0xec;
      FUN_005ef920(iVar9,uVar7,uVar10,1);
      local_4._0_1_ = 0xeb;
      CString_Destruct();
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    if ((((*(short *)(iVar15 + 0x92e) != 0) || (*(short *)(iVar15 + 0x930) != 0)) ||
        (*(short *)(iVar15 + 0x932) != 0)) || (*(short *)(iVar15 + 0x934) != 0)) {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df6);
      FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
      CString_Destruct();
      sVar6 = *(short *)(iVar15 + 0x92e);
      if (sVar6 != 0) {
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2dfa);
        local_114 = PTR_DAT_008c1758;
        local_100 = (int *)PTR_DAT_008c1758;
        local_4._0_1_ = 0xef;
        CString_Format(&local_114,&DAT_008a6a68,*puVar8);
        CString_Format(&local_100,&PTR_DAT_008abd14,(int)sVar6);
        uVar10 = FUN_005ef760(&local_114,&local_100,0,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0xee;
        CString_Destruct();
        local_4._0_1_ = 0xed;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      sVar6 = *(short *)(iVar15 + 0x930);
      if (sVar6 != 0) {
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df7);
        local_114 = PTR_DAT_008c1758;
        local_100 = (int *)PTR_DAT_008c1758;
        local_4._0_1_ = 0xf2;
        CString_Format(&local_114,&DAT_008a6a68,*puVar8);
        CString_Format(&local_100,&PTR_DAT_008abd14,(int)sVar6);
        uVar10 = FUN_005ef760(&local_114,&local_100,0,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0xf1;
        CString_Destruct();
        local_4._0_1_ = 0xf0;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      sVar6 = *(short *)(iVar15 + 0x932);
      if (sVar6 != 0) {
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df9);
        local_114 = PTR_DAT_008c1758;
        local_100 = (int *)PTR_DAT_008c1758;
        local_4._0_1_ = 0xf5;
        CString_Format(&local_114,&DAT_008a6a68,*puVar8);
        CString_Format(&local_100,&PTR_DAT_008abd14,(int)sVar6);
        uVar10 = FUN_005ef760(&local_114,&local_100,0,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0xf4;
        CString_Destruct();
        local_4._0_1_ = 0xf3;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      sVar6 = *(short *)(iVar15 + 0x934);
      if (sVar6 != 0) {
        puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df8);
        local_114 = PTR_DAT_008c1758;
        local_100 = (int *)PTR_DAT_008c1758;
        local_4._0_1_ = 0xf8;
        CString_Format(&local_114,&DAT_008a6a68,*puVar8);
        CString_Format(&local_100,&PTR_DAT_008abd14,(int)sVar6);
        uVar10 = FUN_005ef760(&local_114,&local_100,0,iVar9);
        FUN_007e8973(uVar10);
        local_4._0_1_ = 0xf7;
        CString_Destruct();
        local_4._0_1_ = 0xf6;
        CString_Destruct();
        local_4._0_1_ = 0x4c;
        CString_Destruct();
      }
      FUN_007e8973(&DAT_008a6a50,iVar9);
    }
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x284b);
    FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    CString_Destruct();
    local_f8 = (undefined4 *)(int)*(short *)(iVar15 + 0x974);
    if (((*(short *)(local_d0 + 0x720) < 1) || ((int)*(short *)(local_d0 + 0x722) <= (int)local_f8))
       || ((int)local_f8 < 0)) {
      puVar8 = (undefined4 *)(local_d0 + 0x71c);
    }
    else {
      puVar8 = (undefined4 *)
               (*(int *)(local_d0 + 0x718) + (int)*(short *)(local_d0 + 0x720) * (int)local_f8 * 4);
    }
    iVar19 = ToNumber(*puVar8);
    iStack_c0 = iVar19;
    if (iVar19 == 0) {
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x285c);
      local_4._0_1_ = 0xfa;
      FUN_005ef9e0(iVar9,uVar10,0,1);
    }
    else {
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x285c);
      local_4._0_1_ = 0xf9;
      FUN_005efaa0(iVar9,uVar10,&PTR_DAT_008abd14,iVar19,1);
    }
    local_4._0_1_ = 0x4c;
    CString_Destruct();
    sVar6 = CGameSprite::GetCriticalHitBonus();
    if (sVar6 != 0) {
      sVar6 = CGameSprite::GetCriticalHitBonus();
      if (sVar6 == 0) {
        uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x404e);
        local_4._0_1_ = 0xfc;
        FUN_005ef9e0(iVar9,uVar10,0,1);
      }
      else {
        uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x404e);
        local_4._0_1_ = 0xfb;
        FUN_005efaa0(iVar9,uVar10,&PTR_DAT_008abd14,(int)sVar6,1);
      }
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    sVar6 = *(short *)(iVar15 + 0x9e6);
    if (sVar6 != 0) {
      if (sVar6 == 0) {
        uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x6ec5);
        local_4._0_1_ = 0xfe;
        FUN_005ef9e0(iVar9,uVar10,0,1);
      }
      else {
        uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x6ec5);
        local_4._0_1_ = 0xfd;
        FUN_005efaa0(iVar9,uVar10,&PTR_DAT_008abd14,(int)sVar6,1);
      }
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    sVar6 = *(short *)(iVar15 + 0x926);
    if (sVar6 != 0) {
      if (sVar6 == 0) {
        uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x6ed5);
        local_4 = 0x100;
        FUN_005ef9e0(iVar9,uVar10,0,1);
        local_4._1_3_ = 0;
      }
      else {
        uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x6ed5);
        local_4._0_1_ = 0xff;
        FUN_005efaa0(iVar9,uVar10,&PTR_DAT_008abd14,(int)sVar6,1);
      }
      local_4._0_1_ = 0x4c;
      CString_Destruct();
    }
    local_f8 = (undefined4 *)(int)*(short *)(iVar15 + 0x974);
    if (((*(short *)(local_d0 + 0x720) < 2) || ((int)*(short *)(local_d0 + 0x722) <= (int)local_f8))
       || ((int)local_f8 < 0)) {
      puVar8 = (undefined4 *)(local_d0 + 0x71c);
    }
    else {
      puVar8 = (undefined4 *)
               (*(int *)(local_d0 + 0x718) +
               ((int)*(short *)(local_d0 + 0x720) * (int)local_f8 + 1) * 4);
    }
    iVar19 = ToNumber(*puVar8);
    iStack_c0 = iVar19;
    if (iVar19 == 0) {
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x2860);
      local_4 = 0x102;
      FUN_005ef9e0(iVar9,uVar10,0,1);
    }
    else {
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x2860);
      local_4 = 0x101;
      FUN_005efaa0(iVar9,uVar10,&PTR_DAT_008abd14,iVar19,1);
    }
    local_4 = 0x4c;
    CString_Destruct();
    local_f8 = (undefined4 *)(int)*(short *)(iVar15 + 0x974);
    if (((*(short *)(local_d0 + 0x720) < 4) || ((int)*(short *)(local_d0 + 0x722) <= (int)local_f8))
       || ((int)local_f8 < 0)) {
      puVar8 = (undefined4 *)(local_d0 + 0x71c);
    }
    else {
      puVar8 = (undefined4 *)
               (*(int *)(local_d0 + 0x718) +
               ((int)*(short *)(local_d0 + 0x720) * (int)local_f8 + 3) * 4);
    }
    iStack_c0 = ToNumber(*puVar8);
    CRuleTables::GetEncumbranceMod(local_104);
    fStack_10c = (float)iStack_c0;
    iVar19 = __ftol();
    if (iVar19 == 0) {
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x2862);
      local_4 = 0x106;
      FUN_005ef9e0(iVar9,uVar10,0,1);
    }
    else {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2862);
      local_114 = PTR_DAT_008c1758;
      local_100 = (int *)PTR_DAT_008c1758;
      local_4._0_1_ = 5;
      local_4._1_3_ = 1;
      CString_Format(&local_114,&DAT_008a6a68,*puVar8);
      CString_Format(&local_100,&PTR_DAT_008abd14,iVar19);
      uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 4;
      CString_Destruct();
      local_4 = CONCAT31(local_4._1_3_,3);
      CString_Destruct();
    }
    local_4 = 0x4c;
    CString_Destruct();
    fStack_10c = (float)CONCAT31(fStack_10c._1_3_,*(undefined1 *)(local_c4 + 8));
    FUN_00542ec0(fStack_10c,*(undefined1 *)(iVar15 + 0x97c),&iStack_c0,auStack_10);
    iVar19 = iStack_c0;
    if (iStack_c0 == 0) {
      uVar10 = CBaldurEngine::FetchString(&fStack_10c,0x2866);
      local_4 = 0x10a;
      FUN_005ef9e0(iVar9,uVar10,0,1);
    }
    else {
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2866);
      local_114 = PTR_DAT_008c1758;
      local_100 = (int *)PTR_DAT_008c1758;
      local_4._0_1_ = 9;
      local_4._1_3_ = 1;
      CString_Format(&local_114,&DAT_008a6a68,*puVar8);
      CString_Format(&local_100,&PTR_DAT_008abd14,iVar19);
      uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 8;
      CString_Destruct();
      local_4 = CONCAT31(local_4._1_3_,7);
      CString_Destruct();
    }
    local_4 = 0x4c;
    CString_Destruct();
    iVar19 = local_c4;
    iVar12 = CAIObjectType::IsClassValid(0xb);
    if (iVar12 != 0) {
      local_f8 = (undefined4 *)(int)*(short *)(iVar15 + 0x976);
      if (((*(short *)(local_d0 + 0x984) < 1) ||
          ((int)*(short *)(local_d0 + 0x986) <= (int)local_f8)) || ((int)local_f8 < 0)) {
        puVar8 = (undefined4 *)(local_d0 + 0x980);
      }
      else {
        puVar8 = (undefined4 *)
                 (*(int *)(local_d0 + 0x97c) + (int)*(short *)(local_d0 + 0x984) * (int)local_f8 * 4
                 );
      }
      iVar19 = ToNumber(*puVar8);
      iStack_c0 = iVar19;
      puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2867);
      local_114 = PTR_DAT_008c1758;
      local_100 = (int *)PTR_DAT_008c1758;
      local_4._0_1_ = 0xd;
      local_4._1_3_ = 1;
      CString_Format(&local_114,&DAT_008a6a68,*puVar8);
      CString_Format(&local_100,&DAT_008abd0c,iVar19);
      uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
      FUN_007e8973(uVar10);
      local_4._0_1_ = 0xc;
      CString_Destruct();
      local_4 = CONCAT31(local_4._1_3_,0xb);
      CString_Destruct();
      local_4 = 0x4c;
      CString_Destruct();
      iVar19 = local_c4;
    }
    iVar12 = CAIObjectType::IsClassValid(2);
    if ((((iVar12 != 0) || (iVar12 = CAIObjectType::IsClassValid(3), iVar12 != 0)) ||
        ((iVar12 = CAIObjectType::IsClassValid(4), iVar12 != 0 ||
         (((iVar12 = CAIObjectType::IsClassValid(7), iVar12 != 0 ||
           (iVar12 = CAIObjectType::IsClassValid(8), iVar12 != 0)) ||
          (iVar12 = CAIObjectType::IsClassValid(10), iVar12 != 0)))))) ||
       (iVar12 = CAIObjectType::IsClassValid(0xb), iVar12 != 0)) {
      local_fc = (undefined *)0x0;
      do {
        uVar5 = CInfGame::GetSpellcasterClass(&local_fc);
        local_f8 = (undefined4 *)CONCAT31(local_f8._1_3_,uVar5);
        uVar13 = 0;
        switch(local_fc) {
        case (undefined *)0x0:
          uVar13 = 2;
          break;
        case (undefined *)0x1:
          uVar13 = 4;
          break;
        case (undefined *)0x2:
          uVar13 = 8;
          break;
        case (undefined *)0x3:
          uVar13 = 0x40;
          break;
        case (undefined *)0x4:
          uVar13 = 0x80;
          break;
        case (undefined *)0x5:
          uVar13 = 0x200;
          break;
        case (undefined *)0x6:
          uVar13 = 0x400;
        }
        if (((*(uint *)(iVar19 + 0x18) & uVar13) != 0) &&
           (CRuleTables::GetMaxKnownSpells
                      (&local_f8,iVar19,puVar1,*(undefined4 *)(local_104 + 0x80c),1,&iStack_c0),
           0 < iStack_c0)) {
          local_110 = PTR_DAT_008c1758;
          local_4 = 0x10e;
          CRuleTables::GetClassStringMixed
                    (local_f8,*(undefined4 *)(local_104 + 0x80c),*(undefined4 *)(local_104 + 0x5ac),
                     &local_110,*(char *)(local_c4 + 0x14) != '\x02');
          puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2868);
          FUN_007e89c1(iVar9,s__s___s_008b32f4,*puVar8,local_110);
          CString_Destruct();
          iVar19 = 1;
          do {
            if (iStack_c0 < 1) break;
            CString_Format(&local_a0,&DAT_008a64c4,iVar19);
            puVar16 = local_a0;
            CMapStringToString_operator__(DAT_008f3324);
            CString_operator_equ(puVar16);
            iStack_dc = iStack_c0;
            if (iStack_c0 == 0) {
              uVar10 = CBaldurEngine::FetchString(&local_f4,0x2869);
              local_4._0_1_ = 0x12;
              FUN_005ef9e0(iVar9,uVar10,0,1);
            }
            else {
              puVar8 = (undefined4 *)CBaldurEngine::FetchString(&local_f0,0x2869);
              local_114 = PTR_DAT_008c1758;
              local_100 = (int *)PTR_DAT_008c1758;
              local_4._0_1_ = 0x11;
              CString_Format(&local_114,&DAT_008a6a68,*puVar8);
              CString_Format(&local_100,&PTR_DAT_008abd14,iStack_dc);
              uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
              FUN_007e8973(uVar10);
              local_4._0_1_ = 0x10;
              CString_Destruct();
              local_4._0_1_ = 0xf;
              CString_Destruct();
            }
            local_4 = CONCAT31(local_4._1_3_,0xe);
            CString_Destruct();
            cStack_e9 = *(char *)(local_c4 + 8);
            iVar19 = iVar19 + 1;
            CRuleTables::GetMaxKnownSpells
                      (&cStack_e9,local_c4,puVar1,*(undefined4 *)(local_104 + 0x80c),iVar19,
                       &iStack_c0);
          } while (iVar19 < 10);
          local_4 = 0x4c;
          CString_Destruct();
          iVar19 = local_c4;
        }
        local_fc = local_fc + 1;
      } while (local_fc < (undefined *)0x7);
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3cb8);
    FUN_007e89c1(iVar9,&DAT_008b3318,*puVar8);
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x94a);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3cdd);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x15;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x14;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x13);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x952);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2dfa);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x18;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x17;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x16);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x954);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df9);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x1b;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x1a;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x19);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x950);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df8);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x1e;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x1d;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x1c);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x948);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3cda);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x21;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x20;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x1f);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x944);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3cba);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x24;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x23;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x22);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x946);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3cd9);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x27;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x26;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x25);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x942);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x3cb9);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x2a;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x29;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x28);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x956);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2df7);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x2d;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x2c;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x2b);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    sVar6 = *(short *)(iVar15 + 0x9da);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x94f7);
    local_114 = PTR_DAT_008c1758;
    local_100 = (int *)PTR_DAT_008c1758;
    local_4._0_1_ = 0x30;
    local_4._1_3_ = 1;
    CString_Format(&local_114,&DAT_008a6a68,*puVar8);
    CString_Format(&local_100,&DAT_008a64c4,(int)sVar6);
    uVar10 = FUN_005ef760(&local_114,&local_100,1,iVar9);
    FUN_007e8973(uVar10);
    local_4._0_1_ = 0x2f;
    CString_Destruct();
    local_4 = CONCAT31(local_4._1_3_,0x2e);
    CString_Destruct();
    local_4 = 0x4c;
    CString_Destruct();
    FUN_007e8973(&DAT_008a6a50,iVar9);
    FUN_007e8973(&DAT_008a6a50,iVar9);
    uStack_90 = 0;
    do {
      cStack_e9 = (char)uStack_90;
      iVar15 = CGameSprite::GetSpells(&cStack_e9);
      if (*(int *)(iVar15 + 0xfc) != 0) {
        uVar7 = 0;
        uVar10 = CInfGame::GetSpellcasterClass(&uStack_90);
        puVar8 = (undefined4 *)CRuleTables::GetClassString(&fStack_10c,uVar10,uVar7);
        FUN_007e89c1(iVar9,s__s_Spells__008b32e8,*puVar8);
        CString_Destruct();
        local_114 = (undefined *)0x0;
        local_f8 = (undefined4 *)0x0;
        do {
          puVar8 = local_f8;
          cStack_e9 = (char)uStack_90;
          iVar15 = CGameSprite::GetSpells(&cStack_e9);
          if ((undefined4 *)0xfb < puVar8) {
            CUtil::UtilAssert(0x9d9,s___Include_FileFormat_h_008a7db4,
                              s_nLevel_<_CSPELLLIST_MAX_LEVELS_008a7dcc,0);
          }
          iVar19 = *(int *)(iVar15 + 4 + (int)puVar8);
          if ((iVar19 != 0) && (*(int *)(iVar15 + 8 + (int)puVar8) - iVar19 >> 4 != 0)) {
            FUN_007e89c1(iVar9,s_Level___1d_008b32dc,local_114);
            local_fc = (undefined *)0x0;
            local_100 = (int *)0x0;
            while( true ) {
              cStack_e9 = (char)uStack_90;
              iVar15 = CGameSprite::GetSpells(&cStack_e9);
              if ((undefined4 *)0xfb < puVar8) {
                CUtil::UtilAssert(0x9d9,s___Include_FileFormat_h_008a7db4,
                                  s_nLevel_<_CSPELLLIST_MAX_LEVELS_008a7dcc,0);
              }
              iVar19 = *(int *)(iVar15 + 4 + (int)puVar8);
              if ((iVar19 == 0) ||
                 ((undefined *)(*(int *)(iVar15 + 8 + (int)puVar8) - iVar19 >> 4) <= local_fc))
              break;
              cStack_e9 = (char)uStack_90;
              iVar15 = CGameSprite::GetSpells(&cStack_e9);
              if ((undefined4 *)0xfb < puVar8) {
                CUtil::UtilAssert(0x9d9,s___Include_FileFormat_h_008a7db4,
                                  s_nLevel_<_CSPELLLIST_MAX_LEVELS_008a7dcc,0);
              }
              iVar19 = *(int *)((int)puVar8 + iVar15 + 4);
              piVar18 = (int *)((int)puVar8 + iVar15 + 4);
              if ((iVar19 == 0) || (*(int *)((int)puVar8 + iVar15 + 8) - iVar19 >> 4 == 0)) {
                CUtil::UtilAssert(0x985,s___Include_FileFormat_h_008a7db4,
                                  s_m_List_size___>_0_008a7e04,0);
              }
              iVar19 = *piVar18;
              if ((iVar19 == 0) ||
                 ((undefined *)(*(int *)((int)puVar8 + iVar15 + 8) - iVar19 >> 4) <= local_fc)) {
                CUtil::UtilAssert(0x986,s___Include_FileFormat_h_008a7db4,
                                  s_nIndex_<_m_List_size()_008a7dec,0);
              }
              piVar3 = local_100;
              iVar15 = *piVar18;
              uVar13 = *(uint *)((int)local_100 + iVar15);
              piVar18 = (int *)(DAT_008cf6dc[0x715] + 0x4bf8);
              if (*(uint *)(DAT_008cf6dc[0x715] + 0x4bfc) <= uVar13) {
                CUtil::UtilAssert(0x28c,s___Include_InfGame_h_008aa854,s_nIndex_<_m_nCount_008aa868,
                                  0);
              }
              if (*piVar18 == 0) {
                CUtil::UtilAssert(0x28d,s___Include_InfGame_h_008aa854,s_m_pList____NULL_008aa844,0)
                ;
              }
              iVar19 = *piVar18;
              iStack_dc = *(int *)(iVar19 + uVar13 * 8);
              iStack_d8 = *(int *)(iVar19 + 4 + uVar13 * 8);
              uVar10 = CResRef::GetResRef(*(undefined4 *)((int)piVar3 + iVar15 + 8),
                                          *(undefined4 *)((int)piVar3 + iVar15 + 4));
              FUN_007e89c1(iVar9,s____s___3d__3d__008b32c8,8,uVar10);
              local_fc = local_fc + 1;
              local_100 = local_100 + 4;
              puVar8 = local_f8;
            }
          }
          local_f8 = puVar8 + 7;
          local_114 = local_114 + 1;
        } while (local_f8 < (undefined4 *)0xfc);
      }
      uStack_90 = uStack_90 + 1;
    } while (uStack_90 < 7);
    iVar15 = local_104;
    if (*(int *)(local_104 + 0x4a80) != 0) {
      FUN_007e89c1(iVar9,s_Domain_Spells__008b32b8);
      local_114 = (undefined *)0x0;
      local_f8 = (undefined4 *)0x0;
      iVar19 = local_104;
      do {
        puVar8 = local_f8;
        iVar15 = iVar19;
        if ((undefined4 *)0xfb < local_f8) {
          CUtil::UtilAssert(0x9d9,s___Include_FileFormat_h_008a7db4,
                            s_nLevel_<_CSPELLLIST_MAX_LEVELS_008a7dcc,0);
          iVar15 = local_104;
        }
        iVar12 = *(int *)(iVar19 + 0x4988 + (int)puVar8);
        if ((iVar12 != 0) && (*(int *)(iVar19 + 0x498c + (int)puVar8) - iVar12 >> 4 != 0)) {
          FUN_007e89c1(iVar9,s_Level___1d_008b32dc,local_114);
          local_fc = (undefined *)0x0;
          local_100 = (int *)0x0;
          while( true ) {
            iVar19 = local_104;
            if ((undefined4 *)0xfb < puVar8) {
              CUtil::UtilAssert(0x9d9,s___Include_FileFormat_h_008a7db4,
                                s_nLevel_<_CSPELLLIST_MAX_LEVELS_008a7dcc,0);
            }
            iVar15 = local_104;
            iVar12 = *(int *)(iVar19 + 0x4988 + (int)puVar8);
            if ((iVar12 == 0) ||
               ((undefined *)(*(int *)(iVar19 + 0x498c + (int)puVar8) - iVar12 >> 4) <= local_fc))
            break;
            if ((undefined4 *)0xfb < puVar8) {
              CUtil::UtilAssert(0x9d9,s___Include_FileFormat_h_008a7db4,
                                s_nLevel_<_CSPELLLIST_MAX_LEVELS_008a7dcc,0);
            }
            iVar19 = *(int *)((int)puVar8 + iVar15 + 0x4988);
            piVar18 = (int *)((int)puVar8 + iVar15 + 0x4988);
            if ((iVar19 == 0) || (*(int *)((int)puVar8 + iVar15 + 0x498c) - iVar19 >> 4 == 0)) {
              CUtil::UtilAssert(0x985,s___Include_FileFormat_h_008a7db4,s_m_List_size___>_0_008a7e04
                                ,0);
            }
            iVar19 = *piVar18;
            if ((iVar19 == 0) ||
               ((undefined *)(*(int *)((int)puVar8 + iVar15 + 0x498c) - iVar19 >> 4) <= local_fc)) {
              CUtil::UtilAssert(0x986,s___Include_FileFormat_h_008a7db4,
                                s_nIndex_<_m_List_size()_008a7dec,0);
            }
            piVar3 = local_100;
            iVar15 = *piVar18;
            uVar13 = *(uint *)((int)local_100 + iVar15);
            piVar18 = (int *)(DAT_008cf6dc[0x715] + 0x4bf8);
            if (*(uint *)(DAT_008cf6dc[0x715] + 0x4bfc) <= uVar13) {
              CUtil::UtilAssert(0x28c,s___Include_InfGame_h_008aa854,s_nIndex_<_m_nCount_008aa868,0)
              ;
            }
            if (*piVar18 == 0) {
              CUtil::UtilAssert(0x28d,s___Include_InfGame_h_008aa854,s_m_pList____NULL_008aa844,0);
            }
            iVar19 = *piVar18;
            fStack_10c = *(float *)(iVar19 + uVar13 * 8);
            fStack_108 = *(float *)(iVar19 + 4 + uVar13 * 8);
            uVar10 = CResRef::GetResRef(*(undefined4 *)((int)piVar3 + iVar15 + 8),
                                        *(undefined4 *)((int)piVar3 + iVar15 + 4));
            FUN_007e89c1(iVar9,s____s___3d__3d__008b32c8,8,uVar10);
            local_fc = local_fc + 1;
            local_100 = local_100 + 4;
            puVar8 = local_f8;
          }
        }
        local_f8 = puVar8 + 7;
        local_114 = local_114 + 1;
        iVar19 = iVar15;
      } while (local_f8 < (undefined4 *)0xfc);
    }
    if ((*(int *)(iVar15 + 0x4a88) != 0) &&
       (*(int *)(iVar15 + 0x4a8c) - *(int *)(iVar15 + 0x4a88) >> 4 != 0)) {
      local_e0 = (undefined *)0x0;
      CGameSprite::GetInnateSpells();
      puVar16 = (undefined *)CSpellListArray_GetSize();
      if (local_e0 < puVar16) {
        do {
          ppuVar21 = &local_e0;
          CGameSprite::GetInnateSpells(ppuVar21);
          FUN_005f6710(ppuVar21);
          fStack_10c = (float)FUN_0045b240();
          pfVar22 = &fStack_10c;
          CBaldurChitin::GetObjectGame(pfVar22);
          CInfGame::GetInnateSpells();
          piVar18 = (int *)CSpellResRefList::Get(pfVar22);
          iStack_dc = *piVar18;
          iStack_d8 = piVar18[1];
          uVar10 = FUN_0045b210();
          uVar10 = CVidMode::GetPointerNumber(uVar10);
          uVar10 = CResRef::GetResRef(uVar10);
          FUN_007e89c1(iVar9,s____s___3d__3d__008b32c8,8,uVar10);
          local_e0 = local_e0 + 1;
          CGameSprite::GetInnateSpells();
          puVar16 = (undefined *)CSpellListArray_GetSize();
        } while (local_e0 < puVar16);
      }
    }
    CGameSprite::GetSongs();
    iVar15 = CSpellListArray_GetSize();
    if (iVar15 != 0) {
      local_e0 = (undefined *)0x0;
      CGameSprite::GetSongs();
      puVar16 = (undefined *)CSpellListArray_GetSize();
      if (local_e0 < puVar16) {
        do {
          ppuVar21 = &local_e0;
          CGameSprite::GetSongs(ppuVar21);
          FUN_005f6710(ppuVar21);
          fStack_10c = (float)FUN_0045b240();
          pfVar22 = &fStack_10c;
          CBaldurChitin::GetObjectGame(pfVar22);
          CInfGame::GetMasterSongLookup();
          piVar18 = (int *)CSpellResRefList::Get(pfVar22);
          iStack_dc = *piVar18;
          iStack_d8 = piVar18[1];
          uVar10 = FUN_0045b210();
          uVar10 = CVidMode::GetPointerNumber(uVar10);
          uVar10 = CResRef::GetResRef(uVar10);
          FUN_007e89c1(iVar9,s____s___3d__3d__008b32c8,8,uVar10);
          local_e0 = local_e0 + 1;
          CGameSprite::GetSongs();
          puVar16 = (undefined *)CSpellListArray_GetSize();
        } while (local_e0 < puVar16);
      }
    }
    CGameSprite::GetShapeshifts();
    iVar15 = CSpellListArray_GetSize();
    if (iVar15 != 0) {
      local_e0 = (undefined *)0x0;
      CGameSprite::GetShapeshifts();
      puVar16 = (undefined *)CSpellListArray_GetSize();
      if (local_e0 < puVar16) {
        do {
          ppuVar21 = &local_e0;
          CGameSprite::GetShapeshifts(ppuVar21);
          FUN_005f6710(ppuVar21);
          fStack_10c = (float)FUN_0045b240();
          pfVar22 = &fStack_10c;
          CBaldurChitin::GetObjectGame(pfVar22);
          CInfGame::GetShapeshifts();
          piVar18 = (int *)CSpellResRefList::Get(pfVar22);
          iStack_dc = *piVar18;
          iStack_d8 = piVar18[1];
          uVar10 = FUN_0045b210();
          uVar10 = CVidMode::GetPointerNumber(uVar10);
          uVar10 = CResRef::GetResRef(uVar10);
          FUN_007e89c1(iVar9,s____s___3d__3d__008b32c8,8,uVar10);
          local_e0 = local_e0 + 1;
          CGameSprite::GetShapeshifts();
          puVar16 = (undefined *)CSpellListArray_GetSize();
        } while (local_e0 < puVar16);
      }
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    FUN_007e8973(&DAT_008a6a50,iVar9);
    FUN_007e8973(&DAT_008a6a50,iVar9);
    lpFileName = (LPCSTR)(**(code **)(*DAT_008cf6dc + 0xa0))();
    UVar17 = GetPrivateProfileIntA
                       (s_Game_Options_008a7c40,s_Output_Full_Detailed_Description_008b3294,0,
                        lpFileName);
    iStack_dc = CONCAT31(iStack_dc._1_3_,UVar17 != 0);
    piVar18 = (int *)CGameSprite::GetEquipment();
    puVar8 = &local_4c;
    for (iVar15 = 0xc; iVar15 != 0; iVar15 = iVar15 + -1) {
      *puVar8 = 0;
      puVar8 = puVar8 + 1;
    }
    *(undefined2 *)puVar8 = 0;
    *(undefined1 *)((int)puVar8 + 2) = 0;
    local_cc = piVar18;
    FUN_007e8973(s__________________________________008b3260,iVar9);
    puVar8 = (undefined4 *)CBaldurEngine::FetchString(&fStack_10c,0x2c1c);
    FUN_007e89c1(iVar9,s_____s__008b3254,*puVar8);
    CString_Destruct();
    FUN_007e8973(s__________________________________008b321c,iVar9);
    iVar15 = iStack_dc;
    if (piVar18[6] != 0) {
      FUN_005efcc0(iVar9,piVar18[6],1,iStack_dc);
      uStack_46 = 1;
    }
    if (*piVar18 != 0) {
      FUN_005efcc0(iVar9,*piVar18,1,iVar15);
      local_4c._0_1_ = 1;
    }
    if (piVar18[1] != 0) {
      FUN_005efcc0(iVar9,piVar18[1],1,iVar15);
      local_4c._1_1_ = 1;
    }
    if (piVar18[4] != 0) {
      FUN_005efcc0(iVar9,piVar18[4],1,iVar15);
      uStack_48 = 1;
    }
    uVar13 = CGameSprite::sub_726800();
    if (piVar18[uVar13 & 0xff] != 0) {
      uVar13 = CGameSprite::sub_726800(1,iVar15);
      FUN_005efcc0(iVar9,piVar18[uVar13 & 0xff]);
      uVar13 = CGameSprite::sub_726800();
      *(undefined1 *)((int)&local_4c + (uVar13 & 0xff)) = 1;
    }
    if (piVar18[5] != 0) {
      FUN_005efcc0(iVar9,piVar18[5],1,iVar15);
      uStack_47 = 1;
    }
    if (piVar18[8] != 0) {
      FUN_005efcc0(iVar9,piVar18[8],1,iVar15);
      uStack_44 = CONCAT31(uStack_44._1_3_,1);
    }
    if (piVar18[7] != 0) {
      FUN_005efcc0(iVar9,piVar18[7],1,iVar15);
      uStack_45 = 1;
    }
    if (piVar18[2] != 0) {
      FUN_005efcc0(iVar9,piVar18[2],1,iVar15);
      local_4c._2_1_ = 1;
    }
    if (piVar18[3] != 0) {
      FUN_005efcc0(iVar9,piVar18[3],1,iVar15);
      local_4c._3_1_ = 1;
    }
    FUN_007e8973(&DAT_008a6a50,iVar9);
    iVar19 = 0;
    local_100 = local_cc + 0x2b;
    do {
      iVar12 = *local_100;
      if (iVar12 != 0) {
        CItem::Demand();
        FUN_005efcc0(iVar9,iVar12,(uint)*(byte *)(local_cc + 0x33) == iVar19 + 0x2bU,iVar15);
        auStack_21[iVar19] = 1;
        if (iVar12 != 0) {
          CItem::Release();
        }
      }
      iVar19 = iVar19 + 1;
      local_100 = local_100 + 1;
    } while (iVar19 < 8);
    FUN_007e8973(&DAT_008a6a50,iVar9);
    iVar19 = 0;
    local_100 = local_cc + 0xb;
    do {
      iVar12 = *local_100;
      if (iVar12 != 0) {
        CItem::Demand();
        FUN_005efcc0(iVar9,iVar12,(uint)*(byte *)(local_cc + 0x33) == iVar19 + 0xbU,iVar15);
        *(undefined1 *)((int)&uStack_44 + iVar19 + 3) = 1;
        if (iVar12 != 0) {
          CItem::Release();
        }
      }
      iVar19 = iVar19 + 1;
      local_100 = local_100 + 1;
    } while (iVar19 < 4);
    FUN_007e8973(&DAT_008a6a50,iVar9);
    iVar19 = 0xf;
    piVar18 = local_cc + 0xf;
    do {
      if (*piVar18 != 0) {
        CItem::Demand();
        FUN_005efcc0(iVar9,*piVar18,1,iVar15);
        CItem::Release();
        *(undefined1 *)((int)&local_4c + iVar19) = 1;
      }
      iVar19 = iVar19 + 1;
      piVar18 = piVar18 + 1;
    } while (iVar19 < 0x12);
    FUN_007e8973(&DAT_008a6a50,iVar9);
    iVar19 = 0;
    local_100 = local_cc;
    do {
      if ((iVar19 != 10) && (iVar12 = *local_100, iVar12 != 0)) {
        CItem::Demand();
        if (*(char *)((int)&local_4c + iVar19) == '\0') {
          FUN_005efcc0(iVar9,iVar12,0,iVar15);
        }
        CItem::Release();
      }
      iVar19 = iVar19 + 1;
      local_100 = local_100 + 1;
    } while (iVar19 < 0x33);
    FUN_007e89fd(iVar9);
    *param_2 = 1;
    uVar23 = 0xffffffff;
    uVar7 = 0;
    uVar10 = local_74;
    CBaldurChitin::GetObjectGame(local_74,0,0xffffffff);
    CInfGame::GetObjectArray();
    CGameObjectArray::ReleaseShare(uVar10,uVar7,uVar23);
    CString_Construct(local_7c);
    local_94 = local_94 | 1;
    local_4._0_1_ = 0x4b;
    CString_Destruct();
    local_4._0_1_ = 0x1c;
    CString_Destruct();
    local_4._0_1_ = 0x14;
    CString_Destruct();
    local_4._0_1_ = 0x13;
    CString_Destruct();
    local_4._0_1_ = 0xf;
    CString_Destruct();
  }
  local_4._0_1_ = 6;
  CString_Destruct();
  local_4._0_1_ = 4;
  CString_Destruct();
  local_4._0_1_ = 3;
  CString_Destruct();
LAB_005f6565:
  local_4._0_1_ = 2;
  CString_Destruct();
  local_4._0_1_ = 1;
  CString_Destruct();
  local_4 = (uint)local_4._1_3_ << 8;
  CString_Destruct();
  ExceptionList = local_c;
  return param_1;
}
```

## Callers

- 0x5E4560 CScreenCharacter::sub_5E4560 (call at 0x5E4582)

## Callees

- 0xAD KERNEL32.DLL::GetPrivateProfileIntA
- 0x40CB10 CBaldurChitin::GetObjectGame
- 0x40D7E0 CAIObjectType::IsClassValid
- 0x427B60 CBaldurEngine::FetchString
- 0x447390 CDerivedStats::GetXP
- 0x447990 CDerivedStats::GetClassLevel
- 0x453030 CInfGame::GetObjectArray
- 0x45B210 FUN_0045b210
- 0x45B240 FUN_0045b240
- 0x45B740 CGameSprite::GetEquipment
- 0x4E82B0 CItem::Demand
- 0x4E82F0 CItem::Release
- 0x4E9610 CItem::GetAbility
- 0x4E97E0 CItem::GetItemType
- 0x504630 CAbilityId::CAbilityId
- 0x5418E0 CRuleTables::GetTrapSenseBonus
- 0x541DA0 CRuleTables::GetClassString
- 0x542EC0 FUN_00542ec0
- 0x5431E0 CRuleTables::GetMaxKnownSpells
- 0x544120 CRuleTables::GetClassStringMixed
- 0x5446B0 CRuleTables::GetCharacterStateDescription
- 0x544DB0 CRuleTables::GetRaceStringMixed
- 0x5467E0 Average
- 0x546AA0 CRuleTables::GetEncumbranceMod
- 0x546B60 CRuleTables::sub_546B60
- 0x547020 CRuleTables::GetIterativeAttackBonus
- 0x585D50 IcewindMisc::GetSneakAttackRolls
- 0x585D80 IcewindMisc::GetSneakAttackDice
- 0x58FEC0 CInfGame::GetMasterSongLookup
- 0x58FED0 CGameSprite::GetShapeshifts
- 0x599A50 CGameObjectArray::GetShare
- 0x599E70 CGameObjectArray::ReleaseShare
- 0x5C09C0 CInfGame::GetDirSaveRoot
- 0x5CA730 CInfGame::GetSpellcasterClass
- 0x5EF760 FUN_005ef760
- 0x5EF920 FUN_005ef920
- 0x5EF9E0 FUN_005ef9e0
- 0x5EFAA0 FUN_005efaa0
- 0x5EFCC0 FUN_005efcc0
- 0x5F66F0 CSpellListArray_GetSize
- 0x5F6710 FUN_005f6710
- 0x5F6790 CSpellResRefList::Get
- 0x5F67F0 CInfGame::GetInnateSpells
- 0x5F6800 CInfGame::GetShapeshifts
- 0x5F6810 CGameSprite::GetInnateSpells
- 0x5F6820 CGameSprite::GetSongs
- 0x718650 CGameSprite::GetLauncher
- 0x71D4B0 CGameSprite::GetQuickWeapon
- 0x71F760 CGameSprite::GetName
- 0x723CC0 CGameSprite::GetAC
- 0x723EA0 CGameSprite_sub_723EA0
- 0x7240A0 CGameSprite::sub_7240A0
- 0x724170 CGameSprite::sub_724170
- 0x724730 CGameSprite::GetSpells
- 0x726270 CGameSprite::sub_726270
- 0x726800 CGameSprite::sub_726800
- 0x73B740 CGameSprite::GetCriticalHitBonus
- 0x73C7E0 CGameSprite::sub_73C7E0
- 0x73CA20 CGameSprite::sub_73CA20
- 0x73CAD0 CGameSprite::sub_73CAD0
- 0x73CAE0 CGameSprite::sub_73CAE0
- 0x73CB10 CGameSprite::sub_73CB10
- 0x73CC40 CGameSprite::sub_73CC40
- 0x73CF10 CGameSprite_B3AUTO_0073cf10
- 0x73D080 CGameSprite_B3AUTO_0073d080
- 0x74F830 CGameSprite::SelectWeaponAbility
- 0x75E930 FUN_0075e930
- 0x763150 CGameSprite::sub_763150
- 0x780C00 CUtil::UtilAssert
- 0x78A960 CResRef::CResRef
- 0x78A9F0 CResRef::CopyToString
- 0x78AA20 CResRef::GetResRef
- 0x78AC30 CResRef::operator=
- 0x78AD40 CResRef::operator=
- 0x799E80 CVidMode::GetPointerNumber
- 0x7ABB90 CVidInf::GetType
- 0x7E7B03 ToNumber
- 0x7E812C __ftol
- 0x7E8973 FUN_007e8973
- 0x7E89C1 FUN_007e89c1
- 0x7E89FD FUN_007e89fd
- 0x7E8AAB CInfGame_B3AUTO_007e8aab
- 0x7FAEE8 CString_Format
- 0x7FC1A5 CMapStringToString_operator[]
- 0x7FC98F CString_Construct
- 0x7FCC1A CString_Destruct
- 0x7FCC88 CString_Construct
- 0x7FCD07 CString_operator_equ
- 0x7FCD57 CString_operator_equ
- 0x7FCDFD CString_Concat
- 0x7FCE63 CString_Concat_With_CString_InPlace
- 0x7FCFAA CString_operator_plus_equ
- 0x7FCFE6 CString_operator_plus_equ
- 0x7FD114 CString_SetAt

## Notes

`CString CScreenCharacter::sub_5EFFD0(BYTE& bSuccess)`, new in round 2 of
the ui-misc stream, ported from the decompilation with the disassembly for
the dropped arguments: the text character sheet of the selected character,
<save root>\<CScreenCharacter::SAVE_NAME>\<name>.txt ("wt"; the name's
\ / : * ? < > | become _), returning "...\<SAVE_NAME>\<name>.txt " (with
the trailing space of 0x8B3320). In order: name, gender, race, alignment
(assert line 15582 for an unknown one; NOTE: the original then fetches an
empty CString's data pointer as the string id); the abilities; the armour
class and hit points; the portrait's state icons and DEAD; the combat feats
in use (NOTE: "%s: %s" and "%s: %s %d" without new lines, so they run
together, as the original); the classes and levels, experience; the attack
rolls (as UpdateEquipmentInformation 0x5DE2C0 but: others without 0x73D420,
the base line with every attack's bonus, the off hand's total with the
shield penalty, no magic-slot test for the attack count; asserts lines
16140, 16160, 16187); the armour class parts ("%d"); the skills (rogue,
bard, ranger, paladin, cleric) and the AI script; a ranger's favoured
enemies (plural strings); the saving throws and trap sense; the weapon
proficiencies (NOTE: CGameSprite 0x75E930, the rank, is `xor eax, eax;
ret 4` in the original, so no line is written; ours writes the header and
leaves the 17 calls out, the proficiencies' strings listed in a NOTE); the
armour class modifiers; the ability bonuses (STRMOD columns 0,
1 and 3; the weight allowance as the x87 computes it, in double; the hit
points per level through CRuleTables 0x542EC0; the chance to learn a spell
from INTMOD); the bonus spells per casting class (NOTE: the levels after the
first ask GetMaxKnownSpells with the AI type's class, 0x5F51DE, as the
original); the resistances; the spells; the items (worn, weapons, quiver,
quick items with a star on the selected, then the rest; Output Full
Detailed Description adds each description); fclose.

NOTE: the spell lists pass the list index (0-6) to CGameSprite::GetSpells,
which wants a class (0x5F5970). Index 0 is no class, so
CInfGame::GetSpellcasterIndex (0x5CA5D0) asserts (0x5CA6A4) and
CUtil::UtilAssert ends the game: the original never finishes the export.
Ours does the same (ruling P4-10: a visible original bug is kept). The
export cannot be started in the vanilla game: GUIREC's panel 2 has no
control 51 (CUIControlButtonCharacter5EAA30).

NOTE: the original inlines the bodies of 0x5EF920 and 0x5EF9E0 (two
CString formats and 0x5EF760) at most lines; ours calls them. The path goes
through compat::ResolvePath for the platform's file system. The inline
helpers Average (0x5467E0), the spell list accessors (0x5F66F0, 0x5F6710,
0x45B210, 0x45B240, 0x799E80) are written out.

Tests: tests/test_ui_misc.cpp "character screen: the text character
sheet up to the original's assert" (a child process runs it; its SIGABRT
handler flushes the file: the sheet's sections up to the resistances are
there, the inventory is not, and the child ended in the assert) and "...
without its folder" (the failure path).
