#include "CSwitchCDStatus.h"

#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CDimm.h"
#include "CGameArea.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CSound.h"
#include "CUtil.h"
#include "CVidInf.h"
#include "CVidMode.h"
#include "CVideo.h"
#include "CWarp.h"

// The message layout, in pixels from the screen's top left: .data constants
// the original keeps for this class alone (only 0x54D2xx-0x54E1xx read them).
// The names are guessed from where Update draws with them. #guess
static int s_nPanelX = 280; // 0x8AE4A0
static int s_nPanelY = 228; // 0x8AE4A4
static int s_nTitleY = 276; // 0x8AE4A8
static int s_nLine1Y = 291; // 0x8AE4AC
static int s_nLine2Y = 306; // 0x8AE4B0
static int s_nLine3Y = 321; // 0x8AE4B4
static int s_nLine4Y = 10; // 0x8AE4B8

// The wrapped message block: width and height, set by the compiler-generated
// initialisers at 0x54D240 (position) and 0x54D260 (size). #guess
static int s_nTextX = 0x23; // 0x8E0320
static int s_nTextY = 0x221; // 0x8E0324
static int s_nTextSize[2] = { 0x2FD, 0x30 }; // 0x8E0330, 0x8E0334

// 0x54D280
CSwitchCDStatus::CSwitchCDStatus()
{
    field_0 = 0;
    field_4 = 0;
    m_bActivateEngine = FALSE;
}

// 0x54D360
CSwitchCDStatus::~CSwitchCDStatus()
{
    if (field_4 != 0) {
        field_2D8.Release();
        field_440.Release();
        // The original releases the fonts' CResCell directly (0x77F820);
        // CVidCell::Release(BOOLEAN&) hides the helper's no-argument one.
        field_4F4.CResHelper<CResCell, 1000>::Release();
        field_9F4.CResHelper<CResCell, 1000>::Release();
        field_4 = 0;
    }
}

// 0x54D460
void CSwitchCDStatus::Init()
{
    int x;
    int y;

    if (g_pBaldurChitin->field_4A28 == 0) {
        x = (CVideo::SCREENWIDTH - 800) / 2;
        y = (CVideo::SCREENHEIGHT - 600) / 2;
    } else {
        s_nPanelX *= 2;
        s_nPanelY *= 2;
        s_nTitleY *= 2;
        s_nLine1Y *= 2;
        s_nLine2Y *= 2;
        s_nLine3Y *= 2;
        s_nLine4Y *= 2;
        s_nTextX *= 2;
        s_nTextY *= 2;
        s_nTextSize[0] *= 2;
        s_nTextSize[1] *= 2;
        x = (CVideo::SCREENWIDTH >> 1) - 800;
        y = (CVideo::SCREENHEIGHT >> 1) - 600;
    }

    s_nPanelX += x;
    s_nPanelY += y;
    s_nTitleY += y;
    s_nLine1Y += y;
    s_nLine2Y += y;
    s_nLine3Y += y;
    s_nTextX += x;
    s_nTextY += y;
}

// 0x54D5A0
void CSwitchCDStatus::Update(BOOLEAN a1, BOOLEAN a2, BYTE a3, const CString& a4, BOOLEAN a5, BOOLEAN a6, BOOLEAN a7)
{
    STR_RES strRes;
    CString sOutputString;
    CRect clipRect;
    CRect mosaicRect;
    CSize mosSize;
    INT x;
    INT y;
    INT nScreenWidth;
    INT nScreenHeight;

    // NOTE: The original constructs a CSound here and never uses it; only its
    // constructor and destructor run (0x7A8BB0 / 0x7A8E20).
    CSound sound;

    CSingleLock renderLock(&field_EF8, FALSE);
    renderLock.Lock(INFINITE);

    if (!g_pChitin->cDimm.m_bCDSwitchActivated && a2 == TRUE) {
        renderLock.Unlock();
        return;
    }

    CBaldurEngine* pEngine = g_pBaldurChitin->GetActiveEngine();
    if (pEngine == NULL || pEngine->pVidMode == NULL) {
        renderLock.Unlock();
        return;
    }

    if ((a1 && a2 != TRUE) || field_4 != 0) {
        if (g_pBaldurChitin->field_4A28 == 0) {
            x = (CVideo::SCREENWIDTH - 800) / 2;
            y = (CVideo::SCREENHEIGHT - 600) / 2;
        } else {
            x = (CVideo::SCREENWIDTH >> 1) - 800;
            y = (CVideo::SCREENHEIGHT >> 1) - 600;
        }

        nScreenWidth = CVideo::SCREENWIDTH;
        nScreenHeight = CVideo::SCREENHEIGHT;

        clipRect.left = 0;
        clipRect.top = 0;
        clipRect.right = nScreenWidth;
        clipRect.bottom = nScreenHeight;

        if (a2 == FALSE) {
            while (g_pBaldurChitin->field_F8 == TRUE) {
                SleepEx(50, FALSE);
            }

            if (a1 == TRUE) {
                for (int side = 0; side < 4; side++) {
                    // NOTE: Inlining.
                    UI_PANELHEADER* panelInfo = side < 4
                        ? &(g_pBaldurChitin->m_aBorderPanels[side])
                        : NULL;

                    field_8[side].SetResRef(CResRef(panelInfo->refMosaic), FALSE, TRUE);
                    field_8[side].m_bDoubleSize = g_pBaldurChitin->field_4A28;
                }

                field_2D8.SetResRef(CResRef("GTRCDBG"), FALSE, TRUE);
                field_2D8.m_bDoubleSize = g_pBaldurChitin->field_4A28;

                field_440.SetResRef(CResRef("GTRCDREQ"), FALSE, TRUE);
                field_440.m_bDoubleSize = g_pBaldurChitin->field_4A28;

                field_4F4.SetResRef(CResRef("NORMAL"), g_pBaldurChitin->field_4A28, FALSE);
                field_4F4.RegisterFont();
                field_4F4.SetColor(RGB(0xE6, 0xE6, 0xE6), RGB(0x1E, 0x1E, 0x1E), FALSE);

                field_9F4.SetResRef(CResRef("NORMAL"), g_pBaldurChitin->field_4A28, FALSE);
                field_9F4.RegisterFont();
                field_9F4.SetColor(RGB(0xE6, 0xE6, 0xE6), RGB(0x1E, 0x1E, 0x1E), FALSE);

                for (int side = 0; side < 4; side++) {
                    if (field_8[side].GetRes() != NULL) {
                        field_8[side].GetRes()->Demand();
                    }
                }

                field_2D8.GetRes()->Demand();
                field_440.GetRes()->Demand();
                field_4F4.GetRes()->Demand();
                field_9F4.GetRes()->Demand();

                g_pBaldurChitin->GetObjectCursor()->SetCursor(100, FALSE);
                g_pBaldurChitin->GetObjectCursor()->m_bVisible = FALSE;
                g_pBaldurChitin->GetObjectCursor()->CursorUpdate(pEngine->pVidMode);

                field_4 = TRUE;
                field_0 = 0;
            } else {
                for (int side = 0; side < 4; side++) {
                    if (field_8[side].GetRes() != NULL) {
                        field_8[side].GetRes()->Release();
                    }
                }

                field_2D8.GetRes()->Release();
                field_440.GetRes()->Release();

                field_4F4.GetRes()->Release();
                field_4F4.Unload();
                field_9F4.GetRes()->Release();
                field_9F4.Unload();

                field_4 = FALSE;

                if (m_bActivateEngine) {
                    m_bActivateEngine = FALSE;

                    if (pEngine != NULL) {
                        pEngine->EngineActivated();
                    }

                    g_pChitin->Resume();
                }

                while (g_pBaldurChitin->m_bReInitializing) {
                    SleepEx(25, FALSE);
                }

                if (!g_pChitin->cProgressBar.m_bProgressBarActivated
                    && g_pBaldurChitin->GetObjectGame()->m_bGameLoaded == 0) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(0, TRUE);
                    g_pBaldurChitin->GetObjectCursor()->m_bVisible = TRUE;
                    g_pBaldurChitin->GetObjectCursor()->CursorUpdate(pEngine->pVidMode);
                }

                pEngine->GetManager()->InvalidateRect(NULL);

                field_0 = 0;
            }
        } else {
            if (field_4 == TRUE && field_0 < 2 && !g_pBaldurChitin->m_bReInitializing) {
                for (int side = 0; side < 4; side++) {
                    // NOTE: Inlining.
                    UI_PANELHEADER* panelInfo = side < 4
                        ? &(g_pBaldurChitin->m_aBorderPanels[side])
                        : NULL;

                    if (g_pBaldurChitin->field_4A28) {
                        mosaicRect.left = 0;
                        mosaicRect.top = 0;
                        mosaicRect.right = 2 * panelInfo->nWidth;
                        mosaicRect.bottom = 2 * panelInfo->nHeight;
                        if (panelInfo->nWidth > 0 && panelInfo->nHeight > 0) {
                            field_8[side].Render(CVIDINF_SURFACE_BACK,
                                2 * panelInfo->x,
                                2 * panelInfo->y,
                                mosaicRect,
                                clipRect,
                                0x1,
                                TRUE);
                        }
                    } else {
                        mosaicRect.left = 0;
                        mosaicRect.top = 0;
                        mosaicRect.right = panelInfo->nWidth;
                        mosaicRect.bottom = panelInfo->nHeight;
                        if (panelInfo->nWidth > 0 && panelInfo->nHeight > 0) {
                            field_8[side].Render(CVIDINF_SURFACE_BACK,
                                panelInfo->x,
                                panelInfo->y,
                                mosaicRect,
                                clipRect,
                                0x1,
                                TRUE);
                        }
                    }
                }

                mosaicRect.left = 0;
                mosaicRect.top = 0;
                mosaicRect.right = nScreenWidth;
                mosaicRect.bottom = nScreenHeight;

                field_2D8.Render(CVIDINF_SURFACE_BACK,
                    x,
                    y,
                    mosaicRect,
                    clipRect,
                    0,
                    TRUE);

                field_440.GetSize(mosSize, TRUE);

                mosaicRect.left = 0;
                mosaicRect.top = 0;
                mosaicRect.right = mosSize.cx;
                mosaicRect.bottom = mosSize.cy;

                field_440.Render(CVIDINF_SURFACE_BACK,
                    s_nPanelX,
                    s_nPanelY,
                    mosaicRect,
                    clipRect,
                    0x1,
                    TRUE);

                CVidInf* pVidInf = static_cast<CVidInf*>(pEngine->pVidMode);
                if (pVidInf->BKLock(clipRect)) {
                    g_pBaldurChitin->GetTlkTable().Fetch(0x407D, strRes);
                    sOutputString.Format(strRes.szText);
                    pVidInf->BKTextOut(&field_4F4,
                        sOutputString,
                        (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                        s_nTitleY,
                        clipRect,
                        0,
                        TRUE);

                    if (a5 == FALSE) {
                        g_pBaldurChitin->GetTlkTable().Fetch(a7 == TRUE ? 0x40EB : 0x40EC, strRes);
                        sOutputString.Format("%s %s", strRes.szText, a4);
                        pVidInf->BKTextOut(&field_4F4,
                            sOutputString,
                            (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                            s_nLine1Y,
                            clipRect,
                            0,
                            TRUE);

                        if (a6 == FALSE) {
                            g_pBaldurChitin->GetTlkTable().Fetch(0x407A, strRes);
                            sOutputString.Format("%s %01d", strRes.szText, a3);
                        } else {
                            g_pBaldurChitin->GetTlkTable().Fetch(0x407B, strRes);
                            sOutputString.Format(strRes.szText);
                        }

                        pVidInf->BKTextOut(&field_4F4,
                            sOutputString,
                            (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                            s_nLine2Y,
                            clipRect,
                            0,
                            TRUE);

                        g_pBaldurChitin->GetTlkTable().Fetch(0x407C, strRes);
                        sOutputString.Format("%s %s", strRes.szText, a4);
                        pVidInf->BKTextOut(&field_4F4,
                            sOutputString,
                            (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                            s_nLine3Y,
                            clipRect,
                            0,
                            TRUE);

                        g_pBaldurChitin->GetTlkTable().Fetch(0x50E0, strRes);
                        sOutputString.Format(strRes.szText);
                        DrawTextBlock(pVidInf,
                            s_nTextX,
                            s_nTextY,
                            s_nTextSize,
                            &clipRect,
                            sOutputString,
                            &field_9F4,
                            0x148);
                    } else {
                        if (a6 == FALSE) {
                            g_pBaldurChitin->GetTlkTable().Fetch(0x407A, strRes);
                            sOutputString.Format("%s %01d", strRes.szText, a3);
                        } else {
                            g_pBaldurChitin->GetTlkTable().Fetch(0x407B, strRes);
                            sOutputString.Format(strRes.szText);
                        }

                        pVidInf->BKTextOut(&field_4F4,
                            sOutputString,
                            (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                            s_nLine1Y,
                            clipRect,
                            0,
                            TRUE);

                        g_pBaldurChitin->GetTlkTable().Fetch(0x407C, strRes);
                        sOutputString.Format("%s %s", strRes.szText, a4);
                        pVidInf->BKTextOut(&field_4F4,
                            sOutputString,
                            (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                            s_nLine2Y,
                            clipRect,
                            0,
                            TRUE);

                        g_pBaldurChitin->GetTlkTable().Fetch(0xAF2, strRes);
                        sOutputString.Format("%s", strRes.szText);
                        pVidInf->BKTextOut(&field_4F4,
                            sOutputString,
                            (CVideo::SCREENWIDTH - field_4F4.GetStringLength(sOutputString, TRUE)) / 2,
                            s_nLine4Y + s_nLine3Y,
                            clipRect,
                            0,
                            TRUE);
                    }

                    pVidInf->BKUnlock();
                }

                field_0 = 1;

                if (!g_pBaldurChitin->field_F8 && !g_pBaldurChitin->m_bReInitializing) {
                    pEngine->pVidMode->Flip(FALSE);
                } else {
                    field_0 = 0;
                }
            }
        }
    }
}

// 0x54E220
void CSwitchCDStatus::DrawTextBlock(CVidInf* pVidInf, int x, int y, int* pSize, CRect* pClip, const CString& sText, CVidFont* pFont, WORD nFlags)
{
    RECT rect;
    RECT intersected;

    rect.left = x;
    rect.top = y;
    rect.right = pSize[0] + x;
    rect.bottom = pSize[1] + y;

    IntersectRect(&intersected, &rect, pClip);

    rect.left = intersected.left;
    rect.right = intersected.right;
    rect.top = intersected.top;
    rect.bottom = intersected.bottom;

    SHORT nFontHeight = pFont->GetFontHeight(TRUE);
    if ((nFlags & 0x100) == 0 || pSize[1] < nFontHeight) {
        nFontHeight = static_cast<SHORT>(pSize[1]);
    }

    int nLines = pSize[1] / nFontHeight;
    CString* pLines = new CString[nLines];
    BYTE nUsed = CUtil::SplitString(pFont,
        sText,
        static_cast<WORD>(pSize[0]),
        static_cast<BYTE>(nLines),
        pLines,
        FALSE,
        TRUE,
        TRUE,
        0xFFFF);

    if ((nFlags & 0x20) == 0) {
        if ((nFlags & 0x80) == 0) {
            y += (pSize[1] >> 1) - ((static_cast<int>(nUsed) * nFontHeight) >> 1);
        } else {
            y += pSize[1] - static_cast<int>(nUsed) * nFontHeight;
        }
    }

    SHORT nBaseLine = pFont->GetBaseLineHeight(TRUE);
    // The left-aligned flag (0x8) only resets x to the block's x, which is
    // where it starts.
    int nLineX = x;
    for (int n = 0; n < nUsed; n++) {
        if ((nFlags & 0x10) == 0) {
            if ((nFlags & 4) != 0) {
                nLineX = 0;
                int nLength = pFont->GetStringLength(pLines[n], TRUE);
                if (-1 < (pSize[0] >> 1) - (nLength >> 1)) {
                    nLength = pFont->GetStringLength(pLines[n], TRUE);
                    nLineX = (pSize[0] >> 1) - (nLength >> 1);
                }
                nLineX = nLineX + x;
            }
        } else {
            int nLength = pFont->GetStringLength(pLines[n], TRUE);
            if (pSize[0] - nLength < 0) {
                nLineX = x;
            } else {
                nLength = pFont->GetStringLength(pLines[n], TRUE);
                nLineX = (pSize[0] - nLength) + x;
            }
        }

        pVidInf->BKTextOut(pFont, pLines[n], nLineX, nBaseLine + y, rect, 0, TRUE);
        y += nFontHeight;
    }

    delete[] pLines;
}
