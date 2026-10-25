// The sprite spell list's search, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CGameSpriteSpellList::Find (0x725E70) walks the id array (begin at +4,
// end at +8, stride 0x10: CGameSpriteSpellListEntry::m_nID is the entry's
// first dword) comparing each entry with *pnID, counting the entries it
// walked past into *pnIndex. __thiscall with two pointer arguments,
// `ret 8`, BOOLEAN in al. The original's list is a zeroed block from its
// operator new with only the two array bounds set over a second block of
// entries; ours is placement-new storage the case fills through the ported
// vector. The boolean and the index it leaves are compared.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CGameSpriteSpells.h"
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

// CGameSpriteSpellList::Find(const UINT& nID, UINT& nIndex) (0x725E70):
// __thiscall, both arguments by reference, `ret 8`, BOOLEAN in al.
using FindFn = uint8_t(__fastcall*)(const void* self, void* edx, const uint32_t* nID, uint32_t* nIndex);
constexpr uintptr_t kFind = 0x725E70;

constexpr size_t kEntrySize = 0x10; // CGameSpriteSpellListEntry

// One list of ids and one query on each side: the boolean and the index.
void CheckFind(CaseContext& ctx, CGameSpriteSpellList* ours, const uint32_t* ids, int count, uint32_t query)
{
    std::string text = "ids [";
    for (int i = 0; i < count; ++i) {
        text += (i ? " " : "") + std::to_string(ids[i]);
    }
    ctx.SetInputs(text + "], Find(" + std::to_string(query) + ")");

    uint8_t originalFound = 0;
    uint32_t originalIndex = 0;
    ctx.CallOriginal([&] {
        auto* self = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(0x1C));
        std::memset(self, 0, 0x1C);
        auto* entries = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kEntrySize * count));
        std::memset(entries, 0, kEntrySize * count);
        for (int i = 0; i < count; ++i) {
            std::memcpy(entries + i * kEntrySize, &ids[i], 4);
        }
        const uint32_t begin = reinterpret_cast<uint32_t>(entries);
        const uint32_t end = begin + static_cast<uint32_t>(kEntrySize * count);
        std::memcpy(self + 4, &begin, 4);
        std::memcpy(self + 8, &end, 4);
        originalFound = reinterpret_cast<FindFn>(kFind)(self, nullptr, &query, &originalIndex);
    });

    ours->m_List.clear();
    for (int i = 0; i < count; ++i) {
        CGameSpriteSpellListEntry entry = {};
        entry.m_nID = ids[i];
        ours->m_List.push_back(entry);
    }
    uint32_t oursIndex = 0;
    const uint8_t oursFound = ours->Find(query, oursIndex);

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalFound, oursFound);
    const std::string inputs = ctx.Inputs();
    ctx.SetInputs(inputs + ", the index left behind");
    oracle::Compare(ctx, originalIndex, oursIndex);
    ctx.SetInputs(inputs);
}

// CGameSpriteSpellList::Find over empty, one- and three-entry lists with the
// query at the first, middle and last entry and past them.
void SpellListFind(CaseContext& ctx)
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameSpriteSpellList)));
    std::memset(storage, 0, sizeof(CGameSpriteSpellList));
    auto* ours = new (storage) CGameSpriteSpellList();

    static const uint32_t kOne[] = { 100 };
    static const uint32_t kThree[] = { 100, 200, 300 };
    CheckFind(ctx, ours, kOne, 0, 999);
    for (const uint32_t query : { 100u, 999u }) {
        CheckFind(ctx, ours, kOne, 1, query);
    }
    for (const uint32_t query : { 100u, 200u, 300u, 999u }) {
        CheckFind(ctx, ours, kThree, 3, query);
    }
}

} // namespace

ORACLE_CASE("spell_list-725E70", SpellListFind);
