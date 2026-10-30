#include "CAIGroup.h"

#include <math.h>
#include <stdlib.h>

#include "CAITrigger.h"
#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CPathSearch.h"
#include "CSearchBitmap.h"

// 0x8479E4
const LONG CAIGroup::OFFSET_MULTIPLIER = 1000;

// 0x8479E8
const SHORT CAIGroup::FORMATION_NONE = 12;

// 0x8479EA
const SHORT CAIGroup::FORMATION_GATHER = CAIGROUP_FORMATION_GATHER;

// 0x8C6D98
const CPoint CAIGroup::FORMATION_GATHER_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(0, -3),
    CPoint(3, -2),
    CPoint(-3, -2),
    CPoint(3, 2),
    CPoint(-3, 2),
    CPoint(0, 3),
    CPoint(3, 5),
    CPoint(-3, 5),
    CPoint(0, 6),
    CPoint(3, 8),
    CPoint(-3, 8),
    CPoint(0, 9),
};

// 0x8479EC
const SHORT CAIGroup::FORMATION_GATHER_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    10,
    6,
    14,
    2,
    0,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847A04
const SHORT CAIGroup::FORMATION_T = CAIGROUP_FORMATION_T;

// 0x8C6DF8
const CPoint CAIGroup::FORMATION_T_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(0, 0),
    CPoint(4, 0),
    CPoint(-4, 0),
    CPoint(0, 4),
    CPoint(0, 7),
    CPoint(0, 10),
    CPoint(4, 4),
    CPoint(-4, 4),
    CPoint(4, 7),
    CPoint(-4, 7),
    CPoint(4, 10),
    CPoint(-4, 10),
};

// 0x847A08
const SHORT CAIGroup::FORMATION_T_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847A20
const SHORT CAIGroup::FORMATION_RANK = CAIGROUP_FORMATION_RANK;

// 0x8C66E0
const CPoint CAIGroup::FORMATION_RANK_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(-2, 0),
    CPoint(2, 0),
    CPoint(-6, 0),
    CPoint(6, 0),
    CPoint(-10, 0),
    CPoint(10, 0),
    CPoint(-2, 3),
    CPoint(2, 3),
    CPoint(-6, 3),
    CPoint(6, 3),
    CPoint(-10, 3),
    CPoint(10, 3),
};

// 0x847A24
const SHORT CAIGroup::FORMATION_RANK_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847A3C
const SHORT CAIGroup::FORMATION_3BY2 = CAIGROUP_FORMATION_3BY2;

// 0x8C6BA0
const CPoint CAIGroup::FORMATION_3BY2_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(0, 0),
    CPoint(4, 0),
    CPoint(-4, 0),
    CPoint(0, 4),
    CPoint(4, 4),
    CPoint(-4, 4),
    CPoint(0, 8),
    CPoint(4, 8),
    CPoint(-4, 8),
    CPoint(0, 12),
    CPoint(4, 12),
    CPoint(-1, 12),
};

// 0x847A40
const SHORT CAIGroup::FORMATION_3BY2_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847A58
const SHORT CAIGroup::FORMATION_2BY3 = CAIGROUP_FORMATION_2BY3;

// 0x8C6C08
const CPoint CAIGroup::FORMATION_2BY3_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(-2, 0),
    CPoint(2, 0),
    CPoint(-2, 4),
    CPoint(2, 4),
    CPoint(-2, 7),
    CPoint(2, 7),
    CPoint(-6, 0),
    CPoint(6, 0),
    CPoint(-6, 4),
    CPoint(6, 4),
    CPoint(-6, 7),
    CPoint(6, 7),
};

// 0x847A5C
const SHORT CAIGroup::FORMATION_2BY3_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847A74
const SHORT CAIGroup::FORMATION_PROTECT = CAIGROUP_FORMATION_PROTECT;

// 0x8C6740
const CPoint CAIGroup::FORMATION_PROTECT_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(0, -3),
    CPoint(4, 0),
    CPoint(-4, 0),
    CPoint(2, 4),
    CPoint(-2, 4),
    CPoint(0, 0),
    CPoint(0, -6),
    CPoint(4, -3),
    CPoint(-4, -3),
    CPoint(5, 5),
    CPoint(-5, 5),
    CPoint(0, 7),
};

// 0x847A78
const SHORT CAIGroup::FORMATION_PROTECT_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    10,
    6,
    14,
    2,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847A90
const SHORT CAIGroup::FORMATION_4AND2 = CAIGROUP_FORMATION_4AND2;

// 0x8C6D30
const CPoint CAIGroup::FORMATION_4AND2_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(-2, 0),
    CPoint(2, 0),
    CPoint(-6, 0),
    CPoint(6, 0),
    CPoint(-2, 4),
    CPoint(2, 4),
    CPoint(-6, 4),
    CPoint(6, 4),
    CPoint(-2, 8),
    CPoint(2, 8),
    CPoint(-6, 8),
    CPoint(6, 8),
};

// 0x847A94
const SHORT CAIGroup::FORMATION_4AND2_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847AAC
const SHORT CAIGroup::FORMATION_V = CAIGROUP_FORMATION_V;

// 0x8C6C68
const CPoint CAIGroup::FORMATION_V_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(-1, 0),
    CPoint(1, 0),
    CPoint(-2, 3),
    CPoint(2, 3),
    CPoint(-4, 6),
    CPoint(4, 6),
    CPoint(-1, 6),
    CPoint(1, 6),
    CPoint(-2, 9),
    CPoint(2, 9),
    CPoint(-4, 12),
    CPoint(4, 12),
};

// 0x847AB0
const SHORT CAIGroup::FORMATION_V_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847AC8
const SHORT CAIGroup::FORMATION_WEDGE = CAIGROUP_FORMATION_WEDGE;

// 0x8C6618
const CPoint CAIGroup::FORMATION_WEDGE_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(0, 0),
    CPoint(4, 3),
    CPoint(-4, 3),
    CPoint(-8, 6),
    CPoint(8, 6),
    CPoint(0, 6),
    CPoint(-8, 0),
    CPoint(8, 0),
    CPoint(-12, 9),
    CPoint(-6, 9),
    CPoint(6, 9),
    CPoint(12, 9),
};

// 0x847ACC
const SHORT CAIGroup::FORMATION_WEDGE_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847AE4
const SHORT CAIGroup::FORMATION_LINE = CAIGROUP_FORMATION_LINE;

// 0x8C6680
const CPoint CAIGroup::FORMATION_LINE_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(0, 0),
    CPoint(0, 3),
    CPoint(0, 6),
    CPoint(0, 9),
    CPoint(0, 12),
    CPoint(0, 15),
    CPoint(0, 18),
    CPoint(0, 21),
    CPoint(0, 24),
    CPoint(0, 27),
    CPoint(0, 30),
    CPoint(0, 33),
};

// 0x847AE8
const SHORT CAIGroup::FORMATION_LINE_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847B00
const SHORT CAIGroup::FORMATION_S = CAIGROUP_FORMATION_S;

// 0x8C6CC8
const CPoint CAIGroup::FORMATION_S_OFFSETS[CAIGROUP_FORMATION_MAX_SIZE] = {
    CPoint(1, 0),
    CPoint(-1, 2),
    CPoint(1, 4),
    CPoint(-1, 6),
    CPoint(1, 8),
    CPoint(-1, 10),
    CPoint(1, 12),
    CPoint(-1, 14),
    CPoint(1, 16),
    CPoint(-1, 18),
    CPoint(1, 20),
    CPoint(-1, 22),
};

// 0x847B04
const SHORT CAIGroup::FORMATION_S_FACE[CAIGROUP_FORMATION_MAX_SIZE] = {
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
    8,
};

// 0x847B1C
const SHORT CAIGroup::FORMATION_FOLLOW = CAIGROUP_FORMATION_FOLLOW;

// 0x847B1E
const SHORT CAIGroup::DEADZONE = 12;

// 0x847B20
const SHORT CAIGroup::WAIT_BASE = 5;

// 0x847B22
const SHORT CAIGroup::FORMATION_MAX_SIZE = CAIGROUP_FORMATION_MAX_SIZE;

// NOTE: Inlined.
CAIGroup::CAIGroup(SHORT id)
{
    m_groupId = id;
    m_groupChanged = FALSE;
}

// FIXME: `action` should be reference.
//
// 0x404D00
void CAIGroup::GroupAction(CAIAction action, BOOL override, CAIAction* leaderAction)
{
    BOOL bLeader = TRUE;

    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (override) {
            pSprite->ClearActions(FALSE);
            pSprite->m_interrupt = TRUE;
        }

        pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
        pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;

        if (bLeader) {
            bLeader = FALSE;

            const CAIAction& memberAction = leaderAction != NULL ? *leaderAction : action;
            if (memberAction.m_actionID == CAIAction::MOVETOPOINT
                && pSprite->m_derivedStats.m_nEncumberance == 2) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_TOOHEAVY_STOPPED, 1, 0, 0, -1, 0, 0);
            } else {
                CMessage* message = new CMessageAddAction(memberAction,
                    pSprite->GetId(),
                    pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            if (override && action.m_actionID != CAIAction::GROUPATTACK) {
                pSprite->PlaySound(CGameSprite::SOUND_SELECT_ACTION, TRUE, FALSE, FALSE);
            }
        } else {
            if (action.m_actionID == CAIAction::MOVETOPOINT
                && pSprite->m_derivedStats.m_nEncumberance == 2) {
                pSprite->FeedBack(CGameSprite::FEEDBACK_TOOHEAVY_STOPPED, 1, 0, 0, -1, 0, 0);
            } else {
                CMessage* message = new CMessageAddAction(action,
                    pSprite->GetId(),
                    pSprite->GetId());
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }

        pSprite->m_inFormation = TRUE;

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// NOTE: The name is a guess: the offsets of the group's members from the
// leader for a formation, in thousandths of a search square, turned toward
// `nDirection`. Formations 0 and 1 use the T table; 12 (FORMATION_NONE)
// takes the members' own places relative to the leader, as does any member
// past the twelfth. Without `bAtLeastSix` there is one offset per member
// (at least one); with it at least six. The caller frees the array.
//
// 0x405370
CPoint* CAIGroup::GetOffsets(SHORT formationType, SHORT nDirection, BOOL bAtLeastSix)
{
    INT nMembers = m_memberList.GetCount();
    BYTE nOffsets;
    if (!bAtLeastSix) {
        nOffsets = static_cast<BYTE>(nMembers);
        if (nMembers < 2) {
            nOffsets = 1;
        }
    } else if (nMembers < 7) {
        nOffsets = 6;
    } else {
        nOffsets = static_cast<BYTE>(nMembers);
    }

    CPoint* pOffsets = new CPoint[nOffsets];
    for (INT index = 0; index < nOffsets; index++) {
        pOffsets[index].x = 0;
        pOffsets[index].y = 0;
    }

    LONG* pIds = NULL;
    CPoint ptLeader(-1, -1);

    if (nOffsets == 1) {
        return pOffsets;
    }

    if (!m_memberList.IsEmpty()) {
        LONG leaderId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));

        CGameObject* pLeader;
        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(leaderId,
                CGameObjectArray::THREAD_ASYNCH,
                &pLeader,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return pOffsets;
        }

        ptLeader = pLeader->GetPos();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pLeader->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (nOffsets > 12) {
        pIds = new LONG[nMembers];
        LONG* pId = pIds;
        POSITION pos = m_memberList.GetHeadPosition();
        while (pos != NULL) {
            *pId++ = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));
        }
    }

    if (formationType == FORMATION_NONE) {
        if (pIds != NULL) {
            delete[] pIds;
        }

        if (nMembers == 0) {
            return pOffsets;
        }

        pIds = new LONG[nMembers];
        LONG* pId = pIds;
        POSITION pos = m_memberList.GetHeadPosition();
        while (pos != NULL) {
            *pId++ = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));
        }
    }

    pOffsets[0].x = 0;
    pOffsets[0].y = 0;

    // A member's place relative to the leader, in search squares; FALSE when
    // the object array refuses it.
    // NOTE: With FORMATION_NONE and at least six offsets for fewer
    // members, the original reads member ids past the end of its array
    // (unchecked). Bounded: an index past the members counts as a member the
    // object array refuses, leaving (0, 0) unscaled, the visible result of
    // a refused id.
    auto MemberOffset = [&](INT index, CPoint& offset) -> BOOL {
        if (pIds == NULL || index >= nMembers) {
            return FALSE;
        }

        LONG id = pIds[index];
        CGameObject* pMember;
        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(id,
                CGameObjectArray::THREAD_ASYNCH,
                &pMember,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return FALSE;
        }

        offset.x = (pMember->GetPos().x - ptLeader.x) / CPathSearch::GRID_SQUARE_SIZEX;
        offset.y = (pMember->GetPos().y - ptLeader.y) / CPathSearch::GRID_SQUARE_SIZEY;

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return TRUE;
    };

    for (INT index = 0; index < nOffsets; index++) {
        if (index >= 12) {
            if (MemberOffset(index, pOffsets[index])) {
                pOffsets[index].x *= 1000;
                pOffsets[index].y *= 1000;
            }
            continue;
        }

        switch (formationType) {
        case 0:
        case CAIGROUP_FORMATION_T:
            pOffsets[index] = FORMATION_T_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_GATHER:
            pOffsets[index] = FORMATION_GATHER_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_4AND2:
            pOffsets[index] = FORMATION_4AND2_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_3BY2:
            pOffsets[index] = FORMATION_3BY2_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_PROTECT:
            pOffsets[index] = FORMATION_PROTECT_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_2BY3:
            pOffsets[index] = FORMATION_2BY3_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_RANK:
            pOffsets[index] = FORMATION_RANK_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_V:
            pOffsets[index] = FORMATION_V_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_WEDGE:
            pOffsets[index] = FORMATION_WEDGE_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_S:
            pOffsets[index] = FORMATION_S_OFFSETS[index];
            break;
        case CAIGROUP_FORMATION_LINE:
            pOffsets[index] = FORMATION_LINE_OFFSETS[index];
            break;
        case 12:
            if (!MemberOffset(index, pOffsets[index])) {
                continue;
            }
            break;
        }

        pOffsets[index].x *= 1000;
        pOffsets[index].y *= 1000;
    }

    RotateOffsets(pOffsets, nOffsets, nDirection);

    if (pIds != NULL) {
        delete[] pIds;
    }

    return pOffsets;
}

// x87 __ftol (0x7E812C): truncation toward zero, and the integer indefinite
// 0x80000000 for a NaN or a value out of range.
static LONG Ftol(double value)
{
    if (!(value > -2147483649.0 && value < 2147483648.0)) {
        return static_cast<LONG>(0x80000000u);
    }
    return static_cast<LONG>(value);
}

// NOTE: The name is a guess: turns each offset about the leader by
// `nDirection` * 2 degrees, through its length and angle (acos of |x| over
// the length, put in its quadrant).
//
// NOTE (precision): the original's sin and cos are x87 fsin/fcos and its
// products x87 arithmetic; ours are doubles. A product a hair from a whole
// number can truncate one apart (docs/evidence/4058E0.md;
// group/rotate_offsets lets only that through).
//
// 0x4058E0
void CAIGroup::RotateOffsets(CPoint* pOffsets, SHORT nOffsets, SHORT nDirection)
{
    const double pi = 3.1415926535;
    double offset = static_cast<double>(nDirection * 2) * pi / 360.0;

    for (INT index = 0; index < nOffsets; index++) {
        LONG x = pOffsets[index].x;
        LONG y = pOffsets[index].y;
        double length = sqrt(static_cast<double>(y * y + x * x));
        double ratio = static_cast<double>(abs(x)) / length;
        // NOTE: An offset of (0, 0) divides 0 by 0. The original's CRT acos
        // (0x7E7F64) takes the NaN through its domain error path and the
        // offset comes out (0, 0) (seen in group/rotate_offsets); ours
        // takes the angle as 0, which gives the same.
        double angle = ratio != ratio ? 0.0 : acos(ratio);

        if (x < 0) {
            if (y < 0) {
                angle = angle + pi;
            } else {
                angle = pi - angle;
            }
        } else if (y < 0) {
            angle = 6.283185307 - angle;
        }

        angle = angle + offset;
        pOffsets[index].x = Ftol(cos(angle) * length);
        pOffsets[index].y = Ftol(sin(angle) * length);
    }
}

// 0x4052D0
void CAIGroup::SetGroupTriggerId(LONG triggerId)
{
    if (m_memberList.IsEmpty()) {
        return;
    }

    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->m_triggerId = triggerId;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}


// 0x405A10
SHORT* CAIGroup::GetFormationFaces(SHORT formationType, SHORT rotationDegrees)
{
    INT nMembers = m_memberList.GetCount();

    SHORT* faces = new SHORT[nMembers];
    for (INT index = 0; index < nMembers; index++) {
        faces[index] = 0;
    }

    if (m_memberList.GetHeadPosition() == NULL) {
        return faces;
    }

    // The leader is share-locked for the whole call; when it cannot be
    // locked the faces stay zero.
    LONG nLeaderId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));

    CGameSprite* pLeader;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nLeaderId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pLeader),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return faces;
    }

    // Members past the formation's tables, and every member of the
    // "no formation" type, face the way they already do.
    LONG* pMemberIds = NULL;
    if (nMembers > CAIGROUP_FORMATION_MAX_SIZE) {
        pMemberIds = new LONG[nMembers];
        LONG* pId = pMemberIds;
        POSITION pos = m_memberList.GetHeadPosition();
        while (pos != NULL) {
            *pId++ = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));
        }
    }

    if (formationType == FORMATION_NONE) {
        if (pMemberIds != NULL) {
            delete[] pMemberIds;
        }

        pMemberIds = new LONG[nMembers];
        LONG* pId = pMemberIds;
        POSITION pos = m_memberList.GetHeadPosition();
        while (pos != NULL) {
            *pId++ = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));
        }
    }

    for (INT index = 0; index < nMembers; index++) {
        if (index >= CAIGROUP_FORMATION_MAX_SIZE || formationType == FORMATION_NONE) {
            CGameSprite* pSprite;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(pMemberIds[index],
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            faces[index] = pSprite->m_nDirection;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pMemberIds[index],
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            switch (formationType) {
            case CAIGROUP_FORMATION_T:
                faces[index] = FORMATION_T_FACE[index];
                break;
            case CAIGROUP_FORMATION_GATHER:
                faces[index] = FORMATION_GATHER_FACE[index];
                break;
            case CAIGROUP_FORMATION_4AND2:
                faces[index] = FORMATION_4AND2_FACE[index];
                break;
            case CAIGROUP_FORMATION_3BY2:
                faces[index] = FORMATION_3BY2_FACE[index];
                break;
            case CAIGROUP_FORMATION_PROTECT:
                faces[index] = FORMATION_PROTECT_FACE[index];
                break;
            case CAIGROUP_FORMATION_2BY3:
                faces[index] = FORMATION_2BY3_FACE[index];
                break;
            case CAIGROUP_FORMATION_RANK:
                faces[index] = FORMATION_RANK_FACE[index];
                break;
            case CAIGROUP_FORMATION_V:
                faces[index] = FORMATION_V_FACE[index];
                break;
            case CAIGROUP_FORMATION_WEDGE:
                faces[index] = FORMATION_WEDGE_FACE[index];
                break;
            case CAIGROUP_FORMATION_S:
                faces[index] = FORMATION_S_FACE[index];
                break;
            case CAIGROUP_FORMATION_LINE:
                faces[index] = FORMATION_LINE_FACE[index];
                break;
            }
        }

        // Turn the face by the formation's rotation (16 faces to 360
        // degrees); the remainder keeps the sign of the sum.
        SHORT nRotated = static_cast<SHORT>(rotationDegrees * 16 / 360 + faces[index]);
        faces[index] = static_cast<SHORT>(nRotated % 16);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(pLeader->GetId(),
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    if (pMemberIds != NULL) {
        delete[] pMemberIds;
    }

    return faces;
}

// 0x405E00
void CAIGroup::RemoveFromSearch(CSearchBitmap* search)
{
    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            CPoint pos = pSprite->GetPos();
            BYTE nPersonalSpace = pSprite->GetAnimation()->GetPersonalSpace();
            BYTE nEnemyAlly = pSprite->GetAIType().m_nEnemyAlly;

            pos.x /= CPathSearch::GRID_SQUARE_SIZEX;
            pos.y /= CPathSearch::GRID_SQUARE_SIZEY;
            search->RemoveObject(pos,
                nEnemyAlly,
                nPersonalSpace,
                pSprite->field_54A8,
                pSprite->field_7430);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x405F60
void CAIGroup::AddToSearch(CSearchBitmap* search)
{
    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            CPoint pos = pSprite->GetPos();
            BYTE nPersonalSpace = pSprite->GetAnimation()->GetPersonalSpace();
            BYTE nEnemyAlly = pSprite->GetAIType().m_nEnemyAlly;

            pos.x /= CPathSearch::GRID_SQUARE_SIZEX;
            pos.y /= CPathSearch::GRID_SQUARE_SIZEY;
            search->AddObject(pos,
                nEnemyAlly,
                nPersonalSpace,
                pSprite->field_54A8,
                pSprite->field_7430);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x4060C0
LONG CAIGroup::GetGroupLeader()
{
    if (m_memberList.GetCount() > 0) {
        return static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));
    }

    return CGameObjectArray::INVALID_INDEX;
}

// 0x4060E0
LONG* CAIGroup::GetGroupList()
{
    LONG* pList = new LONG[m_memberList.GetCount()];

    int index = 0;
    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        pList[index++] = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));
    }

    return pList;
}

// 0x406110
void CAIGroup::Add(CGameSprite* pSprite)
{
    if (pSprite->m_groupPosition != NULL) {
        // NOTE: Uninline.
        Remove(pSprite);
    }

    pSprite->m_groupPosition = Add(pSprite->m_id);
}

// 0x406170
POSITION CAIGroup::Add(LONG characterId)
{
    SHORT portraitNum = g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(characterId);
    m_groupChanged = TRUE;
    if (portraitNum == -1) {
        return m_memberList.AddTail(reinterpret_cast<LONG*>(characterId));
    }

    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetAt(pos)));
        SHORT memberPortraitNum = g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(memberId);
        if (memberPortraitNum == -1 || memberPortraitNum > portraitNum) {
            return m_memberList.InsertBefore(pos, reinterpret_cast<LONG*>(characterId));
        }
        m_memberList.GetNext(pos);
    }

    return m_memberList.AddTail(reinterpret_cast<LONG*>(characterId));
}

// 0x406210
void CAIGroup::Remove(CGameSprite* pSprite)
{
    m_groupChanged = TRUE;
    if (pSprite->m_groupPosition != NULL) {
        m_memberList.RemoveAt(pSprite->m_groupPosition);
        m_groupChanged = TRUE;
    }
    pSprite->m_groupPosition = NULL;
}

// 0x406250
BOOL CAIGroup::IsPartyLeader()
{
    if (m_memberList.GetCount() == 0) {
        return FALSE;
    }

    LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));
    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(memberId) != -1) {
        return TRUE;
    }

    return g_pBaldurChitin->GetObjectGame()->m_familiars.Find(reinterpret_cast<int*>(memberId)) != NULL;
}

// 0x4062B0
void CAIGroup::Sort()
{
    CAIGroup tempGroup;
    POSITION pos;

    m_groupChanged = TRUE;

    pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));
        tempGroup.Add(memberId);
    }

    m_memberList.RemoveAll();

    pos = tempGroup.m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(tempGroup.m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->m_groupPosition = m_memberList.AddTail(reinterpret_cast<LONG*>(memberId));

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x4063E0
void CAIGroup::GroupSetTarget(CPoint target, BOOL additive, SHORT formationType, CPoint cursor)
{
    if (formationType == 0) {
        FollowLeader(target, additive);
        return;
    }

    if (abs(target.x - cursor.x) < 12 && abs(target.y - cursor.y) < 9) {
        cursor.x = -1;
        cursor.y = -1;
    }

    if (m_memberList.GetCount() == 0) {
        return;
    }

    if (m_memberList.GetCount() == 1) {
        // One member walks to the point itself (0x4064BA-0x406774).
        LONG nMemberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nMemberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        if (pSprite->GetDerivedStats()->m_nEncumberance == 2) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_TOOHEAVY_STOPPED, 1, 0, 0, -1, 0, 0);
        } else {
            CAIAction action(CAIAction::MOVETOPOINT, target, 0, -1);
            action.m_specificID3 = 0;
            action.m_internalFlags = 0;

            if (additive) {
                pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
                pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;
                pSprite->InsertAction(action);
                if (pSprite->m_curAction.m_actionID != CAIAction::MOVETOPOINT) {
                    pSprite->m_interrupt = TRUE;
                }
            } else {
                pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
                pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;
                pSprite->AddAction(action);
                pSprite->m_interrupt = TRUE;
            }

            pSprite->PlaySound(CGameSprite::SOUND_SELECT_ACTION, TRUE, FALSE, FALSE);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nMemberId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }

    // TODO: 0x406774-0x4071CD not ported: a group of two or more takes its
    // formation's offsets (0x405370, 0x405A10) around the point, each
    // member's square adjusted (CGameArea::AdjustTarget), with a Face and,
    // when the game's walk-sound option is on, a SmallWait of rand() % 7.
}

// 0x407280
void CAIGroup::GroupProtectPoint(CPoint target, SHORT formationType, CPoint cursor, LONG range)
{
    // TODO: Incomplete.
}

// 0x407FC0
void CAIGroup::GroupDrawMove(CPoint target, SHORT formationType, CPoint cursor)
{
    if (formationType == 0) {
        // Without a formation only the leader is given the point.
        if (m_memberList.GetCount() != 0) {
            LONG nLeaderId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));

            CGameSprite* pLeader;
            BYTE rc;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nLeaderId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pLeader),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                pLeader->m_targetPoint.x = target.x;
                pLeader->m_targetPoint.y = target.y;
                pLeader->m_groupMove = TRUE;

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nLeaderId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
        return;
    }

    if (abs(target.x - cursor.x) < 12 && abs(target.y - cursor.y) < 9) {
        cursor.x = -1;
        cursor.y = -1;
    }

    BOOL bFirst = TRUE;

    if (m_memberList.GetCount() == 0) {
        return;
    }

    RemoveFromSearch(&g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->m_search);

    CPoint* pOffsets = NULL;

    if (m_memberList.GetHeadPosition() != NULL) {
        INT nIndex = 0;

        POSITION pos = m_memberList.GetHeadPosition();
        while (pos != NULL) {
            LONG nMemberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

            if (bFirst) {
                CGameSprite* pSprite;
                BYTE rc;
                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nMemberId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc != CGameObjectArray::SUCCESS) {
                    continue;
                }

                bFirst = FALSE;

                // The formation faces from the cursor to the point, or from
                // the leader when the cursor is on the point.
                INT cursorX = cursor.x;
                INT cursorY = cursor.y;
                if (cursorX == -1 && cursorY == -1) {
                    CPoint& ptLeader = pSprite->GetPos();
                    cursorX = ptLeader.x;
                    cursorY = ptLeader.y;
                }

                INT dx = target.x - cursorX;
                INT dy = target.y - cursorY;
                double dist = sqrt(static_cast<double>(dx * dx + dy * dy));
                double angle = acos(static_cast<double>(abs(dx)) / dist);

                // NOTE: Same constants and zero-distance behavior as
                // GroupProtectPoint.
                if (dx < 0) {
                    if (dy < 0) {
                        angle = angle + 3.1415926535;
                    } else {
                        angle = 3.1415926535 - angle;
                    }
                } else if (dy < 0) {
                    angle = 6.283185307 - angle;
                }

                INT nAngle = 0;
                double degrees = angle * -360.0 / 6.283185307;
                if (degrees == degrees) {
                    nAngle = static_cast<INT>(degrees);
                }

                pOffsets = GetOffsets(formationType, static_cast<SHORT>(static_cast<SHORT>(90 - nAngle) % 360), FALSE);

                INT targetX = target.x;
                INT targetY = target.y;
                POINT goal;
                goal.x = targetX / CPathSearch::GRID_SQUARE_SIZEX;
                goal.y = targetY / CPathSearch::GRID_SQUARE_SIZEY;

                CPoint& ptLeader = pSprite->GetPos();
                if (ptLeader.x / CPathSearch::GRID_SQUARE_SIZEX != goal.x
                    || ptLeader.y / CPathSearch::GRID_SQUARE_SIZEY != goal.y) {
                    BYTE nPersonalSpace = pSprite->GetAnimation()->GetPersonalSpace();
                    if (!pSprite->GetArea()->AdjustTarget(CPoint(ptLeader.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                              ptLeader.y / CPathSearch::GRID_SQUARE_SIZEY),
                            &goal,
                            nPersonalSpace,
                            10)) {
                        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nMemberId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                        AddToSearch(&g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->m_search);
                        delete[] pOffsets;
                        return;
                    }
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nMemberId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                // Keep the exact point when its square survived the adjustment.
                if (targetX / CPathSearch::GRID_SQUARE_SIZEX == goal.x
                    && targetY / CPathSearch::GRID_SQUARE_SIZEY == goal.y) {
                    target.x = targetX;
                    target.y = targetY;
                } else {
                    target.x = goal.x * CPathSearch::GRID_SQUARE_SIZEX;
                    target.y = goal.y * CPathSearch::GRID_SQUARE_SIZEY;
                }
            }

            INT nOffsetX = (pOffsets[nIndex].x * CPathSearch::GRID_SQUARE_SIZEX) / OFFSET_MULTIPLIER;
            INT nOffsetY = (pOffsets[nIndex].y * CPathSearch::GRID_SQUARE_SIZEY) / OFFSET_MULTIPLIER;

            POINT dest;
            dest.x = (target.x + nOffsetX) / CPathSearch::GRID_SQUARE_SIZEX;
            dest.y = (target.y + nOffsetY) / CPathSearch::GRID_SQUARE_SIZEY;

            CGameSprite* pSprite;
            BYTE rc;
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nMemberId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                CPoint& ptPos = pSprite->GetPos();
                INT destX;
                INT destY;
                if (ptPos.x / CPathSearch::GRID_SQUARE_SIZEX == dest.x
                    && ptPos.y / CPathSearch::GRID_SQUARE_SIZEY == dest.y) {
                    destX = target.x + nOffsetX;
                    destY = target.y + nOffsetY;
                } else {
                    BYTE nPersonalSpace = pSprite->GetAnimation()->GetPersonalSpace();
                    if (pSprite->GetArea()->AdjustTarget(CPoint(ptPos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                             ptPos.y / CPathSearch::GRID_SQUARE_SIZEY),
                            &dest,
                            nPersonalSpace,
                            10)) {
                        destX = target.x + nOffsetX;
                        destY = target.y + nOffsetY;
                    } else {
                        destX = target.x;
                        destY = target.y;
                    }
                }

                pSprite->m_targetPoint.x = destX;
                pSprite->m_targetPoint.y = destY;
                pSprite->m_groupMove = TRUE;

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nMemberId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                nIndex++;
            }
        }
    }

    AddToSearch(&g_pBaldurChitin->GetObjectGame()->GetVisibleArea()->m_search);
    delete[] pOffsets;
}

// 0x408660
void CAIGroup::GroupCancelMove()
{
    if (m_memberList.GetCount() == 0) {
        return;
    }

    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->m_targetPoint.x = -1;
            pSprite->m_targetPoint.y = -1;
            pSprite->m_groupMove = FALSE;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x408720
void CAIGroup::ClearActions()
{
    if (m_memberList.IsEmpty()) {
        return;
    }

    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->DropPath();

            CMessageStopActions* pMessage = new CMessageStopActions(memberId,
                memberId,
                pSprite);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x4088C0
void CAIGroup::GroupSetTarget(LONG iObject)
{
    if (m_memberList.IsEmpty()) {
        return;
    }

    CAIObjectType target(0, 0, 0, 0, 0, 0, 0, 0, iObject, 0, 0);
    CAIObjectType actee(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);

    if (m_memberList.GetCount() > 1) {
        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            actee.Set(pSprite->GetAIType());
            actee.m_nInstance = -1;
            actee.m_sName = CString("");

            GroupAction(CAIAction(CAIAction::GROUPATTACK, actee, 0, 0, 0),
                TRUE,
                NULL);

            // FIXME: Both params immediately destroyed after call which means
            // they are constructed just for the call which leads to pretty
            // ugly invocation.
            CAIAction leaderAction(CAIAction::ATTACK, target, 0, 0, 0);
            GroupAction(CAIAction(CAIAction::ATTACK, target, 0, 0, 0),
                TRUE,
                &leaderAction);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(iObject,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    } else {
        GroupAction(CAIAction(CAIAction::ATTACK, target, 0, 0, 0),
            TRUE,
            NULL);
    }
}

// 0x4091B0
void CAIGroup::AddTrigger(CAITrigger& trigger)
{
    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            CMessageSetTrigger* pMessage = new CMessageSetTrigger(trigger,
                memberId,
                memberId);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x4093E0
BOOL CAIGroup::InList(LONG characterId)
{
    return m_memberList.Find(reinterpret_cast<LONG*>(characterId)) != NULL;
}

// 0x409400
void CAIGroup::FollowLeader(CPoint target, BOOL additive)
{
    CTypedPtrList<CPtrList, CAIAction*> actions;

    if (m_memberList.IsEmpty()) {
        return;
    }

    if (m_memberList.GetHeadPosition() != NULL) {
        CAIAction* action = new CAIAction(CAIAction::LEADER, target, additive, -1);
        actions.AddTail(action);

        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetHead()));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->PlaySoundA(CGameSprite::SOUND_SELECT_ACTION,
                TRUE,
                FALSE,
                FALSE);

            if (additive) {
                pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
                pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;

                if (pSprite->m_curAction.m_actionID != 23
                    && pSprite->m_curAction.m_actionID != 84
                    && pSprite->m_curAction.m_actionID != 83
                    && pSprite->m_curAction.m_actionID != 88) {
                    pSprite->m_interrupt = TRUE;
                }

                while (actions.GetCount() != 0) {
                    CAIAction* action = actions.RemoveTail();
                    pSprite->InsertAction(*action);
                    delete action;
                }
            } else {
                pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
                pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;
                pSprite->m_interrupt = TRUE;

                while (actions.GetCount() != 0) {
                    CAIAction* action = actions.RemoveTail();
                    pSprite->AddAction(*action);
                    delete action;
                }
            }

            actions.RemoveAll();

            pSprite->m_inFormation = TRUE;

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            POSITION pos = m_memberList.GetHeadPosition();
            while (pos != NULL) {
                LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

                CMessageStopFollow* pMessage = new CMessageStopFollow(memberId, memberId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }
        }
    }
}

// 0x409820
void CAIGroup::HandleFollowPath(LONG* pPath, INT nPath, CPoint target, CPoint start, BOOL additive)
{
    // NOTE: CAIAction's constants at 0x847826 (89, Follow) and 0x84783A (99,
    // an action with no ACTION.IDS entry), which CAIAction.h does not
    // declare.
    const SHORT FOLLOW = 89;
    const SHORT ACTION_99 = 99;

    CTypedPtrList<CPtrList, CAIAction*> actions(10);
    BOOL bLeader = TRUE;
    SHORT nIndex = 0;

    POSITION pos = m_memberList.GetHeadPosition();
    while (pos != NULL) {
        LONG memberId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(pos)));

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(memberId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            continue;
        }

        if (bLeader) {
            bLeader = FALSE;

            // NOTE: The original calls GetPos through the vtable here and
            // drops the result (0x4098DE).
            pSprite->GetPos();

            if (pos != NULL) {
                POSITION posNext = pos;
                LONG nextId = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(posNext)));

                // The leader is the caller, the next member the target
                // (m_targetId at +4 holds the next member, 0x409908).
                CMessage* message = new CMessageStartFollow(memberId, nextId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        } else {
            CAIAction* pAction = new CAIAction(FOLLOW, target, nIndex, -1);
            // NOTE: The original stores the start's y as the second
            // specific (0x409A2F); its x is never read.
            pAction->m_specificID2 = start.y;
            actions.AddTail(pAction);

            if (!additive) {
                actions.AddTail(new CAIAction(CAIAction::SMALLWAIT, CPoint(-1, -1), 5, -1));

                pAction = new CAIAction(ACTION_99, CPoint(-1, -1), 0, -1);
                if (pos != NULL) {
                    POSITION posNext = pos;
                    pSprite->m_followLeaderNext = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(posNext)));
                }
                actions.AddTail(pAction);

                pSprite->ClearStoredPaths();
                pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
                pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;
                pSprite->m_interrupt = TRUE;

                while (!actions.IsEmpty()) {
                    pAction = actions.RemoveHead();
                    pSprite->AddAction(*pAction);
                    delete pAction;
                }
            } else {
                pSprite->m_userCommandPause = CGameSprite::USER_OVERRIDE_COUNT;
                pSprite->m_triggerId = CGameObjectArray::INVALID_INDEX;

                SHORT actionID = pSprite->m_curAction.m_actionID;
                if (actionID != CAIAction::MOVETOPOINT
                    && actionID != CAIAction::FACE
                    && actionID != CAIAction::SMALLWAIT
                    && actionID != FOLLOW
                    && actionID != CAIAction::MOVETOPOINTNORECTICLE) {
                    actions.AddTail(new CAIAction(CAIAction::SMALLWAIT, CPoint(-1, -1), 5, -1));

                    pAction = new CAIAction(ACTION_99, CPoint(-1, -1), 0, -1);
                    if (pos != NULL) {
                        POSITION posNext = pos;
                        pSprite->m_followLeaderNext = static_cast<LONG>(reinterpret_cast<INT_PTR>(m_memberList.GetNext(posNext)));
                    }
                    actions.AddTail(pAction);

                    pSprite->ClearStoredPaths();
                    pSprite->m_interrupt = TRUE;
                }

                while (!actions.IsEmpty()) {
                    pAction = actions.RemoveTail();
                    pSprite->InsertAction(*pAction);
                    delete pAction;
                }
            }

            actions.RemoveAll();

            // NOTE: A plain operator new block, as CGameSprite::
            // ClearStoredPaths deletes it.
            int* pPathCopy = static_cast<int*>(::operator new(sizeof(LONG) * nPath));
            memcpy(pPathCopy, pPath, sizeof(LONG) * nPath);
            pSprite->m_pPathTemp.AddTail(pPathCopy);
            pSprite->m_nPathTemp.AddTail(reinterpret_cast<int*>(static_cast<INT_PTR>(nPath)));
        }

        pSprite->m_inFormation = TRUE;

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(memberId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        nIndex++;
    }
}

// 0x68C000
BYTE CAIGroup::GetCount()
{
    return static_cast<BYTE>(m_memberList.GetCount());
}
