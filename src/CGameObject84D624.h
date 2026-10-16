#ifndef CGAMEOBJECT84D624_H_
#define CGAMEOBJECT84D624_H_

#include "CGameObject.h"
#include "CParticle.h"
#include "CUtil.h"

class CVidMode;

// The sparkles and explosions of particles (vtable 0x84D624; the original's
// Baldur\CParticle.cpp and Include\CParticle.h, by its asserts): the death
// effect's stone and ice explosions, the Sparkle effect (opcode 41) and the
// sparkle trail of projectiles. Its colours are a row of SPRKLCLR.2DA, whose
// row labels name the colour types (CSPARKLE_COLORTYPE_BLUE and so on). The
// names of TYPE_EXPLOSION (the header's assert "m_wType & TYPE_EXPLOSION")
// and m_particleList (the assert "!m_particleList.IsEmpty()") are the
// original's; the others are ours.
class CGameObject84D624 : public CGameObject {
public:
    static const WORD COLORTYPE_ICE;
    static const WORD COLORTYPE_STONE;
    static const WORD EXPLOSION_PUFFS;
    static const WORD TYPE_SPARKLE;
    static const WORD TYPE_SHOWER;
    static const WORD TYPE_EXPLOSION;

    CGameObject84D624(WORD nColorType, WORD wType, WORD nPuffs);
    /* 0000 */ ~CGameObject84D624() override;
    /* 0008 */ void AddToArea(CGameArea* pNewArea, const CPoint& pos, LONG posZ, BYTE listType) override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;

    void GenerateExplosion();

    // NOTE: Inlined (Include\CParticle.h, line 511): the point an explosion
    // flies away from.
    void SetSource(const CPoint& ptSource)
    {
        // __FILE__: .\Include\CParticle.h
        // __LINE__: 511
        UTIL_ASSERT(m_wType & TYPE_EXPLOSION);

        m_ptSource = ptSource;
    }

    /* 006E */ CVidMode* m_pVidMode;
    /* 0072 */ CTypedPtrList<CPtrList, CParticle*> m_particleList;
    /* 008E */ COLORREF m_rgbColors[5];
    /* 00A2 */ BYTE m_nFadeHeights[4];
    /* 00A6 */ CRect m_rBounding;
    /* 00B6 */ WORD m_wType;
    /* 00B8 */ DWORD m_nPuff;
    /* 00BC */ WORD m_nPuffs;
    /* 00BE */ CPoint m_ptSource;
    /* 00C6 */ LONG m_nGravity;
};

#endif /* CGAMEOBJECT84D624_H_ */
