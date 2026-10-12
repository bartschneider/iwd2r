// iwd2r. Distributed under LICENSE.md.
#pragma once

#include "win_types.h"

class CWnd {
public:
    CWnd()
        : m_hWnd(NULL)
    {
    }

    BOOL Attach(HWND hWnd)
    {
        m_hWnd = hWnd;
        return hWnd != NULL ? TRUE : FALSE;
    }
    HWND Detach()
    {
        HWND old = m_hWnd;
        m_hWnd = NULL;
        return old;
    }
    // MFC tolerates a null this here; the engine never calls this through a
    // null pointer, so this is a plain member read.
    HWND GetSafeHwnd() const { return m_hWnd; }

    HWND m_hWnd;
};
