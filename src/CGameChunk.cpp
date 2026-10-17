#include "CGameChunk.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CPathSearch.h"
#include "CUtil.h"
#include "CVidMode.h"

#include <math.h>

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
        // NOTE: The original checks only the upper bounds, and
        // CSearchBitmap::GetLOSCost reads the map at x + width * y with no
        // lower bound, so it reads before the map for a square whose index
        // is negative (left of the area's first row, or above the area).
        // Ours bounces the chunk off such a square (P4-10); a square left of
        // the area on a later row reads the row above's last squares, as
        // the original does.
        if (ptNewGrid.x + m_pArea->m_search.m_GridSquareDimensions.cx * ptNewGrid.y >= 0
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

// -----------------------------------------------------------------------------

// 0x70FD50
CGameObject85BF7C::CGameObject85BF7C(USHORT animationID, BYTE* colorRangeValues, const CString& sSound, CGameArea* pArea, const CPoint& ptExact, LONG posZ, const CPoint& velocity, WORD nLife, BYTE nFade, BYTE nWallMode)
{
    // NOTE: Uninline. The animation faces the way the piece flies.
    m_animation.SetAnimationType(animationID,
        colorRangeValues,
        CGameSprite::GetDirection(CPoint(0, 0), velocity));

    m_sound.SetResRef(CResRef(sSound), TRUE, TRUE);
    m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));

    m_bVisible = TRUE;
    m_ptExact.x = ptExact.x;
    m_ptExact.y = ptExact.y;
    m_velocity.x = velocity.x;
    m_velocity.y = velocity.y;
    m_nLife = nLife;
    m_nFade = nFade;
    m_nWallMode = nWallMode;

    // NOTE: The 16 bytes at 0x8A8154 are the visible terrain table's values.
    memcpy(m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(m_terrainTable));

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc == CGameObjectArray::SUCCESS) {
        CGameObject::AddToArea(pArea, CPoint(ptExact.x >> 10, ptExact.y >> 10), posZ, LIST_FRONT);
    } else {
        // NOTE: The original deletes the piece in its own constructor; the
        // caller drops the pointer.
        delete this;
    }
}

// 0x710000
CGameObject85BF7C::~CGameObject85BF7C()
{
}

// 0x710090
void CGameObject85BF7C::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    // A piece without a life lasts to the end of its sequence.
    if (m_nLife == 0) {
        // NOTE: Uninline.
        if (m_animation.IsEndOfSequence()) {
            RemoveFromArea();
            return;
        }
    } else {
        if (--m_nLife == 0) {
            RemoveFromArea();
            return;
        }
    }

    CPoint ptOldGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);

    m_ptExact.y += m_velocity.y;
    m_ptExact.x += m_velocity.x;
    m_pos.y = m_ptExact.y >> 10;
    m_pos.x = m_ptExact.x >> 10;

    CPoint ptNewGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
    if (ptOldGrid.x != ptNewGrid.x || ptOldGrid.y != ptNewGrid.y) {
        // NOTE: CSearchBitmap::GetLOSCost reads the map at x + width * y with
        // no lower bound, so the original reads before the map for a square
        // whose index is negative (left of the area's first row, or above
        // the area). Ours takes such a square for a wall (P4-10); a square
        // left of the area on a later row reads the row above's last
        // squares, as the original does.
        SHORT nTableIndex;
        BYTE nCost = CPathSearch::COST_IMPASSABLE;
        if (ptNewGrid.x + m_pArea->m_search.m_GridSquareDimensions.cx * ptNewGrid.y >= 0) {
            nCost = m_pArea->m_search.GetLOSCost(ptNewGrid, m_terrainTable, nTableIndex, FALSE);
        }

        if (nCost == CPathSearch::COST_IMPASSABLE) {
            if (m_nWallMode == 1) {
                RemoveFromArea();
                return;
            }

            if (m_nWallMode == 0) {
                // Bounce off the wall and face the new way.
                CPoint ptOld = m_ptExact;

                if (ptOldGrid.x != ptNewGrid.x) {
                    m_ptExact.x -= 2 * m_velocity.x;
                    m_pos.x = m_ptExact.x >> 10;
                    m_velocity.x = -m_velocity.x;
                }

                if (ptOldGrid.y != ptNewGrid.y) {
                    m_ptExact.y -= 2 * m_velocity.y;
                    m_pos.y = m_ptExact.y >> 10;
                    m_velocity.y = -m_velocity.y;
                }

                // NOTE: Uninline.
                m_animation.ChangeDirection(CGameSprite::GetDirection(ptOld, m_ptExact));
            } else {
                m_bVisible = FALSE;
            }
        } else {
            m_bVisible = TRUE;
        }
    }

    // NOTE: Uninline.
    m_animation.IncrementFrame();

    if (rand() % 200 == 0 && !m_sound.IsSoundPlaying()) {
        m_sound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }
}

// 0x710330
void CGameObject85BF7C::Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 14202
    UTIL_ASSERT(pVidMode != NULL);

    BOOL bFadeOut = FALSE;
    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    if (!m_bVisible) {
        return;
    }

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    if (m_nLife < static_cast<SHORT>(m_nFade)) {
        bFadeOut = TRUE;
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
        FALSE,
        bFadeOut,
        m_posZ,
        0);
}

// 0x710550
void CGameObject85BF7C::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 14270
        UTIL_ASSERT(FALSE);
        return;
    }

    delete this;
}

// -----------------------------------------------------------------------------

// 0x7105B0
CGameObject85BFE8::CGameObject85BFE8(BYTE nType, BYTE* colorRangeValues, CGameArea* pArea, const CPoint& pos, SHORT nRadius, BYTE nSpeed, BYTE nWallMode, WORD nDuration)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    CVidBitmap* pMasterBitmap = g_pBaldurChitin->GetObjectGame()->GetMasterBitmap();

    memcpy(m_colorRangeValues, colorRangeValues, sizeof(m_colorRangeValues));
    m_nWallMode = nWallMode;
    m_nExpandTicks = static_cast<BYTE>((nRadius - 1) / nSpeed + 1);
    m_nDuration = nDuration;

    // The sound plays at once; the flying and lingering pieces' effect
    // animations (the WORDs at 0x85AA18 and 0x85AA2E..0x85AA3C).
    switch (nType) {
    case 0:
        // A fire pillar, without pieces.
        m_sound.SetResRef(CResRef("EFF_M21"), TRUE, TRUE);
        m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));
        m_sound.Play(pos.x, pos.y, 0, FALSE);
        m_sPieceSound = "";
        m_nFlyingAnimation = 0;
        m_nLingeringAnimation = 0;
        m_animation.SetResRef(CResRef("SPFIREPI"), FALSE, TRUE, TRUE);
        m_animation.SequenceSet(0);
        m_animation.FrameSet(0);
        break;
    case 1:
    case 2:
        m_sound.SetResRef(CResRef("EFF_M18a"), TRUE, TRUE);
        m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));
        m_sound.Play(pos.x, pos.y, 0, FALSE);
        m_sPieceSound = "EFF_M18c";
        m_nFlyingAnimation = 0x500;
        m_nLingeringAnimation = 0x510;
        m_animation.SetResRef(CResRef("SPBOOM"), FALSE, TRUE, TRUE);
        m_animation.SetPalette(m_palette);
        m_palette.SetRange(0, colorRangeValues[0], *pMasterBitmap);
        m_palette.SetRange(1, colorRangeValues[1], *pMasterBitmap);
        m_animation.SequenceSet(0);
        m_animation.FrameSet(0);
        break;
    case 3:
        m_sound.SetResRef(CResRef("EFF_M34"), TRUE, TRUE);
        m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));
        m_sound.Play(pos.x, pos.y, 0, FALSE);
        m_sPieceSound = "";
        m_nFlyingAnimation = 0x600;
        m_nLingeringAnimation = 0x610;
        m_animation.SetResRef(CResRef("SPBOOM"), FALSE, TRUE, TRUE);
        m_animation.SetPalette(m_palette);
        m_palette.SetRange(0, colorRangeValues[0], *pMasterBitmap);
        m_palette.SetRange(1, colorRangeValues[1], *pMasterBitmap);
        m_animation.SequenceSet(0);
        m_animation.FrameSet(0);
        break;
    case 4:
        // Played backwards from the last frame.
        m_sound.SetResRef(CResRef("EFF_M31b"), TRUE, TRUE);
        m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));
        m_sound.Play(pos.x, pos.y, 0, FALSE);
        m_sPieceSound = "";
        m_nFlyingAnimation = 0x700;
        m_nLingeringAnimation = 0x710;
        m_animation.SetResRef(CResRef("SPBOOM"), FALSE, TRUE, TRUE);
        m_animation.SetPalette(m_palette);
        m_palette.SetRange(0, colorRangeValues[0], *pMasterBitmap);
        m_animation.SequenceSet(0);
        m_animation.FrameSet(static_cast<SHORT>(static_cast<BYTE>(m_animation.GetSequenceLength(m_animation.m_nCurrentSequence, FALSE)) - 1));
        break;
    case 5:
        m_sound.SetResRef(CResRef("EFF_M19"), TRUE, TRUE);
        m_sound.SetChannel(14, reinterpret_cast<DWORD_PTR>(pArea));
        m_sound.Play(pos.x, pos.y, 0, FALSE);
        m_sPieceSound = "";
        m_nFlyingAnimation = 0x800;
        m_nLingeringAnimation = 0x810;
        m_animation.SetResRef(CResRef("SPBOOM"), FALSE, TRUE, TRUE);
        m_animation.SetPalette(m_palette);
        m_palette.SetRange(0, colorRangeValues[0], *pMasterBitmap);
        m_animation.SequenceSet(0);
        m_animation.FrameSet(static_cast<SHORT>(static_cast<BYTE>(m_animation.GetSequenceLength(m_animation.m_nCurrentSequence, FALSE)) - 1));
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 14401
        UTIL_ASSERT(FALSE);
        break;
    }

    // NOTE: CGameObject84F0DC's ring (0x56C620..0x56CE52), instruction for
    // instruction.
    m_pParticles = NULL;
    m_pStates = NULL;

    LONG nRadiusSquares = (nRadius - 1) / CPathSearch::GRID_SQUARE_SIZEX;
    m_nGridX = nRadiusSquares + 1;
    m_nGridY = nRadiusSquares + 1;
    m_pGrid = new BYTE[(2 * m_nGridX + 1) * (2 * m_nGridY + 1)];
    if (m_pGrid == NULL) {
        delete this;
        return;
    }

    LONG nAxis = (nRadius - 1) / (2 * CPathSearch::GRID_SQUARE_SIZEX) + 1;
    m_nGridX = nAxis;
    m_nGridY = nAxis;

    CVidMode* pVidMode = g_pBaldurChitin->GetCurrentVideoMode();
    m_nArc1 = pVidMode->GetEllipseArcPixelList(m_nGridX, nAxis, m_pGrid) + 1;
    m_pGrid[m_nArc1 - 1] = 1;

    pVidMode = g_pBaldurChitin->GetCurrentVideoMode();
    m_nArc2 = pVidMode->GetEllipseArcPixelList(m_nGridY, m_nGridX, m_pGrid + m_nArc1) + 1;
    m_pGrid[m_nArc1 + m_nArc2 - 1] = 1;

    m_nGridX = nRadiusSquares + 1;
    m_nGridY = nRadiusSquares + 1;

    LONG nParticles = m_nArc1 + m_nArc2;
    m_pParticles = new Particle[4 * nParticles];
    m_pStates = new BYTE[4 * nParticles];
    memset(m_pStates, 0, 4 * nParticles);

    LONG nCentreX = pos.x;
    LONG nCentreY = (pos.y << 2) / 3;
    LONG x = nCentreX;
    LONG y = nRadius + nCentreY;
    for (SHORT nPoint = 0; nPoint < m_nArc1; nPoint++) {
        LONG dy = y - nCentreY;
        LONG dx = x - nCentreX;
        LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

        Particle& particle = m_pParticles[nPoint];
        Particle& particle1 = m_pParticles[nParticles + nPoint];
        Particle& particle2 = m_pParticles[2 * nParticles + nPoint];
        Particle& particle3 = m_pParticles[3 * nParticles + nPoint];

        particle.x = 0;
        particle1.x = 0;
        particle2.x = 0;
        particle3.x = 0;
        particle.y = 0;
        particle1.y = 0;
        particle2.y = 0;
        particle3.y = 0;

        particle.vx = (dx << 10) * nSpeed / nDistance;
        particle1.vx = particle.vx;
        if (particle.vx == 0) {
            m_pStates[2 * nParticles + nPoint] = 2;
            m_pStates[3 * nParticles + nPoint] = 2;
        }
        particle2.vx = -particle.vx;
        particle3.vx = -particle.vx;

        particle.vy = (dy << 10) * nSpeed / nDistance;
        particle1.vy = -particle.vy;
        particle2.vy = particle.vy;
        particle3.vy = -particle.vy;

        x += 2 * CPathSearch::GRID_SQUARE_SIZEX;
        y -= 2 * CPathSearch::GRID_SQUARE_SIZEY * m_pGrid[nPoint];
    }

    x = nRadius + nCentreX;
    y = nCentreY;
    for (SHORT nPoint = 0; nPoint < m_nArc2; nPoint++) {
        LONG dx = x - nCentreX;
        LONG dy = y - nCentreY;
        LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

        LONG nIndex = m_nArc1 + nPoint;
        Particle& particle = m_pParticles[nIndex];
        Particle& particle1 = m_pParticles[nParticles + nIndex];
        Particle& particle2 = m_pParticles[2 * nParticles + nIndex];
        Particle& particle3 = m_pParticles[3 * nParticles + nIndex];

        particle.x = 0;
        particle1.x = 0;
        particle2.x = 0;
        particle3.x = 0;
        particle.y = 0;
        particle1.y = 0;
        particle2.y = 0;
        particle3.y = 0;

        particle.vx = (dx << 10) * nSpeed / nDistance;
        particle1.vx = particle.vx;
        particle2.vx = -particle.vx;
        particle3.vx = -particle.vx;

        particle.vy = (dy << 10) * nSpeed / nDistance;
        particle1.vy = -particle.vy;
        particle2.vy = particle.vy;
        particle3.vy = -particle.vy;

        // NOTE: As in CGameObject84F0DC, the vy of particle nPoint of the
        // first arc is tested.
        if (m_pParticles[nPoint].vy == 0) {
            m_pStates[nParticles + nIndex] = 2;
            m_pStates[3 * nParticles + nIndex] = 2;
        }

        x -= 2 * CPathSearch::GRID_SQUARE_SIZEX * m_pGrid[nIndex];
        y += 2 * CPathSearch::GRID_SQUARE_SIZEY;
    }

    // NOTE: The 16 bytes at 0x8A8154 are the visible terrain table's values.
    memcpy(m_terrainTable, CGameObject::DEFAULT_VISIBLE_TERRAIN_TABLE, sizeof(m_terrainTable));

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // NOTE: The original deletes the explosion in its own constructor;
        // the caller drops the pointer.
        delete this;
        return;
    }

    CGameObject::AddToArea(pArea, pos, 0, LIST_FRONT);

    // A lingering piece at the centre.
    if (m_nFlyingAnimation != 0) {
        new CGameObject85BF7C(m_nLingeringAnimation,
            m_colorRangeValues,
            m_sPieceSound,
            m_pArea,
            CPoint(m_pos.x << 10, m_pos.y << 10),
            0,
            CPoint(0, 0),
            static_cast<WORD>((m_nExpandTicks << 1) + m_nDuration),
            0,
            m_nWallMode);
    }
}

// 0x711CB0
CGameObject85BFE8::~CGameObject85BFE8()
{
    if (m_pGrid != NULL) {
        delete[] m_pGrid;
    }

    if (m_pParticles != NULL) {
        delete[] m_pParticles;
    }

    if (m_pStates != NULL) {
        delete[] m_pStates;
    }
}

// 0x711D90
void CGameObject85BFE8::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    m_nExpandTicks--;
    BOOL bDone = TRUE;

    // Expanded, it waits for its animation's end.
    if (m_nExpandTicks == 0) {
        if (m_animation.IsEndOfSequence(FALSE)) {
            RemoveFromArea();
            return;
        }

        m_nExpandTicks++;
        m_animation.FrameAdvance();
        return;
    }

    LONG nGridSize = (2 * m_nGridY + 1) * (2 * m_nGridX + 1);
    memset(m_pGrid, 0, nGridSize);

    for (SHORT nParticle = 0; nParticle < 4 * (m_nArc1 + m_nArc2); nParticle++) {
        if (m_pStates[nParticle] == 2) {
            continue;
        }

        Particle& particle = m_pParticles[nParticle];
        particle.x += particle.vx;
        particle.y += particle.vy;

        CPoint ptGrid(((particle.x >> 10) + m_pos.x) / CPathSearch::GRID_SQUARE_SIZEX,
            (((particle.y * 3) / 4 >> 10) + m_pos.y) / CPathSearch::GRID_SQUARE_SIZEY);

        // NOTE: CSearchBitmap::GetLOSCost reads the map at x + width * y with
        // no lower bound, so the original reads before the map for a square
        // whose index is negative (left of the area's first row, or above
        // the area). Ours takes such a square for a wall (P4-10); a square
        // left of the area on a later row reads the row above's last
        // squares, as the original does.
        SHORT nTableIndex;
        BYTE nCost = CPathSearch::COST_IMPASSABLE;
        if (ptGrid.x + m_pArea->m_search.m_GridSquareDimensions.cx * ptGrid.y >= 0) {
            nCost = m_pArea->m_search.GetLOSCost(ptGrid, m_terrainTable, nTableIndex, FALSE);
        }

        if (nCost == CPathSearch::COST_IMPASSABLE) {
            if (m_nWallMode == 1) {
                m_pStates[nParticle] = 2;
            }

            if (m_nWallMode != 0) {
                if (m_pStates[nParticle] == 0) {
                    bDone = FALSE;
                }
                continue;
            }

            // Bounce off the wall.
            LONG nOldGridY = (((particle.y - particle.vy) * 3) / 4 >> 10) + m_pos.y;
            nOldGridY /= CPathSearch::GRID_SQUARE_SIZEY;
            LONG nOldGridX = ((particle.x - particle.vx) >> 10) + m_pos.x;
            nOldGridX /= CPathSearch::GRID_SQUARE_SIZEX;

            if (nOldGridX != ptGrid.x) {
                particle.x -= 2 * particle.vx;
                particle.vx = -particle.vx;
            }

            if (nOldGridY != ptGrid.y) {
                particle.y -= 2 * particle.vy;
                particle.vy = -particle.vy;
            }
        }

        LONG nCell = ((particle.y / CPathSearch::GRID_SQUARE_SIZEX >> 10) + m_nGridY) * (2 * m_nGridX + 1)
            + (particle.x / CPathSearch::GRID_SQUARE_SIZEX >> 10)
            + m_nGridX;

        // Now and then a lingering piece, less often while the cloud is
        // young: one chance in ((ticks left)^3 / 1000 + 10) * 3 / 2.
        if (m_nFlyingAnimation != 0) {
            UINT nTicks = m_nExpandTicks;
            UINT nRange = (nTicks * nTicks * nTicks / 1000 + 10) * 3 >> 1;
            if (nRange == 0 || rand() % static_cast<INT>(nRange) == 0) {
                new CGameObject85BF7C(m_nLingeringAnimation,
                    m_colorRangeValues,
                    m_sPieceSound,
                    m_pArea,
                    CPoint(particle.x + (m_pos.x << 10), (particle.y * 3) / 4 + (m_pos.y << 10)),
                    0,
                    CPoint(0, 0),
                    static_cast<WORD>((m_nExpandTicks << 1) + m_nDuration),
                    0,
                    m_nWallMode);
            }
        }

        // NOTE: The original does not check the cell against the grid; ours
        // counts a cell outside it as entered (P4-10).
        BYTE nEntered = 1;
        if (nCell >= 0 && nCell < nGridSize) {
            nEntered = m_pGrid[nCell]++;
        }

        if (nEntered == 0 && m_pStates[nParticle] == 0) {
            // The first particle in a square throws a flying piece.
            new CGameObject85BF7C(m_nFlyingAnimation,
                m_colorRangeValues,
                m_sPieceSound,
                m_pArea,
                CPoint(particle.x + (m_pos.x << 10), (particle.y * 3) / 4 + (m_pos.y << 10)),
                0,
                CPoint(particle.vx, (particle.vy * 3) / 4),
                m_nExpandTicks,
                0,
                m_nWallMode);

            m_pStates[nParticle] = 1;
        } else if (m_pStates[nParticle] == 0) {
            bDone = FALSE;
        }
    }

    if (bDone && m_animation.IsEndOfSequence(FALSE)) {
        RemoveFromArea();
        return;
    }

    m_animation.FrameAdvance();
}

// 0x7122E0
void CGameObject85BFE8::Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface)
{
    (void)pArea;
    (void)nSurface;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 14661
    UTIL_ASSERT(pVidMode != NULL);
}

// 0x712310
void CGameObject85BFE8::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
        CGameObjectArray::THREAD_ASYNCH,
        NULL,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 14752
        UTIL_ASSERT(FALSE);
        return;
    }

    delete this;
}
