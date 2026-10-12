// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "compat/mfc_file.h"

#include "compat/path_hook.h"

#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

const HANDLE CFile::hFileNull = (HANDLE)(intptr_t)-1;

namespace {

FILE* asFile(HANDLE handle) { return static_cast<FILE*>(handle); }

}

CFile::CFile()
    : m_hFile(hFileNull)
{
}

CFile::CFile(const char* path, UINT flags)
    : m_hFile(hFileNull)
{
    Open(path, flags);
}

CFile::~CFile()
{
    Close();
}

BOOL CFile::Open(const char* path, UINT flags, void*)
{
    Close();
    const std::string resolved = compat::ResolvePath(path);
    const UINT access = flags & 0x3;
    const bool create = (flags & modeCreate) != 0;
    const bool truncate = create && (flags & modeNoTruncate) == 0;

    FILE* file = nullptr;
    if (access == modeRead) {
        file = fopen(resolved.c_str(), "rb");
    } else if (truncate) {
        file = fopen(resolved.c_str(), "w+b");
    } else {
        file = fopen(resolved.c_str(), "r+b");
        if (!file && create && errno == ENOENT)
            file = fopen(resolved.c_str(), "w+b");
    }
    if (!file)
        return FALSE;
    m_hFile = file;
    m_path = resolved;
    return TRUE;
}

UINT CFile::Read(void* buffer, UINT count)
{
    if (m_hFile == hFileNull || count == 0)
        return 0;
    return static_cast<UINT>(fread(buffer, 1, count, asFile(m_hFile)));
}

void CFile::Write(const void* buffer, UINT count)
{
    if (m_hFile == hFileNull || count == 0)
        return;
    fwrite(buffer, 1, count, asFile(m_hFile));
}

LONG CFile::Seek(LONG offset, UINT from)
{
    if (m_hFile == hFileNull)
        return -1;
    static const int whence[] = { SEEK_SET, SEEK_CUR, SEEK_END };
    if (from > end || fseeko(asFile(m_hFile), offset, whence[from]) != 0)
        return -1;
    return static_cast<LONG>(ftello(asFile(m_hFile)));
}

void CFile::SeekToBegin()
{
    Seek(0, begin);
}

DWORD CFile::SeekToEnd()
{
    return static_cast<DWORD>(Seek(0, end));
}

DWORD CFile::GetLength() const
{
    if (m_hFile == hFileNull)
        return 0;
    fflush(asFile(m_hFile));
    struct stat info;
    if (fstat(fileno(asFile(m_hFile)), &info) != 0)
        return 0;
    return static_cast<DWORD>(info.st_size);
}

DWORD CFile::GetPosition() const
{
    if (m_hFile == hFileNull)
        return 0;
    return static_cast<DWORD>(ftello(asFile(m_hFile)));
}

void CFile::SetLength(DWORD length)
{
    if (m_hFile == hFileNull)
        return;
    fflush(asFile(m_hFile));
    if (ftruncate(fileno(asFile(m_hFile)), static_cast<off_t>(length)) != 0)
        return;
}

void CFile::Close()
{
    if (m_hFile != hFileNull) {
        fclose(asFile(m_hFile));
        m_hFile = hFileNull;
    }
}

void CFile::Abort()
{
    Close();
}

void CFile::Flush()
{
    if (m_hFile != hFileNull)
        fflush(asFile(m_hFile));
}

CString CFile::GetFilePath() const
{
    return CString(m_path.c_str());
}

CString CFile::GetFileName() const
{
    const size_t slash = m_path.find_last_of("/\\");
    return CString(slash == std::string::npos ? m_path.c_str() : m_path.c_str() + slash + 1);
}

BOOL CFile::GetStatus(const char* path, CFileStatus& status)
{
    const std::string resolved = compat::ResolvePath(path);
    struct stat info;
    if (stat(resolved.c_str(), &info) != 0)
        return FALSE;
    status.m_size = static_cast<ULONGLONG>(info.st_size);
    status.m_mtime = CTime(info.st_mtime);
    status.m_ctime = CTime(info.st_ctime);
    status.m_atime = CTime(info.st_atime);
    BYTE attribute = normal;
    if (S_ISDIR(info.st_mode))
        attribute |= directory;
    if (access(resolved.c_str(), W_OK) != 0)
        attribute |= readOnly;
    status.m_attribute = attribute;
    strncpy(status.m_szFullName, resolved.c_str(), MAX_PATH - 1);
    status.m_szFullName[MAX_PATH - 1] = '\0';
    return TRUE;
}

void CFile::SetStatus(const char* path, const CFileStatus& status)
{
    const std::string resolved = compat::ResolvePath(path);
    struct stat info;
    if (stat(resolved.c_str(), &info) != 0)
        return;
    mode_t mode = info.st_mode;
    if (status.m_attribute & readOnly)
        mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
    else
        mode |= S_IWUSR;
    if (mode != info.st_mode)
        chmod(resolved.c_str(), mode);
    if (status.m_mtime.GetTime() != 0) {
        utimbuf times;
        times.actime = info.st_atime;
        times.modtime = status.m_mtime.GetTime();
        utime(resolved.c_str(), &times);
    }
}

void CFile::Remove(const char* path)
{
    const std::string resolved = compat::ResolvePath(path);
    unlink(resolved.c_str());
}

void CFile::Rename(const char* oldPath, const char* newPath)
{
    const std::string from = compat::ResolvePath(oldPath);
    const std::string to = compat::ResolvePath(newPath);
    rename(from.c_str(), to.c_str());
}
