#ifndef CPROJECTILE850F9C_H_
#define CPROJECTILE850F9C_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "IcewindCVisualEffect.h"

// The glow a caster shows while casting (effect 235; vtable 0x850F9C): it
// plays its animation's sequence 2 after a delay, loops sequence 1 or 0 for
// the casting time, then plays sequence 2 again and arrives.
class CProjectile850F9C : public CProjectile {
public:
    CProjectile850F9C(const CString& sAnimation, const CString& sSound, BYTE nLoops, BYTE nDelay, const IcewindCVisualEffect& visualEffect);
    /* 0000 */ ~CProjectile850F9C() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 0192 */ CVidCell m_glow;
    /* 026C */ CVidBitmap field_26C;
    /* 0326 */ DWORD field_326;
    /* 032A */ BYTE field_32A;
    /* 032B */ BYTE field_32B;
    /* 032C */ BYTE m_nLoops;
    /* 032D */ BYTE m_nDelay;
    /* 032E */ IcewindCVisualEffect m_visualEffect;
};

#endif /* CPROJECTILE850F9C_H_ */
