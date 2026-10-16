#ifndef CPROJECTILE84DFA4_H_
#define CPROJECTILE84DFA4_H_

#include "CProjectileBAM.h"

// The burst a projectile or an effect plays once at a point (SPBOOM, vtable
// 0x84DFA4): the Visual Spell Hit effect's modes 0x20..0x22 and some types
// of DecodeProjectile. It is not fired: PlayAt adds it at the point, and it
// removes itself at the end of its sequence.
class CProjectile84DFA4 : public CProjectileBAM {
public:
    CProjectile84DFA4();
    /* 000C */ void AIUpdate() override;
    /* 0088 */ virtual void PlayAt(CGameArea* pArea, CPoint pos);
};

#endif /* CPROJECTILE84DFA4_H_ */
