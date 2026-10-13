// 2DA lookups (C2DArray), the XP rules (CRuleTables) and arcane spell failure
// (CGameSprite), the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. The original parses 2DA text only inside C2DArray::Load
// (0x402B70), after its resource manager hands it the resource
// (CDimm::GetResObject, CResText::Demand): it has no routine that parses text
// on its own, so the parse itself is the pending case tables/load. Our side
// parses each text with C2DArray::LoadText, the parsing half of our Load. The
// original's C2DArray constructor is inlined everywhere, so its object is a
// zeroed 0x24-byte block from the original's operator new, filled with what
// our parse produced: the label and cell arrays laid out as the original's
// new CString[n] lays them out (a count dword, then the strings) and every
// string built by the original's CString(LPCSTR) constructor. Lookups then
// run on both tables, and only plain data (text, positions, numbers) is
// compared. Original objects are never freed.
//
// Calling conventions (objdump of the vanilla exe): the C2DArray and
// CRuleTables methods are __thiscall (this in ecx, callee pops), called here
// through __fastcall pointers whose edx argument is unused. None returns a
// struct, so there is no hidden return pointer.
#include "C2DArray.h"
#include "CRuleTables.h"

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret; Load's
// new CString[n] calls it (0x402E6B) and pops its argument.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CString::CString(LPCSTR) (0x7FCC88): this in ecx, the text at [esp+4],
// ret 4. A pointer whose high word is zero would be read as a string
// resource id; ours never are.
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

// C2DArray::GetAt(const CString& nX, const CString& nY) (0x403340): ret 8,
// returns const CString& in eax.
using GetAtLabelsFn = const void*(__fastcall*)(const void* self, void* edx, const void* x, const void* y);
constexpr uintptr_t kGetAtLabels = 0x403340;
// C2DArray::GetAtLong(const CPoint&) (0x403440): ret 4, LONG in eax.
using GetAtLongFn = int32_t(__fastcall*)(const void* self, void* edx, const int32_t* point);
constexpr uintptr_t kGetAtLong = 0x403440;
// C2DArray::GetAt(const CPoint&) (0x622320): ret 4 at 0x622350 and 0x622357,
// returns const CString& in eax.
using GetAtPointFn = const void*(__fastcall*)(const void* self, void* edx, const int32_t* point);
constexpr uintptr_t kGetAtPoint = 0x622320;
// C2DArray::Find(CString&, CPoint&) (0x4038E0): ret 8 at 0x403962, BOOLEAN
// in al.
using FindRowFn = uint8_t(__fastcall*)(const void* self, void* edx, const void* text, int32_t* point);
constexpr uintptr_t kFindRow = 0x4038E0;
// CRuleTables::GetNextLevelXP(BYTE) (0x5438D0): ret 4 at 0x5439AB; it masks
// the pushed dword to its low byte (0x54392D). Reads only this+0x604, the
// C2DArray m_tExperienceLevels, and CRuleTables::GetClassString (0x541DA0),
// which copies a global CString without reading this.
using NextLevelXpFn = uint32_t(__fastcall*)(const void* self, void* edx, uint32_t level);
constexpr uintptr_t kGetNextLevelXP = 0x5438D0;
// CRuleTables::GetXPCap (0x544C70): `mov eax, 0x80E80; ret`; reads neither
// ecx nor the stack.
using XpCapFn = uint32_t(__fastcall*)(const void* self, void* edx);
constexpr uintptr_t kGetXPCap = 0x544C70;

// The original C2DArray: m_pNamesX at 0x10, m_pNamesY 0x14, m_pArray 0x18,
// m_default 0x1C, m_nSizeX 0x20, m_nSizeY 0x22 (GetAt 0x622320, Load's
// stores at 0x402E3C); 0x24 bytes (CRuleTables' members are 0x24 apart).
constexpr size_t kC2DArraySize = 0x24;
// CInfGame::m_ruleTables runs from 0 to 0x1B58 (CInfGame.h).
constexpr size_t kRuleTablesSize = 0x1B58;
constexpr size_t kExperienceLevelsOffset = 0x604;

void* OriginalNew(size_t size)
{
    return reinterpret_cast<OriginalNewFn>(kOriginalNew)(size);
}

void OriginalCString(void* at, const std::string& text)
{
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(at, nullptr, text.c_str());
}

// An original CString holding text, in leaked original memory.
const void* OriginalString(const std::string& text)
{
    void* at = OriginalNew(4);
    OriginalCString(at, text);
    return at;
}

const char* OriginalText(const void* cstring)
{
    return *static_cast<const char* const*>(cstring);
}

// A table as plain data: what our parse made of a 2DA text.
struct TableData {
    std::vector<std::string> namesX; // column labels
    std::vector<std::string> namesY; // row labels
    std::vector<std::string> cells; // row by row
    std::string defaultValue;
    int sizeX = 0;
    int sizeY = 0;
};

// Our C2DArray's members, read directly so that a fixture never depends on a
// function under test. Pointers to protected members taken in a derived
// class apply to any C2DArray.
struct Members : C2DArray {
    static const CString* NamesX(const C2DArray& t) { return t.*(&Members::m_pNamesX); }
    static const CString* NamesY(const C2DArray& t) { return t.*(&Members::m_pNamesY); }
    static const CString* Cells(const C2DArray& t) { return t.*(&Members::m_pArray); }
    static const CString& Default(const C2DArray& t) { return t.*(&Members::m_default); }
    static int SizeX(const C2DArray& t) { return t.*(&Members::m_nSizeX); }
    static int SizeY(const C2DArray& t) { return t.*(&Members::m_nSizeY); }
};

TableData DataOf(const C2DArray& table)
{
    TableData data;
    data.sizeX = Members::SizeX(table);
    data.sizeY = Members::SizeY(table);
    for (int x = 0; x < data.sizeX; ++x) {
        data.namesX.push_back(static_cast<const char*>(Members::NamesX(table)[x]));
    }
    for (int y = 0; y < data.sizeY; ++y) {
        data.namesY.push_back(static_cast<const char*>(Members::NamesY(table)[y]));
    }
    for (int i = 0; i < data.sizeX * data.sizeY; ++i) {
        data.cells.push_back(static_cast<const char*>(Members::Cells(table)[i]));
    }
    data.defaultValue = static_cast<const char*>(Members::Default(table));
    return data;
}

// new CString[n] as the original lays it out: the count, then n strings.
uint32_t OriginalStringArray(const std::vector<std::string>& texts)
{
    auto* block = static_cast<uint32_t*>(OriginalNew(4 + 4 * texts.size()));
    block[0] = static_cast<uint32_t>(texts.size());
    for (size_t i = 0; i < texts.size(); ++i) {
        OriginalCString(&block[1 + i], texts[i]);
    }
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block + 1));
}

// Fills the original C2DArray at `at` (zeroed by the caller).
void FillOriginalTable(uint8_t* at, const TableData& data)
{
    const uint32_t namesX = OriginalStringArray(data.namesX);
    const uint32_t namesY = OriginalStringArray(data.namesY);
    const uint32_t cells = OriginalStringArray(data.cells);
    std::memcpy(at + 0x10, &namesX, 4);
    std::memcpy(at + 0x14, &namesY, 4);
    std::memcpy(at + 0x18, &cells, 4);
    OriginalCString(at + 0x1C, data.defaultValue);
    const int16_t sizeX = static_cast<int16_t>(data.sizeX);
    const int16_t sizeY = static_cast<int16_t>(data.sizeY);
    std::memcpy(at + 0x20, &sizeX, 2);
    std::memcpy(at + 0x22, &sizeY, 2);
}

// Synthetic 2DA texts for the parsing edges the lookups must agree on.
// "signed": signs, overflow and junk after digits, a short row, a row of
// only a label, a row with more cells than labels, a lower-case row label.
const char kSignedTable[] = "2DA V1.0\r\n"
                            "0\r\n"
                            "        A       B       C       D       E\r\n"
                            "ROW1    1       +5      -5      --5     +-5\r\n"
                            "ROW2    2147483647      2147483648      -2147483648     -2147483649     99999999999\r\n"
                            "ROW3    12abc   abc     0x10    007     -\r\n"
                            "ROW4    1\r\n"
                            "ROW5\r\n"
                            "row6    4294967296      -0      +       4294967295      x       extra\r\n";
// "labels": a default with junk, duplicate and lower-case labels, a row
// whose label is empty, a blank line, tabs and LF-only line ends.
//
// Every text and lookup word is ASCII. The original's CString::MakeUpper
// (0x7FD0F0) is the CRT's _mbsupr (0x7EB13E), which upper-cases bytes past
// 0x7F through the multibyte tables _setmbcp built from the system's ANSI
// code page (on code page 1252, 0xE9 becomes 0xC9; on a double-byte code page
// lead bytes go through LCMapString); ours upper-cases ASCII only. The game's
// tables are ASCII, so this is not compared: see docs/evidence/403340.md.
const char kLabelsTable[] = "2DA V1.0\n"
                            "-12abc\n"
                            "\tlower\tUPPER\tMixed\tlower\tcafe\n"
                            "DUP a b c d e\n"
                            "DUP f g h i j\n"
                            "  x y\n"
                            "\n"
                            "EMPTY\n"
                            "last\t-7\t+8\t\t9";
// "empty": a default and nothing else.
const char kEmptyTable[] = "2DA V1.0\r\n"
                           "  +42  \r\n";

std::string ReadTable(const CaseContext& ctx, const char* name)
{
    if (ctx.tables.empty()) {
        throw std::runtime_error(std::string("no --tables folder for ") + name);
    }
    const std::string path = ctx.tables + "\\" + name;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + path + " (tools/oracle.py run extracts it)");
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct Fixture {
    std::string name;
    C2DArray* ours = nullptr; // owned by OurTables
    const uint8_t* original = nullptr;
    TableData data;
};

// Our C2DArrays, freed at the end of a case (they own only our memory).
struct OurTables {
    std::vector<C2DArray*> tables;
    ~OurTables()
    {
        for (C2DArray* table : tables) {
            delete table;
        }
    }
};

Fixture MakeFixture(CaseContext& ctx, OurTables& ours, const std::string& name, const std::string& text)
{
    Fixture f;
    f.name = name;
    f.ours = new C2DArray();
    ours.tables.push_back(f.ours);
    f.ours->LoadText(CString(text.c_str()));
    f.data = DataOf(*f.ours);
    ctx.SetInputs("table " + name + ": building the original's C2DArray");
    ctx.CallOriginal([&] {
        auto* at = static_cast<uint8_t*>(OriginalNew(kC2DArraySize));
        std::memset(at, 0, kC2DArraySize);
        FillOriginalTable(at, f.data);
        f.original = at;
    });
    return f;
}

std::vector<Fixture> Fixtures(CaseContext& ctx, OurTables& ours)
{
    std::vector<Fixture> fixtures;
    fixtures.push_back(MakeFixture(ctx, ours, "XPLEVEL.2DA", ReadTable(ctx, "XPLEVEL.2DA")));
    fixtures.push_back(MakeFixture(ctx, ours, "signed", kSignedTable));
    fixtures.push_back(MakeFixture(ctx, ours, "labels", kLabelsTable));
    fixtures.push_back(MakeFixture(ctx, ours, "empty", kEmptyTable));
    return fixtures;
}

std::string Lower(std::string text)
{
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

// Lookup strings: every label and cell, each in lower case too, and labels no
// table has.
std::vector<std::string> Words(const TableData& data, bool withCells)
{
    std::vector<std::string> words = { "", "NOPE", " A", "A ", "ROW1\r", "1 ", "-1" };
    auto add = [&](const std::string& word) {
        for (const std::string& known : words) {
            if (known == word) {
                return;
            }
        }
        words.push_back(word);
    };
    for (const auto* list : { &data.namesX, &data.namesY }) {
        for (const std::string& name : *list) {
            add(name);
            add(Lower(name));
        }
    }
    if (withCells) {
        for (const std::string& cell : data.cells) {
            add(cell);
            add(Lower(cell));
        }
        add(data.defaultValue);
    }
    return words;
}

std::string Quoted(const std::string& text)
{
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '\r') {
            out += "\\r";
        } else if (c < 0x20 || c >= 0x7F) {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02X", c);
            out += hex;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

// Where a returned CString lives: the default, a cell (x, y), or elsewhere,
// with its text.
std::string Where(const void* found, const void* defaultValue, const void* firstCell, size_t cellSize,
    const TableData& data, const std::string& text)
{
    if (found == defaultValue) {
        return "the default " + Quoted(text);
    }
    const auto offset = static_cast<const uint8_t*>(found) - static_cast<const uint8_t*>(firstCell);
    const long long count = static_cast<long long>(data.sizeX) * data.sizeY;
    if (firstCell && offset >= 0 && offset % static_cast<long long>(cellSize) == 0
        && offset / static_cast<long long>(cellSize) < count) {
        const long long i = offset / static_cast<long long>(cellSize);
        return "cell (" + std::to_string(i % data.sizeX) + ", " + std::to_string(i / data.sizeX) + ") "
            + Quoted(text);
    }
    return "another string " + Quoted(text);
}

std::string OriginalWhere(const Fixture& f, const void* found)
{
    uint32_t cells = 0;
    std::memcpy(&cells, f.original + 0x18, 4);
    return Where(found, f.original + 0x1C, reinterpret_cast<const void*>(static_cast<uintptr_t>(cells)), 4, f.data,
        OriginalText(found));
}

std::string OursWhere(const Fixture& f, const CString& found)
{
    return Where(&found, &Members::Default(*f.ours), Members::Cells(*f.ours), sizeof(CString), f.data,
        static_cast<const char*>(found));
}

// A generator pair over [0, n) for random picks, apart from other streams by
// `salt`.
struct Picks {
    Picks(uint64_t seed, uint64_t salt)
        : state(seed ^ salt)
        , bits(oracle::SplitMix64(&state))
    {
    }
    int Below(int n) { return n <= 0 ? 0 : static_cast<int>(*bits.Next() % static_cast<uint32_t>(n)); }
    int Between(int lo, int hi) { return lo + Below(hi - lo + 1); }
    // The word with each letter's case flipped at random.
    std::string Mangle(std::string word)
    {
        for (char& c : word) {
            if (((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) && Below(2)) {
                c = static_cast<char>(c ^ 0x20);
            }
        }
        return word;
    }

    uint64_t state;
    oracle::RandomGen<uint32_t> bits;
};

void CompareGetAtLabels(CaseContext& ctx, const Fixture& f, const std::string& x, const std::string& y)
{
    ctx.SetInputs("table " + f.name + ": GetAt(" + Quoted(x) + ", " + Quoted(y) + ")");
    std::string original;
    ctx.CallOriginal([&] {
        const void* nx = OriginalString(x);
        const void* ny = OriginalString(y);
        original = OriginalWhere(f, reinterpret_cast<GetAtLabelsFn>(kGetAtLabels)(f.original, nullptr, nx, ny));
    });
    const std::string ours = OursWhere(f, f.ours->GetAt(CString(x.c_str()), CString(y.c_str())));
    ctx.CountInputs(1);
    if (original != ours) {
        ctx.Fail(ctx.Inputs(), original, ours);
    }
}

// C2DArray::GetAt(const CString&, const CString&) at 0x403340: every pair of
// lookup words, then --iterations random pairs (letters' case flipped at
// random), per table.
void GetAtLabels(CaseContext& ctx)
{
    OurTables owned;
    for (const Fixture& f : Fixtures(ctx, owned)) {
        const std::vector<std::string> words = Words(f.data, false);
        for (const std::string& x : words) {
            for (const std::string& y : words) {
                CompareGetAtLabels(ctx, f, x, y);
            }
        }
        Picks picks(ctx.seed, 0x4765744174000000ull); // "GetAt"
        const int n = static_cast<int>(words.size());
        for (int i = 0; i < ctx.iterations; ++i) {
            const std::string x = picks.Mangle(words[picks.Below(n)]);
            CompareGetAtLabels(ctx, f, x, picks.Mangle(words[picks.Below(n)]));
        }
    }
}

// The points to look up: every (x, y) one step around the table, the 16-bit
// and 32-bit extremes, then --iterations random points near the table.
std::vector<std::pair<int32_t, int32_t>> Points(const CaseContext& ctx, const TableData& data, uint64_t salt)
{
    std::vector<std::pair<int32_t, int32_t>> points;
    for (int y = -2; y <= data.sizeY + 1; ++y) {
        for (int x = -2; x <= data.sizeX + 1; ++x) {
            points.emplace_back(x, y);
        }
    }
    const int32_t extremes[] = { INT32_MIN, -65536, -32768, 32767, 32768, 65535, 65536,
        INT32_MAX };
    for (const int32_t e : extremes) {
        points.emplace_back(e, 0);
        points.emplace_back(0, e);
        points.emplace_back(e, e);
    }
    Picks picks(ctx.seed, salt);
    for (int i = 0; i < ctx.iterations; ++i) {
        const int x = picks.Between(-3, data.sizeX + 3);
        points.emplace_back(x, picks.Between(-3, data.sizeY + 3));
    }
    return points;
}

std::string PointText(const char* function, const std::pair<int32_t, int32_t>& p)
{
    return std::string(function) + "(CPoint(" + std::to_string(p.first) + ", " + std::to_string(p.second) + "))";
}

// C2DArray::GetAtLong(const CPoint&) at 0x403440: atol of the cell, or of
// the default outside the table.
void GetAtLong(CaseContext& ctx)
{
    OurTables owned;
    for (const Fixture& f : Fixtures(ctx, owned)) {
        for (const auto& p : Points(ctx, f.data, 0x4C6F6E6700000000ull)) { // "Long"
            ctx.SetInputs("table " + f.name + ": " + PointText("GetAtLong", p));
            const int32_t point[2] = { p.first, p.second };
            int32_t original = 0;
            ctx.CallOriginal(
                [&] { original = reinterpret_cast<GetAtLongFn>(kGetAtLong)(f.original, nullptr, point); });
            const int32_t ours = static_cast<int32_t>(f.ours->GetAtLong(CPoint(p.first, p.second)));
            ctx.CountInputs(1);
            oracle::Compare(ctx, original, ours);
        }
    }
}

// C2DArray::GetAt(const CPoint&) at 0x622320: which string comes back.
void GetAtPoint(CaseContext& ctx)
{
    OurTables owned;
    for (const Fixture& f : Fixtures(ctx, owned)) {
        for (const auto& p : Points(ctx, f.data, 0x506F696E74000000ull)) { // "Point"
            ctx.SetInputs("table " + f.name + ": " + PointText("GetAt", p));
            const int32_t point[2] = { p.first, p.second };
            std::string original;
            ctx.CallOriginal([&] {
                original = OriginalWhere(f, reinterpret_cast<GetAtPointFn>(kGetAtPoint)(f.original, nullptr, point));
            });
            const std::string ours = OursWhere(f, f.ours->GetAt(CPoint(p.first, p.second)));
            ctx.CountInputs(1);
            if (original != ours) {
                ctx.Fail(ctx.Inputs(), original, ours);
            }
        }
    }
}

void CompareFind(CaseContext& ctx, const Fixture& f, const std::string& word)
{
    ctx.SetInputs("table " + f.name + ": Find(" + Quoted(word) + ", CPoint(-7, -7))");
    int32_t point[2] = { -7, -7 };
    uint8_t found = 0;
    ctx.CallOriginal([&] {
        const void* text = OriginalString(word);
        found = reinterpret_cast<FindRowFn>(kFindRow)(f.original, nullptr, text, point);
    });
    CString text(word.c_str());
    CPoint ours(-7, -7);
    const BOOLEAN oursFound = f.ours->Find(text, ours);
    ctx.CountInputs(1);
    const std::string originalText = std::to_string(found) + ", CPoint(" + std::to_string(point[0]) + ", "
        + std::to_string(point[1]) + ")";
    const std::string oursText = std::to_string(oursFound) + ", CPoint(" + std::to_string(ours.x) + ", "
        + std::to_string(ours.y) + ")";
    if (originalText != oursText) {
        ctx.Fail(ctx.Inputs(), originalText, oursText);
    }
}

// C2DArray::Find(CString&, CPoint&) at 0x4038E0: every lookup word (labels,
// cells, their lower case, missing words), then --iterations random ones.
void FindRow(CaseContext& ctx)
{
    OurTables owned;
    for (const Fixture& f : Fixtures(ctx, owned)) {
        const std::vector<std::string> words = Words(f.data, true);
        for (const std::string& word : words) {
            CompareFind(ctx, f, word);
        }
        Picks picks(ctx.seed, 0x46696E6400000000ull); // "Find"
        for (int i = 0; i < ctx.iterations; ++i) {
            CompareFind(ctx, f, picks.Mangle(words[picks.Below(static_cast<int>(words.size()))]));
        }
    }
}

// XP tables for GetNextLevelXP: the game's, then synthetic ones whose
// FIGHTER row has signs, overflow, junk and empty cells, or is missing.
const char kSignedXpTable[] = "2DA V1.0\r\n"
                              "-1\r\n"
                              "  1  2  3  4  5  6  7  8  9  10  255  256  0  01\r\n"
                              "fighter  -5  +5  --5  2147483648  -2147483649  99999999999  12abc  abc  -0  007  "
                              "1234  4321  77  88\r\n"
                              "BARD 1 2 3\r\n";
const char kShortXpTable[] = "2DA V1.0\r\n"
                             "-2147483649\r\n"
                             "  1  2  3\r\n"
                             "FIGHTER  1000\r\n";
const char kNoFighterXpTable[] = "2DA V1.0\r\n"
                                 "+300\r\n"
                                 "  1  2  3\r\n"
                                 "FIGHTERS  1  2  3\r\n"
                                 "BARBARIAN  4  5  6\r\n";

// CRuleTables::GetNextLevelXP(BYTE) at 0x5438D0, per XP table: every level
// 0-255 (0, past the table, the byte's top), then --iterations random ones.
// The original CRuleTables is a zeroed 0x1B58 block with only
// m_tExperienceLevels (0x604) filled; ours is zeroed storage with only that
// member constructed, which is all GetNextLevelXP reads on either side.
void NextLevelXp(CaseContext& ctx)
{
    struct XpTable {
        const char* name;
        std::string text;
    };
    const XpTable tables[] = { { "XPLEVEL.2DA", ReadTable(ctx, "XPLEVEL.2DA") }, { "signed XP", kSignedXpTable },
        { "short XP", kShortXpTable }, { "no FIGHTER XP", kNoFighterXpTable } };
    for (const XpTable& table : tables) {
        // Ours. The storage is never freed: CRuleTables' destructor would
        // destroy members that were never constructed.
        auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
        std::memset(storage, 0, sizeof(CRuleTables));
        auto* rules = reinterpret_cast<CRuleTables*>(storage);
        new (&rules->m_tExperienceLevels) C2DArray();
        rules->m_tExperienceLevels.LoadText(CString(table.text.c_str()));
        const TableData data = DataOf(rules->m_tExperienceLevels);

        ctx.SetInputs(std::string("XP table ") + table.name + ": building the original's CRuleTables");
        const uint8_t* original = nullptr;
        ctx.CallOriginal([&] {
            auto* at = static_cast<uint8_t*>(OriginalNew(kRuleTablesSize));
            std::memset(at, 0, kRuleTablesSize);
            FillOriginalTable(at + kExperienceLevelsOffset, data);
            original = at;
        });

        std::vector<int> levels;
        for (int level = 0; level <= 255; ++level) {
            levels.push_back(level);
        }
        Picks picks(ctx.seed, 0x5850000000000000ull); // "XP"
        for (int i = 0; i < ctx.iterations; ++i) {
            levels.push_back(picks.Between(0, 255));
        }
        for (const int level : levels) {
            ctx.SetInputs(std::string("XP table ") + table.name + ": GetNextLevelXP(" + std::to_string(level) + ")");
            uint32_t originalXp = 0;
            ctx.CallOriginal([&] {
                originalXp = reinterpret_cast<NextLevelXpFn>(kGetNextLevelXP)(original, nullptr,
                    static_cast<uint32_t>(level));
            });
            const uint32_t oursXp = rules->GetNextLevelXP(static_cast<BYTE>(level));
            ctx.CountInputs(1);
            oracle::Compare(ctx, originalXp, oursXp);
        }
    }
}

// CRuleTables::GetXPCap at 0x544C70: a constant on both sides.
void XpCap(CaseContext& ctx)
{
    ctx.SetInputs("GetXPCap()");
    uint32_t original = 0;
    ctx.CallOriginal([&] {
        auto* at = static_cast<uint8_t*>(OriginalNew(kRuleTablesSize));
        std::memset(at, 0, kRuleTablesSize);
        original = reinterpret_cast<XpCapFn>(kGetXPCap)(at, nullptr);
    });
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    const uint32_t ours = reinterpret_cast<const CRuleTables*>(storage)->GetXPCap();
    ::operator delete(storage);
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, ours);
}

} // namespace

ORACLE_CASE("tables/getat_labels", GetAtLabels);
ORACLE_CASE("tables/getatlong", GetAtLong);
ORACLE_CASE("tables/getat_point", GetAtPoint);
ORACLE_CASE("tables/find_row", FindRow);
ORACLE_CASE("tables/next_level_xp", NextLevelXp);
ORACLE_CASE("tables/xp_cap", XpCap);
// C2DArray::Load (0x402B70) parses the text only after its resource manager
// has found and demanded the resource (CDimm::GetResObject 0x786DF0,
// CRes::Request 0x77E610, CResText::Demand 0x7808E0); calling that is out of
// the oracle's scope. Cover it in replays.
ORACLE_CASE_PENDING("tables/load", nullptr, "needs engine state (C2DArray::Load parses only resource-manager text)");
// CGameSprite::CheckAranceFailure (0x7243F0) gets the chance from
// sub_724430, which reads the equipped armour and shield through
// CItem::GetItemType (0x4E97E0, the item resource) and the sprite's feats,
// and on a failed roll calls CGameSprite::FeedBack (0x7349A0), which reads
// g_pBaldurChitin (0x8CF6DC). See docs/evidence/7243F0.md.
ORACLE_CASE_PENDING("tables/check_arcane_failure", nullptr,
    "needs engine state (equipped items, feats, FeedBack through g_pBaldurChitin)");
