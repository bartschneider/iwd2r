// iwd2r. Distributed under LICENSE.md.
// MFC CList, CPtrList, CObList and CTypedPtrList. A POSITION is the address of
// a list node, as in MFC, so positions stay valid across removals of other
// elements.
#pragma once

#include "compat/win_types.h"

#include <cassert>

struct __POSITION;
typedef __POSITION* POSITION;

class CObject {
public:
    virtual ~CObject() { }
};

template <class TYPE, class ARG_TYPE = const TYPE&>
class CList {
protected:
    struct CNode {
        CNode* pNext;
        CNode* pPrev;
        TYPE data;
        explicit CNode(ARG_TYPE value)
            : pNext(nullptr)
            , pPrev(nullptr)
            , data(value)
        {
        }
    };

    static CNode* Node(POSITION position) { return reinterpret_cast<CNode*>(position); }
    static POSITION Pos(CNode* node) { return reinterpret_cast<POSITION>(node); }

public:
    CList(int blockSize = 10)
    {
        (void)blockSize;
    }
    CList(const CList&) = delete;
    CList& operator=(const CList&) = delete;
    ~CList() { RemoveAll(); }

    int GetCount() const { return m_nCount; }
    int GetSize() const { return m_nCount; }
    BOOL IsEmpty() const { return m_nCount == 0; }

    TYPE& GetHead()
    {
        assert(m_pNodeHead != nullptr);
        return m_pNodeHead->data;
    }
    const TYPE& GetHead() const
    {
        assert(m_pNodeHead != nullptr);
        return m_pNodeHead->data;
    }
    TYPE& GetTail()
    {
        assert(m_pNodeTail != nullptr);
        return m_pNodeTail->data;
    }
    const TYPE& GetTail() const
    {
        assert(m_pNodeTail != nullptr);
        return m_pNodeTail->data;
    }

    TYPE RemoveHead()
    {
        assert(m_pNodeHead != nullptr);
        CNode* old = m_pNodeHead;
        TYPE value = old->data;
        m_pNodeHead = old->pNext;
        if (m_pNodeHead != nullptr)
            m_pNodeHead->pPrev = nullptr;
        else
            m_pNodeTail = nullptr;
        delete old;
        --m_nCount;
        return value;
    }
    TYPE RemoveTail()
    {
        assert(m_pNodeTail != nullptr);
        CNode* old = m_pNodeTail;
        TYPE value = old->data;
        m_pNodeTail = old->pPrev;
        if (m_pNodeTail != nullptr)
            m_pNodeTail->pNext = nullptr;
        else
            m_pNodeHead = nullptr;
        delete old;
        --m_nCount;
        return value;
    }

    POSITION AddHead(ARG_TYPE newElement)
    {
        CNode* node = new CNode(newElement);
        node->pNext = m_pNodeHead;
        if (m_pNodeHead != nullptr)
            m_pNodeHead->pPrev = node;
        else
            m_pNodeTail = node;
        m_pNodeHead = node;
        ++m_nCount;
        return Pos(node);
    }
    POSITION AddTail(ARG_TYPE newElement)
    {
        CNode* node = new CNode(newElement);
        node->pPrev = m_pNodeTail;
        if (m_pNodeTail != nullptr)
            m_pNodeTail->pNext = node;
        else
            m_pNodeHead = node;
        m_pNodeTail = node;
        ++m_nCount;
        return Pos(node);
    }
    void AddHead(CList* pNewList)
    {
        assert(pNewList != nullptr);
        POSITION position = pNewList->GetTailPosition();
        while (position != nullptr)
            AddHead(pNewList->GetPrev(position));
    }
    void AddTail(CList* pNewList)
    {
        assert(pNewList != nullptr);
        POSITION position = pNewList->GetHeadPosition();
        while (position != nullptr)
            AddTail(pNewList->GetNext(position));
    }

    void RemoveAll()
    {
        CNode* node = m_pNodeHead;
        while (node != nullptr) {
            CNode* next = node->pNext;
            delete node;
            node = next;
        }
        m_pNodeHead = nullptr;
        m_pNodeTail = nullptr;
        m_nCount = 0;
    }

    POSITION GetHeadPosition() const { return Pos(m_pNodeHead); }
    POSITION GetTailPosition() const { return Pos(m_pNodeTail); }

    TYPE& GetNext(POSITION& rPosition)
    {
        CNode* node = Node(rPosition);
        assert(node != nullptr);
        rPosition = Pos(node->pNext);
        return node->data;
    }
    const TYPE& GetNext(POSITION& rPosition) const
    {
        CNode* node = Node(rPosition);
        assert(node != nullptr);
        rPosition = Pos(node->pNext);
        return node->data;
    }
    TYPE& GetPrev(POSITION& rPosition)
    {
        CNode* node = Node(rPosition);
        assert(node != nullptr);
        rPosition = Pos(node->pPrev);
        return node->data;
    }
    const TYPE& GetPrev(POSITION& rPosition) const
    {
        CNode* node = Node(rPosition);
        assert(node != nullptr);
        rPosition = Pos(node->pPrev);
        return node->data;
    }

    TYPE& GetAt(POSITION position)
    {
        assert(position != nullptr);
        return Node(position)->data;
    }
    const TYPE& GetAt(POSITION position) const
    {
        assert(position != nullptr);
        return Node(position)->data;
    }
    void SetAt(POSITION position, ARG_TYPE newElement)
    {
        assert(position != nullptr);
        Node(position)->data = newElement;
    }

    void RemoveAt(POSITION position)
    {
        CNode* node = Node(position);
        assert(node != nullptr);
        if (node == m_pNodeHead)
            m_pNodeHead = node->pNext;
        else
            node->pPrev->pNext = node->pNext;
        if (node == m_pNodeTail)
            m_pNodeTail = node->pPrev;
        else
            node->pNext->pPrev = node->pPrev;
        delete node;
        --m_nCount;
    }

    POSITION InsertBefore(POSITION position, ARG_TYPE newElement)
    {
        if (position == nullptr)
            return AddHead(newElement);
        CNode* old = Node(position);
        CNode* node = new CNode(newElement);
        node->pPrev = old->pPrev;
        node->pNext = old;
        if (old->pPrev != nullptr)
            old->pPrev->pNext = node;
        else
            m_pNodeHead = node;
        old->pPrev = node;
        ++m_nCount;
        return Pos(node);
    }
    POSITION InsertAfter(POSITION position, ARG_TYPE newElement)
    {
        if (position == nullptr)
            return AddTail(newElement);
        CNode* old = Node(position);
        CNode* node = new CNode(newElement);
        node->pPrev = old;
        node->pNext = old->pNext;
        if (old->pNext != nullptr)
            old->pNext->pPrev = node;
        else
            m_pNodeTail = node;
        old->pNext = node;
        ++m_nCount;
        return Pos(node);
    }

    POSITION Find(ARG_TYPE searchValue, POSITION startAfter = nullptr) const
    {
        CNode* node = Node(startAfter);
        node = (node == nullptr) ? m_pNodeHead : node->pNext;
        for (; node != nullptr; node = node->pNext) {
            if (node->data == searchValue)
                return Pos(node);
        }
        return nullptr;
    }
    POSITION FindIndex(int nIndex) const
    {
        if (nIndex < 0 || nIndex >= m_nCount)
            return nullptr;
        CNode* node = m_pNodeHead;
        while (nIndex-- > 0)
            node = node->pNext;
        return Pos(node);
    }

protected:
    CNode* m_pNodeHead = nullptr;
    CNode* m_pNodeTail = nullptr;
    int m_nCount = 0;
};

class CPtrList : public CList<void*, void*> {
public:
    CPtrList(int blockSize = 10)
        : CList<void*, void*>(blockSize)
    {
    }
};

class CObList : public CList<CObject*, CObject*> {
public:
    CObList(int blockSize = 10)
        : CList<CObject*, CObject*>(blockSize)
    {
    }
};

template <class BASE_CLASS, class TYPE>
class CTypedPtrList : public BASE_CLASS {
public:
    CTypedPtrList(int blockSize = 10)
        : BASE_CLASS(blockSize)
    {
    }

    TYPE& GetHead() { return reinterpret_cast<TYPE&>(BASE_CLASS::GetHead()); }
    TYPE GetHead() const { return static_cast<TYPE>(BASE_CLASS::GetHead()); }
    TYPE& GetTail() { return reinterpret_cast<TYPE&>(BASE_CLASS::GetTail()); }
    TYPE GetTail() const { return static_cast<TYPE>(BASE_CLASS::GetTail()); }
    TYPE RemoveHead() { return static_cast<TYPE>(BASE_CLASS::RemoveHead()); }
    TYPE RemoveTail() { return static_cast<TYPE>(BASE_CLASS::RemoveTail()); }

    TYPE& GetNext(POSITION& rPosition) { return reinterpret_cast<TYPE&>(BASE_CLASS::GetNext(rPosition)); }
    TYPE GetNext(POSITION& rPosition) const { return static_cast<TYPE>(BASE_CLASS::GetNext(rPosition)); }
    TYPE& GetPrev(POSITION& rPosition) { return reinterpret_cast<TYPE&>(BASE_CLASS::GetPrev(rPosition)); }
    TYPE GetPrev(POSITION& rPosition) const { return static_cast<TYPE>(BASE_CLASS::GetPrev(rPosition)); }

    TYPE& GetAt(POSITION position) { return reinterpret_cast<TYPE&>(BASE_CLASS::GetAt(position)); }
    TYPE GetAt(POSITION position) const { return static_cast<TYPE>(BASE_CLASS::GetAt(position)); }
    void SetAt(POSITION position, TYPE newElement) { BASE_CLASS::SetAt(position, newElement); }

    POSITION AddHead(TYPE newElement) { return BASE_CLASS::AddHead(newElement); }
    POSITION AddTail(TYPE newElement) { return BASE_CLASS::AddTail(newElement); }
    void AddHead(CTypedPtrList<BASE_CLASS, TYPE>* pNewList) { BASE_CLASS::AddHead(pNewList); }
    void AddTail(CTypedPtrList<BASE_CLASS, TYPE>* pNewList) { BASE_CLASS::AddTail(pNewList); }

    POSITION InsertBefore(POSITION position, TYPE newElement) { return BASE_CLASS::InsertBefore(position, newElement); }
    POSITION InsertAfter(POSITION position, TYPE newElement) { return BASE_CLASS::InsertAfter(position, newElement); }
    POSITION Find(TYPE searchValue, POSITION startAfter = nullptr) const { return BASE_CLASS::Find(searchValue, startAfter); }
    POSITION FindIndex(int nIndex) const { return BASE_CLASS::FindIndex(nIndex); }

    void RemoveAt(POSITION position) { BASE_CLASS::RemoveAt(position); }
    void RemoveAll() { BASE_CLASS::RemoveAll(); }
    int GetCount() const { return BASE_CLASS::GetCount(); }
    int GetSize() const { return BASE_CLASS::GetSize(); }
    BOOL IsEmpty() const { return BASE_CLASS::IsEmpty(); }
    POSITION GetHeadPosition() const { return BASE_CLASS::GetHeadPosition(); }
    POSITION GetTailPosition() const { return BASE_CLASS::GetTailPosition(); }
};
