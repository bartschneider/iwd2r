#include "CUIControlEditMultiLine.h"

#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CUIControlScrollBar.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CWarp.h"

// 0x4D9410
CUIControlEditMultiLine::CUIControlEditMultiLine(CUIPanel* panel, UI_CONTROL_EDIT* controlInfo, int a3)
    : CUIControlBase(panel, &(controlInfo->base), 0)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ChUIControls.cpp
    // __LINE__: 2458
    UTIL_ASSERT(panel != NULL && controlInfo != NULL);

    if (m_pPanel->m_pManager->m_bDoubleSize) {
        m_ptText.x = 2 * controlInfo->x;
        m_ptText.y = 2 * controlInfo->y;
    } else {
        m_ptText.x = controlInfo->x;
        m_ptText.y = controlInfo->y;
    }

    field_350 = 0;
    field_354 = 0;
    m_nTextCapitalization = controlInfo->nTextCapitalization;
    field_85A = controlInfo->field_68;
    m_nRenderCount = 0;
    m_bFocused = FALSE;
    m_sText = controlInfo->initialText;
    field_86A = -1;
    field_86E = 0;
    field_877 = 1;

    CResRef cursorResRef(controlInfo->refCursor);
    m_vcCursor.SetResRef(cursorResRef, m_pPanel->m_pManager->m_bDoubleSize, TRUE, TRUE);

    m_vcCursor.m_bDoubleSize = m_pPanel->m_pManager->m_bDoubleSize;

    m_vcCursor.SequenceSet(controlInfo->nSequence);
    m_vcCursor.FrameSet(controlInfo->nFrame);

    m_cVidFont.SetResRef(CResRef(controlInfo->refFont), m_pPanel->m_pManager->m_bDoubleSize, FALSE);
    m_cVidFont.SetColor(RGB(200, 200, 200), RGB(60, 60, 60), 0);

    field_872 = a3;
    field_88A = -1;
    field_88E = -1;
    field_87A = 0;
    field_878 = 0;
    field_888 = 0;
    m_rgbForegroundColor = RGB(200, 200, 200);
    m_rgbBackgroundColor = RGB(60, 60, 60);
    field_36 = 1;
    field_876 = 1;

    // NOTE: Uninline.
    SetNeedMouseMove();

    m_size.cx -= m_ptText.x;
    m_size.cy -= m_ptText.y;
}

// 0x4D96C0
void CUIControlEditMultiLine::SetColor(COLORREF rgbForegroundColor, COLORREF rgbBackgroundColor)
{
    m_rgbForegroundColor = rgbForegroundColor;
    m_rgbBackgroundColor = rgbBackgroundColor;
}

// 0x4D96E0
void CUIControlEditMultiLine::OnEditReturn(CString sText)
{
}

// 0x4D9740
CUIControlEditMultiLine::~CUIControlEditMultiLine()
{
}

// 0x4D9800
void CUIControlEditMultiLine::Remove()
{
    m_sText.Empty();
    field_86A = 0;
    InvalidateRect();
}

// 0x4D9830
INT CUIControlEditMultiLine::ReConstitute()
{
    CString sLine;
    INT nLines = 0;

    if (field_877) {
        m_cVidFont.Load(FALSE);
        SHORT nFontHeight = m_cVidFont.GetFontHeight(FALSE);
        field_877 = 0;
        field_878 = static_cast<SHORT>((m_size.cy - m_ptText.y) / nFontHeight);
    }

    m_cVidFont.pRes->Demand();

    INT nWidth = m_size.cx - 10;

    // The soft line breaks (`\r`) go; the hard ones (`\n`) stay.
    INT nIndex = CUtil::Find(m_sText, '\r', 0);
    while (nIndex != -1) {
        m_sText = m_sText.Left(nIndex) + m_sText.Mid(nIndex + 1);
        if (nIndex < field_86A) {
            field_86A--;
        }
        nIndex = CUtil::Find(m_sText, '\r', 0);
    }

    INT nStart = 0;
    sLine.Empty();
    if (m_sText.GetLength() > 0) {
        do {
            INT nLength = CUtil::Find(m_sText.Mid(nStart), '\n', 0);
            if (nLength == -1) {
                nLength = m_sText.GetLength();
            }

            sLine = m_sText.Mid(nStart, nLength);
            if (m_cVidFont.GetStringLength(sLine, TRUE) < nWidth) {
                nStart += nLength + 1;
                if (nStart >= m_sText.GetLength()) {
                    nStart = m_sText.GetLength();
                }
            } else {
                INT nParser = sLine.GetLength();
                if (m_cVidFont.GetStringLength(sLine.Left(nParser), TRUE) >= nWidth) {
                    do {
                        CString sPart = sLine.Left(nParser);
                        INT nSpace = CUtil::Find(sPart, ' ', 0);
                        INT nLastSpace = nSpace;
                        while (nSpace >= 0) {
                            nLastSpace = nSpace;
                            nSpace = CUtil::Find(sPart, ' ', nLastSpace + 1);
                        }

                        if (nLastSpace > 0) {
                            nParser = nLastSpace;
                        } else {
                            if (g_pChitin->field_1A0) {
                                if (CUtil::IsDBCSTrailByte(sLine, nParser - 1)) {
                                    nParser--;
                                }
                            }
                            nParser--;
                        }

                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ChUIControls.cpp
                        // __LINE__: 2660
                        UTIL_ASSERT(nParser > 0);
                    } while (m_cVidFont.GetStringLength(sLine.Left(nParser), TRUE) >= nWidth);
                }

                CString sRight = m_sText.Right(m_sText.GetLength() - nParser - nStart - 1);
                INT nBreak = nParser + nStart + 1;
                m_sText = m_sText.Left(nBreak) + '\r' + sRight;
                if (nBreak < field_86A) {
                    field_86A++;
                }

                nStart = nBreak;
                if (nStart >= m_sText.GetLength()) {
                    nStart = m_sText.GetLength();
                }
            }

            nLines++;
        } while (nStart < m_sText.GetLength());
    }

    // A text ending in a hard break has an empty last line.
    BOOL bTrailingBreak;
    if (m_sText.Right(1) == "\n"
        && (!g_pChitin->field_1A0 || !CUtil::IsDBCSTrailByte(m_sText, m_sText.GetLength() - 1))) {
        bTrailingBreak = TRUE;
    } else {
        bTrailingBreak = FALSE;
    }

    if (bTrailingBreak) {
        nLines++;
    }

    field_87A = static_cast<SHORT>(nLines);
    AdjustVisibleIndex();

    field_86A = min(field_86A, m_sText.GetLength());

    m_cVidFont.pRes->Release();

    return nLines;
}

// 0x4D9D00
INT CUIControlEditMultiLine::CalculateNewLines(BYTE nMode)
{
    CString sWork;

    if (field_877) {
        m_cVidFont.Load(FALSE);
        SHORT nFontHeight = m_cVidFont.GetFontHeight(FALSE);
        field_877 = 0;
        field_878 = static_cast<SHORT>((m_size.cy - m_ptText.y) / nFontHeight);
    }

    m_cVidFont.pRes->Demand();

    INT nWidth = m_size.cx - 10;

    // NOTE: With Double Byte Character Support every change reflows the
    // whole text.
    if (g_pChitin->field_1A0) {
        nMode = 3;
    }

    switch (nMode) {
    case 1:
        // A character was added at the cursor: break the cursor's line where
        // it no longer fits, and the lines after it while they do not.
        if (field_86A == m_sText.GetLength()) {
            sWork = m_sText;
            CUtil::MakeReverse(sWork);
            INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
            if (nBack == -1) {
                nBack = sWork.GetLength();
            }

            BOOL bFits = m_cVidFont.GetStringLength(m_sText.Right(nBack), TRUE) < nWidth;
            m_cVidFont.pRes->Release();

            if (bFits) {
                return field_87A;
            }

            return ReConstitute();
        } else {
            INT nCursor = field_86A;
            sWork = m_sText.Right(m_sText.GetLength() - nCursor);
            INT nEnd = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
            if (nEnd == -1) {
                nEnd = sWork.GetLength();
            }

            sWork = m_sText.Left(nCursor);
            CUtil::MakeReverse(sWork);
            INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
            if (nBack == -1) {
                nBack = sWork.GetLength();
            }

            sWork = m_sText.Left(nCursor + nEnd);
            sWork = sWork.Right(sWork.GetLength() - nCursor + nBack);
            if (m_cVidFont.GetStringLength(sWork, TRUE) < nWidth) {
                m_cVidFont.pRes->Release();
                return field_87A;
            }

            INT nStart = nCursor - nBack;
            INT nPos = nEnd + nCursor;
            do {
                do {
                    sWork = m_sText.Left(nPos - 1);
                    CUtil::MakeReverse(sWork);
                    INT nSpace = CUtil::Find(sWork, ' ', 0);
                    if (nSpace == -1) {
                        nSpace = 1;
                    }

                    if (nPos - nSpace <= nStart) {
                        nSpace = 1;
                    }

                    CString sRight = m_sText.Right(m_sText.GetLength() - nPos - 1);
                    nPos -= nSpace;
                    INT nBreak = nPos - 1;
                    CString sWord = m_sText.Mid(nBreak, nSpace + 1);
                    CString sMid = m_sText.Mid(nStart, nPos - nStart - 1);
                    m_sText = m_sText.Left(nStart) + sMid + '\r' + sWord + sRight;

                    nPos = nBreak;
                    sWork = m_sText.Mid(nStart, nPos - nStart);
                    if (field_86A > nPos && field_86A <= nSpace + nPos + 1) {
                        field_86A++;
                    }
                } while (m_cVidFont.GetStringLength(sWork, TRUE) > nWidth);

                nStart = nPos + 1;
                sWork = m_sText.Right(m_sText.GetLength() - nStart);
                nPos = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
                if (nPos == -1) {
                    nPos = sWork.GetLength();
                }
                nPos += nStart;
                sWork = m_sText.Mid(nStart, nPos - nStart);
            } while (m_cVidFont.GetStringLength(sWork, TRUE) > nWidth);

            m_cVidFont.pRes->Release();
            AdjustVisibleIndex();
            return field_87A;
        }
    case 2: {
        // Characters went at the cursor: pull words up over the soft breaks
        // from the cursor's line on while they fit.
        sWork = m_sText.Left(field_86A);
        CUtil::MakeReverse(sWork);
        INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
        INT nPrevStart;
        INT nPos;
        if (nBack == -1) {
            nPrevStart = 0;
            nPos = CUtil::FindOneOf(m_sText, CString("\n\r"), 0);
            if (nPos == -1) {
                nPos = m_sText.GetLength();
            }
        } else {
            sWork = sWork.Right(sWork.GetLength() - nBack - 1);
            nPrevStart = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
            if (nPrevStart == -1) {
                nPrevStart = sWork.GetLength();
            }
            nPos = field_86A - nBack;
            nPrevStart = nPos - nPrevStart - 1;
        }

        for (;;) {
            sWork = m_sText.Right(m_sText.GetLength() - nPos);
            INT nWord = CUtil::FindOneOf(sWork, CString(" \n\r"), 0);
            if (nWord == -1) {
                nWord = sWork.GetLength();
            }

            if (nWord == 0) {
                m_cVidFont.pRes->Release();
                return ReConstitute();
            }

            BOOL bMoved = FALSE;
            if (m_sText[nPos - 1] == '\r') {
                sWork = m_sText.Mid(nPrevStart, nWord - nPrevStart + nPos + 1);
                if (m_cVidFont.GetStringLength(sWork, TRUE) <= nWidth) {
                    CString sRight = m_sText.Right(m_sText.GetLength() - nWord - nPos - 1);
                    CString sMid = m_sText.Mid(nPos, nWord + 1);
                    m_sText = m_sText.Left(nPos - 1) + sMid + '\r' + sRight;
                    nPos += nWord + 1;

                    // NOTE: The original measures the same string again; it
                    // still fits, so it goes on with the next word.
                    if (m_cVidFont.GetStringLength(sWork, TRUE) <= nWidth) {
                        continue;
                    }

                    bMoved = TRUE;
                }
            }

            if (!bMoved) {
                nWord = 0;
            }

            nPrevStart = nWord + nPos;
            sWork = m_sText.Right(m_sText.GetLength() - nPrevStart);
            INT nBreak = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
            if (nBreak == -1 || sWork[nBreak] == '\n') {
                break;
            }

            nPos = nBreak + nPrevStart + 1;
            sWork = m_sText.Right(m_sText.GetLength() - nPos);
            nWord = CUtil::FindOneOf(sWork, CString(" \n\r"), 0);
            if (nWord == -1) {
                nWord = sWork.GetLength();
            }

            sWork = m_sText.Mid(nPrevStart, nPos + (nWord - nPrevStart) + 1);
            if (m_cVidFont.GetStringLength(sWork, TRUE) > nWidth) {
                break;
            }
        }

        m_cVidFont.pRes->Release();
        AdjustVisibleIndex();
        return field_87A;
    }
    case 3:
        m_cVidFont.pRes->Release();
        return ReConstitute();
    }

    m_cVidFont.pRes->Release();
    return field_87A;
}

// 0x4DAA10
CString CUIControlEditMultiLine::GetText()
{
    CString sText;
    sText.Empty();

    if (m_sText.GetLength() > 0) {
        int start = 0;
        int end;
        do {
            end = CUtil::Find(m_sText, '\r', start);
            if (end == -1) {
                end = m_sText.GetLength();
            }

            sText += m_sText.Mid(start, end - start);
            start = end + 1;
        } while (end < m_sText.GetLength());
    }

    return sText;
}

// 0x4DAAF0
void CUIControlEditMultiLine::KillFocus()
{
    // FIXME: Unused.
    CWnd* pWnd = g_pChitin->GetWnd();
    if (g_pChitin->cImm.field_128) {
        g_pChitin->cImm.sub_7C2E10(g_pChitin->GetWnd()->GetSafeHwnd());
        m_pPanel->InvalidateRect(&(m_pPanel->m_rImeSuggestionsFrame));
        m_pPanel->m_rImeSuggestionsFrame.SetRectEmpty();
        m_pPanel->m_pImeSuggestionsFont = NULL;
    }

    static_cast<CBaldurEngine*>(m_pPanel->m_pManager->m_pWarp)->DisableEditKeys();

    m_bFocused = FALSE;
    InvalidateRect();
}

// 0x4DAB80
void CUIControlEditMultiLine::SetFocus()
{
    m_bFocused = TRUE;
    field_866 = m_sText;

    InvalidateRect();

    static_cast<CBaldurEngine*>(m_pPanel->m_pManager->m_pWarp)->EnableEditKeys(field_85A);
}

// 0x4DABD0
BOOL CUIControlEditMultiLine::OnLButtonDown(CPoint pt)
{
    if (!m_bActive) {
        return FALSE;
    }

    INT x = pt.x - (m_ptOrigin.x + m_ptText.x);
    INT y = pt.y - (m_ptOrigin.y + m_ptText.y);

    if (x < 0 || x >= m_size.cx || y < 0 || y >= m_size.cy) {
        if (m_bFocused == TRUE) {
            if (field_872) {
                m_sText = field_866;
                ReConstitute();
            }

            // NOTE: Uninline.
            m_pPanel->m_pManager->KillCapture();

            field_88A = -1;
            field_88E = -1;
        }
    } else {
        m_pPanel->m_pManager->SetCapture(this, CUIManager::MOUSELBUTTON);

        m_cVidFont.pRes->Demand();

        INT nColumn = 0;

        // The line clicked: the visible line under the cursor, at most the
        // last line.
        INT nRow = field_878;
        if (y / m_cVidFont.GetFontHeight(FALSE) < nRow) {
            nRow = y / m_cVidFont.GetFontHeight(FALSE);
        }

        INT nLine = field_87A;
        if (field_86E + nRow < field_87A) {
            nRow = field_878;
            if (y / m_cVidFont.GetFontHeight(FALSE) < nRow) {
                nLine = field_86E + y / m_cVidFont.GetFontHeight(FALSE);
            } else {
                nLine = field_86E + nRow;
            }
        }

        INT nStart = 0;
        for (INT nCount = 0; nCount < nLine; nCount++) {
            if (nStart >= m_sText.GetLength()) {
                break;
            }

            // NOTE: The original adds the find's result before it checks
            // for -1; on a line without a break past the first one the
            // start stays where it is.
            nStart += CUtil::FindOneOf(m_sText.Mid(nStart), CString("\n\r"), 0);
            if (nStart == -1) {
                nStart = m_sText.GetLength();
            } else {
                nStart++;
            }
        }

        if (nStart == m_sText.GetLength()) {
            field_86A = m_sText.GetLength();
            m_cVidFont.pRes->Release();
            field_88E = field_86A;
            if (!m_pPanel->m_pManager->m_pWarp->GetShiftKey()) {
                field_88A = field_88E;
            }
            field_888 = 1;
        } else {
            INT nLength = CUtil::FindOneOf(m_sText.Mid(nStart), CString("\n\r"), 0);
            if (nLength == -1) {
                nLength = m_sText.GetLength();
            }

            if (x > m_cVidFont.GetStringLength(m_sText.Mid(nStart, nLength), FALSE)) {
                nColumn = nLength + 1;
            } else {
                if (x > m_cVidFont.GetStringLength(m_sText.Mid(nStart, 0), FALSE)) {
                    do {
                        if (g_pChitin->field_1A0) {
                            if (IsDBCSLeadByte(m_sText[nStart + nColumn - 1])) {
                                nColumn++;
                            }
                        }

                        nColumn++;

                        // NOTE: The original compares the column with the
                        // whole text's length.
                        if (nColumn == m_sText.GetLength()) {
                            break;
                        }
                    } while (x > m_cVidFont.GetStringLength(m_sText.Mid(nStart, nColumn), FALSE));
                }
            }

            m_cVidFont.pRes->Release();

            if (g_pChitin->field_1A0) {
                if (CUtil::IsDBCSTrailByte(m_sText, nStart + nColumn - 1)) {
                    nColumn--;
                }
            }

            field_86A = min(nStart + nColumn - 1, m_sText.GetLength());
            field_88E = field_86A;
            if (!m_pPanel->m_pManager->m_pWarp->GetShiftKey()) {
                field_88A = field_88E;
            }
            field_888 = 1;

            InvalidateRect();
        }
    }

    return TRUE;
}

// 0x4DB070
void CUIControlEditMultiLine::OnLButtonUp(CPoint pt)
{
    if (m_bActive) {
        field_888 = 0;
        m_pPanel->m_pManager->SetCapture(this, 2);
        if (field_88E == field_88A) {
            field_86A = field_88E;
            field_88E = -1;
        }

        InvalidateRect();
    }
}

// 0x4DB0C0
BOOL CUIControlEditMultiLine::OnRButtonDown(CPoint pt)
{
    if (!m_bActive) {
        return FALSE;
    }

    int x = pt.x - (m_ptText.x + m_ptOrigin.x);
    int y = pt.y - (m_ptText.y + m_ptOrigin.y);

    if (x < 0 || x >= m_size.cx || y < 0 || y >= m_size.cy) {
        if (m_bFocused == TRUE) {
            if (field_872) {
                m_sText = field_866;
                ReConstitute();
            }

            // NOTE: Uninline.
            m_pPanel->m_pManager->KillCapture();
        }
    }

    return TRUE;
}

// 0x4DB160
void CUIControlEditMultiLine::OnKeyDown(SHORT nKey)
{
    CString sWork;
    CWarp* pWarp = m_pPanel->m_pManager->m_pWarp;

    g_pChitin->GetWnd();

    // The IME's composition (Double Byte Character Support).
    if (g_pChitin->cImm.field_128) {
        if (nKey == VK_F3 && PRIMARYLANGID(GetSystemDefaultLangID()) == LANG_JAPANESE) {
            CWnd* pWnd = g_pChitin->GetWnd();
            g_pChitin->cImm.sub_7C2CC0(pWnd != NULL ? pWnd->GetSafeHwnd() : NULL);
        }

        CString sComposition = g_pChitin->cImm.sub_4D89E0();
        if (sComposition != "") {
            BYTE nLength = static_cast<BYTE>(sComposition.GetLength());
            BYTE nMode;
            if (field_88E == -1 || field_88A == -1) {
                m_sText = m_sText.Left(field_86A) + sComposition + m_sText.Right(m_sText.GetLength() - field_86A);
                field_86A += nLength;
                nMode = 1;
            } else {
                if (field_88E < field_88A) {
                    m_sText = m_sText.Left(field_88E) + sComposition + m_sText.Right(m_sText.GetLength() - field_88A);
                } else {
                    m_sText = m_sText.Left(field_88A) + sComposition + m_sText.Right(m_sText.GetLength() - field_88E);
                    field_86A = nLength + field_88A;
                }
                field_88A = -1;
                field_88E = -1;
                nMode = 3;
            }
            CalculateNewLines(nMode);
            InvalidateRect();
        }

        if (nKey == VK_F4
            || (nKey == VK_RETURN && PRIMARYLANGID(GetSystemDefaultLangID()) == LANG_KOREAN)) {
            CWnd* pWnd = g_pChitin->GetWnd();
            g_pChitin->cImm.sub_7C2E10(pWnd != NULL ? pWnd->GetSafeHwnd() : NULL);
            m_pPanel->InvalidateRect(&(m_pPanel->m_rImeSuggestionsFrame));
            m_pPanel->m_rImeSuggestionsFrame.SetRectEmpty();
            m_pPanel->m_pImeSuggestionsFont = NULL;
            m_pPanel->InvalidateRect(NULL);
        }

        return;
    }

    if (pWarp->GetMenuKey()) {
        return;
    }

    if (pWarp->GetCtrlKey()) {
        switch (nKey) {
        case VK_END:
            if (pWarp->GetShiftKey() && field_88A == -1) {
                field_88A = field_86A;
            }

            // NOTE: The original sets the top line to the text's length;
            // AdjustVisibleIndex brings it back to the last page.
            field_86E = m_sText.GetLength();
            field_86A = m_sText.GetLength();
            if (!pWarp->GetShiftKey()) {
                field_88A = -1;
                field_88E = -1;
                AdjustVisibleIndex();
            } else {
                field_88E = field_86A;
                AdjustVisibleIndex();
            }
            InvalidateRect();
            return;
        case VK_HOME:
            if (pWarp->GetShiftKey() && field_88A == -1) {
                field_88A = field_86A;
            }

            field_86E = 0;
            field_86A = 0;
            if (!pWarp->GetShiftKey()) {
                field_88A = -1;
                field_88E = -1;
                AdjustVisibleIndex();
            } else {
                field_88E = field_86A;
                AdjustVisibleIndex();
            }
            InvalidateRect();
            return;
        case VK_LEFT:
            // The previous word.
            if (field_86A == 0) {
                return;
            }

            if (pWarp->GetShiftKey() && field_88A == -1) {
                field_88A = field_86A;
            }

            sWork = m_sText.Left(field_86A);
            CUtil::MakeReverse(sWork);
            if (1) {
                INT nWord = CUtil::FindOneOf(sWork, CString("\n\r "), 0);
                if (nWord == -1) {
                    field_86A = 0;
                } else {
                    if (nWord == 0) {
                        if (sWork.Mid(0, 1) == CString(' ', 1)) {
                            INT nNext = CUtil::FindOneOf(sWork.Right(sWork.GetLength() - 1), CString("\n\r "), 0);
                            if (nNext == -1) {
                                field_86A = 0;
                            } else {
                                nWord = nNext + 1;
                            }
                        } else {
                            nWord = 1;
                        }
                    }

                    field_86A = max(field_86A - nWord, 0);
                }
            }

            if (pWarp->GetShiftKey()) {
                field_88E = field_86A;
                AdjustVisibleIndex();
            } else {
                field_88A = -1;
                field_88E = -1;
                AdjustVisibleIndex();
            }
            InvalidateRect();
            return;
        case VK_RIGHT:
            // The next word.
            if (field_86A == m_sText.GetLength()) {
                return;
            }

            if (pWarp->GetShiftKey() && field_88A == -1) {
                field_88A = field_86A;
            }

            if (1) {
                INT nCursor;
                INT nWord = CUtil::FindOneOf(m_sText.Mid(field_86A), CString("\n\r "), 0);
                if (nWord == -1) {
                    nCursor = m_sText.GetLength();
                } else {
                    if (nWord == 0) {
                        if (m_sText.Mid(field_86A, 1) == CString(' ', 1)) {
                            INT nNext = CUtil::FindOneOf(m_sText.Mid(field_86A + 1), CString("\n\r "), 0);
                            if (nNext == -1) {
                                field_86A = m_sText.GetLength();
                            } else {
                                nWord = nNext + 1;
                            }
                        } else {
                            nWord = 1;
                        }
                    }

                    if (m_sText.Mid(nWord + field_86A, 1) == CString(' ', 1)) {
                        nWord++;
                    }

                    nCursor = max(nWord + field_86A, 0);
                }

                field_86A = nCursor;
            }

            if (!pWarp->GetShiftKey()) {
                field_88A = -1;
                field_88E = -1;
                AdjustVisibleIndex();
            } else {
                field_88E = field_86A;
                AdjustVisibleIndex();
            }
            InvalidateRect();
            return;
        case VK_INSERT:
        case 'C':
            // Copy.
            if (!pWarp->GetShiftKey()) {
                CopyToClipboard(FALSE);
                InvalidateRect();
                return;
            }
            return;
        case 'V':
            // Paste.
            if (!pWarp->GetShiftKey()) {
                if (OpenClipboard(NULL)) {
                    PasteFromClipboard();
                    CloseClipboard();
                    CalculateNewLines(3);
                }
                InvalidateRect();
                return;
            }
            return;
        case 'X':
            // Cut.
            if (!pWarp->GetShiftKey()) {
                CopyToClipboard(TRUE);
                InvalidateRect();
                return;
            }
            return;
        }

        return;
    }

    switch (nKey) {
    case VK_BACK:
        if (field_86A == 0) {
            return;
        }

        field_86A--;
        if (g_pChitin->field_1A0 && CUtil::IsDBCSTrailByte(m_sText, field_86A)) {
            field_86A--;
        }

        if (field_88E != -1 && field_88A != -1) {
            // The selection goes.
            if (field_88E < field_88A) {
                m_sText = m_sText.Left(field_88E) + m_sText.Right(m_sText.GetLength() - field_88A);
            } else {
                m_sText = m_sText.Left(field_88A) + m_sText.Right(m_sText.GetLength() - field_88E);
                field_86A = field_88A;
            }
            break;
        }

        // NOTE: The last character goes without a reflow, unless it is a
        // line break.
        if (field_86A == m_sText.GetLength() - 1
            && m_sText.Mid(field_86A, 1) != "\r"
            && m_sText.Mid(field_86A, 1) != "\n") {
            m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
            InvalidateRect();
            return;
        }

        if (!g_pChitin->field_1A0 || !IsDBCSLeadByte(m_sText[field_86A])) {
            if (m_sText.Mid(field_86A, 1) == "\r") {
                // A soft break goes with the character before it.
                m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
                field_86A--;
                if (!g_pChitin->field_1A0 || !CUtil::IsDBCSTrailByte(m_sText, field_86A)) {
                    m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
                } else {
                    field_86A--;
                    m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 2);
                }
                CalculateNewLines(3);
                InvalidateRect();
                return;
            }
        }

        if (g_pChitin->field_1A0 && IsDBCSLeadByte(m_sText[field_86A])) {
            m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 2);
        } else {
            m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
        }
        CalculateNewLines(2);
        InvalidateRect();
        return;
    case VK_RETURN:
        if (field_88E == -1 || field_88A == -1) {
            m_sText = m_sText.Left(field_86A) + '\n' + m_sText.Right(m_sText.GetLength() - field_86A);
        } else if (field_88E < field_88A) {
            m_sText = m_sText.Left(field_88E) + '\n' + m_sText.Right(m_sText.GetLength() - field_88A);
            field_88A = -1;
            field_88E = -1;
        } else {
            m_sText = m_sText.Left(field_88A) + '\n' + m_sText.Right(m_sText.GetLength() - field_88E);
            field_86A = field_88A;
            field_88A = -1;
            field_88E = -1;
        }
        field_86A++;
        CalculateNewLines(3);
        InvalidateRect();
        return;
    case VK_ESCAPE:
        field_88A = -1;
        field_88E = -1;
        m_sText = field_866;
        ReConstitute();

        // NOTE: Uninline.
        m_pPanel->m_pManager->KillCapture();

        InvalidateRect();
        return;
    case VK_SPACE:
        if (field_85A == 2) {
            return;
        }

        if (InsertCharacter(' ')) {
            return;
        }
        break;
    case VK_PRIOR:
        OnPageUpKey();
        return;
    case VK_NEXT:
        OnPageDownKey();
        return;
    case VK_END:
        if (pWarp->GetShiftKey() && field_88A == -1) {
            field_88A = field_86A;
        }

        if (1) {
            INT nEnd = CUtil::FindOneOf(m_sText.Mid(field_86A), CString("\n\r"), 0);
            if (nEnd == -1) {
                field_86A = m_sText.GetLength();
            } else {
                field_86A = min(field_86A + nEnd, m_sText.GetLength());
            }
        }

        if (!pWarp->GetShiftKey()) {
            field_88A = -1;
            field_88E = -1;
            AdjustVisibleIndex();
        } else {
            field_88E = field_86A;
            AdjustVisibleIndex();
        }
        InvalidateRect();
        return;
    case VK_HOME:
        if (pWarp->GetShiftKey() && field_88A == -1) {
            field_88A = field_86A;
        }

        sWork = m_sText.Left(field_86A);
        CUtil::MakeReverse(sWork);
        if (1) {
            INT nStart = CUtil::FindOneOf(sWork, CString("\r\n"), 0);
            if (nStart == -1) {
                field_86A = 0;
            } else {
                field_86A = max(field_86A - nStart, 0);
            }
        }

        if (!pWarp->GetShiftKey()) {
            field_88A = -1;
            field_88E = -1;
            AdjustVisibleIndex();
        } else {
            field_88E = field_86A;
            AdjustVisibleIndex();
        }
        InvalidateRect();
        return;
    case VK_LEFT:
        if (field_86A == 0) {
            return;
        }

        if (pWarp->GetShiftKey() && field_88A == -1) {
            field_88A = field_86A;
        }

        // Soft breaks are stepped over.
        do {
            field_86A--;
            if (g_pChitin->field_1A0 && CUtil::IsDBCSTrailByte(m_sText, field_86A)) {
                field_86A--;
                break;
            }
        } while (m_sText.Mid(field_86A, 1) == "\r");

        if (!pWarp->GetShiftKey()) {
            field_88A = -1;
            field_88E = -1;
            AdjustVisibleIndex();
        } else {
            field_88E = field_86A;
            AdjustVisibleIndex();
        }
        InvalidateRect();
        return;
    case VK_UP:
        OnUpKey();
        return;
    case VK_RIGHT:
        if (field_86A == m_sText.GetLength()) {
            return;
        }

        if (pWarp->GetShiftKey() && field_88A == -1) {
            field_88A = field_86A;
        }

        do {
            if (g_pChitin->field_1A0 && IsDBCSLeadByte(m_sText[field_86A])) {
                field_86A += 2;
                break;
            }
            field_86A++;
        } while (m_sText.Mid(field_86A, 1) == "\r");

        if (pWarp->GetShiftKey()) {
            field_88E = field_86A;
            AdjustVisibleIndex();
        } else {
            field_88A = -1;
            field_88E = -1;
            AdjustVisibleIndex();
        }
        InvalidateRect();
        return;
    case VK_DOWN:
        OnDownKey();
        return;
    case VK_INSERT:
        // Shift+Insert pastes.
        if (pWarp->GetShiftKey() && OpenClipboard(NULL)) {
            PasteFromClipboard();
            CloseClipboard();
            CalculateNewLines(3);
        }
        InvalidateRect();
        return;
    case VK_DELETE:
        if (pWarp->GetShiftKey()) {
            // Shift+Delete cuts.
            CopyToClipboard(TRUE);
            InvalidateRect();
            return;
        }

        if (field_88E != -1 && field_88A != -1) {
            if (field_88A <= field_88E) {
                m_sText = m_sText.Left(field_88A) + m_sText.Right(m_sText.GetLength() - field_88E);
                field_86A = field_88A;
            } else {
                m_sText = m_sText.Left(field_88E) + m_sText.Right(m_sText.GetLength() - field_88A);
            }
            break;
        }

        if (field_86A == m_sText.GetLength()) {
            return;
        }

        if (!g_pChitin->field_1A0 || !IsDBCSLeadByte(m_sText[field_86A])) {
            if (m_sText.Mid(field_86A, 1) == "\r") {
                // A soft break goes with the character after it.
                m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
                if (g_pChitin->field_1A0 && IsDBCSLeadByte(m_sText[field_86A])) {
                    m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 2);
                } else {
                    m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
                }
                CalculateNewLines(3);
                InvalidateRect();
                return;
            }
        }

        if (g_pChitin->field_1A0 && IsDBCSLeadByte(m_sText[field_86A])) {
            m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 2);
        } else {
            m_sText = m_sText.Left(field_86A) + m_sText.Right(m_sText.GetLength() - field_86A - 1);
        }
        CalculateNewLines(2);
        InvalidateRect();
        return;
    case VK_F4:
        if (g_pBaldurChitin->field_1A0) {
            m_pPanel->InvalidateRect(NULL);
        }
        // FALLTHROUGH
    case VK_F3:
    case VK_PROCESSKEY:
        if (g_pChitin->field_1A4) {
            CWnd* pWnd = g_pChitin->GetWnd();
            g_pChitin->cImm.sub_7C2CC0(pWnd != NULL ? pWnd->GetSafeHwnd() : NULL);
            g_pChitin->GetWnd();
            if (g_pChitin->cImm.field_128) {
                CPoint ptPanel = m_pPanel->m_ptOrigin;
                CSize panelSize = m_pPanel->m_size;
                CRect rSuggestions = g_pChitin->cImm.sub_7C3020(ptPanel,
                    panelSize,
                    m_ptOrigin,
                    m_size,
                    m_cVidFont.GetFontHeight(FALSE));
                if (rSuggestions.left == 0
                    && rSuggestions.right == 0
                    && rSuggestions.top == 0
                    && rSuggestions.bottom == 0) {
                    m_pPanel->InvalidateRect(&(m_pPanel->m_rImeSuggestionsFrame));
                }
                m_pPanel->m_rImeSuggestionsFrame = rSuggestions;
                m_pPanel->m_pImeSuggestionsFont = &m_cVidFont;
            }
        }
        InvalidateRect();
        return;
    default:
        if (nKey >= 'A' && nKey <= 'Z') {
            if (field_85A == 2) {
                InvalidateRect();
                return;
            }

            if (m_nTextCapitalization == 0) {
                if (pWarp->GetShiftKey() == pWarp->GetCapsLockKey()) {
                    nKey += 'a' - 'A';
                }
            } else if (m_nTextCapitalization == 2) {
                nKey += 'a' - 'A';
            }

            if (InsertCharacter(static_cast<char>(nKey))) {
                return;
            }
        } else if ((nKey >= '0' && nKey <= '9') || (nKey >= VK_NUMPAD0 && nKey <= VK_NUMPAD9)) {
            if (nKey >= VK_NUMPAD0 && nKey <= VK_NUMPAD9) {
                nKey -= VK_NUMPAD0 - '0';
            }

            // 0x8ABA54 and 0x8ABA48
            static const char shiftedDigits[] = ")!@#$%^&*(";
            static const char digits[] = "0123456789";
            if (InsertCharacter(pWarp->GetShiftKey() ? shiftedDigits[nKey - '0'] : digits[nKey - '0'])) {
                return;
            }
        } else if (nKey == VK_MULTIPLY
            || nKey == VK_DIVIDE
            || nKey == VK_ADD
            || nKey == VK_SUBTRACT
            || nKey == VK_DECIMAL) {
            char ch;
            switch (nKey) {
            case VK_MULTIPLY:
                ch = '*';
                break;
            case VK_ADD:
                ch = '+';
                break;
            case VK_SUBTRACT:
                ch = '-';
                break;
            case VK_DECIMAL:
                ch = '.';
                break;
            case VK_DIVIDE:
                ch = '/';
                break;
            default:
                ch = 'X';
                break;
            }

            if (InsertCharacter(ch)) {
                return;
            }
        } else {
            // The punctuation keys (0x8ABA3C), the characters they type
            // (0x8ABA24) and with Shift (0x8ABA30).
            static const BYTE keys[12] = { 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xDB, 0xDC, 0xDD, 0xDE, 0x00 };
            static const char characters[12] = { ';', '=', ',', '-', '.', '/', '`', '[', '\\', ']', '\'', '\0' };
            static const char shiftedCharacters[12] = { ':', '+', '<', '_', '>', '?', '~', '{', '|', '}', '"', '\0' };

            BYTE nIndex = 0;
            while (static_cast<char>(keys[nIndex]) != static_cast<char>(nKey)) {
                nIndex++;
                if (nIndex >= 12) {
                    InvalidateRect();
                    return;
                }
            }

            if (InsertCharacter(pWarp->GetShiftKey() ? shiftedCharacters[nIndex] : characters[nIndex])) {
                return;
            }
        }
        break;
    }

    // A selection was replaced: it goes, and the whole text reflows.
    field_88A = -1;
    field_88E = -1;
    CalculateNewLines(3);
    InvalidateRect();
}

// NOTE: Not a function of the original: OnKeyDown (0x4DB160) repeats this in
// Ctrl+C, Ctrl+Insert, Ctrl+X and Shift+Delete. Copies the selection (the
// whole text without one) as CF_TEXT; a cut also takes it out of the text.
// The selection goes either way.
void CUIControlEditMultiLine::CopyToClipboard(BOOL bCut)
{
    if (!OpenClipboard(NULL)) {
        return;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, m_sText.GetLength() + 1);
    if (hMem != NULL) {
        LPVOID pMem = GlobalLock(hMem);
        if (pMem == NULL) {
            GlobalFree(hMem);
        } else {
            CString sCopy;
            if (field_88E == -1 || field_88A == -1) {
                sCopy = m_sText;
                if (bCut) {
                    m_sText.Empty();
                    field_86A = 0;
                }
            } else {
                if (field_88E < field_88A) {
                    sCopy = m_sText.Mid(field_88E, field_88A - field_88E);
                    if (bCut) {
                        m_sText = m_sText.Left(field_88E) + m_sText.Mid(field_88A);
                        field_86A = field_88E;
                    }
                } else {
                    sCopy = m_sText.Mid(field_88A, field_88E - field_88A);
                    if (bCut) {
                        m_sText = m_sText.Left(field_88A) + m_sText.Mid(field_88E);
                        field_86A = field_88A;
                    }
                }

                field_88A = -1;
                field_88E = -1;
            }

            memcpy(pMem, sCopy.GetBuffer(sCopy.GetLength() + 1), sCopy.GetLength() + 1);
            sCopy.ReleaseBuffer();

            GlobalUnlock(hMem);

            if (SetClipboardData(CF_TEXT, hMem) == NULL) {
                // Ownership was not transferred.
                GlobalFree(hMem);
            }
        }
    }

    CloseClipboard();
}

// NOTE: Not a function of the original: OnKeyDown (0x4DB160) repeats this in
// Ctrl+V and Shift+Insert, with the clipboard open. Puts the clipboard's text
// at the cursor, or over the selection; the cursor stays where it is (at the
// selection's start for a selection made backwards).
void CUIControlEditMultiLine::PasteFromClipboard()
{
    HGLOBAL hMem = GetClipboardData(CF_TEXT);
    if (hMem == NULL) {
        return;
    }

    LPVOID pMem = GlobalLock(hMem);
    if (pMem == NULL) {
        return;
    }

    LPCSTR pText = reinterpret_cast<LPCSTR>(pMem);
    if (field_88E == -1 || field_88A == -1) {
        m_sText = m_sText.Left(field_86A) + pText + m_sText.Right(m_sText.GetLength() - field_86A);
    } else if (field_88E < field_88A) {
        m_sText = m_sText.Left(field_88E) + pText + m_sText.Right(m_sText.GetLength() - field_88A);
        field_88A = -1;
        field_88E = -1;
    } else {
        m_sText = m_sText.Left(field_88A) + pText + m_sText.Right(m_sText.GetLength() - field_88E);
        field_86A = field_88A;
        field_88A = -1;
        field_88E = -1;
    }

    GlobalUnlock(hMem);
}

// NOTE: Not a function of the original: Insert's and the typing keys' code
// in OnKeyDown (0x4DB160). With no selection the character goes in at the
// cursor, the cursor moves on, the line reflows and TRUE comes back; over a
// selection the character replaces it (the cursor after it for a selection
// made backwards) and FALSE comes back for OnKeyDown to clear the selection
// and reflow the whole text.
BOOL CUIControlEditMultiLine::InsertCharacter(char ch)
{
    if (field_88E == -1 || field_88A == -1) {
        m_sText = m_sText.Left(field_86A) + ch + m_sText.Right(m_sText.GetLength() - field_86A);
        field_86A++;
        CalculateNewLines(1);
        InvalidateRect();
        return TRUE;
    }

    if (field_88E < field_88A) {
        m_sText = m_sText.Left(field_88E) + ch + m_sText.Right(m_sText.GetLength() - field_88A);
    } else {
        m_sText = m_sText.Left(field_88A) + ch + m_sText.Right(m_sText.GetLength() - field_88E);
        field_86A = field_88A + 1;
    }

    return FALSE;
}

// NOTE: Not a function of the original: Page Up in OnKeyDown (0x4DB160). Up
// a page of lines, at the column nearest the cursor's.
void CUIControlEditMultiLine::OnPageUpKey()
{
    CString sWork;
    CWarp* pWarp = m_pPanel->m_pManager->m_pWarp;

    if (field_86E == 0) {
        return;
    }

    m_cVidFont.pRes->Demand();

    sWork = m_sText.Left(field_86A);
    CUtil::MakeReverse(sWork);
    INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
    if (nBack == -1) {
        m_cVidFont.pRes->Release();
        return;
    }

    INT nColumnX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nBack, nBack), TRUE);

    INT nPos = 0;
    INT nLines = 0;
    sWork = m_sText.Left(field_86A - nBack - 1);
    CUtil::MakeReverse(sWork);
    if (field_878 > 0) {
        do {
            INT nLength = CUtil::FindOneOf(sWork.Mid(nPos), CString("\n\r"), 0);
            if (nLength == -1) {
                if (nLines + 1 < field_878) {
                    m_cVidFont.pRes->Release();
                    return;
                }
                nLength = sWork.Mid(nPos).GetLength();
            }
            nLines++;
            nPos += nLength + 1;
        } while (nLines < field_878);
    }

    field_86E -= nLines;
    AdjustVisibleIndex();

    CUtil::MakeReverse(sWork);
    INT nLength = CUtil::FindOneOf(sWork.Mid(sWork.GetLength() - nPos + 1), CString("\n\r"), 0);
    INT nLineX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nBack - nPos, nLength), TRUE);

    if (pWarp->GetShiftKey() && field_88A == -1) {
        field_88A = field_86A;
    }

    if (nLineX <= nColumnX) {
        field_86A += nLength - nBack - nPos;
        m_cVidFont.pRes->Release();
        if (pWarp->GetShiftKey()) {
            field_88E = field_86A;
        } else {
            field_88A = -1;
            field_88E = -1;
        }
        InvalidateRect();
        return;
    }

    INT nBackUp = 0;
    INT nCount = nLength;
    do {
        if (nCount <= 0) {
            break;
        }
        nBackUp++;
        nCount--;
        if (g_pChitin->field_1A0
            && CUtil::IsDBCSTrailByte(m_sText, nLength - nBack - nBackUp - nPos + field_86A - 1)) {
            nBackUp++;
            nCount--;
        }
        nLineX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nBack - nPos, nCount), TRUE);
    } while (nLineX > nColumnX);

    field_86A += nLength - nBack - nBackUp - nPos;
    m_cVidFont.pRes->Release();
    if (pWarp->GetShiftKey()) {
        field_88E = field_86A;
    } else {
        field_88A = -1;
        field_88E = -1;
    }
    InvalidateRect();
}

// NOTE: Not a function of the original: Page Down in OnKeyDown (0x4DB160).
// Down a page of lines, at the column nearest the cursor's.
void CUIControlEditMultiLine::OnPageDownKey()
{
    CString sWork;
    CWarp* pWarp = m_pPanel->m_pManager->m_pWarp;

    if (field_878 + field_86E >= field_87A) {
        return;
    }

    m_cVidFont.pRes->Demand();

    sWork = m_sText.Left(field_86A);
    CUtil::MakeReverse(sWork);
    INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
    if (nBack == -1) {
        nBack = sWork.GetLength();
    }

    INT nColumnX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nBack, nBack), TRUE);

    INT nPos = 0;
    INT nLines = 0;
    sWork = m_sText.Right(m_sText.GetLength() - field_86A);
    if (field_878 > 0) {
        do {
            INT nLength = CUtil::FindOneOf(sWork.Mid(nPos), CString("\n\r"), 0);
            if (nLength == -1) {
                m_cVidFont.pRes->Release();
                return;
            }
            nLines++;
            nPos += nLength + 1;
        } while (nLines < field_878);
    }

    field_86E += nLines;
    AdjustVisibleIndex();

    INT nLength = CUtil::FindOneOf(sWork.Mid(nPos), CString("\n\r"), 0);
    if (nLength == -1) {
        nLength = sWork.Mid(nPos).GetLength();
    }

    INT nLineX = m_cVidFont.GetStringLength(m_sText.Mid(nPos + field_86A, nLength), TRUE);

    if (pWarp->GetShiftKey() && field_88A == -1) {
        field_88A = field_86A;
    }

    if (nLineX <= nColumnX) {
        field_86A = nLength + nPos + field_86A;
        m_cVidFont.pRes->Release();
        if (pWarp->GetShiftKey()) {
            field_88E = field_86A;
        } else {
            field_88A = -1;
            field_88E = -1;
        }
        InvalidateRect();
        return;
    }

    INT nBackUp = 0;
    INT nCount = nLength;
    do {
        if (nCount < 1) {
            break;
        }
        nBackUp++;
        nCount--;
        if (g_pChitin->field_1A0
            && CUtil::IsDBCSTrailByte(m_sText, nPos + (field_86A - nBackUp) - 1 + nLength)) {
            nBackUp++;
            nCount--;
        }
        nLineX = m_cVidFont.GetStringLength(m_sText.Mid(nPos + field_86A, nCount), TRUE);
    } while (nLineX > nColumnX);

    field_86A = nLength + nPos + (field_86A - nBackUp);
    m_cVidFont.pRes->Release();
    if (pWarp->GetShiftKey()) {
        field_88E = field_86A;
    } else {
        field_88A = -1;
        field_88E = -1;
    }
    InvalidateRect();
}

// NOTE: Not a function of the original: Up in OnKeyDown (0x4DB160). The
// line above, at the column nearest the cursor's.
void CUIControlEditMultiLine::OnUpKey()
{
    CString sWork;
    CWarp* pWarp = m_pPanel->m_pManager->m_pWarp;

    m_cVidFont.pRes->Demand();

    sWork = m_sText.Left(field_86A);
    CUtil::MakeReverse(sWork);
    INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
    if (nBack == -1) {
        m_cVidFont.pRes->Release();
        return;
    }

    if (pWarp->GetShiftKey() && field_88A == -1) {
        field_88A = field_86A;
    }

    sWork = m_sText.Left(field_86A - nBack - 1);
    CUtil::MakeReverse(sWork);
    INT nLength = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
    if (nLength == -1) {
        nLength = sWork.GetLength();
    }

    INT nLineX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nLength - nBack - 1, nLength), TRUE);
    INT nColumnX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nBack, nBack), TRUE);

    if (nLineX <= nColumnX) {
        field_86A += -1 - nBack;
        AdjustVisibleIndex();
        m_cVidFont.pRes->Release();
        InvalidateRect();
        if (pWarp->GetShiftKey()) {
            field_88E = field_86A;
        } else {
            field_88A = -1;
            field_88E = -1;
        }
        return;
    }

    INT nBackUp = 0;
    CUtil::MakeReverse(sWork);
    INT nCount = nLength;
    do {
        if (nCount < 1) {
            break;
        }
        nBackUp++;
        nCount--;
        if (g_pChitin->field_1A0 && CUtil::IsDBCSTrailByte(m_sText, field_86A - nBack - nBackUp - 2)) {
            nBackUp++;
            nCount--;
        }
        nLineX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nLength - nBack - 1, nCount), TRUE);
    } while (nLineX > nColumnX);

    field_86A += -1 - nBack - nBackUp;
    if (pWarp->GetShiftKey()) {
        field_88E = field_86A;
    } else {
        field_88A = -1;
        field_88E = -1;
    }
    AdjustVisibleIndex();
    m_cVidFont.pRes->Release();
    InvalidateRect();
}

// NOTE: Not a function of the original: Down in OnKeyDown (0x4DB160). The
// line below, at the column nearest the cursor's.
void CUIControlEditMultiLine::OnDownKey()
{
    CString sWork;
    CWarp* pWarp = m_pPanel->m_pManager->m_pWarp;

    if (field_86A == m_sText.GetLength()) {
        return;
    }

    m_cVidFont.pRes->Demand();

    sWork = m_sText.Left(field_86A);
    CUtil::MakeReverse(sWork);
    INT nBack = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
    if (nBack == -1) {
        nBack = sWork.GetLength();
    }

    INT nSkip = 0;
    BOOL bAtBreak = FALSE;
    if (!g_pChitin->field_1A0 || !IsDBCSLeadByte(m_sText[field_86A])) {
        char ch = m_sText[field_86A];
        if (ch == '\n' || ch == '\r') {
            nSkip = 1;
            bAtBreak = TRUE;
        }
    }

    if (!bAtBreak) {
        sWork = m_sText.Right(m_sText.GetLength() - field_86A - 1);
        INT nBreak = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
        if (nBreak == -1) {
            m_cVidFont.pRes->Release();
            return;
        }
        nSkip = nBreak + 2;
    }

    sWork = m_sText.Right(m_sText.GetLength() - nSkip - field_86A);
    INT nLength = CUtil::FindOneOf(sWork, CString("\n\r"), 0);
    if (nLength == -1) {
        nLength = sWork.GetLength();
    }

    INT nColumnX = m_cVidFont.GetStringLength(m_sText.Mid(field_86A - nBack, nBack), TRUE);
    INT nLineX = m_cVidFont.GetStringLength(m_sText.Mid(nSkip + field_86A, nLength), TRUE);

    if (pWarp->GetShiftKey() && field_88A == -1) {
        field_88A = field_86A;
    }

    if (nLineX <= nColumnX) {
        field_86A = nLength + field_86A + nSkip;
        if (pWarp->GetShiftKey()) {
            field_88E = field_86A;
        } else {
            field_88A = -1;
            field_88E = -1;
        }
        AdjustVisibleIndex();
        m_cVidFont.pRes->Release();
        InvalidateRect();
        return;
    }

    INT nBackUp = 0;
    INT nCount = nLength;
    do {
        if (nCount < 1) {
            break;
        }
        nBackUp++;
        nCount--;
        if (g_pChitin->field_1A0
            && CUtil::IsDBCSTrailByte(m_sText, nLength + (nSkip - nBackUp) + field_86A - 1)) {
            nBackUp++;
            nCount--;
        }
        nLineX = m_cVidFont.GetStringLength(m_sText.Mid(nSkip + field_86A, nCount), TRUE);
    } while (nLineX > nColumnX);

    field_86A = nLength + field_86A + (nSkip - nBackUp);
    if (pWarp->GetShiftKey()) {
        field_88E = field_86A;
    } else {
        field_88A = -1;
        field_88E = -1;
    }
    AdjustVisibleIndex();
    m_cVidFont.pRes->Release();
    InvalidateRect();
}

// 0x4E05B0
void CUIControlEditMultiLine::OnMouseMove(CPoint pt)
{
    if (!m_bActive || !field_888) {
        return;
    }

    INT x = pt.x - (m_ptOrigin.x + m_ptText.x);
    INT y = pt.y - (m_ptOrigin.y + m_ptText.y);

    if (min(x, m_size.cx) < 1) {
        x = 1;
    } else if (x > m_size.cx) {
        x = m_size.cx;
    }

    // Dragging past the top or the bottom scrolls one line.
    if (y < 0) {
        if (field_86E > 0) {
            field_86E--;
            AdjustVisibleIndex();
        }
        y = 0;
    }

    if (y > m_size.cy) {
        if (field_878 + field_86E < field_87A) {
            field_86E++;
            AdjustVisibleIndex();
        }
        y = m_size.cy - 1;
    }

    m_cVidFont.pRes->Demand();

    INT nColumn = 0;

    INT nRow = field_878;
    if (y / m_cVidFont.GetFontHeight(FALSE) < nRow) {
        nRow = y / m_cVidFont.GetFontHeight(FALSE);
    }

    INT nLine = field_87A;
    if (field_86E + nRow < field_87A) {
        nRow = field_878;
        if (y / m_cVidFont.GetFontHeight(FALSE) < nRow) {
            nLine = field_86E + y / m_cVidFont.GetFontHeight(FALSE);
        } else {
            nLine = field_86E + nRow;
        }
    }

    INT nStart = 0;
    for (INT nCount = 0; nCount < nLine; nCount++) {
        if (nStart >= m_sText.GetLength()) {
            break;
        }

        // NOTE: As in OnLButtonDown (0x4DABD0), the find's result is added
        // before the check for -1.
        nStart += CUtil::FindOneOf(m_sText.Mid(nStart), CString("\n\r"), 0);
        if (nStart == -1) {
            nStart = m_sText.GetLength();
        } else {
            nStart++;
        }
    }

    if (nStart == m_sText.GetLength()) {
        field_88E = m_sText.GetLength();
        field_86A = m_sText.GetLength();
        m_cVidFont.pRes->Release();
    } else {
        INT nLength = CUtil::FindOneOf(m_sText.Mid(nStart), CString("\n\r"), 0);
        if (nLength == -1) {
            nLength = m_sText.GetLength();
        }

        if (x > m_cVidFont.GetStringLength(m_sText.Mid(nStart, nLength), FALSE)) {
            nColumn = nLength + 1;
        } else {
            if (x > m_cVidFont.GetStringLength(m_sText.Mid(nStart, 0), FALSE)) {
                do {
                    if (g_pChitin->field_1A0) {
                        if (IsDBCSLeadByte(m_sText[nStart + nColumn - 1])) {
                            nColumn++;
                        }
                    }

                    nColumn++;

                    // NOTE: The original compares the column with the whole
                    // text's length.
                    if (nColumn == m_sText.GetLength()) {
                        break;
                    }
                } while (x > m_cVidFont.GetStringLength(m_sText.Mid(nStart, nColumn), FALSE));
            }
        }

        m_cVidFont.pRes->Release();

        if (g_pChitin->field_1A0) {
            if (CUtil::IsDBCSTrailByte(m_sText, nStart + nColumn - 1)) {
                nColumn--;
            }
        }

        field_88E = min(nStart + nColumn - 1, m_sText.GetLength());
        field_86A = field_88E;

        InvalidateRect();
    }
}

// 0x4E09F0
BOOL CUIControlEditMultiLine::NeedRender()
{
    return (m_bActive || m_bInactiveRender) && m_nRenderCount > 0;
}

// 0x4E0A20
BOOL CUIControlEditMultiLine::Render(BOOL bForce)
{
    CVidInf* pVidInf = g_pBaldurChitin->pActiveEngine != NULL
        ? static_cast<CVidInf*>(g_pBaldurChitin->pActiveEngine->pVidMode)
        : NULL;
    CString sText;
    CString sLine;

    if (!m_bActive && !m_bInactiveRender) {
        return FALSE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return FALSE;
    }

    if (field_877) {
        m_cVidFont.Load(FALSE);
        SHORT nFontHeight = m_cVidFont.GetFontHeight(FALSE);
        field_877 = 0;
        field_878 = static_cast<SHORT>((m_size.cy - m_ptText.y) / nFontHeight);
    }

    if (m_nRenderCount != 0) {
        CSingleLock renderLock(&(m_pPanel->m_pManager->field_56), FALSE);
        renderLock.Lock(INFINITE);
        m_nRenderCount--;
        renderLock.Unlock();
    }

    INT x = m_ptText.x + m_ptOrigin.x + m_pPanel->m_ptOrigin.x;
    INT y = m_ptText.y + m_pPanel->m_ptOrigin.y + m_ptOrigin.y;

    m_cVidFont.pRes->Demand();

    SHORT nBaseLineHeight = m_cVidFont.GetBaseLineHeight(TRUE);

    x++;
    y++;

    CRect rText(x, y, x + m_size.cx, y + m_size.cy);
    CRect rClip;
    rClip.IntersectRect(rText, m_rDirty);

    if (!pVidInf->BKLock(rClip)) {
        m_cVidFont.pRes->Release();
        return TRUE;
    }

    y -= rClip.top;
    x -= rClip.left;
    rClip.OffsetRect(-rClip.left, -rClip.top);

    // NOTE: The original compares the last character with "\r\n" (0x8ABA78),
    // which one character never equals, so the text is drawn whole.
    if (m_sText.Right(1) == "\r\n"
        && (!g_pChitin->field_1A0 || !CUtil::IsDBCSTrailByte(m_sText, m_sText.GetLength() - 1))) {
        sText = m_sText.Left(m_sText.GetLength() - 1);
    } else {
        sText = m_sText;
    }

    sLine.Empty();

    CPoint ptCursor(-1, -1);

    INT nSelectionEnd = field_88A;
    INT nSelectionStart = field_88E;
    if (nSelectionEnd < nSelectionStart) {
        nSelectionStart = field_88A;
        nSelectionEnd = field_88E;
    }

    if (sText.GetLength() > 0) {
        INT nStart = 0;
        USHORT nLine = 0;
        INT nLength;
        do {
            if (field_878 + field_86E <= nLine) {
                break;
            }

            nLength = CUtil::FindOneOf(sText.Mid(nStart), CString("\n\r"), 0);
            if (nLength == -1) {
                nLength = sText.GetLength();
            }

            sLine += sText.Mid(nStart, nLength);

            if (nStart <= field_86A && field_86A <= nLength + nStart) {
                ptCursor.y = m_cVidFont.GetFontHeight(TRUE) * (nLine - field_86E);
                ptCursor.x = m_cVidFont.GetStringLength(sText.Mid(nStart, field_86A - nStart), TRUE);
            }

            if (field_86E <= nLine) {
                INT nTextY = nBaseLineHeight + y + m_cVidFont.GetFontHeight(TRUE) * (nLine - field_86E);
                INT nEnd = nLength + nStart;
                if (nSelectionEnd == -1 || nSelectionStart == -1) {
                    m_cVidFont.SetColor(m_rgbForegroundColor, m_rgbBackgroundColor, FALSE);
                    pVidInf->BKTextOut(&m_cVidFont, sLine, x, nTextY, rClip, 0, TRUE);
                } else if (nSelectionStart < nStart || nEnd < nSelectionStart) {
                    if (nSelectionEnd < nStart || nEnd < nSelectionEnd) {
                        // The line is all selected or not at all.
                        if (nSelectionStart < nStart && nStart <= nSelectionEnd) {
                            m_cVidFont.SetColor(RGB(200, 60, 60), m_rgbBackgroundColor, FALSE);
                        } else {
                            m_cVidFont.SetColor(m_rgbForegroundColor, m_rgbBackgroundColor, FALSE);
                        }
                        pVidInf->BKTextOut(&m_cVidFont, sLine, x, nTextY, rClip, 0, TRUE);
                    } else {
                        // The selection ends in this line.
                        m_cVidFont.SetColor(RGB(200, 60, 60), m_rgbBackgroundColor, FALSE);
                        INT nSelected = nSelectionEnd - nStart;
                        pVidInf->BKTextOut(&m_cVidFont, sLine.Left(nSelected), x, nTextY, rClip, 0, TRUE);
                        INT nOffset = m_cVidFont.GetStringLength(sLine.Left(nSelected), TRUE);
                        m_cVidFont.SetColor(m_rgbForegroundColor, m_rgbBackgroundColor, FALSE);
                        pVidInf->BKTextOut(&m_cVidFont,
                            sLine.Right(sLine.GetLength() - nSelectionEnd + nStart),
                            nOffset + x,
                            nTextY,
                            rClip,
                            0,
                            TRUE);
                    }
                } else {
                    // The selection starts in this line.
                    m_cVidFont.SetColor(m_rgbForegroundColor, m_rgbBackgroundColor, FALSE);
                    INT nUnselected = nSelectionStart - nStart;
                    pVidInf->BKTextOut(&m_cVidFont, sLine.Left(nUnselected), x, nTextY, rClip, 0, TRUE);
                    m_cVidFont.SetColor(RGB(200, 60, 60), m_rgbBackgroundColor, FALSE);
                    INT nOffset = m_cVidFont.GetStringLength(sLine.Left(nUnselected), TRUE);
                    if (nEnd < nSelectionEnd) {
                        pVidInf->BKTextOut(&m_cVidFont,
                            sLine.Right(sLine.GetLength() - nSelectionStart + nStart),
                            nOffset + x,
                            nTextY,
                            rClip,
                            0,
                            TRUE);
                    } else {
                        INT nSelected = nSelectionEnd - nSelectionStart;
                        pVidInf->BKTextOut(&m_cVidFont,
                            sLine.Mid(nUnselected, nSelected),
                            nOffset + x,
                            nTextY,
                            rClip,
                            0,
                            TRUE);
                        nOffset += m_cVidFont.GetStringLength(sLine.Mid(nUnselected, nSelected), TRUE);
                        m_cVidFont.SetColor(m_rgbForegroundColor, m_rgbBackgroundColor, FALSE);
                        pVidInf->BKTextOut(&m_cVidFont,
                            sLine.Right(sLine.GetLength() - nSelectionEnd + nStart),
                            nOffset + x,
                            nTextY,
                            rClip,
                            0,
                            TRUE);
                    }
                }
            }

            nStart += nLength + 1;
            if (nStart >= sText.GetLength()) {
                nStart = sText.GetLength();
            }

            nLine++;
            sLine.Empty();
        } while (nLength < sText.GetLength());
    }

    m_vcCursor.pRes->Demand();

    CPoint ptCenter;
    m_vcCursor.GetCurrentCenterPoint(ptCenter, TRUE);

    if (m_bFocused) {
        if ((ptCursor.x == -1 || ptCursor.y == -1) && field_86A == 0 && field_86E == 0) {
            ptCursor.x = 0;
            ptCursor.y = 0;
        }

        m_vcCursor.RealizePalette(1);
        pVidInf->BKRender(&m_vcCursor,
            ptCursor.x + ptCenter.x + x,
            ptCenter.y + ptCursor.y + y,
            rClip,
            TRUE,
            0);
    }

    m_vcCursor.pRes->Release();
    m_cVidFont.pRes->Release();
    pVidInf->BKUnlock();

    return TRUE;
}

// 0x4E1580
void CUIControlEditMultiLine::InvalidateRect()
{
    if (m_bActive || m_bInactiveRender) {
        CSingleLock lock(&(m_pPanel->m_pManager->field_56), FALSE);
        lock.Lock(INFINITE);
        m_nRenderCount = CUIManager::RENDER_COUNT;
        lock.Unlock();
    }
}

// 0x4E1610
void CUIControlEditMultiLine::AdjustVisibleIndex()
{
    if (field_87A > field_878) {
        if (field_86A == 0) {
            field_86E = 0;
        }

        if (field_86A != m_sText.GetLength()) {
            int numberOfLines = 0;
            if (m_sText.GetLength() > 0) {
                int start = 0;
                int end;
                do {
                    end = CUtil::FindOneOf(m_sText.Mid(start), CString("\n\r"), 0);
                    if (end == -1) {
                        end = m_sText.GetLength();
                    }

                    if (start <= field_86A && field_86A <= end + start) {
                        break;
                    }

                    start += end + 1;
                    numberOfLines++;

                    if (start >= m_sText.GetLength()) {
                        start = m_sText.GetLength();
                    }
                } while (end < m_sText.GetLength());
            }

            if (field_86E >= numberOfLines || numberOfLines >= field_86E + field_878) {
                if (field_86E < numberOfLines) {
                    field_86E = max(numberOfLines - field_878 + 1, 0);
                } else {
                    field_86E = numberOfLines;
                }
            } else {
                if (field_87A - field_86E < field_878) {
                    field_86E = field_87A - field_878;
                }

                if (field_86E < numberOfLines) {
                    field_86E = max(numberOfLines - field_878 + 1, 0);
                } else {
                    field_86E = numberOfLines;
                }
            }
        } else {
            field_86E = field_87A - field_878;
        }
    } else {
        field_86E = 0;
    }
}

// 0x4E17B0
void CUIControlEditMultiLine::SetText(CString sText)
{
    field_866 = sText;
    if (sText != m_sText) {
        m_sText = sText;
        field_86A = 0;
        if (sText.GetLength() != 0) {
            CalculateNewLines(3);
            InvalidateRect();
        }
    }
}

// -----------------------------------------------------------------------------

// 0x4E1860
CUIControlEditMultiLineScroller::CUIControlEditMultiLineScroller(CUIPanel* panel, UI_CONTROL_EDIT* controlInfo)
    : CUIControlEditMultiLine(panel, controlInfo, 0)
{
    field_896 = 0;
    field_897 = 0;
    field_898 = 0;
    field_899 = 0;
    field_892 = field_36;
}

// 0x4E18D0
CUIControlEditMultiLineScroller::~CUIControlEditMultiLineScroller()
{
}

// 0x4E1990
void CUIControlEditMultiLineScroller::AdjustVisibleIndex()
{
    CUIControlEditMultiLine::AdjustVisibleIndex();

    if (field_892 != -1) {
        CUIControlScrollBar* pScrollBar = static_cast<CUIControlScrollBar*>(m_pPanel->GetControl(field_892));

        // NOTE: Uninline.
        pScrollBar->AdjustScrollBar(field_878, field_878, field_87A);
    }
}

// NOTE: Uninline.
void CUIControlEditMultiLineScroller::OnScroll(SHORT a1, SHORT a2)
{
    if (field_87A < field_878) {
        return;
    }

    // NOTE: As the original's x87 code (inlined in
    // CUIControlEditScrollBar::OnScroll, 0x4E5A2D-0x4E5A6F): the operands as
    // floats, the quotient and the product at the runtime's double
    // precision, truncated.
    int v1 = static_cast<int>(static_cast<double>(static_cast<float>(a1))
        / static_cast<float>(a2)
        * static_cast<float>(field_87A - field_878));

    if (v1 == field_86E || v1 < 0 || v1 > field_87A - field_878) {
        return;
    }

    field_86E = v1;

    if (field_892 != -1) {
        CUIControlEditScrollBar* pScrollBar = static_cast<CUIControlEditScrollBar*>(m_pPanel->GetControl(field_892));

        // NOTE: Uninline.
        pScrollBar->AdjustScrollBar(field_86E, field_87A, field_878);
    }

    InvalidateRect();
}

// NOTE: Uninline.
void CUIControlEditMultiLineScroller::OnScrollUp()
{
    field_86E--;
    if (field_86E < 0) {
        field_86E = 0;
    }

    if (field_892 != -1) {
        CUIControlEditScrollBar* pScrollBar = static_cast<CUIControlEditScrollBar*>(m_pPanel->GetControl(field_892));

        // NOTE: Uninline.
        pScrollBar->AdjustScrollBar(field_86E, field_87A, field_878);
    }

    InvalidateRect();
}

// NOTE: Uninline.
void CUIControlEditMultiLineScroller::OnScrollDown()
{
    field_86E++;
    if (field_86E > field_87A - field_878) {
        field_86E = field_87A - field_878;
    }

    if (field_892 != -1) {
        CUIControlEditScrollBar* pScrollBar = static_cast<CUIControlEditScrollBar*>(m_pPanel->GetControl(field_892));

        // NOTE: Uninline.
        pScrollBar->AdjustScrollBar(field_86E, field_87A, field_878);
    }

    InvalidateRect();
}

// NOTE: Uninline.
void CUIControlEditMultiLineScroller::OnPageUp(DWORD nLines)
{
    SHORT v1 = static_cast<SHORT>(nLines);
    if (v1 > field_878) {
        v1 = field_878;
    }

    field_86E -= v1;
    if (field_86E < 0) {
        field_86E = 0;
    }

    if (field_892 != -1) {
        CUIControlEditScrollBar* pScrollBar = static_cast<CUIControlEditScrollBar*>(m_pPanel->GetControl(field_892));

        // NOTE: Uninline.
        pScrollBar->AdjustScrollBar(field_86E, field_87A, field_878);
    }

    InvalidateRect();
}

// NOTE: Uninline.
void CUIControlEditMultiLineScroller::OnPageDown(DWORD nLines)
{
    if (field_87A < field_878) {
        field_86E = 0;
    } else {
        SHORT v1 = static_cast<SHORT>(nLines);
        if (v1 > field_878) {
            v1 = field_878;
        }

        field_86E += v1;
        if (field_86E > field_87A - field_878) {
            field_86E = field_87A - field_878;
        }
    }

    if (field_892 != -1) {
        CUIControlEditScrollBar* pScrollBar = static_cast<CUIControlEditScrollBar*>(m_pPanel->GetControl(field_892));

        // NOTE: Uninline.
        pScrollBar->AdjustScrollBar(field_86E, field_87A, field_878);
    }

    InvalidateRect();
}
