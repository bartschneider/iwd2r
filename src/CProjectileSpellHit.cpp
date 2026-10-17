#include "CProjectileSpellHit.h"

#include "CBaldurChitin.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CProjectileShaft.h"
#include "CProjectileSunray.h"
#include "CUtil.h"
#include "IcewindMisc.h"

// 0x57E490
CProjectileSpellHit::CProjectileSpellHit(CString sResRef, const IcewindCVisualEffect& visualEffect)
    : CProjectileSingleTarget(CResRef(sResRef))
{
    m_bDrawBehind = FALSE;
    m_bLoopingSequence = FALSE;
    m_nLoopingSequence = 0;
    m_nLoopCount = 0;
    m_bPickRandomSequence = TRUE;
    m_bDestroyOnEndOfSequence = TRUE;
    field_1D4 = 0;
    field_1D8 = 1;
    m_bHasHeight = FALSE;
    field_1C2 = FALSE;
    m_visualEffect = visualEffect;
}

// 0x57E580
void CProjectileSpellHit::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: As in CProjectileSingleTarget::AIUpdate, the time stop caster is
    // compared with the projectile's own id.
    if (pGame->m_nTimeStop != 0 && pGame->m_nTimeStopCaster != m_id) {
        return;
    }

    if (!m_animation->IsEndOfSequence(FALSE)) {
        m_animation->FrameAdvance();
    }

    if (m_bLoopingSequence == TRUE
        && static_cast<DWORD>(static_cast<WORD>(m_animation->m_nCurrentSequence)) == m_nLoopingSequence
        && m_nLoopCount > 0) {
        m_nLoopCount--;
        if (m_animation->IsEndOfSequence(FALSE)) {
            m_animation->FrameSet(0);
        }
    }

    if (m_animation->IsEndOfSequence(FALSE)) {
        if (m_bDestroyOnEndOfSequence == TRUE) {
            if (m_sound.IsSoundPlaying()) {
                field_1DE = FALSE;
                return;
            }
        } else {
            WORD nSequence = static_cast<WORD>(m_animation->m_nCurrentSequence);
            if (static_cast<INT>(nSequence) < static_cast<INT>(static_cast<BYTE>(m_animation->GetNumberSequences(FALSE))) - 1) {
                m_animation->SequenceSet(static_cast<SHORT>(nSequence + 1));
                m_animation->FrameSet(0);
                m_sound.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
                return;
            }

            if (m_sound.IsSoundPlaying()) {
                field_1DE = FALSE;
                return;
            }
        }

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

// 0x57E710
void CProjectileSpellHit::Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType)
{
    (void)source;
    (void)target;
    (void)nHeight;
    (void)nType;

    if (m_bLoopingSequence) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCProjectiles.cpp
        // __LINE__: 1714
        UTIL_ASSERT(!(m_bLoopingSequence && m_bPickRandomSequence));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCProjectiles.cpp
        // __LINE__: 1715
        UTIL_ASSERT(!(m_bLoopingSequence && m_bDestroyOnEndOfSequence));
    }

    // NOTE: The spell hit has no effects of its own; the ones added to it
    // go to its target at once (m_targetId as it stands, not the target
    // argument, which Fire never stores).
    DeliverEffects();

    INT nSequences = static_cast<BYTE>(m_animation->GetNumberSequences(FALSE));
    SHORT nSequence;
    if (m_bPickRandomSequence == TRUE && nSequences > 1) {
        nSequence = static_cast<SHORT>(rand() % nSequences);
    } else {
        nSequence = 0;
    }
    m_animation->SequenceSet(nSequence);

    LONG y = targetPos.y;
    if (m_bDrawBehind == TRUE) {
        y -= 100;
    } else {
        y += 1;
    }

    m_pos.x = targetPos.x;
    m_pArea = pArea;
    m_pos.y = y;

    BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        delete this;
        return;
    }

    CGameObject::AddToArea(pArea, m_pos, 0, CGameObject::LIST_FRONT);
    PlaySound(m_fireSoundRef, m_loopFireSound, FALSE);
}

// -----------------------------------------------------------------------------

// 0x560310
CProjectile* CProjectile::DecodeSpellHitProjectile(DWORD nType, CGameSprite* pSprite, BOOLEAN bPriest)
{
    CProjectileSpellHit* pSpellHit = NULL;
    IcewindCVisualEffect visualEffect;

    switch (nType) {
    case 0:
        return new CProjectile84D8B8();
    case 1:
        pSpellHit = new CProjectileSpellHit("AbjurH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(215, 140, 70, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P01";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M02";
        }
        break;
    case 2:
        pSpellHit = new CProjectileSpellHit("AlterH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(75, 210, 160, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P07";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M08";
        }
        break;
    case 3:
        pSpellHit = new CProjectileSpellHit("InvocH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(107, 6, 201, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P05";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M06";
        }
        break;
    case 4:
        pSpellHit = new CProjectileSpellHit("NecroH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(180, 210, 80, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P06";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M07";
        }
        break;
    case 5:
        pSpellHit = new CProjectileSpellHit("ConjuH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(105, 215, 70, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P02";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M03";
        }
        break;
    case 6:
        pSpellHit = new CProjectileSpellHit("EnchaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(80, 210, 80, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P04";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M05";
        }
        break;
    case 7:
        pSpellHit = new CProjectileSpellHit("IllusH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(80, 90, 210, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        pSpellHit->m_fireSoundRef = "EFF_M34";
        break;
    case 8:
        pSpellHit = new CProjectileSpellHit("DivinH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(100, 70, 210, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        if (bPriest == TRUE) {
            pSpellHit->m_fireSoundRef = "EFF_P03";
        } else {
            pSpellHit->m_fireSoundRef = "EFF_M04";
        }
        break;
    case 9:
        pSpellHit = new CProjectileSpellHit("ArmorH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M48";
        break;
    case 10:
        pSpellHit = new CProjectileSpellHit("SArmorH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M36";
        break;
    case 11:
        pSpellHit = new CProjectileSpellHit("GArmorH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M36";
        break;
    case 12:
        pSpellHit = new CProjectileSpellHit("StrengH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M12";
        break;
    case 13:
        pSpellHit = new CProjectileSpellHit("ConfusH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M16";
        break;
    case 14:
        pSpellHit = new CProjectileSpellHit("SOFlamH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 15:
        pSpellHit = new CProjectileSpellHit("DSpellH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 16:
        pSpellHit = new CProjectileSpellHit("DisintH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M43";
        break;
    case 17:
        pSpellHit = new CProjectileSpellHit("PWSileH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M44";
        break;
    case 18:
        pSpellHit = new CProjectileSpellHit("PWStunH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M38";
        break;
    case 19:
        pSpellHit = new CProjectileSpellHit("FODeatH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M07";
        break;
    case 20:
        pSpellHit = new CProjectileSpellHit("MSwordH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 21:
        pSpellHit = new CProjectileSpellHit("MSumm1H", visualEffect);
        pSpellHit->m_bPickRandomSequence = FALSE;
        pSpellHit->m_bDestroyOnEndOfSequence = TRUE;
        pSpellHit->m_bLoopingSequence = FALSE;
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 22:
        pSpellHit = new CProjectileSpellHit("MSumm2H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 23:
        pSpellHit = new CProjectileSpellHit("MSumm3H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 24:
        pSpellHit = new CProjectileSpellHit("MSumm4H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 25:
        pSpellHit = new CProjectileSpellHit("MSumm5H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 26:
        pSpellHit = new CProjectileSpellHit("MSumm6H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 27:
        pSpellHit = new CProjectileSpellHit("MSumm7H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 28:
        pSpellHit = new CProjectileSpellHit("CFElemH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 29:
        pSpellHit = new CProjectileSpellHit("CEElemH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 30:
        pSpellHit = new CProjectileSpellHit("CWElemH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 31:
        pSpellHit = new CProjectileSpellHit("BlessH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P31";
        break;
    case 32:
        pSpellHit = new CProjectileSpellHit("CurseH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P32";
        break;
    case 33:
        pSpellHit = new CProjectileSpellHit("PrayerH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P31";
        break;
    case 34:
        pSpellHit = new CProjectileSpellHit("RecitaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P44";
        break;
    case 35:
        pSpellHit = new CProjectileSpellHit("CLWounH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P26";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 128, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 36:
        pSpellHit = new CProjectileSpellHit("CMWounH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P26";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 128, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 37:
        pSpellHit = new CProjectileSpellHit("CSWounH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P34";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 128, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 38:
        pSpellHit = new CProjectileSpellHit("CCWounH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P46";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 128, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 39:
        pSpellHit = new CProjectileSpellHit("HealH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P37";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 128, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 40:
        pSpellHit = new CProjectileSpellHit("ASumm1H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 41:
        pSpellHit = new CProjectileSpellHit("ASumm2H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 42:
        pSpellHit = new CProjectileSpellHit("ASumm3H", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 43:
        pSpellHit = new CProjectileSpellHit("SPoisoH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P17";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(255, 128, 0, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 44:
        pSpellHit = new CProjectileSpellHit("NPoisoH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P41";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(192, 128, 0, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 45: {
        CProjectileShaft* pShaft = new CProjectileShaft(CString("CallLiH"), CString(""), 0xFF, TRUE, FALSE, 20);
        pShaft->m_fireSoundRef = CResRef("EFF_P19");
        return pShaft;
    }
    case 46:
        pSpellHit = new CProjectileSpellHit("SChargH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P42";
        break;
    case 47:
        pSpellHit = new CProjectileSpellHit("RParalH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P33";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 0, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 48:
        pSpellHit = new CProjectileSpellHit("FActioH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P40";
        break;
    case 49:
        pSpellHit = new CProjectileSpellHit("MMagicH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P13";
        break;
    case 50:
        pSpellHit = new CProjectileSpellHit("SOOneH", visualEffect);
        pSpellHit->m_fireSoundRef = "EFF_P35";
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(16, 64, 128, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 51:
        pSpellHit = new CProjectileSpellHit("CStrenH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M41";
        break;
    case 52: {
        CProjectileShaft* pShaft = new CProjectileShaft(CString("FStrikH"), CString(""), 0xFF, TRUE, FALSE, 28);
        pShaft->m_fireSoundRef = CResRef("EFF_P16");
        return pShaft;
    }
    case 53:
        pSpellHit = new CProjectileSpellHit("RDeadH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P18";
        break;
    case 54:
        pSpellHit = new CProjectileSpellHit("ResurrH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P18";
        break;
    case 55:
        pSpellHit = new CProjectileSpellHit("CCommaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P47";
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(128, 0, 128, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 56:
        pSpellHit = new CProjectileSpellHit("RWOTFaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P36";
        break;
    case 57:
        return new CProjectileSunray(CString("SunrayX"), CString(""), 0xFF, TRUE, FALSE, 325);
    case 58:
        pSpellHit = new CProjectileSpellHit("SStoneA", visualEffect);
        pSpellHit->m_fireSoundRef = "CRE_P03";
        break;
    case 59:
        pSpellHit = new CProjectileSpellHit("DDoorH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M09";
        break;
    case 60:
        pSpellHit = new CProjectileSpellHit("DDoorH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M09";
        break;
    case 61:
        pSpellHit = new CProjectileSpellHit("CoColdH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 62:
        pSpellHit = new CProjectileSpellHit("SSOrbH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P38";
        break;
    case 63:
        pSpellHit = new CProjectileSpellHit("FireH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            CMessage* message = new CMessageAddEffect(IcewindMisc::CreateEffectColorGlowDissipate(255, 0, 0, 30),
                pSprite->GetId(),
                pSprite->GetId());
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        break;
    case 64:
        pSpellHit = new CProjectileSpellHit("ColdH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 0, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 65:
        pSpellHit = new CProjectileSpellHit("ElectrH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(255, 75, 255, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 66:
        pSpellHit = new CProjectileSpellHit("AcidH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(0, 255, 0, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 67:
        pSpellHit = new CProjectileSpellHit("ParalH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        if (pSprite != NULL) {
            pSprite->AddEffect(IcewindMisc::CreateEffectColorGlowDissipate(45, 0, 90, 30),
                CGameAIBase::EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        break;
    case 68:
        pSpellHit = new CProjectileSpellHit("MRageH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 69:
        pSpellHit = new CProjectileSpellHit("RWOTFaG", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        break;
    case 70:
        pSpellHit = new CProjectileSpellHit("BDeath", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 71:
        pSpellHit = new CProjectileSpellHit("PortalH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 72: {
        CProjectileShaft* pShaft = new CProjectileShaft(CString("SunscoH"), CString(""), 0xFF, TRUE, FALSE, 24);
        pShaft->m_fireSoundRef = CResRef("EFF_P39");
        return pShaft;
    }
    case 73:
        pSpellHit = new CProjectileSpellHit("BBarrH1", visualEffect);
        pSpellHit->m_visualEffect.sub_586A60(TRUE);
        pSpellHit->m_bLoopingSequence = TRUE;
        pSpellHit->m_nLoopingSequence = 1;
        pSpellHit->m_nLoopCount = 165;
        pSpellHit->m_bPickRandomSequence = FALSE;
        pSpellHit->m_bDestroyOnEndOfSequence = FALSE;
        pSpellHit->m_fireSoundRef = "ARE_P20";
        break;
    case 74:
        pSpellHit = new CProjectileSpellHit("BBarrH2", visualEffect);
        pSpellHit->m_visualEffect.sub_586A60(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        pSpellHit->m_bLoopingSequence = TRUE;
        pSpellHit->m_nLoopingSequence = 1;
        pSpellHit->m_nLoopCount = 165;
        pSpellHit->m_bPickRandomSequence = FALSE;
        pSpellHit->m_bDestroyOnEndOfSequence = FALSE;
        break;
    case 75:
        pSpellHit = new CProjectileSpellHit("CoBonH1", visualEffect);
        pSpellHit->m_visualEffect.sub_586A60(TRUE);
        pSpellHit->m_bLoopingSequence = TRUE;
        pSpellHit->m_nLoopingSequence = 1;
        pSpellHit->m_nLoopCount = 265;
        pSpellHit->m_bPickRandomSequence = FALSE;
        pSpellHit->m_bDestroyOnEndOfSequence = FALSE;
        pSpellHit->m_fireSoundRef = "ARE_P21";
        break;
    case 76:
        pSpellHit = new CProjectileSpellHit("CoBonH2", visualEffect);
        pSpellHit->m_bDrawBehind = TRUE;
        pSpellHit->m_visualEffect.sub_586A60(TRUE);
        pSpellHit->m_bLoopingSequence = TRUE;
        pSpellHit->m_nLoopingSequence = 1;
        pSpellHit->m_nLoopCount = 265;
        pSpellHit->m_bPickRandomSequence = FALSE;
        pSpellHit->m_bDestroyOnEndOfSequence = FALSE;
        break;
    case 77:
        pSpellHit = new CProjectileSpellHit("CLDamaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P103";
        break;
    case 78:
        pSpellHit = new CProjectileSpellHit("CMDamaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P103";
        break;
    case 79:
        pSpellHit = new CProjectileSpellHit("CSDamaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P103";
        break;
    case 80:
        pSpellHit = new CProjectileSpellHit("CCDamaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P103";
        break;
    case 81:
        pSpellHit = new CProjectileSpellHit("CDiseaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P108";
        break;
    case 82:
        pSpellHit = new CProjectileSpellHit("PoisonH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P108";
        break;
    case 83:
        pSpellHit = new CProjectileSpellHit("SLivinH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P109";
        break;
    case 84:
        pSpellHit = new CProjectileSpellHit("HarmH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P103";
        break;
    case 85:
        pSpellHit = new CProjectileSpellHit("DestruH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P113";
        break;
    case 86:
        pSpellHit = new CProjectileSpellHit("ExaltaH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P106";
        break;
    case 87:
        pSpellHit = new CProjectileSpellHit("CloudbH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 88:
        pSpellHit = new CProjectileSpellHit("MTouchH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P107";
        break;
    case 89:
        pSpellHit = new CProjectileSpellHit("MTouchH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P107";
        break;
    case 90:
        pSpellHit = new CProjectileSpellHit("CGraceH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 91:
        pSpellHit = new CProjectileSpellHit("SEaterH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bPickRandomSequence = TRUE;
        pSpellHit->m_bLoopingSequence = FALSE;
        pSpellHit->m_fireSoundRef = "EFF_M104";
        break;
    case 92:
        pSpellHit = new CProjectileSpellHit("SWaveH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_P110";
        break;
    case 93:
        pSpellHit = new CProjectileSpellHit("SuffocA", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 94:
        pSpellHit = new CProjectileSpellHit("ADHWilH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M105";
        break;
    case 95:
        pSpellHit = new CProjectileSpellHit("MFMissX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_fireSoundRef = "EFF_M103";
        break;
    case 96:
        pSpellHit = new CProjectileSpellHit("VSpherX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 97:
        pSpellHit = new CProjectileSpellHit("WVDeatH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 98:
        pSpellHit = new CProjectileSpellHit("UWardX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        pSpellHit->m_bLoopingSequence = TRUE;
        pSpellHit->m_nLoopingSequence = 1;
        pSpellHit->m_nLoopCount = 965;
        pSpellHit->m_bPickRandomSequence = FALSE;
        pSpellHit->m_bDestroyOnEndOfSequence = FALSE;
        break;
    case 99:
        pSpellHit = new CProjectileSpellHit("WVHitH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 100:
        pSpellHit = new CProjectileSpellHit("WDeath1", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 101:
        pSpellHit = new CProjectileSpellHit("WDeath2", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 102:
        pSpellHit = new CProjectileSpellHit("DDeath", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 103:
        pSpellHit = new CProjectileSpellHit("DDeath2", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 104:
        pSpellHit = new CProjectileSpellHit("MSumm1X", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        break;
    case 105:
        pSpellHit = new CProjectileSpellHit("ASumm1X", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        break;
    case 106:
        pSpellHit = new CProjectileSpellHit("CEElemX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        break;
    case 107:
        pSpellHit = new CProjectileSpellHit("CFElemX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        break;
    case 108:
        pSpellHit = new CProjectileSpellHit("CWElemX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        pSpellHit->m_bDrawBehind = TRUE;
        break;
    case 109:
        pSpellHit = new CProjectileSpellHit("GELoopX", visualEffect);
        pSpellHit->m_visualEffect.sub_586A90(TRUE);
        break;
    case 110:
        pSpellHit = new CProjectileSpellHit("DAttacH", visualEffect);
        pSpellHit->m_visualEffect.sub_586A60(TRUE);
        pSpellHit->m_fireSoundRef = "CRE_P01";
        break;
    case 111:
        pSpellHit = new CProjectileSpellHit("WoMoonX", visualEffect);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\IcewindCGameEffects.cpp
        // __LINE__: 2981
        UTIL_ASSERT(FALSE);
        break;
    }

    return pSpellHit;
}
