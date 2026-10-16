#include "CProjectile84E940.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CInfinity.h"
#include "CUtil.h"

// 0x535320
CProjectile84E940::CProjectile84E940(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue)
{
    m_sAnimation = sAnimation;
    m_animation.SetResRef(CResRef(sAnimation + "2"), FALSE, TRUE, TRUE);

    if (sPalette.GetLength() == 0) {
        m_bPalette = FALSE;
    } else {
        m_palette.SetResRef(CResRef(sPalette), TRUE, TRUE);
        m_palette.m_bDoubleSize = FALSE;
        m_bPalette = TRUE;
    }

    m_animation.SequenceSet(0);
    m_animation.FrameSet(0);
    m_fireSoundRef = sSound;
    m_nTransValue = nTransValue;
    m_nState = 0;
    m_dwFlags = dwFlags;
}

// 0x5356C0
CProjectile84E940::~CProjectile84E940()
{
}

// 0x5357B0
void CProjectile84E940::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileBAM::AIUpdate, the time stop caster is compared
    // with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    switch (m_nState) {
    case 0:
        // The door opens ("<animation>2"), then the loop ("<animation>1").
        if (m_animation.IsEndOfSequence(FALSE)) {
            m_animation.SetResRef(CResRef(m_sAnimation + "1"), FALSE, TRUE, TRUE);
            m_animation.SequenceSet(0);
            m_animation.FrameSet(0);
            m_nState++;
        } else {
            m_animation.FrameAdvance();
        }
        break;
    case 1:
    case 2:
    case 3:
        if (m_animation.IsEndOfSequence(FALSE)) {
            m_animation.FrameSet(0);
            m_nState++;
        }
        m_animation.FrameAdvance();
        break;
    case 4:
        // The door again, from its last frame.
        if (m_animation.IsEndOfSequence(FALSE)) {
            m_animation.SetResRef(CResRef(m_sAnimation + "2"), FALSE, TRUE, TRUE);
            m_animation.SequenceSet(0);
            m_animation.FrameSet(m_animation.GetSequenceLength(m_animation.m_nCurrentSequence, FALSE) - 1);
            m_nState++;
        } else {
            m_animation.FrameAdvance();
        }
        break;
    case 5:
        // ... played backwards, then it arrives.
        if (m_animation.m_nCurrentFrame == 0) {
            OnArrival();
            return;
        }
        m_animation.m_nCurrentFrame--;
        break;
    }

    m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
}

// 0x535A40
void CProjectile84E940::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    (void)pArea;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\CProjectile.cpp
    // __LINE__: 9365
    UTIL_ASSERT(pVidMode != NULL);

    DWORD dwFlags = m_dwFlags;

    if (!m_pArea->m_visibility.IsTileVisible(m_pArea->m_visibility.PointToTile(m_pos))) {
        return;
    }

    CInfinity* pInfinity = &(m_pArea->m_cInfinity);

    CRect rView;
    rView.left = pInfinity->nCurrentX;
    rView.top = pInfinity->nCurrentY;
    rView.right = pInfinity->rViewPort.right - pInfinity->rViewPort.left + rView.left;
    rView.bottom = pInfinity->rViewPort.bottom - pInfinity->rViewPort.top + rView.top;

    CPoint ptPos;
    ptPos.x = m_pos.x;
    ptPos.y = m_pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y - m_posZ;

    CPoint ptReference;
    CSize frameSize;
    m_animation.GetCurrentCenterPoint(ptReference, FALSE);
    m_animation.GetCurrentFrameSize(frameSize, FALSE);

    CRect rFx(0, 0, frameSize.cx, frameSize.cy);

    CRect rBounds;
    rBounds.left = ptPos.x - ptReference.x;
    rBounds.top = ptPos.y - ptReference.y;
    rBounds.right = rFx.right - rFx.left + rBounds.left;
    rBounds.bottom = rFx.bottom - rFx.top + rBounds.top;

    CRect rVisible;
    rVisible.IntersectRect(rBounds, rView);
    if (rVisible.IsRectEmpty()) {
        return;
    }

    if (dwFlags == 0) {
        dwFlags = CInfinity::FXPREP_CLEARFILL | 0x1;
    } else {
        dwFlags |= CInfinity::FXPREP_COPYFROMBACK;
    }

    pInfinity->FXPrep(rFx, dwFlags, a3, ptPos, ptReference);
    if (!pInfinity->FXLock(rFx, dwFlags)) {
        return;
    }

    if (!m_bPalette) {
        pInfinity->FXRender(&m_animation, ptReference.x, ptReference.y, dwFlags, m_nTransValue);
    } else {
        m_palette.GetRes()->Demand();
        m_animation.SetPalette(m_palette.GetRes()->GetColorTable(),
            m_palette.GetRes()->GetColorCount(),
            CVidPalette::TYPE_RESOURCE);
        pInfinity->FXRender(&m_animation, ptReference.x, ptReference.y, dwFlags, m_nTransValue);
        m_palette.GetRes()->Release();
    }

    pInfinity->FXRenderClippingPolys(ptPos.x, ptPos.y, 0, ptReference, rBounds, FALSE, dwFlags);
    pInfinity->FXUnlock(dwFlags, NULL, CPoint(0, 0));
    pInfinity->FXBltFrom(a3, rFx, ptPos.x, ptPos.y, ptReference.x, ptReference.y, dwFlags);
}

// 0x535D20
void CProjectile84E940::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)targetPos;
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

    CPoint ptTarget;
    GetSourcePosition(m_targetId, ptTarget, FALSE);
    ptTarget.y--;
    CGameObject::AddToArea(pArea, ptTarget, 0, CGameObject::LIST_FRONT);

    // NOTE: Inlined PlaySound.
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);
}
