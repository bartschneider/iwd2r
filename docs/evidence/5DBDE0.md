---
address: 0x5DBDE0
name: CScreenCharacter::UpdateMainPanel
exemption: "main panel update; covered by tests/test_character_screen.cpp (the main panel enables Level Up with the experience for it)"
kind: read-only
ghidra_version: 12.1.4
---

# 0x5DBDE0 CScreenCharacter::UpdateMainPanel

## Decompiled (Ghidra)

```c
void CScreenCharacter::UpdateMainPanel(int param_1)

{
  uint *puVar1;
  short sVar2;
  char cVar3;
  int iVar4;
  int iVar5;
  int in_ECX;
  int *piVar6;
  undefined4 uVar7;
  int local_3c;
  undefined *puStack_38;
  int *local_34;
  undefined4 local_30;
  undefined *puStack_2c;
  int local_28;
  undefined *local_24;
  undefined *local_20;
  undefined *local_1c;
  undefined4 uStack_18;
  undefined4 auStack_14 [2];
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  puStack_8 = &LAB_00825e18;
  local_c = ExceptionList;
  local_1c = PTR_DAT_008c1758;
  local_20 = PTR_DAT_008c1758;
  local_24 = PTR_DAT_008c1758;
  local_4._0_1_ = 2;
  local_4._1_3_ = 0;
  if (*(short *)(in_ECX + 0x28) < *(short *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x3846)) {
    uVar7 = *(undefined4 *)
             (*(int *)(DAT_008cf6dc + 0x1c54) + 0x382e + *(short *)(in_ECX + 0x28) * 4);
    ExceptionList = &local_c;
    local_30 = uVar7;
  }
  else {
    uVar7 = 0xffffffff;
    local_30 = 0xffffffff;
    ExceptionList = &local_c;
  }
  do {
    do {
      cVar3 = CGameObjectArray::GetShare(uVar7,0,&local_3c,0xffffffff);
      iVar5 = local_3c;
    } while (cVar3 == '\x04');
  } while (cVar3 == '\x05');
  if (cVar3 == '\0') {
    piVar6 = (int *)(local_3c + 0x5a4);
    puVar1 = (uint *)(local_3c + 0x920);
    local_28 = local_3c + 0x3e36;
    local_34 = piVar6;
    iVar4 = CUIManager::GetPanel(2);
    if (iVar4 == 0) {
      CUtil::UtilAssert(0x1191,s_C__Projects_Icewind2_src_Baldur__008b2afc,s_pPanel____NULL_008a76c0
                        ,0);
    }
    uVar7 = CUIPanel::GetControl(0x2e);
    *(undefined4 *)(in_ECX + 0xfa) = uVar7;
    if (*(int *)(in_ECX + 0x7fa) == 0) {
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
    }
    else {
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::HighlightLabel
                (iVar4,0x1000002f,
                 *(ushort *)(iVar5 + 0x974) != (ushort)*(byte *)((int)local_34 + 0x25e),0xffff);
      CRuleTables::GetAbilityScoreModifier((int)*(short *)(iVar5 + 0x974));
      CBaldurEngine::UpdateLabel();
      sVar2 = *(short *)(iVar5 + 0x974);
      if ((sVar2 == 10) || (sVar2 == 0xb)) {
        uVar7 = 0;
      }
      else {
        uVar7 = 1;
      }
      CBaldurEngine::HighlightLabel(iVar4,0x10000035,uVar7,((9 < sVar2) - 1 & 0xffff01ff) + 0xff00);
      CBaldurEngine::UpdateLabel();
      CRuleTables::GetAbilityScoreModifier((int)*(short *)(iVar5 + 0x97a));
      CBaldurEngine::UpdateLabel();
      sVar2 = *(short *)(iVar5 + 0x97a);
      if ((sVar2 == 10) || (sVar2 == 0xb)) {
        uVar7 = 0;
      }
      else {
        uVar7 = 1;
      }
      CBaldurEngine::HighlightLabel(iVar4,0x10000036,uVar7,((9 < sVar2) - 1 & 0xffff01ff) + 0xff00);
      CBaldurEngine::UpdateLabel();
      CRuleTables::GetAbilityScoreModifier((int)*(short *)(iVar5 + 0x97c));
      CBaldurEngine::UpdateLabel();
      sVar2 = *(short *)(iVar5 + 0x97c);
      if ((sVar2 == 10) || (sVar2 == 0xb)) {
        uVar7 = 0;
      }
      else {
        uVar7 = 1;
      }
      CBaldurEngine::HighlightLabel(iVar4,0x10000037,uVar7,((9 < sVar2) - 1 & 0xffff01ff) + 0xff00);
      CBaldurEngine::UpdateLabel();
      CRuleTables::GetAbilityScoreModifier((int)*(short *)(iVar5 + 0x976));
      CBaldurEngine::UpdateLabel();
      sVar2 = *(short *)(iVar5 + 0x976);
      if ((sVar2 == 10) || (sVar2 == 0xb)) {
        uVar7 = 0;
      }
      else {
        uVar7 = 1;
      }
      CBaldurEngine::HighlightLabel(iVar4,0x10000038,uVar7,((9 < sVar2) - 1 & 0xffff01ff) + 0xff00);
      CBaldurEngine::UpdateLabel();
      CRuleTables::GetAbilityScoreModifier((int)*(short *)(iVar5 + 0x978));
      CBaldurEngine::UpdateLabel();
      sVar2 = *(short *)(iVar5 + 0x978);
      if ((sVar2 == 10) || (sVar2 == 0xb)) {
        uVar7 = 0;
      }
      else {
        uVar7 = 1;
      }
      CBaldurEngine::HighlightLabel(iVar4,0x10000039,uVar7,((9 < sVar2) - 1 & 0xffff01ff) + 0xff00);
      CBaldurEngine::UpdateLabel();
      CRuleTables::GetAbilityScoreModifier((int)*(short *)(iVar5 + 0x97e));
      CBaldurEngine::UpdateLabel();
      sVar2 = *(short *)(iVar5 + 0x97e);
      if ((sVar2 == 10) || (sVar2 == 0xb)) {
        uVar7 = 0;
      }
      else {
        uVar7 = 1;
      }
      CBaldurEngine::HighlightLabel(iVar4,0x1000003a,uVar7,((9 < sVar2) - 1 & 0xffff01ff) + 0xff00);
      piVar6 = local_34;
    }
    CBaldurEngine::HighlightLabel
              (iVar4,0x10000009,*(ushort *)(iVar5 + 0x97a) != (ushort)*(byte *)((int)piVar6 + 0x261)
               ,0xffff);
    CBaldurEngine::HighlightLabel
              (iVar4,0x1000000a,*(ushort *)(iVar5 + 0x97c) != (ushort)*(byte *)((int)piVar6 + 0x262)
               ,0xffff);
    CBaldurEngine::HighlightLabel
              (iVar4,0x1000000b,*(ushort *)(iVar5 + 0x976) != (ushort)*(byte *)((int)piVar6 + 0x25f)
               ,0xffff);
    CBaldurEngine::HighlightLabel
              (iVar4,0x1000000c,*(ushort *)(iVar5 + 0x978) != (ushort)*(byte *)(piVar6 + 0x98),
               0xffff);
    CBaldurEngine::HighlightLabel
              (iVar4,0x1000000d,*(ushort *)(iVar5 + 0x97e) != (ushort)*(byte *)((int)piVar6 + 0x263)
               ,0xffff);
    if (*(int *)(in_ECX + 0x7fa) == 0) {
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      puStack_38 = PTR_DAT_008c1758;
      local_4._0_1_ = 5;
      CRuleTables::GetRaceStringMixed(0,&puStack_38,0);
      CBaldurEngine::UpdateLabel();
      local_4 = CONCAT31(local_4._1_3_,2);
      CString_Destruct();
    }
    else {
      CBaldurEngine::UpdateLabel();
      CBaldurEngine::UpdateLabel();
      CGameSprite::GetAC();
      CBaldurEngine::UpdateLabel();
      uStack_18 = CONCAT31(uStack_18._1_3_,*(undefined1 *)(local_28 + 7));
      auStack_14[0] = CONCAT31(auStack_14[0]._1_3_,*(undefined1 *)(local_28 + 6));
      puStack_38 = PTR_DAT_008c1758;
      local_4._0_1_ = 4;
      CRuleTables::GetRaceStringMixed(auStack_14[0],&puStack_38,uStack_18);
      CBaldurEngine::UpdateLabel();
      local_4 = CONCAT31(local_4._1_3_,2);
      CString_Destruct();
      auStack_14[0] = CONCAT31(auStack_14[0]._1_3_,*(undefined1 *)(local_28 + 0x15));
      switch(*(undefined1 *)(local_28 + 0x15)) {
      case 0:
      case 0x11:
      case 0x12:
      case 0x13:
      case 0x21:
      case 0x22:
      case 0x23:
      case 0x31:
      case 0x32:
      case 0x33:
        break;
      default:
        CUtil::UtilAssert(0x60a,s_C__Projects_Icewind2_src_Baldur__008b2afc,s_FALSE_008a636c,0);
      }
    }
    if (*piVar6 == -1) {
      CGameSprite::GetName();
      CBaldurEngine::UpdateLabel();
    }
    else {
      CBaldurEngine::FetchString(auStack_14,*piVar6);
      local_4._0_1_ = 6;
      CBaldurEngine::UpdateLabel();
      local_4 = CONCAT31(local_4._1_3_,2);
      CString_Destruct();
    }
    iVar5 = CUIPanel::GetControl(0x2d);
    local_28 = 0;
    if (param_1 == 0) {
      local_28 = (int)*(short *)(iVar5 + 0x5a);
    }
    puStack_2c = PTR_DAT_008c1758;
    local_4 = CONCAT31(local_4._1_3_,7);
    CString_Format(&puStack_2c,&DAT_008b2d30,0x21);
    CString_operator_equ(&puStack_2c);
    CUIControlTextDisplay::RemoveAll();
    if (*(int *)(in_ECX + 0x7fa) != 0) {
      param_1 = 0x3c;
      do {
        CUIPanel::GetControl(param_1);
        CUIControlButton3State::SetSelected((uint)*(byte *)(in_ECX + 0x1844) == param_1 - 0x3cU);
        param_1 = param_1 + 1;
      } while (param_1 < 0x40);
      switch(*(undefined1 *)(in_ECX + 0x1844)) {
      case 0:
        UpdateGeneralInformation(iVar5,local_3c);
        break;
      case 1:
        UpdateEquipmentInformation(iVar5,local_3c);
        break;
      case 2:
        CBaldurEngine::FetchString(auStack_14,0x2ecf);
        local_4._0_1_ = 8;
        CBaldurEngine::UpdateTextForceColor();
        local_4._0_1_ = 7;
        CString_Destruct();
        CGameSprite::DisplaySkills(iVar5);
        CBaldurEngine::UpdateText();
        CBaldurEngine::FetchString(auStack_14,0x8e09);
        local_4._0_1_ = 9;
        CBaldurEngine::UpdateTextForceColor();
        local_4 = CONCAT31(local_4._1_3_,7);
        CString_Destruct();
        CGameSprite::DisplayFeats(iVar5);
        break;
      case 3:
        UpdateMiscInformation(iVar5,local_3c);
      }
    }
    uVar7 = CPtrList_FindIndex(local_28);
    CUIControlTextDisplay::SetTopString(uVar7);
    CheckMultiPlayerViewableModifyable();
    CUIPanel::GetControl(2);
    CResRef::CResRef(local_34 + 0xd);
    CUIControlButtonCharGenAppearancePortrait::SetPortrait(auStack_14);
    iVar5 = CUIPanel::GetControl(1);
    if (iVar5 == 0) {
      CUtil::UtilAssert(0x12e8,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                        s_pButton____NULL_008b2a7c,0);
    }
    CUIControlButton::SetEnabled(*(undefined4 *)(in_ECX + 0x7fa));
    iVar5 = CUIPanel::GetControl(0x32);
    if (iVar5 == 0) {
      CUtil::UtilAssert(0x12f3,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                        s_pButton____NULL_008b2a7c,0);
    }
    if ((*(int *)(in_ECX + 0x7fe) == 0) || ((*puVar1 & 0x800) != 0)) {
      uVar7 = 0;
    }
    else {
      uVar7 = 1;
    }
    CUIControlButton::SetEnabled(uVar7);
    iVar5 = CUIPanel::GetControl(0x24);
    if (iVar5 == 0) {
      CUtil::UtilAssert(0x12f8,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                        s_pButton____NULL_008b2a7c,0);
    }
    if ((((*(int *)(in_ECX + 0x7fe) == 0) || ((*(uint *)(local_3c + 0x5ac) & 0x800) == 0)) ||
        (*(int *)(local_3c + 0x9ba) != 0)) || ((*puVar1 & 0x800) != 0)) {
      uVar7 = 0;
    }
    else {
      uVar7 = 1;
    }
    CUIControlButton::SetEnabled(uVar7);
    iVar5 = CUIPanel::GetControl(0x25);
    if (iVar5 == 0) {
      CUtil::UtilAssert(0x1302,s_C__Projects_Icewind2_src_Baldur__008b2afc,
                        s_pButton____NULL_008b2a7c,0);
    }
    iVar5 = FUN_005e4e80(local_3c);
    if ((iVar5 == 0) || ((*puVar1 & 0x800) != 0)) {
      uVar7 = 0;
    }
    else {
      uVar7 = 1;
    }
    CUIControlButton::SetEnabled(uVar7);
    CGameObjectArray::ReleaseShare(local_30,0,0xffffffff);
    local_4._0_1_ = 2;
    CString_Destruct();
    local_4._0_1_ = 1;
    CString_Destruct();
    local_4 = (uint)local_4._1_3_ << 8;
    CString_Destruct();
    local_4 = 0xffffffff;
    CString_Destruct();
    ExceptionList = local_c;
    return;
  }
  local_4._0_1_ = 1;
  CString_Destruct();
  local_4 = (uint)local_4._1_3_ << 8;
  CString_Destruct();
  local_4 = 0xffffffff;
  CString_Destruct();
  ExceptionList = local_c;
  return;
}
```

## Callers

- 0x5D6A80 CScreenCharacter::EngineActivated (call at 0x5D6B6A)
- 0x5D78E0 CScreenCharacter::OnPortraitLClick (call at 0x5D7997)
- 0x5E1DF0 CScreenCharacter::DismissPopup (call at 0x5E1E91)
- 0x5E86D0 CScreenCharacter::UpdateCharacterStatus (call at 0x5E87D2)
- 0x5F8B50 CUIControlButtonCharacterInformationFolder::OnLButtonClick (call at 0x5F8BE3)

## Callees

- 0x427B60 CBaldurEngine::FetchString
- 0x427C20 CBaldurEngine::UpdateText
- 0x427EA0 CBaldurEngine::UpdateTextForceColor
- 0x427FE0 CBaldurEngine::UpdateLabel
- 0x428130 CBaldurEngine::HighlightLabel
- 0x4D2CA0 CUIPanel::GetControl
- 0x4D4000 CUIManager::GetPanel
- 0x4D57C0 CUIControlButton::SetEnabled
- 0x4D5AE0 CUIControlButton3State::SetSelected
- 0x4E2B50 CUIControlTextDisplay::RemoveAll
- 0x4E3CA0 CUIControlTextDisplay::SetTopString
- 0x542510 CRuleTables::GetAbilityScoreModifier
- 0x544DB0 CRuleTables::GetRaceStringMixed
- 0x599A50 CGameObjectArray::GetShare
- 0x599E70 CGameObjectArray::ReleaseShare
- 0x5DC9D0 CScreenCharacter::UpdateGeneralInformation
- 0x5DE2C0 CScreenCharacter::UpdateEquipmentInformation
- 0x5E0200 CScreenCharacter::UpdateMiscInformation
- 0x5E4E80 FUN_005e4e80
- 0x5E94C0 CScreenCharacter::CheckMultiPlayerViewableModifyable
- 0x61D0E0 CUIControlButtonCharGenAppearancePortrait::SetPortrait
- 0x71F760 CGameSprite::GetName
- 0x723CC0 CGameSprite::GetAC
- 0x765C50 CGameSprite::DisplayFeats
- 0x765E40 CGameSprite::DisplaySkills
- 0x780C00 CUtil::UtilAssert
- 0x78A9D0 CResRef::CResRef
- 0x7FAEE8 CString_Format
- 0x7FB6C0 CPtrList_FindIndex
- 0x7FCC1A CString_Destruct
- 0x7FCD07 CString_operator_equ

## Notes

Fixed: the Level Up button (37) is enabled by CanLevelUp (0x5E4E80) when the creature is not dead; ours left that call commented out, so Level Up never enabled. Added the race label (0x1000000F, GetRaceStringMixed of the start AI type, or of race 0 when the character is not viewable) and the alignment check (an assert for alignments outside the nine valid values) that the original makes; ours had neither.

## WSO delta

disagreed with WSO at e8dd16a: his body drops the race-label fill and the whole alignment switch the exe performs, and drops the exe's gated level-up enable entirely; case dump-5DBDE0 decides

WSO's UpdateMainPanel never calls GetRaceStringMixed/UpdateLabel(0x1000000F) in either branch, so the race text would keep its previous value, where ours fills it like the decompile. The alignment block (valid 0x00, 0x11-0x13, 0x21-0x23, 0x31-0x33; default UTIL_ASSERT at InfScreenCharacter.cpp:1546) is gone entirely. He renames pText->field_5A to m_nTopIndex (a member our tree does not carry, unportable), drops the (LPCSTR) casts, and comments the exe's SetEnabled(CanLevelUp(pSprite) && (m_generalState & STATE_DEAD) == 0) out entirely (fork src/CScreenCharacter.cpp at e8dd16a, lines 2491-2492 — his tree's only IsLevelUpButtonClickable occurrence is that comment; the helper exists neither in the exe nor in our tree), so the button would keep whatever enabled-state it had; ours keeps the gated enable (src/CScreenCharacter.cpp:2827-2828). Sealed dump comparison stands with ours.
