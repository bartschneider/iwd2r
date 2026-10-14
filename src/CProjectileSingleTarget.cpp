#include "CProjectileSingleTarget.h"

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CParticle.h"
#include "CPathSearch.h"
#include "CUtil.h"

#include <math.h>
#include <string.h>

// 0x577E40
CProjectileSingleTarget::CProjectileSingleTarget()
    : m_palette(CVidPalette::TYPE_RANGE)
{
    m_speed = 20;
    m_sourceId = 0;
    m_targetId = 0;
    m_posExactX = 0;
    m_deltaX = 0;
    m_targetPos.x = 0;
    m_driftX = 0;
    m_randomMinX = 0;
    m_randomMaxX = 0;
    m_posExactY = 0;
    m_deltaY = 0;
    m_targetPos.y = 0;
    m_driftY = 0;
    m_randomMinY = 0;
    m_randomMaxY = 0;
    m_nStartHeight = 0;
    m_bLimitedLifetime = FALSE;
    m_nLifetime = 0x7FFF;
    // NOTE: No animation: the one difference from 0x578110.
    m_animation = NULL;
    m_shadow = NULL;
    field_1D4 = 0;
    field_1C6 = 0;
    field_E6 = 0x20000;
    field_1DA = 0;
    field_29C = 0;
    field_1DE = TRUE;
    memcpy(m_terrainTable, CGameObject::DEFAULT_TERRAIN_TABLE, sizeof(m_terrainTable));
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_1C2 = TRUE;
    field_182 = CGameObjectArray::INVALID_INDEX;
    m_bHasHeight = TRUE;
    field_2A0 = 1;
    field_2A1 = 1;
}

// 0x578110
CProjectileSingleTarget::CProjectileSingleTarget(CResRef resRef)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    m_speed = 20;
    m_posExactX = 0;
    m_deltaX = 0;
    m_targetPos.x = 0;
    m_driftX = 0;
    m_randomMinX = 0;
    m_randomMaxX = 0;
    m_sourceId = 0;
    m_posExactY = 0;
    m_deltaY = 0;
    m_targetPos.y = 0;
    m_driftY = 0;
    m_randomMinY = 0;
    m_randomMaxY = 0;
    m_targetId = 0;
    m_nStartHeight = 0;
    m_bLimitedLifetime = FALSE;
    m_nLifetime = 0x7FFF;
    m_animation = new CVidCell(resRef, FALSE);
    m_shadow = NULL;
    field_1D4 = 0;
    field_1C6 = 0;
    field_E6 = 0x20000;
    field_1DA = 0;
    field_29C = 0;
    field_1DE = TRUE;
    memcpy(m_terrainTable, CGameObject::DEFAULT_TERRAIN_TABLE, sizeof(m_terrainTable));
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_1C2 = TRUE;
    field_182 = CGameObjectArray::INVALID_INDEX;
    m_bHasHeight = TRUE;
    field_2A0 = 1;
    field_2A1 = 1;
}

// 0x5783E0
CProjectileSingleTarget::~CProjectileSingleTarget()
{
    if (m_animation != NULL) {
        delete m_animation;
    }

    if (m_shadow != NULL) {
        delete m_shadow;
    }
}

// 0x578AB0
void CProjectileSingleTarget::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: The original compares the time stop caster with the projectile's
    // own id, so every projectile stands still during a time stop.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    m_animation->FrameAdvance();

    if (m_targetPos.x / 16 == m_pos.x / 16 && m_targetPos.y / 12 == m_pos.y / 12) {
        OnArrival();
        return;
    }

    LONG nDistance = (m_targetPos.y - m_pos.y) * (m_targetPos.y - m_pos.y) * 16 / 9
        + (m_targetPos.x - m_pos.x) * (m_targetPos.x - m_pos.x);
    LONG nStep = m_speed + 1;
    if (nDistance <= nStep * nStep) {
        OnArrival();
        return;
    }

    if (m_nStartDistance == 0) {
        OnArrival();
        return;
    }

    if (m_nLifetime-- == 0) {
        RemoveSelf();
        return;
    }

    if (m_targetId != CGameObjectArray::INVALID_INDEX) {
        CGameObject* pTarget;

        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            &pTarget,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            RemoveSelf();
            return;
        }

        if (m_pArea != pTarget->GetArea()) {
            LONG nTargetId = m_targetId;
            RemoveSelf();
            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            return;
        }

        CPoint ptTarget = pTarget->GetPos();

        if (!m_bTrackTargetHeight) {
            SHORT nHeight = static_cast<SHORT>(m_nStartHeight * nDistance / m_nStartDistance);
            m_posZ += nHeight - m_nCurrentHeight;
            m_nCurrentHeight = nHeight;
        } else {
            if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite;

                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_targetId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED);

                if (rc == CGameObjectArray::SUCCESS) {
                    CRect rFx;
                    CPoint ptReference;
                    pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, pSprite->m_posZ);

                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_targetId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);

                    LONG posZ = m_posZ;
                    if (posZ != (rFx.bottom - rFx.top) >> 1) {
                        // NOTE: The original steps the height towards the
                        // middle of the target and then stores the old height
                        // back (0x578DC2-0x578DDA), so the height never
                        // changes.
                        if (m_nStartHeight < 0) {
                            m_posZ = posZ + 1;
                        } else {
                            m_posZ = posZ - 1;
                        }
                        m_posZ = posZ;
                    }
                }
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        UpdateBasedOnTargetPoint(ptTarget.x, ptTarget.y);
    } else {
        UpdateBasedOnTargetPoint(m_targetPos.x, m_targetPos.y);
    }

    if (m_bSparkle) {
        // TODO: Incomplete (0x578E38-0x578EA2). The original adds a sparkle
        // object (operator new(0xCA), 0x51AE40 with m_nSparkleColour and the
        // WORDs at 0x84D600 and 0x84D5F0) to the area at the projectile's
        // position; its class is not ported.
    }

    m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
}

// 0x578480
void CProjectileSingleTarget::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    // TODO: Incomplete. The original draws m_animation (and m_shadow when
    // field_1D4 is set) through CInfinity's FX functions when the tile under
    // the projectile is visible and passable.
    (void)pArea;
    (void)pVidMode;
    (void)a3;
}

// 0x5791D0
void CProjectileSingleTarget::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    CGameObject* pObject;
    BYTE rc;
    BYTE nTargetType;
    CPoint ptTarget;
    CPoint ptReference;
    CRect rFx;

    (void)nHeight;
    (void)nType;

    m_targetId = target;
    m_sourceId = source;
    m_pArea = pArea;

    if (target != CGameObjectArray::INVALID_INDEX) {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(target,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        nTargetType = pObject->GetObjectType();
        ptTarget = pObject->GetPos();

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            CGameSprite* pSprite;

            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED);

            if (rc == CGameObjectArray::SUCCESS) {
                pSprite->GetAnimation()->CalculateFxRect(rFx, ptReference, pSprite->m_posZ);

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            } else {
                ptReference.x = 32;
                ptReference.y = 50;
                rFx.left = 0;
                rFx.top = 0;
                rFx.right = 64;
                rFx.bottom = 64;
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    } else {
        ptTarget = targetPos;
        ptReference.x = 32;
        ptReference.y = 50;
        rFx.left = 0;
        rFx.top = 0;
        rFx.right = 64;
        rFx.bottom = 64;
        nTargetType = CGameObject::TYPE_NONE;
    }

    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        &pObject,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    LONG nSourceHeight = DetermineHeight(static_cast<CGameSprite*>(pObject));
    CPoint ptSource = pObject->GetPos();
    BOOL bSourceIsSprite = pObject->GetObjectType() == CGameObject::TYPE_SPRITE;

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    CPoint ptLaunch;
    GetSourcePosition(m_sourceId, ptLaunch, TRUE);

    if (bSourceIsSprite) {
        LONG dy = ptTarget.y / CPathSearch::GRID_SQUARE_SIZEY - ptLaunch.y / CPathSearch::GRID_SQUARE_SIZEY;
        LONG dx = ptTarget.x / CPathSearch::GRID_SQUARE_SIZEX - ptLaunch.x / CPathSearch::GRID_SQUARE_SIZEX;
        if (dx * dx + dy * dy <= 2) {
            ptLaunch = ptSource;
        }
    }

    if (strcmp(m_sVisualEffect, "") != 0) {
        // TODO: Incomplete (0x5794F1-0x5796A8). The original creates the
        // visual effect object named m_sVisualEffect (0x554D20, from its VEF
        // resource) at the launch point aimed at the target and keeps its id
        // in field_182; it then sets the object's speed (+0x82) to m_speed
        // and its flag +0x9A bit 0. The object's class is not ported, so
        // field_182 stays INVALID_INDEX. NOTE: The original also queues a
        // message (vtable 0x84D328, BROADCAST_OTHERS) with the effect, the
        // area, the two points and the height; its Run is empty and it only
        // goes to the other players (single player only,
        // docs/adr/0002-single-player-only.md).
    }

    m_nStartDistance = (ptTarget.y - ptLaunch.y) * (ptTarget.y - ptLaunch.y) * 16 / 9
        + (ptTarget.x - ptLaunch.x) * (ptTarget.x - ptLaunch.x);

    if (m_bLimitedLifetime) {
        SHORT nDistance = static_cast<SHORT>(static_cast<LONG>(sqrt(static_cast<double>(m_nStartDistance)) + 0.5));
        m_nLifetime = static_cast<SHORT>(nDistance / m_speed + 1);
    }

    SHORT nStartHeight;
    if (nTargetType == CGameObject::TYPE_SPRITE) {
        nStartHeight = static_cast<SHORT>(nSourceHeight - ((rFx.bottom - rFx.top) >> 1));
    } else {
        m_nStartDistance = 1;
        nStartHeight = 0;
    }

    m_nStartHeight = nStartHeight;
    m_nCurrentHeight = nStartHeight;

    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CGameObject::AddToArea(pArea, ptLaunch, nSourceHeight, CGameObject::LIST_FRONT);
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);

    m_posExactX = ptLaunch.x << CParticle::RESOLUTION_INC;
    m_targetPos.x = ptTarget.x;
    m_posExactY = ((ptLaunch.y << CParticle::RESOLUTION_INC) * 4) / 3;
    m_targetPos.y = ptTarget.y;
    m_nDirection = GetDirection(ptTarget);
}

// 0x5297D0
DWORD CProjectileSingleTarget::GetField_E6()
{
    return field_E6;
}

// 0x578ED0
void CProjectileSingleTarget::UpdateBasedOnTargetPoint(LONG x, LONG y)
{
    m_nDirection = GetDirection(CPoint(x, y));

    LONG dy = (y * 4) / 3 - (m_pos.y * 4) / 3;
    LONG dx = x - m_pos.x;
    LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

    if (nDistance == 0) {
        m_deltaX = 1;
        m_deltaY = 1;
    } else {
        m_deltaX = (dx << CParticle::RESOLUTION_INC) * m_speed / nDistance;
        m_targetPos.x = x;
        m_deltaY = (dy << CParticle::RESOLUTION_INC) * m_speed / nDistance;
        m_targetPos.y = y;
        m_deltaX += m_driftX;
        m_deltaY += m_driftY;

        LONG nRangeX = m_randomMaxX - m_randomMinX;
        LONG nRangeY = m_randomMaxY - m_randomMinY;
        if (nRangeX > 0) {
            m_deltaX += m_randomMinX + rand() % nRangeX;
        }
        if (nRangeY > 0) {
            m_deltaY += m_randomMinY + rand() % nRangeY;
        }
    }

    m_posExactX += m_deltaX;
    m_posExactY += m_deltaY;

    if (m_driftX < 0) {
        if (-static_cast<LONG>(m_driftDecay) < m_driftX) {
            m_driftX = 0;
        } else {
            m_driftX += m_driftDecay;
        }
    } else if (m_driftX > 0) {
        if (m_driftX < static_cast<LONG>(m_driftDecay)) {
            m_driftX = 0;
        } else {
            m_driftX -= m_driftDecay;
        }
    }

    if (m_driftY < 0) {
        if (-static_cast<LONG>(m_driftDecay) < m_driftY) {
            m_driftY = 0;
        } else {
            m_driftY += m_driftDecay;
        }
    } else if (m_driftY > 0) {
        if (m_driftY < static_cast<LONG>(m_driftDecay)) {
            m_driftY = 0;
        } else {
            m_driftY -= m_driftDecay;
        }
    }

    m_pos.x = m_posExactX >> CParticle::RESOLUTION_INC;
    m_pos.y = ((m_posExactY * 3) / 4) >> CParticle::RESOLUTION_INC;

    if (field_182 != CGameObjectArray::INVALID_INDEX) {
        CGameObject* pObject;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(field_182,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pObject->m_posZ = m_posZ;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(field_182,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// -----------------------------------------------------------------------------

// x87 __ftol (0x7E812C): truncation toward zero, and the integer indefinite
// 0x80000000 for a NaN or a value out of range (as CAIGroup.cpp's).
static LONG Ftol(double value)
{
    if (!(value > -2147483649.0 && value < 2147483648.0)) {
        return static_cast<LONG>(0x80000000u);
    }
    return static_cast<LONG>(value);
}

// NOTE: Inlined in 0x57A970: CProjectileSingleTarget's constructor
// (0x578110), then the vtable 0x850D54.
CProjectile850D54::CProjectile850D54(CResRef resRef)
    : CProjectileSingleTarget(resRef)
{
}

// 0x579B30
void CProjectile850D54::DeliverEffects()
{
    CProjectile::DeliverEffects();
}

// 0x579B40
CProjectile850DDC::CProjectile850DDC(const char* szAnimation, const char* szPuff)
{
    m_nWidth = 200;
    m_nLength = 350;
    m_nHalfWidth = 25;
    m_sPuff = szPuff;
    field_2EE = 1;
    m_nPuffSpacing = 20;
    m_nPuffSpeed = 35;
    m_nPuffSpeedRandom = 45;
    m_nFrames = 15;
    m_nWaveInterval = 5;
    m_bRandomPuffFrame = 0;
    field_317 = 1;
    m_nFrame = 0;
    m_bDelivered = FALSE;
    m_nPuffs = 0;
    m_nHeight = 0;
    m_nType = 0;
    field_2A0 = 0;
    field_2A1 = 1;

    if (strlen(szAnimation) != 0) {
        CResRef resRef = CResRef(CString(szAnimation));
        if (m_animation != NULL) {
            delete m_animation;
        }
        m_animation = new CVidCell(resRef, FALSE);
        field_1DE = TRUE;
    } else {
        // NOTE: The string at 0x8AF4E8.
        CResRef resRef = CResRef("MMissiT");
        if (m_animation != NULL) {
            delete m_animation;
        }
        m_animation = new CVidCell(resRef, FALSE);
        field_1DE = FALSE;
    }

    m_bHasHeight = TRUE;
}

// 0x579E50
void CProjectile850DDC::AIUpdate()
{
    m_nFrame++;
    if (m_nFrame > m_nFrames) {
        if (m_bDelivered == TRUE) {
            RemoveFromArea();

            BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
                CGameObjectArray::THREAD_ASYNCH,
                NULL,
                INFINITE);
            if (rc == CGameObjectArray::SUCCESS) {
                delete this;
                return;
            }
        }
    } else if (m_nFrame % m_nWaveInterval == 1) {
        sub_88();
    }

    if (!m_bDelivered) {
        CProjectileSingleTarget::AIUpdate();
    }
}

// 0x579EF0
void CProjectile850DDC::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    m_nHeight = nHeight;
    m_nType = nType;
    m_sourceId = source;
    m_pArea = pArea;

    CGameObject* pSource;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    CProjectile::GetSourcePosition(m_sourceId, m_ptSource, TRUE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    m_nPuffs = m_nWidth / m_nPuffSpacing / 2;

    // NOTE: The original computes in x87 doubles (53-bit precision control)
    // and truncates with __ftol; the order of the operations is kept.
    LONG dx = targetPos.x - m_ptSource.x;
    LONG dy = targetPos.y - m_ptSource.y;
    LONG ndy = -dy;
    LONG ndx = -dx;
    double fDx = static_cast<double>(dx);
    double fDy = static_cast<double>(dy);
    double fLength2 = static_cast<double>(dy * dy + dx * dx);
    double fScale;

    // The two corners at the source, half the width apart.
    fScale = sqrt(static_cast<double>(m_nHalfWidth * m_nHalfWidth) / static_cast<double>(ndy * ndy + dx * dx));
    m_corners[0].x = Ftol(static_cast<double>(ndy) * fScale + static_cast<double>(m_ptSource.x));
    m_corners[0].y = Ftol(fDx * fScale + static_cast<double>(m_ptSource.y));

    fScale = sqrt(static_cast<double>(m_nHalfWidth * m_nHalfWidth) / static_cast<double>(ndx * ndx + dy * dy));
    m_corners[1].x = Ftol(fDy * fScale + static_cast<double>(m_ptSource.x));
    m_corners[1].y = Ftol(static_cast<double>(ndx) * fScale + static_cast<double>(m_ptSource.y));

    // The far end, the length (plus the half width) along the target's
    // direction.
    LONG nReach = m_nHalfWidth + m_nLength;
    fScale = sqrt(static_cast<double>(nReach * nReach) / fLength2);
    CPoint ptFar;
    ptFar.x = Ftol(fDx * fScale + static_cast<double>(m_ptSource.x));
    ptFar.y = Ftol(fDy * fScale + static_cast<double>(m_ptSource.y));

    fScale = sqrt(static_cast<double>(m_nLength * m_nLength) / fLength2);
    m_ptEnd.x = Ftol(fDx * fScale + static_cast<double>(m_ptSource.x));
    m_ptEnd.y = Ftol(fDy * fScale + static_cast<double>(m_ptSource.y));

    // The two corners at the far end.
    LONG nHalf = m_nHalfWidth + m_nWidth / 2;
    fScale = sqrt(static_cast<double>(nHalf * nHalf) / static_cast<double>(ndx * ndx + dy * dy));
    m_corners[2].x = Ftol(fDy * fScale + static_cast<double>(ptFar.x));
    m_corners[2].y = Ftol(static_cast<double>(ndx) * fScale + static_cast<double>(ptFar.y));

    fScale = sqrt(static_cast<double>(nHalf * nHalf) / static_cast<double>(ndy * ndy + dx * dx));
    m_corners[3].x = Ftol(static_cast<double>(ndy) * fScale + static_cast<double>(ptFar.x));
    m_corners[3].y = Ftol(fDx * fScale + static_cast<double>(ptFar.y));

    // The puff points across the end, m_nPuffSpacing apart.
    for (LONG n = -m_nPuffs; n < m_nPuffs; n++) {
        CPoint pt;
        if (n < 0) {
            LONG nOffset = -(m_nPuffSpacing * n);
            fScale = sqrt(static_cast<double>(nOffset * nOffset) / static_cast<double>(ndy * ndy + dx * dx));
            pt.x = Ftol(static_cast<double>(ndy) * fScale + static_cast<double>(m_ptEnd.x));
            pt.y = Ftol(fDx * fScale + static_cast<double>(m_ptEnd.y));
        } else if (n > 0) {
            LONG nOffset = m_nPuffSpacing * n;
            fScale = sqrt(static_cast<double>(nOffset * nOffset) / static_cast<double>(ndx * ndx + dy * dy));
            pt.x = Ftol(fDy * fScale + static_cast<double>(m_ptEnd.x));
            pt.y = Ftol(static_cast<double>(ndx) * fScale + static_cast<double>(m_ptEnd.y));
        } else {
            pt = m_ptEnd;
        }

        // NOTE: Inlined `std::vector::insert` at the end (0x57AB70).
        m_puffPoints.push_back(pt);
    }

    m_puffPoints.push_back(m_ptEnd);

    CProjectileSingleTarget::Fire(pArea, source, target, targetPos, nHeight, nType);
}

// 0x57A530
void CProjectile850DDC::OnArrival()
{
    CProjectile* pProjectile;
    BYTE rc;

    if (m_callBackProjectile != CGameObjectArray::INVALID_INDEX) {
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_callBackProjectile,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        pProjectile->CallBack();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_callBackProjectile,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    // NOTE: Uninline.
    PlaySound(m_arrivalSoundRef, m_loopArrivalSound, TRUE);

    if (field_182 != CGameObjectArray::INVALID_INDEX) {
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(field_182,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pProjectile->RemoveSelf();

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(field_182,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    // NOTE: Unlike CProjectile::OnArrival, it stays: AIUpdate removes it
    // once its frames have run out.
    DeliverEffects();
}

// 0x57A670
void CProjectile850DDC::DeliverEffects()
{
    CRect rBounding;
    rBounding.left = min(min(m_corners[0].x, m_corners[1].x), min(m_corners[2].x, m_corners[3].x));
    rBounding.top = min(min(m_corners[0].y, m_corners[1].y), min(m_corners[2].y, m_corners[3].y));
    rBounding.right = max(max(m_corners[0].x, m_corners[1].x), max(m_corners[2].x, m_corners[3].x));
    rBounding.bottom = max(max(m_corners[0].y, m_corners[1].y), max(m_corners[2].y, m_corners[3].y));

    CTypedPtrList<CPtrList, LONG*> found;
    m_pArea->GetAllInPoly(rBounding,
        m_corners,
        4,
        CAIObjectType::ANYONE,
        m_pArea->m_visibleTerrainTable,
        found,
        TRUE);

    POSITION pos = found.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(found.GetNext(pos)));
        if (nId == m_sourceId) {
            continue;
        }

        CGameObject* pObject;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if ((pObject->GetObjectType() & CGameObject::TYPE_AIBASE) != 0
            && m_pArea->CheckLOS(m_ptSource, pObject->GetPos(), m_terrainTable, FALSE)) {
            POSITION posEffect = m_effectList.GetHeadPosition();
            while (posEffect != NULL) {
                CGameEffect* pEffect = m_effectList.GetNext(posEffect);
                CMessage* message = new CMessageAddEffect(pEffect->Copy(), m_sourceId, nId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    m_bDelivered = TRUE;
}

// 0x57A970
void CProjectile850DDC::sub_88()
{
    if (m_sPuff.length() == 0) {
        return;
    }

    for (std::vector<CPoint>::iterator it = m_puffPoints.begin(); it != m_puffPoints.end(); ++it) {
        CProjectile850D54* pPuff = new CProjectile850D54(CResRef(CString(m_sPuff.c_str())));
        pPuff->m_visualEffect = m_visualEffect;
        pPuff->m_bHasHeight = TRUE;

        if (m_bRandomPuffFrame == 1) {
            BYTE nLength = pPuff->m_animation->GetSequenceLength(0, FALSE);
            if (nLength == 0) {
                pPuff->m_animation->FrameSet(0);
            } else {
                pPuff->m_animation->FrameSet(static_cast<SHORT>(rand() % nLength));
            }
        }

        pPuff->field_2A0 = field_2A0;
        pPuff->field_1D8 = static_cast<WORD>(field_2EE);

        LONG nRandom = m_nPuffSpeedRandom != 0 ? rand() % m_nPuffSpeedRandom : 0;
        pPuff->m_speed = static_cast<SHORT>((m_nPuffSpeed + nRandom) * m_speed / 100);

        pPuff->Fire(m_pArea,
            m_sourceId,
            CGameObjectArray::INVALID_INDEX,
            *it,
            m_nHeight,
            static_cast<SHORT>(m_nType));
    }
}

