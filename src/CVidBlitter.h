#ifndef CVIDBLITTER_H_
#define CVIDBLITTER_H_

#include "mfc.h"

#include "platform/ddraw_shim.h"

class CVidBlitter {
public:
    typedef HRESULT (CVidBlitter::*BltFunc)(IDirectDrawSurface*, LPRECT, IDirectDrawSurface*, LPRECT, DWORD, LPDDBLTFX);

    CVidBlitter();
    HRESULT DxBlt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);
    HRESULT DxBltFast(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);
    HRESULT Blt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);
    HRESULT BltFast(IDirectDrawSurface* lpDDSurface, DWORD dwX, DWORD dwY, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans);
    void Init();
    void Reset();
    HRESULT SoftBlt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);
    HRESULT SoftSrcKeyBlt(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);
    HRESULT SoftColorFill(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, LPDDBLTFX lpDDBltFx);
    int SoftLockSurfaces(IDirectDrawSurface* lpDDSurface, LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect);
    static void SoftMirrorRow(BYTE* pDest, const BYTE* pSrc, DWORD nBytes);
    static void SoftMirrorKeyRow(BYTE* pDest, const BYTE* pSrc, DWORD nBytes, WORD nKey);
    static void SoftKeyRow(BYTE* pDest, const BYTE* pSrc, DWORD nBytes, WORD nKey);

    /* 0000 */ BOOLEAN m_bSoftSrcKeyBltFast; // #guess
    /* 0001 */ BOOLEAN m_bSoftBltFast; // #guess
    /* 0002 */ BOOLEAN m_bSoftSrcKeyBlt; // #guess
    /* 0003 */ BOOLEAN m_bSoftBlt; // #guess
    /* 0004 */ BOOLEAN m_bSoftMirrorBlt; // #guess
    /* 0006 */ DDSURFACEDESC m_destDesc;
    /* 0072 */ DDSURFACEDESC m_srcDesc;
    /* 00DE */ LPVOID m_pSrcSurface; // NOTE: The original's +0xDE holds the source row.
    /* 00E2 */ LPVOID m_pDestSurface; // NOTE: and its +0xE2 the destination row.
    /* 00E6 */ unsigned char field_E6;
    /* 00E8 */ BltFunc m_pfnBltNoFX; // #guess
    /* 00EC */ BltFunc m_pfnSrcKeyBltNoFX; // #guess
    /* 00F0 */ BltFunc m_pfnBltFX; // #guess
    /* 00F4 */ BltFunc m_pfnSrcKeyBltFX; // #guess
    /* 00F8 */ BltFunc m_pfnBltFast; // #guess
    /* 00FC */ BltFunc m_pfnSrcKeyBltFast; // #guess
};

#endif /* CVIDBLITTER_H_ */
