// The rule tables' druid shapeshift count, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CRuleTables::GetMaxDruidShapeshifts (0x543770), __thiscall
// `ret 8`: GetMaxDruidShapeshifts(CCreatureFileHeader& bstats, INT nLevel).
// It reads bstats.m_druidLevel (a byte at +0x86), and for a druid fetches
// MxDrdShp.2DA at CPoint(nLevel - 1, 0), atol's the cell and adds
// bstats.m_featExtraShapeshifting (a byte at +0x1E6) when the cell is
// nonzero. The runner's data has no MxDrdShp.2DA, so both sides get the
// same synthesized three-by-three table (default 99) loaded through ours'
// LoadText seam on ours' side and filled byte-for-byte into the original's
// C2DArray layout (array at +0x18, default at +0x1C, sizes at +0x20/+0x22)
// on the original's. The original reads row nLevel - 1, column 0
// (m_pArray[sizeX * (nLevel - 1)], the row bound against m_nSizeY) while
// ours' GetAt(CPoint) reads column nLevel - 1 of row 0 (m_pArray[x +
// sizeX * y] with x = nLevel - 1, y = 0): the diagonals 10 vs 1 and 20 vs
// 2 at levels 2 and 3 are a mismatch finding, not something to fix.
//
// 0x5468B0 (CInfGame_CheckUseMagicDevice) stays pending: it walks the
// sprite's item slots through CGameSprite and CItem resrefs, which a case
// cannot set up.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CRuleTables.h"
#include "FileFormat.h"
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
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

using MaxShiftsFn = int32_t(__fastcall*)(const void* self, void* edx, const void* bstats, int32_t nLevel);
constexpr uintptr_t kMaxShifts = 0x543770;

// The synthesized MxDrdShp.2DA: three columns, three rows, default 99. The
// trailing newline ends the row walk.
const char* const kSynth2da = "2DA V1.0\n"
                              "99\n"
                              " COL_A COL_B COL_C\n"
                              "ROW_A 0 1 2\n"
                              "ROW_B 10 11 12\n"
                              "ROW_C 20 21 22\n";

const char* const kCellText[9] = { "0", "1", "2", "10", "11", "12", "20", "21", "22" };

// The original's CRuleTables with the same table laid out by hand; only
// what 0x543770 touches is filled (it never reads the label arrays).
uint8_t* BuildOriginalRules()
{
    auto* rules = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(0x598 + 0x24));
    std::memset(rules, 0, 0x598 + 0x24);
    auto* cells = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(9 * 4));
    std::memset(cells, 0, 9 * 4);
    for (int i = 0; i < 9; ++i) {
        reinterpret_cast<OriginalCStringFn>(kOriginalCString)(cells + i * 4, nullptr, kCellText[i]);
    }
    uint8_t* table = rules + 0x598;
    const uint32_t cellsAddress = reinterpret_cast<uint32_t>(cells);
    std::memcpy(table + 0x18, &cellsAddress, 4); // m_pArray
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(table + 0x1C, nullptr, "99"); // m_default
    const int16_t sizeX = 3;
    const int16_t sizeY = 3;
    std::memcpy(table + 0x20, &sizeX, 2); // m_nSizeX
    std::memcpy(table + 0x22, &sizeY, 2); // m_nSizeY
    return rules;
}

void CheckMaxShifts(CaseContext& ctx, CRuleTables* rules, CCreatureFileHeader* oursHeader,
    uint8_t* originalRules, uint8_t* originalHeader, uint8_t druid, uint8_t feat, int32_t level)
{
    ctx.SetInputs("druid level " + std::to_string(druid)
        + ", extra shapeshifts " + std::to_string(feat)
        + ", class level " + std::to_string(level));

    oursHeader->m_druidLevel = druid;
    oursHeader->m_featExtraShapeshifting = feat;
    const INT oursResult = rules->GetMaxDruidShapeshifts(*oursHeader, level);

    int32_t originalResult = 0;
    ctx.CallOriginal([&] {
        originalHeader[0x86] = druid;
        originalHeader[0x1E6] = feat;
        originalResult = reinterpret_cast<MaxShiftsFn>(kMaxShifts)(originalRules, nullptr, originalHeader, level);
    });

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalResult, static_cast<int32_t>(oursResult));
}

// GetMaxDruidShapeshifts over the level domain with and without the bonus
// feat, plus a non-druid and the out-of-range levels.
void RuleTablesDruidShifts(CaseContext& ctx)
{
    auto* rules = reinterpret_cast<CRuleTables*>(::operator new(sizeof(CRuleTables)));
    std::memset(rules, 0, sizeof(CRuleTables));
    // Through the member: ours' compiled offset is whatever it is, and the
    // original's +0x598 belongs to the original block above.
    auto* table = new (&rules->m_tMaxDruidShapeshifts) C2DArray();
    table->LoadText(kSynth2da);

    auto* header = reinterpret_cast<CCreatureFileHeader*>(::operator new(sizeof(CCreatureFileHeader)));
    std::memset(header, 0, sizeof(CCreatureFileHeader));

    uint8_t* originalRules = nullptr;
    uint8_t* originalHeader = nullptr;
    ctx.CallOriginal([&] {
        originalRules = BuildOriginalRules();
        originalHeader = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(0x200));
        std::memset(originalHeader, 0, 0x200);
    });

    for (const int32_t level : { 1, 2, 3, 4, 0, -1 }) {
        CheckMaxShifts(ctx, rules, header, originalRules, originalHeader, 1, 2, level);
    }
    CheckMaxShifts(ctx, rules, header, originalRules, originalHeader, 1, 0, 3);
    CheckMaxShifts(ctx, rules, header, originalRules, originalHeader, 0, 2, 2);
}

} // namespace

ORACLE_CASE("rule_tables-543770", RuleTablesDruidShifts);
ORACLE_CASE_PENDING("rule_tables-5468B0", nullptr,
    "walks the sprite's item slots through CGameSprite and CItem resrefs, "
    "which a case cannot set up");
