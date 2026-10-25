// The directory-alias parser and the list that collects its results, the
// original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// CAlias::ParseRawData (0x781800) splits "name:=value;value" text into the
// flag at +4, the name at +8 and the CStringList at +0xC; the case builds an
// original CAlias with the original operator new and CString ctor (an empty
// name over a zeroed block whose list node-block-size field at +0x24 the
// original's AddTail fills from) and an ours over placement-new storage, then
// compares everything the call left behind. CAliasList::AddAlias (0x781A90)
// appends a parsed alias only when its flag is set; the case compares the
// boolean, the count and the head entry, plus the null-alias answer.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CAlias.h"
#include "CAliasList.h"
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

// CAlias::ParseRawData(CString&) (0x781800): __thiscall, the CString by
// reference, BOOLEAN in eax.
using ParseFn = int32_t(__fastcall*)(void* self, void* edx, void* raw);
constexpr uintptr_t kParseRawData = 0x781800;

// CAliasList::AddAlias(CAlias*) (0x781A90): __thiscall, `ret 4`, BOOLEAN in
// eax.
using AddFn = int32_t(__fastcall*)(void* self, void* edx, void* alias);
constexpr uintptr_t kAddAlias = 0x781A90;

// The original CAlias: flag at +4, CString name at +8, CStringList at +0xC
// (head at +0x10, count at +0x18, node block size at +0x24; the list's nodes
// carry the CString at +8). The original CAliasList: the same list fields
// from +4.
constexpr size_t kOriginalAliasSize = 0x28;
constexpr size_t kOriginalListSize = 0x20;

// A zeroed block from ours' heap with a default-constructed object on it.
CAlias* MakeAlias()
{
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CAlias)));
    std::memset(storage, 0, sizeof(CAlias));
    CString empty;
    return new (storage) CAlias(empty);
}

// The original alias's values joined with '|', walking the CStringList nodes.
std::string JoinOriginal(const uint8_t* alias)
{
    std::string joined;
    const uint8_t* node = *reinterpret_cast<const uint8_t* const*>(alias + 0x10);
    while (node != nullptr) {
        if (!joined.empty()) {
            joined += '|';
        }
        const char* const text = *reinterpret_cast<const char* const*>(node + 8);
        if (text != nullptr) {
            joined += text;
        }
        node = *reinterpret_cast<const uint8_t* const*>(node);
    }
    return joined;
}

// Ours, through the ported list API.
std::string JoinOurs(const CAlias& alias)
{
    std::string joined;
    for (POSITION pos = alias.field_C.GetHeadPosition(); pos != NULL;) {
        if (!joined.empty()) {
            joined += '|';
        }
        joined += static_cast<const char*>(alias.field_C.GetNext(pos));
    }
    return joined;
}

void CheckParse(CaseContext& ctx, const char* raw)
{
    ctx.SetInputs(std::string("ParseRawData(\"") + raw + "\")");

    int32_t originalRet = 0;
    uint8_t* original = nullptr;
    ctx.CallOriginal([&] {
        original = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kOriginalAliasSize));
        std::memset(original, 0, kOriginalAliasSize);
        reinterpret_cast<OriginalCStringFn>(kOriginalCString)(original + 8, nullptr, "");
        *reinterpret_cast<int32_t*>(original + 0x24) = 10; // list node block size
        uint32_t rawStorage = 0;
        reinterpret_cast<OriginalCStringFn>(kOriginalCString)(&rawStorage, nullptr, raw);
        originalRet = reinterpret_cast<ParseFn>(kParseRawData)(original, nullptr, &rawStorage);
    });

    auto* ours = MakeAlias();
    CString rawText(raw);
    const int32_t oursRet = ours->ParseRawData(rawText);

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalRet, oursRet);
    const std::string inputs = ctx.Inputs();
    ctx.SetInputs(inputs + ", the flag");
    oracle::Compare(ctx, *reinterpret_cast<const int32_t*>(original + 4), static_cast<int32_t>(ours->field_4));
    ctx.SetInputs(inputs + ", the name");
    oracle::Compare(ctx, std::string(*reinterpret_cast<const char* const*>(original + 8)),
            std::string(static_cast<const char*>(ours->field_8)));
    ctx.SetInputs(inputs + ", the values");
    oracle::Compare(ctx, JoinOriginal(original), JoinOurs(*ours));
    ctx.SetInputs(inputs);
}

void CheckAdd(CaseContext& ctx, int32_t flag, bool nullAlias)
{
    ctx.SetInputs(std::string("AddAlias(") + (nullAlias ? "NULL" : "a flag-" + std::to_string(flag) + " alias") + ")");

    int32_t originalRet = 0;
    uint8_t* originalList = nullptr;
    uint8_t* originalAlias = nullptr;
    ctx.CallOriginal([&] {
        originalList = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(kOriginalListSize));
        std::memset(originalList, 0, kOriginalListSize);
        *reinterpret_cast<int32_t*>(originalList + 0x18) = 10; // node block size
        if (!nullAlias) {
            originalAlias = static_cast<uint8_t*>(reinterpret_cast<OriginalNewFn>(kOriginalNew)(8));
            std::memset(originalAlias, 0, 8);
            *reinterpret_cast<int32_t*>(originalAlias + 4) = flag;
        }
        originalRet = reinterpret_cast<AddFn>(kAddAlias)(originalList, nullptr, originalAlias);
    });

    auto* listStorage = static_cast<unsigned char*>(::operator new(sizeof(CAliasList)));
    std::memset(listStorage, 0, sizeof(CAliasList));
    auto* oursList = new (listStorage) CAliasList();

    CAlias* oursAlias = nullptr;
    if (!nullAlias) {
        oursAlias = MakeAlias();
        oursAlias->field_4 = flag;
    }
    const int32_t oursRet = oursList->AddAlias(oursAlias);

    const CAlias* oursHead = nullptr;
    if (POSITION pos = oursList->GetHeadPosition()) {
        oursHead = static_cast<const CAlias*>(oursList->GetNext(pos));
    }
    const uint8_t* originalNode = *reinterpret_cast<const uint8_t* const*>(originalList + 4);
    const void* originalHead = originalNode != nullptr ? *reinterpret_cast<const void* const*>(originalNode + 8) : nullptr;

    ctx.CountInputs(1);
    oracle::Compare(ctx, originalRet, oursRet);
    const std::string inputs = ctx.Inputs();
    ctx.SetInputs(inputs + ", the count");
    oracle::Compare(ctx, *reinterpret_cast<const int32_t*>(originalList + 0xC), static_cast<int32_t>(oursList->GetCount()));
    ctx.SetInputs(inputs + ", the head entry");
    oracle::Compare(ctx, originalHead == originalAlias, oursHead == oursAlias);
    ctx.SetInputs(inputs);
}

// CAlias::ParseRawData over plain, multi-value, escaped-semicolon,
// empty-value and malformed text.
void AliasParseRawData(CaseContext& ctx)
{
    for (const char* const raw : {
                "a:=b",
                "a:=b;c",
                "a:=b;c;d",
                "a:=b\\;c",
                "a:=b;",
                "a:b",
                "a=b",
                "a:=",
                "abc",
                ":=x",
                "longname:=v1;v2;v3",
                "a:=b\\",
        }) {
        CheckParse(ctx, raw);
    }
}

// CAliasList::AddAlias over a flag-set alias, a flag-clear alias and NULL.
void AliasListAddAlias(CaseContext& ctx)
{
    for (const int32_t flag : { 1, 0 }) {
        CheckAdd(ctx, flag, false);
    }
    CheckAdd(ctx, 0, true);
}

} // namespace

ORACLE_CASE("aliases-781800", AliasParseRawData);
ORACLE_CASE("aliases-781A90", AliasListAddAlias);
