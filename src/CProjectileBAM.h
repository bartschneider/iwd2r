#ifndef CPROJECTILEBAM_H_
#define CPROJECTILEBAM_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"

// The animated projectile that flies to its target (vtable 0x84D9C4).
// CProjectileSingleTarget (0x578110) is a copy of it with an
// IcewindCVisualEffect.
class CProjectileBAM : public CProjectile {
public:
    CProjectileBAM(CResRef resRef);
    /* 0000 */ ~CProjectileBAM() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0080 */ virtual DWORD GetField_E6();
    /* 0084 */ virtual void UpdateBasedOnTargetPoint(LONG x, LONG y);

    void CalculateFxRect(CRect& rFx, CPoint& ptReference, CVidCell* pVidCell);
    void SetColorRange(USHORT nRange, USHORT nColor);
    void SetSequenceDirection(CVidCell* pVidCell);

    /* 0192 */ CVidCell* m_animation;
    /* 0196 */ CVidCell* m_shadow;
    /* 019A */ CVidPalette m_palette;
    /* 01BE */ BOOL field_1BE;
    /* 01C2 */ BOOL field_1C2;
    /* 01C6 */ DWORD field_1C6;
    /* 01CA */ DWORD field_1CA;
    /* 01CE */ DWORD field_1CE;
    /* 01D4 */ DWORD field_1D4;
    /* 01D8 */ WORD field_1D8;
    /* 01DA */ WORD field_1DA;
    /* 01DC */ SHORT m_nDirection;
    /* 01DE */ BOOL field_1DE;
    /* 01E2 */ CVidBitmap field_1E2;
    /* 029C */ BYTE field_29C;
    /* 029D */ BOOLEAN m_bLimitedLifetime;
    /* 029E */ SHORT m_nLifetime;
    /* 02A0 */ BYTE field_2A0;
    /* 02A1 */ BYTE field_2A1;
};

// The invisible projectile an area projectile fires at each target, type 79
// (vtable 0x84DF1C).
class CProjectile84DF1C : public CProjectileBAM {
public:
    CProjectile84DF1C();
};

#endif /* CPROJECTILEBAM_H_ */
