// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_array.h"
#include "compat/mfc_map.h"

#include <set>
#include <type_traits>
#include <utility>

namespace {

struct Rect {
    int l;
};

}

TEST_CASE("array SetAtGrow Add and upper bound", "[array]")
{
    CArray<int, int> a;
    REQUIRE(a.GetSize() == 0);
    REQUIRE(a.GetUpperBound() == -1);
    REQUIRE(a.GetData() == nullptr);
    a.SetAtGrow(5, 7);
    REQUIRE(a.GetSize() == 6);
    REQUIRE(a.GetCount() == 6);
    REQUIRE(a[5] == 7);
    REQUIRE(a[0] == 0);
    REQUIRE(a.GetUpperBound() == a.GetSize() - 1);

    CArray<int, int> b;
    REQUIRE(b.Add(10) == 0);
    REQUIRE(b.Add(20) == 1);
    REQUIRE(b.GetAt(1) == 20);
    b.SetAt(0, 11);
    b.ElementAt(1) = 21;
    REQUIRE(b[0] == 11);
    REQUIRE(b[1] == 21);
    REQUIRE(b.GetData()[1] == 21);
    const CArray<int, int>& cb = b;
    REQUIRE(cb[1] == 21);
}

TEST_CASE("array SetSize InsertAt RemoveAt", "[array]")
{
    CArray<int, int> a;
    a.SetSize(3, 10);
    REQUIRE(a.GetSize() == 3);
    REQUIRE(a[2] == 0);
    a[0] = 1;
    a[1] = 2;
    a[2] = 3;
    a.InsertAt(0, 9);
    REQUIRE(a.GetSize() == 4);
    REQUIRE(a[0] == 9);
    REQUIRE(a[1] == 1);
    REQUIRE(a[3] == 3);
    a.InsertAt(1, 5, 2);
    REQUIRE(a.GetSize() == 6);
    REQUIRE(a[1] == 5);
    REQUIRE(a[2] == 5);
    REQUIRE(a[3] == 1);
    a.RemoveAt(0, 2);
    REQUIRE(a.GetSize() == 4);
    REQUIRE(a[0] == 5);
    a.RemoveAt(0);
    REQUIRE(a.GetSize() == 3);
    a.InsertAt(5, 8);
    REQUIRE(a.GetSize() == 6);
    REQUIRE(a[3] == 0);
    REQUIRE(a[5] == 8);
    a.SetSize(2);
    REQUIRE(a.GetSize() == 2);
    a.FreeExtra();
    a.RemoveAll();
    REQUIRE(a.GetSize() == 0);
}

TEST_CASE("array Append and Copy", "[array]")
{
    CArray<int, int> a, b;
    a.Add(1);
    a.Add(2);
    b.Add(3);
    REQUIRE(b.Append(a) == 1);
    REQUIRE(b.GetSize() == 3);
    REQUIRE(b[2] == 2);
    b.Copy(a);
    REQUIRE(b.GetSize() == 2);
    REQUIRE(b[0] == 1);
}

TEST_CASE("array engine instantiations", "[array]")
{
    int x = 1, y = 2;
    CTypedPtrArray<CPtrArray, int*> p;
    REQUIRE(p.Add(&x) == 0);
    p.Add(&y);
    static_assert(std::is_same<decltype(p.GetAt(0)), int*>::value, "GetAt returns int*");
    static_assert(std::is_same<decltype(p[0]), int*&>::value, "operator[] returns int*&");
    static_assert(std::is_same<decltype(p.GetData()), int**>::value, "GetData returns int**");
    REQUIRE(p.GetAt(1) == &y);
    REQUIRE(p[0] == &x);
    p.SetAt(0, &y);
    REQUIRE(*p.ElementAt(0) == 2);
    p.InsertAt(0, &x);
    REQUIRE(p.GetSize() == 3);
    REQUIRE(p[0] == &x);
    p.SetAtGrow(4, &x);
    REQUIRE(p.GetSize() == 5);
    REQUIRE(p[3] == nullptr);
    REQUIRE(p.GetData()[4] == &x);

    Rect r { 4 };
    CArray<Rect*> rs;
    rs.Add(&r);
    REQUIRE(rs[0]->l == 4);

    CDWordArray d;
    d.Add(0xFFFFFFFFu);
    REQUIRE(d[0] == 0xFFFFFFFFu);
}

TEST_CASE("map WordToPtr lookup and removal", "[map]")
{
    CMapWordToPtr m;
    int v = 0;
    void* q = nullptr;
    REQUIRE(m.GetStartPosition() == NULL);
    m.SetAt(3, &v);
    REQUIRE(m.Lookup(3, q) == TRUE);
    REQUIRE(q == &v);
    q = nullptr;
    REQUIRE(m.Lookup(4, q) == FALSE);
    REQUIRE(q == nullptr);
    REQUIRE(m.GetCount() == 1);
    m.SetAt(3, nullptr);
    REQUIRE(m.GetCount() == 1);
    REQUIRE(m[9] == nullptr);
    REQUIRE(m.GetCount() == 2);
    m[9] = &v;
    REQUIRE(m.Lookup(9, q));
    REQUIRE(q == &v);
    REQUIRE(m.RemoveKey(3) == TRUE);
    REQUIRE(m.RemoveKey(3) == FALSE);
    REQUIRE(m.Lookup(3, q) == FALSE);
    m.RemoveAll();
    REQUIRE(m.GetCount() == 0);
    REQUIRE(m.GetStartPosition() == NULL);
}

TEST_CASE("map iteration visits every pair once in insertion order", "[map]")
{
    CMapWordToPtr m;
    int vals[4];
    const WORD keys[4] = { 40, 7, 1000, 2 };
    for (int i = 0; i < 4; ++i)
        m.SetAt(keys[i], &vals[i]);
    int n = 0;
    for (POSITION pos = m.GetStartPosition(); pos != NULL;) {
        WORD k = 0;
        void* v = nullptr;
        m.GetNextAssoc(pos, k, v);
        REQUIRE(k == keys[n]);
        REQUIRE(v == &vals[n]);
        ++n;
    }
    REQUIRE(n == 4);

    CMapStringToString s;
    s.SetAt("a", "1");
    s.SetAt("b", "2");
    s.SetAt("c", "3");
    s.RemoveKey("b");
    std::set<std::string> seen;
    POSITION pos = s.GetStartPosition();
    while (pos != NULL) {
        CString k, v;
        s.GetNextAssoc(pos, k, v);
        seen.insert(std::string(k) + "=" + std::string(v));
    }
    REQUIRE(seen == std::set<std::string> { "a=1", "c=3" });
}

TEST_CASE("map StringToString is case sensitive", "[map]")
{
    CMapStringToString m;
    m.SetAt("A", "upper");
    m.SetAt("a", "lower");
    REQUIRE(m.GetCount() == 2);
    CString out;
    REQUIRE(m.Lookup("A", out));
    REQUIRE(out == "upper");
    REQUIRE(m.Lookup("a", out));
    REQUIRE(out == "lower");
    REQUIRE_FALSE(m.Lookup("b", out));
    m["c"] = "x";
    REQUIRE(m.GetCount() == 3);
    REQUIRE(m["c"] == "x");
    REQUIRE(m.RemoveKey("A"));
    REQUIRE_FALSE(m.Lookup("A", out));
    REQUIRE(m.GetCount() == 2);
}

TEST_CASE("map removing the returned key during iteration", "[map]")
{
    for (int count = 1; count <= 3; ++count) {
        CMapWordToPtr m;
        for (int i = 1; i <= count; ++i)
            m.SetAt(static_cast<WORD>(i), nullptr);
        int visited = 0;
        for (POSITION pos = m.GetStartPosition(); pos != NULL;) {
            WORD k = 0;
            void* v = nullptr;
            m.GetNextAssoc(pos, k, v);
            ++visited;
            REQUIRE(k == visited);
            REQUIRE(m.RemoveKey(k));
        }
        REQUIRE(visited == count);
        REQUIRE(m.GetCount() == 0);
    }
}

TEST_CASE("map removing the last element then continuing iteration", "[map]")
{
    CMapWordToPtr m;
    m.SetAt(1, nullptr);
    m.SetAt(2, nullptr);
    m.SetAt(3, nullptr);
    POSITION pos = m.GetStartPosition();
    WORD k = 0;
    void* v = nullptr;
    m.GetNextAssoc(pos, k, v);
    REQUIRE(k == 1);
    m.GetNextAssoc(pos, k, v);
    REQUIRE(k == 2);
    REQUIRE(pos != NULL);
    m.RemoveKey(3);
    m.RemoveKey(2);
    m.SetAt(4, nullptr);
    m.GetNextAssoc(pos, k, v);
    REQUIRE(k == 4);
    REQUIRE(pos == NULL);
}
