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
| `src/CGameContainer.cpp:777` | `pPoints` in packed `m_pickPointStart` | Packed marshalling field; live crash once project 3 writes the area-save consumer |
| `src/CGameContainer.cpp:801` | `pItems` in packed `m_startingItem` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameDoor.cpp:659` | `pPoints` in packed `m_closedSearchSquaresStart` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameDoor.cpp:676` | `pPoints` in packed `m_openSearchSquaresStart` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameDoor.cpp:693` | `pPoints` in packed `m_closedSelectionPointStart` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameDoor.cpp:710` | `pPoints` in packed `m_openSelectionPointStart` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameSprite.cpp:6425` | `creatureData` in packed `m_creatureOffset` | Packed marshalling field; live crash once the area-save consumer exists |
| `src/CGameSprite.cpp:6509` | `pCreature` in `CSavedGamePartyCreature::m_creatureOffset` | Peer-request path: written here, read back as a pointer at `CMessage.cpp:2148` |
| `src/CMessage.cpp:2148` | `m_creatureOffset` read back as `BYTE*` | Peer-request path: dereferences the truncated pointer from `CGameSprite.cpp:6509` |
| `src/CVidImage.cpp:79` | `pImage` added to `nAUCounter` as the pulse phase | Animation phase seed: the add wraps in 32 bits as the original's does, but on 64-bit only the low 32 bits count and the signed `%` can go negative when bit 31 is set |

Project 3 should replace the packed-field pointers with offsets or a side table before writing area saves, and give the palette keys a 64-bit-safe hash.
