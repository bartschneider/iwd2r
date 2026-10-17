---
address: 0x5BDFE0
name: CInfGame_B3AUTO_005bdfe0
exemption: "needs the party in the object array; reached only by CAIObjectType::Decode (0x40C445, whose cases are still a TODO in CAIObjectType.cpp)"
kind: read-only
ghidra_version: 12.1.4
---

# 0x5BDFE0 CInfGame_B3AUTO_005bdfe0

## Decompiled (Ghidra)

```c
undefined4 CInfGame_B3AUTO_005bdfe0(int *param_1)

{
  char cVar1;
  short sVar2;
  int iVar3;
  int in_ECX;
  undefined4 uVar4;
  byte bVar5;
  byte bVar6;
  ushort uVar7;
  int iVar8;
  short sVar9;
  bool bVar10;
  undefined4 local_8;
  int local_4;
  
  iVar8 = 0;
  local_8 = 0xffffffff;
  local_4 = 0;
  switch(param_1._0_2_) {
  case 1:
    sVar2 = 0;
    uVar7 = 0x7fff;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar2 < *(short *)(in_ECX + 0x3846)) goto LAB_005be032;
        uVar4 = 0xffffffff;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), cVar1 != '\x04' &&
               (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            if ((param_1[0x248] & 0x800U) == 0) {
              bVar6 = *(byte *)((int)param_1 + 0x966);
              if (bVar6 < uVar7) {
                if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                  local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
                }
                else {
                  local_8 = 0xffffffff;
                }
                iVar3 = CDerivedStats::GetClassMaskLevel(0xd1);
              }
              else {
                if ((bVar6 != uVar7) ||
                   (iVar3 = CDerivedStats::GetClassMaskLevel(0xd1), iVar8 <= iVar3))
                goto LAB_005be0f9;
                if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                  local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
                }
                else {
                  local_8 = 0xffffffff;
                }
                bVar6 = *(byte *)((int)param_1 + 0x966);
              }
              uVar7 = (ushort)bVar6;
              iVar8 = iVar3;
            }
LAB_005be0f9:
            if (sVar2 < *(short *)(in_ECX + 0x3846)) {
              uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
            }
            else {
              uVar4 = 0xffffffff;
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar2 = sVar2 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar2) {
            return local_8;
          }
LAB_005be032:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
        }
      } while( true );
    }
    break;
  case 2:
    sVar2 = 0;
    bVar6 = 0;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar2 < *(short *)(in_ECX + 0x3846)) goto LAB_005be16d;
        uVar4 = 0xffffffff;
        bVar5 = bVar6;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), bVar6 = bVar5,
               cVar1 != '\x04' && (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            if ((param_1[0x248] & 0x800U) == 0) {
              bVar6 = *(byte *)((int)param_1 + 0x966);
              bVar10 = bVar6 == bVar5;
              if (bVar5 < bVar6) {
                if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                  local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
                }
                else {
                  local_8 = 0xffffffff;
                }
                iVar8 = CDerivedStats::GetClassMaskLevel(0xd1);
              }
              else {
                bVar6 = bVar5;
                if ((bVar10) && (iVar3 = CDerivedStats::GetClassMaskLevel(0xd1), iVar8 < iVar3)) {
                  if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                    local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
                  }
                  else {
                    local_8 = 0xffffffff;
                  }
                  bVar6 = *(byte *)((int)param_1 + 0x966);
                  iVar8 = iVar3;
                }
              }
            }
            if (sVar2 < *(short *)(in_ECX + 0x3846)) {
              uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
            }
            else {
              uVar4 = 0xffffffff;
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar2 = sVar2 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar2) {
            return local_8;
          }
LAB_005be16d:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
          bVar5 = bVar6;
        }
      } while( true );
    }
    break;
  case 3:
    sVar9 = 0;
    sVar2 = 0;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar9 < *(short *)(in_ECX + 0x3846)) goto LAB_005be435;
        uVar4 = 0xffffffff;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), cVar1 != '\x04' &&
               (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            if ((param_1[0x248] & 0x800U) == 0) {
              if ((int)sVar2 < (int)(short)param_1[0x249] - (int)(short)param_1[0x170]) {
                if (sVar9 < *(short *)(in_ECX + 0x3846)) {
                  local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
                }
                else {
                  local_8 = 0xffffffff;
                }
                sVar2 = (short)param_1[0x249] - (short)param_1[0x170];
              }
              if (sVar9 < *(short *)(in_ECX + 0x3846)) goto LAB_005be4df;
              uVar4 = 0xffffffff;
            }
            else if (sVar9 < *(short *)(in_ECX + 0x3846)) {
LAB_005be4df:
              uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
            }
            else {
              uVar4 = 0xffffffff;
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar9 = sVar9 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar9) {
            return local_8;
          }
LAB_005be435:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
        }
      } while( true );
    }
    break;
  case 4:
    sVar9 = 0;
    sVar2 = 0x7fff;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar9 < *(short *)(in_ECX + 0x3846)) goto LAB_005be546;
        uVar4 = 0xffffffff;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), cVar1 != '\x04' &&
               (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            if ((param_1[0x248] & 0x800U) == 0) {
              if ((int)(short)param_1[0x249] - (int)(short)param_1[0x170] < (int)sVar2) {
                if (sVar9 < *(short *)(in_ECX + 0x3846)) {
                  local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
                }
                else {
                  local_8 = 0xffffffff;
                }
                sVar2 = (short)param_1[0x249] - (short)param_1[0x170];
              }
              if (sVar9 < *(short *)(in_ECX + 0x3846)) goto LAB_005be5f0;
              uVar4 = 0xffffffff;
            }
            else if (sVar9 < *(short *)(in_ECX + 0x3846)) {
LAB_005be5f0:
              uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
            }
            else {
              uVar4 = 0xffffffff;
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar9 = sVar9 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar9) {
            return local_8;
          }
LAB_005be546:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
        }
      } while( true );
    }
    break;
  case 5:
    sVar2 = 0;
    bVar6 = 0;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar2 < *(short *)(in_ECX + 0x3846)) goto LAB_005be2ac;
        uVar4 = 0xffffffff;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), cVar1 != '\x04' &&
               (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            bVar5 = bVar6;
            if ((param_1[0x248] & 0x800U) == 0) {
              iVar8 = (**(code **)(*param_1 + 0x10))();
              if (*(char *)(iVar8 + 0x14) == '\x01') {
                bVar5 = *(byte *)((int)param_1 + 0x966);
                bVar10 = bVar5 == bVar6;
                if (bVar6 < bVar5) {
                  if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                    local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
                  }
                  else {
                    local_8 = 0xffffffff;
                  }
                  local_4 = CDerivedStats::GetClassMaskLevel(0xd1);
                }
                else {
                  bVar5 = bVar6;
                  if ((bVar10) && (iVar8 = CDerivedStats::GetClassMaskLevel(0xd1), local_4 < iVar8))
                  {
                    if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                      local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
                    }
                    else {
                      local_8 = 0xffffffff;
                    }
                    bVar5 = *(byte *)((int)param_1 + 0x966);
                    local_4 = iVar8;
                  }
                }
                goto LAB_005be3c1;
              }
              if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
              }
              else {
                uVar4 = 0xffffffff;
              }
            }
            else {
LAB_005be3c1:
              bVar6 = bVar5;
              if (sVar2 < *(short *)(in_ECX + 0x3846)) {
                uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
              }
              else {
                uVar4 = 0xffffffff;
              }
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar2 = sVar2 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar2) {
            return local_8;
          }
LAB_005be2ac:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar2 * 4);
        }
      } while( true );
    }
    break;
  case 6:
    sVar9 = 0;
    sVar2 = -0x14;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar9 < *(short *)(in_ECX + 0x3846)) goto LAB_005be65c;
        uVar4 = 0xffffffff;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), cVar1 != '\x04' &&
               (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            if (((param_1[0x248] & 0x800U) == 0) && (iVar8 = CGameSprite::GetAC(), sVar2 < iVar8)) {
              if (sVar9 < *(short *)(in_ECX + 0x3846)) {
                local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
              }
              else {
                local_8 = 0xffffffff;
              }
              sVar2 = CGameSprite::GetAC();
            }
            if (sVar9 < *(short *)(in_ECX + 0x3846)) {
              uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
            }
            else {
              uVar4 = 0xffffffff;
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar9 = sVar9 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar9) {
            return local_8;
          }
LAB_005be65c:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
        }
      } while( true );
    }
    break;
  case 7:
    sVar9 = 0;
    sVar2 = 0x14;
    if (0 < *(short *)(in_ECX + 0x3846)) {
      do {
        if (sVar9 < *(short *)(in_ECX + 0x3846)) goto LAB_005be753;
        uVar4 = 0xffffffff;
        while ((cVar1 = CGameObjectArray::GetShare(uVar4,0,&param_1,0xffffffff), cVar1 != '\x04' &&
               (cVar1 != '\x05'))) {
          if (cVar1 == '\0') {
            if (((param_1[0x248] & 0x800U) == 0) && (iVar8 = CGameSprite::GetAC(), iVar8 < sVar2)) {
              if (sVar9 < *(short *)(in_ECX + 0x3846)) {
                local_8 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
              }
              else {
                local_8 = 0xffffffff;
              }
              sVar2 = CGameSprite::GetAC();
            }
            if (sVar9 < *(short *)(in_ECX + 0x3846)) {
              uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
            }
            else {
              uVar4 = 0xffffffff;
            }
            CGameObjectArray::ReleaseShare(uVar4,0,0xffffffff);
          }
          sVar9 = sVar9 + 1;
          if (*(short *)(in_ECX + 0x3846) <= sVar9) {
            return local_8;
          }
LAB_005be753:
          uVar4 = *(undefined4 *)(in_ECX + 0x382e + sVar9 * 4);
        }
      } while( true );
    }
  }
  return 0xffffffff;
}
```

## Callers

- 0x40B880 CAIObjectType::Decode (call at 0x40C445)

## Callees

- 0x447950 CDerivedStats::GetClassMaskLevel
- 0x599A50 CGameObjectArray::GetShare
- 0x599E70 CGameObjectArray::ReleaseShare
- 0x723CC0 CGameSprite::GetAC

## Notes

The party member a Decode object picks: 1 the lowest level (a tie: the lower class mask level 0xD1), 2 the highest (a tie: the higher class mask level), 3 the most damaged (max HP - HP), 4 the least damaged, 5 the highest-level male (NOTE: its tie compares a stack slot it never sets, local_4 at 0x5BE35E; ours starts it at 0), 6 the worst AC, 7 the best AC. Dead members (derived STATE_DEAD) are skipped; a busy share is retried. INVALID_INDEX when none qualifies. Decode's case (CAIObjectType.cpp, ai-dispatch's file) still answers NOONE; wiring it is a request.
