#ifndef CRESITEM_H_
#define CRESITEM_H_

#include "BalDataTypes.h"
#include "CRes.h"

class CResItem : public CRes {
public:
    CResItem();
    ~CResItem();
    void* Demand();
    ITEM_ABILITY* GetAbility(int nAbility);
    ITEM_EFFECT* GetAbilityEffect(int nAbility, int nEffect);
    int GetAbilityEffectNo(int nAbility);
    int GetEquipedEffectNo();
    BOOL Parse(void* pData);
    ITEM_EFFECT* GetEquipedEffect(int nEffect);

    /* 0050 */ ITEM_ABILITY* m_pAbilities;
    /* 0054 */ ITEM_EFFECT* m_pEffects;
    /* 0058 */ ITEM_HEADER* m_pHeader;
    /* 005C */ BOOL m_bParsed;
};

#endif /* CRESITEM_H_ */
