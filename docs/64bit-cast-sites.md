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
| `src/CGameSprite.cpp:6425` | `creatureData` in packed `m_creatureOffset` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameSprite.cpp:6509` | `pCreature` in `CSavedGamePartyCreature::m_creatureOffset` | Peer-request path: written here, read back as a pointer at `CMessage.cpp:2148` |
| `src/CMessage.cpp:2148` | `m_creatureOffset` read back as `BYTE*` | Peer-request path: dereferences the truncated pointer from `CGameSprite.cpp:6509` |
| `src/CInfGame.cpp:1934` | party `m_creatureOffset` read back as `BYTE*` (CInfGame::Marshal) | Consumer of `CGameSprite.cpp:6509`; resolves an `iwd2r::UnmarshalPtr` handle (compat/marshal_ptr.h); an unknown handle gives NULL and the bytes stay zero |
| `src/CInfGame.cpp:1973` | global creature `m_creatureOffset` read back as `BYTE*` (CInfGame::Marshal) | Consumer, as above |
| `src/CGameArea.cpp:2219` | creature `m_creatureOffset` read back as `BYTE*` (CGameArea::Marshal) | Consumer of `CGameSprite.cpp:6425`; `UnmarshalPtr` |
| `src/CGameArea.cpp:2261` | container `m_pickPointStart` read back as `CAreaPoint*` | Consumer of `CGameContainer.cpp:777`; `UnmarshalPtr` |
| `src/CGameArea.cpp:2280` | door `m_openSelectionPointStart` read back | Consumer of `CGameDoor.cpp:710`; `UnmarshalPtr` |
| `src/CGameArea.cpp:2291` | door `m_closedSelectionPointStart` read back | Consumer of `CGameDoor.cpp:693`; `UnmarshalPtr` |
| `src/CGameArea.cpp:2302` | door `m_openSearchSquaresStart` read back | Consumer of `CGameDoor.cpp:676`; `UnmarshalPtr` |
| `src/CGameArea.cpp:2313` | door `m_closedSearchSquaresStart` read back | Consumer of `CGameDoor.cpp:659`; `UnmarshalPtr` |
| `src/CGameArea.cpp:2331` | trigger `m_pickPointStart` read back | Consumer for CGameTrigger's marshal (0x4CFF70, not in the tree yet; dead until it lands); `UnmarshalPtr` |
| `src/CGameArea.cpp:2401` | container `m_startingItem` read back as `CCreatureFileItem*` | Consumer of `CGameContainer.cpp:801`; `UnmarshalPtr` |
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

`src/CGameSprite.cpp:6425` (`creatureData` in `m_creatureOffset`) stays in the table above until its owner converts it; the sprite-extras stream has, pending merge.

The palette keys still need a 64-bit-safe hash.
