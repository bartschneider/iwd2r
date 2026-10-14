// The INI spawns' string parsing, the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures (R10b). CSpawnPointArray::Parse (0x76BD90) reads and writes only
// its own CTypedPtrArray (m_pData 4, m_nSize 8, m_nMaxSize 0xC, m_nGrowBy
// 0x10) and mDefaultFacing (0x14), and allocates its CSpawnPoints through
// the original's operator new: the array is a zeroed block of the original's
// size from its operator new with only mDefaultFacing set, never freed. The
// string is a CString from the original's CString(LPCSTR) (0x7FCC88) in a
// leaked original block. Ours is a CSpawnPointArray from our constructor.
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CSpawn.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CString::CString(LPCSTR) (0x7FCC88), `ret 4`.
using CStringCtorFn = void*(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kCStringCtor = 0x7FCC88;

// CSpawnPointArray::Parse(const CString&) (0x76BD90), `ret 4`, int in eax.
using ParseFn = int32_t(__fastcall*)(void* self, void* edx, const void* sData);
constexpr uintptr_t kParse = 0x76BD90;

// CObject's vtable pointer, m_pData, m_nSize, m_nMaxSize, m_nGrowBy and
// mDefaultFacing.
constexpr size_t kArraySize = 0x18;
constexpr size_t kArrayData = 0x4;
constexpr size_t kArrayCount = 0x8;
constexpr size_t kDefaultFacing = 0x14;
// CSpawnPoint: vtable pointer, mLocation (x, y), mFacing.
constexpr size_t kPointX = 0x4;
constexpr size_t kPointY = 0x8;
constexpr size_t kPointFacing = 0xC;

constexpr int32_t kDefault = 7;

uint8_t* OriginalBlock(size_t size)
{
    auto* at = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(size));
    std::memset(at, 0, size);
    return at;
}

template <class T>
T Get(const uint8_t* at, size_t offset)
{
    T value;
    std::memcpy(&value, at + offset, sizeof(value));
    return value;
}

std::string Point(int32_t x, int32_t y, int32_t facing)
{
    char text[64];
    std::snprintf(text, sizeof(text), " (%d,%d:%d)", x, y, facing);
    return text;
}

// No colon (the default facing), a trailing colon (facing 0), both
// separators, a separator first or last (skipped), several points, spaces,
// negatives, two colons, unbalanced and reversed brackets, nothing.
const char* const kInputs[] = { "[100.200]", "[100.200:]", "[100,200:3]", "[.200:3]", "[100.:3]", "[100,:3]",
    "[1.2:1] [3,4:2][5.6]", "  [ 12 . 34 : 5 ]  ", "[-5.-6:-1]", "[1.2:3:4]", "[1.2:1", "1.2:1]", "]x[1.2:9]",
    "[1.2][", "[]", "[:]", "[1:2]", "", "   ", "garbage", "[1.2:1]garbage[3.4:2]" };

void ParseCase(CaseContext& ctx)
{
    for (const char* input : kInputs) {
        ctx.SetInputs(std::string("Parse(\"") + input + "\")");

        int32_t originalCount = 0;
        std::string original;
        ctx.CallOriginal([&] {
            uint8_t* array = OriginalBlock(kArraySize);
            std::memcpy(array + kDefaultFacing, &kDefault, sizeof(kDefault));
            uint8_t* sData = OriginalBlock(4);
            reinterpret_cast<CStringCtorFn>(kCStringCtor)(sData, nullptr, input);
            originalCount = reinterpret_cast<ParseFn>(kParse)(array, nullptr, sData);

            const int32_t size = Get<int32_t>(array, kArrayCount);
            const uint8_t* const* data = Get<const uint8_t* const*>(array, kArrayData);
            original = "returned " + std::to_string(originalCount) + ", " + std::to_string(size) + " points";
            for (int32_t i = 0; i < size; i++) {
                original += Point(Get<int32_t>(data[i], kPointX), Get<int32_t>(data[i], kPointY),
                    Get<int32_t>(data[i], kPointFacing));
            }
        });

        CSpawnPointArray* ours = new CSpawnPointArray();
        ours->SetDefaultFacing(kDefault);
        const int oursCount = ours->Parse(CString(input));
        std::string oursText = "returned " + std::to_string(oursCount) + ", " + std::to_string(ours->GetSize()) + " points";
        for (int i = 0; i < ours->GetSize(); i++) {
            const CSpawnPoint* point = ours->GetAt(i);
            oursText += Point(point->GetLocation().x, point->GetLocation().y, point->GetFacing());
        }
        delete ours;

        ctx.CountInputs(1);
        if (original != oursText) {
            ctx.Fail(ctx.Inputs(), original, oursText);
        }
    }
}

} // namespace

ORACLE_CASE("spawn/parse_points", ParseCase);
ORACLE_CASE_PENDING("spawn/spawn_point", nullptr,
    "needs engine state (CGameSpawning::Spawn builds sprites from CRE resources through the original's "
    "CGameSprite constructor, adds them to its object array and area, and reads the game's party, group, "
    "visibility and search maps); ours covered by tests/test_gam.cpp [spawn][data]");
