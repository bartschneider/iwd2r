// MFC CMapWordToPtr, CMapStringToString and CTypedPtrMap over
// std::unordered_map. Iteration follows insertion order; a POSITION is the
// sequence number of the next entry.
#pragma once

#include "compat/mfc_list.h"
#include "compat/mfc_string.h"
#include "compat/win_types.h"

#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <string_view>
#include <unordered_map>

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
    typedef KEY BaseKey;
    typedef VALUE BaseValue;

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
        value = it->second.value;
        return TRUE;
    }
    void SetAt(const KEY& key, const VALUE& value) { (*this)[key] = value; }
    VALUE& operator[](const KEY& key)
    {
        auto result = m_map.try_emplace(key);
        if (result.second) {
            result.first->second.sequence = m_nextSequence++;
            m_order.emplace(result.first->second.sequence, key);
        }
        return result.first->second.value;
    }
    BOOL RemoveKey(const KEY& key)
    {
        auto it = m_map.find(key);
        if (it == m_map.end())
            return FALSE;
        m_order.erase(it->second.sequence);
        m_map.erase(it);
        return TRUE;
    }
    void RemoveAll()
    {
        m_map.clear();
        m_order.clear();
    }

    POSITION GetStartPosition() const
    {
        return m_order.empty() ? NULL : ToPosition(m_order.begin()->first);
    }
    void GetNextAssoc(POSITION& position, KEY& key, VALUE& value) const
    {
        // The position holds the sequence number of the next entry, so it
        // survives removal of entries already returned; if the entry it named
        // was removed, the next live one is used.
        auto it = m_order.lower_bound(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(position)));
        assert(it != m_order.end());
        if (it == m_order.end()) {
            position = NULL;
            return;
        }
        key = it->second;
        value = m_map.find(key)->second.value;
        ++it;
        position = it == m_order.end() ? NULL : ToPosition(it->first);
    }

private:
    struct Entry {
        VALUE value {};
        uint64_t sequence = 0;
    };

    static POSITION ToPosition(uint64_t sequence) { return reinterpret_cast<POSITION>(static_cast<uintptr_t>(sequence)); }

    std::unordered_map<KEY, Entry, CMapKeyHash> m_map;
    std::map<uint64_t, KEY> m_order;
    uint64_t m_nextSequence = 1;
};

class CMapWordToPtr : public CMapBase<WORD, void*> { };

class CMapStringToString : public CMapBase<CString, CString> { };

// Typed view of a pointer map such as CMapWordToPtr. operator[] hands out the
// stored void* reinterpreted as VALUE&, as MFC does (see CTypedPtrList).
template <class BASE_CLASS, class KEY, class VALUE>
class CTypedPtrMap : public BASE_CLASS {
public:
    BOOL Lookup(KEY key, VALUE& value) const
    {
        typename BASE_CLASS::BaseValue stored {};
        if (!BASE_CLASS::Lookup(key, stored))
            return FALSE;
        value = static_cast<VALUE>(stored);
        return TRUE;
    }
    VALUE& operator[](KEY key) { return reinterpret_cast<VALUE&>(BASE_CLASS::operator[](key)); }
    void SetAt(KEY key, VALUE value) { BASE_CLASS::SetAt(key, value); }
    BOOL RemoveKey(KEY key) { return BASE_CLASS::RemoveKey(key); }
    void GetNextAssoc(POSITION& position, KEY& key, VALUE& value) const
    {
        typename BASE_CLASS::BaseKey baseKey {};
        typename BASE_CLASS::BaseValue stored {};
        BASE_CLASS::GetNextAssoc(position, baseKey, stored);
        key = static_cast<KEY>(baseKey);
        value = static_cast<VALUE>(stored);
    }
};
