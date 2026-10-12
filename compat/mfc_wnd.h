// iwd2r. Distributed under LICENSE.md.
#pragma once

#include "win_types.h"

#include "platform/win32_api.h"

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
    operator HWND() const { return m_hWnd; }

    // MFC returns nothing; the point or both rectangle corners are converted
    // with the Win32 calls, which leave them unchanged for a null window.
    void ClientToScreen(LPPOINT point) const { ::ClientToScreen(m_hWnd, point); }
    void ClientToScreen(LPRECT rect) const
    {
        POINT corners[2] = { { rect->left, rect->top }, { rect->right, rect->bottom } };
        ::ClientToScreen(m_hWnd, &corners[0]);
        ::ClientToScreen(m_hWnd, &corners[1]);
        rect->left = corners[0].x;
        rect->top = corners[0].y;
        rect->right = corners[1].x;
        rect->bottom = corners[1].y;
    }
    void ScreenToClient(LPPOINT point) const { ::ScreenToClient(m_hWnd, point); }
    void ScreenToClient(LPRECT rect) const
    {
        POINT corners[2] = { { rect->left, rect->top }, { rect->right, rect->bottom } };
        ::ScreenToClient(m_hWnd, &corners[0]);
        ::ScreenToClient(m_hWnd, &corners[1]);
        rect->left = corners[0].x;
        rect->top = corners[0].y;
        rect->right = corners[1].x;
        rect->bottom = corners[1].y;
    }

    HWND m_hWnd;
};
