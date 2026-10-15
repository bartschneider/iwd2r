#include "CVidPoly.h"

#include "CChitin.h"
#include "CUtil.h"
#include "CVidMode.h"
#include "CVideo3d.h"

#include <cstdlib>
#include <cstring>

// 0x85EAA4
const BYTE CVidPoly::m_aDitherMask[] = {
    // clang-format off
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
    // clang-format on
};

// 0x7C0DB0
CVidPoly::CVidPoly()
{
    field_0 = 0;
    m_pVertices = NULL;
    m_pActiveEdges = NULL;
    m_pEdgeTable = NULL;
    m_nVertices = -1;
    m_pDrawHLineFunction = &CVidPoly::DrawHLine16;
}

// 0x7C0DD0
LONG CVidPoly::CalculateLineVIntersection(const CPoint& lineStart, const CPoint& lineEnd, LONG vertical, const CPoint& linePrev)
{
    if (lineStart.x == vertical) {
        if ((lineEnd.x <= vertical || linePrev.x <= vertical)
            && (lineEnd.x >= vertical || linePrev.x >= vertical)) {
            return lineStart.y;
        }
    } else if (lineEnd.x != vertical) {
        if ((lineStart.x >= vertical || lineEnd.x >= vertical)
            && (lineStart.x <= vertical || lineEnd.x <= vertical)) {
            return lineStart.y + (vertical - lineStart.x) * (lineEnd.y - lineStart.y) / (lineEnd.x - lineStart.x);
        }
    }

    return -1;
}

// 0x7C0E40
BOOLEAN CVidPoly::IsPtInPoly(const CPoint* pPoly, SHORT nPoly, const CPoint& pt)
{
    SHORT numIntersectingEdges = 0;
    SHORT cnt;
    LONG interSectionY;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidPoly.cpp
    // __LINE__: 166
    UTIL_ASSERT(pPoly != NULL && nPoly > 0);

    if (nPoly == 1) {
        return pPoly[0] == pt;
    } else if (nPoly == 2) {
        // A two-point polygon is its one segment, its end standing in for
        // the previous point (0x7C0EA6).
        interSectionY = CalculateLineVIntersection(pPoly[0], pPoly[1], pt.x, pPoly[1]);
        return interSectionY == pt.y;
    }

    for (cnt = 0; cnt < nPoly; cnt++) {
        if (cnt == nPoly - 1) {
            interSectionY = CalculateLineVIntersection(pPoly[cnt], pPoly[0], pt.x, pPoly[cnt - 1]);
        } else if (cnt != 0) {
            interSectionY = CalculateLineVIntersection(pPoly[cnt], pPoly[cnt + 1], pt.x, pPoly[cnt - 1]);
        } else {
            interSectionY = CalculateLineVIntersection(pPoly[0], pPoly[cnt + 1], pt.x, pPoly[nPoly - 1]);
        }

        if (interSectionY == pt.y) {
            return TRUE;
        }

        if (interSectionY != -1 && interSectionY < pt.y) {
            numIntersectingEdges++;
        }
    }

    return (numIntersectingEdges & 1);
}

// 0x7C0F40
BOOL CVidPoly::FillConvexPoly(WORD* pSurface, LONG lPitch, const CRect& rClipRect, DWORD dwColor, DWORD dwFlags, const CPoint& ptRef)
{
    SHORT nRightXDir;
    SHORT nLeftXDir;
    UINT nMinYIndex;
    UINT nMinYPt;
    UINT nMaxYPt;
    UINT nRightX;
    UINT nLeftX;
    UINT nRightIndex;
    UINT nLeftIndex;
    INT nRightDx;
    INT nRightDy;
    // NOTE: Initialised only to quiet -Wmaybe-uninitialized: with a flat
    // top the original leaves these unset and the first row sets them.
    UINT nRightAdjUp = 0;
    UINT nRightRun = 0;
    INT nRightErrTerm = 0;
    INT nLeftDx;
    INT nLeftDy;
    UINT nLeftAdjUp = 0;
    UINT nLeftRun = 0;
    INT nLeftErrTerm = 0;
    SHORT nIndex;
    CRect rClip;
    CPoint ptRefAdjusted;

    ptRefAdjusted.x = ptRef.x;
    ptRefAdjusted.y = ptRef.y;

    rClip.left = rClipRect.left;
    rClip.top = rClipRect.top;
    rClip.right = rClipRect.right - 1;
    rClip.bottom = rClipRect.bottom - 1;

    nLeftXDir = 1;
    nRightXDir = 1;

    if (m_nVertices < 3) {
        return FALSE;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidPoly.cpp
    // __LINE__: 267
    UTIL_ASSERT_MSG(m_nVertices < CVPOLY_MAX_VERTICIES, "Excessive poly vertex count");

    SetHLineFunction(dwFlags);

    nMinYIndex = 0;
    nMinYPt = m_pVertices[nMinYIndex].y;
    nMaxYPt = m_pVertices[nMinYIndex].y;
    ;
    for (nIndex = 1; nIndex < m_nVertices; nIndex++) {
        if (m_pVertices[nIndex].y < nMinYPt) {
            nMinYPt = m_pVertices[nIndex].y;
            nMinYIndex = nIndex;
        } else if (m_pVertices[nIndex].y > nMaxYPt) {
            nMaxYPt = m_pVertices[nIndex].y;
        }
    }

    nRightX = m_pVertices[nMinYIndex].x;
    nLeftX = m_pVertices[nMinYIndex].x;
    nRightIndex = (nMinYIndex + 1) % m_nVertices;
    nLeftIndex = (nMinYIndex + m_nVertices - 1) % m_nVertices;

    nRightDx = m_pVertices[nRightIndex].x - nRightX;
    if (nRightDx < 0) {
        nRightXDir = -1;
        nRightDx = -nRightDx;
    }
    nRightDy = m_pVertices[nRightIndex].y - nMinYPt;

    // NOTE: With nRightDy <= 0 (a flat top) Run, AdjUp and ErrTerm stay
    // unset: the loop's first step recomputes them (0x7C1058).
    if (nRightDy > 0) {
        nRightRun = nRightDx / nRightDy;
        nRightAdjUp = nRightDx % nRightDy;
        nRightErrTerm = nRightAdjUp - nRightDy;
    }

    nLeftDx = m_pVertices[nLeftIndex].x - nLeftX;
    if (nLeftDx < 0) {
        nLeftXDir = -1;
        nLeftDx = -nLeftDx;
    }
    nLeftDy = m_pVertices[nLeftIndex].y - nMinYPt;

    if (nLeftDy > 0) {
        nLeftRun = nLeftDx / nLeftDy;
        nLeftAdjUp = nLeftDx % nLeftDy;
        nLeftErrTerm = nLeftAdjUp - nLeftDy;
    }

    // NOTE: The row offsets are signed (32-bit int in the original): a
    // polygon starting above the clip moves the pointer back.
    if ((dwFlags & 0x8) != 0) {
        pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + lPitch * (rClip.bottom - static_cast<LONG>(nMinYPt)));
        lPitch = -lPitch;
    } else {
        pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + lPitch * (static_cast<LONG>(nMinYPt) - rClip.top));
    }

    ptRefAdjusted.y += static_cast<LONG>(nMinYPt) - rClip.top;

    while (nMinYPt <= nMaxYPt) {
        if (nMinYPt == m_pVertices[nRightIndex].y) {
            do {
                nRightX = m_pVertices[nRightIndex].x;
                nRightIndex = (nRightIndex + 1) % m_nVertices;
                nRightDy = m_pVertices[nRightIndex].y - nMinYPt;
            } while (nRightDy == 0);

            nRightDx = m_pVertices[nRightIndex].x - nRightX;
            if (nRightDx < 0) {
                nRightXDir = -1;
                nRightDx = -nRightDx;
            } else {
                nRightXDir = 1;
            }

            nRightRun = nRightDx / nRightDy;
            nRightAdjUp = nRightDx % nRightDy;
            nRightErrTerm = nRightAdjUp - nRightDy;
        }

        if (nMinYPt >= m_pVertices[nLeftIndex].y) {
            do {
                nLeftX = m_pVertices[nLeftIndex].x;
                nLeftIndex = (nLeftIndex + m_nVertices - 1) % m_nVertices;
                nLeftDy = m_pVertices[nLeftIndex].y - nMinYPt;
            } while (nLeftDy == 0);

            nLeftDx = m_pVertices[nLeftIndex].x - nLeftX;
            if (nLeftDx < 0) {
                nLeftXDir = -1;
                nLeftDx = -nLeftDx;
            } else {
                nLeftXDir = 1;
            }

            nLeftRun = nLeftDx / nLeftDy;
            nLeftAdjUp = nLeftDx % nLeftDy;
            nLeftErrTerm = nLeftAdjUp - nLeftDy;
        }

        // NOTE: Signed compare.
        if (static_cast<LONG>(nMinYPt) >= rClip.top && static_cast<LONG>(nMinYPt) <= rClip.bottom) {
            INT xMin;
            INT xMax;
            if (nLeftX > nRightX) {
                xMax = min(static_cast<LONG>(nLeftX), rClip.right) - rClip.left;
                xMin = max(static_cast<LONG>(nRightX) - rClip.left, 0);
            } else {
                xMax = min(static_cast<LONG>(nRightX), rClip.right) - rClip.left;
                xMin = max(static_cast<LONG>(nLeftX) - rClip.left, 0);
            }

            (this->*m_pDrawHLineFunction)(pSurface,
                xMin,
                xMax,
                dwColor,
                rClip,
                ptRefAdjusted);
        }

        nLeftX += nLeftRun * nLeftXDir;
        nLeftErrTerm += nLeftAdjUp;
        if (nLeftErrTerm > 0) {
            nLeftX += nLeftXDir;
            nLeftErrTerm -= nLeftDy;
        }

        nRightX += nRightRun * nRightXDir;
        nRightErrTerm += nRightAdjUp;
        if (nRightErrTerm > 0) {
            nRightX += nRightXDir;
            nRightErrTerm -= nRightDy;
        }

        nMinYPt++;
        ptRefAdjusted.y++;
        pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + lPitch);
    }

    return TRUE;
}

namespace {

// The edges FillPoly builds and the cache copies in and out.
// 0xA07B20
CVIDPOLY_EDGE g_aPolyEdges[CVPOLY_MAX_VERTICIES];

// A cached edge table: the edges of one vertex list, keyed by the list's
// address and vertex count.
class CVidPolyEdgeCacheEntry {
public:
    CVidPolyEdgeCacheEntry()
    {
        m_nVertices = 0;
        m_pEdgeTable = NULL;
        m_pEdges = NULL;
        m_pVertices = NULL;
    }

    // 0x7C1D40
    virtual ~CVidPolyEdgeCacheEntry()
    {
        if (m_pEdges != NULL) {
            delete[] m_pEdges;
        }
    }

    /* 0004 */ INT m_nVertices;
    /* 0008 */ CVIDPOLY_EDGE* m_pEdges;
    /* 000C */ CVIDPOLY_EDGE* m_pEdgeTable;
    /* 0010 */ CVIDPOLY_VERTEX* m_pVertices;
};

// FillPoly's cache of the last 16 edge tables it built.
class CVidPolyEdgeCache : public CPtrList {
public:
    // 0x7C1D70
    CVidPolyEdgeCache()
        : CPtrList(10)
    {
    }

    // 0x7C0CF0
    ~CVidPolyEdgeCache()
    {
        m_critSect.Lock();

        POSITION pos = GetHeadPosition();
        while (pos != NULL) {
            CVidPolyEdgeCacheEntry* pEntry = static_cast<CVidPolyEdgeCacheEntry*>(GetNext(pos));
            if (pEntry != NULL) {
                delete pEntry;
            }
        }

        RemoveAll();
        m_critSect.Unlock();
    }

    BOOL Find(CVidPoly* pPoly);
    void Add(CVidPoly* pPoly);

    /* 001C */ CCriticalSection m_critSect;
};

// 0x7C1DE0
BOOL CVidPolyEdgeCache::Find(CVidPoly* pPoly)
{
    m_critSect.Lock();

    POSITION pos = GetHeadPosition();
    while (pos != NULL) {
        POSITION posEntry = pos;
        CVidPolyEdgeCacheEntry* pEntry = static_cast<CVidPolyEdgeCacheEntry*>(GetNext(pos));
        if (pEntry != NULL
            && pPoly->m_pVertices == pEntry->m_pVertices
            && pPoly->m_nVertices == pEntry->m_nVertices) {
            INT nVertices = pEntry->m_nVertices;
            CVIDPOLY_VERTEX* pVertices = pEntry->m_pVertices;
            if (pEntry->m_pEdges != NULL && pEntry->m_pEdgeTable != NULL && pVertices != NULL) {
                pPoly->m_pEdgeTable = pEntry->m_pEdgeTable;
                memcpy(g_aPolyEdges, pEntry->m_pEdges, sizeof(CVIDPOLY_EDGE) * nVertices);
                pPoly->m_nVertices = nVertices;
                pPoly->m_pVertices = pVertices;
            }

            RemoveAt(posEntry);
            AddHead(pEntry);

            m_critSect.Unlock();
            return TRUE;
        }
    }

    m_critSect.Unlock();
    return FALSE;
}

// 0x7C1EC0
void CVidPolyEdgeCache::Add(CVidPoly* pPoly)
{
    m_critSect.Lock();

    // NOTE: The original evicts from the tail, where it also adds, so the
    // newest entry is the first to go once 16 are held.
    while (GetCount() > 15) {
        CVidPolyEdgeCacheEntry* pEntry = static_cast<CVidPolyEdgeCacheEntry*>(RemoveTail());
        if (pEntry != NULL) {
            delete pEntry;
        }
    }

    if (pPoly != NULL) {
        CVidPolyEdgeCacheEntry* pEntry = new CVidPolyEdgeCacheEntry();
        if (pPoly->m_nVertices != 0) {
            pEntry->m_pEdges = new CVIDPOLY_EDGE[pPoly->m_nVertices];
            if (pEntry->m_pEdges != NULL) {
                memcpy(pEntry->m_pEdges, g_aPolyEdges, sizeof(CVIDPOLY_EDGE) * pPoly->m_nVertices);
                pEntry->m_pEdgeTable = pPoly->m_pEdgeTable;
                pEntry->m_nVertices = pPoly->m_nVertices;
                pEntry->m_pVertices = pPoly->m_pVertices;
            }
        }
        AddTail(pEntry);
    }

    m_critSect.Unlock();
}

// 0xA09F20
CVidPolyEdgeCache g_cPolyEdgeCache;

} // namespace

// 0x7C13A0
BOOL CVidPoly::FillPoly(WORD* pSurface, LONG lPitch, const CRect& rClipRect, DWORD dwColor, DWORD dwFlags, const CPoint& ptRef)
{
    CPoint ptRefAdjusted;
    CRect rClip;
    LONG lStep;
    SHORT nY;
    INT nRows;

    ptRefAdjusted.x = ptRef.x;
    ptRefAdjusted.y = ptRef.y;

    rClip.left = rClipRect.left;
    rClip.top = rClipRect.top;
    rClip.right = rClipRect.right - 1;
    rClip.bottom = rClipRect.bottom - 1;

    lStep = lPitch;

    if (m_nVertices < 3) {
        return FALSE;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidPoly.cpp
    // __LINE__: 546
    UTIL_ASSERT_MSG(m_nVertices < CVPOLY_MAX_VERTICIES, "Excessive poly vertex count");

    if (m_nVertices >= CVPOLY_MAX_VERTICIES) {
        return FALSE;
    }

    SetHLineFunction(dwFlags);

    if ((dwFlags & 0x2) == 0) {
        if (!g_cPolyEdgeCache.Find(this)) {
            BuildEdgeTable(g_aPolyEdges);
            g_cPolyEdgeCache.Add(this);
        }
    } else {
        BuildEdgeTable(g_aPolyEdges);
    }

    m_pActiveEdges = NULL;

    // NOTE: With every edge horizontal the edge table is empty and the
    // original reads the first edge through a NULL pointer (0x7C147E);
    // bounded here by drawing nothing, which is what its loop would do.
    if (m_pEdgeTable == NULL) {
        return TRUE;
    }

    nY = static_cast<SHORT>(m_pEdgeTable->nY);

    if ((dwFlags & 0x8) != 0) {
        nRows = rClip.bottom - nY;
        lStep = -lPitch;
    } else {
        nRows = nY - rClip.top;
    }

    // NOTE: The row offset is signed (32-bit int in the original): a
    // polygon starting above the clip moves the pointer back.
    pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + nRows * lPitch);
    ptRefAdjusted.y += nY - rClip.top;

    while (m_pEdgeTable != NULL || m_pActiveEdges != NULL) {
        // Move the edges that start on this row to the active list, which
        // stays sorted by x.
        CVIDPOLY_EDGE* pNext = m_pActiveEdges;
        CVIDPOLY_EDGE** ppPrev = &m_pActiveEdges;
        while (m_pEdgeTable != NULL) {
            if (m_pEdgeTable->nY != nY) {
                break;
            }

            while (pNext != NULL && pNext->nX < static_cast<SHORT>(m_pEdgeTable->nX)) {
                ppPrev = &(pNext->pNext);
                pNext = pNext->pNext;
            }

            CVIDPOLY_EDGE* pEdge = m_pEdgeTable;
            *ppPrev = pEdge;
            m_pEdgeTable = m_pEdgeTable->pNext;
            pEdge->pNext = pNext;
            ppPrev = &(pEdge->pNext);
        }

        if (nY >= rClip.top && nY <= rClip.bottom) {
            CVIDPOLY_EDGE* pEdge = m_pActiveEdges;
            while (pEdge != NULL) {
                INT xMin;
                if (pEdge->nX > rClip.left) {
                    xMin = pEdge->nX - rClip.left;
                } else {
                    xMin = 0;
                }

                CVIDPOLY_EDGE* pPair = pEdge->pNext;
                INT xMax;
                if (pPair->nX < rClip.right) {
                    xMax = pPair->nX - rClip.left;
                } else {
                    xMax = rClip.right - rClip.left;
                }

                (this->*m_pDrawHLineFunction)(pSurface,
                    xMin,
                    xMax,
                    dwColor,
                    rClip,
                    ptRefAdjusted);

                pEdge = pPair->pNext;
            }
        }

        AdvanceActiveEdges();

        // Bubble sort the active list by x.
        if (m_pActiveEdges != NULL) {
            BOOL bSwapped;
            do {
                CVIDPOLY_EDGE** ppLink = &m_pActiveEdges;
                CVIDPOLY_EDGE* pEdge = m_pActiveEdges;
                bSwapped = FALSE;
                if (pEdge->pNext == NULL) {
                    break;
                }

                do {
                    CVIDPOLY_EDGE* pFollow = pEdge->pNext;
                    if (pFollow->nX < pEdge->nX) {
                        *ppLink = pFollow;
                        bSwapped = TRUE;
                        pEdge->pNext = pFollow->pNext;
                        pFollow->pNext = pEdge;
                    }
                    ppLink = &((*ppLink)->pNext);
                    pEdge = *ppLink;
                } while (pEdge->pNext != NULL);
            } while (bSwapped);
        }

        nY++;
        ptRefAdjusted.y++;
        pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + lStep);
    }

    return TRUE;
}

// 0x7C15F0
BOOL CVidPoly::FillConvexPoly3d(const CRect& rClip, DWORD dwColor, DWORD dwFlags, const CPoint& ptRef)
{
    if (m_nVertices < 3) {
        return FALSE;
    }

    CVideo3d::glColor4f(static_cast<float>(GetRValue(dwColor)) / 255.0f,
        static_cast<float>(GetRValue(dwColor)) / 255.0f,
        static_cast<float>(GetRValue(dwColor)) / 255.0f,
        static_cast<float>(dwColor >> 24) / 255.0f);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x2) != 0) {
        CVideo3d::glPolygonStipple(m_aDitherMask);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glEnable(GL_POLYGON_STIPPLE);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    CVideo3d::glDisable(GL_BLEND);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glDisable(GL_TEXTURE_2D);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glBegin(GL_POLYGON);
    for (INT nIndex = 0; nIndex < m_nVertices - 1; nIndex++) {
        INT nXFrom = m_pVertices[nIndex].x;
        INT nYFrom = m_pVertices[nIndex].y;
        INT nXTo = m_pVertices[nIndex + 1].x;
        INT nYTo = m_pVertices[nIndex + 1].y;
        if (CVidMode::ClipLine(nXFrom, nYFrom, nXTo, nYTo, rClip)) {
            CVideo3d::glVertex3f(static_cast<float>(nXFrom) + CVideo3d::SUB_PIXEL_SHIFT,
                static_cast<float>(nYFrom) + CVideo3d::SUB_PIXEL_SHIFT,
                0.0f);
            CVideo3d::glVertex3f(static_cast<float>(nXTo) + CVideo3d::SUB_PIXEL_SHIFT,
                static_cast<float>(nYTo) + CVideo3d::SUB_PIXEL_SHIFT,
                0.0f);
        }
    }

    CVideo3d::glEnd();
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x2) != 0) {
        CVideo3d::glDisable(GL_POLYGON_STIPPLE);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    return TRUE;
}

// 0x7C18B0
void CVidPoly::SetPoly(WORD* pVertices, WORD nVertices)
{
    m_pVertices = reinterpret_cast<CVIDPOLY_VERTEX*>(pVertices);
    m_nVertices = nVertices;
}

// 0x7C18D0
void CVidPoly::SetPoly(CVIDPOLY_VERTEX* pVertices, WORD nVertices)
{
    m_pVertices = pVertices;
    // The count is sign-extended (movsx at 0x7C18D0), unlike the WORD
    // overload's.
    m_nVertices = static_cast<SHORT>(nVertices);
}

// 0x7C18F0
void CVidPoly::AdvanceActiveEdges()
{
    CVIDPOLY_EDGE* pEdge = m_pActiveEdges;
    CVIDPOLY_EDGE* pPrev = NULL;

    while (pEdge != NULL) {
        // NOTE: While edges are still waiting in the edge table an edge
        // ends when its count reaches 0; after that it is drawn one row
        // more, to -1 (0x7C1903).
        BOOL bDone;
        if (m_pEdgeTable == NULL) {
            bDone = --pEdge->nCount == -1;
        } else {
            bDone = --pEdge->nCount == 0;
        }

        if (bDone) {
            if (pPrev == NULL) {
                m_pActiveEdges = pEdge->pNext;
            } else {
                pPrev->pNext = pEdge->pNext;
            }
        } else {
            pEdge->nX += pEdge->nXStep;
            pEdge->nErrTerm += pEdge->nAdjUp;
            pPrev = pEdge;
            if (pEdge->nErrTerm >= 0) {
                pEdge->nX += pEdge->nXDir;
                pEdge->nErrTerm -= pEdge->nAdjDown;
            }
        }

        pEdge = pEdge->pNext;
    }
}

// 0x7C1970
void CVidPoly::BuildEdgeTable(CVIDPOLY_EDGE* pEdges)
{
    m_pEdgeTable = NULL;

    for (INT nIndex = 0; nIndex < m_nVertices; nIndex++) {
        SHORT nNext = static_cast<SHORT>((nIndex + 1) % m_nVertices);
        UINT nTopX;
        UINT nTopY;
        UINT nBottomX;
        UINT nBottomY;

        if (m_pVertices[nIndex].y < m_pVertices[nNext].y) {
            nTopX = m_pVertices[nIndex].x;
            nTopY = m_pVertices[nIndex].y;
            nBottomX = m_pVertices[nNext].x;
            nBottomY = m_pVertices[nNext].y;
        } else {
            nTopX = m_pVertices[nNext].x;
            nTopY = m_pVertices[nNext].y;
            nBottomX = m_pVertices[nIndex].x;
            nBottomY = m_pVertices[nIndex].y;
        }

        INT nDy = nBottomY - nTopY;
        if (nDy != 0) {
            INT nDx = nBottomX - nTopX;
            pEdges->nErrTerm = 1 - nDy;
            pEdges->nXDir = nDx >= 0 ? 1 : -1;
            pEdges->pNext = NULL;
            pEdges->nX = nTopX;
            pEdges->nY = nTopY;
            pEdges->nCount = nDy;
            pEdges->nAdjDown = nDy;
            pEdges->nXStep = (abs(nDx) / nDy) * pEdges->nXDir;
            pEdges->nAdjUp = abs(nDx) % nDy;

            // The edge table is sorted by the top y, then x.
            CVIDPOLY_EDGE* pNext = m_pEdgeTable;
            CVIDPOLY_EDGE** ppPrev = &m_pEdgeTable;
            while (pNext != NULL
                && pNext->nY <= static_cast<INT>(nTopY)
                && (pNext->nY != static_cast<INT>(nTopY) || pNext->nX <= static_cast<INT>(nTopX))) {
                ppPrev = &(pNext->pNext);
                pNext = pNext->pNext;
            }

            *ppPrev = pEdges;
            pEdges->pNext = pNext;
            pEdges++;
        }
    }
}

// 0x7C1AA0
void CVidPoly::DrawHLine16(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned short* pSurface16 = reinterpret_cast<unsigned short*>(pSurface);

    // The span is written as pixel pairs of dwColor | dwColor << 16 (rep stosd
    // at 0x7C1ADD), after one odd pixel of dwColor's low word: a colour with
    // bits above 15 puts its high word ORed with its low word in the second
    // pixel of each pair.
    DWORD dwPair = dwColor | (dwColor << 16);

    if (xMax - xMin >= 0) {
        int width = xMax - xMin + 1;
        pSurface16 += xMin;

        if ((width & 1) != 0) {
            *pSurface16++ = static_cast<unsigned short>(dwPair);
            width--;
        }

        for (int nPairs = width / 2; nPairs > 0; nPairs--) {
            *pSurface16++ = static_cast<unsigned short>(dwPair);
            *pSurface16++ = static_cast<unsigned short>(dwPair >> 16);
        }
    }
}

// 0x7C1AF0
void CVidPoly::DrawHLineMirrored16(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned short* pSurface16 = reinterpret_cast<unsigned short*>(pSurface);

    // As DrawHLine16, walking left from the pixel at the surface's width -
    // xMin: one odd pixel, then pairs whose higher-addressed pixel gets the
    // pair's high word (0x7C1B3A).
    DWORD dwPair = dwColor | (dwColor << 16);

    if (xMax - xMin >= 0) {
        int width = xMax - xMin + 1;
        pSurface16 += rSurface.Width() - xMin;

        if ((width & 1) != 0) {
            *pSurface16-- = static_cast<unsigned short>(dwPair);
            width--;
        }

        for (int nPairs = width >> 1; nPairs > 0; nPairs--) {
            pSurface16[-1] = static_cast<unsigned short>(dwPair);
            pSurface16[0] = static_cast<unsigned short>(dwPair >> 16);
            pSurface16 -= 2;
        }
    }
}

// 0x7C1B50
void CVidPoly::DrawHLineDithered16(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned short* pSurface16 = reinterpret_cast<unsigned short*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface16 += xMin;

        if ((ptRef.y & 1) != 0) {
            if (((xMin + ptRef.x) & 1) != 0) {
                pSurface16++;
                width--;
            }
        } else {
            if (((xMin + ptRef.x) & 1) == 0) {
                pSurface16++;
                width--;
            }
        }

        width = (width + 1) / 2;
        for (int x = 0; x < width; x++) {
            *pSurface16 = static_cast<unsigned short>(dwColor);
            pSurface16 += 2;
        }
    }
}

// 0x7C1BB0
void CVidPoly::DrawHLineDitheredMirrored16(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned short* pSurface16 = reinterpret_cast<unsigned short*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface16 += rSurface.Width() - xMin;

        if ((ptRef.y & 1) != 0) {
            if (((rSurface.Width() + xMin + ptRef.x - 1) & 1) != 0) {
                pSurface16--;
                width--;
            }
        } else {
            if (((rSurface.Width() + xMin + ptRef.x - 1) & 1) == 0) {
                pSurface16--;
                width--;
            }
        }

        width = (width + 1) / 2;
        for (int x = 0; x < width; x++) {
            *pSurface16 = static_cast<unsigned short>(dwColor);
            pSurface16 -= 2;
        }
    }
}

// 0x7C1C20
void CVidPoly::SetHLineFunction(DWORD dwFlags)
{
    switch (g_pChitin->cVideo.m_nBpp) {
    case 32:
        if ((dwFlags & 0x1) != 0) {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineMirrored32;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLine32;
            }
        } else if ((dwFlags & 0x10) != 0) {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineShadedMirrored32;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineShaded32;
            }
        } else {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineDitheredMirrored32;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineDithered32;
            }
        }
        break;
    case 24:
        if ((dwFlags & 0x1) != 0) {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineMirrored24;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLine24;
            }
        } else if ((dwFlags & 0x10) != 0) {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineShadedMirrored24;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineShaded24;
            }
        } else {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineDitheredMirrored24;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineDithered24;
            }
        }
        break;
    default:
        if ((dwFlags & 0x1) != 0) {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineMirrored16;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLine16;
            }
        } else if ((dwFlags & 0x10) != 0) {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineShadedMirrored16;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineShaded16;
            }
        } else {
            if ((dwFlags & 0x4) != 0) {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineDitheredMirrored16;
            } else {
                m_pDrawHLineFunction = &CVidPoly::DrawHLineDithered16;
            }
        }
        break;
    }
}

// 0x7C1FD0
void CVidPoly::DrawHLineShaded16(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    // Each pixel becomes the colour scaled by the pixel's intensity / 4.
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    int width = xMax - xMin + 1;
    if (xMax - xMin >= 0 && width > 0) {
        WORD* pSurface16 = reinterpret_cast<WORD*>(pSurface) + xMin;

        do {
            DWORD nIntensity = pVidMode->GetIntensity16(*pSurface16);
            *pSurface16 = static_cast<WORD>(pVidMode->ScaleColor16(dwColor & 0xFFFF, nIntensity >> 2));
            pSurface16++;
            width--;
        } while (width != 0);
    }
}

// 0x7C2040
void CVidPoly::DrawHLineShadedMirrored16(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();

    int width = xMax - xMin + 1;
    if (xMax - xMin >= 0 && width > 0) {
        WORD* pSurface16 = reinterpret_cast<WORD*>(pSurface) + 1 + (rSurface.right - rSurface.left - xMin);

        do {
            pSurface16--;
            DWORD nIntensity = pVidMode->GetIntensity16(*pSurface16);
            width--;
            *pSurface16 = static_cast<WORD>(pVidMode->ScaleColor16(dwColor & 0xFFFF, nIntensity >> 2));
        } while (width != 0);
    }
}

// 0x7D6970
void CVidPoly::DrawHLine24(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned char* pSurface8 = reinterpret_cast<unsigned char*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface8 += xMin * 3;

        for (int x = 0; x < width; x++) {
            *pSurface8++ = GetRValue(dwColor);
            *pSurface8++ = GetGValue(dwColor);
            *pSurface8++ = GetBValue(dwColor);
        }
    }
}

// 0x7D69B0
void CVidPoly::DrawHLineMirrored24(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned char* pSurface8 = reinterpret_cast<unsigned char*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface8 += (rSurface.Width() - xMin) * 3;

        for (int x = 0; x < width; x++) {
            pSurface8[0] = GetRValue(dwColor);
            pSurface8[1] = GetGValue(dwColor);
            pSurface8[2] = GetBValue(dwColor);
            pSurface8 -= 3;
        }
    }
}

// 0x7D6A00
void CVidPoly::DrawHLineDithered24(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned char* pSurface8 = reinterpret_cast<unsigned char*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        // Three bytes a pixel (add at 0x7D6A26, lea at 0x7D6A2E).
        pSurface8 += xMin * 3;

        if ((ptRef.y & 1) != 0) {
            if (((xMin + ptRef.x) & 1) != 0) {
                pSurface8 += 3;
                width--;
            }
        } else {
            if (((xMin + ptRef.x) & 1) == 0) {
                pSurface8 += 3;
                width--;
            }
        }

        width = (width + 1) / 2;
        for (int x = 0; x < width; x++) {
            pSurface8[0] = GetRValue(dwColor);
            pSurface8[1] = GetGValue(dwColor);
            pSurface8[2] = GetBValue(dwColor);
            pSurface8 += 6;
        }
    }
}

// 0x7D6A70
void CVidPoly::DrawHLineDitheredMirrored24(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned char* pSurface8 = reinterpret_cast<unsigned char*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface8 += (rSurface.Width() - xMin) * 3;

        if ((ptRef.y & 1) != 0) {
            if (((rSurface.Width() + xMin + ptRef.x - 1) & 1) != 0) {
                pSurface8 -= 3;
                width--;
            }
        } else {
            if (((rSurface.Width() + xMin + ptRef.x - 1) & 1) == 0) {
                pSurface8 -= 3;
                width--;
            }
        }

        width = (width + 1) / 2;
        for (int x = 0; x < width; x++) {
            pSurface8[0] = GetRValue(dwColor);
            pSurface8[1] = GetGValue(dwColor);
            pSurface8[2] = GetBValue(dwColor);
            pSurface8 -= 6;
        }
    }
}

// 0x7D6AF0
void CVidPoly::DrawHLine32(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned int* pSurface32 = reinterpret_cast<unsigned int*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface32 += xMin;

        for (int x = 0; x < width; x++) {
            *pSurface32++ = dwColor;
        }
    }
}

// 0x7D6B20
void CVidPoly::DrawHLineMirrored32(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned int* pSurface32 = reinterpret_cast<unsigned int*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface32 += rSurface.Width() - xMin;

        for (int x = 0; x < width; x++) {
            *pSurface32-- = dwColor;
        }
    }
}

// 0x7D6B60
void CVidPoly::DrawHLineDithered32(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned int* pSurface32 = reinterpret_cast<unsigned int*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface32 += xMin;

        if (g_pChitin->cVideo.Is3dAccelerated()) {
            for (int x = 0; x < width; x++) {
                unsigned int rgb = *pSurface32;
                *pSurface32++ = (rgb & 0xFFFFFF) | ((rgb >> 1) & 0x7F000000);
            }
        } else {
            if ((ptRef.y & 1) != 0) {
                if (((xMin + ptRef.x) & 1) != 0) {
                    pSurface32++;
                    width--;
                }
            } else {
                if (((xMin + ptRef.x) & 1) == 0) {
                    pSurface32++;
                    width--;
                }
            }

            width = (width + 1) / 2;
            for (int x = 0; x < width; x++) {
                *pSurface32 = dwColor;
                pSurface32 += 2;
            }
        }
    }
}

// 0x7D6BF0
void CVidPoly::DrawHLineDitheredMirrored32(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    unsigned int* pSurface32 = reinterpret_cast<unsigned int*>(pSurface);

    int width = xMax - xMin + 1;
    if (width > 0) {
        pSurface32 += rSurface.Width() - xMin;

        if (g_pChitin->cVideo.Is3dAccelerated()) {
            for (int x = 0; x < width; x++) {
                unsigned int rgb = *pSurface32;
                *pSurface32-- = (rgb & 0xFFFFFF) | ((rgb >> 1) & 0x7F000000);
            }
        } else {
            if ((ptRef.y & 1) != 0) {
                if (((rSurface.Width() + xMin + ptRef.x - 1) & 1) != 0) {
                    pSurface32--;
                    width--;
                }
            } else {
                if (((rSurface.Width() + xMin + ptRef.x - 1) & 1) == 0) {
                    pSurface32--;
                    width--;
                }
            }

            width = (width + 1) / 2;
            for (int x = 0; x < width; x++) {
                *pSurface32 = dwColor;
                pSurface32 -= 2;
            }
        }
    }
}

// 0x7D6CA0
void CVidPoly::DrawHLineShaded24(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    DWORD nColor0 = dwColor & 0xFF;
    DWORD nColor1 = (dwColor >> 8) & 0xFF;
    DWORD nColor2 = (dwColor >> 16) & 0xFF;
    INT nIntensity = static_cast<INT>(nColor0 + nColor2 + nColor1) / 3;

    int width = xMax - xMin + 1;
    if (xMax - xMin >= 0 && width > 0) {
        BYTE* pSurface8 = reinterpret_cast<BYTE*>(pSurface) + xMin * 3;

        do {
            // NOTE: The original reads only the pixel's first byte and takes
            // it, it plus one and it plus two for the three channels
            // (0x7D6D1F to 0x7D6D2F).
            INT nValue = pSurface8[0];
            INT nSum = (nValue + 2) + (nValue + 1) + nValue;
            if (nSum >= 0x20 && (nSum >> 3) <= nIntensity) {
                INT nInverse = 255 - nIntensity;
                pSurface8[1] = static_cast<BYTE>((nColor1 * nIntensity + nInverse * (nValue + 1)) >> 8);
                pSurface8[0] = static_cast<BYTE>((nColor2 * nIntensity + nInverse * nValue) >> 8);
                pSurface8[2] = static_cast<BYTE>((nColor0 * nIntensity + nInverse * (nValue + 2)) >> 8);
            }
            pSurface8 += 3;
            width--;
        } while (width != 0);
    }
}

// 0x7D6DA0
void CVidPoly::DrawHLineShadedMirrored24(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    DWORD nColor0 = dwColor & 0xFF;
    DWORD nColor1 = (dwColor >> 8) & 0xFF;
    DWORD nColor2 = (dwColor >> 16) & 0xFF;
    INT nIntensity = static_cast<INT>(nColor0 + nColor2 + nColor1) / 3;

    int width = xMax - xMin + 1;
    if (xMax - xMin >= 0 && width > 0) {
        BYTE* pSurface8 = reinterpret_cast<BYTE*>(pSurface) + (rSurface.right - rSurface.left - xMin + 1) * 3;

        do {
            pSurface8 -= 3;
            // NOTE: As DrawHLineShaded24, the first byte stands for all three.
            INT nValue = pSurface8[0];
            INT nSum = (nValue + 2) + (nValue + 1) + nValue;
            if (nSum >= 0x20 && (nSum >> 3) <= nIntensity) {
                INT nInverse = 255 - nIntensity;
                pSurface8[1] = static_cast<BYTE>((nColor1 * nIntensity + nInverse * (nValue + 1)) >> 8);
                pSurface8[0] = static_cast<BYTE>((nColor2 * nIntensity + nInverse * nValue) >> 8);
                pSurface8[2] = static_cast<BYTE>((nColor0 * nIntensity + nInverse * (nValue + 2)) >> 8);
            }
            width--;
        } while (width != 0);
    }
}

// 0x7D6EB0
void CVidPoly::DrawHLineShaded32(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    DWORD nRed = (dwColor >> 16) & 0xFF;
    DWORD nGreen = (dwColor >> 8) & 0xFF;
    DWORD nBlue = dwColor & 0xFF;
    DWORD nIntensity = (nRed + nBlue + nGreen) / 3;

    int width = xMax - xMin + 1;
    if (xMax - xMin >= 0 && width > 0) {
        DWORD* pSurface32 = reinterpret_cast<DWORD*>(pSurface) + xMin;

        do {
            DWORD nPixel = *pSurface32;
            DWORD nPixelRed = (nPixel >> 16) & 0xFF;
            DWORD nPixelGreen = (nPixel >> 8) & 0xFF;
            DWORD nPixelBlue = nPixel & 0xFF;
            DWORD nSum = nPixelBlue + nPixelGreen + nPixelRed;
            if (nSum > 31 && static_cast<DWORD>(static_cast<INT>(nSum) >> 3) <= nIntensity) {
                INT nInverse = 255 - nIntensity;
                *pSurface32 = ((((nRed * nIntensity + nInverse * nPixelRed) >> 8) << 8
                                   | ((nGreen * nIntensity + nInverse * nPixelGreen) >> 8))
                                  << 8)
                    | ((nBlue * nIntensity + nInverse * nPixelBlue) >> 8);
            }
            pSurface32++;
            width--;
        } while (width != 0);
    }
}

// 0x7D6FD0
void CVidPoly::DrawHLineShadedMirrored32(void* pSurface, int xMin, int xMax, DWORD dwColor, const CRect& rSurface, const CPoint& ptRef)
{
    DWORD nRed = (dwColor >> 16) & 0xFF;
    DWORD nGreen = (dwColor >> 8) & 0xFF;
    DWORD nBlue = dwColor & 0xFF;
    DWORD nIntensity = (nBlue + nRed + nGreen) / 3;

    int width = xMax - xMin + 1;
    if (xMax - xMin >= 0 && width > 0) {
        DWORD* pSurface32 = reinterpret_cast<DWORD*>(pSurface) + 1 + (rSurface.right - rSurface.left - xMin);

        do {
            pSurface32--;
            DWORD nPixel = *pSurface32;
            DWORD nPixelRed = (nPixel >> 16) & 0xFF;
            DWORD nPixelGreen = (nPixel >> 8) & 0xFF;
            DWORD nPixelBlue = nPixel & 0xFF;
            DWORD nSum = nPixelBlue + nPixelGreen + nPixelRed;
            if (nSum > 31 && static_cast<DWORD>(static_cast<INT>(nSum) >> 3) <= nIntensity) {
                INT nInverse = 255 - nIntensity;
                *pSurface32 = ((((nRed * nIntensity + nInverse * nPixelRed) >> 8) << 8
                                   | ((nGreen * nIntensity + nInverse * nPixelGreen) >> 8))
                                  << 8)
                    | ((nBlue * nIntensity + nInverse * nPixelBlue) >> 8);
            }
            width--;
        } while (width != 0);
    }
}
