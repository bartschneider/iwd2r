#include "CProjectile84DFA4.h"

#include "CBaldurChitin.h"
#include "CInfGame.h"

// 0x530300
CProjectile84DFA4::CProjectile84DFA4()
    : CProjectileBAM(CResRef("SPBOOM"))
{
    m_animation->SequenceSet(0);
    m_palette.SetRange(0, 67, *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
    m_animation->SetPalette(m_palette);
    field_1BE = FALSE;
    field_1C2 = FALSE;
    field_1C6 = 0;
    field_1D4 = 0;
    m_fireSoundRef = "";
    field_1D8 = 1;
}

// 0x5306F0
void CProjectile84DFA4::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileBAM::AIUpdate, the time stop caster is compared
    // with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    m_animation->FrameAdvance();

    if (m_animation->IsEndOfSequence(FALSE)) {
        RemoveFromArea();

        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
            CGameObjectArray::THREAD_ASYNCH,
            NULL,
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            delete this;
            return;
        }
    }

    m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
}

// 0x530510
void CProjectile84DFA4::PlayAt(CGameArea* pArea, CPoint pos)
{
    m_pos = pos;
    m_pArea = pArea;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CGameObject::AddToArea(pArea, m_pos, 0, CGameObject::LIST_FRONT);

    // NOTE: Inlined PlaySound.
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);
}
