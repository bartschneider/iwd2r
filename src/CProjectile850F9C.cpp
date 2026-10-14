#include "CProjectile850F9C.h"

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CInfGame.h"
#include "CParticle.h"

// 0x57CAC0
CProjectile850F9C::CProjectile850F9C(const CString& sAnimation, const CString& sSound, BYTE nLoops, BYTE nDelay, const IcewindCVisualEffect& visualEffect)
{
    m_callBackProjectile = CGameObjectArray::INVALID_INDEX;
    field_326 = 0;
    field_32A = 0;
    field_32B = 0;

    m_glow.SetResRef(CResRef(sAnimation), FALSE, TRUE, TRUE);
    m_glow.FrameSet(0);

    m_sound.Stop();
    m_sound.SetResRef(CResRef(sSound), TRUE, TRUE);
    if (m_sound.m_nLooping == 0) {
        m_sound.SetFireForget(TRUE);
    }
    m_sound.SetChannel(4, reinterpret_cast<DWORD_PTR>(m_pArea));
    m_fireSoundRef = sSound;

    m_nLoops = nLoops;
    if (m_nLoops > m_glow.GetSequenceLength(2, FALSE)) {
        m_glow.SequenceSet(0);
        m_nLoops -= m_glow.GetSequenceLength(2, FALSE);
    } else {
        m_glow.SequenceSet(2);
        m_nLoops = 0;
    }

    m_nDelay = nDelay;
    m_bHasHeight = TRUE;
    m_visualEffect = visualEffect;
}

// 0x57CE80
CProjectile850F9C::~CProjectile850F9C()
{
}

// 0x57CEE0
void CProjectile850F9C::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileSingleTarget::AIUpdate, the time stop caster is
    // compared with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    if (m_nDelay > 0) {
        m_nDelay--;
        return;
    }

    if (m_nLoops > 0) {
        m_nLoops--;
        if (m_glow.IsEndOfSequence(FALSE)) {
            m_glow.SequenceSet(1);
            m_glow.FrameSet(0);
        } else {
            m_glow.FrameAdvance();
        }
    } else {
        if (m_glow.m_nCurrentSequence != 2) {
            m_glow.SequenceSet(2);
            m_glow.FrameSet(0);
        } else {
            if (m_glow.IsEndOfSequence(FALSE)) {
                OnArrival();
                return;
            }

            m_glow.FrameAdvance();
        }
    }

    m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
}

// 0x57CFB0
void CProjectile850F9C::Render(CGameArea* pArea, CVidMode* pVidMode, int a3)
{
    // TODO: Incomplete. The original draws m_glow at the caster through
    // CInfinity's FX functions.
    (void)pArea;
    (void)pVidMode;
    (void)a3;
}

// 0x57D230
void CProjectile850F9C::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)targetPos;
    (void)nType;

    m_sourceId = source;
    m_targetId = target;
    m_pArea = pArea;

    CGameObject* pTarget;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(target,
        CGameObjectArray::THREAD_ASYNCH,
        &pTarget,
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        // NOTE: The original returns without deleting the projectile.
        return;
    }

    CPoint ptTarget = pTarget->GetPos();

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    CPoint ptStart;
    GetSourcePosition(m_sourceId, ptStart, TRUE);

    m_sound.Play(ptStart.x, ptStart.y, 0, FALSE);

    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CGameObject::AddToArea(pArea, ptStart, nHeight, CGameObject::LIST_FRONT);

    m_posExactX = ptStart.x << CParticle::RESOLUTION_INC;
    m_targetPos.x = ptTarget.x;
    m_posExactY = ((ptStart.y << CParticle::RESOLUTION_INC) * 4) / 3;
    m_targetPos.y = ptTarget.y;

    DeliverEffects();
}
