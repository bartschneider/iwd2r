// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include "compat/mfc_string.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

struct EmptyStringStorage {
    CStringData header;
    char terminator;
};

// Shared by every empty CString; never written to and never freed.
EmptyStringStorage g_empty = { { -1, 0, 0 }, '\0' };

char* EmptyData()
{
    return &g_empty.terminator;
}

char AsciiLower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

char AsciiUpper(char c)
{
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

bool IsSpace(char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

} // namespace

int CStringCompare(const char* a, const char* b)
{
    return std::strcmp(a, b);
}

void CString::Init()
{
    m_pchData = EmptyData();
}

bool CString::IsShared() const
{
    return m_pchData == EmptyData();
}

void CString::Release()
{
    if (!IsShared())
        std::free(Header());
    m_pchData = EmptyData();
}

// Allocates a header plus alloc + 1 characters; the caller installs it and releases the old buffer.
static char* NewBuffer(int len, int alloc)
{
    void* raw = std::malloc(sizeof(CStringData) + static_cast<size_t>(alloc) + 1);
    if (!raw)
        throw std::bad_alloc();
    CStringData* header = static_cast<CStringData*>(raw);
    header->nRefs = 1;
    header->nDataLength = len;
    header->nAllocLength = alloc;
    char* data = reinterpret_cast<char*>(header + 1);
    data[len] = '\0';
    return data;
}

void CString::AllocateExact(int len)
{
    // Replaces the buffer; contents are not preserved.
    char* data = NewBuffer(len, len);
    Release();
    m_pchData = data;
}

void CString::EnsureCapacity(int len)
{
    // Keeps the current contents; grows geometrically.
    if (!IsShared() && Header()->nAllocLength >= len)
        return;
    int oldLen = GetLength();
    int alloc = len;
    if (!IsShared() && Header()->nAllocLength * 2 > alloc)
        alloc = Header()->nAllocLength * 2;
    char* data = NewBuffer(oldLen, alloc);
    std::memcpy(data, m_pchData, static_cast<size_t>(oldLen));
    Release();
    m_pchData = data;
}

void CString::ChopTo(int len)
{
    if (IsShared())
        return;
    Header()->nDataLength = len;
    m_pchData[len] = '\0';
}

void CString::Assign(const char* pch, int len)
{
    if (len <= 0) {
        Release();
        return;
    }
    // Build first so that pch may point into our own buffer.
    char* data = NewBuffer(len, len);
    std::memcpy(data, pch, static_cast<size_t>(len));
    Release();
    m_pchData = data;
}

void CString::Append(const char* pch, int len)
{
    if (len <= 0)
        return;
    int oldLen = GetLength();
    if (!IsShared() && Header()->nAllocLength >= oldLen + len) {
        std::memmove(m_pchData + oldLen, pch, static_cast<size_t>(len));
        Header()->nDataLength = oldLen + len;
        m_pchData[oldLen + len] = '\0';
        return;
    }
    int alloc = oldLen + len;
    if (!IsShared() && Header()->nAllocLength * 2 > alloc)
        alloc = Header()->nAllocLength * 2;
    char* data = NewBuffer(oldLen + len, alloc);
    std::memcpy(data, m_pchData, static_cast<size_t>(oldLen));
    std::memcpy(data + oldLen, pch, static_cast<size_t>(len));
    Release();
    m_pchData = data;
}

CString::CString()
{
    Init();
}

CString::CString(const CString& other)
{
    Init();
    Assign(other.m_pchData, other.GetLength());
}

CString::CString(const char* psz)
{
    Init();
    if (psz)
        Assign(psz, static_cast<int>(std::strlen(psz)));
}

CString::CString(const char* pch, int len)
{
    Init();
    if (pch)
        Assign(pch, len);
}

CString::CString(char ch, int repeat)
{
    Init();
    if (repeat > 0) {
        AllocateExact(repeat);
        std::memset(m_pchData, ch, static_cast<size_t>(repeat));
    }
}

CString::~CString()
{
    Release();
}

CString& CString::operator=(const CString& other)
{
    if (this != &other)
        Assign(other.m_pchData, other.GetLength());
    return *this;
}

CString& CString::operator=(const char* psz)
{
    Assign(psz, psz ? static_cast<int>(std::strlen(psz)) : 0);
    return *this;
}

CString& CString::operator=(char ch)
{
    Assign(&ch, 1);
    return *this;
}

void CString::Empty()
{
    Release();
}

int CString::Compare(const char* psz) const
{
    return std::strcmp(m_pchData, psz);
}

int CString::CompareNoCase(const char* psz) const
{
    const unsigned char* a = reinterpret_cast<const unsigned char*>(m_pchData);
    const unsigned char* b = reinterpret_cast<const unsigned char*>(psz);
    for (;; ++a, ++b) {
        int ca = static_cast<unsigned char>(AsciiLower(static_cast<char>(*a)));
        int cb = static_cast<unsigned char>(AsciiLower(static_cast<char>(*b)));
        if (ca != cb)
            return ca < cb ? -1 : 1;
        if (ca == 0)
            return 0;
    }
}

CString CString::Mid(int first) const
{
    return Mid(first, INT_MAX);
}

CString CString::Mid(int first, int count) const
{
    int len = GetLength();
    if (first < 0)
        first = 0;
    if (count < 0)
        count = 0;
    if (first > len)
        return CString();
    if (count > len - first)
        count = len - first;
    return CString(m_pchData + first, count);
}

CString CString::Left(int count) const
{
    if (count < 0)
        count = 0;
    if (count > GetLength())
        count = GetLength();
    return CString(m_pchData, count);
}

CString CString::Right(int count) const
{
    if (count < 0)
        count = 0;
    if (count > GetLength())
        count = GetLength();
    return CString(m_pchData + GetLength() - count, count);
}

int CString::Find(char ch) const
{
    return Find(ch, 0);
}

int CString::Find(const char* psz) const
{
    return Find(psz, 0);
}

int CString::Find(char ch, int start) const
{
    if (start < 0 || start > GetLength())
        return -1;
    if (ch == '\0')
        return -1;
    const char* p = std::strchr(m_pchData + start, ch);
    return p ? static_cast<int>(p - m_pchData) : -1;
}

int CString::Find(const char* psz, int start) const
{
    if (start < 0 || start > GetLength())
        return -1;
    const char* p = std::strstr(m_pchData + start, psz);
    return p ? static_cast<int>(p - m_pchData) : -1;
}

int CString::FindOneOf(const char* set) const
{
    const char* p = std::strpbrk(m_pchData, set);
    return p ? static_cast<int>(p - m_pchData) : -1;
}

int CString::ReverseFind(char ch) const
{
    if (ch == '\0')
        return -1;
    const char* p = std::strrchr(m_pchData, ch);
    return p ? static_cast<int>(p - m_pchData) : -1;
}

void CString::TrimLeft()
{
    const char* p = m_pchData;
    while (IsSpace(*p))
        ++p;
    if (p != m_pchData)
        Assign(p, GetLength() - static_cast<int>(p - m_pchData));
}

void CString::TrimLeft(char ch)
{
    const char* p = m_pchData;
    while (*p && *p == ch)
        ++p;
    if (p != m_pchData)
        Assign(p, GetLength() - static_cast<int>(p - m_pchData));
}

void CString::TrimLeft(const char* set)
{
    const char* p = m_pchData;
    while (*p && std::strchr(set, *p))
        ++p;
    if (p != m_pchData)
        Assign(p, GetLength() - static_cast<int>(p - m_pchData));
}

void CString::TrimRight()
{
    int len = GetLength();
    while (len > 0 && IsSpace(m_pchData[len - 1]))
        --len;
    if (len != GetLength())
        Assign(m_pchData, len);
}

void CString::TrimRight(char ch)
{
    int len = GetLength();
    while (len > 0 && m_pchData[len - 1] == ch)
        --len;
    if (len != GetLength())
        Assign(m_pchData, len);
}

void CString::TrimRight(const char* set)
{
    int len = GetLength();
    while (len > 0 && std::strchr(set, m_pchData[len - 1]))
        --len;
    if (len != GetLength())
        Assign(m_pchData, len);
}

void CString::MakeUpper()
{
    for (int i = 0, n = GetLength(); i < n; ++i)
        m_pchData[i] = AsciiUpper(m_pchData[i]);
}

void CString::MakeLower()
{
    for (int i = 0, n = GetLength(); i < n; ++i)
        m_pchData[i] = AsciiLower(m_pchData[i]);
}

void CString::MakeReverse()
{
    for (int i = 0, j = GetLength() - 1; i < j; ++i, --j) {
        char t = m_pchData[i];
        m_pchData[i] = m_pchData[j];
        m_pchData[j] = t;
    }
}

char* CString::GetBuffer(int minLen)
{
    if (minLen < GetLength())
        minLen = GetLength();
    if (IsShared()) {
        char* data = NewBuffer(0, minLen);
        m_pchData = data;
    } else {
        EnsureCapacity(minLen);
    }
    return m_pchData;
}

void CString::ReleaseBuffer(int newLen)
{
    if (IsShared())
        return;
    if (newLen < 0)
        newLen = static_cast<int>(std::strlen(m_pchData));
    if (newLen > Header()->nAllocLength)
        newLen = Header()->nAllocLength;
    ChopTo(newLen);
}

char* CString::GetBufferSetLength(int newLen)
{
    if (newLen < 0)
        newLen = 0;
    GetBuffer(newLen);
    ChopTo(newLen);
    return m_pchData;
}

CString CString::SpanIncluding(const char* set) const
{
    return Left(static_cast<int>(std::strspn(m_pchData, set)));
}

CString CString::SpanExcluding(const char* set) const
{
    return Left(static_cast<int>(std::strcspn(m_pchData, set)));
}

int CString::Remove(char ch)
{
    int len = GetLength();
    int out = 0;
    for (int i = 0; i < len; ++i) {
        if (m_pchData[i] != ch)
            m_pchData[out++] = m_pchData[i];
    }
    int removed = len - out;
    if (removed > 0)
        ChopTo(out);
    return removed;
}

int CString::Replace(char from, char to)
{
    int count = 0;
    for (int i = 0, n = GetLength(); i < n; ++i) {
        if (m_pchData[i] == from) {
            m_pchData[i] = to;
            ++count;
        }
    }
    return count;
}

int CString::Replace(const char* from, const char* to)
{
    int fromLen = from ? static_cast<int>(std::strlen(from)) : 0;
    if (fromLen == 0)
        return 0;
    int toLen = to ? static_cast<int>(std::strlen(to)) : 0;

    CString result;
    int count = 0;
    const char* cur = m_pchData;
    const char* hit;
    while ((hit = std::strstr(cur, from)) != nullptr) {
        result.Append(cur, static_cast<int>(hit - cur));
        result.Append(to, toLen);
        cur = hit + fromLen;
        ++count;
    }
    if (count == 0)
        return 0;
    result.Append(cur, static_cast<int>(std::strlen(cur)));
    *this = result;
    return count;
}

int CString::Insert(int index, char ch)
{
    return Insert(index, CString(ch).m_pchData);
}

int CString::Insert(int index, const char* psz)
{
    int len = GetLength();
    if (index < 0)
        index = 0;
    if (index > len)
        index = len;
    int insLen = psz ? static_cast<int>(std::strlen(psz)) : 0;
    if (insLen == 0)
        return len;

    CString result;
    result.AllocateExact(len + insLen);
    std::memcpy(result.m_pchData, m_pchData, static_cast<size_t>(index));
    std::memcpy(result.m_pchData + index, psz, static_cast<size_t>(insLen));
    std::memcpy(result.m_pchData + index + insLen, m_pchData + index, static_cast<size_t>(len - index));
    *this = result;
    return GetLength();
}

int CString::Delete(int index, int count)
{
    int len = GetLength();
    if (index < 0)
        index = 0;
    if (count < 0 || index >= len)
        return len;
    if (count > len - index)
        count = len - index;
    std::memmove(m_pchData + index, m_pchData + index + count, static_cast<size_t>(len - index - count));
    ChopTo(len - count);
    return len - count;
}

void CString::FormatV(const char* fmt, va_list args)
{
    va_list sizing;
    va_copy(sizing, args);
    int needed = std::vsnprintf(nullptr, 0, fmt, sizing);
    va_end(sizing);
    if (needed <= 0) {
        // Arguments may alias our buffer, so only clear after sizing.
        Empty();
        return;
    }
    CString temp;
    temp.AllocateExact(needed);
    std::vsnprintf(temp.m_pchData, static_cast<size_t>(needed) + 1, fmt, args);
    *this = temp;
}

void CString::FormatPrintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    FormatV(fmt, args);
    va_end(args);
}

CString& CString::operator+=(const CString& other)
{
    Append(other.m_pchData, other.GetLength());
    return *this;
}

CString& CString::operator+=(const char* psz)
{
    if (psz)
        Append(psz, static_cast<int>(std::strlen(psz)));
    return *this;
}

CString& CString::operator+=(char ch)
{
    Append(&ch, 1);
    return *this;
}

CString operator+(const CString& a, const CString& b)
{
    CString r(a);
    r += b;
    return r;
}

CString operator+(const CString& a, const char* b)
{
    CString r(a);
    r += b;
    return r;
}

CString operator+(const char* a, const CString& b)
{
    CString r(a);
    r += b;
    return r;
}

CString operator+(const CString& a, char b)
{
    CString r(a);
    r += b;
    return r;
}

CString operator+(char a, const CString& b)
{
    CString r(a);
    r += b;
    return r;
}
