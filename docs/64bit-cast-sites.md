# 64-bit pointer-truncation sites

Engine sites where a pointer is stored in, or read back from, a 32-bit integer and fits neither R19 idiom (an id in a `CPtrList` slot, or an area pointer used as a `DWORD_PTR` sound key). Project 1 keeps the low 32 bits so the engine compiles on 64-bit targets, and records the sites here for project 3. Line numbers are as of the end of project 2 (branch `first`). The list comes from commit `f0f562a` ("fits neither idiom") plus the sites the final reviews added. Classifications are the final reviewer's.

| Site | What is stored | Classification |
|---|---|---|
| `src/CGameAnimationTypeMonsterMulti.cpp:219` | `m_g2VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterMulti.cpp:231` | `m_g1VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterMulti.cpp:232` | `m_g2VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterMulti.cpp:233` | `m_g3VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterMulti.cpp:234` | `m_g4VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterMulti.cpp:235` | `m_g5VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterQuadrant.cpp:107` | `m_g1VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterQuadrant.cpp:108` | `m_g2VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterQuadrant.cpp:109` | `m_g3VidCellBase` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterQuadrant.cpp:132` | `m_g1VidCellExtend` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterQuadrant.cpp:133` | `m_g2VidCellExtend` as `m_nAUCounterBase` | Palette-cache key |
| `src/CGameAnimationTypeMonsterQuadrant.cpp:134` | `m_g3VidCellExtend` as `m_nAUCounterBase` | Palette-cache key |
| `src/CVidPalette.cpp:565` | `m_pPalette` as the palette base `nBase` | Palette-cache key |
| `src/CVidMode.cpp:1087` | `lpSurface` low bits in a `UTIL_ASSERT` | Parity assert; only bit 0 is tested, so truncation is exact |
| `src/CResFile.cpp:113` | `field_20 + 2` returned as a `DWORD` | Unreachable |
| `src/CVidImage.cpp:79` | `pImage` added to `nAUCounter` as the pulse phase | Animation phase seed: the add wraps in 32 bits as the original's does, but on 64-bit only the low 32 bits count and the signed `%` can go negative when bit 31 is set |

## Resolved by compat/marshal_ptr.h

Project 3 chose a side table for the packed marshalling fields: the producers store a 32-bit handle from `iwd2r::MarshalPtr`, and the consumers that read the fields back as pointers before the area file is written (`CGameArea::Marshal`, `CGameArea::Unmarshal`) resolve it with `iwd2r::UnmarshalPtr`. `ResetMarshalPtrs` empties the table at the start of each save (`CInfGame::SaveGame`).

Sites are named by file and function, with the statement as the anchor to search for; line numbers drifted with every merge.

| Site | Anchor | What is stored |
|---|---|---|
| `src/CGameContainer.cpp`, CGameContainer::Marshal | `m_pickPointStart = iwd2r::MarshalPtr(pPoints)` | `pPoints` in packed `m_pickPointStart` |
| `src/CGameContainer.cpp`, CGameContainer::Marshal | `m_startingItem = iwd2r::MarshalPtr(pItems)` | `pItems` in packed `m_startingItem` |
| `src/CGameDoor.cpp`, CGameDoor::Marshal | `m_closedSearchSquaresStart = iwd2r::MarshalPtr(pPoints)` | `pPoints` in packed `m_closedSearchSquaresStart` |
| `src/CGameDoor.cpp`, CGameDoor::Marshal | `m_openSearchSquaresStart = iwd2r::MarshalPtr(pPoints)` | `pPoints` in packed `m_openSearchSquaresStart` |
| `src/CGameDoor.cpp`, CGameDoor::Marshal | `m_closedSelectionPointStart = iwd2r::MarshalPtr(pPoints)` | `pPoints` in packed `m_closedSelectionPointStart` |
| `src/CGameDoor.cpp`, CGameDoor::Marshal | `m_openSelectionPointStart = iwd2r::MarshalPtr(pPoints)` | `pPoints` in packed `m_openSelectionPointStart` |
| `src/CGameTrigger.cpp`, CGameTrigger::Marshal (0x4CFF70) | `m_pickPointStart = iwd2r::MarshalPtr(pPoints)` | `pPoints` in packed `m_pickPointStart` |
| `src/CGameSprite.cpp`, CGameSprite::Marshal (area creature) | `m_creatureOffset = iwd2r::MarshalPtr(creatureData)` | `creatureData` in packed `m_creatureOffset` |
| `src/CGameSprite.cpp`, CGameSprite::Marshal (0x70BEE0, party creature) | `m_creatureOffset = iwd2r::MarshalPtr(pCreature)` | `pCreature` in `CSavedGamePartyCreature::m_creatureOffset` |

Consumers that resolve the handles with `iwd2r::UnmarshalPtr`. Each checks for NULL (a handle the table does not know), and the block then stays zero:

| Site | Anchor | What is read back |
|---|---|---|
| `src/CInfGame.cpp`, CInfGame::Marshal | `UnmarshalPtr(partyCreatures[cnt3].m_creatureOffset)` | party `m_creatureOffset` as `BYTE*` |
| `src/CInfGame.cpp`, CInfGame::Marshal | `UnmarshalPtr(pGlobalCreature[nGlobal].m_creatureOffset)` | global creature `m_creatureOffset` as `BYTE*` |
| `src/CMessage.cpp`, CBaldurMessage::DemandCharacterSlotReply | `UnmarshalPtr(partyCreature.m_creatureOffset)` | party `m_creatureOffset` as `BYTE*` |
| `src/CGameArea.cpp`, CGameArea::Marshal | `UnmarshalPtr(pCreature->m_creatureOffset)` | creature `m_creatureOffset` as `BYTE*` |
| `src/CGameArea.cpp`, CGameArea::Marshal | `UnmarshalPtr(pContainerObject->m_pickPointStart)` | container `m_pickPointStart` |
| `src/CGameArea.cpp`, CGameArea::Marshal | `UnmarshalPtr(pDoorObject->m_openSelectionPointStart)` and the other three door starts | the four door point starts |
| `src/CGameArea.cpp`, CGameArea::Marshal | `UnmarshalPtr(pTriggerObject->m_pickPointStart)` | trigger `m_pickPointStart` |
| `src/CGameArea.cpp`, CGameArea::Marshal | `UnmarshalPtr(pContainerObject->m_startingItem)` | container `m_startingItem` |

The palette keys still need a 64-bit-safe hash.
