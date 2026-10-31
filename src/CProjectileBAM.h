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
    // NOTE: The constructor (0x52AD60) leaves +0x1BE, +0x1C2, +0x1D2 and
    // +0x1D8 to the subclasses, and not all of them set every one:
    // CProjectile84DFA4 sets +0x1BE, +0x1C2 and +0x1D8, CProjectile84DF1C
    // only +0x1D8, and CProjectile8510A4 (0x57E030, type 218) all but
    // +0x1D8, which Render reads, so the original mirrors that missile by
    // direction whenever the heap garbage there is 2 to 0x7FFF (the WORD
    // compare is signed). Bounded (ruling P4-10): ours starts all four at
    // 0, so it never mirrors.
    /* 01BE */ DWORD field_1BE = 0;
    /* 01C2 */ BOOL field_1C2 = FALSE;
    /* 01C6 */ DWORD field_1C6;
    /* 01CA */ DWORD field_1CA;
    /* 01CE */ DWORD field_1CE;
    /* 01D2 */ WORD field_1D2 = 0;
    /* 01D4 */ DWORD field_1D4;
    /* 01D8 */ WORD field_1D8 = 0;
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

// Several projectiles of one type fired together (vtable 0x84E030): the
// constructor makes nCount of type nType + 1 (DecodeProjectile); Fire fires
// them all at the target, each with copies of this projectile's effects and
// its speed changed by up to 10 either way, spreads them in pairs to either
// side of the line of flight, and deletes this one.
class CProjectile84E030 : public CProjectileBAM {
public:
    CProjectile84E030(WORD nType, CResRef resRef, WORD nCount, WORD nPaletteType);
    /* 0000 */ ~CProjectile84E030() override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 02A0 */ WORD m_nCount;
    /* 02A2 */ CTypedPtrList<CPtrList, CProjectile*> m_projectiles;
    /* 02BE */ WORD m_nType;
};

// Magic Missile, types 68 to 72 (vtable 0x84E0C4): 1 to 5 missiles of type
// 218.
class CProjectile84E0C4 : public CProjectile84E030 {
public:
    CProjectile84E0C4(WORD nCount, WORD nPaletteType);
};

// One magic missile, type 218 (vtable 0x8510A4).
class CProjectile8510A4 : public CProjectileBAM {
public:
    CProjectile8510A4(WORD nPaletteType);
};

// The shared tail of the SPBRNHND spray family: a CProjectileBAM whose
// +0x2A0 word is the spray animation lifetime. WillScarlettOhara models the
// family as CProjectileSprayLifetime < CProjectileTravelling; the exe
// constructors chain to the CProjectileBAM constructor 0x52AD60 (SPBRNHND's
// 0x532CA0 calls it directly), which is the anchor used here.
class CProjectileSprayLifetime : public CProjectileBAM {
protected:
    // No separate exe function: the family constructors call 0x52AD60
    // directly.
    CProjectileSprayLifetime(CResRef resRef)
        : CProjectileBAM(resRef)
    {
    }

public:
    /* 02A0 */ SHORT m_animationLifetime;
};

// One spray cell a SPBRNHND spawns into its area (vtable 0x84E724). The
// constructor has no standalone exe body: the compiler inlined it at both
// news in SpawnSpray (0x533729 and 0x533806).
class CProjectileSprayCell : public CProjectileSprayLifetime {
public:
    CProjectileSprayCell(CResRef resRef, SHORT nLifetime, SHORT nFacing, const CVidPalette& cPalette);
    /* 0088 */ virtual void AddVisualToArea(CGameArea* pArea, const CPoint& pos, LONG posZ);
};

// The SPBRNHND projectile itself (constructor 0x532CA0, chaining straight to
// the CProjectileBAM constructor 0x52AD60): spawns the one-shot spray cells
// around the impact point, a cell per 32 pixels of flight distance, mirrored
// to both sides of the line of flight.
class CProjectileSPBRNHND : public CProjectileSprayLifetime {
public:
    // 0x533600
    void SpawnSpray(const CPoint& origin, LONG count, SHORT facing, BOOL oneSide);
};

// The follow-up projectile (Magic Missile's second and later missiles,
// vtable 0x84E7B0): on Fire it launches a child projectile of one type
// higher and hands it the effects, then keeps re-firing refreshed children
// from CallBack/AIUpdate while shots remain. The exe constructor 0x533CB0
// chains straight to the CProjectileBAM constructor 0x52AD60; WillScarlettOhara
// models the family under CProjectileTravelling, the anchor used here is the
// exe's actual base. His separate m_targetX/m_targetY are this tree's
// CPoint m_targetPos.
class CProjectileFollowUp : public CProjectileBAM {
public:
    /* 02A0 */ WORD m_remainingShots;
    /* 02A2 */ WORD m_interval;
    /* 02A8 */ WORD m_childType;
    /* 02AA */ CProjectile* m_nextProjectile;
    /* 02AE */ BOOL m_ready;
    /* 02EE */ LONG m_launchHeight;
    /* 02F8 */ WORD m_delay;

    // 0x533F40
    void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
};

#endif /* CPROJECTILEBAM_H_ */
