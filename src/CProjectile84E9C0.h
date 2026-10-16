#ifndef CPROJECTILE84E9C0_H_
#define CPROJECTILE84E9C0_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"

// An animation played once at its target, in its own palette when one is
// given (vtable 0x84E9C0): the Visual Spell Hit effect's local projectile
// ("SHAir", "SHEarth", "SHWater" with the palettes "SHAir1".."SHAir7" and so
// on, "SPFDEATH") and many types of DecodeProjectile. It delivers its
// effects when fired and arrives at the end of its sequence.
class CProjectile84E9C0 : public CProjectile {
public:
    CProjectile84E9C0(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue);
    /* 0000 */ ~CProjectile84E9C0() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 0192 */ CVidCell m_animation;
    /* 026C */ CVidBitmap m_palette;
    /* 0326 */ DWORD m_dwFlags;
    /* 032A */ BOOLEAN m_bPalette;
    /* 032B */ BYTE m_nTransValue;
};

#endif /* CPROJECTILE84E9C0_H_ */
