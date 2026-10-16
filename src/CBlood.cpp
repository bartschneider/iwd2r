#include "CBlood.h"

#include "CChitin.h"
#include "CGameArea.h"
#include "CInfinity.h"
#include "CParticle.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CWarp.h"

// 0x84D5AC
const BYTE CBlood::NUM_COLORS = CBLOOD_NUM_COLORS;

// 0x84D5B0
const COLORREF CBlood::RGB_COLOR1 = RGB(109, 36, 0);

// 0x84D5B4
const COLORREF CBlood::RGB_COLOR2 = RGB(159, 42, 0);

// 0x84D5B8
const COLORREF CBlood::RGB_COLOR3 = RGB(180, 50, 15);

// 0x84D5BC
const USHORT CBlood::SQUIRT_HIGH = 0;

// 0x84D5BE
const USHORT CBlood::SQUIRT_MEDIUM = 1;

// 0x84D5C0
const USHORT CBlood::SQUIRT_LOW = 2;

// 0x84D5C2
const USHORT CBlood::GUSH_HIGH = 3;

// 0x84D5C4
const USHORT CBlood::GUSH_MEDIUM = 4;

// 0x84D5C6
const USHORT CBlood::GUSH_LOW = 5;

// 0x84D5C8
const USHORT CBlood::PULSATING_ARTERY = 6;

// 0x84D5CA
const USHORT CBlood::EXPLODING_DEATH = 7;

// 0x84D5CC
const SHORT CBlood::STREAM_LENGTH = 5;

// 0x84D5CE
const SHORT CBlood::SQUIRT_STREAMS = 20;

// 0x84D5D0
const SHORT CBlood::FULL_VELOCITY = 3000;

// 0x84D5D2
const SHORT CBlood::GUSH_HEIGHT_VELOCITY = 4000;

// 0x84D5D4
const SHORT CBlood::SQUIRT_HEIGHT_VELOCITY = 8000;

// 0x84D5D6
const SHORT CBlood::VELOCITY_INC = 100;

// 0x84D5D8
const SHORT CBlood::SPLASH_LENGTH = 2;

// 0x84D5DA
const SHORT CBlood::SQUIRT_LIFESPAN = 15;

// 0x84D5DC
const USHORT CBlood::NUM_EFFECTS = 8;

// 0x5195B0
CBlood::CBlood(CGameArea* pArea, LONG nX, LONG nY, LONG nZ, SHORT nDirection, USHORT nType, LONG nCharacterHeight)
{
    m_pArea = pArea;
    m_pos.x = nX;
    m_pos.y = nY;
    m_posZ = nZ;
    m_nCharHeight = nCharacterHeight;
    m_nDirection = nDirection;
    m_nTimeStamp = 0;
    m_aColors[0] = RGB_COLOR1;
    m_aColors[1] = RGB_COLOR2;
    m_aColors[2] = RGB_COLOR3;
    m_particleType = CParticle::CONNECT;
    m_bloodType = nType;
    m_bLeavePool = TRUE;
    CalcBoundingRect();
    GenerateParticles();
}

// 0x519680
CBlood::~CBlood()
{
    POSITION pos;

    pos = m_lstSplashParticles.GetHeadPosition();
    while (pos != NULL) {
        delete m_lstSplashParticles.GetNext(pos);
    }
    m_lstSplashParticles.RemoveAll();

    pos = m_particleList.GetHeadPosition();
    while (pos != NULL) {
        delete m_particleList.GetNext(pos);
    }
    m_particleList.RemoveAll();
}

// 0x519740
BOOLEAN CBlood::AsynchronousUpdate()
{
    POSITION pos;
    POSITION posCurrent;

    m_nTimeStamp++;

    if (m_lstSplashParticles.GetCount() != 0) {
        pos = m_lstSplashParticles.GetHeadPosition();
        while (pos != NULL) {
            posCurrent = pos;
            CParticle* pParticle = m_lstSplashParticles.GetNext(pos);
            if (pParticle->AsynchronousUpdate() == CParticle::DEAD) {
                m_lstSplashParticles.RemoveAt(posCurrent);
                delete pParticle;
            }
        }
    }

    if (m_particleList.GetCount() != 0) {
        pos = m_particleList.GetHeadPosition();
        while (pos != NULL) {
            posCurrent = pos;
            CParticle* pParticle = m_particleList.GetNext(pos);
            if (m_nTimeStamp < pParticle->m_nRenderTime) {
                break;
            }

            BYTE nResult = pParticle->AsynchronousUpdate();
            if (nResult != CParticle::AIRBORN) {
                if (nResult == CParticle::GROUNDED) {
                    if (!pParticle->m_bTag) {
                        m_lstSplashParticles.AddTail(new CParticle(*pParticle));

                        CParticle* pSplash = m_lstSplashParticles.GetTail();
                        pSplash->m_pos.x = pParticle->m_pos.x;
                        pSplash->m_pos.y = pParticle->m_pos.y;
                        pSplash->m_pos.z = 0;

                        INT nRandom = rand() % 4;
                        pSplash->m_vel.x = pParticle->m_vel.x;
                        pSplash->m_vel.y = (pParticle->m_vel.y * 3) >> 2;
                        pSplash->m_vel.z = (nRandom + 1) * CParticle::GRAVITY;
                        pSplash->m_nTailLength = SPLASH_LENGTH;
                    }
                } else if (nResult == CParticle::DEAD) {
                    m_particleList.RemoveAt(posCurrent);
                    delete pParticle;
                } else {
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\CBlood.cpp
                    // __LINE__: 333
                    UTIL_ASSERT(FALSE);
                }
            }
        }

        if (m_particleList.GetCount() != 0) {
            return TRUE;
        }
    }

    return m_lstSplashParticles.GetCount() != 0;
}

// 0x519920
void CBlood::Render(CVidMode* pVidMode, INT nSurface)
{
    CInfinity* pInfinity = &(m_pArea->m_cInfinity);
    COLORREF rgbGlobalLighting = pInfinity->GetGlobalLighting();
    DWORD dwRenderFlags = CInfinity::FXPREP_CLEARFILL | 0x1;

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    if (m_particleList.GetCount() == 0 && m_lstSplashParticles.GetCount() == 0) {
        return;
    }

    if (!rView.IntersectRect(rView, m_rBounding)) {
        return;
    }

    CRect rFX(0, 0, m_rBounding.right - m_rBounding.left, m_rBounding.bottom - m_rBounding.top);

    pInfinity->FXPrep(rFX, dwRenderFlags, -1, CPoint(0, 0), CPoint(0, 0));
    if (!pInfinity->FXLock(rFX, dwRenderFlags)) {
        return;
    }

    CVidInf* pVidInf = static_cast<CVidInf*>(pInfinity->pVidMode);

    POSITION pos = m_lstSplashParticles.GetHeadPosition();
    while (pos != NULL) {
        CParticle* pParticle = m_lstSplashParticles.GetNext(pos);
        COLORREF rgbColor = pParticle->m_rgbColor;
        pParticle->m_rgbColor = (((((rgbColor >> 16) & 0xFF) * ((rgbGlobalLighting >> 16) & 0xFF)) >> 8) & 0xFF) << 16
            | (((((rgbColor >> 8) & 0xFF) * ((rgbGlobalLighting >> 8) & 0xFF)) >> 8) & 0xFF) << 8
            | ((((rgbColor & 0xFF) * (rgbGlobalLighting & 0xFF)) >> 8) & 0xFF);
        pVidInf->FXRender(pParticle, m_rBounding, CParticle::DOTS, 5);
        pParticle->m_rgbColor = rgbColor;
    }

    pos = m_particleList.GetHeadPosition();
    while (pos != NULL) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CBlood.cpp
        // __LINE__: 419
        UTIL_ASSERT(!m_particleList.IsEmpty());

        CParticle* pParticle = m_particleList.GetNext(pos);
        if (m_nTimeStamp < pParticle->m_nRenderTime) {
            break;
        }

        COLORREF rgbColor = pParticle->m_rgbColor;
        pParticle->m_rgbColor = (((((rgbColor >> 16) & 0xFF) * ((rgbGlobalLighting >> 16) & 0xFF)) >> 8) & 0xFF) << 16
            | (((((rgbGlobalLighting >> 8) & 0xFF) * ((rgbColor >> 8) & 0xFF)) >> 8) & 0xFF) << 8
            | (((rgbGlobalLighting & 0xFF) * (rgbColor & 0xFF)) >> 8);

        USHORT nBlobSize;
        if (m_particleType == CParticle::BLOB) {
            nBlobSize = static_cast<USHORT>(rand() % 5 + 1);
        } else {
            nBlobSize = 5;
        }

        pVidInf->FXRender(pParticle, m_rBounding, m_particleType, nBlobSize);
        pParticle->m_rgbColor = rgbColor;
    }

    CRect rClip(m_rBounding.left,
        m_rBounding.top + m_posZ,
        m_rBounding.right,
        m_rBounding.bottom + m_posZ);
    pInfinity->FXRenderClippingPolys(m_pos.x, m_pos.y, -m_posZ, CPoint(0, 0), rClip, FALSE, dwRenderFlags);
    pInfinity->FXUnlock(dwRenderFlags, NULL, CPoint(0, 0));
    pInfinity->FXBltFrom(nSurface, rFX, m_refPoint.x, m_refPoint.y, 0, 0, dwRenderFlags);
}

// 0x519C40
void CBlood::CalcBoundingRect()
{
    LONG nTop;
    LONG nLeft;
    LONG nRight;
    LONG nBottom;

    if (m_bloodType == PULSATING_ARTERY) {
        switch (m_nDirection) {
        case 0:
            m_rBounding.SetRect(max(m_pos.x - 20, 0),
                max(m_pos.y - m_posZ - 40, 0),
                m_pos.x + 20,
                m_pos.y - m_posZ + 40);
            m_refPoint.x = m_pos.x - 20;
            break;
        case 1:
        case 2:
        case 3:
            m_rBounding.SetRect(max(m_pos.x - 65, 0),
                max(m_pos.y - m_posZ - 40, 0),
                m_pos.x + 10,
                m_pos.y - m_posZ + 40);
            m_refPoint.x = m_pos.x - 65;
            break;
        case 4:
            m_rBounding.SetRect(max(m_pos.x - 65, 0),
                max(m_pos.y - m_posZ - 40, 0),
                m_pos.x + 15,
                m_pos.y - m_posZ + 40);
            m_refPoint.x = m_pos.x - 65;
            break;
        case 5:
        case 6:
        case 7:
            m_rBounding.SetRect(max(m_pos.x - 65, 0),
                max(m_pos.y - m_posZ - 50, 0),
                m_pos.x + 20,
                m_pos.y - m_posZ + 40);
            m_refPoint.x = m_pos.x - 65;
            m_refPoint.y = m_pos.y - m_posZ - 50;
            return;
        case 8:
            m_rBounding.SetRect(max(m_pos.x - 20, 0),
                max(m_pos.y - m_posZ - 50, 0),
                m_pos.x + 20,
                m_pos.y - m_posZ + 20);
            m_refPoint.x = m_pos.x - 20;
            m_refPoint.y = m_pos.y - m_posZ - 50;
            return;
        case 9:
        case 10:
        case 11:
            m_rBounding.SetRect(max(m_pos.x - 10, 0),
                max(m_pos.y - m_posZ - 50, 0),
                m_pos.x + 60,
                m_pos.y - m_posZ + 40);
            m_refPoint.x = m_pos.x - 10;
            m_refPoint.y = m_pos.y - m_posZ - 50;
            return;
        case 12:
            m_rBounding.SetRect(max(m_pos.x - 10, 0),
                max(m_pos.y - m_posZ - 40, 0),
                m_pos.x + 70,
                m_pos.y - m_posZ + 40);
            m_refPoint.x = m_pos.x - 10;
            break;
        case 13:
        case 14:
        case 15:
            m_rBounding.SetRect(max(m_pos.x - 10, 0),
                max(m_pos.y - m_posZ - 40, 0),
                m_pos.x + 65,
                m_pos.y - m_posZ + 45);
            m_refPoint.x = m_pos.x - 10;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CBlood.cpp
            // __LINE__: 559
            UTIL_ASSERT(FALSE);
            return;
        }

        m_refPoint.y = m_pos.y - m_posZ - 40;
        return;
    }

    if (m_bloodType == EXPLODING_DEATH) {
        CVidMode* pVidMode = g_pChitin->pActiveEngine != NULL ? g_pChitin->pActiveEngine->pVidMode : NULL;

        CSize size;
        pVidMode->GetFXSize(size);
        size.cx--;
        size.cy--;

        LONG nY = m_pos.y - m_posZ;
        nTop = nY - (size.cy >> 1);
        nLeft = m_pos.x - (size.cx >> 1);
        m_rBounding.SetRect(max(nLeft, 0),
            max(nTop, 0),
            (size.cx >> 1) + m_pos.x,
            (size.cy >> 1) + nY);
        m_refPoint.x = m_pos.x - (size.cx >> 1);
        m_refPoint.y = m_pos.y - (size.cy >> 1) - m_posZ;
        return;
    }

    switch (m_nDirection) {
    case 0:
        nLeft = m_pos.x - 45;
        nTop = m_pos.y - m_posZ - 10;
        nRight = m_pos.x + 45;
        nBottom = m_pos.y - m_posZ + 80;
        break;
    case 1:
    case 2:
    case 3:
        nLeft = m_pos.x - 65;
        nTop = m_pos.y - m_posZ - 10;
        nRight = m_pos.x + 10;
        nBottom = m_pos.y - m_posZ + 80;
        break;
    case 4:
        m_rBounding.SetRect(max(m_pos.x - 65, 0),
            max(m_pos.y - m_posZ - 15, 0),
            m_pos.x + 15,
            m_pos.y - m_posZ + 70);
        m_refPoint.x = m_pos.x - 65;
        m_refPoint.y = m_pos.y - m_posZ - 15;
        return;
    case 5:
    case 6:
    case 7:
        m_rBounding.SetRect(max(m_pos.x - 65, 0),
            max(m_pos.y - m_posZ - 30, 0),
            m_pos.x + 20,
            m_pos.y - m_posZ + 60);
        m_refPoint.x = m_pos.x - 65;
        m_refPoint.y = m_pos.y - m_posZ - 30;
        return;
    case 8:
        m_rBounding.SetRect(max(m_pos.x - 45, 0),
            max(m_pos.y - m_posZ - 30, 0),
            m_pos.x + 45,
            m_pos.y - m_posZ + 50);
        m_refPoint.x = m_pos.x - 45;
        m_refPoint.y = m_pos.y - m_posZ - 30;
        return;
    case 9:
    case 10:
    case 11:
        m_rBounding.SetRect(max(m_pos.x - 10, 0),
            max(m_pos.y - m_posZ - 30, 0),
            m_pos.x + 60,
            m_pos.y - m_posZ + 50);
        m_refPoint.x = m_pos.x - 10;
        m_refPoint.y = m_pos.y - m_posZ - 30;
        return;
    case 12:
        nLeft = m_pos.x - 10;
        nTop = m_pos.y - m_posZ - 10;
        nRight = m_pos.x + 70;
        nBottom = m_pos.y - m_posZ + 70;
        break;
    case 13:
    case 14:
    case 15:
        nLeft = m_pos.x - 10;
        nTop = m_pos.y - m_posZ - 10;
        nRight = m_pos.x + 65;
        nBottom = m_pos.y - m_posZ + 75;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CBlood.cpp
        // __LINE__: 624
        UTIL_ASSERT(FALSE);
        return;
    }

    m_rBounding.SetRect(max(nLeft, 0), max(nTop, 0), nRight, nBottom);
    m_refPoint.x = nLeft;
    m_refPoint.y = nTop;
}

// 0x51A2A0
SHORT CBlood::GenerateExplodingBlood(LONG nX, LONG nY, LONG nZ)
{
    CParticle particle;

    LONG nMinZ = (m_nCharHeight << 11) / 10;
    particle.m_nRenderTime = m_nTimeStamp;
    LONG nRangeZ = (m_nCharHeight << 13) / 10 - nMinZ;
    USHORT nParticles = 0;
    m_particleType = CParticle::BLOB;
    particle.m_nTailLength = 1;

    INT nCount = 0;
    do {
        INT nRandom1 = rand();
        LONG nVelX = nRandom1 % 12000 - 6000;
        INT nRandom2 = rand();

        LONG nPosZ;
        if (nRangeZ == 0) {
            nPosZ = 0;
        } else {
            nPosZ = rand() % nRangeZ;
        }

        INT nRandom3 = rand();

        particle.m_pos.y = nY;
        particle.m_pos.x = nX;
        particle.m_pos.z = nPosZ + nMinZ;
        particle.m_rgbColor = m_aColors[rand() % 2];

        // NOTE: The first particle of each burst keeps the velocity the
        // previous burst left in the template.
        m_particleList.AddTail(new CParticle(particle));

        SHORT nStream = static_cast<SHORT>(nRandom3 % 20);
        INT nStreamCount = nStream;
        nParticles++;
        if (nStreamCount > 0) {
            LONG nVelY = (nRandom2 % 12000 - nRandom1 % 12000 + nVelX) * 3;
            nParticles += static_cast<USHORT>(nStreamCount);
            do {
                particle.m_rgbColor = m_aColors[rand() % 2];
                particle.m_vel.z = rand() % 8000;
                particle.m_vel.y = nVelY >> 2;
                particle.m_vel.x = nVelX;
                m_particleList.AddTail(new CParticle(particle));
                nVelX += 50;
                nVelY += 150;
                nStreamCount--;
            } while (nStreamCount != 0);
        }

        nCount += rand() % 5;
    } while (nCount < 30);

    return nParticles;
}

// 0x51A500
SHORT CBlood::GenerateParticles()
{
    CParticle particle;

    SHORT nTimeStamp = m_nTimeStamp;
    LONG nY = (m_pos.y << 12) / 3;
    LONG nX = m_pos.x << 10;
    LONG nZ = m_posZ << 10;
    SHORT nParticles = 0;
    BOOL bLeft = FALSE;
    BOOL bRight = FALSE;
    BOOL bCenter = FALSE;

    LONG nSpread = 0;
    LONG nLength = 0;
    LONG nHeight = 0;
    LONG nStreams = 0;

    switch (m_bloodType) {
    case 0:
        bRight = TRUE;
        bCenter = TRUE;
        bLeft = TRUE;
        m_particleType = CParticle::DOTS;
        particle.m_nTailLength = 1;
        particle.m_nLifeSpan = SQUIRT_LIFESPAN;
        nLength = STREAM_LENGTH;
        nHeight = SQUIRT_HEIGHT_VELOCITY;
        nStreams = SQUIRT_STREAMS;
        m_bLeavePool = FALSE;
        nSpread = 1500;
        break;
    case 1:
        bCenter = TRUE;
        m_particleType = CParticle::DOTS;
        particle.m_nTailLength = 1;
        particle.m_nLifeSpan = SQUIRT_LIFESPAN;
        nLength = STREAM_LENGTH;
        nHeight = SQUIRT_HEIGHT_VELOCITY;
        nStreams = 10;
        m_bLeavePool = FALSE;
        nSpread = 1500;
        break;
    case 2:
        m_particleType = CParticle::DOTS;
        bCenter = TRUE;
        particle.m_nTailLength = 1;
        particle.m_nLifeSpan = SQUIRT_LIFESPAN;
        nLength = STREAM_LENGTH;
        nHeight = SQUIRT_HEIGHT_VELOCITY;
        nStreams = 5;
        m_bLeavePool = FALSE;
        nSpread = 1500;
        break;
    case 3:
        m_particleType = CParticle::CONNECT;
        bCenter = TRUE;
        particle.m_nTailLength = 5;
        nLength = STREAM_LENGTH;
        nHeight = GUSH_HEIGHT_VELOCITY;
        nStreams = SQUIRT_STREAMS;
        nSpread = FULL_VELOCITY;
        break;
    case 4:
        m_particleType = CParticle::CONNECT;
        bCenter = TRUE;
        particle.m_nTailLength = 5;
        nLength = STREAM_LENGTH;
        nHeight = GUSH_HEIGHT_VELOCITY;
        nStreams = 10;
        nSpread = FULL_VELOCITY;
        break;
    case 5:
        m_particleType = CParticle::CONNECT;
        nStreams = 5;
        nSpread = 1500;
        bCenter = TRUE;
        particle.m_nTailLength = 5;
        nHeight = GUSH_HEIGHT_VELOCITY;
        nLength = nStreams;
        break;
    case 6:
        nSpread = 750;
        m_particleType = CParticle::CONNECT;
        particle.m_pos.z = 0;
        particle.m_nTailLength = 4;
        nStreams = 6;
        particle.m_pos.x = nX;
        particle.m_pos.y = nY;
        nHeight = GUSH_HEIGHT_VELOCITY;
        nLength = nStreams;
        break;
    case 7:
        return GenerateExplodingBlood(nX, nY, nZ);
    }

    LONG nStartX = 0;
    LONG nStepX = 0;
    LONG nStartY = 0;
    LONG nStepY = 0;
    LONG nRightX = 0;
    LONG nRightY = 0;
    LONG nLeftY = 0;
    LONG nLeftX = 0;

    switch (m_nDirection) {
    case 0:
        nRightX = -nSpread;
        nStepX = 100;
        nStartX = -1000;
        nStartY = nSpread;
        nStepY = 0;
        nRightY = nSpread;
        nLeftY = nSpread;
        nLeftX = nSpread;
        break;
    case 1:
    case 2:
    case 3:
        nStartX = -1000 - nSpread;
        nRightX = -nSpread;
        nStepX = 100;
        nLeftX = 0;
        nRightY = 0;
        nStartY = nSpread - 1000;
        nStepY = 100;
        nLeftY = nSpread;
        break;
    case 4:
        nStartX = -nSpread;
        nStepX = 0;
        nStartY = -1000;
        nStepY = 100;
        nRightY = nStartX;
        nRightX = nStartX;
        nLeftY = nSpread;
        nLeftX = nStartX;
        break;
    case 5:
    case 6:
    case 7:
        nSpread = -nSpread;
        nStartX = nSpread + 1000;
        nStepX = -100;
        nLeftY = 0;
        nRightX = 0;
        nStartY = nSpread - 1000;
        nStepY = 100;
        nRightY = nSpread;
        nLeftX = nSpread;
        break;
    case 8:
        nStartY = -nSpread;
        nStepX = 100;
        nStartX = -1000;
        nStepY = 0;
        nRightY = nStartY;
        nRightX = nSpread;
        nLeftY = nStartY;
        nLeftX = nStartY;
        break;
    case 9:
    case 10:
    case 11:
        nStartX = nSpread - 1000;
        nStepX = 100;
        nLeftX = 0;
        nRightY = 0;
        nStartY = -1000 - nSpread;
        nStepY = 100;
        nRightX = nSpread;
        nLeftY = -nSpread;
        break;
    case 12:
        nStepX = 0;
        nStartY = -1000;
        nStepY = 100;
        nStartX = nSpread;
        nRightY = nSpread;
        nRightX = nSpread;
        nLeftY = -nSpread;
        nLeftX = nSpread;
        break;
    case 13:
    case 14:
    case 15:
        nStartX = nSpread + 1000;
        nStepX = -100;
        nLeftY = 0;
        nRightX = 0;
        nStartY = nSpread - 1000;
        nStepY = 100;
        nRightY = nSpread;
        nLeftX = nSpread;
        break;
    }

    LONG nSteps = static_cast<SHORT>(nLength);
    LONG nHeightDec = nHeight / nSteps;
    LONG nStartXDec = nStartX / nSteps;
    LONG nStartYDec = nStartY / nSteps;
    LONG nStepYDec = nStepY / nSteps;
    LONG nStepXDec = nStepX / nSteps;
    LONG nStreamsDec = nStreams / nSteps;

    if (m_bloodType == PULSATING_ARTERY) {
        if (static_cast<SHORT>(nLength) > 0) {
            LONG nVelZ = nHeight - 500;
            LONG nCount = nSteps;
            nParticles = 0;
            do {
                particle.m_nRenderTime = particle.m_nTailLength * nTimeStamp;

                LONG nVelX = nStartX;
                LONG nVelY = nStartY;
                for (SHORT nStream = 0; nStream < nStreams; nStream++) {
                    particle.m_rgbColor = m_aColors[rand() % 2];

                    INT nRandomZ = rand();
                    INT nRandomY = rand();
                    INT nRandomX = rand();
                    particle.m_vel.x = nRandomX % 500 - 500 + nVelX;
                    particle.m_vel.y = ((nRandomY % 500 - 500 + nVelY) * 3) >> 2;
                    particle.m_vel.z = nVelZ + nRandomZ % 500;
                    m_particleList.AddTail(new CParticle(particle));

                    nVelX += nStepX;
                    nParticles++;
                    nVelY += nStepY;
                }

                nVelZ -= nHeightDec;
                nStartX -= nStartXDec;
                nStreams -= nStreamsDec;
                nStepX -= nStepXDec;
                nStepY -= nStepYDec;
                nTimeStamp += 10;
                nStartY -= nStartYDec;
                nCount--;
            } while (nCount != 0);
        }
    } else if (static_cast<SHORT>(nLength) > 0) {
        LONG nStartY3 = nStartY * 3;
        LONG nStepY3 = nStepY * 3;
        nParticles = 0;
        do {
            particle.m_pos.y = nY;
            particle.m_nRenderTime = particle.m_nTailLength * nTimeStamp;
            particle.m_pos.x = nX;
            particle.m_pos.z = nZ;

            if (bLeft) {
                particle.m_rgbColor = m_aColors[rand() % 2];
                if (nHeight == 0) {
                    particle.m_vel.z = 0;
                } else {
                    particle.m_vel.z = rand() % nHeight;
                }
                particle.m_vel.x = nLeftX;
                particle.m_vel.y = (nLeftY * 3) >> 2;
                m_particleList.AddTail(new CParticle(particle));
                nParticles++;
            }

            if (bCenter && nStreams > 0) {
                LONG nVelX = nStartX;
                LONG nVelY3 = nStartY3;
                for (SHORT nStream = 0; nStream < nStreams; nStream++) {
                    particle.m_rgbColor = m_aColors[rand() % 2];
                    if (nHeight == 0) {
                        particle.m_vel.z = 0;
                    } else {
                        particle.m_vel.z = rand() % nHeight;
                    }
                    particle.m_vel.y = nVelY3 >> 2;
                    particle.m_vel.x = nVelX;
                    m_particleList.AddTail(new CParticle(particle));
                    nParticles++;
                    nVelX += nStepX;
                    nVelY3 += nStepY3;
                }
            }

            if (bRight) {
                particle.m_rgbColor = m_aColors[rand() % 2];
                if (nHeight == 0) {
                    particle.m_vel.z = 0;
                } else {
                    particle.m_vel.z = rand() % nHeight;
                }
                particle.m_vel.x = nRightX;
                particle.m_vel.y = (nRightY * 3) >> 2;
                m_particleList.AddTail(new CParticle(particle));
                nParticles++;
            }

            nStartX -= nStartXDec;
            nHeight -= nHeightDec;
            nStartY3 -= nStartYDec * 3;
            nStreams -= nStreamsDec;
            nStepX -= nStepXDec;
            nStepY3 -= nStepYDec * 3;
            nTimeStamp++;
            nSteps--;
        } while (nSteps != 0);
    }

    return nParticles;
}
