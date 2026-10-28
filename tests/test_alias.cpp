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

TEST_CASE("CAlias::ParseRawData only \":=\" names an alias, not a lone ':' or '='", "[alias]")
{
    // The separator the original searches for is the two-character string
    // ":=" (0x8B99EC, CString::Find at 0x7FAB62): a lone ':' or '=' in the
    // raw line parses nothing, so the alias stays empty (oracle case
    // aliases-781800).
    for (const char* raw : {"a:b", "a=b", "a: b", "a=b=c"}) {
        CString line(raw);
        CAlias alias(line);

        INFO(raw);
        CHECK(alias.field_4 == FALSE);
        CHECK(std::string(static_cast<const char*>(alias.field_8)).empty());
        CHECK(Values(alias).empty());
    }

    // With both characters the name is the text before the ':' plus the
    // path separator, and the value starts after ":=".
    CString line("a:=b");
    CAlias alias(line);

    CHECK(alias.field_4 == TRUE);
    CHECK(std::string(static_cast<const char*>(alias.field_8)) == "a:\\");
    CHECK(Values(alias) == "b\\");
}
