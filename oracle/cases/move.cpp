// Movement and targeting helpers, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CAIGroup::HandleFollowPath (0x409820) works only on sprites it reaches
// through the original's object array (CGameObjectArray::GetDeny on
// g_pBaldurChitin->m_pObjectGame + 0x372C) and their virtual AddAction and
// InsertAction; with no member granted it does nothing. The original's
// sprites can only come from its CGameSprite constructor (0x6EF990), which
// needs the resource manager and a loaded creature file (R10a), so the case
// is pending. Our side is covered by the Catch2 test "[move]
// HandleFollowPath" on the started engine (tests/test_move.cpp).
//
// CGameSprite::IsVisibleToCaller (0x727B80) reads only the derived general
// state (0x920) and the creature header's byte at 0x8A0 (R10b): a zeroed
// block from the original's operator new with those two set, against zeroed
// CGameSprite storage of ours with the same members set. __thiscall with one
// argument, `ret 4` at 0x727BA0, called through a __fastcall pointer whose
// edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CGameSprite.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace {

using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

using IsVisibleFn = int32_t(__fastcall*)(const void* self, void* edx, int32_t bSeeInvisible);
constexpr uintptr_t kIsVisibleToCaller = 0x727B80;
constexpr size_t kGeneralStateOffset = 0x920;
constexpr size_t kField2FCOffset = 0x8A0;
constexpr size_t kSpriteBlockSize = 0x924;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

// Every combination of the three inputs' interesting values: seeInvisible
// 0, 1 and 2; the general state with and without STATE_INVISIBLE (alone and
// with other bits); field_2FC with and without its low bit.
void IsVisibleToCaller(CaseContext& ctx)
{
    uint8_t* original = nullptr;
    ctx.SetInputs("allocating the original's sprite block");
    ctx.CallOriginal([&] { original = OriginalBlock(kSpriteBlockSize); });

    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameSprite)));
    std::memset(storage, 0, sizeof(CGameSprite));
    auto* ours = reinterpret_cast<CGameSprite*>(storage);

    const int32_t sees[] = { 0, 1, 2 };
    const uint32_t states[] = { 0, STATE_INVISIBLE, STATE_INVISIBLE | STATE_DEAD, STATE_DEAD, 0xFFFFFFFFu,
        0xFFFFFFFFu & ~static_cast<uint32_t>(STATE_INVISIBLE) };
    const uint8_t fields[] = { 0, 1, 2, 3, 0xFE, 0xFF };

    for (int32_t see : sees) {
        for (uint32_t state : states) {
            for (uint8_t field : fields) {
                ctx.SetInputs("IsVisibleToCaller(" + std::to_string(see) + ") with general state "
                    + std::to_string(state) + " and field_2FC " + std::to_string(field));
                int32_t originalResult = 0;
                ctx.CallOriginal([&] {
                    std::memcpy(original + kGeneralStateOffset, &state, 4);
                    original[kField2FCOffset] = field;
                    originalResult = reinterpret_cast<IsVisibleFn>(kIsVisibleToCaller)(original, nullptr, see);
                });
                ours->m_derivedStats.m_generalState = state;
                ours->m_baseStats.field_2FC = field;
                const BOOL oursResult = ours->IsVisibleToCaller(see);
                ctx.CountInputs(1);
                oracle::Compare(ctx, originalResult, oursResult);
            }
        }
    }
}

} // namespace

ORACLE_CASE("move/visible_to_caller", IsVisibleToCaller);
ORACLE_CASE_PENDING("move/follow_path", nullptr,
    "needs engine state (sprites from the original's CGameSprite constructor in its object array, the "
    "message handler); ours covered by tests/test_move.cpp");
