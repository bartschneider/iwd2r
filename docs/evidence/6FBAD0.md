---
address: 0x6FBAD0
name: CGameSprite::ChangeDirection
exemption: "needs a sprite with an animation and the game's world timer; tests/test_sprite_actions.cpp [ai][data] checks the turn gating"
kind: read-only
ghidra_version: 12.1.4
---

# 0x6FBAD0 CGameSprite::ChangeDirection

## Decompiled (Ghidra)

```c
char * CGameSprite::ChangeDirection(void)

{
  int *piVar1;
  undefined2 uVar2;
  byte bVar3;
  undefined1 uVar4;
  short sVar5;
  char *pcVar6;
  int iVar7;
  int in_ECX;
  uint uVar8;
  uint uVar9;
  int unaff_EBX;
  int iStack_1c;
  undefined1 auStack_18 [4];
  uint uStack_14;
  int iStack_10;
  undefined1 auStack_c [12];
  
  pcVar6 = (char *)(uint)*(ushort *)(in_ECX + 0x537e);
  if (*(ushort *)(in_ECX + 0x537e) != *(ushort *)(in_ECX + 0x5380)) {
    if (*(int *)(in_ECX + 0x50f0) == 0) {
      CUtil::UtilAssert(0x827,s___Include_ObjAnimation_h_008a64f0,s_m_animation____NULL_008a650c,0);
    }
    bVar3 = (**(code **)(**(int **)(in_ECX + 0x50f0) + 0x40))();
    if (bVar3 < 0xc) {
      uVar9 = *(uint *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x1b78);
      if (*(int *)(in_ECX + 0x50f0) == 0) {
        CUtil::UtilAssert(0x827,s___Include_ObjAnimation_h_008a64f0,s_m_animation____NULL_008a650c,0
                         );
      }
      uVar4 = (**(code **)(**(int **)(in_ECX + 0x50f0) + 0x40))();
      uStack_14 = CONCAT31(uStack_14._1_3_,uVar4);
      if (*(int *)(in_ECX + 0x50f0) == 0) {
        CUtil::UtilAssert(0x827,s___Include_ObjAnimation_h_008a64f0,s_m_animation____NULL_008a650c,0
                         );
      }
      bVar3 = (**(code **)(**(int **)(in_ECX + 0x50f0) + 0x40))();
      uVar8 = 2 - (uStack_14 & 0xff) / 6;
      if (uVar9 % uVar8 != *(uint *)(in_ECX + 0x5c) % (2 - bVar3 / 6)) {
        return (char *)(uVar9 / uVar8);
      }
    }
    iStack_1c = CONCAT31(iStack_1c._1_3_,bVar3);
    if ((0 < *(short *)(in_ECX + 0x537a)) &&
       (sVar5 = *(short *)(in_ECX + 0x537a) + -2, *(short *)(in_ECX + 0x537a) = sVar5, sVar5 < 2)) {
      uVar9 = *(short *)(in_ECX + 0x5380) + 0x10 + *(short *)(in_ECX + 0x537c) * 2 & 0x8000000f;
      if ((int)uVar9 < 0) {
        uVar9 = (uVar9 - 1 | 0xfffffff0) + 1;
      }
      *(short *)(in_ECX + 0x5380) = (short)uVar9;
    }
    uVar9 = *(short *)(in_ECX + 0x5380) + 0x10 + (int)*(short *)(in_ECX + 0x537c) & 0x8000000f;
    if ((int)uVar9 < 0) {
      uVar9 = (uVar9 - 1 | 0xfffffff0) + 1;
    }
    sVar5 = (short)uVar9;
    *(short *)(in_ECX + 0x5380) = sVar5;
    if (*(short *)(in_ECX + 0x537e) != sVar5) {
      uVar9 = sVar5 + 0x10 + (int)*(short *)(in_ECX + 0x537c) & 0x8000000f;
      if ((int)uVar9 < 0) {
        uVar9 = (uVar9 - 1 | 0xfffffff0) + 1;
      }
      *(short *)(in_ECX + 0x5380) = (short)uVar9;
    }
    uVar2 = *(undefined2 *)(in_ECX + 0x5380);
    if (*(int *)(in_ECX + 0x50f0) == 0) {
      CUtil::UtilAssert(0x81a,s___Include_ObjAnimation_h_008a64f0,s_m_animation____NULL_008a650c,0);
    }
    (**(code **)(**(int **)(in_ECX + 0x50f0) + 0xc))(uVar2);
    pcVar6 = *(char **)(in_ECX + 0x920);
    if (((uint)pcVar6 & 0x1000) == 0) {
      if ((*(int *)(*(int *)(DAT_008cf6dc + 0x1c54) + 0x4416) == 0) &&
         (pcVar6 = (char *)CInfGame::GetCharacterPortraitNum(*(undefined4 *)(in_ECX + 0x5c)),
         (short)pcVar6 != -1)) {
        return pcVar6;
      }
      pcVar6 = *(char **)(DAT_008cf6dc + 0x1c54);
      if (*(int *)(in_ECX + 0x12) == *(int *)(pcVar6 + (uint)(byte)pcVar6[0x37e0] * 4 + 0x37e2)) {
        *(undefined1 *)(in_ECX + 0x4dff) = 0;
        CSound::Stop();
        if (*(int *)(in_ECX + 0x50f0) == 0) {
          CUtil::UtilAssert(0x82e,s___Include_ObjAnimation_h_008a64f0,s_m_animation____NULL_008a650c
                            ,0);
        }
        pcVar6 = (char *)(**(code **)(**(int **)(in_ECX + 0x50f0) + 0x60))();
        if (*pcVar6 != '\0') {
          CResRef::CResRef(pcVar6);
          piVar1 = (int *)(in_ECX + 0x4d3a + (uint)*(byte *)(in_ECX + 0x4dff) * 100);
          if ((unaff_EBX != piVar1[2]) || (iStack_1c != piVar1[3])) {
            if ((piVar1[1] != 0) &&
               ((iVar7 = CResRef::operator!=(&DAT_008c5cf8), iVar7 != 0 && (*piVar1 != 0)))) {
              CRes::CancelRequest();
            }
            iVar7 = CResRef::operator==(&DAT_008c5cf8);
            if (iVar7 == 0) {
              iVar7 = CDimm::GetResObject(&stack0xffffffe0,4,1);
              if (iVar7 == 0) {
                piVar1[1] = 0;
                CResRef::operator=(auStack_c,&DAT_008c5cf8);
              }
              else {
                piVar1[1] = iVar7;
                *piVar1 = 1;
                CRes::Request();
                CResRef::operator=(auStack_c,&stack0xffffffe0);
              }
            }
            else {
              piVar1[1] = 0;
              CResRef::operator=(auStack_c,&DAT_008c5cf8);
            }
          }
          operator_delete(pcVar6);
          CSoundMixer::GetListenPosition(&uStack_14,auStack_18);
          iStack_10 = iStack_10 - *(int *)(in_ECX + 10);
          iVar7 = uStack_14 - *(int *)(in_ECX + 6);
          iVar7 = iVar7 * iVar7;
          uVar9 = 99 - (((iStack_10 * iStack_10) / 0x90 +
                        ((int)(iVar7 + (iVar7 >> 0x1f & 0xffU)) >> 8)) * 99) / 0x1900;
          CSound::SetPriority(uVar9 & ((int)uVar9 < 0) - 1);
          pcVar6 = (char *)CSound::Play(*(undefined4 *)(in_ECX + 6),*(undefined4 *)(in_ECX + 10),
                                        *(undefined4 *)(in_ECX + 0xe),0);
        }
      }
    }
  }
  return pcVar6;
}
```

## Callers

- 0x6F5FF0 CGameSprite::AIUpdate (call at 0x6F7629)

## Callees

- 0x5AF4E0 CInfGame::GetCharacterPortraitNum
- 0x77E370 CRes::CancelRequest
- 0x77E610 CRes::Request
- 0x780C00 CUtil::UtilAssert
- 0x786DF0 CDimm::GetResObject
- 0x78A990 CResRef::CResRef
- 0x78AB40 CResRef::operator!=
- 0x78ABB0 CResRef::operator==
- 0x78AC30 CResRef::operator=
- 0x78AD40 CResRef::operator=
- 0x7A9DB0 CSound::Play
- 0x7AA6C0 CSound::SetPriority
- 0x7AA910 CSound::Stop
- 0x7AB310 CSoundMixer::GetListenPosition
- 0x7FC984 operator_delete

## Notes

Turns the sprite one step toward m_nNewDirection (+0x537E) and plays its armor sound. vis-probe round 5: the gate at 0x6FBAEC-0x6FBBEE was missing from ours. With the animation's move scale (m_animation's virtual GetMoveScale, slot 0x40, read three times) below twice the byte at 0x85AB4C (6), the turn happens only when the world time (CInfGame::m_worldTime.m_gameTime, +0x1B78) and the sprite's id agree modulo (2 - scale / 6) (unsigned divisions): a slow walker (scale below 6) turns every other world tick, on the ticks its id's parity picks; otherwise it returns before turning. Ours turned (and drew GetSndArmor's rand) every update: Jorun at walk_and_talk frame 2953. The rest (the skip delta, the two delta steps, ChangeDirection on the animation, slot 0xC, and the armor sound when not silenced, footsteps on or not a party member, in the area on view) matches ours.
