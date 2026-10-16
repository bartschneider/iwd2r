#include "CGameObject84D624.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CVidInf.h"
#include "CWarp.h"

#include <cstdio>

// 0x84D5EA
const WORD CGameObject84D624::COLORTYPE_ICE = 9;

// 0x84D5EC
const WORD CGameObject84D624::COLORTYPE_STONE = 10;

// 0x84D5F0
const WORD CGameObject84D624::EXPLOSION_PUFFS = 25;

// 0x84D600
const WORD CGameObject84D624::TYPE_SPARKLE = 1;

// 0x84D602
const WORD CGameObject84D624::TYPE_SHOWER = 2;

// 0x84D604
const WORD CGameObject84D624::TYPE_EXPLOSION = 4;

// 0x51AE40
CGameObject84D624::CGameObject84D624(WORD nColorType, WORD wType, WORD nPuffs)
{
    m_nPuff = 0;

    const C2DArray& tSparkleColors = g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_tSparkleColors;

    m_pVidMode = g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL;
    m_rBounding.SetRect(0, 0, 0, 0);
    m_wType = wType;
    m_nPuffs = nPuffs;
    m_nGravity = 50;

    // The row of SPRKLCLR.2DA each colour type reads (its row labels:
    // CSPARKLE_COLORTYPE_BLUE, GOLD, PURPLE, ICE, STONE, BLACK, CHROMATIC,
    // RED, GREEN, WHITE, MAGENTA and ORANGE).
    INT nRow;
    switch (nColorType) {
    case 1:
        nRow = 5;
        break;
    case 2:
        nRow = 0;
        break;
    case 3:
        nRow = 6;
        break;
    case 4:
        nRow = 1;
        break;
    case 5:
        nRow = 8;
        break;
    case 6:
        nRow = 2;
        break;
    case 7:
        nRow = 7;
        break;
    case 8:
        nRow = 9;
        break;
    case 9:
        nRow = 3;
        break;
    case 10:
        nRow = 4;
        break;
    case 11:
        nRow = 10;
        break;
    case 12:
        nRow = 11;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CParticle.cpp
        // __LINE__: 1975
        UTIL_ASSERT(FALSE);
        return;
    }

    // NOTE: The original's three components are uninitialized: a cell
    // sscanf cannot read (the table's default) keeps the previous colour's,
    // or garbage for the first. Bounded here (ruling P4-10): they start at
    // 0.
    UINT nRed = 0;
    UINT nGreen = 0;
    UINT nBlue = 0;
    for (INT nColumn = 0; nColumn < 5; nColumn++) {
        sscanf(tSparkleColors.GetAt(CPoint(nColumn, nRow)), "RGB(%u,%u,%u)", &nRed, &nGreen, &nBlue);
        m_rgbColors[nColumn] = RGB(nRed, nGreen, nBlue);
    }
}

// 0x51C330
CGameObject84D624::~CGameObject84D624()
{
    POSITION pos = m_particleList.GetHeadPosition();
    while (pos != NULL) {
        CParticle* pParticle = m_particleList.GetNext(pos);
        if (pParticle != NULL) {
            delete pParticle;
        }
    }
    m_particleList.RemoveAll();
}

// 0x51C3D0
void CGameObject84D624::AddToArea(CGameArea* pNewArea, const CPoint& pos, LONG posZ, BYTE listType)
{
    LONG nGravity = m_nGravity;

    m_nFadeHeights[0] = static_cast<BYTE>(posZ * 99 / 100);
    m_nFadeHeights[1] = static_cast<BYTE>(posZ * 85 / 100);
    m_nFadeHeights[2] = static_cast<BYTE>(posZ * 75 / 100);
    m_nFadeHeights[3] = static_cast<BYTE>(posZ * 55 / 100);

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CGameObject::AddToArea(pNewArea, pos, posZ, listType);

    if ((m_wType & TYPE_EXPLOSION) != 0) {
        GenerateExplosion();
        return;
    }

    LONG nX;
    LONG nY;
    if ((m_wType & TYPE_SHOWER) != 0) {
        nX = m_pos.x + 3;
        nY = m_pos.y - 5;
        m_nPuff = 0;
        m_rBounding.SetRect(max(m_pos.x - 13, 0),
            max(m_pos.y - posZ - 13, 0),
            m_pos.x + 13,
            m_pos.y + 13);
    } else {
        nX = m_pos.x - 5;
        nY = m_pos.y - 5;
        m_rBounding.SetRect(max(nX, 0),
            max(nY - posZ, 0),
            nX + 9,
            nY + 9);
    }

    LONG nPosZ = posZ << CParticle::RESOLUTION_INC;
    for (SHORT nParticle = 1; nParticle < 15; nParticle++) {
        INT nRandX = rand() % 10;
        INT nRandY = rand() % 10;
        LONG nParticleX = (nRandX + nX) << CParticle::RESOLUTION_INC;
        LONG nParticleY = (((nRandY + nY) * 4) / 3) << CParticle::RESOLUTION_INC;

        COLORREF rgbColor = (nRandX & 1) != 0 ? m_rgbColors[1] : m_rgbColors[0];
        rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyFadeAmount(rgbColor);
        rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyBrightnessContrast(rgbColor);

        m_particleList.AddTail(new CParticle(nParticleX,
            nParticleY,
            nPosZ,
            (nParticle & 1) != 0 ? 4 : -4,
            0,
            0,
            rgbColor,
            0,
            nGravity,
            CParticle::CONSTANT_VELOCITY));

        nGravity += 10;
    }
}

// 0x51C6E0
void CGameObject84D624::AIUpdate()
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CParticle.cpp
    // __LINE__: 2143
    UTIL_ASSERT(!m_particleList.IsEmpty());

    POSITION pos = m_particleList.GetHeadPosition();
    while (pos != NULL) {
        POSITION posCurrent = pos;
        CParticle* pParticle = m_particleList.GetNext(pos);

        // Below each of its heights a particle takes the next colour,
        // brightened by its height's low two bits.
        LONG nHeight = pParticle->m_pos.z >> CParticle::RESOLUTION_INC;
        UINT nScale = static_cast<BYTE>((nHeight & 3) + 1);
        COLORREF rgbFade = 0;
        BOOL bFade = TRUE;
        if (nHeight < m_nFadeHeights[3]) {
            rgbFade = m_rgbColors[4];
        } else if (nHeight < m_nFadeHeights[2]) {
            rgbFade = m_rgbColors[3];
        } else if (nHeight < m_nFadeHeights[1]) {
            rgbFade = m_rgbColors[2];
        } else if (nHeight < m_nFadeHeights[0]) {
            rgbFade = m_rgbColors[1];
        } else {
            bFade = FALSE;
        }

        if (bFade) {
            UINT nRed = GetRValue(rgbFade) * nScale;
            UINT nGreen = GetGValue(rgbFade) * nScale;
            UINT nBlue = GetBValue(rgbFade) * nScale;
            pParticle->m_rgbColor = RGB(nRed > 255 ? 255 : nRed,
                nGreen > 255 ? 255 : nGreen,
                nBlue > 255 ? 255 : nBlue);
        }

        if ((m_wType & TYPE_EXPLOSION) == 0 && (rand() & 4) != 0) {
            pParticle->m_vel.x = -pParticle->m_vel.x;
            pParticle->m_vel.y = (pParticle->m_vel.y * 3) >> 2;
        }

        LONG nVelZ = pParticle->m_vel.z - pParticle->m_nGravity;
        pParticle->m_pos.z = max(pParticle->m_pos.z + nVelZ, 0);

        if (pParticle->m_pos.z <= 0) {
            m_particleList.RemoveAt(posCurrent);
            if (pParticle != NULL) {
                delete pParticle;
            }

            if (m_particleList.GetCount() == 0) {
                RemoveFromArea();

                BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
                    CGameObjectArray::THREAD_ASYNCH,
                    NULL,
                    INFINITE);
                if (rc == CGameObjectArray::SUCCESS) {
                    delete this;
                }
                return;
            }
        } else {
            pParticle->m_pos.x += pParticle->m_vel.x;
            pParticle->m_pos.y += pParticle->m_vel.y;
            pParticle->m_vel.z = nVelZ;
        }
    }

    // A shower adds a puff each update, at eight places around its
    // position in turn, until it has made m_nPuffs.
    if ((m_wType & TYPE_SHOWER) == 0) {
        return;
    }

    if (m_nPuff >= m_nPuffs) {
        return;
    }

    // NOTE: The original's default case (unreachable) uses its locals
    // uninitialized.
    LONG nX = 0;
    LONG nY = 0;
    switch (m_nPuff++ & 7) {
    case 0:
        nX = m_pos.x + 3;
        nY = m_pos.y - 5;
        break;
    case 1:
        nX = m_pos.x + 5;
        nY = m_pos.y + 5;
        break;
    case 2:
        nX = m_pos.x - 5;
        nY = m_pos.y + 3;
        break;
    case 3:
        nX = m_pos.x - 15;
        nY = m_pos.y + 5;
        break;
    case 4:
        nX = m_pos.x - 13;
        nY = m_pos.y - 5;
        break;
    case 5:
        nX = m_pos.x - 15;
        nY = m_pos.y - 15;
        break;
    case 6:
        nX = m_pos.x - 5;
        nY = m_pos.y - 13;
        break;
    case 7:
        nX = m_pos.x + 5;
        nY = m_pos.y - 15;
        break;
    }

    LONG nPosZ = m_posZ << CParticle::RESOLUTION_INC;
    COLORREF rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyFadeAmount(m_rgbColors[0]);
    rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyBrightnessContrast(rgbColor);

    WORD nGravity = 50;
    for (SHORT nParticle = 1; nParticle < 15; nParticle++) {
        WORD nRandX = static_cast<WORD>(rand() % 10);
        WORD nRandY = static_cast<WORD>(rand() % 10);
        LONG nParticleX = (nRandX + nX) << CParticle::RESOLUTION_INC;
        LONG nParticleY = (((nRandY + nY) * 4) / 3) << CParticle::RESOLUTION_INC;

        m_particleList.AddTail(new CParticle(nParticleX,
            nParticleY,
            nPosZ,
            (nParticle & 1) != 0 ? 4 : -4,
            0,
            0,
            rgbColor,
            0,
            nGravity,
            CParticle::CONSTANT_VELOCITY));

        nGravity += 10;
    }
}

// 0x51CC00
void CGameObject84D624::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    (void)pArea;
    (void)pVidMode;

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    DWORD dwFlags = CInfinity::FXPREP_CLEARFILL | 0x1;

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    if (!rView.IntersectRect(rView, m_rBounding)) {
        return;
    }

    CRect rFX(0,
        0,
        m_rBounding.right - m_rBounding.left,
        m_rBounding.bottom - m_rBounding.top);
    pInfinity->FXPrep(rFX, dwFlags, -1, CPoint(0, 0), CPoint(0, 0));

    CPoint ptBlit;
    ptBlit.x = m_pos.x - ((m_rBounding.right - m_rBounding.left) >> 1);
    if (m_wType == TYPE_SHOWER) {
        ptBlit.y = m_pos.y - m_posZ - 8;
    } else if (m_wType == TYPE_EXPLOSION) {
        ptBlit.y = m_pos.y - m_posZ - 50;
    } else {
        ptBlit.y = m_pos.y - m_posZ;
    }

    if (!pInfinity->FXLock(rFX, dwFlags)) {
        return;
    }

    POSITION pos = m_particleList.GetHeadPosition();
    while (pos != NULL) {
        CParticle* pParticle = m_particleList.GetNext(pos);
        COLORREF rgbColor = pParticle->m_rgbColor;
        pParticle->m_rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyFadeAmount(rgbColor);
        pParticle->m_rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyBrightnessContrast(pParticle->m_rgbColor);
        static_cast<CVidInf*>(pInfinity->pVidMode)->FXRender(pParticle, m_rBounding, CParticle::DOTS, 5);
        pParticle->m_rgbColor = rgbColor;
    }

    pInfinity->FXRenderClippingPolys(m_pos.x, m_pos.y, 0, CPoint(0, 0), m_rBounding, FALSE, dwFlags);
    pInfinity->FXUnlock(dwFlags, NULL, CPoint(0, 0));
    pInfinity->FXBltFrom(a3, rFX, ptBlit.x, ptBlit.y, 0, 0, dwFlags);
}

// 0x51CE50
void CGameObject84D624::GenerateExplosion()
{
    // The direction from the position to the source, in eight steps.
    BYTE nDirection;
    if (m_ptSource.y >= m_pos.y) {
        if (m_ptSource.x >= m_pos.x) {
            LONG dx = m_ptSource.x - m_pos.x;
            LONG dy = m_ptSource.y - m_pos.y;
            nDirection = CGameSprite::DIR_E;
            if (dy * 27 > dx * 16) {
                nDirection = CGameSprite::DIR_SE;
                if (dy * 7 > dx * 38) {
                    nDirection = CGameSprite::DIR_S;
                }
            }
        } else {
            LONG dx = m_pos.x - m_ptSource.x;
            LONG dy = m_ptSource.y - m_pos.y;
            nDirection = CGameSprite::DIR_W;
            if (dy * 27 > dx * 16) {
                nDirection = CGameSprite::DIR_SW;
                if (dy * 7 > dx * 38) {
                    nDirection = CGameSprite::DIR_S;
                }
            }
        }
    } else {
        if (m_ptSource.x >= m_pos.x) {
            LONG dy = m_pos.y - m_ptSource.y;
            LONG dx = m_ptSource.x - m_pos.x;
            nDirection = CGameSprite::DIR_E;
            if (dy * 27 > dx * 16) {
                nDirection = CGameSprite::DIR_NE;
                if (dy * 7 > dx * 38) {
                    nDirection = CGameSprite::DIR_N;
                }
            }
        } else {
            LONG dy = m_pos.y - m_ptSource.y;
            LONG dx = m_pos.x - m_ptSource.x;
            nDirection = CGameSprite::DIR_W;
            if (dy * 27 > dx * 16) {
                nDirection = CGameSprite::DIR_NW;
                if (dy * 7 > dx * 38) {
                    nDirection = CGameSprite::DIR_N;
                }
            }
        }
    }

    LONG nTop = m_pos.y - m_posZ;
    m_rBounding.SetRect(max(m_pos.x - 50, 0),
        max(nTop - 50, 0),
        m_pos.x + 50,
        nTop + 50);

    // Three streams of particles fan out from the direction: one each
    // side of it and one along it.
    // NOTE: The original's default case (an odd direction, which the code
    // above never gives) uses its locals uninitialized.
    SHORT nVelX1 = 0;
    SHORT nVelY1 = 0;
    SHORT nVelX2 = 0;
    SHORT nVelY2 = 0;
    SHORT nVelX3 = 0;
    SHORT nVelY3 = 0;
    switch (nDirection) {
    case 0:
        nVelX1 = 700;
        nVelY1 = 700;
        nVelX2 = 0;
        nVelY2 = 1000;
        nVelX3 = -700;
        nVelY3 = 700;
        break;
    case 2:
        nVelX1 = 0;
        nVelY1 = 1000;
        nVelX2 = -700;
        nVelY2 = 700;
        nVelX3 = -1000;
        nVelY3 = 0;
        break;
    case 4:
        nVelX1 = -700;
        nVelY1 = 700;
        nVelX2 = -1000;
        nVelY2 = 0;
        nVelX3 = -700;
        nVelY3 = -700;
        break;
    case 6:
        nVelX1 = -1000;
        nVelY1 = 0;
        nVelX2 = -700;
        nVelY2 = -700;
        nVelX3 = 0;
        nVelY3 = -1000;
        break;
    case 8:
        nVelX1 = -700;
        nVelY1 = -700;
        nVelX2 = 0;
        nVelY2 = -1000;
        nVelX3 = 700;
        nVelY3 = -700;
        break;
    case 10:
        nVelX1 = 0;
        nVelY1 = -1000;
        nVelX2 = 700;
        nVelY2 = -700;
        nVelX3 = 1000;
        nVelY3 = 0;
        break;
    case 12:
        nVelX1 = 700;
        nVelY1 = -700;
        nVelX2 = 1000;
        nVelY2 = 0;
        nVelX3 = 700;
        nVelY3 = 700;
        break;
    case 14:
        nVelX1 = 1000;
        nVelY1 = 0;
        nVelX2 = 700;
        nVelY2 = 700;
        nVelX3 = 0;
        nVelY3 = 1000;
        break;
    }

    LONG nY = m_pos.y - 10;
    LONG nX = m_pos.x - 10;
    LONG nPosZ = m_posZ << CParticle::RESOLUTION_INC;
    COLORREF rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyFadeAmount(m_rgbColors[0]);
    rgbColor = (g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL)->ApplyBrightnessContrast(rgbColor);

    LONG nGravity = 50;
    for (INT nParticle = 14; nParticle != 0; nParticle--) {
        INT nRandX = (rand() % 10) * 2;
        INT nRandY = (rand() % 10) * 2;
        m_particleList.AddTail(new CParticle((nRandX + nX) << CParticle::RESOLUTION_INC,
            (((nRandY + nY) * 4) / 3) << CParticle::RESOLUTION_INC,
            nPosZ,
            nVelX1,
            nVelY1,
            1000,
            rgbColor,
            0,
            nGravity,
            CParticle::CONSTANT_VELOCITY));
        nGravity += 10;
    }

    nGravity = 50;
    for (INT nParticle = 14; nParticle != 0; nParticle--) {
        INT nRandX = (rand() % 10) * 2;
        INT nRandY = (rand() % 10) * 2;
        m_particleList.AddTail(new CParticle((nRandX + nX) << CParticle::RESOLUTION_INC,
            (((nRandY + nY) * 4) / 3) << CParticle::RESOLUTION_INC,
            nPosZ,
            nVelX2,
            nVelY2,
            1000,
            rgbColor,
            0,
            nGravity,
            CParticle::CONSTANT_VELOCITY));
        nGravity += 10;
    }

    nGravity = 50;
    for (INT nParticle = 14; nParticle != 0; nParticle--) {
        INT nRandX = (rand() % 10) * 2;
        INT nRandY = (rand() % 10) * 2;
        m_particleList.AddTail(new CParticle((nRandX + nX) << CParticle::RESOLUTION_INC,
            (((nRandY + nY) * 4) / 3) << CParticle::RESOLUTION_INC,
            nPosZ,
            nVelX3,
            nVelY3,
            1000,
            rgbColor,
            0,
            nGravity,
            CParticle::CONSTANT_VELOCITY));
        nGravity += 10;
    }
}
