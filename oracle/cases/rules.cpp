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
#include <random>
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

// ---------------------------------------------------------------------------
// rules/check_limits

// CDerivedStats::CheckLimits() (0x4468F0), plain ret at 0x446DC9: clamps the
// fields below in place. Fixture (R10b): the original's side is a zeroed
// block from its operator new, large enough for the last field read
// (0x153); ours is a CDerivedStats from our constructor. Every field the
// function reads is set on both sides by offset there and by name here, and
// compared after the call.
using CheckLimitsFn = void(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kCheckLimits = 0x4468F0;
constexpr size_t kDerivedStatsBlock = 0x200;

struct LimitField {
    const char* name;
    size_t offset;
    int size; // 1, 2 or 4 bytes
};

const LimitField kLimitFields[] = {
    { "generalState", 0x0, 4 },
    { "maxHitPoints", 0x4, 2 },
    { "acCrushing", 0xE, 2 },
    { "acMissile", 0x10, 2 },
    { "acPiercing", 0x12, 2 },
    { "acSlashing", 0x14, 2 },
    { "thac0", 0x18, 2 },
    { "attacks", 0x1A, 2 },
    { "saveFortitude", 0x1C, 2 },
    { "saveReflex", 0x1E, 2 },
    { "saveWill", 0x20, 2 },
    { "fatigue", 0x38, 2 },
    { "intoxication", 0x3A, 2 },
    { "luck", 0x3C, 2 },
    { "level", 0x46, 1 },
    { "str", 0x54, 2 },
    { "int", 0x56, 2 },
    { "wis", 0x58, 2 },
    { "dex", 0x5A, 2 },
    { "con", 0x5C, 2 },
    { "chr", 0x5E, 2 },
    { "xpValue", 0x60, 4 },
    { "xp", 0x64, 4 },
    { "gold", 0x68, 4 },
    { "moraleBreak", 0x6C, 4 },
    { "moraleRecovery", 0x70, 4 },
    { "reputation", 0x74, 2 },
    { "damageBonus", 0x86, 2 },
    { "spellFailureArcane", 0x88, 2 },
    { "spellFailureDivine", 0x8A, 2 },
    { "durationMage", 0x8C, 2 },
    { "durationPriest", 0x8E, 2 },
    { "turnUndead", 0x90, 2 },
    { "backstab", 0x92, 2 },
    { "layOnHands", 0x94, 2 },
    { "translucent", 0xB2, 2 },
    { "skill6", 0x14A, 1 },
    { "skill9", 0x14D, 1 },
    { "skill10", 0x14E, 1 },
    { "skill11", 0x14F, 1 },
    { "skill12", 0x150, 1 },
    { "skill14", 0x152, 1 },
    { "skill15", 0x153, 1 },
};
constexpr size_t kLimitFieldCount = sizeof(kLimitFields) / sizeof(kLimitFields[0]);

// Our field i, as the bits of its type, through a reference to it.
template <typename F>
void OurLimitField(CDerivedStats& stats, size_t i, F f)
{
    switch (i) {
    case 0:
        f(stats.m_generalState);
        break;
    case 1:
        f(stats.m_nMaxHitPoints);
        break;
    case 2:
        f(stats.m_nACCrushingMod);
        break;
    case 3:
        f(stats.m_nACMissileMod);
        break;
    case 4:
        f(stats.m_nACPiercingMod);
        break;
    case 5:
        f(stats.m_nACSlashingMod);
        break;
    case 6:
        f(stats.m_nTHAC0);
        break;
    case 7:
        f(stats.m_nNumberOfAttacks);
        break;
    case 8:
        f(stats.m_nSaveVSFortitude);
        break;
    case 9:
        f(stats.m_nSaveVSReflex);
        break;
    case 10:
        f(stats.m_nSaveVSWill);
        break;
    case 11:
        f(stats.m_nFatigue);
        break;
    case 12:
        f(stats.m_nIntoxication);
        break;
    case 13:
        f(stats.m_nLuck);
        break;
    case 14:
        f(stats.m_nLevel);
        break;
    case 15:
        f(stats.m_nSTR);
        break;
    case 16:
        f(stats.m_nINT);
        break;
    case 17:
        f(stats.m_nWIS);
        break;
    case 18:
        f(stats.m_nDEX);
        break;
    case 19:
        f(stats.m_nCON);
        break;
    case 20:
        f(stats.m_nCHR);
        break;
    case 21:
        f(stats.m_nXPValue);
        break;
    case 22:
        f(stats.m_nXP);
        break;
    case 23:
        f(stats.m_nGold);
        break;
    case 24:
        f(stats.m_nMoraleBreak);
        break;
    case 25:
        f(stats.m_nMoraleRecoveryTime);
        break;
    case 26:
        f(stats.m_nReputation);
        break;
    case 27:
        f(stats.m_nDamageBonus);
        break;
    case 28:
        f(stats.m_nSpellFailureArcane);
        break;
    case 29:
        f(stats.m_nSpellFailureDivine);
        break;
    case 30:
        f(stats.m_nSpellDurationModMage);
        break;
    case 31:
        f(stats.m_nSpellDurationModPriest);
        break;
    case 32:
        f(stats.m_nTurnUndeadLevel);
        break;
    case 33:
        f(stats.m_nBackstabDamageMultiplier);
        break;
    case 34:
        f(stats.m_nLayOnHandsAmount);
        break;
    case 35:
        f(stats.m_nTranslucent);
        break;
    case 36:
        f(stats.m_nSkills[6]);
        break;
    case 37:
        f(stats.m_nSkills[9]);
        break;
    case 38:
        f(stats.m_nSkills[10]);
        break;
    case 39:
        f(stats.m_nSkills[11]);
        break;
    case 40:
        f(stats.m_nSkills[12]);
        break;
    case 41:
        f(stats.m_nSkills[14]);
        break;
    case 42:
        f(stats.m_nSkills[15]);
        break;
    }
}

// The values a field is set to: the clamps' bounds and their neighbours,
// the sign boundaries and the type's extremes, then random bits.
uint32_t LimitValue(std::mt19937& rng, int size)
{
    static const int32_t kEdges[] = { 0, 1, -1, 2, 5, 9, 10, 11, 19, 20, 21, 39, 40, 41, 49, 50, 51, 99, 100, 101,
        127, 128, 199, 200, 201, 254, 255, 256, -20, -21, -40, -41, -128, -129, -255, -256, 0x7FFF, -0x8000,
        0x7FFFFFFF, static_cast<int32_t>(0x80000000), static_cast<int32_t>(0xFFFFFFFF),
        static_cast<int32_t>(0xFFFFFFFE) };
    uint32_t value = (rng() % 3 == 0) ? rng() : static_cast<uint32_t>(kEdges[rng() % (sizeof(kEdges) / sizeof(kEdges[0]))]);
    if (size == 1) {
        value &= 0xFF;
    } else if (size == 2) {
        value &= 0xFFFF;
    }
    return value;
}

void CheckLimitsCase(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed ^ 0x4468F0u));
    uint8_t* block = nullptr;
    ctx.CallOriginal([&] {
        block = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kDerivedStatsBlock));
    });
    CDerivedStats ours;

    // A death state: the dead bit and others (0x800 and 0xFFFFFFFF).
    const int rounds = ctx.iterations < 200 ? 200 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        uint32_t values[kLimitFieldCount];
        std::string inputs = "round " + std::to_string(round) + ":";
        for (size_t i = 0; i < kLimitFieldCount; ++i) {
            values[i] = LimitValue(rng, kLimitFields[i].size);
            if (i == 0 && round % 4 == 0) {
                values[i] |= 0x800;
            }
            inputs += std::string(" ") + kLimitFields[i].name + "=" + std::to_string(values[i]);
        }
        ctx.SetInputs(inputs);

        uint32_t original[kLimitFieldCount];
        ctx.CallOriginal([&] {
            std::memset(block, 0, kDerivedStatsBlock);
            for (size_t i = 0; i < kLimitFieldCount; ++i) {
                std::memcpy(block + kLimitFields[i].offset, &values[i], kLimitFields[i].size);
            }
            reinterpret_cast<CheckLimitsFn>(kCheckLimits)(block, nullptr);
            for (size_t i = 0; i < kLimitFieldCount; ++i) {
                original[i] = 0;
                std::memcpy(&original[i], block + kLimitFields[i].offset, kLimitFields[i].size);
            }
        });

        for (size_t i = 0; i < kLimitFieldCount; ++i) {
            OurLimitField(ours, i, [&](auto& field) {
                std::memcpy(&field, &values[i], sizeof(field));
            });
        }
        ours.CheckLimits();
        ctx.CountInputs(1);
        for (size_t i = 0; i < kLimitFieldCount; ++i) {
            uint32_t value = 0;
            OurLimitField(ours, i, [&](auto& field) {
                std::memcpy(&value, &field, sizeof(field));
            });
            if (!oracle::Compare(ctx, std::string(kLimitFields[i].name) + "=" + std::to_string(original[i]),
                    std::string(kLimitFields[i].name) + "=" + std::to_string(value))) {
                break;
            }
        }
    }
}

// The exe's CDerivedStats constructor zeroes the two 10-entry
// decrementing arrays at +0x328 and +0x378 (0x443CA3, 0x443CB9) and, after
// BonusInit, empties them again (0x4E7050 at 0x443FD5, 0x443FE0).  Our
// header does not declare those members yet, so ours cannot hold this
// state and the case stays pending: it checks the original's contract,
// not a comparison.
//
// The fixture is the raw pre-construction bytes of the object: a full-size
// block from the original's operator new (0xE58 bytes, the exe's sizeof
// CDerivedStats -- m_disabledSpellTypes[3] ends at 0xE58), filled with a
// deterministic poison so a skipped region cannot pass by the luck of a
// zeroing allocator.  The ctor never reads pre-construction bytes, so the
// poison is safe to leave under every member it initializes.
constexpr uintptr_t kDStatsCtor = 0x443B30;
constexpr size_t kDStatsFullSize = 0xE58;
constexpr uintptr_t kDStatsSpellLevelVtable = 0x84852c;
constexpr size_t kDecrementingBegin = 0x328;
constexpr size_t kDecrementingEnd = 0x3C8;
using DStatsCtorFn = void(__fastcall*)(void* self, void* edx);

void DStatsCtorZeroesDecrementing(CaseContext& ctx)
{
    uint8_t* block = nullptr;
    ctx.CallOriginal([&] {
        block = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kDStatsFullSize));
    });

    const int rounds = ctx.iterations < 8 ? 8 : ctx.iterations;
    for (int round = 0; round < rounds; ++round) {
        std::mt19937 poison(static_cast<uint32_t>(ctx.seed ^ (0xC0DEC7E5u + static_cast<uint32_t>(round))));
        for (size_t i = 0; i < kDStatsFullSize; ++i) {
            block[i] = static_cast<uint8_t>(0x01 + (poison() >> 24) % 0xFF);
        }
        ctx.SetInputs("round " + std::to_string(round));
        ctx.CallOriginal([&] {
            reinterpret_cast<DStatsCtorFn>(kDStatsCtor)(block, nullptr);
        });
        ctx.CountInputs(1);

        // The ctor ran: it installed the spell-level list vtable at +0x188.
        uintptr_t vtable = 0;
        std::memcpy(&vtable, block + 0x188, sizeof(vtable));
        if (!oracle::Compare(ctx, kDStatsSpellLevelVtable, vtable)) {
            break;
        }
        // The disagreement: both arrays are zero after construction, not
        // pre-construction garbage.
        for (size_t off = kDecrementingBegin; off < kDecrementingEnd; off += 4) {
            uint32_t value = 0;
            std::memcpy(&value, block + off, sizeof(value));
            if (!oracle::Compare(ctx, 0u, value)) {
                break;
            }
        }
    }
}

} // namespace

ORACLE_CASE("rules/spell_automatic_picker", SpellAutomaticPicker);
ORACLE_CASE("rules/starting_hp", StartingHitPoints);
ORACLE_CASE("rules/roll_hit_points", RollHitPoints);
ORACLE_CASE("rules/check_limits", CheckLimitsCase);
ORACLE_CASE_PENDING("rules/dstats_ctor_zeroes_decrementing", DStatsCtorZeroesDecrementing, "ours cannot hold the +0x328..+0x3C7 state until CDerivedStats.h declares the two 10-entry decrementing arrays; original-only until then");
