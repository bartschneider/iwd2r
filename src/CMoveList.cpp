#include "CMoveList.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"

// 0x5170A0
CMoveListEntry::CMoveListEntry()
{
    m_nSpriteId = CGameObjectArray::INVALID_INDEX;
    m_areaResRef = "";
    m_ptDestination.x = 0;
    m_ptDestination.y = 0;
    m_nFacing = 0;
    m_nDelay = 0;
}

// NOTE: Inlined.
CMoveList::CMoveList()
{
}

// 0x59EAD0
CMoveList::~CMoveList()
{
    ClearAll();
}

// 0x4E71B0
void CMoveList::ClearAll()
{
    POSITION pos = GetHeadPosition();
    while (pos != NULL) {
        CMoveListEntry* pEntry = GetNext(pos);
        delete pEntry;
    }
    RemoveAll();
}

// NOTE: The name is a guess: it is called only while the game and its
// master area are marshalled, and it re-applies each pending move to its
// sprite as a timed effect so the save carries it.
//
// 0x5170F0
void CMoveList::Marshal(CGameArea* pArea)
{
    ITEM_EFFECT effect;
    CPoint ptTarget;
    CGameSprite* pSprite;
    BYTE rc;

    CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_JUMPTOAREA);

    POSITION pos = GetHeadPosition();
    while (pos != NULL) {
        CMoveListEntry* pNode = GetNext(pos);

        effect.dwFlags = pNode->m_nFacing;
        effect.effectAmount = pNode->m_nDelay;
        pNode->m_areaResRef.GetResRef(effect.res);
        effect.durationType = 1;
        ptTarget.x = -1;
        ptTarget.y = -1;

        CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect,
            pNode->m_ptDestination,
            pNode->m_nSpriteId,
            ptTarget);

        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(pNode->m_nSpriteId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // TODO: Incomplete. The original adds the sprite's id to the
            // area's list at 0xAAA here, which CGameArea::Marshal then saves
            // with the area's creatures:
            //   pArea->field_AAA.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(pSprite->GetId())));
            // `field_AAA` is declared by the game-file stream's CGameArea.h,
            // not yet merged here.
            pSprite->AddEffect(pEffect, CGameAIBase::EFFECT_LIST_TIMED, TRUE, FALSE);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(pNode->m_nSpriteId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x517210
void CMoveList::CheckLoad(CGameArea* pArea)
{
    CResRef areaResRef = pArea->m_resRef;

    POSITION pos = GetHeadPosition();
    while (pos != NULL) {
        CMoveListEntry* pNode = GetNext(pos);
        if (pNode->m_areaResRef == areaResRef) {
            CGameSprite* pSprite;

            BYTE rc;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(pNode->m_nSpriteId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                pSprite->MoveOntoArea(pArea, pNode->m_ptDestination, pNode->m_nFacing);

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(pNode->m_nSpriteId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }
}

// 0x517300
void CMoveList::AIUpdate()
{
    // TODO: Incomplete.
}
