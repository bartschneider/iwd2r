#ifndef CGAMEOBJECT84F0DC_H_
#define CGAMEOBJECT84F0DC_H_

#include "CGameObject.h"
#include "CProjectileArea.h"
#include "CSound.h"
#include "CVidCell.h"
#include "CVidPalette.h"
#include "IcewindCVisualEffect.h"

class CVidMode;

// The explosion of an area projectile (vtable 0x84F0DC; the original's
// Baldur\IcewindCGameFireball.cpp, by its asserts), made where the
// projectile arrives (CProjectileArea::OnArrival 0x56F410). It plays the
// first description's animation at its centre and sends a ring of
// particles out at the projectile's speed until they reach its radius;
// where a particle first enters a square of the ring's grid, it throws a
// flying piece of the second description (CGameObject84F14C), and now and
// then it leaves a lingering piece of the third at the point it reached.
// The particles move in 1/1024 of a pixel, their y scaled by 4/3, in four
// quarters mirrored from two arcs of the circle's octants
// (CVidMode::GetEllipseArcPixelList).
class CGameObject84F0DC : public CGameObject {
public:
    static const BYTE WALL_BOUNCE;
    static const BYTE WALL_STOP;

    // A particle (16 bytes): its offset from the centre and its velocity.
    class Particle {
    public:
        /* 0000 */ LONG y;
        /* 0004 */ LONG x;
        /* 0008 */ LONG vy;
        /* 000C */ LONG vx;
    };

    // The particles' states (+0x1A4).
    static const BYTE STATE_MOVING;
    static const BYTE STATE_THROWN;
    static const BYTE STATE_STOPPED;

    CGameObject84F0DC(const CProjectileAreaExplosion& explosion1, const CProjectileAreaExplosion2& explosion2, const CProjectileAreaExplosion3& explosion3, CGameArea* pArea, const CPoint& pos, SHORT nRadius, BYTE nSpeed, BYTE nWallMode, WORD nDuration);
    /* 0000 */ ~CGameObject84F0DC() override;
    /* 000C */ void AIUpdate() override;
    /* 0048 */ void RemoveFromArea() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface) override;

    /* 006E */ BYTE m_terrainTable[16];
    /* 007E */ WORD field_7E;
    /* 0080 */ WORD field_80;
    /* 008A */ CVidCell m_animation;
    /* 0164 */ CVidPalette m_palette;
    /* 0188 */ WORD m_nDuration;
    /* 018A */ BYTE m_nExpandTicks;
    /* 018B */ BYTE m_nWallMode;
    /* 018C */ LONG m_nGridX;
    /* 0190 */ LONG m_nGridY;
    /* 0194 */ BYTE* m_pGrid;
    /* 0198 */ LONG m_nArc1;
    /* 019C */ LONG m_nArc2;
    /* 01A0 */ Particle* m_pParticles;
    /* 01A4 */ BYTE* m_pStates;
    /* 01A8 */ CSound m_sound;
    /* 020C */ CString field_20C;
    /* 0210 */ CProjectileAreaExplosion2 m_explosion2;
    /* 0242 */ CProjectileAreaExplosion3 m_explosion3;
    /* 0288 */ LONG m_nPieces;
    /* 028C */ LONG m_nTicks;
    /* 0290 */ IcewindCVisualEffect m_visualEffect;
    /* 029C */ LONG m_nLingerCooldown;
    /* 02A0 */ BOOLEAN m_bAnimation;
};

#endif /* CGAMEOBJECT84F0DC_H_ */
