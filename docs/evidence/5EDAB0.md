---
address: 0x5EDAB0
name: CUIControlButtonCharacterAbilitiesPlusMinus::OnLButtonDown
exemption: "ability +/- button press: sets the MINIMUM/MAXIMUM tokens and the help text; UI only, checked against the asm"
ghidra_version: 12.1.4
---

# 0x5EDAB0 CUIControlButtonCharacterAbilitiesPlusMinus::OnLButtonDown

## Decompiled (Ghidra)

Ghidra has no function at 0x5EDAB0 (the code after the class's scalar
deleting destructor is not disassembled into one); the vtable 0x852734 slot
0x18 points here. Read from the asm (objdump of the vanilla exe):

```
5edab0: SEH frame; edi = g_pBaldurChitin->m_pEngineCharacter
5edad6: assert pCharacter != NULL (line 0x37A2 = 14242)
5edafe: switch (m_nID - 16) over 0..11 (jump table 0x5EDD20):
        16/17 STR 0x256E (9582), 18/19 DEX 0x2570 (9584), 20/21 CON 0x256F (9583),
        22/23 INT 0x2571 (9585), 24/25 WIS 0x2572 (9586), 26/27 CHR 0x2573 (9587);
        each loads the screen's minimum (0x177C..) and maximum (0x1782..)
5edbc4: default: assert FALSE (line 0x37D0 = 14288), help -1, min = max = 0
5edbea: if (!m_bActive) return FALSE
5edc18: if ((m_nMouseButtons & LBUTTON) == 0) return FALSE
5edc4d: Format("%d", min) -> token 0x8F3B94 (CScreenCreateChar::TOKEN_MINIMUM)
5edc8b: Format("%d", max) -> token 0x8F3B98 (CScreenCreateChar::TOKEN_MAXIMUM)
5edce1: UpdateHelp(m_pPanel->m_nID, 29, help)
5edcf2: return CUIControlButtonPlusMinus::OnLButtonDown(pt)
```

## Callers

- vtable 0x852734 slot 0x18 (CUIControlButtonCharacterAbilitiesPlusMinus)

## Callees

- 0x780C00 CUtil::UtilAssert
- 0x7FAEE8 CString_Format
- 0x7FC1A5 CMapStringToString_operator[]
- 0x5E6840 CScreenCharacter::UpdateHelp
- 0x4E6460 CUIControlButtonPlusMinus::OnLButtonDown

## Notes

Fixed: our port carried the chargen button's address (0x619220, CScreenCreateChar's AbilitiesPlusMinus) as its label; the character screen's override is at 0x5EDAB0 (vtable slot 0x18). The body matched; added the default case's values (help -1, minimum and maximum 0) after the assert, which ours left uninitialized.
