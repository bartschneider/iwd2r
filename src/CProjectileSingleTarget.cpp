#include "CProjectileSingleTarget.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CParticle.h"
#include "CPathSearch.h"
#include "CUtil.h"

#include <math.h>

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
