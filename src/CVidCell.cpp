#include "CVidCell.h"

#include "CUtil.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "CVidPoly.h"
#include "CVideo3d.h"
#include "render/api.h"

// 0x8BAC88
DWORD CVidCell::dword_8BAC88 = -1;

// 0x9064F4
BOOL CVidCell::TRANSLUCENT_BLTS_ON;

// 0x8BAC8C
BOOL CVidCell::TRANSLUCENT_SHADOWS_ON = TRUE;

// #binary-identical
// 0x7ACD70
CVidCell::CVidCell()
{
    m_nCurrentFrame = 0;
    m_nCurrentSequence = 0;
    m_bPaletteChanged = FALSE;
    m_pFrame = NULL;
    m_bDoubleSize = FALSE;
    field_C8 = 1;
    m_bShadowOn = TRUE;
}

// 0x7ACE50
CVidCell::CVidCell(CResRef cNewResRef, BOOL bDoubleSize)
{
    // NOTE: Uninline.
    SetResRef(cNewResRef, bDoubleSize, TRUE, TRUE);

    field_C8 = 1;
    m_nCurrentFrame = 0;
    m_nCurrentSequence = 0;
    m_bPaletteChanged = FALSE;
    m_pFrame = NULL;
    m_bShadowOn = TRUE;
}

// 0x7AD0F0
CVidCell::~CVidCell()
{
    for (int index = 0; index < CVidPalette::NUM_RANGES; index++) {
        if (m_paletteAffects.pRangeTints[index] != NULL) {
            delete m_paletteAffects.pRangeTints[index];
        }

        if (m_paletteAffects.pRangeAdds[index] != NULL) {
            delete m_paletteAffects.pRangeAdds[index];
        }

        if (m_paletteAffects.pRangeLights[index] != NULL) {
            delete m_paletteAffects.pRangeLights[index];
        }
    }
}

// #binary-identical
// 0x7AD280
void CVidCell::AddRangeAffect(USHORT nAffectType, USHORT nRangeId, COLORREF rgbAffectColor, BYTE pulsePeriod)
{
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 523
    UTIL_ASSERT(nRangeId < CVidPalette::NUM_RANGES);

    switch (nAffectType) {
    case 0:
        if (m_paletteAffects.pRangeTints[nRangeId] == NULL) {
            m_paletteAffects.pRangeTints[nRangeId] = new COLORREF;
        }
        *m_paletteAffects.pRangeTints[nRangeId] = rgbAffectColor;
        m_paletteAffects.aRangeTintPeriods[nRangeId] = pulsePeriod;
        break;
    case 1:
    case 3:
        if (m_paletteAffects.pRangeAdds[nRangeId] == NULL) {
            m_paletteAffects.pRangeAdds[nRangeId] = new COLORREF;
        }
        *m_paletteAffects.pRangeAdds[nRangeId] = rgbAffectColor;
        m_paletteAffects.aRangeAddPeriods[nRangeId] = pulsePeriod;
        break;
    case 2:
        if (m_paletteAffects.pRangeLights[nRangeId] == NULL) {
            m_paletteAffects.pRangeLights[nRangeId] = new COLORREF;
        }
        *m_paletteAffects.pRangeLights[nRangeId] = rgbAffectColor;
        m_paletteAffects.aRangeLightPeriods[nRangeId] = pulsePeriod;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 565
        UTIL_ASSERT(FALSE);
    }
}

// #binary-identical
// 0x7AD3A0
void CVidCell::DeleteResPaletteAffect()
{
    if (m_paletteAffects.pRangeTints[0] != NULL) {
        delete m_paletteAffects.pRangeTints[0];
        m_paletteAffects.pRangeTints[0] = NULL;
        m_paletteAffects.aRangeTintPeriods[0] = 0;
    }

    if (m_paletteAffects.pRangeAdds[0] != NULL) {
        delete m_paletteAffects.pRangeAdds[0];
        m_paletteAffects.pRangeAdds[0] = NULL;
        m_paletteAffects.aRangeAddPeriods[0] = 0;
    }

    if (m_paletteAffects.pRangeLights[0] != NULL) {
        delete m_paletteAffects.pRangeLights[0];
        m_paletteAffects.pRangeLights[0] = NULL;
        m_paletteAffects.aRangeLightPeriods[0] = 0;
    }
}

// #binary-identical
// 0x7AD400
void CVidCell::DeleteRangeAffects(USHORT nRangeId)
{
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 635
    UTIL_ASSERT(nRangeId < CVidPalette::NUM_RANGES);

    if (m_paletteAffects.pRangeTints[nRangeId] != NULL) {
        delete m_paletteAffects.pRangeTints[nRangeId];
        m_paletteAffects.pRangeTints[nRangeId] = NULL;
    }

    if (m_paletteAffects.pRangeAdds[nRangeId] != NULL) {
        delete m_paletteAffects.pRangeAdds[nRangeId];
        m_paletteAffects.pRangeAdds[nRangeId] = NULL;
    }

    if (m_paletteAffects.pRangeLights[nRangeId] != NULL) {
        delete m_paletteAffects.pRangeLights[nRangeId];
        m_paletteAffects.pRangeLights[nRangeId] = NULL;
    }
}

// 0x7AD490
BOOL CVidCell::Demand(BOOLEAN& bDemanded)
{
    if (!bDemanded) {
        if (m_header.GetResRef() != "") {
            if (m_header.pRes->m_pBamHeaderCopy != NULL) {
                bDemanded = TRUE;
            }

            if (!bDemanded) {
                if (m_header.pRes->Demand() == NULL) {
                    return FALSE;
                }
            }
        } else {
            if (pRes == NULL) {
                return FALSE;
            }

            if (pRes->m_pBamHeaderCopy != NULL) {
                bDemanded = TRUE;
            }

            if (!bDemanded) {
                if (pRes->Demand() == NULL) {
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

// 0x7AD520
BOOL CVidCell::GetCurrentCenterPoint(CPoint& ptReference, BOOLEAN bDemanded)
{
    if (pRes == NULL) {
        ptReference.x = 0;
        ptReference.y = 0;
        return FALSE;
    }

    if (!Demand(bDemanded)) {
        ptReference.x = 0;
        ptReference.y = 0;
        return FALSE;
    }

    SHORT nSequence;
    // NOTE: Uninline. The sequence compares unsigned (jb at 0x7AD5AC), so
    // a negative one counts as past the end.
    if (static_cast<WORD>(m_nCurrentSequence) < GetResBamHeader()->nSequences) {
        nSequence = m_nCurrentSequence;
    } else {
        nSequence = 0;
    }

    SHORT nFrame;
    // NOTE: Uninline.
    if (m_nCurrentFrame < GetResSequences()[nSequence].nFrames) {
        nFrame = m_nCurrentFrame;
    } else {
        if (field_C8) {
            // NOTE: Uninline.
            if (GetResSequences()[nSequence].nFrames > 0) {
                // NOTE: Uninline.
                nFrame = m_nCurrentFrame % GetResSequences()[nSequence].nFrames;
            } else {
                nFrame = 0;
            }
        } else {
            // NOTE: Uninline.
            nFrame = GetResSequences()[nSequence].nFrames - 1;
        }
    }

    if (nFrame < 0) {
        if (field_C8) {
            // FIXME: Repeating calls.
            // NOTE: Lots of inlining.
            if (GetResSequences()[nSequence].nFrames > 0) {
                nFrame = GetResSequences()[nSequence].nFrames + nFrame % GetResSequences()[nSequence].nFrames;
                if (nFrame == GetResSequences()[nSequence].nFrames) {
                    nFrame = 0;
                }
            } else {
                nFrame = 0;
            }
        } else {
            nFrame = 0;
        }
    }

    BOOL bSuccess;

    // NOTE: Uninline.
    FRAMEENTRY* pFrame = GetResFrame(nSequence, nFrame);
    if (pFrame != NULL) {
        ptReference.x = pFrame->nCenterX;
        ptReference.y = pFrame->nCenterY;
        bSuccess = TRUE;
    } else {
        bSuccess = FALSE;
    }

    // NOTE: Uninline.
    Release(bDemanded);

    return bSuccess;
}

// 0x7AD800
BOOL CVidCell::GetCurrentFrameSize(CSize& frameSize, BOOLEAN bDemanded)
{
    if (pRes == NULL) {
        frameSize.cx = 0;
        frameSize.cy = 0;
        return FALSE;
    }

    if (!Demand(bDemanded)) {
        frameSize.cx = 0;
        frameSize.cy = 0;
        return FALSE;
    }

    SHORT nSequence;
    // NOTE: Uninline. The sequence compares unsigned (jb at 0x7AD88C), so
    // a negative one counts as past the end.
    if (static_cast<WORD>(m_nCurrentSequence) < GetResBamHeader()->nSequences) {
        nSequence = m_nCurrentSequence;
    } else {
        nSequence = 0;
    }

    SHORT nFrame;
    // NOTE: Uninline.
    if (m_nCurrentFrame < GetResSequences()[nSequence].nFrames) {
        nFrame = m_nCurrentFrame;
    } else {
        if (field_C8) {
            // NOTE: Uninline.
            if (GetResSequences()[nSequence].nFrames > 0) {
                // NOTE: Uninline.
                nFrame = m_nCurrentFrame % GetResSequences()[nSequence].nFrames;
            } else {
                nFrame = 0;
            }
        } else {
            // NOTE: Uninline.
            nFrame = GetResSequences()[nSequence].nFrames - 1;
        }
    }

    if (nFrame < 0) {
        if (field_C8) {
            // FIXME: Repeating calls.
            // NOTE: Lots of inlining.
            if (GetResSequences()[nSequence].nFrames > 0) {
                nFrame = GetResSequences()[nSequence].nFrames + nFrame % GetResSequences()[nSequence].nFrames;
                if (nFrame == GetResSequences()[nSequence].nFrames) {
                    nFrame = 0;
                }
            } else {
                nFrame = 0;
            }
        } else {
            nFrame = 0;
        }
    }

    BOOL bSuccess;

    // NOTE: Uninline.
    FRAMEENTRY* pFrame = GetResFrame(nSequence, nFrame);
    if (pFrame != NULL) {
        frameSize.cx = pFrame->nWidth;
        frameSize.cy = pFrame->nHeight;
        bSuccess = TRUE;
    } else {
        bSuccess = FALSE;
    }

    // NOTE: Uninline.
    Release(bDemanded);

    return bSuccess;
}

// 0x7ADAE0
BYTE CVidCell::GetSequenceLength(WORD nSequence, BOOLEAN bDemanded)
{
    if (!Demand(bDemanded)) {
        return 0;
    }

    BYTE nFrames;
    // NOTE: Uninline.
    if (nSequence < GetResBamHeader()->nSequences) {
        // NOTE: Uninline.
        nFrames = static_cast<BYTE>(GetResSequences()[nSequence].nFrames);
    } else {
        nFrames = 0;
    }

    // NOTE: Uninline.
    Release(bDemanded);

    return nFrames;
}

// 0x7ADBC0
BOOL CVidCell::GetFrameCenterPoint(SHORT nSequence, SHORT nFrame, CSize& ptCtr, BOOLEAN bDemanded)
{
    if (nSequence == -1) {
        nSequence = m_nCurrentSequence;
    }

    if (nFrame == -1) {
        nFrame = m_nCurrentFrame;
        nSequence = m_nCurrentSequence;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1176
    UTIL_ASSERT(pRes != NULL);

    if (!Demand(bDemanded)) {
        return FALSE;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1189
    UTIL_ASSERT(nSequence < GetResBamHeader()->nSequences);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1190
    UTIL_ASSERT(nFrame < GetResSequences()[nSequence].nFrames);

    BOOL bSuccess;

    FRAMEENTRY* pFrame = GetResFrame(nSequence, nFrame);
    if (pFrame != NULL) {
        ptCtr.cx = pFrame->nCenterX;
        ptCtr.cy = pFrame->nCenterY;
        bSuccess = TRUE;
    } else {
        bSuccess = FALSE;
    }

    Release(bDemanded);

    return bSuccess;
}

// 0x7ADD80
BOOL CVidCell::GetFrameSize(SHORT nSequence, SHORT nFrame, CSize& frameSize, BOOLEAN bDemanded)
{
    if (nSequence == -1) {
        nSequence = m_nCurrentSequence;
    }

    if (nFrame == -1) {
        nFrame = m_nCurrentFrame;
        nSequence = m_nCurrentSequence;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1258
    UTIL_ASSERT(pRes != NULL);

    if (!Demand(bDemanded)) {
        return FALSE;
    }

    // NOTE: Uninline.
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1271
    UTIL_ASSERT(nSequence < GetResBamHeader()->nSequences);

    // NOTE: Uninline.
    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1272
    UTIL_ASSERT(nFrame < GetResSequences()[nSequence].nFrames);

    BOOL bSuccess;

    // NOTE: Uninline.
    FRAMEENTRY* pFrame = GetResFrame(nSequence, nFrame);
    if (pFrame != NULL) {
        frameSize.cx = pFrame->nWidth;
        frameSize.cy = pFrame->nHeight;
        bSuccess = TRUE;
    } else {
        bSuccess = FALSE;
    }

    // NOTE: Uninline.
    Release(bDemanded);

    return bSuccess;
}

// 0x7ADF40
SHORT CVidCell::GetNumberSequences(BOOLEAN bDemanded)
{
    if (pRes == NULL) {
        return 0;
    }

    if (!Demand(bDemanded)) {
        return 0;
    }

    SHORT nSequences = GetResBamHeader()->nSequences;

    Release(bDemanded);

    return nSequences;
}

// #binary-identical
// 0x7ADFE0
BOOL CVidCell::FrameAdvance()
{
    m_nCurrentFrame++;
    return TRUE;
}

// #binary-identical
// 0x7ADFF0
BOOL CVidCell::FrameSet(SHORT nFrame)
{
    m_nCurrentFrame = nFrame;
    return TRUE;
}

// 0x7AE010
BOOL CVidCell::IsEndOfSequence(BOOLEAN bDemanded)
{
    BOOL bResult = FALSE;

    if (pRes == NULL) {
        return TRUE;
    }

    if (!Demand(bDemanded)) {
        return FALSE;
    }

    // NOTE: Uninline. The sequence compares and indexes unsigned (jae at
    // 0x7AE084), so a negative one counts as past the end.
    WORD nSequence = static_cast<WORD>(m_nCurrentSequence);
    if (nSequence < GetResBamHeader()->nSequences) {
        bResult = TRUE;
        // FIXME: Repeating calls.
        // NOTE: Uninline.
        if (m_nCurrentFrame < GetResSequences()[nSequence].nFrames - 1) {
            // NOTE: Uninline.
            if (GetResSequences()[nSequence].nFrames != 0) {
                bResult = FALSE;
            }
        }
    }

    // NOTE: Uninline.
    Release(bDemanded);

    return bResult;
}

// 0x7AE140
void CVidCell::RestoreBackground(INT nFrom, INT nTo, const CRect& rClip)
{
    RECT srcRect;
    srcRect.left = 0;
    srcRect.top = 0;
    srcRect.right = rClip.Width();
    srcRect.bottom = rClip.Height();

    do {
        HRESULT hr = g_pChitin->cVideo.cVidBlitter.BltFast(g_pChitin->GetCurrentVideoMode()->pSurfaces[nTo],
            rClip.left,
            rClip.top,
            g_pChitin->GetCurrentVideoMode()->pSurfaces[nFrom],
            &srcRect,
            DDBLTFAST_WAIT);
        g_pChitin->GetCurrentVideoMode()->CheckResults(hr);
        if (hr != DDERR_SURFACELOST && hr != DDERR_WASSTILLDRAWING) {
            break;
        }
    } while (!g_pChitin->field_1932);
}

// 0x7AE210
void CVidCell::RealizePalette(DWORD dwFlags)
{
    if (!m_bPaletteChanged) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 1537
        UTIL_ASSERT(pRes != NULL);

        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, g_pChitin->cVideo.m_nBpp, dwFlags, &m_paletteAffects, 255);
}

// #binary-identical
// 0x7AE290
BOOL CVidCell::SequenceSet(SHORT nSequence)
{
    m_nCurrentSequence = nSequence;
    return TRUE;
}

// 0x7AE2B0
void CVidCell::SetPalette(RGBQUAD* pPalette, int nEntries, USHORT nType)
{
    if (pPalette != NULL) {
        m_cPalette.SetPalette(pPalette, nEntries, nType);
        m_bPaletteChanged = TRUE;
    } else {
        m_cPalette.SetPalette(NULL, 0, CVidPalette::TYPE_RESOURCE);
        m_bPaletteChanged = FALSE;
    }
}

// 0x7AE300
void CVidCell::SetPalette(const CVidPalette& cPalette)
{
    m_cPalette.SetPalette(cPalette);
    m_bPaletteChanged = TRUE;
}

// #binary-identical
// 0x7AE320
void CVidCell::AddResPaletteAffect(USHORT nAffectType, COLORREF rgbAffectColor, BYTE pulsePeriod)
{
    switch (nAffectType) {
    case 0:
        if (m_paletteAffects.pRangeTints[0] == NULL) {
            m_paletteAffects.pRangeTints[0] = new COLORREF;
        }
        *m_paletteAffects.pRangeTints[0] = rgbAffectColor;
        m_paletteAffects.aRangeTintPeriods[0] = pulsePeriod;
        break;
    case 1:
    case 3:
        if (m_paletteAffects.pRangeAdds[0] == NULL) {
            m_paletteAffects.pRangeAdds[0] = new COLORREF;
        }
        *m_paletteAffects.pRangeAdds[0] = rgbAffectColor;
        m_paletteAffects.aRangeAddPeriods[0] = pulsePeriod;
        break;
    case 2:
        if (m_paletteAffects.pRangeLights[0] == NULL) {
            m_paletteAffects.pRangeLights[0] = new COLORREF;
        }
        *m_paletteAffects.pRangeLights[0] = rgbAffectColor;
        m_paletteAffects.aRangeLightPeriods[0] = pulsePeriod;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 1704
        UTIL_ASSERT(FALSE);
    }
}

// 0x7AE3F0
void CVidCell::StoreBackground(INT nFrom, INT nTo, INT x, INT y, const CRect& rClip, CRect& rStorage, BOOLEAN bNumbered)
{
    if (GetFrame(FALSE)) {
        rStorage.left = x - m_pFrame->nCenterX;
        rStorage.top = y - m_pFrame->nCenterY;

        rStorage.left = x - m_pFrame->nCenterX;
        rStorage.top = y - m_pFrame->nCenterY;

        if (bNumbered) {
            rStorage.right = max(rStorage.left + m_pFrame->nWidth, rStorage.left + m_pFrame->nCenterX + 16);
            rStorage.bottom = max(rStorage.top + m_pFrame->nHeight, rStorage.top + m_pFrame->nCenterY + 16);
        } else {
            rStorage.right = rStorage.left + m_pFrame->nWidth;
            rStorage.bottom = rStorage.top + m_pFrame->nHeight;
        }

        // NOTE: Looks like intersection.
        if (rStorage.left < rClip.left) {
            rStorage.left = rClip.left;
        }

        if (rStorage.top < rClip.top) {
            rStorage.top = rClip.top;
        }

        if (rStorage.right >= rClip.right) {
            rStorage.right = rClip.right;
        }

        if (rStorage.bottom >= rClip.bottom) {
            rStorage.bottom = rClip.bottom;
        }

        if (rStorage.left < rStorage.right && rStorage.top < rStorage.bottom) {
            do {
                HRESULT hr = g_pChitin->cVideo.cVidBlitter.BltFast(g_pChitin->GetCurrentVideoMode()->pSurfaces[nTo],
                    0,
                    0,
                    g_pChitin->GetCurrentVideoMode()->pSurfaces[nFrom],
                    &rStorage,
                    DDBLTFAST_WAIT);
                g_pChitin->GetCurrentVideoMode()->CheckResults(hr);
                if (hr != DDERR_SURFACELOST && hr != DDERR_WASSTILLDRAWING) {
                    break;
                }
            } while (!g_pChitin->field_1932);
        }
    }
}

// 0x7AE590
void CVidCell::SuppressTint(SHORT nRange)
{
    m_paletteAffects.suppressTints |= 1 << nRange;
}

// 0x7AE5B0
void CVidCell::UnsuppressTint(SHORT nRange)
{
    m_paletteAffects.suppressTints &= ~(1 << nRange);
}

// 0x7AE5D0
BOOL CVidCell::Render(WORD* pSurface, LONG lPitch, INT nRefPointX, INT nRefPointY, DWORD dwFlags, INT nTransVal)
{
    BOOL bSuccess;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 1902
    UTIL_ASSERT(!((dwFlags & CVIDIMG_TRANSLUCENT) && (dwFlags & CVIDCELL_BRIGHTEST)));

    if (m_pFrame == NULL) {
        if (!GetFrame(FALSE)) {
            // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
            // __LINE__: 1913
            UTIL_ASSERT_MSG(FALSE, GetResRef().GetResRefStr() + ".BAM is corrupted (GetFrame() failed)");

            // Nothing is drawn or released (xor eax,eax at 0x7AE691).
            return FALSE;
        }
    }

    if ((dwFlags & 0x200) != 0 && TRANSLUCENT_BLTS_ON) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 1932
        UTIL_ASSERT(nTransVal >= 0);

        switch (g_pChitin->cVideo.m_nBpp) {
        case 16:
            bSuccess = sub_7B49D0(pSurface
                    + lPitch / 2 * (nRefPointY - m_pFrame->nCenterY)
                    + (nRefPointX - m_pFrame->nCenterX),
                lPitch,
                dwFlags);
            break;
        case 24:
            bSuccess = sub_7D0C60(reinterpret_cast<BYTE*>(pSurface)
                    + lPitch * (nRefPointY - m_pFrame->nCenterY)
                    + 3 * (nRefPointX - m_pFrame->nCenterX),
                lPitch,
                dwFlags);
            break;
        case 32:
            bSuccess = sub_7D1190(reinterpret_cast<DWORD*>(pSurface)
                    + lPitch / 4 * (nRefPointY - m_pFrame->nCenterY)
                    + (nRefPointX - m_pFrame->nCenterX),
                lPitch,
                dwFlags,
                SurfaceExtent(pSurface, lPitch));
            break;
        default:
            bSuccess = FALSE;
            break;
        }
    } else {
        if ((dwFlags & CVIDIMG_TRANSLUCENT) != 0) {
            // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
            // __LINE__: 1954
            UTIL_ASSERT(nTransVal >= 0);

            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                bSuccess = sub_7AFED0(pSurface
                        + lPitch / 2 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags,
                    nTransVal);
                break;
            case 24:
                bSuccess = sub_7CF8D0(reinterpret_cast<BYTE*>(pSurface)
                        + lPitch * (nRefPointY - m_pFrame->nCenterY)
                        + 3 * (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags,
                    nTransVal);
                break;
            case 32:
                bSuccess = sub_7D0950(reinterpret_cast<DWORD*>(pSurface)
                        + lPitch / 4 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags,
                    nTransVal,
                    SurfaceExtent(pSurface, lPitch));
                break;
            default:
                bSuccess = FALSE;
                break;
            }
        } else if ((dwFlags & 0x8) != 0) {
            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                bSuccess = sub_7AFB50(pSurface
                        + lPitch / 2 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            case 24:
                bSuccess = sub_7CF650(reinterpret_cast<BYTE*>(pSurface)
                        + lPitch * (nRefPointY - m_pFrame->nCenterY)
                        + 3 * (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            case 32:
                bSuccess = Blt8To32Brightest(reinterpret_cast<DWORD*>(pSurface)
                        + lPitch / 4 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            default:
                bSuccess = FALSE;
                break;
            }
        } else if ((dwFlags & 0x4) != 0 && CVidCell::TRANSLUCENT_SHADOWS_ON) {
            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                bSuccess = sub_7AF8A0(pSurface
                        + lPitch / 2 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            case 24:
                bSuccess = sub_7CF3F0(reinterpret_cast<BYTE*>(pSurface)
                        + lPitch * (nRefPointY - m_pFrame->nCenterY)
                        + 3 * (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            case 32:
                bSuccess = Blt8To32ClearShadow(reinterpret_cast<DWORD*>(pSurface)
                        + lPitch / 4 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            default:
                bSuccess = FALSE;
                break;
            }
        } else {
            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                bSuccess = sub_7AF1C0(pSurface
                        + lPitch / 2 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            case 24:
                bSuccess = sub_7CECD0(reinterpret_cast<BYTE*>(pSurface)
                        + lPitch * (nRefPointY - m_pFrame->nCenterY)
                        + 3 * (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags);
                break;
            case 32:
                bSuccess = Blt8To32(reinterpret_cast<DWORD*>(pSurface)
                        + lPitch / 4 * (nRefPointY - m_pFrame->nCenterY)
                        + (nRefPointX - m_pFrame->nCenterX),
                    lPitch,
                    dwFlags,
                    255);
                break;
            default:
                bSuccess = FALSE;
                break;
            }
        }
    }

    pRes->Release();
    m_pFrame = NULL;

    return bSuccess;
}

// 0x7AEAD0
BOOL CVidCell::Render(INT nSurface, int x, int y, const CRect& rClip, CVidPoly* pClipPoly, int nPolys, DWORD dwFlags, int nTransVal)
{
    if (auto* b = render::Active(); b && b->Clipped(rClip).DrawCell(*this, m_nCurrentFrame, { x, y }, &m_cPalette, dwFlags, nTransVal)) return render::CellHandled(*this);
    BOOL bSuccess;

    if (g_pChitin->cVideo.m_bIs3dAccelerated) {
        return Render3d(x, y, rClip, pClipPoly, nPolys, dwFlags | 0x1, nTransVal, 0);
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 2065
    UTIL_ASSERT(nSurface < g_pChitin->GetCurrentVideoMode()->GetNumSurfaces() && g_pChitin->GetCurrentVideoMode()->pSurfaces[nSurface] != NULL);

    if (m_nFxSurface == -1) {
        if (!g_pChitin->GetCurrentVideoMode()->GetFXSurface(m_nFxSurface, dwFlags)) {
            return FALSE;
        }
    }

    DDCOLORKEY ddck;
    g_pChitin->GetCurrentVideoMode()->pSurfaces[m_nFxSurface]->GetColorKey(DDCKEY_SRCBLT, &ddck);

    if (m_pFrame == NULL) {
        if (!GetFrame(FALSE)) {
            return FALSE;
        }
    }

    int left = x - m_pFrame->nCenterX;
    int top = y - m_pFrame->nCenterY;
    CRect rSrc(0, 0, m_pFrame->nWidth, m_pFrame->nHeight);
    CRect rDest(left, top, left + m_pFrame->nWidth, top + m_pFrame->nHeight);

    DDSURFACEDESC ddsd;

    if ((dwFlags & CVIDIMG_TRANSLUCENT) || ((dwFlags & 0x4) != 0 && CVidCell::TRANSLUCENT_SHADOWS_ON)) {
        BltBackGroundToFx(g_pChitin->GetCurrentVideoMode()->pSurfaces[nSurface],
            rDest,
            rClip,
            dwFlags);

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 2112
        UTIL_ASSERT(m_nFxSurface != -1);

        ddsd.dwSize = sizeof(ddsd);
        if (!g_pChitin->GetCurrentVideoMode()->LockSurface(m_nFxSurface, &ddsd, rSrc)) {
            pRes->Release();
            return FALSE;
        }

        if ((dwFlags & 0x4) != 0) {
            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                bSuccess = sub_7AF8A0(reinterpret_cast<WORD*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    dwFlags);
                break;
            case 24:
                bSuccess = sub_7CF3F0(reinterpret_cast<BYTE*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    dwFlags);
                break;
            case 32:
                bSuccess = Blt8To32ClearShadow(reinterpret_cast<DWORD*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    dwFlags);
                break;
            default:
                bSuccess = FALSE;
                break;
            }
        } else {
            // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
            // __LINE__: 2145
            UTIL_ASSERT(nTransVal >= 0);

            switch (g_pChitin->cVideo.m_nBpp) {
            case 16:
                bSuccess = sub_7AFED0(reinterpret_cast<WORD*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    dwFlags,
                    nTransVal);
                break;
            case 24:
                bSuccess = sub_7CF8D0(reinterpret_cast<BYTE*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    dwFlags,
                    nTransVal);
                break;
            case 32:
                bSuccess = sub_7D0950(reinterpret_cast<DWORD*>(ddsd.lpSurface),
                    ddsd.lPitch,
                    dwFlags,
                    nTransVal,
                    SurfaceExtent(ddsd));
                break;
            default:
                bSuccess = FALSE;
                break;
            }
        }
    } else {
        DDBLTFX fx;
        fx.dwSize = sizeof(fx);
        fx.dwFillColor = ddck.dwColorSpaceLowValue;

        g_pChitin->cVideo.cVidBlitter.Blt(g_pChitin->GetCurrentVideoMode()->pSurfaces[m_nFxSurface],
            rSrc,
            NULL,
            NULL,
            DDBLT_COLORFILL | DDBLT_WAIT,
            &fx);

        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 2180
        UTIL_ASSERT(m_nFxSurface != -1);

        ddsd.dwSize = sizeof(ddsd);
        if (!g_pChitin->GetCurrentVideoMode()->LockSurface(m_nFxSurface, &ddsd, rSrc)) {
            pRes->Release();
            return FALSE;
        }

        switch (g_pChitin->cVideo.m_nBpp) {
        case 16:
            bSuccess = sub_7AF1C0(reinterpret_cast<WORD*>(ddsd.lpSurface),
                ddsd.lPitch,
                dwFlags);
            break;
        case 24:
            bSuccess = sub_7CECD0(reinterpret_cast<BYTE*>(ddsd.lpSurface),
                ddsd.lPitch,
                dwFlags);
            break;
        case 32:
            bSuccess = Blt8To32(reinterpret_cast<DWORD*>(ddsd.lpSurface),
                ddsd.lPitch,
                dwFlags,
                255);
            break;
        default:
            bSuccess = FALSE;
            break;
        }
    }

    if (pClipPoly != NULL) {
        for (int nIndex = 0; nIndex < nPolys; nIndex++) {
            pClipPoly[nIndex].FillPoly(reinterpret_cast<WORD*>(ddsd.lpSurface),
                ddsd.lPitch,
                rSrc,
                ddck.dwColorSpaceLowValue,
                0,
                CPoint(left, top));
        }
    }

    g_pChitin->GetCurrentVideoMode()->UnLockSurface(m_nFxSurface, ddsd.lpSurface);

    if (bSuccess) {
        BltFromFX(g_pChitin->GetCurrentVideoMode()->pSurfaces[nSurface],
            left,
            top,
            rClip,
            dwFlags);
    }

    pRes->Release();
    m_pFrame = NULL;

    return bSuccess;
}

// 0x7AF060
BOOL CVidCell::Render(WORD* pSurface, LONG lPitch, INT nRefPtX, INT nRefPtY, const CRect& rClip, BOOLEAN bDemanded, DWORD dwFlags, const CPoint& ptSource)
{
    if (m_pFrame == NULL) {
        if (!GetFrame(bDemanded)) {
            return FALSE;
        }
    }

    if (pRes == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
        // __LINE__: 2289
        UTIL_ASSERT_MSG(FALSE, "CVidCell::Render m_pFrame = GARBAGE");
        return FALSE;
    }

    CPoint pt(nRefPtX - m_pFrame->nCenterX, nRefPtY - m_pFrame->nCenterY);

    BOOL bResult;
    switch (g_pChitin->cVideo.m_nBpp) {
    case 16:
        bResult = Blt8To16(pSurface,
            lPitch,
            pt,
            rClip,
            dwFlags,
            ptSource);
        break;
    case 24:
        bResult = Blt8To24(reinterpret_cast<BYTE*>(pSurface),
            lPitch,
            pt,
            rClip,
            dwFlags,
            ptSource);
        break;
    case 32:
        bResult = Blt8To32(reinterpret_cast<DWORD*>(pSurface),
            lPitch,
            pt,
            rClip,
            dwFlags,
            ptSource,
            0);
        break;
    default:
        bResult = FALSE;
        break;
    }

    m_pFrame = NULL;

    if (!bDemanded) {
        pRes->Release();
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 2323
    UTIL_ASSERT(bResult);

    return bResult;
}

// 0x7AF1C0
BOOL CVidCell::sub_7AF1C0(WORD* pSurface, LONG lPitch, DWORD dwFlags)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7AF3C0
BOOL CVidCell::Blt8To16(WORD* pSurface, LONG lPitch, const CPoint& pt, const CRect& rClip, DWORD dwFlags, const CPoint& ptSource)
{
    INT nFrameWidth = m_pFrame->nWidth;
    INT pos = 0;
    INT nBltHeight = m_pFrame->nHeight - ptSource.y;
    INT nBltWidth = nFrameWidth - ptSource.x;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 2624
    UTIL_ASSERT(ptSource.x >= 0 && ptSource.y >= 0);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 2625
    UTIL_ASSERT(rClip.top >= 0 && rClip.top <= rClip.bottom && rClip.left >= 0 && rClip.left <= rClip.right);

    if (pt.y > rClip.bottom
        || pt.x > rClip.right
        || pt.y + nBltHeight < rClip.top
        || pt.x + nBltWidth < rClip.left) {
        return TRUE;
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    lPitch >>= 1;
    WORD* pDest = pSurface + lPitch * pt.y + pt.x;

    if (!pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        INT nSrcJump = ptSource.x;
        INT nSrc = m_pFrame->nWidth * ptSource.y + ptSource.x;
        INT nBltH = nBltHeight;

        if (pt.y < rClip.top) {
            pDest += (rClip.top - pt.y) * lPitch;
            nSrc += (rClip.top - pt.y) * m_pFrame->nWidth;
            nBltH = pt.y - rClip.top + nBltHeight;
        }

        if (nBltHeight + pt.y > rClip.bottom) {
            nBltH += rClip.bottom - pt.y - nBltHeight;
        }

        INT nBltW = nBltWidth;
        if (pt.x < rClip.left) {
            nSrcJump = ptSource.x + rClip.left - pt.x;
            nBltW = pt.x - rClip.left + nBltWidth;
            nSrc += rClip.left - pt.x;
            pDest += rClip.left - pt.x;
        }

        if (nBltWidth + pt.x > rClip.right) {
            nSrcJump += pt.x - rClip.right + nBltWidth;
            nBltW += rClip.right - pt.x - nBltWidth;
        }

        for (INT y = nBltH; y > 0; y--) {
            for (INT x = nBltW; x > 0; x--) {
                if (pFrameData[nSrc] != nTransparentColor) {
                    *pDest = static_cast<WORD>(CVidImage::rgbTempPal[pFrameData[nSrc]]);
                }
                pDest++;
                nSrc++;
            }
            pDest += lPitch - nBltW;
            nSrc += nSrcJump;
        }
    } else {
        INT nRunLength = 0;
        INT nRemaining;
        INT nStep;

        // Skip the source rows above ptSource.
        for (INT y = ptSource.y; y > 0; y--) {
            nRemaining = nFrameWidth;
            while (nRemaining > 0) {
                if (pFrameData[pos] == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemaining == nRunLength) {
                        nRunLength = 0;
                        pos += 2;
                        break;
                    }

                    if (nRemaining <= nRunLength) {
                        nRunLength -= nRemaining;
                        break;
                    }

                    pos += 2;
                    nStep = -nRunLength;
                    nRunLength = 0;
                } else {
                    pos++;
                    nStep = -1;
                }
                nRemaining += nStep;
            }
        }

        // Skip the rows above the clip.
        INT nSkipRows = pt.y < rClip.top ? rClip.top - pt.y : 0;
        for (INT y = nSkipRows; y > 0; y--) {
            nRemaining = nFrameWidth;
            while (nRemaining > 0) {
                if (pFrameData[pos] == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemaining == nRunLength) {
                        nRunLength = 0;
                        pDest += nRemaining;
                        pos += 2;
                        break;
                    }

                    if (nRemaining <= nRunLength) {
                        pDest += nRemaining;
                        nRunLength -= nRemaining;
                        break;
                    }

                    pos += 2;
                    nStep = -nRunLength;
                    pDest += nRunLength;
                    nRunLength = 0;
                } else {
                    pDest++;
                    pos++;
                    nStep = -1;
                }
                nRemaining += nStep;
            }
            pDest += lPitch - nFrameWidth;
        }

        SHORT nLeft = static_cast<SHORT>(rClip.left) - static_cast<SHORT>(pt.x);
        INT nSkipLeft = nLeft < 0 ? 0 : nLeft;
        if (nBltWidth != nSkipLeft) {
            INT nRight = nBltWidth;
            if (rClip.right - pt.x <= nBltWidth) {
                nRight = rClip.right - pt.x;
            }

            INT nRows = nBltHeight - nSkipRows;
            if (nRows < 0) {
                nRows = 0;
            }
            if (nRows > rClip.bottom - pt.y) {
                nRows = rClip.bottom - pt.y;
            }

            for (; nRows > 0; nRows--) {
                // Skip the source columns left of ptSource.
                nRemaining = ptSource.x;
                while (nRemaining > 0) {
                    if (pFrameData[pos] == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = pFrameData[pos + 1] + 1;
                        }

                        if (nRemaining == nRunLength) {
                            nRunLength = 0;
                            pos += 2;
                            break;
                        }

                        if (nRemaining <= nRunLength) {
                            nRunLength -= nRemaining;
                            break;
                        }

                        pos += 2;
                        nStep = -nRunLength;
                        nRunLength = 0;
                    } else {
                        pos++;
                        nStep = -1;
                    }
                    nRemaining += nStep;
                }

                // The row: pixels from nSkipLeft to nRight are drawn.
                nRemaining = nBltWidth;
                while (nRemaining > 0) {
                    if (pFrameData[pos] == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = pFrameData[pos + 1] + 1;
                        }

                        if (nRemaining == nRunLength) {
                            nRunLength = 0;
                            pDest += nRemaining;
                            pos += 2;
                            break;
                        }

                        if (nRemaining <= nRunLength) {
                            pDest += nRemaining;
                            nRunLength -= nRemaining;
                            break;
                        }

                        pos += 2;
                        nStep = -nRunLength;
                        pDest += nRunLength;
                        nRunLength = 0;
                    } else {
                        if (nRemaining <= nBltWidth - nSkipLeft && nBltWidth - nRight < nRemaining) {
                            *pDest = static_cast<WORD>(CVidImage::rgbTempPal[pFrameData[pos]]);
                        }
                        pDest++;
                        pos++;
                        nStep = -1;
                    }
                    nRemaining += nStep;
                }

                pDest += lPitch - nBltWidth;
            }
        }
    }

    return TRUE;
}

// The clear-shadow 16-bit blit of the whole frame at the surface pointer: the
// 8-bit frame's indices through the realized 16-bit palette (one WORD a pixel),
// the transparent index skipped, rows bottom-up by lPitch in bytes. With the
// cell's shadow on, index 1 is not a palette entry but half the destination
// (per channel through the mode's masks); with it off, the shadow entry was
// replaced by the mode's shadow colour and the frame is drawn plainly. The
// 16-bit twin of Blt8To32ClearShadow. NOTE: The survey (report-T50.md) names it
// Blt8To16ClearShadow after its dispatcher role; the export has only the
// address for it, so the code keeps the address-derived name the stub had and
// the dispatcher's call sites stay untouched.
//
// 0x7AF8A0
BOOL CVidCell::sub_7AF8A0(WORD* pSurface, LONG lPitch, DWORD dwFlags)
{
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    // NOTE: The original has no zero-size guard at all; its do-while loops
    // would spin on an empty frame (Render never passes one). The other fast
    // 16-bit blits return TRUE there and so does this.
    if (nWidth == 0 || nHeight == 0) {
        return TRUE;
    }

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 16, dwFlags, &m_paletteAffects, 0xFF);

    // Half of every channel with the bit shifted into the next one cleared.
    DWORD dwShadowMask = 0;

    if (!m_bShadowOn) {
        // Only the low word of the entry is written (mov word ptr at 0x7AF932).
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] =
            (CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] & 0xFFFF0000u)
            | (static_cast<DWORD>(g_pChitin->GetCurrentVideoMode()->field_24) & 0xFFFF);
    } else {
        CVidMode* pVidMode = g_pChitin->GetCurrentVideoMode();
        dwShadowMask = ((pVidMode->m_dwRBitMask >> 1) & pVidMode->m_dwRBitMask)
            + ((pVidMode->m_dwGBitMask >> 1) & pVidMode->m_dwGBitMask)
            + ((pVidMode->m_dwBBitMask >> 1) & pVidMode->m_dwBBitMask);
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    // NOTE: The pitch stays in bytes; the original computes the rest of a row
    // once (lPitch - width * 2) and adds it after each row's pixels.
    LONG lRowStep = lPitch - nWidth * 2;

    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        int nRunLength = 0;
        int pos = 0;

        for (int y = 0; y < nHeight; y++) {
            int nRemainingWidth = nWidth;

            while (nRemainingWidth != 0) {
                BYTE nColor = pFrameData[pos];

                if (nColor == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemainingWidth == nRunLength) {
                        pos += 2;

                        pSurface += nRemainingWidth;
                        nRunLength = 0;
                        nRemainingWidth = 0;
                    } else if (nRemainingWidth > nRunLength) {
                        pos += 2;

                        pSurface += nRunLength;
                        nRemainingWidth -= nRunLength;
                        nRunLength = 0;
                    } else {
                        pSurface += nRemainingWidth;
                        nRunLength -= nRemainingWidth;
                        nRemainingWidth = 0;
                    }
                } else {
                    pos++;

                    if (m_bShadowOn && nColor == CVidPalette::SHADOW_ENTRY) {
                        *pSurface = static_cast<WORD>((*pSurface >> 1) & dwShadowMask);
                    } else {
                        // NOTE: With the shadow off the original's compressed
                        // branch halves the palette value with its local half
                        // mask, which it only computes when the shadow is on
                        // (an uninitialized read at 0x7AFA6C); the 32-bit
                        // sibling writes the realized palette there, as this
                        // does (the shadow entry is the mode's colour).
                        *pSurface = static_cast<WORD>(CVidImage::rgbTempPal[nColor]);
                    }

                    pSurface++;
                    nRemainingWidth--;
                }
            }

            pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + lRowStep);
        }
    } else {
        for (int y = 0; y < nHeight; y++) {
            for (int x = 0; x < nWidth; x++) {
                BYTE nColor = *pFrameData;
                if (nColor != nTransparentColor) {
                    // NOTE: The raw walk halves for any index below two (cmp
                    // eax,1; ja at 0x7AFADB), where the compressed one halves
                    // only index 1.
                    if (m_bShadowOn && nColor < 2) {
                        *pSurface = static_cast<WORD>((*pSurface >> 1) & dwShadowMask);
                    } else {
                        *pSurface = static_cast<WORD>(CVidImage::rgbTempPal[nColor]);
                    }
                }
                pFrameData++;
                pSurface++;
            }
            pSurface = reinterpret_cast<WORD*>(reinterpret_cast<BYTE*>(pSurface) + lRowStep);
        }
    }

    return TRUE;
}

// 0x7AFB50
BOOL CVidCell::sub_7AFB50(WORD* pSurface, LONG lPitch, DWORD dwFlags)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7AFED0
BOOL CVidCell::sub_7AFED0(WORD* pSurface, LONG lPitch, DWORD dwFlags, INT nTransVal)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7B0490
void CVidCell::BltBackGroundToFx(LPDIRECTDRAWSURFACE pSurface, CRect& rSrc, const CRect& rClip, DWORD dwFlags)
{
    HRESULT hr;

    int destX = 0;
    int destY = 0;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 4908
    UTIL_ASSERT(m_nFxSurface != -1);

    CRect rSrcSurface(rSrc);

    if (rSrcSurface.left < rClip.left) {
        rSrcSurface.left = rClip.left;
        destX = rClip.left - rSrc.left;
    }

    if (rSrcSurface.top < rClip.top) {
        rSrcSurface.top = rClip.top;
        destY = rClip.top - rSrc.top;
    }

    if (rSrcSurface.right >= rClip.right) {
        rSrcSurface.right = rClip.right;
    }

    if (rSrcSurface.bottom >= rClip.bottom) {
        rSrcSurface.bottom = rClip.bottom;
    }

    if ((dwFlags & 0x10) != 0 || (dwFlags & 0x20) != 0) {
        DDBLTFX fx;
        fx.dwSize = sizeof(fx);
        fx.dwDDFX = 0;
        if ((dwFlags & 0x10) != 0) {
            fx.dwDDFX |= DDBLTFX_MIRRORLEFTRIGHT;
        }
        if ((dwFlags & 0x20) != 0) {
            fx.dwDDFX |= DDBLTFX_MIRRORUPDOWN;
        }

        do {
            RECT rDest;
            rDest.left = destX;
            rDest.top = destY;
            rDest.right = destX + rSrc.Width();
            rDest.bottom = destY + rSrc.Height();

            hr = g_pChitin->cVideo.cVidBlitter.Blt(g_pChitin->GetCurrentVideoMode()->pSurfaces[m_nFxSurface],
                &rDest,
                pSurface,
                &rSrc,
                DDBLT_WAIT | DDBLT_DDFX,
                &fx);
            g_pChitin->GetCurrentVideoMode()->CheckResults(hr);
        } while (hr == DDERR_SURFACELOST || hr == DDERR_WASSTILLDRAWING);
    } else if (rSrcSurface.top < rSrcSurface.bottom && rSrcSurface.left < rSrcSurface.right) {
        do {
            hr = g_pChitin->cVideo.cVidBlitter.BltFast(g_pChitin->GetCurrentVideoMode()->pSurfaces[m_nFxSurface],
                destX,
                destY,
                pSurface,
                rSrcSurface,
                DDBLTFAST_WAIT);
            g_pChitin->GetCurrentVideoMode()->CheckResults(hr);
            if (hr != DDERR_SURFACELOST && hr != DDERR_WASSTILLDRAWING) {
                break;
            }
        } while (!g_pChitin->field_1932);
    }
}

// 0x7B06E0
BOOL CVidCell::BltFromFX(LPDIRECTDRAWSURFACE pSurface, int x, int y, const CRect& rClip, DWORD dwFlags)
{
    HRESULT hr;

    int destX = x;
    int destY = y;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage.cpp
    // __LINE__: 5003
    UTIL_ASSERT(m_nFxSurface != -1);

    CRect rSrcSurface;
    rSrcSurface.left = 0;
    rSrcSurface.top = 0;
    rSrcSurface.right = m_pFrame->nWidth;
    rSrcSurface.bottom = m_pFrame->nHeight;

    if (x < rClip.left) {
        destX = rClip.left;
        rSrcSurface.left = rClip.left - x;
    }

    if (y < rClip.top) {
        destY = rClip.top;
        rSrcSurface.top = rClip.top - y;
    }

    if (x + m_pFrame->nWidth - 1 >= rClip.right) {
        rSrcSurface.right -= x + m_pFrame->nWidth - rClip.right;
    }

    if (y + m_pFrame->nHeight - 1 >= rClip.bottom) {
        rSrcSurface.bottom -= y + m_pFrame->nHeight - rClip.bottom;
    }

    if (rSrcSurface.bottom > rSrcSurface.top && rSrcSurface.right > rSrcSurface.left) {
        if ((dwFlags & 0x10) != 0 || (dwFlags & 0x20) != 0) {
            DDBLTFX fx;
            fx.dwSize = sizeof(fx);
            fx.dwDDFX = 0;
            if ((dwFlags & 0x10) != 0) {
                fx.dwDDFX |= DDBLTFX_MIRRORLEFTRIGHT;
            }
            if ((dwFlags & 0x20) != 0) {
                fx.dwDDFX |= DDBLTFX_MIRRORUPDOWN;
            }

            do {
                RECT rDest;
                rDest.left = destX;
                rDest.top = destY;
                rDest.right = rSrcSurface.right - rSrcSurface.left;
                rDest.bottom = rSrcSurface.bottom - rSrcSurface.top;

                // The colour key goes with the mirror (0x1008800 at
                // 0x7B090F).
                hr = g_pChitin->cVideo.cVidBlitter.Blt(pSurface,
                    &rDest,
                    g_pChitin->GetCurrentVideoMode()->pSurfaces[m_nFxSurface],
                    &rSrcSurface,
                    DDBLT_WAIT | DDBLT_KEYSRC | DDBLT_DDFX,
                    &fx);
                g_pChitin->GetCurrentVideoMode()->CheckResults(hr);
            } while (hr == DDERR_SURFACELOST || hr == DDERR_WASSTILLDRAWING);
        } else {
            do {
                hr = g_pChitin->cVideo.cVidBlitter.BltFast(pSurface,
                    destX,
                    destY,
                    g_pChitin->GetCurrentVideoMode()->pSurfaces[m_nFxSurface],
                    &rSrcSurface,
                    DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY);
                g_pChitin->GetCurrentVideoMode()->CheckResults(hr);
                if (hr != DDERR_SURFACELOST && hr != DDERR_WASSTILLDRAWING) {
                    break;
                }
            } while (!g_pChitin->field_1932);
        }
    }

    return TRUE;
}

// 0x7B0980
BOOL CVidCell::GetFrame(BOOLEAN bDemanded)
{
    if (pRes == NULL) {
        return FALSE;
    }

    if (!bDemanded) {
        if (pRes->Demand() == NULL) {
            return FALSE;
        }
    }

    BAMHEADER* pHeader;
    if (pRes->m_bCacheHeader) {
        pHeader = pRes->m_pBamHeaderCopy;
    } else {
        pHeader = pRes->m_pBamHeader;
    }

    // The sequence compares unsigned (jb at 0x7B09CA): a negative one is
    // reset too.
    if (!(static_cast<WORD>(m_nCurrentSequence) < pHeader->nSequences)) {
        m_nCurrentSequence = 0;
    }

    if (!(m_nCurrentFrame < pRes->m_pSequences[m_nCurrentSequence].nFrames)) {
        if (field_C8) {
            if (pRes->m_pSequences[m_nCurrentSequence].nFrames > 0) {
                m_nCurrentFrame = m_nCurrentFrame % pRes->m_pSequences[m_nCurrentSequence].nFrames;
            } else {
                m_nCurrentFrame = 0;
            }
        } else {
            m_nCurrentFrame = pRes->m_pSequences[m_nCurrentSequence].nFrames - 1;
        }
    }

    if (m_nCurrentFrame < 0) {
        if (field_C8) {
            if (pRes->m_pSequences[m_nCurrentSequence].nFrames > 0) {
                m_nCurrentFrame = pRes->m_pSequences[m_nCurrentSequence].nFrames + m_nCurrentFrame % pRes->m_pSequences[m_nCurrentSequence].nFrames;
                if (m_nCurrentFrame == pRes->m_pSequences[m_nCurrentSequence].nFrames) {
                    m_nCurrentFrame = 0;
                }
            } else {
                m_nCurrentFrame = 0;
            }
        } else {
            m_nCurrentFrame = 0;
        }
    }

    m_pFrame = pRes->GetFrame(m_nCurrentSequence, m_nCurrentFrame, m_bDoubleSize);

    return m_pFrame != NULL;
}

// 0x7C4120
void CVidCell::StoreBackground(INT x, INT y, const CRect& rClip, CRect& rStorage, BOOLEAN bNumbered)
{
    if (GetFrame(FALSE)) {
        rStorage.left = x - m_pFrame->nCenterX;
        rStorage.top = y - m_pFrame->nCenterY;

        if (bNumbered) {
            rStorage.right = max(rStorage.left + m_pFrame->nWidth, rStorage.left + m_pFrame->nCenterX + 16);
            rStorage.bottom = max(rStorage.top + m_pFrame->nHeight, rStorage.top + m_pFrame->nCenterY + 16);
        } else {
            rStorage.right = rStorage.left + m_pFrame->nWidth;
            rStorage.bottom = rStorage.top + m_pFrame->nHeight;
        }

        // NOTE: Looks like intersection.
        if (rStorage.left < rClip.left) {
            rStorage.left = rClip.left;
        }

        if (rStorage.top < rClip.top) {
            rStorage.top = rClip.top;
        }

        if (rStorage.right >= rClip.right) {
            rStorage.right = rClip.right;
        }

        if (rStorage.bottom >= rClip.bottom) {
            rStorage.bottom = rClip.bottom;
        }
    } else {
        rStorage.SetRect(0, 0, 0, 0);
    }
}

// The plain 24-bit blit of the whole frame at the surface pointer: the 8-bit
// frame through the realized 24-bit palette (three bytes a pixel, bottom-up by
// lPitch in bytes), the transparent index skipped. The 24-bit twin of the fast
// Blt8To32; unlike that one it has no nTransVal (the palette is realized with
// 255) and none of its field_174 block. NOTE: The survey (report-T50.md) names
// it Blt8To24 after its dispatcher role; the export has only the address for
// it, so the code keeps the address-derived name the stub had and the
// dispatcher's call sites stay untouched.
//
// 0x7CECD0
BOOL CVidCell::sub_7CECD0(BYTE* pSurface, LONG lPitch, DWORD dwFlags)
{
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    if (nWidth == 0 || nHeight == 0) {
        return TRUE;
    }

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 24, dwFlags, &m_paletteAffects, 0xFF);

    if (!m_bShadowOn) {
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = g_pChitin->GetCurrentVideoMode()->field_24;
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    // NOTE: The pitch stays in bytes; the original computes the rest of a row
    // once (lPitch - width * 3) and adds it after each row's pixels.
    LONG lRowStep = lPitch - nWidth * 3;

    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        int nRunLength = 0;
        int pos = 0;

        for (int y = 0; y < nHeight; y++) {
            int nRemainingWidth = nWidth;

            while (nRemainingWidth != 0) {
                BYTE nColor = pFrameData[pos];

                if (nColor == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemainingWidth == nRunLength) {
                        pos += 2;

                        pSurface += nRemainingWidth * 3;
                        nRunLength = 0;
                        nRemainingWidth = 0;
                    } else if (nRemainingWidth > nRunLength) {
                        pos += 2;

                        pSurface += nRunLength * 3;
                        nRemainingWidth -= nRunLength;
                        nRunLength = 0;
                    } else {
                        pSurface += nRemainingWidth * 3;
                        nRunLength -= nRemainingWidth;
                        nRemainingWidth = 0;
                    }
                } else {
                    pos++;

                    DWORD rgb = CVidImage::rgbTempPal[nColor];
                    *reinterpret_cast<WORD*>(pSurface) = static_cast<WORD>(rgb);
                    pSurface[2] = static_cast<BYTE>(rgb >> 16);
                    pSurface += 3;
                    nRemainingWidth--;
                }
            }

            pSurface += lRowStep;
        }
    } else {
        for (int y = 0; y < nHeight; y++) {
            for (int x = 0; x < nWidth; x++) {
                BYTE nColor = *pFrameData;
                if (nColor != nTransparentColor) {
                    DWORD rgb = CVidImage::rgbTempPal[nColor];
                    *reinterpret_cast<WORD*>(pSurface) = static_cast<WORD>(rgb);
                    pSurface[2] = static_cast<BYTE>(rgb >> 16);
                }
                pFrameData++;
                pSurface += 3;
            }
            pSurface += lRowStep;
        }
    }

    return TRUE;
}

// 0x7CEEB0
BOOL CVidCell::Blt8To24(BYTE* pSurface, LONG lPitch, const CPoint& pt, const CRect& rClip, DWORD dwFlags, const CPoint& ptSource)
{
    INT pos = 0;
    INT nFrameWidth = m_pFrame->nWidth;
    INT nBltHeight = m_pFrame->nHeight - ptSource.y;
    INT nBltWidth = nFrameWidth - ptSource.x;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage24.cpp
    // __LINE__: 3073
    UTIL_ASSERT(ptSource.x >= 0 && ptSource.y >= 0);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage24.cpp
    // __LINE__: 3074
    UTIL_ASSERT(rClip.top >= 0 && rClip.top <= rClip.bottom && rClip.left >= 0 && rClip.left <= rClip.right);

    if (pt.y > rClip.bottom
        || pt.x > rClip.right
        || pt.y + nBltHeight < rClip.top
        || pt.x + nBltWidth < rClip.left) {
        return TRUE;
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    BYTE* pDest = pSurface + pt.x * 3 + pt.y * lPitch;

    if (!pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        INT nSrcJump = ptSource.x;
        INT nSrc = m_pFrame->nWidth * ptSource.y + ptSource.x;
        INT nBltH = nBltHeight;

        if (pt.y < rClip.top) {
            nSrc += (rClip.top - pt.y) * m_pFrame->nWidth;
            pDest += (rClip.top - pt.y) * lPitch;
            nBltH = pt.y - rClip.top + nBltHeight;
        }

        if (nBltHeight + pt.y > rClip.bottom) {
            nBltH += rClip.bottom - pt.y - nBltHeight;
        }

        INT nBltW = nBltWidth;
        if (pt.x < rClip.left) {
            nSrcJump = ptSource.x + rClip.left - pt.x;
            nBltW = pt.x - rClip.left + nBltWidth;
            pDest += (rClip.left - pt.x) * 3;
            nSrc += rClip.left - pt.x;
        }

        if (nBltWidth + pt.x > rClip.right) {
            nSrcJump += pt.x - rClip.right + nBltWidth;
            nBltW += rClip.right - pt.x - nBltWidth;
        }

        for (INT y = nBltH; y > 0; y--) {
            for (INT x = nBltW; x > 0; x--) {
                if (pFrameData[nSrc] != nTransparentColor) {
                    *reinterpret_cast<WORD*>(pDest) = static_cast<WORD>(CVidImage::rgbTempPal[pFrameData[nSrc]]);
                    pDest[2] = static_cast<BYTE>(CVidImage::rgbTempPal[pFrameData[nSrc]] >> 16);
                }
                pDest += 3;
                nSrc++;
            }
            pDest += lPitch - nBltW * 3;
            nSrc += nSrcJump;
        }
    } else {
        // The compressed walk counts in bytes of the destination, three per
        // pixel.
        INT nRunLength = 0;
        INT nRemaining;
        INT nStep;

        // Skip the source rows above ptSource.
        for (INT y = ptSource.y; y > 0; y--) {
            nRemaining = nFrameWidth * 3;
            while (nRemaining > 0) {
                if (pFrameData[pos] == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = (pFrameData[pos + 1] + 1) * 3;
                    }

                    if (nRemaining == nRunLength) {
                        nRunLength = 0;
                        pos += 2;
                        break;
                    }

                    if (nRemaining <= nRunLength) {
                        nRunLength -= nRemaining;
                        break;
                    }

                    pos += 2;
                    nStep = -nRunLength;
                    nRunLength = 0;
                } else {
                    pos++;
                    nStep = -3;
                }
                nRemaining += nStep;
            }
        }

        // Skip the rows above the clip.
        INT nSkipRows = pt.y < rClip.top ? rClip.top - pt.y : 0;
        for (INT y = nSkipRows; y > 0; y--) {
            nRemaining = nFrameWidth * 3;
            while (nRemaining > 0) {
                if (pFrameData[pos] == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = (pFrameData[pos + 1] + 1) * 3;
                    }

                    if (nRemaining == nRunLength) {
                        pDest += nRemaining;
                        nRunLength = 0;
                        pos += 2;
                        break;
                    }

                    if (nRemaining <= nRunLength) {
                        pDest += nRemaining;
                        nRunLength -= nRemaining;
                        break;
                    }

                    pos += 2;
                    nStep = -nRunLength;
                    pDest += nRunLength;
                    nRunLength = 0;
                } else {
                    pDest += 3;
                    pos++;
                    nStep = -3;
                }
                nRemaining += nStep;
            }
            pDest += lPitch - nFrameWidth * 3;
        }

        SHORT nLeft = static_cast<SHORT>(rClip.left) - static_cast<SHORT>(pt.x);
        INT nSkipLeft = nLeft < 0 ? 0 : nLeft;
        INT nDrawEnd = (nBltWidth - nSkipLeft) * 3;
        if (nDrawEnd != 0) {
            INT nRight = nBltWidth;
            if (rClip.right - pt.x <= nBltWidth) {
                nRight = rClip.right - pt.x;
            }

            INT nRows = nBltHeight - nSkipRows;
            if (nRows < 0) {
                nRows = 0;
            }
            if (nRows > rClip.bottom - pt.y) {
                nRows = rClip.bottom - pt.y;
            }

            for (; nRows > 0; nRows--) {
                // Skip the source columns left of ptSource.
                nRemaining = ptSource.x * 3;
                while (nRemaining > 0) {
                    if (pFrameData[pos] == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = (pFrameData[pos + 1] + 1) * 3;
                        }

                        if (nRemaining == nRunLength) {
                            nRunLength = 0;
                            pos += 2;
                            break;
                        }

                        if (nRemaining <= nRunLength) {
                            nRunLength -= nRemaining;
                            break;
                        }

                        pos += 2;
                        nStep = -nRunLength;
                        nRunLength = 0;
                    } else {
                        pos++;
                        nStep = -3;
                    }
                    nRemaining += nStep;
                }

                // The row: pixels from nSkipLeft to nRight are drawn.
                nRemaining = nBltWidth * 3;
                while (nRemaining > 0) {
                    if (pFrameData[pos] == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = (pFrameData[pos + 1] + 1) * 3;
                        }

                        if (nRemaining == nRunLength) {
                            pDest += nRemaining;
                            nRunLength = 0;
                            pos += 2;
                            break;
                        }

                        if (nRemaining <= nRunLength) {
                            pDest += nRemaining;
                            nRunLength -= nRemaining;
                            break;
                        }

                        pos += 2;
                        nStep = -nRunLength;
                        pDest += nRunLength;
                        nRunLength = 0;
                    } else {
                        if (nRemaining <= nDrawEnd && (nBltWidth - nRight) * 3 < nRemaining) {
                            *reinterpret_cast<WORD*>(pDest) = static_cast<WORD>(CVidImage::rgbTempPal[pFrameData[pos]]);
                            pDest[2] = static_cast<BYTE>(CVidImage::rgbTempPal[pFrameData[pos]] >> 16);
                        }
                        pDest += 3;
                        pos++;
                        nStep = -3;
                    }
                    nRemaining += nStep;
                }

                pDest += lPitch - nBltWidth * 3;
            }
        }
    }

    return TRUE;
}

// 0x7CF3F0
BOOL CVidCell::sub_7CF3F0(BYTE* pSurface, LONG lPitch, DWORD dwFlags)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7CF650
BOOL CVidCell::sub_7CF650(BYTE* pSurface, LONG lPitch, DWORD dwFlags)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7CF8D0
BOOL CVidCell::sub_7CF8D0(BYTE* pSurface, LONG lPitch, DWORD dwFlags, INT nTransVal)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7CFBC0
BOOL CVidCell::Blt8To32(DWORD* pSurface, LONG lPitch, DWORD dwFlags, INT nTransVal)
{
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    if (nWidth == 0 || nHeight == 0) {
        return TRUE;
    }

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags, &m_paletteAffects, nTransVal);

    if (!m_bShadowOn) {
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = g_pChitin->GetCurrentVideoMode()->field_24;
    }

    if (g_pChitin->field_174) {
        if (m_nCurrentFrame == 0) {
            for (int index = CVidPalette::SHADOW_ENTRY + 1; index < 256; index++) {
                CVidImage::rgbTempPal[index] = g_pChitin->GetCurrentVideoMode()->ConvertToSurfaceRGB(255);
            }
        }
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    // NOTE: Original code is slightly different, but does the same thing.
    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        int nRunLength = 0;
        int pos = 0;
        for (int y = 0; y < nHeight; y++) {
            int nRemainingWidth = nWidth;
            while (nRemainingWidth != 0) {
                BYTE nColor = pFrameData[pos];
                if (nColor == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemainingWidth == nRunLength) {
                        pos += 2;

                        pSurface += nRemainingWidth;
                        nRunLength = 0;
                        nRemainingWidth = 0;
                    } else if (nRemainingWidth > nRunLength) {
                        pos += 2;

                        pSurface += nRunLength;
                        nRemainingWidth -= nRunLength;
                        nRunLength = 0;
                    } else {
                        pSurface += nRemainingWidth;
                        nRunLength -= nRemainingWidth;
                        nRemainingWidth = 0;
                    }
                } else {
                    pos++;

                    *pSurface++ = CVidImage::rgbTempPal[nColor];
                    nRemainingWidth--;
                }
            }
            pSurface += lPitch / 4 - nWidth;
        }
    } else {
        for (int y = 0; y < nHeight; y++) {
            for (int x = 0; x < nWidth; x++) {
                BYTE nColor = *pFrameData;
                if (nColor != nTransparentColor) {
                    *pSurface = CVidImage::rgbTempPal[nColor];
                }
                pFrameData++;
                pSurface++;
            }
            pSurface += lPitch / 4 - nWidth;
        }
    }

    return TRUE;
}

// 0x7CFDF0
BOOL CVidCell::Blt8To32(DWORD* pSurface, LONG lPitch, const CPoint& pt, const CRect& rClip, DWORD dwFlags, const CPoint& ptSource, INT nTransVal)
{
    INT nFrameWidth = m_pFrame->nWidth;
    INT pos = 0;
    INT nBltHeight = m_pFrame->nHeight - ptSource.y;
    INT nBltWidth = nFrameWidth - ptSource.x;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage24.cpp
    // __LINE__: 5029
    UTIL_ASSERT(ptSource.x >= 0 && ptSource.y >= 0);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage24.cpp
    // __LINE__: 5030
    UTIL_ASSERT(rClip.top >= 0 && rClip.top <= rClip.bottom && rClip.left >= 0 && rClip.left <= rClip.right);

    if (pt.y > rClip.bottom
        || pt.x > rClip.right
        || pt.y + nBltHeight < rClip.top
        || pt.x + nBltWidth < rClip.left) {
        return TRUE;
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    lPitch >>= 2;
    DWORD* pDest = pSurface + lPitch * pt.y + pt.x;

    if (!pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        INT nSrcJump = ptSource.x;
        INT nSrc = m_pFrame->nWidth * ptSource.y + ptSource.x;
        INT nBltH = nBltHeight;

        if (pt.y < rClip.top) {
            pDest += (rClip.top - pt.y) * lPitch;
            nSrc += (rClip.top - pt.y) * m_pFrame->nWidth;
            nBltH = pt.y - rClip.top + nBltHeight;
        }

        if (nBltHeight + pt.y > rClip.bottom) {
            nBltH += rClip.bottom - pt.y - nBltHeight;
        }

        INT nBltW = nBltWidth;
        if (pt.x < rClip.left) {
            nSrcJump = ptSource.x + rClip.left - pt.x;
            nBltW = pt.x - rClip.left + nBltWidth;
            nSrc += rClip.left - pt.x;
            pDest += rClip.left - pt.x;
        }

        if (nBltWidth + pt.x > rClip.right) {
            nSrcJump += pt.x - rClip.right + nBltWidth;
            nBltW += rClip.right - pt.x - nBltWidth;
        }

        if (nTransVal == 0) {
            for (INT y = nBltH; y > 0; y--) {
                for (INT x = nBltW; x > 0; x--) {
                    if (pFrameData[nSrc] != nTransparentColor) {
                        *pDest = CVidImage::rgbTempPal[pFrameData[nSrc]];
                    }
                    pDest++;
                    nSrc++;
                }
                nSrc += nSrcJump;
                pDest += lPitch - nBltW;
            }
        } else {
            // The frame's indices scale the surface's alpha byte; a
            // transparent pixel clears it.
            for (INT y = nBltH; y > 0; y--) {
                for (INT x = nBltW; x > 0; x--) {
                    BYTE nColor = pFrameData[nSrc];
                    if (nColor == nTransparentColor) {
                        *pDest &= 0xFFFFFF;
                    } else {
                        *pDest = (*pDest & 0xFFFFFF) | (((*pDest >> 24) * nColor) << 16) & 0xFF000000;
                    }
                    pDest++;
                    nSrc++;
                }
                pDest += lPitch - nBltW;
                nSrc += nSrcJump;
            }
        }
    } else {
        INT nRunLength = 0;
        INT nRemaining;
        INT nStep;

        // Skip the source rows above ptSource.
        for (INT y = ptSource.y; y > 0; y--) {
            nRemaining = nFrameWidth;
            while (nRemaining > 0) {
                if (pFrameData[pos] == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemaining == nRunLength) {
                        nRunLength = 0;
                        pos += 2;
                        break;
                    }

                    if (nRemaining <= nRunLength) {
                        nRunLength -= nRemaining;
                        break;
                    }

                    pos += 2;
                    nStep = -nRunLength;
                    nRunLength = 0;
                } else {
                    pos++;
                    nStep = -1;
                }
                nRemaining += nStep;
            }
        }

        // Skip the rows above the clip.
        INT nSkipRows = pt.y < rClip.top ? rClip.top - pt.y : 0;
        for (INT y = nSkipRows; y > 0; y--) {
            nRemaining = nFrameWidth;
            while (nRemaining > 0) {
                if (pFrameData[pos] == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemaining == nRunLength) {
                        nRunLength = 0;
                        pDest += nRemaining;
                        pos += 2;
                        break;
                    }

                    if (nRemaining <= nRunLength) {
                        pDest += nRemaining;
                        nRunLength -= nRemaining;
                        break;
                    }

                    pos += 2;
                    nStep = -nRunLength;
                    pDest += nRunLength;
                    nRunLength = 0;
                } else {
                    pDest++;
                    pos++;
                    nStep = -1;
                }
                nRemaining += nStep;
            }
            pDest += lPitch - nFrameWidth;
        }

        SHORT nLeft = static_cast<SHORT>(rClip.left) - static_cast<SHORT>(pt.x);
        INT nSkipLeft = nLeft < 0 ? 0 : nLeft;
        INT nDrawEnd = nBltWidth - nSkipLeft;
        if (nDrawEnd != 0) {
            INT nRight = nBltWidth;
            if (rClip.right - pt.x <= nBltWidth) {
                nRight = rClip.right - pt.x;
            }
            INT nDrawStart = nBltWidth - nRight;

            INT nRows = nBltHeight - nSkipRows;
            if (nRows < 0) {
                nRows = 0;
            }
            if (nRows > rClip.bottom - pt.y) {
                nRows = rClip.bottom - pt.y;
            }

            for (; nRows > 0; nRows--) {
                // Skip the source columns left of ptSource.
                nRemaining = ptSource.x;
                while (nRemaining > 0) {
                    if (pFrameData[pos] == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = pFrameData[pos + 1] + 1;
                        }

                        if (nRemaining == nRunLength) {
                            nRunLength = 0;
                            pos += 2;
                            break;
                        }

                        if (nRemaining <= nRunLength) {
                            nRunLength -= nRemaining;
                            break;
                        }

                        pos += 2;
                        nStep = -nRunLength;
                        nRunLength = 0;
                    } else {
                        pos++;
                        nStep = -1;
                    }
                    nRemaining += nStep;
                }

                // The row: pixels whose remaining count lies in
                // (nDrawStart, nDrawEnd] are drawn, the clip's columns.
                nRemaining = nBltWidth;
                if (nTransVal != 0) {
                    while (nRemaining > 0) {
                        BYTE nColor = pFrameData[pos];
                        if (nColor == nTransparentColor) {
                            if (nRunLength == 0) {
                                nRunLength = pFrameData[pos + 1] + 1;
                            }

                            if (nRemaining == nRunLength) {
                                do {
                                    if (nRemaining <= nDrawEnd && nDrawStart < nRemaining) {
                                        *pDest &= 0xFFFFFF;
                                    }
                                    pDest++;
                                    nRemaining--;
                                } while (nRemaining > 0);
                                pos += 2;
                                nRunLength = 0;
                                continue;
                            }

                            if (nRemaining <= nRunLength) {
                                nRunLength -= nRemaining;
                                for (; nRemaining > 0; nRemaining--) {
                                    if (nRemaining <= nDrawEnd && nDrawStart < nRemaining) {
                                        reinterpret_cast<BYTE*>(pDest)[3] = 0;
                                    }
                                    pDest++;
                                }
                                break;
                            }

                            pos += 2;
                            for (; nRunLength > 0; nRunLength--) {
                                if (nRemaining <= nDrawEnd && nDrawStart < nRemaining) {
                                    *pDest &= 0xFFFFFF;
                                }
                                pDest++;
                                nRemaining--;
                            }
                        } else {
                            if (nRemaining <= nDrawEnd && nDrawStart < nRemaining) {
                                *pDest = (*pDest & 0xFFFFFF) | (((*pDest >> 24) * nColor) << 16) & 0xFF000000;
                            }
                            pDest++;
                            pos++;
                            nRemaining--;
                        }
                    }
                } else {
                    while (nRemaining > 0) {
                        if (pFrameData[pos] == nTransparentColor) {
                            if (nRunLength == 0) {
                                nRunLength = pFrameData[pos + 1] + 1;
                            }

                            if (nRemaining == nRunLength) {
                                nRunLength = 0;
                                pDest += nRemaining;
                                pos += 2;
                                break;
                            }

                            if (nRemaining <= nRunLength) {
                                pDest += nRemaining;
                                nRunLength -= nRemaining;
                                break;
                            }

                            pos += 2;
                            nStep = -nRunLength;
                            pDest += nRunLength;
                            nRunLength = 0;
                        } else {
                            if (nRemaining <= nDrawEnd && nDrawStart < nRemaining) {
                                *pDest = CVidImage::rgbTempPal[pFrameData[pos]];
                            }
                            pDest++;
                            pos++;
                            nStep = -1;
                        }
                        nRemaining += nStep;
                    }
                }

                pDest += lPitch - nBltWidth;
            }
        }
    }

    return TRUE;
}

// 0x7D0470
BOOL CVidCell::Blt8To32ClearShadow(DWORD* pSurface, LONG lPitch, DWORD dwFlags)
{
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags, &m_paletteAffects, 255);

    if (!m_bShadowOn) {
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = g_pChitin->GetCurrentVideoMode()->field_24;
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);
    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    if (nWidth == 0 || nHeight == 0) {
        return FALSE;
    }

    // NOTE: Original code is slightly different, but does the same thing.
    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        if (m_bShadowOn) {
            int nRunLength = 0;
            int pos = 0;
            for (int y = 0; y < nHeight; y++) {
                int nRemainingWidth = nWidth;
                while (nRemainingWidth != 0) {
                    BYTE nColor = pFrameData[pos];
                    if (nColor == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = pFrameData[pos + 1] + 1;
                        }

                        if (nRemainingWidth == nRunLength) {
                            pos += 2;

                            pSurface += nRemainingWidth;
                            nRunLength = 0;
                            nRemainingWidth = 0;
                        } else if (nRemainingWidth > nRunLength) {
                            pos += 2;

                            pSurface += nRunLength;
                            nRemainingWidth -= nRunLength;
                            nRunLength = 0;
                        } else {
                            pSurface += nRemainingWidth;
                            nRunLength -= nRemainingWidth;
                            nRemainingWidth = 0;
                        }
                    } else if (nColor == 1) {
                        pos++;

                        *pSurface++ = (*pSurface / 2) & 0x7F7F7F;
                        nRemainingWidth--;
                    } else {
                        pos++;

                        *pSurface++ = CVidImage::rgbTempPal[nColor];
                        nRemainingWidth--;
                    }
                }
                pSurface += lPitch / 4 - nWidth;
            }
        } else {
            int nRunLength = 0;
            int pos = 0;
            for (int y = 0; y < nHeight; y++) {
                int nRemainingWidth = nWidth;
                while (nRemainingWidth != 0) {
                    BYTE nColor = pFrameData[pos];
                    if (nColor == nTransparentColor) {
                        if (nRunLength == 0) {
                            nRunLength = pFrameData[pos + 1] + 1;
                        }

                        if (nRemainingWidth == nRunLength) {
                            pos += 2;

                            pSurface += nRemainingWidth;
                            nRunLength = 0;
                            nRemainingWidth = 0;
                        } else if (nRemainingWidth > nRunLength) {
                            pos += 2;

                            pSurface += nRunLength;
                            nRemainingWidth -= nRunLength;
                            nRunLength = 0;
                        } else {
                            pSurface += nRemainingWidth;
                            nRunLength -= nRemainingWidth;
                            nRemainingWidth = 0;
                        }
                    } else {
                        pos++;

                        *pSurface++ = CVidImage::rgbTempPal[nColor];
                        nRemainingWidth--;
                    }
                }
                pSurface += lPitch / 4 - nWidth;
            }
        }
    } else {
        for (int y = 0; y < nHeight; y++) {
            for (int x = 0; x < nWidth; x++) {
                BYTE nColor = *pFrameData;
                if (nColor != nTransparentColor) {
                    if (m_bShadowOn && nColor == 1) {
                        *pSurface = (*pSurface / 2) & 0x7F7F7F;
                    } else {
                        *pSurface = CVidImage::rgbTempPal[nColor];
                    }
                }
                pFrameData++;
                pSurface++;
            }
            pSurface += lPitch / 4 - nWidth;
        }
    }

    return TRUE;
}

// 0x7D0730
BOOL CVidCell::Blt8To32Brightest(DWORD* pSurface, LONG lPitch, DWORD dwFlags)
{
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    if (nWidth == 0 || nHeight == 0) {
        return TRUE;
    }

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags, &m_paletteAffects, 255);

    if (!m_bShadowOn) {
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = g_pChitin->GetCurrentVideoMode()->field_24;
    }

    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    // The original is hand-written assembly (0x7D0837 to 0x7D093A): each
    // channel byte takes the larger of the surface's and the palette's, and
    // the surface's top byte is kept.
    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        int nRunLength = 0;
        int pos = 0;
        for (int y = 0; y < nHeight; y++) {
            int nRemainingWidth = nWidth;
            while (nRemainingWidth != 0) {
                BYTE nColor = pFrameData[pos];
                if (nColor == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemainingWidth == nRunLength) {
                        pos += 2;

                        pSurface += nRemainingWidth;
                        nRunLength = 0;
                        nRemainingWidth = 0;
                    } else if (nRemainingWidth > nRunLength) {
                        pos += 2;

                        pSurface += nRunLength;
                        nRemainingWidth -= nRunLength;
                        nRunLength = 0;
                    } else {
                        pSurface += nRemainingWidth;
                        nRunLength -= nRemainingWidth;
                        nRemainingWidth = 0;
                    }
                } else {
                    pos++;

                    // NOTE: Not present in uncompressed data branch.
                    if ((CVidImage::rgbTempPal[nColor] & 0xF0F0F0) != 0) {
                        *pSurface = (*pSurface & 0xFF000000)
                            | RGB(max(GetRValue(*pSurface), GetRValue(CVidImage::rgbTempPal[nColor])),
                                max(GetGValue(*pSurface), GetGValue(CVidImage::rgbTempPal[nColor])),
                                max(GetBValue(*pSurface), GetBValue(CVidImage::rgbTempPal[nColor])));
                    }

                    pSurface++;
                    nRemainingWidth--;
                }
            }
            pSurface += lPitch / 4 - nWidth;
        }
    } else {
        for (int y = 0; y < nHeight; y++) {
            for (int x = 0; x < nWidth; x++) {
                BYTE nColor = *pFrameData;
                if (nColor != nTransparentColor) {
                    *pSurface = (*pSurface & 0xFF000000)
                        | RGB(max(GetRValue(*pSurface), GetRValue(CVidImage::rgbTempPal[nColor])),
                            max(GetGValue(*pSurface), GetGValue(CVidImage::rgbTempPal[nColor])),
                            max(GetBValue(*pSurface), GetBValue(CVidImage::rgbTempPal[nColor])));
                }
                pFrameData++;
                pSurface++;
            }
            pSurface += lPitch / 4 - nWidth;
        }
    }

    return TRUE;
}

namespace {

// The bytes from the first row's start to the last row's end of a block of
// nHeight rows of nWidth pixels, lPitch bytes apart, ordered.
CVidCell::SurfaceRange RangeOf(const void* pFirstPixel, LONG lPitch, int nWidth, int nHeight)
{
    if (pFirstPixel == NULL || nWidth <= 0 || nHeight <= 0) {
        return CVidCell::SurfaceRange { NULL, NULL };
    }

    const BYTE* pFirst = static_cast<const BYTE*>(pFirstPixel);
    const BYTE* pLastRow = pFirst + static_cast<ptrdiff_t>(lPitch) * (nHeight - 1);
    return pFirst <= pLastRow ? CVidCell::SurfaceRange { pFirst, pLastRow + nWidth * 4 }
                              : CVidCell::SurfaceRange { pLastRow, pFirst + nWidth * 4 };
}

// When range is unknown (null), the frame's footprint at pFrame.
CVidCell::SurfaceRange Bounded(const CVidCell::SurfaceRange& range, const DWORD* pFrame, LONG lPitch, int nWidth, int nHeight)
{
    if (range.pBegin != NULL) {
        return range;
    }

    return RangeOf(pFrame, lPitch, nWidth, nHeight);
}

bool Writable(const CVidCell::SurfaceRange& range, const DWORD* pPixel)
{
    const BYTE* p = reinterpret_cast<const BYTE*>(pPixel);
    return reinterpret_cast<uintptr_t>(p) >= reinterpret_cast<uintptr_t>(range.pBegin)
        && reinterpret_cast<uintptr_t>(p) + 4 <= reinterpret_cast<uintptr_t>(range.pEnd);
}

}

// The locked back or FX surface that CVidInf::BKRender and FXRender pass
// to CVidCell::Render(pSurface, ...): the current video mode's locked
// description when pSurface is its start, otherwise unknown.
CVidCell::SurfaceRange CVidCell::SurfaceExtent(const void* pSurface, LONG lPitch)
{
    CVidInf* pVidInf = static_cast<CVidInf*>(g_pChitin->GetCurrentVideoMode());
    if (pVidInf != NULL
        && pVidInf->m_SurfaceDesc.lpSurface == pSurface
        && pVidInf->m_SurfaceDesc.lPitch == lPitch) {
        return RangeOf(pSurface,
            lPitch,
            static_cast<int>(pVidInf->m_SurfaceDesc.dwWidth),
            static_cast<int>(pVidInf->m_SurfaceDesc.dwHeight));
    }

    return SurfaceRange { NULL, NULL };
}

CVidCell::SurfaceRange CVidCell::SurfaceExtent(const DDSURFACEDESC& ddsd)
{
    return RangeOf(ddsd.lpSurface, ddsd.lPitch, static_cast<int>(ddsd.dwWidth), static_cast<int>(ddsd.dwHeight));
}

// 0x7D0950
BOOL CVidCell::sub_7D0950(DWORD* pSurface, LONG lPitch, DWORD dwFlags, INT nTransVal, const SurfaceRange& range)
{
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    if (nHeight == 0 || nWidth == 0) {
        return TRUE;
    }

    DWORD rgbShadow = g_pChitin->GetCurrentVideoMode()->field_24;
    DWORD nInverse = 256 - nTransVal;

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage24.cpp
    // __LINE__: 6515
    UTIL_ASSERT(nTransVal <= 256);

    // In bytes.
    LONG nRowJump = lPitch - nWidth * 4;

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags, &m_paletteAffects, 255);

    if (!m_bShadowOn) {
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = rgbShadow;
    }

    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    // NOTE: The original is hand-written assembly; the run decoding below
    // walks the same pixels.
    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        int nRunLength = 0;
        int pos = 0;
        for (int y = 0; y < nHeight; y++) {
            int nRemainingWidth = nWidth;
            while (nRemainingWidth != 0) {
                BYTE nColor = pFrameData[pos];
                if (nColor == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemainingWidth == nRunLength) {
                        pos += 2;

                        pSurface += nRemainingWidth;
                        nRunLength = 0;
                        nRemainingWidth = 0;
                    } else if (nRemainingWidth > nRunLength) {
                        pos += 2;

                        pSurface += nRunLength;
                        nRemainingWidth -= nRunLength;
                        nRunLength = 0;
                    } else {
                        pSurface += nRemainingWidth;
                        nRunLength -= nRemainingWidth;
                        nRemainingWidth = 0;
                    }
                } else {
                    pos++;

                    // The shadow colour is drawn opaque, everything else
                    // blended; the top byte ends up clear.
                    DWORD rgb = CVidImage::rgbTempPal[nColor] & 0xFFFFFF;
                    if (rgb != rgbShadow) {
                        DWORD nDest = *pSurface;
                        DWORD nRed = ((rgb >> 16) & 0xFF) * nInverse + ((nDest >> 16) & 0xFF) * nTransVal;
                        DWORD nGreen = ((rgb >> 8) & 0xFF) * nInverse + ((nDest >> 8) & 0xFF) * nTransVal;
                        DWORD nBlue = (rgb & 0xFF) * nInverse + (nDest & 0xFF) * nTransVal;
                        rgb = ((nRed << 8) & 0xFFFF0000) | (nGreen & 0xFF00) | ((nBlue >> 8) & 0xFF);
                    }
                    *pSurface++ = rgb;
                    nRemainingWidth--;
                }
            }
            pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nRowJump);
        }
    } else {
        // Every pixel is blended (no shadow test) and keeps its top byte.
        //
        // NOTE: Ours skips the pixels outside the surface (or, when the
        // surface is unknown, the frame's footprint): with the original's
        // row stride (below) every row after the first lands outside the
        // frame, and the original writes past the surface (ruling P4-10).
        SurfaceRange rWritable = Bounded(range, pSurface, lPitch, nWidth, nHeight);
        int pos = 0;
        for (int y = nHeight; y > 0; y--) {
            for (int x = nWidth; x > 0; x--) {
                BYTE nColor = pFrameData[pos];
                if (nColor != nTransparentColor && Writable(rWritable, pSurface)) {
                    DWORD rgb = CVidImage::rgbTempPal[nColor];
                    DWORD nDest = *pSurface;
                    INT nRed = static_cast<INT>(((rgb >> 16) & 0xFF) * nInverse + ((nDest >> 16) & 0xFF) * nTransVal) >> 8;
                    INT nGreen = static_cast<INT>(((rgb >> 8) & 0xFF) * nInverse + ((nDest >> 8) & 0xFF) * nTransVal) >> 8;
                    INT nBlue = static_cast<INT>((rgb & 0xFF) * nInverse + (nDest & 0xFF) * nTransVal) >> 8;
                    *pSurface = (nDest & 0xFF000000)
                        | ((nRed & 0xFF) << 16)
                        | ((nGreen & 0xFF) << 8)
                        | (nBlue & 0xFF);
                }
                pSurface++;
                pos++;
            }
            // NOTE: The original steps the row jump, a byte count, in
            // pixels (0x7D0C3F): rows land four times too far apart.
            pSurface += nRowJump;
        }
    }

    return TRUE;
}

// 0x7B49D0
BOOL CVidCell::sub_7B49D0(WORD* pSurface, LONG lPitch, DWORD dwFlags)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7D0C60
BOOL CVidCell::sub_7D0C60(BYTE* pSurface, LONG lPitch, DWORD dwFlags)
{
    // TODO: Incomplete.

    return TRUE;
}

// 0x7D1190
BOOL CVidCell::sub_7D1190(DWORD* pSurface, LONG lPitch, DWORD dwFlags, const SurfaceRange& range)
{
    DWORD rgbPalette[256];
    int nWidth = m_pFrame->nWidth;
    int nHeight = m_pFrame->nHeight;

    if (nHeight == 0 || nWidth == 0) {
        return TRUE;
    }

    // In pixels.
    LONG nRowJump = (lPitch >> 2) - nWidth;

    BYTE* pFrameData = pRes->GetFrameData(m_pFrame, m_bDoubleSize);

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(rgbPalette, 32, dwFlags, &m_paletteAffects, 255);

    if (!m_bShadowOn) {
        rgbPalette[CVidPalette::SHADOW_ENTRY] = g_pChitin->GetCurrentVideoMode()->field_24;
    }

    BAMHEADER* pBamHeader = pRes->m_bCacheHeader
        ? pRes->m_pBamHeaderCopy
        : pRes->m_pBamHeader;
    BYTE nTransparentColor = pBamHeader->nTransparentColor;

    // NOTE: The original is hand-written assembly; the run decoding below
    // walks the same pixels.
    if (pRes->GetCompressed(m_pFrame, m_bDoubleSize)) {
        // In bytes.
        nRowJump = lPitch - nWidth * 4;

        int nRunLength = 0;
        int pos = 0;
        for (int y = 0; y < nHeight; y++) {
            int nRemainingWidth = nWidth;
            while (nRemainingWidth != 0) {
                BYTE nColor = pFrameData[pos];
                if (nColor == nTransparentColor) {
                    if (nRunLength == 0) {
                        nRunLength = pFrameData[pos + 1] + 1;
                    }

                    if (nRemainingWidth == nRunLength) {
                        pos += 2;

                        pSurface += nRemainingWidth;
                        nRunLength = 0;
                        nRemainingWidth = 0;
                    } else if (nRemainingWidth > nRunLength) {
                        pos += 2;

                        pSurface += nRunLength;
                        nRemainingWidth -= nRunLength;
                        nRunLength = 0;
                    } else {
                        pSurface += nRemainingWidth;
                        nRunLength -= nRemainingWidth;
                        nRemainingWidth = 0;
                    }
                } else {
                    pos++;

                    // Dark colours are skipped, bright ones drawn opaque,
                    // the rest blended by their brightness over pixels no
                    // brighter than that.
                    DWORD rgb = rgbPalette[nColor];
                    DWORD nSum = ((rgb >> 16) & 0xFF) + (rgb & 0xFF) + ((rgb >> 8) & 0xFF);
                    if (nSum > 0x81) {
                        if (nSum >= 0x28C) {
                            *pSurface = rgb;
                        } else {
                            DWORD nAlpha = (nSum * 0x2A) >> 7;
                            DWORD nDest = *pSurface;
                            DWORD nEighths = (nDest >> 3) & 0x1F1F1F;
                            BYTE nDestSum = static_cast<BYTE>(nEighths + (nEighths >> 8) + (nEighths >> 16));
                            if (nAlpha >= nDestSum) {
                                DWORD nInverse = nAlpha ^ 0xFF;
                                DWORD nSrc = rgb & 0xFFFFFF;
                                nDest &= 0xFFFFFF;
                                DWORD nRed = (((nSrc >> 16) * nAlpha + (nDest >> 16) * nInverse) << 8) & 0xFF0000;
                                DWORD nRedGreen = ((((rgb >> 8) & 0xFF) * nAlpha | nRed) + ((nDest >> 8) & 0xFF) * nInverse) & 0xFFFF00;
                                DWORD nBlue = (((rgb & 0xFF) * nAlpha + (nDest & 0xFF) * nInverse) >> 8) & 0xFF;
                                *pSurface = nRedGreen | nBlue;
                            }
                        }
                    }
                    pSurface++;
                    nRemainingWidth--;
                }
            }
            pSurface = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pSurface) + nRowJump);
        }
    } else {
        // NOTE: The original walks nHeight + 1 rows of nWidth + 1 pixels
        // with no transparency test (0x7D13ED), and the row jump is for
        // nWidth pixels, so each row starts one pixel further right. It also
        // stores the pixel's brightness in an unread global (0x8BDDF8).
        //
        // NOTE: Ours reads no frame data past the nWidth * nHeight bytes of
        // the frame and writes no pixel outside the surface (or, when the
        // surface is unknown, the frame's footprint); those pixels are
        // skipped (ruling P4-10).
        SurfaceRange rWritable = Bounded(range, pSurface, lPitch, nWidth, nHeight);
        int nFrameBytes = nWidth * nHeight;
        int pos = 0;
        if (nHeight >= 0) {
            for (int y = nHeight + 1; y != 0; y--) {
                if (nWidth >= 0) {
                    for (int x = nWidth + 1; x != 0; x--) {
                        if (pos >= nFrameBytes || !Writable(rWritable, pSurface)) {
                            pSurface++;
                            pos++;
                            continue;
                        }

                        DWORD rgb = rgbPalette[pFrameData[pos]];
                        DWORD nDest = *pSurface;
                        INT nDestBrightness = static_cast<INT>((nDest & 0xFF) + ((nDest >> 8) & 0xFF) + ((nDest >> 16) & 0xFF)) >> 3;
                        INT nBrightness = static_cast<INT>((rgb & 0xFF) + ((rgb >> 16) & 0xFF) + ((rgb >> 8) & 0xFF)) / 3;
                        if (nBrightness >= nDestBrightness) {
                            INT nInverse = 255 - nBrightness;
                            INT nBlue = (static_cast<INT>(nDest & 0xFF) * nInverse + static_cast<INT>(rgb & 0xFF) * nBrightness) >> 8;
                            INT nGreen = (static_cast<INT>((nDest >> 8) & 0xFF) * nInverse + static_cast<INT>((rgb >> 8) & 0xFF) * nBrightness) >> 8;
                            INT nRed = (static_cast<INT>((rgb >> 16) & 0xFF) * nBrightness + static_cast<INT>((nDest >> 16) & 0xFF) * nInverse) >> 8;
                            *pSurface = (nDest & 0xFF000000)
                                | ((nRed & 0xFF) << 16)
                                | ((nGreen & 0xFF) << 8)
                                | (nBlue & 0xFF);
                        }
                        pSurface++;
                        pos++;
                    }
                }
                pSurface += nRowJump;
            }
        }
    }

    return TRUE;
}

// 0x7C4240
void CVidCell::RenderTexture(INT x, INT y, const CRect& rFxRect, CSize dataPitch, const CRect& rClip, DWORD dwFlags)
{
    CSize texSize(dataPitch);
    float x1;
    float y1;
    float x2;
    float y2;
    float x3;
    float y3;
    float x4;
    float y4;

    if (x > rClip.right
        || y > rClip.bottom
        || x + rFxRect.Width() < rClip.left
        || y + rFxRect.Height() < rClip.top) {
        return;
    }

    x1 = static_cast<float>(x);
    y1 = static_cast<float>(y);
    x2 = static_cast<float>(x + rFxRect.Width());
    y2 = static_cast<float>(y + rFxRect.Height());

    if (g_pChitin->cVideo.field_13A) {
        CVideo3d::glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_RGBA,
            CVidInf::FX_WIDTH,
            CVidInf::FX_HEIGHT,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            CVideo3d::texImageData);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    } else {
        CVideo3d::glTexSubImage2D(GL_TEXTURE_2D,
            0,
            0,
            0,
            texSize.cx,
            texSize.cy,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            CVideo3d::texImageData);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    if ((dwFlags & 0x20) != 0) {
        y3 = static_cast<float>(rFxRect.bottom) / static_cast<float>(CVidInf::FX_HEIGHT);
        y4 = static_cast<float>(rFxRect.top) / static_cast<float>(CVidInf::FX_HEIGHT);

        if (y < rClip.top) {
            y1 = static_cast<float>(rClip.top);
            y3 -= static_cast<float>(rClip.top - y) / static_cast<float>(CVidInf::FX_HEIGHT);
        }

        if (y + rFxRect.Height() > rClip.bottom) {
            y2 = static_cast<float>(rClip.bottom);
            y4 += static_cast<float>(y + rFxRect.Height() - rClip.bottom) / static_cast<float>(CVidInf::FX_HEIGHT);
        }
    } else {
        y3 = static_cast<float>(rFxRect.top) / static_cast<float>(CVidInf::FX_HEIGHT);
        y4 = static_cast<float>(rFxRect.bottom) / static_cast<float>(CVidInf::FX_HEIGHT);

        if (y < rClip.top) {
            y1 = static_cast<float>(rClip.top);
            y3 = static_cast<float>(rClip.top - y) / static_cast<float>(CVidInf::FX_HEIGHT);
        }

        if (y + rFxRect.Height() > rClip.bottom) {
            y2 = static_cast<float>(rClip.bottom);
            y4 = static_cast<float>(rClip.bottom - y) / static_cast<float>(CVidInf::FX_HEIGHT);
        }
    }

    if ((dwFlags & 0x10) != 0) {
        x3 = static_cast<float>(rFxRect.right) / static_cast<float>(CVidInf::FX_WIDTH);
        x4 = static_cast<float>(rFxRect.left) / static_cast<float>(CVidInf::FX_WIDTH);

        if (x < rClip.left) {
            x1 = static_cast<float>(rClip.left);
            x3 -= static_cast<float>(rClip.left - x) / static_cast<float>(CVidInf::FX_WIDTH);
        }

        if (x + rFxRect.Width() > rClip.right) {
            x2 = static_cast<float>(rClip.right);
            x4 += static_cast<float>(x + rFxRect.Width() - rClip.right) / static_cast<float>(CVidInf::FX_WIDTH);
        }
    } else {
        x3 = static_cast<float>(rFxRect.left) / static_cast<float>(CVidInf::FX_WIDTH);
        x4 = static_cast<float>(rFxRect.right) / static_cast<float>(CVidInf::FX_WIDTH);

        if (x < rClip.left) {
            x1 = static_cast<float>(rClip.left);
            x3 = static_cast<float>(rClip.left - x) / static_cast<float>(CVidInf::FX_WIDTH);
        }

        if (x + rFxRect.Width() > rClip.right) {
            x2 = static_cast<float>(rClip.right);
            x4 = static_cast<float>(rClip.right - x) / static_cast<float>(CVidInf::FX_WIDTH);
        }
    }

    CVideo3d::glEnable(GL_BLEND);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x200) != 0) {
        CVideo3d::glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    } else if ((dwFlags & 0x8) != 0) {
        CVideo3d::glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    } else {
        CVideo3d::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    if ((dwFlags & 0x80000000) != 0) {
        BYTE r = GetRValue(dword_8BAC88);
        BYTE g = GetGValue(dword_8BAC88);
        BYTE b = GetBValue(dword_8BAC88);

        BYTE nBrightnessCorrection = g_pChitin->GetCurrentVideoMode()->m_nBrightnessCorrection;
        if (nBrightnessCorrection != 0) {
            r = ~((~nBrightnessCorrection * ~r) >> 8);
            g = ~((~nBrightnessCorrection * ~g) >> 8);
            b = ~((~nBrightnessCorrection * ~b) >> 8);
        }

        BYTE nGammaCorrection = g_pChitin->GetCurrentVideoMode()->m_nGammaCorrection;
        if (nGammaCorrection != 0) {
            DWORD nLight = (1 << CVidPalette::LIGHT_SCALE) + nGammaCorrection;
            r = static_cast<BYTE>(min((nLight * r) / 8, 255));
            g = static_cast<BYTE>(min((nLight * g) / 8, 255));
            b = static_cast<BYTE>(min((nLight * b) / 8, 255));
        }

        CVideo3d::glColor4f(static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            1.0f);
    }

    x1 += CVideo3d::SUB_PIXEL_SHIFT;
    y1 += CVideo3d::SUB_PIXEL_SHIFT;
    x2 += CVideo3d::SUB_PIXEL_SHIFT;
    y2 += CVideo3d::SUB_PIXEL_SHIFT;

    CVideo3d::glBegin(GL_TRIANGLE_STRIP);
    CVideo3d::glTexCoord2f(x3, y3);
    CVideo3d::glVertex3f(x1, y1, 0.0f);
    CVideo3d::glTexCoord2f(x3, y4);
    CVideo3d::glVertex3f(x1, y2, 0.0f);
    CVideo3d::glTexCoord2f(x4, y3);
    CVideo3d::glVertex3f(x2, y1, 0.0f);
    CVideo3d::glTexCoord2f(x4, y4);
    CVideo3d::glVertex3f(x2, y2, 0.0f);
    CVideo3d::glEnd();
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    if ((dwFlags & 0x200) != 0) {
        CVideo3d::glBlendFunc(GL_SRC_COLOR, GL_ONE);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glBegin(GL_TRIANGLE_STRIP);
        CVideo3d::glTexCoord2f(x3, y3);
        CVideo3d::glVertex3f(x1, y1, 0.0f);
        CVideo3d::glTexCoord2f(x3, y4);
        CVideo3d::glVertex3f(x1, y2, 0.0f);
        CVideo3d::glTexCoord2f(x4, y3);
        CVideo3d::glVertex3f(x2, y1, 0.0f);
        CVideo3d::glTexCoord2f(x4, y4);
        CVideo3d::glVertex3f(x2, y2, 0.0f);
        CVideo3d::glEnd();
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        CVideo3d::glDisable(GL_BLEND);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    CVideo3d::glDisable(GL_BLEND);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
}

// 0x7C4960
BOOL CVidCell::Render3d(INT nRefPtX, INT nRefPtY, DWORD dwFlags, INT nTransVal)
{
    INT nNewTransVal = 255;
    if (nTransVal != -1) {
        nNewTransVal = 255 - nTransVal;
    }

    if (m_pFrame == NULL) {
        if (!GetFrame(FALSE)) {
            // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
            // __LINE__: 627
            UTIL_ASSERT(FALSE);
        }
    }

    if (!m_bPaletteChanged) {
        m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
    }

    m_cPalette.Realize(CVidImage::rgbTempPal, 32, dwFlags, &m_paletteAffects, nNewTransVal);

    if (!m_bShadowOn) {
        CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = g_pChitin->GetCurrentVideoMode()->field_24;
    }

    CRect rClip(0, 0, CVideo::SCREENWIDTH, CVideo::SCREENHEIGHT);

    BOOLEAN bResult = Render3d(nRefPtX, nRefPtY, rClip, FALSE, dwFlags);

    pRes->Release();
    m_pFrame = NULL;

    return bResult;
}

// 0x7C4A90
BOOL CVidCell::Render3d(INT nRefPtX, INT nRefPtY, const CRect& rClip, CVidPoly* pClipPoly, int nPolys, DWORD dwFlags, INT nTransVal, BOOL bDemanded)
{
    INT nNewTransVal = 255;
    if (nTransVal != -1) {
        nNewTransVal = 255 - nTransVal;
    }

    if (m_pFrame == NULL) {
        if (!GetFrame(FALSE)) {
            return FALSE;
        }
    }

    // NOTE: No reference to `CVidInf` constants.
    if (m_pFrame->nWidth > CVIDINF_FX_WIDTH || m_pFrame->nHeight > CVIDINF_FX_HEIGHT) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
        // __LINE__: 690
        UTIL_ASSERT_MSG(FALSE, "CVidcell larger then fx");
    }

    INT x = nRefPtX - m_pFrame->nCenterX;
    INT y = nRefPtY - m_pFrame->nCenterY;

    CRect rFxRect(0, 0, m_pFrame->nWidth, m_pFrame->nHeight);

    CVideo3d::glEnable(GL_TEXTURE_2D);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    g_pChitin->cVideo.field_13E = 2;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, 2);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    LONG lPitch = g_pChitin->cVideo.field_13A
        ? CVidTile::BYTES_PER_TEXEL * CVIDINF_FX_WIDTH
        : CVidTile::BYTES_PER_TEXEL * m_pFrame->nWidth;

    CVidInf::FXClear(CVideo3d::texImageData, lPitch * m_pFrame->nHeight / CVidTile::BYTES_PER_TEXEL);

    BOOL bResult = Blt8To32(reinterpret_cast<DWORD*>(CVideo3d::texImageData),
        lPitch,
        dwFlags,
        nNewTransVal);
    if (bResult) {
        if (pClipPoly != NULL) {
            for (int nPoly = 0; nPoly < nPolys; nPoly++) {
                pClipPoly[nPoly].FillPoly(reinterpret_cast<WORD*>(CVideo3d::texImageData),
                    lPitch,
                    rFxRect,
                    RGB(0, 255, 0),
                    0,
                    CPoint(x, y));
            }
        }

        CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        if ((dwFlags & 0x40) == 0) {
            RenderTexture(x,
                y,
                rFxRect,
                CSize(m_pFrame->nWidth, m_pFrame->nHeight),
                rClip,
                dwFlags);
        }
    }

    if (!bDemanded) {
        pRes->Release();
    }
    m_pFrame = NULL;

    return bResult;
}

// 0x7C4E20
BOOL CVidCell::Render3d(INT nRefPtX, INT nRefPtY, const CRect& rClip, BOOLEAN bDemanded, DWORD dwFlags)
{
    CPoint ptStart;
    CPoint ptSource;
    CRect rFrameClip;
    CRect rFxRect;
    BOOLEAN bResult;
    INT x;
    INT y;
    LONG lPitch;

    ptStart.x = 0;
    ptStart.y = 0;

    if (m_pFrame == NULL) {
        if (!GetFrame(bDemanded)) {
            return FALSE;
        }
    }

    x = nRefPtX - m_pFrame->nCenterX;
    y = nRefPtY - m_pFrame->nCenterY;

    if ((dwFlags & 0x40) != 0) {
        ptStart.x = x;
        ptStart.y = y;
    }

    CVideo3d::glEnable(GL_TEXTURE_2D);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    g_pChitin->cVideo.field_13E = 2;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, 2);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    for (INT yOffset = 0; yOffset < static_cast<LONG>(m_pFrame->nHeight); yOffset += CVidInf::FX_HEIGHT) {
        for (INT xOffset = 0; xOffset < static_cast<LONG>(m_pFrame->nWidth); xOffset += CVidInf::FX_WIDTH) {
            if (yOffset != 0 || xOffset != 0) {
                CVidInf::FXClear(CVideo3d::texImageData, CVidInf::FX_WIDTH * CVidInf::FX_HEIGHT);
            }

            rFrameClip.SetRect(0,
                0,
                min(m_pFrame->nWidth - xOffset, CVidInf::FX_WIDTH),
                min(m_pFrame->nHeight - yOffset, CVidInf::FX_HEIGHT));
            lPitch = g_pChitin->cVideo.field_13A
                ? CVidTile::BYTES_PER_TEXEL * CVIDINF_FX_WIDTH
                : CVidTile::BYTES_PER_TEXEL * rFrameClip.Width();
            ptSource.x = xOffset;
            ptSource.y = yOffset;

            bResult = Blt8To32(reinterpret_cast<DWORD*>(CVideo3d::texImageData),
                lPitch,
                ptStart,
                rFrameClip,
                dwFlags,
                ptSource,
                0);
            if (!bResult) {
                // FIXME: Does not check `bDemanded` before releasing.
                pRes->Release();
                m_pFrame = NULL;
                return FALSE;
            }

            if ((dwFlags & 0x40) == 0) {
                rFxRect.SetRect(0,
                    0,
                    min(m_pFrame->nWidth - xOffset, CVidInf::FX_WIDTH),
                    min(m_pFrame->nHeight - yOffset, CVidInf::FX_HEIGHT));

                CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

                RenderTexture(x + xOffset,
                    y + yOffset,
                    rFxRect,
                    rFrameClip.Size(),
                    rClip,
                    dwFlags);
            }
        }
    }

    if ((dwFlags & 0x40) != 0) {
        CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);
    }

    m_pFrame = NULL;

    if (!bDemanded) {
        pRes->Release();
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
    // __LINE__: 918
    UTIL_ASSERT(bResult);

    return bResult;
}

// 0x7C5210
BOOL CVidCell::FXRender3d(INT nRefPtX, INT nRefPtY, const CRect& rFXRect, DWORD dwFlags, INT nTransVal, BOOL bAlphaValues)
{
    BOOL bResult;

    INT nNewTransVal = 255;
    if (nTransVal != -1) {
        nNewTransVal = 255 - nTransVal;
    }

    if (m_pFrame == NULL) {
        if (!GetFrame(FALSE)) {
            // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidImage3d.cpp
            // __LINE__: 960
            UTIL_ASSERT(FALSE);
        }
    }

    if (!bAlphaValues) {
        if (!m_bPaletteChanged) {
            m_cPalette.SetPalette(pRes->m_pPalette, 256, CVidPalette::TYPE_RESOURCE);
        }

        m_cPalette.Realize(CVidImage::rgbTempPal,
            32,
            dwFlags,
            &m_paletteAffects,
            nNewTransVal);

        if (!m_bShadowOn) {
            CVidImage::rgbTempPal[CVidPalette::SHADOW_ENTRY] = RGB(0, 0, 0);
        }
    }

    CRect rClip(0, 0, CVideo::SCREENWIDTH, CVideo::SCREENHEIGHT);
    bResult = FXRender3d(nRefPtX, nRefPtY, rFXRect, rClip, TRUE, dwFlags, bAlphaValues);
    m_pFrame = NULL;

    return bResult;
}

// 0x7C5330
BOOL CVidCell::FXRender3d(INT nRefPtX, INT nRefPtY, const CRect& rFXRect, const CRect& rClip, BOOLEAN bDemanded, DWORD dwFlags, BOOL bAlphaValues)
{
    BOOL bResult;
    CPoint pt(0, 0);
    CSize size;
    CRect r;

    if (m_pFrame == NULL) {
        if (!GetFrame(bDemanded)) {
            return FALSE;
        }
    }

    INT x = nRefPtX - m_pFrame->nCenterX;
    INT y = nRefPtY - m_pFrame->nCenterY;

    size.cx = min(m_pFrame->nWidth, CVidInf::FX_WIDTH);
    size.cy = min(m_pFrame->nHeight, CVidInf::FX_HEIGHT);

    if ((dwFlags & 0x40) != 0) {
        pt.x = x;
        pt.y = y;
        r.SetRect(max(x, rClip.left),
            max(y, rClip.top),
            min(x + size.cx, rClip.right),
            min(y + size.cy, rClip.bottom));
    } else {
        r.SetRect(pt, pt + size);
    }

    r.NormalizeRect();

    CVideo3d::glEnable(GL_TEXTURE_2D);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    g_pChitin->cVideo.field_13E = 2;
    CVideo3d::glBindTexture(GL_TEXTURE_2D, 2);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    CVideo3d::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

    LONG lPitch = g_pChitin->cVideo.field_13A
        ? CVidTile::BYTES_PER_TEXEL * CVIDINF_FX_WIDTH
        : CVidTile::BYTES_PER_TEXEL * rFXRect.Width();

    bResult = Blt8To32(CVideo3d::texImageData,
        lPitch,
        pt,
        r,
        dwFlags,
        CPoint(0, 0),
        bAlphaValues);

    if (bResult) {
        CVideo3d::glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        g_pChitin->GetCurrentVideoMode()->CheckResults3d(0);

        if ((dwFlags & 0x40) == 0) {
            RenderTexture(x,
                y,
                CRect(0, 0, size.cx, size.cy),
                rFXRect.Size(),
                rClip,
                dwFlags);
        }
    }

    if (!bDemanded) {
        pRes->Release();
    }

    m_pFrame = NULL;

    return bResult;
}

// 0x58FC70
void CVidCell::SetResRef(const CResRef& cNewResRef, BOOL bDoubleSize, BOOL bSetAutoRequest, BOOL bWarningIfMissing)
{
    CResHelper<CResCell, 1000>::SetResRef(cNewResRef, bSetAutoRequest, bWarningIfMissing);
    m_header.SetResRef(cNewResRef, bSetAutoRequest, FALSE);

    if (pRes != NULL) {
        pRes->m_bCacheHeader = m_header.GetResRef() == "";
    }

    m_bDoubleSize = bDoubleSize;
}

// 0x45B2B0
WORD CVidCell::GetCurrentFrameId()
{
    return m_nCurrentFrame;
}

// 0x45B2C0
WORD CVidCell::GetCurrentSequenceId()
{
    return m_nCurrentSequence;
}

// NOTE: Inlined in many places. Seen in `CVidCell::GetFrameCenterPoint`
// assertion.
BAMHEADER* CVidCell::GetResBamHeader()
{
    if (m_header.GetResRef() != "") {
        return m_header.pRes->m_pBamHeaderCopy;
    } else {
        if (pRes->m_bCacheHeader) {
            return pRes->m_pBamHeaderCopy;
        } else {
            return pRes->m_pBamHeader;
        }
    }
}

// NOTE: Inlined in many places.
FRAMEENTRY* CVidCell::GetResFrame(WORD nSequence, WORD nFrame)
{
    if (m_header.GetResRef() != "") {
        return m_header.pRes->GetFrame(nSequence, nFrame, m_bDoubleSize);
    } else {
        return pRes->GetFrame(nSequence, nFrame, m_bDoubleSize);
    }
}

// NOTE: Inlined in many places. Seen in `CVidCell::GetFrameCenterPoint`
// assertion.
SEQUENCEENTRY* CVidCell::GetResSequences()
{
    if (m_header.GetResRef() != "") {
        return m_header.pRes->m_pSequences;
    } else {
        return pRes->m_pSequences;
    }
}

// NOTE: Inlined in many places.
void CVidCell::Release(BOOLEAN& bDemanded)
{
    if (!bDemanded) {
        if (m_header.GetResRef() != "") {
            m_header.Release();
        } else {
            pRes->Release();
        }
    }
}

// NOTE: Inlined.
void CVidCell::UnsuppressTintAllRanges()
{
    m_paletteAffects.suppressTints = 0;
}
