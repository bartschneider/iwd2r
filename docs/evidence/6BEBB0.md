---
address: 0x6BEBB0
name: CGameAnimationTypeMonsterLayered::Render
exemption: "a render function; checked by the world_entry render shot (tools/play.py --linux --headless, scenarios/pending/world_entry.txt)"
ghidra_version: 12.1.4
---

# 0x6BEBB0 CGameAnimationTypeMonsterLayered::Render

## Decompiled (Ghidra)

```c
void CGameAnimationTypeMonsterLayered::Render
               (int param_1,int param_2,undefined4 param_3,undefined4 *param_4,int *param_5,
               int *param_6,uint param_7,undefined4 param_8,undefined4 *param_9,undefined4 param_10,
               int param_11,int param_12,char param_13)

{
  int iVar1;
  char *pcVar2;
  uint uVar3;
  int in_ECX;
  uint uVar4;
  undefined4 *puVar5;
  int iStack_30;
  int iStack_2c;
  int iStack_28;
  int iStack_24;
  undefined4 uStack_20;
  undefined4 uStack_1c;
  undefined4 uStack_18;
  undefined4 uStack_14;
  undefined4 uStack_10;
  int iStack_c;
  undefined4 uStack_8;
  int iStack_4;
  
  if ((param_1 == 0) || (param_2 == 0)) {
    CUtil::UtilAssert(0x358b,s_C__Projects_Icewind2_src_Baldur__008b6234,
                      s_pInfinity____NULL____pVidMode____008a8a38,0);
  }
  if (*(int *)(in_ECX + 0x402) == 0) {
    CUtil::UtilAssert(0x358c,s_C__Projects_Icewind2_src_Baldur__008b6234,
                      s_m_currentVidCell____NULL_008aeb6c,0);
  }
  iStack_30 = *param_5;
  uStack_20 = *param_4;
  uStack_1c = param_4[1];
  uStack_18 = param_4[2];
  uStack_14 = param_4[3];
  iStack_2c = param_5[1] + param_12;
  if (DAT_008f8e14 == 0) {
    param_7 = param_7 | 4;
  }
  else {
    if ((short)(ushort)*(byte *)(in_ECX + 0xb3f) < *(short *)(in_ECX + 0xb34)) {
      param_7 = param_7 | 0x10;
    }
    if (param_13 == '\0') {
      uVar4 = param_7 | 0x101;
      goto LAB_006bec8c;
    }
  }
  uVar4 = param_7 | 0x80;
  if (param_13 != '\0') {
    uVar4 = param_7 | 0x82;
  }
LAB_006bec8c:
  CInfinity::FXPrep(&uStack_20,uVar4,param_3,&iStack_30,param_6);
  iVar1 = CInfinity::FXLock(&uStack_20,uVar4);
  if (iVar1 != 0) {
    CAIObjectType::SetRadius(param_8);
    if ((((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0x79a) != 0)) &&
        (pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0)) && (*pcVar2 != '\0')) {
      CAIObjectType::SetRadius(param_8);
    }
    if ((DAT_008f8e14 == 0) ||
       (*(short *)(in_ECX + 0xb34) <= (short)(ushort)*(byte *)(in_ECX + 0xb3f))) {
      uVar3 = (uint)*(ushort *)(in_ECX + 0xb34);
    }
    else {
      uVar3 = 0x10U - (int)*(short *)(in_ECX + 0xb34) & 0x8000000f;
      if ((int)uVar3 < 0) {
        uVar3 = (uVar3 - 1 | 0xfffffff0) + 1;
      }
    }
    switch((short)uVar3) {
    case 0:
    case 1:
    case 2:
      CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x402),*param_6,param_6[1],uVar4,param_13);
      if ((((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0x79a) != 0)) &&
          (pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0)) && (*pcVar2 != '\0')) {
        CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x79a),*param_6,param_6[1],uVar4,param_13);
      }
      break;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
      if ((((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0xb3a) == 0)) &&
          (*(int *)(in_ECX + 0x79a) != 0)) &&
         ((pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0 && (*pcVar2 != '\0')))) {
        CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x79a),*param_6,param_6[1],uVar4,param_13);
      }
      CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x402),*param_6,param_6[1],uVar4,param_13);
      if (((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0xb3a) != 0)) &&
         ((*(int *)(in_ECX + 0x79a) != 0 &&
          ((pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0 && (*pcVar2 != '\0')))))) {
        CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x79a),*param_6,param_6[1],uVar4,param_13);
      }
      break;
    case 8:
      if (((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0x79a) != 0)) &&
         ((pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0 && (*pcVar2 != '\0')))) {
        CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x79a),*param_6,param_6[1],uVar4,param_13);
      }
      CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x402),*param_6,param_6[1],uVar4,param_13);
      break;
    case 9:
    case 10:
    case 0xb:
    case 0xc:
    case 0xd:
    case 0xe:
    case 0xf:
      if (((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0xb3a) != 0)) &&
         ((*(int *)(in_ECX + 0x79a) != 0 &&
          ((pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0 && (*pcVar2 != '\0')))))) {
        CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x79a),*param_6,param_6[1],uVar4,param_13);
      }
      CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x402),*param_6,param_6[1],uVar4,param_13);
      if ((((*(int *)(in_ECX + 0xb36) != 0) && (*(int *)(in_ECX + 0xb3a) == 0)) &&
          (*(int *)(in_ECX + 0x79a) != 0)) &&
         ((pcVar2 = (char *)CResRef::GetResRef(), pcVar2 != (char *)0x0 && (*pcVar2 != '\0')))) {
        CInfinity::FXRender(*(undefined4 *)(in_ECX + 0x79a),*param_6,param_6[1],uVar4,param_13);
      }
      break;
    default:
      CUtil::UtilAssert(0x35f2,s_C__Projects_Icewind2_src_Baldur__008b6234,s_FALSE_008a636c,0);
    }
    uStack_10 = *param_9;
    iStack_c = param_9[1] - param_12;
    uStack_8 = param_9[2];
    iStack_4 = param_9[3] - param_12;
    CInfinity::FXRenderClippingPolys
              (iStack_30,iStack_2c - param_12,param_12,param_6,&uStack_10,param_10,uVar4);
    if (param_11 == 0) {
      iStack_28 = 0;
      iStack_24 = 0;
      puVar5 = (undefined4 *)0x0;
    }
    else {
      iStack_28 = *param_6 + iStack_30;
      iStack_24 = param_6[1] + iStack_2c;
      puVar5 = &uStack_20;
    }
    CInfinity::FXUnlock(uVar4,puVar5,&iStack_28);
    CInfinity::FXBltFrom(param_3,&uStack_20,iStack_30,iStack_2c,*param_6,param_6[1],uVar4);
  }
  return;
}
```

## Callers

- 0x85B5F8 (not in a function)

## Callees

- 0x5CE0A0 CInfinity::FXBltFrom
- 0x5CE130 CInfinity::FXPrep
- 0x5CE1F0 CInfinity::FXLock
- 0x5CE280 CInfinity::FXRender
- 0x5CE350 CInfinity::FXRenderClippingPolys
- 0x5CE930 CInfinity::FXUnlock
- 0x780C00 CUtil::UtilAssert
- 0x78AA20 CResRef::GetResRef
- 0x7ACD60 CAIObjectType::SetRadius

## Notes

In the not-fading branch (bFadeOut == 0) the original passes NULL as pFxRect: 0x6BF054 `xor eax,eax`, `je` on bFadeOut == 0, then 0x6BF08C `push eax` (0) as pFxRect after pushing &CPoint(0, 0), for CInfinity::FXUnlock 0x5CE930 at 0x6BF090. The fading branch passes &rFXRect and ptPos + ptReference. Ours passed &rFXRect in both branches; now NULL in the not-fading one. Once CVidPoly::FillPoly 0x7C13A0 is ported, a non-NULL pFxRect makes FXUnlock fill the rect with the transparent colour through a dithered FillPoly, so every creature would draw 50% dithered.

Address: slot 36 (Render) of the CGameAnimationTypeMonsterLayered vtable 0x85B568 (entry at 0x85B5F8) holds 0x6BEBB0.
