// Animation-type virtuals, the original's against ours: the Ankheg's
// sequence/frame pair, the old character's armor-sound name, and the flyer's
// answer to "can you lie down".
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Ours ported GetCurrentSequenceAndFrame with SHORT by value, so the values
// it assigns never reach the caller; the original's two arguments are
// pointers and it writes both. GetSndArmor rolls the digit with rand(), so
// each side's generator is seeded with the same stream (iwd2r_srand for ours,
// the original's srand inside CallOriginal) over a fresh zeroed block, which
// also empties the last-name cache at 0x3F2 before every input. 0x6A3C60 is
// one `xor al,al; ret` body the original's linker folded several
// return-zero virtuals into (ours labels it CGameObject::IsProjectile, two
// DetectedByInfravision overrides and the base GetListType); Flying's vtable
// slot 0x28 — CanLieDown — points at it too, so the original answers 0 while
// ours' port carries no Flying override and inherits the base's TRUE.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CGameAnimationType.h"
#include "CGameAnimationTypeCharacterOld.h"
#include "CGameAnimationTypeFlying.h"
#include "CGameAnimationTypeMonsterAnkheg.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "msvc_rand.h"
#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

using Srand = void(__cdecl*)(unsigned int);

// CGameAnimationTypeMonsterAnkheg::GetCurrentSequenceAndFrame (0x6C2D70):
// __thiscall, two SHORT* out-parameters, `ret 8`, BOOLEAN in al. Reads the
// pair from the vid cell at this+0x402 (sequence at +0xC6, frame at +0xC4).
using SequenceFn = uint8_t(__fastcall*)(const void* self, void* edx, int16_t* sequence, int16_t* frame);
constexpr uintptr_t kGetSequenceAndFrame = 0x6C2D70;

// CGameAnimationTypeCharacterOld::GetSndArmor (0x6DD5B0): __thiscall, no
// arguments, char* in eax — a new'd "A_Lthr<n>"-style name for armor codes
// '2'/'3'/'4', "" for '1', and "" for any other code in the original (NULL
// in ours).
using SndArmorFn = const char*(__fastcall*)(const void* self, void* edx);
constexpr uintptr_t kGetSndArmor = 0x6DD5B0;

// Flying's CanLieDown (vtable 0x85AD1C + 0x28 → 0x6A3C60): the shared
// return-zero fold, `xor al,al; ret`, no this dereference.
using BoolFn = uint8_t(__fastcall*)(const void* self, void* edx);
constexpr uintptr_t kCanLieDown = 0x6A3C60;

// Covers m_nCurrentSequence at 0xC6; ours' CVidCell puts the pair at the
// same offsets.
constexpr size_t kVidCellSize = 0xC8;

void OriginalSrand(unsigned int seed)
{
    reinterpret_cast<Srand>(static_cast<uintptr_t>(kVanillaSrand))(seed);
}

// One vid cell on each heap with the same sequence/frame pair.
uint8_t* MakeCell(bool original, int16_t sequence, int16_t frame)
{
    auto* cell = static_cast<uint8_t*>(original
            ? reinterpret_cast<OriginalNewFn>(kOriginalNew)(kVidCellSize)
            : ::operator new(kVidCellSize));
    std::memset(cell, 0, kVidCellSize);
    std::memcpy(cell + 0xC6, &sequence, 2);
    std::memcpy(cell + 0xC4, &frame, 2);
    return cell;
}

std::string Render(const char* text)
{
    return text ? std::string(text) : std::string("(null)");
}

void CheckSequenceAndFrame(CaseContext& ctx, int16_t sequence, int16_t frame)
{
    ctx.SetInputs("vidcell sequence " + std::to_string(sequence) + ", frame " + std::to_string(frame));

    uint8_t originalRet = 0;
    int16_t originalSequence = 0;
    int16_t originalFrame = 0;
    ctx.CallOriginal([&] {
        auto* self = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(sizeof(CGameAnimationTypeMonsterAnkheg)));
        std::memset(self, 0, sizeof(CGameAnimationTypeMonsterAnkheg));
        auto* cell = MakeCell(true, sequence, frame);
        const uint32_t cellAddress = reinterpret_cast<uint32_t>(cell);
        std::memcpy(self + 0x402, &cellAddress, 4); // m_currentVidCell
        originalRet = reinterpret_cast<SequenceFn>(kGetSequenceAndFrame)(self, nullptr, &originalSequence, &originalFrame);
    });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameAnimationTypeMonsterAnkheg)));
    std::memset(storage, 0, sizeof(CGameAnimationTypeMonsterAnkheg));
    auto* ours = reinterpret_cast<CGameAnimationTypeMonsterAnkheg*>(storage);
    ours->m_currentVidCell = reinterpret_cast<CVidCell*>(MakeCell(false, sequence, frame));

    // Sentinels: ours takes the pair by value, so unless the port delivers
    // them somehow these stay behind.
    int16_t oursSequence = 100;
    int16_t oursFrame = 200;
    const uint8_t oursRet = ours->CGameAnimationTypeMonsterAnkheg::GetCurrentSequenceAndFrame(oursSequence, oursFrame);

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalRet, oursRet);
    const std::string inputs = ctx.Inputs();
    ctx.SetInputs(inputs + ", the sequence it reports");
    oracle::Compare(ctx, originalSequence, oursSequence);
    ctx.SetInputs(inputs + ", the frame it reports");
    oracle::Compare(ctx, originalFrame, oursFrame);
    ctx.SetInputs(inputs);
}

void CheckSndArmor(CaseContext& ctx, int armorCode, unsigned int seed)
{
    const std::string codeText = armorCode >= 32 && armorCode < 127
        ? std::string(1, static_cast<char>(armorCode))
        : std::to_string(armorCode);
    ctx.SetInputs("armor code '" + codeText + "', srand seed " + std::to_string(seed));

    const char* originalText = nullptr;
    ctx.CallOriginal([&] {
        auto* self = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(sizeof(CGameAnimationTypeCharacterOld)));
        std::memset(self, 0, sizeof(CGameAnimationTypeCharacterOld));
        self[0x40E] = static_cast<uint8_t>(armorCode); // m_armorCode
        OriginalSrand(seed);
        originalText = reinterpret_cast<SndArmorFn>(kGetSndArmor)(self, nullptr);
    });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameAnimationTypeCharacterOld)));
    std::memset(storage, 0, sizeof(CGameAnimationTypeCharacterOld));
    auto* ours = reinterpret_cast<CGameAnimationTypeCharacterOld*>(storage);
    ours->m_armorCode = static_cast<CHAR>(armorCode);
    iwd2r_srand(seed);
    const char* oursText = ours->CGameAnimationTypeCharacterOld::GetSndArmor();

    ctx.CountInputs(1);
    oracle::Compare(ctx, Render(originalText), Render(oursText));
}

// CGameAnimationTypeMonsterAnkheg::GetCurrentSequenceAndFrame over a fake vid
// cell.
void AnkhegSequenceAndFrame(CaseContext& ctx)
{
    static const struct {
        int16_t sequence;
        int16_t frame;
    } kPairs[] = {
        { 0, 0 },
        { 5, 7 },
        { -1, 32767 },
        { 1, 2 },
    };
    for (const auto& pair : kPairs) {
        CheckSequenceAndFrame(ctx, pair.sequence, pair.frame);
    }
}

// CGameAnimationTypeCharacterOld::GetSndArmor over every armor-code branch
// with three generator seeds.
void CharacterOldSndArmor(CaseContext& ctx)
{
    static const int kCodes[] = { '1', '2', '3', '4', 0 };
    for (const int code : kCodes) {
        for (const unsigned int seed : { 0u, 1u, 42u }) {
            CheckSndArmor(ctx, code, seed);
        }
    }
}

// CGameAnimationTypeFlying::CanLieDown: the original's fold against the
// inherited base ours answers with.
void FlyerCanLieDown(CaseContext& ctx)
{
    ctx.SetInputs("a zeroed CGameAnimationTypeFlying");

    uint8_t originalRet = 0;
    ctx.CallOriginal([&] {
        auto* self = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(8));
        std::memset(self, 0, 8);
        originalRet = reinterpret_cast<BoolFn>(kCanLieDown)(self, nullptr);
    });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameAnimationTypeFlying)));
    std::memset(storage, 0, sizeof(CGameAnimationTypeFlying));
    const uint8_t oursRet = reinterpret_cast<CGameAnimationTypeFlying*>(storage)->CGameAnimationType::CanLieDown();

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalRet, oursRet);
}

} // namespace

ORACLE_CASE("anim_types-6C2D70", AnkhegSequenceAndFrame);
ORACLE_CASE("anim_types-6DD5B0", CharacterOldSndArmor);
ORACLE_CASE("anim_types-6A3C60", FlyerCanLieDown);
