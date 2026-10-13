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

| Site | What is stored |
|---|---|
| `src/CGameContainer.cpp:777` (CGameContainer::Marshal) | `pPoints` in packed `m_pickPointStart` |
| `src/CGameContainer.cpp:801` (CGameContainer::Marshal) | `pItems` in packed `m_startingItem` |
| `src/CGameDoor.cpp:659` (CGameDoor::Marshal) | `pPoints` in packed `m_closedSearchSquaresStart` |
| `src/CGameDoor.cpp:676` (CGameDoor::Marshal) | `pPoints` in packed `m_openSearchSquaresStart` |
| `src/CGameDoor.cpp:693` (CGameDoor::Marshal) | `pPoints` in packed `m_closedSelectionPointStart` |
| `src/CGameDoor.cpp:710` (CGameDoor::Marshal) | `pPoints` in packed `m_openSelectionPointStart` |
| `src/CGameTrigger.cpp` (CGameTrigger::Marshal, 0x4CFF70) | `pPoints` in packed `m_pickPointStart` |
| `src/CGameSprite.cpp:6427` (CGameSprite::Marshal, area creature) | `creatureData` in packed `m_creatureOffset` |
| `src/CGameSprite.cpp:6701` (CGameSprite::Marshal 0x70BEE0, party creature) | `pCreature` in `CSavedGamePartyCreature::m_creatureOffset` |

Consumers that resolve the handles with `iwd2r::UnmarshalPtr` (a handle the table does not know gives NULL, and the block stays zero):

| Site | What is read back |
|---|---|
| `src/CInfGame.cpp:1941` (CInfGame::Marshal) | party `m_creatureOffset` as `BYTE*` |
| `src/CInfGame.cpp:1980` (CInfGame::Marshal) | global creature `m_creatureOffset` as `BYTE*` |
| `src/CMessage.cpp:2151` (CBaldurMessage::DemandCharacterSlotReply) | party `m_creatureOffset` as `BYTE*` |
| `src/CGameArea.cpp:2219` (CGameArea::Marshal) | creature `m_creatureOffset` as `BYTE*` |
| `src/CGameArea.cpp:2261` (CGameArea::Marshal) | container `m_pickPointStart` |
| `src/CGameArea.cpp:2280`, `:2291`, `:2302`, `:2313` (CGameArea::Marshal) | the four door point starts |
| `src/CGameArea.cpp:2331` (CGameArea::Marshal) | trigger `m_pickPointStart` |
| `src/CGameArea.cpp:2401` (CGameArea::Marshal) | container `m_startingItem` |

The palette keys still need a 64-bit-safe hash.
