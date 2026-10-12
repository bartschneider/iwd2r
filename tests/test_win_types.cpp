// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/win_types.h"

#include <type_traits>

TEST_CASE("typedef widths", "[win_types]")
{
    STATIC_REQUIRE(sizeof(DWORD) == 4);
    STATIC_REQUIRE(sizeof(LONG) == 4);
    STATIC_REQUIRE(sizeof(BOOL) == 4);
    STATIC_REQUIRE(sizeof(BOOLEAN) == 1);
    STATIC_REQUIRE(sizeof(WORD) == 2);
    STATIC_REQUIRE(sizeof(WPARAM) == sizeof(void*));
    STATIC_REQUIRE(!std::is_same<HWND, HDC>::value);
    STATIC_REQUIRE(!std::is_same<HWND, HANDLE>::value);
    STATIC_REQUIRE(std::is_same<HMODULE, HINSTANCE>::value);
    STATIC_REQUIRE(TRUE == 1);
    STATIC_REQUIRE(FALSE == 0);
}

TEST_CASE("rect helpers", "[win_types]")
{
    RECT a;
    RECT b;
    RECT out;
    SetRect(&a, 0, 0, 10, 10);
    SetRect(&b, 5, 5, 20, 20);
    REQUIRE(IntersectRect(&out, &a, &b) == TRUE);
    REQUIRE(EqualRect(&out, &(const RECT&)RECT { 5, 5, 10, 10 }));

    SetRect(&b, 10, 0, 20, 10);
    out = RECT { 1, 2, 3, 4 };
    REQUIRE(IntersectRect(&out, &a, &b) == FALSE);
    REQUIRE((out.left == 0 && out.top == 0 && out.right == 0 && out.bottom == 0));

    POINT edge = { 10, 10 };
    POINT inside = { 9, 9 };
    REQUIRE(PtInRect(&a, edge) == FALSE);
    REQUIRE(PtInRect(&a, inside) == TRUE);

    RECT empty;
    SetRectEmpty(&empty);
    REQUIRE(IsRectEmpty(&empty) == TRUE);
    SetRect(&b, 3, 3, 7, 7);
    REQUIRE(UnionRect(&out, &empty, &b) == TRUE);
    REQUIRE(EqualRect(&out, &b));
    REQUIRE(UnionRect(&out, &b, &empty) == TRUE);
    REQUIRE(EqualRect(&out, &b));
    REQUIRE(UnionRect(&out, &empty, &empty) == FALSE);
    REQUIRE(IsRectEmpty(&out) == TRUE);

    SetRect(&b, 5, 5, 20, 20);
    REQUIRE(UnionRect(&out, &a, &b) == TRUE);
    REQUIRE(EqualRect(&out, &(const RECT&)RECT { 0, 0, 20, 20 }));

    RECT c;
    CopyRect(&c, &a);
    REQUIRE(EqualRect(&c, &a));
    OffsetRect(&c, 2, 3);
    REQUIRE(EqualRect(&c, &(const RECT&)RECT { 2, 3, 12, 13 }));
    InflateRect(&c, 1, 2);
    REQUIRE(EqualRect(&c, &(const RECT&)RECT { 1, 1, 13, 15 }));
}

TEST_CASE("color and word helpers", "[win_types]")
{
    COLORREF c = RGB(1, 2, 3);
    REQUIRE(c == 0x030201u);
    REQUIRE(GetRValue(c) == 1);
    REQUIRE(GetGValue(c) == 2);
    REQUIRE(GetBValue(c) == 3);
    REQUIRE(LOWORD(0x12345678) == 0x5678);
    REQUIRE(HIWORD(0x12345678) == 0x1234);
    REQUIRE(LOBYTE(0x1234) == 0x34);
    REQUIRE(HIBYTE(0x1234) == 0x12);
    REQUIRE(MAKELONG(0x5678, 0x1234) == 0x12345678);
    REQUIRE(MAKEWORD(0x34, 0x12) == 0x1234);
    REQUIRE(SUCCEEDED(S_OK));
    REQUIRE(SUCCEEDED(S_FALSE));
    REQUIRE(FAILED(E_FAIL));
    REQUIRE(static_cast<uint32_t>(E_NOINTERFACE) == 0x80004002u);
    REQUIRE(static_cast<uint32_t>(E_OUTOFMEMORY) == 0x8007000Eu);
}

DEFINE_GUID(IID_Test, 0x12345678, 0x1234, 0x5678, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08);

TEST_CASE("guid", "[win_types]")
{
    REQUIRE(IsEqualGUID(GUID_NULL, GUID {}));
    REQUIRE_FALSE(IsEqualGUID(IID_Test, GUID_NULL));
    REQUIRE(IsEqualGUID(IID_Test, IID_Test));
    REQUIRE(IID_Test.Data1 == 0x12345678u);
}

TEST_CASE("vk and wm values", "[win_types]")
{
    REQUIRE(VK_LBUTTON == 1);
    REQUIRE(VK_ESCAPE == 0x1B);
    REQUIRE(VK_OEM_1 == 0xBA);
    REQUIRE(VK_PROCESSKEY == 0xE5);
    REQUIRE(VK_F12 == 0x7B);
    REQUIRE(VK_NUMPAD0 == 0x60);
    REQUIRE(VK_OEM_8 == 0xDF);
    REQUIRE(WM_CLOSE == 0x10);
    REQUIRE(WM_MOUSEWHEEL == 0x20A);
    REQUIRE(WM_SYSKEYDOWN == 0x104);
    REQUIRE(WM_QUIT == 0x12);
    REQUIRE(WM_USER == 0x400);
    REQUIRE(WAIT_TIMEOUT == 0x102);
    REQUIRE(MAX_PATH == 260);
}
