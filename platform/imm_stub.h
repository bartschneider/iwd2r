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

#define IMN_CHANGECANDIDATE 0x0003
#define IMN_CLOSECANDIDATE 0x0004
#define IMN_OPENCANDIDATE 0x0005
#define IMN_SETOPENSTATUS 0x0008

#define HKL_NEXT 1
#define KLF_REORDER 0x00000008

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
// One keyboard layout only: nothing to switch to, so NULL (failure).
HKL ActivateKeyboardLayout(HKL layout, UINT flags);
