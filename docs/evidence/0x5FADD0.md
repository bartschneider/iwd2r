---
address: 0x5FADD0
name: CScreenConnection::EngineInitialized
exemption: "read IWD2.exe 0x5FADD0 (bundle decompile plus objdump) against ours and against his fork body"
kind: read-only
ghidra_version: 12.1.4
---

# 0x5FADD0 CScreenConnection::EngineInitialized

## Reading

The exe fInit's GUICON with the chitin+0x4a28 flag, shifts and adds the four
border panels, deactivates panels in the order 1, 3, 4, 6, 7, 8, 11, 5, 12,
21, 19, 20, 22, 23, 24, 25, then writes the four application-GUID dwords
0x588c135f / 0x4a020b15 / 0x69042d8f / 0x4e90e27b at cNetwork+0x95a
(m_nApplicationGuid, GUID little-endian dword packing) and the byte at
cNetwork+0x96a (m_bApplicationGuidDefined) = 1. Ours expresses the same order
and constants.

His fork diff for this function is a rename only; the row hinges on ours,
which matches the exe.
