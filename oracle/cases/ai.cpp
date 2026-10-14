// Script AI helpers (CGameAIBase::GetGlobalVariable, CAIObjectType::Decode),
// the original's against ours.
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
//
// Fixtures. Both functions reach the game through g_pBaldurChitin (0x8CF6DC)
// -> m_pObjectGame (0x1C54). The original's chitin and game are zeroed blocks
// from its operator new with only the members the functions read filled in:
// the GLOBAL variables (0x47FC, built in place by CVariableHash's constructor
// 0x54FB40 and filled by its own AddKey), the named creatures (0x4808,
// CNamedCreatureVariableHash's constructor 0x550A20 and AddKey), the area
// table (0x37E2, zeroed areas holding only their resref at 0x1F0 and their
// variables at 0xACE), the party (0x3816) and the object array (0x372C:
// m_pArray 0x20, m_nArraySize 0x24, m_nMaxArrayIndex 0x28, m_nNextObjectId
// 0x2A; its critical section is never taken on the AI thread). The objects
// Decode walks are original CGameAIBases from their constructor (0x44C4B0)
// with only their id, position and script object fields set. Ours gets the
// same: zeroed storage for the chitin and the game, our hashes and
// CGameAIBases from their constructors, and our g_pBaldurChitin, put back to
// null when the case ends. Original objects are never freed; ours are leaked
// too (their destructors reach the object array).
//
// Calling conventions (objdump of the vanilla exe): __thiscall, called here
// through __fastcall pointers whose edx argument is unused; GetGlobalVariable
// is a static __cdecl.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CGameAIBase.h"
#include "CGameArea.h"
#include "CGameDoor.h"
#include "CGameObjectArray.h"
#include "CInfGame.h"
#include "CSearchBitmap.h"
#include "CVariableHash.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/registry.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <random>
#include <string>
#include <vector>

namespace {

// operator new (0x7FC95B): __cdecl, the size at [esp+4], plain ret.
using OriginalNewFn = void*(__cdecl*)(size_t);
constexpr uintptr_t kOriginalNew = 0x7FC95B;

// CString::CString(const char*) (0x7FCC88): `ret 4`.
using OriginalCStringFn = void(__fastcall*)(void* self, void* edx, const char* text);
constexpr uintptr_t kOriginalCString = 0x7FCC88;

// CGameAIBase::GetGlobalVariable(const CString& sScope, const CString& sName,
// int) (0x4686C0): static __cdecl, plain ret at 0x468893; the CVariable* in
// eax.
using GetGlobalVariableFn = uint8_t*(__cdecl*)(const void* scope, const void* name, int a3);
constexpr uintptr_t kGetGlobalVariable = 0x4686C0;

// CVariableHash::CVariableHash(LONG nSize) (0x54FB40), `ret 4` at 0x54FC08;
// CNamedCreatureVariableHash::CNamedCreatureVariableHash(LONG nSize)
// (0x550A20), `ret 4` at 0x550A8C. AddKey(CVariable&) is their first
// virtual, `ret 4`.
using HashCtorFn = void(__fastcall*)(void* self, void* edx, int32_t nSize);
constexpr uintptr_t kVariableHashCtor = 0x54FB40;
constexpr uintptr_t kNamedHashCtor = 0x550A20;
using AddKeyFn = int32_t(__fastcall*)(void* self, void* edx, void* var);

// CAIObjectType::CAIObjectType(BYTE x8, int nInstance, BYTE nAvClass,
// int nClassMask) (0x40AE80), `ret 0x2C` at 0x40AFB1; Decode(CGameAIBase*)
// (0x40B880), `ret 4` at 0x40BAB2.
using TypeCtorFn = void(__fastcall*)(void* self, void* edx, uint32_t ea, uint32_t general, uint32_t race,
    uint32_t subRace, uint32_t cls, uint32_t specific, uint32_t gender, uint32_t alignment, int32_t instance,
    uint32_t avClass, int32_t classMask);
constexpr uintptr_t kTypeCtor = 0x40AE80;
using DecodeFn = void(__fastcall*)(void* self, void* edx, void* caller);
constexpr uintptr_t kDecode = 0x40B880;
constexpr size_t kTypeSize = 0x3C;

// CGameAIBase::CGameAIBase() (0x44C4B0), plain ret at 0x44CB0D.
using AIBaseCtorFn = void(__fastcall*)(void* self, void* edx);
constexpr uintptr_t kAIBaseCtor = 0x44C4B0;
constexpr size_t kAIBaseSize = 0x598;

constexpr uintptr_t kBaldurChitin = 0x8CF6DC;
constexpr size_t kChitinSize = 0x1C58;
constexpr size_t kObjectGameOffset = 0x1C54;
constexpr size_t kGameSize = 0x4820;
constexpr size_t kGameVariablesOffset = 0x47FC;
constexpr size_t kGameNamedOffset = 0x4808;
constexpr size_t kGameAreasOffset = 0x37E2;
constexpr size_t kGameCharactersOffset = 0x3816;
constexpr size_t kObjectArrayOffset = 0x372C;
constexpr size_t kAreaSize = 0xAE0;
constexpr size_t kAreaResRefOffset = 0x1F0;
constexpr size_t kAreaVariablesOffset = 0xACE;
constexpr size_t kVariableSize = 0x54;

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

template <typename T>
void Put(uint8_t* at, T value)
{
    std::memcpy(at, &value, sizeof(T));
}

template <typename T>
T Get(const uint8_t* at)
{
    T value;
    std::memcpy(&value, at, sizeof(T));
    return value;
}

const void* OriginalString(const std::string& text)
{
    void* at = OriginalNew(4);
    reinterpret_cast<OriginalCStringFn>(kOriginalCString)(at, nullptr, text.c_str());
    return at;
}

// An original CVariable named `name` holding `value`, added to the original
// hash at `hash` through its own AddKey.
void OriginalAddKey(uint8_t* hash, const std::string& name, int32_t value)
{
    uint8_t* var = OriginalBlock(kVariableSize);
    std::strncpy(reinterpret_cast<char*>(var), name.c_str(), 32);
    Put<int32_t>(var + 0x28, value);
    const uint32_t vtable = Get<uint32_t>(hash);
    const uint32_t addKey = *reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(vtable));
    reinterpret_cast<AddKeyFn>(static_cast<uintptr_t>(addKey))(hash, nullptr, var);
}

void OursAddKey(CVariableHashBase* hash, const std::string& name, int32_t value)
{
    CVariable var;
    var.SetName(CString(name.c_str()));
    var.m_intValue = value;
    hash->AddKey(var);
}

// Puts our g_pBaldurChitin back when the case ends, however it ends.
struct OurChitinReset {
    ~OurChitinReset() { g_pBaldurChitin = nullptr; }
};

// Our chitin and game: zeroed storage with only what the functions read.
CInfGame* OursGame()
{
    auto* chitin = static_cast<unsigned char*>(::operator new(sizeof(CBaldurChitin)));
    std::memset(chitin, 0, sizeof(CBaldurChitin));
    auto* game = static_cast<unsigned char*>(::operator new(sizeof(CInfGame)));
    std::memset(game, 0, sizeof(CInfGame));
    CInfGame* pGame = reinterpret_cast<CInfGame*>(game);
    reinterpret_cast<CBaldurChitin*>(chitin)->m_pObjectGame = pGame;
    g_pBaldurChitin = reinterpret_cast<CBaldurChitin*>(chitin);
    new (&pGame->m_variables) CVariableHash(512);
    new (&pGame->m_namedCreatures) CNamedCreatureVariableHash(512);
    return pGame;
}

// The original's chitin and game, g_pBaldurChitin pointing at them.
uint8_t* OriginalGame()
{
    uint8_t* chitin = OriginalBlock(kChitinSize);
    uint8_t* game = OriginalBlock(kGameSize);
    Put<uint32_t>(chitin + kObjectGameOffset, Address(game));
    Put<uint32_t>(reinterpret_cast<uint8_t*>(kBaldurChitin), Address(chitin));
    reinterpret_cast<HashCtorFn>(kVariableHashCtor)(game + kGameVariablesOffset, nullptr, 512);
    reinterpret_cast<HashCtorFn>(kNamedHashCtor)(game + kGameNamedOffset, nullptr, 512);
    return game;
}

// ---------------------------------------------------------------------------
// ai/global_variable

struct VariableInput {
    std::string scope;
    std::string name;
};

const char* const kScopes[] = { "GLOBAL", "global", "LOCALS", "MYAREA", "AR1000", "ar1000", "AR1001", "AR9999", "" };
const char* const kNames[] = { "CHAPTER", "chapter", "AR1000_VISITED", "HEDRON_TALKED", "NOT_THERE", "", "A_NAME_LONGER_THAN_THIRTY_TWO_CHARS" };

void GlobalVariable(CaseContext& ctx)
{
    OurChitinReset reset;
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed));

    for (int round = 0; round < 20; ++round) {
        // A fresh game per round: two areas, a few variables in each table.
        std::vector<VariableInput> calls;
        for (int i = 0; i < 12; ++i) {
            calls.push_back({ kScopes[rng() % (sizeof(kScopes) / sizeof(kScopes[0]))],
                kNames[rng() % (sizeof(kNames) / sizeof(kNames[0]))] });
        }
        const int32_t chapter = static_cast<int32_t>(rng() % 9) - 1;
        const int32_t visited = static_cast<int32_t>(rng() % 3);
        const int32_t talked = static_cast<int32_t>(rng() % 5);

        std::vector<std::string> original;
        ctx.SetInputs("round " + std::to_string(round) + ": building the original's game");
        ctx.CallOriginal([&] {
            uint8_t* game = OriginalGame();
            OriginalAddKey(game + kGameVariablesOffset, "CHAPTER", chapter);
            OriginalAddKey(game + kGameVariablesOffset, "HEDRON_TALKED", talked);
            const char* const areaNames[] = { "AR1000", "AR1001" };
            for (int a = 0; a < 2; ++a) {
                uint8_t* area = OriginalBlock(kAreaSize);
                std::memcpy(area + kAreaResRefOffset, areaNames[a], 6);
                reinterpret_cast<HashCtorFn>(kVariableHashCtor)(area + kAreaVariablesOffset, nullptr, 32);
                if (a == 0) {
                    OriginalAddKey(area + kAreaVariablesOffset, "AR1000_VISITED", visited);
                }
                Put<uint32_t>(game + kGameAreasOffset + 4 * a, Address(area));
            }
            for (const VariableInput& call : calls) {
                const uint8_t* var = reinterpret_cast<GetGlobalVariableFn>(kGetGlobalVariable)(
                    OriginalString(call.scope), OriginalString(call.name), 0);
                if (var == nullptr) {
                    original.push_back("null");
                } else {
                    original.push_back(std::string(reinterpret_cast<const char*>(var),
                                           strnlen(reinterpret_cast<const char*>(var), 32))
                        + "=" + std::to_string(Get<int32_t>(var + 0x28)));
                }
            }
        });

        CInfGame* pGame = OursGame();
        OursAddKey(&pGame->m_variables, "CHAPTER", chapter);
        OursAddKey(&pGame->m_variables, "HEDRON_TALKED", talked);
        const char* const areaNames[] = { "AR1000", "AR1001" };
        for (int a = 0; a < 2; ++a) {
            auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameArea)));
            std::memset(storage, 0, sizeof(CGameArea));
            CGameArea* pArea = reinterpret_cast<CGameArea*>(storage);
            new (&pArea->m_resRef) CResRef(areaNames[a]);
            new (&pArea->m_variables) CVariableHash(32);
            if (a == 0) {
                OursAddKey(&pArea->m_variables, "AR1000_VISITED", visited);
            }
            pGame->m_gameAreas[a] = pArea;
        }

        for (size_t i = 0; i < calls.size(); ++i) {
            ctx.SetInputs("round " + std::to_string(round) + " (CHAPTER " + std::to_string(chapter)
                + ", AR1000_VISITED " + std::to_string(visited) + "), call " + std::to_string(i) + ": scope \""
                + calls[i].scope + "\", name \"" + calls[i].name + "\"");
            const CVariable* var = CGameAIBase::GetGlobalVariable(CString(calls[i].scope.c_str()),
                CString(calls[i].name.c_str()),
                0);
            std::string ours = "null";
            if (var != nullptr) {
                ours = std::string(var->m_name, strnlen(var->m_name, 32)) + "=" + std::to_string(var->m_intValue);
            }
            ctx.CountInputs(1);
            oracle::Compare(ctx, original[i], ours);
        }
    }
}

// ---------------------------------------------------------------------------
// ai/decode

// The object specifiers Decode follows without a game area. The nearest
// and strongest families (4-7, 12, 14, 28-50, 52, 53) and Protagonist (27,
// which reads the world screen) are not inputs: ours does not port them yet
// (docs/evidence/40B880.md) and the original's need an area.
const uint8_t kSpecials[] = { 1, 2, 3, 8, 9, 10, 11, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 51, 54, 55,
    56, 100, 254, 255 };

// The CAIObjectType fields of a CGameAIBase that the specifiers follow, by
// offset.
const size_t kFieldOffsets[] = { 0x6E, 0xAE, 0xEA, 0x126, 0x162, 0x1DA, 0x216, 0x252, 0x28E, 0x2CA, 0x342, 0x37E, 0x3BA };

CAIObjectType* OursField(CGameAIBase* p, size_t offset)
{
    switch (offset) {
    case 0x6E:
        return &p->m_lAttacker;
    case 0xAE:
        return &p->m_lOrderedBy;
    case 0xEA:
        return &p->field_EA;
    case 0x126:
        return &p->field_126;
    case 0x162:
        return &p->field_162;
    case 0x1DA:
        return &p->m_lHelp;
    case 0x216:
        return &p->m_lTrigger;
    case 0x252:
        return &p->m_lSeen;
    case 0x28E:
        return &p->m_lTalkedTo;
    case 0x2CA:
        return &p->m_lHeard;
    case 0x342:
        return &p->field_342;
    case 0x37E:
        return &p->field_37E;
    default:
        return &p->field_3BA;
    }
}

constexpr int kObjects = 5;

struct ObjectSetup {
    uint8_t ea[kObjects];
    uint8_t general[kObjects];
    int32_t fieldTarget[kObjects][13]; // -1: NOONE, else an object index
    int32_t party[6]; // -1: none
};

struct DecodeInput {
    std::string name;
    int32_t instance; // -1, or an object index
    uint8_t specials[5];
};

int32_t ObjectId(int index)
{
    return (index << 16) | (index + 1);
}

std::string DescribeType(const uint8_t* t)
{
    std::string text = "ea " + std::to_string(t[4]) + " general " + std::to_string(t[5]) + " race "
        + std::to_string(t[6]) + " subrace " + std::to_string(t[7]) + " class " + std::to_string(t[8])
        + " instance " + std::to_string(Get<int32_t>(t + 0xA)) + " special";
    for (int i = 0; i < 5; ++i) {
        text += " " + std::to_string(t[0xE + i]);
    }
    text += " specific " + std::to_string(t[0x13]) + " gender " + std::to_string(t[0x14]) + " alignment "
        + std::to_string(t[0x15]) + " avclass " + std::to_string(t[0x16]) + " classmask "
        + std::to_string(Get<int32_t>(t + 0x18)) + " location " + std::to_string(Get<int32_t>(t + 0x1C));
    return text;
}

std::string DescribeOurs(const CAIObjectType& t)
{
    uint8_t bytes[0x20] = {};
    bytes[4] = t.m_nEnemyAlly;
    bytes[5] = t.m_nGeneral;
    bytes[6] = t.m_nRace;
    bytes[7] = t.m_nSubRace;
    bytes[8] = t.m_nClass;
    Put<int32_t>(bytes + 0xA, t.m_nInstance);
    std::memcpy(bytes + 0xE, t.m_SpecialCase, 5);
    bytes[0x13] = t.m_nSpecific;
    bytes[0x14] = t.m_nGender;
    bytes[0x15] = t.m_nAlignment;
    bytes[0x16] = t.m_nAvClass;
    Put<int32_t>(bytes + 0x18, t.m_nClassMask);
    Put<int32_t>(bytes + 0x1C, t.m_nLocationType);
    return DescribeType(bytes);
}

std::string DescribeInput(const DecodeInput& in)
{
    std::string text = "name \"" + in.name + "\" instance " + std::to_string(in.instance) + " specials";
    for (uint8_t s : in.specials) {
        text += " " + std::to_string(s);
    }
    return text;
}

void Decode(CaseContext& ctx)
{
    OurChitinReset reset;
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed ^ 0xDEC0DEu));

    for (int round = 0; round < 25; ++round) {
        ObjectSetup setup;
        for (int o = 0; o < kObjects; ++o) {
            setup.ea[o] = static_cast<uint8_t>(rng() % 256);
            setup.general[o] = static_cast<uint8_t>(rng() % 8);
            for (int f = 0; f < 13; ++f) {
                setup.fieldTarget[o][f] = rng() % 3 == 0 ? -1 : static_cast<int32_t>(rng() % kObjects);
            }
        }
        for (int32_t& p : setup.party) {
            p = rng() % 4 == 0 ? -1 : static_cast<int32_t>(rng() % kObjects);
        }

        std::vector<DecodeInput> inputs;
        for (int i = 0; i < 16; ++i) {
            DecodeInput in;
            const int pickName = static_cast<int>(rng() % 5);
            in.name = pickName == 0 ? "OBJECT3" : pickName == 1 ? "nobody"
                : pickName == 2                                 ? "object2"
                                                                : "";
            in.instance = rng() % 5 == 0 ? static_cast<int32_t>(rng() % kObjects) : -1;
            const int count = static_cast<int>(rng() % 4);
            std::memset(in.specials, 0, sizeof(in.specials));
            for (int s = 0; s < count; ++s) {
                in.specials[s] = kSpecials[rng() % sizeof(kSpecials)];
            }
            if (rng() % 8 == 0) {
                // Five specifiers: the original returns without a result.
                for (uint8_t& s : in.specials) {
                    s = kSpecials[rng() % 3];
                }
            }
            inputs.push_back(in);
        }

        std::vector<std::string> original;
        ctx.SetInputs("round " + std::to_string(round) + ": building the original's objects");
        ctx.CallOriginal([&] {
            uint8_t* game = OriginalGame();
            uint8_t* objects[kObjects];
            uint8_t* entries = OriginalBlock(kObjects * 0xC);
            for (int o = 0; o < kObjects; ++o) {
                objects[o] = OriginalBlock(kAIBaseSize);
                reinterpret_cast<AIBaseCtorFn>(kAIBaseCtor)(objects[o], nullptr);
                Put<int32_t>(objects[o] + 0x5C, ObjectId(o));
                Put<int32_t>(objects[o] + 0x6, 100 * o);
                Put<int32_t>(objects[o] + 0xA, 50 * o);
                objects[o][0x20 + 4] = setup.ea[o];
                objects[o][0x20 + 5] = setup.general[o];
                Put<int32_t>(objects[o] + 0x20 + 0xA, ObjectId(o));
                Put<int16_t>(entries + o * 0xC + 6, static_cast<int16_t>(ObjectId(o) & 0xFFFF));
                Put<uint32_t>(entries + o * 0xC + 8, Address(objects[o]));
            }
            for (int o = 0; o < kObjects; ++o) {
                for (int f = 0; f < 13; ++f) {
                    if (setup.fieldTarget[o][f] >= 0) {
                        uint8_t* field = objects[o] + kFieldOffsets[f];
                        std::memset(field + 4, 0, 0x38);
                        Put<int32_t>(field + 0xA, ObjectId(setup.fieldTarget[o][f]));
                    }
                }
            }
            uint8_t* array = game + kObjectArrayOffset;
            Put<uint32_t>(array + 0x20, Address(entries));
            Put<int16_t>(array + 0x24, kObjects);
            Put<int16_t>(array + 0x28, kObjects - 1);
            Put<int16_t>(array + 0x2A, kObjects + 1);
            for (int p = 0; p < 6; ++p) {
                Put<int32_t>(game + kGameCharactersOffset + 4 * p,
                    setup.party[p] < 0 ? -1 : ObjectId(setup.party[p]));
            }
            OriginalAddKey(game + kGameNamedOffset, "OBJECT3", ObjectId(3));
            OriginalAddKey(game + kGameNamedOffset, "OBJECT2", ObjectId(2));

            for (const DecodeInput& in : inputs) {
                uint8_t* type = OriginalBlock(kTypeSize);
                reinterpret_cast<TypeCtorFn>(kTypeCtor)(type, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
                const void* name = OriginalString(in.name);
                std::memcpy(type, name, 4);
                Put<int32_t>(type + 0xA, in.instance < 0 ? -1 : ObjectId(in.instance));
                std::memcpy(type + 0xE, in.specials, 5);
                reinterpret_cast<DecodeFn>(kDecode)(type, nullptr, objects[0]);
                const char* text = *reinterpret_cast<const char* const*>(type);
                original.push_back(DescribeType(type) + " name \"" + std::string(text) + "\"");
            }
        });

        CInfGame* pGame = OursGame();
        CGameAIBase* objects[kObjects];
        auto* entries = new CGameObjectArrayEntry[kObjects];
        for (int o = 0; o < kObjects; ++o) {
            objects[o] = new CGameAIBase();
            objects[o]->m_id = ObjectId(o);
            objects[o]->m_pos.x = 100 * o;
            objects[o]->m_pos.y = 50 * o;
            objects[o]->m_typeAI.m_nEnemyAlly = setup.ea[o];
            objects[o]->m_typeAI.m_nGeneral = setup.general[o];
            objects[o]->m_typeAI.m_nInstance = ObjectId(o);
            std::memset(&entries[o], 0, sizeof(entries[o]));
            entries[o].m_objectId = static_cast<SHORT>(ObjectId(o) & 0xFFFF);
            entries[o].m_objectPtr = objects[o];
        }
        for (int o = 0; o < kObjects; ++o) {
            for (int f = 0; f < 13; ++f) {
                if (setup.fieldTarget[o][f] >= 0) {
                    CAIObjectType* field = OursField(objects[o], kFieldOffsets[f]);
                    CAIObjectType blank(0, 0, 0, 0, 0, 0, 0, 0, ObjectId(setup.fieldTarget[o][f]), 0, 0);
                    field->Set(blank);
                    field->m_nLocationType = 0;
                    field->m_ptCenter = CPoint(0, 0);
                    field->m_nRadius = 0;
                    field->m_rect = CRect(0, 0, 0, 0);
                }
            }
        }
        CGameObjectArray* array = &pGame->m_cObjectArray;
        array->m_pArray = entries;
        array->m_nArraySize = kObjects;
        array->m_nMaxArrayIndex = kObjects - 1;
        array->m_nNextObjectId = kObjects + 1;
        for (int p = 0; p < 6; ++p) {
            pGame->m_characters[p] = setup.party[p] < 0 ? -1 : ObjectId(setup.party[p]);
        }
        OursAddKey(&pGame->m_namedCreatures, "OBJECT3", ObjectId(3));
        OursAddKey(&pGame->m_namedCreatures, "OBJECT2", ObjectId(2));

        for (size_t i = 0; i < inputs.size(); ++i) {
            ctx.SetInputs("round " + std::to_string(round) + ", " + DescribeInput(inputs[i]));
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, inputs[i].instance < 0 ? -1 : ObjectId(inputs[i].instance), 0, 0);
            type.m_sName = inputs[i].name.c_str();
            std::memcpy(type.m_SpecialCase, inputs[i].specials, 5);
            type.Decode(objects[0]);
            const std::string ours = DescribeOurs(type) + " name \"" + std::string(type.m_sName) + "\"";
            ctx.CountInputs(1);
            oracle::Compare(ctx, original[i], ours);
        }
    }
}

// ---------------------------------------------------------------------------
// ai/closer_dest_pt

// CGameDoor::GetCloserDestPt(const CPoint& pt) (0x48B2C0), `ret 4`: reads
// the walk-to points at 0x5E0 and 0x5E8 and returns the address of one.
using CloserDestFn = const int32_t*(__fastcall*)(void* self, void* edx, const int32_t* pt);
constexpr uintptr_t kCloserDest = 0x48B2C0;
constexpr size_t kDoorSize = 0x710;

void CloserDestPt(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed ^ 0xD00Bu));
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CGameDoor)));
    std::memset(storage, 0, sizeof(CGameDoor));
    CGameDoor* ours = reinterpret_cast<CGameDoor*>(storage);
    uint8_t* door = nullptr;
    ctx.CallOriginal([&] { door = OriginalBlock(kDoorSize); });

    for (int i = 0; i < 2000; ++i) {
        // Mostly map-sized coordinates, sometimes equal distances.
        int32_t v[6];
        for (int32_t& x : v) {
            x = static_cast<int32_t>(rng() % 4000);
        }
        if (i % 7 == 0) {
            v[2] = 2 * v[0] - v[4];
            v[3] = 2 * v[1] - v[5];
        }
        const int32_t pt[2] = { v[0], v[1] };
        ctx.SetInputs("pt (" + std::to_string(v[0]) + "," + std::to_string(v[1]) + "), dest1 ("
            + std::to_string(v[2]) + "," + std::to_string(v[3]) + "), dest2 (" + std::to_string(v[4]) + ","
            + std::to_string(v[5]) + ")");
        int original = 0;
        ctx.CallOriginal([&] {
            std::memcpy(door + 0x5E0, &v[2], 8);
            std::memcpy(door + 0x5E8, &v[4], 8);
            const int32_t* at = reinterpret_cast<CloserDestFn>(kCloserDest)(door, nullptr, pt);
            original = at == reinterpret_cast<const int32_t*>(door + 0x5E0) ? 1
                : at == reinterpret_cast<const int32_t*>(door + 0x5E8)      ? 2
                                                                            : 0;
        });
        ours->m_ptDest1 = CPoint(v[2], v[3]);
        ours->m_ptDest2 = CPoint(v[4], v[5]);
        const CPoint& at = ours->GetCloserDestPt(CPoint(v[0], v[1]));
        const int oursWhich = &at == &ours->m_ptDest1 ? 1 : &at == &ours->m_ptDest2 ? 2
                                                                                    : 0;
        ctx.CountInputs(1);
        oracle::Compare(ctx, original, oursWhich);
    }
}

// ---------------------------------------------------------------------------
// ai/named_add_key

// CNamedCreatureVariableHash::AddKey(CVariable&) (0x550D10), `ret 4`, the
// first virtual. Fixture (R10a): the original's hash from its constructor
// (0x550A20) in a block from its operator new, ours from our constructor;
// the variables are zeroed blocks with a name and a value. After each call
// the return value and the variable's name (AddKey writes the stored form
// back into it) are compared; after each round, every bucket's chain (the
// entries' names and values, next at +0x54).
constexpr size_t kNamedHashSize = 0x10;
constexpr size_t kNamedEntryNext = 0x54;

const char* const kAddKeyNames[] = { "Hedron", "HEDRON", "he dron", " Reig ", "reig", "", "   ",
    "A_NAME_LONGER_THAN_THIRTY_TWO_CHARACTERS_X", "AN AME WITH MANY SPACES IN IT AND MORE THAN 32", "AR1000_Door5",
    "ar1000_door5", "Probe", "p r o b e" };

std::string RandomName(std::mt19937& rng)
{
    if (rng() % 2 == 0) {
        return kAddKeyNames[rng() % (sizeof(kAddKeyNames) / sizeof(kAddKeyNames[0]))];
    }
    static const char kLetters[] = "abcXYZ_ 09";
    std::string name;
    const size_t length = rng() % 40;
    for (size_t i = 0; i < length; ++i) {
        name += kLetters[rng() % (sizeof(kLetters) - 1)];
    }
    return name;
}

void NamedAddKey(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed ^ 0x550D10u));
    const int32_t kSizes[] = { 16, 64, 512 };
    for (int round = 0; round < 60; ++round) {
        const int32_t size = kSizes[round % 3];
        std::vector<std::string> names;
        std::vector<int32_t> values;
        for (int i = 0; i < 12; ++i) {
            names.push_back(RandomName(rng));
            values.push_back(static_cast<int32_t>(rng() % 1000));
        }

        std::vector<std::string> original;
        std::string originalTable;
        ctx.SetInputs("round " + std::to_string(round) + ": the original's hash");
        ctx.CallOriginal([&] {
            uint8_t* hash = OriginalBlock(kNamedHashSize);
            reinterpret_cast<HashCtorFn>(kNamedHashCtor)(hash, nullptr, size);
            const uint32_t addKey = *reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(Get<uint32_t>(hash)));
            for (size_t i = 0; i < names.size(); ++i) {
                uint8_t* var = OriginalBlock(kVariableSize);
                std::strncpy(reinterpret_cast<char*>(var), names[i].c_str(), 32);
                Put<int32_t>(var + 0x28, values[i]);
                const int32_t rc = reinterpret_cast<AddKeyFn>(static_cast<uintptr_t>(addKey))(hash, nullptr, var);
                original.push_back(std::to_string(rc) + " "
                    + std::string(reinterpret_cast<const char*>(var), strnlen(reinterpret_cast<const char*>(var), 32)));
            }
            const int32_t entries = Get<int32_t>(hash + 4);
            const uint8_t* table = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(Get<uint32_t>(hash + 8)));
            for (int32_t b = 0; b < entries; ++b) {
                const uint8_t* entry = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(Get<uint32_t>(table + 4 * b)));
                while (entry != nullptr) {
                    originalTable += std::to_string(b) + ":"
                        + std::string(reinterpret_cast<const char*>(entry), strnlen(reinterpret_cast<const char*>(entry), 32))
                        + "=" + std::to_string(Get<int32_t>(entry + 0x28)) + " ";
                    entry = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(Get<uint32_t>(entry + kNamedEntryNext)));
                }
            }
        });

        CNamedCreatureVariableHash* ours = new CNamedCreatureVariableHash(size);
        for (size_t i = 0; i < names.size(); ++i) {
            ctx.SetInputs("round " + std::to_string(round) + " (size " + std::to_string(size) + "), call "
                + std::to_string(i) + ": \"" + names[i] + "\" = " + std::to_string(values[i]));
            CVariable var;
            std::memset(&var, 0, sizeof(var));
            std::strncpy(var.m_name, names[i].c_str(), 32);
            var.m_intValue = values[i];
            const BOOL rc = ours->AddKey(var);
            ctx.CountInputs(1);
            oracle::Compare(ctx, original[i], std::to_string(rc) + " " + std::string(var.m_name, strnlen(var.m_name, 32)));
        }
        std::string oursTable;
        for (LONG b = 0; b < ours->m_nTableEntries; ++b) {
            for (CNamedCreatureVariableHashEntry* entry = ours->m_hashEntries[b]; entry != NULL; entry = entry->m_pNext) {
                oursTable += std::to_string(b) + ":"
                    + std::string(entry->m_variable.m_name, strnlen(entry->m_variable.m_name, 32)) + "="
                    + std::to_string(entry->m_variable.m_intValue) + " ";
            }
        }
        ctx.SetInputs("round " + std::to_string(round) + " (size " + std::to_string(size) + "): the table");
        oracle::Compare(ctx, originalTable, oursTable);
    }
}

// ---------------------------------------------------------------------------
// ai/door_blocker_points

// CSearchBitmap 0x54A210(const CPoint& pt, BYTE nPersonalSpace, const CPoint*
// pPoints, USHORT nPoints), `ret 0x10`, the answer in al. It reads only the
// grid size (+0xE6, +0xEA). Fixture (R10b): a zeroed block from the
// original's operator new with the size set; ours a zeroed CSearchBitmap
// with m_GridSquareDimensions set.
using DoorBlockerFn = uint8_t(__fastcall*)(void* self, void* edx, const int32_t* pt, uint32_t nPersonalSpace,
    const int32_t* pPoints, uint32_t nPoints);
constexpr uintptr_t kDoorBlocker = 0x54A210;
constexpr size_t kSearchBitmapBlock = 0x100;

void DoorBlockerPoints(CaseContext& ctx)
{
    std::mt19937 rng(static_cast<uint32_t>(ctx.seed ^ 0x54A210u));
    uint8_t* block = nullptr;
    ctx.CallOriginal([&] { block = OriginalBlock(kSearchBitmapBlock); });
    auto* storage = static_cast<unsigned char*>(::operator new(sizeof(CSearchBitmap)));
    std::memset(storage, 0, sizeof(CSearchBitmap));
    CSearchBitmap* ours = reinterpret_cast<CSearchBitmap*>(storage);

    const uint32_t kSpaces[] = { 0, 1, 2, 3, 5, 7, 9, 255 };
    for (int i = 0; i < 4000; ++i) {
        // Map sizes around AR1000's (200 x 192) and the 320 bound.
        const int32_t width = 1 + static_cast<int32_t>(rng() % 330);
        const int32_t height = 1 + static_cast<int32_t>(rng() % 330);
        // Centres at the edges, inside, outside and past 320.
        int32_t pt[2];
        for (int k = 0; k < 2; ++k) {
            const int32_t extent = k == 0 ? width : height;
            switch (rng() % 6) {
            case 0:
                pt[k] = 0;
                break;
            case 1:
                pt[k] = extent - 1;
                break;
            case 2:
                pt[k] = static_cast<int32_t>(rng() % 3) - 2;
                break;
            case 3:
                pt[k] = 318 + static_cast<int32_t>(rng() % 4);
                break;
            default:
                pt[k] = static_cast<int32_t>(rng() % (extent + 4)) - 2;
                break;
            }
        }
        const uint32_t space = kSpaces[rng() % (sizeof(kSpaces) / sizeof(kSpaces[0]))];
        // A few points near the centre (a door's closed squares).
        const uint32_t count = rng() % 9;
        std::vector<int32_t> points(2 * (count == 0 ? 1 : count));
        for (uint32_t n = 0; n < count; ++n) {
            points[2 * n] = pt[0] + static_cast<int32_t>(rng() % 15) - 7;
            points[2 * n + 1] = pt[1] + static_cast<int32_t>(rng() % 15) - 7;
        }
        std::string inputs = "grid " + std::to_string(width) + "x" + std::to_string(height) + ", pt ("
            + std::to_string(pt[0]) + "," + std::to_string(pt[1]) + "), space " + std::to_string(space) + ", points";
        for (uint32_t n = 0; n < count; ++n) {
            inputs += " (" + std::to_string(points[2 * n]) + "," + std::to_string(points[2 * n + 1]) + ")";
        }
        ctx.SetInputs(inputs);

        uint32_t original = 0;
        ctx.CallOriginal([&] {
            Put<int32_t>(block + 0xE6, width);
            Put<int32_t>(block + 0xEA, height);
            original = reinterpret_cast<DoorBlockerFn>(kDoorBlocker)(block, nullptr, pt, space, points.data(), count);
        });
        ours->m_GridSquareDimensions.cx = width;
        ours->m_GridSquareDimensions.cy = height;
        std::vector<CPoint> ourPoints;
        for (uint32_t n = 0; n < count; ++n) {
            ourPoints.push_back(CPoint(points[2 * n], points[2 * n + 1]));
        }
        const uint32_t answer = ours->sub_54A210(CPoint(pt[0], pt[1]), static_cast<BYTE>(space),
            ourPoints.empty() ? NULL : ourPoints.data(), static_cast<USHORT>(count));
        ctx.CountInputs(1);
        oracle::Compare(ctx, original & 0xFF, answer);
    }
}

}

ORACLE_CASE("ai/global_variable", GlobalVariable);
ORACLE_CASE("ai/closer_dest_pt", CloserDestPt);
ORACLE_CASE("ai/decode", Decode);
ORACLE_CASE("ai/named_add_key", NamedAddKey);
ORACLE_CASE("ai/door_blocker_points", DoorBlockerPoints);
