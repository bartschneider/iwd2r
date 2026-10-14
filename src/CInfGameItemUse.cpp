#include "CInfGameItemUse.h"

#include "CAIObjectType.h"
#include "CBaldurChitin.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CGameContainer.h"
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
