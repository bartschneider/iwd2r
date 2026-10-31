---
address: 0x4BAFC0
name: CGameEffectNon_CumulativeLuck::ApplyEffect
kind: read-only
exemption: "a one-instruction stat add; the unit test pins the sign (tests/test_effects.cpp, non-cumulative luck (133))"
ghidra_version: 12.1.4
---

# 0x4BAFC0 CGameEffectNon_CumulativeLuck::ApplyEffect

## Decompiled (Ghidra)

```c
undefined4 CGameEffectNon_CumulativeLuck::ApplyEffect(int param_1)

{
  int in_ECX;
  
  *(short *)(param_1 + 0x95c) = *(short *)(param_1 + 0x95c) + (short)*(char *)(in_ECX + 0x18);
  return 1;
}
```

## Callers

- 0x849B84 (not in a function)

## Callees

None.

## Notes

The exe reads the amount's low byte sign-extended (`movsx cx, byte ptr [ecx+0x18]` at 0x4BAFC0) and adds it to the derived stats' luck word (+0x95C). The port cast it to `BYTE`, so a negative luck (an IESDP-documented use of opcode 133) was added as 256 - n; it now casts to `signed char`. Found by the 2026-10-30 IESDP cross-check.
