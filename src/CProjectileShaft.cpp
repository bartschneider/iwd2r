#include "CProjectileShaft.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CUtil.h"

// 0x5348C0
CProjectileShaft::CProjectileShaft(const CString& sAnimation, const CString& sSound, BYTE nColor, BOOL bTransparent, BOOL bRandomFrame, WORD nDuration)
    : m_palette(CVidPalette::TYPE_RANGE)
{
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

// 0x534B30
CProjectileShaft::~CProjectileShaft()
{
}

// 0x534C10
void CProjectileShaft::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileBAM::AIUpdate, the time stop caster is compared
    // with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    if (--m_nDuration == 0) {
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

    if (m_bRandomFrame) {
        BYTE nLength = m_animation.GetSequenceLength(0, FALSE);
        if (nLength == 0) {
            m_animation.FrameSet(0);
        } else {
            m_animation.FrameSet(static_cast<SHORT>(rand() % nLength));
        }
    } else {
        if (m_animation.IsEndOfSequence(FALSE)) {
            m_animation.FrameSet(0);
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

// 0x534DD0
void CProjectileShaft::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
    // __LINE__: 9080
    UTIL_ASSERT(pVidMode != NULL);

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    DWORD dwFlags = 0;

    // The sequences stand on each other from the ground up, the first at
    // the bottom.
    CPoint ptPos;
    ptPos.x = m_pos.x;
    ptPos.y = m_pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y;
    if (ptPos.y >= pInfinity->nAreaY) {
        ptPos.y = pInfinity->nAreaY - 1;
    }

    CPoint ptStart = ptPos;

    BYTE nSequence = 0;
    if (static_cast<BYTE>(m_animation.GetNumberSequences(FALSE)) == 0) {
        return;
    }

    do {
        if (ptPos.y < 0) {
            return;
        }

        if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(ptPos))) {
            return;
        }

        m_animation.SequenceSet(nSequence);

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
            dwFlags |= CInfinity::FXPREP_CLEARFILL | 0x20001;
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
            pInfinity->FXBltFrom(a3, rFx, ptPos.x, ptPos.y, ptReference.x, ptReference.y, dwFlags);
        }

        ptPos.y -= ptReference.y;
        nSequence++;
    } while (nSequence < static_cast<BYTE>(m_animation.GetNumberSequences(FALSE)));
}

// 0x535100
void CProjectileShaft::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
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
