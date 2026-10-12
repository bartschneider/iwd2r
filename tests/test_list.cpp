// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_list.h"

#include <type_traits>
#include <vector>

namespace {

struct Item {
    int a;
    int b;
    bool operator==(const Item& o) const { return a == o.a && b == o.b; }
};

}

TEST_CASE("list iteration order and end position", "[list]")
{
    int v1 = 1, v2 = 2, v3 = 3;
    CTypedPtrList<CPtrList, int*> l;
    REQUIRE(l.IsEmpty());
    l.AddTail(&v1);
    l.AddTail(&v2);
    l.AddTail(&v3);
    REQUIRE(l.GetCount() == 3);
    REQUIRE(l.GetSize() == 3);

    std::vector<int> seen;
    POSITION pos = l.GetHeadPosition();
    while (pos)
        seen.push_back(*l.GetNext(pos));
    REQUIRE(seen == std::vector<int> { 1, 2, 3 });
    REQUIRE(pos == NULL);

    seen.clear();
    pos = l.GetTailPosition();
    while (pos)
        seen.push_back(*l.GetPrev(pos));
    REQUIRE(seen == std::vector<int> { 3, 2, 1 });
    REQUIRE(pos == NULL);
}

TEST_CASE("list value list of DWORD", "[list]")
{
    CList<DWORD, DWORD> l;
    l.AddTail(1);
    l.AddTail(2);
    l.AddTail(3);
    std::vector<DWORD> seen;
    POSITION pos = l.GetHeadPosition();
    while (pos)
        seen.push_back(l.GetNext(pos));
    REQUIRE(seen == std::vector<DWORD> { 1, 2, 3 });
    REQUIRE(pos == NULL);

    pos = l.GetHeadPosition();
    while (pos) {
        POSITION cur = pos;
        DWORD v = l.GetNext(pos);
        if (v == 2)
            l.RemoveAt(cur);
    }
    seen.clear();
    pos = l.GetHeadPosition();
    while (pos)
        seen.push_back(l.GetNext(pos));
    REQUIRE(seen == std::vector<DWORD> { 1, 3 });

    POSITION p3 = l.Find(3);
    REQUIRE(p3 != NULL);
    REQUIRE(l.GetAt(p3) == 3);
    REQUIRE(l.Find(1, l.GetHeadPosition()) == NULL);
    REQUIRE(l.FindIndex(1) == p3);
    REQUIRE(l.FindIndex(2) == NULL);
    REQUIRE(l.FindIndex(-1) == NULL);
    REQUIRE(l.RemoveHead() == 1);
    REQUIRE(l.RemoveTail() == 3);
    REQUIRE(l.IsEmpty());
}

TEST_CASE("list remove while iterating", "[list]")
{
    int v1 = 1, v2 = 2, v3 = 3;
    CTypedPtrList<CPtrList, int*> l;
    l.AddTail(&v1);
    l.AddTail(&v2);
    l.AddTail(&v3);

    POSITION pos = l.GetHeadPosition();
    while (pos) {
        POSITION cur = pos;
        int* v = l.GetNext(pos);
        if (*v == 2)
            l.RemoveAt(cur);
    }
    REQUIRE(l.GetCount() == 2);
    std::vector<int> seen;
    pos = l.GetHeadPosition();
    while (pos)
        seen.push_back(*l.GetNext(pos));
    REQUIRE(seen == std::vector<int> { 1, 3 });
}

TEST_CASE("list remove head and tail via RemoveAt", "[list]")
{
    int v1 = 1, v2 = 2, v3 = 3;
    CTypedPtrList<CPtrList, int*> l;
    l.AddTail(&v1);
    l.AddTail(&v2);
    l.AddTail(&v3);
    l.RemoveAt(l.GetHeadPosition());
    l.RemoveAt(l.GetTailPosition());
    REQUIRE(l.GetCount() == 1);
    REQUIRE(l.GetHead() == &v2);
    REQUIRE(l.GetTail() == &v2);
    l.RemoveAt(l.GetHeadPosition());
    REQUIRE(l.IsEmpty());
    REQUIRE(l.GetHeadPosition() == NULL);
    REQUIRE(l.GetTailPosition() == NULL);
}

TEST_CASE("list find and findindex", "[list]")
{
    int v1 = 1, v2 = 2, v3 = 3;
    CTypedPtrList<CPtrList, int*> l;
    POSITION p1 = l.AddTail(&v1);
    POSITION p2 = l.AddTail(&v2);
    l.AddTail(&v3);

    POSITION f = l.Find(&v2);
    REQUIRE(f == p2);
    REQUIRE(l.GetAt(f) == &v2);

    POSITION f3 = l.Find(&v3, p2);
    REQUIRE(f3 != NULL);
    REQUIRE(l.GetAt(f3) == &v3);

    REQUIRE(l.Find(&v1, p1) == NULL);
    REQUIRE(l.Find(&v1) == p1);

    REQUIRE(l.GetAt(l.FindIndex(1)) == &v2);
    REQUIRE(l.FindIndex(5) == NULL);
    REQUIRE(l.FindIndex(3) == NULL);
    REQUIRE(l.FindIndex(-1) == NULL);
}

TEST_CASE("list insert, set, remove head", "[list]")
{
    int v1 = 1, v2 = 2, v3 = 3, x = 9, y = 8;
    CTypedPtrList<CPtrList, int*> l;
    l.AddTail(&v1);
    l.AddTail(&v2);

    POSITION px = l.InsertBefore(l.GetHeadPosition(), &x);
    REQUIRE(l.GetHead() == &x);
    REQUIRE(l.GetAt(px) == &x);

    POSITION py = l.InsertAfter(l.GetTailPosition(), &y);
    REQUIRE(l.GetTail() == &y);
    REQUIRE(l.GetAt(py) == &y);

    l.SetAt(py, &v3);
    REQUIRE(l.GetTail() == &v3);

    // NULL positions behave like AddHead / AddTail
    l.InsertBefore(NULL, &y);
    REQUIRE(l.GetHead() == &y);
    l.InsertAfter(NULL, &y);
    REQUIRE(l.GetTail() == &y);
    REQUIRE(l.GetCount() == 6);

    int* h = l.RemoveHead();
    REQUIRE(h == &y);
    REQUIRE(l.GetCount() == 5);
    REQUIRE(l.RemoveTail() == &y);
    REQUIRE(l.GetCount() == 4);
    REQUIRE(l.GetHead() == &x);

    l.AddHead(&v1);
    REQUIRE(l.GetHead() == &v1);
}

TEST_CASE("list append another list", "[list]")
{
    int v1 = 1, v2 = 2, v3 = 3;
    CTypedPtrList<CPtrList, int*> l, other;
    l.AddTail(&v1);
    other.AddTail(&v2);
    other.AddTail(&v3);
    l.AddTail(&other);
    REQUIRE(l.GetCount() == 3);
    REQUIRE(other.GetCount() == 2);
    std::vector<int> seen;
    POSITION pos = l.GetHeadPosition();
    while (pos)
        seen.push_back(*l.GetNext(pos));
    REQUIRE(seen == std::vector<int> { 1, 2, 3 });

    CTypedPtrList<CPtrList, int*> front;
    front.AddTail(&v1);
    front.AddHead(&other);
    REQUIRE(front.GetCount() == 3);
    seen.clear();
    pos = front.GetHeadPosition();
    while (pos)
        seen.push_back(*front.GetNext(pos));
    REQUIRE(seen == std::vector<int> { 2, 3, 1 });
}

TEST_CASE("list of POSITION values", "[list]")
{
    int v1 = 1, v2 = 2;
    CTypedPtrList<CPtrList, int*> src;
    POSITION p1 = src.AddTail(&v1);
    POSITION p2 = src.AddTail(&v2);

    CTypedPtrList<CPtrList, POSITION> l;
    l.AddTail(p1);
    l.AddTail(p2);
    POSITION pos = l.GetHeadPosition();
    POSITION a = l.GetNext(pos);
    POSITION b = l.GetNext(pos);
    REQUIRE(a == p1);
    REQUIRE(b == p2);
    REQUIRE(pos == NULL);
    REQUIRE(src.GetAt(a) == &v1);
}

TEST_CASE("list struct values and const access", "[list]")
{
    CList<Item, Item> l;
    l.AddTail(Item { 1, 2 });
    l.AddTail(Item { 3, 4 });
    const CList<Item, Item>& cl = l;
    REQUIRE(cl.GetHead().a == 1);
    REQUIRE(cl.GetTail().b == 4);
    POSITION pos = cl.GetHeadPosition();
    const Item& first = cl.GetNext(pos);
    REQUIRE(first.b == 2);
    REQUIRE(cl.GetAt(pos).a == 3);
    l.GetAt(pos).a = 7;
    REQUIRE(l.GetTail().a == 7);
    REQUIRE(l.Find(Item { 7, 4 }) != NULL);

    CList<Item> byRef;
    byRef.AddHead(Item { 5, 6 });
    REQUIRE(byRef.GetHead().a == 5);
}

TEST_CASE("list RemoveAll and other pointer instantiations", "[list]")
{
    int v = 1;
    LONG lv = 2;
    CTypedPtrList<CPtrList, LONG*> ll;
    ll.AddTail(&lv);
    CTypedPtrList<CPtrList, void*> vl;
    vl.AddTail(&v);
    REQUIRE(vl.GetHead() == &v);

    CTypedPtrList<CPtrList, int*> l;
    l.AddTail(&v);
    l.AddTail(&v);
    REQUIRE(l.GetCount() == 2);
    l.RemoveAll();
    REQUIRE(l.GetCount() == 0);
    REQUIRE(l.IsEmpty());
    ll.RemoveAll();
    REQUIRE(ll.GetCount() == 0);

    CObList ob;
    CObject o;
    ob.AddTail(&o);
    REQUIRE(ob.GetHead() == &o);

    STATIC_REQUIRE(!std::is_copy_constructible<CPtrList>::value);
    STATIC_REQUIRE(!std::is_copy_assignable<CPtrList>::value);
}
