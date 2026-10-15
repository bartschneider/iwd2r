// Combat rules of the sprite, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CGameSprite::GetAC (0x723CC0), the touch armour class 0x723DB0 and the
// monk's wisdom bonus 0x723F60 read, with no armour (+0x4ADC) and no item in
// the off-hand slot (+0x4AD8 + 4 * (2 * weapon set + 44)): the derived
// armour class modifiers (+0x926, +0x928, +0x92A, +0x92C), the derived
// general state (+0x920, STATE_BLIND), DEX (+0x97A) and WIS (+0x978), the
// feats (+0x75C, blind fight through 0x763150 and 0x763200, which reads
// nothing else for it), the monk level (+0x62C, GetClassLevel 0x762760) and
// the weapon set (+0x4C68). GetAbilityScoreModifier (0x542510) is called on
// g_pBaldurChitin (0x8CF6DC) -> m_pObjectGame (0x1C54) and reads nothing of
// it. The original's sprite is a zeroed block from its operator new with
// those members set (R10b), and g_pBaldurChitin points at a zeroed chitin
// holding only a pointer to a zeroed game; ours is zeroed CGameSprite
// storage with the same members set, our g_pBaldurChitin likewise, put back
// to null when the case ends. Neither function draws a rand.
//
// Calling conventions (objdump of the vanilla exe): __thiscall without
// arguments, plain ret, INT in eax; called here through __fastcall pointers
// whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CBaldurChitin.h"
#include "CGameSprite.h"
#include "CInfGame.h"
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

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

using ArmorClassFn = int32_t(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kGetAC = 0x723CC0;
constexpr uintptr_t kTouchAC = 0x723DB0;
constexpr uintptr_t kMonkBonus = 0x723F60;

constexpr uintptr_t kBaldurChitin = 0x8CF6DC;
constexpr size_t kObjectGameOffset = 0x1C54;

constexpr size_t kSpriteBlockSize = 0x7800;
constexpr size_t kGeneralStateOffset = 0x920;
constexpr size_t kArmorClassOffset = 0x926; // four SHORTs
constexpr size_t kWisOffset = 0x978;
constexpr size_t kDexOffset = 0x97A;
constexpr size_t kFeatsOffset = 0x75C;
constexpr size_t kMonkLevelOffset = 0x62C;
constexpr size_t kWeaponSetOffset = 0x4C68;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

struct OurChitinReset {
    ~OurChitinReset() { g_pBaldurChitin = nullptr; }
};

struct Inputs {
    int16_t ac[4];
    int16_t dex;
    int16_t wis;
    uint32_t generalState;
    uint32_t feats;
    uint8_t monkLevel;
    uint8_t weaponSet;
};

std::string Describe(const Inputs& in)
{
    return "AC mods " + std::to_string(in.ac[0]) + " " + std::to_string(in.ac[1]) + " " + std::to_string(in.ac[2]) + " "
        + std::to_string(in.ac[3]) + ", DEX " + std::to_string(in.dex) + ", WIS " + std::to_string(in.wis)
        + ", general state " + std::to_string(in.generalState) + ", feats " + std::to_string(in.feats)
        + ", monk level " + std::to_string(in.monkLevel) + ", weapon set " + std::to_string(in.weaponSet);
}

void ArmorClass(CaseContext& ctx)
{
    OurChitinReset reset;
    uint8_t* original = nullptr;
    ctx.SetInputs("building the original's sprite block and g_pBaldurChitin");
    ctx.CallOriginal([&] {
        original = OriginalBlock(kSpriteBlockSize);
        uint8_t* chitin = OriginalBlock(kObjectGameOffset + 4);
        uint8_t* game = OriginalBlock(0x100);
        const uint32_t gamePointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(game));
        std::memcpy(chitin + kObjectGameOffset, &gamePointer, 4);
        const uint32_t chitinPointer = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(chitin));
        std::memcpy(reinterpret_cast<void*>(kBaldurChitin), &chitinPointer, 4);
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

    uint64_t state = ctx.seed ^ 0x723CC0ull;
    auto next = [&]() { return static_cast<uint32_t>(oracle::SplitMix64(&state)); };
    auto pick = [&](int32_t lo, int32_t hi) {
        const uint32_t span = static_cast<uint32_t>(hi - lo + 1);
        return static_cast<int32_t>(lo + static_cast<int32_t>(next() % span));
    };

    const uint32_t states[] = { 0, STATE_BLIND, STATE_BLIND | STATE_INVISIBLE, STATE_INVISIBLE, 0xFFFFFFFFu };
    const long long iterations = ctx.iterations > 0 ? ctx.iterations : 2000;
    for (long long n = 0; n < iterations; n++) {
        Inputs in;
        for (int16_t& ac : in.ac) {
            ac = static_cast<int16_t>(pick(-30, 30));
        }
        in.dex = static_cast<int16_t>(pick(-5, 60));
        in.wis = static_cast<int16_t>(pick(-5, 60));
        in.generalState = states[pick(0, 4)];
        in.feats = (pick(0, 1) ? (1u << CGAMESPRITE_FEAT_BLIND_FIGHT) : 0u) | (next() & 0xFFFFFF80u);
        in.monkLevel = static_cast<uint8_t>(pick(0, 1) ? 0 : pick(1, 30));
        in.weaponSet = static_cast<uint8_t>(pick(0, 3));
        ctx.SetInputs(Describe(in));

        int32_t originalResults[3] = {};
        ctx.CallOriginal([&] {
            std::memcpy(original + kArmorClassOffset, in.ac, sizeof(in.ac));
            std::memcpy(original + kGeneralStateOffset, &in.generalState, 4);
            std::memcpy(original + kDexOffset, &in.dex, 2);
            std::memcpy(original + kWisOffset, &in.wis, 2);
            std::memcpy(original + kFeatsOffset, &in.feats, 4);
            original[kMonkLevelOffset] = in.monkLevel;
            original[kWeaponSetOffset] = in.weaponSet;
            originalResults[0] = reinterpret_cast<ArmorClassFn>(kGetAC)(original, nullptr);
            originalResults[1] = reinterpret_cast<ArmorClassFn>(kTouchAC)(original, nullptr);
            originalResults[2] = reinterpret_cast<ArmorClassFn>(kMonkBonus)(original, nullptr);
        });

        ours->m_derivedStats.field_6 = in.ac[0];
        ours->m_derivedStats.field_8 = in.ac[1];
        ours->m_derivedStats.field_A = in.ac[2];
        ours->m_derivedStats.field_C = in.ac[3];
        ours->m_derivedStats.m_generalState = in.generalState;
        ours->m_derivedStats.m_nDEX = in.dex;
        ours->m_derivedStats.m_nWIS = in.wis;
        ours->m_baseStats.m_feats[0] = in.feats;
        ours->m_baseStats.m_monkLevel = in.monkLevel;
        ours->m_nWeaponSet = in.weaponSet;
        const int32_t oursResults[3] = { ours->GetAC(), ours->sub_723DB0(), ours->sub_723F60() };
        ctx.CountInputs(1);
        oracle::Compare(ctx, std::to_string(originalResults[0]) + " " + std::to_string(originalResults[1]) + " " + std::to_string(originalResults[2]),
            std::to_string(oursResults[0]) + " " + std::to_string(oursResults[1]) + " " + std::to_string(oursResults[2]));
    }
}

} // namespace

ORACLE_CASE("combat/armor_class", ArmorClass);
