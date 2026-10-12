// iwd2r. Distributed under LICENSE.md.
// MFC CMapWordToPtr and CMapStringToString over std::unordered_map. Iteration
// follows insertion order; a POSITION is the one-based index into that order.
#pragma once

#include "compat/mfc_list.h"
#include "compat/mfc_string.h"
#include "compat/win_types.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

struct CMapKeyHash {
    size_t operator()(const CString& key) const
    {
        return std::hash<std::string_view>()(std::string_view(static_cast<const char*>(key)));
    }
    size_t operator()(WORD key) const { return std::hash<WORD>()(key); }
};

template <class KEY, class VALUE>
class CMapBase {
public:
    CMapBase() = default;
    CMapBase(const CMapBase&) = delete;
    CMapBase& operator=(const CMapBase&) = delete;

    int GetCount() const { return static_cast<int>(m_map.size()); }
    int GetSize() const { return GetCount(); }
    BOOL IsEmpty() const { return m_map.empty(); }

    BOOL Lookup(const KEY& key, VALUE& value) const
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return FALSE;
        value = it->second;
        return TRUE;
    }
    void SetAt(const KEY& key, const VALUE& value)
    {
        auto result = m_map.insert_or_assign(key, value);
        if (result.second)
            m_order.push_back(key);
    }
    VALUE& operator[](const KEY& key)
    {
        auto result = m_map.try_emplace(key);
        if (result.second)
            m_order.push_back(key);
        return result.first->second;
    }
    BOOL RemoveKey(const KEY& key)
    {
        if (m_map.erase(key) == 0)
            return FALSE;
        m_order.erase(std::find(m_order.begin(), m_order.end(), key));
        return TRUE;
    }
    void RemoveAll()
    {
        m_map.clear();
        m_order.clear();
    }

    POSITION GetStartPosition() const
    {
        return m_order.empty() ? NULL : reinterpret_cast<POSITION>(static_cast<uintptr_t>(1));
    }
    void GetNextAssoc(POSITION& position, KEY& key, VALUE& value) const
    {
        uintptr_t index = reinterpret_cast<uintptr_t>(position);
        assert(index >= 1 && index <= m_order.size());
        key = m_order[index - 1];
        value = m_map.find(key)->second;
        position = index < m_order.size() ? reinterpret_cast<POSITION>(index + 1) : NULL;
    }

private:
    std::unordered_map<KEY, VALUE, CMapKeyHash> m_map;
    std::vector<KEY> m_order;
};

class CMapWordToPtr : public CMapBase<WORD, void*> { };

class CMapStringToString : public CMapBase<CString, CString> { };
