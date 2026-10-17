---
address: 0x7ADBC0
name: CVidCell::GetFrameCenterPoint
exemption: "reads the cell's resource"
kind: read-only
ghidra_version: 12.1.4
---

# 0x7ADBC0 CVidCell::GetFrameCenterPoint

## Decompiled (Ghidra)

```c
bool CVidCell::GetFrameCenterPoint(short param_1,short param_2,int *param_3,char param_4)

{
  undefined4 uVar1;
  char cVar2;
  int iVar3;
  int in_ECX;
  bool bVar4;
  
  if (param_1 == -1) {
    param_1 = *(short *)(in_ECX + 0xc6);
  }
  if (param_2 == -1) {
    param_2 = *(short *)(in_ECX + 0xc4);
    param_1 = *(short *)(in_ECX + 0xc6);
  }
  if (*(int *)(in_ECX + 0xa8) == 0) {
    CUtil::UtilAssert(0x498,s_C__Projects_Icewind2_src_chitin__008bac90,s_pRes____NULL_008afc08,0);
  }
  cVar2 = Demand(&param_4);
  if (cVar2 != '\0') {
    iVar3 = CResRef::operator!=(&DAT_008c5cf8);
    if (iVar3 == 0) {
      iVar3 = *(int *)(in_ECX + 0xa8);
      if (*(int *)(iVar3 + 0x7e) == 0) {
        iVar3 = *(int *)(iVar3 + 0x54);
      }
      else {
        iVar3 = *(int *)(iVar3 + 0x58);
      }
    }
    else {
      iVar3 = *(int *)(*(int *)(in_ECX + 0xb8) + 0x58);
    }
    if ((short)(ushort)*(byte *)(iVar3 + 10) <= param_1) {
      CUtil::UtilAssert(0x4a5,s_C__Projects_Icewind2_src_chitin__008bac90,
                        s_nSequence_<_GetResBamHeader()_>n_008bad14,0);
    }
    iVar3 = CResRef::operator!=(&DAT_008c5cf8);
    if (iVar3 == 0) {
      iVar3 = *(int *)(*(int *)(in_ECX + 0xa8) + 0x60);
    }
    else {
      iVar3 = *(int *)(*(int *)(in_ECX + 0xb8) + 0x60);
    }
    if (*(short *)(iVar3 + param_1 * 4) <= param_2) {
      CUtil::UtilAssert(0x4a6,s_C__Projects_Icewind2_src_chitin__008bac90,
                        s_nFrame_<_GetResSequences()[nSequ_008bace4,0);
    }
    uVar1 = *(undefined4 *)(in_ECX + 0xd6);
    iVar3 = CResRef::operator!=(&DAT_008c5cf8);
    if (iVar3 == 0) {
      iVar3 = CResCell::GetFrame(param_1,param_2,uVar1);
    }
    else {
      iVar3 = CResCellHeader::GetFrame(param_1,param_2,uVar1);
    }
    bVar4 = iVar3 != 0;
    if (bVar4) {
      *param_3 = (int)*(short *)(iVar3 + 4);
      param_3[1] = (int)*(short *)(iVar3 + 6);
    }
    if (param_4 == '\0') {
      iVar3 = CResRef::operator!=(&DAT_008c5cf8);
      if (iVar3 != 0) {
        CResCellHeader::Release();
        return bVar4;
      }
      CResCell::Release();
    }
    return bVar4;
  }
  return false;
}
```

## Callers

- 0x4E52B0 CUIControlScrollBar::Render (call at 0x4E54D6)
- 0x4E52B0 CUIControlScrollBar::Render (call at 0x4E5711)
- 0x69CC10 CScreenWorldMap::GetAreaRect (call at 0x69CCDE)

## Callees

- 0x77F520 CResCell::GetFrame
- 0x77F820 CResCell::Release
- 0x77FDA0 CResCellHeader::GetFrame
- 0x77FE40 CResCellHeader::Release
- 0x780C00 CUtil::UtilAssert
- 0x78AB40 CResRef::operator!=
- 0x7AD490 CVidCell::Demand

## Notes

Checked against the decompile. Matches: -1 picks the current sequence (and frame, which also takes the current sequence), the pRes assert (line 1176), FALSE when the demand fails, the signed sequence and frame asserts (1189, 1190), the frame's centre from the header or cell resource with m_bDoubleSize, released unless demanded. Only the comment on the frame assert's line changed: 1190 (0x4A6), not 1189.
