#ifndef CPROJECTILESPELLHIT_H_
#define CPROJECTILESPELLHIT_H_

#include "CProjectileSingleTarget.h"

// The animation a spell plays on its target, built by
// DecodeSpellHitProjectile (vtable 0x851234). It delivers its effects the
// moment it is fired and only plays its animation and sound.
class CProjectileSpellHit : public CProjectileSingleTarget {
public:
    CProjectileSpellHit(CString sResRef, const IcewindCVisualEffect& visualEffect);
    /* 000C */ void AIUpdate() override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 02AE */ BOOLEAN m_bDrawBehind;
    /* 02AF */ BOOLEAN m_bLoopingSequence;
    /* 02B0 */ DWORD m_nLoopingSequence;
    /* 02B4 */ LONG m_nLoopCount;
    /* 02B8 */ BOOLEAN m_bPickRandomSequence;
    /* 02B9 */ BOOLEAN m_bDestroyOnEndOfSequence;
};

#endif /* CPROJECTILESPELLHIT_H_ */
