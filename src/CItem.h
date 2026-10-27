#ifndef CITEM_H_
#define CITEM_H_

#include "CResItem.h"
#include "CSound.h"
#include "FileFormat.h"

class CGameEffect;
class CGameObject;
class CGameSprite;
class CUIControlTextDisplay;
class CWeaponIdentification;

// The original's CItem has a vtable at 0 (its constructor 0x4E7840 stores
// 0x84CED0 there), so its CResHelper base starts at 0x0004 and cResRef is
// at 0x000C (0x4E785C: lea ebx, [esi+0xC] before CResRef::CResRef).
// tools/gen_fields.py reads the base comment.
class CItem : public CResHelper<CResItem, 1005> /* base 0x0004 */ {
public:
    static const CString VALUE;

    CItem();
    CItem(const CItem& item);
    CItem(const CCreatureFileItem& item);
    CItem(CResRef id, WORD useCount1, WORD useCount2, WORD useCount3, int wear, DWORD flags);
    ~CItem();

    CCreatureFileItem GetItemFile();
    BOOL Demand();
    BOOL Release();
    BOOL ReleaseAll();
    void SetResRef(const CResRef& cNewResRef, BOOL bSetAutoRequest);
    INT GetAbilityCount();
    WORD GetUsageCount(INT nAbility);
    WORD GetMaxUsageCount(INT nAbility);
    void SetUsageCount(INT nAbility, WORD nUseCount);
    void Equip(CGameSprite* pSprite, LONG slotNum, BOOL animationOnly);
    void Unequip(CGameSprite* pSprite, LONG slotNum, BOOL recalculateEffects, BOOL animationOnly);
    WORD GetAnimationType();
    BYTE sub_4E9250(CString& sAnimation, CGameSprite* pSprite);
    ITEM_ABILITY* GetAbility(INT nAbility);
    CGameEffect* GetAbilityEffect(LONG abilityNum, LONG effectNum, CGameObject* pObject);
    INT GetMaxEffectSpellLevel();
    WORD GetItemType();
    SHORT sub_4E9840(const ITEM_ABILITY* ability);
    DWORD GetCriticalHitMultiplier();
    DWORD GetWeight();
    CResRef GetUsedUpItemId();
    STRREF GetGenericName();
    STRREF GetIdentifiedName();
    DWORD GetFlagsFile();
    DWORD GetNotUsableBy();
    DWORD GetNotUsableBy2();
    CResRef GetGroundIcon();
    CResRef GetItemIcon();
    WORD GetMaxStackable();
    DWORD GetBaseValue();
    WORD GetLoreValue();
    STRREF GetDescription();
    CResRef GetDescriptionPicture();
    void LoadWeaponIdentification(CWeaponIdentification& weaponId);
    BYTE GetMinLevelRequired();
    BYTE GetMinSTRRequired();
    BYTE GetMinINTRequired();
    BYTE GetMinDEXRequired();
    BYTE GetMinWISRequired();
    BYTE GetMinCONRequired();
    BYTE GetMinCHRRequired();
    void sub_4EA4A0(CString& sResRef);
    void sub_4EA580(CUIControlTextDisplay* pText, COLORREF rgbColor);
    void sub_4EA750(CUIControlTextDisplay* pText, COLORREF rgbColor);

    CItem& operator=(const CItem& other);
    bool operator==(const CItem& other);

    /* 0014 */ INT m_nAbilities;
    /* 0018 */ WORD m_useCount1;
    /* 001A */ WORD m_useCount2;
    /* 001C */ WORD m_useCount3;
    /* 001E */ WORD m_wear;
    /* 0020 */ DWORD m_flags;
    /* 0024 */ CSound m_useSound[2];
    /* 00EC */ SHORT m_numSounds;
};

#endif /* CITEM_H_ */
