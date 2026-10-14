#include "CGameSpawning.h"

#include "CAIGroup.h"
#include "CBaldurChitin.h"
#include "CCreatureFile.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CPathSearch.h"
#include "CUtil.h"

// 0x4C9780
CGameSpawning::CGameSpawning(CGameArea* pArea, CAreaFileRandomMonsterSpawningPoint* pSpawningObject)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSpawning.cpp
    // __LINE__: 108
    UTIL_ASSERT(pArea != NULL && pSpawningObject != NULL);

    m_objectType = TYPE_SPAWNING;

    memcpy(&m_spawningObject, pSpawningObject, sizeof(CAreaFileRandomMonsterSpawningPoint));

    CVariable variable;
    variable.SetName(m_spawningObject.m_scriptName);
    pArea->GetNamedCreatures()->AddKey(variable);

    if (m_spawningObject.m_probDay == 0) {
        m_spawningObject.m_probDay = 100;
    }

    if (m_spawningObject.m_probNight == 0) {
        m_spawningObject.m_probNight = 100;
    }

    field_136 = 0;

    // NOTE: This assignment is slightly incorrect. Original code refers to
    // other offsets (0x8A8168 and 0x8A8154 respectively). It can mean two
    // things:
    // - There was second set of terrain table defaults, which is exactly the
    // same as in `CGameObject`.
    // - Such split was a result of some compiler optimization.
    //
    // Since both sets have the same values, there is no harm to reuse what's
    // already present.
    memcpy(m_terrainTable, DEFAULT_TERRAIN_TABLE, 16);
    memcpy(m_visibleTerrainTable, DEFAULT_VISIBLE_TERRAIN_TABLE, 16);

    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE) == CGameObjectArray::SUCCESS) {
        AddToArea(pArea,
            CPoint(m_spawningObject.m_posX, m_spawningObject.m_posY),
            0,
            LIST_BACK);
    } else {
        // FIXME: Doesn't look cool.
        delete this;
    }
}

// 0x4C9990
CGameSpawning::~CGameSpawning()
{
}

// 0x766660
BOOLEAN CGameSpawning::CompressTime(DWORD deltaTime)
{
    return TRUE;
}

// 0x4CA3D0
void CGameSpawning::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSpawning.cpp
        // __LINE__: 352
        UTIL_ASSERT(FALSE);
    }

    delete this;
}

// 0x4C9CE0
void CGameSpawning::Spawn(const CPoint& ptFacingTowards, BYTE nTotalCharacterLevels)
{
    CCreatureFile creatureFile;
    DWORD nTotalPower = 0;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CVisibilityMap* pVisibility = &(m_pArea->m_visibility);
    CPoint* pOffsets = NULL;

    if (m_spawningObject.m_activated == 0) {
        return;
    }

    if (field_136 && (m_spawningObject.m_wFlags & 0x1) != 0) {
        return;
    }

    // NOTE: Inlined.
    BYTE nHour = pGame->m_worldTime.GetCurrentHour();
    if (((m_spawningObject.m_timeOfDayVisible >> nHour) & 0x1) == 0) {
        return;
    }

    if (pVisibility->IsTileVisible(pVisibility->PointToTile(m_pos))) {
        return;
    }

    if (nTotalCharacterLevels == 0) {
        for (SHORT nPortrait = 0; nPortrait < pGame->m_nCharacters; nPortrait++) {
            LONG iSprite = pGame->GetCharacterId(nPortrait);

            CGameSprite* pSprite;
            BYTE rc = pGame->GetObjectArray()->GetShare(iSprite,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                // NOTE: Summed into the BYTE (six characters of level 30
                // stay below 256).
                nTotalCharacterLevels += pSprite->m_derivedStats.GetLevel();

                pGame->GetObjectArray()->ReleaseShare(iSprite,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }

    CPoint ptGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);

    INT nIndex;
    if (m_spawningObject.m_randomCreatureNum != 0) {
        nIndex = rand() % m_spawningObject.m_randomCreatureNum;
    } else {
        nIndex = 0;
    }

    SHORT nCount = 0;
    if (m_spawningObject.m_maxTypeNum != 0) {
        do {
            BYTE* pResRef = m_spawningObject.m_randomCreature[nIndex];

            // NOTE: Uninline.
            creatureFile.SetResRef(CResRef(pResRef), TRUE, TRUE);

            // NOTE: The original evaluates the size first.
            DWORD nSize = creatureFile.GetDataSize();
            BYTE* pData = creatureFile.GetData();

            CGameSprite* pSprite = new CGameSprite(pData,
                nSize,
                0,
                1,
                m_spawningObject.m_lifeSpan * CTimerWorld::TIMESCALE_MSEC_PER_SEC + pGame->m_worldTime.m_gameTime,
                m_spawningObject.m_huntingRange,
                m_spawningObject.m_followRange,
                0x7FFFFFFF,
                CPoint(m_pos.x, m_pos.y),
                0);

            // NOTE: As in `CSpawn::ExecuteSpawn`, the original sets the
            // resref before the NULL check below.
            pSprite->SetResRef(CResRef(creatureFile.GetResRef().GetResRefStr()));

            creatureFile.ReleaseData();

            if (pSprite == NULL) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSpawning.cpp
                // __LINE__: 280
                UTIL_ASSERT(FALSE);
            } else {
                // NOTE: The original builds a CString from the RESREF with
                // `CString::operator=(LPCSTR)`, reading on into the next entry
                // when this one has 8 characters; the resref keeps 8. Ours
                // takes the 8 bytes (bounded), the same visible result.
                pSprite->SetResRef(CResRef(reinterpret_cast<const char*>(pResRef)));

                nTotalPower += pSprite->m_derivedStats.m_nXP;

                BOOL bPlace = TRUE;
                if (nCount != 0 || nIndex != 0) {
                    if (nTotalPower > m_spawningObject.m_difficulty * static_cast<DWORD>(nTotalCharacterLevels) / 100) {
                        // NOTE: A sprite whose Delete fails is leaked.
                        if (pGame->GetObjectArray()->Delete(pSprite->GetId(), CGameObjectArray::THREAD_ASYNCH, NULL, INFINITE) == CGameObjectArray::SUCCESS) {
                            delete pSprite;
                        }

                        if (nCount != 0) {
                            break;
                        }

                        // NOTE: The rejected creature's power stays in
                        // the total.
                        nCount = -1;
                        nIndex = 0;
                        bPlace = FALSE;
                    }
                }

                if (bPlace) {
                    if (nCount == 0) {
                        SHORT nDirection = pSprite->GetDirection(ptFacingTowards);
                        pOffsets = pGame->m_group.GetOffsets(static_cast<SHORT>(rand() % 9 + 1),
                            static_cast<SHORT>(nDirection * 360 / 16 % 360),
                            TRUE);
                    }

                    // NOTE: The original indexes the offsets with
                    // `nCount % FORMATION_MAX_SIZE` but GetOffsets gave it
                    // max(members, 6) of them, so from the seventh creature
                    // a party of six or fewer reads past the heap block.
                    // Bounded: an index past the array reads (0, 0).
                    INT nMembers = pGame->m_group.m_memberList.GetCount();
                    INT nOffsets = nMembers < 7 ? 6 : static_cast<BYTE>(nMembers);
                    INT nOffset = nCount % CAIGroup::FORMATION_MAX_SIZE;
                    CPoint ptOffset(0, 0);
                    if (nOffset < nOffsets) {
                        ptOffset = pOffsets[nOffset];
                    }

                    CPoint ptStart(ptOffset.x / CAIGroup::OFFSET_MULTIPLIER + ptGrid.x,
                        ptOffset.y / CAIGroup::OFFSET_MULTIPLIER + ptGrid.y);

                    CPoint ptSquare = m_pArea->m_search.GetNearestOpenSquare(ptStart,
                        m_terrainTable,
                        pSprite->m_animation.GetPersonalSpace(),
                        -1);

                    CPoint ptSpawn(ptSquare.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                        ptSquare.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);

                    if (pVisibility->IsTileVisible(pVisibility->PointToTile(ptSpawn))) {
                        if (pGame->GetObjectArray()->Delete(pSprite->GetId(), CGameObjectArray::THREAD_ASYNCH, NULL, INFINITE) == CGameObjectArray::SUCCESS
                            && pSprite != NULL) {
                            delete pSprite;
                        }

                        nCount--;
                        break;
                    }

                    pSprite->AddToArea(m_pArea, ptSpawn, 0, pSprite->m_animation.GetListType());
                    pSprite->SetFacing(pSprite->GetDirection(ptFacingTowards));

                    if (nTotalPower == m_spawningObject.m_difficulty * static_cast<DWORD>(nTotalCharacterLevels) / 100) {
                        break;
                    }
                }
            }

            nCount++;
        } while (nCount < m_spawningObject.m_maxTypeNum);

        if (pOffsets != NULL) {
            // NOTE: The original frees it with scalar operator delete; ours
            // matches GetOffsets' `new[]`.
            delete[] pOffsets;
        }

        if (nCount < 0) {
            return;
        }
    }

    field_136 = 1;
}

// 0x4CA430
void CGameSpawning::Marshal(CAreaFileRandomMonsterSpawningPoint** pSpawningObject)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSpawning.cpp
    // __LINE__: 379
    UTIL_ASSERT(pSpawningObject != NULL);

    *pSpawningObject = new CAreaFileRandomMonsterSpawningPoint;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameSpawning.cpp
    // __LINE__: 382
    UTIL_ASSERT(*pSpawningObject != NULL);

    memcpy(*pSpawningObject, &m_spawningObject, sizeof(CAreaFileRandomMonsterSpawningPoint));
}
