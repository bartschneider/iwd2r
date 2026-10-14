#include "data_view.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// The strings of the array "key": [ "a", "b" ] (no escapes beyond \" and \\).
std::vector<std::string> StringArray(const std::string& text, const std::string& key)
{
    std::vector<std::string> out;
    const size_t k = text.find("\"" + key + "\"");
    if (k == std::string::npos)
        return out;
    size_t p = text.find('[', k);
    const size_t end = text.find(']', k);
    if (p == std::string::npos || end == std::string::npos)
        return out;
    while (p < end) {
        const size_t open = text.find('"', p);
        if (open == std::string::npos || open > end)
            break;
        std::string item;
        size_t i = open + 1;
        for (; i < text.size() && text[i] != '"'; ++i) {
            if (text[i] == '\\' && i + 1 < text.size())
                ++i;
            item += text[i];
        }
        out.push_back(item);
        p = i + 1;
    }
    return out;
}

bool Inside(const fs::path& path, const fs::path& folder)
{
    const fs::path p = fs::weakly_canonical(path);
    const fs::path f = fs::weakly_canonical(folder);
    auto pi = p.begin();
    for (auto fi = f.begin(); fi != f.end(); ++fi, ++pi) {
        if (pi == p.end() || *pi != *fi)
            return false;
    }
    return true;
}

bool OnWindowsDrive(const fs::path& path)
{
#ifdef _WIN32
    (void)path;
    return true;
#else
    // WSL: /mnt/<letter>/
    const std::string s = path.generic_string();
    return s.size() > 7 && s.compare(0, 5, "/mnt/") == 0 && std::isalpha(static_cast<unsigned char>(s[5])) && s[6] == '/';
#endif
}

void LinkEntry(const fs::path& src, const fs::path& dst, bool windowsLinks)
{
    std::error_code ec;
    if (!windowsLinks) {
        fs::create_symlink(src, dst, ec);
        if (ec)
            throw std::runtime_error("symlink " + dst.string() + ": " + ec.message());
        return;
    }
    // A Windows engine does not follow a Linux symlink: junctions for
    // folders, hard links (else copies) for files.
    if (fs::is_directory(src)) {
#ifdef _WIN32
        const std::string cmd = "cmd /c mklink /J \"" + dst.string() + "\" \"" + src.string() + "\" > nul";
#else
        const std::string cmd = "cd /mnt/c && cmd.exe /c mklink /J \"$(wslpath -w '" + dst.string() + "')\" \"$(wslpath -w '" + src.string() + "')\" > /dev/null 2>&1";
#endif
        if (std::system(cmd.c_str()) != 0)
            throw std::runtime_error("mklink /J " + dst.string() + " failed");
        return;
    }
    fs::create_hard_link(src, dst, ec);
    if (ec) {
        ec.clear();
        fs::copy_file(src, dst, ec);
        if (ec)
            throw std::runtime_error("copy " + dst.string() + ": " + ec.message());
    }
}

}

DataViewRules LoadDataViewRules(const fs::path& file)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot read " + file.string());
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    DataViewRules rules;
    rules.emptyDirs = StringArray(text, "empty_dirs");
    rules.copiedDirs = StringArray(text, "copied_dirs");
    rules.copiedFiles = StringArray(text, "copied_files");
    return rules;
}

bool DataViewGlob(const std::string& pattern, const std::string& name)
{
    const std::string p = Lower(pattern);
    const std::string n = Lower(name);
    size_t pi = 0, ni = 0, star = std::string::npos, mark = 0;
    while (ni < n.size()) {
        if (pi < p.size() && (p[pi] == '?' || p[pi] == n[ni])) {
            ++pi;
            ++ni;
        } else if (pi < p.size() && p[pi] == '*') {
            star = pi++;
            mark = ni;
        } else if (star != std::string::npos) {
            pi = star + 1;
            ni = ++mark;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == '*')
        ++pi;
    return pi == p.size();
}

void MakeDataView(const fs::path& data, const fs::path& view, const DataViewRules& rules)
{
    if (Inside(view, data))
        throw std::runtime_error("the view " + view.string() + " is inside the data folder");
    if (Inside(data, view))
        throw std::runtime_error("the data folder is inside the view " + view.string());
    const bool windowsLinks = OnWindowsDrive(fs::absolute(view));
    fs::create_directories(view);
    std::set<std::string> present; // lower case
    for (const auto& entry : fs::directory_iterator(data)) {
        const std::string name = entry.path().filename().string();
        const std::string low = Lower(name);
        const fs::path dst = view / name;
        present.insert(low);
        if (fs::symlink_status(dst).type() != fs::file_type::not_found)
            continue;
        const bool isDir = fs::is_directory(entry.path());
        const auto has = [&](const std::vector<std::string>& v) {
            return std::any_of(v.begin(), v.end(), [&](const std::string& n) { return Lower(n) == low; });
        };
        if (isDir && has(rules.emptyDirs)) {
            fs::create_directory(dst);
        } else if (isDir && has(rules.copiedDirs)) {
            fs::copy(entry.path(), dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks);
        } else if (!isDir && std::any_of(rules.copiedFiles.begin(), rules.copiedFiles.end(), [&](const std::string& pat) { return DataViewGlob(pat, name); })) {
            fs::copy_file(entry.path(), dst);
        } else {
            LinkEntry(entry.path(), dst, windowsLinks);
        }
    }
    for (const std::string& name : rules.emptyDirs) {
        if (present.count(Lower(name)) == 0)
            fs::create_directories(view / name);
    }
}

DataViewRules WithoutSaves(DataViewRules rules)
{
    const std::string root = "mpsave";
    auto& copied = rules.copiedDirs;
    copied.erase(std::remove_if(copied.begin(), copied.end(), [&](const std::string& n) { return Lower(n) == root; }), copied.end());
    if (std::none_of(rules.emptyDirs.begin(), rules.emptyDirs.end(), [&](const std::string& n) { return Lower(n) == root; }))
        rules.emptyDirs.push_back(root);
    return rules;
}

void RemoveDataView(const fs::path& view)
{
    std::error_code ec;
    if (fs::symlink_status(view, ec).type() == fs::file_type::not_found)
        return;
    for (const auto& entry : fs::directory_iterator(view, ec)) {
        const auto type = entry.symlink_status().type();
        // A symlink (or a Windows junction, which std::filesystem reports as
        // one or as a directory with the reparse flag) is removed as an
        // entry; only real folders are emptied.
        if (type == fs::file_type::directory) {
#ifdef _WIN32
            // Junctions: rmdir removes the link, not the target's files.
            const std::string cmd = "rmdir \"" + entry.path().string() + "\" 2> nul";
            if (std::system(cmd.c_str()) == 0)
                continue;
#endif
            fs::remove_all(entry.path(), ec);
        } else {
            fs::remove(entry.path(), ec);
        }
    }
    fs::remove(view, ec);
}

DataView::DataView(const std::string& data, Saves saves)
{
    static int counter = 0;
#ifdef _WIN32
    const long long pid = _getpid();
#else
    const long long pid = getpid();
#endif
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    m_root = fs::temp_directory_path() / ("iwd2r-view-" + std::to_string(pid) + "-" + std::to_string(stamp) + "-" + std::to_string(counter++));
    const fs::path rules = fs::path(IWD2R_SOURCE_DIR) / "tools" / "dataview_rules.json";
    const fs::path view = m_root / "data";
    const DataViewRules loaded = LoadDataViewRules(rules);
    MakeDataView(data, view, saves == Saves::None ? WithoutSaves(loaded) : loaded);
    m_path = view.string();
}

DataView::~DataView()
{
    const char* keep = std::getenv("IWD2R_KEEP_DATA_VIEW");
    if (keep != nullptr && std::string(keep) == "1")
        return;
    RemoveDataView(m_root / "data");
    std::error_code ec;
    fs::remove(m_root, ec);
}
