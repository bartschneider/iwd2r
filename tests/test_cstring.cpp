// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "compat/mfc_resource.h"
#include "compat/mfc_string.h"

#include <cstring>
#include <string>

TEST_CASE("cstring layout", "[cstring]")
{
    STATIC_REQUIRE(sizeof(CString) == sizeof(void*));
    CHECK(CString().GetLength() == 0);
    CHECK(CString().IsEmpty());

    CString a("hello");
    CString b(a);
    b.SetAt(0, 'J');
    CHECK(a == "hello");
    CHECK(b == "Jello");

    CString c;
    c = a;
    c.MakeUpper();
    CHECK(a == "hello");
    CHECK(c == "HELLO");
}

TEST_CASE("cstring clamping", "[cstring]")
{
    CString s("abc");
    CHECK(s.Mid(1) == "bc");
    CHECK(s.Mid(5) == "");
    CHECK(s.Mid(1, 50) == "bc");
    CHECK(s.Left(50) == "abc");
    CHECK(s.Left(-1) == "");
    CHECK(s.Right(2) == "bc");
    CHECK(s.Right(50) == "abc");
}

TEST_CASE("cstring find", "[cstring]")
{
    CString s("a,b,c");
    CHECK(s.Find(',') == 1);
    CHECK(s.Find(',', 2) == 3);
    CHECK(s.Find("c") == 4);
    CHECK(s.Find('x') == -1);
    CHECK(s.FindOneOf("xc") == 4);
    CHECK(s.ReverseFind(',') == 3);
    CHECK(s.Find(',', 99) == -1);
}

TEST_CASE("cstring trim", "[cstring]")
{
    CString s("  a b \t");
    s.TrimLeft();
    CHECK(s == "a b \t");

    CString t("  a b \t");
    t.TrimRight();
    CHECK(t == "  a b");

    CString u("xxay");
    u.TrimLeft('x');
    CHECK(u == "ay");

    CString v("axyxy");
    v.TrimRight("yx");
    CHECK(v == "a");
}

TEST_CASE("cstring compare", "[cstring]")
{
    CHECK(CString("ABC").CompareNoCase("abc") == 0);
    CHECK(CString("abc").Compare("abd") < 0);
    CHECK(CString("a") == "a");
    CHECK("a" == CString("a"));
    CHECK(CString("a") != "b");
    CHECK(CString("a") < CString("b"));
    CHECK("b" > CString("a"));
    CHECK(CString("a") <= "a");
    CHECK(CString("a") >= CString("a"));
}

TEST_CASE("cstring case", "[cstring]")
{
    CString s("aBc1");
    s.MakeUpper();
    CHECK(s == "ABC1");
    s.MakeLower();
    CHECK(s == "abc1");
}

TEST_CASE("cstring format", "[cstring]")
{
    CString s;
    s.Format("%d-%s-%c", 7, CString("x"), 'y');
    CHECK(s == "7-x-y");

    s = "hi";
    s.Format("%s!", s);
    CHECK(s == "hi!");

    s.Format("%5.2f", 3.14159);
    CHECK(s == " 3.14");

    s.Format("%s", "");
    CHECK(s == "");

    CString big(std::string(5000, 'q').c_str());
    const CString& cbig = big;
    s.Format("%s", cbig);
    CHECK(s.GetLength() == 5000);
    CHECK(s == big);
}

TEST_CASE("cstring buffer", "[cstring]")
{
    CString s;
    char* p = s.GetBuffer(10);
    strcpy(p, "abc");
    s.ReleaseBuffer();
    CHECK(s == "abc");
    CHECK(s.GetLength() == 3);

    p = s.GetBuffer(10);
    strcpy(p, "abc");
    s.ReleaseBuffer(2);
    CHECK(s == "ab");
}

TEST_CASE("cstring span and edit", "[cstring]")
{
    CHECK(CString("abc123").SpanIncluding("abc") == "abc");
    CHECK(CString("abc123").SpanExcluding("12") == "abc");

    CString a("abcb");
    CHECK(a.Remove('b') == 2);
    CHECK(a == "ac");

    CString b("abab");
    CHECK(b.Replace("ab", "x") == 2);
    CHECK(b == "xx");

    CString c("abcd");
    c.Delete(1, 2);
    CHECK(c == "ad");
    c.Insert(1, "XY");
    CHECK(c == "aXYd");
}

TEST_CASE("cstring operators", "[cstring]")
{
    CHECK(CString("a") + "b" + CString("c") + 'd' == "abcd");

    CString s("a");
    s += "b";
    s += CString("c");
    s += 'd';
    CHECK(s == "abcd");
    CHECK(s.GetAt(1) == 'b');
    s.SetAt(1, 'Z');
    CHECK(s[1] == 'Z');
    CHECK(s == "aZcd");
}

TEST_CASE("cstring LoadStringA reads the string table", "[cstring]")
{
    CString s("keep");
    CHECK(s.LoadStringA(IDS_ENGLISH) == TRUE);
    CHECK(s == CString(iwd2r_ResourceString(IDS_ENGLISH)));
    CHECK(s.LoadStringA(9999) == FALSE);
    CHECK(s == CString(iwd2r_ResourceString(IDS_ENGLISH)));
}

TEST_CASE("cstring GetBuffer without a length", "[cstring]")
{
    CString s("abc");
    char* p = s.GetBuffer();
    CHECK(std::strcmp(p, "abc") == 0);
    s.ReleaseBuffer();
    CHECK(s.GetLength() == 3);
}
