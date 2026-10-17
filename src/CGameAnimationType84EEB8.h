#ifndef CGAMEANIMATIONTYPE84EEB8_H_
#define CGAMEANIMATIONTYPE84EEB8_H_

#include "CGameAnimationType.h"
#include "CVidCell.h"
#include "IcewindCVisualEffect.h"

#include <string>

class CProjectileAreaExplosion2;
class CProjectileAreaExplosion3;

// The animation of a piece of an area projectile's explosion (vtable
// 0x84EEB8; the original's Baldur\IcewindCGameAnimationTypeEffect.cpp and
// Include\IcewindCGameAnimationTypeEffect.h, by its asserts): one BAM named
// by the explosion's description, drawn with the description's visual
// effect, its sequences played in turn or at random. CGameObject84F14C
// holds it. Its layout up to +0x5E7 is CGameAnimationTypeEffect's, and
// three of its functions are that class's code (the linker merged them);
// it does not derive from it (its constructors and destructor skip
// CGameAnimationTypeEffect's).
class CGameAnimationType84EEB8 : public CGameAnimationType {
public:
    CGameAnimationType84EEB8(const CProjectileAreaExplosion2& explosion, WORD nDirection);
    CGameAnimationType84EEB8(const CProjectileAreaExplosion3& explosion, WORD nDirection);
    /* 0000 */ ~CGameAnimationType84EEB8() override;
    /* 0004 */ void CalculateFxRect(CRect& rFx, CPoint& ptReference, LONG posZ) override;
    /* 0008 */ void CalculateGCBoundsRect(CRect& rGCBounds, const CPoint& pos, const CPoint& ptReference, LONG posZ, LONG nWidth, LONG nHeight) override;
    /* 000C */ void ChangeDirection(SHORT nDirection) override;
    /* 002C */ BOOLEAN DetectedByInfravision() override;
    /* 007C */ BOOL IsMirroring() override;
    /* 0084 */ BOOL IsEndOfSequence() override;
    /* 0088 */ void IncrementFrame() override;
    /* 0090 */ void Render(CInfinity* pInfinity, CVidMode* pVidMode, INT nSurface, const CRect& rectFX, const CPoint& ptNewPos, const CPoint& ptReference, DWORD dwRenderFlags, COLORREF rgbTintColor, const CRect& rGCBounds, BOOL bDithered, BOOL bFadeOut, LONG posZ, BYTE transparency) override;
    /* 00AC */ SHORT SetSequence(SHORT nSequence) override;

    void SetAnimation(const char* szAnimation);

    /* 03FE */ CVidCell* m_currentVidCell;
    /* 0402 */ CVidCell* m_currentVidCellShadow;
    /* 0406 */ CVidCell m_g1VidCell;
    /* 04E0 */ CVidCell m_g1VidCellShadow;
    /* 05BA */ CVidPalette m_palette;
    /* 05DE */ unsigned char field_5DE;
    /* 05DF */ BOOLEAN m_translucent;
    /* 05E0 */ BOOLEAN m_bRender;
    /* 05E1 */ unsigned char field_5E1;
    /* 05E2 */ SHORT m_currentBamSequence;
    /* 05E4 */ SHORT m_currentBamDirection;
    /* 05E6 */ BYTE m_extendDirectionTest;
    /* 05E7 */ BOOLEAN m_bRandomSequence;
    /* 05E8 */ BOOLEAN m_bPlayOnce;
    /* 05EA */ IcewindCVisualEffect m_visualEffect;
    /* 05F6 */ std::string m_sAnimation;
};

#endif /* CGAMEANIMATIONTYPE84EEB8_H_ */
