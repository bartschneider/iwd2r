// Save-game helpers (CVariableHash, CInfButtonArray, Icewind586B70), the
// original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. Each original object is a zeroed block from the original's
// operator new with only the members the function reads filled in; original
// blocks, and what the original allocates for its results, are never freed.
// None of these functions reads a global.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CGameJournal.h"
#include "CInfButtonArray.h"
#include "CVariableHash.h"
#include "Icewind586B70.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CVariableHash::Marshal(CVariable** pList, WORD* pCount) (0x5503A0):
// `ret 8` at 0x550594. Reads m_nTableEntries (this + 4) and m_hashEntries
// (this + 8), an array of 0x54-byte CVariables; the vtable is not read. The
// list it returns comes from the original's operator new.
using VariableMarshalFn = void(__fastcall*)(void* self, void* edx, uint8_t** pList, uint16_t* pCount);
constexpr uintptr_t kVariableMarshal = 0x5503A0;
// The original CVariable: m_name 0x00 (32 bytes), m_type 0x20,
// m_resRefType 0x22, m_dwValue 0x24, m_intValue 0x28, m_floatValue 0x2C,
// m_stringValue 0x34.
constexpr size_t kVariableSize = 0x54;

// CInfButtonArray::UpdateCustomizableButtonTypes(INT* pButtonList)
// (0x595E70, `ret 4` at 0x595EA3) writes nine INTs at this + 0x19B6;
// UpdateSelectedWeaponSet(BYTE nIndex) (0x595F70, `ret 4` at 0x595F9E)
// writes the byte at this + 0x19E8. An index above 3 is asserted, so it is
// not an input.
using ButtonTypesFn = void(__fastcall*)(void* self, void* edx, const int32_t* pButtonList);
constexpr uintptr_t kButtonTypes = 0x595E70;
using WeaponSetFn = void(__fastcall*)(void* self, void* edx, uint32_t nIndex);
constexpr uintptr_t kWeaponSet = 0x595F70;
constexpr size_t kButtonTypesOffset = 0x19B6;
constexpr size_t kWeaponSetOffset = 0x19E8;
constexpr size_t kButtonArraySize = 0x1A00;

// Icewind586B70::sub_587510 (0x587510): returns a std::vector<int> through
// the hidden pointer pushed by the caller (SaveGame, 0x5AC76B), `ret 4` at
// 0x587603. Reads the VC6 vector at this + 4 (first) and this + 8 (last) of
// 10-byte entries: two INT ids and a flag. The result's first and last are at
// +4 and +8.
using LimitIdsFn = void*(__fastcall*)(void* self, void* edx, void* result);
constexpr uintptr_t kLimitIds = 0x587510;
constexpr size_t kLimitEntrySize = 10;
constexpr int kLimitEntries = 6;

// CGameJournal (0x14 bytes: a CPtrArray of seven CPtrLists). The
// constructor (0x4C6200, plain `ret`) builds the lists through MFC's
// originals; Unmarshal(CSavedGameJournalEntry*, DWORD) (0x4C6CD0, `ret 8` at
// 0x4C6D86) adds an entry per 12-byte record to the list of its chapter;
// Marshal(CSavedGameJournalEntry*) (0x4C6BF0, `ret 4` at 0x4C6CC6) writes the
// entries back, chapter by chapter. None reads a global.
using JournalConstructFn = void*(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kJournalConstruct = 0x4C6200;
using JournalUnmarshalFn = void(__fastcall*)(void* self, void* edx, const uint8_t* pSaved, uint32_t nSaved);
constexpr uintptr_t kJournalUnmarshal = 0x4C6CD0;
using JournalMarshalFn = void(__fastcall*)(void* self, void* edx, uint8_t* pSaved);
constexpr uintptr_t kJournalMarshal = 0x4C6BF0;
constexpr size_t kJournalSize = 0x14;
constexpr size_t kJournalEntrySize = 12;

void* OriginalNew(size_t size)
{
    return reinterpret_cast<OriginalNewFn>(kOriginalNew)(size);
}

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(OriginalNew(size));
    std::memset(at, 0, size);
    return at;
}

uint32_t Address(const void* p)
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

struct Variable {
    std::string name;
    uint16_t type;
    uint32_t dwValue;
    int32_t intValue;
};

// Random tables: a size from 1 to 40, each slot filled with a probability
// of one in three.
std::vector<std::vector<Variable>> VariableTables(uint64_t seed, int count)
{
    std::mt19937 rng(static_cast<uint32_t>(seed));
    std::vector<std::vector<Variable>> tables;
    for (int t = 0; t < count; ++t) {
        std::vector<Variable> table(1 + rng() % 40);
        int n = 0;
        for (Variable& v : table) {
            if (rng() % 3 == 0) {
                v.name = "VAR_" + std::to_string(t) + "_" + std::to_string(n++);
                v.type = static_cast<uint16_t>(rng() % 6);
                v.dwValue = static_cast<uint32_t>(rng());
                v.intValue = static_cast<int32_t>(rng());
            } else {
                v.type = 0;
                v.dwValue = 0;
                v.intValue = 0;
            }
        }
        tables.push_back(table);
    }
    return tables;
}

std::string Describe(const std::vector<Variable>& table)
{
    std::string text = "table of " + std::to_string(table.size()) + ": [";
    for (size_t i = 0; i < table.size(); ++i) {
        text += (i ? " " : "") + (table[i].name.empty() ? std::string("-") : table[i].name);
    }
    return text + "]";
}

void VariableHashMarshal(CaseContext& ctx)
{
    for (const std::vector<Variable>& table : VariableTables(ctx.seed, 40)) {
        ctx.SetInputs(Describe(table));
        const size_t size = table.size();

        std::vector<Variable> original;
        uint16_t originalCount = 0;
        ctx.CallOriginal([&] {
            uint8_t* self = OriginalBlock(0xC);
            uint8_t* entries = OriginalBlock(size * kVariableSize);
            for (size_t i = 0; i < size; ++i) {
                uint8_t* at = entries + i * kVariableSize;
                std::strncpy(reinterpret_cast<char*>(at), table[i].name.c_str(), 32);
                std::memcpy(at + 0x20, &table[i].type, 2);
                std::memcpy(at + 0x24, &table[i].dwValue, 4);
                std::memcpy(at + 0x28, &table[i].intValue, 4);
            }
            const uint32_t nEntries = static_cast<uint32_t>(size);
            const uint32_t pEntries = Address(entries);
            std::memcpy(self + 4, &nEntries, 4);
            std::memcpy(self + 8, &pEntries, 4);
            uint8_t* list = nullptr;
            reinterpret_cast<VariableMarshalFn>(kVariableMarshal)(self, nullptr, &list, &originalCount);
            for (uint16_t i = 0; i < originalCount; ++i) {
                const uint8_t* at = list + i * kVariableSize;
                Variable v;
                v.name = std::string(reinterpret_cast<const char*>(at), strnlen(reinterpret_cast<const char*>(at), 32));
                std::memcpy(&v.type, at + 0x20, 2);
                std::memcpy(&v.dwValue, at + 0x24, 4);
                std::memcpy(&v.intValue, at + 0x28, 4);
                original.push_back(v);
            }
        });

        CVariableHash hash(static_cast<LONG>(size));
        for (size_t i = 0; i < size; ++i) {
            CVariable& var = hash.m_hashEntries[i];
            std::memset(var.m_name, 0, sizeof(var.m_name));
            std::strncpy(var.m_name, table[i].name.c_str(), 32);
            var.m_type = table[i].type;
            var.m_dwValue = table[i].dwValue;
            var.m_intValue = table[i].intValue;
        }
        CVariable* pList = nullptr;
        WORD oursCount = 0;
        hash.Marshal(&pList, &oursCount);

        ctx.CountInputs(1);
        const std::string inputs = ctx.Inputs();
        if (oracle::Compare(ctx, originalCount, oursCount)) {
            for (uint16_t i = 0; i < oursCount; ++i) {
                ctx.SetInputs(inputs + ", slot " + std::to_string(i));
                const CVariable& ours = pList[i];
                oracle::Compare(ctx, original[i].name,
                    std::string(ours.m_name, strnlen(ours.m_name, 32)));
                oracle::Compare(ctx, original[i].type, ours.m_type);
                oracle::Compare(ctx, original[i].dwValue, ours.m_dwValue);
                oracle::Compare(ctx, original[i].intValue, ours.m_intValue);
            }
        }
        delete[] pList;
    }
}

void ButtonArrayUpdates(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed));
    for (int iteration = 0; iteration < 20; ++iteration) {
        int32_t types[9];
        for (int32_t& type : types) {
            type = static_cast<int32_t>(rng());
        }
        const uint32_t nIndex = static_cast<uint32_t>(iteration % 4);
        ctx.SetInputs("button types from rand #" + std::to_string(iteration) + ", weapon set "
            + std::to_string(nIndex));

        int32_t originalTypes[9] = {};
        uint8_t originalSet = 0;
        ctx.CallOriginal([&] {
            uint8_t* self = OriginalBlock(kButtonArraySize);
            reinterpret_cast<ButtonTypesFn>(kButtonTypes)(self, nullptr, types);
            reinterpret_cast<WeaponSetFn>(kWeaponSet)(self, nullptr, nIndex);
            std::memcpy(originalTypes, self + kButtonTypesOffset, sizeof(originalTypes));
            originalSet = self[kWeaponSetOffset];
        });

        CInfButtonArray* ours = new CInfButtonArray();
        ours->UpdateCustomizableButtonTypes(types);
        ours->UpdateSelectedWeaponSet(static_cast<BYTE>(nIndex));

        ctx.CountInputs(1);
        for (int i = 0; i < 9; ++i) {
            oracle::Compare(ctx, originalTypes[i], ours->m_customizableButtonTypes[i]);
        }
        oracle::Compare(ctx, originalSet, ours->m_nSelectedWeaponSet);
        delete ours;
    }
}

void LimitIds(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed));
    for (int iteration = 0; iteration < 40; ++iteration) {
        int32_t first[kLimitEntries];
        int32_t second[kLimitEntries];
        std::string text = "entries [";
        for (int i = 0; i < kLimitEntries; ++i) {
            first[i] = rng() % 2 ? static_cast<int32_t>(rng()) : 0;
            second[i] = rng() % 2 ? static_cast<int32_t>(rng()) : 0;
            text += (i ? " " : "") + std::to_string(first[i]) + "/" + std::to_string(second[i]);
        }
        ctx.SetInputs(text + "]");

        std::vector<int32_t> original;
        ctx.CallOriginal([&] {
            uint8_t* self = OriginalBlock(0x10);
            uint8_t* entries = OriginalBlock(kLimitEntries * kLimitEntrySize);
            for (int i = 0; i < kLimitEntries; ++i) {
                std::memcpy(entries + i * kLimitEntrySize, &first[i], 4);
                std::memcpy(entries + i * kLimitEntrySize + 4, &second[i], 4);
            }
            const uint32_t begin = Address(entries);
            const uint32_t end = Address(entries + kLimitEntries * kLimitEntrySize);
            std::memcpy(self + 4, &begin, 4);
            std::memcpy(self + 8, &end, 4);
            std::memcpy(self + 0xC, &end, 4);
            uint8_t* result = OriginalBlock(0x10);
            reinterpret_cast<LimitIdsFn>(kLimitIds)(self, nullptr, result);
            uint32_t resultFirst = 0;
            uint32_t resultLast = 0;
            std::memcpy(&resultFirst, result + 4, 4);
            std::memcpy(&resultLast, result + 8, 4);
            for (uint32_t at = resultFirst; at < resultLast; at += 4) {
                int32_t id = 0;
                std::memcpy(&id, reinterpret_cast<const void*>(static_cast<uintptr_t>(at)), 4);
                original.push_back(id);
            }
        });

        Icewind586B70 limits;
        for (int i = 0; i < kLimitEntries; ++i) {
            limits.mEntries[i].field_0 = first[i];
            limits.mEntries[i].field_4 = second[i];
        }
        const std::vector<int> ours = limits.sub_587510();

        ctx.CountInputs(1);
        const std::string inputs = ctx.Inputs();
        if (oracle::Compare(ctx, original.size(), ours.size())) {
            for (size_t i = 0; i < ours.size(); ++i) {
                ctx.SetInputs(inputs + ", id " + std::to_string(i));
                oracle::Compare(ctx, original[i], ours[i]);
            }
        }
    }
}

// Saved journal records: a strref, a time, a chapter from 0 to 6 (the byte
// after it 0: a non-zero one indexes past the chapters in the original), a
// type and a character, 0 one time in three (Unmarshal reads it as 1).
void JournalRoundTrip(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed));
    for (int iteration = 0; iteration < 40; ++iteration) {
        const uint32_t count = rng() % 30;
        std::vector<uint8_t> saved(count * kJournalEntrySize);
        for (uint32_t i = 0; i < count; ++i) {
            uint8_t* at = saved.data() + i * kJournalEntrySize;
            const uint32_t strref = rng();
            const uint32_t time = rng();
            std::memcpy(at, &strref, 4);
            std::memcpy(at + 4, &time, 4);
            at[8] = static_cast<uint8_t>(rng() % 7);
            at[9] = 0;
            at[10] = static_cast<uint8_t>(rng());
            at[11] = rng() % 3 == 0 ? 0 : static_cast<uint8_t>(rng());
        }
        ctx.SetInputs(std::to_string(count) + " journal records from rand #" + std::to_string(iteration));

        std::vector<uint8_t> original(count * kJournalEntrySize, 0xCD);
        ctx.CallOriginal([&] {
            uint8_t* self = OriginalBlock(kJournalSize);
            reinterpret_cast<JournalConstructFn>(kJournalConstruct)(self, nullptr);
            uint8_t* input = OriginalBlock(saved.size() + 1);
            std::memcpy(input, saved.data(), saved.size());
            reinterpret_cast<JournalUnmarshalFn>(kJournalUnmarshal)(self, nullptr, input, count);
            uint8_t* output = OriginalBlock(original.size() + 1);
            std::memset(output, 0xCD, original.size());
            reinterpret_cast<JournalMarshalFn>(kJournalMarshal)(self, nullptr, output);
            std::memcpy(original.data(), output, original.size());
        });

        std::vector<uint8_t> ours(count * kJournalEntrySize + 1, 0xCD);
        {
            CGameJournal journal;
            journal.Unmarshal(reinterpret_cast<CSavedGameJournalEntry*>(saved.data()), count);
            journal.Marshal(reinterpret_cast<CSavedGameJournalEntry*>(ours.data()));
        }

        ctx.CountInputs(1);
        const std::string inputs = ctx.Inputs();
        for (uint32_t i = 0; i < count; ++i) {
            ctx.SetInputs(inputs + ", record " + std::to_string(i));
            for (size_t b = 0; b < kJournalEntrySize; ++b) {
                oracle::Compare(ctx, original[i * kJournalEntrySize + b], ours[i * kJournalEntrySize + b]);
            }
        }
    }
}

}

ORACLE_CASE("save/variable_hash_marshal", VariableHashMarshal);
ORACLE_CASE("save/journal_round_trip", JournalRoundTrip);
ORACLE_CASE("save/button_array_updates", ButtonArrayUpdates);
ORACLE_CASE("save/limit_ids", LimitIds);
