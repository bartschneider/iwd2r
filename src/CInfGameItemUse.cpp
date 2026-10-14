#include "CInfGameItemUse.h"

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CButtonData.h"
#include "CGameContainer.h"
#include "CGameEffect.h"
#include "CGameSpriteEquipment.h"
#include "CItem.h"
#include "CMessage.h"
#include "CUtil.h"

// 0x5B7850
BOOLEAN CInfGame_SwapItemGround(CInfGame* pGame, LONG nContainerId, SHORT nSlotNum, CItem*& pItem, STRREF& errorCode, WORD wCount, BOOLEAN bFromServer)
{
    CGameContainer* pContainer;

    // NOTE: A single attempt, as in the original.
    BYTE rc = pGame->GetObjectArray()->GetDeny(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    // NOTE: A client sends a swap request to the host here
    // (CBaldurMessage 0x431560); multiplayer is removed (ADR 0002).

    CItem* pGroundItem = pContainer->GetItem(nSlotNum);
    if (pGroundItem != NULL) {
        if (pItem == NULL
            && pGroundItem->GetMaxStackable() > 1
            && wCount < pGroundItem->GetUsageCount(0)) {
            // Take part of the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 12276
            UTIL_ASSERT(wCount != 0);

            pGroundItem->SetUsageCount(0, pGroundItem->GetUsageCount(0) - wCount);

            pItem = new CItem(pGroundItem->cResRef, 0, 0, 0, 0, 0);
            pItem->SetUsageCount(0, wCount);
            pItem->m_flags = (pItem->m_flags & ~0x1) | (pGroundItem->m_flags & 0x1);

            pGame->GetObjectArray()->ReleaseDeny(nContainerId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            errorCode = -1;
            return TRUE;
        }

        if (pItem != NULL
            && pGroundItem->GetMaxStackable() > 1
            && pItem->cResRef == pGroundItem->cResRef
            && ((pItem->m_flags ^ pGroundItem->m_flags) & 0x1) == 0) {
            // Add to the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 12286
            UTIL_ASSERT(wCount != 0);

            INT nSpace;
            if (pItem->GetUsageCount(0) < pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0)) {
                nSpace = pItem->GetUsageCount(0);
            } else {
                nSpace = pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0);
            }

            if (wCount >= nSpace) {
                if (pItem->GetUsageCount(0) < pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0)) {
                    wCount = pItem->GetUsageCount(0);
                } else {
                    wCount = pGroundItem->GetMaxStackable() - pGroundItem->GetUsageCount(0);
                }
            }

            pGroundItem->SetUsageCount(0, pGroundItem->GetUsageCount(0) + wCount);

            if (pItem->GetUsageCount(0) == wCount) {
                // NOTE: Uninline.
                g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);
                pItem = NULL;
            } else {
                pItem->SetUsageCount(0, pItem->GetUsageCount(0) - wCount);
            }

            pGame->GetObjectArray()->ReleaseDeny(nContainerId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
            errorCode = -1;
            return TRUE;
        }
    }

    if (pItem == NULL) {
        // Pick the item up.
        pContainer->SetItem(nSlotNum, NULL);

        CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(CItem(),
            nSlotNum,
            FALSE,
            pContainer->GetId(),
            pContainer->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

        pItem = pGroundItem;
    } else if (pGroundItem != NULL && pGroundItem->cResRef == "MISC07") {
        // Gold on the ground goes to the party; the item stays in hand.
        CInfGame* pInfGame = g_pBaldurChitin->GetObjectGame();
        pInfGame->AddPartyGold(pGroundItem->GetUsageCount(0));

        // NOTE: Uninline.
        pInfGame->AddDisposableItem(pGroundItem);

        pContainer->SetItem(nSlotNum, NULL);

        CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(CItem(),
            nSlotNum,
            FALSE,
            pContainer->GetId(),
            pContainer->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    } else {
        // Put the item down, taking whatever lay there. The slot is set by
        // the message from a copy of the item.
        CMessageContainerAddItem* pMessage = new CMessageContainerAddItem(*pItem,
            nSlotNum,
            FALSE,
            pContainer->GetId(),
            pContainer->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->AddDisposableItem(pItem);

        pItem = pGroundItem;
    }

    pGame->GetObjectArray()->ReleaseDeny(nContainerId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
    errorCode = -1;
    return TRUE;
}

// 0x5B81C0
BOOL CInfGame_SwapItemPersonal(CInfGame* pGame, SHORT nPortrait, SHORT nSlotNum, CItem*& pItem, STRREF& errorCode, WORD wCount, BOOLEAN bFromServer)
{
    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(nPortrait);

    CItem* pNewItem = pItem;
    BOOL bResult = TRUE;
    BOOL bEquip = TRUE;
    BOOL bQuickItemSlot = FALSE;

    // NOTE: The original reads an uninitialized local when no item is held;
    // bounded here (P4-10) with a type no slot check matches.
    SHORT nType = 0;

    if (pNewItem != NULL) {
        nType = pNewItem->GetItemType();
    }

    errorCode = -1;

    if (nSlotNum >= CGameSpriteEquipment::NUM_SLOT) {
        errorCode = 9374;
        return FALSE;
    }

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        errorCode = 9373;
        return FALSE;
    }

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != pSprite->m_remotePlayerID) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        errorCode = 20695;
        return FALSE;
    }

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());

    // `bDone` jumps to the final release, as the original's shared exit.
    BOOL bDone = FALSE;

    if (pItem != NULL) {
        DWORD nNotUsableBy = pItem->GetNotUsableBy();

        // NOTE: The original inlines the alignment checks of
        // CRuleTables::IsUsableByAlignment in another order.
        if (!g_pBaldurChitin->GetObjectGame()->GetRuleTables().IsUsableByAlignment(nNotUsableBy, typeAI.m_nAlignment)) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x800000) != 0 && typeAI.m_nRace == CAIObjectType::R_ELF) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x1000000) != 0 && typeAI.m_nRace == CAIObjectType::R_DWARF) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x2000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ELF) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x4000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALFLING) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x8000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HUMAN) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x10000000) != 0 && typeAI.m_nRace == CAIObjectType::R_GNOME) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if ((nNotUsableBy & 0x20000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ORC) {
            bResult = FALSE;
            errorCode = 9382;
        }

        if (!bResult) {
            bDone = TRUE;
        } else if (g_pBaldurChitin->GetObjectGame()->GetRuleTables().ShouldCheckItemRequirements(pItem)) {
            CDerivedStats& stats = pSprite->m_derivedStats;

            if (stats.m_nLevel < pItem->GetMinLevelRequired()) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nSTR < static_cast<SHORT>(pItem->GetMinSTRRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nINT < static_cast<SHORT>(pItem->GetMinINTRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nDEX < static_cast<SHORT>(pItem->GetMinDEXRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nWIS < static_cast<SHORT>(pItem->GetMinWISRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nCON < static_cast<SHORT>(pItem->GetMinCONRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (stats.m_nCHR < static_cast<SHORT>(pItem->GetMinCHRRequired())) {
                bResult = FALSE;
                errorCode = 9382;
            }

            if (!bResult) {
                bDone = TRUE;
            }
        }
    }

    if (!bDone) {
        // Can the slot take the item?
        INT nSlot = nSlotNum;
        CItem** pSlots = pSprite->m_equipment.m_items;

        switch (nSlot) {
        case 0:
            if (pItem != NULL && nType != 1 && nType != 70) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 1:
            if (pItem != NULL
                && nType != 60
                && nType != 61
                && nType != 66
                && nType != 62
                && nType != 63
                && nType != 64
                && nType != 65
                && nType != 67
                && nType != 68) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 2:
            if (pItem != NULL && nType != 3) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 3:
            if (pItem != NULL && nType != 4) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 4:
            if (pItem != NULL && nType != 32) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 5:
            if (pItem != NULL && nType != 6 && nType != 73) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 6:
            if (pItem != NULL && nType != 7 && nType != 72) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 7:
        case 8:
            if (pItem != NULL && nType != 10) {
                errorCode = 9375;
                bResult = FALSE;
                bDone = TRUE;
            }
            break;
        case 10:
            errorCode = 9375;
            bResult = FALSE;
            bDone = TRUE;
            break;
        case 11:
        case 12:
        case 13:
        case 14:
            // Quivers.
            if (pItem != NULL && nType != 5 && nType != 31 && nType != 14) {
                bResult = FALSE;
                errorCode = 9375;
            }
            bEquip = FALSE;
            break;
        case 15:
        case 16:
        case 17:
            // Quick items.
            bQuickItemSlot = TRUE;
            if (pItem != NULL) {
                if (nType != 0
                    && nType != 8
                    && nType != 9
                    && nType != 11
                    && nType != 35
                    && nType != 71) {
                    bResult = FALSE;
                    errorCode = 9375;
                    bEquip = FALSE;
                    break;
                }

                if (pItem->cResRef == "SPWI110Z") {
                    errorCode = 9375;
                    bResult = FALSE;
                } else if (pItem->cResRef == "MISC86") {
                    errorCode = 9375;
                    bResult = FALSE;
                } else if (pItem->GetAbilityCount() == 0) {
                    errorCode = 9375;
                    bResult = FALSE;
                } else if (g_pBaldurChitin->GetObjectGame()->CheckItemUsable(pSprite, pItem) == 0) {
                    errorCode = 9382;
                    bResult = FALSE;
                }
            }
            bEquip = FALSE;
            break;
        case 43:
        case 45:
        case 47:
        case 49:
            // Main hand.
            if (pSprite->m_nWeaponSet != (nSlot - 43) / 2) {
                bEquip = FALSE;
            }

            if (pItem != NULL
                && nType != 15
                && nType != 16
                && nType != 17
                && nType != 44
                && nType != 18
                && nType != 19
                && nType != 20
                && nType != 57
                && nType != 69
                && nType != 21
                && nType != 22
                && nType != 23
                && nType != 24
                && nType != 25
                && nType != 26
                && nType != 27
                && nType != 29
                && nType != 30) {
                bResult = FALSE;
                errorCode = 9375;
            }

            if (nType != 15 && nType != 27 && nType != 18) {
                // A melee weapon: no ranged ability beside an off-hand
                // weapon, no two-handed weapon beside anything.
                CItem* pOffHand = pSlots[nSlot + 1];
                if (pOffHand != NULL
                    && pOffHand->GetItemType() != 47
                    && pOffHand->GetItemType() != 53
                    && pOffHand->GetItemType() != 49
                    && pOffHand->GetItemType() != 41
                    && pItem != NULL) {
                    pItem->Demand();
                    for (INT nAbility = 0; nAbility < pItem->GetAbilityCount() && bResult; nAbility++) {
                        ITEM_ABILITY* pAbility = pItem->GetAbility(nAbility);
                        if (pAbility != NULL) {
                            if ((pAbility->type & 0xFF) == 2) {
                                errorCode = 28765;
                                bResult = FALSE;
                            } else if ((pAbility->type & 0xFF) == 4) {
                                errorCode = 28766;
                                bResult = FALSE;
                            }
                        }
                    }
                    pItem->Release();

                    if (!bResult) {
                        bDone = TRUE;
                        break;
                    }
                }

                if (pItem != NULL
                    && (pItem->GetFlagsFile() & 0x2) != 0
                    && pSlots[nSlot + 1] != NULL) {
                    if (pSlots[nSlot + 1]->GetItemType() == 47
                        || pSlots[nSlot + 1]->GetItemType() == 53
                        || pSlots[nSlot + 1]->GetItemType() == 49
                        || pSlots[nSlot + 1]->GetItemType() == 41) {
                        errorCode = 9380;
                    } else {
                        errorCode = 28758;
                    }
                    bResult = FALSE;
                    bDone = TRUE;
                }
                break;
            }

            // A launcher: nothing in the off hand but a shield, and a shield
            // only beside a sling.
            if (pSlots[nSlot + 1] != NULL) {
                if (pSlots[nSlot + 1]->GetItemType() != 47
                    && pSlots[nSlot + 1]->GetItemType() != 53
                    && pSlots[nSlot + 1]->GetItemType() != 49
                    && pSlots[nSlot + 1]->GetItemType() != 41) {
                    errorCode = 28765;
                    bResult = FALSE;
                    bDone = TRUE;
                    break;
                }

                if (nType != 18) {
                    bResult = FALSE;
                    errorCode = 9380;
                }
            }
            bEquip = FALSE;
            break;
        case 44:
        case 46:
        case 48:
        case 50:
            // Off hand.
            if (pSprite->m_nWeaponSet != (nSlot - 43) / 2) {
                bEquip = FALSE;
            }

            if (pItem != NULL) {
                if (nType == 16
                    || nType == 17
                    || nType == 44
                    || nType == 18
                    || nType == 19
                    || nType == 20
                    || nType == 69
                    || nType == 57
                    || nType == 21
                    || nType == 22
                    || nType == 23
                    || nType == 24
                    || nType == 25
                    || nType == 26
                    || nType == 27
                    || nType == 29
                    || nType == 30) {
                    if ((pItem->GetFlagsFile() & 0x2) != 0) {
                        errorCode = 26342;
                        bResult = FALSE;
                        bDone = TRUE;
                        break;
                    }

                    pItem->Demand();
                    for (INT nAbility = 0; nAbility < pItem->GetAbilityCount() && bResult; nAbility++) {
                        ITEM_ABILITY* pAbility = pItem->GetAbility(nAbility);
                        if (pAbility != NULL
                            && ((pAbility->type & 0xFF) == 2 || (pAbility->type & 0xFF) == 4)) {
                            errorCode = 1314;
                            bResult = FALSE;
                        }
                    }
                    pItem->Release();
                } else if (nType != 47 && nType != 49 && nType != 53 && nType != 41) {
                    errorCode = 9375;
                    bResult = FALSE;
                    bDone = TRUE;
                    break;
                }
            }

            if (pSlots[42] != NULL
                && (pSlots[42]->GetFlagsFile() & 0x2) != 0
                && pItem != NULL) {
                errorCode = 9381;
                bResult = FALSE;
                bDone = TRUE;
                break;
            }

            if (pSlots[nSlot - 1] != NULL && pItem != NULL) {
                CItem* pMainHand = pSlots[nSlot - 1];
                if ((pMainHand->GetFlagsFile() & 0x2) != 0
                    && pMainHand != pSlots[CGameSpriteEquipment::SLOT_FIST]) {
                    errorCode = 9381;
                    bResult = FALSE;
                    bDone = TRUE;
                    break;
                }

                if (nType != 47 && nType != 53 && nType != 49 && nType != 41) {
                    pMainHand->Demand();
                    for (INT nAbility = 0; nAbility < pMainHand->GetAbilityCount() && bResult; nAbility++) {
                        ITEM_ABILITY* pAbility = pMainHand->GetAbility(nAbility);
                        if (pAbility != NULL) {
                            if ((pAbility->type & 0xFF) == 2) {
                                errorCode = 1310;
                                bResult = FALSE;
                            } else if ((pAbility->type & 0xFF) == 4) {
                                errorCode = 1311;
                                bResult = FALSE;
                            }
                        }
                    }
                    pMainHand->Release();
                }
            }
            break;
        default:
            bEquip = FALSE;
            break;
        }
    }

    if (!bDone && bResult) {
        // Immunities to the item and its type.
        if (pNewItem != NULL) {
            STRREF strError;
            CGameEffect* pEffect;
            if (pSprite->m_derivedStats.m_cImmunitiesItemEquip.OnList(pNewItem->cResRef, strError, pEffect)) {
                bResult = FALSE;
                errorCode = strError;
                if (pEffect != NULL) {
                    delete pEffect;
                }
                bDone = TRUE;
            } else if (pSprite->m_derivedStats.m_cImmunitiesItemTypeEquip.OnList(pNewItem->GetItemType(), strError, pEffect)) {
                bResult = FALSE;
                errorCode = strError;
                if (pEffect != NULL) {
                    delete pEffect;
                }
                bDone = TRUE;
            }
        }
    }

    if (!bDone && bResult) {
        CItem** pSlots = pSprite->m_equipment.m_items;

        // A created weapon holds the weapon and quiver slots.
        if (pSlots[42] != NULL
            && ((nSlotNum >= 43 && nSlotNum <= 50) || (nSlotNum >= 11 && nSlotNum <= 14))) {
            errorCode = 10141;
            bResult = FALSE;
            bDone = TRUE;
        } else if (pItem != NULL && pGame->IsItemExclusive(pSprite, nSlotNum, pItem, errorCode)) {
            bResult = FALSE;
            bDone = TRUE;
        } else {
            if (pSlots[nSlotNum] != NULL) {
                // NOTE: Leftovers of Baldur's Gate II's joinable NPCs.
                CString sScriptName(pSprite->m_scriptName);
                if (sScriptName.CompareNoCase("MINSC") == 0 && pSlots[nSlotNum]->cResRef == "MISC84") {
                    errorCode = 10218;
                    bResult = FALSE;
                } else if (sScriptName.CompareNoCase("ALORA") == 0 && pSlots[nSlotNum]->cResRef == "MISC88") {
                    errorCode = 10219;
                    bResult = FALSE;
                } else if (sScriptName.CompareNoCase("EDWIN") == 0 && pSlots[nSlotNum]->cResRef == "MISC89") {
                    errorCode = 10222;
                    bResult = FALSE;
                }
            }

            if (!bResult) {
                bDone = TRUE;
            } else {
                CItem* pOldItem = pSlots[nSlotNum];
                if (pOldItem != NULL) {
                    DWORD dwFlags = pOldItem->GetFlagsFile();
                    if ((dwFlags & 0x10) != 0) {
                        // Cursed.
                        bResult = FALSE;
                        pOldItem->m_flags |= 0x1;
                        errorCode = 16304;
                    }

                    if ((dwFlags & 0x4) == 0) {
                        // Not movable.
                        errorCode = 25697;
                        pOldItem->m_flags |= 0x1;
                        bResult = FALSE;
                        bDone = TRUE;
                    }
                }

                if (!bDone && !bResult) {
                    bDone = TRUE;
                }
            }
        }
    }

    if (!bDone && bQuickItemSlot && pItem != NULL) {
        pItem->Demand();
        ITEM_ABILITY* pAbility = pItem->GetAbility(0);
        if (pAbility == NULL) {
            errorCode = 17317;
            bResult = FALSE;
        } else if ((pItem->m_flags & 0x1) == 0) {
            if (((pAbility->type >> 8) & 0x1) != 0) {
                errorCode = 17316;
                bResult = FALSE;
            }
        } else {
            if (((pAbility->type >> 8) & 0x2) != 0) {
                errorCode = 17316;
                bResult = FALSE;
            }
        }
        pItem->Release();

        if (!bResult) {
            bDone = TRUE;
        }
    }

    if (bDone || !bResult) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return bResult;
    }

    if (pItem != NULL && (pItem->GetFlagsFile() & 0x10) != 0) {
        // A cursed item reveals itself and is equipped.
        pItem->m_flags |= 0x1;
        errorCode = 16304;
        bEquip = TRUE;
    }

    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        errorCode = 9373;
        return FALSE;
    }

    // NOTE: A client sends the swap to the host here (CBaldurMessage
    // 0x431560); multiplayer is removed (ADR 0002).

    CItem** pSlots = pSprite->m_equipment.m_items;
    CItem* pOldItem = pSlots[nSlotNum];

    BOOL bStacked = FALSE;
    if (pOldItem != NULL) {
        if (pItem == NULL
            && pOldItem->GetMaxStackable() > 1
            && wCount < pOldItem->GetUsageCount(0)) {
            // Take part of the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 13383
            UTIL_ASSERT(wCount != 0);

            pOldItem->SetUsageCount(0, pOldItem->GetUsageCount(0) - wCount);

            pItem = new CItem(pOldItem->cResRef, 0, 0, 0, 0, 0);
            pItem->SetUsageCount(0, wCount);
            pItem->m_flags = (pItem->m_flags & ~0x1) | (pOldItem->m_flags & 0x1);

            bStacked = TRUE;
        } else if (pItem != NULL
            && pOldItem->GetMaxStackable() > 1
            && pItem->cResRef == pOldItem->cResRef
            && ((pItem->m_flags ^ pOldItem->m_flags) & 0x1) == 0) {
            // Add to the stack.

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfGame.cpp
            // __LINE__: 13393
            UTIL_ASSERT(wCount != 0);

            INT nSpace;
            if (pItem->GetUsageCount(0) < pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0)) {
                nSpace = pItem->GetUsageCount(0);
            } else {
                nSpace = pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0);
            }

            if (wCount >= nSpace) {
                if (pItem->GetUsageCount(0) < pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0)) {
                    wCount = pItem->GetUsageCount(0);
                } else {
                    wCount = pOldItem->GetMaxStackable() - pOldItem->GetUsageCount(0);
                }
            }

            pOldItem->SetUsageCount(0, pOldItem->GetUsageCount(0) + wCount);

            if (pItem->GetUsageCount(0) == wCount) {
                // NOTE: Uninline.
                pGame->AddDisposableItem(pItem);
                pItem = NULL;
            } else {
                pItem->SetUsageCount(0, pItem->GetUsageCount(0) - wCount);
            }

            bStacked = TRUE;
        }
    }

    if (!bStacked) {
        CGameSpriteEquipment& equipment = pSprite->m_equipment;

        if (equipment.m_selectedWeapon == nSlotNum) {
            equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_FIST;
            equipment.m_selectedWeaponAbility = 0;

            if (!bEquip) {
                if (pSlots[nSlotNum] != NULL) {
                    if (nSlotNum >= 43 && nSlotNum <= 50) {
                        BYTE buttonNum = static_cast<BYTE>(nSlotNum - 43);

                        // NOTE: Uninline.
                        pSprite->SetQuickWeapon(buttonNum, CButtonData());
                        pSprite->SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
                    }

                    pSlots[nSlotNum]->Demand();
                    pSlots[nSlotNum]->Unequip(pSprite, nSlotNum, TRUE, FALSE);

                    ITEM_ABILITY* pAbility = pSlots[nSlotNum]->GetAbility(equipment.m_selectedWeaponAbility);
                    if (pAbility != NULL) {
                        SHORT nLauncherSlot;
                        CItem* pLauncher = pSprite->GetLauncher(pAbility, nLauncherSlot);
                        if (pLauncher != NULL) {
                            pLauncher->Unequip(pSprite, nLauncherSlot, TRUE, FALSE);
                        }
                    }

                    pSlots[nSlotNum]->Release();
                }
            }
        } else {
            INT nSelected = equipment.m_selectedWeapon;
            pSlots[nSelected]->Demand();

            ITEM_ABILITY* pAbility = pSlots[equipment.m_selectedWeapon]->GetAbility(equipment.m_selectedWeaponAbility);
            if (pAbility != NULL) {
                SHORT nLauncherSlot;
                CItem* pLauncher = pSprite->GetLauncher(pAbility, nLauncherSlot);
                if (pLauncher != NULL && pLauncher == pSlots[nSlotNum]) {
                    // The launcher of the ammunition in use goes.
                    pSlots[nSlotNum]->Unequip(pSprite, nSlotNum, TRUE, FALSE);
                    pSlots[nSlotNum] = NULL;

                    pSlots[equipment.m_selectedWeapon]->Unequip(pSprite, equipment.m_selectedWeapon, TRUE, FALSE);
                    equipment.m_selectedWeapon = CGameSpriteEquipment::SLOT_FIST;
                    equipment.m_selectedWeaponAbility = 0;
                }
            }

            pSlots[nSelected]->Release();
        }

        if (bEquip) {
            if (pSlots[nSlotNum] != NULL) {
                pSlots[nSlotNum]->Unequip(pSprite, nSlotNum, TRUE, FALSE);
                pSlots[nSlotNum] = NULL;
            }

            if (pItem != NULL) {
                pItem->Equip(pSprite, nSlotNum, FALSE);
            }
        }

        pSlots[nSlotNum] = pItem;
        pItem = pOldItem;

        if (nSlotNum >= 43 && nSlotNum <= 50) {
            BOOL bSetWeapons = TRUE;
            if (bEquip != TRUE && pSprite->m_nWeaponSet != (nSlotNum - 43) / 2) {
                pSprite->sub_726F20();
                bSetWeapons = FALSE;
            }

            if (bSetWeapons) {
                BOOL bOffHand = nSlotNum == 44 || nSlotNum == 46 || nSlotNum == 48 || nSlotNum == 50;
                if (pOldItem != NULL && !pSprite->sub_737910(bOffHand)) {
                    bResult = FALSE;
                } else {
                    bResult = TRUE;
                    pSprite->sub_726810(static_cast<BYTE>((nSlotNum - 43) / 2));
                }
            }
        }
    }

    g_pBaldurChitin->GetActiveEngine()->UpdatePersonalItemStatus(pSprite->GetId());

    if ((g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID)
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        CMessageSpriteEquipment* pMessage = new CMessageSpriteEquipment(pSprite,
            nCharacterId,
            nCharacterId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    // TODO: The original calls CGameSprite 0x723830 here (it sets
    // m_baseStats byte 0x49 from CRuleTables::sub_546B60, at most 5), which
    // is not in CGameSprite.cpp yet.

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return bResult;
}

// 0x5BE840
SHORT CInfGame_FindEmptyItemPersonalInventory(CInfGame* pGame, SHORT nPortrait)
{
    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(nPortrait);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        for (INT nIndex = 0; nIndex < 24; nIndex++) {
            if (pSprite->m_equipment.m_items[18 + nIndex] == NULL) {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                return static_cast<SHORT>(nIndex + 18);
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    return -1;
}

// 0x5B9BA0
BOOL CInfGame_CheckItemForClassAndKitUsability(CInfGame* pGame, DWORD nClassMask, DWORD nSpecialization, CItem* pItem)
{
    SHORT nType = pItem->GetItemType();
    if (nType == 35 || nType == 11) {
        return TRUE;
    }

    DWORD nNotUsable1 = pItem->GetNotUsableBy();
    DWORD nNotUsable2 = pItem->GetNotUsableBy2();

    if ((nNotUsable1 & nClassMask) == nClassMask) {
        return (nNotUsable2 & nSpecialization) != nSpecialization;
    }

    DWORD nKit = nNotUsable2 & nSpecialization;
    if (nKit != 0
        && (~(nNotUsable1 & nClassMask) & nClassMask & 0x39B) == 0
        && ((nNotUsable1 & 0x4) != 0 || (nClassMask & 0x4) == 0 || (nKit & 0xFF8000) != 0)
        && ((nNotUsable1 & 0x20) != 0 || (nClassMask & 0x20) == 0 || (nKit & 0x38) != 0)
        && ((nNotUsable1 & 0x40) != 0 || (nClassMask & 0x40) == 0 || (nKit & 0x7) != 0)
        && ((nNotUsable1 & 0x400) != 0 || (nClassMask & 0x400) == 0 || (nKit & 0x7FC0) != 0)) {
        return FALSE;
    }

    return TRUE;
}

// 0x5B9C60
BOOL CInfGame_CheckIfPlayerInPortraitSlotCanUseItem(CInfGame* pGame, SHORT nPortrait, CItem* pItem, STRREF* pError, BOOL bAsynch)
{
    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(nPortrait);

    BYTE nThread = bAsynch ? CGameObjectArray::THREAD_ASYNCH : CGameObjectArray::THREAD_1;

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            nThread,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        BOOL bResult = CInfGame_CheckIfSpriteCanUseItem(pGame, pSprite, pItem, pError, bAsynch);

        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            nThread,
            INFINITE);

        return bResult;
    }

    *pError = 9373;

    return FALSE;
}

// 0x5B9D20
BOOL CInfGame_CheckIfSpriteCanUseItem(CInfGame* pGame, CGameSprite* pSprite, CItem* pItem, STRREF* pError, BOOL bAsynch)
{
    BOOL bUsable = TRUE;

    if (pItem != NULL) {
        // NOTE: The original fetches the item type here and drops it.
        pItem->GetItemType();
    }

    *pError = -1;

    if (pItem == NULL) {
        return bUsable;
    }

    DWORD nNotUsable = pItem->GetNotUsableBy();

    CAIObjectType typeAI;
    typeAI.Set(pSprite->GetAIType());

    if (!g_pBaldurChitin->GetObjectGame()->GetRuleTables().IsUsableByAlignment(nNotUsable, typeAI.m_nAlignment)) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x800000) != 0 && typeAI.m_nRace == CAIObjectType::R_ELF) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x1000000) != 0 && typeAI.m_nRace == CAIObjectType::R_DWARF) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x2000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ELF) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x4000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALFLING) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x8000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HUMAN) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x10000000) != 0 && typeAI.m_nRace == CAIObjectType::R_GNOME) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if ((nNotUsable & 0x20000000) != 0 && typeAI.m_nRace == CAIObjectType::R_HALF_ORC) {
        bUsable = FALSE;
        *pError = 9382;
    }

    if (!bUsable) {
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetRuleTables().ShouldCheckItemRequirements(pItem)) {
        if (pSprite->GetActiveStats()->m_nLevel < pItem->GetMinLevelRequired()) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nSTR < static_cast<SHORT>(pItem->GetMinSTRRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nINT < static_cast<SHORT>(pItem->GetMinINTRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nDEX < static_cast<SHORT>(pItem->GetMinDEXRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nWIS < static_cast<SHORT>(pItem->GetMinWISRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nCON < static_cast<SHORT>(pItem->GetMinCONRequired())) {
            bUsable = FALSE;
            *pError = 9382;
        }

        if (pSprite->GetActiveStats()->m_nCHR < static_cast<SHORT>(pItem->GetMinCHRRequired())) {
            *pError = 9382;
            return FALSE;
        }
    }

    if (!bUsable) {
        return FALSE;
    }

    BOOL bResult = CInfGame_CheckItemForClassAndKitUsability(pGame,
        pSprite->GetActiveStats()->m_classMask,
        pSprite->GetActiveStats()->m_nSpecialization,
        pItem);
    if (!bResult) {
        *pError = 9382;
    }

    return bResult;
}
