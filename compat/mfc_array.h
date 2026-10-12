// iwd2r. Distributed under LICENSE.md.
// MFC CArray, CPtrArray, CDWordArray and CTypedPtrArray over std::vector.
#pragma once

#include "compat/win_types.h"

#include <cassert>
#include <cstddef>
#include <vector>

template <class TYPE, class ARG_TYPE = const TYPE&>
class CArray {
public:
    CArray() = default;

    int GetSize() const { return static_cast<int>(m_data.size()); }
    int GetCount() const { return GetSize(); }
    int GetUpperBound() const { return GetSize() - 1; }

    void SetSize(int newSize, int growBy = -1)
    {
        assert(newSize >= 0);
        if (growBy > 0 && static_cast<size_t>(newSize) > m_data.capacity())
            m_data.reserve(static_cast<size_t>(newSize) + static_cast<size_t>(growBy));
        m_data.resize(static_cast<size_t>(newSize));
    }
    void FreeExtra() { m_data.shrink_to_fit(); }
    void RemoveAll() { m_data.clear(); }

    TYPE GetAt(int index) const
    {
        assert(index >= 0 && index < GetSize());
        return m_data[static_cast<size_t>(index)];
    }
    void SetAt(int index, ARG_TYPE value)
    {
        assert(index >= 0 && index < GetSize());
        m_data[static_cast<size_t>(index)] = value;
    }
    TYPE& ElementAt(int index)
    {
        assert(index >= 0 && index < GetSize());
        return m_data[static_cast<size_t>(index)];
    }
    const TYPE& ElementAt(int index) const
    {
        assert(index >= 0 && index < GetSize());
        return m_data[static_cast<size_t>(index)];
    }

    TYPE* GetData() { return m_data.empty() ? nullptr : m_data.data(); }
    const TYPE* GetData() const { return m_data.empty() ? nullptr : m_data.data(); }

    void SetAtGrow(int index, ARG_TYPE value)
    {
        assert(index >= 0);
        if (index >= GetSize())
            m_data.resize(static_cast<size_t>(index) + 1);
        m_data[static_cast<size_t>(index)] = value;
    }
    int Add(ARG_TYPE value)
    {
        int index = GetSize();
        m_data.push_back(value);
        return index;
    }
    int Append(const CArray& src)
    {
        int index = GetSize();
        // Copy first so that appending an array to itself is safe.
        std::vector<TYPE> copy(src.m_data);
        m_data.insert(m_data.end(), copy.begin(), copy.end());
        return index;
    }
    void Copy(const CArray& src)
    {
        if (this != &src)
            m_data = src.m_data;
    }

    TYPE& operator[](int index) { return ElementAt(index); }
    const TYPE& operator[](int index) const { return ElementAt(index); }

    void InsertAt(int index, ARG_TYPE value, int count = 1)
    {
        assert(index >= 0 && count > 0);
        if (index >= GetSize())
            m_data.resize(static_cast<size_t>(index));
        TYPE copy = value;
        m_data.insert(m_data.begin() + index, static_cast<size_t>(count), copy);
    }
    void RemoveAt(int index, int count = 1)
    {
        assert(index >= 0 && count >= 0 && index + count <= GetSize());
        m_data.erase(m_data.begin() + index, m_data.begin() + index + count);
    }

protected:
    std::vector<TYPE> m_data;
};

class CPtrArray : public CArray<void*, void*> { };

class CDWordArray : public CArray<DWORD, DWORD> { };

template <class BASE_CLASS, class TYPE>
class CTypedPtrArray : public BASE_CLASS {
    static_assert(sizeof(TYPE) == sizeof(void*), "CTypedPtrArray requires pointer-sized TYPE");

public:
    TYPE GetAt(int index) const { return reinterpret_cast<TYPE>(BASE_CLASS::GetAt(index)); }
    TYPE& ElementAt(int index) { return reinterpret_cast<TYPE&>(BASE_CLASS::ElementAt(index)); }
    const TYPE& ElementAt(int index) const { return reinterpret_cast<const TYPE&>(BASE_CLASS::ElementAt(index)); }
    void SetAt(int index, TYPE value) { BASE_CLASS::SetAt(index, reinterpret_cast<void*>(value)); }
    void SetAtGrow(int index, TYPE value) { BASE_CLASS::SetAtGrow(index, reinterpret_cast<void*>(value)); }
    int Add(TYPE value) { return BASE_CLASS::Add(reinterpret_cast<void*>(value)); }
    void InsertAt(int index, TYPE value, int count = 1) { BASE_CLASS::InsertAt(index, reinterpret_cast<void*>(value), count); }
    TYPE* GetData() { return reinterpret_cast<TYPE*>(BASE_CLASS::GetData()); }
    const TYPE* GetData() const { return reinterpret_cast<const TYPE*>(BASE_CLASS::GetData()); }
    TYPE& operator[](int index) { return ElementAt(index); }
    const TYPE& operator[](int index) const { return ElementAt(index); }
};
