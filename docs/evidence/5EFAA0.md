---
address: 0x5EFAA0
name: FUN_005efaa0
exemption: "writes the selected character's text sheet; checked by tests/test_ui_misc.cpp [uimisc][character]"
kind: read-only
ghidra_version: 12.1.4
---

# 0x5EFAA0 FUN_005efaa0

## Decompiled (Ghidra)

```c
void FUN_005efaa0(undefined4 param_1,undefined4 *param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5)

{
  undefined4 uVar1;
  undefined *local_14;
  undefined *local_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  puStack_8 = &LAB_00827bd0;
  local_c = ExceptionList;
  local_10 = PTR_DAT_008c1758;
  local_14 = PTR_DAT_008c1758;
  local_4._0_1_ = 1;
  local_4._1_3_ = 0;
  ExceptionList = &local_c;
  CString_Format(&local_10,&DAT_008a6a68,*param_2);
  CString_Format(&local_14,param_3,param_4);
  uVar1 = FUN_005ef760(&local_10,&local_14,param_5,param_1);
  FUN_007e8973(uVar1);
  local_4 = (uint)local_4._1_3_ << 8;
  CString_Destruct();
  local_4 = 0xffffffff;
  CString_Destruct();
  ExceptionList = local_c;
  return;
}
```

## Callers

- 0x5EFFD0 FUN_005effd0 (call at 0x5F48D4)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F4958)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F49D1)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F4A47)
- 0x5EFFD0 FUN_005effd0 (call at 0x5F4B15)

## Callees

- 0x5EF760 FUN_005ef760
- 0x7E8973 FUN_007e8973
- 0x7FAEE8 CString_Format
- 0x7FCC1A CString_Destruct

## Notes

`static void CScreenCharacter::sub_5EFAA0(FILE* file, const CString&
sLabel, const char* format, INT nValue, BYTE bIndent)` (__cdecl): as
0x5EF920 with the value formatted by `format` ("%+d" and "%d%%" in the
export).

Tests: through the text character sheet tests in tests/test_ui_misc.cpp.
