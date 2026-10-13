// Pending cases for project 3 (combat): saving throws, the attack roll and
// attack bonus and damage. Ours does not implement these functions yet, so
// nothing is compared with ours. A case runs the original
// alone where it can on a fixture built here (R11b): the same seed and inputs
// twice, and the two results (and the rand value after each call, which
// shows both calls drew alike) must be equal. In the report a failure's
// "original" is the first call and "ours" the second. A function that needs
// live engine state is listed without a body, with the reason. The names are
// tools/ghidra/extra_labels.txt's and docs/evidence/<ADDR>.md holds each
// function's evidence.
//
// When ours exists, a case here becomes a comparison case in the file of its
// class and its evidence file's oracle_case follows it.
//
// This file includes no engine header and no <windows.h>: it reaches the
// original only through CallOriginal and fixed addresses.
#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace {

using Srand = void(__cdecl*)(unsigned int);
using Rand = int(__cdecl*)();
// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CInfGame::m_ruleTables runs from 0 to 0x1B58 (CInfGame.h).
constexpr size_t kRuleTablesSize = 0x1B58;

// CRuleTables::GetIterativeAttackBonus (0x547020): max(0, nBonus - nStep *
// nAttack). __thiscall with three stack arguments, `ret 0xc` at 0x547033;
// its first instruction overwrites ecx (`mov ecx, [esp+8]`), so `this` is
// never read. INT in eax. Called here through a __fastcall pointer whose edx
// argument is unused.
using IterativeAttackBonusFn = int32_t(__fastcall*)(const void* self, void* edx, int32_t nBonus, int32_t nStep,
    int32_t nAttack);
constexpr uintptr_t kGetIterativeAttackBonus = 0x547020;

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

int OriginalRand()
{
    return reinterpret_cast<Rand>(static_cast<uintptr_t>(kVanillaRand))();
}

// The original CRuleTables GetIterativeAttackBonus is called on: a zeroed
// block of its size from the original's operator new. It is not read
// (above), so no member is filled in. Leaked, like every original object the cases build.
const void* RuleTablesFixture(CaseContext& ctx)
{
    void* at = nullptr;
    ctx.CallOriginal([&] { at = reinterpret_cast<OriginalNewFn>(kOriginalNew)(kRuleTablesSize); });
    std::memset(at, 0, kRuleTablesSize);
    return at;
}

// One call's result and the original rand value drawn right after it.
struct Draw {
    int32_t result = 0;
    int next = 0;
};

// Runs call() twice from srand(seed) and requires the same result and the
// same following rand value both times.
template <class F>
void CheckTwice(CaseContext& ctx, const std::string& inputs, unsigned int seed, F call)
{
    ctx.SetInputs("srand(" + oracle::Show(seed) + "), " + inputs);
    Draw first;
    Draw second;
    ctx.CallOriginal([&] {
        OriginalSrand(seed);
        first.result = call();
        first.next = OriginalRand();
        OriginalSrand(seed);
        second.result = call();
        second.next = OriginalRand();
    });
    ctx.CountInputs(1);
    oracle::Compare(ctx, first.result, second.result);
    const std::string withNext = ctx.Inputs();
    ctx.SetInputs(withNext + ", the following rand");
    oracle::Compare(ctx, first.next, second.next);
    ctx.SetInputs(withNext);
}

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

std::string Args(std::initializer_list<int32_t> values)
{
    std::string text;
    for (const int32_t v : values) {
        text += (text.empty() ? "" : ", ") + std::to_string(v);
    }
    return text;
}

// CRuleTables::GetIterativeAttackBonus (0x547020), original only: every
// nBonus in [-5, 40] with steps 0, 1, 3, 5 and -5 and nAttack in [0, 5]; the
// 32-bit extremes; then --iterations random triples. It draws no rand, so
// the check pins only that it is a pure function of its arguments.
void IterativeAttackBonus(CaseContext& ctx)
{
    const void* rules = RuleTablesFixture(ctx);
    Picks picks(ctx.seed, 0x4974657241747400ull); // "IterAtt"
    auto check = [&](int32_t bonus, int32_t step, int32_t attack) {
        CheckTwice(ctx, "GetIterativeAttackBonus(" + Args({ bonus, step, attack }) + ")", picks.Bits(), [&] {
            return reinterpret_cast<IterativeAttackBonusFn>(kGetIterativeAttackBonus)(rules, nullptr, bonus, step,
                attack);
        });
    };
    for (int32_t bonus = -5; bonus <= 40; ++bonus) {
        for (const int32_t step : { 0, 1, 3, 5, -5 }) {
            for (int32_t attack = 0; attack <= 5; ++attack) {
                check(bonus, step, attack);
            }
        }
    }
    for (const int32_t e : { INT32_MIN, -1, 0, 1, INT32_MAX }) {
        check(e, 5, 1);
        check(10, e, 1);
        check(10, 5, e);
        check(e, e, e);
    }
    for (int i = 0; i < ctx.iterations; ++i) {
        const int32_t bonus = picks.Between(-50, 100);
        const int32_t step = picks.Between(-10, 10);
        check(bonus, step, picks.Between(-2, 10));
    }
}

} // namespace

ORACLE_CASE_PENDING("pending/iterative_attack_bonus", IterativeAttackBonus,
    "ours not implemented (CRuleTables::GetIterativeAttackBonus 0x547020); original-only determinism check");
// CGameEffect::CheckSave (0x4A42F0) first reads g_pBaldurChitin (0x8CF6DC)
// ->m_pObjectGame, then the target sprite's virtual GetAIType, its feats
// (0x763150), class levels and, for some effects, the game's object array
// (CGameObjectArray::GetShare) and party; it reports through FeedBack,
// CBaldurMessage::DisplayText and CGameSprite_DisplaySaveString.
ORACLE_CASE_PENDING("pending/check_save", nullptr,
    "ours not implemented (CGameEffect::CheckSave 0x4A42F0); needs engine state (g_pBaldurChitin's game, a "
    "sprite with its vtable, feats and class levels, the object array)");
// CGameSprite::ProcessEffectList (0x72DE60; src/ also labels the address
// CGameSprite::sub_72DE60) rolls the four save d20s (rand() % 20 + 1) and a
// rand() % 100 into the sprite (0x70F6-0x70FA) as part of processing the
// sprite's whole effect list.
ORACLE_CASE_PENDING("pending/save_rolls", nullptr,
    "ours not implemented (CGameSprite::ProcessEffectList 0x72DE60 rolls the save dice); needs engine state "
    "(the sprite's effect lists, derived stats and g_pBaldurChitin's game)");
// CGameSprite::Hit (0x73B760) rolls the attack d20 (CUtil::UtilRandInt(20,
// luck) + 1) and compares it with the target's AC.
ORACLE_CASE_PENDING("pending/hit", nullptr,
    "ours not implemented (CGameSprite::Hit 0x73B760); needs engine state (equipped CItems with loaded "
    "abilities, an attacker and a target sprite, g_pBaldurChitin's game and options)");
// CGameSprite::Damage (0x73D560) rolls the weapon's damage dice
// (CUtil::UtilRandInt(size, luck) + 1 per die) and builds the damage effect.
ORACLE_CASE_PENDING("pending/damage", nullptr,
    "ours not implemented (CGameSprite::Damage 0x73D560); needs engine state (equipped CItems with loaded "
    "abilities, the target's animation, g_pBaldurChitin's game, the message handler)");
