# scenarios/chr

CHR fixtures exported from the original engine in the Windows Sandbox
(differential oracle).  Raised through the cheat console; production and
findings under `.superpowers/sdd/2026-10-23-frida-imports/task-49-report.md`.

| file | bytes | sha256 (prefix) | content | run |
|------|-------|-----------------|---------|-----|
| RYSDRD01.CHR | 2982 | `6558f934` | druid 1, chargen-fresh (scriptname `Rysdrd7`) | `runs/20261027-010318` |

Interim, and the reason is the task's main finding.  The run raised the
chargen druid with one `CTRLALTDELETE:SETCURRENTXP(21000)` and one level-up
walk; its `exported` checkpoint shows the character live at level 7 in the
very frame the export ran (xp 21000, level 7, class_levels druid 7), yet
`world_end` — one ESC, 64 frames later — reads level 1 again: the raise
lived in the uncommitted level-up session and never reached `m_baseStats`,
which is what `CGameSprite::Marshal` memcpy's into the CRE.  The file
therefore parses as chargen level 1 (xp 0, HP 8/8, stats 10/10/12/12/8/10,
skills 4+4 — the chargen spends).  Every console-raised export from tasks
43-48 parses the same way: RYSDRD7.CHR (launch 2), RYSDRD00.CHR (launch 3)
and RYSDRD01.CHR (launch 4) are byte-identical (md5
`1d14aa28e370b68a87fd8328a9d2c222`).  Why the multi-level chain never
commits its popup-55 skill points is the open problem, tracked for the
sorcerer task.

The parse test pinning this content is `ChrFixtureTest` in
`tests/test_tools.py`; a future fixture that does carry level 7 updates it
deliberately.
