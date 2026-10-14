#ifndef CPROJECTILESINGLETARGET_H_
#define CPROJECTILESINGLETARGET_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"
#include "CVidPalette.h"
#include "IcewindCVisualEffect.h"

// The animated projectile that flies to one target (vtable 0x850CAC). It has
// the members of CProjectileBAM (0x52AD60) and an IcewindCVisualEffect; the
// spell hit and area projectiles derive from it.
class CProjectileSingleTarget : public CProjectile {
public:
    CProjectileSingleTarget(CResRef resRef);
    /* 0000 */ ~CProjectileSingleTarget() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0080 */ virtual DWORD GetField_E6();
    /* 0084 */ virtual void UpdateBasedOnTargetPoint(LONG x, LONG y);

    /* 0192 */ CVidCell* m_animation;
    /* 0196 */ CVidCell* m_shadow;
    /* 019A */ CVidPalette m_palette;
    /* 01C2 */ BOOL field_1C2;
    /* 01C6 */ DWORD field_1C6;
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
    /* 02A2 */ IcewindCVisualEffect m_visualEffect;
};

#endif /* CPROJECTILESINGLETARGET_H_ */
