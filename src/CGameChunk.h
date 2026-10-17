#ifndef CGAMECHUNK_H_
#define CGAMECHUNK_H_

#include "CGameAnimation.h"
#include "CGameObject.h"
#include "CSound.h"
#include "CVidCell.h"
#include "CVidPalette.h"

class CVidMode;

// A chunk a shattered or gibbed creature leaves (vtable 0x85BF10; the
// original's Baldur\ObjCreature.cpp, by its asserts). CGameSprite::Shatter
// (0x70A810) throws them up from the creature; each falls, bounces up to
// three times, slides off walls and fades out where it lies. The class name
// is IEex's (CGameChunk_Construct); the member names are ours.
class CGameChunk : public CGameObject {
public:
    CGameChunk(USHORT animationID, BYTE* colorRangeValues, const CString& sSound, CGameArea* pArea, const CPoint& pos, const CPoint& velocity, LONG posZ, LONG nPosZDelta, BYTE nLife, BYTE nFade);
    /* 0000 */ ~CGameChunk() override;
    /* 000C */ void AIUpdate() override;
    /* 0028 */ BOOLEAN CanSaveGame(STRREF& strError) override;
    /* 0048 */ void RemoveFromArea() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface) override;

    /* 006E */ BYTE m_terrainTable[16];
    /* 007E */ CGameAnimation m_animation;
    /* 0088 */ BOOL m_bMoving;
    /* 008C */ CPoint m_velocity;
    /* 0094 */ LONG m_posZDelta;
    /* 0098 */ BYTE m_nBounces;
    /* 0099 */ BYTE m_nLife;
    /* 009A */ BYTE m_nFade;
    /* 009C */ CSound m_sound;
};

// A piece the cloud explosions throw (vtable 0x85BF7C; ObjCreature.cpp by
// its asserts): CGameObject84F14C's flight with an animation of
// CGameAnimationType::SetAnimationType (an effect animation ID and colour
// range values) facing its velocity, and a sound it plays now and then
// (one chance in 200 a tick). CGameObject85BFE8 and the cloud projectiles
// (0x52E4D0, 0x52EC80, not ported) make them.
class CGameObject85BF7C : public CGameObject {
public:
    CGameObject85BF7C(USHORT animationID, BYTE* colorRangeValues, const CString& sSound, CGameArea* pArea, const CPoint& ptExact, LONG posZ, const CPoint& velocity, WORD nLife, BYTE nFade, BYTE nWallMode);
    /* 0000 */ ~CGameObject85BF7C() override;
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
};

// The explosion of a cloud spell (vtable 0x85BFE8; ObjCreature.cpp by its
// asserts): CGameObject84F0DC's ring of particles, with one of six
// hardcoded looks (a fire pillar, three clouds of effect animations 0x500,
// 0x600 and 0x700, and 0x800) instead of an area projectile's
// descriptions. Its particles throw CGameObject85BF7C pieces; it draws
// nothing itself. Its callers (0x52E940, 0x52F1C0) are not ported.
class CGameObject85BFE8 : public CGameObject {
public:
    class Particle {
    public:
        /* 0000 */ LONG y;
        /* 0004 */ LONG x;
        /* 0008 */ LONG vy;
        /* 000C */ LONG vx;
    };

    CGameObject85BFE8(BYTE nType, BYTE* colorRangeValues, CGameArea* pArea, const CPoint& pos, SHORT nRadius, BYTE nSpeed, BYTE nWallMode, WORD nDuration);
    /* 0000 */ ~CGameObject85BFE8() override;
    /* 000C */ void AIUpdate() override;
    /* 0048 */ void RemoveFromArea() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int nSurface) override;

    /* 006E */ BYTE m_terrainTable[16];
    /* 007E */ WORD m_nFlyingAnimation;
    /* 0080 */ WORD m_nLingeringAnimation;
    /* 0082 */ BYTE m_colorRangeValues[7];
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
    /* 020C */ CString m_sPieceSound;
};

#endif /* CGAMECHUNK_H_ */
