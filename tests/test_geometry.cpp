#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_geometry.h"

#include <type_traits>

TEST_CASE("geometry layout", "[geometry]")
{
    STATIC_REQUIRE(sizeof(CRect) == sizeof(RECT));
    STATIC_REQUIRE(sizeof(CPoint) == sizeof(POINT));
    STATIC_REQUIRE(sizeof(CSize) == sizeof(SIZE));
    STATIC_REQUIRE(std::is_standard_layout<CRect>::value);
    STATIC_REQUIRE(std::is_standard_layout<CPoint>::value);
    STATIC_REQUIRE(std::is_standard_layout<CSize>::value);
}

TEST_CASE("crect basics", "[geometry]")
{
    CRect r(0, 0, 10, 20);
    REQUIRE(r.Width() == 10);
    REQUIRE(r.Height() == 20);
    REQUIRE(r.Size() == CSize(10, 20));
    REQUIRE(r.CenterPoint() == CPoint(5, 10));
    REQUIRE(CRect().IsRectNull());
    REQUIRE(CRect().IsRectEmpty());
    REQUIRE_FALSE(r.IsRectNull());
    REQUIRE_FALSE(r.IsRectEmpty());

    r.TopLeft().x = 3;
    r.BottomRight().y = 30;
    REQUIRE(r == CRect(3, 0, 10, 30));
    REQUIRE(CRect(CPoint(1, 2), CSize(3, 4)) == CRect(1, 2, 4, 6));
    REQUIRE(CRect(CPoint(1, 2), CPoint(3, 4)) == CRect(1, 2, 3, 4));
    RECT raw = { 1, 2, 3, 4 };
    REQUIRE(CRect(raw) == CRect(1, 2, 3, 4));
    REQUIRE(CRect(&raw) == CRect(1, 2, 3, 4));
    REQUIRE(CRect(1, 2, 3, 4) != CRect(1, 2, 3, 5));
}

TEST_CASE("crect normalize and point tests", "[geometry]")
{
    CRect n(10, 10, 0, 0);
    n.NormalizeRect();
    REQUIRE(n == CRect(0, 0, 10, 10));
    CRect m(5, 1, 2, 9);
    m.NormalizeRect();
    REQUIRE(m == CRect(2, 1, 5, 9));

    CRect r(0, 0, 10, 10);
    REQUIRE(r.PtInRect(CPoint(9, 9)));
    REQUIRE_FALSE(r.PtInRect(CPoint(10, 9)));
    REQUIRE_FALSE(r.PtInRect(CPoint(9, 10)));
    REQUIRE(r.PtInRect(CPoint(0, 0)));
}

TEST_CASE("crect intersect and union", "[geometry]")
{
    REQUIRE((CRect(0, 0, 10, 10) & CRect(5, 5, 20, 20)) == CRect(5, 5, 10, 10));
    REQUIRE((CRect(0, 0, 10, 10) | CRect(20, 20, 30, 30)) == CRect(0, 0, 30, 30));
    CRect disjoint = CRect(0, 0, 10, 10) & CRect(20, 20, 30, 30);
    REQUIRE(disjoint.IsRectNull());

    CRect a(0, 0, 10, 10);
    a &= CRect(5, 5, 20, 20);
    REQUIRE(a == CRect(5, 5, 10, 10));
    a |= CRect(0, 0, 1, 1);
    REQUIRE(a == CRect(0, 0, 10, 10));

    CRect out;
    REQUIRE(out.IntersectRect(CRect(0, 0, 10, 10), CRect(5, 5, 20, 20)));
    REQUIRE(out == CRect(5, 5, 10, 10));
    REQUIRE_FALSE(out.IntersectRect(CRect(0, 0, 1, 1), CRect(5, 5, 6, 6)));
    REQUIRE(out.IsRectNull());
    REQUIRE(out.UnionRect(CRect(0, 0, 1, 1), CRect(5, 5, 6, 6)));
    REQUIRE(out == CRect(0, 0, 6, 6));
}

TEST_CASE("crect inflate, deflate, offset", "[geometry]")
{
    CRect r(1, 1, 2, 2);
    r.InflateRect(1, 2);
    REQUIRE(r == CRect(0, -1, 3, 4));
    r.DeflateRect(1, 2);
    REQUIRE(r == CRect(1, 1, 2, 2));
    r.InflateRect(CSize(1, 2));
    REQUIRE(r == CRect(0, -1, 3, 4));
    r.DeflateRect(CSize(1, 2));
    REQUIRE(r == CRect(1, 1, 2, 2));
    r.InflateRect(1, 2, 3, 4);
    REQUIRE(r == CRect(0, -1, 5, 6));
    r.DeflateRect(1, 2, 3, 4);
    REQUIRE(r == CRect(1, 1, 2, 2));
    RECT d = { 1, 2, 3, 4 };
    r.InflateRect(&d);
    REQUIRE(r == CRect(0, -1, 5, 6));
    r.DeflateRect(&d);
    REQUIRE(r == CRect(1, 1, 2, 2));

    REQUIRE(CRect(1, 1, 2, 2).OffsetRect(CPoint(5, 5)) == CRect(6, 6, 7, 7));
    CRect o(1, 1, 2, 2);
    o.OffsetRect(1, 2);
    REQUIRE(o == CRect(2, 3, 3, 4));
    o.OffsetRect(CSize(1, 1));
    REQUIRE(o == CRect(3, 4, 4, 5));
}

TEST_CASE("crect operators", "[geometry]")
{
    CRect r(1, 1, 2, 2);
    REQUIRE(r + CPoint(5, 5) == CRect(6, 6, 7, 7));
    REQUIRE(r - CPoint(5, 5) == CRect(-4, -4, -3, -3));
    REQUIRE(r + CSize(1, 2) == CRect(2, 3, 3, 4));
    REQUIRE(r - CSize(1, 2) == CRect(0, -1, 1, 0));
    RECT d = { 1, 2, 3, 4 };
    REQUIRE(r + &d == CRect(0, -1, 5, 6));
    REQUIRE(r - &d == CRect(2, 3, -1, -2));

    CRect q = r;
    q += CPoint(1, 1);
    REQUIRE(q == CRect(2, 2, 3, 3));
    q -= CPoint(1, 1);
    REQUIRE(q == r);
    q += CSize(1, 1);
    q -= CSize(1, 1);
    REQUIRE(q == r);
    q += &d;
    REQUIRE(q == CRect(0, -1, 5, 6));
    q -= &d;
    REQUIRE(q == r);
}

TEST_CASE("crect set and copy", "[geometry]")
{
    CRect r;
    r.SetRect(1, 2, 3, 4);
    REQUIRE(r == CRect(1, 2, 3, 4));
    r.SetRect(CPoint(5, 6), CPoint(7, 8));
    REQUIRE(r == CRect(5, 6, 7, 8));
    CRect c;
    c.CopyRect(&r);
    REQUIRE(c.EqualRect(&r));
    c.SetRectEmpty();
    REQUIRE(c.IsRectNull());
    REQUIRE_FALSE(c.EqualRect(&r));
}

TEST_CASE("crect converts to RECT pointer", "[geometry]")
{
    CRect r;
    RECT* p = r;
    p->right = 4;
    REQUIRE(r.right == 4);
    const CRect cr(1, 2, 3, 4);
    const RECT* cp = cr;
    REQUIRE(cp->bottom == 4);
}

TEST_CASE("cpoint", "[geometry]")
{
    REQUIRE(CPoint(1, 2) + CSize(3, 4) == CPoint(4, 6));
    REQUIRE(CPoint(4, 6) - CSize(3, 4) == CPoint(1, 2));
    REQUIRE(CPoint(4, 6) - CPoint(1, 2) == CSize(3, 4));
    REQUIRE(CPoint(1, 2) + CPoint(3, 4) == CPoint(4, 6));
    const POINT offset = { 10, 20 };
    REQUIRE(CPoint(1, 2) + offset == CPoint(11, 22));
    REQUIRE(CPoint(DWORD(0x00020001)) == CPoint(1, 2));
    REQUIRE(CPoint(DWORD(0xFFFFFFFE)) == CPoint(-2, -1));
    REQUIRE(CPoint(CSize(7, 8)) == CPoint(7, 8));
    REQUIRE(CPoint() == CPoint(0, 0));
    REQUIRE(CPoint(1, 2) != CPoint(1, 3));
    POINT pt = { 1, 2 };
    REQUIRE(CPoint(pt) == CPoint(1, 2));

    CPoint p(1, 2);
    p.Offset(1, 1);
    REQUIRE(p == CPoint(2, 3));
    p.Offset(CPoint(1, 1));
    REQUIRE(p == CPoint(3, 4));
    p.Offset(CSize(1, 1));
    REQUIRE(p == CPoint(4, 5));
    p += CSize(1, 1);
    REQUIRE(p == CPoint(5, 6));
    p -= CSize(1, 1);
    REQUIRE(p == CPoint(4, 5));
    p += CPoint(1, 1);
    p -= CPoint(2, 2);
    REQUIRE(p == CPoint(3, 4));
}

TEST_CASE("csize", "[geometry]")
{
    REQUIRE(CSize(1, 2) + CSize(3, 4) == CSize(4, 6));
    REQUIRE(CSize(4, 6) - CSize(3, 4) == CSize(1, 2));
    REQUIRE(-CSize(1, -2) == CSize(-1, 2));
    REQUIRE(CSize(DWORD(0x00020001)) == CSize(1, 2));
    REQUIRE(CSize(DWORD(0xFFFFFFFE)) == CSize(-2, -1));
    REQUIRE(CSize(CPoint(7, 8)) == CSize(7, 8));
    REQUIRE(CSize() == CSize(0, 0));
    REQUIRE(CSize(1, 2) != CSize(2, 1));
    SIZE sz = { 1, 2 };
    REQUIRE(CSize(sz) == CSize(1, 2));
    CSize s(1, 1);
    s += CSize(2, 2);
    REQUIRE(s == CSize(3, 3));
    s -= CSize(1, 1);
    REQUIRE(s == CSize(2, 2));
}

TEST_CASE("cpoint from a packed DWORD and negation", "[geometry]")
{
    STATIC_REQUIRE(std::is_convertible<DWORD, CPoint>::value);
    const CPoint p = static_cast<DWORD>(0xFFFE0003u);
    CHECK(p.x == 3);
    CHECK(p.y == -2);
    const CPoint n = -CPoint(4, -5);
    CHECK(n.x == -4);
    CHECK(n.y == 5);
}
