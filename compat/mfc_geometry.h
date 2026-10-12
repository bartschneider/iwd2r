// iwd2r. Distributed under LICENSE.md.
#pragma once

#include "compat/win_types.h"

class CSize;
class CRect;

class CPoint : public POINT {
public:
    CPoint() { x = 0, y = 0; }
    CPoint(int initX, int initY)
    {
        x = initX;
        y = initY;
    }
    CPoint(POINT initPt) { *static_cast<POINT*>(this) = initPt; }
    CPoint(SIZE initSize)
    {
        x = initSize.cx;
        y = initSize.cy;
    }
    // Low word is x, high word is y, both as signed 16-bit values.
    explicit CPoint(DWORD packed)
    {
        x = static_cast<short>(LOWORD(packed));
        y = static_cast<short>(HIWORD(packed));
    }

    void Offset(int dx, int dy)
    {
        x += dx;
        y += dy;
    }
    void Offset(POINT p) { Offset(p.x, p.y); }
    void Offset(SIZE s) { Offset(s.cx, s.cy); }

    bool operator==(POINT p) const { return x == p.x && y == p.y; }
    bool operator!=(POINT p) const { return !(*this == p); }

    void operator+=(POINT p) { Offset(p); }
    void operator+=(SIZE s) { Offset(s); }
    void operator-=(POINT p) { Offset(-p.x, -p.y); }
    void operator-=(SIZE s) { Offset(-s.cx, -s.cy); }

    CPoint operator+(SIZE s) const { return CPoint(x + s.cx, y + s.cy); }
    CPoint operator+(POINT p) const { return CPoint(x + p.x, y + p.y); }
    CPoint operator-(SIZE s) const { return CPoint(x - s.cx, y - s.cy); }
    inline CSize operator-(POINT p) const;
};

class CSize : public SIZE {
public:
    CSize() { cx = 0, cy = 0; }
    CSize(int initCX, int initCY)
    {
        cx = initCX;
        cy = initCY;
    }
    CSize(SIZE initSize) { *static_cast<SIZE*>(this) = initSize; }
    CSize(POINT initPt)
    {
        cx = initPt.x;
        cy = initPt.y;
    }
    // Low word is cx, high word is cy, both as signed 16-bit values.
    explicit CSize(DWORD packed)
    {
        cx = static_cast<short>(LOWORD(packed));
        cy = static_cast<short>(HIWORD(packed));
    }

    bool operator==(SIZE s) const { return cx == s.cx && cy == s.cy; }
    bool operator!=(SIZE s) const { return !(*this == s); }

    void operator+=(SIZE s)
    {
        cx += s.cx;
        cy += s.cy;
    }
    void operator-=(SIZE s)
    {
        cx -= s.cx;
        cy -= s.cy;
    }

    CSize operator+(SIZE s) const { return CSize(cx + s.cx, cy + s.cy); }
    CSize operator-(SIZE s) const { return CSize(cx - s.cx, cy - s.cy); }
    CSize operator-() const { return CSize(-cx, -cy); }
};

inline CSize CPoint::operator-(POINT p) const { return CSize(x - p.x, y - p.y); }

class CRect : public RECT {
public:
    CRect() { ::SetRectEmpty(this); }
    CRect(int l, int t, int r, int b) { ::SetRect(this, l, t, r, b); }
    CRect(const RECT& src) { ::CopyRect(this, &src); }
    CRect(const RECT* src) { ::CopyRect(this, src); }
    CRect(POINT topLeft, SIZE size)
    {
        ::SetRect(this, topLeft.x, topLeft.y, topLeft.x + size.cx, topLeft.y + size.cy);
    }
    CRect(POINT topLeft, POINT bottomRight) { ::SetRect(this, topLeft.x, topLeft.y, bottomRight.x, bottomRight.y); }

    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    CSize Size() const { return CSize(Width(), Height()); }
    CPoint& TopLeft() { return *reinterpret_cast<CPoint*>(&left); }
    const CPoint& TopLeft() const { return *reinterpret_cast<const CPoint*>(&left); }
    CPoint& BottomRight() { return *reinterpret_cast<CPoint*>(&right); }
    const CPoint& BottomRight() const { return *reinterpret_cast<const CPoint*>(&right); }
    CPoint CenterPoint() const { return CPoint((left + right) / 2, (top + bottom) / 2); }

    BOOL IsRectEmpty() const { return ::IsRectEmpty(this); }
    BOOL IsRectNull() const { return left == 0 && top == 0 && right == 0 && bottom == 0; }
    BOOL PtInRect(POINT p) const { return ::PtInRect(this, p); }

    void SetRect(int l, int t, int r, int b) { ::SetRect(this, l, t, r, b); }
    void SetRect(POINT topLeft, POINT bottomRight) { ::SetRect(this, topLeft.x, topLeft.y, bottomRight.x, bottomRight.y); }
    void SetRectEmpty() { ::SetRectEmpty(this); }
    void CopyRect(const RECT* src) { ::CopyRect(this, src); }
    BOOL EqualRect(const RECT* other) const { return ::EqualRect(this, other); }

    void InflateRect(int x, int y) { ::InflateRect(this, x, y); }
    void InflateRect(SIZE s) { ::InflateRect(this, s.cx, s.cy); }
    void InflateRect(const RECT* r) { InflateRect(r->left, r->top, r->right, r->bottom); }
    void InflateRect(int l, int t, int r, int b)
    {
        left -= l;
        top -= t;
        right += r;
        bottom += b;
    }
    void DeflateRect(int x, int y) { ::InflateRect(this, -x, -y); }
    void DeflateRect(SIZE s) { ::InflateRect(this, -s.cx, -s.cy); }
    void DeflateRect(const RECT* r) { InflateRect(-r->left, -r->top, -r->right, -r->bottom); }
    void DeflateRect(int l, int t, int r, int b) { InflateRect(-l, -t, -r, -b); }

    CRect& OffsetRect(int x, int y)
    {
        ::OffsetRect(this, x, y);
        return *this;
    }
    CRect& OffsetRect(POINT p) { return OffsetRect(p.x, p.y); }
    CRect& OffsetRect(SIZE s) { return OffsetRect(s.cx, s.cy); }

    CRect& NormalizeRect()
    {
        if (left > right) {
            LONG t = left;
            left = right;
            right = t;
        }
        if (top > bottom) {
            LONG t = top;
            top = bottom;
            bottom = t;
        }
        return *this;
    }

    BOOL IntersectRect(const RECT* a, const RECT* b) { return ::IntersectRect(this, a, b); }
    BOOL UnionRect(const RECT* a, const RECT* b) { return ::UnionRect(this, a, b); }

    operator RECT*() { return this; }
    operator const RECT*() const { return this; }

    bool operator==(const RECT& r) const { return ::EqualRect(this, &r); }
    bool operator!=(const RECT& r) const { return !::EqualRect(this, &r); }

    void operator+=(POINT p) { OffsetRect(p); }
    void operator+=(SIZE s) { OffsetRect(s); }
    void operator+=(const RECT* r) { InflateRect(r); }
    void operator-=(POINT p) { OffsetRect(-p.x, -p.y); }
    void operator-=(SIZE s) { OffsetRect(-s.cx, -s.cy); }
    void operator-=(const RECT* r) { DeflateRect(r); }
    void operator&=(const RECT& r) { ::IntersectRect(this, this, &r); }
    void operator|=(const RECT& r) { ::UnionRect(this, this, &r); }

    CRect operator+(POINT p) const
    {
        CRect r(*this);
        r.OffsetRect(p);
        return r;
    }
    CRect operator-(POINT p) const
    {
        CRect r(*this);
        r.OffsetRect(-p.x, -p.y);
        return r;
    }
    CRect operator+(SIZE s) const
    {
        CRect r(*this);
        r.OffsetRect(s);
        return r;
    }
    CRect operator-(SIZE s) const
    {
        CRect r(*this);
        r.OffsetRect(-s.cx, -s.cy);
        return r;
    }
    CRect operator+(const RECT* o) const
    {
        CRect r(*this);
        r.InflateRect(o);
        return r;
    }
    CRect operator-(const RECT* o) const
    {
        CRect r(*this);
        r.DeflateRect(o);
        return r;
    }
    // Exact-match overloads for RECT*: with only the const RECT* forms, `rect - ptr` is
    // ambiguous against built-in pointer subtraction through operator RECT*().
    CRect operator+(RECT* o) const { return *this + static_cast<const RECT*>(o); }
    CRect operator-(RECT* o) const { return *this - static_cast<const RECT*>(o); }
    CRect operator&(const RECT& o) const
    {
        CRect r;
        ::IntersectRect(&r, this, &o);
        return r;
    }
    CRect operator|(const RECT& o) const
    {
        CRect r;
        ::UnionRect(&r, this, &o);
        return r;
    }
};
