#ifndef CPROJECTILESHAFT_H_
#define CPROJECTILESHAFT_H_

#include "CProjectile.h"
#include "CVidCell.h"
#include "CVidPalette.h"

// A column of light or fire standing on its target for a while (vtable
// 0x84E8C0; IEex names its constructor CProjectileShaft_Construct): the
// sequences of its animation stacked from the ground up. It delivers its
// effects when fired. The Visual Spell Hit effect's flame strike (mode
// 0x23) and holy smite (0x24), and the spell hits Call Lightning, Flame
// Strike and Sunscorch of DecodeSpellHitProjectile.
class CProjectileShaft : public CProjectile {
public:
    CProjectileShaft(const CString& sAnimation, const CString& sSound, BYTE nColor, BOOL bTransparent, BOOL bRandomFrame, WORD nDuration);
    /* 0000 */ ~CProjectileShaft() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 0192 */ CVidCell m_animation;
    /* 026C */ CVidPalette m_palette;
    /* 0290 */ BOOL m_bTransparent;
    /* 0294 */ BOOL m_bRandomFrame;
    /* 0298 */ WORD m_nDuration;
};

#endif /* CPROJECTILESHAFT_H_ */
