#ifndef CGAMEDIALOG_H_
#define CGAMEDIALOG_H_

#include "BalDataTypes.h"
#include "CAICondition.h"
#include "CAIResponse.h"
#include "CResRef.h"
#include "CStrRes.h"

class CGameDialogEntry;
class CGameDialogReply;
class CGameSprite;

// The DLG V1.0 file (0x482A40 CGameDialogSprite::Unmarshal): the header's
// counts and offsets follow the "DLG V1.0" signature at 0x08.
typedef struct {
    /* 0000 */ DWORD m_nEntries;
    /* 0004 */ DWORD m_entriesOffset;
    /* 0008 */ DWORD m_nReplies;
    /* 000C */ DWORD m_repliesOffset;
    /* 0010 */ DWORD m_startConditionsOffset;
    /* 0014 */ DWORD m_nStartConditions;
    /* 0018 */ DWORD m_replyConditionsOffset;
    /* 001C */ DWORD m_nReplyConditions;
    /* 0020 */ DWORD m_responsesOffset;
    /* 0024 */ DWORD m_nResponses;
} CGameDialogFileHeader;

typedef struct {
    /* 0000 */ STRREF m_dialogText;
    /* 0004 */ DWORD m_startingReply;
    /* 0008 */ DWORD m_nReplies;
    /* 000C */ DWORD m_StartingConditionalScript;
} CGameDialogFileEntry;

typedef struct {
    /* 0000 */ DWORD m_flags;
    /* 0004 */ STRREF m_replyText;
    /* 0008 */ STRREF m_journalEntry;
    /* 000C */ DWORD m_conditionalScript;
    /* 0010 */ DWORD m_responseScript;
    /* 0014 */ RESREF m_nextDialog;
    /* 001C */ DWORD m_nextEntry;
} CGameDialogFileReply;

typedef struct {
    /* 0000 */ DWORD m_offset;
    /* 0004 */ DWORD m_length;
} CGameDialogFileScript;

class CGameDialogSprite {
public:
    CGameDialogSprite();
    ~CGameDialogSprite();
    void ClearMarshal();
    BOOL StartDialog(CGameSprite* pSprite);
    void Load(CResRef file, LONG characterIndex, LONG talkerIndex);
    void Unmarshal(BYTE* pDialog, DWORD dialogSize, LONG characterIndex, LONG talkerIndex);
    BOOL FetchRumor(const CResRef& file, CGameSprite* pSprite, LONG& nIndex, STR_RES& strRes);
    void EndDialog();
    BOOL EnterDialog(DWORD index, CGameSprite* pSprite, int a3);
    void AsynchronousUpdate();
    void Reload(CResRef file, CGameSprite* pTalker);
    void UpdateDialogColors();

    /* 0000 */ CResRef m_file;
    /* 0008 */ CTypedPtrArray<CPtrArray, CGameDialogEntry*> m_dialogEntries;
    /* 001C */ CTypedPtrArray<CPtrArray, CGameDialogEntry*> m_dialogEntriesOrdered;
    /* 0030 */ LONG m_characterIndex;
    /* 0034 */ LONG m_talkerIndex;
    /* 0038 */ DWORD m_currentEntryIndex;
    /* 003C */ BOOL m_waitingForResponse;
    /* 0040 */ LONG m_responseMarker;
    /* 0044 */ COLORREF m_playerColor;
    /* 0048 */ CString m_playerName;
    /* 004C */ LONG m_dialogFreezeCounter;
    /* 0050 */ LONG m_dialogFreezeMultiplayer;
    /* 0054 */ unsigned char field_54;
    /* 0055 */ unsigned char field_55;
    /* 0056 */ CString field_56;
    /* 005A */ BOOL m_bMusicThreadPriorityChanged;
    /* 005E */ int m_nMusicThreadPriority;
};

class CGameDialogEntry : public CTypedPtrArray<CPtrArray, CGameDialogReply*> {
public:
    CGameDialogEntry();
    ~CGameDialogEntry() override;
    void RemoveReplies(LONG lMarker, COLORREF rgbNameColor, const CString& sName);
    void Handle(CGameSprite* pSprite, COLORREF playerColor, int a3);

    /* 0014 */ STRREF m_dialogText;
    /* 0018 */ CAICondition m_startCondition;
    /* 0034 */ BOOL m_picked;
    /* 0038 */ DWORD m_conditionPriority;
    /* 003C */ DWORD m_dialogIndex;
    /* 0040 */ BOOLEAN m_bDisplayButton;
};

// What a picked reply leads to (0x485750 CGameDialogReply::Pick).
class CGameDialogNextEntry {
public:
    /* 0000 */ CResRef m_file;
    /* 0008 */ DWORD m_entryIndex;
};

class CGameDialogReply {
public:
    CGameDialogReply();
    CGameDialogNextEntry* Pick(CGameSprite* pSprite);

    /* 0000 */ DWORD m_flags;
    /* 0004 */ STRREF m_replyText;
    /* 0008 */ STRREF m_journalEntry;
    /* 000C */ CAICondition m_condition;
    /* 0028 */ CAIResponse m_response;
    /* 004C */ CResRef m_nextDialog;
    /* 0054 */ DWORD m_nextEntryIndex;
    /* 0058 */ POSITION m_displayPosition;
    /* 005C */ BOOL m_removeIfPicked;
    /* 0060 */ BYTE m_displayListId;
};

#endif /* CGAMEDIALOG_H_ */
