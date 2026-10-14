#include "CSpell.h"

#include "CBaldurChitin.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CRuleTables.h"
#include "CUtil.h"

// NOTE: Inlined.
CSpell::CSpell()
{
}

// FIXME: `res` should be reference.
//
// 0x585FD0
CSpell::CSpell(CResRef res)
{
    SetResRef(res, TRUE, TRUE);
}

// 0x54A380
BOOL CSpell::Demand()
{
    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();

            return pRes != NULL;
        }
    }

    return FALSE;
}

// 0x54A3C0
BOOL CSpell::Release()
{
    if (pRes != NULL) {
        pRes->Release();

        return TRUE;
    }

    return FALSE;
}

// NOTE: For unknown reason this function is not `const`.
//
// 0x54A3E0
INT CSpell::GetAbilityCount()
{
    INT nAbilityCount = 0;

    // NOTE: Probably inlined `Demand` and `Release`.
    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nAbilityCount = pRes->m_pHeader->abilityCount;
                pRes->Release();
            }
        }
    }

    return nAbilityCount;
}

// NOTE: For unknown reason this function is not `const`.
//
// 0x54A430
BOOL CSpell::GetIcon(BYTE* resRef)
{
    // NOTE: Probably inlined `Demand` and `Release`.
    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                CResRef iconResRef(pRes->m_pHeader->itemIcon);
                iconResRef.GetResRef(resRef);
                pRes->Release();
            }
        }
    }

    return FALSE;
}

// 0x54A4A0
SPELL_ABILITY* CSpell::GetAbility(LONG abilityNum) const
{
    SPELL_ABILITY* pAbility = NULL;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                pAbility = pRes->GetAbility(abilityNum);
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 265
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return pAbility;
}

// 0x54A510
CGameEffect* CSpell::GetAbilityEffect(LONG nAbility, LONG nEffect, CGameObject* pObject, BYTE nClass, DWORD nSpecialization, BYTE nCasterLevel)
{
    if (GetResRef() == "") {
        return NULL;
    }

    if (pRes == NULL) {
        return NULL;
    }

    pRes->Demand();

    if (pRes == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
        // __LINE__: 310
        UTIL_ASSERT(FALSE);
        return NULL;
    }

    if (nEffect >= pRes->GetAbilityEffectNo(nAbility)) {
        pRes->Release();
        return NULL;
    }

    if (nCasterLevel == 0) {
        if (pObject != NULL && pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            // NOTE: The SHORT is kept as a byte (0x54A5C4).
            nCasterLevel = static_cast<BYTE>(static_cast<CGameSprite*>(pObject)->GetCasterLevel(this, nClass, nSpecialization));
        } else {
            nCasterLevel = 10;
        }
    }

    CGameEffect* pEffect = CGameEffect::DecodeEffect(pRes->GetAbilityEffect(nAbility, nEffect, nCasterLevel),
        CPoint(-1, -1),
        -1,
        CPoint(-1, -1));
    if (pEffect != NULL) {
        pEffect->m_school = pRes->m_pHeader->school;
        pEffect->m_sourceType = 1;
        pEffect->field_48 = pRes->m_pHeader->secondaryType;
        pEffect->m_sourceRes = GetResRef();
        pEffect->m_sourceFlags = pRes->m_pHeader->itemFlags;

        if (pObject != NULL) {
            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                CGameSprite* pSprite = static_cast<CGameSprite*>(pObject);
                const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

                if (pEffect->m_school != 0) {
                    BYTE nSchool = ruleTables.MapCharacterSpecializationToSchool(static_cast<WORD>(pSprite->m_baseStats.m_specialization));

                    UINT nFeat;
                    BOOL bFocus = TRUE;
                    switch (pEffect->m_school) {
                    case 4:
                        nFeat = CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT;
                        break;
                    case 6:
                        nFeat = CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION;
                        break;
                    case 7:
                        nFeat = CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY;
                        break;
                    case 8:
                        nFeat = CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE;
                        break;
                    default:
                        nFeat = 0;
                        bFocus = FALSE;
                        break;
                    }

                    if (bFocus) {
                        if (pSprite->sub_763150(nFeat)) {
                            pEffect->m_saveMod += 2 * pSprite->GetFeatValue(nFeat);
                        }
                    }

                    if (nSchool == pEffect->m_school) {
                        pEffect->m_saveMod += 2;
                    }
                }

                if (GetResRef() == "SPIN126"
                    || GetResRef() == "SPIN127"
                    || GetResRef() == "SPIN128"
                    || GetResRef() == "SPIN129"
                    || GetResRef() == "SPIN130"
                    || GetResRef() == "SPIN131"
                    || GetResRef() == "SPIN132") {
                    // NOTE: Unsigned: the average below is an unsigned
                    // divide (0x54A8BF), see 54A510.md. Kept.
                    UINT nTotal = 0;
                    UINT nCount = 0;

                    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_WIZARD)) {
                        DWORD dwClassMask = CLASSMASK_WIZARD;
                        if (pSprite->sub_725330(CResRef("SPWI710"), dwClassMask, 9, FALSE) == TRUE) {
                            nTotal = ruleTables.GetSaveAdjustmentForSourceSprite(pSprite,
                                GetResRef(),
                                0,
                                CAIOBJECTTYPE_C_WIZARD,
                                0,
                                nCasterLevel);
                            nCount = 1;
                        }
                    }

                    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_SORCERER)) {
                        DWORD dwClassMask = CLASSMASK_SORCERER;
                        if (pSprite->sub_725330(CResRef("SPWI710"), dwClassMask, 9, FALSE) == TRUE) {
                            nTotal += ruleTables.GetSaveAdjustmentForSourceSprite(pSprite,
                                GetResRef(),
                                0,
                                CAIOBJECTTYPE_C_SORCERER,
                                0,
                                nCasterLevel);
                            nCount++;
                        }
                    }

                    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_CLERIC)
                        && (pSprite->m_baseStats.m_specialization & 0xFF8000) == SPECMASK_CLERIC_HELM) {
                        UINT nLevel = 9;
                        if (pSprite->sub_7256B0(CResRef("SPWI710"), nLevel, FALSE) == TRUE) {
                            nTotal += ruleTables.GetSaveAdjustmentForSourceSprite(pSprite,
                                GetResRef(),
                                0,
                                CAIOBJECTTYPE_C_CLERIC,
                                SPECMASK_CLERIC_HELM,
                                nCasterLevel);
                            nCount++;
                        }
                    }

                    if (nCount != 0) {
                        pEffect->m_saveMod += nTotal / nCount;
                    }
                }

                pEffect->m_casterLevel = nCasterLevel;

                // NOTE: Twice the Spell Penetration feat (0x54A8D2).
                pEffect->m_spellPenetrationBonus = pSprite->GetFeatValue(CGAMESPRITE_FEAT_SPELL_PENETRATION) << 1;
            } else {
                pEffect->m_casterLevel = 1;
            }

            pEffect->m_casterLevel |= nClass << 8;

            if (nClass == CAIOBJECTTYPE_C_CLERIC) {
                BYTE nIndex = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetSpecializationIndex(CAIOBJECTTYPE_C_CLERIC, nSpecialization);
                pEffect->m_casterLevel |= nIndex << 16;
            }
        }
    }

    pRes->Release();

    return pEffect;
}

// 0x54A970
SHORT CSpell::GetLevel() const
{
    SHORT nLevel = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nLevel = static_cast<SHORT>(pRes->m_pHeader->spellLevel);
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 492
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nLevel;
}

// 0x54A9E0
DWORD CSpell::GetNotUsableBy() const
{
    DWORD nNotUsableBy = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nNotUsableBy = pRes->m_pHeader->notUsableBy;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 536
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nNotUsableBy;
}

// 0x54ACD0
STRREF CSpell::GetGenericName() const
{
    STRREF nGenericName = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nGenericName = pRes->m_pHeader->genericName;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 755
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nGenericName;
}

// 0x54AA40
BOOL CSpell::CheckUsableBy(CGameSprite* pSprite) const
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
    // __LINE__: 623
    UTIL_ASSERT(pSprite != NULL);

    // NOTE: Uninline.
    DWORD dwNotUsableBy = GetNotUsableBy() & 0xE0000000;

    DWORD dwClassMask = pSprite->GetAIType().m_nClassMask;

    if ((dwClassMask & CLASSMASK_CLERIC) != 0
        && (dwNotUsableBy & 0x40000000) == 0) {
        return TRUE;
    }

    if ((dwClassMask & CLASSMASK_DRUID) != 0
        && (dwNotUsableBy & 0x80000000) == 0) {
        return TRUE;
    }

    if ((dwClassMask & CLASSMASK_SORCERER) != 0
        && (dwNotUsableBy & 0x80000000) == 0) {
        return TRUE;
    }

    if ((dwClassMask & CLASSMASK_WIZARD) != 0) {
        // NOTE: Uninline.
        BYTE nSchool = GetSchool();

        // NOTE: Uninline.
        dwNotUsableBy |= GetNotUsableBy() & 0x7FC0;

        switch (pSprite->GetSpecialization()) {
        case SPECMASK_WIZARD_ABJURER:
            return nSchool != 5 && nSchool != 8
                ? (dwNotUsableBy & 0x40) == 0
                : FALSE;
        case SPECMASK_WIZARD_CONJURER:
            return nSchool != 6
                ? (dwNotUsableBy & 0x80) == 0
                : FALSE;
        case SPECMASK_WIZARD_DIVINER:
            return nSchool != 2
                ? (dwNotUsableBy & 0x100) == 0
                : FALSE;
        case SPECMASK_WIZARD_ENCHANTER:
            return nSchool != 6
                ? (dwNotUsableBy & 0x200) == 0
                : FALSE;
        case SPECMASK_WIZARD_ILLUSIONIST:
            return nSchool != 7 && nSchool != 1
                ? (dwNotUsableBy & 0x400) == 0
                : FALSE;
        case SPECMASK_WIZARD_EVOKER:
            return nSchool != 3 && nSchool != 2
                ? (dwNotUsableBy & 0x800) == 0
                : FALSE;
        case SPECMASK_WIZARD_NECROMANCER:
            return nSchool != 5 && nSchool != 4
                ? (dwNotUsableBy & 0x1000) == 0
                : FALSE;
        case SPECMASK_WIZARD_TRANSMUTER:
            return nSchool != 7 && nSchool != 1
                ? (dwNotUsableBy & 0x2000) == 0
                : FALSE;
        }

        return (dwNotUsableBy & 0x4000) != 0;
    }

    if ((dwNotUsableBy & 0x0) == 0) {
        return TRUE;
    }

    return FALSE;
}

// 0x54AD30
STRREF CSpell::GetDescription() const
{
    STRREF nGenericDescription = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nGenericDescription = pRes->m_pHeader->genericDescription;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 799
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nGenericDescription;
}

// 0x54AD90
SHORT CSpell::GetCasterType() const
{
    SHORT nCasterType = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nCasterType = pRes->m_pHeader->itemType;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 844
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nCasterType;
}

// 0x54AE00
CGameEffect* CSpell::GetCastingEffect(int nEffect) const
{
    CGameEffect* pEffect = NULL;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();

            if (nEffect < pRes->GetCastingEffectNo()) {
                CPoint source(-1, -1);
                CPoint target(-1, -1);
                pEffect = CGameEffect::DecodeEffect(pRes->GetCastingEffect(nEffect),
                    source,
                    -1,
                    target);
            }

            pRes->Release();
        }
    }

    return pEffect;
}

// 0x54AEC0
SHORT CSpell::GetCastingEffectNo() const
{
    SHORT nCastingEffects;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nCastingEffects = pRes->GetCastingEffectNo();
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 940
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nCastingEffects;
}

// 0x54AF30
WORD CSpell::GetAnimationType() const
{
    WORD nAnimationType = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nAnimationType = pRes->m_pHeader->castingAnimationType;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 985
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nAnimationType;
}

// 0x54AFA0
DWORD CSpell::GetItemFlags() const
{
    DWORD dwItemFlags = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                dwItemFlags = pRes->m_pHeader->itemFlags;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 1076
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return dwItemFlags;
}

// 0x54B000
BYTE CSpell::GetSchool() const
{
    BYTE nSchool = 0;

    if (GetResRef() != "") {
        if (pRes != NULL) {
            pRes->Demand();
            if (pRes != NULL) {
                nSchool = pRes->m_pHeader->school;
                pRes->Release();
            } else {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\CSpell.cpp
                // __LINE__: 1118
                UTIL_ASSERT(FALSE);
            }
        }
    }

    return nSchool;
}
