---
address: 0x7BCBB0
name: CVidMode::DrawEllipseHOctant3d
exemption: "needs a 3d-mode ellipse render; checked against the disassembly"
kind: read-only
ghidra_version: 12.1.4
---

# 0x7BCBB0 CVidMode::DrawEllipseHOctant3d

## Decompiled (Ghidra)

LONG CVidMode::DrawEllipseHOctant3d(char *param_1,undefined4 param_2)

{
  char cVar1;
  POINT pt;
  BOOL BVar2;
  int unaff_EBX;
  uint in_stack_00000018;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  RECT *lprc;
  
  iVar6 = 0xbe2;
  (*DAT_00907610)(0xbe2,param_1,param_2);
  CheckResults3d(0);
  iVar5 = 0xde1;
  (*DAT_00907610)();
  CheckResults3d(0);
  iVar4 = 0x3f800000;
  lprc = (RECT *)(float)(in_stack_00000018 & 0xff);
  (*DAT_009075a8)((float)lprc / 255.0,(float)(in_stack_00000018 >> 8 & 0xff) / 255.0,
                  (float)(in_stack_00000018 >> 0x10 & 0xff) / 255.0);
  CheckResults3d(0);
  iVar3 = 0x3f800000;
  (*DAT_00907830)();
  CheckResults3d(0);
  (*DAT_00907524)(0);
  if (lprc == (RECT *)0x0) {
    for (; iVar4 != 0; iVar4 = iVar4 + -1) {
      (*DAT_00907a14)((float)unaff_EBX + 0.2,(float)iVar3 + 0.2,0);
      cVar1 = *param_1;
      param_1 = param_1 + 1;
      if (cVar1 != '\0') {
        iVar3 = iVar3 + iVar6;
      }
      unaff_EBX = unaff_EBX + iVar5;
    }
  }
  else {
    for (; iVar4 != 0; iVar4 = iVar4 + -1) {
      pt.y = iVar3;
      pt.x = unaff_EBX;
      BVar2 = PtInRect(lprc,pt);
      if (BVar2 != 0) {
        (*DAT_00907a14)((float)unaff_EBX + 0.2,(float)iVar3 + 0.2,0);
      }
      cVar1 = *param_1;
      param_1 = param_1 + 1;
      if (cVar1 != '\0') {
        iVar3 = iVar3 + iVar6;
      }
      unaff_EBX = unaff_EBX + iVar5;
    }
  }
  (*DAT_0090763c)();
  if (*(int *)(DAT_008cf6d8 + 0x3c4) == 0) {
    CheckResults3d(0);
    return iVar3;
  }
  CheckResults3d(0);
  return iVar3;
}

## Callers

### CVidMode::DrawEllipse3d @ 0x7bc580

undefined4 CVidMode::DrawEllipse3d(undefined4 param_1,int *param_2)

{
  int iVar1;
  byte *pbVar2;
  int iVar3;
  int iVar4;
  int *in_ECX;
  int *unaff_EBP;
  undefined4 unaff_ESI;
  undefined4 unaff_retaddr;
  
  iVar1 = *param_2;
  if (*param_2 <= param_2[1]) {
    iVar1 = param_2[1];
  }
  pbVar2 = (byte *)operator_new(iVar1);
  iVar3 = (**(code **)(*in_ECX + 0xc0))(*param_2,param_2[1],pbVar2);
  iVar1 = *param_2;
  if (*param_2 <= param_2[1]) {
    iVar1 = param_2[1];
  }
  if (iVar1 < iVar3) {
    CUtil::UtilAssert(0x2a3,s_C__Projects_Icewind2_src_chitin__008bd5a8,
                      s_nArcLength_<__max(axis_cx,_axis__008ba3f0,0);
  }
  DrawEllipseHOctant3d
            (*unaff_EBP,unaff_EBP[1] - param_2[1],iVar3,0xffffffff,1,pbVar2,unaff_retaddr,param_1);
  iVar1 = iVar3 + -1;
  DrawEllipseHOctant3d
            (*unaff_EBP + 1,((uint)*pbVar2 - param_2[1]) + unaff_EBP[1],iVar1,1,1,pbVar2 + 1,
             unaff_retaddr,param_1);
  DrawEllipseHOctant3d
            (*unaff_EBP,unaff_EBP[1] + param_2[1],iVar3,0xffffffff,0xffffffff,pbVar2,unaff_retaddr,
             param_1);
  DrawEllipseHOctant3d
            (*unaff_EBP + 1,(unaff_EBP[1] - (uint)*pbVar2) + param_2[1],iVar1,1,0xffffffff,
             pbVar2 + 1,unaff_retaddr,param_1);
  iVar4 = (**(code **)(*in_ECX + 0xc0))(param_2[1],*param_2,pbVar2);
### CVidMode::DrawRecticle3d @ 0x7bc7a0

undefined4 CVidMode::DrawRecticle3d(int *param_1,int *param_2)

{
  short sVar1;
  byte *pbVar2;
  int iVar3;
  undefined4 uVar4;
  int iVar5;
  int *in_ECX;
  int unaff_EBP;
  undefined4 unaff_retaddr;
  tagRECT local_10;
  
  sVar1 = (short)param_1[2];
  if ((short)param_1[2] <= *(short *)((int)param_1 + 10)) {
    sVar1 = *(short *)((int)param_1 + 10);
  }
  pbVar2 = (byte *)operator_new((int)sVar1);
  SetRect(&local_10,*param_2,param_2[1],param_2[2] + -1,param_2[3] + -1);
  iVar3 = (int)*(short *)((int)param_1 + 10);
  local_10.right = (**(code **)(*in_ECX + 0xc0))((int)(short)param_1[2],iVar3,pbVar2);
  local_10.right = local_10.right + -1;
  sVar1 = (short)param_1[2];
  if ((short)param_1[2] <= *(short *)((int)param_1 + 10)) {
    sVar1 = *(short *)((int)param_1 + 10);
  }
  if (sVar1 <= local_10.right) {
    CUtil::UtilAssert(0x2f5,s_C__Projects_Icewind2_src_chitin__008bd5a8,
                      s_nArcLength_<_max(recticleDesc_xA_008ba424,0);
  }
  uVar4 = DrawEllipseHOctant3d
                    (*param_1,param_1[1] - unaff_EBP,local_10.right - (short)param_1[5],0xffffffff,1
                     ,pbVar2,local_10.bottom,unaff_retaddr);
  DrawLine3d(((short)param_1[5] - local_10.right) + *param_1,uVar4,*param_1,
             (*(short *)((int)param_1 + 0xe) - unaff_EBP) + param_1[1],&stack0xffffffe4,
             unaff_retaddr);
  uVar4 = DrawEllipseHOctant3d
                    (*param_1 + 1,((uint)*pbVar2 - unaff_EBP) + param_1[1],
                     (local_10.right - (short)param_1[5]) + -1,1,1,pbVar2 + 1,local_10.bottom,

## Callees

- 0x126 @ 0x126  (not in src)
- CVidMode::CheckResults3d @ 0x7bea80  CVideo3d.cpp:2365 BOOL CVidMode::CheckResults3d(int rc)

## Notes

Imported: the four vertex offsets add CVideo3d::SUB_PIXEL_SHIFT (0.2f,
src/CVideo3d.cpp:11) where ours added 0.5f behind NOTE comments; ours'
NOTE comments go with the deviation.

The exe adds the constant at ds:0x848398 (pool bytes cd cc 4c 3e = 0.2f)
before every glVertex3f call: 0x7BCD2B, 0x7BCD41 (x and y in one branch)
and 0x7BCDA1, 0x7BCDB7 (x and y in the other). Ours' bodies were otherwise
identical to the fork's.
