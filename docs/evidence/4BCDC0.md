---
address: 0x4BCDC0
name: CGameEffectCastSpellPoint::ApplyEffect
exemption: "static read against the disassembly; no oracle case is registered for this effect"
kind: read-only
ghidra_version: 12.1.4
---

# 0x4BCDC0 CGameEffectCastSpellPoint::ApplyEffect

## Decompiled (Ghidra)

```c
undefined4 CGameEffectCastSpellPoint::ApplyEffect(int param_1)

{
  undefined4 uVar1;
  undefined4 uVar2;
  undefined4 *puVar3;
  int in_ECX;
  undefined **ppuStack_1d8;
  undefined *local_1c4;
  undefined4 *local_1c0;
  undefined2 local_1bc [91];
  undefined4 local_106;
  undefined4 local_102;
  undefined4 local_fe;
  undefined *local_fa;
  undefined *local_f6;
  undefined4 local_f2;
  undefined4 local_ee;
  undefined4 local_ea;
  void *local_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_008132c0;
  local_c = ExceptionList;
  ppuStack_1d8 = (undefined **)0x0;
  ExceptionList = &local_c;
  CAIObjectType::CAIObjectType(0,0,0,0,0,0,0,0,0xffffffff,0);
  ppuStack_1d8 = (undefined **)0x0;
  local_4 = 0;
  CAIObjectType::CAIObjectType(0,0,0,0,0,0,0,0,0xffffffff,0);
  ppuStack_1d8 = (undefined **)0x0;
  local_4._0_1_ = 1;
  CAIObjectType::CAIObjectType(0,0,0,0,0,0,0,0,0xffffffff,0);
  local_fa = PTR_DAT_008c1758;
  local_f6 = PTR_DAT_008c1758;
  local_4 = CONCAT31(local_4._1_3_,4);
  ppuStack_1d8 = (undefined **)0x4bce8b;
  CAIAction::CAIAction();
  ppuStack_1d8 = (undefined **)0x4bce97;
  CAIAction::~CAIAction();
  local_1bc[0] = 0x72;
  local_106 = 0;
  local_f2 = 0xffffffff;
  local_ee = 0xffffffff;
  local_102 = 0xffffffff;
  local_fe = 0;
  local_ea = 0;
  local_1c4 = PTR_DAT_008c1758;
  ppuStack_1d8 = &local_1c4;
  local_4._0_1_ = 6;
  local_4._1_3_ = 0;
  CResRef::CopyToString();
  local_1c0 = &ppuStack_1d8;
  CString_Construct(&local_1c4);
  CAIAction::SetString1();
  local_106 = *(undefined4 *)(in_ECX + 0x18);
  local_ee = *(undefined4 *)(in_ECX + 0x88);
  local_f2 = *(undefined4 *)(in_ECX + 0x84);
  ppuStack_1d8 = (undefined **)0xe2;
  puVar3 = (undefined4 *)operator_new();
  local_1c0 = puVar3;
  if (puVar3 == (undefined4 *)0x0) {
    puVar3 = (undefined4 *)0x0;
  }
  else {
    uVar1 = *(undefined4 *)(param_1 + 0x5c);
    uVar2 = *(undefined4 *)(in_ECX + 0x10c);
    *puVar3 = &PTR__CMessage_00847b40;
    puVar3[2] = uVar1;
    puVar3[1] = uVar2;
    local_4._0_1_ = 8;
    ppuStack_1d8 = (undefined **)0x4bcf78;
    CAIAction::CAIAction();
    ppuStack_1d8 = (undefined **)local_1bc;
    local_4._0_1_ = 9;
    *puVar3 = &PTR_FUN_008489b4;
    CAIAction::operator=();
  }
  ppuStack_1d8 = (undefined **)0x0;
  local_4._0_1_ = 6;
  CMessageHandler::AddMessage(puVar3);
  *(undefined4 *)(in_ECX + 0x110) = 1;
  local_4 = CONCAT31(local_4._1_3_,5);
  ppuStack_1d8 = (undefined **)0x4bcfcc;
  CString_Destruct();
  local_4 = 0xd;
  ppuStack_1d8 = (undefined **)0x4bcfe3;
  CString_Destruct();
  local_4._0_1_ = 0xc;
  ppuStack_1d8 = (undefined **)0x4bcff7;
  CString_Destruct();
  local_4._0_1_ = 0xb;
  ppuStack_1d8 = (undefined **)0x4bd00b;
  CString_Destruct();
  local_4 = CONCAT31(local_4._1_3_,10);
  ppuStack_1d8 = (undefined **)0x4bd01c;
  CString_Destruct();
  local_4 = 0xffffffff;
  ppuStack_1d8 = (undefined **)0x4bd030;
  CString_Destruct();
  ExceptionList = local_c;
  return 1;
}
```

## Callers

- 0x849954 (not in a function)

## Callees

- 0x403A40 CAIAction::CAIAction
- 0x403B40 CAIAction::~CAIAction
- 0x405090 CAIAction::operator=
- 0x40AE80 CAIObjectType::CAIObjectType
- 0x41CD60 CAIAction::SetString1
- 0x4F7500 CMessageHandler::AddMessage
- 0x78A9F0 CResRef::CopyToString
- 0x7FC95B operator_new
- 0x7FC98F CString_Construct
- 0x7FCC1A CString_Destruct

## Notes

Imported WillScarlettOhara's tail: the exe stores 1 to m_done at +0x110 (0x4bcfb5) right after AddMessage (call 0x4f7500 at 0x4bcfac), as in CGameEffectCastSpell::ApplyEffect. Ours dropped the store; everything else already matched.

