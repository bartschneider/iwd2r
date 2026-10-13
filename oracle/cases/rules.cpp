// Hit point rules (CRuleTables), the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. CRuleTables::GetStartingHitPoints (0x542FA0) reads the class
// levels of two CDerivedStats (CDerivedStats::GetClassLevel, 0x447990: the
// byte at 0x46 + class), the five hit point tables of CRuleTables (0x310
// HPWAR, 0x334 HPWIZ, 0x358 HPPRS, 0x37C HPROG, 0x3C4 HPMONK, through
// sub_542CB0 and sub_542B40) and, once per level, the game option "maximum
// hit points": g_pBaldurChitin (0x8CF6DC) -> m_pObjectGame (0x1C54) ->
// m_cOptions.m_bMaxHP (0x448A). The original's objects are zeroed blocks from
// its operator new with only those members filled in: the tables as
// tables.cpp builds them (our parse of the extracted 2DA text, laid out as
// the original's C2DArray), the class levels, and a CBaldurChitin and a
// CInfGame holding only the pointer and the option. The runner restores
// .data, and so g_pBaldurChitin, before every case. Ours gets the same:
// zeroed storage with only the tables constructed and our g_pBaldurChitin
// pointing at zeroed storage whose m_pObjectGame holds the option; it is put
// back to null when the case ends. Original objects are never freed.
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
#include "C2DArray.h"
#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CDerivedStats.h"
#include "CInfGame.h"
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
#include <initializer_list>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Srand = void(__cdecl*)(unsigned int);
using Rand = int(__cdecl*)();
// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CString::CString(LPCSTR) (0x7FCC88): this in ecx, the text at [esp+4],
// ret 4.
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

// CRuleTables::GetStartingHitPoints(typeAI, oldDStats, newDStats, nCON)
// (0x542FA0): four stack arguments, `ret 0x10` at 0x542FDD; it reads only
// the second and third (CompleteCharacterAbilities' call at 0x6129EB pushes
// the CON byte as a dword). INT in eax.
using StartingHitPointsFn = int32_t(__fastcall*)(const void* self, void* edx, const void* typeAI,
    void* oldDStats, void* newDStats, uint32_t nCON);
constexpr uintptr_t kGetStartingHitPoints = 0x542FA0;
// CRuleTables::RollHitPoints (0x542A90): (nSides, nRolls, nLevel, nMinRoll,
// nModifier), `ret 0x14` at 0x542B30; ecx is never read or written. INT in
// eax. nSides 0 is asserted (CUtil::UtilAssert, 0x780C00) and divides by
// zero, so it is not an input.
using RollHitPointsFn = int32_t(__fastcall*)(const void* self, void* edx, int32_t nSides, int32_t nRolls,
    int32_t nLevel, int32_t nMinRoll, int32_t nModifier);
constexpr uintptr_t kRollHitPoints = 0x542A90;

// CRuleTables::sub_545EB0(BYTE nLevel, BYTE nIndex) (0x545EB0): returns a
// CResRef through the hidden pointer pushed first, `ret 0xc` at 0x545FCB; it
// writes the eight bytes itself (0x545FA4) and returns the pointer in eax.
// Reads only this+0x1858, the C2DArray m_tSpellAutomaticPicker.
using SpellPickFn = void*(__fastcall*)(const void* self, void* edx, void* result, uint32_t nLevel, uint32_t nIndex);
constexpr uintptr_t kSpellPick = 0x545EB0;
constexpr size_t kSpellPickerOffset = 0x1858;

// g_pBaldurChitin, CBaldurChitin::m_pObjectGame and CInfGame's
// m_cOptions.m_bMaxHP (sub_542B40 at 0x542C0E).
constexpr uintptr_t kBaldurChitin = 0x8CF6DC;
constexpr size_t kObjectGameOffset = 0x1C54;
constexpr size_t kMaxHpOffset = 0x448A;
// CInfGame::m_ruleTables runs from 0 to 0x1B58 (CInfGame.h).
constexpr size_t kRuleTablesSize = 0x1B58;
// The original C2DArray: m_pNamesX at 0x10, m_pNamesY 0x14, m_pArray 0x18,
// m_default 0x1C, m_nSizeX 0x20, m_nSizeY 0x22 (tables.cpp).
constexpr size_t kC2DArraySize = 0x24;
// CDerivedStats: the eleven class levels from 0x47 (GetClassLevel reads
// [this + class + 0x46]).
constexpr size_t kDerivedStatsSize = 0x100;
constexpr size_t kClassLevelsOffset = 0x47;

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

int OriginalRand()
{
    return reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))();
}

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

void OriginalCString(void* at, const std::string& text)
{
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(at, nullptr, text.c_str());
}

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

// Our C2DArray's parse as plain data, read directly.
struct Members : C2DArray {
    static const CString* NamesX(const C2DArray& t) { return t.*(&Members::m_pNamesX); }
    static const CString* NamesY(const C2DArray& t) { return t.*(&Members::m_pNamesY); }
    static const CString* Cells(const C2DArray& t) { return t.*(&Members::m_pArray); }
    static const CString& Default(const C2DArray& t) { return t.*(&Members::m_default); }
    static int SizeX(const C2DArray& t) { return t.*(&Members::m_nSizeX); }
    static int SizeY(const C2DArray& t) { return t.*(&Members::m_nSizeY); }
};

// new CString[n] as the original lays it out: the count, then n strings.
uint32_t OriginalStringArray(const CString* texts, int count)
{
    auto* block = static_cast<uint32_t*>(OriginalNew(4 + 4 * static_cast<size_t>(count)));
    block[0] = static_cast<uint32_t>(count);
    for (int i = 0; i < count; ++i) {
        OriginalCString(&block[1 + i], static_cast<const char*>(texts[i]));
    }
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block + 1));
}

// Fills the original C2DArray at `at` (zeroed) with what our parse made.
void FillOriginalTable(uint8_t* at, const C2DArray& ours)
{
    const int sizeX = Members::SizeX(ours);
    const int sizeY = Members::SizeY(ours);
    const uint32_t namesX = OriginalStringArray(Members::NamesX(ours), sizeX);
    const uint32_t namesY = OriginalStringArray(Members::NamesY(ours), sizeY);
    const uint32_t cells = OriginalStringArray(Members::Cells(ours), sizeX * sizeY);
    std::memcpy(at + 0x10, &namesX, 4);
    std::memcpy(at + 0x14, &namesY, 4);
    std::memcpy(at + 0x18, &cells, 4);
    OriginalCString(at + 0x1C, static_cast<const char*>(Members::Default(ours)));
    const int16_t x = static_cast<int16_t>(sizeX);
    const int16_t y = static_cast<int16_t>(sizeY);
    std::memcpy(at + 0x20, &x, 2);
    std::memcpy(at + 0x22, &y, 2);
}

// The hit point tables sub_542CB0 picks, by member and offset.
struct HitPointTable {
    const char* name;
    C2DArray CRuleTables::*member;
    size_t offset;
};
const HitPointTable kHitPointTables[] = {
    { "HPWAR.2DA", &CRuleTables::m_tHitPointsWarrior, 0x310 },
    { "HPWIZ.2DA", &CRuleTables::m_tHitPointsWizard, 0x334 },
    { "HPPRS.2DA", &CRuleTables::m_tHitPointsPriest, 0x358 },
    { "HPROG.2DA", &CRuleTables::m_tHitPointsRogue, 0x37C },
    { "HPMONK.2DA", &CRuleTables::m_tHitPointsMonk, 0x3C4 },
};

struct Fixture {
    CRuleTables* ours = nullptr;
    const uint8_t* original = nullptr;
    uint8_t* originalGame = nullptr; // the original CInfGame g_pBaldurChitin reaches
    CInfGame* oursGame = nullptr;
};

// Both sides' rule tables with the hit point tables loaded, and both
// g_pBaldurChitin pointing at a game whose options say bMaxHP. Ours is never
// freed: CRuleTables' destructor would destroy members never constructed.
Fixture MakeFixture(CaseContext& ctx)
{
    Fixture f;
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    f.ours = reinterpret_cast<CRuleTables*>(storage);
    for (const HitPointTable& table : kHitPointTables) {
        C2DArray* member = new (&(f.ours->*table.member)) C2DArray();
        member->LoadText(CString(ReadTable(ctx, table.name).c_str()));
    }

    auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CBaldurChitin)));
    std::memset(chitin, 0, sizeof(CBaldurChitin));
    auto* game = static_cast<unsigned char*>(::operator new(sizeof(CInfGame)));
    std::memset(game, 0, sizeof(CInfGame));
    f.oursGame = reinterpret_cast<CInfGame*>(game);
    reinterpret_cast<CBaldurChitin*>(chitin)->m_pObjectGame = f.oursGame;
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(chitin);

    ctx.SetInputs("building the original's CRuleTables and g_pBaldurChitin");
    ctx.CallOriginal([&] {
        uint8_t* rules = OriginalBlock(kRuleTablesSize);
        for (const HitPointTable& table : kHitPointTables) {
            uint8_t* at = rules + table.offset;
            std::memset(at, 0, kC2DArraySize);
            FillOriginalTable(at, f.ours->*table.member);
        }
        f.original = rules;
        uint8_t* originalChitin = OriginalBlock(kObjectGameOffset + 4);
        f.originalGame = OriginalBlock(kMaxHpOffset + 4);
        const uint32_t gamePointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(f.originalGame));
        std::memcpy(originalChitin + kObjectGameOffset, &gamePointer, 4);
        const uint32_t chitinPointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(originalChitin));
        std::memcpy(reinterpret_cast<void*>(kBaldurChitin), &chitinPointer, 4);
    });
    return f;
}

void SetMaxHp(Fixture& f, int32_t bMaxHp)
{
    std::memcpy(f.originalGame + kMaxHpOffset, &bMaxHp, 4);
    f.oursGame->m_cOptions.m_bMaxHP = bMaxHp;
}

// Puts our g_pBaldurChitin back when the case ends, however it ends.
struct OurChitinReset {
    ~OurChitinReset() { g_pBaldurChitin = nullptr; }
};

// Picks inputs and seeds from the run's --seed, apart from other streams by
// a salt.
struct Picks {
    Picks(uint64_t seed, uint64_t salt)
        : state(seed ^ salt)
        , bits(oracle::SplitMix64(&state))
    {
    }
    uint32_t Bits() { return *bits.Next(); }
    int32_t Between(int32_t lo, int32_t hi)
    {
        const uint32_t span = static_cast<uint32_t>(hi) - static_cast<uint32_t>(lo) + 1u;
        return static_cast<int32_t>(static_cast<uint32_t>(lo) + (span ? Bits() % span : Bits()));
    }

    uint64_t state;
    oracle::RandomGen<uint32_t> bits;
};

std::string Levels(const uint8_t (&levels)[CAIOBJECT_CLASS_MAX])
{
    std::string text = "[";
    for (int i = 0; i < CAIOBJECT_CLASS_MAX; ++i) {
        text += (i ? " " : "") + std::to_string(levels[i]);
    }
    return text + "]";
}

// One call on each side from srand(seed): the result and the rand value
// drawn after it.
void CompareStartingHitPoints(CaseContext& ctx, Fixture& f, const uint8_t (&oldLevels)[CAIOBJECT_CLASS_MAX],
    const uint8_t (&newLevels)[CAIOBJECT_CLASS_MAX], int32_t bMaxHp, unsigned int seed)
{
    ctx.SetInputs("srand(" + std::to_string(seed) + "), bMaxHP " + std::to_string(bMaxHp)
        + ", GetStartingHitPoints(old levels " + Levels(oldLevels) + ", new levels " + Levels(newLevels) + ")");
    SetMaxHp(f, bMaxHp);

    int32_t original = 0;
    int originalNext = 0;
    ctx.CallOriginal([&] {
        uint8_t* oldStats = OriginalBlock(kDerivedStatsSize);
        uint8_t* newStats = OriginalBlock(kDerivedStatsSize);
        std::memcpy(oldStats + kClassLevelsOffset, oldLevels, CAIOBJECT_CLASS_MAX);
        std::memcpy(newStats + kClassLevelsOffset, newLevels, CAIOBJECT_CLASS_MAX);
        const uint8_t* typeAI = OriginalBlock(0x40);
        OriginalSrand(seed);
        original = reinterpret_cast<StartingHitPointsFn>(kGetStartingHitPoints)(f.original, nullptr, typeAI,
            oldStats, newStats, 10);
        originalNext = OriginalRand();
    });

    CDerivedStats oldDStats;
    CDerivedStats newDStats;
    for (int i = 0; i < CAIOBJECT_CLASS_MAX; ++i) {
        oldDStats.m_nClassLevels[i] = oldLevels[i];
        newDStats.m_nClassLevels[i] = newLevels[i];
    }
    CAIObjectType typeAI;
    iwd2r_srand(seed);
    const int32_t ours = f.ours->GetStartingHitPoints(typeAI, oldDStats, newDStats, 10);
    const int oursNext = iwd2r_rand();

    ctx.CountInputs(1);
    oracle::Compare(ctx, original, ours);
    const std::string inputs = ctx.Inputs();
    ctx.SetInputs(inputs + ", the following rand");
    oracle::Compare(ctx, originalNext, oursNext);
    ctx.SetInputs(inputs);
}

// CRuleTables::GetStartingHitPoints (0x542FA0) with the game's hit point
// tables: every class 1-11 alone at every level 1-30 from level 0, with
// maximum hit points off and on; then --iterations random multiclass level
// sets, each from random lower old levels.
void StartingHitPoints(CaseContext& ctx)
{
    OurChitinReset reset;
    Fixture f = MakeFixture(ctx);
    Picks picks(ctx.seed, 0x5374617274485000ull); // "StartHP"
    for (int nClass = 1; nClass <= CAIOBJECT_CLASS_MAX; ++nClass) {
        for (int level = 1; level <= 30; ++level) {
            uint8_t oldLevels[CAIOBJECT_CLASS_MAX] = {};
            uint8_t newLevels[CAIOBJECT_CLASS_MAX] = {};
            newLevels[nClass - 1] = static_cast<uint8_t>(level);
            for (const int32_t bMaxHp : { 0, 1 }) {
                CompareStartingHitPoints(ctx, f, oldLevels, newLevels, bMaxHp, picks.Bits());
            }
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        uint8_t oldLevels[CAIOBJECT_CLASS_MAX] = {};
        uint8_t newLevels[CAIOBJECT_CLASS_MAX] = {};
        const int classes = picks.Between(1, 3);
        for (int c = 0; c < classes; ++c) {
            const int index = picks.Between(0, CAIOBJECT_CLASS_MAX - 1);
            newLevels[index] = static_cast<uint8_t>(picks.Between(1, 30));
            oldLevels[index] = static_cast<uint8_t>(picks.Between(0, newLevels[index]));
        }
        CompareStartingHitPoints(ctx, f, oldLevels, newLevels, picks.Between(0, 1), picks.Bits());
    }
}

std::string Args(std::initializer_list<int32_t> values)
{
    std::string text;
    for (const int32_t v : values) {
        text += (text.empty() ? "" : ", ") + std::to_string(v);
    }
    return text;
}

// CRuleTables::RollHitPoints (0x542A90): nSides 1, 2, 3, 4, 6, 8, 10, 12, 20
// and 100 by nRolls -1, 0, 1, 2, 5 and 20 by nLevel 0, 1, 2 and 30 by
// nMinRoll 0, 1, 4 and 21 by nModifier -3, 0 and 5, then --iterations random
// tuples, each from its own srand seed; the result and the following rand
// value. Neither side reads `this`; the original gets a zeroed block.
void RollHitPoints(CaseContext& ctx)
{
    const uint8_t* original = nullptr;
    ctx.CallOriginal([&] { original = OriginalBlock(kRuleTablesSize); });
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    const CRuleTables* ours = reinterpret_cast<const CRuleTables*>(storage);

    Picks picks(ctx.seed, 0x526F6C6C48500000ull); // "RollHP"
    auto check = [&](int32_t sides, int32_t rolls, int32_t level, int32_t minRoll, int32_t modifier) {
        const unsigned int seed = picks.Bits();
        ctx.SetInputs("srand(" + std::to_string(seed) + "), RollHitPoints("
            + Args({ sides, rolls, level, minRoll, modifier }) + ")");
        int32_t originalResult = 0;
        int originalNext = 0;
        ctx.CallOriginal([&] {
            OriginalSrand(seed);
            originalResult = reinterpret_cast<RollHitPointsFn>(kRollHitPoints)(original, nullptr, sides, rolls,
                level, minRoll, modifier);
            originalNext = OriginalRand();
        });
        iwd2r_srand(seed);
        const int32_t oursResult = ours->RollHitPoints(sides, rolls, level, minRoll, modifier);
        const int oursNext = iwd2r_rand();
        ctx.CountInputs(1);
        oracle::Compare(ctx, originalResult, oursResult);
        const std::string inputs = ctx.Inputs();
        ctx.SetInputs(inputs + ", the following rand");
        oracle::Compare(ctx, originalNext, oursNext);
    };
    for (const int32_t sides : { 1, 2, 3, 4, 6, 8, 10, 12, 20, 100 }) {
        for (const int32_t rolls : { -1, 0, 1, 2, 5, 20 }) {
            for (const int32_t level : { 0, 1, 2, 30 }) {
                for (const int32_t minRoll : { 0, 1, 4, 21 }) {
                    for (const int32_t modifier : { -3, 0, 5 }) {
                        check(sides, rolls, level, minRoll, modifier);
                    }
                }
            }
        }
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t sides = picks.Between(1, 100);
        const int32_t rolls = picks.Between(-2, 40);
        const int32_t level = picks.Between(-1, 40);
        const int32_t minRoll = picks.Between(-2, 101);
        check(sides, rolls, level, minRoll, picks.Between(-10, 10));
    }
    ::operator delete(storage);
}

std::string ResRefText(const uint8_t* bytes)
{
    std::string text;
    for (int i = 0; i < 8; ++i) {
        char hex[4];
        std::snprintf(hex, sizeof(hex), "%02X", bytes[i]);
        text += hex;
    }
    return text;
}

// CRuleTables::sub_545EB0 (0x545EB0), the automatic spell picker's lookup,
// with the game's SPLAUTOP.2DA: every nLevel and nIndex byte (the level
// clamped to the nine columns, the index past the rows reading the default).
// Both sides' rule tables hold only that table; the eight bytes of the
// CResRef are compared.
void SpellAutomaticPicker(CaseContext& ctx)
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    auto* ours = reinterpret_cast<CRuleTables*>(storage);
    new (&ours->m_tSpellAutomaticPicker) C2DArray();
    ours->m_tSpellAutomaticPicker.LoadText(CString(ReadTable(ctx, "SPLAUTOP.2DA").c_str()));

    const uint8_t* original = nullptr;
    ctx.SetInputs("building the original's CRuleTables");
    ctx.CallOriginal([&] {
        uint8_t* rules = OriginalBlock(kRuleTablesSize);
        FillOriginalTable(rules + kSpellPickerOffset, ours->m_tSpellAutomaticPicker);
        original = rules;
    });

    for (uint32_t level = 0; level <= 255; ++level) {
        for (uint32_t index = 0; index <= 255; ++index) {
            ctx.SetInputs("sub_545EB0(" + std::to_string(level) + ", " + std::to_string(index) + ")");
            uint8_t originalBytes[8] = {};
            ctx.CallOriginal([&] {
                reinterpret_cast<SpellPickFn>(kSpellPick)(original, nullptr, originalBytes, level, index);
            });
            const CResRef result = ours->sub_545EB0(static_cast<BYTE>(level), static_cast<BYTE>(index));
            uint8_t oursBytes[8];
            std::memcpy(oursBytes, result.GetResRef(), 8);
            ctx.CountInputs(1);
            const std::string a = ResRefText(originalBytes);
            const std::string b = ResRefText(oursBytes);
            if (a != b) {
                ctx.Fail(ctx.Inputs(), a, b);
            }
        }
    }
}

} // namespace

ORACLE_CASE("rules/spell_automatic_picker", SpellAutomaticPicker);
ORACLE_CASE("rules/starting_hp", StartingHitPoints);
ORACLE_CASE("rules/roll_hit_points", RollHitPoints);
