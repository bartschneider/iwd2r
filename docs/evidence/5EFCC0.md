---
address: 0x5EFCC0
name: FUN_005efcc0
exemption: "writes the selected character's text sheet; checked by tests/test_ui_misc.cpp [uimisc][character]"
ghidra_version: 12.1.4
---

# 0x5EFCC0 FUN_005efcc0

## Decompiled (Ghidra)

```c
void FUN_005efcc0(undefined4 param_1,int param_2,char param_3,char param_4)

{
  int iVar1;
  ushort uVar2;
  undefined4 uVar3;
  undefined4 *puVar4;
  undefined4 *puVar5;
  undefined1 local_14 [4];
  undefined1 local_10 [4];
  void *local_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  iVar1 = param_2;
  local_4 = 0xffffffff;
  puStack_8 = &LAB_00827c30;
  local_c = ExceptionList;
  ExceptionList = &local_c;
  uVar2 = CItem::GetUsageCount(0);
  if (uVar2 < 2) {
    if ((*(byte *)(iVar1 + 0x20) & 1) == 0) {
      if (param_3 == '\0') {
        puVar4 = (undefined4 *)CBaldurEngine::FetchString(local_10,0x42d4);
        local_4 = 3;
        uVar3 = CItem::GetGenericName();
        puVar5 = (undefined4 *)CBaldurEngine::FetchString(local_14,uVar3);
        FUN_007e89c1(param_1,s_____s____s_008b31a8,*puVar5,*puVar4);
        CString_Destruct();
        local_4 = 0xffffffff;
      }
      else {
        puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_2,0x42d4);
        local_4 = 2;
        uVar3 = CItem::GetGenericName();
        puVar5 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
        FUN_007e89c1(param_1,s_____s____s___008b31b4,*puVar5,*puVar4);
        CString_Destruct();
        local_4 = 0xffffffff;
      }
    }
    else if (param_3 == '\0') {
      uVar3 = CItem::GetGenericName();
      puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
      FUN_007e89c1(param_1,s_____s_008b31c4,*puVar4);
    }
    else {
      uVar3 = CItem::GetGenericName();
      puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
      FUN_007e89c1(param_1,s_____s___008b31cc,*puVar4);
    }
  }
  else if ((*(byte *)(iVar1 + 0x20) & 1) == 0) {
    if (param_3 == '\0') {
      puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_2,0x42d4);
      local_4 = 1;
      uVar3 = CItem::GetGenericName();
      puVar5 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
      FUN_007e89c1(param_1,s_____s___d_____s_008b31d8,*puVar5,uVar2,*puVar4);
      CString_Destruct();
      local_4 = 0xffffffff;
    }
    else {
      puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_2,0x42d4);
      local_4 = 0;
      uVar3 = CItem::GetGenericName();
      puVar5 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
      FUN_007e89c1(param_1,s_____s___d_____s___008b31ec,*puVar5,uVar2,*puVar4);
      CString_Destruct();
      local_4 = 0xffffffff;
    }
  }
  else if (param_3 == '\0') {
    uVar3 = CItem::GetGenericName();
    puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
    FUN_007e89c1(param_1,s_____s___d__008b3200,*puVar4,uVar2);
  }
  else {
    uVar3 = CItem::GetGenericName();
    puVar4 = (undefined4 *)CBaldurEngine::FetchString(&param_3,uVar3);
    FUN_007e89c1(param_1,s_____s___d____008b320c,*puVar4,uVar2);
  }
  CString_Destruct();
  if (param_4 != '\0') {
    uVar3 = CItem::GetDescription();
    FUN_005efb60(param_1,uVar3);
    FUN_007e8973(&DAT_008b31a4,param_1);
  }
  ExceptionList = local_c;
  return;
}
```

## Callers

- 0x5EFFD0 FUN_005effd0 (call at 0x5F6196)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F61B1)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F61CD)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F61E9)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F6223)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F624D)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F6269)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F6285)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F62A1)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F62BD)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F6318)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F638C)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F63E4)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F6442)

## Callees

- 0x427B60 CBaldurEngine::FetchString
- 0x4E84B0 CItem::GetUsageCount
- 0x4E9B10 CItem::GetGenericName
- 0x4E9FD0 CItem::GetDescription
- 0x5EFB60 FUN_005efb60
- 0x7E8973 FUN_007e8973
- 0x7E89C1 FUN_007e89c1
- 0x7FCC1A CString_Destruct

## Notes

`static void CScreenCharacter::sub_5EFCC0(FILE* file, CItem* pItem, BYTE
bEquipped, BYTE bDescription)` (__cdecl): an item's line of the text
character sheet. Uses below 2 (GetUsageCount(0)): "-- %s\n" / "-- %s *\n"
(equipped), unidentified (m_flags & 1 clear) "-- %s - %s" with string
17108 "NOT IDENTIFIED"; else "-- %s (%d)" with the uses. With
`bDescription` the description (0x5EFB60) and "\n\n\n".

Tests: tests/test_ui_misc.cpp "character screen: the text character
sheet's lines" (equipped, unidentified, a stack of ten bolts).
