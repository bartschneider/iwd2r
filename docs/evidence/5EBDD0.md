---
address: 0x5EBDD0
name: FUN_005ebdd0
exemption: "sets a known-spell button's spell and icon; UI only"
kind: read-only
ghidra_version: 12.1.4
---

# 0x5EBDD0 FUN_005ebdd0

## Decompiled (Ghidra)

```c
void FUN_005ebdd0(void)

{
  int iVar1;
  undefined4 uVar2;
  int *in_ECX;
  undefined *local_40;
  undefined4 local_3c;
  undefined4 local_38;
  undefined1 local_34 [8];
  undefined1 local_2c [8];
  undefined1 local_24 [8];
  int local_1c;
  int local_18;
  void *pvStack_c;
  code *pcStack_8;
  uint local_4;
  
  pcStack_8 = SEH_5EBDD0;
  pvStack_c = ExceptionList;
  local_40 = PTR_DAT_008c1758;
  local_4 = 0;
  ExceptionList = &pvStack_c;
  iVar1 = CResRef::operator!=(&stack0x00000004);
  if (iVar1 != 0) {
    CResRef::operator=(&local_3c,&stack0x00000004);
    CResRef::operator=(&local_3c,&DAT_008c5cf8);
    iVar1 = CResRef::operator!=(&DAT_008c5cf8);
    if (iVar1 == 0) {
      (**(code **)(*in_ECX + 0x44))(0xffffffff,0xffffffff,0xffffffff);
    }
    else {
      local_3c = *(undefined4 *)((int)in_ECX + 0x676);
      local_38 = *(undefined4 *)((int)in_ECX + 0x67a);
      CResRef::CResRef();
      local_18 = 0;
      CResRef::operator=(local_2c,&DAT_008c5cf8);
      local_1c = 0;
      local_4._0_1_ = 1;
      CResHelper_CResSpell_SetResRef(&local_3c,1,1);
      local_4 = CONCAT31(local_4._1_3_,2);
      CSpell::Demand();
      if (local_18 != 0) {
        CSpell::GetIcon(local_34);
        CResRef::operator=(local_24,local_34);
        CResRef::CopyToString(&local_40);
        CString_SetAt(*(int *)(local_40 + -8) + -1,0x42);
        CResRef::operator=(local_24,&local_40);
      }
      uVar2 = CSpell::GetGenericName();
      (**(code **)(*in_ECX + 0x44))(uVar2,0xffffffff,0xffffffff);
      CSpell::Release();
      local_4 = local_4 & 0xffffff00;
      if (local_18 != 0) {
        iVar1 = CResRef::operator!=(&DAT_008c5cf8);
        if (iVar1 != 0) {
          if (local_1c != 0) {
            CRes::CancelRequest();
            local_1c = 0;
          }
          CDimm::ReleaseResObject(local_18);
        }
      }
      local_18 = 0;
    }
  }
  local_4 = 0xffffffff;
  CString_Destruct();
  ExceptionList = pvStack_c;
  return;
}
```

## Callers

- 0x5D9970 CScreenCharacter::ResetSpellsPanel (call at 0x5D9DC2)
- 0x5D9970 CScreenCharacter::ResetSpellsPanel (call at 0x5D9F97)

## Callees

- 0x4699F0 CResHelper_CResSpell_SetResRef
- 0x54A380 CSpell::Demand
- 0x54A3C0 CSpell::Release
- 0x54A430 CSpell::GetIcon
- 0x54ACD0 CSpell::GetGenericName
- 0x77E370 CRes::CancelRequest
- 0x787CE0 CDimm::ReleaseResObject
- 0x78A960 CResRef::CResRef
- 0x78A9F0 CResRef::CopyToString
- 0x78AAF0 CResRef::operator!=
- 0x78AB40 CResRef::operator!=
- 0x78AC30 CResRef::operator=
- 0x78AC50 CResRef::operator=
- 0x78ACC0 CResRef::operator=
- 0x78AD40 CResRef::operator=
- 0x7FCC1A CString_Destruct
- 0x7FD114 CString_SetAt

## Notes

Ported (was missing). When the spell changes: icon cleared; for a spell, the icon is the spell's icon with its last letter made 'B', and the tooltip the spell's name (set even when the resource is missing); for none, no tooltip.
