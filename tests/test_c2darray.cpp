#include <catch2/catch_test_macros.hpp>

#include "C2DArray.h"

namespace {

const char kTable[] = "2DA V1.0\r\n"
                      "-1\r\n"
                      "  1  2\r\n"
                      "BARBARIAN  0  1000\r\n"
                      "bard  0  2147483648\r\n"
                      "FIGHTER  0\r\n";

} // namespace

TEST_CASE("C2DArray::LoadText parses labels, cells and the default", "[2da]")
{
    C2DArray table;
    table.LoadText(CString(kTable));
    REQUIRE(table.GetWidth() == 2);
    REQUIRE(table.GetHeight() == 3);
    REQUIRE(table.GetDefault() == "-1");
    REQUIRE(table.GetColumnNames()[1] == "BARD");
    REQUIRE(table.GetAt(CString("2"), CString("barbarian")) == "1000");
    REQUIRE(table.GetAt(CString("2"), CString("FIGHTER")) == "");
    REQUIRE(table.GetAt(CString("3"), CString("FIGHTER")) == "-1");
}

TEST_CASE("C2DArray::GetAtLong wraps like VC6 atol", "[2da]")
{
    C2DArray table;
    table.LoadText(CString(kTable));
    REQUIRE(table.GetAtLong(CPoint(1, 0)) == 1000);
    REQUIRE(table.GetAtLong(CPoint(1, 1)) == INT32_MIN);
    REQUIRE(table.GetAtLong(CPoint(2, 0)) == -1);
}

TEST_CASE("C2DArray::Find(CString&, CPoint&) finds every row label", "[2da]")
{
    C2DArray table;
    table.LoadText(CString(kTable));
    for (int y = 0; y < 3; ++y) {
        CString label = table.GetColumnNames()[y];
        CPoint location(-7, -7);
        REQUIRE(table.Find(label, location));
        REQUIRE(location.x == -1);
        REQUIRE(location.y == y);
    }
    CString missing("1");
    CPoint location(-7, -7);
    REQUIRE_FALSE(table.Find(missing, location));
    REQUIRE(location.x == -7);
    REQUIRE(location.y == -7);
}
