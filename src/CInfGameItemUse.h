#ifndef CINFGAMEITEMUSE_H_
#define CINFGAMEITEMUSE_H_

#include "mfc.h"

#include "BalDataTypes.h"

class CGameSprite;
class CInfGame;
class CItem;

// NOTE: These are `CInfGame` members in the original (thiscall, `pGame` is
// `this`). They live here until `CInfGame.cpp`'s owner folds them in.

BOOL CInfGame_CheckItemForClassAndKitUsability(CInfGame* pGame, DWORD nClassMask, DWORD nSpecialization, CItem* pItem);
BOOL CInfGame_CheckIfPlayerInPortraitSlotCanUseItem(CInfGame* pGame, SHORT nPortrait, CItem* pItem, STRREF* pError, BOOL bAsynch);
BOOL CInfGame_CheckIfSpriteCanUseItem(CInfGame* pGame, CGameSprite* pSprite, CItem* pItem, STRREF* pError, BOOL bAsynch);

#endif /* CINFGAMEITEMUSE_H_ */
