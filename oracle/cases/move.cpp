// Movement: the group's follow path, the original's against ours.
//
// CAIGroup::HandleFollowPath (0x409820) works only on sprites it reaches
// through the original's object array (CGameObjectArray::GetDeny on
// g_pBaldurChitin->m_pObjectGame + 0x372C) and their virtual AddAction and
// InsertAction; with no member granted it does nothing. The original's
// sprites can only come from its CGameSprite constructor (0x6EF990), which
// needs the resource manager and a loaded creature file (R10a), so the case
// is pending. Our side is covered by the Catch2 test "[move]
// HandleFollowPath" on the started engine (tests/test_move.cpp).
#include "oracle/runner/registry.h"

ORACLE_CASE_PENDING("move/follow_path", nullptr,
    "needs engine state (sprites from the original's CGameSprite constructor in its object array, the "
    "message handler); ours covered by tests/test_move.cpp");
