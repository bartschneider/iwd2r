---
address: 0x4CECB0
name: CGameTrigger::IsOverActivate
exemption: "needs an area with its triggers; pinned by tests/test_creature_render.cpp "trigger: a creature inside a travel region sets it off""
kind: read-only
ghidra_version: 12.1.4
---

# 0x4CECB0 CGameTrigger::IsOverActivate

## Decompiled (Ghidra)

```c
undefined1 CGameTrigger::IsOverActivate(POINT *param_1)

{
  undefined1 uVar1;
  BOOL BVar2;
  int in_ECX;
  
  BVar2 = PtInRect((RECT *)(in_ECX + 0x59a),*param_1);
  if (BVar2 == 0) {
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

- 0x84C874 (not in a function)

## Callees

- 0x126 USER32.DLL::PtInRect
- 0x7C0E40 CVidPoly::IsPtInPoly

## Notes

Checked against the disassembly (0x4CECB0-0x4CED04).
`PtInRect(&m_rBounding, pt)` (+0x59A): a zero result returns FALSE
(`test eax, eax; jne 0x4CECD5` at 0x4CECCC). Without a polygon (+0x5DE)
the trigger is set off anywhere in its rectangle; otherwise IsPtInPoly
(0x7C0E40) with the polygon and its count (+0x5E2) decides.

Fixed: ours returned FALSE inside the rectangle, as IsOver (0x4CEBE0)
did. So CGameTrigger::DoAIUpdate (0x4CD220), which asks this of each
sprite's position, never set off a trap or a travel region.

The replays of walk_and_talk and world_entry (seed 1) still match the
original's recorded dumps at every checkpoint: 35 and 27. The party does
not enter a region in either.
