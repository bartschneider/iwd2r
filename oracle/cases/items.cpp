// Item resources and item names (CResItem, CItem, the random treasure
// tiers), the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. Original objects come from the original's constructors in leaked
// original memory (CResItem 0x401560, CItem 0x4E7840); ours from ours, leaked
// too (their destructors reach g_pChitin's resource manager). The item files
// are the game's, extracted from the vanilla copy by oracle.py run (TABLES)
// and read here from --tables. Engine blocks a function only reads are zeroed
// original blocks with only those members filled in.
//
// CResItem::Parse asserts on a bad header: CUtil::UtilAssert (0x780C00) calls
// g_pChitin->SuspendThreads (0x78EC20) and then the virtual
// g_pChitin->ShutDown (vtable + 0x9C) before it exits the process. To compare
// those paths both sides' g_pChitin point at a zeroed block (no threads to
// suspend) whose vtable is a table of ShutDownHook, which records the
// message and throws AssertCaught: the throw unwinds the original's frames
// through its own handlers (destroying its temporary CStrings) as it unwinds
// ours. The original's g_pChitin is .data, restored before every case; ours
// is put back when the case ends.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
// The engine headers' anonymous unions (C4201) and packed members (C4121)
// are as the original lays them out; the runner's warning level does not
// hold them.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CChitin.h"
#include "CDimmKeyTable.h"
#include "CItem.h"
#include "CResItem.h"
#include "CRuleTables.h"
#include "msvc_rand.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CResItem::CResItem (0x401560): this in ecx, plain ret. Calls CRes::CRes
// (0x77E250, which constructs its CCriticalSection) and zeroes 0x50-0x5C.
using ResItemCtorFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kResItemCtor = 0x401560;
constexpr size_t kResItemSize = 0x60;
// CResItem::Parse(void* pData) (0x4016F0): `ret 4`, BOOL in eax. Stores
// pData at 0x58 before the checks; on "ITM V2.0" sets 0x54 (effects) and
// 0x50 (abilities) from the header's offsets at 0x6A and 0x64 and m_bParsed
// (0x5C). The assert paths read m_pDimmKeyTableEntry (0x10), whose CResRef
// is at 0.
using ParseFn = uint32_t(__fastcall*)(void* self, void* edx, const void* data);
constexpr uintptr_t kParse = 0x4016F0;
constexpr size_t kKeyTableEntryOffset = 0x10;
constexpr size_t kKeyTableEntrySize = 0x18;

// g_pChitin.
constexpr uintptr_t kChitin = 0x8CF6D8;
constexpr size_t kChitinSize = 0x2000;
constexpr size_t kVtableSlots = 64;

void* OriginalNew(size_t size)
{
    return reinterpret_cast<OriginalNewFn>(kOriginalNew)(size);
}

// A zeroed block from the original's operator new, leaked.
uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(OriginalNew(size));
    std::memset(at, 0, size);
    return at;
}

uint32_t Address(const void* p)
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

std::vector<uint8_t> ReadGameFile(const CaseContext& ctx, const std::string& name)
{
    if (ctx.tables.empty()) {
        throw std::runtime_error("no --tables folder for " + name);
    }
    const std::string path = ctx.tables + "\\" + name;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot read " + path + " (tools/oracle.py run extracts it)");
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string Quoted(const std::string& text)
{
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c < 0x20 || c >= 0x7F) {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02X", c);
            out += hex;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string Hex(uint32_t value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%X", value);
    return text;
}

// What a ShutDown call through the hooked vtable saw.
struct AssertCaught {
    int line;
    std::string file;
    std::string text;
};

// Every slot of both fake vtables: CChitin::ShutDown(int nLineNumber, const
// char* szFileName, const char* text) is __thiscall, so ecx is this and the
// three arguments are on the stack as for a __fastcall with edx unused.
void __fastcall ShutDownHook(void* self, void* edx, int line, const char* file, const char* text)
{
    (void)self;
    (void)edx;
    throw AssertCaught { line, file ? file : "(null)", text ? text : "(null)" };
}

void* g_hookVtable[kVtableSlots];

void* HookVtable()
{
    for (void*& slot : g_hookVtable) {
        slot = reinterpret_cast<void*>(&ShutDownHook);
    }
    return g_hookVtable;
}

// Puts our g_pChitin back when the case ends.
struct ChitinRestore {
    CChitin* saved = g_pChitin;
    ~ChitinRestore() { g_pChitin = saved; }
};

// Points both sides' g_pChitin at a zeroed block whose vtable is the hook's.
// Ours is never freed.
void HookAsserts(CaseContext& ctx)
{
    void* vtable = HookVtable();
    ctx.SetInputs("hooking g_pChitin's ShutDown");
    ctx.CallOriginal([&] {
        uint8_t* chitin = OriginalBlock(kChitinSize);
        const uint32_t vptr = Address(vtable);
        std::memcpy(chitin, &vptr, 4);
        const uint32_t pointer = Address(chitin);
        std::memcpy(reinterpret_cast<void*>(kChitin), &pointer, 4);
    });
    auto* ours = static_cast<unsigned char*>(::operator new(sizeof(CChitin)));
    std::memset(ours, 0, sizeof(CChitin));
    std::memcpy(ours, &vtable, sizeof(vtable));
    g_pChitin = reinterpret_cast<CChitin*>(ours);
}

// ---------------------------------------------------------------------------
// CResItem::Parse (0x4016F0)

// The state Parse leaves, as text: the return, m_bParsed, the three pointers
// relative to `base`, the buffer of this call or, for Parse(NULL), of the
// call before (both sides parse the same buffers),
// and the assert's message if one was raised. The assert's line and file are
// not compared: the original passes its own source's (1356 or 1369,
// BalDataTypes.cpp), ours its own __LINE__ and __FILE__.
std::string ParseState(bool asserted, uint32_t result, uint32_t parsed, uint32_t header, uint32_t effects,
    uint32_t abilities, const uint8_t* base, const std::string& message)
{
    auto pointer = [&](uint32_t p) {
        if (p == 0) {
            return std::string("null");
        }
        return "data+" + Hex(p - Address(base));
    };
    std::string text = asserted ? "asserted " + Quoted(message) : "returned " + std::to_string(result);
    return text + ", m_bParsed " + std::to_string(parsed) + ", m_pHeader " + pointer(header) + ", m_pEffects "
        + pointer(effects) + ", m_pAbilities " + pointer(abilities);
}

struct ParsePair {
    uint8_t* original;
    CResItem* ours;
};

// Both sides' CResItem, from their constructors, with a key table entry
// naming `resRef` (eight bytes, NUL-padded) for the assert messages.
ParsePair MakeResItems(CaseContext& ctx, const std::string& resRef)
{
    uint8_t name[8] = {};
    std::memcpy(name, resRef.data(), resRef.size() < 8 ? resRef.size() : 8);
    ParsePair pair {};
    ctx.CallOriginal([&] {
        pair.original = OriginalBlock(kResItemSize);
        reinterpret_cast<ResItemCtorFn>(kResItemCtor)(pair.original, nullptr);
        uint8_t* entry = OriginalBlock(kKeyTableEntrySize);
        std::memcpy(entry, name, 8);
        const uint32_t pointer = Address(entry);
        std::memcpy(pair.original + kKeyTableEntryOffset, &pointer, 4);
    });
    pair.ours = new CResItem();
    auto* entry = static_cast<unsigned char*>(::operator new(sizeof(CDimmKeyTableEntry)));
    std::memset(entry, 0, sizeof(CDimmKeyTableEntry));
    std::memcpy(entry, name, 8); // CResRef's eight bytes are its first member
    pair.ours->m_pDimmKeyTableEntry = reinterpret_cast<CDimmKeyTableEntry*>(entry);
    return pair;
}

// One Parse on each side; data may be null, base is what pointers are shown
// relative to.
void CompareParse(CaseContext& ctx, const ParsePair& pair, const uint8_t* data, const uint8_t* base,
    const std::string& what)
{
    ctx.SetInputs(what);
    bool originalAsserted = false;
    uint32_t originalResult = 0;
    std::string originalMessage;
    ctx.CallOriginal([&] {
        try {
            originalResult = reinterpret_cast<ParseFn>(kParse)(pair.original, nullptr, data);
        } catch (const AssertCaught& caught) {
            originalAsserted = true;
            originalMessage = caught.text;
        }
    });
    uint32_t words[4];
    std::memcpy(words, pair.original + 0x50, sizeof(words));
    const std::string original = ParseState(originalAsserted, originalResult, words[3], words[2], words[1],
        words[0], base, originalMessage);

    bool oursAsserted = false;
    uint32_t oursResult = 0;
    std::string oursMessage;
    try {
        oursResult = static_cast<uint32_t>(pair.ours->Parse(const_cast<uint8_t*>(data)));
    } catch (const AssertCaught& caught) {
        oursAsserted = true;
        oursMessage = caught.text;
    }
    const std::string ours = ParseState(oursAsserted, oursResult, static_cast<uint32_t>(pair.ours->m_bParsed),
        Address(pair.ours->m_pHeader), Address(pair.ours->m_pEffects), Address(pair.ours->m_pAbilities), base,
        oursMessage);
    ctx.CountInputs(1);
    if (original != ours) {
        ctx.Fail(ctx.Inputs(), original, ours);
    }
}

// A buffer in leaked original memory: Parse keeps a pointer to it.
uint8_t* Leaked(CaseContext& ctx, const std::vector<uint8_t>& bytes)
{
    uint8_t* at = nullptr;
    ctx.CallOriginal([&] {
        at = OriginalBlock(bytes.size() ? bytes.size() : 1);
        std::memcpy(at, bytes.data(), bytes.size());
    });
    return at;
}

// The header Parse reads ends with the effects offset at 0x6A-0x6D; every
// buffer here is at least the 0x72-byte V2.0 header.
constexpr size_t kHeaderSize = 0x72;

// The game's items: a weapon, an armour, a scroll, a potion, an amulet, a
// gem and the item CItem's constructor special-cases.
const char* const kItems[] = { "00SWDL01", "00CHAN01", "00SCRL01", "00POTN01", "00AMUL01", "00GBGA01", "MISC07" };

// CResItem::Parse at 0x4016F0. Per item: on a fresh object Parse(NULL),
// Parse(item), Parse(NULL). Then, from the long sword's bytes: the item cut
// at every length from 0 to the header's end with the rest filled with 0xCD
// (Parse takes no size, so a short resource is read past its end; here the
// pattern stands for what follows), wrong signatures and versions (the
// byte-reversed constants an earlier port compared, lower case, V1.0, V1.1,
// V2.1, a CHR and a SPL header) and extreme offsets; each bad buffer on a
// fresh object (Parse(bad), Parse(NULL)) and after a good parse (Parse(item),
// Parse(bad), Parse(NULL): m_bParsed stays set while m_pHeader moves). The
// assert messages name the key table entry's resref, NUL-padded (MISC07)
// or full.
void ResItemParse(CaseContext& ctx)
{
    ChitinRestore restore;
    HookAsserts(ctx);

    std::vector<uint8_t> sword;
    for (const char* name : kItems) {
        std::vector<uint8_t> bytes = ReadGameFile(ctx, std::string(name) + ".ITM");
        if (bytes.size() < kHeaderSize) {
            throw std::runtime_error(std::string(name) + ".ITM is shorter than the item header");
        }
        if (sword.empty()) {
            sword = bytes;
        }
        const uint8_t* data = Leaked(ctx, bytes);
        ParsePair pair = MakeResItems(ctx, name);
        CompareParse(ctx, pair, nullptr, data, std::string(name) + ": fresh Parse(NULL)");
        CompareParse(ctx, pair, data, data, std::string(name) + ": Parse(" + name + ".ITM)");
        CompareParse(ctx, pair, data, data, std::string(name) + ": Parse(" + name + ".ITM) again");
        CompareParse(ctx, pair, nullptr, data, std::string(name) + ": Parse(NULL) after");
    }

    struct Bad {
        std::string what;
        std::vector<uint8_t> bytes;
    };
    std::vector<Bad> bad;
    for (size_t cut = 0; cut <= kHeaderSize; ++cut) {
        std::vector<uint8_t> bytes(sword.begin(), sword.begin() + static_cast<std::ptrdiff_t>(cut));
        bytes.resize(kHeaderSize, 0xCD);
        bad.push_back({ "00SWDL01.ITM cut at " + std::to_string(cut) + " bytes, 0xCD after", bytes });
    }
    const char* const signatures[] = { " MTI0.2V", "itm v2.0", "ITM v2.0", "itm V2.0", "ITM V1.0", "ITM V1.1",
        "ITM V2.1", "ITM V2.0", "ITMV2.0 ", "CHR V2.2", "SPL V2.0", "\0\0\0\0\0\0\0\0" };
    for (const char* signature : signatures) {
        std::vector<uint8_t> bytes = sword;
        std::memcpy(bytes.data(), signature, 8);
        bad.push_back({ "00SWDL01.ITM with the signature " + Quoted(std::string(signature, 8)), bytes });
    }
    const uint32_t offsets[] = { 0, 1, 0x72, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    for (const uint32_t abilities : offsets) {
        for (const uint32_t effects : offsets) {
            std::vector<uint8_t> bytes = sword;
            std::memcpy(bytes.data() + 0x64, &abilities, 4);
            std::memcpy(bytes.data() + 0x6A, &effects, 4);
            bad.push_back({ "00SWDL01.ITM with ability offset " + Hex(abilities) + ", effects offset " + Hex(effects),
                bytes });
        }
    }

    const uint8_t* good = Leaked(ctx, sword);
    for (const Bad& b : bad) {
        const uint8_t* data = Leaked(ctx, b.bytes);
        for (const char* resRef : { "00SWDL01", "MISC07" }) {
            const std::string key = std::string(" (key ") + resRef + ")";
            ParsePair fresh = MakeResItems(ctx, resRef);
            CompareParse(ctx, fresh, data, data, "fresh: Parse(" + b.what + ")" + key);
            CompareParse(ctx, fresh, nullptr, data, "fresh: Parse(" + b.what + "), Parse(NULL)" + key);
            ParsePair parsed = MakeResItems(ctx, resRef);
            CompareParse(ctx, parsed, good, good, "Parse(00SWDL01.ITM)" + key);
            CompareParse(ctx, parsed, data, data, "Parse(00SWDL01.ITM), Parse(" + b.what + ")" + key);
            CompareParse(ctx, parsed, nullptr, data, "Parse(00SWDL01.ITM), Parse(" + b.what + "), Parse(NULL)" + key);
        }
    }
}

// ---------------------------------------------------------------------------
// CRuleTables::sub_5461E0 (0x5461E0)

// sub_5461E0(const CString& sResRef, const CString& sUpgrade): the CString
// comes back through the hidden pointer pushed first, `ret 0xc` at 0x54637F;
// ecx is never read. The tier letters are the CString at 0x8DE8C8, built
// from "lmrvvvvv" by the static initializer at 0x5461A0, which has run when
// the runner takes its snapshot.
using TierUpgradeFn = void*(__fastcall*)(const void* self, void* edx, void* result, const void* resRef,
    const void* upgrade);
constexpr uintptr_t kTierUpgrade = 0x5461E0;
// CString::CString(LPCSTR) (0x7FCC88): this in ecx, the text at [esp+4],
// ret 4.
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

// An original CString holding text, in leaked original memory.
const void* OriginalString(const std::string& text)
{
    void* at = OriginalNew(4);
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(at, nullptr, text.c_str());
    return at;
}

const char* OriginalText(const void* cstring)
{
    return *static_cast<const char* const*>(cstring);
}

// The whitespace-separated words of a 2DA text past its two header lines:
// the column labels, then each row's label and cells. Plain input words; the
// function under test reads no table.
std::vector<std::string> TableWords(const std::string& text, std::vector<std::string>* rowLabels)
{
    std::vector<std::string> words;
    size_t line = 0;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) {
            end = text.size();
        }
        if (line >= 2) {
            bool first = true;
            size_t i = at;
            while (i < end) {
                while (i < end && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r')) {
                    ++i;
                }
                size_t j = i;
                while (j < end && text[j] != ' ' && text[j] != '\t' && text[j] != '\r') {
                    ++j;
                }
                if (j > i) {
                    words.push_back(text.substr(i, j - i));
                    if (first && line >= 3 && rowLabels) {
                        rowLabels->push_back(words.back());
                    }
                    first = false;
                }
                i = j;
            }
        }
        ++line;
        at = end + 1;
    }
    return words;
}

void AddUnique(std::vector<std::string>& list, const std::string& word)
{
    for (const std::string& known : list) {
        if (known == word) {
            return;
        }
    }
    list.push_back(word);
}

void CompareTierUpgrade(CaseContext& ctx, const void* originalThis, const CRuleTables* ours,
    const std::string& resRef, const std::string& upgrade)
{
    ctx.SetInputs("sub_5461E0(" + Quoted(resRef) + ", " + Quoted(upgrade) + ")");
    std::string original;
    ctx.CallOriginal([&] {
        void* result = OriginalBlock(4);
        reinterpret_cast<TierUpgradeFn>(kTierUpgrade)(originalThis, nullptr, result, OriginalString(resRef),
            OriginalString(upgrade));
        original = OriginalText(result);
    });
    const CString result = ours->sub_5461E0(CString(resRef.c_str()), CString(upgrade.c_str()));
    ctx.CountInputs(1);
    const std::string oursText = static_cast<const char*>(result);
    if (original != oursText) {
        ctx.Fail(ctx.Inputs(), Quoted(original), Quoted(oursText));
    }
}

// CRuleTables::sub_5461E0 at 0x5461E0, the random treasure tier upgrade
// sub_5463E0 applies to an "UP" cell of RT_NORM or RT_FURY. The resrefs: the
// row labels of RT_NORM.2DA, RT_FURY.2DA and RNDTRES.2DA (each tier, l m r
// v), stems ending in "_" and every tier letter in either case, a letter not
// in any tier, a digit or another "_", and names too short or without the
// "_" second from the end. The upgrades: every cell of those tables that
// starts with "up" in any case (the game's are all "UP"), "up1" to "up3",
// "upp" and the other tails the switch refuses, "u", "", and words that are
// not upgrades. Every resref with every upgrade, then --iterations random
// pairs over the letters that matter. Ours runs on zeroed CRuleTables
// storage (it reads no member); the original gets a zeroed block.
void TierUpgrade(CaseContext& ctx)
{
    std::vector<std::string> resRefs;
    std::vector<std::string> upgrades = { "", "u", "U", "p", "up", "UP", "Up", "uP", "up1", "UP1", "up2", "Up2",
        "up3", "uP3", "up0", "up4", "up9", "upp", "UPP", "upP", "uppp", "upl", "upx", "up_", "up12", "up13",
        "up21", "upgrade", "uq1", "pu1", "down", "1", "u p1", "UP\xE9" };
    for (const char* table : { "RT_NORM.2DA", "RT_FURY.2DA", "RNDTRES.2DA" }) {
        const std::vector<uint8_t> bytes = ReadGameFile(ctx, table);
        std::vector<std::string> labels;
        for (const std::string& word : TableWords(std::string(bytes.begin(), bytes.end()), &labels)) {
            if (word.size() >= 2 && (word[0] == 'u' || word[0] == 'U') && (word[1] == 'p' || word[1] == 'P')) {
                AddUnique(upgrades, word);
            }
        }
        for (const std::string& label : labels) {
            AddUnique(resRefs, label);
        }
    }
    for (const char* stem : { "RT01_", "rt99_", "a_", "_", "__", "X_Y_" }) {
        for (const char tier : std::string("lmrvLMRVnaz0_")) {
            AddUnique(resRefs, std::string(stem) + tier);
        }
    }
    for (const char* name : { "", "l", "_l", "L_", "ab", "RT01L", "RT0_1L", "RT01_", "RT01_LL", "RT01__", "RT01_l ",
             "00SWDL01", "no_drop", "RT01_\xE9" }) {
        AddUnique(resRefs, name);
    }

    const uint8_t* originalThis = nullptr;
    ctx.CallOriginal([&] { originalThis = OriginalBlock(0x10); });
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    const CRuleTables* ours = reinterpret_cast<const CRuleTables*>(storage);

    for (const std::string& resRef : resRefs) {
        for (const std::string& upgrade : upgrades) {
            CompareTierUpgrade(ctx, originalThis, ours, resRef, upgrade);
        }
    }
    uint64_t state = ctx.seed ^ 0x5469657255700000ull; // "TierUp"
    const std::string letters = "lmrvLMRVuUpP0123_ax";
    auto word = [&](size_t maxLength) {
        std::string text(static_cast<size_t>(oracle::SplitMix64(&state) % (maxLength + 1)), ' ');
        for (char& c : text) {
            c = letters[static_cast<size_t>(oracle::SplitMix64(&state) % letters.size())];
        }
        return text;
    };
    for (int i = 0; i < ctx.iterations; ++i) {
        std::string resRef = word(8);
        if (resRef.size() >= 2 && oracle::SplitMix64(&state) % 2) {
            resRef[resRef.size() - 2] = '_';
        }
        std::string upgrade = word(4);
        if (oracle::SplitMix64(&state) % 2) {
            upgrade = (oracle::SplitMix64(&state) % 2 ? "up" : "Up") + upgrade;
        }
        CompareTierUpgrade(ctx, originalThis, ours, resRef, upgrade);
    }
    ::operator delete(storage);
}

// ---------------------------------------------------------------------------
// CItem::sub_4EA4A0 (0x4EA4A0)

// CItem::CItem() (0x4E7840): this in ecx, plain ret; sets m_useCount1-3
// (0x18, 0x1A, 0x1C) to 1. CItem::sub_4EA4A0(CString& sResRef): `ret 4` at
// 0x4EA4FC; reads the name (CString::GetBuffer 0x7FCFFE, _strnicmp
// 0x7EA8C0) and m_useCount1 and m_useCount2, writes those two and may call
// rand (0x7E8160). The resource helper (pRes at 0x4) is not read.
using ItemCtorFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kItemCtor = 0x4E7840;
using UseCountRollFn = void(__fastcall*)(void* self, void* edx, const void* resRef);
constexpr uintptr_t kUseCountRoll = 0x4EA4A0;
// CItem is 0xEE bytes (m_numSounds at 0xEC).
constexpr size_t kItemSize = 0x100;
constexpr size_t kItemResOffset = 0x4;
constexpr size_t kUseCountsOffset = 0x18;

using Srand = void(__cdecl*)(unsigned int);
using Rand = int(__cdecl*)();

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

int OriginalRand()
{
    return reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))();
}

std::string UseCounts(const uint16_t (&counts)[3])
{
    return std::to_string(counts[0]) + "/" + std::to_string(counts[1]) + "/" + std::to_string(counts[2]);
}

struct ItemPair {
    uint8_t* original;
    CItem* ours;
};

// Both sides' CItem from their default constructors, each holding a parsed
// CResItem of `itemFile` the way CResHelper holds its resource (pRes), which
// the function does not read.
ItemPair MakeItems(CaseContext& ctx, const uint8_t* itemFile, const std::string& name)
{
    ItemPair pair {};
    ParsePair res = MakeResItems(ctx, name);
    ctx.CallOriginal([&] {
        reinterpret_cast<ParseFn>(kParse)(res.original, nullptr, itemFile);
        pair.original = OriginalBlock(kItemSize);
        reinterpret_cast<ItemCtorFn>(kItemCtor)(pair.original, nullptr);
        const uint32_t pointer = Address(res.original);
        std::memcpy(pair.original + kItemResOffset, &pointer, 4);
    });
    res.ours->Parse(const_cast<uint8_t*>(itemFile));
    pair.ours = new CItem();
    pair.ours->pRes = res.ours;
    return pair;
}

void CompareUseCountRoll(CaseContext& ctx, const ItemPair& items, const std::string& resRef, uint16_t useCount1,
    uint16_t useCount2, unsigned int seed)
{
    const uint16_t useCount3 = 5;
    ctx.SetInputs("srand(" + std::to_string(seed) + "), use counts " + std::to_string(useCount1) + "/"
        + std::to_string(useCount2) + "/" + std::to_string(useCount3) + ", sub_4EA4A0(" + Quoted(resRef) + ")");

    uint16_t original[3] = {};
    std::string originalName;
    int originalNext = 0;
    ctx.CallOriginal([&] {
        const uint16_t counts[3] = { useCount1, useCount2, useCount3 };
        std::memcpy(items.original + kUseCountsOffset, counts, sizeof(counts));
        const void* name = OriginalString(resRef);
        OriginalSrand(seed);
        reinterpret_cast<UseCountRollFn>(kUseCountRoll)(items.original, nullptr, name);
        originalNext = OriginalRand();
        std::memcpy(original, items.original + kUseCountsOffset, sizeof(original));
        originalName = OriginalText(name);
    });

    items.ours->m_useCount1 = useCount1;
    items.ours->m_useCount2 = useCount2;
    items.ours->m_useCount3 = useCount3;
    CString name(resRef.c_str());
    iwd2r_srand(seed);
    items.ours->sub_4EA4A0(name);
    const int oursNext = iwd2r_rand();
    const uint16_t ours[3] = { items.ours->m_useCount1, items.ours->m_useCount2, items.ours->m_useCount3 };

    ctx.CountInputs(1);
    const std::string originalText = "use counts " + UseCounts(original) + ", name " + Quoted(originalName)
        + ", next rand " + std::to_string(originalNext);
    const std::string oursText = "use counts " + UseCounts(ours) + ", name " + Quoted(static_cast<const char*>(name))
        + ", next rand " + std::to_string(oursNext);
    if (originalText != oursText) {
        ctx.Fail(ctx.Inputs(), originalText, oursText);
    }
}

// CItem::sub_4EA4A0 at 0x4EA4A0, which CItem's constructors call on the
// item's name to roll the use count of the random gem bags. Per item (the
// long sword, the scroll, MISC07, each a CItem holding its parsed CResItem):
// the names 00RTAVE1 in three cases, other 00RTAVE and 00RIITM names (the
// seven-letter 00RIITM branch reads the terminator, so it never rolls), the
// item's own name and names of other lengths, by m_useCount1 and
// m_useCount2 0, 1, 2, 7, 0x7FFF, 0x8000 and 0xFFFF (m_useCount3 5), each
// from its own srand seed; then --iterations random counts and names. The
// three use counts, the name and the rand value drawn after are compared.
void UseCountRoll(CaseContext& ctx)
{
    const char* const names[] = { "00RTAVE1", "00rtave1", "00RtAvE1", "00RTAVE2", "00RTAVE3", "00RTAVE",
        "00RTAVE10", "00RTAVE1 ", "0RTAVE1", "00RIITM", "00riitm", "00RIITM1", "00RIITM5", "00RIITM9", "00RIITM0",
        "00RIITMX", "00RIIT", "", "MISC07", "00SWDL01", "00SCRL01", "00GBGA01" };
    const uint16_t counts[] = { 0, 1, 2, 7, 0x7FFF, 0x8000, 0xFFFF };
    uint64_t state = ctx.seed ^ 0x5573654300000000ull; // "UseC"
    auto bits = [&] { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };

    for (const char* item : { "00SWDL01", "00SCRL01", "MISC07" }) {
        const uint8_t* file = Leaked(ctx, ReadGameFile(ctx, std::string(item) + ".ITM"));
        const ItemPair items = MakeItems(ctx, file, item);
        for (const char* name : names) {
            for (const uint16_t useCount1 : counts) {
                for (const uint16_t useCount2 : counts) {
                    CompareUseCountRoll(ctx, items, name, useCount1, useCount2, bits());
                }
            }
        }
        for (int i = 0; i < ctx.iterations; ++i) {
            const std::string name = names[bits() % (sizeof(names) / sizeof(names[0]))];
            const uint16_t useCount1 = static_cast<uint16_t>(bits());
            const uint16_t useCount2 = static_cast<uint16_t>(bits() % 4 == 0 ? 0 : bits());
            CompareUseCountRoll(ctx, items, name, useCount1, useCount2, bits());
        }
    }
}

} // namespace

ORACLE_CASE("items/res_item_parse", ResItemParse);
ORACLE_CASE("items/tier_upgrade", TierUpgrade);
ORACLE_CASE("items/use_count_roll", UseCountRoll);
