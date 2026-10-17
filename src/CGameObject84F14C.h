#ifndef CGAMEOBJECT84F14C_H_
#define CGAMEOBJECT84F14C_H_

#include "CGameAnimation.h"
#include "CGameObject.h"
#include "CSound.h"

class CProjectileAreaExplosion2;
class CProjectileAreaExplosion3;
class CProjectileAreaPoints;
class CVidMode;

// A piece of an area projectile's explosion (vtable 0x84F14C; the
// original's Baldur\IcewindCGameTemporal.cpp, by its asserts): an animation
// of the explosion's second or third description (CGameAnimationType84EEB8)
// that flies at a fixed-point velocity (1/1024 of a pixel), bounces off,
// stops at or disappears behind walls, and lives for a number of ticks or,
// with none, to the end of its sequence. A piece of the third description
// may wander from point to point of the explosion's burned points, or burn
// as an incendiary cloud. CGameObject84F0DC makes them.
class CGameObject84F14C : public CGameObject {
public:
    CGameObject84F14C(const CProjectileAreaExplosion2& explosion, CGameArea* pArea, const CPoint& ptExact, LONG posZ, const CPoint& velocity, WORD nLife, BYTE nFade, BYTE nWallMode);
    CGameObject84F14C(const CProjectileAreaExplosion3& explosion, CGameArea* pArea, const CPoint& ptExact, LONG posZ, const CPoint& velocity, WORD nLife, BYTE nFade, BYTE nWallMode);
    /* 0000 */ ~CGameObject84F14C() override;
    /* 000C */ void AIUpdate() override;
    /* 0048 */ void RemoveFromArea() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface) override;

    /* 006E */ BYTE m_terrainTable[16];
    /* 007E */ CGameAnimation m_animation;
    /* 0088 */ BOOL m_bVisible;
    /* 008C */ CPoint m_ptExact;
    /* 0094 */ CPoint m_velocity;
    /* 009C */ SHORT m_nLife;
    /* 009E */ BYTE m_nFade;
    /* 009F */ BYTE m_nWallMode;
    /* 00A0 */ CSound m_sound;
    /* 0104 */ CProjectileAreaPoints* m_pPoints;
    /* 0108 */ LONG m_nPoint;
    /* 010C */ BOOLEAN m_bWander;
    /* 010D */ BOOLEAN m_bIncendiary;
    /* 010E */ LONG m_nTicks;
};

#endif /* CGAMEOBJECT84F14C_H_ */
