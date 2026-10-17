---
address: 0x7AEAD0
name: CVidCell::Render
exemption: render API dispatch guard
kind: read-only
ghidra_version: 12.1.4
---

# 0x7AEAD0 CVidCell::Render

## Decompiled (Ghidra)

```c
int CVidCell::Render(int param_1,int param_2,int param_3,undefined4 param_4,int param_5,
                    undefined4 param_6,uint param_7,undefined4 param_8)

{
  int *piVar1;
  short sVar2;
  ushort *puVar3;
  undefined4 uVar4;
  int iVar5;
  int iVar6;
  int iVar7;
  int *in_ECX;
  int *piVar8;
  int yTop;
  int unaff_retaddr;
  undefined4 uStack_114;
  int iStack_110;
  int iStack_10c;
  int iStack_108;
  undefined4 uStack_104;
  tagRECT tStack_fc;
  tagRECT tStack_ec;
  undefined4 auStack_dc [4];
  undefined4 uStack_cc;
  undefined4 uStack_b8;
  undefined4 auStack_70 [20];
  undefined4 uStack_20;
  int iStack_8;
  int iStack_4;
  
  if (*(int *)(DAT_008cf6d8 + 0x91c) != 0) {
    iVar5 = Render3d(param_2,param_3,param_4,param_5,param_6,param_7 | 1,param_8,0);
    return iVar5;
  }
  iVar5 = CVidMode_GetNumSurfaces();
  if (param_1 < iVar5) {
    if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
      iVar5 = 0;
    }
    else {
      iVar5 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
    }
    if (*(int *)(*(int *)(iVar5 + 0xa0) + param_1 * 4) != 0) goto LAB_007aeb9b;
  }
  CUtil::UtilAssert(0x811,s_C__Projects_Icewind2_src_chitin__008bac90,
                    s_nSurface_<_g_pChitin_>GetCurrent_008badc0,0);
LAB_007aeb9b:
  piVar1 = in_ECX + 1;
  if (in_ECX[1] == -1) {
    if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
      piVar8 = (int *)0x0;
    }
    else {
      piVar8 = *(int **)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
    }
    iVar5 = (**(code **)(*piVar8 + 0xc4))(piVar1,param_7);
    if (iVar5 == 0) {
      return 0;
    }
  }
  if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
    iVar5 = 0;
  }
  else {
    iVar5 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
  }
  piVar8 = *(int **)(*(int *)(iVar5 + 0xa0) + *piVar1 * 4);
  (**(code **)(*piVar8 + 0x40))(piVar8,8,&tStack_fc.top);
  if ((in_ECX[0x34] == 0) && (iVar5 = (**(code **)(*in_ECX + 0x18))(0), iVar5 == 0)) {
    return 0;
  }
  puVar3 = (ushort *)in_ECX[0x34];
  iVar5 = iStack_4 - (short)puVar3[2];
  yTop = unaff_retaddr - (short)puVar3[3];
  SetRect(&tStack_fc,0,0,(uint)*puVar3,(uint)puVar3[1]);
  SetRect(&tStack_ec,iVar5,yTop,(uint)*(ushort *)in_ECX[0x34] + iVar5,
          (uint)((ushort *)in_ECX[0x34])[1] + yTop);
  if (((param_7 & 2) == 0) && (((param_7 & 4) == 0 || (DAT_008bac8c == 0)))) {
    auStack_70[0] = 100;
    uStack_20 = uStack_104;
    if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
      iVar6 = 0;
    }
    else {
      iVar6 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
    }
    CVidBlitter::Blt(*(undefined4 *)(*(int *)(iVar6 + 0xa0) + *piVar1 * 4),&tStack_fc,0,0,0x1000400,
                     auStack_70);
    if (*piVar1 == -1) {
      CUtil::UtilAssert(0x884,s_C__Projects_Icewind2_src_chitin__008bac90,
                        s_m_nFxSurface_____1_008afc18,0);
    }
    auStack_dc[0] = 0x6c;
    iVar6 = CVidMode::LockSurface(*piVar1,auStack_dc,&tStack_fc);
    if (iVar6 == 0) {
      CResCell::Release();
      return 0;
    }
    sVar2 = *(short *)(DAT_008cf6d8 + 0x7ea);
    uStack_114 = uStack_b8;
    if (sVar2 == 0x10) {
      iVar6 = sub_7AF1C0(uStack_b8,uStack_cc,param_7);
    }
    else if (sVar2 == 0x18) {
      iVar6 = sub_7CECD0(uStack_b8,uStack_cc,param_7);
    }
    else if (sVar2 == 0x20) {
      iVar6 = Blt8To32(uStack_b8,uStack_cc,param_7,0xff);
    }
    else {
      iVar6 = 0;
    }
  }
  else {
    if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
      iVar6 = 0;
    }
    else {
      iVar6 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
    }
    BltBackGroundToFx(*(undefined4 *)(*(int *)(iVar6 + 0xa0) + iStack_8 * 4),&tStack_ec,param_1,
                      param_7);
    if (*piVar1 == -1) {
      CUtil::UtilAssert(0x840,s_C__Projects_Icewind2_src_chitin__008bac90,
                        s_m_nFxSurface_____1_008afc18,0);
    }
    auStack_dc[0] = 0x6c;
    iVar6 = CVidMode::LockSurface(*piVar1,auStack_dc,&tStack_fc);
    uVar4 = uStack_b8;
    if (iVar6 == 0) {
      CResCell::Release();
      return 0;
    }
    uStack_114 = uStack_b8;
    if ((param_7 & 4) == 0) {
      if (param_5 < 0) {
        CUtil::UtilAssert(0x861,s_C__Projects_Icewind2_src_chitin__008bac90,
                          s_nTransVal_>__0_008bad40,0);
      }
      sVar2 = *(short *)(DAT_008cf6d8 + 0x7ea);
      if (sVar2 == 0x10) {
        iVar6 = sub_7AFED0(uVar4,uStack_cc,param_7,param_5);
      }
      else if (sVar2 == 0x18) {
        iVar6 = sub_7CF8D0(uVar4,uStack_cc,param_7,param_5);
      }
      else if (sVar2 == 0x20) {
        iVar6 = sub_7D0950(uVar4,uStack_cc,param_7,param_5);
      }
      else {
        iVar6 = 0;
      }
    }
    else {
      sVar2 = *(short *)(DAT_008cf6d8 + 0x7ea);
      if (sVar2 == 0x10) {
        iVar6 = sub_7AF8A0(uStack_b8,uStack_cc,param_7);
      }
      else if (sVar2 == 0x18) {
        iVar6 = sub_7CF3F0(uStack_b8,uStack_cc,param_7);
      }
      else if (sVar2 == 0x20) {
        iVar6 = Blt8To32ClearShadow(uStack_b8,uStack_cc,param_7);
      }
      else {
        iVar6 = 0;
      }
    }
  }
  if ((param_2 != 0) && (0 < param_3)) {
    iStack_110 = param_3;
    do {
      iStack_10c = iVar5;
      iStack_108 = yTop;
      CVidPoly::FillPoly(uStack_114,uStack_cc,&tStack_fc,uStack_104,0,&iStack_10c);
      iStack_110 = iStack_110 + -1;
    } while (iStack_110 != 0);
  }
  CVidMode::UnLockSurface(in_ECX[1],uStack_b8);
  if (iVar6 != 0) {
    if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
      iVar7 = 0;
    }
    else {
      iVar7 = *(int *)(*(int *)(DAT_008cf6d8 + 0x3c4) + 4);
    }
    BltFromFX(*(undefined4 *)(*(int *)(iVar7 + 0xa0) + iStack_8 * 4),iVar5,yTop,param_1,param_7);
  }
  CResCell::Release();
  in_ECX[0x34] = 0;
  return iVar6;
}
```

## Callers

- 0x4E66E0 CIcon::RenderIcon (call at 0x4E6895)
- 0x4E66E0 CIcon::RenderIcon (call at 0x4E69A6)
- 0x4E66E0 CIcon::RenderIcon (call at 0x4E6A96)
- 0x4E66E0 CIcon::RenderIcon (call at 0x4E6C8C)
- 0x5950F0 CButtonArray_PostRender (call at 0x595355)
- 0x5950F0 CButtonArray_PostRender (call at 0x595662)
- 0x5950F0 CButtonArray_PostRender (call at 0x595714)
- 0x5F7340 CUIControlButtonCharacterFeatsCircle::Render (call at 0x5F7738)
- 0x6236B0 CUIControlButtonCharGenFeatsCircle::Render (call at 0x623ACA)
- 0x6331D0 CUIControlButtonInventoryRequesterItem::Render (call at 0x633486)
- 0x66E6A0 CUIControlButtonSpellbookSpellInfoIcon::Render (call at 0x66E96E)
- 0x6817A0 CUIControlButtonStoreStoreItem::Render (call at 0x681A64)
- 0x6817A0 CUIControlButtonStoreStoreItem::Render (call at 0x681AF6)
- 0x6817A0 CUIControlButtonStoreStoreItem::Render (call at 0x681C24)
- 0x6817A0 CUIControlButtonStoreStoreItem::Render (call at 0x681CA1)
- 0x6817A0 CUIControlButtonStoreStoreItem::Render (call at 0x681D49)
- 0x6822F0 CUIControlButtonStoreGroupItem::Render (call at 0x6825B4)
- 0x6822F0 CUIControlButtonStoreGroupItem::Render (call at 0x682646)
- 0x6822F0 CUIControlButtonStoreGroupItem::Render (call at 0x682774)
- 0x6822F0 CUIControlButtonStoreGroupItem::Render (call at 0x6827F1)
- 0x6822F0 CUIControlButtonStoreGroupItem::Render (call at 0x682899)
- 0x682D10 CUIControlButtonStoreStoreSpell::Render (call at 0x682FE8)
- 0x683F00 CUIControlButtonStoreRequesterItem::Render (call at 0x6841B6)
- 0x696140 CUIControlButtonWorldContainerSlot::Render (call at 0x696472)
- 0x696140 CUIControlButtonWorldContainerSlot::Render (call at 0x69650D)
- 0x696140 CUIControlButtonWorldContainerSlot::Render (call at 0x69660B)
- 0x696140 CUIControlButtonWorldContainerSlot::Render (call at 0x696686)
- 0x696140 CUIControlButtonWorldContainerSlot::Render (call at 0x696730)
- 0x6975E0 CUIControlButtonWorldContainerIcon::Render (call at 0x697788)
- 0x79FBA0 CVidInf::RenderPointerImage (call at 0x79FF3F)
- 0x62DDE0 CUIControlButtonInventorySlot::Render (call at 0x62E38F)
- 0x62DDE0 CUIControlButtonInventorySlot::Render (call at 0x62E43B)
- 0x62DDE0 CUIControlButtonInventorySlot::Render (call at 0x62E542)
- 0x62DDE0 CUIControlButtonInventorySlot::Render (call at 0x62E5CA)
- 0x62DDE0 CUIControlButtonInventorySlot::Render (call at 0x62E677)
- 0x633F10 FUN_00633f10 (call at 0x634227)
- 0x85DD4C (not in a function)
- 0x85E6FC (not in a function)

## Callees

- 0x11B USER32.DLL::SetRect
- 0x77F820 CResCell::Release
- 0x780C00 CUtil::UtilAssert
- 0x799F80 CVidMode_GetNumSurfaces
- 0x799FA0 CVidMode::LockSurface
- 0x79A820 CVidMode::UnLockSurface
- 0x7AF1C0 CVidCell::sub_7AF1C0
- 0x7AF8A0 CVidCell::sub_7AF8A0
- 0x7AFED0 CVidCell::sub_7AFED0
- 0x7B0490 CVidCell::BltBackGroundToFx
- 0x7B06E0 CVidCell::BltFromFX
- 0x7B6840 CVidBlitter::Blt
- 0x7C13A0 CVidPoly::FillPoly
- 0x7C4A90 CVidCell::Render3d
- 0x7CECD0 CVidCell::sub_7CECD0
- 0x7CF3F0 CVidCell::sub_7CF3F0
- 0x7CF8D0 CVidCell::sub_7CF8D0
- 0x7CFBC0 CVidCell::Blt8To32
- 0x7D0470 CVidCell::Blt8To32ClearShadow
- 0x7D0950 CVidCell::sub_7D0950

## Notes

Ours adds one line at the top of the body, the render API dispatch guard (src/render/api.h): with a backend installed it calls Backend::DrawCell (the cell, its current frame, (x, y), its palette, dwFlags, nTransVal; clip rClip) and returns when the backend handled it; with none, the classic renderer, render::Active() is null, the guard falls through and the rest of the body is the decompiled one, unchanged. A handled call returns render::CellHandled(*this), the body's own tail: when the frame is fetched it releases the resource and clears m_pFrame, so the demand count is what the body would leave.

Render task 5, fix round 1: the 32-bpp translucent call to sub_7D0950 passes the bytes it may write, CVidCell::SurfaceExtent(ddsd), the surface this body locked (its first row to the end of its last). Ours only: the original's helper writes past the frame on raw frames, and ours keeps those writes inside the locked surface (ruling P4-10; docs/evidence/7D0950.md).
