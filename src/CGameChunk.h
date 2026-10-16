#ifndef CGAMECHUNK_H_
#define CGAMECHUNK_H_

#include "CGameAnimation.h"
#include "CGameObject.h"
#include "CSound.h"

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

#endif /* CGAMECHUNK_H_ */
