#include "CProjectileArea.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameObject84D624.h"
#include "CGameObject84F0DC.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CParticle.h"
#include "CUtil.h"
#include "IcewindMisc.h"

#include <math.h>

// 0x56EDD0
CProjectileArea::CProjectileArea(WORD nRadius)
    : CProjectileSingleTarget(CResRef("SPMAGMIS"))
{
    m_explosion2.field_2C = 0;
    m_explosion2.field_2E = 0x7FFFFFFF;
    m_explosion3.m_pPoints = NULL;
    m_explosion3.field_30 = 0;
    m_explosion3.field_34 = 0;
    m_explosion3.field_35 = 0;
    m_explosion3.field_36 = 0;
    m_explosion3.field_38 = 250;
    m_explosion3.field_3C = 6;
    m_explosion3.field_40 = 30;
    m_explosion3.field_44 = 0;
    field_2B4 = 0;
    field_2BA = 0;
    field_2BC = 0;
    field_2FA = 0;
    field_2FE = 0;
    field_2FF = 0;
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    m_sVisualEffect = "";
    field_182 = CGameObjectArray::INVALID_INDEX;
    m_nSecondaryType = 78;
    m_bExploded = FALSE;
    m_targetType.Set(CAIObjectType::ANYONE);
    m_nRadius = nRadius;
    m_bAnimation1 = FALSE;
    m_bAnimation2 = FALSE;
    m_bCheckForNonSprites = FALSE;
    m_nTriggerCountdown = 0;
    m_bTravels = FALSE;
    field_1DE = FALSE;
    m_nDuration = 45;
    m_nTriggerInterval = 10000;
    m_nHitModulus = 10;
}

// 0x56FAF0
void CProjectileArea::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileSingleTarget::AIUpdate, the time stop caster is
    // compared with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    if (!m_bExploded) {
        if (!m_bTravels) {
            m_pos.x = m_targetPos.x;
            m_pos.y = m_targetPos.y;
            OnArrival();
            return;
        }

        m_animation->FrameAdvance();

        LONG dy = m_targetPos.y - m_pos.y;
        LONG dx = m_targetPos.x - m_pos.x;
        LONG nStep = m_speed + 1;
        if (dy * dy * 16 / 9 + dx * dx <= nStep * nStep) {
            OnArrival();
            return;
        }

        UpdateBasedOnTargetPoint(m_targetPos.x, m_targetPos.y);

        if (m_bSparkle) {
            // A puff of sparkles at the projectile.
            CGameObject84D624* pSparkle = new CGameObject84D624(m_nSparkleColour,
                CGameObject84D624::TYPE_SPARKLE,
                CGameObject84D624::EXPLOSION_PUFFS);
            pSparkle->AddToArea(m_pArea, m_pos, m_posZ, CGameObject::LIST_FRONT);
        }

        m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
        return;
    }

    if (m_bAnimation1) {
        m_animation1.FrameAdvance();
    }

    if (m_bAnimation2) {
        m_animation2.FrameAdvance();
    }

    if (--m_nTriggerCountdown <= 0) {
        std::list<LONG> targets = GetTargets();
        ApplyToTargets(targets);
        m_nTriggerCountdown = m_nTriggerInterval;
    }

    if (!m_explosion2.m_sound.empty() && !m_loopingSound.IsSoundPlaying()) {
        m_loopingSound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }

    if (--m_nDuration <= 0) {
        RemoveSelf();
    }
}

// 0x56F3F0
void CProjectileArea::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    CProjectileSingleTarget::Render(pArea, pVidMode, a3);
}

// 0x56F820
void CProjectileArea::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)nHeight;
    (void)nType;

    if (pArea == NULL) {
        return;
    }

    m_sourceId = source;
    m_targetId = target;
    m_pArea = pArea;

    CGameObject* pSource;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
            CGameObjectArray::THREAD_ASYNCH,
            &pSource,
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (!m_effectList.IsEmpty()) {
        CGameEffect* pEffect = m_effectList.GetTail();
        if (pEffect != NULL) {
            m_nDuration = GetDuration(static_cast<BYTE>(pEffect->m_casterLevel));
        }
    }

    CPoint ptStart;
    GetSourcePosition(m_sourceId, ptStart, TRUE);
    if (!m_bTravels) {
        ptStart = targetPos;
    }

    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        LONG nSourceId = m_sourceId;
        delete this;

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nSourceId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return;
    }

    CGameObject::AddToArea(pArea,
        ptStart,
        DetermineHeight(static_cast<CGameSprite*>(pSource)),
        CGameObject::LIST_FRONT);
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);

    m_targetPos.x = targetPos.x;
    m_pos.x = ptStart.x;
    m_posExactX = ptStart.x << CParticle::RESOLUTION_INC;
    m_posExactY = ((ptStart.y << CParticle::RESOLUTION_INC) * 4) / 3;
    m_targetPos.y = targetPos.y;
    m_pos.y = ptStart.y;

    LONG dy = (m_targetPos.y * 4) / 3 - (m_pos.y * 4) / 3;
    LONG dx = m_targetPos.x - m_pos.x;
    LONG nDistance = static_cast<LONG>(sqrt(static_cast<double>(dx * dx + dy * dy)) + 0.5);

    if (nDistance == 0) {
        OnArrival();
    } else {
        m_deltaX = (dx << CParticle::RESOLUTION_INC) * m_speed / nDistance;
        m_deltaY = (dy << CParticle::RESOLUTION_INC) * m_speed / nDistance;
        m_nDirection = GetDirection(m_targetPos);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x56F410
void CProjectileArea::OnArrival()
{
    if (m_callBackProjectile != CGameObjectArray::INVALID_INDEX) {
        CProjectile* pProjectile;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_callBackProjectile,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pProjectile),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        pProjectile->CallBack();

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_callBackProjectile,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    m_bExploded = TRUE;
    field_1DE = FALSE;

    PlaySound(m_arrivalSoundRef, m_loopArrivalSound, TRUE);

    // The points the explosion's lingering pieces burn at. NOTE: The
    // original never deletes them here; the last piece does (none made, they
    // stay).
    if (!m_explosion3.m_animation.empty()) {
        m_explosion3.m_pPoints = new CProjectileAreaPoints();
    }

    if (!m_explosion2.m_animation.empty()
        || !m_explosion3.m_animation.empty()
        || !m_explosion1.m_animation.empty()) {
        // Its particles stop at walls.
        new CGameObject84F0DC(m_explosion1,
            m_explosion2,
            m_explosion3,
            m_pArea,
            m_pos,
            static_cast<SHORT>(m_nRadius),
            static_cast<BYTE>(m_speed),
            CGameObject84F0DC::WALL_STOP,
            static_cast<WORD>(m_nDuration));
    }

    if (!m_explosion1.m_sound.empty()) {
        CString sSound(m_explosion1.m_sound.c_str());
        m_explosionSound.SetResRef(CResRef(sSound), TRUE, TRUE);
        m_explosionSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
        m_explosionSound.Play(m_pos.x, m_pos.y, 0, FALSE);
    }

    if (!m_explosion2.m_sound.empty()) {
        CString sSound(m_explosion2.m_sound.c_str());
        m_loopingSound.SetResRef(CResRef(sSound), TRUE, TRUE);
        m_loopingSound.SetLoopingFlag(TRUE);
        m_loopingSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
    }
}

// 0x78E730
void CProjectileArea::sub_88()
{
}

// 0x5703E0
LONG CProjectileArea::GetDuration(BYTE nUnknown)
{
    (void)nUnknown;
    return m_nDuration;
}

// 0x56FED0
std::list<LONG> CProjectileArea::GetTargets()
{
    CTypedPtrList<CPtrList, LONG*> found;

    m_pArea->GetAllInRange(m_posVertList,
        m_pos,
        m_targetType,
        m_nRadius,
        m_terrainTable,
        found,
        TRUE,
        m_bCheckForNonSprites);
    m_pArea->GetAllInRangeBack(m_pos,
        m_targetType,
        m_nRadius,
        m_terrainTable,
        found,
        TRUE,
        FALSE,
        m_bCheckForNonSprites);

    // Forget the targets that are no longer in range.
    std::map<LONG, LONG>::iterator it = m_targetHits.begin();
    while (it != m_targetHits.end()) {
        BOOL bFound = FALSE;
        POSITION pos = found.GetHeadPosition();
        while (pos != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(found.GetNext(pos)));
            if (nId == it->first) {
                bFound = TRUE;
                break;
            }
        }

        if (bFound) {
            ++it;
        } else {
            std::map<LONG, LONG>::iterator next = it;
            ++next;
            m_targetHits.erase(it);
            it = next;
        }
    }

    std::list<LONG> targets;

    POSITION pos = found.GetHeadPosition();
    while (pos != NULL) {
        LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(found.GetNext(pos)));

        if (m_targetHits.find(nId) == m_targetHits.end()) {
            m_targetHits[nId] = 0;
        }

        if (m_targetHits[nId] % m_nHitModulus == 0) {
            targets.push_back(nId);
        }

        m_targetHits[nId]++;
    }

    return targets;
}

// 0x5701B0
void CProjectileArea::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CreateSecondary(*it);
    }
}

// 0x5701E0
void CProjectileArea::CreateSecondary(LONG nTargetId)
{
    CGameObject* pTarget;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTargetId,
        CGameObjectArray::THREAD_ASYNCH,
        &pTarget,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (m_excludedTargets.find(pTarget->GetId()) != m_excludedTargets.end()) {
        // NOTE: The original returns without releasing the share
        // (0x57024A).
        return;
    }

    if ((pTarget->GetObjectType() & CGameObject::TYPE_AIBASE) != 0 || m_bCheckForNonSprites) {
        if (pTarget->GetObjectType() != CGameObject::TYPE_SPRITE) {
            pTarget->GetPos();
        }

        if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE
            && IcewindMisc::IsDead(static_cast<CGameSprite*>(pTarget)) == TRUE) {
            // NOTE: The original returns without releasing the share
            // (0x5702A6).
            return;
        }

        if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
            static_cast<CGameSprite*>(pTarget)->m_posOld = pTarget->GetPos();
        }

        CProjectile* pProjectile = CProjectile::DecodeProjectile(m_nSecondaryType + 1, NULL, 0);
        pProjectile->m_sourceRes = m_sourceRes;
        pProjectile->m_nSpellLevel = m_nSpellLevel;
        pProjectile->m_fireSoundRef = CResRef("");

        POSITION pos = m_effectList.GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pEffect = m_effectList.GetNext(pos);
            pProjectile->AddEffect(pEffect->Copy());
        }

        pProjectile->Fire(m_pArea, m_id, nTargetId, pTarget->GetPos(), 50, 0);

        // NOTE: The original releases a share of the callback projectile
        // here (0x570384), which it never took; the target's share is
        // released below.
        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_callBackProjectile,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// 0x5704E0
void CProjectileArea::TargetEnemy(CGameAIBase* pCaster)
{
    if (pCaster != NULL) {
        if (pCaster->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ENEMY;
            m_targetType.Set(type);
        } else {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ALLY;
            m_targetType.Set(type);
        }
    }
}

// 0x5703F0
void CProjectileArea::TargetFriend(CGameAIBase* pCaster)
{
    if (pCaster != NULL) {
        if (pCaster->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ALLY;
            m_targetType.Set(type);
        } else {
            CAIObjectType type(0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0);
            type.m_nEnemyAlly = CAIObjectType::EA_ENEMY;
            m_targetType.Set(type);
        }
    }
}

// -----------------------------------------------------------------------------

// 0x571E80
CProjectileArea84F580::CProjectileArea84F580()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then 200.
    : CProjectileArea(256)
{
    // NOTE: Inlined: the travel animation of a name that is not empty.
    const char* szTravel = "FirebaT";
    if (strlen(szTravel) != 0) {
        field_1DE = TRUE;
        SetTravelAnimation(CResRef(szTravel));
        m_bTravels = TRUE;
    } else {
        field_1DE = FALSE;
        m_bTravels = FALSE;
    }

    m_fireSoundRef = "TRA_06";
    m_visualEffect.sub_586A90(TRUE);

    m_explosion1.m_animation = "FirebaX";
    m_explosion1.m_sound = "RNG_M03";
    m_explosion1.m_visualEffect.sub_586A90(TRUE);

    m_explosion2.m_animation = "FirebaR";
    m_explosion2.m_visualEffect.sub_586A90(TRUE);
    m_explosion2.field_2C = TRUE;
    m_explosion2.field_2E = 20;

    m_explosion3.m_animation = "FirebaA";
    m_explosion3.m_visualEffect.sub_586A90(TRUE);

    m_nTriggerCountdown = 0;
    m_speed <<= 1;
    m_explosion3.field_34 = TRUE;
    m_explosion3.field_35 = TRUE;
    m_explosion3.field_36 = TRUE;
    m_nTriggerInterval = 10000;
    m_nHitModulus = 10;
    m_nDuration = 45;
    field_1D8 = 16;
    m_nRadius = 200;
}

// 0x572080
void CProjectileArea::SetTravelAnimation(const CResRef& resRef)
{
    if (m_animation != NULL) {
        delete m_animation;
    }

    m_animation = new CVidCell(resRef, FALSE);
}

// 0x574970
CProjectileArea850078::CProjectileArea850078()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then the WORD at
    // 0x84F07C (275).
    : CProjectileArea(256)
{
    m_nRadius = 275;
}

// 0x5749A0
void CProjectileArea850078::ApplyToTargets(std::list<LONG>& targets)
{
    if (!targets.empty()) {
        CreateSecondary(targets.front());
        return;
    }

    CGameObject* pSource;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
        CGameSprite* pSprite = static_cast<CGameSprite*>(pSource);
        // NOTE: The sprite effect and intensity are the bytes at 0x85BB4B
        // (2) and 0x85BB4E (0).
        pSprite->StartSpriteEffect(2, 0, 30, FALSE);
        pSprite->FeedBack(CGameSprite::FEEDBACK_STATICCHARGEEND, 0, 0, 0, -1, 0, 0);
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// -----------------------------------------------------------------------------

// 0x574A70
CProjectileArea850114::CProjectileArea850114()
    // NOTE: The radius is the WORD at 0x84F1BC (256), then the WORD at
    // 0x84F084 (275).
    : CProjectileArea(256)
{
    m_nRadius = 275;
}

// 0x574AA0
void CProjectileArea850114::ApplyToTargets(std::list<LONG>& targets)
{
    CGameObject* pSource;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        &pSource,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return;
    }

    CGameSprite* pSprite = NULL;
    if (pSource->GetObjectType() == CGameObject::TYPE_SPRITE) {
        pSprite = static_cast<CGameSprite*>(pSource);
    }

    if ((pSource->GetArea()->m_header.m_areaType & 0x1) != 0) {
        if (!targets.empty()) {
            CreateSecondary(targets.front());
        } else if (pSprite != NULL) {
            // NOTE: The sprite effect and intensity are the bytes at
            // 0x85BB4B (2) and 0x85BB4E (0).
            pSprite->StartSpriteEffect(2, 0, 30, FALSE);
            pSprite->FeedBack(CGameSprite::FEEDBACK_CALLLIGHTNINGEND, 0, 0, 0, -1, 0, 0);
        }
    } else {
        // NOTE: Indoors the original calls FeedBack on the source without
        // checking that it is a sprite (0x574B36). Bounded (ruling P4-10):
        // a source that is not a sprite gets no feedback.
        if (pSprite != NULL) {
            pSprite->FeedBack(CGameSprite::FEEDBACK_CALLLIGHTNINGEND, 0, 0, 0, -1, 0, 0);
        }
    }

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_sourceId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
}

// -----------------------------------------------------------------------------

// 0x575B00
CProjectileArea8504BC::CProjectileArea8504BC(WORD nRadius)
    : CProjectileArea(nRadius)
{
}

// 0x575B30
void CProjectileArea8504BC::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        if (*it == m_sourceId) {
            targets.erase(it);
            break;
        }
    }

    CProjectileArea::ApplyToTargets(targets);
}

// -----------------------------------------------------------------------------

// 0x575CD0
CProjectileArea8505F4::CProjectileArea8505F4(WORD nRadius)
    : CProjectileArea(nRadius)
{
}

// 0x575CF0
void CProjectileArea8505F4::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc != CGameObjectArray::SUCCESS) {
            // NOTE: The original returns here, leaving the other targets
            // (0x575D36).
            return;
        }

        if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
            && !static_cast<CGameSprite*>(pObject)->GetDerivedStats()->m_spellStates.test(SPLSTATE_MOLD_TOUCH)) {
            CreateSecondary(*it);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            // One target only.
            return;
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x575DD0
CProjectileArea850690::CProjectileArea850690(WORD nRadius)
    : CProjectileArea(nRadius)
{
}

// 0x575DF0
void CProjectileArea850690::ApplyToTargets(std::list<LONG>& targets)
{
    for (std::list<LONG>::iterator it = targets.begin(); it != targets.end(); ++it) {
        CGameObject* pObject;
        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(*it,
            CGameObjectArray::THREAD_ASYNCH,
            &pObject,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                && !static_cast<CGameSprite*>(pObject)->GetDerivedStats()->m_spellStates.test(SPLSTATE_SHROUD_OF_FLAME)) {
                CreateSecondary(*it);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(*it,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// -----------------------------------------------------------------------------

// 0x5868C0
CProjectileAreaPoints::Point::Point(LONG x, LONG y, BYTE bTaken)
{
    this->x = x;
    this->y = y;
    m_bTaken = bTaken;
}

// 0x5868E0
CProjectileAreaPoints::CProjectileAreaPoints()
{
    m_nRefCount = 0;
    m_points.reserve(512);
}

// -----------------------------------------------------------------------------

// 0x56FEC0
LONG CProjectileAreaExplosion3::GetChanceRange()
{
    return 10000;
}
