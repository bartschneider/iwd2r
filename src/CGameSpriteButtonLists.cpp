#include "CGameSpriteButtonLists.h"

#include "CBaldurChitin.h"
#include "CButtonData.h"
#include "CGameButtonList.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CSpell.h"
#include "CUtil.h"
#include "IcewindMisc.h"

// 0x84EA4C
static const INT dword_84EA4C = 0;

// 0x84EA50
static const INT dword_84EA50 = 1;

// 0x714F70
CGameButtonList* CGameSprite_GetSpellQuickButtons(CGameSprite* pSprite, const BYTE& nClass)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Set once; a spell without an ability within the caster level
    // takes the previous spell's.
    SPELL_ABILITY* pAbility = NULL;

    CGameButtonList* pList = new CGameButtonList();

    UINT nClassIndex = pGame->GetSpellcasterIndex(nClass);

    // __FILE__: .\Include\FileFormat.h
    // __LINE__: 2572
    UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

    CGameSpriteGroupedSpellList* pSpells = &(pSprite->m_spells.m_spellsByClass[nClassIndex]);

    for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++) {
        // __FILE__: .\Include\FileFormat.h
        // __LINE__: 2514
        UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

        CGameSpriteSpellList* pLevel = &(pSpells->m_lists[nLevel]);

        // The bard and the sorcerer cast from the level's slots.
        if ((nClassIndex == 0 || nClassIndex == 5) && pLevel->field_18 == 0) {
            continue;
        }

        for (UINT nIndex = 0; nIndex < pLevel->m_List.size(); nIndex++) {
            CGameSpriteSpellListEntry* pEntry = pLevel->Get(nIndex);
            if ((pEntry->field_C & 0x1) != 0) {
                continue;
            }

            if (nClassIndex != 0 && nClassIndex != 5) {
                if (pEntry->field_4 == 0 || pEntry->field_8 == 0) {
                    continue;
                }
            }

            UINT nID = pEntry->m_nID;
            const CResRef& cResRef = pGame->m_spells.Get(nID);

            CSpell cSpell;
            cSpell.SetResRef(cResRef, TRUE, TRUE);
            if (!cSpell.Demand()) {
                continue;
            }

            if (cSpell.pRes == NULL) {
                cSpell.Release();
                continue;
            }

            if (cResRef == CGameSprite::SPWI110) {
                cSpell.Release();
                continue;
            }

            SHORT nCasterLevel = pSprite->GetCasterLevel(&cSpell, nClass, 0);
            if (nCasterLevel < 2) {
                nCasterLevel = 1;
            }

            for (INT nAbility = 0; nAbility < cSpell.GetAbilityCount(); nAbility++) {
                if (nCasterLevel < cSpell.GetAbility(nAbility)->minCasterLevel) {
                    break;
                }
                pAbility = cSpell.GetAbility(nAbility);
            }

            if (pAbility != NULL && pAbility->quickSlotType == 2) {
                CButtonData* pButtonData = new CButtonData();
                pButtonData->m_icon = CString(pAbility->quickSlotIcon);
                pButtonData->m_abilityId.m_itemType = 1;
                pButtonData->m_abilityId.m_res = cResRef;
                pButtonData->m_abilityId.m_targetType = pAbility->actionType;

                INT nDisabledType;
                switch (cSpell.GetCasterType()) {
                case 1:
                    nDisabledType = 0;
                    break;
                case 2:
                    nDisabledType = 1;
                    break;
                case 4:
                    nDisabledType = 2;
                    break;
                default:
                    nDisabledType = 0;
                    break;
                }
                pButtonData->m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[nDisabledType]);

                pButtonData->m_abilityId.field_10 = cSpell.GetGenericName();
                pButtonData->m_name = cSpell.GetGenericName();
                pButtonData->m_abilityId.field_18 = pGame->GetRuleTables().GetClassSuffixStringRef(nClass);
                pButtonData->m_abilityId.m_nClass = nClass;
                pButtonData->m_abilityId.field_1D = static_cast<BYTE>(nLevel + 1);
                pButtonData->m_abilityId.field_1E = 0;

                if (!pSprite->CanCast(nClass, 0, &cSpell)) {
                    pButtonData->m_bDisabled = TRUE;
                    pButtonData->m_abilityId.field_18 = pGame->GetRuleTables().GetClassBeyondCastingAbilityStringRef(nClass);
                }

                SHORT nCount;
                if (nClassIndex == 0 || nClassIndex == 5) {
                    nCount = static_cast<SHORT>(pLevel->field_18);
                } else {
                    nCount = static_cast<SHORT>(pEntry->field_8);
                }
                pButtonData->m_count = nCount;
                if (nCount < 1) {
                    pButtonData->m_bDisabled = TRUE;
                }

                pList->AddTail(pButtonData);
            }

            cSpell.Release();
        }
    }

    return pList;
}

// 0x7155C0
CGameButtonList* CGameSprite_GetDomainQuickButtons(CGameSprite* pSprite, const BYTE& nClass, DWORD nDomain)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Set once, as in 0x714F70.
    SPELL_ABILITY* pAbility = NULL;

    CGameButtonList* pList = new CGameButtonList();

    UINT nClassIndex = pGame->GetSpellcasterIndex(nClass);

    CGameSpriteGroupedSpellList* pSpells = &(pSprite->m_domainSpells);

    for (UINT nLevel = 0; nLevel < pSpells->m_nHighestLevel; nLevel++) {
        // __FILE__: .\Include\FileFormat.h
        // __LINE__: 2514
        UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

        CGameSpriteSpellList* pLevel = &(pSpells->m_lists[nLevel]);

        if ((nClassIndex == 0 || nClassIndex == 5) && pLevel->field_18 == 0) {
            continue;
        }

        for (UINT nIndex = 0; nIndex < pLevel->m_List.size(); nIndex++) {
            CGameSpriteSpellListEntry* pEntry = pLevel->Get(nIndex);
            if ((pEntry->field_C & 0x1) != 0
                || pEntry->field_4 == 0
                || pEntry->field_8 == 0) {
                continue;
            }

            UINT nID = pEntry->m_nID;
            const CResRef& cResRef = pGame->m_spells.Get(nID);

            CSpell cSpell;
            cSpell.SetResRef(cResRef, TRUE, TRUE);
            if (!cSpell.Demand()) {
                continue;
            }

            if (cSpell.pRes == NULL) {
                cSpell.Release();
                continue;
            }

            if (cResRef == CGameSprite::SPWI110) {
                cSpell.Release();
                continue;
            }

            SHORT nCasterLevel = pSprite->GetCasterLevel(&cSpell, nClass, nDomain);
            if (nCasterLevel < 2) {
                nCasterLevel = 1;
            }

            for (INT nAbility = 0; nAbility < cSpell.GetAbilityCount(); nAbility++) {
                if (nCasterLevel < cSpell.GetAbility(nAbility)->minCasterLevel) {
                    break;
                }
                pAbility = cSpell.GetAbility(nAbility);
            }

            if (pAbility != NULL && pAbility->quickSlotType == 2) {
                CButtonData* pButtonData = new CButtonData();
                pButtonData->m_icon = CString(pAbility->quickSlotIcon);
                pButtonData->m_abilityId.m_itemType = 1;
                pButtonData->m_abilityId.m_res = cResRef;
                pButtonData->m_abilityId.m_targetType = pAbility->actionType;

                INT nDisabledType;
                switch (cSpell.GetCasterType()) {
                case 1:
                    nDisabledType = 0;
                    break;
                case 2:
                    nDisabledType = 1;
                    break;
                case 4:
                    nDisabledType = 2;
                    break;
                default:
                    nDisabledType = 0;
                    break;
                }
                pButtonData->m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[nDisabledType]);

                pButtonData->m_abilityId.field_10 = cSpell.GetGenericName();
                pButtonData->m_name = cSpell.GetGenericName();
                pButtonData->m_abilityId.field_18 = pGame->GetRuleTables().GetClassSuffixStringRef(nClass);
                pButtonData->m_abilityId.field_1D = static_cast<BYTE>(nLevel + 1);
                pButtonData->m_abilityId.m_nClass = nClass;
                pButtonData->m_abilityId.field_1E = pGame->GetRuleTables().GetSpecializationIndex(nClass, nDomain);

                if (!pSprite->CanCast(nClass, nDomain, &cSpell)) {
                    pButtonData->m_bDisabled = TRUE;
                    pButtonData->m_abilityId.field_18 = pGame->GetRuleTables().GetClassBeyondCastingAbilityStringRef(nClass);
                }

                SHORT nCount = static_cast<SHORT>(pEntry->field_8);
                pButtonData->m_count = nCount;
                if (nCount < 1) {
                    pButtonData->m_bDisabled = TRUE;
                }

                pList->AddTail(pButtonData);
            }

            cSpell.Release();
        }
    }

    return pList;
}

// 0x715BD0
CGameButtonList* CGameSprite_GetInnateQuickButtons(CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Set once for both loops, as in 0x714F70.
    SPELL_ABILITY* pAbility = NULL;

    CGameButtonList* pList = new CGameButtonList();

    for (UINT nIndex = 0; nIndex < pSprite->m_innateSpells.m_List.size(); nIndex++) {
        CGameSpriteSpellListEntry* pEntry = pSprite->m_innateSpells.Get(nIndex);
        if ((pEntry->field_C & 0x1) != 0
            || pEntry->field_4 == 0
            || pEntry->field_8 == 0) {
            continue;
        }

        UINT nID = pEntry->m_nID;
        const CResRef& cResRef = pGame->m_innateSpells.Get(nID);

        CSpell cSpell;
        cSpell.SetResRef(cResRef, TRUE, TRUE);
        if (cSpell.Demand()) {
            if (cSpell.pRes != NULL && cResRef != CGameSprite::SPIN285) {
                SHORT nCasterLevel = pSprite->GetCasterLevel(&cSpell, 0, 0);
                if (nCasterLevel < 2) {
                    nCasterLevel = 1;
                }

                for (INT nAbility = 0; nAbility < cSpell.GetAbilityCount(); nAbility++) {
                    if (nCasterLevel < cSpell.GetAbility(nAbility)->minCasterLevel) {
                        break;
                    }
                    pAbility = cSpell.GetAbility(nAbility);
                }

                if (pAbility != NULL && pAbility->quickSlotType == 4) {
                    CButtonData* pButtonData = new CButtonData();
                    pButtonData->m_icon = CString(pAbility->quickSlotIcon);
                    pButtonData->m_name = cSpell.GetGenericName();
                    pButtonData->m_abilityId.m_itemType = 1;
                    pButtonData->m_abilityId.m_res = cResRef;
                    pButtonData->m_abilityId.m_targetType = pAbility->actionType;
                    pButtonData->m_abilityId.field_10 = cSpell.GetGenericName();
                    pButtonData->m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[2]);

                    SHORT nCount = static_cast<SHORT>(pEntry->field_8);
                    pButtonData->m_count = nCount;
                    if (nCount < 1) {
                        pButtonData->m_bDisabled = TRUE;
                    }

                    if (pButtonData->m_abilityId.m_res == CGameSprite::SPIN275) {
                        pButtonData->m_count = pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)
                            ? static_cast<SHORT>(pSprite->field_4C54[1])
                            : 0;
                        if (!pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
                            pButtonData->m_bDisabled = TRUE;
                        }
                    } else if (pButtonData->m_abilityId.m_res == CGameSprite::SPIN276) {
                        pButtonData->m_count = pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)
                            ? static_cast<SHORT>(pSprite->field_4C54[0])
                            : 0;
                        if (!pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)) {
                            pButtonData->m_bDisabled = TRUE;
                        }
                    } else if (pButtonData->m_abilityId.m_res == CGameSprite::SPIN277) {
                        pButtonData->m_bDisplayCount = FALSE;
                        pButtonData->m_count = 0;
                        if (!pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)) {
                            pButtonData->m_bDisabled = TRUE;
                        }
                    } else if (pButtonData->m_abilityId.m_res == CGameSprite::SPIN278) {
                        pButtonData->m_bDisplayCount = FALSE;
                        pButtonData->m_count = 0;
                        if (!pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)) {
                            pButtonData->m_bDisabled = TRUE;
                        }
                    } else if (pButtonData->m_abilityId.m_res == CGameSprite::SPIN279) {
                        pButtonData->m_bDisplayCount = FALSE;
                        pButtonData->m_count = 0;
                        if (!pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)) {
                            pButtonData->m_bDisabled = TRUE;
                        }
                    }

                    pList->AddTail(pButtonData);
                }
            }

            cSpell.Release();
        }
    }

    for (UINT nIndex = 0; nIndex < pSprite->m_shapeshifts.m_List.size(); nIndex++) {
        CGameSpriteSpellListEntry* pEntry = pSprite->m_shapeshifts.Get(nIndex);
        if ((pEntry->field_C & 0x1) != 0
            || pEntry->field_4 == 0
            || pEntry->field_8 == 0) {
            continue;
        }

        UINT nID = pEntry->m_nID;
        const CResRef& cResRef = pGame->m_shapeshifts.Get(nID);

        CSpell cSpell;
        cSpell.SetResRef(cResRef, TRUE, TRUE);
        if (!cSpell.Demand()) {
            continue;
        }

        if (cSpell.pRes == NULL) {
            cSpell.Release();
            continue;
        }

        SHORT nCasterLevel = pSprite->GetCasterLevel(&cSpell, 0, 0);
        if (nCasterLevel < 2) {
            nCasterLevel = 1;
        }

        for (INT nAbility = 0; nAbility < cSpell.GetAbilityCount(); nAbility++) {
            if (nCasterLevel < cSpell.GetAbility(nAbility)->minCasterLevel) {
                break;
            }
            pAbility = cSpell.GetAbility(nAbility);
        }

        if (pAbility != NULL && pAbility->quickSlotType == 4) {
            CButtonData* pButtonData = new CButtonData();
            pButtonData->m_icon = CString(pAbility->quickSlotIcon);
            pButtonData->m_name = cSpell.GetGenericName();
            pButtonData->m_abilityId.m_itemType = 1;
            pButtonData->m_abilityId.m_res = cResRef;
            pButtonData->m_abilityId.m_targetType = pAbility->actionType;
            pButtonData->m_abilityId.field_10 = cSpell.GetGenericName();
            pButtonData->m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[2]);

            SHORT nCount;
            if (cResRef == CGameSprite::SPIN122) {
                nCount = static_cast<SHORT>(pEntry->field_8);
            } else {
                nCount = static_cast<SHORT>(pSprite->m_shapeshifts.field_18);
            }
            pButtonData->m_count = nCount;
            pButtonData->m_bDisplayCount = TRUE;
            if (nCount < 1) {
                pButtonData->m_bDisabled = TRUE;
            }

            pList->AddTail(pButtonData);
        }

        cSpell.Release();
    }

    return pList;
}

// 0x716E80
CGameButtonList* CGameSprite_sub_716E80(CGameSprite* pSprite, BOOL bFirstOnly)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const CRuleTables& ruleTables = pGame->GetRuleTables();

    CGameButtonList* pList = new CGameButtonList();

    for (SHORT nSlot = 0; nSlot <= 50; nSlot++) {
        if (nSlot >= 18 && nSlot <= 42) {
            continue;
        }

        if (pGame->m_cOptions.m_bQuickItemMapping && nSlot >= 15 && nSlot <= 17) {
            continue;
        }

        CItem* pItem = pSprite->m_equipment.m_items[nSlot];
        if (pItem == NULL) {
            continue;
        }

        if (nSlot == 43 || nSlot == 45 || nSlot == 47 || nSlot == 49) {
            if ((pItem->GetFlagsFile() & 0x2) != 0
                && pSprite->m_equipment.m_items[static_cast<BYTE>(nSlot + 1)] != NULL) {
                continue;
            }
        }

        if (!pItem->Demand()) {
            continue;
        }

        for (INT nAbility = 0; nAbility < pItem->GetAbilityCount(); nAbility++) {
            ITEM_ABILITY* pAbility = pItem->GetAbility(nAbility);

            if (pItem->GetItemType() == 11) {
                CGameEffect* pEffect = pItem->GetAbilityEffect(nAbility, 0, NULL);
                if (pEffect != NULL) {
                    if (pEffect->m_effectID == 147) {
                        // NOTE: The original leaks the effect here.
                        delete pEffect;
                        continue;
                    }
                    delete pEffect;
                }
            }

            if (pAbility == NULL || pAbility->quickSlotType != 3) {
                continue;
            }

            if ((pAbility->type & 0x100) != 0 && (pItem->m_flags & 0x1) == 0) {
                continue;
            }

            if ((pAbility->type & 0x200) != 0 && (pItem->m_flags & 0x1) != 0) {
                continue;
            }

            if ((pAbility->type & 0xFF) == 4) {
                continue;
            }

            if ((pAbility->type & 0xFF) == 2 && !pSprite->CheckLauncherType(pAbility, NULL)) {
                continue;
            }

            CButtonData* pButtonData = new CButtonData();
            pButtonData->m_icon = CString(pAbility->quickSlotIcon);
            pButtonData->m_name = pItem->GetGenericName();
            pButtonData->m_abilityId.m_itemNum = nSlot;
            pButtonData->m_abilityId.m_itemType = 2;
            pButtonData->m_abilityId.m_abilityNum = static_cast<SHORT>(nAbility);
            pButtonData->m_abilityId.m_targetType = pAbility->actionType;
            pButtonData->m_abilityId.field_10 = ruleTables.GetItemAbilityDescription(pItem->GetResRef(), nAbility);
            if (pButtonData->m_abilityId.field_10 == -1) {
                pButtonData->m_abilityId.field_10 = pItem->GetGenericName();
            }

            pButtonData->m_count = 0;
            if (pItem->GetMaxStackable() > 1 || pItem->GetMaxUsageCount(nAbility) != 0) {
                pButtonData->m_count = pItem->GetUsageCount(nAbility);
            }

            SHORT nLauncherSlot;
            CItem* pLauncher = pSprite->GetLauncher(pAbility, nLauncherSlot);
            if (pLauncher != NULL) {
                pButtonData->m_launcherIcon = pLauncher->GetItemIcon();
                pButtonData->m_launcherName = pLauncher->GetGenericName();
            }

            pList->AddTail(pButtonData);

            if (bFirstOnly) {
                pItem->Release();
                return pList;
            }
        }

        pItem->Release();
    }

    return pList;
}

// 0x717250
//
// NOTE: The machine code is the same as 0x716E80's.
CGameButtonList* CGameSprite_sub_717250(CGameSprite* pSprite, BOOL bFirstOnly)
{
    return CGameSprite_sub_716E80(pSprite, bFirstOnly);
}

// 0x717FC0
CGameButtonList* CGameSprite_sub_717FC0(CGameSprite* pSprite, const CResRef& cResRef, UINT nCount)
{
    SPELL_ABILITY* pAbility = NULL;

    CGameButtonList* pList = new CGameButtonList();

    CResRef cSpellResRef = cResRef;

    CSpell cSpell;
    cSpell.SetResRef(cSpellResRef, TRUE, TRUE);

    // NOTE: The original returns NULL without freeing the list on every
    // failure below.
    if (!cSpell.Demand()) {
        delete pList;
        return NULL;
    }

    if (cSpell.pRes == NULL) {
        cSpell.Release();
        delete pList;
        return NULL;
    }

    SHORT nCasterLevel = pSprite->GetCasterLevel(&cSpell, 0, 0);
    if (nCasterLevel < 2) {
        nCasterLevel = 1;
    }

    if (cSpell.GetAbilityCount() > 0) {
        for (INT nAbility = 0; nAbility < cSpell.GetAbilityCount(); nAbility++) {
            if (nCasterLevel < cSpell.GetAbility(nAbility)->minCasterLevel) {
                break;
            }
            pAbility = cSpell.GetAbility(nAbility);
        }

        if (pAbility != NULL && pAbility->quickSlotType == 4) {
            for (UINT nIndex = 0; nIndex < nCount; nIndex++) {
                CButtonData* pButtonData = new CButtonData();
                pButtonData->m_icon = CString(pAbility->quickSlotIcon);
                pButtonData->m_name = cSpell.GetGenericName();
                pButtonData->m_abilityId.m_itemType = 1;
                pButtonData->m_abilityId.m_res = cResRef;
                pButtonData->m_abilityId.m_targetType = pAbility->actionType;
                pButtonData->m_abilityId.field_10 = cSpell.GetGenericName();
                pButtonData->m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[2]);
                pButtonData->m_abilityId.field_18 = nIndex + 39831;
                if (nIndex != 0) {
                    pButtonData->m_count = static_cast<SHORT>(nIndex);
                }
                pList->AddTail(pButtonData);
            }

            cSpell.Release();
            return pList;
        }
    }

    cSpell.Release();
    delete pList;
    return NULL;
}

// 0x716770
void CGameSprite_sub_716770(CGameSprite* pSprite, CButtonData* pData)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 17075
    UTIL_ASSERT(pData != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 17078
    UTIL_ASSERT(pData->m_abilityId.m_nClass == CAIOBJECTTYPE_C_CLERIC);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 17079
    UTIL_ASSERT(pData->m_abilityId.field_1E == 0);

    UINT nSpellLevel = pData->m_abilityId.field_1D;
    UINT nClericLevel = pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_CLERIC);
    BYTE nClass = CAIOBJECTTYPE_C_CLERIC;
    UINT nMaxLevel = pGame->GetRuleTables().GetMaxSpellLevel(nClass, nClericLevel);

    UINT nLevel;
    if (nSpellLevel < nMaxLevel) {
        nLevel = nSpellLevel;
    } else {
        nLevel = nMaxLevel;
    }

    INT nColumn = dword_84EA4C;
    if (IcewindMisc::IsEvil(pSprite) == TRUE) {
        nColumn = dword_84EA50;
    }

    BOOLEAN bFound = FALSE;
    CResRef cResRef;
    CSpell* pSpell = NULL;

    while (nLevel != 0) {
        if (bFound) {
            break;
        }

        cResRef = pGame->GetRuleTables().m_tSpontaneousCasting.GetAt(CPoint(nColumn, nLevel - 1));

        pSpell = new CSpell(cResRef);
        if (pSpell->Demand() == TRUE) {
            bFound = TRUE;
        } else {
            delete pSpell;
            pSpell = NULL;
            nLevel--;
        }
    }

    if (!bFound) {
        return;
    }

    if (pSpell->pRes == NULL) {
        pSpell->Release();
        delete pSpell;
        return;
    }

    SPELL_ABILITY* pAbility = NULL;

    SHORT nCasterLevel = pSprite->GetCasterLevel(pSpell, pData->m_abilityId.m_nClass, 0);
    if (nCasterLevel < 2) {
        nCasterLevel = 1;
    }

    for (INT nAbility = 0; nAbility < pSpell->GetAbilityCount(); nAbility++) {
        if (nCasterLevel < pSpell->GetAbility(nAbility)->minCasterLevel) {
            break;
        }
        pAbility = pSpell->GetAbility(nAbility);
    }

    BYTE nDataClass = pData->m_abilityId.m_nClass;

    CButtonData buttonData;
    if (pAbility != NULL && pAbility->quickSlotType == 2) {
        UINT nID = 0;
        if (pGame->m_spells.Find(cResRef, nID) == TRUE) {
            buttonData.m_icon = CString(pAbility->quickSlotIcon);
            buttonData.m_abilityId.m_res = cResRef;
            buttonData.m_abilityId.m_targetType = pAbility->actionType;
            buttonData.m_abilityId.m_itemType = 3;

            if (cResRef == CGameSprite::SPWI110) {
                buttonData.m_bDisabled = TRUE;
                buttonData.m_abilityId.field_10 = 11555;
                buttonData.m_name = 11555;
                buttonData.m_count = 1;
            } else {
                INT nDisabledType;
                switch (pSpell->GetCasterType()) {
                case 1:
                    nDisabledType = 0;
                    break;
                case 2:
                    nDisabledType = 1;
                    break;
                case 4:
                    nDisabledType = 2;
                    break;
                default:
                    nDisabledType = 0;
                    break;
                }

                buttonData.m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[nDisabledType]);
                buttonData.m_abilityId.field_1D = static_cast<BYTE>(nLevel);
                buttonData.m_abilityId.m_nClass = nDataClass;
                buttonData.m_abilityId.field_1E = 0;
                buttonData.m_abilityId.field_10 = pSpell->GetGenericName();
                buttonData.m_abilityId.field_18 = pGame->GetRuleTables().GetClassSuffixStringRef(CAIOBJECTTYPE_C_CLERIC);
                buttonData.m_name = pSpell->GetGenericName();
                buttonData.m_count = 1;
            }
        }
    }

    pSpell->Release();
    delete pSpell;

    UINT nLevelIndex = nSpellLevel - 1;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 26177
    UTIL_ASSERT(nLevelIndex < CSPELLLIST_MAX_LEVELS);

    UINT nClassIndex = pGame->GetSpellcasterIndex(nDataClass);

    UINT nID = 0;
    if (pGame->m_spells.Find(pData->m_abilityId.m_res, nID) == TRUE) {
        // __FILE__: .\Include\FileFormat.h
        // __LINE__: 2572
        UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 26832
        UTIL_ASSERT(nLevelIndex < CSPELLLIST_MAX_LEVELS);

        CGameSpriteSpellList* pLevel = &(pSprite->m_spells.m_spellsByClass[nClassIndex].m_lists[nLevelIndex]);
        for (UINT nIndex = 0; nIndex < pLevel->m_List.size(); nIndex++) {
            CGameSpriteSpellListEntry* pEntry = &(pLevel->m_List[nIndex]);
            if (pEntry->m_nID == nID) {
                INT nRemaining = static_cast<INT>(pEntry->field_8) - 1;
                if (nRemaining < 0) {
                    nRemaining = 0;
                }
                pLevel->field_10 -= pEntry->field_8;
                pEntry->field_8 = nRemaining;
                pLevel->field_10 += nRemaining;
                break;
            }
        }
    }

    // TODO: Incomplete. The original calls CGameSprite::CheckQuickLists
    // (0x71ABA0) with `&pData->m_abilityId`, -1, 0, 0 here; it is not ported
    // (CGameSprite.cpp, another stream's file). Until then the quick spell
    // slots keep the converted spell's old count until they are rebuilt.

    pSprite->FeedBack(CGameSprite::FEEDBACK_SPONTANEOUSCASTING,
        0,
        0,
        0,
        buttonData.m_abilityId.field_10,
        0,
        0);

    pSprite->sub_71A0E0(buttonData, FALSE);
}

// 0x718390
BOOL CGameSprite_sub_718390(CGameSprite* pSprite, const CResRef& cResRef, BYTE nClass, DWORD nSpecialization, CButtonData& buttonData)
{
    BOOL bResult = FALSE;

    CResRef cSpellResRef = cResRef;

    CSpell cSpell;
    cSpell.SetResRef(cSpellResRef, TRUE, TRUE);
    if (!cSpell.Demand()) {
        return FALSE;
    }

    if (cSpell.pRes == NULL) {
        cSpell.Release();
        return FALSE;
    }

    SPELL_ABILITY* pAbility = NULL;

    SHORT nCasterLevel = pSprite->GetCasterLevel(&cSpell, nClass, nSpecialization);
    if (nCasterLevel < 2) {
        nCasterLevel = 1;
    }

    if (cSpell.GetAbilityCount() > 0) {
        for (INT nAbility = 0; nAbility < cSpell.GetAbilityCount(); nAbility++) {
            if (nCasterLevel < cSpell.GetAbility(nAbility)->minCasterLevel) {
                break;
            }
            pAbility = cSpell.GetAbility(nAbility);
        }

        if (pAbility != NULL) {
            buttonData.m_icon = CString(pAbility->quickSlotIcon);
            buttonData.m_name = cSpell.GetGenericName();
            buttonData.m_abilityId.m_itemType = 1;
            buttonData.m_abilityId.m_res = cResRef;
            buttonData.m_abilityId.m_targetType = pAbility->actionType;
            buttonData.m_abilityId.field_10 = cSpell.GetGenericName();
            buttonData.m_bDisabled = FALSE;
            buttonData.m_count = 0;
            bResult = TRUE;
        }
    }

    cSpell.Release();

    return bResult;
}
