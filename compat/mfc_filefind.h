// iwd2r. Distributed under LICENSE.md.
// MFC CFileFind over FindFirstFileA/FindNextFileA (platform/win32_api.h).
// Header-only so iwd2r_compat does not link against the platform layer.
#pragma once

#include "compat/mfc_string.h"
#include "compat/win_types.h"
#include "platform/win32_api.h"

class CFileFind {
public:
    CFileFind() = default;
    CFileFind(const CFileFind&) = delete;
    CFileFind& operator=(const CFileFind&) = delete;
    virtual ~CFileFind() { Close(); }

    // Starts a search; NULL means "*.*". The first match becomes current
    // after the first FindNextFile, as in MFC.
    BOOL FindFile(LPCTSTR name = NULL, DWORD = 0)
    {
        Close();
        CString pattern = name != NULL ? name : "*.*";
        m_hFind = FindFirstFileA(pattern, &m_next);
        if (m_hFind == INVALID_HANDLE_VALUE) {
            m_hFind = NULL;
            return FALSE;
        }
        // The root is the pattern's directory part, kept in the engine's spelling.
        int cut = -1;
        for (int i = 0; i < pattern.GetLength(); i++) {
            if (pattern[i] == '\\' || pattern[i] == '/')
                cut = i;
        }
        m_root = cut >= 0 ? pattern.Left(cut + 1) : CString();
        m_hasNext = TRUE;
        return TRUE;
    }

    // Makes the pending match current and fetches the one after it. Returns
    // FALSE when the current match is the last.
    BOOL FindNextFile()
    {
        if (m_hFind == NULL || !m_hasNext)
            return FALSE;
        m_found = m_next;
        m_hasFound = TRUE;
        m_hasNext = FindNextFileA(m_hFind, &m_next);
        return m_hasNext;
    }

    void Close()
    {
        if (m_hFind != NULL)
            FindClose(m_hFind);
        m_hFind = NULL;
        m_hasFound = FALSE;
        m_hasNext = FALSE;
    }

    CString GetFileName() const { return m_hasFound ? CString(m_found.cFileName) : CString(); }
    CString GetFilePath() const { return m_hasFound ? m_root + m_found.cFileName : CString(); }
    CString GetRoot() const { return m_root; }
    ULONGLONG GetLength() const
    {
        return m_hasFound ? ((static_cast<ULONGLONG>(m_found.nFileSizeHigh) << 32) | m_found.nFileSizeLow) : 0;
    }

    BOOL MatchesMask(DWORD mask) const { return m_hasFound && (m_found.dwFileAttributes & mask) != 0 ? TRUE : FALSE; }
    BOOL IsDirectory() const { return MatchesMask(FILE_ATTRIBUTE_DIRECTORY); }
    BOOL IsReadOnly() const { return MatchesMask(FILE_ATTRIBUTE_READONLY); }
    BOOL IsDots() const
    {
        if (!IsDirectory())
            return FALSE;
        const char* n = m_found.cFileName;
        return (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0'))) ? TRUE : FALSE;
    }

private:
    HANDLE m_hFind = NULL;
    WIN32_FIND_DATAA m_found {};
    WIN32_FIND_DATAA m_next {};
    BOOL m_hasFound = FALSE;
    BOOL m_hasNext = FALSE;
    CString m_root;
};
