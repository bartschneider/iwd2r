// Pending cases for project 3 (combat): saving throws, the attack roll and
// damage. A function that needs live engine state no fixture can honestly
// provide is listed without a body, with the reason. The names are
// tools/ghidra/extra_labels.txt's and docs/evidence/<ADDR>.md holds each
// function's evidence. (GetIterativeAttackBonus's original-only check became
// rules/iterative_attack_bonus in combat.cpp when ours was ported.)
//
// When ours exists, a case here becomes a comparison case in the file of its
// class and its evidence file's oracle_case follows it.
//
// This file includes no engine header and no <windows.h>: it reaches the
// original only through CallOriginal and fixed addresses.
#include "oracle/runner/compare.h"
#include "oracle/runner/generators.h"
#include "oracle/runner/registry.h"
#include "vanilla_config.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

// CGameEffect::CheckSave (0x4A42F0) first reads g_pBaldurChitin (0x8CF6DC)
// ->m_pObjectGame, then the target sprite's virtual GetAIType, its feats
// (0x763150), class levels and, for some effects, the game's object array
// (CGameObjectArray::GetShare) and party; it reports through FeedBack,
// CBaldurMessage::DisplayText and CGameSprite_DisplaySaveString.
ORACLE_CASE_PENDING("pending/check_save", nullptr,
    "needs engine state (g_pBaldurChitin's game, a sprite from the original's constructor for its vtable, "
    "feats and class levels, the object array); ours covered by tests/test_effects_save.cpp");
// CGameSprite::ProcessEffectList (0x72DE60; src/ also labels the address
// CGameSprite::sub_72DE60) rolls the four save d20s (rand() % 20 + 1) and a
// rand() % 100 into the sprite (0x70F6-0x70FA) as part of processing the
// sprite's whole effect list.
// Ours is ported (M1); the original's sprite and its effect lists come only
// from its constructors (R10a).
ORACLE_CASE_PENDING("pending/save_rolls", nullptr,
    "needs engine state (CGameSprite::ProcessEffectList 0x72DE60: the sprite's effect lists, derived stats and "
    "g_pBaldurChitin's game, from the original's constructors); ours covered by tests/test_effects.cpp");
// CGameSprite::Hit (0x73B760) rolls the attack d20 (CUtil::UtilRandInt(20,
// luck) + 1) and compares it with the target's AC. Ours is ported; the
// original needs sprites from its constructor (their vtables: GetAIType,
// GetPos), equipped CItems with loaded abilities and the game's options, so
// no fixture is honest (R10a). tests/test_combat.cpp pins ours' draws.
ORACLE_CASE_PENDING("pending/hit", nullptr,
    "needs engine state (equipped CItems with loaded abilities, an attacker and a target sprite from the "
    "original's constructor, g_pBaldurChitin's game and options); ours covered by tests/test_combat.cpp");
// CGameSprite::Damage (0x73D560) rolls the weapon's damage dice
// (CUtil::UtilRandInt(size, luck) + 1 per die) and builds the damage effect.
// Ours is ported; pending for the same reasons as Hit.
ORACLE_CASE_PENDING("pending/damage", nullptr,
    "needs engine state (equipped CItems with loaded abilities, the target's animation and vtable, "
    "g_pBaldurChitin's game, the message handler); ours covered by tests/test_combat.cpp");
// CGameDialogSprite::FetchRumor (0x483B70) loads the rumor DLG through
// CGameDialogSprite::Load (0x482210: g_pChitin's resource manager, the
// object array for the speaker's colour and name), parses its scripts with
// CAIScriptFile (the IDS lists through the resource manager) and holds each
// state's start condition against the sprite (RandomNum and the other status
// triggers through the sprite's vtable). tests/test_dialog.cpp checks ours
// with the game's 10SALTY.DLG (the only store rumor file, 10GOHAR.STO).
ORACLE_CASE_PENDING("dialog/fetch_rumor", nullptr,
    "needs engine state (g_pChitin's resource manager for the DLG and the IDS lists, the object array, a "
    "sprite whose status triggers evaluate the start conditions)");
