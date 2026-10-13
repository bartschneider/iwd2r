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
    // TODO: Incomplete.
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
