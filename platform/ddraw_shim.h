// DirectDraw shim: the subset of the DirectDraw 2 API that the classic
// renderer calls, implemented on 32-bpp XRGB8888 software surfaces. The
// interfaces are plain C++ classes with virtual methods, not COM objects.
// Struct field order and constant values follow the public documentation.
#pragma once

#include "compat/win_types.h"

#include <cstdint>

// Results.
#define DD_OK ((HRESULT)0)
#define DDENUMRET_OK ((HRESULT)1)
#define DDERR_INVALIDPARAMS ((HRESULT)0x80070057)
#define DDERR_OUTOFMEMORY ((HRESULT)0x8007000E)
#define DDERR_UNSUPPORTED ((HRESULT)0x80004001)
#define DDERR_INVALIDMODE ((HRESULT)0x88760078)
#define DDERR_INVALIDOBJECT ((HRESULT)0x88760082)
#define DDERR_NOTFOUND ((HRESULT)0x887600FF)
#define DDERR_GENERIC ((HRESULT)0x80004005)
#define DDERR_SURFACEBUSY ((HRESULT)0x887601AE)
#define DDERR_SURFACELOST ((HRESULT)0x887601C2)
#define DDERR_WASSTILLDRAWING ((HRESULT)0x8876021C)
#define DDERR_HWNDSUBCLASSED ((HRESULT)0x88760244)
#define DDERR_EXCLUSIVEMODEALREADYSET ((HRESULT)0x88760245)
#define DDERR_HWNDALREADYSET ((HRESULT)0x88760246)

// DDSURFACEDESC::dwFlags.
#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_BACKBUFFERCOUNT 0x00000020
#define DDSD_LPSURFACE 0x00000800
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_REFRESHRATE 0x00040000

// DDSCAPS::dwCaps.
#define DDSCAPS_BACKBUFFER 0x00000004
#define DDSCAPS_COMPLEX 0x00000008
#define DDSCAPS_FLIP 0x00000010
#define DDSCAPS_OFFSCREENPLAIN 0x00000040
#define DDSCAPS_PRIMARYSURFACE 0x00000200
#define DDSCAPS_SYSTEMMEMORY 0x00000800
#define DDSCAPS_VIDEOMEMORY 0x00004000

// DDCAPS::dwCaps.
#define DDCAPS_BLT 0x00000040
#define DDCAPS_NOHARDWARE 0x02000000

// DDPIXELFORMAT::dwFlags.
#define DDPF_RGB 0x00000040

// SetCooperativeLevel.
#define DDSCL_FULLSCREEN 0x00000001
#define DDSCL_NORMAL 0x00000008
#define DDSCL_EXCLUSIVE 0x00000010

// Lock, Flip, SetColorKey.
#define DDLOCK_WAIT 0x00000001
#define DDFLIP_WAIT 0x00000001
#define DDCKEY_SRCBLT 0x00000008

// Blt and BltFast.
#define DDBLT_COLORFILL 0x00000400
#define DDBLT_DDFX 0x00000800
#define DDBLT_KEYSRC 0x00008000
#define DDBLT_WAIT 0x01000000
#define DDBLTFAST_SRCCOLORKEY 0x00000001
#define DDBLTFAST_WAIT 0x00000010
#define DDBLTFX_MIRRORLEFTRIGHT 0x00000002
#define DDBLTFX_MIRRORUPDOWN 0x00000004

struct DDCOLORKEY {
    DWORD dwColorSpaceLowValue;
    DWORD dwColorSpaceHighValue;
};
typedef DDCOLORKEY* LPDDCOLORKEY;

struct DDSCAPS {
    DWORD dwCaps;
};
typedef DDSCAPS* LPDDSCAPS;

struct DDPIXELFORMAT {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwRGBAlphaBitMask;
};
typedef DDPIXELFORMAT* LPDDPIXELFORMAT;

struct DDSURFACEDESC {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    union {
        LONG lPitch;
        DWORD dwLinearSize;
    };
    DWORD dwBackBufferCount;
    union {
        DWORD dwMipMapCount;
        DWORD dwZBufferBitDepth;
        DWORD dwRefreshRate;
    };
    DWORD dwAlphaBitDepth;
    DWORD dwReserved;
    LPVOID lpSurface;
    DDCOLORKEY ddckCKDestOverlay;
    DDCOLORKEY ddckCKDestBlt;
    DDCOLORKEY ddckCKSrcOverlay;
    DDCOLORKEY ddckCKSrcBlt;
    DDPIXELFORMAT ddpfPixelFormat;
    DDSCAPS ddsCaps;
};
typedef DDSURFACEDESC* LPDDSURFACEDESC;

// The documented layout; members that are surface pointers in the original
// unions are kept as their DWORD alternative.
struct DDBLTFX {
    DWORD dwSize;
    DWORD dwDDFX;
    DWORD dwROP;
    DWORD dwDDROP;
    DWORD dwRotationAngle;
    DWORD dwZBufferOpCode;
    DWORD dwZBufferLow;
    DWORD dwZBufferHigh;
    DWORD dwZBufferBaseDest;
    DWORD dwZDestConstBitDepth;
    DWORD dwZDestConst;
    DWORD dwZSrcConstBitDepth;
    DWORD dwZSrcConst;
    DWORD dwAlphaEdgeBlendBitDepth;
    DWORD dwAlphaEdgeBlend;
    DWORD dwReserved;
    DWORD dwAlphaDestConstBitDepth;
    DWORD dwAlphaDestConst;
    DWORD dwAlphaSrcConstBitDepth;
    DWORD dwAlphaSrcConst;
    union {
        DWORD dwFillColor;
        DWORD dwFillDepth;
        DWORD dwFillPixel;
    };
    DDCOLORKEY ddckDestColorkey;
    DDCOLORKEY ddckSrcColorkey;
};
typedef DDBLTFX* LPDDBLTFX;

// The leading documented fields, through ddsCaps; nothing in the tree reads
// the later ones.
struct DDCAPS {
    DWORD dwSize;
    DWORD dwCaps;
    DWORD dwCaps2;
    DWORD dwCKeyCaps;
    DWORD dwFXCaps;
    DWORD dwFXAlphaCaps;
    DWORD dwPalCaps;
    DWORD dwSVCaps;
    DWORD dwAlphaBltConstBitDepths;
    DWORD dwAlphaBltPixelBitDepths;
    DWORD dwAlphaBltSurfaceBitDepths;
    DWORD dwAlphaOverlayConstBitDepths;
    DWORD dwAlphaOverlayPixelBitDepths;
    DWORD dwAlphaOverlaySurfaceBitDepths;
    DWORD dwZBufferBitDepths;
    DWORD dwVidMemTotal;
    DWORD dwVidMemFree;
    DWORD dwMaxVisibleOverlays;
    DWORD dwCurrVisibleOverlays;
    DWORD dwNumFourCCCodes;
    DWORD dwAlignBoundarySrc;
    DWORD dwAlignSizeSrc;
    DWORD dwAlignBoundaryDest;
    DWORD dwAlignSizeDest;
    DWORD dwAlignStrideAlign;
    DWORD dwRops[8];
    DDSCAPS ddsCaps;
};
typedef DDCAPS* LPDDCAPS;

struct PALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};
typedef PALETTEENTRY* LPPALETTEENTRY;

typedef HRESULT(CALLBACK* LPDDENUMMODESCALLBACK)(LPDDSURFACEDESC, LPVOID);

extern const GUID IID_IDirectDraw2;

class IDirectDrawClipper {
public:
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT SetHWnd(DWORD dwFlags, HWND hWnd) = 0;

protected:
    virtual ~IDirectDrawClipper() = default;
};

class IDirectDrawPalette {
public:
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;

protected:
    virtual ~IDirectDrawPalette() = default;
};

class IDirectDrawSurface {
public:
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) = 0;
    virtual HRESULT Unlock(LPVOID lpSurfaceData) = 0;
    virtual HRESULT Blt(LPRECT lpDestRect, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) = 0;
    virtual HRESULT BltFast(DWORD dwX, DWORD dwY, IDirectDrawSurface* lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) = 0;
    virtual HRESULT Flip(IDirectDrawSurface* lpDDSurfaceTargetOverride, DWORD dwFlags) = 0;
    virtual HRESULT GetAttachedSurface(LPDDSCAPS lpDDSCaps, IDirectDrawSurface** lplpDDAttachedSurface) = 0;
    virtual HRESULT SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) = 0;
    virtual HRESULT GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) = 0;
    virtual HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) = 0;
    virtual HRESULT SetClipper(IDirectDrawClipper* lpDDClipper) = 0;
    virtual HRESULT SetPalette(IDirectDrawPalette* lpDDPalette) = 0;
    virtual HRESULT GetPalette(IDirectDrawPalette** lplpDDPalette) = 0;
    virtual HRESULT GetDC(HDC* lphDC) = 0;
    virtual HRESULT ReleaseDC(HDC hDC) = 0;
    virtual HRESULT Restore() = 0;
    virtual HRESULT IsLost() = 0;

protected:
    virtual ~IDirectDrawSurface() = default;
};

class IDirectDraw {
public:
    virtual HRESULT QueryInterface(REFIID riid, LPVOID* ppvObj) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT SetCooperativeLevel(HWND hWnd, DWORD dwFlags) = 0;
    virtual HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP) = 0;
    virtual HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) = 0;
    virtual HRESULT RestoreDisplayMode() = 0;
    virtual HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) = 0;
    virtual HRESULT EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) = 0;
    virtual HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) = 0;
    virtual HRESULT CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, IDirectDrawSurface** lplpDDSurface, LPVOID pUnkOuter) = 0;
    virtual HRESULT CreateClipper(DWORD dwFlags, IDirectDrawClipper** lplpDDClipper, LPVOID pUnkOuter) = 0;
    virtual HRESULT CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, IDirectDrawPalette** lplpDDPalette, LPVOID pUnkOuter) = 0;

protected:
    virtual ~IDirectDraw() = default;
};

typedef IDirectDraw IDirectDraw2;
typedef IDirectDraw* LPDIRECTDRAW;
typedef IDirectDraw2* LPDIRECTDRAW2;
typedef IDirectDrawSurface* LPDIRECTDRAWSURFACE;
typedef IDirectDrawClipper* LPDIRECTDRAWCLIPPER;
typedef IDirectDrawPalette* LPDIRECTDRAWPALETTE;

HRESULT DirectDrawCreate(GUID* lpGUID, IDirectDraw** lplpDD, LPVOID pUnkOuter);
