#define NOMINMAX
#include "platform/fs.h"

#include "compat/path_hook.h"
#include "platform/gpu_probe.h"
#include "platform/handles.h"
#include "platform/win32_api.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>
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

    // The entry's name in the narrow encoding. On Windows a name with characters
    // outside the ANSI code page cannot be converted (the standard library
    // throws); such an entry is skipped, as the engine could not name it.
    bool NarrowName(const stdfs::path& p, std::string& name)
    {
        try {
            name = p.filename().generic_string();
            return true;
        } catch (const std::system_error&) {
            return false;
        }
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
        for (const stdfs::directory_iterator end; iter != end; iter.increment(ec)) {
            std::string name;
            if (NarrowName(iter->path(), name))
                names.push_back(std::move(name));
        }
        if (ec)
            return false;
        entries = &(g_dirCache[dir] = std::move(names));
        return true;
    }

    // Caller holds g_mutex.
    std::string FoldCase(const std::string& path)
    {
        // A native absolute path: "/..." or, on Windows, "X:/...".
        const size_t rootLength = (!path.empty() && path[0] == '/') ? 1 : (HasDrive(path) && path.size() > 2 && path[2] == '/') ? 3
                                                                                                                                : 0;
        const bool absolute = rootLength > 0;
        // Relative paths are looked up under the data root (the engine's working directory).
        const std::string base = absolute ? std::string() : g_dataRoot;
        std::string result = path.substr(0, rootLength);
        std::string real = absolute ? result : base; // the on-disk path matching `result`

        size_t pos = rootLength;
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
#ifdef _WIN32
            // A drive letter outside the install prefix is a native path here.
            if (!matched)
                return FoldCase(p);
#endif
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
        std::string name;
        if (!NarrowName(p, name))
            return false;
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
    // Win32 lists "." and ".." first in a directory other than a root when the
    // pattern is "*" or "*.*"; the engine skips them by name or by the
    // directory attribute, and relies on them to tell an empty directory from
    // a missing one (CDimm::DirectoryCopyFiles of an empty mpsave\default).
    const stdfs::path searched = dir.empty() ? stdfs::current_path(ec) : stdfs::path(dir);
    if ((wildcard == "*" || wildcard == "*.*") && !ec && searched.has_relative_path()) {
        for (const char* dots : { ".", ".." }) {
            WIN32_FIND_DATAA entry;
            if (platform::fs::FillFindData(searched / dots, entry)) {
                std::strncpy(entry.cFileName, dots, MAX_PATH - 1);
                record->entries.push_back(entry);
            }
        }
    }
    ec.clear();
    for (const stdfs::directory_iterator end; iter != end; iter.increment(ec)) {
        if (ec)
            break;
        std::string name;
        if (!platform::fs::NarrowName(iter->path(), name))
            continue;
        if (name == "." || name == ".." || !platform::fs::WildcardMatch(wildcard.c_str(), name.c_str()))
            continue;
        WIN32_FIND_DATAA entry;
        if (platform::fs::FillFindData(iter->path(), entry))
            record->entries.push_back(entry);
    }
    // NTFS lists a directory in its $UpCase collation: names compared byte by
    // byte after upper-casing (so '_' sorts after the letters). The engine
    // inherits that order (CDimm::CompressDirectory packs files in it), so
    // sort the snapshot the same way, after the dot entries; ties, possible
    // only on a case-sensitive file system, go by the raw name.
    const size_t dots = static_cast<size_t>(std::count_if(record->entries.begin(), record->entries.end(),
        [](const WIN32_FIND_DATAA& e) { return std::strcmp(e.cFileName, ".") == 0 || std::strcmp(e.cFileName, "..") == 0; }));
    std::sort(record->entries.begin() + static_cast<std::ptrdiff_t>(dots), record->entries.end(),
        [](const WIN32_FIND_DATAA& a, const WIN32_FIND_DATAA& b) {
            for (size_t i = 0;; ++i) {
                unsigned char ca = static_cast<unsigned char>(a.cFileName[i]);
                unsigned char cb = static_cast<unsigned char>(b.cFileName[i]);
                const unsigned char ua = ca >= 'a' && ca <= 'z' ? static_cast<unsigned char>(ca - 'a' + 'A') : ca;
                const unsigned char ub = cb >= 'a' && cb <= 'z' ? static_cast<unsigned char>(cb - 'a' + 'A') : cb;
                if (ua != ub)
                    return ua < ub;
                if (ca == '\0')
                    break;
            }
            return std::strcmp(a.cFileName, b.cFileName) < 0;
        });
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
#ifdef _WIN32
    // MSVC maps the permission bits to the read-only attribute only when no
    // write bit at all remains.
    const stdfs::perms writeBits = stdfs::perms::owner_write | stdfs::perms::group_write | stdfs::perms::others_write;
#else
    const stdfs::perms writeBits = stdfs::perms::owner_write;
#endif
    stdfs::permissions(p, writeBits, option, ec);
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
#elif defined(__APPLE__)
    exe = platform::gpu_probe::SelfExe();
#endif
    return platform::fs::CopyOut(exe, buffer, size);
}

DWORD GetLogicalDrives() { return 0; }

UINT GetDriveTypeA(LPCSTR) { return DRIVE_NO_ROOT_DIR; }

BOOL GetVolumeInformationA(LPCSTR, LPSTR, DWORD, DWORD*, DWORD*, DWORD*, LPSTR, DWORD)
{
    return FALSE;
}
