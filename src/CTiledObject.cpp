#include "CTiledObject.h"

#include "CInfinity.h"
#include "CResWED.h"
#include "CUtil.h"

// 0x84EC08
const WORD CTiledObject::STATE_PRIMARY_TILE = 1;

// 0x84EC0A
const WORD CTiledObject::STATE_SECONDARY_TILE = 2;

// NOTE: Inlined.
CTiledObject::CTiledObject()
{
    m_posAreaList = NULL;
    m_pResWed = NULL;
    m_nWedIndex = 0;
    m_wAIState = 0;
    m_wRenderState = 0;
}

// 0x54E5B0
void CTiledObject::CheckTileState(CInfinity& cInfinity)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CTiledObject.cpp
    // __LINE__: 84
    UTIL_ASSERT(m_pResWed != NULL);

    WORD wState = m_wAIState;

    if (wState == m_wRenderState) {
        return;
    }

    if (m_nWedIndex == -1) {
        return;
    }

    m_wRenderState = wState;

    WORD* pTiledObjectList = m_pResWed->GetTiledObjectList();
    WORD* pTileList = m_pResWed->GetTileList(0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CTiledObject.cpp
    // __LINE__: 99
    UTIL_ASSERT(pTiledObjectList != NULL);

    if (pTiledObjectList == NULL) {
        return;
    }

    WED_TILEDOBJECT* pTiledObject = m_pResWed->GetTiledObject(static_cast<USHORT>(m_nWedIndex));
    WORD wStartingTile = pTiledObject->nStartingTile;
    WORD wEndTile = wStartingTile + pTiledObject->nNumTiles;

    // The polys' type byte (WED_POLYLIST.nType, +8) carries the tile state in
    // bit 1: cleared for the primary polys and set for the secondary ones in
    // the primary state, the reverse in the secondary state. The offset is a
    // byte offset into the WED data (0x54E720/0x54E7B8).
    auto TogglePolys = [&](DWORD nOffset, WORD nCount, bool bSet) {
        if (nCount == 0xFFFF || nCount == 0) {
            return;
        }

        for (WORD i = 0; i < nCount; i++) {
            BYTE* pType = reinterpret_cast<BYTE*>(m_pResWed->m_pHeader) + nOffset + i * sizeof(WED_POLYLIST) + 8;
            if (bSet) {
                *pType |= 0x2;
            } else {
                *pType &= ~0x2;
            }
        }
    };

    if (m_wRenderState == STATE_PRIMARY_TILE) {
        for (WORD i = wStartingTile; i < wEndTile; i++) {
            WED_TILEDATA* pTileData = m_pResWed->GetTileData(0, pTiledObjectList[i]);
            pTileData->wFlags = m_wRenderState;

            // Swap the tile's secondary image out for its primary one
            // (detach the secondary, attach the primary; 0x54E790-0x54E79A:
            // nSecondary is the last push, i.e. the first argument).
            cInfinity.SwapVRamTiles(pTileData->nSecondary, pTileList[pTileData->nStartingTile]);
        }

        TogglePolys(pTiledObject->nOffsetToPrimaryPolys, pTiledObject->nNumPrimaryPolys, false);
        TogglePolys(pTiledObject->nOffsetToSecondaryPolys, pTiledObject->nNumSecondaryPolys, true);
    } else {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CTiledObject.cpp
        // __LINE__: 130
        UTIL_ASSERT(m_wRenderState == STATE_SECONDARY_TILE);

        if (m_wRenderState != STATE_SECONDARY_TILE) {
            return;
        }

        for (WORD i = wStartingTile; i < wEndTile; i++) {
            WED_TILEDATA* pTileData = m_pResWed->GetTileData(0, pTiledObjectList[i]);
            pTileData->wFlags = m_wRenderState;

            // The reverse swap (detach the primary, attach the secondary;
            // 0x54E6B4-0x54E6BC: the tile list's value is the last push, i.e.
            // the first argument).
            cInfinity.SwapVRamTiles(pTileList[pTileData->nStartingTile], pTileData->nSecondary);
        }

        TogglePolys(pTiledObject->nOffsetToPrimaryPolys, pTiledObject->nNumPrimaryPolys, true);
        TogglePolys(pTiledObject->nOffsetToSecondaryPolys, pTiledObject->nNumSecondaryPolys, false);
    }
}

// 0x54E830
void CTiledObject::Initialize(CResWED* pResWed, CResRef resID, POSITION posAreaList, WORD wInitialState)
{
    CResRef cResRef;
    WED_TILEDOBJECT* pTiledObject;
    ULONG nTiledObjects;
    WORD nIndex;
    WORD nPoly;
    WED_POLYLIST* pPoly;

    m_pResWed = pResWed;
    m_posAreaList = posAreaList;

    pTiledObject = m_pResWed->GetTiledObjects();
    nTiledObjects = m_pResWed->GetNumTiledObjects();

    for (nIndex = 0; nIndex < nTiledObjects; nIndex++) {
        cResRef = pTiledObject->resID;
        if (resID == cResRef) {
            m_nWedIndex = nIndex;
            break;
        }
        pTiledObject++;
    }

    m_resId = resID;

    if (nIndex == nTiledObjects) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CTiledObject.cpp
        // __LINE__: 186
        UTIL_ASSERT_MSG(FALSE, "Tiled object resref not found");

        m_nWedIndex = -1;
    }

    m_wAIState = wInitialState;
    m_wRenderState = STATE_PRIMARY_TILE;

    if (m_nWedIndex != -1) {
        if (pTiledObject->nNumPrimaryPolys != 0xFFFF) {
            for (nPoly = 0; nPoly < pTiledObject->nNumPrimaryPolys; nPoly++) {
                pPoly = reinterpret_cast<WED_POLYLIST*>(reinterpret_cast<BYTE*>(m_pResWed->m_pHeader) + pTiledObject->nOffsetToPrimaryPolys) + nPoly;
                pPoly->nType &= ~0x2;
            }
        }

        if (pTiledObject->nNumSecondaryPolys != 0xFFFF) {
            for (nPoly = 0; nPoly < pTiledObject->nNumSecondaryPolys; nPoly++) {
                pPoly = reinterpret_cast<WED_POLYLIST*>(reinterpret_cast<BYTE*>(m_pResWed->m_pHeader) + pTiledObject->nOffsetToSecondaryPolys) + nPoly;
                pPoly->nType |= 0x2;
            }
        }
    }
}

// 0x54E980
void CTiledObject::SetNewResWED(CResWED* pNewResWED)
{
    CResRef cResRef;
    WED_TILEDOBJECT* pTiledObject;
    ULONG nTiledObjects;
    WORD nIndex;
    WORD nPoly;
    WED_POLYLIST* pPoly;

    m_pResWed = pNewResWED;

    pTiledObject = m_pResWed->GetTiledObjects();
    nTiledObjects = m_pResWed->GetNumTiledObjects();

    for (nIndex = 0; nIndex < nTiledObjects; nIndex++) {
        cResRef = pTiledObject->resID;
        if (m_resId == cResRef) {
            m_nWedIndex = nIndex;
            break;
        }
        pTiledObject++;
    }

    if (nIndex == nTiledObjects) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CTiledObject.cpp
        // __LINE__: 251
        UTIL_ASSERT_MSG(FALSE, "Tiled object resref not found");

        m_nWedIndex = -1;
    }

    m_wRenderState = STATE_PRIMARY_TILE;

    if (m_nWedIndex != -1) {
        if (pTiledObject->nNumPrimaryPolys != 0xFFFF) {
            for (nPoly = 0; nPoly < pTiledObject->nNumPrimaryPolys; nPoly++) {
                pPoly = reinterpret_cast<WED_POLYLIST*>(reinterpret_cast<BYTE*>(m_pResWed->m_pHeader) + pTiledObject->nOffsetToPrimaryPolys) + nPoly;
                pPoly->nType &= ~0x2;
            }
        }

        if (pTiledObject->nNumSecondaryPolys != 0xFFFF) {
            for (nPoly = 0; nPoly < pTiledObject->nNumSecondaryPolys; nPoly++) {
                pPoly = reinterpret_cast<WED_POLYLIST*>(reinterpret_cast<BYTE*>(m_pResWed->m_pHeader) + pTiledObject->nOffsetToSecondaryPolys) + nPoly;
                pPoly->nType |= 0x2;
            }
        }
    }
}
