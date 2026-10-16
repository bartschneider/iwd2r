#include "CGameChunk.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CPathSearch.h"
#include "CUtil.h"

// 0x70F4F0
CGameChunk::CGameChunk(USHORT animationID, BYTE* colorRangeValues, const CString& sSound, CGameArea* pArea, const CPoint& pos, const CPoint& velocity, LONG posZ, LONG nPosZDelta, BYTE nLife, BYTE nFade)
{
    // NOTE: Uninline.
    m_animation.SetAnimationType(animationID, colorRangeValues, 0);

    m_sound.SetResRef(CResRef(sSound), TRUE, TRUE);
    m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));

    m_bMoving = TRUE;
    m_velocity.x = velocity.x;
    m_velocity.y = velocity.y;
    m_posZDelta = nPosZDelta;
    m_nLife = nLife;
    m_nFade = nFade;

    // NOTE: The 16 bytes at 0x8A8154 are the visible terrain table's values.
    memcpy(m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(m_terrainTable));

    m_nBounces = 3;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CGameObject::AddToArea(pArea, pos, posZ, LIST_FRONT);
    } else {
        // NOTE: The original deletes the chunk in its own constructor; the
        // caller drops the pointer.
        delete this;
    }
}

// 0x70F770
CGameChunk::~CGameChunk()
{
}

// 0x70F740
BOOLEAN CGameChunk::CanSaveGame(STRREF& strError)
{
    strError = 16502; // "You cannot save at this time."
    return TRUE;
}

// 0x70F800
void CGameChunk::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    BOOL bPlaySound = FALSE;
    SHORT nTableIndex;

    if (!m_bMoving) {
        m_nLife--;
        if (m_nLife == 0) {
            RemoveFromArea();
        }
        return;
    }

    CPoint ptOldGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);

    m_posZ -= m_posZDelta--;
    if (m_posZ >= 0) {
        // On the ground: gone in deep water, else bounce while bounces are
        // left.
        m_posZ = 0;
        m_pArea->m_search.GetLOSCost(ptOldGrid, m_terrainTable, nTableIndex, FALSE);
        if (nTableIndex == 12) {
            RemoveFromArea();
            return;
        }

        if (m_nBounces > 0) {
            bPlaySound = TRUE;
            m_posZDelta = (rand() % 3 + 1) * m_nBounces;
            m_nBounces--;
        } else {
            m_bMoving = FALSE;
        }
    }

    SHORT nOldHeight = static_cast<SHORT>(m_pArea->GetHeightOffset(m_pos, LIST_BACK));
    m_pos.x += m_velocity.x;
    m_pos.y += m_velocity.y;
    SHORT nNewHeight = static_cast<SHORT>(m_pArea->GetHeightOffset(m_pos, LIST_BACK));

    CPoint ptNewGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
    if (ptOldGrid.x != ptNewGrid.x || ptOldGrid.y != ptNewGrid.y) {
        // NOTE: The original checks only the upper bounds, so a chunk
        // crossing the area's left or top edge reads the search map before
        // its start. We bounce it off the edge instead (P4-10).
        if (ptNewGrid.x >= 0
            && ptNewGrid.y >= 0
            && ptNewGrid.x < m_pArea->m_search.m_GridSquareDimensions.cx
            && ptNewGrid.y < m_pArea->m_search.m_GridSquareDimensions.cy
            && m_pArea->m_search.GetLOSCost(ptNewGrid, m_terrainTable, nTableIndex, FALSE) != CPathSearch::COST_IMPASSABLE) {
            m_posZ += nNewHeight - nOldHeight;
        } else {
            // Bounce off the wall.
            if (ptOldGrid.x != ptNewGrid.x) {
                m_pos.x -= 2 * m_velocity.x;
                m_velocity.x = -m_velocity.x;
            }

            if (ptOldGrid.y != ptNewGrid.y) {
                m_pos.y -= 2 * m_velocity.y;
                m_velocity.y = -m_velocity.y;
            }

            bPlaySound = TRUE;
        }
    }

    // NOTE: Uninline.
    m_animation.IncrementFrame();

    if (bPlaySound && !m_sound.IsSoundPlaying()) {
        m_sound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }
}

// 0x70FA90
void CGameChunk::Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 13882
    UTIL_ASSERT(pVidMode != NULL);

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);
    BOOL bDithered = FALSE;
    BOOL bFadeOut = m_nLife < m_nFade;

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    CRect rFx;
    CPoint ptReference;

    // NOTE: Uninline.
    m_animation.CalculateFxRect(rFx, ptReference, m_posZ);

    CPoint ptPos(m_pos.x, m_pos.y);
    ptPos.y += m_pArea->GetHeightOffset(m_pos, LIST_BACK);

    CRect rGCBounds;
    m_animation.CalculateGCBoundsRect(rGCBounds,
        ptPos,
        ptReference,
        m_posZ,
        rFx.right - rFx.left,
        rFx.bottom - rFx.top);

    if (!rView.IntersectRect(rGCBounds, rView)) {
        return;
    }

    COLORREF rgbTint = m_pArea->GetTintColor(CPoint(ptPos.x, ptPos.y + m_posZ), m_listType);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (m_id == m_pArea->m_iPicked
        || pGame->GetOptions()->m_bAlwaysDither
        || pGame->m_bForceDither) {
        bDithered = TRUE;
    }

    // NOTE: Uninline.
    m_animation.Render(pInfinity,
        pVidMode,
        nSurface,
        rFx,
        ptPos,
        ptReference,
        0x30000,
        rgbTint,
        rGCBounds,
        bDithered,
        bFadeOut,
        m_posZ,
        0);
}

// 0x70FCF0
void CGameChunk::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 13954
        UTIL_ASSERT(FALSE);
        return;
    }

    delete this;
}
