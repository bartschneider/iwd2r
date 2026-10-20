// CScreenStore::CheckSpellUsable (0x67E560), the original's against ours.
// Includes engine headers, so not <windows.h> (see spawn.cpp). Rule-2 probe
// for WSO row 0x67E560. The decompile reads no `this` (ecx is pushed and
// never loaded): both sides run with this = 1, so any member touch faults
// loudly. It reads only g_pBaldurChitin (0x8CF6DC, the original's .data,
// restored between cases) and the fixture reachable from it (chitin+0x1C54
// -> pGame, portraits/count/array, sprite state/race/effect lists, each
// effect's byte at +0x58 = ours' m_effectAmount2, the bit the WSO delta
// turned on): zeroed blocks from the original's operator new, never freed.
// SPPR307's GetFlagsFile arm needs the resource manager and is not
// exercised (its input walks an empty equipment array); compared normalized
// (nonzero), the BOOL both sides return.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CBaldurChitin.h"
#include "CGameEffect.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CResRef.h"
#include "CScreenStore.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "oracle/runner/registry.h"
#include <cstdint>
#include <cstring>

namespace {
// operator new (0x7FC95B): __cdecl, plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;
// CScreenStore::CheckSpellUsable(short, const CResRef*) (0x67E560), `ret 8`.
using CheckFn = uint32_t(__fastcall*)(void* self, void* edx, int portrait, const void* resref);
constexpr uintptr_t kCheckSpellUsable = 0x67E560;
constexpr uintptr_t kGpBaldurChitin = 0x8CF6DC;
constexpr size_t kChitinSize = 0x2000, kChitinGame = 0x1C54, kGameSize = 0x4200, kGamePortraits = 0x382E;
constexpr size_t kGameCount = 0x3846, kGameArray = 0x372C, kArrayEntries = 0x20;
constexpr size_t kEntryStride = 0xC, kEntryId = 0x6, kEntryObject = 0x8, kArrayNextId = 0x2A;
constexpr size_t kSpriteSize = 0x5540, kSpriteState = 0x920, kSpriteRace = 0x3E00, kSpriteEquipped = 0x54FE;
constexpr size_t kSpriteTimed = 0x552A, kNodeSize = 0xC, kNodeData = 0x8, kEffectSize = 0x60;
constexpr size_t kEffectFlags = 0x54, kEffectAmount2 = 0x58, kCharacterId = 1;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}
uint32_t Ptr32(const void* at) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(at)); }
void Write(uint8_t* at, size_t offset, uint32_t value) { std::memcpy(at + offset, &value, 4); }
void Write16(uint8_t* at, size_t offset, uint16_t value) { std::memcpy(at + offset, &value, 2); }

struct Input {
    const char* resref;
    short portrait;
    uint32_t state, flags;
    int32_t amount2;
    int list;
    uint8_t race; // list 0 none / 1 timed / 2 equipped; race 2 elf
};
// Decisive pair first (bit 0 of the effect byte at +0x58 set with m_flags
// clear, then the inverse), then the arm matrix: lists, dead, the not-dead
// arms, SPPR712's raw 0x800 and default, SPPR504's dead gate and elf
// override, SPPR314/404/308's masks, the SPPR307 empty walk, a bad portrait.
const Input kInputs[] = {
    { "SPPR303", 0, 0x0, 0x0, 1, 1, 0 },
    { "SPPR303", 0, 0x0, 0x1, 0, 1, 0 },
    { "SPPR303", 0, 0x0, 0x0, 2, 1, 0 },
    { "SPPR303", 0, 0x0, 0x0, 1, 2, 0 },
    { "SPPR303", 0, 0x800, 0x0, 1, 1, 0 },
    { "SPPR101", 0, 0x0, 0x0, 0, 0, 0 },
    { "SPPR712", 0, 0x800, 0x0, 0, 0, 0 },
    { "SPPR712", 0, 0x0, 0x0, 0, 0, 0 },
    { "SPPR504", 0, 0x0, 0x0, 0, 0, 0 },
    { "SPPR504", 0, 0x800, 0x0, 0, 0, 0 },
    { "SPPR504", 0, 0x800, 0x0, 0, 0, 2 },
    { "SPPR314", 0, 0x80000, 0x0, 0, 0, 0 },
    { "SPPR404", 0, 0x4000, 0x0, 0, 0, 0 },
    { "SPPR308", 0, 0x8, 0x0, 0, 0, 0 },
    { "SPPR307", 0, 0x0, 0x0, 0, 0, 0 },
    { "SPPR101", 5, 0x0, 0x0, 0, 0, 0 },
};
// The g_pBaldurChitin write lands in the original's .data, restored between cases.
uint32_t OriginalRun(const Input& in)
{
    uint8_t* chitin = OriginalBlock(kChitinSize);
    uint8_t* game = OriginalBlock(kGameSize);
    uint8_t* array = game + kGameArray;
    uint8_t* entries = OriginalBlock(kEntryStride);
    uint8_t* sprite = OriginalBlock(kSpriteSize);
    Write(chitin, kChitinGame, Ptr32(game));
    Write(game, kGameArray, Ptr32(array));
    Write16(game, kGameCount, 1);
    Write(game, kGamePortraits, kCharacterId);
    Write(array, kArrayEntries, Ptr32(entries));
    Write16(array, kArrayNextId, 2);
    Write16(entries, kEntryId, static_cast<uint16_t>(kCharacterId));
    Write(entries, kEntryObject, Ptr32(sprite));
    Write(sprite, kSpriteState, in.state);
    sprite[kSpriteRace] = in.race;
    if (in.list != 0) {
        uint8_t *node = OriginalBlock(kNodeSize), *effect = OriginalBlock(kEffectSize);
        Write(effect, kEffectFlags, in.flags);
        Write(effect, kEffectAmount2, static_cast<uint32_t>(in.amount2));
        Write(node, kNodeData, Ptr32(effect));
        Write(sprite, in.list == 1 ? kSpriteTimed : kSpriteEquipped, Ptr32(node));
    }
    const uint32_t chitinPtr = Ptr32(chitin);
    std::memcpy(reinterpret_cast<void*>(kGpBaldurChitin), &chitinPtr, 4);
    char resref[9] = {};
    std::memcpy(resref, in.resref, std::strlen(in.resref));
    return reinterpret_cast<CheckFn>(kCheckSpellUsable)(
        reinterpret_cast<void*>(static_cast<uintptr_t>(1)), nullptr, in.portrait, resref);
}
// Ours: the same fixture through our headers; everything public, everything leaked.
uint32_t OursRun(const Input& in)
{
    auto* chitin = reinterpret_cast<CBaldurChitin*>(OriginalBlock(sizeof(CBaldurChitin)));
    auto* game = reinterpret_cast<CInfGame*>(OriginalBlock(sizeof(CInfGame)));
    auto* sprite = reinterpret_cast<CGameSprite*>(OriginalBlock(sizeof(CGameSprite)));
    game->m_nCharacters = 1;
    game->m_characterPortraits[0] = kCharacterId;
    CGameObjectArrayEntry entries[1];
    entries[0].m_objectId = static_cast<SHORT>(kCharacterId);
    entries[0].m_objectPtr = sprite;
    CGameObjectArray* array = game->GetObjectArray();
    array->m_pArray = entries;
    array->m_nNextObjectId = 2;
    sprite->m_derivedStats.m_generalState = in.state;
    sprite->m_liveTypeAI.m_nRace = in.race;
    if (in.list != 0) {
        auto* effect = reinterpret_cast<CGameEffect*>(OriginalBlock(sizeof(CGameEffect)));
        effect->m_effectAmount2 = in.amount2;
        effect->m_flags = in.flags;
        (in.list == 1 ? sprite->m_timedEffectList : sprite->m_equipedEffectList).AddHead(effect);
    }
    chitin->m_pObjectGame = game;
    g_pBaldurChitin = chitin;
    auto* store = reinterpret_cast<CScreenStore*>(static_cast<uintptr_t>(1));
    const uint32_t result = store->CheckSpellUsable(in.portrait, CResRef(in.resref));
    g_pBaldurChitin = nullptr;
    return result;
}
void CheckSpellUsableCase(CaseContext& ctx)
{
    for (size_t i = 0; i < sizeof(kInputs) / sizeof(kInputs[0]); i++) {
        ctx.SetInputs("#" + std::to_string(i) + " " + kInputs[i].resref);
        uint32_t original = 0;
        ctx.CallOriginal([&] { original = OriginalRun(kInputs[i]); });
        const uint32_t ours = OursRun(kInputs[i]);
        ctx.CountInputs(1);
        if ((original != 0) != (ours != 0)) {
            ctx.Fail(ctx.Inputs(), std::to_string(original != 0), std::to_string(ours != 0));
        }
    }
}
} // namespace
ORACLE_CASE("store/check_spell_usable", CheckSpellUsableCase);
