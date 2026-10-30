// CChitin::ParseCommandLine (0x7917F0), the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// ParseCommandLine reads m_sCommandLine (this+0xDC) and the start-up fields at
// +0x110..+0x136, and nothing else: no virtual is dispatched and no global is
// read. The fixture is therefore a zeroed CChitin from the original's operator
// new (0x1E80 bytes) whose six CString members (0xDC, 0x118, 0x11C, 0x120,
// 0x12C, 0x132) are built with the original's CString constructor (0x7FCC88),
// and, for ours, zeroed storage of sizeof(CChitin) with the same members
// placement-constructed. The case compares the six strings and the six fields
// the parse can touch, over a fixed sweep of command lines and a random one
// built from the option words, quoted values and stray tokens.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CChitin.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

using OriginalCStringDtorFn = void(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kOriginalCStringDtor = 0x7FCC1A;

using ParseFn = void(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kParseCommandLine = 0x7917F0;

// sizeof(CChitin) in the original, and the CString members the parse touches:
// m_sCommandLine, m_sStartUpAddress (+0x118), field_11C, m_sStartUpPlayer,
// m_sStartUpSession, m_sStartUpGameSpyLocation.
constexpr size_t kOriginalChitinSize = 0x1E80;
constexpr size_t kOriginalStrings[] = { 0xDC, 0x118, 0x11C, 0x120, 0x12C, 0x132 };

// An original CString is a pointer to its text at offset 0.
std::string OriginalString(const uint8_t* chitin, size_t offset)
{
    const char* const text = *reinterpret_cast<const char* const*>(chitin + offset);
    return text != nullptr ? text : "<null>";
}

int OriginalInt(const uint8_t* chitin, size_t offset)
{
    int value;
    std::memcpy(&value, chitin + offset, sizeof(value));
    return value;
}

void CheckCommandLine(CaseContext& ctx, const std::string& commandLine)
{
    ctx.SetInputs("command line \"" + commandLine + "\"");

    // Ours: a zeroed CChitin with only the CString members constructed.
    std::vector<unsigned char> oursStorage(sizeof(CChitin), 0);
    auto* ours = reinterpret_cast<CChitin*>(oursStorage.data());
    new (&ours->m_sCommandLine) CString();
    new (&ours->m_sStartUpAddress) CString();
    new (&ours->field_11C) CString();
    new (&ours->m_sStartUpPlayer) CString();
    new (&ours->m_sStartUpSession) CString();
    new (&ours->m_sStartUpGameSpyLocation) CString();

    ours->m_sCommandLine = commandLine.c_str();
    ours->ParseCommandLine();

    const std::string oursCommandLine = ours->m_sCommandLine;
    const std::string oursAddress = ours->m_sStartUpAddress;
    const std::string ours11C = ours->field_11C;
    const std::string oursPlayer = ours->m_sStartUpPlayer;
    const std::string oursSession = ours->m_sStartUpSession;
    const std::string oursLocation = ours->m_sStartUpGameSpyLocation;
    const int ours110 = ours->field_110;
    const int ours114 = ours->m_bStartUpConnect;
    const int ours124 = ours->field_124;
    const int ours128 = ours->field_128;
    const int ours131 = ours->field_131;
    const int ours136 = ours->field_136;

    ours->m_sCommandLine.~CString();
    ours->m_sStartUpAddress.~CString();
    ours->field_11C.~CString();
    ours->m_sStartUpPlayer.~CString();
    ours->m_sStartUpSession.~CString();
    ours->m_sStartUpGameSpyLocation.~CString();

    // The original: the same block, from its own heap (leaked by design -
    // case files may allocate and leak through the original), with the same
    // strings from its CString constructor.
    std::string originalCommandLine, originalAddress, original11C, originalPlayer;
    std::string originalSession, originalLocation;
    int original110 = 0, original114 = 0, original124 = 0, original128 = 0;
    int original131 = 0, original136 = 0;
    ctx.CallOriginal([&] {
        auto* block = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kOriginalChitinSize));
        std::memset(block, 0, kOriginalChitinSize);
        for (const size_t offset : kOriginalStrings) {
            reinterpret_cast<OriginalCStringFn>(kOriginalCString)(block + offset, nullptr,
                offset == 0xDC ? commandLine.c_str() : "");
        }

        reinterpret_cast<ParseFn>(kParseCommandLine)(block, nullptr);

        originalCommandLine = OriginalString(block, 0xDC);
        originalAddress = OriginalString(block, 0x118);
        original11C = OriginalString(block, 0x11C);
        originalPlayer = OriginalString(block, 0x120);
        originalSession = OriginalString(block, 0x12C);
        originalLocation = OriginalString(block, 0x132);
        original110 = OriginalInt(block, 0x110);
        original114 = OriginalInt(block, 0x114);
        original124 = OriginalInt(block, 0x124);
        original128 = OriginalInt(block, 0x128);
        original131 = block[0x131];
        original136 = OriginalInt(block, 0x136);

        for (const size_t offset : kOriginalStrings) {
            reinterpret_cast<OriginalCStringDtorFn>(kOriginalCStringDtor)(block + offset, nullptr);
        }
    });

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalCommandLine, oursCommandLine);
    oracle::Compare(ctx, originalAddress, oursAddress);
    oracle::Compare(ctx, original11C, ours11C);
    oracle::Compare(ctx, originalPlayer, oursPlayer);
    oracle::Compare(ctx, originalSession, oursSession);
    oracle::Compare(ctx, originalLocation, oursLocation);
    oracle::Compare(ctx, original110, ours110);
    oracle::Compare(ctx, original114, ours114);
    oracle::Compare(ctx, original124, ours124);
    oracle::Compare(ctx, original128, ours128);
    oracle::Compare(ctx, original131, ours131);
    oracle::Compare(ctx, original136, ours136);
}

void ChitinParseCommandLine(CaseContext& ctx)
{
    static const char* const kLines[] = {
        "",
        " ",
        "   ",
        "\t",
        "junk",
        "junk more",
        "junk +name A more",
        "+name Hero",
        "  +name   Hero   ",
        "+name",
        "+name \"\"",
        "+name \"Hero of Neverwinter\"",
        "+name \"unclosed",
        "+name \"A B\" rest",
        "+name A \"B C\"",
        "+name X junk +location here",
        "+name A +name B",
        "+NAME hero",
        "-name hero",
        "+name=Hero",
        "+name\tHero",
        "+connect 192.168.0.1:4000",
        "+connect host",
        "+connect host:",
        "+connect :4000",
        "+connect \"host:12\"",
        "+connect \"host:12\" +name Bob",
        "+connect a +host b",
        "+host a +connect b",
        "+host Server",
        "+host \"My Server\"",
        "+host Server +hostname Session",
        "+hostname Session",
        "+hostname Session +host Server",
        "+hostname \"My Session\" +host S",
        "+newgame",
        "+loadgame",
        "+newgame +loadgame",
        "+loadgame +newgame",
        "+location Here",
        "+location \"GameSpy Room\"",
        "+location",
        "+maxclients 4",
        "+maxclients 0",
        "+maxclients 99",
        "+maxclients -1",
        "+maxclients \"6\"",
        "+maxclients x",
        "+maxclients",
        "+maxclients 4 +name A",
        "+host S +hostname N +location L +maxclients 3",
        "+connect 1.2.3.4:5 +name P",
        "\"quoted\" +name A",
        "+name A \"B",
        " +host S  +connect 1:2  ",
        "+name A +unknown B +name C",
    };
    for (const char* line : kLines) {
        CheckCommandLine(ctx, line);
    }

    // Random lines from the option words, quoted values and stray tokens: the
    // tokenizer's edges (adjacent quotes, missing values, repeats) in
    // combinations the fixed list does not spell out.
    static const char* const kTokens[] = {
        "+host",
        "+connect",
        "+name",
        "+newgame",
        "+loadgame",
        "+hostname",
        "+location",
        "+maxclients",
        "junk",
        "+NAME",
        "-name",
        "12",
        "0",
        "host",
        "10.0.0.1:4000",
        "\"quoted value\"",
        "\"unclosed",
        "\"\"",
        "a:b",
        "A B",
    };
    constexpr uint32_t kTokenCount = sizeof(kTokens) / sizeof(kTokens[0]);
    oracle::RandomGen<uint32_t> count(ctx.seed, 0, 8);
    oracle::RandomGen<uint32_t> token(ctx.seed ^ 0x9E3779B9u, 0, kTokenCount - 1);
    oracle::RandomGen<uint32_t> spaces(ctx.seed ^ 0x51ED270Bu, 1, 3);
    const int randomLines = 200 + ctx.iterations / 10;
    for (int n = 0; n < randomLines; ++n) {
        std::string line;
        for (uint32_t i = 0, words = *count.Next(); i < words; ++i) {
            line.append(*spaces.Next(), ' ');
            line += kTokens[*token.Next()];
        }
        CheckCommandLine(ctx, line);
    }
}

} // namespace

ORACLE_CASE("chitin/parse_command_line", ChitinParseCommandLine);
