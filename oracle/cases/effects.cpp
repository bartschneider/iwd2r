// CGameEffectDeath::Copy (0x452E70), the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// The original builds an ITEM_EFFECT through GetItemEffect (a plain call,
// 0x4A4B00), constructs a 0x190-byte copy through the base ctor 0x48C310,
// stamps its dtor vtable, copies field_18C and replays the base members
// through CopyFromBase (0x48C670); ours is the same port. The only virtual
// dispatched on the source is UsesDice (slot 7, byte offset 0x1C), so both
// sides get one shared fake vtable whose slot 7 returns FALSE: both take
// GetItemEffect's minLevel/maxLevel branch. The source is a zeroed
// 0x190-byte block with the same member bytes on both sides. Compared
// dword by dword: the CopyFromBase-written base range [0x04,0xD0), the
// ctor-written range [0x10C,0x124) and the death tail [0x188,0x190).
// +0x00 is the (different, real) vtable, [0xD0,0x10C) is unwritten m_pad
// and [0x124,0x188) an untouched CSound: heap garbage on both sides.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CGameEffect.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CGameEffectDeath::Copy (0x452E70): __thiscall, no arguments, the copy in
// eax.
using CopyFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kCopy = 0x452E70;

constexpr size_t kEffectSize = 0x190; // sizeof(CGameEffectDeath)

// The neutral stub every fake-vtable slot but UsesDice points at; it must
// return 0 so a BOOL virtual would be deterministic if one ever fired.
uintptr_t __fastcall NeutralStub(const void*, void*)
{
    return 0;
}

uint8_t __fastcall FalseStub(const void*, void*)
{
    return FALSE;
}

void* g_sourceVtable[0x10];

void* SourceVtable()
{
    if (g_sourceVtable[0] == 0) {
        for (void*& slot : g_sourceVtable) {
            slot = reinterpret_cast<void*>(&NeutralStub);
        }
        g_sourceVtable[7] = reinterpret_cast<void*>(&FalseStub); // UsesDice
    }
    return g_sourceVtable;
}

// The source members the copy path reads, at CGameEffectDeath object
// offsets (the CGameEffectBase subobject starts at +4).
struct EffectInput {
    uint32_t effectID;
    uint32_t targetType;
    uint32_t spellLevel;
    int32_t effectAmount;
    uint32_t dwFlags;
    uint32_t durationType;
    int32_t duration;
    uint16_t probabilityUpper;
    uint16_t probabilityLower;
    const char* res;
    uint32_t numDice;
    uint32_t diceSize;
    uint32_t savingThrow;
    int32_t saveMod;
    uint32_t special;
    uint32_t school;
    int32_t field48;
    uint32_t minLevel;
    uint32_t maxLevel;
    uint32_t flags;
    int32_t amounts[4];
    const char* res2;
    const char* res3;
    int32_t source[2];
    int32_t target[2];
    uint32_t sourceType;
    const char* sourceRes;
    uint32_t sourceFlags;
    uint32_t projectileType;
    int32_t slotNum;
    const char* scriptName;
    uint32_t casterLevel;
    uint32_t spellPenetrationBonus;
    uint32_t secondaryType;
    int32_t sourceID;
    int32_t field18C;
};

void PutResRef(uint8_t* at, const char* text)
{
    std::memset(at, 0, 8);
    if (text != nullptr) {
        const size_t length = std::strlen(text);
        std::memcpy(at, text, length < 8 ? length : 8);
    }
}

void FillSource(uint8_t* at, const EffectInput& in, void* vtable)
{
    std::memset(at, 0, kEffectSize);
    std::memcpy(at, &vtable, 4);
    std::memcpy(at + 0x0C, &in.effectID, 4);
    std::memcpy(at + 0x10, &in.targetType, 4);
    std::memcpy(at + 0x14, &in.spellLevel, 4);
    std::memcpy(at + 0x18, &in.effectAmount, 4);
    std::memcpy(at + 0x1C, &in.dwFlags, 4);
    std::memcpy(at + 0x20, &in.durationType, 4);
    std::memcpy(at + 0x24, &in.duration, 4);
    std::memcpy(at + 0x28, &in.probabilityUpper, 2);
    std::memcpy(at + 0x2A, &in.probabilityLower, 2);
    PutResRef(at + 0x2C, in.res);
    std::memcpy(at + 0x34, &in.numDice, 4);
    std::memcpy(at + 0x38, &in.diceSize, 4);
    std::memcpy(at + 0x3C, &in.savingThrow, 4);
    std::memcpy(at + 0x40, &in.saveMod, 4);
    std::memcpy(at + 0x44, &in.special, 4);
    std::memcpy(at + 0x48, &in.school, 4);
    std::memcpy(at + 0x4C, &in.field48, 4);
    std::memcpy(at + 0x50, &in.minLevel, 4);
    std::memcpy(at + 0x54, &in.maxLevel, 4);
    std::memcpy(at + 0x58, &in.flags, 4);
    std::memcpy(at + 0x5C, in.amounts, 16);
    PutResRef(at + 0x6C, in.res2);
    PutResRef(at + 0x74, in.res3);
    std::memcpy(at + 0x7C, in.source, 8);
    std::memcpy(at + 0x84, in.target, 8);
    std::memcpy(at + 0x8C, &in.sourceType, 4);
    PutResRef(at + 0x90, in.sourceRes);
    std::memcpy(at + 0x98, &in.sourceFlags, 4);
    std::memcpy(at + 0x9C, &in.projectileType, 4);
    std::memcpy(at + 0xA0, &in.slotNum, 4);
    std::memset(at + 0xA4, 0, 32);
    if (in.scriptName != nullptr) {
        const size_t length = std::strlen(in.scriptName);
        std::memcpy(at + 0xA4, in.scriptName, length < 32 ? length : 32);
    }
    std::memcpy(at + 0xC4, &in.casterLevel, 4);
    std::memcpy(at + 0xC8, &in.spellPenetrationBonus, 4);
    std::memcpy(at + 0xCC, &in.secondaryType, 4);
    std::memcpy(at + 0x10C, &in.sourceID, 4);
    std::memcpy(at + 0x18C, &in.field18C, 4);
}

// One dwords-of-the-copy comparison over a written range.
void CompareRange(CaseContext& ctx, const uint8_t* ours, const uint8_t* original, size_t from, size_t to, const char* what)
{
    const std::string inputs = ctx.Inputs();
    for (size_t at = from; at < to; at += 4) {
        uint32_t oursDword = 0;
        uint32_t originalDword = 0;
        std::memcpy(&oursDword, ours + at, 4);
        std::memcpy(&originalDword, original + at, 4);
        ctx.SetInputs(inputs + ", " + what + " dword at byte " + std::to_string(at));
        oracle::Compare(ctx, originalDword, oursDword);
    }
    ctx.SetInputs(inputs);
}

void CheckCopy(CaseContext& ctx, const char* name, const EffectInput& in)
{
    ctx.SetInputs(name);
    void* vtable = SourceVtable();

    uint8_t oursSource[kEffectSize];
    FillSource(oursSource, in, vtable);
    auto* oursEffect = reinterpret_cast<CGameEffectDeath*>(oursSource);
    CGameEffect* oursCopy = oursEffect->CGameEffectDeath::Copy();

    void* originalCopy = nullptr;
    ctx.CallOriginal([&] {
        auto* self = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kEffectSize));
        FillSource(self, in, vtable);
        originalCopy = reinterpret_cast<CopyFn>(kCopy)(self, nullptr);
    });

    ctx.CountInputs(1);
    const auto* oursBytes = reinterpret_cast<const uint8_t*>(oursCopy);
    const auto* originalBytes = static_cast<const uint8_t*>(originalCopy);
    if (!oracle::Compare(ctx, originalCopy != nullptr, oursCopy != nullptr)) {
        return;
    }
    CompareRange(ctx, oursBytes, originalBytes, 0x04, 0xD0, "the base members");
    CompareRange(ctx, oursBytes, originalBytes, 0x10C, 0x124, "the ctor members");
    CompareRange(ctx, oursBytes, originalBytes, 0x188, 0x190, "the death tail");
}

// Copy over a zeroed effect, a populated one and a negative-values one.
void EffectDeathCopy(CaseContext& ctx)
{
    EffectInput zeroed = {};
    zeroed.res = "";
    zeroed.res2 = "";
    zeroed.res3 = "";
    zeroed.sourceRes = "";
    zeroed.scriptName = "";
    CheckCopy(ctx, "a zeroed effect", zeroed);

    EffectInput populated = {};
    populated.effectID = 13; // CGAMEEFFECT_DEATH
    populated.targetType = 2;
    populated.spellLevel = 5;
    populated.effectAmount = 777;
    populated.dwFlags = 0x4;
    populated.durationType = 9;
    populated.duration = 1234;
    populated.probabilityUpper = 100;
    populated.probabilityLower = 1;
    populated.res = "SPWI123";
    populated.numDice = 2;
    populated.diceSize = 6;
    populated.savingThrow = 0x11;
    populated.saveMod = -2;
    populated.special = 0x99;
    populated.school = 4;
    populated.field48 = 8;
    populated.minLevel = 3;
    populated.maxLevel = 9;
    populated.flags = 0x3;
    populated.amounts[0] = 11;
    populated.amounts[1] = 12;
    populated.amounts[2] = 13;
    populated.amounts[3] = 14;
    populated.res2 = "RES2TEST";
    populated.res3 = "RES3TEST";
    populated.source[0] = 10;
    populated.source[1] = 20;
    populated.target[0] = 30;
    populated.target[1] = 40;
    populated.sourceType = 1;
    populated.sourceRes = "SRCRES01";
    populated.sourceFlags = 0x55;
    populated.projectileType = 6;
    populated.slotNum = 2;
    populated.scriptName = "deathscr";
    populated.casterLevel = 7;
    populated.spellPenetrationBonus = 2;
    populated.secondaryType = 3;
    populated.sourceID = 0x1234;
    populated.field18C = 5;
    CheckCopy(ctx, "a populated effect", populated);

    EffectInput negative = zeroed;
    negative.effectAmount = -5;
    negative.duration = -1;
    negative.saveMod = -3;
    negative.field48 = -4;
    negative.amounts[0] = -6;
    negative.amounts[1] = -7;
    negative.source[0] = -1;
    negative.source[1] = -2;
    negative.target[0] = -3;
    negative.target[1] = -4;
    negative.slotNum = -8;
    negative.probabilityUpper = 100;
    negative.probabilityLower = 100;
    negative.flags = 0x2;
    negative.sourceID = -1;
    negative.field18C = -1;
    CheckCopy(ctx, "a negative effect", negative);
}

} // namespace

ORACLE_CASE("effects-452E70", EffectDeathCopy);
