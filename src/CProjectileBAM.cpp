#include "CProjectileBAM.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CParticle.h"
#include "CPathSearch.h"
#include "CUtil.h"

#include <math.h>

// 0x52AD60
CProjectileBAM::CProjectileBAM(CResRef resRef)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    field_1CA = 0;
    field_1CE = 0;
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
    field_1DE = TRUE;
    field_E6 = 0x20000;
    field_1DA = 0;
    field_29C = 0;
    memcpy(m_terrainTable, CGameObject::DEFAULT_TERRAIN_TABLE, sizeof(m_terrainTable));
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;

    // NOTE: The original leaves +0x1BE, +0x1C2 and +0x1D8 to the subclasses,
    // and some never set them, yet Render reads all three (uninitialized
    // heap memory). Bounded here (ruling P4-10): they start at 0.
    field_1BE = FALSE;
    field_1C2 = FALSE;
    field_1D8 = 0;
}

// 0x52B030
CProjectileBAM::~CProjectileBAM()
{
    if (m_animation != NULL) {
        delete m_animation;
    }

    if (m_shadow != NULL) {
        delete m_shadow;
    }
}

// 0x52B900
void CProjectileBAM::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileSingleTarget::AIUpdate (the same code), the
    // time stop caster is compared with the projectile's own id.
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

// 0x52B190
void CProjectileBAM::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
    // __LINE__: 4190
    UTIL_ASSERT(pVidMode != NULL);

    CRect rBounds(0, 0, 0, 0);
    COLORREF rgbTint = RGB(CVidPalette::NO_TINT, CVidPalette::NO_TINT, CVidPalette::NO_TINT);

    DWORD dwFlags = GetField_E6();
    if (field_1C6) {
        dwFlags |= 0x200;
    }

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    CPoint ptGrid(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX, m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
    if (m_pArea->m_search.GetMobileCost(ptGrid, m_terrainTable, 3, TRUE) == CPathSearch::COST_IMPASSABLE) {
        return;
    }

    if (!field_1DE) {
        return;
    }

    if (field_1BE) {
        dwFlags |= 0x10000;
    }

    if (field_1D4) {
        dwFlags |= 0x4;
    }

    if (field_1D8 > 1) {
        SetSequenceDirection(NULL);
    }

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    CRect rFx;
    CPoint ptReference;
    CalculateFxRect(rFx, ptReference, NULL);

    CPoint ptPos;
    LONG nHeight;
    ptPos.x = m_pos.x;
    if (!field_1C6 && !field_1D4) {
        ptPos.y = m_pos.y - m_posZ;
        nHeight = 0;
    } else {
        ptPos.y = m_pos.y;
        nHeight = m_posZ;
    }

    if (field_1C2) {
        ptPos.y += m_pArea->GetHeightOffset(m_pos, m_listType);
    }

    // Mirrored left to right past north, upside down between west and
    // east.
    if (field_1DA > CGameSprite::DIR_N) {
        rBounds.left = ptReference.x - rFx.right + ptPos.x + rFx.left;
    } else {
        rBounds.left = ptPos.x - ptReference.x;
    }

    if (field_1DA > CGameSprite::DIR_W && field_1DA < CGameSprite::DIR_E) {
        if (!field_1C6 && !field_1D4) {
            rBounds.top = rFx.top - rFx.bottom + ptReference.y + ptPos.y;
        } else {
            ptReference.y = rFx.bottom - rFx.top - ptReference.y;
            rBounds.top = ptPos.y - m_posZ - ptReference.y;
        }
        nHeight = -nHeight;
    } else {
        rBounds.top = ptPos.y - ptReference.y;
    }

    rBounds.right = rFx.right - rFx.left + rBounds.left;
    rBounds.bottom = rFx.bottom - rFx.top + rBounds.top;

    if (!rView.IntersectRect(rBounds, rView)) {
        return;
    }

    if (field_1BE || field_1C6) {
        rgbTint = m_pArea->GetTintColor(ptPos, m_listType);
    }

    if (field_1DA > CGameSprite::DIR_N) {
        dwFlags |= CInfinity::MIRROR_FX;
    }

    if (field_1DA > CGameSprite::DIR_W && field_1DA < CGameSprite::DIR_E) {
        dwFlags |= CInfinity::MIRROR_FX_UPDOWN;
    }

    dwFlags |= CInfinity::FXPREP_COPYFROMBACK;

    pInfinity->FXPrep(rFx, dwFlags, a3, ptPos, ptReference);
    if (!pInfinity->FXLock(rFx, dwFlags)) {
        return;
    }

    if (field_1BE) {
        m_animation->SetTintColor(rgbTint);
    }

    if (field_1D4) {
        pInfinity->FXRender(m_shadow, ptReference.x, ptReference.y, dwFlags, 0);
    }

    if (field_29C) {
        field_1E2.GetRes()->Demand();
        m_animation->SetPalette(field_1E2.GetRes()->GetColorTable(),
            field_1E2.GetRes()->GetColorCount(),
            CVidPalette::TYPE_RESOURCE);
        pInfinity->FXRender(m_animation, ptReference.x, ptReference.y - nHeight, dwFlags, 128);
        field_1E2.GetRes()->Release();
    } else {
        pInfinity->FXRender(m_animation, ptReference.x, ptReference.y - nHeight, dwFlags, 128);
    }

    pInfinity->FXRenderClippingPolys(ptPos.x, ptPos.y, 0, ptReference, rBounds, FALSE, dwFlags);
    pInfinity->FXUnlock(dwFlags, NULL, CPoint(0, 0));
    pInfinity->FXBltFrom(a3, rFx, ptPos.x, ptPos.y, ptReference.x, ptReference.y, dwFlags);
}

// 0x52B6B0
void CProjectileBAM::CalculateFxRect(CRect& rFx, CPoint& ptReference, CVidCell* pVidCell)
{
    CPoint ptCenter;
    CPoint ptShadowCenter;
    CSize frameSize;

    if (pVidCell == NULL) {
        pVidCell = m_animation;
    }

    if (!field_1C6) {
        if (!field_1D4) {
            pVidCell->GetCurrentCenterPoint(ptReference, FALSE);
            pVidCell->GetCurrentFrameSize(frameSize, FALSE);
            rFx.SetRect(0, 0, frameSize.cx, frameSize.cy);
        }
    }

    // With a shadow: the frame and the shadow's frame on one reference
    // point, the frame raised by the height.
    if (field_1D4) {
        pVidCell->GetCurrentCenterPoint(ptCenter, FALSE);
        ptCenter.y += m_posZ;
        m_shadow->GetCurrentCenterPoint(ptShadowCenter, FALSE);

        ptReference = ptCenter;
        if (ptReference.x < ptShadowCenter.x) {
            ptReference.x = ptShadowCenter.x;
        }
        if (ptReference.y < ptShadowCenter.y) {
            ptReference.y = ptShadowCenter.y;
        }

        pVidCell->GetCurrentFrameSize(frameSize, FALSE);
        frameSize.cx += ptReference.x - ptCenter.x;
        frameSize.cy += ptReference.y - ptCenter.y;
        rFx.SetRect(0, 0, frameSize.cx, frameSize.cy);

        m_shadow->GetCurrentFrameSize(frameSize, FALSE);
        frameSize.cx += ptReference.x - ptShadowCenter.x;
        frameSize.cy += ptReference.y - ptShadowCenter.y;
        if (frameSize.cx > rFx.right) {
            rFx.right = frameSize.cx;
        }
        if (frameSize.cy > rFx.bottom) {
            rFx.bottom = frameSize.cy;
        }
    }

    // With field_1C6: the frame, flipped upside down between west and
    // east, raised by the height, and the rectangle of field_1CA by
    // field_1CE around the reference point.
    if (field_1C6) {
        pVidCell->GetCurrentCenterPoint(ptCenter, FALSE);
        pVidCell->GetCurrentFrameSize(frameSize, FALSE);
        if (field_1DA > CGameSprite::DIR_W && field_1DA < CGameSprite::DIR_E) {
            ptCenter.y = frameSize.cy - ptCenter.y;
        }
        ptCenter.y += m_posZ;

        ptReference = ptCenter;
        if (ptReference.x < static_cast<LONG>(field_1CA)) {
            ptReference.x = field_1CA;
        }
        if (ptReference.y < static_cast<LONG>(field_1CE)) {
            ptReference.y = field_1CE;
        }

        frameSize.cx += ptReference.x - ptCenter.x;
        frameSize.cy += ptReference.y - ptCenter.y;
        rFx.SetRect(0, 0, frameSize.cx, frameSize.cy);

        LONG nRight = ptReference.x - field_1CA + field_1CA * 2;
        LONG nBottom = ptReference.y - field_1CE + field_1CE * 2;
        if (nRight > rFx.right) {
            rFx.right = nRight;
        }
        if (nBottom > rFx.bottom) {
            rFx.bottom = nBottom;
        }
    }
}

// 0x52C8C0
void CProjectileBAM::SetSequenceDirection(CVidCell* pVidCell)
{
    if (pVidCell == NULL) {
        pVidCell = m_animation;
    }

    if (field_1D8 == 1) {
        return;
    }

    if (field_1DA == m_nDirection) {
        return;
    }

    if (field_1D8 == 16) {
        switch (m_nDirection) {
        case 0:
        case 8:
            pVidCell->SequenceSet(0);
            break;
        case 1:
        case 7:
        case 9:
        case 15:
            pVidCell->SequenceSet(1);
            break;
        case 2:
        case 6:
        case 10:
        case 14:
            pVidCell->SequenceSet(2);
            break;
        case 3:
        case 5:
        case 11:
        case 13:
            pVidCell->SequenceSet(3);
            break;
        case 4:
        case 12:
            pVidCell->SequenceSet(4);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
            // __LINE__: 4930
            UTIL_ASSERT(FALSE);
            break;
        }
    } else if (field_1D8 == 8) {
        switch ((m_nDirection / 2) * 2) {
        case 0:
        case 8:
            pVidCell->SequenceSet(0);
            break;
        case 2:
        case 6:
        case 10:
        case 14:
            pVidCell->SequenceSet(2);
            break;
        case 4:
        case 12:
            pVidCell->SequenceSet(4);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
            // __LINE__: 4965
            UTIL_ASSERT(FALSE);
            break;
        }
    }

    if (pVidCell == m_animation) {
        field_1DA = m_nDirection;
    }
}

// 0x52C050
void CProjectileBAM::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
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

    // NOTE: Inlined DetermineHeight (0x52C27A-0x52C2E3).
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
    // NOTE: Inlined PlaySound.
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);

    m_posExactX = ptLaunch.x << CParticle::RESOLUTION_INC;
    m_targetPos.x = ptTarget.x;
    m_posExactY = ((ptLaunch.y << CParticle::RESOLUTION_INC) * 4) / 3;
    m_targetPos.y = ptTarget.y;
    m_nDirection = GetDirection(ptTarget);
}

// 0x52B150
void CProjectileBAM::SetColorRange(USHORT nRange, USHORT nColor)
{
    m_animation->SetPalette(m_palette);
    m_palette.SetRange(nRange, nColor, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
}

// 0x5297D0
DWORD CProjectileBAM::GetField_E6()
{
    return field_E6;
}

// 0x52BD20
void CProjectileBAM::UpdateBasedOnTargetPoint(LONG x, LONG y)
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

// 0x5300E0
CProjectile84DF1C::CProjectile84DF1C()
    : CProjectileBAM(CResRef("SPFIREBL"))
{
    m_animation->SequenceSet(0);
    m_palette.SetRange(0, 67, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_animation->SetPalette(m_palette);
    field_1DE = FALSE;
    field_1D8 = 16;
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
    m_bLimitedLifetime = TRUE;
}
