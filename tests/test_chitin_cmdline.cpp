// CChitin::ParseCommandLine (0x7917F0): the start-up options.
//
// The function reads and writes only m_sCommandLine (this+0xDC) and the
// start-up fields at +0x110..+0x136: no virtual is dispatched and no other
// member is touched, so the test runs it on a zeroed CChitin whose CStrings
// are constructed - the same fixture the oracle case uses. The expectations
// below are the original's control flow, quoted in src/CChitin.cpp.
#include <catch2/catch_test_macros.hpp>

#include "CChitin.h"

#include <new>
#include <string>
#include <vector>

namespace {

class ChitinCommandLine {
public:
    ChitinCommandLine()
    {
        m_storage.resize(sizeof(CChitin), 0);
        m_chitin = reinterpret_cast<CChitin*>(m_storage.data());
        new (&m_chitin->m_sCommandLine) CString();
        new (&m_chitin->m_sStartUpAddress) CString();
        new (&m_chitin->field_11C) CString();
        new (&m_chitin->m_sStartUpPlayer) CString();
        new (&m_chitin->m_sStartUpSession) CString();
        new (&m_chitin->m_sStartUpGameSpyLocation) CString();
    }

    ~ChitinCommandLine()
    {
        m_chitin->m_sCommandLine.~CString();
        m_chitin->m_sStartUpAddress.~CString();
        m_chitin->field_11C.~CString();
        m_chitin->m_sStartUpPlayer.~CString();
        m_chitin->m_sStartUpSession.~CString();
        m_chitin->m_sStartUpGameSpyLocation.~CString();
    }

    void Parse(const std::string& commandLine)
    {
        m_chitin->m_sCommandLine = commandLine.c_str();
        m_chitin->ParseCommandLine();
    }

    CChitin* operator->() { return m_chitin; }

private:
    std::vector<unsigned char> m_storage;
    CChitin* m_chitin = nullptr;
};

} // namespace

// The options that set a flag and a string.
TEST_CASE("chitin: ParseCommandLine takes +name, +connect and +host", "[chitin]")
{
    SECTION("+name")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+name Hero");
        CHECK(chitin->m_sStartUpPlayer == "Hero");
        CHECK(chitin->m_sCommandLine == "");
    }

    SECTION("+name takes the rest of the line as separate options")
    {
        ChitinCommandLine chitin;
        chitin.Parse("  +name   Hero   ");
        CHECK(chitin->m_sStartUpPlayer == "Hero");
        CHECK(chitin->m_sCommandLine == "");
    }

    SECTION("+name with a quoted value")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+name \"Hero of Neverwinter\"");
        CHECK(chitin->m_sStartUpPlayer == "Hero of Neverwinter");
        CHECK(chitin->m_sCommandLine == "");
    }

    SECTION("+name drops an unclosed quoted value")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+name \"unclosed");
        CHECK(chitin->m_sStartUpPlayer == "");
        CHECK(chitin->m_sCommandLine == "");
    }

    SECTION("a quoted value may be followed by more words")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+name \"A B\" rest");
        CHECK(chitin->m_sStartUpPlayer == "A B");
        CHECK(chitin->m_sCommandLine == " rest");
    }

    SECTION("+connect splits address:port")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+connect 192.168.0.1:4000");
        CHECK(chitin->m_bStartUpConnect == TRUE);
        CHECK(chitin->m_sStartUpAddress == "192.168.0.1");
        CHECK(chitin->field_11C == "4000");
    }

    SECTION("+connect without a port")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+connect host");
        CHECK(chitin->m_bStartUpConnect == TRUE);
        CHECK(chitin->m_sStartUpAddress == "host");
        CHECK(chitin->field_11C == "");
    }

    SECTION("+connect with a quoted address:port")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+connect \"host:12\" +name Bob");
        CHECK(chitin->m_bStartUpConnect == TRUE);
        CHECK(chitin->m_sStartUpAddress == "host");
        CHECK(chitin->field_11C == "12");
        CHECK(chitin->m_sStartUpPlayer == "Bob");
    }

    SECTION("+host sets field_110 and field_11C")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+host Server");
        CHECK(chitin->field_110 == 1);
        CHECK(chitin->field_11C == "Server");
        CHECK(chitin->m_bStartUpConnect == FALSE);
    }

    SECTION("+host and +connect: the first one wins")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+connect a +host b");
        CHECK(chitin->m_bStartUpConnect == TRUE);
        CHECK(chitin->m_sStartUpAddress == "a");
        CHECK(chitin->field_110 == 0);
        CHECK(chitin->field_11C == "");
    }

    SECTION("+host and +connect: the first one wins, the other way")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+host a +connect b");
        CHECK(chitin->field_110 == 1);
        CHECK(chitin->field_11C == "a");
        CHECK(chitin->m_bStartUpConnect == FALSE);
        CHECK(chitin->m_sStartUpAddress == "");
    }

    SECTION("+hostname sets the session only after +host")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+host Server +hostname Session");
        CHECK(chitin->m_sStartUpSession == "Session");
    }

    SECTION("+hostname without +host is ignored")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+hostname Session");
        CHECK(chitin->m_sStartUpSession == "");
    }

    SECTION("+hostname before +host is ignored")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+hostname Session +host Server");
        CHECK(chitin->m_sStartUpSession == "");
        CHECK(chitin->field_110 == 1);
        CHECK(chitin->field_11C == "Server");
    }
}

// The swaps and the numeric option.
TEST_CASE("chitin: ParseCommandLine takes +newgame, +loadgame and +maxclients", "[chitin]")
{
    SECTION("+newgame wins whichever comes first")
    {
        ChitinCommandLine both;
        both.Parse("+newgame +loadgame");
        CHECK(both->field_124 == 1);
        CHECK(both->field_128 == 0);

        ChitinCommandLine reversed;
        reversed.Parse("+loadgame +newgame");
        CHECK(reversed->field_128 == 1);
        CHECK(reversed->field_124 == 0);
    }

    SECTION("+maxclients stores whatever ToNumber returns")
    {
        const struct {
            const char* commandLine;
            int expected;
        } cases[] = {
            { "+maxclients 4", 4 },
            { "+maxclients 0", 0 },
            { "+maxclients 99", 99 },
            { "+maxclients -1", -1 },
            { "+maxclients \"6\"", 6 },
            { "+maxclients x", 0 },
            { "+maxclients", 0 },
        };
        for (const auto& c : cases) {
            INFO(c.commandLine);
            ChitinCommandLine chitin;
            chitin.Parse(c.commandLine);
            CHECK(chitin->field_136 == c.expected);
        }
    }
}

// The GameSpy location, the case-sensitive option names and the words the
// parse leaves in m_sCommandLine.
TEST_CASE("chitin: ParseCommandLine keeps the words it does not know", "[chitin]")
{
    SECTION("+location")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+location \"GameSpy Room\"");
        CHECK(chitin->field_131 == 1);
        CHECK(chitin->m_sStartUpGameSpyLocation == "GameSpy Room");
    }

    SECTION("option names are case-sensitive")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+NAME hero");
        CHECK(chitin->m_sStartUpPlayer == "");
        CHECK(chitin->m_sCommandLine == " +NAME hero");
    }

    SECTION("unknown words are restored, each with a leading space")
    {
        ChitinCommandLine chitin;
        chitin.Parse("junk +name A more");
        CHECK(chitin->m_sStartUpPlayer == "A");
        CHECK(chitin->m_sCommandLine == " junk more");

        ChitinCommandLine plain;
        plain.Parse("junk");
        CHECK(plain->m_sCommandLine == " junk");

        ChitinCommandLine empty;
        empty.Parse("   ");
        CHECK(empty->m_sCommandLine == "");
    }

    SECTION("the last value of a repeated option wins")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+name A +name B");
        CHECK(chitin->m_sStartUpPlayer == "B");
    }

    SECTION("quotes in an unknown word stay in the restored line")
    {
        ChitinCommandLine chitin;
        chitin.Parse("+name A \"B C\"");
        CHECK(chitin->m_sStartUpPlayer == "A");
        CHECK(chitin->m_sCommandLine == " \"B C\"");
    }
}
