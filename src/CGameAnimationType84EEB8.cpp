#include "CGameAnimationType84EEB8.h"

#include "CGameSprite.h"
#include "CInfinity.h"
#include "CProjectileArea.h"
#include "CUtil.h"

// 0x55CD70
CGameAnimationType84EEB8::CGameAnimationType84EEB8(const CProjectileAreaExplosion2& explosion, WORD nDirection)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    field_5DE = 0;
    m_translucent = FALSE;
    field_5E1 = 0;
    m_currentBamSequence = 0;
    m_currentBamDirection = 0;
    m_extendDirectionTest = 0;
    m_animationID = 0;
    m_visualEffect = explosion.m_visualEffect;
    m_bPlayOnce = FALSE;
    m_bRandomSequence = explosion.field_2C;
    m_colorChunks = -1;
    m_bRender = TRUE;
    m_pSndDeath = "";

    if (!explosion.m_animation.empty()) {
        const char* szAnimation = explosion.m_animation.c_str();
        m_g1VidCell.SetResRef(CResRef(CString(szAnimation)), FALSE, TRUE, TRUE);

        if (m_bRandomSequence == TRUE) {
            BYTE nSequences = static_cast<BYTE>(m_g1VidCell.GetNumberSequences(FALSE));
            if (nSequences == 0) {
                m_currentBamSequence = 0;
            } else {
                m_currentBamSequence = static_cast<SHORT>(rand() % nSequences);
            }
        } else {
            m_currentBamSequence = 0;
        }

        m_g1VidCell.SequenceSet(m_currentBamSequence);
        m_g1VidCell.FrameSet(0);
        m_sAnimation = szAnimation;
    } else {
        m_g1VidCell.SetResRef(CResRef("GreaseA"), FALSE, TRUE, TRUE);
    }

    m_currentVidCell = &m_g1VidCell;
    m_currentVidCellShadow = NULL;
    m_currentBamDirection = nDirection;
    m_extendDirectionTest = CGameSprite::DIR_N;
}

// 0x55D3A0
CGameAnimationType84EEB8::CGameAnimationType84EEB8(const CProjectileAreaExplosion3& explosion, WORD nDirection)
    : m_palette(CVidPalette::TYPE_RANGE)
{
    field_5DE = 0;
    m_translucent = FALSE;
    field_5E1 = 0;
    m_currentBamSequence = 0;
    m_currentBamDirection = 0;
    m_extendDirectionTest = 0;
    m_animationID = 0;
    m_visualEffect = explosion.m_visualEffect;
    m_bPlayOnce = explosion.field_36;
    m_bRandomSequence = explosion.field_35;
    m_colorChunks = -1;
    m_bRender = TRUE;
    m_pSndDeath = "";

    if (!explosion.m_animation.empty()) {
        const char* szAnimation = explosion.m_animation.c_str();
        m_g1VidCell.SetResRef(CResRef(CString(szAnimation)), FALSE, TRUE, TRUE);

        if (m_bRandomSequence == TRUE) {
            BYTE nSequences = static_cast<BYTE>(m_g1VidCell.GetNumberSequences(FALSE));
            if (nSequences == 0) {
                m_currentBamSequence = 0;
            } else {
                m_currentBamSequence = static_cast<SHORT>(rand() % nSequences);
            }
        } else {
            m_currentBamSequence = 0;
        }

        m_g1VidCell.SequenceSet(m_currentBamSequence);
        m_g1VidCell.FrameSet(0);
        m_sAnimation = szAnimation;
    } else {
        m_g1VidCell.SetResRef(CResRef("GreaseA"), FALSE, TRUE, TRUE);
    }

    m_currentVidCell = &m_g1VidCell;
    m_currentVidCellShadow = NULL;
    m_currentBamDirection = nDirection;
    m_extendDirectionTest = CGameSprite::DIR_N;
}

// 0x55D2E0
CGameAnimationType84EEB8::~CGameAnimationType84EEB8()
{
}

// 0x55D690
void CGameAnimationType84EEB8::CalculateFxRect(CRect& rFx, CPoint& ptReference, LONG posZ)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameAnimationTypeEffect.cpp
    // __LINE__: 157
    UTIL_ASSERT(m_currentVidCell != NULL);

    CPoint ptChar;
    m_currentVidCell->GetCurrentCenterPoint(ptChar, FALSE);

    if (m_currentVidCellShadow != NULL) {
        ptChar.y += posZ;
    }

    ptReference = ptChar;

    CSize frameSize;
    m_currentVidCell->GetCurrentFrameSize(frameSize, FALSE);

    if (m_currentVidCellShadow != NULL) {
        CPoint ptShadow;
        m_currentVidCellShadow->GetCurrentCenterPoint(ptShadow, FALSE);

        ptReference.x = max(ptShadow.x, ptReference.x);
        ptReference.y = max(ptShadow.y, ptReference.y);

        frameSize.cx += ptReference.x - ptChar.x;
        frameSize.cy += ptReference.y - ptChar.y;
        rFx.SetRect(0, 0, frameSize.cx, frameSize.cy);

        m_currentVidCellShadow->GetCurrentFrameSize(frameSize, FALSE);

        rFx.right = max(frameSize.cx + ptReference.x - ptShadow.x, rFx.right);
        rFx.bottom = max(frameSize.cy + ptReference.y - ptShadow.y, rFx.bottom);

        if (m_currentVidCellShadow != NULL) {
            ptReference.y = max(ptChar.y - posZ, ptShadow.y + posZ);
        }
    } else {
        rFx.SetRect(0, 0, frameSize.cx, frameSize.cy);
    }
}

// 0x6A33F0
void CGameAnimationType84EEB8::CalculateGCBoundsRect(CRect& rGCBounds, const CPoint& pos, const CPoint& ptReference, LONG posZ, LONG nWidth, LONG nHeight)
{
    // NOTE: CGameAnimationTypeEffect::CalculateGCBoundsRect's code; the
    // original's two vtables share it.
    if (m_currentBamDirection > m_extendDirectionTest) {
        rGCBounds.left = ptReference.x + pos.x - nWidth;
    } else {
        rGCBounds.left = pos.x - ptReference.x;
    }

    rGCBounds.top = posZ + pos.y - ptReference.y;
    rGCBounds.right = rGCBounds.left + nWidth;
    rGCBounds.bottom = rGCBounds.top + nHeight;
}

// 0x55D800
void CGameAnimationType84EEB8::ChangeDirection(SHORT nDirection)
{
    m_currentBamDirection = nDirection;
}

// 0x6A3C60
BOOLEAN CGameAnimationType84EEB8::DetectedByInfravision()
{
    // NOTE: CGameAnimationTypeEffect::DetectedByInfravision's code.
    return FALSE;
}

// 0x55D2A0
BOOL CGameAnimationType84EEB8::IsMirroring()
{
    // NOTE: The code CGameAnimationTypeEffect's vtable shares (`setg` of
    // the direction, +0x5E4, against m_extendDirectionTest).
    return m_currentBamDirection > m_extendDirectionTest;
}

// 0x55D260
BOOL CGameAnimationType84EEB8::IsEndOfSequence()
{
    // __FILE__: .\Include\IcewindCGameAnimationTypeEffect.h
    // __LINE__: 38
    UTIL_ASSERT(m_currentVidCell != NULL);

    return m_currentVidCell->IsEndOfSequence(FALSE);
}

// 0x55D810
void CGameAnimationType84EEB8::IncrementFrame()
{
    if (!m_currentVidCell->IsEndOfSequence(FALSE)) {
        m_currentVidCell->FrameAdvance();
        if (m_currentVidCellShadow != NULL) {
            m_currentVidCellShadow->FrameAdvance();
        }
        return;
    }

    // At the end of a sequence: played once, it is hidden; else the next
    // sequence, or one at random.
    if (m_bPlayOnce == TRUE) {
        m_bRender = FALSE;
        return;
    }

    if (m_bRandomSequence == TRUE) {
        BYTE nSequences = static_cast<BYTE>(m_g1VidCell.GetNumberSequences(FALSE));
        m_currentBamSequence = nSequences != 0 ? static_cast<SHORT>(rand() % nSequences) : 0;
    } else {
        m_currentBamSequence++;
        // NOTE: Signed compare with the count's low byte.
        if (m_currentBamSequence >= static_cast<BYTE>(m_g1VidCell.GetNumberSequences(FALSE))) {
            m_currentBamSequence = static_cast<SHORT>(static_cast<BYTE>(m_g1VidCell.GetNumberSequences(FALSE)) - 1);
        }
    }

    m_g1VidCell.SequenceSet(m_currentBamSequence);
    m_g1VidCell.FrameSet(0);

    if (m_currentVidCellShadow != NULL) {
        m_g1VidCellShadow.SequenceSet(m_currentBamSequence);
        m_g1VidCellShadow.FrameSet(0);
    }
}

// 0x55D950
void CGameAnimationType84EEB8::Render(CInfinity* pInfinity, CVidMode* pVidMode, INT nSurface, const CRect& rectFX, const CPoint& ptNewPos, const CPoint& ptReference, DWORD dwRenderFlags, COLORREF rgbTintColor, const CRect& rGCBounds, BOOL bDithered, BOOL bFadeOut, LONG posZ, BYTE transparency)
{
    (void)transparency;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameAnimationTypeEffect.cpp
    // __LINE__: 262
    UTIL_ASSERT(pInfinity != NULL && pVidMode != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameAnimationTypeEffect.cpp
    // __LINE__: 263
    UTIL_ASSERT(m_currentVidCell != NULL);

    if (!m_bRender) {
        return;
    }

    CPoint ptPos(ptNewPos.x, ptNewPos.y + posZ);
    CRect rFXRect(rectFX);

    if (m_currentVidCellShadow == NULL
        && !m_visualEffect.field_3
        && !m_visualEffect.field_2
        && !m_visualEffect.field_1) {
        dwRenderFlags |= CInfinity::FXPREP_CLEARFILL;
    } else {
        dwRenderFlags |= CInfinity::FXPREP_COPYFROMBACK;
    }

    pInfinity->FXPrep(rFXRect, dwRenderFlags, nSurface, ptPos, ptReference);

    if (pInfinity->FXLock(rFXRect, dwRenderFlags)) {
        if (m_visualEffect.field_0 == TRUE) {
            m_currentVidCell->SetTintColor(rgbTintColor);
        }

        if (m_currentVidCellShadow != NULL) {
            pInfinity->FXRender(m_currentVidCellShadow,
                ptReference.x,
                ptReference.y - posZ,
                m_visualEffect.m_dwFlags | 0x4,
                0);
        }

        if (m_visualEffect.field_3 == TRUE) {
            pInfinity->FXRender(m_currentVidCell,
                ptReference.x,
                ptReference.y,
                m_visualEffect.m_dwFlags,
                m_visualEffect.m_nTransValue);
        } else {
            pInfinity->FXRender(m_currentVidCell,
                ptReference.x,
                ptReference.y,
                m_visualEffect.m_dwFlags,
                0);
        }

        pInfinity->FXRenderClippingPolys(ptPos.x,
            ptPos.y,
            0,
            ptReference,
            rGCBounds,
            static_cast<BOOLEAN>(bDithered),
            m_visualEffect.m_dwFlags);

        if (bFadeOut) {
            pInfinity->FXUnlock(dwRenderFlags, &rFXRect, ptPos + ptReference);
        } else {
            pInfinity->FXUnlock(dwRenderFlags, NULL, CPoint(0, 0));
        }

        pInfinity->FXBltFrom(nSurface,
            rFXRect,
            ptPos.x,
            ptPos.y,
            ptReference.x,
            ptReference.y,
            m_visualEffect.m_dwFlags | 0x1);
    }
}

// 0x55DBC0
SHORT CGameAnimationType84EEB8::SetSequence(SHORT nSequence)
{
    // NOTE: CGameAnimationTypeEffect::SetSequence's code.
    (void)nSequence;
    return CGameSprite::SEQ_READY;
}

// 0x55DBD0
void CGameAnimationType84EEB8::SetAnimation(const char* szAnimation)
{
    m_g1VidCell.SetResRef(CResRef(CString(szAnimation)), FALSE, TRUE, TRUE);

    if (m_bRandomSequence == TRUE) {
        BYTE nSequences = static_cast<BYTE>(m_g1VidCell.GetNumberSequences(FALSE));
        if (nSequences == 0) {
            m_currentBamSequence = 0;
        } else {
            m_currentBamSequence = static_cast<SHORT>(rand() % nSequences);
        }
    } else {
        m_currentBamSequence = 0;
    }

    m_g1VidCell.SequenceSet(m_currentBamSequence);
    m_g1VidCell.FrameSet(0);
    m_sAnimation = szAnimation;
}
