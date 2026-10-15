#ifndef CGAMESPRITEBUTTONLISTS_H_
#define CGAMESPRITEBUTTONLISTS_H_

#include "mfc.h"

#include "BalDataTypes.h"

class CButtonData;
class CGameButtonList;
class CGameSprite;
class CResRef;

// TODO: These functions are `CGameSprite` members in the original (thiscall,
// `pSprite` is `this`). This file is a temporary seam in the manner of ruling
// M5-1 (`CInfGameItemUse`): `CGameSprite.*` belongs to another stream this
// wave, so the action bar's button lists live here until they can become
// members.

CGameButtonList* CGameSprite_GetSpellQuickButtons(CGameSprite* pSprite, const BYTE& nClass);
CGameButtonList* CGameSprite_GetDomainQuickButtons(CGameSprite* pSprite, const BYTE& nClass, DWORD nDomain);
CGameButtonList* CGameSprite_GetInnateQuickButtons(CGameSprite* pSprite);
CGameButtonList* CGameSprite_sub_716E80(CGameSprite* pSprite, BOOL bFlag);
CGameButtonList* CGameSprite_sub_717250(CGameSprite* pSprite, BOOL bFlag);
CGameButtonList* CGameSprite_sub_717FC0(CGameSprite* pSprite, const CResRef& cResRef, UINT nCount);
void CGameSprite_sub_716770(CGameSprite* pSprite, CButtonData* pButtonData);
void CGameSprite_sub_718390(CGameSprite* pSprite, const CResRef& cResRef, BYTE nClass, BYTE nLevel, CButtonData& buttonData);

#endif /* CGAMESPRITEBUTTONLISTS_H_ */
