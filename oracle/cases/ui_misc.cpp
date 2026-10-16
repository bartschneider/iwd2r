// The ui-misc stream's pure functions, the original's against ours:
// CGameSprite's dexterity bonus to the armour class (0x723EA0),
// CUtil::FindOneOf (0x781180) and CRuleTables' hit points per level from
// constitution (0x542EC0).
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures:
// - 0x723EA0 reads, with no armour (+0x4ADC): DEX (+0x97A), the derived
//   general state (+0x920, STATE_BLIND) and the feats (+0x75C, Blind Fight
//   through 0x763150); GetAbilityScoreModifier (0x542510) is called on
//   g_pBaldurChitin (0x8CF6DC) -> m_pObjectGame (0x1C54) and reads nothing
//   of it. As combat/armor_class: the original's sprite is a zeroed block
//   from its operator new with those members set (R10b), g_pBaldurChitin a
//   zeroed chitin holding a zeroed game; ours zeroed storage of our classes'
//   sizes with the same members, g_pBaldurChitin put back to null at the
//   end.
// - FindOneOf reads g_pChitin (0x8CF6D8) + 0x1A0 (Double Byte Character
//   Support) and calls IsDBCSLeadByte. The original's is the system's (the
//   host's ANSI code page); ours is the shim's stub (platform/
//   win32_stubs.cpp), FALSE for every byte unless a test installs a lead
//   byte table. So the case covers the single-byte behaviour only: with
//   the flag on, no separator is removed and no byte skipped (the input's
//   high bytes are not lead bytes on the host, or the two would differ).
//   The double-byte paths have the unit test (tests/test_ui_misc.cpp).
//   Both strings are original CStrings from its constructor (0x7FCC88) in
//   leaked memory (R10a); the original copies the separators (0x7FC98F, the
//   copy constructor) into their argument's slot (0x7811A5).
// - 0x542EC0 reads neither `this` nor the class (zeroed blocks).
//
// Calling conventions (objdump of the vanilla exe): 0x723EA0 __thiscall
// without arguments, plain ret; FindOneOf __cdecl (const CString&, const
// CString&, int), plain ret; 0x542EC0
// __thiscall with four stack arguments, `ret 0x10`.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CBaldurChitin.h"
#include "CChitin.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CRuleTables.h"
#include "CUtil.h"
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

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CString::CString(LPCSTR) (0x7FCC88): this in ecx, the text at [esp+4],
// ret 4.
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

using DexArmorClassFn = int32_t(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kDexArmorClass = 0x723EA0;

using FindOneOfFn = int32_t(__cdecl*)(const void* sString, const void* sSeparators, int32_t start);
constexpr uintptr_t kFindOneOf = 0x781180;

using HpConBonusFn = void(__fastcall*)(const void* self, void* edx, uint32_t nClass, uint32_t nCON, int32_t* bonus,
    int32_t* rolls);
constexpr uintptr_t kHpConBonus = 0x542EC0;

constexpr uintptr_t kBaldurChitin = 0x8CF6DC;
constexpr uintptr_t kChitin = 0x8CF6D8;
constexpr size_t kObjectGameOffset = 0x1C54;
constexpr size_t kDbcsOffset = 0x1A0;

constexpr size_t kSpriteBlockSize = 0x7800;
constexpr size_t kGeneralStateOffset = 0x920;
constexpr size_t kDexOffset = 0x97A;
constexpr size_t kFeatsOffset = 0x75C;
constexpr size_t kRuleTablesSize = 0x1B58;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

void SetOriginalPointer(uintptr_t global, const void* value)
{
    const uint32_t pointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
    std::memcpy(reinterpret_cast<void*>(global), &pointer, 4);
}

struct OurGlobalsReset {
    ~OurGlobalsReset()
    {
        g_pBaldurChitin = nullptr;
        g_pChitin = nullptr;
    }
};

// 0x723EA0 with DEX -5..60, blind or not, with or without Blind Fight and
// other feats set; --iterations random states.
void DexArmorClass(CaseContext& ctx)
{
    OurGlobalsReset reset;
    uint8_t* original = nullptr;
    ctx.SetInputs("building the original's sprite block and g_pBaldurChitin");
    ctx.CallOriginal([&] {
        original = OriginalBlock(kSpriteBlockSize);
        uint8_t* chitin = OriginalBlock(kObjectGameOffset + 4);
        uint8_t* game = OriginalBlock(0x100);
        const uint32_t gamePointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(game));
        std::memcpy(chitin + kObjectGameOffset, &gamePointer, 4);
        SetOriginalPointer(kBaldurChitin, chitin);
    });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameSprite)));
    std::memset(storage, 0, sizeof(CGameSprite));
    auto* ours = reinterpret_cast<CGameSprite*>(storage);
    auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CBaldurChitin)));
    std::memset(chitin, 0, sizeof(CBaldurChitin));
    auto* game = static_cast<unsigned char*>(::operator new(sizeof(CInfGame)));
    std::memset(game, 0, sizeof(CInfGame));
    reinterpret_cast<CBaldurChitin*>(chitin)->m_pObjectGame = reinterpret_cast<CInfGame*>(game);
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(chitin);

    uint64_t state = ctx.seed ^ 0x723EA0ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };

    auto check = [&](int16_t dex, uint32_t generalState, uint32_t feats) {
        ctx.SetInputs("DEX " + std::to_string(dex) + ", general state " + std::to_string(generalState) + ", feats "
            + std::to_string(feats));
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(original + kGeneralStateOffset, &generalState, 4);
            std::memcpy(original + kDexOffset, &dex, 2);
            std::memcpy(original + kFeatsOffset, &feats, 4);
            originalResult = reinterpret_cast<DexArmorClassFn>(kDexArmorClass)(original, nullptr);
        });
        ours->m_derivedStats.m_generalState = generalState;
        ours->m_derivedStats.m_nDEX = dex;
        ours->m_baseStats.m_feats[0] = feats;
        ctx.CountInputs(1);
        oracle::Compare(ctx, originalResult, static_cast<int32_t>(ours->sub_723EA0()));
    };

    const uint32_t states[] = { 0, STATE_BLIND, STATE_BLIND | STATE_INVISIBLE, STATE_INVISIBLE, 0xFFFFFFFFu };
    const uint32_t featSets[] = { 0, 1u << CGAMESPRITE_FEAT_BLIND_FIGHT, 0xFFFFFFFFu, 0xFFFFFFFFu & ~(1u << CGAMESPRITE_FEAT_BLIND_FIGHT) };
    for (int16_t dex = -5; dex <= 60; dex++) {
        for (uint32_t generalState : states) {
            for (uint32_t feats : featSets) {
                check(dex, generalState, feats);
            }
        }
    }
    for (long long n = 0; n < ctx.iterations; n++) {
        check(static_cast<int16_t>(next() % 81) - 10, next(), next());
    }
}

// FindOneOf over strings of letters, separators and high bytes, every start
// from -1 to past the end, with Double Byte Character Support off and on
// (no lead bytes on either side; see the fixtures); then --iterations
// random cases.
void FindOneOf(CaseContext& ctx)
{
    OurGlobalsReset reset;
    uint8_t* originalChitin = nullptr;
    ctx.SetInputs("building the original's g_pChitin");
    ctx.CallOriginal([&] {
        originalChitin = OriginalBlock(kDbcsOffset + 4);
        SetOriginalPointer(kChitin, originalChitin);
    });
    auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CChitin)));
    std::memset(chitin, 0, sizeof(CChitin));
    g_pChitin = reinterpret_cast<CChitin*>(chitin);

    uint64_t state = ctx.seed ^ 0x781180ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };
    const char alphabet[] = "ab,.; :\xA1\xE9\x82";

    auto check = [&](const std::string& sString, const std::string& sSeparators, int32_t start, int32_t dbcs) {
        ctx.SetInputs("FindOneOf(\"" + sString + "\", \"" + sSeparators + "\", " + std::to_string(start) + "), DBCS "
            + std::to_string(dbcs));
        int32_t originalResult = 0;
        ctx.CallOriginal([&] {
            std::memcpy(originalChitin + kDbcsOffset, &dbcs, 4);
            void* original = reinterpret_cast<OriginalNewFn>(kOriginalNew)(4);
            reinterpret_cast<OriginalCStringFn>(kOriginalCString)(original, nullptr, sString.c_str());
            void* originalSeparators = reinterpret_cast<OriginalNewFn>(kOriginalNew)(4);
            reinterpret_cast<OriginalCStringFn>(kOriginalCString)(originalSeparators, nullptr, sSeparators.c_str());
            originalResult = reinterpret_cast<FindOneOfFn>(kFindOneOf)(original, originalSeparators, start);
        });
        g_pChitin->field_1A0 = dbcs;
        ctx.CountInputs(1);
        oracle::Compare(ctx,
            originalResult,
            static_cast<int32_t>(CUtil::FindOneOf(CString(sString.c_str()), CString(sSeparators.c_str()), start)));
    };

    const char* strings[] = { "", "a", ",", "a,b.c", "abc", "abcdefghijklmnopqrstuvwxyz;", "\xA1,b,c", "a b:c" };
    const char* separators[] = { "", ",", ",.", ";", " :", "xyz", "\xA1" };
    for (const char* sString : strings) {
        for (const char* sSeparators : separators) {
            const int32_t length = static_cast<int32_t>(std::strlen(sString));
            for (int32_t start = -1; start <= length + 1; start++) {
                check(sString, sSeparators, start, 0);
                check(sString, sSeparators, start, 1);
            }
        }
    }
    for (long long n = 0; n < ctx.iterations; n++) {
        std::string sString;
        std::string sSeparators;
        const uint32_t stringLength = next() % 40;
        const uint32_t separatorLength = next() % 5;
        for (uint32_t i = 0; i < stringLength; i++) {
            sString += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        for (uint32_t i = 0; i < separatorLength; i++) {
            sSeparators += alphabet[next() % (sizeof(alphabet) - 1)];
        }
        check(sString, sSeparators, static_cast<int32_t>(next() % (stringLength + 2)), static_cast<int32_t>(next() % 2));
    }
}

// 0x542EC0 for every CON byte with classes 0..12; --iterations random pairs.
void HpConBonus(CaseContext& ctx)
{
    void* original = nullptr;
    ctx.SetInputs("building the original's CRuleTables block");
    ctx.CallOriginal([&] { original = OriginalBlock(kRuleTablesSize); });
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CRuleTables)));
    std::memset(storage, 0, sizeof(CRuleTables));
    const CRuleTables* ours = reinterpret_cast<const CRuleTables*>(storage);

    auto check = [&](uint32_t nClass, uint32_t nCON) {
        ctx.SetInputs("sub_542EC0(" + std::to_string(nClass) + ", " + std::to_string(nCON) + ")");
        int32_t originalBonus = 0x55555555;
        int32_t originalRolls = 0x55555555;
        ctx.CallOriginal([&] {
            reinterpret_cast<HpConBonusFn>(kHpConBonus)(original, nullptr, nClass, nCON, &originalBonus, &originalRolls);
        });
        INT oursBonus = 0x55555555;
        INT oursRolls = 0x55555555;
        ours->sub_542EC0(static_cast<BYTE>(nClass), static_cast<BYTE>(nCON), oursBonus, oursRolls);
        ctx.CountInputs(1);
        oracle::Compare(ctx,
            std::to_string(originalBonus) + " " + std::to_string(originalRolls),
            std::to_string(oursBonus) + " " + std::to_string(oursRolls));
    };

    for (uint32_t nClass = 0; nClass <= 12; nClass++) {
        for (uint32_t nCON = 0; nCON <= 255; nCON++) {
            check(nClass, nCON);
        }
    }
    uint64_t state = ctx.seed ^ 0x542EC0ull;
    for (long long n = 0; n < ctx.iterations; n++) {
        const uint32_t nClass = static_cast<uint32_t>(oracle::SplitMix64(&state));
        const uint32_t nCON = static_cast<uint32_t>(oracle::SplitMix64(&state));
        check(nClass & 0xFF, nCON & 0xFF);
    }
}

} // namespace

ORACLE_CASE("combat/dex_armor_class", DexArmorClass);
ORACLE_CASE("util/find_one_of", FindOneOf);
ORACLE_CASE("rules/hp_con_bonus", HpConBonus);
