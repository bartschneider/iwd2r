// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "compat/mfc_file.h"

#include "compat/path_hook.h"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

const HANDLE CFile::hFileNull = (HANDLE)(intptr_t)-1;

namespace {

FILE* asFile(HANDLE handle) { return static_cast<FILE*>(handle); }

// C++17 has no clock_cast, so approximate by comparing the two clocks' "now".
time_t toTimeT(fs::file_time_type t)
{
    const auto sys = std::chrono::system_clock::now() + (t - fs::file_time_type::clock::now());
    return std::chrono::system_clock::to_time_t(std::chrono::time_point_cast<std::chrono::system_clock::duration>(sys));
}

fs::file_time_type fromTimeT(time_t t)
{
    const auto delta = std::chrono::system_clock::from_time_t(t) - std::chrono::system_clock::now();
    return fs::file_time_type::clock::now() + std::chrono::duration_cast<fs::file_time_type::duration>(delta);
}

}

CFile::CFile()
    : m_hFile(hFileNull)
    , m_lastOp(opNone)
{
}

CFile::CFile(const char* path, UINT flags)
    : m_hFile(hFileNull)
    , m_lastOp(opNone)
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
    m_lastOp = opNone;
    return TRUE;
}

UINT CFile::Read(void* buffer, UINT count)
{
    if (m_hFile == hFileNull || count == 0)
        return 0;
    // Switching from writing to reading needs a positioning call in between.
    if (m_lastOp == opWrite)
        fseek(asFile(m_hFile), 0, SEEK_CUR);
    m_lastOp = opRead;
    return static_cast<UINT>(fread(buffer, 1, count, asFile(m_hFile)));
}

void CFile::Write(const void* buffer, UINT count)
{
    if (m_hFile == hFileNull || count == 0)
        return;
    if (m_lastOp == opRead)
        fseek(asFile(m_hFile), 0, SEEK_CUR);
    m_lastOp = opWrite;
    fwrite(buffer, 1, count, asFile(m_hFile));
}

LONG CFile::Seek(LONG offset, UINT from)
{
    if (m_hFile == hFileNull)
        return -1;
    static const int whence[] = { SEEK_SET, SEEK_CUR, SEEK_END };
    if (from > end || fseek(asFile(m_hFile), offset, whence[from]) != 0)
        return -1;
    m_lastOp = opNone;
    return static_cast<LONG>(ftell(asFile(m_hFile)));
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
    FILE* file = asFile(m_hFile);
    const long position = ftell(file);
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    fseek(file, position, SEEK_SET);
    m_lastOp = opNone;
    return length < 0 ? 0 : static_cast<DWORD>(length);
}

DWORD CFile::GetPosition() const
{
    if (m_hFile == hFileNull)
        return 0;
    const long position = ftell(asFile(m_hFile));
    return position < 0 ? 0 : static_cast<DWORD>(position);
}

void CFile::SetLength(DWORD length)
{
    if (m_hFile == hFileNull)
        return;
    fflush(asFile(m_hFile));
    std::error_code ec;
    fs::resize_file(fs::path(m_path), length, ec);
    fseek(asFile(m_hFile), 0, SEEK_CUR);
    m_lastOp = opNone;
}

void CFile::Close()
{
    if (m_hFile != hFileNull) {
        fclose(asFile(m_hFile));
        m_hFile = hFileNull;
    }
    m_path.clear();
    m_lastOp = opNone;
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

void CFile::SetFilePath(const char* path) { m_path = path ? path : ""; }

CString CFile::GetFileName() const
{
    const size_t slash = m_path.find_last_of("/\\");
    return CString(slash == std::string::npos ? m_path.c_str() : m_path.c_str() + slash + 1);
}

BOOL CFile::GetStatus(CFileStatus& status) const
{
    if (m_hFile == hFileNull)
        return FALSE;
    return GetStatus(m_path.c_str(), status);
}

BOOL CFile::GetStatus(const char* path, CFileStatus& status)
{
    const std::string resolved = compat::ResolvePath(path);
    const fs::path p(resolved);
    std::error_code ec;
    const fs::file_status info = fs::status(p, ec);
    if (ec || !fs::exists(info))
        return FALSE;

    const bool isDirectory = fs::is_directory(info);
    status.m_size = 0;
    if (!isDirectory) {
        const auto size = fs::file_size(p, ec);
        if (!ec)
            status.m_size = static_cast<ULONGLONG>(size);
    }
    CTime modified;
    const auto written = fs::last_write_time(p, ec);
    if (!ec)
        modified = CTime(toTimeT(written));
    status.m_mtime = modified;
    status.m_ctime = modified;
    status.m_atime = modified;

    BYTE attribute = normal;
    if (isDirectory)
        attribute |= directory;
    const fs::perms writeBits = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
    if ((info.permissions() & writeBits) == fs::perms::none)
        attribute |= readOnly;
    status.m_attribute = attribute;
    strncpy(status.m_szFullName, resolved.c_str(), MAX_PATH - 1);
    status.m_szFullName[MAX_PATH - 1] = '\0';
    return TRUE;
}

void CFile::SetStatus(const char* path, const CFileStatus& status)
{
    const fs::path p(compat::ResolvePath(path));
    std::error_code ec;
    if (!fs::exists(p, ec))
        return;
    if (status.m_attribute & readOnly)
        fs::permissions(p, fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write, fs::perm_options::remove, ec);
    else
        fs::permissions(p, fs::perms::owner_write, fs::perm_options::add, ec);
    if (status.m_mtime.GetTime() != 0)
        fs::last_write_time(p, fromTimeT(status.m_mtime.GetTime()), ec);
}

void CFile::Remove(const char* path)
{
    std::error_code ec;
    fs::remove(fs::path(compat::ResolvePath(path)), ec);
}

void CFile::Rename(const char* oldPath, const char* newPath)
{
    std::error_code ec;
    fs::rename(fs::path(compat::ResolvePath(oldPath)), fs::path(compat::ResolvePath(newPath)), ec);
}
