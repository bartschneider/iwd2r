#include "CVisibility.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CInfGame.h"
#include "CPathSearch.h"
#include "CSearchBitmap.h"
#include "CUtil.h"
#include "CVidMode.h"

// 0x84EDCF
const BYTE CVisibilityMap::EXPLORED_RANGE_0 = 4;

// 0x84EDD0
const BYTE CVisibilityMap::EXPLORED_RANGE_1 = 8;

// 0x84EDD1
const BYTE CVisibilityMap::EXPLORED_RANGE_2 = 12;

// 0x84EDD2
const BYTE CVisibilityMap::EXPLORED_RANGE_3 = 14;

// 0x84EDD3
const BYTE CVisibilityMap::EXPLORED_RANGE_4 = 16;

// 0x84EDD4
const BYTE CVisibilityMap::EXPLORED_RANGE_5 = 20;

// 0x84EDD5
const BYTE CVisibilityMap::EXPLORED_RANGE_6 = 24;

// 0x84EDD6
const BYTE CVisibilityMap::SQUARE_SIZEX = 32;

// 0x84EDD7
const BYTE CVisibilityMap::SQUARE_SIZEY = 32;

// 0x84EDC4
const BYTE CVisibilityMap::SEARCH_TO_VIS_Y[8] = { 0, 0, 1, 1, 1, 2, 2, 2 };

// 0x84EDCC
const BYTE CVisibilityMap::VIS_TO_SEARCH_Y[3] = { 0, 2, 5 };

// 0x84EDDB
const BYTE CVisibilityMap::EXPLORED = 0;

// 0x84EDD8
const BYTE CVisibilityMap::EXPLORED_FLAG = 0x80;

// 0x84EDD9
const BYTE CVisibilityMap::VISIBILITY_MASK = 0x3F;

// 0x84EDDA
const BYTE CVisibilityMap::UNEXPLORED = 0xF;

// 0x84EDDC
const TILE_CODE CVisibilityMap::EXPLORED_FULL = { 0, 0, 0, 0 };

// 0x84EDE0
const BYTE CVisibilityMap::UNEXPLORED_N = 9;

// 0x84EDE1
const BYTE CVisibilityMap::UNEXPLORED_NE = 13;

// 0x84EDE2
const BYTE CVisibilityMap::UNEXPLORED_E = 12;

// 0x84EDE3
const BYTE CVisibilityMap::UNEXPLORED_SE = 14;

// 0x84EDE4
const BYTE CVisibilityMap::UNEXPLORED_S = 6;

// 0x84EDE5
const BYTE CVisibilityMap::UNEXPLORED_SW = 7;

// 0x84EDE6
const BYTE CVisibilityMap::UNEXPLORED_W = 3;

// 0x84EDE7
const BYTE CVisibilityMap::UNEXPLORED_NW = 11;

// 0x84EDE8
const TILE_CODE CVisibilityMap::UNEXPLORED_FULL = { 0xF, 0xF, 0xF, 0xF };

// 0x84EDEC
const BYTE CVisibilityMap::FOGOWAR_EXPLORED_MASK = 0xF;

// 0x84EDED
const BYTE CVisibilityMap::FOGOWAR_EXPLORED_SHIFT = 0;

// 0x84EDEE
const BYTE CVisibilityMap::FOGOWAR_MASK = 0xF0;

// 0x84EDEF
const BYTE CVisibilityMap::FOGOWAR_SHIFT = 4;

// 0x8AE818
POLYCOLOR CVisibilityMap::FOGOWARSHADE = { 0.0f, 0.0f, 0.0f, 0.5f };

// 0x8AE828
POLYCOLOR CVisibilityMap::EXPLORESHADE = { 0.0f, 0.0f, 0.0f, 1.0f };

// 0x8E1810
POLYCOLOR CVisibilityMap::CLEARSHADE;

// 0x551770
CVisibilityMap::CVisibilityMap()
{
    field_4 = 0;
    m_nWidth = 0;
    m_nHeight = 0;
    field_1A = 0;
    memset(m_aCharacterIds, 0, sizeof(m_aCharacterIds));
    m_pMap = NULL;
    m_pSearchMap = NULL;
    field_12 = 0;
    field_16 = 0;
    field_E = 14;
    field_10 = 10;
    sub_5518A0();
    m_bOutDoor = FALSE;
}

// 0x551800
CVisibilityMap::~CVisibilityMap()
{
    if (field_12) {
        delete field_12;
    }

    if (field_16) {
        delete field_16;
    }
}

// 0x551840
BOOL CVisibilityMap::AddCharacter(const CPoint& pos, LONG charId, const BYTE* pVisibleTerrainTable)
{
    SHORT nIndex;
    for (nIndex = 0; nIndex < 6; nIndex++) {
        if (m_aCharacterIds[nIndex] == CGameObjectArray::INVALID_INDEX) {
            m_aCharacterIds[nIndex] = charId;
            break;
        }

        if (m_aCharacterIds[nIndex] == charId) {
            break;
        }
    }

    if (nIndex != 6) {
        PrivateAddCharacter(pos, 1 << nIndex, pVisibleTerrainTable);
        return TRUE;
    }

    return FALSE;
}

// 0x5518A0
void CVisibilityMap::sub_5518A0()
{
    if (field_12 != NULL) {
        delete field_12;
    }

    if (field_16 != NULL) {
        delete field_16;
    }

    field_12 = new BYTE[field_E];
    field_16 = new BYTE[field_10];

    field_4 = static_cast<BYTE>(g_pBaldurChitin->GetCurrentVideoMode()->GetEllipseArcPixelList(field_E,
        field_10,
        field_12));

    field_1A = static_cast<BYTE>(g_pBaldurChitin->GetCurrentVideoMode()->GetEllipseArcPixelList(field_10,
        field_E,
        field_16));
}

// 0x551950
void CVisibilityMap::ClimbWall(const CPoint& ptStart, const CPoint& ptEnd, BYTE charId, const BYTE* pVisibileTerrainTable, SHORT nHighest)
{
    LONG nTopYValue = max(ptEnd.y, 0);
    LONG nCurrTile = ptStart.x + m_nWidth * ptStart.y;

    switch (nHighest) {
    case 3:
        if (ptStart.y - 2 >= nTopYValue) {
            // NOTE: Uninline.
            SetTileVisible(nCurrTile - 2 * m_nWidth, charId);
        }
        // FALLTHROUGH
    case 2:
        if (ptStart.y - 1 >= nTopYValue) {
            // NOTE: Uninline.
            SetTileVisible(nCurrTile - m_nWidth, charId);
        }
        // FALLTHROUGH
    case 1:
        if (ptStart.y >= nTopYValue) {
            // NOTE: Uninline.
            SetTileVisible(nCurrTile, charId);
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
        // __LINE__: 333
        UTIL_ASSERT(FALSE);
    }
}

// 0x551A90
void CVisibilityMap::BltDitherPattern(LPDIRECTDRAWSURFACE pSurface, const TILE_CODE& tileCode)
{
    // TODO: Incomplete.
}

// 0x551FF0
void CVisibilityMap::BltFogOWar(LPDIRECTDRAWSURFACE pSurface, const TILE_CODE& tileCode)
{
    // TODO: Incomplete.
}

// 0x552590
BOOL CVisibilityMap::IsCharacterIdOnMap(LONG charId)
{
    for (SHORT nIndex = 0; nIndex < 6; nIndex++) {
        if (m_aCharacterIds[nIndex] == charId) {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x5525C0
BYTE CVisibilityMap::GetSubTileCode(LONG nTile)
{
    BYTE code = 0;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 857
    UTIL_ASSERT(nTile >= 0 && nTile < (m_nWidth * m_nHeight));

    if (!IsTileVisible(nTile)) {
        code |= 0x1;
    }

    if (!IsTileVisible(m_nWidth + nTile)) {
        code |= 0x2;
    }

    if (!IsTileVisible(m_nWidth + nTile + 1)) {
        code |= 0x4;
    }

    if (!IsTileVisible(nTile + 1)) {
        code |= 0x8;
    }

    code <<= 8;

    if (!IsTileExplored(nTile)) {
        code |= 0x1;
    }

    if (!IsTileExplored(m_nWidth + nTile)) {
        code |= 0x2;
    }

    if (!IsTileExplored(m_nWidth + nTile + 1)) {
        code |= 0x4;
    }

    if (!IsTileExplored(nTile + 1)) {
        code |= 0x8;
    }

    return code;
}

// 0x5526E0
void CVisibilityMap::GetTileCode(SHORT n64x64Tile, TILE_CODE& tileCode)
{
    // NOTE: Uninline.
    LONG nBossTile = GetBossTile(n64x64Tile);
    LONG nSecondRowTile = m_nWidth + nBossTile;

    BOOLEAN bCheckRight = (nBossTile + 1) % m_nWidth != 0;
    BOOLEAN bCheckDown = nSecondRowTile < m_nWidth * m_nHeight;

    CSingleLock mapLock(&m_critSect, FALSE);
    mapLock.Lock(INFINITE);

    tileCode.tileNW = GetSubTileCode(nBossTile);

    if (bCheckRight) {
        tileCode.tileNE = GetSubTileCode(nBossTile + 1);
    }

    if (bCheckDown) {
        tileCode.tileSW = GetSubTileCode(nSecondRowTile);

        if (bCheckRight) {
            tileCode.tileSE = GetSubTileCode(nSecondRowTile + 1);
        }
    }

    mapLock.Unlock();
}

// 0x5527E0
void CVisibilityMap::Init(CGameArea* pArea)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 988
    UTIL_ASSERT(m_pMap == NULL);

    m_pSearchMap = &(pArea->m_search);
    m_nWidth = static_cast<SHORT>(pArea->GetInfinity()->nAreaX / 32 + 1);
    m_nHeight = static_cast<SHORT>(pArea->GetInfinity()->nAreaY / 32 + 1);
    m_nMapSize = m_nWidth * m_nHeight;

    if (m_pMap != NULL) {
        delete m_pMap;
    }

    m_pMap = new BYTE[m_nMapSize];
    memset(m_pMap, 0, m_nMapSize);

    for (SHORT nIndex = 0; nIndex < 6; nIndex++) {
        m_aCharacterIds[nIndex] = CGameObjectArray::INVALID_INDEX;
    }

    m_bOutDoor = (pArea->m_header.m_areaType & 0x1) != 0;
}

// 0x5528B0
void CVisibilityMap::Marshal(BYTE** pData, DWORD* pnData)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 1045
    UTIL_ASSERT(pData != NULL && pnData != NULL);

    *pnData = (m_nMapSize + 7) / 8;
    *pData = new BYTE[*pnData];
    if (*pData == NULL) {
        *pnData = NULL;
        return;
    }

    LONG nTile = 0;
    LONG nByte = 0;
    BYTE byte;
    BYTE byteMask;
    BYTE nMaxBit;

    while (nTile < m_nMapSize) {
        nMaxBit = static_cast<BYTE>(min(m_nMapSize - nTile, 8));
        byte = 0;
        byteMask = 1;
        while (nMaxBit > 0) {
            // NOTE: Uninline.
            if (IsTileExplored(nTile)) {
                byte |= byteMask;
            }
            byteMask <<= 1;
            nTile++;
            nMaxBit--;
        }
        (*pData)[nByte++] = byte;
    }
}

// 0x552990
void CVisibilityMap::Unmarshal(BYTE* pData, DWORD nData)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 1100
    UTIL_ASSERT(pData != NULL && nData > 0);

    LONG nTile = 0;
    LONG nByte = 0;
    BYTE byte;
    BYTE byteMask;
    BYTE nMaxBit;

    while (nTile < m_nMapSize) {
        nMaxBit = static_cast<BYTE>(min(m_nMapSize - nTile, 8));
        byte = pData[nByte++];
        byteMask = 1;
        while (nMaxBit > 0) {
            if ((byte & byteMask) != 0) {
                // NOTE: Uninline.
                SetTileExplored(nTile);
            }
            byteMask <<= 1;
            nTile++;
            nMaxBit--;
        }
    }
}

// 0x552A50
void CVisibilityMap::MarkTileLine(const CPoint& ptView, const CPoint& ptSearchMapView, LONG nDestX, LONG nDestY, const CPoint& ptOffset, BYTE charId, const BYTE* pVisibleTerrainTable, BOOLEAN bLookingUp)
{
    SHORT nStepX = 1;
    SHORT nStepY = 1;
    INT nXFrom = ptView.x;
    INT nYFrom = ptView.y;
    INT nXTo = nDestX;
    INT nYTo = nDestY;
    CPoint ptSearch = ptSearchMapView;
    CPoint ptTile;
    LONG nLastTileX = -1;
    LONG nLastTileY = -1;
    SHORT nHighest = 0;
    BOOLEAN bHitWall = FALSE;
    SHORT nTableIndex;
    BYTE cost;
    SHORT nHeight;

    // NOTE: ptOffset is not read; the original's fifth argument is unused.

    // NOTE: The clipped start is not used afterwards, and the result is
    // ignored: a destination the clip leaves outside the map trips
    // SetTileVisible's assert, as in the original.
    CRect rClip(0, 0, m_nWidth - 1, m_nHeight - 1);
    CVidMode::ClipLine(nXFrom, nYFrom, nXTo, nYTo, rClip);

    CPoint ptDest(nXTo, nYTo);

    CPoint ptSearchDest;
    ptSearchDest.x = ptDest.x * 2;
    ptSearchDest.y = VIS_TO_SEARCH_Y[ptDest.y % 3] + (ptDest.y / 3) * 8;

    LONG nDeltaX = ptSearchDest.x - ptSearch.x;
    LONG nDeltaY = ptSearchDest.y - ptSearch.y;

    if (nDeltaX < 0) {
        nStepX = -1;
        nDeltaX = -nDeltaX;
    }

    if (nDeltaY < 0) {
        nStepY = -1;
        nDeltaY = -nDeltaY;
    }

    LONG nIncrementE;
    LONG nIncrementNE;
    LONG nDecision;

    if (nDeltaY < nDeltaX) {
        nIncrementE = nDeltaY * 2;
        nDecision = nIncrementE - nDeltaX;
        nIncrementNE = (nDeltaY - nDeltaX) * 2;

        while (ptSearch.x != ptSearchDest.x || ptSearch.y != ptSearchDest.y) {
            if (ptDest.x != m_nWidth - 1 || bHitWall) {
                if (ptDest.y != m_nHeight - 1 || bHitWall) {
                    cost = m_pSearchMap->GetLOSCost(ptSearch, pVisibleTerrainTable, nTableIndex, m_bOutDoor);
                    if (cost == CPathSearch::COST_IMPASSABLE) {
                        bHitWall = TRUE;
                        nHeight = m_pSearchMap->GetStructureHeight(nTableIndex);
                        if (nHeight < nHighest) {
                            return;
                        }

                        nHighest = nHeight;

                        if (bLookingUp) {
                            ptTile.x = ptSearch.x >> 1;
                            ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
                            if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                                ClimbWall(ptTile, ptDest, charId, pVisibleTerrainTable, nHeight);
                                nLastTileY = ptTile.y;
                                nLastTileX = ptTile.x;
                            }
                        }
                    } else {
                        if (bHitWall) {
                            return;
                        }
                    }
                } else {
                    ptTile.x = ptSearch.x >> 1;
                    ptTile.y = ptDest.y;
                    if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                        // NOTE: Uninline.
                        SetTileVisible(m_nWidth * ptDest.y + ptTile.x, charId);
                    }
                }
            } else {
                ptTile.x = ptDest.x;
                ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
                if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                    // NOTE: Uninline.
                    SetTileVisible(m_nWidth * ptTile.y + ptDest.x, charId);
                }
            }

            ptTile.x = ptSearch.x >> 1;
            ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
            if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                // NOTE: Uninline.
                SetTileVisible(m_nWidth * ptTile.y + ptTile.x, charId);
                nLastTileY = ptTile.y;
                nLastTileX = ptTile.x;
            }

            if (nDecision < 1) {
                nDecision += nIncrementE;
                ptSearch.x += nStepX;
            } else {
                nDecision += nIncrementNE;
                ptSearch.y += nStepY;
                ptSearch.x += nStepX;
            }
        }
    } else {
        nIncrementE = nDeltaX * 2;
        nDecision = nIncrementE - nDeltaY;
        nIncrementNE = (nDeltaX - nDeltaY) * 2;

        while (ptSearch.x != ptSearchDest.x || ptSearch.y != ptSearchDest.y) {
            if (ptDest.x != m_nWidth - 1 || bHitWall) {
                if (ptDest.y != m_nHeight - 1 || bHitWall) {
                    cost = m_pSearchMap->GetLOSCost(ptSearch, pVisibleTerrainTable, nTableIndex, m_bOutDoor);
                    if (cost == CPathSearch::COST_IMPASSABLE) {
                        bHitWall = TRUE;
                        nHeight = m_pSearchMap->GetStructureHeight(nTableIndex);
                        if (nHeight < nHighest) {
                            return;
                        }

                        nHighest = nHeight;

                        if (bLookingUp) {
                            ptTile.x = ptSearch.x >> 1;
                            ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
                            if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                                ClimbWall(ptTile, ptDest, charId, pVisibleTerrainTable, nHeight);
                                nLastTileY = ptTile.y;
                                nLastTileX = ptTile.x;
                            }
                        }
                    } else {
                        if (bHitWall) {
                            return;
                        }
                    }
                } else {
                    ptTile.x = ptSearch.x >> 1;
                    ptTile.y = ptDest.y;
                    if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                        // NOTE: Uninline.
                        SetTileVisible(m_nWidth * ptDest.y + ptTile.x, charId);
                    }
                }
            } else {
                ptTile.x = ptDest.x;
                ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
                if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                    // NOTE: Uninline.
                    SetTileVisible(m_nWidth * ptTile.y + ptDest.x, charId);
                }
            }

            ptTile.x = ptSearch.x >> 1;
            ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
            if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
                // NOTE: Uninline.
                SetTileVisible(m_nWidth * ptTile.y + ptTile.x, charId);
                nLastTileY = ptTile.y;
                nLastTileX = ptTile.x;
            }

            if (nDecision < 1) {
                nDecision += nIncrementE;
                ptSearch.y += nStepY;
            } else {
                nDecision += nIncrementNE;
                ptSearch.y += nStepY;
                ptSearch.x += nStepX;
            }
        }
    }

    if (ptDest.x == m_nWidth - 1) {
        ptTile.x = ptDest.x;
        ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
        if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
            // NOTE: Uninline.
            SetTileVisible(m_nWidth * ptTile.y + ptDest.x, charId);
        }
    } else if (ptDest.y == m_nHeight - 1) {
        ptTile.x = ptSearch.x >> 1;
        ptTile.y = ptDest.y;
        if (nLastTileX != ptTile.x || nLastTileY != ptTile.y) {
            // NOTE: Uninline.
            SetTileVisible(m_nWidth * ptDest.y + ptTile.x, charId);
        }
    } else {
        cost = m_pSearchMap->GetLOSCost(ptSearch, pVisibleTerrainTable, nTableIndex, m_bOutDoor);
        if (cost == CPathSearch::COST_IMPASSABLE) {
            nHeight = m_pSearchMap->GetStructureHeight(nTableIndex);
            if (nHeight < nHighest) {
                return;
            }

            ptTile.x = ptSearch.x >> 1;
            ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
            if (nLastTileX == ptTile.x && nLastTileY == ptTile.y) {
                return;
            }

            if (bLookingUp) {
                ClimbWall(ptTile, ptDest, charId, pVisibleTerrainTable, nHeight);
                return;
            }
        } else {
            if (bHitWall) {
                return;
            }

            ptTile.x = ptSearch.x >> 1;
            ptTile.y = SEARCH_TO_VIS_Y[ptSearch.y & 7] + (ptSearch.y >> 3) * 3;
            if (nLastTileX == ptTile.x && nLastTileY == ptTile.y) {
                return;
            }
        }

        // NOTE: Uninline.
        SetTileVisible(m_nWidth * ptTile.y + ptTile.x, charId);
    }
}

// 0x553290
void CVisibilityMap::PrivateAddCharacter(const CPoint& ptPos, BYTE visId, const BYTE* pVisibleTerrainTable)
{
    CPoint ptEar;
    LONG earZ;

    CPoint ptView;
    ptView.x = ptPos.x / 32;
    ptView.y = ptPos.y / 32;

    CPoint ptOffset;
    ptOffset.x = ptPos.x % 32;
    ptOffset.y = ptPos.y % 32;

    CPoint ptSearchMapView;
    ptSearchMapView.x = ptPos.x / CPathSearch::GRID_SQUARE_SIZEX;
    ptSearchMapView.y = ptPos.y / CPathSearch::GRID_SQUARE_SIZEY;

    sub_553800(ptView, ptOffset, ptSearchMapView, visId, pVisibleTerrainTable);
    sub_553990(ptView, ptOffset, ptSearchMapView, visId, pVisibleTerrainTable);

    g_pBaldurChitin->cSoundMixer.GetListenPosition(ptEar, earZ);
    if (ptEar.x == -1 && ptEar.y == -1) {
        if (m_pSearchMap->GetArea() == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()) {
            m_pSearchMap->GetArea()->GetInfinity()->UpdateListenPosition();
        }
    }
}

// 0x5533B0
void CVisibilityMap::RemoveCharacter(const CPoint& pos, LONG charId, const BYTE* pVisibleTerrainTable)
{
    CPoint pt;
    pt.x = pos.x / 32;
    pt.y = pos.y / 32;

    for (SHORT nIndex = 0; nIndex < 6; nIndex++) {
        if (m_aCharacterIds[nIndex] == charId) {
            m_aCharacterIds[nIndex] = CGameObjectArray::INVALID_INDEX;

            BYTE visId = 1 << nIndex;
            if (visId != -1) {
                sub_553440(pt, visId);
                sub_5535D0(pt, visId);
            }

            break;
        }
    }
}

// 0x553440
void CVisibilityMap::sub_553440(const CPoint& pt, BYTE charId)
{
    LONG nLeft = pt.x;
    LONG nRight;
    const BYTE* pArc = field_12;
    BYTE nArcLength = field_4;
    LONG y = pt.y - field_10;
    SHORT nStepY = 1;
    SHORT nRemaining;
    SHORT nNext;
    BYTE code;
    LONG nStart;
    LONG nEnd;
    LONG nCount;
    LONG nIndex;

    for (INT nPass = 0; nPass < 2; nPass++) {
        nRemaining = nArcLength;
        nRight = nLeft;
        if (nRemaining != 0) {
            do {
                code = *pArc;
                nNext = nRemaining - 1;
                while (TRUE) {
                    nRight++;
                    pArc++;
                    nRemaining = nNext;
                    if (code != 0) {
                        break;
                    }

                    nRemaining = nNext - 1;
                    if (nNext == 0) {
                        break;
                    }

                    code = *pArc;
                    nLeft--;
                    nNext = nRemaining;
                }

                if (y >= 0 && y < m_nHeight) {
                    nStart = max(nLeft, 0);
                    nEnd = min(nRight, m_nWidth - 1);
                    if (nStart <= nEnd) {
                        nCount = nEnd - nStart + 1;
                        nIndex = m_nWidth * y + nStart;
                        do {
                            // NOTE: Uninline.
                            DecrementVisibility(nIndex, charId);
                            nCount--;
                            nIndex++;
                        } while (nCount != 0);
                    }
                }

                y += nStepY;
                nLeft--;
            } while (nRemaining > 0);
        }

        nLeft = pt.x;
        y = field_10 + pt.y;
        pArc = field_12;
        nArcLength = field_4;
        nStepY = -1;
    }
}

// 0x5535D0
void CVisibilityMap::sub_5535D0(const CPoint& pt, BYTE charId)
{
    LONG y = pt.y;
    LONG nRight = pt.x + field_E;
    const BYTE* pArc = field_16;
    LONG nLeft = pt.x - field_E + 1;
    SHORT nArcLength = field_1A;
    SHORT nStepY = -1;
    SHORT nRows;
    LONG nStart;
    LONG nEnd;
    LONG nCount;
    LONG nIndex;
    BYTE nFirst;

    for (INT nPass = 0; nPass < 2; nPass++) {
        if (nArcLength >= 0) {
            nRows = nArcLength + 1;
            do {
                if (y >= 0 && y < m_nHeight) {
                    nStart = max(nLeft, 0);
                    nEnd = min(nRight, m_nWidth - 1);
                    if (nStart <= nEnd) {
                        nCount = nEnd - nStart + 1;
                        nIndex = m_nWidth * y + nStart;
                        do {
                            // NOTE: Uninline.
                            DecrementVisibility(nIndex, charId);
                            nCount--;
                            nIndex++;
                        } while (nCount != 0);
                    }
                }

                if (*pArc++ != 0) {
                    nLeft++;
                    nRight--;
                }

                y += nStepY;
                nRows--;
            } while (nRows != 0);
        }

        nFirst = *field_16;
        y = pt.y + 1;
        nStepY = 1;
        nLeft = pt.x + 1 + (nFirst - field_E);
        nRight = (pt.x - nFirst) + field_E;
        pArc = field_16 + 1;
        nArcLength = field_1A - 1;
    }
}

// 0x553740
void CVisibilityMap::SetAreaExplored()
{
    for (LONG nTile = 0; nTile < m_nWidth * m_nHeight; nTile++) {
        // NOTE: Uninline.
        SetTileExplored(nTile);
    }
}

// 0x5537A0
void CVisibilityMap::SetAreaUnexplored()
{
    for (LONG nTile = 0; nTile < m_nWidth * m_nHeight; nTile++) {
        // NOTE: Uninline.
        SetTileUnexplored(nTile);
    }
}

// 0x553800
void CVisibilityMap::sub_553800(const CPoint& ptView, const CPoint& ptOffset, const CPoint& ptSearchMapView, BYTE charId, const BYTE* pVisibleTerrainTable)
{
    const BYTE* pArc = field_12;
    LONG y = ptView.y - field_10;
    LONG x = ptView.x;
    BYTE nArcLength = field_4;
    BOOLEAN bLookingUp = TRUE;
    SHORT nStepY = 1;
    LONG nRight;
    LONG nStepLeft;
    LONG nStepRight;
    LONG nCount;

    for (INT nPass = 0; nPass < 2; nPass++) {
        if (nArcLength != 0) {
            nStepLeft = x + 1;
            nCount = nArcLength;
            nStepRight = x;
            nRight = x;
            do {
                nRight++;
                MarkTileLine(ptView, ptSearchMapView, x, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                MarkTileLine(ptView, ptSearchMapView, nRight, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                x--;
                nStepLeft--;
                nStepRight++;
                if (*pArc++ != 0) {
                    y += nStepY;
                    MarkTileLine(ptView, ptSearchMapView, nStepLeft, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                    MarkTileLine(ptView, ptSearchMapView, nStepRight, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                }
                nCount--;
            } while (nCount != 0);
        }

        x = ptView.x;
        pArc = field_12;
        y = field_10 + ptView.y;
        nArcLength = field_4;
        bLookingUp = FALSE;
        nStepY = -1;
    }
}

// 0x553990
void CVisibilityMap::sub_553990(const CPoint& ptView, const CPoint& ptOffset, const CPoint& ptSearchMapView, BYTE charId, const BYTE* pVisibleTerrainTable)
{
    const BYTE* pArc = field_16;
    LONG y = ptView.y;
    LONG nLeft = ptView.x - field_E + 1;
    SHORT nArcLength = field_1A;
    BOOLEAN bLookingUp = TRUE;
    SHORT nStepY = -1;
    LONG nRight = ptView.x + field_E;
    SHORT nRows;
    BYTE nFirst;

    for (INT nPass = 0; nPass < 2; nPass++) {
        if (nArcLength >= 0) {
            nRows = nArcLength + 1;
            do {
                MarkTileLine(ptView, ptSearchMapView, nLeft, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                MarkTileLine(ptView, ptSearchMapView, nRight, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                if (pArc != NULL && *pArc++ != 0) {
                    nLeft++;
                    nRight--;
                    MarkTileLine(ptView, ptSearchMapView, nLeft, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                    MarkTileLine(ptView, ptSearchMapView, nRight, y, ptOffset, charId, pVisibleTerrainTable, bLookingUp);
                }
                y += nStepY;
                nRows--;
            } while (nRows != 0);
        }

        nFirst = *field_16;
        y = ptView.y + 1;
        bLookingUp = FALSE;
        nLeft = (nFirst - field_E) + 1 + ptView.x;
        nRight = (ptView.x - nFirst) + field_E;
        pArc = field_16 + 1;
        nStepY = 1;
        nArcLength = field_1A - 1;
    }
}

// 0x553B10
void CVisibilityMap::Uninit()
{
    if (m_pMap != NULL) {
        delete m_pMap;
        m_pMap = NULL;
    }

    for (SHORT nIndex = 0; nIndex < 6; nIndex++) {
        m_aCharacterIds[nIndex] = CGameObjectArray::INVALID_INDEX;
    }
}

// 0x553B40
void CVisibilityMap::UpDate(const CPoint& ptOldPos, const CPoint& ptNewPos, LONG charId, const BYTE* pVisibleTerrainTable)
{
    CPoint pt;
    pt.x = ptOldPos.x / 32;
    pt.y = ptOldPos.y / 32;

    for (SHORT nIndex = 0; nIndex < 6; nIndex++) {
        if (m_aCharacterIds[nIndex] == charId) {
            // NOTE: The original compares the signed char 1 << nIndex with
            // -1, which never matches.
            BYTE visId = 1 << nIndex;
            sub_553440(pt, visId);
            sub_5535D0(pt, visId);
            PrivateAddCharacter(ptNewPos, visId, pVisibleTerrainTable);
            return;
        }
    }
}

// 0x553BD0
BOOLEAN CVisibilityMap::IsTileExplored(LONG nIndex)
{
    if (nIndex >= 0 && nIndex < m_nMapSize) {
        return (m_pMap[nIndex] & EXPLORED_FLAG) != 0;
    } else {
        return FALSE;
    }
}

// 0x553C00
BOOLEAN CVisibilityMap::IsTileVisible(LONG nIndex)
{
    if (nIndex >= 0 && nIndex < m_nMapSize) {
        return (m_pMap[nIndex] & VISIBILITY_MASK) != 0;
    } else {
        return FALSE;
    }
}

// 0x553C30
void CVisibilityMap::SetAreaVisible(BOOLEAN bVisible)
{
    for (INT nIndex = 0; nIndex < m_nWidth * m_nHeight; nIndex++) {
        if (bVisible == TRUE) {
            m_pMap[nIndex] |= VISIBILITY_MASK;
        } else {
            m_pMap[nIndex] &= ~VISIBILITY_MASK;
        }
    }
}

// 0x553D00
void CVisibilityMap::BltFogOWar3d(INT x, INT y, const CRect& rDest, const TILE_CODE& tileCode)
{
    BYTE aTileCodes[4];
    CSize aSizes[4];
    CRect r;
    BYTE nFogWarCode;
    BYTE nExploredCode;

    aSizes[0].cx = x;
    aSizes[0].cy = y;
    aSizes[1].cx = x + 32;
    aSizes[1].cy = y;
    aSizes[2].cx = x;
    aSizes[2].cy = y + 32;
    aSizes[3].cx = x + 32;
    aSizes[3].cy = y + 32;

    aTileCodes[0] = tileCode.tileNW;
    aTileCodes[1] = tileCode.tileNE;
    aTileCodes[2] = tileCode.tileSW;
    aTileCodes[3] = tileCode.tileSE;

    COLORREF rgb = g_pChitin->GetCurrentVideoMode()->ApplyFadeAmount(RGB(0, 0, 0));

    FOGOWARSHADE.fRed = static_cast<float>(GetRValue(rgb)) / 255.0f;
    EXPLORESHADE.fRed = FOGOWARSHADE.fRed;
    CLEARSHADE.fRed = FOGOWARSHADE.fRed;

    FOGOWARSHADE.fGreen = static_cast<float>(GetGValue(rgb)) / 255.0f;
    EXPLORESHADE.fGreen = FOGOWARSHADE.fGreen;
    CLEARSHADE.fGreen = FOGOWARSHADE.fGreen;

    FOGOWARSHADE.fBlue = static_cast<float>(GetBValue(rgb)) / 255.0f;
    EXPLORESHADE.fBlue = FOGOWARSHADE.fBlue;
    CLEARSHADE.fBlue = FOGOWARSHADE.fBlue;

    for (int index = 0; index < 4; index++) {
        nExploredCode = (aTileCodes[index] & FOGOWAR_EXPLORED_MASK) >> FOGOWAR_EXPLORED_SHIFT;
        nFogWarCode = (aTileCodes[index] & FOGOWAR_MASK) >> FOGOWAR_SHIFT;
        r.SetRect(aSizes[index].cx,
            aSizes[index].cy,
            aSizes[index].cx + 32,
            aSizes[index].cy + 32);
        if (nExploredCode != nFogWarCode) {
            BltVisibility3d(nFogWarCode, r);
        }
        BltExploration3d(nExploredCode, r);
    }
}

// 0x553EA0
void CVisibilityMap::BltExploration3d(BYTE code, const CRect& rDest)
{
    VERTEX_DESC verts[6];
    int cnt = 0;

    verts[0].fX = static_cast<float>(rDest.left + 16);
    verts[0].fY = static_cast<float>(rDest.top + 16);
    verts[1].fX = static_cast<float>(rDest.left);
    verts[1].fY = static_cast<float>(rDest.top);
    verts[2].fX = static_cast<float>(rDest.left);
    verts[2].fY = static_cast<float>(rDest.bottom);
    verts[3].fX = static_cast<float>(rDest.right);
    verts[3].fY = static_cast<float>(rDest.bottom);
    verts[4].fX = static_cast<float>(rDest.right);
    verts[4].fY = static_cast<float>(rDest.top);
    verts[5].fX = static_cast<float>(rDest.left);
    verts[5].fY = static_cast<float>(rDest.top);

    if ((code & 0x1) != 0) {
        verts[1].color = EXPLORESHADE;
        verts[5].color = EXPLORESHADE;
        cnt++;
    } else {
        verts[1].color = CLEARSHADE;
        verts[5].color = CLEARSHADE;
    }

    if ((code & 0x2) != 0) {
        verts[2].color = EXPLORESHADE;
        cnt++;
    } else {
        verts[2].color = CLEARSHADE;
    }

    if ((code & 0x4) != 0) {
        verts[3].color = EXPLORESHADE;
        cnt++;
    } else {
        verts[3].color = CLEARSHADE;
    }

    if ((code & 0x8) != 0) {
        verts[4].color = EXPLORESHADE;
        cnt++;
    } else {
        verts[4].color = CLEARSHADE;
    }

    switch (cnt) {
    case 3:
        verts[0].color = EXPLORESHADE;
        CVidMode::RenderFan(verts, 6);
        break;
    case 1:
        verts[0].color = CLEARSHADE;
        CVidMode::RenderFan(verts, 6);
        break;
    default:
        CVidMode::FillRect3d(&(verts[1]));
        break;
    }
}

// 0x554170
void CVisibilityMap::BltVisibility3d(BYTE code, const CRect& rDest)
{
    VERTEX_DESC verts[6];
    int cnt = 0;

    verts[0].fX = static_cast<float>(rDest.left + 16);
    verts[0].fY = static_cast<float>(rDest.top + 16);
    verts[1].fX = static_cast<float>(rDest.left);
    verts[1].fY = static_cast<float>(rDest.top);
    verts[2].fX = static_cast<float>(rDest.left);
    verts[2].fY = static_cast<float>(rDest.bottom);
    verts[3].fX = static_cast<float>(rDest.right);
    verts[3].fY = static_cast<float>(rDest.bottom);
    verts[4].fX = static_cast<float>(rDest.right);
    verts[4].fY = static_cast<float>(rDest.top);
    verts[5].fX = static_cast<float>(rDest.left);
    verts[5].fY = static_cast<float>(rDest.top);

    if ((code & 0x1) != 0) {
        verts[1].color = FOGOWARSHADE;
        verts[5].color = FOGOWARSHADE;
        cnt++;
    } else {
        verts[1].color = CLEARSHADE;
        verts[5].color = CLEARSHADE;
    }

    if ((code & 0x2) != 0) {
        verts[2].color = FOGOWARSHADE;
        cnt++;
    } else {
        verts[2].color = CLEARSHADE;
    }

    if ((code & 0x4) != 0) {
        verts[3].color = FOGOWARSHADE;
        cnt++;
    } else {
        verts[3].color = CLEARSHADE;
    }

    if ((code & 0x8) != 0) {
        verts[4].color = FOGOWARSHADE;
        cnt++;
    } else {
        verts[4].color = CLEARSHADE;
    }

    switch (cnt) {
    case 3:
        verts[0].color = FOGOWARSHADE;
        CVidMode::RenderFan(verts, 6);
        break;
    case 1:
        verts[0].color = CLEARSHADE;
        CVidMode::RenderFan(verts, 6);
        break;
    default:
        CVidMode::FillRect3d(&(verts[1]));
        break;
    }
}

// 0x76B120
SHORT CVisibilityMap::PointToTile(const CPoint& pos)
{
    return static_cast<SHORT>(pos.x / SQUARE_SIZEX + m_nWidth * (pos.y / SQUARE_SIZEY));
}

// NOTE: Inlined.
LONG CVisibilityMap::GetBossTile(LONG n64x64Tile)
{
    return 2 * (n64x64Tile % (m_nWidth / 2) + m_nWidth * (n64x64Tile / (m_nWidth / 2)));
}

// NOTE: Inlined.
BYTE CVisibilityMap::GetCharacterPos(LONG charId)
{
    for (SHORT index = 0; index < 6; index++) {
        if (m_aCharacterIds[index] == charId) {
            return 1 << static_cast<BYTE>(index);
        }
    }
    return -1;
}

// NOTE: Inlined.
void CVisibilityMap::SetTileExplored(LONG nIndex)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 2281
    UTIL_ASSERT_MSG((nIndex >= 0 && nIndex < m_nMapSize), "Index into vis map out of range in SetTileExplored");

    m_pMap[nIndex] |= EXPLORED_FLAG;
}

// NOTE: Inlined.
void CVisibilityMap::SetTileUnexplored(LONG nIndex)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 2288
    UTIL_ASSERT_MSG((nIndex >= 0 && nIndex < m_nMapSize), "Index into vis map out of range in SetTileUnexplored");

    m_pMap[nIndex] &= ~EXPLORED_FLAG;
}

// NOTE: Inlined.
void CVisibilityMap::SetTileVisible(LONG nIndex, BYTE charId)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 2295
    UTIL_ASSERT_MSG((nIndex >= 0 && nIndex < m_nMapSize), "Index into vis map out of range in SetTileVisible");

    m_pMap[nIndex] |= charId | EXPLORED_FLAG;
}

// NOTE: Inlined.
void CVisibilityMap::DecrementVisibility(LONG nIndex, BYTE charId)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CVisibility.cpp
    // __LINE__: 2274
    UTIL_ASSERT_MSG((nIndex >= 0 && nIndex < m_nMapSize), "Index into vis map out of range in DecrementVisibility");

    m_pMap[nIndex] &= ~charId;
}
