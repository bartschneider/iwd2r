---
address: 0x5ADAE0
name: CInfGame::SelectToolbar
exemption: "needs the object array, the selection and the world screen; run by Unmarshal in world_entry"
ghidra_version: 12.1.4
---

# 0x5ADAE0 CInfGame::SelectToolbar

## Decompiled (Ghidra)

```c
void CInfGame::SelectToolbar(void)

{
  char cVar1;
  uint uVar2;
  undefined4 uVar3;
  undefined4 *puVar4;
  int iVar5;
  int in_ECX;
  int local_4;
  
  *(undefined2 *)(in_ECX + 0x1b96) = 0;
  *(undefined4 *)(in_ECX + 0x35f6) = 100;
  uVar2 = *(uint *)(in_ECX + 0x3896) & 0xff;
  if (uVar2 == 0) {
    uVar3 = 0;
  }
  else {
    if (uVar2 == 1) {
      local_4 = in_ECX;
      puVar4 = (undefined4 *)CAIGroup::GetGroupList();
      if (puVar4 == (undefined4 *)0x0) {
        CInfButtonArray::SetState(0,0);
        operator_delete(0);
        return;
      }
      do {
        do {
          cVar1 = CGameObjectArray::GetShare(*puVar4,0,&local_4,0xffffffff);
        } while (cVar1 == '\x04');
      } while (cVar1 == '\x05');
      if (cVar1 != '\0') {
        CInfButtonArray::SetState(0,0);
        operator_delete(puVar4);
        return;
      }
      cVar1 = IcewindMisc::IsPC(local_4);
      if (cVar1 == '\x01') {
        CButtonArray_UpdateCustomizableButtonTypes(local_4 + 0x3d14);
        CButtonArray_UpdateSelectedWeaponSet(*(undefined1 *)(local_4 + 0x4c68));
        CInfButtonArray::SetState(0x72,0);
        CGameObjectArray::ReleaseShare(*puVar4,0,0xffffffff);
        operator_delete(puVar4);
      }
      else {
        CButtonArray_UpdateSelectedWeaponSet(*(undefined1 *)(local_4 + 0x4c68));
        CInfButtonArray::SetState(0x6e,0);
        CGameObjectArray::ReleaseShare(*puVar4,0,0xffffffff);
        operator_delete(puVar4);
      }
      goto LAB_005adc5b;
    }
    uVar3 = 0x6e;
  }
  CInfButtonArray::SetState(uVar3,0);
LAB_005adc5b:
  iVar5 = CScreenWorld_GetActivePanelID();
  if (iVar5 == 8) {
    CScreenWorld::StopContainer();
  }
  return;
}
```

## Callers

- 0x5C3B20 FUN_005c3b20 (call at 0x5C44F1)
- 0x5C4AD0 FUN_005c4ad0 (call at 0x5C5958)
- 0x5C4AD0 FUN_005c4ad0 (call at 0x5C5DE3)
- 0x5C4AD0 FUN_005c4ad0 (call at 0x5C5E73)
- 0x475DC0 CGameArea::OnActionButtonUp (call at 0x476571)
- 0x4AB510 CGameEffect_opcode_13_2 (call at 0x4ABE92)
- 0x751CD0 CGameSprite::LeaveParty (call at 0x752649)
- 0x74F6D0 CGameSprite::MoveGlobal (call at 0x74F73A)
- 0x5A7E40 CInfGame::Unmarshal (call at 0x5A8F10)
- 0x5AD470 CInfGame::SelectAll (call at 0x5AD7CA)
- 0x5AF870 CInfGame::OnPortraitLClick (call at 0x5AFC82)
- 0x64C600 CScreenMultiPlayer_OnMainDoneButtonClick (call at 0x64D4BA)
- 0x6629E0 CScreenSinglePlayer::OnMainDoneButtonClick (call at 0x663589)
- 0x6629E0 CScreenSinglePlayer::OnMainDoneButtonClick (call at 0x66359F)
- 0x69D0F0 FUN_0069d0f0 (call at 0x69E1FA)
- 0x69D0F0 FUN_0069d0f0 (call at 0x69E8D2)
- 0x69D0F0 FUN_0069d0f0 (call at 0x69E97B)
- 0x6F5FF0 CGameSprite::AIUpdate (call at 0x6F6AEC)
- 0x6FF8A0 CGameSprite::OnActionButton (call at 0x700728)
- 0x72B9A0 CGameSprite_ProcessAI (call at 0x72C242)
- 0x74B340 CGameSprite_LeaveAreaLUA (call at 0x74BEA5)
- 0x74CDB0 CGameSprite_LeaveAreaName (call at 0x74ED87)
- 0x74CDB0 CGameSprite_LeaveAreaName (call at 0x74F52F)
- 0x74CDB0 CGameSprite_LeaveAreaName (call at 0x74F588)
- 0x614EF0 CUIControlButtonCharGenAccept::OnLButtonClick (call at 0x6152E4)
- 0x6F5FF0 CGameSprite::AIUpdate (call at 0x6F8A88)
- 0x6872D0 CScreenWorld_OnKeyDown (call at 0x687D73)
- 0x6872D0 CScreenWorld_OnKeyDown (call at 0x6883C5)

## Callees

- 0x4060E0 CAIGroup::GetGroupList
- 0x5851F0 IcewindMisc::IsPC
- 0x589110 CInfButtonArray::SetState
- 0x595E70 CButtonArray_UpdateCustomizableButtonTypes
- 0x595F70 CButtonArray_UpdateSelectedWeaponSet
- 0x599A50 CGameObjectArray::GetShare
- 0x599E70 CGameObjectArray::ReleaseShare
- 0x690EB0 CScreenWorld::StopContainer
- 0x691130 CScreenWorld_GetActivePanelID
- 0x7FC984 operator_delete

## Notes

Ported from the decompile, checked against the disassembly (the object
array is the game's own, `this + 0x372C`). Resets `m_nState` and the button
array's selected button (100), then by selection size (`m_group`, the count
of its member list at 0x3896): none gives toolbar state 0; one selected
sprite gives 114 for a PC and 110 otherwise (state 0 when the group list is
empty or the sprite cannot be shared); several give 110. When the world
screen's active panel is 8 (the container panel) the container closes.

Partial: for one selected sprite the original first copies its nine
customizable button types (`field_3D14`, sprite + 0x3D14) into the button
array at +0x19B6 (0x595E70) and, for PCs and others alike, its weapon set
(`m_nWeaponSet`, +0x4C68) into the button array at +0x19E8 (0x595F70).
CInfButtonArray has neither function nor fields (TODO in the code). The
active panel accessor 0x691130 is not in our tree either; it returns
CScreenWorld::field_EA4, which ours reads directly.

No oracle case: it needs the object array, the selection and the world
screen. Unmarshal calls it at every game load (`world_entry`).
