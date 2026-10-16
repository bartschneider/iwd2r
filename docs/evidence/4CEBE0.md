---
address: 0x4CEBE0
name: CGameTrigger::IsOver
exemption: "needs an area with its triggers and a party; pinned by tests/test_creature_render.cpp "trigger: a travel region is picked inside its bounds""
ghidra_version: 12.1.4
---

# 0x4CEBE0 CGameTrigger::IsOver

## Decompiled (Ghidra)

```c
undefined1 CGameTrigger::IsOver(POINT *param_1)

{
  undefined1 uVar1;
  BOOL BVar2;
  int iVar3;
  int in_ECX;
  
  if ((*(uint *)(in_ECX + 0x5d6) & 0x100) != 0) {
    return 0;
  }
  if ((*(short *)(in_ECX + 0x598) == 0) &&
     ((((*(short *)(in_ECX + 0x612) == 0 || (*(short *)(in_ECX + 0x614) == 0)) ||
       (*(short *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x1b96) != 2)) ||
      (*(char *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x1b98) != '$')))) {
    return 0;
  }
  BVar2 = PtInRect((RECT *)(in_ECX + 0x59a),*param_1);
  if (BVar2 == 0) {
    return 0;
  }
  iVar3 = CAIGroup::IsPartyLeader();
  if (iVar3 == 0) {
    return 0;
  }
  if (*(int *)(in_ECX + 0x5de) == 0) {
    return 1;
  }
  uVar1 = CVidPoly::IsPtInPoly(*(int *)(in_ECX + 0x5de),*(undefined2 *)(in_ECX + 0x5e2),param_1);
  return uVar1;
}
```

## Callers

- 0x84C7F0 (not in a function)

## Callees

- 0x126 USER32.DLL::PtInRect
- 0x406250 CAIGroup::IsPartyLeader
- 0x7C0E40 CVidPoly::IsPtInPoly

## Notes

Checked against the disassembly (0x4CEBE0-0x4CECA9). Flag 0x100
(+0x5D6) makes the trigger unpickable. A trap (type 0, +0x598) is picked
only when it is activated (+0x612) and detected (+0x614), in game state 2
with icon 36. Then `PtInRect(&m_rBounding, pt)` (+0x59A): a zero result
returns FALSE (`test eax, eax; jne 0x4CEC5A` at 0x4CEC51). The group must
be led by a party member (0x406250). Last, the polygon (+0x5DE, count
+0x5E2) decides, and a trigger without one is picked anywhere in its
rectangle.

Fixed: ours returned FALSE *inside* the rectangle, so no trigger was ever
picked. In walk_and_talk, at the `door_open` checkpoint the mouse rests on
the opened AR1000_Door5 at world (845,900). The original picks the travel
region To1005 there and shows its cursor, 42 (the constructor turns the
file's door cursor 30 into 42). Ours picked nothing and showed the walk
cursor, 4. After the fix a headless replay of walk_and_talk (seed 1)
matches the original at all 35 checkpoints, and the cursor in the
`door_open` shot is the original's.

IsOverActivate (0x4CECB0) has the same inversion. It is not changed here:
it decides when a creature sets a trap or travel region off, which is
simulation, not the pointer.
