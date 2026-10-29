#include "CVidBlitter.h"

#include "CChitin.h"
#include "CUtil.h"
#include "CWarp.h"

#include <cstring>

// 0x7B6780
CVidBlitter::CVidBlitter()
{
    m_bSoftSrcKeyBltFast = FALSE;
    m_bSoftBltFast = FALSE;
    m_bSoftSrcKeyBlt = FALSE;
    m_bSoftBlt = FALSE;
    m_bSoftMirrorBlt = FALSE;
    field_E6 = 0;
    m_pSrcSurface = NULL;
    m_pDestSurface = NULL;
    m_pfnBltFast = &CVidBlitter::DxBltFast;
    m_pfnBltNoFX = &CVidBlitter::DxBlt;
    m_pfnSrcKeyBltFast = &CVidBlitter::DxBltFast;
    m_pfnSrcKeyBltNoFX = &CVidBlitter::DxBlt;
    m_pfnBltFX = &CVidBlitter::DxBlt;
    m_pfnSrcKeyBltFX = &CVidBlitter::DxBlt;
}

// #binary-identical
// 0x7B67E0
HRESULT CVidBlitter::DxBlt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    return lpDDSurface->Blt(lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
}

// #binary-identical
// 0x7B6810
HRESULT CVidBlitter::DxBltFast(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    return lpDDSurface->BltFast(lpDestRect->left, lpDestRect->top, lpDDSrcSurface, lpSrcRect, dwFlags);
}

// #binary-identical
// 0x7B6960
void CVidBlitter::Init()
{
    if (g_pChitin->cVideo.m_nBpp != 16) {
        m_pfnBltNoFX = &CVidBlitter::DxBlt;
        m_pfnSrcKeyBltNoFX = &CVidBlitter::DxBlt;
        m_pfnBltFX = &CVidBlitter::DxBlt;
        m_pfnSrcKeyBltFX = &CVidBlitter::DxBlt;
        m_pfnBltFast = &CVidBlitter::DxBltFast;
        m_pfnSrcKeyBltFast = &CVidBlitter::DxBltFast;
    } else {
        if (m_bSoftMirrorBlt) {
            m_pfnBltFX = &CVidBlitter::SoftBlt;
            m_pfnSrcKeyBltFX = &CVidBlitter::SoftSrcKeyBlt;
        } else {
            if (m_bSoftSrcKeyBlt) {
                m_pfnBltFX = &CVidBlitter::DxBlt;
                m_pfnSrcKeyBltFX = &CVidBlitter::SoftSrcKeyBlt;
            } else {
                m_pfnBltFX = &CVidBlitter::DxBlt;
                m_pfnSrcKeyBltFX = &CVidBlitter::DxBlt;
            }
        }

        if (m_bSoftSrcKeyBltFast) {
            m_pfnSrcKeyBltFast = &CVidBlitter::SoftSrcKeyBlt;
        } else {
            m_pfnSrcKeyBltFast = &CVidBlitter::DxBltFast;
        }

        if (m_bSoftSrcKeyBlt) {
            m_pfnSrcKeyBltNoFX = &CVidBlitter::SoftSrcKeyBlt;
        } else {
            m_pfnSrcKeyBltNoFX = &CVidBlitter::DxBlt;
        }

        if (m_bSoftBltFast) {
            m_pfnBltFast = &CVidBlitter::SoftBlt;
        } else {
            m_pfnBltFast = &CVidBlitter::DxBltFast;
        }

        if (m_bSoftBlt) {
            m_pfnBltNoFX = &CVidBlitter::SoftBlt;
        } else {
            m_pfnBltNoFX = &CVidBlitter::DxBlt;
        }
    }
}

// 0x7B6A40
void CVidBlitter::Reset()
{
    if (g_pChitin->pActiveEngine != NULL) {
        g_pChitin->pActiveEngine->EngineDeactivated();
    }

    g_pChitin->m_bReInitializing = TRUE;

    CVidMode* pVidMode = g_pChitin->cVideo.m_pCurrentVidMode;
    g_pChitin->cVideo.CleanUp();
    g_pChitin->cVideo.m_pCurrentVidMode = pVidMode;

    if (!g_pChitin->cVideo.Initialize(g_pChitin->cWnd.GetSafeHwnd(), g_pChitin->m_bFullscreen)) {
        // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidSoftBlt.cpp
        // __LINE__: 342
        UTIL_ASSERT_MSG(FALSE, "Software Blitter re-initialization failed.");
    }

    g_pChitin->Resume();

    if (g_pChitin->pActiveEngine != NULL) {
        g_pChitin->pActiveEngine->EngineActivated();
    }

    field_E6 = 0;
}

// 0x7B6840
HRESULT CVidBlitter::Blt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    if ((dwFlags & DDBLT_DDFX) != 0) {
        if ((dwFlags & DDBLT_KEYSRC) != 0) {
            return (this->*m_pfnSrcKeyBltFX)(lpDDSurface, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
        } else {
            return (this->*m_pfnBltFX)(lpDDSurface, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
        }
    } else {
        if ((dwFlags & DDBLT_KEYSRC) != 0) {
            return (this->*m_pfnSrcKeyBltNoFX)(lpDDSurface, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
        } else {
            return (this->*m_pfnBltNoFX)(lpDDSurface, lpDestRect, lpDDSrcSurface, lpSrcRect, dwFlags, lpDDBltFx);
        }
    }
}

// 0x7B68B0
HRESULT CVidBlitter::BltFast(IDirectDrawSurface* lpDDSurface, DWORD dwX, DWORD dwY, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans)
{
    if ((dwTrans & DDBLTFAST_SRCCOLORKEY) != 0) {
        RECT dest;
        dest.left = dwX;
        dest.top = dwY;
        dest.right = dwX + lpSrcRect->right - lpSrcRect->left;
        dest.bottom = dwY + lpSrcRect->bottom - lpSrcRect->top;
        return (this->*m_pfnSrcKeyBltFast)(lpDDSurface, &dest, lpDDSrcSurface, lpSrcRect, dwTrans, NULL);
    } else {
        RECT dest;
        dest.left = dwX;
        dest.top = dwY;
        dest.right = dwX + lpSrcRect->right - lpSrcRect->left;
        dest.bottom = dwY + lpSrcRect->bottom - lpSrcRect->top;
        return (this->*m_pfnBltFast)(lpDDSurface, &dest, lpDDSrcSurface, lpSrcRect, dwTrans, NULL);
    }
}

// The two surfaces' lock/unlock pair the software blits share: the blitter's
// own DDSURFACEDESC blocks (+0x06 for the destination, +0x72 for the source)
// take the locks, and their lpSurface (dest +0x2A, src +0x96) is copied into
// m_pDestSurface (+0xE2) and m_pSrcSurface (+0xDE). A surface that is still
// drawing restores the surfaces and retries. The HRESULTs are mapped to the
// codes the original answers (0x7B6B20): E_OUTOFMEMORY and the invalid-
// parameter family become DDERR_OUTOFMEMORY / DDERR_INVALIDPARAMS, anything
// else DDERR_GENERIC.
//
// 0x7B6B20
int CVidBlitter::SoftLockSurfaces(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect)
{
    HRESULT hr;

    do {
        while (true) {
            m_destDesc.dwSize = sizeof(DDSURFACEDESC);
            hr = lpDDSurface->Lock(lpDestRect, &m_destDesc, DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH, NULL);

            if (hr != DDERR_WASSTILLDRAWING) {
                break;
            }

            // NOTE: The original calls through the current video mode's
            // RestoreSurfaces (vtable +0x100) here.
            g_pChitin->GetCurrentVideoMode()->RestoreSurfaces();
        }
    } while (hr == DDERR_SURFACEBUSY);

    if (hr != DD_OK) {
        return hr;
    }

    do {
        while (true) {
            m_srcDesc.dwSize = sizeof(DDSURFACEDESC);
            hr = lpDDSrcSurface->Lock(lpSrcRect, &m_srcDesc, DDSD_CAPS | DDSD_LPSURFACE, NULL);

            if (hr != DDERR_WASSTILLDRAWING) {
                break;
            }

            g_pChitin->GetCurrentVideoMode()->RestoreSurfaces();
        }
    } while (hr == DDERR_SURFACEBUSY);

    if (hr != DD_OK) {
        lpDDSurface->Unlock(m_destDesc.lpSurface);

        // NOTE: The original's mapping (0x7B6B20); the engine only ever
        // reports the first failure it meets.
        switch (hr) {
        case DDERR_OUTOFMEMORY:
            return DDERR_OUTOFMEMORY;
        case DDERR_INVALIDPARAMS:
        case DDERR_INVALIDOBJECT:
            return DDERR_INVALIDPARAMS;
        default:
            return DDERR_GENERIC;
        }
    }

    m_pDestSurface = m_destDesc.lpSurface;
    m_pSrcSurface = m_srcDesc.lpSurface;

    return DD_OK;
}

// One row of the destination from one row of the source, mirrored left to
// right: the 16-bit pixels in the opposite order. The original's dword loop
// (0x7B6CC0) stores each source dword with its halves swapped, which is the
// same sequence; it splits that loop by the destination's alignment and
// asserts on an odd one - the per-pixel form needs neither.
//
// 0x7B6CC0
void CVidBlitter::SoftMirrorRow(BYTE* pDest, const BYTE* pSrc, DWORD nBytes)
{
    WORD* pDestPixels = reinterpret_cast<WORD*>(pDest);
    const WORD* pSrcPixels = reinterpret_cast<const WORD*>(pSrc);
    DWORD nPixels = nBytes / 2;

    for (DWORD index = 0; index < nPixels; index++) {
        pDestPixels[index] = pSrcPixels[nPixels - 1 - index];
    }
}

// The same mirror, with the source's key: a pixel equal to the key leaves the
// destination's pixel as it was (0x7B6DC0's per-half writes).
//
// 0x7B6DC0
void CVidBlitter::SoftMirrorKeyRow(BYTE* pDest, const BYTE* pSrc, DWORD nBytes, WORD nKey)
{
    WORD* pDestPixels = reinterpret_cast<WORD*>(pDest);
    const WORD* pSrcPixels = reinterpret_cast<const WORD*>(pSrc);
    DWORD nPixels = nBytes / 2;

    for (DWORD index = 0; index < nPixels; index++) {
        WORD nPixel = pSrcPixels[nPixels - 1 - index];

        if (nPixel != nKey) {
            pDestPixels[index] = nPixel;
        }
    }
}

// A row with the source's key, left to right (0x7B6F20).
//
// 0x7B6F20
void CVidBlitter::SoftKeyRow(BYTE* pDest, const BYTE* pSrc, DWORD nBytes, WORD nKey)
{
    WORD* pDestPixels = reinterpret_cast<WORD*>(pDest);
    const WORD* pSrcPixels = reinterpret_cast<const WORD*>(pSrc);
    DWORD nPixels = nBytes / 2;

    for (DWORD index = 0; index < nPixels; index++) {
        WORD nPixel = pSrcPixels[index];

        if (nPixel != nKey) {
            pDestPixels[index] = nPixel;
        }
    }
}

// The DDBLT_COLORFILL form: the destination rect filled with the DDBLTFX's
// 16-bit fill colour, two pixels a dword (0x7B7070). With no destination rect
// the original reads the surface's own size with GetSurfaceDesc, which our
// shim does not have; Lock(NULL) returns the whole surface instead, the same
// rectangle.
//
// 0x7B7070
HRESULT CVidBlitter::SoftColorFill(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, LPDDBLTFX lpDDBltFx)
{
    RECT rDest;
    DWORD nWidth;
    LONG nHeight;

    if (lpDestRect == NULL) {
        rDest.left = 0;
        rDest.top = 0;
        rDest.right = 0;
        rDest.bottom = 0;
        nWidth = 0;
        nHeight = 0;
    } else {
        rDest = *lpDestRect;
        nWidth = static_cast<DWORD>(lpDestRect->right - lpDestRect->left);
        nHeight = lpDestRect->bottom - lpDestRect->top;
    }

    HRESULT hr;

    do {
        while (true) {
            m_destDesc.dwSize = sizeof(DDSURFACEDESC);
            hr = lpDDSurface->Lock(lpDestRect, &m_destDesc, DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH, NULL);

            if (hr != DDERR_WASSTILLDRAWING) {
                break;
            }

            g_pChitin->GetCurrentVideoMode()->RestoreSurfaces();
        }
    } while (hr == DDERR_SURFACEBUSY);

    if (hr != DD_OK) {
        return hr;
    }

    if (lpDestRect == NULL) {
        nWidth = m_destDesc.dwWidth;
        nHeight = static_cast<LONG>(m_destDesc.dwHeight);
    }

    m_pDestSurface = m_destDesc.lpSurface;

    if (nHeight > 0) {
        BYTE* pRow = static_cast<BYTE*>(m_pDestSurface);
        LONG lPitch = m_destDesc.lPitch;
        WORD nColor = static_cast<WORD>(lpDDBltFx->dwFillColor);

        for (LONG y = 0; y < nHeight; y++) {
            WORD* pPixels = reinterpret_cast<WORD*>(pRow);

            for (DWORD x = 0; x < nWidth; x++) {
                pPixels[x] = nColor;
            }

            pRow += lPitch;
        }
    }

    hr = lpDDSurface->Unlock(m_destDesc.lpSurface);

    switch (hr) {
    case DD_OK:
        return DD_OK;
    case DDERR_OUTOFMEMORY:
        return DDERR_OUTOFMEMORY;
    case DDERR_INVALIDPARAMS:
    case DDERR_INVALIDOBJECT:
        return DDERR_INVALIDPARAMS;
    default:
        return DDERR_GENERIC;
    }
}

// The plain software blit of a 16-bit surface: the destination rect from the
// source rect's size, both surfaces locked, then the rows copied - a dword at
// a time with the odd pixel's bytes in the tail - honouring the mirror flags
// of a DDBLT_DDFX request (left-right through the dword-swapping row copy,
// up-down by starting the source on its last row and walking it backwards).
//
// 0x7B7280
HRESULT CVidBlitter::SoftBlt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    if ((dwFlags & DDBLT_COLORFILL) != 0) {
        // NOTE: The original passes the DDBLTFX as it came (0x7B7070).
        return SoftColorFill(lpDDSurface, lpDestRect, lpDDBltFx);
    }

    DWORD nWidth = static_cast<DWORD>(lpSrcRect->right - lpSrcRect->left);
    LONG nHeight = lpSrcRect->bottom - lpSrcRect->top;

    RECT rDest;
    rDest.left = lpDestRect->left;
    rDest.top = lpDestRect->top;
    rDest.right = lpDestRect->left + static_cast<LONG>(nWidth);
    rDest.bottom = lpDestRect->top + nHeight;

    HRESULT hr = SoftLockSurfaces(lpDDSurface, &rDest, lpDDSrcSurface, lpSrcRect);
    if (hr != DD_OK) {
        return hr;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidSoftBlt.cpp
    // __LINE__: 1340
    UTIL_ASSERT(m_pSrcSurface != NULL);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidSoftBlt.cpp
    // __LINE__: 1341
    UTIL_ASSERT(m_pDestSurface != NULL);

    DWORD nWidthBytes = nWidth * 2;
    LONG nDestPitch = m_destDesc.lPitch >> 1;
    LONG nSrcPitch = m_srcDesc.lPitch >> 1;

    if ((dwFlags & DDBLT_DDFX) != 0) {
        DWORD dwDDFX = lpDDBltFx->dwDDFX;

        if ((dwDDFX & DDBLTFX_MIRRORLEFTRIGHT) != 0) {
            if ((dwDDFX & DDBLTFX_MIRRORUPDOWN) == 0) {
                for (; nHeight > 0; nHeight--) {
                    SoftMirrorRow(static_cast<BYTE*>(m_pDestSurface), static_cast<const BYTE*>(m_pSrcSurface), nWidthBytes);

                    m_pDestSurface = static_cast<BYTE*>(m_pDestSurface) + nDestPitch * 2;
                    m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + nSrcPitch * 2;
                }

                lpDDSurface->Unlock(m_destDesc.lpSurface);
            } else {
                m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + (nHeight - 1) * nSrcPitch * 2;

                for (; nHeight > 0; nHeight--) {
                    SoftMirrorRow(static_cast<BYTE*>(m_pDestSurface), static_cast<const BYTE*>(m_pSrcSurface), nWidthBytes);

                    m_pDestSurface = static_cast<BYTE*>(m_pDestSurface) + nDestPitch * 2;
                    m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) - nSrcPitch * 2;
                }

                lpDDSurface->Unlock(m_destDesc.lpSurface);
            }

            lpDDSurface->Unlock(m_srcDesc.lpSurface);

            return DD_OK;
        }

        if ((dwDDFX & DDBLTFX_MIRRORUPDOWN) != 0) {
            m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + (nHeight - 1) * nSrcPitch * 2;
            nSrcPitch = -nSrcPitch;
        }
    }

    for (; nHeight > 0; nHeight--) {
        // NOTE: The original splits this into a dword loop and the odd
        // pixel's bytes; memcpy stores the same bytes.
        std::memcpy(m_pDestSurface, m_pSrcSurface, nWidthBytes);

        m_pDestSurface = static_cast<BYTE*>(m_pDestSurface) + nDestPitch * 2;
        m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + nSrcPitch * 2;
    }

    lpDDSurface->Unlock(m_destDesc.lpSurface);
    lpDDSurface->Unlock(m_srcDesc.lpSurface);

    return DD_OK;
}

// The keyed software blit: the same skeleton, with the source surface's
// DDCKEY_SRCBLT colour key (its low word in both halves of the compared
// dword) and the keyed row copies, so a source pixel equal to the key leaves
// the destination's pixel untouched. Mirrored left-right through the keyed
// mirror row copy.
//
// 0x7B74F0
HRESULT CVidBlitter::SoftSrcKeyBlt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    DWORD nWidth = static_cast<DWORD>(lpSrcRect->right - lpSrcRect->left);
    LONG nHeight = lpSrcRect->bottom - lpSrcRect->top;

    RECT rDest;
    rDest.left = lpDestRect->left;
    rDest.top = lpDestRect->top;
    rDest.right = lpDestRect->left + static_cast<LONG>(nWidth);
    rDest.bottom = lpDestRect->top + nHeight;

    DDCOLORKEY ddColorKey;
    ddColorKey.dwColorSpaceLowValue = 0;
    ddColorKey.dwColorSpaceHighValue = 0;
    lpDDSrcSurface->GetColorKey(DDCKEY_SRCBLT, &ddColorKey);

    HRESULT hr = SoftLockSurfaces(lpDDSurface, &rDest, lpDDSrcSurface, lpSrcRect);
    if (hr != DD_OK) {
        return hr;
    }

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidSoftBlt.cpp
    // __LINE__: 1445
    UTIL_ASSERT(m_pSrcSurface != NULL);

    // __FILE__: C:\Projects\Icewind2\src\chitin\ChVidSoftBlt.cpp
    // __LINE__: 1446
    UTIL_ASSERT(m_pDestSurface != NULL);

    DWORD nWidthBytes = nWidth * 2;
    LONG nDestPitch = m_destDesc.lPitch >> 1;
    LONG nSrcPitch = m_srcDesc.lPitch >> 1;
    WORD nKey = static_cast<WORD>(ddColorKey.dwColorSpaceLowValue);

    if ((dwFlags & DDBLT_DDFX) != 0) {
        DWORD dwDDFX = lpDDBltFx->dwDDFX;

        if ((dwDDFX & DDBLTFX_MIRRORLEFTRIGHT) != 0) {
            if ((dwDDFX & DDBLTFX_MIRRORUPDOWN) == 0) {
                for (; nHeight > 0; nHeight--) {
                    SoftMirrorKeyRow(static_cast<BYTE*>(m_pDestSurface), static_cast<const BYTE*>(m_pSrcSurface), nWidthBytes, nKey);

                    m_pDestSurface = static_cast<BYTE*>(m_pDestSurface) + nDestPitch * 2;
                    m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + nSrcPitch * 2;
                }
            } else {
                m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + (nHeight - 1) * nSrcPitch * 2;

                for (; nHeight > 0; nHeight--) {
                    SoftMirrorKeyRow(static_cast<BYTE*>(m_pDestSurface), static_cast<const BYTE*>(m_pSrcSurface), nWidthBytes, nKey);

                    m_pDestSurface = static_cast<BYTE*>(m_pDestSurface) + nDestPitch * 2;
                    m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) - nSrcPitch * 2;
                }
            }

            lpDDSurface->Unlock(m_destDesc.lpSurface);
            lpDDSurface->Unlock(m_srcDesc.lpSurface);

            return DD_OK;
        }

        if ((dwDDFX & DDBLTFX_MIRRORUPDOWN) != 0) {
            m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + (nHeight - 1) * nSrcPitch * 2;
            nSrcPitch = -nSrcPitch;
        }
    }

    for (; nHeight > 0; nHeight--) {
        SoftKeyRow(static_cast<BYTE*>(m_pDestSurface), static_cast<const BYTE*>(m_pSrcSurface), nWidthBytes, nKey);

        m_pDestSurface = static_cast<BYTE*>(m_pDestSurface) + nDestPitch * 2;
        m_pSrcSurface = static_cast<BYTE*>(m_pSrcSurface) + nSrcPitch * 2;
    }

    lpDDSurface->Unlock(m_destDesc.lpSurface);
    lpDDSurface->Unlock(m_srcDesc.lpSurface);

    return DD_OK;
}
