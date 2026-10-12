// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "ini.h"
#include "path_hook.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct Document {
    std::vector<std::string> lines; // without line terminators; a BOM stays on line 0
    std::string eol = "\n";
};

std::mutex s_mutex;
std::map<std::string, Document> s_cache;

bool IsBlank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

std::string Trim(const std::string& s)
{
    size_t b = 0;
    size_t e = s.size();
    while (b < e && IsBlank(s[b]))
        ++b;
    while (e > b && IsBlank(s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

char Lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

bool EqualNoCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (Lower(a[i]) != Lower(b[i]))
            return false;
    }
    return true;
}

enum class Kind { Other,
    Header,
    Pair };

struct Parsed {
    Kind kind = Kind::Other;
    std::string name; // section name or key
    std::string value; // raw text after the first '='
    size_t valueStart = 0; // offset of the text after '=' within the line
};

Parsed ParseLine(const std::string& line, bool first)
{
    Parsed p;
    size_t start = (first && line.compare(0, 3, "\xEF\xBB\xBF") == 0) ? 3 : 0;
    while (start < line.size() && IsBlank(line[start]))
        ++start;
    if (start >= line.size() || line[start] == ';' || line[start] == '#')
        return p;
    if (line[start] == '[') {
        size_t close = line.find(']', start);
        if (close != std::string::npos) {
            p.kind = Kind::Header;
            p.name = Trim(line.substr(start + 1, close - start - 1));
        }
        return p;
    }
    size_t eq = line.find('=', start);
    if (eq == std::string::npos)
        return p;
    p.kind = Kind::Pair;
    p.name = Trim(line.substr(start, eq - start));
    p.valueStart = eq + 1;
    p.value = line.substr(eq + 1);
    return p;
}

std::string CleanValue(const std::string& raw)
{
    std::string v = Trim(raw);
    if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'') && v.back() == v.front())
        v = v.substr(1, v.size() - 2);
    return v;
}

bool ReadDocument(const std::string& path, Document& doc)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    size_t firstNl = data.find('\n');
    if (firstNl != std::string::npos && firstNl > 0 && data[firstNl - 1] == '\r')
        doc.eol = "\r\n";
    size_t pos = 0;
    while (pos < data.size()) {
        size_t nl = data.find('\n', pos);
        size_t end = (nl == std::string::npos) ? data.size() : nl;
        size_t lineEnd = end;
        if (lineEnd > pos && data[lineEnd - 1] == '\r')
            --lineEnd;
        doc.lines.push_back(data.substr(pos, lineEnd - pos));
        pos = (nl == std::string::npos) ? data.size() : nl + 1;
    }
    return true;
}

bool WriteDocument(const std::string& path, const Document& doc)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    for (const std::string& line : doc.lines)
        out << line << doc.eol;
    out.flush();
    return static_cast<bool>(out);
}

// Returns the cached document, loading it on first use; nullptr if the file does not exist.
Document* Load(const std::string& path)
{
    auto it = s_cache.find(path);
    if (it != s_cache.end())
        return &it->second;
    Document doc;
    if (!ReadDocument(path, doc))
        return nullptr;
    return &s_cache.emplace(path, std::move(doc)).first->second;
}

// Index range of the first section named `name`: header index and one past its last line.
bool FindSection(const Document& doc, const std::string& name, size_t& header, size_t& end)
{
    bool found = false;
    for (size_t i = 0; i < doc.lines.size(); ++i) {
        Parsed p = ParseLine(doc.lines[i], i == 0);
        if (p.kind != Kind::Header)
            continue;
        if (found) {
            end = i;
            return true;
        }
        if (EqualNoCase(p.name, name)) {
            found = true;
            header = i;
        }
    }
    if (found)
        end = doc.lines.size();
    return found;
}

bool Lookup(const Document& doc, const std::string& section, const std::string& key, std::string& value)
{
    bool inSection = false;
    for (size_t i = 0; i < doc.lines.size(); ++i) {
        Parsed p = ParseLine(doc.lines[i], i == 0);
        if (p.kind == Kind::Header)
            inSection = EqualNoCase(p.name, section);
        else if (inSection && p.kind == Kind::Pair && EqualNoCase(p.name, key)) {
            value = CleanValue(p.value);
            return true;
        }
    }
    return false;
}

DWORD CopyString(const std::string& s, LPSTR out, DWORD size)
{
    size_t n = std::min<size_t>(s.size(), size - 1);
    std::memcpy(out, s.data(), n);
    out[n] = '\0';
    return static_cast<DWORD>(n);
}

// `names` holds each entry followed by its NUL; the list ends with one more NUL.
DWORD CopyList(const std::string& names, LPSTR out, DWORD size)
{
    if (names.size() + 1 <= size) {
        std::memcpy(out, names.data(), names.size());
        out[names.size()] = '\0';
        return static_cast<DWORD>(names.size());
    }
    if (size < 2) {
        out[0] = '\0';
        return 0;
    }
    std::memcpy(out, names.data(), size - 2);
    out[size - 2] = '\0';
    out[size - 1] = '\0';
    return size - 2;
}

}

DWORD GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName)
{
    if (!lpReturnedString || nSize == 0)
        return 0;
    std::string def = lpDefault ? lpDefault : "";
    std::string path = compat::ResolvePath(lpFileName ? lpFileName : "");
    std::lock_guard<std::mutex> lock(s_mutex);
    const Document* doc = Load(path);

    if (!lpAppName) {
        std::string names;
        if (doc) {
            for (size_t i = 0; i < doc->lines.size(); ++i) {
                Parsed p = ParseLine(doc->lines[i], i == 0);
                if (p.kind == Kind::Header)
                    names += p.name + '\0';
            }
        }
        return CopyList(names, lpReturnedString, nSize);
    }
    if (!lpKeyName) {
        std::string names;
        if (doc) {
            bool inSection = false;
            for (size_t i = 0; i < doc->lines.size(); ++i) {
                Parsed p = ParseLine(doc->lines[i], i == 0);
                if (p.kind == Kind::Header)
                    inSection = EqualNoCase(p.name, lpAppName);
                else if (inSection && p.kind == Kind::Pair)
                    names += p.name + '\0';
            }
        }
        return CopyList(names, lpReturnedString, nSize);
    }
    std::string value;
    if (doc && Lookup(*doc, lpAppName, lpKeyName, value))
        return CopyString(value, lpReturnedString, nSize);
    return CopyString(def, lpReturnedString, nSize);
}

UINT GetPrivateProfileIntA(LPCSTR lpAppName, LPCSTR lpKeyName, INT nDefault, LPCSTR lpFileName)
{
    UINT def = static_cast<UINT>(nDefault);
    if (!lpAppName || !lpKeyName)
        return def;
    std::string path = compat::ResolvePath(lpFileName ? lpFileName : "");
    std::string value;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const Document* doc = Load(path);
        if (!doc || !Lookup(*doc, lpAppName, lpKeyName, value))
            return def;
    }
    size_t i = 0;
    bool negative = false;
    if (i < value.size() && (value[i] == '+' || value[i] == '-'))
        negative = value[i++] == '-';
    if (i >= value.size() || value[i] < '0' || value[i] > '9')
        return def;
    UINT result = 0;
    for (; i < value.size() && value[i] >= '0' && value[i] <= '9'; ++i)
        result = result * 10u + static_cast<UINT>(value[i] - '0');
    return negative ? 0u - result : result;
}

BOOL WritePrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName)
{
    if (!lpAppName || !lpFileName)
        return FALSE;
    std::string path = compat::ResolvePath(lpFileName);
    std::lock_guard<std::mutex> lock(s_mutex);
    Document* loaded = Load(path);
    bool existed = loaded != nullptr;
    Document fresh;
    Document& doc = loaded ? *loaded : fresh;
    std::string section = lpAppName;
    size_t header = 0;
    size_t end = 0;
    bool haveSection = FindSection(doc, section, header, end);
    bool changed = false;

    if (!lpKeyName) {
        // Delete the whole section including its header.
        if (haveSection) {
            doc.lines.erase(doc.lines.begin() + header, doc.lines.begin() + end);
            changed = true;
        }
    } else {
        std::string key = lpKeyName;
        size_t keyLine = end;
        if (haveSection) {
            for (size_t i = header + 1; i < end; ++i) {
                Parsed p = ParseLine(doc.lines[i], false);
                if (p.kind == Kind::Pair && EqualNoCase(p.name, key)) {
                    keyLine = i;
                    break;
                }
            }
        }
        if (!lpString) {
            if (keyLine < end) {
                doc.lines.erase(doc.lines.begin() + keyLine);
                changed = true;
            }
        } else if (keyLine < end) {
            std::string& line = doc.lines[keyLine];
            Parsed p = ParseLine(line, false);
            size_t cut = p.valueStart;
            while (cut < line.size() && (line[cut] == ' ' || line[cut] == '\t'))
                ++cut;
            line = line.substr(0, cut) + lpString;
            changed = true;
        } else if (haveSection) {
            size_t at = end;
            while (at > header + 1 && Trim(doc.lines[at - 1]).empty())
                --at;
            doc.lines.insert(doc.lines.begin() + at, key + "=" + lpString);
            changed = true;
        } else {
            if (!doc.lines.empty() && !Trim(doc.lines.back()).empty())
                doc.lines.push_back("");
            doc.lines.push_back("[" + section + "]");
            doc.lines.push_back(key + "=" + lpString);
            changed = true;
        }
    }

    if (!changed)
        return TRUE;
    if (!WriteDocument(path, doc)) {
        s_cache.erase(path);
        return FALSE;
    }
    if (!existed)
        s_cache.emplace(path, std::move(fresh));
    return TRUE;
}

namespace compat::ini {

void FlushCache()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_cache.clear();
}

}
