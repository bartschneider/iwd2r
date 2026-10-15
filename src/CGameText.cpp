#include "CGameText.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CUtil.h"
#include "CVidMode.h"
#include "CWarp.h"

// 0x8D6570
const CSize CGameText::PADDING(0, 0);

// 0x4CB340
CGameText::CGameText(CGameArea* pArea, const CPoint& pt, BYTE nDuration, BYTE nBeginFade, const CString& sText)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameText.cpp
    // __LINE__: 110
    UTIL_ASSERT(pArea != NULL);

    m_nDuration = 0;
    m_nBeginFade = 0;
    m_nMaxLines = 0;
    m_objectType = TYPE_TEXT;
    m_szLine = NULL;
    m_nLines = 0;

    m_textFont.SetResRef(CResRef("INFOFONT"), FALSE, TRUE);
    m_vidCell.SequenceSet(0);
    m_vidCell.FrameSet(0);

    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE) == CGameObjectArray::SUCCESS) {
        AddToArea(pArea, pt, 0, LIST_FLIGHT);
        SetText(pt, nDuration, nBeginFade, sText);
        field_1C = 0;
    } else {
        // FIXME: Doesn't look cool.
        delete this;
    }
}

// 0x4CB4B0
void CGameText::SetText(const CPoint& pt, BYTE nDuration, BYTE nBeginFade, const CString& sText)
{
    CString sMutableText(sText);
    sMutableText.TrimRight();

    CSize size;
    if (m_vidCell.GetRes() != NULL) {
        m_vidCell.GetCurrentFrameSize(size, FALSE);
    } else {
        g_pBaldurChitin->GetCurrentVideoMode()->GetFXSize(size);
        size.cx = size.cx / 2 - 1;
        size.cy = size.cy / 2 - 1;
    }

    if (m_textFont.GetRes()->Demand() != NULL) {
        m_textFont.SetColor(RGB(255, 255, 255), RGB(0, 0, 0), TRUE);

        m_nMaxLines = static_cast<BYTE>((size.cy - 2 * PADDING.cy) / m_textFont.GetFontHeight(TRUE));

        if (m_szLine != NULL) {
            delete[] m_szLine;
        }

        m_szLine = new CString[m_nMaxLines];

        // __FILE__: C:\Projects\Icewind2\src\Baldur\CGameText.cpp
        // __LINE__: 222
        UTIL_ASSERT(m_szLine != NULL);

        m_nLines = CUtil::SplitString(&m_textFont,
            sMutableText,
            static_cast<WORD>(size.cx - 2 * PADDING.cx),
            m_nMaxLines,
            m_szLine,
            FALSE,
            TRUE,
            TRUE,
            -1);

        m_textFont.GetRes()->Release();
    }

    m_nDuration = 5 * nDuration * m_nLines;

    if (m_nDuration < 45) {
        m_nDuration = 65;
    }

    if (sText.GetLength() > 255) {
        m_nDuration = 500;
    }

    m_nBeginFade = nBeginFade;
    m_pos = pt;
}

// 0x4CB820
//
// Moves the text to pt, then by the amount it sticks out of the viewport.
void CGameText::SetPositionInView(const CPoint& pt)
{
    CVidMode* pVidMode = g_pBaldurChitin->pActiveEngine != NULL ? g_pBaldurChitin->pActiveEngine->pVidMode : NULL;
    const CInfinity& cInfinity = m_pArea->m_cInfinity;

    m_pos = pt;

    // The half extents: the frame's centre with a picture, else from the FX
    // size and the lines.
    CPoint ptHalf;
    if (m_vidCell.GetRes() != NULL) {
        CSize frameSize;
        m_vidCell.GetCurrentCenterPoint(ptHalf, FALSE);
        m_vidCell.GetCurrentFrameSize(frameSize, FALSE);
    } else {
        CSize size;
        pVidMode->GetFXSize(size);
        size.cx /= 2;
        size.cy /= 2;
        if (static_cast<BYTE>(m_nLines) == 1) {
            size.cx = m_textFont.GetStringLength(m_szLine[0], FALSE);
        }
        ptHalf.x = size.cx / 2;
        ptHalf.y = static_cast<BYTE>(m_nLines) * size.cy / m_nMaxLines / 2;
    }

    LONG nY = m_pos.y - m_posZ;
    LONG nLeft = m_pos.x - ptHalf.x;
    LONG nRight = m_pos.x + ptHalf.x;
    LONG nTop = nY - ptHalf.y;
    LONG nBottom = nY + ptHalf.y;

    LONG nViewLeft = cInfinity.nCurrentX;
    LONG nViewTop = cInfinity.nCurrentY;
    LONG nViewRight = cInfinity.rViewPort.right - cInfinity.rViewPort.left + nViewLeft;
    LONG nViewBottom = cInfinity.rViewPort.bottom - cInfinity.rViewPort.top + nViewTop;

    LONG dx = nLeft < nViewLeft ? nViewLeft - nLeft : 0;
    if (nRight > nViewRight) {
        dx = nViewRight - nRight;
    }

    // NOTE: A text below the viewport is moved further down (the original
    // adds the overhang, 0x4CB97C-0x4CB982); kept.
    LONG dy = 0;
    if (nBottom > nViewBottom) {
        dy = nBottom - nViewBottom;
    }
    if (nTop < nViewTop) {
        dy = nViewTop - nTop;
    }

    m_pos.x += dx;
    m_pos.y += dy;
}

// 0x4CB9B0
//
// Whether the text's box meets the viewport.
BOOL CGameText::IsOnScreen()
{
    if (static_cast<BYTE>(m_nLines) == 0) {
        return FALSE;
    }

    CVidMode* pVidMode = g_pBaldurChitin->pActiveEngine->pVidMode;
    const CInfinity& cInfinity = m_pArea->m_cInfinity;

    CPoint ptHalf;
    if (m_vidCell.GetRes() != NULL) {
        CSize frameSize;
        m_vidCell.GetCurrentCenterPoint(ptHalf, FALSE);
        m_vidCell.GetCurrentFrameSize(frameSize, FALSE);
    } else {
        CSize size;
        pVidMode->GetFXSize(size);
        size.cx /= 2;
        size.cy /= 2;
        LONG nWidth = size.cx;
        if (static_cast<BYTE>(m_nLines) == 1) {
            nWidth = m_textFont.GetStringLength(m_szLine[0], FALSE);
        }
        ptHalf.x = nWidth / 2;
        ptHalf.y = static_cast<BYTE>(m_nLines) * size.cy / m_nMaxLines / 2;
    }

    LONG nY = m_pos.y - m_posZ;
    CRect rText(m_pos.x - ptHalf.x, nY - ptHalf.y, m_pos.x + ptHalf.x, nY + ptHalf.y);
    CRect rView(cInfinity.nCurrentX,
        cInfinity.nCurrentY,
        cInfinity.rViewPort.right - cInfinity.rViewPort.left + cInfinity.nCurrentX,
        cInfinity.rViewPort.bottom - cInfinity.rViewPort.top + cInfinity.nCurrentY);
    return IntersectRect(&rView, &rText, &rView) != FALSE;
}

// 0x4CB700
BOOLEAN CGameText::DoAIUpdate(BOOLEAN active, LONG counter)
{
    return (counter & m_AISpeed) == (m_AISpeed & m_id);
}

// 0x4CB720
void CGameText::AIUpdate()
{
    if (m_nDuration == 0) {
        RemoveFromArea();
        return;
    }

    if (m_nDuration > 0) {
        m_nDuration--;
    }

    if (m_vidCell.GetRes() == NULL) {
        if (m_nDuration < m_nBeginFade) {
            LONG nScale = 144 * m_nDuration / m_nBeginFade + 100;
            m_textFont.SetColor(RGB(nScale, nScale, nScale), 0, TRUE);
        }
    }

    m_vidCell.FrameAdvance();
}

// 0x4CB7A0
void CGameText::RemoveFromArea()
{
    CGameObject::RemoveFromArea();

    // NOTE: This implementation is slightly different from other `CGameObject`
    // subclasses (looping until success).
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
            CGameObjectArray::THREAD_ASYNCH,
            NULL,
            INFINITE);
    } while (rc != CGameObjectArray::SUCCESS);

    delete this;
}

// 0x4CC0D0
CGameText::~CGameText()
{
    if (m_szLine != NULL) {
        delete[] m_szLine;
    }

    if (field_1C != 0) {
        CGameObject* pObject;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(field_1C,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pObject->field_1C = 0;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(field_1C,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x47C830
BOOLEAN CGameText::CanSaveGame(STRREF& strError)
{
    strError = -1;
    return TRUE;
}
