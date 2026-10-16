---
address: 0x4DABD0
name: CUIControlEditMultiLine::OnLButtonDown
exemption: "checked by tests/test_action_bar.cpp [editml] (clicking and dragging)"
ghidra_version: 12.1.4
---

# 0x4DABD0 CUIControlEditMultiLine::OnLButtonDown

## Decompiled (Ghidra)

```c
undefined4 CUIControlEditMultiLine::OnLButtonDown(int param_1,int param_2)

{
  BYTE TestChar;
  int *piVar1;
  uint uVar2;
  short sVar3;
  undefined4 uVar4;
  int iVar5;
  int iVar6;
  BOOL BVar7;
  HWND in_ECX;
  int iVar8;
  int local_24;
  int local_20;
  undefined1 local_1c [4];
  int local_18;
  undefined1 local_14 [4];
  undefined4 local_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_00815270;
  local_c = ExceptionList;
  if (*(char *)((int)&in_ECX[7].unused + 2) == '\0') {
    uVar4 = 0;
  }
  else {
    param_1 = param_1 - (*(int *)((int)&in_ECX[3].unused + 2) + in_ECX[0xd2].unused);
    param_2 = param_2 - (*(int *)((int)&in_ECX[4].unused + 2) + in_ECX[0xd3].unused);
    if ((((param_1 < 0) || (*(int *)((int)&in_ECX[5].unused + 2) <= param_1)) || (param_2 < 0)) ||
       (*(int *)((int)&in_ECX[6].unused + 2) <= param_2)) {
      if (*(int *)((int)&in_ECX[0x217].unused + 2) == 1) {
        ExceptionList = &local_c;
        if (*(int *)((int)&in_ECX[0x21c].unused + 2) != 0) {
          ExceptionList = &local_c;
          CString_operator_equ((undefined1 *)((int)&in_ECX[0x219].unused + 2));
          ReConstitute();
        }
        iVar8 = **(int **)((int)&in_ECX[1].unused + 2);
        piVar1 = *(int **)(iVar8 + 0x14);
        if (piVar1 != (int *)0x0) {
          (**(code **)(*piVar1 + 0xc))();
          *(undefined4 *)(iVar8 + 0x14) = 0;
        }
        *(undefined4 *)((int)&in_ECX[0x222].unused + 2) = 0xffffffff;
        *(undefined4 *)((int)&in_ECX[0x223].unused + 2) = 0xffffffff;
      }
    }
    else {
      ExceptionList = &local_c;
      CUIManager::SetCapture(in_ECX);
      CResCell::Demand();
      local_24 = 0;
      iVar8 = (int)(short)in_ECX[0x21e].unused;
      sVar3 = CVidFont::GetFontHeight(0);
      if (param_2 / (int)sVar3 < iVar8) {
        sVar3 = CVidFont::GetFontHeight(0);
        iVar8 = param_2 / (int)sVar3;
      }
      local_20 = (int)*(short *)((int)&in_ECX[0x21e].unused + 2);
      if (*(int *)((int)&in_ECX[0x21b].unused + 2) + iVar8 < local_20) {
        iVar8 = (int)(short)in_ECX[0x21e].unused;
        sVar3 = CVidFont::GetFontHeight(0);
        if (param_2 / (int)sVar3 < iVar8) {
          sVar3 = CVidFont::GetFontHeight(0);
          local_20 = *(int *)((int)&in_ECX[0x21b].unused + 2) + param_2 / (int)sVar3;
        }
        else {
          local_20 = *(int *)((int)&in_ECX[0x21b].unused + 2) + iVar8;
        }
      }
      iVar8 = 0;
      local_18 = 0;
      if (0 < local_20) {
        piVar1 = (int *)((int)&in_ECX[0x218].unused + 2);
        do {
          if (*(int *)(*piVar1 + -8) <= iVar8) break;
          CString_Construct(&DAT_008aba70);
          local_4 = 0;
          uVar4 = CString_Mid(local_14,iVar8);
          local_4._0_1_ = 1;
          iVar5 = CUtil::FindOneOf(uVar4,local_1c,0);
          local_4 = (uint)local_4._1_3_ << 8;
          CString_Destruct();
          local_4 = 0xffffffff;
          CString_Destruct();
          if (iVar8 + iVar5 == -1) {
            iVar8 = *(int *)(*piVar1 + -8);
          }
          else {
            iVar8 = iVar8 + iVar5 + 1;
          }
          local_18 = local_18 + 1;
        } while (local_18 < local_20);
      }
      iVar5 = *(int *)(*(int *)((int)&in_ECX[0x218].unused + 2) + -8);
      piVar1 = (int *)((int)&in_ECX[0x218].unused + 2);
      if (iVar8 == iVar5) {
        *(int *)((int)&in_ECX[0x21a].unused + 2) = iVar5;
        CResCell::Release();
        piVar1 = *(int **)((int)&in_ECX[1].unused + 2);
        *(undefined4 *)((int)&in_ECX[0x223].unused + 2) =
             *(undefined4 *)((int)&in_ECX[0x21a].unused + 2);
        iVar8 = (**(code **)(**(int **)(*piVar1 + 0x10) + 0x44))();
        if (iVar8 == 0) {
          *(undefined4 *)((int)&in_ECX[0x222].unused + 2) =
               *(undefined4 *)((int)&in_ECX[0x223].unused + 2);
        }
        *(undefined1 *)&in_ECX[0x222].unused = 1;
      }
      else {
        CString_Construct(&DAT_008aba70);
        local_4 = 2;
        uVar4 = CString_Mid(&local_10,iVar8);
        local_4._0_1_ = 3;
        iVar5 = CUtil::FindOneOf(uVar4,&local_18,0);
        local_4 = CONCAT31(local_4._1_3_,2);
        CString_Destruct();
        local_4 = 0xffffffff;
        CString_Destruct();
        if (iVar5 == -1) {
          iVar5 = *(int *)(*piVar1 + -8);
        }
        uVar4 = CString_Mid(&local_10,iVar8,iVar5);
        local_4 = 4;
        iVar6 = CVidFont::GetStringLength(uVar4,0);
        local_4 = 0xffffffff;
        CString_Destruct();
        if (iVar6 < param_1) {
          local_24 = iVar5 + 1;
        }
        else {
          uVar4 = CString_Mid(&local_20,iVar8,0);
          local_4 = 5;
          iVar5 = CVidFont::GetStringLength(uVar4,0);
          local_4 = 0xffffffff;
          CString_Destruct();
          while (iVar5 < param_1) {
            if (*(int *)(DAT_008cf6d8 + 0x1a0) != 0) {
              TestChar = *(BYTE *)(*piVar1 + iVar8 + -1 + local_24);
              uVar2 = (uint)local_10 >> 8;
              local_10 = CONCAT31((int3)uVar2,TestChar);
              BVar7 = IsDBCSLeadByte(TestChar);
              if (BVar7 != 0) {
                local_24 = local_24 + 1;
              }
            }
            local_24 = local_24 + 1;
            if (local_24 == *(int *)(*piVar1 + -8)) break;
            uVar4 = CString_Mid(&local_20,iVar8,local_24);
            local_4 = 5;
            iVar5 = CVidFont::GetStringLength(uVar4,0);
            local_4 = 0xffffffff;
            CString_Destruct();
          }
        }
        CResCell::Release();
        if ((*(int *)(DAT_008cf6d8 + 0x1a0) != 0) &&
           (iVar5 = CUtil::IsDBCSTrailByte(piVar1,iVar8 + -1 + local_24), iVar5 != 0)) {
          local_24 = local_24 + -1;
        }
        local_24 = iVar8 + -1 + local_24;
        iVar8 = *(int *)(*piVar1 + -8);
        if (local_24 < *(int *)(*piVar1 + -8)) {
          iVar8 = local_24;
        }
        *(int *)((int)&in_ECX[0x21a].unused + 2) = iVar8;
        *(int *)((int)&in_ECX[0x223].unused + 2) = iVar8;
        iVar8 = (**(code **)(**(int **)(**(int **)((int)&in_ECX[1].unused + 2) + 0x10) + 0x44))();
        if (iVar8 == 0) {
          *(undefined4 *)((int)&in_ECX[0x222].unused + 2) =
               *(undefined4 *)((int)&in_ECX[0x223].unused + 2);
        }
        *(undefined1 *)&in_ECX[0x222].unused = 1;
        (**(code **)(in_ECX->unused + 0x50))();
      }
    }
    uVar4 = 1;
  }
  ExceptionList = local_c;
  return uVar4;
}
```

## Callers

- 0x84CB70 (not in a function)
- 0x84CBEC (not in a function)
- 0x85679C (not in a function)
- 0x85CFC0 (not in a function)

## Callees

- 0x94 KERNEL32.DLL::IsDBCSLeadByte
- 0x4D4030 CUIManager::SetCapture
- 0x4D9830 CUIControlEditMultiLine::ReConstitute
- 0x77F4E0 CResCell::Demand
- 0x77F820 CResCell::Release
- 0x781180 CUtil::FindOneOf
- 0x7815D0 CUtil::IsDBCSTrailByte
- 0x792D40 CVidFont::GetStringLength
- 0x793050 CVidFont::GetFontHeight
- 0x7FA93D CString_Mid
- 0x7FA960 CString_Mid
- 0x7FCC1A CString_Destruct
- 0x7FCC88 CString_Construct
- 0x7FCD07 CString_operator_equ

## Notes

Ported (was a TODO stub), from the disassembly (0x4DABD0-0x4DB05E). Outside the text area with the focus: a pending edit (field_872) restores the saved text and reflows, the capture goes and the selection ends. Inside: takes the mouse capture (CUIManager::MOUSELBUTTON, 0x84C89C), finds the clicked line (the visible line under the cursor, at most the last), walks the lines to it and the characters to the click's x with the font's string lengths (with the double-byte steps), and puts the cursor there (the line's end right of its text); the selection's end follows and, without Shift, its start too; field_888 starts the drag. NOTEs in the code: the original adds the find's result before checking for -1, so a line search past the last break stays put; the column loop compares the column with the whole text's length.
