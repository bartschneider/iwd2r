#include <catch2/catch_test_macros.hpp>

#include "CAlias.h"
#include "CAliasList.h"

#include <string>

namespace {

// The parsed values joined with '|', in list order.
std::string Values(const CAlias& alias)
{
    std::string joined;
    for (POSITION pos = alias.field_C.GetHeadPosition(); pos != NULL;) {
        if (!joined.empty()) {
            joined += '|';
        }
        joined += static_cast<const char*>(alias.field_C.GetNext(pos));
    }
    return joined;
}

} // namespace

TEST_CASE("CAlias::ParseRawData ends a value without a trailing backslash in one", "[alias]")
{
    // The GOG icewind2.ini ships HD0: without the trailing separator.
    CString raw("HD0:=C:\\GOG Games\\Icewind Dale II");
    CAlias alias(raw);

    CHECK(alias.field_4 == TRUE);
    CHECK(std::string(static_cast<const char*>(alias.field_8)) == "HD0:\\");
    CHECK(Values(alias) == "C:\\GOG Games\\Icewind Dale II\\");
}

TEST_CASE("CAlias::ParseRawData ends every semicolon-separated value in a backslash", "[alias]")
{
    CString raw("a:=b;c\\");
    CAlias alias(raw);

    CHECK(alias.field_4 == TRUE);
    CHECK(Values(alias) == "b\\|c\\");
}

TEST_CASE("CAliasList::ResolveFileName joins a parsed alias without a semicolon", "[alias]")
{
    CString raw("HD0:=C:\\GOG Games\\Icewind Dale II");
    CAliasList aliases;
    aliases.AddAlias(new CAlias(raw));

    CString requested("HD0:\\data\\GUIfont.bif");
    CString resolved;
    CHECK(aliases.ResolveFileName(requested, resolved) == TRUE);
    CHECK(std::string(static_cast<const char*>(resolved)) == "C:\\GOG Games\\Icewind Dale II\\data\\GUIfont.bif");
}
