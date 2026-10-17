---
address: 0x4BBCB0
name: CGameEffect_opcode_141_2
exemption: "visual only; needs a sprite in an area; tested in tests/test_visual_projectiles.cpp [visualprojectile]"
kind: read-only
ghidra_version: 12.1.4
---

# 0x4BBCB0 CGameEffect_opcode_141_2

## Decompiled (Ghidra)

```c
undefined4 CGameEffect_opcode_141_2(int *param_1)

{
  uint3 uVar1;
  undefined2 extraout_var;
  int *piVar2;
  int iVar3;
  uint uVar4;
  undefined4 uVar5;
  undefined4 uVar6;
  undefined4 *puVar7;
  int in_ECX;
  undefined2 extraout_var_00;
  uint uVar8;
  int iVar9;
  int *piVar10;
  int iVar11;
  char *pcVar12;
  undefined4 uVar13;
  undefined *local_3c;
  undefined *local_38;
  undefined *local_34;
  uint local_30;
  undefined1 local_2c [4];
  undefined1 local_28 [4];
  undefined1 local_24 [4];
  undefined1 local_20 [4];
  undefined1 local_1c [4];
  undefined1 local_18 [4];
  undefined1 local_14 [4];
  int local_10;
  void *pvStack_c;
  undefined1 *puStack_8;
  int local_4;
  
  piVar2 = param_1;
  puStack_8 = &LAB_00813121;
  pvStack_c = ExceptionList;
  uVar8 = 0;
  piVar10 = (int *)0x0;
  local_30 = 0;
  local_34 = PTR_DAT_008c1758;
  local_38 = PTR_DAT_008c1758;
  local_3c = PTR_DAT_008c1758;
  local_4._1_3_ = 0;
  uVar1 = local_4._1_3_;
  local_4._0_1_ = 2;
  local_4._1_3_ = 0;
  ExceptionList = &pvStack_c;
  local_10 = in_ECX;
  if (*(int *)((int)param_1 + 0x12) == 0) goto LAB_004bbdc1;
  uVar4 = *(uint *)(in_ECX + 0x1c);
  switch(uVar4) {
  case 0x20:
    ExceptionList = &pvStack_c;
    iVar3 = operator_new(0x2a0);
    local_4._0_1_ = 3;
    if (iVar3 != 0) {
      piVar10 = (int *)FUN_00530300();
    }
    uVar5 = 0x43;
    break;
  case 0x21:
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    iVar3 = operator_new(0x2a0);
    local_4._0_1_ = 4;
    if (iVar3 != 0) {
      piVar10 = (int *)FUN_00530300();
    }
    uVar5 = 0x41;
    break;
  case 0x22:
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    iVar3 = operator_new(0x2a0);
    local_4._0_1_ = 5;
    if (iVar3 != 0) {
      piVar10 = (int *)FUN_00530300();
    }
    uVar5 = 0x45;
    break;
  case 0x23:
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    iVar3 = operator_new(0x29a);
    local_4._0_1_ = 6;
    if (iVar3 == 0) {
      piVar2 = (int *)0x0;
    }
    else {
      CString_Construct(&DAT_008c5cf8);
      local_30 = 1;
      local_4 = CONCAT31(local_4._1_3_,7);
      CString_Construct(s_FlmStrk_008aaa24);
      uVar8 = 3;
      local_30 = 3;
      local_4 = 8;
      piVar2 = (int *)CProjectileShaft_Construct
                                (local_2c,local_28,0xff,0,0,CONCAT22(extraout_var,0xf) << 1);
    }
    local_4 = 9;
    if ((uVar8 & 2) != 0) {
      uVar8 = uVar8 & 0xfffffffd;
      local_30 = uVar8;
      CString_Destruct();
    }
    local_4._0_1_ = 2;
    local_4._1_3_ = 0;
    if ((uVar8 & 1) != 0) {
      CString_Destruct();
    }
    local_10 = *(int *)((int)param_1 + 0x12);
    iVar3 = param_1[0x17];
    iVar9 = *piVar2;
    uVar6 = 0;
    uVar5 = CProjectile::DetermineHeight(param_1);
    puVar7 = (undefined4 *)(**(code **)(*param_1 + 0x1c))(uVar5,uVar6);
    (**(code **)(iVar9 + 0x6c))(local_10,iVar3,iVar3,*puVar7,puVar7[1]);
    *(undefined4 *)(in_ECX + 0x110) = 1;
    goto LAB_004bc412;
  case 0x24:
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    iVar3 = operator_new(0x29a);
    local_4._0_1_ = 0xb;
    if (iVar3 != 0) {
      CString_Construct(&DAT_008c5cf8);
      local_30 = 4;
      uVar8 = 0xc;
      local_4 = CONCAT31(local_4._1_3_,0xc);
      CString_Construct(s_HLYMITE_008aaa1c);
      local_30 = 0xc;
      local_4 = 0xd;
      piVar10 = (int *)CProjectileShaft_Construct
                                 (local_24,local_20,0xff,1,0,CONCAT22(extraout_var_00,0xf) << 1);
    }
    local_4 = 0xe;
    if ((uVar8 & 8) != 0) {
      uVar8 = uVar8 & 0xfffffff7;
      local_30 = uVar8;
      CString_Destruct();
    }
    local_4._0_1_ = 2;
    local_4._1_3_ = 0;
    if ((uVar8 & 4) != 0) {
      CString_Destruct();
    }
    uVar5 = *(undefined4 *)((int)param_1 + 0x12);
    iVar3 = param_1[0x17];
    iVar9 = *piVar10;
    uVar13 = 0;
    uVar6 = CProjectile::DetermineHeight(param_1);
    puVar7 = (undefined4 *)(**(code **)(*param_1 + 0x1c))(uVar6,uVar13);
    uVar6 = puVar7[1];
    uVar13 = *puVar7;
    goto LAB_004bc3fe;
  case 0x25:
    param_1 = (int *)0x93;
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    CString_operator_equ(s_SHAir_008aaa14);
    pcVar12 = s_SHAir7_008aaa0c;
LAB_004bc110:
    CString_operator_equ(pcVar12);
LAB_004bc119:
    CString_operator_equ(&DAT_008c5cf8);
    goto LAB_004bc33e;
  case 0x26:
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    iVar3 = operator_new(0x332);
    local_4._0_1_ = 0x10;
    if (iVar3 == 0) {
      param_1 = (int *)0x0;
    }
    else {
      CString_Construct(&DAT_008c5cf8);
      local_30 = 0x10;
      local_4 = CONCAT31(local_4._1_3_,0x11);
      CString_Construct(&DAT_008c5cf8);
      local_30 = 0x30;
      local_4 = 0x12;
      CString_Construct(s_DDoorH_008aaa04);
      uVar8 = 0x70;
      local_30 = 0x70;
      local_4 = 0x13;
      param_1 = (int *)FUN_00535320(local_1c,local_18,local_14,0,0);
    }
    local_4 = 0x15;
    if ((uVar8 & 0x40) != 0) {
      uVar8 = uVar8 & 0xffffffbf;
      local_30 = uVar8;
      CString_Destruct();
    }
    local_4 = 0x14;
    if ((uVar8 & 0x20) != 0) {
      uVar8 = uVar8 & 0xffffffdf;
      local_30 = uVar8;
      CString_Destruct();
    }
    local_4._0_1_ = 2;
    local_4._1_3_ = 0;
    if ((uVar8 & 0x10) != 0) {
      CString_Destruct();
    }
    local_10 = *param_1;
    iVar3 = piVar2[0x17];
    uVar5 = *(undefined4 *)((int)piVar2 + 0x12);
    puVar7 = (undefined4 *)(**(code **)(*piVar2 + 0x1c))(0,0);
    (**(code **)(local_10 + 0x6c))(uVar5,iVar3,iVar3,*puVar7,puVar7[1]);
    *(undefined4 *)(in_ECX + 0x110) = 1;
    goto LAB_004bc412;
  case 0x27:
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    CString_operator_equ(s_SPFDEATH_008aa9f8);
    param_1 = (int *)0xcf;
    goto LAB_004bc10b;
  default:
    switch(uVar4) {
    case 0:
      param_1 = (int *)0x76;
      break;
    case 1:
      param_1 = (int *)0x77;
      break;
    case 2:
      param_1 = (int *)0x78;
      break;
    case 4:
      param_1 = (int *)0x79;
      break;
    case 5:
      param_1 = (int *)0x7a;
      break;
    case 6:
      param_1 = (int *)0x7b;
      break;
    case 8:
      param_1 = (int *)0x7c;
      break;
    case 9:
      param_1 = (int *)0x7d;
      break;
    case 10:
      param_1 = (int *)0x7e;
      break;
    case 0xc:
      param_1 = (int *)0x7f;
      break;
    case 0xd:
      param_1 = (int *)0x80;
      break;
    case 0xe:
      param_1 = (int *)0x81;
      break;
    case 0x10:
      param_1 = (int *)0x82;
      break;
    case 0x11:
      param_1 = (int *)0x83;
      break;
    case 0x12:
      param_1 = (int *)0x84;
      break;
    case 0x14:
      param_1 = (int *)0x85;
      break;
    case 0x15:
      param_1 = (int *)0x86;
      break;
    case 0x16:
      param_1 = (int *)0x87;
      break;
    case 0x18:
      param_1 = (int *)0x88;
      break;
    case 0x19:
      param_1 = (int *)0x89;
      break;
    case 0x1a:
      param_1 = (int *)0x8a;
      break;
    case 0x1c:
      param_1 = (int *)0x8b;
      break;
    case 0x1d:
      param_1 = (int *)0x8c;
      break;
    case 0x1e:
      param_1 = (int *)0x8d;
    }
    uVar4 = uVar4 & 3;
    if (uVar4 == 0) {
      ExceptionList = &pvStack_c;
      local_4._1_3_ = uVar1;
      CString_operator_equ(s_SHAir_008aaa14);
      if ((*(uint *)(in_ECX + 0x1c) & 0xfffffffc) == 0) goto LAB_004bc10b;
      CString_operator_equ(s_SHAir0_008aa9f0);
      CString_SetAt(*(int *)(local_3c + -8) + -1,
                    CONCAT31((uint3)(*(uint *)(in_ECX + 0x1c) >> 10),
                             (char)(*(uint *)(in_ECX + 0x1c) >> 2) + '0'));
      goto LAB_004bc119;
    }
    if (uVar4 == 1) {
      ExceptionList = &pvStack_c;
      local_4._1_3_ = uVar1;
      CString_operator_equ(s_SHEarth_008aa9e8);
      if ((*(uint *)(in_ECX + 0x1c) & 0xfffffffc) != 0) {
        CString_operator_equ(s_SHEarth0_008aa9dc);
        iVar3 = *(int *)(local_3c + -8);
LAB_004bc30a:
        CString_SetAt(iVar3 + -1,
                      CONCAT31((uint3)(*(uint *)(in_ECX + 0x1c) >> 10),
                               (char)(*(uint *)(in_ECX + 0x1c) >> 2) + '0'));
        goto LAB_004bc119;
      }
LAB_004bc10b:
      pcVar12 = &DAT_008c5cf8;
      goto LAB_004bc110;
    }
    if (uVar4 == 2) {
      ExceptionList = &pvStack_c;
      local_4._1_3_ = uVar1;
      CString_operator_equ(s_SHWater_008aa9d4);
      if ((*(uint *)(in_ECX + 0x1c) & 0xfffffffc) != 0) {
        CString_operator_equ(s_SHWater0_008aa9c8);
        iVar3 = *(int *)(local_3c + -8);
        goto LAB_004bc30a;
      }
      goto LAB_004bc10b;
    }
    ExceptionList = &pvStack_c;
    local_4._1_3_ = uVar1;
    CUtil::UtilAssert(19999,s_C__Projects_Icewind2_src_Baldur__008aa610,s_FALSE_008a636c,0);
LAB_004bc33e:
    local_30 = operator_new(0x32c);
    local_4._0_1_ = 0x17;
    if (local_30 != 0) {
      piVar10 = (int *)FUN_00535f20(&local_34,&local_38,&local_3c,8,0);
    }
    local_4._0_1_ = 2;
    local_30 = operator_new(0x24);
    local_4._0_1_ = 0x18;
    if (local_30 == 0) {
      uVar5 = 0;
    }
    else {
      iVar3 = piVar2[0x17];
      uVar13 = 0;
      iVar9 = iVar3;
      iVar11 = iVar3;
      uVar5 = CProjectile::DetermineHeight(piVar2);
      uVar6 = (**(code **)(*piVar2 + 0x1c))(uVar5,iVar9,iVar11,uVar13);
      uVar5 = CMessageFireProjectile::CMessageFireProjectile
                        (param_1,iVar3,uVar6,uVar5,iVar9,iVar11,uVar13);
    }
    local_4._0_1_ = 2;
    CMessageHandler::AddMessage(uVar5,0);
    uVar5 = *(undefined4 *)((int)piVar2 + 0x12);
    iVar3 = piVar2[0x17];
    iVar9 = *piVar10;
    uVar13 = 0;
    uVar6 = CProjectile::DetermineHeight(piVar2);
    puVar7 = (undefined4 *)(**(code **)(*piVar2 + 0x1c))(uVar6,uVar13);
    uVar6 = puVar7[1];
    uVar13 = *puVar7;
LAB_004bc3fe:
    (**(code **)(iVar9 + 0x6c))(uVar5,iVar3,iVar3,uVar13,uVar6);
    *(undefined4 *)(local_10 + 0x110) = 1;
    goto LAB_004bc412;
  }
  local_4._0_1_ = 2;
  FUN_0052b150(0,uVar5);
  puVar7 = (undefined4 *)(**(code **)(*param_1 + 0x1c))();
  (**(code **)(*piVar10 + 0x88))(*(undefined4 *)((int)param_1 + 0x12),*puVar7,puVar7[1] + -2);
  uVar1 = local_4._1_3_;
LAB_004bbdc1:
  local_4._1_3_ = uVar1;
  *(undefined4 *)(in_ECX + 0x110) = 1;
LAB_004bc412:
  local_4._0_1_ = 1;
  CString_Destruct();
  local_4 = (uint)local_4._1_3_ << 8;
  CString_Destruct();
  local_4 = 0xffffffff;
  CString_Destruct();
  ExceptionList = pvStack_c;
  return 1;
}
```

## Callers

- 0x849A6C (not in a function)

## Callees

- 0x4F7500 CMessageHandler::AddMessage
- 0x500470 CMessageFireProjectile::CMessageFireProjectile
- 0x529F40 CProjectile::DetermineHeight
- 0x52B150 FUN_0052b150
- 0x530300 FUN_00530300
- 0x5348C0 CProjectileShaft_Construct
- 0x535320 FUN_00535320
- 0x535F20 FUN_00535f20
- 0x780C00 CUtil::UtilAssert
- 0x7FC95B operator_new
- 0x7FCC1A CString_Destruct
- 0x7FCC88 CString_Construct
- 0x7FCD57 CString_operator_equ
- 0x7FD114 CString_SetAt

## Notes

CGameEffectVisualSpellHit::ApplyEffect, opcode 141 (vtable 0x849A64), ported from the disassembly (0x4BBCB0..0x4BC456). Three local CStrings (+0x10 the palette, +0x14 the sound, +0x18 the animation). Without an area it only ends. Modes 0x20, 0x21, 0x22: a burst (CProjectile84DFA4 0x530300) with colour range 0 set to 0x43, 0x41 or 0x45 (0x52B150), played at the sprite two pixels up (slot 0x88, 0x530510). 0x23 and 0x24: a shaft (CProjectileShaft 0x5348C0) of "FlmStrk" or "HLYMITE" (the latter with +0x290 set), no sound, no colour (0xFF), no random frame, duration TIMESCALE_MSEC_PER_SEC (0x84EC0C) * 2, fired at the sprite (slot 0x6C) at its DetermineHeight (0: m_bHasHeight is clear). 0x26: a door (CProjectile84E940 0x535320) "DDoorH", fired at the sprite at height 0. The others play a local hit (CProjectile84E9C0 0x535F20, render flags 8): 0x25 "SHAir" in palette "SHAir7", type 0x93; 0x27 "SPFDEATH", type 0xCF; 0..0x1E by their low two bits "SHAir", "SHEarth" or "SHWater", in the palette "<name>N" for N = the mode >> 2 (none for N 0; CString::SetAt on the last character of "<name>0"), types 0x76..0x8D (a second switch); low bits 3 assert (line 19999). The hit: a CMessageFireProjectile of the type at the sprite with the hit's DetermineHeight (the sprite's cast height: the hit sets m_bHasHeight), then the hit fired at the sprite at that height. Every branch ends done and returns 1.

NOTE (an address-dependent value, inert in single player): the projectile type lives in the sprite argument, so a mode without an entry (low bits 3, after the assert at line 19999, or above 0x27) fires the sprite's address as the type; CMessageFireProjectile fires nothing without a service provider (tests/test_projectile.cpp), so ours sends no message for such a mode (its local hit still plays).

tests/test_visual_projectiles.cpp applies the effects of SPWI984 (modes 24, 25, 26), SPWI980 (32) and SPWI995 (38) to a goblin in AR1000: the messages, the objects, their render and their end.
