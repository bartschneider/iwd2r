---
address: 0x4CCAB0
name: CGameTrigger::CGameTrigger
exemption: "needs an area, the object array and the ARE data; covered by tests/test_sprite_actions.cpp (trigger: DoAIUpdate ..., every AR1000 trigger's m_scriptRes is its override script's name); the polygon set-up is pinned by tests/test_creature_render.cpp 'area objects: AR1000's polygons keep the original's points'"
kind: read-only
ghidra_version: 12.1.4
---

# 0x4CCAB0 CGameTrigger::CGameTrigger

## Decompiled (Ghidra)

```c
void CGameTrigger::CGameTrigger(int param_1,char *param_2,int param_3,uint param_4)

{
  ushort uVar1;
  int iVar2;
  int iVar3;
  short sVar4;
  char cVar5;
  short sVar6;
  int iVar7;
  undefined4 uVar8;
  undefined2 *puVar9;
  undefined4 *in_ECX;
  int iVar10;
  char *pcVar11;
  uint uVar12;
  undefined4 *puVar13;
  undefined1 *puStack_6c;
  uint uStack_68;
  char acStack_60 [32];
  undefined2 uStack_40;
  undefined2 uStack_3e;
  undefined4 uStack_3c;
  undefined4 uStack_38;
  undefined4 uStack_34;
  undefined4 uStack_30;
  char acStack_2c [32];
  void *pvStack_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_00813f29;
  pvStack_c = ExceptionList;
  ExceptionList = &pvStack_c;
  CGameAIBase::CGameAIBase();
  local_4 = 0;
  CResRef::CResRef();
  FUN_007fb7de();
  in_ECX[0x18c] = &PTR_LAB_00848ed8;
  local_4._0_1_ = 1;
  *in_ECX = &PTR_FUN_0084c7bc;
  if ((((param_1 == 0) || (param_2 == (char *)0x0)) || (param_3 == 0)) ||
     ((param_4 & 0xffff) < (uint)*(ushort *)(param_2 + 0x2a) + *(int *)(param_2 + 0x2c))) {
    CUtil::UtilAssert(0x6e,s_C__Projects_Icewind2_src_Baldur__008ab6c8);
  }
  *(undefined1 *)(in_ECX + 1) = 0x41;
  *(undefined2 *)(in_ECX + 0x166) = *(undefined2 *)(param_2 + 0x20);
  *(uint *)((int)in_ECX + 0x59a) = (uint)*(ushort *)(param_2 + 0x22);
  *(uint *)((int)in_ECX + 0x59e) = (uint)*(ushort *)(param_2 + 0x24);
  *(uint *)((int)in_ECX + 0x5a2) = *(ushort *)(param_2 + 0x26) + 1;
  *(uint *)((int)in_ECX + 0x5a6) = *(ushort *)(param_2 + 0x28) + 1;
  if (*(int *)(param_2 + 0x34) == 0x1e) {
    *(undefined4 *)((int)in_ECX + 0x5aa) = 0x2a;
  }
  else {
    *(int *)((int)in_ECX + 0x5aa) = *(int *)(param_2 + 0x34);
  }
  *(undefined4 *)((int)in_ECX + 0x5ae) = *(undefined4 *)(param_2 + 0x38);
  *(undefined4 *)((int)in_ECX + 0x5b2) = *(undefined4 *)(param_2 + 0x3c);
  pcVar11 = param_2 + 0x40;
  puVar13 = (undefined4 *)((int)in_ECX + 0x5b6);
  for (iVar10 = 8; iVar10 != 0; iVar10 = iVar10 + -1) {
    *puVar13 = *(undefined4 *)pcVar11;
    pcVar11 = pcVar11 + 4;
    puVar13 = puVar13 + 1;
  }
  *(undefined4 *)((int)in_ECX + 0x5d6) = *(undefined4 *)(param_2 + 0x60);
  *(undefined4 *)((int)in_ECX + 0x5da) = *(undefined4 *)(param_2 + 100);
  *(undefined2 *)((int)in_ECX + 0x5e2) = *(undefined2 *)(param_2 + 0x2a);
  *(undefined4 *)((int)in_ECX + 0x5e6) = *(undefined4 *)(param_2 + 0x7c);
  *(undefined4 *)((int)in_ECX + 0x5ea) = *(undefined4 *)(param_2 + 0x80);
  _strncpy((char *)((int)in_ECX + 0x5ee),param_2,0x20);
  puStack_6c = (undefined1 *)operator_new(0x28);
  local_4._0_1_ = 2;
  if (puStack_6c != (undefined1 *)0x0) {
    CResRef::CResRef(param_2 + 0x7c);
    CAIScript::CAIScript();
  }
  local_4 = CONCAT31(local_4._1_3_,1);
  CGameAIBase::SetScript();
  *(undefined2 *)((int)in_ECX + 0x60e) = *(undefined2 *)(param_2 + 0x68);
  *(undefined2 *)(in_ECX + 0x184) = *(undefined2 *)(param_2 + 0x6a);
  *(undefined2 *)((int)in_ECX + 0x612) = *(undefined2 *)(param_2 + 0x6c);
  *(undefined2 *)(in_ECX + 0x185) = *(undefined2 *)(param_2 + 0x6e);
  uVar1 = *(ushort *)(param_2 + 0x72);
  *(uint *)((int)in_ECX + 0x616) = (uint)*(ushort *)(param_2 + 0x70);
  *(uint *)((int)in_ECX + 0x61a) = (uint)uVar1;
  CResRef::operator=();
  uVar1 = *(ushort *)((int)in_ECX + 0x5e2);
  *(undefined2 *)((int)in_ECX + 0x626) = 0;
  if (uVar1 == 0) {
    *(undefined4 *)((int)in_ECX + 0x5de) = 0;
  }
  else {
    iVar10 = operator_new();
    if (iVar10 == 0) {
      iVar10 = 0;
    }
    else {
      puStack_6c = (undefined1 *)(uVar1 - 1);
    }
    *(int *)((int)in_ECX + 0x5de) = iVar10;
    if (iVar10 == 0) {
      CUtil::UtilAssert(0x95,s_C__Projects_Icewind2_src_Baldur__008ab6c8);
      *(undefined2 *)((int)in_ECX + 0x5e2) = 0;
    }
    else {
      sVar4 = 0;
      sVar6 = 0;
      if (*(short *)((int)in_ECX + 0x5e2) != 0) {
        iVar10 = 0;
        do {
          iVar7 = iVar10 - sVar4;
          *(uint *)(*(int *)((int)in_ECX + 0x5de) + iVar7 * 8) =
               (uint)*(ushort *)(param_3 + (*(int *)(param_2 + 0x2c) + iVar10) * 4);
          *(uint *)(*(int *)((int)in_ECX + 0x5de) + 4 + iVar7 * 8) =
               (uint)*(ushort *)(param_3 + 2 + (*(int *)(param_2 + 0x2c) + iVar10) * 4);
          if (1 < sVar6) {
            puStack_6c = *(undefined1 **)((int)in_ECX + 0x5de);
            iVar10 = *(int *)(puStack_6c + iVar7 * 8 + -8);
            iVar2 = *(int *)(puStack_6c + iVar7 * 8 + -0x10);
            if ((iVar2 != iVar10) || (iVar10 != *(int *)(puStack_6c + iVar7 * 8))) {
              iVar3 = *(int *)(puStack_6c + iVar7 * 8 + -4);
              if (((*(int *)(puStack_6c + iVar7 * 8 + -0xc) != iVar3) ||
                  (iVar3 != *(int *)(puStack_6c + iVar7 * 8 + 4))) &&
                 (((iVar2 == iVar10 || (iVar10 == *(int *)(puStack_6c + iVar7 * 8))) ||
                  (((*(int *)(puStack_6c + iVar7 * 8 + -0xc) - iVar3) * 1000) / (iVar2 - iVar10) !=
                   ((iVar3 - *(int *)(puStack_6c + iVar7 * 8 + 4)) * 1000) /
                   (iVar10 - *(int *)(puStack_6c + iVar7 * 8)))))) goto LAB_004cce39;
            }
            *(undefined4 *)(puStack_6c + iVar7 * 8 + -8) = *(undefined4 *)(puStack_6c + iVar7 * 8);
            *(undefined4 *)(puStack_6c + iVar7 * 8 + -4) =
                 *(undefined4 *)(puStack_6c + iVar7 * 8 + 4);
            sVar4 = sVar4 + 1;
          }
LAB_004cce39:
          sVar6 = sVar6 + 1;
          iVar10 = (int)sVar6;
        } while (iVar10 < (int)(uint)*(ushort *)((int)in_ECX + 0x5e2));
      }
      *(short *)((int)in_ECX + 0x5e2) = *(short *)((int)in_ECX + 0x5e2) - sVar4;
    }
  }
  cVar5 = CGameObjectArray::Add(in_ECX + 0x17);
  if (cVar5 == '\0') {
    if (*(short *)(in_ECX + 0x166) == 0) {
      puStack_6c = (undefined1 *)
                   (((uint)*(ushort *)(param_2 + 0x26) + (uint)*(ushort *)(param_2 + 0x22)) / 2);
      uStack_68 = ((uint)*(ushort *)(param_2 + 0x28) + (uint)*(ushort *)(param_2 + 0x24)) / 2;
    }
    else if ((*(short *)(param_2 + 0x84) == 0) && (*(short *)(param_2 + 0x86) == 0)) {
      puStack_6c = (undefined1 *)
                   (((uint)*(ushort *)(param_2 + 0x26) + (uint)*(ushort *)(param_2 + 0x22)) / 2);
      uStack_68 = ((uint)*(ushort *)(param_2 + 0x28) + (uint)*(ushort *)(param_2 + 0x24)) / 2;
    }
    else {
      puStack_6c = (undefined1 *)
                   (((uint)*(ushort *)(param_2 + 0x26) + (uint)*(ushort *)(param_2 + 0x22)) / 2);
      uStack_68 = ((uint)*(ushort *)(param_2 + 0x28) + (uint)*(ushort *)(param_2 + 0x24)) / 2;
    }
    CGameObject::AddToArea(param_1,&puStack_6c);
    iVar10 = (int)(((uint)*(ushort *)(param_2 + 0x28) - (uint)*(ushort *)(param_2 + 0x24)) * 2) / 3;
    iVar7 = (int)((uint)*(ushort *)(param_2 + 0x26) - (uint)*(ushort *)(param_2 + 0x22)) / 2;
    if (iVar7 <= iVar10) {
      iVar7 = iVar10;
    }
    *(short *)(in_ECX + 0x179) = (short)iVar7;
    CString_Construct();
    local_4._0_1_ = 3;
    CString_operator_equ();
    local_4 = CONCAT31(local_4._1_3_,1);
    CString_Destruct();
    _strncpy(acStack_60,&DAT_008c5cf8,0x20);
    uStack_40 = 0;
    uStack_3e = 0;
    uStack_3c = 0;
    uStack_38 = 0;
    uStack_34 = 0;
    uStack_30 = 0;
    _strncpy(acStack_2c,&DAT_008c5cf8,0x20);
    puStack_6c = &stack0xffffff68;
    CString_Construct();
    CVariable::SetName();
    uStack_38 = in_ECX[0x17];
    (*(code *)**(undefined4 **)(param_1 + 0xada))();
    if ((*(uint *)(param_2 + 0x60) & 0x200) != 0) {
      in_ECX[0x18a] = *(undefined4 *)(param_2 + 0x88);
      in_ECX[0x18b] = *(undefined4 *)(param_2 + 0x8c);
    }
    CGameAIBase::sub_481890();
    puVar9 = *(undefined2 **)((int)in_ECX + 0x5de);
    uVar12 = (uint)*(ushort *)((int)in_ECX + 0x5e2);
    in_ECX[0x191] = 0;
    in_ECX[0x191] = 0;
    if (uVar12 != 0) {
      uVar8 = operator_new();
      in_ECX[0x191] = uVar8;
      if (uVar12 != 0) {
        iVar10 = 0;
        do {
          *(undefined2 *)(iVar10 + in_ECX[0x191]) = *puVar9;
          *(undefined2 *)(iVar10 + 2 + in_ECX[0x191]) = puVar9[2];
          puVar9 = puVar9 + 4;
          iVar10 = iVar10 + 4;
          uVar12 = uVar12 - 1;
        } while (uVar12 != 0);
      }
    }
  }
  else if (in_ECX != (undefined4 *)0x0) {
    (**(code **)*in_ECX)();
  }
  ExceptionList = pvStack_c;
  return;
}
```

## Callers

- 0x472DE0 CGameArea::Unmarshal (call at 0x47423C)

## Callees

- 0x44C4B0 CGameAIBase::CGameAIBase
- 0x452C60 CVariable::SetName
- 0x45D280 CGameAIBase::SetScript
- 0x45FE00 CAIScript::CAIScript
- 0x481890 CGameAIBase::sub_481890
- 0x4C7BE0 CGameObject::AddToArea
- 0x4CD130 FUN_004cd130
- 0x59A0F0 CGameObjectArray::Add
- 0x780C00 CUtil::UtilAssert
- 0x78A960 CResRef::CResRef
- 0x78A9D0 CResRef::CResRef
- 0x78AC50 CResRef::operator=
- 0x7E83A0 _strncpy
- 0x7FB7DE FUN_007fb7de
- 0x7FC95B operator_new
- 0x7FCC1A CString_Destruct
- 0x7FCC88 CString_Construct
- 0x7FCD07 CString_operator_equ

## Notes

Ported earlier; this entry records one fix. The original copies the ARE trigger's script name into m_scriptRes (+0x5E6): `mov ecx,[ebx+0x7C]; mov [ebp+0x5E6],ecx; mov edx,[esi+4]; mov [ebp+0x5EA],edx` at 0x4CCC01-0x4CCC1A, just before the strncpy of the script name (0x7E83A0, 0x20 bytes). Ours never set it, so m_scriptRes held whatever the allocation held; CGameTrigger::DoAIUpdate (0x4CD220) and Marshal read it. Replay-verified with IWD2R_RAND_TRACE (docs/rand-trace.md) on walk_and_talk, seed 1: with the round-2 fixes our AI thread's rand() calls in the load frame (Party Formation's Done, frame 1491) and the paused frames after it (1492-1497) match the original's call for call.

## Polygon set-up (render round 9)

A point is dropped only when it lies on the line through its two
neighbours. With x2,y2 two points back, x1,y1 the previous point and x0,y0
the current one, it goes when:

- x2 == x1 == x0, or y2 == y1 == y0; or
- x2 != x1, x1 != x0, and 1000*(y2-y1)/(x2-x1) == 1000*(y1-y0)/(x1-x0)
  (idiv truncation).

The equal-slope case is `cmp eax, <slope>; jne <keep>` at 0x4CCE1E (the loop 0x4CCD9C-0x4CCE35). Dropping
copies the current point over the previous one and counts it in `adjust`,
as ours does.

Fixed: ours compared the slopes with `!=`, so it dropped the middle point
whenever the two slopes *differed*, that is almost every point. Half-Built Ship kept 2 of its 35 points; Info Text for Cranes 16 of 42. AR1000
now keeps 292 points in its triggers, doors and containers, the count the
original saved (Probe Save of the crossload run 20261016-171655). Ours kept
133. The only point dropped is one of Barrel Three's ten. The test checks
all 54 polygons against that save.

## WSO delta

disagreed with WSO at e8dd16a: he renames field_628/field_630/field_644 and sub_481890 to member names our headers do not declare and moves the m_scriptRes copy above m_dwFlags; case dump-4CCAB0 decides

The three members are declared in CGameTrigger.h as field_628 (CPoint, +0x628),
field_630 (CArray<CRect*>, +0x630) and field_644 (CAreaPoint*, +0x644), and
the rect splitter is CGameAIBase::sub_481890 (0x481890) shared with the door
and container ctors; his m_ptUsePoint, m_boundingGrid, m_pPolygonPoints and
SplitRectIntoGrid exist nowhere in this tree, so his body does not compile
here. The moved memcpy stores a distinct destination from a distinct source
between stores to other distinct fields, so both orders are the same program.
On every point where the two bodies differ in behavior they do not: the
bounding rect, the cursor 30-to-42 swap, the pick points and the assert are
character-for-character equal, and both match the decompile. ours stands.
