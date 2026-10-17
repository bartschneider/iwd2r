#ifndef CPROJECTILESUNRAY_H_
#define CPROJECTILESUNRAY_H_

#include "CProjectile.h"
#include "CVidCell.h"
#include "CVidPalette.h"

// The sunray spell hit (vtable 0x8512BC; IEex names its constructor
// CProjectileSunrayX_Construct; the original's Baldur\IcewindCProjectiles.cpp
// by its asserts): four sequences stacked from the ground up, as the shaft
// (CProjectileShaft) stacks its sequences, in three blocks of four: the
// rays rising (0-3), standing (4-7) while the duration lasts, and fading
// (8-11), after which it removes itself. Spell hit type 57 of
// DecodeSpellHitProjectile. It delivers its effects when fired.
class CProjectileSunray : public CProjectile {
public:
    static const DWORD STATE_RISING;
    static const DWORD STATE_STANDING;
    static const DWORD STATE_FADING;

    CProjectileSunray(const CString& sAnimation, const CString& sSound, BYTE nColor, BOOL bTransparent, BOOL bRandomFrame, WORD nDuration);
    /* 0000 */ ~CProjectileSunray() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 0192 */ CVidCell m_animation;
    /* 026C */ CVidPalette m_palette;
    /* 0290 */ BOOL m_bTransparent;
    /* 0294 */ BOOL m_bRandomFrame;
    /* 0298 */ WORD m_nDuration;
    /* 029A */ DWORD m_nState;
};

#endif /* CPROJECTILESUNRAY_H_ */
