// iwd2r. Distributed under LICENSE.md.
// Input method (IMM32) calls the engine makes. No input method context ever
// exists, so the engine's IME paths see "no context" and do nothing.
// Implemented in win32_stubs.cpp.
#pragma once

#include "compat/win_types.h"

#define GCS_COMPSTR 0x0008
#define GCS_COMPATTR 0x0010
#define GCS_RESULTSTR 0x0800
#define GCS_RESULTCLAUSE 0x1000

#define IME_CMODE_NATIVE 0x0001
#define IME_CMODE_KATAKANA 0x0002
#define IME_CMODE_FULLSHAPE 0x0008

struct CANDIDATELIST {
    DWORD dwSize;
    DWORD dwStyle;
    DWORD dwCount;
    DWORD dwSelection;
    DWORD dwPageStart;
    DWORD dwPageSize;
    DWORD dwOffset[1];
};
typedef CANDIDATELIST* LPCANDIDATELIST;

HIMC ImmGetContext(HWND hWnd);
BOOL ImmReleaseContext(HWND hWnd, HIMC context);
LONG ImmGetCompositionStringA(HIMC context, DWORD index, LPVOID buffer, DWORD bufferLength);
BOOL ImmGetConversionStatus(HIMC context, LPDWORD conversion, LPDWORD sentence);
BOOL ImmSetConversionStatus(HIMC context, DWORD conversion, DWORD sentence);
BOOL ImmGetOpenStatus(HIMC context);
BOOL ImmSetOpenStatus(HIMC context, BOOL open);
DWORD ImmGetCandidateListA(HIMC context, DWORD index, LPCANDIDATELIST list, DWORD bufferLength);
UINT ImmGetDescriptionA(HKL layout, LPSTR description, UINT bufferLength);
