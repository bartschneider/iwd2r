// Message id getters (task-34 rows 1-6 and 20-32), the original's against
// ours. The getters are folded constants in the original (task-34 report
// §3): every one is a `return <byte>;` over no member, so a case calls the
// original at its address with a this it never reads and compares what
// comes back. Ours is called the same way through a qualified name on zeroed
// storage (its getters read no member either): no constructor runs, ours or
// the original's.
//
// The rows whose message class ours has not ported (ProjectileTrailingVFX,
// 117, ProjectileImmumityUpdate) are pending with a body: the case pins the
// original's constant, and when ours gains the class the comparison is
// added and the evidence file's oracle_case follows it. The MarshalMessage
// rows (20-32) are pending without a body: no fixture can honestly build
// the object array and target sprite they walk (ADR 0002 defers finishing
// CMessage).
//
// This file includes engine headers and so must not include <windows.h>: it
// reaches the original only through CallOriginal and fixed addresses.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4121 4201)
#endif
#include "CMessage.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace {

// The original's getters are __thiscall taking no argument and reading no
// member (a folded constant), called here through a __fastcall pointer whose
// edx argument is unused.
using ConstantGetterFn = uint8_t(__fastcall*)(const void* self, void* edx);

// What the original's getter at address returns, with this never read.
uint8_t OriginalConstant(CaseContext& ctx, uintptr_t address, const char* name)
{
    ctx.SetInputs(std::string(name) + " on a zeroed block");
    uint8_t value = 0;
    ctx.CallOriginal([&] {
        uint8_t self[8] = {};
        value = reinterpret_cast<ConstantGetterFn>(address)(self, nullptr);
    });
    return value;
}

// Zeroed storage for a qualified call into ours: the getters read no
// member, so nothing of the object is ever touched.

// CMessageDisplayText::GetMsgType (0x40A0E0): CBaldurMessage::MSG_TYPE_CMESSAGE.
// The address is the original's shared fold of every CMessage subclass's
// GetMsgType (Ghidra names it CMessageAddAction::GetMsgType; task-34 calls
// the row CMessage105), and ours returns the same constant.
void DisplayTextMsgType(CaseContext& ctx)
{
    alignas(CMessageDisplayText) uint8_t storage[sizeof(CMessageDisplayText)] = {};
    ctx.SetInputs("CMessageDisplayText::GetMsgType on a zeroed block");
    const uint8_t original = OriginalConstant(ctx, 0x40A0E0, "the original's 0x40A0E0");
    const uint8_t ours = reinterpret_cast<CMessageDisplayText*>(storage)->CMessageDisplayText::GetMsgType();
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, ours);
}

// CMessageDisplayText::GetMsgSubType (0x43E060): ours returns
// MSG_SUBTYPE_CMESSAGE_CUT_SCENE_MODE_STATUS, which the decompile of the
// original says is not the byte it returns.
void DisplayTextMsgSubType(CaseContext& ctx)
{
    alignas(CMessageDisplayText) uint8_t storage[sizeof(CMessageDisplayText)] = {};
    ctx.SetInputs("CMessageDisplayText::GetMsgSubType on a zeroed block");
    const uint8_t original = OriginalConstant(ctx, 0x43E060, "the original's 0x43E060");
    const uint8_t ours = reinterpret_cast<CMessageDisplayText*>(storage)->CMessageDisplayText::GetMsgSubType();
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, ours);
}

// CMessageSetVariable::GetMsgSubType (0x453560): ours returns the literal 53
// the original's byte at 0x84CF0C holds.
void SetVariableMsgSubType(CaseContext& ctx)
{
    alignas(CMessageSetVariable) uint8_t storage[sizeof(CMessageSetVariable)] = {};
    ctx.SetInputs("CMessageSetVariable::GetMsgSubType on a zeroed block");
    const uint8_t original = OriginalConstant(ctx, 0x453560, "the original's 0x453560");
    const uint8_t ours = reinterpret_cast<CMessageSetVariable*>(storage)->CMessageSetVariable::GetMsgSubType();
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, ours);
}

// CMessageProjectileTrailingVFX::GetMsgSubType (0x4F6A70): original-only,
// ours has no CMessageProjectileTrailingVFX.
void ProjectileTrailingVfxSubType(CaseContext& ctx)
{
    ctx.SetInputs("the original's CMessageProjectileTrailingVFX::GetMsgSubType on a zeroed block");
    const uint8_t original = OriginalConstant(ctx, 0x4F6A70, "the original's 0x4F6A70");
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, uint8_t(0x56));
}

// CMessage117::GetMsgSubType (0x4F6FA0): original-only, ours has no
// CMessage117.
void Message117SubType(CaseContext& ctx)
{
    ctx.SetInputs("the original's CMessage117::GetMsgSubType on a zeroed block");
    const uint8_t original = OriginalConstant(ctx, 0x4F6FA0, "the original's 0x4F6FA0");
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, uint8_t(0x75));
}

// CMessageProjectileImmumityUpdate::GetMsgSubType (0x516D40):
// original-only, ours has no CMessageProjectileImmumityUpdate.
void ProjectileImmunitySubType(CaseContext& ctx)
{
    ctx.SetInputs("the original's CMessageProjectileImmumityUpdate::GetMsgSubType on a zeroed block");
    const uint8_t original = OriginalConstant(ctx, 0x516D40, "the original's 0x516D40");
    ctx.CountInputs(1);
    oracle::Compare(ctx, original, uint8_t(0x68));
}

} // namespace

ORACLE_CASE("messages-40A0E0", DisplayTextMsgType);
ORACLE_CASE("messages-43E060", DisplayTextMsgSubType);
ORACLE_CASE("messages-453560", SetVariableMsgSubType);
ORACLE_CASE_PENDING("messages-4F6A70", ProjectileTrailingVfxSubType,
    "ours has no CMessageProjectileTrailingVFX yet (0x4F6A70): the case pins the original's constant "
    "until the class is ported");
ORACLE_CASE_PENDING("messages-4F6FA0", Message117SubType,
    "ours has no CMessage117 yet (0x4F6FA0): the case pins the original's constant until the class "
    "is ported");
ORACLE_CASE_PENDING("messages-516D40", ProjectileImmunitySubType,
    "ours has no CMessageProjectileImmumityUpdate yet (0x516D40): the case pins the original's "
    "constant until the class is ported");
// CMessageAddEffect::MarshalMessage (0x4F8470) walks the game's object array
// through CGameObjectArray::GetShare and the target sprite's virtuals; the
// in-game path never runs (task-34 rows 20-32) and ADR 0002 defers finishing
// CMessage, so no case can set that state up.
ORACLE_CASE_PENDING("messages-4F8470", nullptr,
    "needs engine state (the game's object array and the target sprite's virtuals; ADR 0002 defers "
    "finishing CMessage)");
