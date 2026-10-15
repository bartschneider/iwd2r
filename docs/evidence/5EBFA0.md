---
address: 0x5EBFA0
name: CUIControlButtonCharacter5EBC60::OnLButtonClick
exemption: "known-spell button click in the level-up; UI state on a sprite"
ghidra_version: 12.1.4
---

# 0x5EBFA0 CUIControlButtonCharacter5EBC60::OnLButtonClick

## Decompiled (Ghidra)

```c
/* WARNING: Type propagation algorithm not settling */

void CUIControlButtonCharacter5EBC60::OnLButtonClick(void)

{
  byte bVar1;
  int *piVar2;
  char cVar3;
  short sVar4;
  uint uVar5;
  int iVar6;
  undefined4 uVar7;
  int iVar8;
  int in_ECX;
  int iVar9;
  undefined4 uVar10;
  char cStack_59;
  int iStack_58;
  int iStack_54;
  undefined4 uStack_50;
  undefined4 uStack_4c;
  int iStack_48;
  int iStack_44;
  int iStack_40;
  int local_3c [3];
  int iStack_30;
  undefined4 uStack_2c;
  undefined4 uStack_28;
  undefined1 auStack_24 [8];
  int iStack_1c;
  int iStack_18;
  void *pvStack_c;
  undefined1 *puStack_8;
  undefined4 uStack_4;
  
  uStack_4 = 0xffffffff;
  puStack_8 = &LAB_008276a0;
  pvStack_c = ExceptionList;
  iVar8 = *(int *)(DAT_008cf6dc + 0x1c54);
  piVar2 = *(int **)(DAT_008cf6dc + 0x1c60);
  ExceptionList = &pvStack_c;
  local_3c[0] = in_ECX;
  if (iVar8 == 0) {
    ExceptionList = &pvStack_c;
    CUtil::UtilAssert(0x32f9,s_C__Projects_Icewind2_src_Baldur__008b2afc,s_pGame____NULL_008a76d0,0)
    ;
  }
  sVar4 = (**(code **)(*piVar2 + 0xd0))();
  if (sVar4 < *(short *)(iVar8 + 0x3846)) {
    uVar10 = *(undefined4 *)(iVar8 + 0x382e + sVar4 * 4);
    uStack_4c = uVar10;
  }
  else {
    uVar10 = 0xffffffff;
    uStack_4c = 0xffffffff;
  }
  do {
    do {
      cVar3 = CGameObjectArray::GetDeny(uVar10,0,&iStack_58,0xffffffff);
    } while (cVar3 == '\x04');
  } while (cVar3 == '\x05');
  if (cVar3 == '\0') {
    iStack_54 = *(int *)((int)piVar2 + 0x7b6);
    cStack_59 = *(char *)((int)piVar2 + 0x106);
    bVar1 = *(byte *)((int)piVar2 + 0x1776);
    uStack_28 = *(undefined4 *)(in_ECX + 0x67a);
    iVar8 = iStack_58 + 0x920;
    iStack_40 = iStack_58 + 0x3e36;
    iStack_44 = CONCAT31(iStack_44._1_3_,bVar1);
    uStack_2c = *(undefined4 *)(in_ECX + 0x676);
    iVar9 = bVar1 - 1;
    CResRef::CResRef();
    iStack_18 = 0;
    CResRef::operator=(auStack_24,&DAT_008c5cf8);
    iStack_1c = 0;
    uStack_4 = 0;
    CResHelper_CResSpell_SetResRef(&uStack_2c,1,1);
    uStack_4 = 1;
    CSpell::Demand();
    CSpell::GetDescription();
    uVar5 = CGameSprite::GetSpecialization();
    if ((uVar5 & 0x4000) == 0) {
      uVar10 = CSpell::GetSchool();
      iVar6 = CGameSprite::IsSpellSchool(uVar10);
      if (iVar6 == 0) {
        FUN_00618d10((char)iVar9 + '\x01',iStack_58);
      }
    }
    CSpell::Release();
    iStack_48 = 0;
    iStack_44 = 1;
    if ((cStack_59 == '\x02') || (cStack_59 == '\n')) {
      iStack_44 = CRuleTables::GetMaxKnownSpells
                            (&cStack_59,iStack_40,iVar8,*(undefined4 *)(iStack_58 + 0x80c),
                             (uint)bVar1,&iStack_48);
      iStack_44 = iStack_44 + iStack_48;
    }
    uVar5 = CInfGame::GetSpellcasterIndex(&cStack_59);
    uStack_50 = 0;
    cVar3 = CSpellResRefList::Find((undefined4 *)(in_ECX + 0x676),&uStack_50);
    if (cVar3 == '\0') {
      CUtil::UtilAssert(0x3327,s_C__Projects_Icewind2_src_Baldur__008b2afc,s_FALSE_008a636c,0);
    }
    iVar8 = local_3c[0];
    if (*(int *)(local_3c[0] + 0x66a) == 0) {
      if ((0 < iStack_54) && (iStack_18 != 0)) {
        iVar6 = CSpell::CheckUsableBy(iStack_58);
        if (iVar6 == 1) {
          CUIControlButton3State::SetSelected(1);
          *(int *)((int)piVar2 + 0x7b6) = iStack_54 + -1;
          local_3c[0] = 0;
          local_3c[1] = 0;
          local_3c[2] = iStack_44;
          iStack_30 = iVar9;
          if (7 < uVar5) {
            CUtil::UtilAssert(0x6cd,s___Include_ObjCreature_h_008aa968,
                              s_nClassIndex_<__CSPELLLIST_NUM_CL_008afab8,0);
          }
          if (6 < uVar5) {
            CUtil::UtilAssert(0xa0c,s___Include_FileFormat_h_008a7db4,
                              s_nClassIndex_<_CSPELLLIST_NUM_CLA_008a7e18,0);
          }
          CGameSpriteGroupedSpellList::Add(&uStack_50,&iStack_30,local_3c + 2,local_3c + 1,local_3c)
          ;
        }
      }
    }
    else {
      CUIControlButton3State::SetSelected(0);
      *(int *)((int)piVar2 + 0x7b6) = iStack_54 + 1;
      local_3c[0] = 0;
      iStack_40 = 0;
      iStack_44 = iVar9;
      if (7 < uVar5) {
        CUtil::UtilAssert(0x6cd,s___Include_ObjCreature_h_008aa968,
                          s_nClassIndex_<__CSPELLLIST_NUM_CL_008afab8,0);
      }
      if (6 < uVar5) {
        CUtil::UtilAssert(0xa0c,s___Include_FileFormat_h_008a7db4,
                          s_nClassIndex_<_CSPELLLIST_NUM_CLA_008a7e18,0);
      }
      CGameSpriteGroupedSpellList::Remove(&uStack_50,&iStack_44,1,&iStack_40,local_3c);
    }
    uVar10 = *(undefined4 *)(*(int *)(iVar8 + 6) + 0x20);
    uVar7 = CSpell::GetDescription();
    CScreenCharacter::UpdateHelp(uVar10,0x1e,uVar7);
    CScreenCharacter::UpdatePopupPanel(*(undefined4 *)(*(int *)(iVar8 + 6) + 0x20),iStack_58);
    CGameObjectArray::ReleaseDeny(uStack_4c,0,0xffffffff);
    uStack_4 = 0xffffffff;
    if (iStack_18 != 0) {
      iVar8 = CResRef::operator!=(&DAT_008c5cf8);
      if (iVar8 != 0) {
        if (iStack_1c != 0) {
          CRes::CancelRequest();
          iStack_1c = 0;
        }
        CDimm::ReleaseResObject(iStack_18);
      }
    }
  }
  ExceptionList = pvStack_c;
  return;
}
```

## Callers

- 0x8522F0 (not in a function)

## Callees

- 0x4699F0 CResHelper_CResSpell_SetResRef
- 0x4D5AE0 CUIControlButton3State::SetSelected
- 0x5431E0 CRuleTables::GetMaxKnownSpells
- 0x54A380 CSpell::Demand
- 0x54A3C0 CSpell::Release
- 0x54AA40 CSpell::CheckUsableBy
- 0x54AD30 CSpell::GetDescription
- 0x54B000 CSpell::GetSchool
- 0x599C70 CGameObjectArray::GetDeny
- 0x59A010 CGameObjectArray::ReleaseDeny
- 0x5CA160 CSpellResRefList::Find
- 0x5CA5D0 CInfGame::GetSpellcasterIndex
- 0x5E0B20 CScreenCharacter::UpdatePopupPanel
- 0x5E6840 CScreenCharacter::UpdateHelp
- 0x618D10 FUN_00618d10
- 0x703170 CGameSprite::IsSpellSchool
- 0x725F20 CGameSpriteGroupedSpellList::Add
- 0x725F90 CGameSpriteGroupedSpellList::Remove
- 0x728570 CGameSprite::GetSpecialization
- 0x77E370 CRes::CancelRequest
- 0x780C00 CUtil::UtilAssert
- 0x787CE0 CDimm::ReleaseResObject
- 0x78A960 CResRef::CResRef
- 0x78AB40 CResRef::operator!=
- 0x78AD40 CResRef::operator=

## Notes

Ported (was a stub). For a non-universal wizard whose school does not match, calls CScreenCreateChar::sub_618D10 (result unused, as the original). Deselecting removes the spell from the class's known list and gives the pick back; selecting needs picks left and a usable spell, adds it with the class's maximum known count (1 for other classes). Help 30 shows the description; the popup is updated.
