// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "platform/fs.h"

#include "compat/path_hook.h"
#include "platform/handles.h"
#include "platform/win32_api.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace stdfs = std::filesystem;

namespace platform::fs {

namespace {

    std::mutex g_mutex;
    std::string g_dataRoot;
    std::string g_installPrefix; // normalized: forward slashes, trailing slash, or empty
    std::unordered_map<std::string, std::vector<std::string>> g_dirCache;

    char Lower(char c)
    {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    bool EqualsNoCase(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (Lower(a[i]) != Lower(b[i]))
                return false;
        }
        return true;
    }

    std::string Slashes(const std::string& s)
    {
        std::string out = s;
        std::replace(out.begin(), out.end(), '\\', '/');
        return out;
    }

    std::string TrimTrailingSlashes(std::string s)
    {
        while (s.size() > 1 && s.back() == '/')
            s.pop_back();
        return s;
    }

    bool HasDrive(const std::string& p)
    {
        if (p.size() < 2 || p[1] != ':')
            return false;
        const char c = Lower(p[0]);
        return c >= 'a' && c <= 'z';
    }

    std::string Join(const std::string& dir, const std::string& name)
    {
        if (dir.empty())
            return name;
        if (dir.back() == '/')
            return dir + name;
        return dir + "/" + name;
    }

    // Lists a directory (cached). Returns false when it cannot be listed.
    // Caller holds g_mutex.
    bool ListDirectory(const std::string& dir, const std::vector<std::string>*& entries, bool& fresh)
    {
        auto it = g_dirCache.find(dir);
        if (it != g_dirCache.end()) {
            entries = &it->second;
            fresh = false;
            return true;
        }
        fresh = true;
        std::error_code ec;
        stdfs::directory_iterator iter(dir.empty() ? stdfs::path(".") : stdfs::path(dir), ec);
        if (ec)
            return false;
        std::vector<std::string> names;
        for (const stdfs::directory_iterator end; iter != end; iter.increment(ec))
            names.push_back(iter->path().filename().generic_string());
        if (ec)
            return false;
        entries = &(g_dirCache[dir] = std::move(names));
        return true;
    }

    // Caller holds g_mutex.
    std::string FoldCase(const std::string& path)
    {
        const bool absolute = !path.empty() && path[0] == '/';
        // Relative paths are looked up under the data root (the engine's working directory).
        const std::string base = absolute ? std::string() : g_dataRoot;
        std::string result = absolute ? "/" : "";
        std::string real = absolute ? "/" : base; // the on-disk path matching `result`

        size_t pos = 0;
        while (pos <= path.size()) {
            size_t next = path.find('/', pos);
            if (next == std::string::npos)
                next = path.size();
            const std::string comp = path.substr(pos, next - pos);
            pos = next + 1;
            if (comp.empty())
                continue;

            std::string chosen = comp;
            if (comp != "." && comp != "..") {
                std::error_code ec;
                const std::string candidate = Join(real, comp);
                if (!stdfs::exists(candidate, ec)) {
                    // A cached listing may predate files created outside the shim:
                    // on a miss, rescan the directory once before giving up.
                    for (int attempt = 0; attempt < 2; ++attempt) {
                        const std::vector<std::string>* entries = nullptr;
                        bool fresh = true;
                        if (!ListDirectory(real, entries, fresh))
                            break;
                        const std::string* best = nullptr;
                        for (const std::string& name : *entries) {
                            if (name == comp) {
                                best = &name;
                                break;
                            }
                            if (EqualsNoCase(name, comp) && (!best || name < *best))
                                best = &name;
                        }
                        if (best) {
                            chosen = *best;
                            break;
                        }
                        if (fresh)
                            break;
                        g_dirCache.erase(real);
                    }
                }
            }
            result = Join(result, chosen);
            real = Join(real, chosen);
        }
        return result;
    }

    std::string ResolveLocked(const char* path)
    {
        if (!path)
            return std::string();
        std::string p = Slashes(path);
        if (g_dataRoot.empty())
            return p;

        if (HasDrive(p)) {
            const std::string root = TrimTrailingSlashes(g_dataRoot);
            bool matched = false;
            if (!g_installPrefix.empty() && p.size() >= g_installPrefix.size() - 1) {
                const std::string withSlash = p + "/";
                if (withSlash.size() >= g_installPrefix.size() && EqualsNoCase(withSlash.substr(0, g_installPrefix.size()), g_installPrefix)) {
                    p = Join(root, withSlash.substr(g_installPrefix.size()));
                    while (p.size() > root.size() + 1 && p.back() == '/')
                        p.pop_back();
                    matched = true;
                }
            }
            if (!matched) {
                size_t start = 2;
                while (start < p.size() && p[start] == '/')
                    ++start;
                p = Join(root, p.substr(start));
            }
            p = TrimTrailingSlashes(p);
        }
        return FoldCase(p);
    }

    FILETIME ToFileTime(stdfs::file_time_type t)
    {
        const auto sys = std::chrono::system_clock::now() + (t - stdfs::file_time_type::clock::now());
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(sys.time_since_epoch()).count();
        const uint64_t ticks = static_cast<uint64_t>(micros) * 10u + 116444736000000000ull;
        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(ticks & 0xFFFFFFFFu);
        ft.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
        return ft;
    }

    DWORD AttributesOf(const stdfs::path& p, std::error_code& ec)
    {
        const stdfs::file_status st = stdfs::status(p, ec);
        if (ec || st.type() == stdfs::file_type::not_found)
            return INVALID_FILE_ATTRIBUTES;
        DWORD attrs = stdfs::is_directory(st) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        if ((st.permissions() & stdfs::perms::owner_write) == stdfs::perms::none)
            attrs |= FILE_ATTRIBUTE_READONLY;
        return attrs;
    }

    bool FillFindData(const stdfs::path& p, WIN32_FIND_DATAA& data)
    {
        std::error_code ec;
        const DWORD attrs = AttributesOf(p, ec);
        if (attrs == INVALID_FILE_ATTRIBUTES)
            return false;
        std::memset(&data, 0, sizeof(data));
        data.dwFileAttributes = attrs;
        uint64_t size = 0;
        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            size = static_cast<uint64_t>(stdfs::file_size(p, ec));
            if (ec)
                size = 0;
        }
        data.nFileSizeHigh = static_cast<DWORD>(size >> 32);
        data.nFileSizeLow = static_cast<DWORD>(size & 0xFFFFFFFFu);
        const auto t = stdfs::last_write_time(p, ec);
        if (!ec) {
            data.ftLastWriteTime = ToFileTime(t);
            data.ftCreationTime = data.ftLastWriteTime;
            data.ftLastAccessTime = data.ftLastWriteTime;
        }
        const std::string name = p.filename().generic_string();
        std::strncpy(data.cFileName, name.c_str(), MAX_PATH - 1);
        data.cFileName[MAX_PATH - 1] = '\0';
        return true;
    }

    struct FindRecord {
        std::vector<WIN32_FIND_DATAA> entries;
        size_t next = 0;
    };

    // Cache keys are full on-disk paths, so relative inputs are keyed under the data root.
    void InvalidateParentOf(const std::string& resolved)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const bool absolute = !resolved.empty() && resolved[0] == '/';
        const std::string full = absolute ? resolved : Join(g_dataRoot, resolved);
        const size_t slash = full.find_last_of('/');
        g_dirCache.erase(slash == std::string::npos ? std::string() : (slash == 0 ? std::string("/") : full.substr(0, slash)));
        g_dirCache.erase(full);
    }

    DWORD CopyOut(const std::string& s, LPSTR buffer, DWORD size)
    {
        if (!buffer || size == 0)
            return 0;
        const size_t n = std::min<size_t>(s.size(), size - 1);
        std::memcpy(buffer, s.data(), n);
        buffer[n] = '\0';
        return static_cast<DWORD>(n);
    }

}

void SetDataRoot(const std::string& root)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_dataRoot = Slashes(root);
    g_dirCache.clear();
}

std::string DataRoot()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_dataRoot;
}

void SetInstallPrefix(const std::string& windowsPrefix)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string prefix = Slashes(windowsPrefix);
    if (!prefix.empty() && prefix.back() != '/')
        prefix += '/';
    g_installPrefix = prefix;
}

std::string Resolve(const char* path)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return ResolveLocked(path);
}

namespace {

    bool PlainWildcardMatch(const char* pattern, const char* name)
    {
        const char* p = pattern;
        const char* n = name;
        const char* starP = nullptr;
        const char* starN = nullptr;
        while (*n) {
            if (*p == '*') {
                starP = ++p;
                starN = n;
            } else if (*p == '?' || (*p && Lower(*p) == Lower(*n))) {
                ++p;
                ++n;
            } else if (starP) {
                p = starP;
                n = ++starN;
            } else {
                return false;
            }
        }
        while (*p == '*')
            ++p;
        return *p == '\0';
    }

}

bool WildcardMatch(const char* pattern, const char* name)
{
    if (PlainWildcardMatch(pattern, name))
        return true;
    // Win32: a trailing ".*" also matches a name with no extension, so "*.*"
    // matches every name and "save.*" matches "save".
    const size_t length = std::strlen(pattern);
    if (length >= 2 && pattern[length - 2] == '.' && pattern[length - 1] == '*' && std::strchr(name, '.') == nullptr) {
        const std::string stem(pattern, length - 2);
        return PlainWildcardMatch(stem.c_str(), name);
    }
    return false;
}

void InvalidateCache()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_dirCache.clear();
}

void InstallResolver()
{
    compat::SetPathResolver(&Resolve);
}

}

using platform::fs::Resolve;

HANDLE FindFirstFileA(LPCSTR pattern, WIN32_FIND_DATAA* data)
{
    if (!pattern || !data)
        return INVALID_HANDLE_VALUE;
    const std::string p = platform::fs::Resolve(pattern);
    // With no directory part this enumerates the current directory, which is the data root by design.
    const size_t slash = p.find_last_of('/');
    const std::string dir = slash == std::string::npos ? std::string() : (slash == 0 ? std::string("/") : p.substr(0, slash));
    const std::string wildcard = slash == std::string::npos ? p : p.substr(slash + 1);

    std::error_code ec;
    stdfs::directory_iterator iter(dir.empty() ? stdfs::path(".") : stdfs::path(dir), ec);
    if (ec)
        return INVALID_HANDLE_VALUE;
    auto record = std::make_unique<platform::fs::FindRecord>();
    for (const stdfs::directory_iterator end; iter != end; iter.increment(ec)) {
        if (ec)
            break;
        const std::string name = iter->path().filename().generic_string();
        if (name == "." || name == ".." || !platform::fs::WildcardMatch(wildcard.c_str(), name.c_str()))
            continue;
        WIN32_FIND_DATAA entry;
        if (platform::fs::FillFindData(iter->path(), entry))
            record->entries.push_back(entry);
    }
    if (record->entries.empty())
        return INVALID_HANDLE_VALUE;
    *data = record->entries[0];
    record->next = 1;
    return record.release();
}

BOOL FindNextFileA(HANDLE find, WIN32_FIND_DATAA* data)
{
    if (!find || find == INVALID_HANDLE_VALUE || !data)
        return FALSE;
    auto* record = static_cast<platform::fs::FindRecord*>(find);
    if (record->next >= record->entries.size())
        return FALSE;
    *data = record->entries[record->next++];
    return TRUE;
}

BOOL FindClose(HANDLE find)
{
    if (!find || find == INVALID_HANDLE_VALUE)
        return FALSE;
    delete static_cast<platform::fs::FindRecord*>(find);
    return TRUE;
}

DWORD GetFileAttributesA(LPCSTR path)
{
    if (!path)
        return INVALID_FILE_ATTRIBUTES;
    std::error_code ec;
    return platform::fs::AttributesOf(stdfs::path(Resolve(path)), ec);
}

BOOL DeleteFileA(LPCSTR path)
{
    if (!path)
        return FALSE;
    const std::string p = Resolve(path);
    std::error_code ec;
    if (stdfs::is_directory(p, ec)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    const bool removed = stdfs::remove(p, ec);
    platform::fs::InvalidateParentOf(p);
    if (removed && !ec)
        return TRUE;
    // The engine tells a missing file apart from a real failure.
    SetLastError(!ec ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL SetFileAttributesA(LPCSTR path, DWORD attributes)
{
    if (!path)
        return FALSE;
    const std::string p = Resolve(path);
    std::error_code ec;
    if (!stdfs::exists(p, ec)) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    const stdfs::perm_options option = (attributes & FILE_ATTRIBUTE_READONLY) ? stdfs::perm_options::remove : stdfs::perm_options::add;
    stdfs::permissions(p, stdfs::perms::owner_write, option, ec);
    return ec ? FALSE : TRUE;
}

BOOL RemoveDirectoryA(LPCSTR path)
{
    if (!path)
        return FALSE;
    const std::string p = Resolve(path);
    std::error_code ec;
    if (!stdfs::is_directory(p, ec)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    // Only an empty directory, as in Win32.
    const bool removed = stdfs::remove(p, ec);
    platform::fs::InvalidateParentOf(p);
    return removed && !ec ? TRUE : FALSE;
}

HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD, HANDLE)
{
    if (!path)
        return INVALID_HANDLE_VALUE;
    const std::string p = Resolve(path);
    std::error_code ec;
    const bool exists = stdfs::is_regular_file(p, ec);
    const bool write = (access & GENERIC_WRITE) != 0;
    const char* mode = nullptr;
    switch (disposition) {
    case OPEN_EXISTING:
        if (exists)
            mode = write ? "r+b" : "rb";
        break;
    case TRUNCATE_EXISTING:
        if (exists)
            mode = "w+b";
        break;
    case CREATE_NEW:
        if (!exists)
            mode = "w+b";
        break;
    case OPEN_ALWAYS:
        mode = exists ? (write ? "r+b" : "rb") : "w+b";
        break;
    case CREATE_ALWAYS:
        mode = "w+b";
        break;
    default:
        SetLastError(ERROR_INVALID_HANDLE);
        return INVALID_HANDLE_VALUE;
    }
    if (!mode) {
        SetLastError(exists ? ERROR_ACCESS_DENIED : ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    std::FILE* file = std::fopen(p.c_str(), mode);
    if (!file) {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    if (!exists)
        platform::fs::InvalidateParentOf(p);
    return platform::handles::MakeFileHandle(file);
}

BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD toRead, LPDWORD read, LPVOID)
{
    if (read)
        *read = 0;
    std::FILE* f = platform::handles::FileOf(file);
    if (!f || (!buffer && toRead > 0)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    const size_t got = std::fread(buffer, 1, toRead, f);
    if (read)
        *read = static_cast<DWORD>(got);
    return std::ferror(f) ? FALSE : TRUE;
}

BOOL CopyFileA(LPCSTR source, LPCSTR destination, BOOL failIfExists)
{
    if (!source || !destination)
        return FALSE;
    const std::string src = Resolve(source);
    const std::string dst = Resolve(destination);
    std::error_code ec;
    const bool copied = stdfs::copy_file(src, dst, failIfExists ? stdfs::copy_options::none : stdfs::copy_options::overwrite_existing, ec);
    platform::fs::InvalidateParentOf(dst);
    return copied && !ec ? TRUE : FALSE;
}

BOOL MoveFileA(LPCSTR source, LPCSTR destination)
{
    if (!source || !destination)
        return FALSE;
    const std::string src = Resolve(source);
    const std::string dst = Resolve(destination);
    std::error_code ec;
    if (stdfs::exists(dst, ec))
        return FALSE;
    stdfs::rename(src, dst, ec);
    platform::fs::InvalidateParentOf(src);
    platform::fs::InvalidateParentOf(dst);
    return ec ? FALSE : TRUE;
}

BOOL CreateDirectoryA(LPCSTR path, LPVOID)
{
    if (!path)
        return FALSE;
    const std::string p = Resolve(path);
    std::error_code ec;
    const bool created = stdfs::create_directory(p, ec);
    platform::fs::InvalidateParentOf(p);
    return created && !ec ? TRUE : FALSE;
}

BOOL GetDiskFreeSpaceA(LPCSTR root, DWORD* sectorsPerCluster, DWORD* bytesPerSector, DWORD* freeClusters, DWORD* totalClusters)
{
    const std::string p = (root && *root) ? Resolve(root) : std::string(".");
    std::error_code ec;
    const stdfs::space_info info = stdfs::space(p.empty() ? stdfs::path(".") : stdfs::path(p), ec);
    if (ec)
        return FALSE;
    const uint64_t clusterBytes = 512u * 8u;
    const uint64_t maxDword = std::numeric_limits<DWORD>::max();
    if (sectorsPerCluster)
        *sectorsPerCluster = 8;
    if (bytesPerSector)
        *bytesPerSector = 512;
    if (freeClusters)
        *freeClusters = static_cast<DWORD>(std::min<uint64_t>(info.available / clusterBytes, maxDword));
    if (totalClusters)
        *totalClusters = static_cast<DWORD>(std::min<uint64_t>(info.capacity / clusterBytes, maxDword));
    return TRUE;
}

DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer)
{
    std::error_code ec;
    const std::string cwd = stdfs::current_path(ec).generic_string();
    if (ec)
        return 0;
    if (!buffer || size <= cwd.size())
        return static_cast<DWORD>(cwd.size() + 1);
    return platform::fs::CopyOut(cwd, buffer, size);
}

BOOL SetCurrentDirectoryA(LPCSTR path)
{
    if (!path)
        return FALSE;
    std::error_code ec;
    stdfs::current_path(Resolve(path), ec);
    return ec ? FALSE : TRUE;
}

DWORD GetModuleFileNameA(HMODULE, LPSTR buffer, DWORD size)
{
    std::string exe;
#ifdef __linux__
    std::error_code ec;
    const stdfs::path target = stdfs::read_symlink("/proc/self/exe", ec);
    if (!ec)
        exe = target.generic_string();
#endif
    return platform::fs::CopyOut(exe, buffer, size);
}

DWORD GetLogicalDrives() { return 0; }

UINT GetDriveTypeA(LPCSTR) { return DRIVE_NO_ROOT_DIR; }

BOOL GetVolumeInformationA(LPCSTR, LPSTR, DWORD, DWORD*, DWORD*, DWORD*, LPSTR, DWORD)
{
    return FALSE;
}
