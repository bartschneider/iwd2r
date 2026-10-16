#ifndef CPROJECTILE84E940_H_
#define CPROJECTILE84E940_H_

#include "CProjectile.h"
#include "CVidBitmap.h"
#include "CVidCell.h"

// A door opening and closing at its target (vtable 0x84E940): the Visual
// Spell Hit effect's mode 0x26 ("DDoorH") and a type of DecodeProjectile.
// It plays "<animation>2", then "<animation>1" four times, then
// "<animation>2" backwards, and arrives. It delivers its effects when fired.
class CProjectile84E940 : public CProjectile {
public:
    CProjectile84E940(const CString& sAnimation, const CString& sSound, const CString& sPalette, DWORD dwFlags, BYTE nTransValue);
    /* 0000 */ ~CProjectile84E940() override;
    /* 000C */ void AIUpdate() override;
    /* 004C */ void Render(CGameArea* pArea, CVidMode* pVidMode, int a3) override;
    /* 006C */ void Fire(CGameArea* pArea, LONG source, LONG target, CPoint targetPos, LONG nHeight, SHORT nType) override;

    /* 0192 */ CVidCell m_animation;
    /* 026C */ CVidBitmap m_palette;
    /* 0326 */ DWORD m_dwFlags;
    /* 032A */ BOOLEAN m_bPalette;
    /* 032B */ BYTE m_nTransValue;
    /* 032C */ CString m_sAnimation;
    /* 0330 */ BYTE m_nState;
};

#endif /* CPROJECTILE84E940_H_ */
