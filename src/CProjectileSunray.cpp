#include "CProjectileSunray.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CUtil.h"

// The values of m_nState (+0x29A): the block of four sequences drawn.
const DWORD CProjectileSunray::STATE_RISING = 0;
const DWORD CProjectileSunray::STATE_STANDING = 1;
const DWORD CProjectileSunray::STATE_FADING = 2;

// 0x57E860
CProjectileSunray::CProjectileSunray(const CString& sAnimation, const CString& sSound, BYTE nColor, BOOL bTransparent, BOOL bRandomFrame, WORD nDuration)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    m_nState = STATE_RISING;

    m_animation.SetResRef(CResRef(sAnimation), FALSE, TRUE, TRUE);

    if (nColor != 0xFF) {
        m_palette.SetRange(0, nColor, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
        m_animation.SetPalette(m_palette);
    }

    m_animation.FrameSet(0);
    m_fireSoundRef = sSound;
    m_bTransparent = bTransparent;
    m_bRandomFrame = bRandomFrame;
    m_nDuration = nDuration;
}

// 0x57EAD0
CProjectileSunray::~CProjectileSunray()
{
}

// 0x57EB30
void CProjectileSunray::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileShaft::AIUpdate, the time stop caster is
    // compared with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    // The duration ends the standing rays; the fading ones play once.
    if (--m_nDuration == 0) {
        m_nState = STATE_FADING;
        m_animation.FrameSet(0);
    }

    if (m_bRandomFrame) {
        BYTE nLength = m_animation.GetSequenceLength(0, FALSE);
        if (nLength == 0) {
            m_animation.FrameSet(0);
        } else {
            m_animation.FrameSet(static_cast<SHORT>(rand() % nLength));
        }
    } else {
        if (m_animation.IsEndOfSequence(FALSE)) {
            switch (m_nState) {
            case STATE_RISING:
                m_nState = STATE_STANDING;
                m_animation.FrameSet(0);
                break;
            case STATE_STANDING:
                m_animation.FrameSet(0);
                break;
            case STATE_FADING:
                RemoveFromArea();

                if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
                        CGameObjectArray::THREAD_ASYNCH,
                        NULL,
                        INFINITE)
                    == CGameObjectArray::SUCCESS) {
                    delete this;
                }
                return;
            }
        } else {
            m_animation.FrameAdvance();
        }
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

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
}

// 0x57ED30
void CProjectileSunray::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCProjectiles.cpp
    // __LINE__: 1867
    UTIL_ASSERT(pVidMode != NULL);

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    DWORD dwFlags = 0;

    // The state's four sequences stand on each other from the ground up,
    // the first at the bottom.
    CPoint ptPos;
    ptPos.x = m_pos.x;
    ptPos.y = m_pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y;
    if (ptPos.y >= pInfinity->nAreaY) {
        ptPos.y = pInfinity->nAreaY - 1;
    }

    CPoint ptStart = ptPos;

    BYTE nSequence = 0;
    do {
        if (ptPos.y < 0) {
            return;
        }

        if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(ptPos))) {
            return;
        }

        m_animation.SequenceSet(static_cast<WORD>((static_cast<WORD>(m_nState) << 2) + nSequence));

        CPoint ptReference;
        CSize frameSize;
        m_animation.GetCurrentCenterPoint(ptReference, FALSE);
        m_animation.GetCurrentFrameSize(frameSize, FALSE);

        CRect rFx(0, 0, frameSize.cx, frameSize.cy);

        CRect rBounds;
        rBounds.left = ptPos.x - ptReference.x;
        rBounds.top = m_posZ - ptReference.y + ptPos.y;
        rBounds.right = rFx.right - rFx.left + rBounds.left;
        rBounds.bottom = rFx.bottom - rFx.top + rBounds.top;

        CRect rVisible;
        rVisible.IntersectRect(rBounds, rView);
        if (rVisible.IsRectEmpty()) {
            return;
        }

        if (m_bTransparent) {
            dwFlags |= CInfinity::FXPREP_COPYFROMBACK | 0x20200;
        } else {
            dwFlags |= CInfinity::FXPREP_CLEARFILL | 0x20000;
        }

        pInfinity->FXPrep(rFx, dwFlags, a3, ptPos, ptReference);
        if (pInfinity->FXLock(rFx, dwFlags)) {
            pInfinity->FXRender(&m_animation, ptReference.x, ptReference.y, dwFlags, 0);

            CRect rClip(rBounds.left,
                m_posZ + rBounds.top,
                rBounds.right,
                m_posZ + rBounds.bottom);
            pInfinity->FXRenderClippingPolys(ptStart.x, m_posZ + ptStart.y, -m_posZ, ptReference, rClip, FALSE, dwFlags);
            pInfinity->FXUnlock(dwFlags, NULL, CPoint(0, 0));
            pInfinity->FXBltFrom(a3, rFx, ptPos.x, ptPos.y, ptReference.x, ptReference.y, dwFlags | 0x1);
        }

        ptPos.y -= ptReference.y;
        nSequence++;
    } while (nSequence < 4);
}

// 0x57F050
void CProjectileSunray::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)nHeight;
    (void)nType;

    m_sourceId = source;
    m_targetId = target;
    m_pArea = pArea;

    DeliverEffects();

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CPoint ptStart;
    if (m_targetId == CGameObjectArray::INVALID_INDEX) {
        ptStart.x = targetPos.x;
        ptStart.y = targetPos.y + 13;
    } else {
        CGameObject* pTarget;

        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            &pTarget,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            // NOTE: The original returns with the projectile in the object
            // array but in no area.
            return;
        }

        ptStart.x = pTarget->GetPos().x;
        ptStart.y = pTarget->GetPos().y + 13;

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    CGameObject::AddToArea(pArea, ptStart, 0, CGameObject::LIST_FRONT);

    // NOTE: Inlined PlaySound.
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);

    if (m_callBackProjectile != CGameObjectArray::INVALID_INDEX) {
        CProjectile* pProjectile;

        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_callBackProjectile,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pProjectile->CallBack();

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_callBackProjectile,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}
