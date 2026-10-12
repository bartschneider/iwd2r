// iwd2r. Distributed under LICENSE.md.
#pragma once

#include "compat/mfc_string.h"
#include "compat/mfc_time.h"
#include "compat/win_types.h"

#include <string>

struct CFileStatus;

class CFile {
public:
    enum OpenFlags {
        modeRead = 0x0000,
        modeWrite = 0x0001,
        modeReadWrite = 0x0002,
        shareCompat = 0x0000,
        shareExclusive = 0x0010,
        shareDenyWrite = 0x0020,
        shareDenyRead = 0x0030,
        shareDenyNone = 0x0040,
        modeNoInherit = 0x0080,
        modeCreate = 0x1000,
        modeNoTruncate = 0x2000,
        typeText = 0x4000,
        typeBinary = 0x8000
    };
    enum SeekPosition { begin = 0, current = 1, end = 2 };
    enum Attribute { normal = 0x00, readOnly = 0x01, hidden = 0x02, system = 0x04, directory = 0x10, archive = 0x20 };

    static const HANDLE hFileNull;

    CFile();
    CFile(const char* path, UINT flags);
    virtual ~CFile();

    CFile(const CFile&) = delete;
    CFile& operator=(const CFile&) = delete;

    virtual BOOL Open(const char* path, UINT flags, void* pException = NULL);
    virtual UINT Read(void* buffer, UINT count);
    virtual void Write(const void* buffer, UINT count);
    virtual LONG Seek(LONG offset, UINT from);
    void SeekToBegin();
    DWORD SeekToEnd();
    virtual DWORD GetLength() const;
    virtual DWORD GetPosition() const;
    virtual void SetLength(DWORD length);
    virtual void Close();
    virtual void Abort();
    virtual void Flush();

    CString GetFilePath() const;
    CString GetFileName() const;

    static BOOL GetStatus(const char* path, CFileStatus& status);
    static void SetStatus(const char* path, const CFileStatus& status);
    static void Remove(const char* path);
    static void Rename(const char* oldPath, const char* newPath);

    HANDLE m_hFile;

private:
    std::string m_path;
};

struct CFileStatus {
    CTime m_ctime, m_mtime, m_atime;
    ULONGLONG m_size;
    BYTE m_attribute;
    char m_szFullName[MAX_PATH];
};
