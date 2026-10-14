#ifndef CPROJECTILEAREA_H_
#define CPROJECTILEAREA_H_

#include "CAIObjectType.h"
#include "CProjectileSingleTarget.h"

#include <list>
#include <map>
#include <set>
#include <string>

// An animation and a sound the explosion object of an area projectile plays
// (built at 0x56FDC0 and 0x56FE30 for the second and third).
class CProjectileAreaExplosion {
public:
    /* 0000 */ std::string m_animation;
    /* 0010 */ std::string m_sound;
    /* 0020 */ IcewindCVisualEffect m_visualEffect;
};

class CProjectileAreaExplosion2 : public CProjectileAreaExplosion {
public:
    /* 002C */ BYTE field_2C;
    /* 002E */ LONG field_2E;
};

class CProjectileAreaExplosion3 : public CProjectileAreaExplosion {
public:
    /* 002C */ LONG field_2C;
    /* 0030 */ LONG field_30;
    /* 0034 */ BYTE field_34;
    /* 0035 */ BYTE field_35;
    /* 0036 */ BYTE field_36;
    /* 0038 */ LONG field_38;
    /* 003C */ LONG field_3C;
    /* 0040 */ LONG field_40;
    /* 0044 */ BYTE field_44;
};

// The projectile that affects everything in a radius around the point it
// reaches (vtable 0x84F1C4). Once it has exploded it fires its secondary
// projectile (type m_nSecondaryType + 1) with a copy of its effects at each
// target found, every m_nHitModulus-th time a target is found.
class CProjectileArea : public CProjectileSingleTarget {
public:
    CProjectileArea(WORD nRadius);
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;
    /* 0070 */ void OnArrival() override;
    /* 0088 */ virtual void sub_88();
    /* 008C */ virtual LONG GetDuration(BYTE nUnknown);
    /* 0090 */ virtual std::list<LONG> GetTargets();
    /* 0094 */ virtual void ApplyToTargets(std::list<LONG>& targets);
    /* 0098 */ virtual void CreateSecondary(LONG nTargetId);

    void TargetEnemy(CGameAIBase* pCaster);
    void TargetFriend(CGameAIBase* pCaster);

    /* 02AE */ WORD m_nRadius;
    /* 02B2 */ WORD m_nSecondaryType;
    /* 02B4 */ WORD field_2B4;
    /* 02B6 */ BOOL m_bExploded;
    /* 02BA */ WORD field_2BA;
    /* 02BC */ WORD field_2BC;
    /* 02BE */ CAIObjectType m_targetType;
    /* 02FA */ DWORD field_2FA;
    /* 02FE */ BYTE field_2FE;
    /* 02FF */ BYTE field_2FF;
    /* 0300 */ BOOL m_bCheckForNonSprites;
    /* 0304 */ BOOL m_bAnimation1;
    /* 0308 */ CVidCell m_animation1;
    /* 03E2 */ BOOL m_bAnimation2;
    /* 03E6 */ CVidCell m_animation2;
    /* 04C0 */ LONG m_nDuration;
    /* 04C4 */ std::map<LONG, LONG> m_targetHits;
    /* 04D4 */ LONG m_nTriggerInterval;
    /* 04D8 */ LONG m_nTriggerCountdown;
    /* 04DC */ LONG m_nHitModulus;
    /* 04E0 */ BOOLEAN m_bTravels;
    /* 04E2 */ CProjectileAreaExplosion m_explosion1;
    /* 050E */ CProjectileAreaExplosion2 m_explosion2;
    /* 0540 */ CProjectileAreaExplosion3 m_explosion3;
    /* 0586 */ CSound m_explosionSound;
    /* 05EA */ CSound m_loopingSound;
    /* 064E */ std::set<LONG> m_excludedTargets;
};

// The area projectile of Static Charge (effect 264, vtable 0x850078): it
// strikes the first target found, else the caster's spell ends.
class CProjectileArea850078 : public CProjectileArea {
public:
    CProjectileArea850078();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// The area projectile of Call Lightning (effect 449, vtable 0x850114): it
// strikes the first target found outdoors, else the caster's spell ends.
class CProjectileArea850114 : public CProjectileArea {
public:
    CProjectileArea850114();
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

// An area projectile that leaves its source out (Cloak of Fear, effect 265;
// vtable 0x8504BC).
class CProjectileArea8504BC : public CProjectileArea {
public:
    CProjectileArea8504BC(WORD nRadius);
    /* 0094 */ void ApplyToTargets(std::list<LONG>& targets) override;
};

#endif /* CPROJECTILEAREA_H_ */
