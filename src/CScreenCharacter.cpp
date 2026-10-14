#include "CScreenCharacter.h"

#include "CAIObjectType.h"
#include "CAIScript.h"
#include "CBaldurChitin.h"
#include "CButtonData.h"
#include "CGameAnimationType.h"
#include "CGameSprite.h"
#include "CIcon.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CMessage.h"
#include "CMultiplayerSettings.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "CSpell.h"
#include "CTimerWorld.h"
#include "CUIControlEditMultiLine.h"
#include "CUIControlFactory.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "IcewindMisc.h"
#include "msvc_atol.h"

#define FEAT_COUNT CGAMESPRITE_FEAT_NUMFEATS
#define FEAT_SLOTS 9

#define SKILL_COUNT CGAMESPRITE_SKILL_NUMSKILLS
#define SKILL_SLOTS 9

// 0x8F3324
const CString CScreenCharacter::TOKEN_SPELLLEVEL("SPELLLEVEL");

// 0x8F3320
const CString CScreenCharacter::TOKEN_WEAPONNAME("WEAPONNAME");

// 0x8F333C
const CString CScreenCharacter::TOKEN_NUMBER("number");

// 0x8F332C
const CString CScreenCharacter::TOKEN_MINIMUM("MINIMUM");

// 0x8F3334
const CString CScreenCharacter::TOKEN_MAXIMUM("MAXIMUM");

// NOTE: Probably does not belong here.
//
// 0x8F3338
CString CScreenCharacter::SAVE_NAME("default");

// 0x5D50F0
void CScreenCharacter::ResetBiography(CGameSprite* pSprite)
{
    switch (pSprite->m_derivedStats.GetBestClass()) {
    case CAIOBJECTTYPE_C_BARBARIAN:
        pSprite->m_baseStats.m_biography = 27862;
        break;
    case CAIOBJECTTYPE_C_BARD:
        pSprite->m_baseStats.m_biography = 19425;
        break;
    case CAIOBJECTTYPE_C_CLERIC:
        pSprite->m_baseStats.m_biography = 19422;
        break;
    case CAIOBJECTTYPE_C_DRUID:
        pSprite->m_baseStats.m_biography = 19421;
        break;
    case CAIOBJECTTYPE_C_FIGHTER:
        pSprite->m_baseStats.m_biography = 19423;
        break;
    case CAIOBJECTTYPE_C_MONK:
        pSprite->m_baseStats.m_biography = 27860;
        break;
    case CAIOBJECTTYPE_C_PALADIN:
        pSprite->m_baseStats.m_biography = 19427;
        break;
    case CAIOBJECTTYPE_C_RANGER:
        pSprite->m_baseStats.m_biography = 19429;
        break;
    case CAIOBJECTTYPE_C_ROGUE:
        pSprite->m_baseStats.m_biography = 19428;
        break;
    case CAIOBJECTTYPE_C_SORCERER:
        pSprite->m_baseStats.m_biography = 27863;
        break;
    case CAIOBJECTTYPE_C_WIZARD:
        pSprite->m_baseStats.m_biography = 19430;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 199
        UTIL_ASSERT(FALSE);
    }
}

// 0x5D5220
CScreenCharacter::CScreenCharacter()
{
    m_bShiftKeyDown = FALSE;
    m_bCapsLockKeyOn = FALSE;
    field_780 = 0;
    m_nExtraSkillPoints = 0;
    field_79C = 0;
    field_7A0 = 0;
    field_7A4 = 0;
    field_7A8 = 0;
    field_7AC = 0;
    field_7B0 = 0;
    m_nExtraMageSpells = 0;
    field_7BA = 0;
    m_nErrorState = 0;
    m_nNumErrorButtons = 0;
    m_nCharacterIndex = 0;
    m_nTopHatedRace = 0;
    m_bMultiPlayerViewable = FALSE;
    m_bMultiPlayerModifyable = FALSE;
    m_nCurrentPortrait = 0;
    m_nPortraitSmallIndex = 0;
    m_nPortraitLargeIndex = 0;
    m_nCustomSoundSetIndex = 0;
    m_nCustomSoundIndex = 0;
    field_81A = 0;
    m_nSpecialization = 0;
    field_1772 = 0;
    m_nSpellLevel = 0;
    m_nHatedRaceIndex = 0;
    m_nClass = 0;
    field_17A4 = 0;
    m_nMinSTR = 0;
    m_nMinDEX = 0;
    m_nMinCON = 0;
    m_nMinINT = 0;
    m_nMinWIS = 0;
    m_nMinCHR = 0;
    m_nMaxSTR = 0;
    m_nMaxDEX = 0;
    m_nMaxCON = 0;
    m_nMaxINT = 0;
    m_nMaxWIS = 0;
    m_nMaxCHR = 0;
    m_nModSTR = 0;
    m_nModDEX = 0;
    m_nModCON = 0;
    m_nModINT = 0;
    m_nModWIS = 0;
    m_nModCHR = 0;
    // NOTE: The original clears only the first DWORD of each copy.
    memset(m_levelUpFeatFlags, 0, sizeof(DWORD));
    memset(m_levelUpFeatValues, 0, sizeof(DWORD));
    memset(m_levelUpSkills, 0, sizeof(DWORD));

    SetVideoMode(0);

    m_pVirtualKeys[0] = CKeyInfo(VK_ESCAPE, -1, 0);
    m_pVirtualKeys[1] = CKeyInfo('1', 0, 0);
    m_pVirtualKeys[2] = CKeyInfo('2', 0, 0);
    m_pVirtualKeys[3] = CKeyInfo('3', 0, 0);
    m_pVirtualKeys[4] = CKeyInfo('4', 0, 0);
    m_pVirtualKeys[5] = CKeyInfo('5', 0, 0);
    m_pVirtualKeys[6] = CKeyInfo('6', 0, 0);
    m_pVirtualKeys[7] = CKeyInfo('7', 0, 0);
    m_pVirtualKeys[8] = CKeyInfo('8', 0, 0);
    m_pVirtualKeys[9] = CKeyInfo('9', 0, 0);
    m_pVirtualKeys[10] = CKeyInfo(VK_SPACE, -1, 0);
    m_pVirtualKeys[11] = CKeyInfo('A', 0, 0);
    m_pVirtualKeys[12] = CKeyInfo('B', 0, 0);
    m_pVirtualKeys[13] = CKeyInfo('C', 0, 0);
    m_pVirtualKeys[14] = CKeyInfo('D', 0, 0);
    m_pVirtualKeys[15] = CKeyInfo('E', 0, 0);
    m_pVirtualKeys[16] = CKeyInfo('F', 0, 0);
    m_pVirtualKeys[17] = CKeyInfo('G', 0, 0);
    m_pVirtualKeys[18] = CKeyInfo('H', 0, 0);
    m_pVirtualKeys[19] = CKeyInfo('I', 0, 0);
    m_pVirtualKeys[20] = CKeyInfo('J', 0, 0);
    m_pVirtualKeys[21] = CKeyInfo('K', 0, 0);
    m_pVirtualKeys[22] = CKeyInfo('L', 0, 0);
    m_pVirtualKeys[23] = CKeyInfo('M', 0, 0);
    m_pVirtualKeys[24] = CKeyInfo('N', 0, 0);
    m_pVirtualKeys[25] = CKeyInfo('O', 0, 0);
    m_pVirtualKeys[26] = CKeyInfo('P', 0, 0);
    m_pVirtualKeys[27] = CKeyInfo('Q', 0, 0);
    m_pVirtualKeys[28] = CKeyInfo('R', 0, 0);
    m_pVirtualKeys[29] = CKeyInfo('S', 0, 0);
    m_pVirtualKeys[30] = CKeyInfo('T', 0, 0);
    m_pVirtualKeys[31] = CKeyInfo('U', 0, 0);
    m_pVirtualKeys[32] = CKeyInfo('V', 0, 0);
    m_pVirtualKeys[33] = CKeyInfo('W', 0, 0);
    m_pVirtualKeys[34] = CKeyInfo('X', 0, 0);
    m_pVirtualKeys[35] = CKeyInfo('Y', 0, 0);
    m_pVirtualKeys[36] = CKeyInfo('Z', 0, 0);
    m_pVirtualKeys[37] = CKeyInfo('0', 0, 0);
    m_pVirtualKeys[38] = CKeyInfo(VK_BACK, 0, 0);
    m_pVirtualKeys[39] = CKeyInfo(VK_END, -1, 0);
    m_pVirtualKeys[40] = CKeyInfo(VK_HOME, -1, 0);
    m_pVirtualKeys[41] = CKeyInfo(VK_LEFT, static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND), static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND));
    m_pVirtualKeys[42] = CKeyInfo(VK_UP, 4, 0);
    m_pVirtualKeys[43] = CKeyInfo(VK_RIGHT, static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND), static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND));
    m_pVirtualKeys[44] = CKeyInfo(VK_DOWN, 4, 0);
    m_pVirtualKeys[45] = CKeyInfo(VK_SNAPSHOT, -1, 0);
    m_pVirtualKeys[46] = CKeyInfo(VK_INSERT, -1, 0);
    m_pVirtualKeys[47] = CKeyInfo(VK_DELETE, 0, 0);
    m_pVirtualKeys[48] = CKeyInfo(VK_NUMPAD0, -1, 0);
    m_pVirtualKeys[49] = CKeyInfo(VK_NUMPAD1, -1, 0);
    m_pVirtualKeys[50] = CKeyInfo(VK_NUMPAD2, -1, 0);
    m_pVirtualKeys[51] = CKeyInfo(VK_NUMPAD3, -1, 0);
    m_pVirtualKeys[52] = CKeyInfo(VK_NUMPAD4, static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND), static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND));
    m_pVirtualKeys[53] = CKeyInfo(VK_NUMPAD5, -1, 0);
    m_pVirtualKeys[54] = CKeyInfo(VK_NUMPAD6, static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND), static_cast<BYTE>(CChitin::TIMER_UPDATES_PER_SECOND));
    m_pVirtualKeys[55] = CKeyInfo(VK_NUMPAD7, -1, 0);
    m_pVirtualKeys[56] = CKeyInfo(VK_NUMPAD8, -1, 0);
    m_pVirtualKeys[57] = CKeyInfo(VK_NUMPAD9, -1, 0);
    m_pVirtualKeys[58] = CKeyInfo(VK_MULTIPLY, -1, 0);
    m_pVirtualKeys[59] = CKeyInfo(VK_ADD, -1, 0);
    m_pVirtualKeys[60] = CKeyInfo(VK_SEPARATOR, -1, 0);
    m_pVirtualKeys[61] = CKeyInfo(VK_SUBTRACT, -1, 0);
    m_pVirtualKeys[62] = CKeyInfo(VK_DECIMAL, -1, 0);
    m_pVirtualKeys[63] = CKeyInfo(VK_DIVIDE, -1, 0);
    m_pVirtualKeys[64] = CKeyInfo(VK_F1, -1, 0);
    m_pVirtualKeys[65] = CKeyInfo(VK_F2, -1, 0);
    m_pVirtualKeys[66] = CKeyInfo(VK_F3, -1, 0);
    m_pVirtualKeys[67] = CKeyInfo(VK_F4, -1, 0);
    m_pVirtualKeys[68] = CKeyInfo(VK_F5, -1, 0);
    m_pVirtualKeys[69] = CKeyInfo(VK_F6, -1, 0);
    m_pVirtualKeys[70] = CKeyInfo(VK_F7, -1, 0);
    m_pVirtualKeys[71] = CKeyInfo(VK_F8, -1, 0);
    m_pVirtualKeys[72] = CKeyInfo(VK_F9, -1, 0);
    m_pVirtualKeys[73] = CKeyInfo(VK_F10, -1, 0);
    m_pVirtualKeys[74] = CKeyInfo(VK_F11, -1, 0);
    m_pVirtualKeys[75] = CKeyInfo(VK_F12, -1, 0);
    m_pVirtualKeys[76] = CKeyInfo(VK_NUMLOCK, -1, 0);
    m_pVirtualKeys[77] = CKeyInfo(VK_RETURN, -1, 0);
    m_pVirtualKeys[78] = CKeyInfo(VK_OEM_3, 0, 0);
    m_pVirtualKeys[79] = CKeyInfo(VK_OEM_MINUS, 0, 0);
    m_pVirtualKeys[80] = CKeyInfo(VK_OEM_PLUS, 0, 0);
    m_pVirtualKeys[81] = CKeyInfo(VK_OEM_4, 0, 0);
    m_pVirtualKeys[82] = CKeyInfo(VK_OEM_6, 0, 0);
    m_pVirtualKeys[83] = CKeyInfo(VK_OEM_5, 0, 0);
    m_pVirtualKeys[84] = CKeyInfo(VK_OEM_1, 0, 0);
    m_pVirtualKeys[85] = CKeyInfo(VK_OEM_7, 0, 0);
    m_pVirtualKeys[86] = CKeyInfo(VK_OEM_COMMA, 0, 0);
    m_pVirtualKeys[87] = CKeyInfo(VK_OEM_PERIOD, 0, 0);
    m_pVirtualKeys[88] = CKeyInfo(VK_OEM_2, 0, 0);
    m_pVirtualKeys[89] = CKeyInfo(VK_TAB, -1, 0);

    // FIXME: Buffer overrun.
    m_pVirtualKeys[90] = CKeyInfo(VK_PROCESSKEY, -1, 0);

    m_bCtrlKeyDown = FALSE;
    m_pScripts = NULL;
    m_nScriptIndex = -1;
    m_pCharacters = NULL;
    m_pPortraits = NULL;
    m_pSounds = NULL;
    m_pTempBaseStats = NULL;
    m_pTempDerivedStats = NULL;
    m_pBeforeLevelUpDerivedStats = NULL;

    for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        if (m_spells.m_lists[nLevel].m_List.size() != 0) {
            m_spells.m_lists[nLevel].m_List.clear();
            m_spells.m_lists[nLevel].field_14 = 0;
            m_spells.m_lists[nLevel].field_18 = 0;
        }
    }
    m_spells.m_nHighestLevel = 0;

    // NOTE: The original clears 0x2B6 (m_spells.m_nHighestLevel) again.
    m_spells.m_nHighestLevel = 0;
    field_866 = 0;
    m_nSpellLevel = 0;
    field_86A = 0;
    m_hatedRaces[0] = CAIObjectType::R_BUGBEAR;
    m_hatedRaces[1] = CAIObjectType::R_DRIDER;
    m_hatedRaces[2] = CAIObjectType::R_GIANT;
    m_hatedRaces[3] = CAIObjectType::R_GOBLIN;
    m_hatedRaces[4] = CAIObjectType::R_HARPY;
    m_hatedRaces[5] = CAIObjectType::R_HOOK_HORROR;
    m_hatedRaces[6] = CAIObjectType::R_LIZARDMAN;
    m_hatedRaces[7] = CAIObjectType::R_OGRE;
    m_hatedRaces[8] = CAIObjectType::R_ORC;
    m_hatedRaces[9] = CAIObjectType::R_SALAMANDER;
    m_hatedRaces[10] = CAIObjectType::R_SHAPESHIFTER;
    m_hatedRaces[11] = CAIObjectType::R_TROLL;
    m_hatedRaces[12] = CAIObjectType::R_UMBERHULK;
    m_hatedRaces[13] = CAIObjectType::R_UNDEAD;
    m_hatedRaces[14] = CAIObjectType::R_WYVERN;
    m_hatedRaces[15] = CAIObjectType::R_YUANTI;
    field_1B8 = 0;
    field_1844 = 0;
    field_1846 = 0;
    field_184A = 0;
    field_184E = 0;
    field_1852 = 0;
}

// 0x5D63B0
CScreenCharacter::~CScreenCharacter()
{
    if (m_pTempDerivedStats != NULL) {
        delete m_pTempDerivedStats;
        m_pTempDerivedStats = NULL;
    }

    if (m_pBeforeLevelUpDerivedStats != NULL) {
        delete m_pBeforeLevelUpDerivedStats;
        m_pBeforeLevelUpDerivedStats = NULL;

        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            if (m_spells.m_lists[nLevel].m_List.size() != 0) {
                m_spells.m_lists[nLevel].m_List.clear();
                m_spells.m_lists[nLevel].field_14 = 0;
                m_spells.m_lists[nLevel].field_18 = 0;
            }
        }
        m_spells.m_nHighestLevel = 0;
    }

    if (m_pTempBaseStats != NULL) {
        delete m_pTempBaseStats;
        m_pTempBaseStats = NULL;
    }
}

// 0x49FC40
BOOL CScreenCharacter::CheckSystemKeyCtrl()
{
    return TRUE;
}

// 0x5D62F0
void CScreenCharacter::SetSystemKeyCtrl(BOOLEAN bValue)
{
    m_bCtrlKeyDown = bValue;
}

// 0x5D6300
BOOL CScreenCharacter::GetShiftKey()
{
    return field_1B8 && m_bShiftKeyDown;
}

// 0x5D6320
void CScreenCharacter::SetSystemKeyShift(BOOL bValue)
{
    m_bShiftKeyDown = bValue;
}

// 0x5D6330
BOOL CScreenCharacter::CheckSystemKeyShift()
{
    return field_1B8;
}

// 0x5D6340
void CScreenCharacter::SetSystemKeyCapsLock(BOOL bValue)
{
    m_bCapsLockKeyOn = bValue;
}

// 0x5D6350
BOOL CScreenCharacter::GetCapsLockKey()
{
    return field_1B8 && m_bCapsLockKeyOn;
}

// 0x5D6330
BOOL CScreenCharacter::CheckSystemKeyCapsLock()
{
    return field_1B8;
}

// 0x49FC40
BOOL CScreenCharacter::CheckMouseMove()
{
    return TRUE;
}

// 0x49FC40
BOOL CScreenCharacter::CheckMouseLButton()
{
    return TRUE;
}

// 0x5D76C0
void CScreenCharacter::OnLButtonDblClk(CPoint pt)
{
    m_cUIManager.OnLButtonDblClk(pt);
}

// 0x636970
void CScreenCharacter::OnLButtonDown(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    m_cUIManager.OnLButtonDown(pt);
}

// 0x6369A0
void CScreenCharacter::OnLButtonUp(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 0;
    m_cUIManager.OnLButtonUp(pt);
}

// 0x49FC40
BOOL CScreenCharacter::CheckMouseRButton()
{
    return TRUE;
}

// 0x6369D0
void CScreenCharacter::OnRButtonDown(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 1;
    m_cUIManager.OnRButtonDown(pt);
}

// 0x636A00
void CScreenCharacter::OnRButtonUp(CPoint pt)
{
    g_pBaldurChitin->GetObjectCursor()->m_nState = 0;
    m_cUIManager.OnRButtonUp(pt);
}

// 0x699600
SHORT CScreenCharacter::GetNumVirtualKeys()
{
    return CSCREENCHARACTER_VIRTUAL_KEYS;
}

// 0x5D6370
CKeyInfo* CScreenCharacter::GetVirtualKeys()
{
    return m_pVirtualKeys;
}

// 0x5D6380
BYTE* CScreenCharacter::GetVirtualKeysFlags()
{
    return m_pVirtualKeysFlags;
}

// 0x5D6A80
void CScreenCharacter::EngineActivated()
{
    if (CChitin::byte_8FB950
        && g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pChitin->cNetwork.GetSessionHosting() == TRUE
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0);
    }

    m_preLoadFontRealms.SetResRef(CResRef("REALMS"), FALSE, TRUE);
    m_preLoadFontRealms.RegisterFont();

    m_preLoadFontStnSml.SetResRef(CResRef("STONESML"), FALSE, TRUE);
    m_preLoadFontStnSml.RegisterFont();

    m_preLoadFontTool.SetResRef(CResRef("TOOLFONT"), FALSE, TRUE);
    m_preLoadFontTool.RegisterFont();

    if (m_cUIManager.m_bInitialized) {
        if (GetTopPopup() == NULL) {
            UpdateMainPanel(FALSE);
        }

        UpdateCursorShape(0);
        CheckEnablePortaits(1);
        CheckEnableLeftPanel();
        g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
        m_cUIManager.InvalidateRect(NULL);
    }
}

// 0x5D6BC0
void CScreenCharacter::EngineDeactivated()
{
    if (CChitin::byte_8FB950) {
        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
            && g_pChitin->cNetwork.GetSessionHosting() == TRUE
            && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
            if (!g_pBaldurChitin->m_pEngineWorld->m_bPaused) {
                g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0);
            }
        }
    }

    m_preLoadFontRealms.Unload();
    m_preLoadFontStnSml.Unload();
    m_preLoadFontTool.Unload();

    if (m_cUIManager.m_pFocusedControl != NULL) {
        // NOTE: Uninline.
        m_cUIManager.KillCapture();
        field_1B8 = FALSE;
    }
}

// 0x5D6C60
void CScreenCharacter::EngineGameInit()
{
    m_cUIManager.fInit(this, CResRef("GUIREC"), g_pBaldurChitin->field_4A28);

    CPoint pt;
    if (g_pBaldurChitin->field_4A28) {
        pt.x = CVideo::SCREENWIDTH / 2 - CBaldurChitin::DEFAULT_SCREEN_WIDTH;
        pt.y = CVideo::SCREENHEIGHT / 2 - CBaldurChitin::DEFAULT_SCREEN_HEIGHT;
    } else {
        pt.x = (CVideo::SCREENWIDTH - CBaldurChitin::DEFAULT_SCREEN_WIDTH) / 2;
        pt.y = (CVideo::SCREENHEIGHT - CBaldurChitin::DEFAULT_SCREEN_HEIGHT) / 2;
    }

    m_cUIManager.ShiftPanels(pt);

    for (int side = 0; side < 4; side++) {
        m_cUIManager.AddPanel(&(g_pBaldurChitin->m_aBorderPanels[side]));
    }

    m_nSelectedCharacter = 0;
    m_pCurrentScrollBar = NULL;
    m_pScripts = NULL;
    m_nScriptIndex = -1;
    m_pCharacters = NULL;
    m_pPortraits = NULL;
    m_pSounds = NULL;
    m_pTempBaseStats = NULL;
    m_pTempDerivedStats = NULL;
    m_pBeforeLevelUpDerivedStats = NULL;

    for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        if (m_spells.m_lists[nLevel].m_List.size() != 0) {
            m_spells.m_lists[nLevel].m_List.clear();
            m_spells.m_lists[nLevel].field_14 = 0;
            m_spells.m_lists[nLevel].field_18 = 0;
        }
    }
    m_spells.m_nHighestLevel = 0;

    field_866 = 0;
    field_86A = 0;

    m_cUIManager.GetPanel(4)->SetActive(FALSE);
    m_cUIManager.GetPanel(6)->SetActive(FALSE);
    m_cUIManager.GetPanel(7)->SetActive(FALSE);
    m_cUIManager.GetPanel(8)->SetActive(FALSE);
    m_cUIManager.GetPanel(16)->SetActive(FALSE);
    m_cUIManager.GetPanel(9)->SetActive(FALSE);
    m_cUIManager.GetPanel(10)->SetActive(FALSE);
    m_cUIManager.GetPanel(11)->SetActive(FALSE);
    m_cUIManager.GetPanel(12)->SetActive(FALSE);
    m_cUIManager.GetPanel(13)->SetActive(FALSE);
    m_cUIManager.GetPanel(17)->SetActive(FALSE);
    m_cUIManager.GetPanel(18)->SetActive(FALSE);
    m_cUIManager.GetPanel(19)->SetActive(FALSE);
    m_cUIManager.GetPanel(20)->SetActive(FALSE);
    m_cUIManager.GetPanel(11)->SetActive(FALSE);
    m_cUIManager.GetPanel(50)->SetActive(FALSE);
    m_cUIManager.GetPanel(51)->SetActive(FALSE);
    m_cUIManager.GetPanel(52)->SetActive(FALSE);
    m_cUIManager.GetPanel(53)->SetActive(FALSE);
    m_cUIManager.GetPanel(54)->SetActive(FALSE);
    m_cUIManager.GetPanel(55)->SetActive(FALSE);
    m_cUIManager.GetPanel(56)->SetActive(FALSE);
    m_cUIManager.GetPanel(57)->SetActive(FALSE);

    CUIPanel* pPanel = m_cUIManager.GetPanel(2);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 797
    UTIL_ASSERT(pPanel != NULL);

    CUIControlBase* pControl;

    pControl = pPanel->GetControl(0x10000028);
    UTIL_ASSERT(pControl != NULL) // 801
    pControl->SetToolTipStrRef(17183, -1, -1); // "Armor Class"

    pControl = pPanel->GetControl(0x10000029);
    UTIL_ASSERT(pControl != NULL) // 805
    pControl->SetToolTipStrRef(17184, -1, -1); // "Current Hit Points"

    pControl = pPanel->GetControl(0x1000002A);
    UTIL_ASSERT(pControl != NULL) // 809
    pControl->SetToolTipStrRef(17378, -1, -1); // "Maximum Hit Points"

    pControl = pPanel->GetControl(0x1000002F);
    UTIL_ASSERT(pControl != NULL) // 815
    pControl->SetToolTipStrRef(1145, -1, -1); // "Strength"

    pControl = pPanel->GetControl(0x10000009);
    UTIL_ASSERT(pControl != NULL) // 819
    pControl->SetToolTipStrRef(1151, -1, -1); // "Dexterity"

    pControl = pPanel->GetControl(0x1000000A);
    UTIL_ASSERT(pControl != NULL) // 823
    pControl->SetToolTipStrRef(1178, -1, -1); // "Constitution"

    pControl = pPanel->GetControl(0x1000000B);
    UTIL_ASSERT(pControl != NULL) // 827
    pControl->SetToolTipStrRef(1179, -1, -1); // "Intelligence"

    pControl = pPanel->GetControl(0x1000000C);
    UTIL_ASSERT(pControl != NULL) // 831
    pControl->SetToolTipStrRef(1180, -1, -1); // "Wisdom"

    pControl = pPanel->GetControl(0x1000000D);
    UTIL_ASSERT(pControl != NULL) // 835
    pControl->SetToolTipStrRef(1181, -1, -1); // "Charisma"

    pControl = pPanel->GetControl(60);
    UTIL_ASSERT(pControl != NULL) // 841
    pControl->SetToolTipStrRef(40316, -1, -1); // "General"

    pControl = pPanel->GetControl(61);
    UTIL_ASSERT(pControl != NULL) // 845
    pControl->SetToolTipStrRef(40317, -1, -1); // "Weapons and Armor"

    pControl = pPanel->GetControl(62);
    UTIL_ASSERT(pControl != NULL) // 849
    pControl->SetToolTipStrRef(40318, -1, -1); // "Skills and Feats"

    pControl = pPanel->GetControl(63);
    UTIL_ASSERT(pControl != NULL) // 853
    pControl->SetToolTipStrRef(33500, -1, -1); // "Miscellaneous"
}

// 0x5D72C0
void CScreenCharacter::EngineGameUninit()
{
    m_cUIManager.fUninit();
}

// 0x5D76E0
void CScreenCharacter::OnMouseMove(CPoint pt)
{
    CUIPanel* pPanel;

    if (m_cUIManager.GetPanel(11)->m_bActive) {
        pPanel = m_cUIManager.GetPanel(11);
        if (pPanel->GetControl(2)->IsOver(pt - pPanel->m_ptOrigin)) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(3));
        } else {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(7));
        }
    } else if (m_cUIManager.GetPanel(16)->m_bActive) {
        pPanel = m_cUIManager.GetPanel(16);
        if (pPanel->GetControl(8)->IsOver(pt - pPanel->m_ptOrigin)) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(9));
        } else {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(1));
        }
    } else if (m_cUIManager.GetPanel(56)->m_bActive) {
        pPanel = m_cUIManager.GetPanel(56);
        if (pPanel->GetControl(92)->IsOver(pt - pPanel->m_ptOrigin)) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(91));
        } else {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(104));
        }
    } else if (m_cUIManager.GetPanel(57)->m_bActive) {
        pPanel = m_cUIManager.GetPanel(57);
        if (pPanel->GetControl(2)->IsOver(pt - pPanel->m_ptOrigin)) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(3));
        } else {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(4));
        }
    }

    m_cUIManager.OnMouseMove(pt);
}

// 0x5D78E0
void CScreenCharacter::OnPortraitLClick(DWORD nPortrait)
{
    // NOTE: Unsigned compare.
    if (nPortrait < static_cast<DWORD>(g_pBaldurChitin->GetObjectGame()->GetNumCharacters())) {
        m_nSelectedCharacter = nPortrait;

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            // NOTE: Uninline.
            LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterId(static_cast<SHORT>(nPortrait));

            for (SHORT index = 0; index < 6; index++) {
                if (g_pBaldurChitin->GetObjectGame()->m_characters[index] == nCharacterId) {
                    g_pBaldurChitin->GetBaldurMessage()->UpdateDemandCharacters(0, index, 0);
                }
            }
        }

        CheckMultiPlayerViewableModifyable();
        UpdateCursorShape(0);
        UpdateMainPanel(TRUE);
        m_cUIManager.InvalidateRect(NULL);
    }
}

// 0x5D79B0
void CScreenCharacter::UpdateSavingThrows(CUIControlTextDisplay* pText, CDerivedStats& DStats)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(17379)); // "Saving Throws"

    UpdateText(pText,
        "%s: %+d",
        (LPCSTR)FetchString(17380), // "Fortitude"
        DStats.m_nSaveVSFortitude);

    INT nTrapSenseBonus = ruleTables.GetTrapSenseBonus(DStats);
    if (nTrapSenseBonus != 0) {
        UpdateText(pText,
            "%s: %+d (%s: %d)",
            (LPCSTR)FetchString(17381), // "Reflex"
            DStats.m_nSaveVSReflex,
            (LPCSTR)FetchString(39802), // "Traps"
            nTrapSenseBonus);
    } else {
        UpdateText(pText,
            "%s: %+d",
            (LPCSTR)FetchString(17381), // "Reflex"
            DStats.m_nSaveVSReflex);
    }

    UpdateText(pText,
        "%s: %+d",
        (LPCSTR)FetchString(17382), // "Will"
        DStats.m_nSaveVSWill);
}

// 0x5D7B80
CString CScreenCharacter::GetCurrentPortrait(CGameSprite* pSprite)
{
    switch (pSprite->m_startTypeAI.m_nGender) {
    case CAIOBJECTTYPE_SEX_MALE:
        return CString(CScreenCreateChar::MALE_PORTRAITS[m_nCurrentPortrait]);
    case CAIOBJECTTYPE_SEX_FEMALE:
        return CString(CScreenCreateChar::FEMALE_PORTRAITS[m_nCurrentPortrait]);
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 1767
        UTIL_ASSERT(FALSE);
    }

    return CString("");
}

// NOTE: Inlined.
void CScreenCharacter::IncCurrentPortrait(CGameSprite* pSprite)
{
    INT nCount;
    switch (pSprite->m_startTypeAI.m_nGender) {
    case CAIOBJECTTYPE_SEX_MALE:
        nCount = CScreenCreateChar::MALE_PORTRAITS_COUNT;
        break;
    case CAIOBJECTTYPE_SEX_FEMALE:
        nCount = CScreenCreateChar::FEMALE_PORTRAITS_COUNT;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 1808
        UTIL_ASSERT(FALSE);
    }

    m_nCurrentPortrait = (m_nCurrentPortrait + 1) % nCount;
}

// NOTE: Inlined.
void CScreenCharacter::DecCurrentPortrait(CGameSprite* pSprite)
{
    INT nCount;
    switch (pSprite->m_startTypeAI.m_nGender) {
    case CAIOBJECTTYPE_SEX_MALE:
        nCount = CScreenCreateChar::MALE_PORTRAITS_COUNT;
        break;
    case CAIOBJECTTYPE_SEX_FEMALE:
        nCount = CScreenCreateChar::FEMALE_PORTRAITS_COUNT;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 1851
        UTIL_ASSERT(FALSE);
    }

    m_nCurrentPortrait = (m_nCurrentPortrait + nCount - 1) % nCount;
}

// 0x5D7C10
void CScreenCharacter::ResetAppearancePanel(CUIPanel* pPanel, CGameSprite* pSprite)
{
    CString sPortrait;

    m_nCurrentPortrait = 0;
    sPortrait = GetCurrentPortrait(pSprite);

    m_cResPortraitSmallTemp = sPortrait + "S";
    m_cResPortraitLargeTemp = sPortrait + "L";
}

// 0x5D7D30
void CScreenCharacter::UpdateAppearancePanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(18);
    if (pPanel != NULL) {
        CUIControlButtonCharacterPortrait* pPortrait = static_cast<CUIControlButtonCharacterPortrait*>(pPanel->GetControl(0));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 1923
        UTIL_ASSERT(pPortrait != NULL);

        CResRef portraitResRef(GetCurrentPortrait(pSprite) + "L");
        pPortrait->SetPortrait(portraitResRef);
        pPortrait->InvalidateRect();

        CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(3));
        pDone->SetEnabled(IsDoneButtonClickable(pSprite));
    }
}

// 0x5D7E30
void CScreenCharacter::ResetCustomPortraitsPanel(CUIPanel* pPanel, CGameSprite* pSprite)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 1961
    UTIL_ASSERT(pGame != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 1964
    UTIL_ASSERT(m_pPortraits == NULL);

    m_pPortraits = pGame->GetPortraits();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 1966
    UTIL_ASSERT(m_pPortraits != NULL);

    m_nPortraitSmallIndex = -1;
    m_nPortraitLargeIndex = -1;
    UpdatePortraitList(pPanel, 3, m_nPortraitSmallIndex);
    UpdatePortraitList(pPanel, 2, m_nPortraitLargeIndex);

    CUIControlButtonCharacterPortrait* pPortrait;

    pPortrait = static_cast<CUIControlButtonCharacterPortrait*>(pPanel->GetControl(1));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 1976
    UTIL_ASSERT(pPortrait != NULL);

    pPortrait->SetPortrait(CResRef(""));

    pPortrait = static_cast<CUIControlButtonCharacterPortrait*>(pPanel->GetControl(0));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 1979
    UTIL_ASSERT(pPortrait != NULL);

    pPortrait->SetPortrait(CResRef(""));
}

// 0x5D7F70
void CScreenCharacter::UpdateCustomPortraitsPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(19);

    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(4));

    if (pPanel != NULL) {
        CString sPortrait;
        CUIControlButtonCharacterPortrait* pPortrait;

        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 2015
        UTIL_ASSERT(pGame != NULL);

        pPortrait = static_cast<CUIControlButtonCharacterPortrait*>(pPanel->GetControl(1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 2019
        UTIL_ASSERT(pPortrait != NULL);

        sPortrait = "";
        if (m_nPortraitSmallIndex >= 0) {
            POSITION pos = m_pPortraits->FindIndex(m_nPortraitSmallIndex);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 2024
            UTIL_ASSERT(pos != NULL);

            sPortrait = m_pPortraits->GetAt(pos);
        }

        pPortrait->SetPortrait(sPortrait);

        UpdateLabel(pPanel, 0x10000006, "%s", (LPCSTR)sPortrait);

        pPortrait = static_cast<CUIControlButtonCharacterPortrait*>(pPanel->GetControl(0));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 2032
        UTIL_ASSERT(pPortrait != NULL);

        sPortrait = "";
        if (m_nPortraitLargeIndex >= 0) {
            POSITION pos = m_pPortraits->FindIndex(m_nPortraitLargeIndex);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 2037
            UTIL_ASSERT(pos != NULL);

            sPortrait = m_pPortraits->GetAt(pos);
        }

        pPortrait->SetPortrait(sPortrait);

        UpdateLabel(pPanel, 0x10000005, "%s", (LPCSTR)sPortrait);

        CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(10));
        pDone->SetEnabled(IsDoneButtonClickable(pSprite));
    }
}

// 0x5D8490
void CScreenCharacter::ResetClassPanel(CGameSprite* pSprite, int a2)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(6);

    DWORD name;
    DWORD description;
    switch (m_nClass) {
    case CAIOBJECTTYPE_C_WIZARD:
        name = 1081; // "<MAGESCHOOL>"
        description = 10942;
        break;
    case CAIOBJECTTYPE_C_PALADIN:
        name = 1078; // "Paladin"
        description = 9558;
        break;
    case CAIOBJECTTYPE_C_MONK:
        name = 33; // "Monk"
        description = 36;
        break;
    case CAIOBJECTTYPE_C_CLERIC:
        name = 1079; // "Cleric"
        description = 9559;
        break;
    }

    UpdateLabel(pPanel, 0x10000000, "%s", (LPCSTR)FetchString(name));
    UpdateHelp(pPanel->m_nID, 13, description);
}

// NOTE: Inlined.
void CScreenCharacter::ResetHatedRacePanel(CGameSprite* pSprite)
{
    m_nHatedRaceIndex = pSprite->GetNextHatedRacesSlot();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2286
    UTIL_ASSERT((pSprite->GetNextHatedRacesSlot() < MAX_HATED_RACES));

    pSprite->GetBaseStats()->m_favoredEnemies[m_nHatedRaceIndex] = CAIObjectType::R_NO_RACE;
    m_nTopHatedRace = 0;

    UpdateHelp(16, 8, 17256);
}

// 0x5D8570
void CScreenCharacter::ResetExportPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(13);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2372
    UTIL_ASSERT(pPanel != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2374
    UTIL_ASSERT(pGame != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2377
    UTIL_ASSERT(m_pCharacters == NULL);

    m_pCharacters = pGame->GetImportCharacters();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2379
    UTIL_ASSERT(m_pCharacters != NULL);

    m_nCharacterIndex = -1;
    UpdateCharacterList(pPanel, 0, m_nCharacterIndex);

    CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(6));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2387
    UTIL_ASSERT(pEdit != NULL);

    pEdit->SetText(CString(""));

    UpdateHelp(pPanel->m_nID, 2, 10962);
}

// 0x5D8680
void CScreenCharacter::UpdateCharacterList(CUIPanel* pPanel, DWORD dwTextId, INT nSelected)
{
    CString sFileName;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2423
    UTIL_ASSERT(m_pCharacters != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2424
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(dwTextId));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2426
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();
    pText->m_rgbHighlightColor = CBaldurChitin::TEXTDISPLAY_COLOR_HIGHLIGHT;
    pText->field_A68 = 32767;

    INT nIndex = 0;
    POSITION pos = m_pCharacters->GetHeadPosition();
    while (pos != NULL) {
        COLORREF rgbTextColor = CBaldurChitin::TEXTDISPLAY_COLOR_SELECT;
        if (nSelected != nIndex) {
            rgbTextColor = pText->m_rgbTextColor;
        }

        sFileName = m_pCharacters->GetAt(pos);

        pText->DisplayString(CString(""),
            sFileName,
            pText->m_rgbLabelColor,
            rgbTextColor,
            nIndex,
            FALSE,
            TRUE);

        m_pCharacters->GetNext(pos);
        nIndex++;
    }

    pText->SetTopString(pText->m_plstStrings->FindIndex(0));
}

// FIXME: `sName` should be reference.
//
// 0x5D87F0
BOOLEAN CScreenCharacter::IsNameOnExportList(CString sName)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2472
    UTIL_ASSERT(m_pCharacters != NULL);

    CString sExistingName;

    POSITION pos = m_pCharacters->GetHeadPosition();
    while (pos != NULL) {
        // FIXME: Unnecessary copy.
        sExistingName = m_pCharacters->GetAt(pos);

        if (sExistingName == sName) {
            return TRUE;
        }

        m_pCharacters->GetNext(pos);
    }

    return FALSE;
}

// 0x5D88E0
void CScreenCharacter::ResetBiographyPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(12);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2658
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(0));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2661
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();

    CString sBiography;

    sBiography = FetchString(pSprite->GetBaseStats()->m_biography);

    pText->DisplayString(CString(""),
        sBiography,
        pText->m_rgbLabelColor,
        pText->m_rgbTextColor,
        -1,
        FALSE,
        TRUE);
}

// 0x5D8A10
void CScreenCharacter::ResetClassDescriptionPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(52);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2729
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(0));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2732
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();

    CString sDescription;

    BYTE nBestClass = pSprite->GetDerivedStats()->GetBestClass();
    STRREF strDescription = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetClassDescriptionStringRef(nBestClass, 0);
    sDescription = FetchString(strDescription);

    pText->DisplayString(CString(""),
        sDescription,
        pText->m_rgbLabelColor,
        pText->m_rgbTextColor,
        -1,
        FALSE,
        TRUE);
}

// 0x5D8B50
void CScreenCharacter::sub_5D8B50(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(53);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2777
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(0));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2780
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();

    CString sDescription;

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    CString sRace = ruleTables.GetRaceString(pSprite->m_startTypeAI.m_nRace, 0);
    CString sSubRaceFile = ruleTables.m_tSrTable.GetAt(CString("SRFILE"), sRace);

    C2DArray tSubRace;
    tSubRace.Load(CResRef(sSubRaceFile));

    BYTE nSubRaceListIndex = static_cast<BYTE>(iwd2r_atol(tSubRace.GetAt(CPoint(0, pSprite->m_startTypeAI.m_nSubRace))));

    // NOTE: The column is the constant at 0x84EB2E.
    sDescription = FetchString(iwd2r_atol(ruleTables.m_tSrList.GetAt(CPoint(3, nSubRaceListIndex))));

    pText->DisplayString(CString(""),
        sDescription,
        pText->m_rgbLabelColor,
        pText->m_rgbTextColor,
        -1,
        FALSE,
        TRUE);
}

// 0x5D8EA0
void CScreenCharacter::LevelUpStartAbilities(CGameSprite* pSprite)
{
    CDerivedStats& DStats = *pSprite->GetDerivedStats();
    CCreatureFileHeader& BStats = *pSprite->GetBaseStats();
    CDerivedStats oldDStats;
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    // NOTE: Unused.
    CResRef v1;

    oldDStats = DStats;

    ruleTables.GetNextLevel(m_nClass, DStats, pSprite);

    INT nNewFortitude = ruleTables.GetSavingThrow(pSprite->m_startTypeAI, DStats, BStats.m_CONBase, CRuleTables::FORTITUDE);
    INT nNewReflex = ruleTables.GetSavingThrow(pSprite->m_startTypeAI, DStats, BStats.m_DEXBase, CRuleTables::REFLEX);
    INT nNewWill = ruleTables.GetSavingThrow(pSprite->m_startTypeAI, DStats, BStats.m_WISBase, CRuleTables::WILL);

    INT nOldFortitude = ruleTables.GetSavingThrow(m_typeAI, oldDStats, m_pTempBaseStats->m_CONBase, CRuleTables::FORTITUDE);
    INT nOldReflex = ruleTables.GetSavingThrow(m_typeAI, oldDStats, m_pTempBaseStats->m_DEXBase, CRuleTables::REFLEX);
    INT nOldWill = ruleTables.GetSavingThrow(m_typeAI, oldDStats, m_pTempBaseStats->m_WISBase, CRuleTables::WILL);

    BStats.m_saveVSFortitudeBase += static_cast<BYTE>(max(nNewFortitude - nOldFortitude, 0));
    BStats.m_saveVSReflexBase += static_cast<BYTE>(max(nNewReflex - nOldReflex, 0));
    BStats.m_saveVSWillBase += static_cast<BYTE>(max(nNewWill - nOldWill, 0));

    // NOTE: The original keeps the class levels of the last hit point roll in
    // 11 bytes at 0x7282 and rolls again only when the new class levels differ
    // from them. It stores the base (pre-level-up) levels there, which the new
    // levels never equal, so every level-up rolls. Our CGameSprite.h declares
    // only the first three bytes (field_7282..field_7284, owned by another
    // stream): we roll every time, as the original does in practice, and keep
    // the declared bytes and the roll up to date.
    INT nHitPoints = ruleTables.GetStartingHitPoints(pSprite->m_startTypeAI,
        oldDStats,
        DStats,
        BStats.m_CONBase);
    pSprite->field_7282 = BStats.m_classLevels[0];
    pSprite->field_7283 = BStats.m_classLevels[1];
    pSprite->field_7284 = BStats.m_classLevels[2];
    pSprite->field_728E = nHitPoints;

    field_86A = pSprite->field_728E;

    BStats.m_maxHitPointsBase += static_cast<SHORT>(nHitPoints);
    BStats.m_hitPoints += static_cast<SHORT>(nHitPoints);

    INT nAttackBase;
    INT nUnused1;
    INT nUnused2;
    ruleTables.sub_546B60(pSprite, nAttackBase, nUnused1, nUnused2, FALSE);
    BStats.m_attackBase = static_cast<BYTE>(nAttackBase);

    DStats.m_nLayOnHandsAmount = ruleTables.GetLayOnHandsAmount(pSprite->m_startTypeAI, DStats);

    memcpy(m_levelUpSkills, BStats.m_skills, sizeof(m_levelUpSkills));
    memcpy(m_levelUpFeatValues, &BStats.m_featBow, sizeof(m_levelUpFeatValues));
    memcpy(m_levelUpFeatFlags, BStats.m_feats, sizeof(m_levelUpFeatFlags));

    BStats.m_characterLevel = DStats.m_nLevel;
    memcpy(BStats.m_classLevels, DStats.m_nClassLevels, sizeof(BStats.m_classLevels));

    LONG nHPCONBonusTotalOld = pSprite->m_nHPCONBonusTotalOld;
    SHORT nHitPointsCurrent = BStats.m_hitPoints;
    SHORT nMaxHitPoints = DStats.m_nMaxHitPoints;
    WORD nMaxHitPointsBase = BStats.m_maxHitPointsBase;
    pSprite->field_562C = 1;
    pSprite->ProcessEffectList();
    BStats.m_hitPoints = nHitPointsCurrent;
    DStats.m_nMaxHitPoints = nMaxHitPoints;
    BStats.m_maxHitPointsBase = nMaxHitPointsBase;
    pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;
}

// 0x5D9610
BOOL CScreenCharacter::ResetAbility(CGameSprite* pSprite, const CString& sMin, const CString& sMax, const CString& sMod, BYTE& nAbility, BYTE& nMin, BYTE& nMax, INT& nMod)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    INT nBestClass = pSprite->m_derivedStats.GetBestClass();

    CString sRace = ruleTables.GetRaceString(pSprite->m_startTypeAI.m_nRace, pSprite->m_startTypeAI.m_nSubRace);
    BYTE nMinRaceReq = static_cast<BYTE>(iwd2r_atol(ruleTables.m_tAbilityRaceReq.GetAt(sMin, sRace)));

    // NOTE: Unused.
    iwd2r_atol(ruleTables.m_tAbilityRaceReq.GetAt(sMax, sRace));

    INT nRaceAdj = iwd2r_atol(ruleTables.m_tAbilityRaceAdj.GetAt(sMod, sRace));

    CString sClass = ruleTables.GetClassString(nBestClass, pSprite->m_baseStats.m_specialization);
    BYTE nMinClassReq = static_cast<BYTE>(iwd2r_atol(ruleTables.m_tAbilityClassReq.GetAt(sMin, sClass)));

    nMax = 50;
    if (nMinRaceReq + nRaceAdj < nMinClassReq) {
        nMin = nMinClassReq;
    } else {
        nMin = static_cast<BYTE>(nMinRaceReq + nRaceAdj);
    }
    nMin = max(nMin, 1);
    nMod = nRaceAdj;

    return TRUE;
}

// 0x5D97B0
void CScreenCharacter::ResetAbilitiesPanel(CGameSprite* pSprite, int a2)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(7);

    if (a2) {
        m_nExtraAbilityPoints = pSprite->GetDerivedStats()->m_nLevel / 4 - m_pTempDerivedStats->m_nLevel / 4;
        ResetAbility(pSprite,
            CRuleTables::MIN_STR,
            CRuleTables::MAX_STR,
            CRuleTables::MOD_STR,
            pSprite->GetBaseStats()->m_STRBase,
            m_nMinSTR,
            m_nMaxSTR,
            m_nModSTR);
        ResetAbility(pSprite,
            CRuleTables::MIN_DEX,
            CRuleTables::MAX_DEX,
            CRuleTables::MOD_DEX,
            pSprite->GetBaseStats()->m_DEXBase,
            m_nMinDEX,
            m_nMaxDEX,
            m_nModDEX);
        ResetAbility(pSprite,
            CRuleTables::MIN_CON,
            CRuleTables::MAX_CON,
            CRuleTables::MOD_CON,
            pSprite->GetBaseStats()->m_CONBase,
            m_nMinCON,
            m_nMaxCON,
            m_nModCON);
        ResetAbility(pSprite,
            CRuleTables::MIN_INT,
            CRuleTables::MAX_INT,
            CRuleTables::MOD_INT,
            pSprite->GetBaseStats()->m_INTBase,
            m_nMinINT,
            m_nMaxINT,
            m_nModINT);
        ResetAbility(pSprite,
            CRuleTables::MIN_WIS,
            CRuleTables::MAX_WIS,
            CRuleTables::MOD_WIS,
            pSprite->GetBaseStats()->m_WISBase,
            m_nMinWIS,
            m_nMaxWIS,
            m_nModWIS);
        ResetAbility(pSprite,
            CRuleTables::MIN_CHR,
            CRuleTables::MAX_CHR,
            CRuleTables::MOD_CHR,
            pSprite->GetBaseStats()->m_CHRBase,
            m_nMinCHR,
            m_nMaxCHR,
            m_nModCHR);

        m_nMaxDEX = 40;
        m_nMaxSTR = 40;
        m_nMaxCHR = 40;
        m_nMaxWIS = 40;
        m_nMaxINT = 40;
        m_nMaxCON = 40;
    }

    UpdateHelp(pPanel->m_nID, 29, 27337);
}

// 0x5D9970
void CScreenCharacter::ResetSpellsPanel(CGameSprite* pSprite, int a2)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const CRuleTables& ruleTables = pGame->GetRuleTables();

    CUIPanel* pPanel = m_cUIManager.GetPanel(8);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3095
    UTIL_ASSERT(pPanel != NULL);

    if (m_nClass == CAIOBJECTTYPE_C_BARD) {
        UINT nLevel = m_nSpellLevel - 1;
        CString sMax = ruleTables.m_tKnownSpellsBard.GetAt(CPoint(m_nSpellLevel - 1,
            pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_BARD) - 1));
        CGameSpriteSpellList* pSpells = pSprite->GetSpellsAtLevel(m_nClass, nLevel);
        INT nKnown = static_cast<INT>(pSpells->m_List.size());
        m_nExtraMageSpells = iwd2r_atol(sMax) - nKnown;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3101
        UTIL_ASSERT(m_nExtraMageSpells > 0);
    } else if (m_nClass == CAIOBJECTTYPE_C_SORCERER) {
        UINT nLevel = m_nSpellLevel - 1;
        CString sMax = ruleTables.m_tKnownSpellsSorcerer.GetAt(CPoint(m_nSpellLevel - 1,
            pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_SORCERER) - 1));
        CGameSpriteSpellList* pSpells = pSprite->GetSpellsAtLevel(m_nClass, nLevel);
        INT nKnown = static_cast<INT>(pSpells->m_List.size());
        m_nExtraMageSpells = iwd2r_atol(sMax) - nKnown;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3107
        UTIL_ASSERT(m_nExtraMageSpells > 0);
    } else if ((m_nSpecialization & SPECMASK_WIZARD_UNIVERSAL) != 0) {
        m_nExtraMageSpells = 2;
    } else {
        m_nExtraMageSpells = 3;
    }

    UINT nLevel = m_nSpellLevel - 1;
    UINT nClassIndex = pGame->GetSpellcasterIndex(m_nClass);

    // __FILE__: .\Include\InfGame.h
    // __LINE__: 772
    UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

    INT nCount = pGame->m_spellsByClass[nClassIndex].m_lists[nLevel].m_nCount;
    INT nUsable = 0;
    INT nIndex = 0;

    for (DWORD nButtonID = 0; nButtonID < 30; nButtonID++) {
        CUIControlButtonCharacter5EBC60* pButton = static_cast<CUIControlButtonCharacter5EBC60*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3130
        UTIL_ASSERT(pButton != NULL);

        CResRef resRef;
        BOOL bFound = FALSE;
        while (nIndex < nCount) {
            UINT nID = pGame->m_spellsByClass[pGame->GetSpellcasterIndex(m_nClass)].m_lists[m_nSpellLevel - 1].Get(nIndex);
            resRef = pGame->m_spells.Get(nID);

            CSpell cProbe;
            cProbe.SetResRef(resRef, TRUE, TRUE);
            if (cProbe.pRes != NULL) {
                bFound = TRUE;
                break;
            }

            nIndex++;
        }

        if (bFound) {
            pButton->SetSpell(resRef);

            CSpell cSpell;
            cSpell.SetResRef(resRef, TRUE, TRUE);
            cSpell.Demand();

            DWORD dwClassMask = pGame->GetClassMask(m_nClass);

            BOOL bEnabled;
            if (cSpell.pRes != NULL
                && cSpell.CheckUsableBy(pSprite) == TRUE
                && !pSprite->sub_725330(resRef, dwClassMask, m_nSpellLevel - 1, FALSE)) {
                bEnabled = TRUE;
            } else {
                bEnabled = FALSE;
            }

            pButton->SetEnabled(bEnabled);

            cSpell.Release();

            nIndex++;

            if (bEnabled) {
                nUsable++;
            }
        } else {
            pButton->SetSpell(CResRef(""));
            pButton->SetEnabled(FALSE);
        }
    }

    m_nExtraMageSpells = min(m_nExtraMageSpells, nUsable);

    for (DWORD nButtonID = 0; nButtonID < 30; nButtonID++) {
        CUIControlButton3State* pButton = static_cast<CUIControlButton3State*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3179
        UTIL_ASSERT(pButton != NULL);

        pButton->SetSelected(FALSE);
    }

    UpdateLabel(pPanel,
        0x10000021,
        "%s: %s %d",
        (LPCSTR)FetchString(31614), // "Arcane Spell Selection"
        (LPCSTR)FetchString(7192), // "Level"
        m_nSpellLevel);

    CString sNumber;
    sNumber.Format("%d", m_nExtraMageSpells);
    g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_NUMBER, sNumber);

    if (m_nClass == CAIOBJECTTYPE_C_SORCERER || m_nClass == CAIOBJECTTYPE_C_BARD) {
        UpdateHelp(pPanel->m_nID, 30, 119);
    } else {
        UpdateHelp(pPanel->m_nID, 30, 17250);
    }
}

// 0x5DA120
void CScreenCharacter::ResetClassSelectionPanel(CUIPanel* pPanel, CGameSprite* pSprite)
{
    pSprite->SetAIType(m_typeAI, TRUE, TRUE);

    CAIObjectType typeAI;
    typeAI.Set(pSprite->m_startTypeAI);
    typeAI.m_nClassMask = 0;

    for (INT iClassType = 1; iClassType <= CAIOBJECT_CLASS_MAX; iClassType++) {
        if (pSprite->GetDerivedStats()->GetClassLevel(iClassType) != 0) {
            typeAI.AddClass(iClassType);
        }
    }

    pSprite->SetAIType(typeAI, TRUE, TRUE);
    m_nClass = 0;

    UpdateHelp(pPanel->m_nID, 13, 17242);
}

// 0x5DA200
void CScreenCharacter::ResetFeatsPanel(CUIPanel* pPanel, CGameSprite* pSprite, int a3)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    CString sNumber;

    if (a3) {
        LONG nHPCONBonusTotalOld = pSprite->m_nHPCONBonusTotalOld;
        WORD nMaxHitPointsBase = pSprite->m_baseStats.m_maxHitPointsBase;
        SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
        SHORT nMaxHitPoints = pSprite->m_derivedStats.m_nMaxHitPoints;
        pSprite->field_562C = 1;
        pSprite->ProcessEffectList();
        pSprite->m_baseStats.m_hitPoints = nHitPoints;
        pSprite->m_derivedStats.m_nMaxHitPoints = nMaxHitPoints;
        pSprite->m_baseStats.m_maxHitPointsBase = nMaxHitPointsBase;
        pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;

        m_nExtraFeats = m_nLevelUpFeats;

        for (m_nTopFeat = 0; m_nTopFeat < FEAT_COUNT; m_nTopFeat++) {
            m_storedFeats[m_nTopFeat] = pSprite->GetFeatValue(m_nTopFeat);
        }

        // Scroll to first available feat.
        for (m_nTopFeat = 0; m_nTopFeat < FEAT_COUNT; m_nTopFeat++) {
            // NOTE: The feat id is fetched and dropped; the check uses the
            // feat number.
            ruleTables.GetFeatId(m_nTopFeat);
            if (pSprite->sub_763A40(m_nTopFeat, 1)) {
                break;
            }
        }
    } else {
        m_nTopFeat = 0;
    }

    sNumber.Format("%d", m_nExtraFeats);
    g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_NUMBER, sNumber);
    UpdateHelp(pPanel->m_nID, 92, 36476);

    INT nSlot = 0;
    for (INT nButtonID = 15; nButtonID < 33; nButtonID += 2) {
        BOOL bEnabled = pSprite->sub_763A40(ruleTables.GetFeatId(nSlot), 1) != FALSE;

        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID - 1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3330
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(bEnabled);
        pButton->SetActive(bEnabled);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3337
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(bEnabled);
        pButton->SetActive(bEnabled);

        nSlot++;
    }
}

// 0x5DA490
void CScreenCharacter::UpdateHatedRacePanel(CGameSprite* pSprite)
{
    CCreatureFileHeader& BStats = *pSprite->GetBaseStats();

    CUIPanel* pPanel = m_cUIManager.GetPanel(16);

    if (pPanel != NULL) {
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(1));

        for (INT nSlot = 0; nSlot < 11; nSlot++) {
            INT nIndex = m_nTopHatedRace + nSlot;

            // NOTE: Uninline.
            BYTE nRace = GetHatedRace(nIndex);

            CUIControlButtonCharacterHatedRaceSelection* pButton = static_cast<CUIControlButtonCharacterHatedRaceSelection*>(pPanel->GetControl(nSlot + 22));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 3391
            UTIL_ASSERT(pButton != NULL);

            STRREF strRace = GetRangerHatedRaceStrref(nRace);
            pButton->SetText(FetchString(strRace));

            pButton->SetSelected(m_hatedRaces[nIndex] == BStats.m_favoredEnemies[m_nHatedRaceIndex]);
            pButton->SetEnabled(!g_pBaldurChitin->GetObjectGame()->GetRuleTables().IsHatedRace(nRace, BStats) || pButton->m_bSelected);
        }

        CUIControlScrollBarCharacterHatedRace* pScrollBar = static_cast<CUIControlScrollBarCharacterHatedRace*>(pPanel->GetControl(1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3403
        UTIL_ASSERT(pScrollBar != NULL);

        // NOTE: Uninline.
        pScrollBar->UpdateScrollBar();

        CUIControlButton* pDoneButton = static_cast<CUIControlButton*>(pPanel->GetControl(10));
        pDoneButton->SetEnabled(IsDoneButtonClickable(pSprite));

        CUIControlButton* pCancelButton = static_cast<CUIControlButton*>(pPanel->GetControl(11));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3413
        UTIL_ASSERT(pCancelButton != NULL);

        pCancelButton->SetEnabled(TRUE);
    }
}

// 0x5DA6E0
void CScreenCharacter::UpdateSpecializationPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(6);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3440
    UTIL_ASSERT(pPanel != NULL);

    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(11));

    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    for (DWORD nButtonID = 2; nButtonID <= 10; nButtonID++) {
        CUIControlButtonCharacterSpecializationSelection* pButton = static_cast<CUIControlButtonCharacterSpecializationSelection*>(pPanel->GetControl(nButtonID));
        DWORD nSpecialization = pButton->GetSpecialization();
        BOOLEAN bSelected = (nSpecialization & m_nSpecialization) != 0;
        BOOLEAN bEnabled = rule.IsValidAlignment(m_nClass,
            pSprite->m_startTypeAI.GetAlignment(),
            nSpecialization);

        switch (m_nClass) {
        case CAIOBJECTTYPE_C_WIZARD:
            if (1) {
                CString sClass;
                pButton->SetSelected(bSelected);
                pButton->SetEnabled(bEnabled);

                rule.GetClassStringLower(CAIOBJECTTYPE_C_WIZARD,
                    pButton->GetSpecialization(),
                    0,
                    sClass,
                    1);
                pButton->SetText(sClass);
            }
            break;
        case CAIOBJECTTYPE_C_CLERIC:
            if (1) {
                pButton->SetSelected(bSelected);
                pButton->SetEnabled(bEnabled);

                STRREF strSpecialization;
                switch (nButtonID) {
                case 2:
                    strSpecialization = 38097;
                    break;
                case 3:
                    strSpecialization = 38098;
                    break;
                case 4:
                    strSpecialization = 38099;
                    break;
                case 5:
                    strSpecialization = 38100;
                    break;
                case 6:
                    strSpecialization = 38101;
                    break;
                case 7:
                    strSpecialization = 38102;
                    break;
                case 8:
                    strSpecialization = 38103;
                    break;
                case 9:
                    strSpecialization = 38106;
                    break;
                case 10:
                    strSpecialization = 38107;
                    break;
                }

                pButton->SetText(FetchString(strSpecialization));
            }
            break;
        default:
            if (nButtonID <= 4) {
                pButton->SetSelected(bSelected);
                pButton->SetEnabled(bEnabled);

                switch (m_nClass) {
                case CAIOBJECTTYPE_C_PALADIN:
                    switch (nButtonID) {
                    case 2:
                        pButton->SetText(FetchString(36875));
                        break;
                    case 3:
                        pButton->SetText(FetchString(36872));
                        break;
                    case 4:
                        pButton->SetText(FetchString(36873));
                        break;
                    }
                    break;
                case CAIOBJECTTYPE_C_MONK:
                    switch (nButtonID) {
                    case 2:
                        pButton->SetText(FetchString(36877));
                        break;
                    case 3:
                        pButton->SetText(FetchString(36878));
                        break;
                    case 4:
                        pButton->SetText(FetchString(36879));
                        break;
                    }
                    break;
                default:
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 3500
                    UTIL_ASSERT(FALSE);
                }
            } else {
                pButton->SetSelected(FALSE);
                pButton->SetEnabled(FALSE);
                pButton->SetActive(FALSE);
                pButton->SetInactiveRender(FALSE);
            }
            break;
        }
    }

    CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(0));
    pDone->SetEnabled(IsDoneButtonClickable(pSprite));
}

// 0x5DAA40
void CScreenCharacter::UpdateAbilitiesPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(7);

    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(28));

    HighlightLabel(pPanel,
        0x10000002,
        m_nExtraAbilityPoints > 0,
        COLOR_LABEL_HIGHLIGHT_BONUS);
    UpdateLabel(pPanel,
        0x10000002,
        "%d",
        m_nExtraAbilityPoints);

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    // STR
    UpdateLabel(pPanel,
        0x10000003,
        "%d",
        pSprite->m_baseStats.m_STRBase);
    UpdateLabel(pPanel,
        0x10000024,
        "%+d",
        ruleTables.GetAbilityScoreModifier(pSprite->m_baseStats.m_STRBase));
    HighlightLabel(pPanel,
        0x10000024,
        pSprite->m_baseStats.m_STRBase != 10 && pSprite->m_baseStats.m_STRBase != 11,
        pSprite->m_baseStats.m_STRBase < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

    // DEX
    UpdateLabel(pPanel,
        0x10000004,
        "%d",
        pSprite->m_baseStats.m_DEXBase);
    UpdateLabel(pPanel,
        0x10000025,
        "%+d",
        ruleTables.GetAbilityScoreModifier(pSprite->m_baseStats.m_DEXBase));
    HighlightLabel(pPanel,
        0x10000025,
        pSprite->m_baseStats.m_DEXBase != 10 && pSprite->m_baseStats.m_DEXBase != 11,
        pSprite->m_baseStats.m_DEXBase < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

    // CON
    UpdateLabel(pPanel,
        0x10000005,
        "%d",
        pSprite->m_baseStats.m_CONBase);
    UpdateLabel(pPanel,
        0x10000026,
        "%+d",
        ruleTables.GetAbilityScoreModifier(pSprite->m_baseStats.m_CONBase));
    HighlightLabel(pPanel,
        0x10000026,
        pSprite->m_baseStats.m_CONBase != 10 && pSprite->m_baseStats.m_CONBase != 11,
        pSprite->m_baseStats.m_CONBase < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

    // INT
    UpdateLabel(pPanel,
        0x10000006,
        "%d",
        pSprite->m_baseStats.m_INTBase);
    UpdateLabel(pPanel,
        0x10000027,
        "%+d",
        ruleTables.GetAbilityScoreModifier(pSprite->m_baseStats.m_INTBase));
    HighlightLabel(pPanel,
        0x10000027,
        pSprite->m_baseStats.m_INTBase != 10 && pSprite->m_baseStats.m_INTBase != 11,
        pSprite->m_baseStats.m_INTBase < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

    // WIS
    UpdateLabel(pPanel,
        0x10000007,
        "%d",
        pSprite->m_baseStats.m_WISBase);
    UpdateLabel(pPanel,
        0x10000028,
        "%+d",
        ruleTables.GetAbilityScoreModifier(pSprite->m_baseStats.m_WISBase));
    HighlightLabel(pPanel,
        0x10000028,
        pSprite->m_baseStats.m_WISBase != 10 && pSprite->m_baseStats.m_WISBase != 11,
        pSprite->m_baseStats.m_WISBase < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

    // CHR
    UpdateLabel(pPanel,
        0x10000008,
        "%d",
        pSprite->m_baseStats.m_CHRBase);
    UpdateLabel(pPanel,
        0x10000029,
        "%+d",
        ruleTables.GetAbilityScoreModifier(pSprite->m_baseStats.m_CHRBase));
    HighlightLabel(pPanel,
        0x10000029,
        pSprite->m_baseStats.m_CHRBase != 10 && pSprite->m_baseStats.m_CHRBase != 11,
        pSprite->m_baseStats.m_CHRBase < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

    CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(0));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3664
    UTIL_ASSERT(pButton != NULL);

    pButton->SetEnabled(IsDoneButtonClickable(pSprite));

    for (INT nIndex = 0; nIndex < 12; nIndex += 2) {
        CUIControlButton* pButton;

        BOOL bCanInc = m_nExtraAbilityPoints != 0;
        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(16 + nIndex));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3685
        UTIL_ASSERT(pButton != NULL);

        pButton->SetActive(bCanInc);
        pButton->SetEnabled(bCanInc);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(17 + nIndex));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3692
        UTIL_ASSERT(pButton != NULL);

        BOOL bCanDec;
        switch (nIndex) {
        case 0:
            bCanDec = m_pTempBaseStats->m_STRBase < pSprite->m_baseStats.m_STRBase;
            break;
        case 2:
            bCanDec = m_pTempBaseStats->m_DEXBase < pSprite->m_baseStats.m_DEXBase;
            break;
        case 4:
            bCanDec = m_pTempBaseStats->m_CONBase < pSprite->m_baseStats.m_CONBase;
            break;
        case 6:
            bCanDec = m_pTempBaseStats->m_INTBase < pSprite->m_baseStats.m_INTBase;
            break;
        case 8:
            bCanDec = m_pTempBaseStats->m_WISBase < pSprite->m_baseStats.m_WISBase;
            break;
        case 10:
            bCanDec = m_pTempBaseStats->m_CHRBase < pSprite->m_baseStats.m_CHRBase;
            break;
        }

        pButton->SetActive(bCanDec);
        pButton->SetEnabled(bCanDec);
    }
}

// 0x5D8190
void CScreenCharacter::UpdatePortraitList(CUIPanel* pPanel, DWORD dwControlId, INT nSelected)
{
    CString sPortrait;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2079
    UTIL_ASSERT(m_pPortraits != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2080
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(dwControlId));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2082
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();
    pText->m_rgbHighlightColor = CBaldurChitin::TEXTDISPLAY_COLOR_HIGHLIGHT;
    pText->field_A68 = 32767;

    INT nIndex = 0;
    POSITION pos = m_pPortraits->GetHeadPosition();
    while (pos != NULL) {
        COLORREF rgbTextColor = CBaldurChitin::TEXTDISPLAY_COLOR_SELECT;
        if (nSelected != nIndex) {
            rgbTextColor = pText->m_rgbTextColor;
        }

        sPortrait = m_pPortraits->GetAt(pos);
        pText->DisplayString(CString(""),
            sPortrait,
            pText->m_rgbLabelColor,
            rgbTextColor,
            nIndex,
            FALSE,
            TRUE);

        m_pPortraits->GetNext(pos);
        nIndex++;
    }

    pText->SetTopString(pText->m_plstStrings->FindIndex(0));
}

// 0x5D8300
void CScreenCharacter::ResetCustomSoundsPanel(CUIPanel* pPanel, CGameSprite* pSprite)
{
    CString sSound;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2137
    UTIL_ASSERT(pGame != NULL);

    m_pSounds = pGame->GetSounds();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2146
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(5));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 2148
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();
    pText->m_rgbHighlightColor = CBaldurChitin::TEXTDISPLAY_COLOR_HIGHLIGHT;
    pText->field_A68 = 32767;

    INT nIndex = 0;
    POSITION pos = m_pSounds->GetHeadPosition();
    while (pos != NULL) {
        // FIXME: Unnecessary copy.
        sSound = m_pSounds->GetAt(pos);
        pText->DisplayString(CString(""),
            sSound,
            pText->m_rgbLabelColor,
            pText->m_rgbTextColor,
            nIndex,
            FALSE,
            TRUE);

        m_pSounds->GetNext(pos);
        nIndex++;
    }

    pos = pText->m_plstStrings->FindIndex(0);
    pText->SetTopString(pos);

    m_nCustomSoundSetIndex = -1;
    m_nCustomSoundIndex = 3;

    UpdateHelp(pPanel->m_nID, 8, 11315);
}

// 0x66A540
void CScreenCharacter::TimerAsynchronousUpdate()
{
    g_pBaldurChitin->m_pEngineWorld->AsynchronousUpdate(FALSE);
    UpdateCursorShape(0);
    m_cUIManager.TimerAsynchronousUpdate();
    g_pBaldurChitin->GetObjectCursor()->CursorUpdate(pVidMode);
}

// 0x5DBA80
void CScreenCharacter::TimerSynchronousUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    pGame->SynchronousUpdate();

    field_17A4 = g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL;

    if (m_lPopupStack.GetCount() > 0 && m_cUIManager.field_2E == 1) {
        SHORT nPortrait;

        for (nPortrait = 0; nPortrait <= 5; nPortrait++) {
            pGame->sub_5AF420(nPortrait, 1);
        }

        for (nPortrait = 50; nPortrait <= 55; nPortrait++) {
            pGame->sub_5AF420(nPortrait, 1);
        }
    }

    field_17A4 = FALSE;

    m_cUIManager.Render();
    pVidMode->Flip(TRUE);
}

// 0x5DAF30
void CScreenCharacter::UpdateInformationPanel()
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(57);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3747
    UTIL_ASSERT(pPanel != NULL);

    UpdateLabel(pPanel, 0x10000004, "%s", (LPCSTR)FetchString(17088));
    UpdateLabel(pPanel, 0x10000005, "%s", (LPCSTR)FetchString(24766));
    UpdateLabel(pPanel, 0x10000006, "%s", (LPCSTR)FetchString(36361));
    UpdateLabel(pPanel, 0x10000007, "%s", (LPCSTR)FetchString(41403));
    UpdateLabel(pPanel, 0x10000008, "%s", (LPCSTR)FetchString(41393));
    UpdateLabel(pPanel, 0x10000009, "%s", (LPCSTR)FetchString(11983));
    UpdateLabel(pPanel, 0x1000000A, "");
    UpdateLabel(pPanel, 0x1000000B, "");
    UpdateLabel(pPanel, 0x1000000C, "");
    UpdateLabel(pPanel, 0x1000000D, "");
    UpdateLabel(pPanel, 0x1000000E, "");

    for (DWORD nButtonID = 33; nButtonID < 38; nButtonID++) {
        static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID))->SetEnabled(FALSE);
    }

    sub_5F8C90(field_1846);
    sub_5F91F0(field_184A);

    CUIControlScrollbarCharacter5F9410* pScrollBar = static_cast<CUIControlScrollbarCharacter5F9410*>(pPanel->GetControl(4));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3775
    UTIL_ASSERT(pScrollBar != NULL);

    // NOTE: Uninline.
    pScrollBar->UpdateScrollBar();
}

// 0x5DB200
void CScreenCharacter::sub_5DB200(CGameSprite* pSprite)
{
    CResRef resRef;
    CString sFavouriteSpellName;
    CString sFavouriteWeaponName;

    DWORD nPartyChapterKillsXPValue;
    DWORD nPartyChapterKillsNumber;
    DWORD nPartyGameKillsXPValue;
    DWORD nPartyGameKillsNumber;
    GetPartyInformation(nPartyChapterKillsXPValue,
        nPartyChapterKillsNumber,
        nPartyGameKillsXPValue,
        nPartyGameKillsNumber);

    ULONG nCurrentTimeWithParty;
    pSprite->m_cGameStats.GetTimeWithParty(nCurrentTimeWithParty);

    STRREF strStrongestKill;
    pSprite->m_cGameStats.GetStrongestKill(strStrongestKill);

    CUIPanel* pPanel = m_cUIManager.GetPanel(4);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3857
    UTIL_ASSERT(pPanel != NULL);

    if (pSprite->GetBaseStats()->m_name == -1) {
        UpdateLabel(pPanel, 0x10000000, "%s", (LPCSTR)pSprite->GetName());
    } else {
        UpdateLabel(pPanel, 0x10000000, "%s", (LPCSTR)FetchString(pSprite->GetBaseStats()->m_name));
    }

    CString sClass;
    g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetClassStringLower(pSprite->m_startTypeAI.m_nClass,
        pSprite->GetBaseStats()->m_specialization,
        pSprite->GetBaseStats()->m_flags,
        sClass,
        1);
    UpdateLabel(pPanel, 0x10000018, "%s", (LPCSTR)sClass);

    UpdateLabel(pPanel, 0x10000005, "%s", (LPCSTR)FetchString(strStrongestKill));

    CString sCurrentTimeWithParty;
    CTimerWorld::GetCurrentTimeString(nCurrentTimeWithParty, 16043, sCurrentTimeWithParty);
    UpdateLabel(pPanel, 0x10000006, "%s", (LPCSTR)sCurrentTimeWithParty);

    pSprite->m_cGameStats.GetFavouriteSpell(resRef);
    if (resRef != "") {
        CSpell cSpell;
        cSpell.SetResRef(resRef, TRUE, TRUE);

        cSpell.Demand();
        if (cSpell.GetRes() != NULL) {
            sFavouriteSpellName = FetchString(cSpell.GetGenericName());
        } else {
            sFavouriteSpellName = "Bad spell resref";
        }
        cSpell.Release();
    } else {
        sFavouriteSpellName = "";
    }

    UpdateLabel(pPanel, 0x10000007, "%s", (LPCSTR)sFavouriteSpellName);

    sFavouriteWeaponName = "";

    pSprite->m_cGameStats.GetFavouriteWeapon(resRef);
    if (resRef != "") {
        CItem cWeapon(resRef, 0, 0, 0, 0, 0);

        if (cWeapon.GetRes() != NULL) {
            STRREF strType;
            switch (cWeapon.GetItemType()) {
            case 5:
                strType = 10476; // "Bow and <WEAPONNAME>"
                break;
            case 14:
                strType = 10478; // "Sling and <WEAPONNAME>"
                break;
            case 31:
                strType = 10477; // "Crossbow and <WEAPONNAME>"
                break;
            default:
                strType = 10479; // "<WEAPONNAME>"
                break;
            }

            g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_WEAPONNAME,
                FetchString(cWeapon.GetGenericName()));

            sFavouriteWeaponName = FetchString(strType);
        }
    }

    UpdateLabel(pPanel, 0x10000008, "%s", (LPCSTR)sFavouriteWeaponName);

    UpdateLabel(pPanel, 0x10000011, "%d", pSprite->m_cGameStats.m_nChapterKillsXPValue);
    UpdateLabel(pPanel, 0x10000012, "%d", pSprite->m_cGameStats.m_nChapterKillsNumber);
    UpdateLabel(pPanel, 0x10000015, "%d", pSprite->m_cGameStats.m_nGameKillsXPValue);
    UpdateLabel(pPanel, 0x10000016, "%d", pSprite->m_cGameStats.m_nGameKillsNumber);

    UpdateLabel(pPanel,
        0x1000000F,
        "%d%%",
        nPartyChapterKillsXPValue > 0
            ? 100 * pSprite->m_cGameStats.m_nChapterKillsXPValue / nPartyChapterKillsXPValue
            : 0);
    UpdateLabel(pPanel,
        0x10000010,
        "%d%%",
        nPartyChapterKillsNumber > 0
            ? 100 * pSprite->m_cGameStats.m_nChapterKillsNumber / nPartyChapterKillsNumber
            : 0);
    UpdateLabel(pPanel,
        0x10000013,
        "%d%%",
        nPartyGameKillsXPValue > 0
            ? 100 * pSprite->m_cGameStats.m_nGameKillsXPValue / nPartyGameKillsXPValue
            : 0);
    UpdateLabel(pPanel,
        0x10000014,
        "%d%%",
        nPartyGameKillsNumber > 0
            ? 100 * pSprite->m_cGameStats.m_nGameKillsNumber / nPartyGameKillsNumber
            : 0);

    CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(24));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 3975
    UTIL_ASSERT(pDone != NULL);

    pDone->SetEnabled(IsDoneButtonClickable(pSprite));
}

// 0x5DB840
void CScreenCharacter::UpdateSpellsPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(8);
    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(31));

    for (DWORD nButtonID = 0; nButtonID <= 29; nButtonID++) {
        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4036
        UTIL_ASSERT(pButton != NULL);

        pButton->SetActive(pButton->m_bEnabled);
    }

    HighlightLabel(pPanel,
        0x10000022,
        m_nExtraMageSpells != 0,
        COLOR_LABEL_HIGHLIGHT_BONUS);
    UpdateLabel(pPanel,
        0x10000022,
        "%d",
        m_nExtraMageSpells);

    CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(33));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4049
    UTIL_ASSERT(pDone != NULL);

    pDone->SetEnabled(IsDoneButtonClickable(pSprite));

    CUIControlButton* pCancelButton = static_cast<CUIControlButton*>(pPanel->GetControl(32));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4054
    UTIL_ASSERT(pCancelButton != NULL);

    pCancelButton->SetEnabled(TRUE);
}

// 0x5DB950
void CScreenCharacter::GetPartyInformation(DWORD& nPartyChapterKillsXPValue, DWORD& nPartyChapterKillsNumber, DWORD& nPartyGameKillsXPValue, DWORD& nPartyGameKillsNumber)
{
    nPartyChapterKillsXPValue = 0;
    nPartyChapterKillsNumber = 0;
    nPartyGameKillsXPValue = 0;
    nPartyGameKillsNumber = 0;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4093
    UTIL_ASSERT(pGame != NULL);

    for (SHORT nIndex = 0; nIndex < pGame->GetNumCharacters(); nIndex++) {
        LONG nCharacterId = pGame->GetCharacterId(nIndex);

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            nPartyChapterKillsXPValue += pSprite->m_cGameStats.m_nChapterKillsXPValue;
            nPartyChapterKillsNumber += pSprite->m_cGameStats.m_nChapterKillsNumber;

            nPartyGameKillsXPValue += pSprite->m_cGameStats.m_nGameKillsXPValue;
            nPartyGameKillsNumber += pSprite->m_cGameStats.m_nGameKillsNumber;

            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x5DBB20
void CScreenCharacter::EnableMainPanel(BOOL bEnable)
{
    CUIPanel* pMainPanel = m_cUIManager.GetPanel(2);
    CUIPanel* pLeftPanel = m_cUIManager.GetPanel(0);
    CUIPanel* pRightPanel = m_cUIManager.GetPanel(1);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4222
    UTIL_ASSERT(pMainPanel != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4223
    UTIL_ASSERT(pLeftPanel != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4224
    UTIL_ASSERT(pRightPanel != NULL);

    if (bEnable) {
        PlayGUISound(RESREF_SOUND_WINDOWCLOSE);
    }

    pMainPanel->SetEnabled(bEnable);
    pLeftPanel->SetEnabled(bEnable);
    pRightPanel->SetEnabled(bEnable);

    if (CVideo::SCREENWIDTH / (g_pBaldurChitin->field_4A28 ? 2 : 1) != CBaldurChitin::DEFAULT_SCREEN_WIDTH) {
        if (m_cUIManager.GetPanel(-5) != NULL) { // iwd2r: no border panel where its margin is zero
            m_cUIManager.GetPanel(-5)->SetEnabled(bEnable);
        }
        if (m_cUIManager.GetPanel(-4) != NULL) { // iwd2r: no border panel where its margin is zero
            m_cUIManager.GetPanel(-4)->SetEnabled(bEnable);
        }
        if (m_cUIManager.GetPanel(-3) != NULL) { // iwd2r: no border panel where its margin is zero
            m_cUIManager.GetPanel(-3)->SetEnabled(bEnable);
        }
        if (m_cUIManager.GetPanel(-2) != NULL) { // iwd2r: no border panel where its margin is zero
            m_cUIManager.GetPanel(-2)->SetEnabled(bEnable);
        }
    }

    CheckEnablePortaits(1);
    CheckEnableLeftPanel();
}

// NOTE: Duplicate of `CScreenCreateChar::GetRangerHatedRaceStrref`.
//
// 0x5DBC80
STRREF CScreenCharacter::GetRangerHatedRaceStrref(BYTE nRace)
{
    STRREF strRace;
    switch (nRace) {
    case CAIOBJECTTYPE_R_OGRE:
        strRace = 15975;
        break;
    case CAIOBJECTTYPE_R_WYVERN:
        strRace = 37617;
        break;
    case CAIOBJECTTYPE_R_GIANT:
        strRace = 3279;
        break;
    case CAIOBJECTTYPE_R_GOBLIN:
        strRace = 3280;
        break;
    case CAIOBJECTTYPE_R_LIZARDMAN:
        strRace = 3281;
        break;
    case CAIOBJECTTYPE_R_ORC:
        strRace = 3282;
        break;
    case CAIOBJECTTYPE_R_SALAMANDER:
        strRace = 3290;
        break;
    case CAIOBJECTTYPE_R_TROLL:
        strRace = 3288;
        break;
    case CAIOBJECTTYPE_R_UMBERHULK:
        strRace = 3289;
        break;
    case CAIOBJECTTYPE_R_UNDEAD:
        strRace = 3285;
        break;
    case CAIOBJECTTYPE_R_YUANTI:
        strRace = 30850;
        break;
    case CAIOBJECTTYPE_R_HARPY:
        strRace = 37616;
        break;
    case CAIOBJECTTYPE_R_BUGBEAR:
        strRace = 28367;
        break;
    case CAIOBJECTTYPE_R_HOOK_HORROR:
        strRace = 37606;
        break;
    case CAIOBJECTTYPE_R_DRIDER:
        strRace = 37607;
        break;
    case CAIOBJECTTYPE_R_SHAPESHIFTER:
        strRace = 37605;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4439
        UTIL_ASSERT(FALSE);

        strRace = -1;
    }
    return strRace;
}

// 0x5DBDE0
void CScreenCharacter::UpdateMainPanel(BOOL bCharacterChanged)
{
    // FIXME: Unused.
    CString v1;
    CString v2;
    CString v3;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const CRuleTables& rule = pGame->GetRuleTables();

    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        CCreatureFileHeader& BStats = *pSprite->GetBaseStats();
        CDerivedStats& DStats = *pSprite->GetDerivedStats();
        CAIObjectType& typeAI = pSprite->m_startTypeAI;

        CUIPanel* pPanel = m_cUIManager.GetPanel(2);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4497
        UTIL_ASSERT(pPanel != NULL);

        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(46));

        if (m_bMultiPlayerViewable) {
            // STR
            UpdateLabel(pPanel, 0x1000002F, "%d", DStats.m_nSTR);
            HighlightLabel(pPanel,
                0x1000002F,
                DStats.m_nSTR != BStats.m_STRBase,
                CBaldurEngine::COLOR_LABEL_HIGHLIGHT_BONUS);
            UpdateLabel(pPanel,
                0x10000035,
                "%+d",
                rule.GetAbilityScoreModifier(DStats.m_nSTR));
            HighlightLabel(pPanel,
                0x10000035,
                DStats.m_nSTR != 10 && DStats.m_nSTR != 11,
                DStats.m_nSTR < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

            // DEX
            UpdateLabel(pPanel, 0x10000009, "%d", DStats.m_nDEX);
            UpdateLabel(pPanel,
                0x10000036,
                "%+d",
                rule.GetAbilityScoreModifier(DStats.m_nDEX));
            HighlightLabel(pPanel,
                0x10000036,
                DStats.m_nDEX != 10 && DStats.m_nDEX != 11,
                DStats.m_nDEX < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

            // CON
            UpdateLabel(pPanel, 0x1000000A, "%d", DStats.m_nCON);
            UpdateLabel(pPanel,
                0x10000037,
                "%+d",
                rule.GetAbilityScoreModifier(DStats.m_nCON));
            HighlightLabel(pPanel,
                0x10000037,
                DStats.m_nCON != 10 && DStats.m_nCON != 11,
                DStats.m_nCON < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

            // INT
            UpdateLabel(pPanel, 0x1000000B, "%d", DStats.m_nINT);
            UpdateLabel(pPanel,
                0x10000038,
                "%+d",
                rule.GetAbilityScoreModifier(DStats.m_nINT));
            HighlightLabel(pPanel,
                0x10000038,
                DStats.m_nINT != 10 && DStats.m_nINT != 11,
                DStats.m_nINT < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

            // WIS
            UpdateLabel(pPanel, 0x1000000C, "%d", DStats.m_nWIS);
            UpdateLabel(pPanel,
                0x10000039,
                "%+d",
                rule.GetAbilityScoreModifier(DStats.m_nWIS));
            HighlightLabel(pPanel,
                0x10000039,
                DStats.m_nWIS != 10 && DStats.m_nWIS != 11,
                DStats.m_nWIS < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));

            // CHR
            UpdateLabel(pPanel, 0x1000000D, "%d", DStats.m_nCHR);
            UpdateLabel(pPanel,
                0x1000003A,
                "%+d",
                rule.GetAbilityScoreModifier(DStats.m_nCHR));
            HighlightLabel(pPanel,
                0x1000003A,
                DStats.m_nCHR != 10 && DStats.m_nCHR != 11,
                DStats.m_nCHR < 10 ? RGB(255, 0, 0) : RGB(0, 255, 0));
        } else {
            // STR
            UpdateLabel(pPanel, 0x1000002F, "");
            UpdateLabel(pPanel, 0x10000035, "");

            // DEX
            UpdateLabel(pPanel, 0x10000009, "");
            UpdateLabel(pPanel, 0x10000036, "");

            // CON
            UpdateLabel(pPanel, 0x1000000A, "");
            UpdateLabel(pPanel, 0x10000037, "");

            // INT
            UpdateLabel(pPanel, 0x1000000B, "");
            UpdateLabel(pPanel, 0x10000038, "");

            // WIS
            UpdateLabel(pPanel, 0x1000000C, "");
            UpdateLabel(pPanel, 0x10000039, "");

            // CHR
            UpdateLabel(pPanel, 0x1000000D, "");
            UpdateLabel(pPanel, 0x1000003A, "");
        }

        HighlightLabel(pPanel,
            0x10000009,
            DStats.m_nDEX != BStats.m_DEXBase,
            CBaldurEngine::COLOR_LABEL_HIGHLIGHT_BONUS);
        HighlightLabel(pPanel,
            0x1000000A,
            DStats.m_nCON != BStats.m_CONBase,
            CBaldurEngine::COLOR_LABEL_HIGHLIGHT_BONUS);
        HighlightLabel(pPanel,
            0x1000000B,
            DStats.m_nINT != BStats.m_INTBase,
            CBaldurEngine::COLOR_LABEL_HIGHLIGHT_BONUS);
        HighlightLabel(pPanel,
            0x1000000C,
            DStats.m_nWIS != BStats.m_WISBase,
            CBaldurEngine::COLOR_LABEL_HIGHLIGHT_BONUS);
        HighlightLabel(pPanel,
            0x1000000D,
            DStats.m_nCHR != BStats.m_CHRBase,
            CBaldurEngine::COLOR_LABEL_HIGHLIGHT_BONUS);

        if (m_bMultiPlayerViewable) {
            if (DStats.m_spellStates[SPLSTATE_SUPPRESS_HP_INFO]) {
                UpdateLabel(pPanel, 0x10000029, "%c", '?');
            } else {
                UpdateLabel(pPanel, 0x10000029, "%d", BStats.m_hitPoints);
            }

            UpdateLabel(pPanel, 0x1000002A, "%d", DStats.m_nMaxHitPoints);
            UpdateLabel(pPanel, 0x10000028, "%d", pSprite->GetAC());

            CString sRace;
            rule.GetRaceStringMixed(typeAI.m_nRace, sRace, typeAI.m_nSubRace);
            UpdateLabel(pPanel, 0x1000000F, "%s", (LPCSTR)sRace);

            switch (typeAI.m_nAlignment) {
            case 0x00:
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x21:
            case 0x22:
            case 0x23:
            case 0x31:
            case 0x32:
            case 0x33:
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 1546
                UTIL_ASSERT(FALSE);
            }
        } else {
            UpdateLabel(pPanel, 0x10000029, "");
            UpdateLabel(pPanel, 0x1000002A, "");
            UpdateLabel(pPanel, 0x10000028, "");

            CString sRace;
            rule.GetRaceStringMixed(0, sRace, 0);
            UpdateLabel(pPanel, 0x1000000F, "%s", (LPCSTR)sRace);
        }

        if (BStats.m_name != -1) {
            UpdateLabel(pPanel, 0x1000000E, "%s", (LPCSTR)FetchString(BStats.m_name));
        } else {
            UpdateLabel(pPanel, 0x1000000E, "%s", (LPCSTR)pSprite->GetName());
        }

        CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(45));

        SHORT nTopString = 0;
        if (!bCharacterChanged) {
            nTopString = pText->field_5A;
        }

        CString sNameSeparator;
        sNameSeparator.Format("%c", '!');
        pText->m_sNameSeparator = sNameSeparator;

        pText->RemoveAll();

        if (m_bMultiPlayerViewable) {
            for (DWORD nButtonID = 60; nButtonID <= 63; nButtonID++) {
                CUIControlButtonCharacterInformationFolder* pButton = static_cast<CUIControlButtonCharacterInformationFolder*>(pPanel->GetControl(nButtonID));
                pButton->SetSelected(field_1844 == nButtonID - 60);
            }

            switch (field_1844) {
            case 0:
                UpdateGeneralInformation(pText, pSprite);
                break;
            case 1:
                UpdateEquipmentInformation(pText, pSprite);
                break;
            case 2:
                UpdateTextForceColor(pText,
                    RGB(200, 200, 0),
                    "%s",
                    (LPCSTR)FetchString(11983));
                pSprite->DisplaySkills(pText);

                UpdateText(pText, "");

                UpdateTextForceColor(pText,
                    RGB(200, 200, 0),
                    "%s",
                    (LPCSTR)FetchString(36361));
                pSprite->DisplayFeats(pText);
                break;
            case 3:
                UpdateMiscInformation(pText, pSprite);
                break;
            }
        }

        pText->SetTopString(pText->m_plstStrings->FindIndex(nTopString));

        CheckMultiPlayerViewableModifyable();

        CUIControlButtonCharacterPortrait* pPortrait = static_cast<CUIControlButtonCharacterPortrait*>(pPanel->GetControl(2));
        pPortrait->SetPortrait(BStats.m_portraitLarge);

        CUIControlButton* pButton;

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4840
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(m_bMultiPlayerViewable);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(50));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4851
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(m_bMultiPlayerModifyable
            && (DStats.m_generalState & STATE_DEAD) == 0);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(36));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4856
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(m_bMultiPlayerModifyable
            && (BStats.m_flags & 0x800) != 0
            && !DStats.m_bPolymorphed
            && (DStats.m_generalState & STATE_DEAD) == 0);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(37));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 4866
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(CanLevelUp(pSprite)
            && (DStats.m_generalState & STATE_DEAD) == 0);

        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5DC9D0
void CScreenCharacter::UpdateGeneralInformation(CUIControlTextDisplay* pText, CGameSprite* pSprite)
{
    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    CTlkTable& tlk = g_pBaldurChitin->GetTlkTable();
    CString sValue;

    DWORD nSpecialization = pSprite->GetSpecialization();
    CDerivedStats* pDStats = pSprite->GetDerivedStats();
    CCreatureFileHeader* pBStats = pSprite->GetBaseStats();
    const CAIObjectType& typeAI = pSprite->GetAIType();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4914
    UTIL_ASSERT(pDStats != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 4916
    UTIL_ASSERT(pBStats != NULL);

    INT nLevel = pDStats->m_nLevel;

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s - %s %d",
        (LPCSTR)FetchString(40308), // "Levels"
        (LPCSTR)FetchString(40309), // "Character Level"
        nLevel);

    for (INT iClassType = 1; iClassType <= CAIOBJECT_CLASS_MAX; iClassType++) {
        BYTE nClassLevel = static_cast<BYTE>(pDStats->GetClassLevel(iClassType));
        if (nClassLevel) {
            rule.GetClassStringMixed(iClassType,
                nSpecialization,
                pBStats->m_flags,
                sValue,
                typeAI.m_nGender != 2);
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)sValue,
                nClassLevel);
        }
    }

    UpdateText(pText, "");

    if ((pDStats->m_generalState & STATE_DEAD) != 0) {
        UpdateText(pText, "%s", (LPCSTR)FetchString(11829)); // "DEAD"
        UpdateText(pText, "");
    }

    // FIXME: Calls `GetEffectiveCharacterLevel` twice.
    if (pSprite->GetEffectiveCharacterLevel() != nLevel) {
        UpdateText(pText,
            "%s: %d",
            (LPCSTR)FetchString(40311), // "Effective Character Level"
            pSprite->GetEffectiveCharacterLevel());
    }

    rule.GetClassStringMixed(pSprite->GetRacialFavoredClass(nSpecialization),
        nSpecialization,
        pBStats->m_flags,
        sValue,
        typeAI.m_nGender != 2);
    UpdateText(pText,
        "%s: %s",
        (LPCSTR)FetchString(40310), // "Favored Class"
        (LPCSTR)sValue);

    UpdateText(pText, "");

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(17089)); // "Experience"

    sValue.Format("%d", pDStats->GetXP());
    tlk.SetToken(CRuleTables::TOKEN_EXPERIENCE, sValue);

    UpdateText(pText, "%s", (LPCSTR)FetchString(36928)); // "Current: <EXPERIENCE>"

    if (pDStats->m_nLevel < 30) {
        sValue.Format("%d", rule.GetNextLevelXP(pSprite->GetEffectiveCharacterLevel() + 1));
    } else {
        sValue.Format("%s", (LPCSTR)FetchString(24342)); // "Godhood"
    }

    UpdateText(pText,
        "%s: %s",
        (LPCSTR)FetchString(17091), // "Next Level"
        (LPCSTR)sValue);

    INT nMulticlassingPenalty = pSprite->GetMulticlassingPenalty();
    if (nMulticlassingPenalty > 0) {
        sValue.Format("%d%%", nMulticlassingPenalty);
        tlk.SetToken(CRuleTables::XPPENALTY, sValue);
        UpdateText(pText, "%s", (LPCSTR)FetchString(39418)); // "Multiclassing Penalty: <XPPENALTY>"
    }

    UpdateText(pText, "");

    if (pSprite->m_portraitIcons.GetCount() > 0) {
        CString sCode;
        CString sDescription;

        UpdateTextForceColor(pText,
            RGB(200, 200, 0),
            "%s",
            (LPCSTR)FetchString(32052)); // "Current Effects"

        POSITION pos = pSprite->m_portraitIcons.GetHeadPosition();
        while (pos != NULL) {
            INT nState = static_cast<int>(reinterpret_cast<INT_PTR>(pSprite->m_portraitIcons.GetAt(pos)));
            sCode.Format("%c", nState + 'B');
            rule.GetCharacterStateDescription(nState, sDescription);
            pText->DisplayString(sCode,
                sDescription,
                pText->m_rgbLabelColor,
                pText->m_rgbTextColor,
                -1,
                FALSE,
                TRUE);
            pSprite->m_portraitIcons.GetNext(pos);
        }

        UpdateText(pText, "");
    }

    BOOL bHasActiveFeats = FALSE;
    if (pDStats->m_spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE]
        || pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE) > 0
        || pDStats->m_spellStates[SPLSTATE_FEAT_HAMSTRING]
        || pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK) > 0
        || pDStats->m_spellStates[SPLSTATE_FEAT_RAPID_SHOT]) {
        UpdateTextForceColor(pText,
            RGB(200, 200, 0),
            "%s",
            (LPCSTR)FetchString(40312)); // "Active Feats"
        bHasActiveFeats = TRUE;
    }

    if (pSprite->sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)
        && pDStats->m_spellStates[SPLSTATE_FEAT_ARTERIAL_STRIKE]) {
        UpdateText(pText, "%s", (LPCSTR)FetchString(35774)); // "Arterial Strike"
    }

    if (pSprite->sub_763150(CGAMESPRITE_FEAT_EXPERTISE)
        && pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE) > 0) {
        UpdateText(pText,
            "%s %d",
            (LPCSTR)FetchString(35785), // "Expertise"
            pSprite->sub_726270(CGAMESPRITE_FEAT_EXPERTISE));
    }

    if (pSprite->sub_763150(CGAMESPRITE_FEAT_HAMSTRING)
        && pDStats->m_spellStates[SPLSTATE_FEAT_HAMSTRING]) {
        UpdateText(pText, "%s", (LPCSTR)FetchString(35787)); // "Hamstring"
    }

    if (pSprite->sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)
        && pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK) > 0) {
        UpdateText(pText,
            "%s %d",
            (LPCSTR)FetchString(35794), // "Power Attack"
            pSprite->sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK));
    }

    if (pSprite->sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)
        && pDStats->m_spellStates[SPLSTATE_FEAT_RAPID_SHOT]) {
        UpdateText(pText, "%s", (LPCSTR)FetchString(35796)); // "Rapid Shot"
    }

    if (bHasActiveFeats) {
        UpdateText(pText, "");
    }

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(1048)); // "Race"

    rule.GetRaceStringMixed(typeAI.m_nRace,
        sValue,
        typeAI.m_nSubRace);

    UpdateText(pText, "%s", (LPCSTR)sValue);
    UpdateText(pText, "");

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(1049)); // "Alignment"

    rule.GetAlignmentStringMixed(typeAI.m_nAlignment,
        sValue);

    UpdateText(pText, "%s", (LPCSTR)sValue);
    UpdateText(pText, "");

    UpdateSavingThrows(pText, *pDStats);
    UpdateText(pText, "");

    BOOL bHasClassFeatures = FALSE;
    if (pSprite->HasClassMask(CLASSMASK_ROGUE | CLASSMASK_PALADIN | CLASSMASK_MONK | CLASSMASK_CLERIC)) {
        UpdateTextForceColor(pText,
            RGB(200, 200, 0),
            "%s",
            (LPCSTR)FetchString(40314)); // "Class Features"
        bHasClassFeatures = TRUE;
    }

    if (pSprite->HasClassMask(CLASSMASK_PALADIN | CLASSMASK_CLERIC) && pDStats->m_nTurnUndeadLevel != 0) {
        UpdateText(pText,
            "%s: %d",
            (LPCSTR)FetchString(12126), // "Turn Undead Level"
            pDStats->m_nTurnUndeadLevel);
    }

    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_MONK)) {
        sValue.Format("%+d", pSprite->sub_723F60());
        tlk.SetToken(TOKEN_NUMBER, sValue);

        UpdateText(pText,
            "%s",
            (LPCSTR)FetchString(39431)); // "Monk Wisdom Bonus: <number> to AC"
    }

    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_PALADIN)) {
        UpdateText(pText,
            "%s: %d",
            (LPCSTR)FetchString(12127), // "Lay on Hands Amount"
            pDStats->m_nLayOnHandsAmount);
    }

    if (pSprite->GetClassLevel(CAIOBJECTTYPE_C_MONK) >= 7) {
        UpdateText(pText,
            "%s: %d",
            (LPCSTR)FetchString(39749), // "Wholeness of Body"
            rule.GetWholenessOfBodyAmount(typeAI, *pDStats));
    }

    INT nLathanderRenewal = rule.GetLathanderRenewalAmount(typeAI, *pDStats);
    if (nLathanderRenewal > 0) {
        UpdateText(pText,
            "%s: %d",
            (LPCSTR)FetchString(38143), // "Lathander's Renewal"
            nLathanderRenewal);
    }

    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_ROGUE)) {
        UpdateText(pText,
            "%s: %dd%d",
            (LPCSTR)FetchString(24898), // "Sneak Attack Damage"
            IcewindMisc::GetSneakAttackRolls(pSprite),
            IcewindMisc::GetSneakAttackDice());
    }

    if (bHasClassFeatures) {
        UpdateText(pText, "");
    }

    if (pSprite->HasClassLevel(CAIOBJECTTYPE_C_RANGER)) {
        UpdateTextForceColor(pText,
            RGB(200, 200, 0),
            "%s",
            (LPCSTR)FetchString(15982)); // "Favored Enemies"

        for (INT nIndex = 0; nIndex < MAX_HATED_RACES; nIndex++) {
            BYTE nRace = pBStats->m_favoredEnemies[nIndex];
            if (nRace != CAIObjectType::R_NO_RACE) {
                STRREF strRace;
                switch (nRace) {
                case CAIOBJECTTYPE_R_OGRE:
                    strRace = 15975; // "Ogres"
                    break;
                case CAIOBJECTTYPE_R_WYVERN:
                    strRace = 37617; // "Wyverns"
                    break;
                case CAIOBJECTTYPE_R_GIANT:
                    strRace = 61; // "Giants"
                    break;
                case CAIOBJECTTYPE_R_GOBLIN:
                    strRace = 29518; // "Goblins"
                    break;
                case CAIOBJECTTYPE_R_LIZARDMAN:
                    strRace = 3275; // "Lizard Men"
                    break;
                case CAIOBJECTTYPE_R_ORC:
                    strRace = 3271; // "Orcs"
                    break;
                case CAIOBJECTTYPE_R_SALAMANDER:
                    strRace = 3272; // "Salamanders"
                    break;
                case CAIOBJECTTYPE_R_TROLL:
                    strRace = 3276; // "Trolls"
                    break;
                case CAIOBJECTTYPE_R_UMBERHULK:
                    strRace = 3277; // "Umber Hulks"
                    break;
                case CAIOBJECTTYPE_R_UNDEAD:
                    strRace = 3278; // "Undead"
                    break;
                case CAIOBJECTTYPE_R_YUANTI:
                    strRace = 30849; // "Yuan-ti"
                    break;
                case CAIOBJECTTYPE_R_HARPY:
                    strRace = 37616; // "Harpies"
                    break;
                case CAIOBJECTTYPE_R_BUGBEAR:
                    // FIXME: The rest of the list are plurals and there is
                    // 28367 - "Bugbears".
                    strRace = 28366; // "Bugbear"
                    break;
                case CAIOBJECTTYPE_R_HOOK_HORROR:
                    strRace = 37606; // "Hook Horrors"
                    break;
                case CAIOBJECTTYPE_R_DRIDER:
                    strRace = 37607; // "Driders"
                    break;
                case CAIOBJECTTYPE_R_SHAPESHIFTER:
                    strRace = 37605; // "Shapeshifters"
                    break;
                default:
                    strRace = -1;
                    break;
                }

                UpdateText(pText,
                    "%s: +%d",
                    (LPCSTR)FetchString(strRace),
                    rule.GetHatedRaceBonus(nRace, *pBStats));
            }
        }

        UpdateText(pText, "");
    }

    BOOLEAN bHasBonusSpells = FALSE;
    BOOL bBonusSpellsHeader = FALSE;
    if (pSprite->HasClassMask(CLASSMASK_WIZARD | CLASSMASK_SORCERER | CLASSMASK_RANGER | CLASSMASK_PALADIN | CLASSMASK_DRUID | CLASSMASK_CLERIC | CLASSMASK_BARD)) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        for (UINT nIndex = 0; nIndex < 7; nIndex++) {
            BYTE nClass = pGame->GetSpellcasterClass(nIndex);

            DWORD dwClassMask;
            switch (nIndex) {
            case 0:
                dwClassMask = CLASSMASK_BARD;
                break;
            case 1:
                dwClassMask = CLASSMASK_CLERIC;
                break;
            case 2:
                dwClassMask = CLASSMASK_DRUID;
                break;
            case 3:
                dwClassMask = CLASSMASK_PALADIN;
                break;
            case 4:
                dwClassMask = CLASSMASK_RANGER;
                break;
            case 5:
                dwClassMask = CLASSMASK_SORCERER;
                break;
            case 6:
                dwClassMask = CLASSMASK_WIZARD;
                break;
            default:
                dwClassMask = 0;
                break;
            }

            if (pSprite->HasClassMask(dwClassMask)) {
                // NOTE: Only the bonus (the out parameter) is shown; the
                // returned maximum is dropped.
                INT nBonus;
                rule.GetMaxKnownSpells(nClass,
                    typeAI,
                    *pDStats,
                    pBStats->m_specialization,
                    1,
                    nBonus);

                if (nBonus > 0) {
                    bHasBonusSpells = TRUE;

                    CString sClass;
                    rule.GetClassStringMixed(nClass,
                        pBStats->m_specialization,
                        pBStats->m_flags,
                        sClass,
                        typeAI.m_nGender != 2);

                    if (!bBonusSpellsHeader) {
                        UpdateTextForceColor(pText,
                            RGB(200, 200, 0),
                            "%s",
                            (LPCSTR)FetchString(10344)); // "Bonus Spells"
                        bBonusSpellsHeader = TRUE;
                    }

                    UpdateText(pText, "%s", (LPCSTR)sClass);

                    INT nSpellLevel = 1;
                    while (nBonus > 0) {
                        sValue.Format("%d", nSpellLevel);
                        tlk.SetToken(TOKEN_SPELLLEVEL, sValue);

                        UpdateTextNoTrim(pText,
                            "  %s: %+d",
                            (LPCSTR)FetchString(10345), // "Level <SPELLLEVEL>"
                            nBonus);

                        nSpellLevel++;
                        rule.GetMaxKnownSpells(nClass,
                            typeAI,
                            *pDStats,
                            pBStats->m_specialization,
                            nSpellLevel,
                            nBonus);
                        if (nSpellLevel > 9) {
                            break;
                        }
                    }
                }
            }
        }

        if (bHasBonusSpells == TRUE) {
            UpdateText(pText, "");
        }
    }

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(40315)); // "Ability Statistics"

    // NOTE: The original divides and multiplies on the x87 stack (extended
    // precision) with float operands; double keeps the intermediate exact
    // enough for these small values.
    INT nStrengthWeight = iwd2r_atol(rule.m_tStrengthMod.GetAt(CPoint(3, pDStats->m_nSTR)));
    INT nTotalWeight = static_cast<INT>(static_cast<double>(static_cast<float>(rule.GetEncumbranceMod(pSprite))) / 100.0
        * static_cast<double>(static_cast<float>(nStrengthWeight)));

    UpdateText(pText,
        "%s: %d %s",
        (LPCSTR)FetchString(10338), // "Weight Allowance"
        nTotalWeight,
        (LPCSTR)FetchString(39537)); // "lb."

    INT nModCON = rule.GetAbilityScoreModifier(pDStats->m_nCON);

    CString sHitPointsPerLvl;
    if (nModCON != 0) {
        sHitPointsPerLvl.Format("%s: %+d",
            (LPCSTR)FetchString(10342), // "Hit Points/Level"
            nModCON);
    } else {
        sHitPointsPerLvl.Format("%s: %d",
            (LPCSTR)FetchString(10342), // "Hit Points/Level"
            0);
    }

    UpdateText(pText, "%s", (LPCSTR)sHitPointsPerLvl);

    if (pDStats->m_nResistAcid != 0
        || pDStats->m_nResistCold != 0
        || pDStats->m_nResistElectricity != 0
        || pDStats->m_nResistFire != 0
        || pDStats->m_nResistMissile != 0
        || pDStats->m_nResistPoison != 0
        || pDStats->m_nResistCrushing != 0
        || pDStats->m_nResistPiercing != 0
        || pDStats->m_nResistSlashing != 0
        || pDStats->m_nMagicDamageResistance != 0) {
        UpdateTextForceColor(pText,
            RGB(200, 200, 0),
            "%s",
            (LPCSTR)FetchString(15544)); // "Resistances"

        if (pDStats->m_nResistAcid != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(15578), // "Acid"
                pDStats->m_nResistAcid);
        }

        if (pDStats->m_nResistCold != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(15546), // "Cold"
                pDStats->m_nResistCold);
        }

        if (pDStats->m_nResistElectricity != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(15577), // "Electricity"
                pDStats->m_nResistElectricity);
        }

        if (pDStats->m_nResistFire != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(15545), // "Fire"
                pDStats->m_nResistFire);
        }

        if (pDStats->m_nResistMissile != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(11767), // "Missile"
                pDStats->m_nResistMissile);
        }

        if (pDStats->m_nResistPoison != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(38135), // "Poison"
                pDStats->m_nResistPoison);
        }

        if (pDStats->m_nResistCrushing != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(11770), // "Bludgeoning"
                pDStats->m_nResistCrushing);
        }

        if (pDStats->m_nResistPiercing != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(11769), // "Piercing"
                pDStats->m_nResistPiercing);
        }

        if (pDStats->m_nResistSlashing != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(11768), // "Slashing"
                pDStats->m_nResistSlashing);
        }

        if (pDStats->m_nMagicDamageResistance != 0) {
            UpdateText(pText,
                "%s: %d",
                (LPCSTR)FetchString(40319), // "Magic Damage"
                pDStats->m_nMagicDamageResistance);
        }
    }

    UpdateText(pText, "");

    UpdateText(pText,
        "%s: %d",
        (LPCSTR)FetchString(15581), // "Spell Resistance"
        pDStats->m_nResistMagic);

    if (pDStats->m_damageReductions.size() > 0) {
        UpdateText(pText, "");

        UpdateTextForceColor(pText,
            RGB(200, 200, 0),
            "%s",
            (LPCSTR)FetchString(39325)); // "Damage Reduction"

        // NOTE: GetBestGeneric ignores its argument, so the first try (5)
        // decides.
        for (INT nEnchantment = 5; nEnchantment >= 0; nEnchantment--) {
            CDamageReduction* pReduction = pDStats->m_damageReductions.GetBestGeneric(nEnchantment);
            if (pReduction != NULL) {
                UpdateText(pText,
                    "%s: %d/+%d",
                    (LPCSTR)FetchString(41406),
                    pReduction->GetEffectiveAmount(),
                    pReduction->m_nEnchantment);
                break;
            }
        }

        static const struct {
            DWORD dwDamageType;
            STRREF strName;
        } kDamageTypes[] = {
            { 0, 11770 }, // "Bludgeoning"
            { 0x100000, 11769 }, // "Piercing"
            { 0x1000000, 11768 }, // "Slashing"
            { 0x2000000, 41404 },
            { 0x4000000, 41405 },
        };

        for (const auto& damageType : kDamageTypes) {
            for (INT nEnchantment = 5; nEnchantment >= 0; nEnchantment--) {
                CDamageReduction* pReduction = pDStats->m_damageReductions.GetBest(nEnchantment, damageType.dwDamageType);
                if (pReduction != NULL) {
                    UpdateText(pText,
                        "%s: %d/+%d",
                        (LPCSTR)FetchString(damageType.strName),
                        pReduction->GetEffectiveAmount(),
                        pReduction->m_nEnchantment);
                    break;
                }
            }
        }
    }
}

// 0x5DE2C0
void CScreenCharacter::UpdateEquipmentInformation(CUIControlTextDisplay* pText, CGameSprite* pSprite)
{
    // TODO: Incomplete.
}

// 0x5E0200
void CScreenCharacter::UpdateMiscInformation(CUIControlTextDisplay* pText, CGameSprite* pSprite)
{
    CResRef resRef;
    CString sFavouriteSpellName;
    CString sFavouriteWeaponName;

    DWORD nPartyChapterKillsXPValue;
    DWORD nPartyChapterKillsNumber;
    DWORD nPartyGameKillsXPValue;
    DWORD nPartyGameKillsNumber;
    GetPartyInformation(nPartyChapterKillsXPValue,
        nPartyChapterKillsNumber,
        nPartyGameKillsXPValue,
        nPartyGameKillsNumber);

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(40320)); // "Favorites"

    pSprite->m_cGameStats.GetFavouriteSpell(resRef);
    if (resRef != "") {
        CSpell cSpell;
        cSpell.SetResRef(resRef, TRUE, TRUE);

        cSpell.Demand();
        if (cSpell.GetRes() != NULL) {
            sFavouriteSpellName = FetchString(cSpell.GetGenericName());
        } else {
            sFavouriteSpellName = "Bad spell resref";
        }
        cSpell.Release();
    }

    UpdateText(pText,
        "%s: %s",
        (LPCSTR)FetchString(11949), // "Favorite Spell (if applicable)"
        (LPCSTR)sFavouriteSpellName);

    sFavouriteWeaponName = "";

    pSprite->m_cGameStats.GetFavouriteWeapon(resRef);
    if (resRef != "") {
        CItem cWeapon(resRef, 0, 0, 0, 0, 0);

        if (cWeapon.GetRes() != NULL) {
            STRREF strType;
            switch (cWeapon.GetItemType()) {
            case 5:
                strType = 10476; // "Bow and <WEAPONNAME>"
                break;
            case 14:
                strType = 10478; // "Sling and <WEAPONNAME>"
                break;
            case 31:
                strType = 10477; // "Crossbow and <WEAPONNAME>"
                break;
            default:
                strType = 10479; // "<WEAPONNAME>"
                break;
            }

            g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_WEAPONNAME,
                FetchString(cWeapon.GetGenericName()));

            sFavouriteWeaponName = FetchString(strType);
        }
    }

    UpdateText(pText,
        "%s: %s",
        (LPCSTR)FetchString(11950), // "Favorite Weapon"
        (LPCSTR)sFavouriteWeaponName);

    UpdateText(pText, "");

    UpdateTextForceColor(pText,
        RGB(200, 200, 0),
        "%s",
        (LPCSTR)FetchString(40322)); // "Combat Statistics"

    STRREF strName;
    pSprite->m_cGameStats.GetStrongestKill(strName);

    UpdateText(pText,
        "%s: %s",
        (LPCSTR)FetchString(11947), // "Most Powerful Vanquished"
        (LPCSTR)FetchString(strName));

    ULONG nCurrentTimeWithParty;
    pSprite->m_cGameStats.GetTimeWithParty(nCurrentTimeWithParty);

    CString sCurrentTimeWithParty;
    CTimerWorld::GetCurrentTimeString(nCurrentTimeWithParty, 16043, sCurrentTimeWithParty);

    UpdateText(pText,
        "%s: %s",
        (LPCSTR)FetchString(11948), // "Time Spent with Party"
        (LPCSTR)sCurrentTimeWithParty);

    UpdateText(pText,
        "%s: %d",
        (LPCSTR)FetchString(11953), // "Experience Value of Kills"
        pSprite->m_cGameStats.m_nGameKillsXPValue);

    UpdateText(pText,
        "%s: %d",
        (LPCSTR)FetchString(11954), // "Number of Kills"
        pSprite->m_cGameStats.m_nGameKillsNumber);

    UpdateText(pText,
        "%s: %d%%",
        (LPCSTR)FetchString(11951), // "Total Experience Value in Party"
        nPartyGameKillsXPValue > 0
            ? 100 * pSprite->m_cGameStats.m_nGameKillsXPValue / nPartyGameKillsXPValue
            : 0);

    UpdateText(pText,
        "%s: %d%%",
        (LPCSTR)FetchString(11952), // "Percentage of Total Kills in Party"
        nPartyGameKillsNumber > 0
            ? 100 * pSprite->m_cGameStats.m_nGameKillsNumber / nPartyGameKillsNumber
            : 0);
}

// 0x5E0880
void CScreenCharacter::ResetPopupPanel(DWORD dwPanelId, CGameSprite* pSprite, int a3)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(dwPanelId);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 6473
    UTIL_ASSERT(pPanel != NULL);

    switch (pPanel->m_nID) {
    case 4:
    case 57:
        break;
    case 6:
        ResetClassPanel(pSprite, a3);
        break;
    case 7:
        ResetAbilitiesPanel(pSprite, a3);
        break;
    case 8:
        ResetSpellsPanel(pSprite, a3);
        break;
    case 9:
    case 10:
    case 50:
        ResetErrorPanel(pPanel);
        break;
    case 11:
        ResetScriptPanel(pPanel);
        break;
    case 12:
        ResetBiographyPanel(pSprite);
        break;
    case 13:
        ResetExportPanel(pSprite);
        break;
    case 16:
        // NOTE: Uninline.
        ResetHatedRacePanel(pSprite);
        break;
    case 17:
        UpdateHelp(pPanel->m_nID, 5, 11327);
        break;
    case 18:
        ResetAppearancePanel(pPanel, pSprite);
        break;
    case 19:
        ResetCustomPortraitsPanel(pPanel, pSprite);
        break;
    case 20:
        ResetCustomSoundsPanel(pPanel, pSprite);
        break;
    case 51:
        ResetCustomizeBiographyPanel(pPanel, pSprite);
        break;
    case 52:
        ResetClassDescriptionPanel(pSprite);
        break;
    case 53:
        sub_5D8B50(pSprite);
        break;
    case 54:
        ResetClassSelectionPanel(pPanel, pSprite);
        break;
    case 55:
        ResetSkillsPanel(pPanel, pSprite, a3);
        break;
    case 56:
        ResetFeatsPanel(pPanel, pSprite, a3);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 6569
        UTIL_ASSERT(FALSE);
    }
}

// 0x5E0B20
void CScreenCharacter::UpdatePopupPanel(DWORD dwPanelId, CGameSprite* pSprite)
{
    CUIPanel* pPanel;
    CUIControlButton* pButton;

    switch (dwPanelId) {
    case 4:
        sub_5DB200(pSprite);
        break;
    case 6:
        UpdateSpecializationPanel(pSprite);
        break;
    case 7:
        UpdateAbilitiesPanel(pSprite);
        break;
    case 8:
        UpdateSpellsPanel(pSprite);
        break;
    case 9:
    case 10:
    case 50:
        break;
    case 11:
        pPanel = m_cUIManager.GetPanel(11);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 10006
        UTIL_ASSERT(pPanel != NULL);

        if (m_pCurrentScrollBar == NULL) {
            m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(3));
        }

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(5));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 10015
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(IsDoneButtonClickable(pSprite));
        break;
    case 12:
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(m_cUIManager.GetPanel(12)->GetControl(1));
        break;
    case 13:
        pPanel = m_cUIManager.GetPanel(13);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3541
        UTIL_ASSERT(pPanel != NULL);

        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(1));

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(4));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 3547
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(IsDoneButtonClickable(pSprite));
        break;
    case 16:
        UpdateHatedRacePanel(pSprite);
        break;
    case 17:
        UpdateCustomizePanel(pSprite);
        break;
    case 18:
        UpdateAppearancePanel(pSprite);
        break;
    case 19:
        UpdateCustomPortraitsPanel(pSprite);
        break;
    case 20:
        pPanel = m_cUIManager.GetPanel(20);

        // NOTE: The original reads the scroll bar before checking the panel.
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(6));

        if (pPanel != NULL) {
            static_cast<CUIControlButton*>(pPanel->GetControl(7))->SetEnabled(m_nCustomSoundSetIndex >= 0);
            static_cast<CUIControlButton*>(pPanel->GetControl(10))->SetEnabled(IsDoneButtonClickable(pSprite));
        }
        break;
    case 51:
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(m_cUIManager.GetPanel(51)->GetControl(3));
        break;
    case 52:
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(m_cUIManager.GetPanel(52)->GetControl(1));
        break;
    case 53:
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(m_cUIManager.GetPanel(53)->GetControl(1));
        break;
    case 54:
        UpdateClassSelectionPanel(pSprite);
        break;
    case 55:
        UpdateSkillsPanel(pSprite);
        break;
    case 56:
        UpdateFeatsPanel(pSprite);
        break;
    case 57:
        UpdateInformationPanel();
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 6688
        UTIL_ASSERT(FALSE);
    }
}

// 0x5E0E50
CUIPanel* CScreenCharacter::GetTopPopup()
{
    return m_lPopupStack.GetTailPosition() != NULL ? m_lPopupStack.GetTail() : NULL;
}

// NOTE: Inlined.
void CScreenCharacter::ShowPopupPanel(DWORD dwPanelId, BOOL bShow)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(dwPanelId);

    pPanel->SetActive(bShow);
    pPanel->SetInactiveRender(bShow);

    if (bShow) {
        pPanel->InvalidateRect(NULL);
        PlayGUISound(RESREF_SOUND_WINDOWOPEN);
    }
}

// NOTE: Inlined.
void CScreenCharacter::EnablePopupPanel(DWORD dwPanelId, BOOL bEnable)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(dwPanelId);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 6446
    UTIL_ASSERT(pPanel != NULL);

    pPanel->SetEnabled(bEnable);
}

// 0x5E0E70
void CScreenCharacter::SummonPopup(DWORD dwPopupId, CGameSprite* pSprite, int a3)
{
    if (m_cUIManager.m_pFocusedControl != NULL) {
        // NOTE: Uninline.
        m_cUIManager.KillCapture();
        field_1B8 = FALSE;
    }

    if (!m_lPopupStack.IsEmpty()) {
        CUIPanel* pPanel = m_lPopupStack.GetTail();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 6771
        UTIL_ASSERT(pPanel != NULL);

        // NOTE: Uninline.
        EnablePopupPanel(pPanel->m_nID, FALSE);
    } else {
        EnableMainPanel(FALSE);
    }

    CUIPanel* pPanel = m_cUIManager.GetPanel(dwPopupId);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 6777
    UTIL_ASSERT(pPanel != NULL);

    m_lPopupStack.AddTail(pPanel);

    ResetPopupPanel(pPanel->m_nID, pSprite, a3);

    // NOTE: Uninline.
    ShowPopupPanel(pPanel->m_nID, TRUE);

    // NOTE: Uninline.
    EnablePopupPanel(pPanel->m_nID, TRUE);

    UpdatePopupPanel(pPanel->m_nID, pSprite);
}

// 0x5E0FD0
BOOL CScreenCharacter::CheckSummonPopup(DWORD dwPopupId, CGameSprite* pSprite)
{
    CInfGame* pGame;
    INT nValue;
    INT nFeat;

    switch (dwPopupId) {
    case 6:
        if ((m_nClass == CAIOBJECTTYPE_C_WIZARD
                || m_nClass == CAIOBJECTTYPE_C_MONK
                || m_nClass == CAIOBJECTTYPE_C_PALADIN
                || m_nClass == CAIOBJECTTYPE_C_CLERIC)
            && m_pBeforeLevelUpDerivedStats->GetClassLevel(m_nClass) == 0) {
            return TRUE;
        }
        return FALSE;
    case 7:
        return (pSprite->GetDerivedStats()->m_nLevel >> 2) - (m_pTempDerivedStats->m_nLevel >> 2) > 0;
    case 8:
        if (m_nClass == CAIOBJECTTYPE_C_BARD || m_nClass == CAIOBJECTTYPE_C_SORCERER) {
            const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

            m_nSpellLevel++;
            while (m_nSpellLevel <= 9) {
                if (m_nClass == CAIOBJECTTYPE_C_BARD) {
                    INT nOld = iwd2r_atol(ruleTables.m_tKnownSpellsBard.GetAt(CPoint(m_nSpellLevel - 1,
                        m_pTempDerivedStats->GetClassLevel(CAIObjectType::C_BARD) - 1)));
                    INT nNew = iwd2r_atol(ruleTables.m_tKnownSpellsBard.GetAt(CPoint(m_nSpellLevel - 1,
                        pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_BARD) - 1)));
                    m_nExtraMageSpells = nNew - nOld;
                } else if (m_nClass == CAIOBJECTTYPE_C_SORCERER) {
                    INT nOld = iwd2r_atol(ruleTables.m_tKnownSpellsSorcerer.GetAt(CPoint(m_nSpellLevel - 1,
                        m_pTempDerivedStats->GetClassLevel(CAIObjectType::C_SORCERER) - 1)));
                    INT nNew = iwd2r_atol(ruleTables.m_tKnownSpellsSorcerer.GetAt(CPoint(m_nSpellLevel - 1,
                        pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_SORCERER) - 1)));
                    m_nExtraMageSpells = nNew - nOld;
                }

                if (m_nExtraMageSpells > 0) {
                    pGame = g_pBaldurChitin->GetObjectGame();

                    UINT nLevel = m_nSpellLevel - 1;
                    UINT nClassIndex = pGame->GetSpellcasterIndex(m_nClass);

                    // __FILE__: .\Include\InfGame.h
                    // __LINE__: 772
                    UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

                    INT nCount = pGame->m_spellsByClass[nClassIndex].m_lists[nLevel].m_nCount;
                    INT nUsable = 0;
                    INT nIndex = 0;
                    UINT nChecked = 0;

                    while (nIndex < nCount) {
                        UINT nID = pGame->m_spellsByClass[pGame->GetSpellcasterIndex(m_nClass)].m_lists[nLevel].Get(nIndex);
                        CResRef resRef = pGame->m_spells.Get(nID);

                        BOOL bFound;
                        {
                            CSpell cProbe;
                            cProbe.SetResRef(resRef, TRUE, TRUE);
                            bFound = cProbe.pRes != NULL;
                        }

                        if (!bFound) {
                            nIndex++;
                            continue;
                        }

                        CSpell cSpell;
                        cSpell.SetResRef(resRef, TRUE, TRUE);
                        cSpell.Demand();

                        DWORD dwClassMask = pGame->GetClassMask(m_nClass);
                        if (cSpell.pRes != NULL
                            && cSpell.CheckUsableBy(pSprite) == TRUE
                            && !pSprite->sub_725330(resRef, dwClassMask, m_nSpellLevel - 1, FALSE)) {
                            nUsable++;
                        }

                        cSpell.Release();

                        nIndex++;
                        nChecked++;
                        if (nChecked > 29) {
                            break;
                        }
                    }

                    m_nExtraMageSpells = min(m_nExtraMageSpells, nUsable);
                }

                if (m_nExtraMageSpells > 0) {
                    return TRUE;
                }

                m_nSpellLevel++;
            }
        }
        return FALSE;
    case 16:
        if (m_nClass != CAIOBJECTTYPE_C_RANGER) {
            return FALSE;
        }

        // NOTE: 5 is the constant at 0x84EA54.
        if (pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_RANGER) / 5 + 1 - pSprite->GetNextHatedRacesSlot() > 0
            && pSprite->GetNextHatedRacesSlot() < MAX_HATED_RACES) {
            return TRUE;
        }
        return FALSE;
    case 54:
        return TRUE;
    case 55:
        return m_nLevelUpSkillPoints > 0;
    case 56:
        pGame = g_pBaldurChitin->GetObjectGame();

        if (m_nClass == CAIOBJECTTYPE_C_CLERIC
            && m_pBeforeLevelUpDerivedStats->GetClassLevel(CAIObjectType::C_CLERIC) == 0) {
            INT nOffset;
            switch (pSprite->GetSpecialization() & SPECMASK_CLERIC) {
            case SPECMASK_CLERIC_ILMATER:
                nOffset = 0;
                break;
            case SPECMASK_CLERIC_LATHANDER:
                nOffset = 1;
                break;
            case SPECMASK_CLERIC_SELUNE:
                nOffset = 2;
                break;
            case SPECMASK_CLERIC_HELM:
                nOffset = 3;
                break;
            case SPECMASK_CLERIC_OGHMA:
                nOffset = 4;
                break;
            case SPECMASK_CLERIC_TEMPUS:
                nOffset = 5;
                break;
            case SPECMASK_CLERIC_BANE:
                nOffset = 6;
                break;
            case SPECMASK_CLERIC_MASK:
                nOffset = 7;
                break;
            case SPECMASK_CLERIC_TALOS:
                nOffset = 8;
                break;
            default:
                // NOTE: The original uses the low bits of the sprite pointer
                // here (the register still holds it); no deity is unreachable
                // after the specialization popup.
                nOffset = 0;
                break;
            }

            for (nFeat = 0; nFeat < pGame->GetRuleTables().m_tFeats.GetHeight(); nFeat++) {
                nValue = iwd2r_atol(pGame->GetRuleTables().m_tFeats.GetAt(CPoint(nOffset + 14, nFeat)));
                INT nCurrent = pSprite->GetFeatValue(nFeat);
                if (nValue > 0) {
                    pSprite->SetFeatValue(nFeat, max(nValue, nCurrent));
                }
            }
        }

        for (nFeat = 0; nFeat < CGAMESPRITE_FEAT_NUMFEATS; nFeat++) {
            nValue = iwd2r_atol(pGame->GetRuleTables().m_tFeats.GetAt(CPoint(m_nClass + 2, nFeat)));
            if (nValue > pSprite->GetFeatValue(nFeat)) {
                pSprite->SetFeatValue(nFeat, nValue);
            }
        }

        return m_nLevelUpFeats > 0;
    }

    return FALSE;
}

// 0x5E1910
BOOL CScreenCharacter::SummonNextPopup(DWORD dwPopupId, CGameSprite* pSprite)
{
    do {
        switch (dwPopupId) {
        case 2:
            dwPopupId = 54;
            break;
        case 6:
            dwPopupId = 7;
            break;
        case 7:
            dwPopupId = 55;
            break;
        case 8:
            dwPopupId = 16;
            break;
        case 16:
            dwPopupId = 2;
            break;
        case 54:
            dwPopupId = 6;
            break;
        case 55:
            dwPopupId = 56;
            break;
        case 56:
            dwPopupId = 8;
            break;
        default:
            // NOTE: The original goes on with the popup id register still
            // holding the sprite pointer; every caller passes a known id.
            //
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7076
            UTIL_ASSERT(FALSE);
        }

        if (CheckSummonPopup(dwPopupId, pSprite)) {
            SummonPopup(dwPopupId, pSprite, TRUE);
            return TRUE;
        }
    } while (dwPopupId != 2);

    LevelUp(pSprite);

    while (m_lPopupStack.GetTailPosition() != NULL) {
        DismissPopup(pSprite);
    }

    if (!g_pChitin->cNetwork.m_bIsHost
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 7102
        UTIL_ASSERT(pGame != NULL);

        LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);
        CMessage120* pMessage = new CMessage120(g_pChitin->cNetwork.m_idLocalPlayer,
            FALSE,
            nCharacterId,
            nCharacterId);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    } else if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
        g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->sub_518660(g_pChitin->cNetwork.m_idLocalPlayer, FALSE);
    }

    return FALSE;
}

// 0x5E1B20
BOOL CScreenCharacter::SummonPreviousPopup(DWORD dwPopupId, CGameSprite* pSprite)
{
    while (TRUE) {
        switch (dwPopupId) {
        case 6:
            if (1) {
                DWORD dwGeneralState = pSprite->GetDerivedStats()->m_generalState;

                pSprite->m_baseStats = *m_pTempBaseStats;
                pSprite->m_derivedStats = *m_pTempDerivedStats;
                pSprite->m_nHPCONBonusTotalOld = field_866;

                LONG nHPCONBonusTotalOld = field_866;
                SHORT nMaxHitPoints = pSprite->m_derivedStats.m_nMaxHitPoints;
                WORD nMaxHitPointsBase = pSprite->m_baseStats.m_maxHitPointsBase;
                SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
                pSprite->field_562C = 1;
                pSprite->ProcessEffectList();
                pSprite->m_baseStats.m_hitPoints = nHitPoints;
                pSprite->m_derivedStats.m_nMaxHitPoints = nMaxHitPoints;
                pSprite->m_baseStats.m_maxHitPointsBase = nMaxHitPointsBase;
                pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;

                if ((dwGeneralState & STATE_DEAD) != 0) {
                    pSprite->m_baseStats.m_generalState |= STATE_DEAD;
                    pSprite->m_derivedStats.m_generalState |= STATE_DEAD;
                }
            }
            dwPopupId = 54;
            break;
        case 7:
            dwPopupId = 6;
            break;
        case 8:
            dwPopupId = 56;
            break;
        case 16:
            dwPopupId = 8;
            break;
        case 54:
            dwPopupId = 2;
            break;
        case 55:
            dwPopupId = 7;
            break;
        case 56:
            dwPopupId = 55;
            break;
        default:
            // NOTE: The original goes on with the popup id register still
            // holding the sprite pointer; every caller passes a known id.
            //
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7177
            UTIL_ASSERT(FALSE);
        }

        if (CheckSummonPopup(dwPopupId, pSprite)) {
            break;
        }

        if (dwPopupId == 2) {
            while (m_lPopupStack.GetTailPosition() != NULL) {
                DismissPopup(pSprite);
            }

            if (!g_pChitin->cNetwork.m_bIsHost
                && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 7200
                UTIL_ASSERT(pGame != NULL);

                LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);
                CMessage120* pMessage = new CMessage120(g_pChitin->cNetwork.m_idLocalPlayer,
                    FALSE,
                    nCharacterId,
                    nCharacterId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            } else if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
                g_pBaldurChitin->GetObjectGame()->GetMultiplayerSettings()->sub_518660(g_pChitin->cNetwork.m_idLocalPlayer, FALSE);
            }

            return FALSE;
        }
    }

    SummonPopup(dwPopupId, pSprite, FALSE);

    return TRUE;
}

// 0x5E1DF0
void CScreenCharacter::DismissPopup(CGameSprite* pSprite)
{
    if (m_cUIManager.m_pFocusedControl != NULL) {
        // NOTE: Uninline
        m_cUIManager.KillCapture();
        field_1B8 = FALSE;
    }

    CUIPanel* pPanel = m_lPopupStack.RemoveTail();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 7255
    UTIL_ASSERT(pPanel != NULL);

    // NOTE: Uninline.
    ShowPopupPanel(pPanel->m_nID, FALSE);

    CUIPanel* pMainPanel = m_cUIManager.GetPanel(2);
    pMainPanel->InvalidateRect(NULL);

    if (m_lPopupStack.GetTailPosition() != NULL) {
        if (pSprite != NULL) {
            CUIPanel* pPanel = m_lPopupStack.GetTail();

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7272
            UTIL_ASSERT(pPanel != NULL);

            // NOTE: Uninline.
            ShowPopupPanel(pPanel->m_nID, TRUE);

            // NOTE: Uninline.
            EnablePopupPanel(pPanel->m_nID, TRUE);

            UpdatePopupPanel(pPanel->m_nID, pSprite);
        }
    } else {
        EnableMainPanel(TRUE);
        UpdateMainPanel(FALSE);
    }
}

// 0x5E1F50
BOOL CScreenCharacter::IsDoneButtonClickable(CGameSprite* pSprite)
{
    CUIPanel* pPanel = GetTopPopup();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 7305
    UTIL_ASSERT(pPanel != NULL);

    CString sText;
    BOOL bResult;
    DWORD dwSpecialization;

    switch (pPanel->m_nID) {
    case 4:
    case 12:
    case 17:
    case 18:
    case 51:
    case 52:
    case 53:
    case 57:
        bResult = TRUE;
        break;
    case 6:
        dwSpecialization = 0;
        if (m_nClass == CAIOBJECTTYPE_C_WIZARD) {
            dwSpecialization = m_nSpecialization & SPECMASK_WIZARD;
        }
        if (m_nClass == CAIOBJECTTYPE_C_PALADIN) {
            dwSpecialization = m_nSpecialization & SPECMASK_PALADIN;
        }
        if (m_nClass == CAIOBJECTTYPE_C_MONK) {
            dwSpecialization = m_nSpecialization & SPECMASK_MONK;
        }
        if (m_nClass == CAIOBJECTTYPE_C_CLERIC) {
            dwSpecialization = m_nSpecialization & SPECMASK_CLERIC;
        }
        bResult = dwSpecialization != 0;
        break;
    case 7:
        bResult = m_nExtraAbilityPoints == 0;
        break;
    case 8:
        bResult = m_nExtraMageSpells == 0;
        break;
    case 11:
        bResult = m_nScriptIndex >= 0;
        break;
    case 13:
        if (1) {
            CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(6));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7346
            UTIL_ASSERT(pEdit != NULL);

            sText = pEdit->GetText();
            sText.TrimLeft();
            sText.TrimRight();

            bResult = strcmp(sText, "") != 0;
        }
        break;
    case 16:
        bResult = pSprite->GetBaseStats()->m_favoredEnemies[m_nHatedRaceIndex] != CAIObjectType::R_NO_RACE;
        break;
    case 19:
        if (m_nPortraitSmallIndex < 0 || m_nPortraitLargeIndex < 0) {
            bResult = FALSE;
        } else {
            bResult = TRUE;
        }
        break;
    case 20:
        bResult = m_nCustomSoundSetIndex >= 0;
        break;
    case 54:
        bResult = m_nClass != 0;
        break;
    case 55:
        bResult = m_nExtraSkillPoints < 2;
        break;
    case 56:
        bResult = m_nExtraFeats == 0;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 7399
        UTIL_ASSERT(FALSE);

        bResult = FALSE;
        break;
    }

    return bResult;
}

// 0x5E2210
void CScreenCharacter::OnDoneButtonClick()
{
    WIN32_FIND_DATAA fileInfo;
    CResRef cResRef;
    CString sScript;
    CString sFileName;
    CString sName;
    CString sForbidden(".?:<>|*/\\\"");
    CString sSoundSet;
    CString sSearch;

    CUIPanel* pPanel = GetTopPopup();
    if (pPanel != NULL) {
        if ((pPanel->m_nID >= 9 && pPanel->m_nID <= 10) || pPanel->m_nID == 50) {
            OnErrorButtonClick(0);
            return;
        }
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 7462
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        renderLock.Unlock();
        return;
    }

    if (IsDoneButtonClickable(pSprite)) {
        pPanel = GetTopPopup();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 7487
        UTIL_ASSERT(pPanel != NULL);

        POSITION pos;
        LONG nExportId;
        INT nLevels;
        CDerivedStats DStats;
        CAIObjectType typeAI;

        switch (pPanel->m_nID) {
        case 4:
        case 53:
            DismissPopup(pSprite);
            break;
        case 6:
            pSprite->GetBaseStats()->m_specialization = field_77C | m_nSpecialization;
            DismissPopup(pSprite);
            SummonNextPopup(pPanel->m_nID, pSprite);
            break;
        case 7:
            DStats = *pSprite->GetDerivedStats();
            nLevels = pSprite->GetDerivedStats()->m_nLevel - m_pTempDerivedStats->m_nLevel;
            *pSprite->GetDerivedStats() = *m_pTempDerivedStats;

            m_nLevelUpFeats = 0;
            m_nLevelUpSkillPoints = pSprite->GetBaseStats()->field_2FF;

            while (nLevels > 0) {
                pSprite->GetDerivedStats()->SetClassLevel(m_nClass,
                    pSprite->GetDerivedStats()->GetClassLevel(m_nClass) + 1);
                m_nLevelUpFeats += pSprite->GetExtraFeats(m_nClass);
                m_nLevelUpSkillPoints += pSprite->GetExtraSkillPoints(m_nClass);
                nLevels--;
            }

            *pSprite->GetDerivedStats() = DStats;

            DismissPopup(pSprite);
            SummonNextPopup(pPanel->m_nID, pSprite);
            break;
        case 8:
            DismissPopup(pSprite);
            if (CheckSummonPopup(pPanel->m_nID, pSprite)) {
                SummonPopup(pPanel->m_nID, pSprite, TRUE);
            } else {
                SummonNextPopup(pPanel->m_nID, pSprite);
            }
            break;
        case 11:
            if (1) {
                pos = m_pScripts->FindIndex(m_nScriptIndex);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 7530
                UTIL_ASSERT(pos != NULL);

                sScript = m_pScripts->GetAt(pos);

                CAIScript* pScript = new CAIScript();
                pScript->m_cResRef = CResRef(sScript);
                pScript->Read(CResRef(sScript), TRUE);

                pSprite->SetScript(3, pScript);

                // NOTE: The original then sends the other players a message
                // (vtable 0x84D398) with the script's resref; it is a
                // BROADCAST_OTHERS message that single player deletes unsent
                // (docs/adr/0002-single-player-only.md), and its class is not
                // ported.

                DismissPopup(pSprite);

                if (m_pScripts != NULL) {
                    delete m_pScripts;
                    m_pScripts = NULL;
                }
            }
            break;
        case 12:
        case 57:
            DismissPopup(pSprite);
            break;
        case 13:
            if (1) {
                CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(6));

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 7577
                UTIL_ASSERT(pEdit != NULL);

                sFileName = pEdit->GetText();
                sFileName.TrimLeft();
                sFileName.TrimRight();

                for (INT nIndex = 0; nIndex < sFileName.GetLength(); nIndex++) {
                    if (sForbidden.Find(sFileName[nIndex]) >= 0) {
                        sFileName.SetAt(nIndex, '_');
                    }
                }

                if (_stricmp(sFileName, "aux") == 0
                    || _stricmp(sFileName, "con") == 0
                    || _stricmp(sFileName, "prn") == 0) {
                    sFileName += "_";
                }

                if (IsNameOnExportList(sFileName)) {
                    field_176E = sFileName;
                    m_nErrorState = 7;
                    m_dwErrorTextId = 26717;
                    m_strErrorButtonText[0] = 15306;
                    m_strErrorButtonText[1] = 13727;
                    SummonPopup(10, pSprite, TRUE);
                } else if (pSprite->HaveUnexportableItems()) {
                    field_176E = sFileName;
                    m_nErrorState = 6;
                    m_dwErrorTextId = 26586;
                    m_strErrorButtonText[0] = 13912;
                    m_strErrorButtonText[1] = 13913;
                    SummonPopup(10, pSprite, TRUE);
                } else {
                    // NOTE: Uninline.
                    nExportId = pGame->GetCharacterId(m_nSelectedCharacter);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 7625
                    UTIL_ASSERT(nExportId != CGameObjectArray::INVALID_INDEX);

                    pGame->CharacterExport(nExportId, sFileName);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 7629
                    UTIL_ASSERT(m_pCharacters != NULL);

                    if (m_pCharacters != NULL) {
                        delete m_pCharacters;
                    }
                    m_pCharacters = NULL;

                    DismissPopup(pSprite);
                }
            }
            break;
        case 16:
            DismissPopup(pSprite);
            if (CheckSummonPopup(pPanel->m_nID, pSprite)) {
                SummonPopup(pPanel->m_nID, pSprite, TRUE);
            } else {
                SummonNextPopup(pPanel->m_nID, pSprite);
            }
            break;
        case 17:
            m_cResPortraitSmallTemp.GetResRef(pSprite->GetBaseStats()->m_portraitSmall);
            m_cResPortraitLargeTemp.GetResRef(pSprite->GetBaseStats()->m_portraitLarge);
            DismissPopup(pSprite);
            g_pBaldurChitin->GetBaldurMessage()->BroadcastUpdateCharacterSlot(FALSE,
                pGame->GetCharacterSlotFromId(pSprite->GetId()),
                FALSE);
            break;
        case 18:
        case 52:
            DismissPopup(pSprite);
            break;
        case 19:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7640
            UTIL_ASSERT((m_nPortraitSmallIndex >= 0) && (m_nPortraitLargeIndex >= 0));

            pos = m_pPortraits->FindIndex(m_nPortraitSmallIndex);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7644
            UTIL_ASSERT(pos != NULL);

            m_cResPortraitSmallTemp = CResRef(m_pPortraits->GetAt(pos));

            pos = m_pPortraits->FindIndex(m_nPortraitLargeIndex);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7650
            UTIL_ASSERT(pos != NULL);

            m_cResPortraitLargeTemp = CResRef(m_pPortraits->GetAt(pos));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7655
            UTIL_ASSERT(m_pPortraits != NULL);

            if (m_pPortraits != NULL) {
                delete m_pPortraits;
            }
            m_pPortraits = NULL;

            DismissPopup(pSprite);
            DismissPopup(pSprite);
            break;
        case 20:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7676
            UTIL_ASSERT(m_nCustomSoundSetIndex >= 0);

            cResRef = "";

            if (m_nCustomSoundSetIndex != -1) {
                pos = m_pSounds->FindIndex(m_nCustomSoundSetIndex);
                if (pos != NULL) {
                    sSoundSet = m_pSounds->GetAt(pos);
                }
            }

            // NOTE: The original copies min(nAllocLength, 32) bytes of the
            // buffer and writes the terminator at nAllocLength; the string
            // stored is the same.
            memcpy(pSprite->field_725A,
                sSoundSet.GetBuffer(sSoundSet.GetLength()),
                min(sSoundSet.GetLength(), 32));
            if (sSoundSet.GetLength() < 32) {
                pSprite->field_725A[sSoundSet.GetLength()] = '\0';
            }

            g_pBaldurChitin->cDimm.AddToDirectoryList(pGame->GetDirSounds() + sSoundSet + '\\', TRUE);

            sSearch = pGame->GetDirSounds() + '\\' + sSoundSet + "\\*.wav";

            // NOTE: The original never closes the search handle.
            if (FindFirstFileA(sSearch, &fileInfo) != INVALID_HANDLE_VALUE) {
                sName = fileInfo.cFileName;
                if ((fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    sName.MakeUpper();
                    sName = sName.SpanExcluding(".");
                    sName = sName.Left(sName.GetLength() - 2);
                    pSprite->m_secondarySounds = CResRef(sName);
                }
            }

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7716
            UTIL_ASSERT(m_pSounds != NULL);

            if (m_pSounds != NULL) {
                delete m_pSounds;
            }
            m_pSounds = NULL;

            DismissPopup(pSprite);
            break;
        case 51:
            if (1) {
                CUIControlEditMultiLine* pEdit = static_cast<CUIControlEditMultiLine*>(m_cUIManager.GetPanel(51)->GetControl(4));

                pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                g_pBaldurChitin->GetObjectGame()->ChangeBiography(static_cast<BYTE>(g_pBaldurChitin->GetObjectGame()->GetFixedOrderCharacterPortraitNum(pSprite->GetId())),
                    pEdit->GetText());

                DismissPopup(pSprite);

                renderLock.Unlock();
            }
            return;
        case 54:
            field_1840 = pSprite->GetId();

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7795
            UTIL_ASSERT(m_nClass != 0);

            LevelUpStartAbilities(pSprite);

            typeAI.Set(pSprite->m_startTypeAI);
            typeAI.m_nClassMask = 0;

            for (INT iClassType = 1; iClassType <= CAIOBJECT_CLASS_MAX; iClassType++) {
                if (pSprite->GetDerivedStats()->GetClassLevel(iClassType) != 0) {
                    typeAI.AddClass(iClassType);
                }
            }

            pSprite->SetAIType(typeAI, TRUE, TRUE);

            DStats = *pSprite->GetDerivedStats();
            nLevels = pSprite->GetDerivedStats()->m_nLevel - m_pTempDerivedStats->m_nLevel;
            *pSprite->GetDerivedStats() = *m_pTempDerivedStats;

            m_nLevelUpFeats = 0;
            m_nLevelUpSkillPoints = pSprite->GetBaseStats()->field_2FF;

            while (nLevels > 0) {
                pSprite->GetDerivedStats()->SetClassLevel(m_nClass,
                    pSprite->GetDerivedStats()->GetClassLevel(m_nClass) + 1);
                m_nLevelUpFeats += pSprite->GetExtraFeats(m_nClass);
                m_nLevelUpSkillPoints += pSprite->GetExtraSkillPoints(m_nClass);
                nLevels--;
            }

            *pSprite->GetDerivedStats() = DStats;

            DismissPopup(pSprite);
            SummonNextPopup(pPanel->m_nID, pSprite);
            break;
        case 55:
            DismissPopup(pSprite);
            switch (m_nClass) {
            case CAIOBJECTTYPE_C_BARD:
            case CAIOBJECTTYPE_C_SORCERER:
                m_nSpellLevel = 0;
                SummonNextPopup(pPanel->m_nID, pSprite);
                break;
            case CAIOBJECTTYPE_C_BARBARIAN:
            case CAIOBJECTTYPE_C_CLERIC:
            case CAIOBJECTTYPE_C_DRUID:
            case CAIOBJECTTYPE_C_FIGHTER:
            case CAIOBJECTTYPE_C_MONK:
            case CAIOBJECTTYPE_C_PALADIN:
            case CAIOBJECTTYPE_C_RANGER:
            case CAIOBJECTTYPE_C_ROGUE:
            case CAIOBJECTTYPE_C_WIZARD:
                SummonNextPopup(pPanel->m_nID, pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 7864
                UTIL_ASSERT(FALSE);
            }
            break;
        case 56:
            DismissPopup(pSprite);
            switch (m_nClass) {
            case CAIOBJECTTYPE_C_BARD:
            case CAIOBJECTTYPE_C_SORCERER:
                m_nSpellLevel = 0;
                SummonNextPopup(pPanel->m_nID, pSprite);
                break;
            case CAIOBJECTTYPE_C_BARBARIAN:
            case CAIOBJECTTYPE_C_CLERIC:
            case CAIOBJECTTYPE_C_DRUID:
            case CAIOBJECTTYPE_C_FIGHTER:
            case CAIOBJECTTYPE_C_MONK:
            case CAIOBJECTTYPE_C_PALADIN:
            case CAIOBJECTTYPE_C_RANGER:
            case CAIOBJECTTYPE_C_ROGUE:
            case CAIOBJECTTYPE_C_WIZARD:
                SummonNextPopup(pPanel->m_nID, pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 7890
                UTIL_ASSERT(FALSE);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 7896
            UTIL_ASSERT(FALSE);
        }
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    renderLock.Unlock();

    m_cUIManager.InvalidateRect(NULL);
}

// 0x5E3B80
BOOL CScreenCharacter::OnCancelButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 7934
    UTIL_ASSERT(pGame != NULL);

    CUIPanel* pPanel = GetTopPopup();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 7936
    UTIL_ASSERT(pPanel != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        renderLock.Unlock();
        return FALSE;
    }

    CCreatureFileHeader& BStats = *pSprite->GetBaseStats();
    INT nIndex;
    DWORD dwGeneralState;

    switch (pPanel->m_nID) {
    case 4:
    case 9:
    case 10:
    case 12:
    case 50:
    case 51:
    case 52:
    case 53:
    case 57:
        DismissPopup(pSprite);
        break;
    case 6:
        BStats.m_specialization = field_77C;
        m_nSpecialization = field_77C;
        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    case 7:
        BStats.m_STRBase = m_pTempBaseStats->m_STRBase;
        BStats.m_DEXBase = m_pTempBaseStats->m_DEXBase;
        BStats.m_CONBase = m_pTempBaseStats->m_CONBase;
        BStats.m_INTBase = m_pTempBaseStats->m_INTBase;
        BStats.m_WISBase = m_pTempBaseStats->m_WISBase;
        BStats.m_CHRBase = m_pTempBaseStats->m_CHRBase;
        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    case 8:
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            if (m_spells.m_lists[nLevel].m_List.size() != 0) {
                m_spells.m_lists[nLevel].m_List.clear();
                m_spells.m_lists[nLevel].field_14 = 0;
                m_spells.m_lists[nLevel].field_18 = 0;
            }
        }
        m_spells.m_nHighestLevel = 0;

        pSprite->m_spells = m_pTempDerivedStats->m_spells;
        pSprite->m_domainSpells = m_pTempDerivedStats->m_domainSpells;

        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    case 11:
        DismissPopup(pSprite);

        if (m_pScripts != NULL) {
            delete m_pScripts;
            m_pScripts = NULL;
        }
        break;
    case 13:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 8031
        UTIL_ASSERT(m_pCharacters != NULL);

        if (m_pCharacters != NULL) {
            delete m_pCharacters;
        }
        m_pCharacters = NULL;

        DismissPopup(pSprite);
        break;
    case 16:
        if (m_pTempBaseStats != NULL) {
            for (nIndex = 0; nIndex < MAX_HATED_RACES; nIndex++) {
                BStats.m_favoredEnemies[nIndex] = m_pTempBaseStats->m_favoredEnemies[nIndex];
            }
        }
        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    case 17:
        if (1) {
            m_cResPortraitSmall.GetResRef(BStats.m_portraitSmall);
            m_cResPortraitLarge.GetResRef(BStats.m_portraitLarge);
            m_cResPortraitSmallTemp = m_cResPortraitSmall;
            m_cResPortraitLargeTemp = m_cResPortraitLarge;

            pSprite->m_secondarySounds = m_cResSoundSet;

            // NOTE: The original copies min(nAllocLength, 32) bytes of the
            // buffer and writes the terminator at nAllocLength; the string
            // stored is the same.
            memcpy(pSprite->field_725A,
                field_856.GetBuffer(field_856.GetLength()),
                min(field_856.GetLength(), 32));
            if (field_856.GetLength() < 32) {
                pSprite->field_725A[field_856.GetLength()] = '\0';
            }

            CAIScript* pScript = new CAIScript();
            pScript->m_cResRef = CResRef(field_832);
            pScript->Read(CResRef(field_832), TRUE);
            pSprite->SetScript(3, pScript);

            DismissPopup(pSprite);
        }
        break;
    case 18:
        m_cResPortraitSmall.GetResRef(BStats.m_portraitSmall);
        m_cResPortraitLarge.GetResRef(BStats.m_portraitLarge);
        m_cResPortraitSmallTemp = m_cResPortraitSmall;
        m_cResPortraitLargeTemp = m_cResPortraitLarge;
        DismissPopup(pSprite);
        break;
    case 19:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 8048
        UTIL_ASSERT(m_pPortraits != NULL);

        if (m_pPortraits != NULL) {
            delete m_pPortraits;
        }
        m_pPortraits = NULL;

        DismissPopup(pSprite);
        break;
    case 20:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 8070
        UTIL_ASSERT(m_pSounds != NULL);

        if (m_pSounds != NULL) {
            delete m_pSounds;
        }
        m_pSounds = NULL;

        pSprite->m_secondarySounds = m_cResSoundSet;

        // NOTE: See case 17.
        memcpy(pSprite->field_725A,
            field_856.GetBuffer(field_856.GetLength()),
            min(field_856.GetLength(), 32));
        if (field_856.GetLength() < 32) {
            pSprite->field_725A[field_856.GetLength()] = '\0';
        }

        DismissPopup(pSprite);
        break;
    case 54:
        g_pBaldurChitin->m_pEngineCharacter->m_nSpecialization = field_77C;

        pSprite->SetAIType(m_typeAI, TRUE, TRUE);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 8094
        UTIL_ASSERT((m_pTempBaseStats != NULL) && (m_pTempDerivedStats != NULL));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 8095
        UTIL_ASSERT(m_pBeforeLevelUpDerivedStats != NULL);

        dwGeneralState = pSprite->GetDerivedStats()->m_generalState;

        BStats = *m_pTempBaseStats;
        *pSprite->GetDerivedStats() = *m_pTempDerivedStats;
        pSprite->m_nHPCONBonusTotalOld = field_866;
        field_866 = 0;

        if (1) {
            SHORT nMaxHitPoints = pSprite->GetDerivedStats()->m_nMaxHitPoints;
            SHORT nHitPoints = BStats.m_hitPoints;
            WORD nMaxHitPointsBase = BStats.m_maxHitPointsBase;
            LONG nHPCONBonusTotalOld = pSprite->m_nHPCONBonusTotalOld;
            pSprite->field_562C = 1;
            pSprite->ProcessEffectList();
            BStats.m_hitPoints = nHitPoints;
            pSprite->GetDerivedStats()->m_nMaxHitPoints = nMaxHitPoints;
            BStats.m_maxHitPointsBase = nMaxHitPointsBase;
            pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;
        }

        if ((dwGeneralState & STATE_DEAD) != 0) {
            BStats.m_generalState |= STATE_DEAD;
            pSprite->GetDerivedStats()->m_generalState |= STATE_DEAD;
        }

        delete m_pTempBaseStats;
        m_pTempBaseStats = NULL;

        if (m_pTempDerivedStats != NULL) {
            delete m_pTempDerivedStats;
        }
        m_pTempDerivedStats = NULL;

        if (m_pBeforeLevelUpDerivedStats != NULL) {
            delete m_pBeforeLevelUpDerivedStats;
        }
        m_pBeforeLevelUpDerivedStats = NULL;

        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    case 55:
        for (m_nTopSkill = 0; static_cast<USHORT>(m_nTopSkill) < SKILL_COUNT; m_nTopSkill++) {
            pSprite->SetSkillValue(static_cast<USHORT>(m_nTopSkill), m_storedSkills[static_cast<USHORT>(m_nTopSkill)]);
        }
        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    case 56:
        for (nIndex = 0; nIndex < FEAT_COUNT; nIndex++) {
            // NOTE: The feats were stored by feat number (ResetFeatsPanel)
            // and come back by feat id.
            pSprite->SetFeatValue(g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetFeatId(nIndex),
                m_storedFeats[nIndex]);
        }
        DismissPopup(pSprite);
        SummonPreviousPopup(pPanel->m_nID, pSprite);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 8163
        UTIL_ASSERT(FALSE);
    }

    pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    renderLock.Unlock();

    m_cUIManager.InvalidateRect(NULL);

    return TRUE;
}

// 0x5E4560
void CScreenCharacter::sub_5E4560()
{
    // TODO: Incomplete.
}

// 0x5E46C0
void CScreenCharacter::OnInformationButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8345
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(57, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E47E0
void CScreenCharacter::OnBiographyButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8403
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(12, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E4900
void CScreenCharacter::OnCustomizeBiographyButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8460
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(51, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E4A20
void CScreenCharacter::sub_5E4A20()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8524
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(52, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E4B30
void CScreenCharacter::sub_5E4B30()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8581
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(53, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E4C40
void CScreenCharacter::OnScriptButtonClick()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8637
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(11, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E4D50
void CScreenCharacter::OnExportButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8728
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(GetSelectedCharacter());

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (m_bMultiPlayerModifyable
            && (pSprite->GetBaseStats()->m_flags & 0x800) != 0
            && !pSprite->GetDerivedStats()->m_bPolymorphed) {
            SummonPopup(13, pSprite, 1);
        }

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E4E80
BOOL CScreenCharacter::CanLevelUp(CGameSprite* pSprite)
{
    BOOL bResult = FALSE;

    if (m_bMultiPlayerModifyable
        && (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == pSprite->m_remotePlayerID)) {
        CDerivedStats DStats;
        DStats = *pSprite->GetDerivedStats();

        g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetNextLevel(pSprite->m_startTypeAI.m_nClass,
            DStats,
            pSprite);

        bResult = (pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) == 0
            && DStats.m_nLevel != pSprite->GetDerivedStats()->m_nLevel;
    }

    return bResult;
}

// 0x5E5330
void CScreenCharacter::OnLevelUpButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8835
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(GetSelectedCharacter());

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (CanLevelUp(pSprite)) {
            CMessageStopActions* pStopActions = new CMessageStopActions(pSprite->GetId(),
                pSprite->GetId(),
                pSprite);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pStopActions, FALSE);

            if (!g_pChitin->cNetwork.m_bIsHost
                && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
                // NOTE: Uninline.
                LONG nId = pGame->GetCharacterId(m_nSelectedCharacter);

                CMessage120* pMessage = new CMessage120(g_pChitin->cNetwork.m_idLocalPlayer,
                    TRUE,
                    nId,
                    nId);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            } else if (g_pChitin->cNetwork.m_bIsHost == TRUE) {
                pGame->GetMultiplayerSettings()->sub_518660(g_pChitin->cNetwork.m_idLocalPlayer, TRUE);
            }

            pSprite->field_562C = 1;
            pSprite->ProcessEffectList();

            m_typeAI.Set(pSprite->m_startTypeAI);

            field_77C = pSprite->GetBaseStats()->m_specialization;
            m_nSpecialization = field_77C;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 8893
            UTIL_ASSERT((m_pTempBaseStats == NULL) && (m_pTempDerivedStats == NULL));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 8894
            UTIL_ASSERT(m_pBeforeLevelUpDerivedStats == NULL);

            m_pTempBaseStats = new CCreatureFileHeader();
            memset(m_pTempBaseStats, 0, sizeof(CCreatureFileHeader));
            *m_pTempBaseStats = *pSprite->GetBaseStats();

            m_pTempDerivedStats = new CDerivedStats();
            *m_pTempDerivedStats = *pSprite->GetDerivedStats();

            m_pBeforeLevelUpDerivedStats = new CDerivedStats();
            *m_pBeforeLevelUpDerivedStats = *pSprite->GetDerivedStats();

            field_866 = pSprite->m_nHPCONBonusTotalOld;

            SummonNextPopup(2, pSprite);
        }

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E56F0
void CScreenCharacter::LevelUp(CGameSprite* pSprite)
{
    CInfGame* pGame;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8936
    UTIL_ASSERT(m_pBeforeLevelUpDerivedStats != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 8937
    UTIL_ASSERT((m_pTempBaseStats != NULL) && (m_pTempDerivedStats != NULL));

    pSprite->sub_71E240(*m_pTempBaseStats, *m_pTempDerivedStats);
    pSprite->sub_71E750(*m_pTempBaseStats, *m_pTempDerivedStats);

    pSprite->GetBaseStats()->field_2FF = static_cast<unsigned char>(m_nExtraSkillPoints);

    pSprite->sub_71E760(*m_pTempDerivedStats, 1);

    BYTE nClass;
    nClass = CAIOBJECTTYPE_C_CLERIC;
    pSprite->AddKnownDivineSpells(nClass);
    nClass = CAIOBJECTTYPE_C_DRUID;
    pSprite->AddKnownDivineSpells(nClass);
    nClass = CAIOBJECTTYPE_C_PALADIN;
    pSprite->AddKnownDivineSpells(nClass);
    nClass = CAIOBJECTTYPE_C_RANGER;
    pSprite->AddKnownDivineSpells(nClass);

    if (m_nClass == CAIOBJECTTYPE_C_SORCERER || m_nClass == CAIOBJECTTYPE_C_BARD) {
        for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

            INT nNewBonus = 0;
            INT nNewMax = ruleTables.GetMaxKnownSpells(m_nClass,
                pSprite->GetAIType(),
                *pSprite->GetDerivedStats(),
                pSprite->GetBaseStats()->m_specialization,
                nLevel + 1,
                nNewBonus);
            nNewMax += nNewBonus;

            INT nOldBonus = 0;
            INT nOldMax = ruleTables.GetMaxKnownSpells(m_nClass,
                pSprite->GetAIType(),
                *m_pTempDerivedStats,
                pSprite->GetBaseStats()->m_specialization,
                nLevel + 1,
                nOldBonus);
            nOldMax += nOldBonus;

            CGameSpriteSpellList* pSpells = pSprite->GetSpellsAtLevel(m_nClass, nLevel);
            if (pSpells->m_List.size() != 0) {
                pSprite->GetSpellsAtLevel(m_nClass, nLevel)->field_14 = nNewMax;

                for (UINT nIndex = 0; nIndex < pSprite->GetSpellsAtLevel(m_nClass, nLevel)->m_List.size(); nIndex++) {
                    pSpells = pSprite->GetSpellsAtLevel(m_nClass, nLevel);

                    // __FILE__: .\Include\FileFormat.h
                    // __LINE__: 2437
                    UTIL_ASSERT(pSpells->m_List.size() > 0);

                    // __FILE__: .\Include\FileFormat.h
                    // __LINE__: 2438
                    UTIL_ASSERT(nIndex < pSpells->m_List.size());

                    UINT nID = pSpells->m_List[nIndex].m_nID;
                    UINT nDelta = nNewMax - nOldMax;
                    UINT nZero1 = 0;
                    UINT nZero2 = 0;
                    pSprite->GetSpells(m_nClass)->Add(nID, nLevel, nDelta, nZero1, nZero2);

                    // NOTE: The original then refreshes the quick spell
                    // buttons of this spell with CGameSprite::CheckQuickLists
                    // (0x71ABA0), which is not ported (CGameSprite.cpp, another
                    // stream's file).
                }

                UINT nDelta = nNewMax - nOldMax;
                pSprite->GetSpells(m_nClass)->sub_726150(nLevel, nDelta, FALSE);
            }
        }
    }

    if (pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_BARBARIAN) >= 15) {
        UINT nRageId = 0;
        UINT nGreaterRageId = 0;
        pGame = g_pBaldurChitin->GetObjectGame();
        if (pGame->m_innateSpells.Find(CGameSprite::SPIN236, nRageId) == TRUE
            && pGame->m_innateSpells.Find(CGameSprite::SPIN260, nGreaterRageId) == TRUE) {
            UINT nIndex = 0;
            if (pSprite->m_innateSpells.Find(nRageId, nIndex) == TRUE) {
                // __FILE__: .\Include\FileFormat.h
                // __LINE__: 2437
                UTIL_ASSERT(pSprite->m_innateSpells.m_List.size() > 0);

                // __FILE__: .\Include\FileFormat.h
                // __LINE__: 2438
                UTIL_ASSERT(nIndex < pSprite->m_innateSpells.m_List.size());

                CGameSpriteSpellListEntry rage = pSprite->m_innateSpells.m_List[nIndex];

                unsigned int nZero = 0;
                pSprite->AddInnateSpell(CGameSprite::SPIN260, rage.field_4, nZero, rage.field_C);
                pSprite->sub_725210(CGameSprite::SPIN260, rage.field_8, FALSE);
                pSprite->sub_725270(CGameSprite::SPIN236, 0, TRUE);
                pSprite->RemoveInnateSpell(CGameSprite::SPIN236, 1, 0, 0);

                UINT nGreaterIndex = 0;
                if (pSprite->m_innateSpells.Find(nGreaterRageId, nGreaterIndex) == TRUE) {
                    // __FILE__: .\Include\FileFormat.h
                    // __LINE__: 2437
                    UTIL_ASSERT(pSprite->m_innateSpells.m_List.size() > 0);

                    // __FILE__: .\Include\FileFormat.h
                    // __LINE__: 2438
                    UTIL_ASSERT(nGreaterIndex < pSprite->m_innateSpells.m_List.size());

                    CGameSpriteSpellListEntry greaterRage = pSprite->m_innateSpells.m_List[nGreaterIndex];

                    CButtonData buttonData;
                    buttonData.m_launcherIcon = "";
                    buttonData.m_icon = buttonData.m_launcherIcon;
                    buttonData.m_launcherName = -1;
                    buttonData.m_name = -1;
                    buttonData.m_count = 0;
                    buttonData.m_bDisabled = FALSE;
                    buttonData.m_bDisplayCount = TRUE;

                    CSpell* curSpell = new CSpell(CGameSprite::SPIN260);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 9046
                    UTIL_ASSERT(curSpell != NULL);

                    if (curSpell->Demand()) {
                        if (curSpell->pRes != NULL) {
                            SHORT nCasterLevel = pSprite->GetCasterLevel(curSpell, 0, 0);
                            if (nCasterLevel <= 1) {
                                nCasterLevel = 1;
                            }

                            SPELL_ABILITY* pAbility = NULL;
                            for (INT nAbility = 0; nAbility < curSpell->GetAbilityCount(); nAbility++) {
                                if (curSpell->GetAbility(nAbility)->minCasterLevel > nCasterLevel) {
                                    break;
                                }
                                pAbility = curSpell->GetAbility(nAbility);
                            }

                            if (pAbility != NULL && pAbility->quickSlotType == 4) {
                                buttonData.m_icon = CResRef(pAbility->quickSlotIcon);
                                buttonData.m_name = curSpell->GetGenericName();
                                buttonData.m_abilityId.m_itemType = 1;
                                buttonData.m_abilityId.m_res = CGameSprite::SPIN260;
                                buttonData.m_abilityId.m_targetType = pAbility->actionType;
                                buttonData.m_abilityId.field_10 = curSpell->GetGenericName();
                                buttonData.m_bDisabled = static_cast<BOOLEAN>(pSprite->GetActiveStats()->m_disabledSpellTypes[2]);
                                buttonData.m_count = static_cast<SHORT>(greaterRage.field_C);
                                if (static_cast<SHORT>(greaterRage.field_C) <= 0) {
                                    buttonData.m_bDisabled = TRUE;
                                }
                            }
                        }

                        curSpell->Release();
                    }

                    delete curSpell;

                    for (BYTE buttonNum = 0; buttonNum < 9; buttonNum++) {
                        CButtonData slot;

                        // __FILE__: .\Include\ObjCreature.h
                        // __LINE__: 2024
                        UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ABILITIES22);

                        slot = pSprite->m_quickInnates[buttonNum];
                        if (slot.m_abilityId.m_res == CGameSprite::SPIN236) {
                            // __FILE__: .\Include\ObjCreature.h
                            // __LINE__: 2034
                            UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ABILITIES22);

                            pSprite->m_quickInnates[buttonNum] = buttonData;
                        }
                    }
                }
            }
        }
    }

    if (m_nClass == CAIOBJECTTYPE_C_DRUID) {
        const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
        INT nNew = ruleTables.GetMaxDruidShapeshifts(*pSprite->GetBaseStats(),
            pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_DRUID));
        INT nOld = ruleTables.GetMaxDruidShapeshifts(*m_pTempBaseStats,
            m_pTempDerivedStats->GetClassLevel(m_nClass));
        INT nDelta = nNew - nOld;
        if (nDelta > 0) {
            unsigned int nCount = nDelta;
            pSprite->sub_724C40(nCount);
            pSprite->m_shapeshifts.sub_725D30(nCount, FALSE);
        }
    }

    if (m_nClass == CAIOBJECTTYPE_C_CLERIC
        && (m_nSpecialization & SPECMASK_CLERIC) == SPECMASK_CLERIC_TALOS
        && m_pBeforeLevelUpDerivedStats->GetClassLevel(CAIObjectType::C_CLERIC) == 0) {
        pSprite->GetBaseStats()->m_resistElectricityBase += 5;
    }

    delete m_pTempBaseStats;
    m_pTempBaseStats = NULL;

    if (m_pTempDerivedStats != NULL) {
        delete m_pTempDerivedStats;
    }
    m_pTempDerivedStats = NULL;

    field_866 = 0;
    field_86A = 0;

    if (m_pBeforeLevelUpDerivedStats != NULL) {
        delete m_pBeforeLevelUpDerivedStats;
    }
    m_pBeforeLevelUpDerivedStats = NULL;

    CheckDropEquipment();

    pSprite->field_562C = 1;
    pSprite->ProcessEffectList();

    if (m_nClass == CAIOBJECTTYPE_C_CLERIC) {
        pGame = g_pBaldurChitin->GetObjectGame();

        UINT nClericLevel = pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_CLERIC);
        nClass = CAIOBJECTTYPE_C_CLERIC;
        UINT nMaxLevel = pGame->GetRuleTables().GetMaxSpellLevel(nClass, nClericLevel);

        DWORD dwDomain = m_nSpecialization & SPECMASK_CLERIC;
        CGroupedSpellList* pDomainSpells = &(pGame->m_spellsByDomain[pGame->GetSpellcasterDomainIndex(dwDomain)]);

        for (UINT nLevel = 0; nLevel < nMaxLevel; nLevel++) {
            UINT nIndex = 0;
            while (TRUE) {
                // __FILE__: .\Include\InfGame.h
                // __LINE__: 772
                UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

                if (nIndex >= pDomainSpells->m_lists[nLevel].m_nCount) {
                    break;
                }

                UINT nID = pDomainSpells->m_lists[nLevel].Get(nIndex);
                const CResRef& resRef = pGame->m_spells.Get(nID);

                CSpell cSpell;
                cSpell.SetResRef(resRef, TRUE, TRUE);

                DWORD dwNotUsableBy = cSpell.GetNotUsableBy();

                BOOL bAdd;
                if (cSpell.pRes == NULL || (dwNotUsableBy & 0x40000000) != 0) {
                    bAdd = TRUE;
                } else if ((dwNotUsableBy & 0x4) != 0 && IcewindMisc::IsGood(pSprite)) {
                    bAdd = FALSE;
                } else if ((dwNotUsableBy & 0x2) != 0 && IcewindMisc::IsEvil(pSprite)) {
                    bAdd = FALSE;
                } else if ((dwNotUsableBy & 0x8) != 0
                    && !IcewindMisc::IsGood(pSprite)
                    && !IcewindMisc::IsEvil(pSprite)) {
                    bAdd = FALSE;
                } else if ((dwNotUsableBy & 0x10) != 0 && IcewindMisc::IsLawful(pSprite)) {
                    bAdd = FALSE;
                } else if ((dwNotUsableBy & 0x1) != 0 && IcewindMisc::IsChaotic(pSprite)) {
                    bAdd = FALSE;
                } else if ((dwNotUsableBy & 0x20) != 0
                    && !IcewindMisc::IsLawful(pSprite)
                    && !IcewindMisc::IsChaotic(pSprite)) {
                    bAdd = FALSE;
                } else {
                    bAdd = TRUE;
                }

                if (bAdd) {
                    unsigned int nZero1 = 0;
                    unsigned int nZero2 = 0;
                    unsigned int nZero3 = 0;
                    pSprite->AddDomainSpell(nLevel, resRef, nZero1, nZero2, nZero3);
                }

                nIndex++;
            }
        }
    }

    if (m_nClass != CAIOBJECTTYPE_C_PALADIN
        && pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_PALADIN) > 0
        && (pSprite->GetBaseStats()->field_2FB & 0x4) == 0) {
        switch (pSprite->GetSpecialization() & SPECMASK_PALADIN) {
        case SPECMASK_PALADIN_ILMATER:
            if (m_nClass != CAIOBJECTTYPE_C_CLERIC
                || (pSprite->GetSpecialization() & SPECMASK_CLERIC) != SPECMASK_CLERIC_ILMATER) {
                pSprite->GetBaseStats()->field_2FB |= 0x4;
            }
            break;
        case SPECMASK_PALADIN_HELM:
            if (m_nClass != CAIOBJECTTYPE_C_FIGHTER) {
                pSprite->GetBaseStats()->field_2FB |= 0x4;
            }
            break;
        case SPECMASK_PALADIN_MYSTRA:
            if (m_nClass != CAIOBJECTTYPE_C_WIZARD) {
                pSprite->GetBaseStats()->field_2FB |= 0x4;
            }
            break;
        }
    }

    if (m_nClass != CAIOBJECTTYPE_C_MONK
        && pSprite->GetDerivedStats()->GetClassLevel(CAIObjectType::C_MONK) > 0
        && (pSprite->GetBaseStats()->field_2FB & 0x8) == 0) {
        switch (pSprite->GetSpecialization() & SPECMASK_MONK) {
        case SPECMASK_MONK_OLD_ORDER:
            if (m_nClass != CAIOBJECTTYPE_C_ROGUE) {
                pSprite->GetBaseStats()->field_2FB |= 0x8;
            }
            break;
        case SPECMASK_MONK_BROKEN_ONES:
            if (m_nClass != CAIOBJECTTYPE_C_CLERIC
                || (pSprite->GetSpecialization() & SPECMASK_CLERIC) != SPECMASK_CLERIC_ILMATER) {
                pSprite->GetBaseStats()->field_2FB |= 0x8;
            }
            break;
        case SPECMASK_MONK_DARK_MOON:
            if (m_nClass != CAIOBJECTTYPE_C_SORCERER) {
                pSprite->GetBaseStats()->field_2FB |= 0x8;
            }
            break;
        }
    }

    WORD classID;
    switch (pSprite->GetDerivedStats()->GetBestClass()) {
    case CAIOBJECTTYPE_C_BARBARIAN:
    case CAIOBJECTTYPE_C_FIGHTER:
    case CAIOBJECTTYPE_C_MONK:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
        classID = 0x100;
        break;
    case CAIOBJECTTYPE_C_BARD:
    case CAIOBJECTTYPE_C_ROGUE:
        classID = 0x300;
        break;
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
        classID = 0;
        break;
    case CAIOBJECTTYPE_C_SORCERER:
    case CAIOBJECTTYPE_C_WIZARD:
        classID = 0x200;
        break;
    default:
        return;
    }

    USHORT animationID = static_cast<USHORT>((static_cast<USHORT>(pSprite->GetBaseStats()->m_animationType) & 0xFCFF) | classID);

    // NOTE: The original compares the DWORD at the animation type, so a
    // non-zero `field_22` counts as a change, and the write below clears it.
    if (static_cast<USHORT>(pSprite->GetBaseStats()->m_animationType) != animationID
        || pSprite->GetBaseStats()->field_22 != 0) {
        pSprite->UnequipAll(FALSE);
        pSprite->GetBaseStats()->m_animationType = animationID;
        pSprite->GetBaseStats()->field_22 = 0;
        pSprite->m_animation.SetAnimationType(animationID, pSprite->GetBaseStats()->m_colors, 0);
        pSprite->EquipAll(FALSE);
    }
}

// 0x5E6840
void CScreenCharacter::UpdateHelp(DWORD dwPanelId, DWORD dwTextId, DWORD dwStrId)
{
    STR_RES strRes;

    CUIPanel* pPanel = m_cUIManager.GetPanel(dwPanelId);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9357
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(dwTextId));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9359
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();

    g_pBaldurChitin->GetTlkTable().Fetch(dwStrId, strRes);
    UpdateText(pText, "%s", (LPCSTR)strRes.szText);
}

// 0x5E6950
void CScreenCharacter::CheckDropEquipment()
{
    CheckDropSlot(43);
    CheckDropSlot(44);
    CheckDropSlot(45);
    CheckDropSlot(46);
    CheckDropSlot(47);
    CheckDropSlot(48);
    CheckDropSlot(49);
    CheckDropSlot(50);
    CheckDropSlot(11);
    CheckDropSlot(12);
    CheckDropSlot(13);
    CheckDropSlot(5);
    CheckDropSlot(1);
    CheckDropSlot(6);
    CheckDropSlot(0);
    CheckDropSlot(2);
    CheckDropSlot(7);
    CheckDropSlot(3);
    CheckDropSlot(4);
    CheckDropSlot(8);
    CheckDropSlot(15);
    CheckDropSlot(16);
    CheckDropSlot(17);
}

// 0x5E6A30
void CScreenCharacter::CheckDropSlot(INT nSlot)
{
    // TODO: Incomplete.
}

// 0x5E7020
void CScreenCharacter::OnRestButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9743
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        STRREF strError;
        if (pGame->CanRestParty(strError, 0, 0, 0)) {
            m_nErrorState = 1;
            m_dwErrorTextId = 15358;
            m_strErrorButtonText[0] = 17199;
            m_strErrorButtonText[1] = 11596;
            m_strErrorButtonText[2] = 13727;
            SummonPopup(50, pSprite, 1);
        } else {
            m_nErrorState = 0;
            m_dwErrorTextId = strError;
            m_strErrorButtonText[0] = 11973;
            SummonPopup(9, pSprite, 1);
        }

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E71A0
void CScreenCharacter::ResetErrorPanel(CUIPanel* pPanel)
{
    switch (pPanel->m_nID) {
    case 9:
        m_nNumErrorButtons = 1;
        break;
    case 10:
        m_nNumErrorButtons = 2;
        break;
    case 50:
        m_nNumErrorButtons = 3;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 9829
        UTIL_ASSERT(FALSE);
    }

    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(m_dwErrorTextId, strRes);

    strRes.cSound.SetChannel(0, 0);
    strRes.cSound.SetFireForget(TRUE);
    strRes.cSound.Play(FALSE);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(3));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9842
    UTIL_ASSERT(pText != NULL);

    pText->RemoveAll();
    UpdateText(pText, "%s", (LPCSTR)strRes.szText);

    for (INT nButton = 0; nButton < m_nNumErrorButtons; nButton++) {
        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButton));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 9851
        UTIL_ASSERT(pButton != NULL);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 10508
        UTIL_ASSERT(0 <= nButton && nButton < CSCREENCHARACTER_ERROR_BUTTONS);

        pButton->SetText(FetchString(m_strErrorButtonText[nButton]));
    }
}

// 0x5E73B0
void CScreenCharacter::ResetScriptPanel(CUIPanel* pPanel)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const CRuleTables& rule = pGame->GetRuleTables();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9880
    UTIL_ASSERT(pPanel != NULL);

    CString sScript;
    CString sTitle;
    CString sDescription;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9887
    UTIL_ASSERT(m_pScripts == NULL);

    RefreshScripts();

    m_nScriptIndex = -1;

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(4));
    pText->RemoveAll();

    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (pSprite->m_special2Script != NULL) {
            pSprite->m_special2Script->m_cResRef.CopyToString(sScript);
            sScript.MakeUpper();
        } else {
            sScript = "none";
        }

        if (sScript != "" && sScript.CompareNoCase("none") != 0) {
            rule.GetScriptDescription(sScript, sTitle, sDescription);
            pText->DisplayString(CString(""),
                sTitle + ' ' + sDescription,
                pText->m_rgbLabelColor,
                pText->m_rgbTextColor,
                -1,
                FALSE,
                TRUE);
        } else {
            rule.GetScriptDescription(sScript, sTitle, sDescription);
            pText->DisplayString(CString(""),
                sTitle,
                pText->m_rgbLabelColor,
                pText->m_rgbTextColor,
                -1,
                FALSE,
                TRUE);
        }

        pText->SetTopString(pText->m_plstStrings->FindIndex(0));

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5E76D0
void CScreenCharacter::UpdateCustomizePanel(CGameSprite* pSprite)
{
    CUIControlButton* pButton;
    CUIPanel* pPanel = m_cUIManager.GetPanel(17);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9962
    UTIL_ASSERT(pPanel != NULL);

    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(6));

    BOOL bEnabled = (pSprite->m_baseStats.m_flags & 0x800) != 0;

    pButton = static_cast<CUIControlButton*>(pPanel->GetControl(0));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9975
    UTIL_ASSERT(pButton != NULL);

    pButton->SetEnabled(bEnabled);

    pButton = static_cast<CUIControlButton*>(pPanel->GetControl(1));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 9979
    UTIL_ASSERT(pButton != NULL);

    pButton->SetEnabled(bEnabled);
}

// NOTE: Inlined.
void CScreenCharacter::UpdateScriptPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(11);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10006
    UTIL_ASSERT(pPanel != NULL);

    if (m_pCurrentScrollBar == NULL) {
        m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(3));
    }

    CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(5));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10015
    UTIL_ASSERT(pButton != NULL);

    pButton->SetEnabled(IsDoneButtonClickable(pSprite));
}

// 0x5E7790
void CScreenCharacter::RefreshScripts()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const CRuleTables& rule = pGame->GetRuleTables();

    CString sScript;
    CString sTitle;
    CString sDescription;
    CStringList lStandardScripts;
    CStringList lCustomScripts;
    CString sOtherScript;
    POSITION pos1;
    POSITION pos2;

    m_nScriptIndex = -1;
    if (m_pScripts != NULL) {
        delete m_pScripts;
        m_pScripts = NULL;
    }

    m_pScripts = pGame->GetScripts();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10062
    UTIL_ASSERT(m_pScripts != NULL);

    BOOL bNoneFound = FALSE;
    pos1 = m_pScripts->GetHeadPosition();
    while (pos1 != NULL) {
        sScript = m_pScripts->GetAt(pos1);
        sScript.MakeUpper();
        rule.GetScriptDescription(sScript, sTitle, sDescription);

        if (sScript.CompareNoCase("none") != 0) {
            if (sDescription.Compare("") != 0) {
                pos2 = lStandardScripts.GetHeadPosition();
                while (pos2 != NULL) {
                    sOtherScript = lStandardScripts.GetAt(pos2);
                    if (strcmp(sScript, sOtherScript) < 0) {
                        break;
                    }
                    lStandardScripts.GetNext(pos2);
                }

                if (pos2 != NULL) {
                    lStandardScripts.InsertBefore(pos2, sScript);
                } else {
                    lStandardScripts.AddTail(sScript);
                }
            } else {
                lCustomScripts.AddTail(sScript);
            }
        } else {
            bNoneFound = TRUE;
        }

        m_pScripts->GetNext(pos1);
    }

    m_pScripts->RemoveAll();
    m_pScripts->AddTail(&lStandardScripts);
    m_pScripts->AddTail(&lCustomScripts);

    if (bNoneFound) {
        m_pScripts->AddHead("None");
    }

    CUIPanel* pPanel = m_cUIManager.GetPanel(11);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10114
    UTIL_ASSERT(pPanel != NULL);

    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(2));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10116
    UTIL_ASSERT(pText != NULL);

    pText->m_rgbHighlightColor = CBaldurChitin::TEXTDISPLAY_COLOR_HIGHLIGHT;
    pText->RemoveAll();
    pText->field_A68 = 0x7FFE;

    INT nIndex = 0;
    pos1 = m_pScripts->GetHeadPosition();
    while (pos1 != NULL) {
        sScript = m_pScripts->GetAt(pos1);
        rule.GetScriptDescription(sScript, sTitle, sDescription);
        if (sScript.CompareNoCase("none") != 0) {
            pText->DisplayString(CString(""),
                sTitle + ' ' + sDescription,
                pText->m_rgbLabelColor,
                pText->m_rgbTextColor,
                nIndex,
                FALSE,
                TRUE);
        } else {
            pText->DisplayString(CString(""),
                sScript,
                pText->m_rgbLabelColor,
                pText->m_rgbTextColor,
                nIndex,
                FALSE,
                TRUE);
        }

        pText->DisplayString(CString(""),
            CString(""),
            pText->m_rgbLabelColor,
            pText->m_rgbTextColor,
            -1,
            FALSE,
            TRUE);

        m_pScripts->GetNext(pos1);
        nIndex++;
    }

    pText->SetTopString(pText->m_plstStrings->FindIndex(0));
}

// 0x5E7C50
void CScreenCharacter::OnScriptItemSelect(INT nItem)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10164
    UTIL_ASSERT(pGame != NULL);

    CString sScript;
    CString sTitle;
    CString sDescription;

    if (nItem >= 0) {
        CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
        renderLock.Lock(INFINITE);

        // NOTE: Uninline.
        INT nGameSprite = pGame->GetCharacterId(m_nSelectedCharacter);

        CGameSprite* pSprite;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetDeny(nGameSprite,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (nItem != m_nScriptIndex) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10198
                UTIL_ASSERT(m_pScripts != NULL);

                CUIPanel* pPanel = m_cUIManager.GetPanel(11);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10202
                UTIL_ASSERT(pPanel != NULL);

                CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(2));

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10204
                UTIL_ASSERT(pText != NULL);

                // NOTE: Each script takes two lines of the list (the entry
                // and a blank line), hence the doubled index.
                if (m_nScriptIndex != -1) {
                    pText->SetItemTextColor(pText->GetItemBossPosition(m_nScriptIndex * 2),
                        pText->m_rgbTextColor);
                }

                m_nScriptIndex = nItem;

                if (m_nScriptIndex != -1) {
                    pText->SetItemTextColor(pText->GetItemBossPosition(m_nScriptIndex * 2),
                        CBaldurChitin::TEXTDISPLAY_COLOR_SELECT);
                }

                CUIControlTextDisplay* pDescriptionText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(4));
                pDescriptionText->RemoveAll();

                sScript = m_pScripts->GetAt(m_pScripts->FindIndex(m_nScriptIndex));
                pGame->GetRuleTables().GetScriptDescription(sScript, sTitle, sDescription);

                if (sScript.CompareNoCase("none") != 0) {
                    pDescriptionText->DisplayString(CString(""),
                        sTitle + ' ' + sDescription,
                        pDescriptionText->m_rgbLabelColor,
                        pDescriptionText->m_rgbTextColor,
                        -1,
                        FALSE,
                        TRUE);
                } else {
                    pDescriptionText->DisplayString(CString(""),
                        sTitle,
                        pDescriptionText->m_rgbLabelColor,
                        pDescriptionText->m_rgbTextColor,
                        -1,
                        FALSE,
                        TRUE);
                }

                pDescriptionText->SetTopString(pDescriptionText->m_plstStrings->FindIndex(0));

                UpdatePopupPanel(pPanel->m_nID, pSprite);
            }

            pGame->GetObjectArray()->ReleaseDeny(nGameSprite,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        renderLock.Unlock();
    }
}

// 0x5E7FF0
void CScreenCharacter::OnErrorButtonClick(INT nButton)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10276
    UTIL_ASSERT(0 <= nButton && nButton < GetNumErrorButtons());

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10278
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    // NOTE: Uninline.
    INT nGameSprite = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        switch (m_nErrorState) {
        case 0:
            switch (nButton) {
            case 0:
                DismissPopup(pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10340
                UTIL_ASSERT(FALSE);
            }
            break;
        case 1:
            switch (nButton) {
            case 0:
                DismissPopup(pSprite);

                CInfGame::m_bHealPartyOnRest = FALSE;
                pGame->RestParty(1, 0);
                break;
            case 1:
                DismissPopup(pSprite);

                CInfGame::m_bHealPartyOnRest = TRUE;
                pGame->RestParty(1, 0);
                CInfGame::m_bHealPartyOnRest = FALSE;
                break;
            case 2:
                DismissPopup(pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10327
                UTIL_ASSERT(FALSE);
            }
            break;
        case 2:
            switch (nButton) {
            case 0:
                DismissPopup(pSprite);
                LevelUp(pSprite);
                break;
            case 1:
                DismissPopup(pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10371
                UTIL_ASSERT(FALSE);
            }
            break;
        case 3:
            switch (nButton) {
            case 0:
                DismissPopup(pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10353
                UTIL_ASSERT(FALSE);
            }
            break;
        case 6:
            switch (nButton) {
            case 0:
                if (1) {
                    DismissPopup(pSprite);

                    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 10426
                    UTIL_ASSERT(nCharacterId != CGameObjectArray::INVALID_INDEX);

                    pGame->CharacterExport(nCharacterId, field_176E);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 10430
                    UTIL_ASSERT(m_pCharacters != NULL);

                    delete m_pCharacters;
                    m_pCharacters = NULL;

                    DismissPopup(pSprite);
                }
                break;
            case 1:
                DismissPopup(pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10443
                UTIL_ASSERT(FALSE);
            }
            break;
        case 7:
            switch (nButton) {
            case 0:
                DismissPopup(pSprite);

                if (pSprite->HaveUnexportableItems()) {
                    m_nErrorState = 6;
                    m_dwErrorTextId = 26586;
                    m_strErrorButtonText[0] = 13912;
                    m_strErrorButtonText[1] = 13913;
                    SummonPopup(10, pSprite, 1);
                } else {
                    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 10395
                    UTIL_ASSERT(nCharacterId != CGameObjectArray::INVALID_INDEX);

                    pGame->CharacterExport(nCharacterId, field_176E);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 10399
                    UTIL_ASSERT(m_pCharacters != NULL);

                    delete m_pCharacters;
                    m_pCharacters = NULL;

                    DismissPopup(pSprite);
                }
                break;
            case 1:
                DismissPopup(pSprite);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10413
                UTIL_ASSERT(FALSE);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 10449
            UTIL_ASSERT(FALSE);
        }

        pGame->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E8460
void CScreenCharacter::OnCharacterItemSelect(INT nItem)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Uninline.
    INT nGameSprite = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (nItem != m_nCharacterIndex) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 10552
            UTIL_ASSERT(m_pCharacters != NULL);

            CUIPanel* pPanel = m_cUIManager.GetPanel(13);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 10556
            UTIL_ASSERT(pPanel != NULL);

            CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(0));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 10558
            UTIL_ASSERT(pText != NULL);

            if (m_nCharacterIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nCharacterIndex),
                    pText->m_rgbTextColor);
            }

            m_nCharacterIndex = nItem;

            if (m_nCharacterIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nCharacterIndex),
                    CBaldurChitin::TEXTDISPLAY_COLOR_SELECT);

                CUIControlEdit* pEdit = static_cast<CUIControlEdit*>(pPanel->GetControl(6));

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10577
                UTIL_ASSERT(pEdit != NULL);

                POSITION pos = m_pCharacters->FindIndex(m_nCharacterIndex);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                // __LINE__: 10579
                UTIL_ASSERT(pos != NULL);

                pEdit->SetText(m_pCharacters->GetAt(pos));
            }

            UpdatePopupPanel(GetTopPopup()->m_nID, pSprite);
        }

        pGame->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5E8690
void CScreenCharacter::CancelEngine()
{
    while (GetTopPopup() != NULL) {
        if (!OnCancelButtonClick()) {
            break;
        }
    }

    while (m_lPopupStack.GetTailPosition() != NULL) {
        DismissPopup(NULL);
    }
}

// 0x5E86D0
void CScreenCharacter::UpdateCharacterStatus(LONG nCharacterId)
{
    CBaldurEngine::UpdateCharacterStatus(nCharacterId);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    if (pGame != NULL && field_17A4 == 0) {
        if (pGame->GetCharacterId(GetSelectedCharacter()) == nCharacterId) {
            CGameSprite* pSprite;

            BYTE rc;
            do {
                rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pSprite),
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                BOOL bIsDead = (pSprite->GetDerivedStats()->m_generalState & STATE_DEAD) != 0;

                pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                if (bIsDead) {
                    while (GetTopPopup() != NULL) {
                        OnCancelButtonClick();
                    }
                }
            }

            if (GetTopPopup() == NULL) {
                UpdateMainPanel(FALSE);
            }
        }
    }
}

// 0x5E87E0
void CScreenCharacter::UpdateFeatsPanel(CGameSprite* pSprite)
{
    CString sName;

    CUIPanel* pPanel = m_cUIManager.GetPanel(56);

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    HighlightLabel(pPanel,
        0x1000000C,
        m_nExtraFeats != 0,
        COLOR_LABEL_HIGHLIGHT_BONUS);
    UpdateLabel(pPanel, 0x1000000C, "%d", m_nExtraFeats);

    LONG nHPCONBonusTotalOld = pSprite->m_nHPCONBonusTotalOld;
    SHORT nMaxHitPoints = pSprite->m_derivedStats.m_nMaxHitPoints;
    WORD nMaxHitPointsBase = pSprite->m_baseStats.m_maxHitPointsBase;
    SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
    pSprite->field_562C = 1;
    pSprite->ProcessEffectList();
    pSprite->m_baseStats.m_hitPoints = nHitPoints;
    pSprite->m_derivedStats.m_nMaxHitPoints = nMaxHitPoints;
    pSprite->m_baseStats.m_maxHitPointsBase = nMaxHitPointsBase;
    pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;

    INT nTopFeat = static_cast<USHORT>(m_nTopFeat);
    INT nMaxTopFeat = FEAT_COUNT - FEAT_SLOTS;
    if (min(nTopFeat, nMaxTopFeat) < 0) {
        nTopFeat = 0;
    } else if (nTopFeat >= nMaxTopFeat) {
        nTopFeat = nMaxTopFeat;
    }
    m_nTopFeat = static_cast<SHORT>(nTopFeat);

    for (DWORD nButtonID = 14; nButtonID <= 30; nButtonID += 2) {
        DWORD nRow = (nButtonID - 14) >> 1;
        DWORD nFeatId = ruleTables.GetFeatId(static_cast<USHORT>(m_nTopFeat) + nRow);

        sName = FetchString(ruleTables.GetFeatName(nFeatId));
        UpdateLabel(pPanel, 0x10000001 + nRow, "%s", (LPCSTR)sName);
    }

    CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(0));
    pDone->SetEnabled(IsDoneButtonClickable(pSprite));

    CUIControlScrollBarCharacterFeats* pScroll = static_cast<CUIControlScrollBarCharacterFeats*>(pPanel->GetControl(104));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10806
    UTIL_ASSERT(pScroll != NULL);

    // NOTE: Uninline.
    pScroll->UpdateScrollBar();

    INT nRow = 0;
    for (INT nButtonID = 15; nButtonID < 33; nButtonID += 2) {
        BOOL bPlus = pSprite->sub_763A40(ruleTables.GetFeatId(static_cast<USHORT>(m_nTopFeat) + nRow), 1)
            && m_nExtraFeats > 0;

        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID - 1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 10824
        UTIL_ASSERT(pButton != NULL);

        pButton->SetActive(bPlus);
        pButton->SetEnabled(bPlus);

        HighlightLabel(pPanel, 0x10000001 + nRow, bPlus == FALSE, COLOR_LABEL_DISABLE);

        INT nFeat = static_cast<USHORT>(m_nTopFeat) + nRow;
        BOOL bMinus = m_storedFeats[nFeat] < pSprite->GetFeatValue(nFeat);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 10836
        UTIL_ASSERT(pButton != NULL);

        pButton->SetActive(bMinus);
        pButton->SetEnabled(bMinus);

        nRow++;
    }

    pPanel->InvalidateRect(NULL);
}

// 0x5E8B70
void CScreenCharacter::UpdateClassSelectionPanel(CGameSprite* pSprite)
{
    CUIPanel* pPanel = m_cUIManager.GetPanel(54);

    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(14));

    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    CDerivedStats DStats;
    DStats = *pSprite->GetDerivedStats();

    rule.GetNextLevel(pSprite->m_startTypeAI.GetClass(),
        DStats,
        pSprite);

    for (DWORD nButtonID = 2; nButtonID <= 12; nButtonID++) {
        CUIControlButtonCharacterClassSelection* pButton = static_cast<CUIControlButtonCharacterClassSelection*>(pPanel->GetControl(nButtonID));
        BYTE nClass = static_cast<BYTE>(nButtonID - 1);

        INT nChange = 0;
        if (m_nClass == nClass) {
            nChange = DStats.m_nLevel - m_pTempDerivedStats->m_nLevel;
            pButton->SetSelected(TRUE);
        } else {
            pButton->SetSelected(FALSE);
        }

        INT nExtraLevels = nChange + pSprite->GetDerivedStats()->GetClassLevel(nClass);
        if (nExtraLevels > 0) {
            UpdateLabel(pPanel, nButtonID + 0x1000000D, "%d", nExtraLevels);
        } else {
            UpdateLabel(pPanel, nButtonID + 0x1000000D, "");
        }

        BOOL bEnabled;

        BOOL bCheckClass;
        switch (pSprite->m_startTypeAI.GetRace()) {
        case CAIOBJECTTYPE_R_HUMAN:
            bCheckClass = TRUE;
            break;
        case CAIOBJECTTYPE_R_ELF:
        case CAIOBJECTTYPE_R_HALF_ELF:
        case CAIOBJECTTYPE_R_DWARF:
        case CAIOBJECTTYPE_R_HALFLING:
        case CAIOBJECTTYPE_R_GNOME:
        case CAIOBJECTTYPE_R_HALF_ORC:
            // NOTE: Every button's class is in 1..11, so this always passes.
            bCheckClass = nClass != 0 && nClass <= 11;
            break;
        default:
            bCheckClass = FALSE;
            break;
        }

        if (bCheckClass) {
            // NOTE: Uninline.
            bEnabled = rule.IsValidAlignment(pButton->GetClass(),
                pSprite->m_startTypeAI.GetAlignment(),
                0);
            if (bEnabled) {
                switch (pButton->m_nID) {
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 9:
                case 10:
                case 11:
                case 12:
                    break;
                case 7:
                    // The monk's button: a fallen monk (CRE 0x2FB bit 8).
                    if ((pSprite->GetBaseStats()->field_2FB & 0x8) != 0) {
                        bEnabled = FALSE;
                    }
                    break;
                case 8:
                    // The paladin's button: a fallen paladin (CRE 0x2FB bit 4).
                    if ((pSprite->GetBaseStats()->field_2FB & 0x4) != 0) {
                        bEnabled = FALSE;
                    }
                    break;
                default:
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
                    // __LINE__: 12808
                    UTIL_ASSERT(FALSE);
                }
            }
        } else {
            bEnabled = FALSE;
        }

        pButton->SetEnabled(bEnabled);
    }

    CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(0));
    pDone->SetEnabled(IsDoneButtonClickable(pSprite));

    CUIControlButton* pCancelButton = static_cast<CUIControlButton*>(pPanel->GetControl(27));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11109
    UTIL_ASSERT(pCancelButton != NULL);

    pCancelButton->SetEnabled(TRUE);
}

// NOTE: Inlined.
INT CScreenCharacter::GetNumHatedRaces()
{
    return CSCREENCHARACTER_NUM_HATEDRACES;
}

// NOTE: Uninline.
void CScreenCharacter::SetTopHatedRace(INT nTopHatedRace)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10613
    UTIL_ASSERT(0 <= nTopHatedRace && nTopHatedRace <= GetNumHatedRaces());

    m_nTopHatedRace = nTopHatedRace;
}

// NOTE: Uninline.
BYTE CScreenCharacter::GetHatedRace(INT nIndex)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 10639
    UTIL_ASSERT(0 <= nIndex && nIndex < CSCREENCHARACTER_NUM_HATEDRACES);

    return m_hatedRaces[nIndex];
}

// 0x5E94C0
void CScreenCharacter::CheckMultiPlayerViewableModifyable()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11144
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        // NOTE: Uninline.
        m_bMultiPlayerViewable = pSprite->InControl();
        m_bMultiPlayerModifyable = m_bMultiPlayerViewable;

        if (!pSprite->Orderable(TRUE)) {
            m_bMultiPlayerViewable = FALSE;
        }

        if (!m_bMultiPlayerViewable) {
            if (g_pBaldurChitin->cNetwork.GetSessionOpen()) {
                m_bMultiPlayerViewable = g_pBaldurChitin->cNetwork.GetSessionHosting()
                    || pGame->m_singlePlayerPermissions.GetSinglePermission(CGamePermission::LEADER)
                    || pGame->m_singlePlayerPermissions.GetSinglePermission(CGamePermission::CHAR_RECORDS);
            }
        }

        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5E9600
void CScreenCharacter::OnCustomizeButtonClick()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11209
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        m_cResPortraitSmall = pSprite->GetBaseStats()->m_portraitSmall;
        m_cResPortraitLarge = pSprite->GetBaseStats()->m_portraitLarge;
        m_cResPortraitSmallTemp = m_cResPortraitSmall;
        m_cResPortraitLargeTemp = m_cResPortraitLarge;
        m_cResSoundSet = pSprite->m_secondarySounds;
        field_856 = pSprite->field_725A;

        if (pSprite->m_special2Script != NULL) {
            pSprite->m_special2Script->m_cResRef.CopyToString(field_832);
            field_832.MakeUpper();
        } else {
            field_832 = "NONE";
        }

        SummonPopup(17, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E9800
void CScreenCharacter::OnAppearanceButtonClick()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11271
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(18, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E9910
void CScreenCharacter::OnSoundsButtonClick()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11328
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(20, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E9A20
void CScreenCharacter::OnPortraitLargeItemSelect(INT nItem)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11436
    UTIL_ASSERT(pGame != NULL);

    // FIXME: What for?
    SHORT nPortrait = g_pBaldurChitin->m_pEngineCharacter->GetSelectedCharacter();

    // NOTE: Uninline.
    INT nGameSprite = pGame->GetCharacterId(nPortrait);

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (nItem != m_nPortraitLargeIndex) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 11456
            UTIL_ASSERT(m_pPortraits != NULL);

            CUIPanel* pPanel = m_cUIManager.GetPanel(19);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 11460
            UTIL_ASSERT(pPanel != NULL);

            CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(2));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 11462
            UTIL_ASSERT(pText != NULL);

            if (m_nPortraitLargeIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nPortraitLargeIndex),
                    pText->m_rgbTextColor);
            }

            m_nPortraitLargeIndex = nItem;

            if (m_nPortraitLargeIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nPortraitLargeIndex),
                    CBaldurChitin::TEXTDISPLAY_COLOR_SELECT);
            }

            UpdatePopupPanel(GetTopPopup()->m_nID, pSprite);
        }

        pGame->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5E9BD0
void CScreenCharacter::OnPortraitSmallItemSelect(INT nItem)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11512
    UTIL_ASSERT(pGame != NULL);

    // FIXME: What for?
    SHORT nPortrait = g_pBaldurChitin->m_pEngineCharacter->GetSelectedCharacter();

    // NOTE: Uninline.
    INT nGameSprite = pGame->GetCharacterId(nPortrait);

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (nItem != m_nPortraitSmallIndex) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 11532
            UTIL_ASSERT(m_pPortraits != NULL);

            CUIPanel* pPanel = m_cUIManager.GetPanel(19);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 11536
            UTIL_ASSERT(pPanel != NULL);

            CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(3));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 11538
            UTIL_ASSERT(pText != NULL);

            if (m_nPortraitSmallIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nPortraitSmallIndex),
                    pText->m_rgbTextColor);
            }

            m_nPortraitSmallIndex = nItem;

            if (m_nPortraitSmallIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nPortraitSmallIndex),
                    CBaldurChitin::TEXTDISPLAY_COLOR_SELECT);
            }

            UpdatePopupPanel(GetTopPopup()->m_nID, pSprite);
        }

        pGame->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5E9D80
void CScreenCharacter::OnCustomPortraitsButtonClick()
{
    CSingleLock renderLock(&(m_cUIManager.field_36), FALSE);
    renderLock.Lock(INFINITE);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11592
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        SummonPopup(19, pSprite, 1);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// 0x5E9E90
void CScreenCharacter::OnPlayButtonClick()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    INT nGameSprite = pGame->GetCharacterId(m_nSelectedCharacter);

    // NOTE: Unused.
    STR_RES strRes;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (m_pSounds->FindIndex(m_nCustomSoundSetIndex) != NULL) {
            // NOTE: Unused.
            CString v2;

            CString sSuffix;
            CString sSoundSetName;
            CString sDirName;
            CString sBaseName;

            int v1 = m_nCustomSoundIndex;

            while (1) {
                if (m_nCustomSoundSetIndex != -1) {
                    POSITION pos = m_pSounds->FindIndex(m_nCustomSoundSetIndex);
                    if (pos != NULL) {
                        sSoundSetName = m_pSounds->GetAt(pos);
                    }
                }

                memcpy(pSprite->field_725A,
                    sSoundSetName.GetBuffer(sSoundSetName.GetLength()),
                    min(sSoundSetName.GetLength(), 32));

                if (sSoundSetName.GetLength() < 32) {
                    pSprite->field_725A[sSoundSetName.GetLength()] = '\0';
                }

                sDirName = g_pBaldurChitin->GetObjectGame()->GetDirSounds() + sSoundSetName + '\\';

                g_pBaldurChitin->cDimm.AddToDirectoryList(sDirName, TRUE);

                CString sFileName;
                CString sPattern;

                sPattern = g_pBaldurChitin->GetObjectGame()->GetDirSounds() + "\\" + sSoundSetName + "\\*.wav";

                WIN32_FIND_DATAA findFileData;
                HANDLE hFindFile = FindFirstFileA(sPattern, &findFileData);
                if (hFindFile != INVALID_HANDLE_VALUE) {
                    sFileName = findFileData.cFileName;

                    if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                        sFileName.MakeUpper();
                        sFileName = sFileName.SpanExcluding(".");
                        sFileName = sFileName.Left(sFileName.GetLength() - 2);
                        sBaseName = sFileName;
                    }

                    // FIXME: Leaking `hFindFile`.
                }

                if (m_nCustomSoundIndex < 10) {
                    sSuffix.Format("0%d", m_nCustomSoundIndex);
                } else {
                    sSuffix.Format("%d", m_nCustomSoundIndex);
                }

                sFileName = sBaseName + sSuffix;

                if (m_nCustomSoundIndex == 40) {
                    m_nCustomSoundIndex = 0;
                } else {
                    m_nCustomSoundIndex++;
                }

                if (m_nCustomSoundIndex == 30) {
                    m_nCustomSoundIndex = 34;
                }

                CSound cSound(CResRef(sFileName), 0, 0, 0, INT_MAX, FALSE);
                cSound.SetFireForget(TRUE);

                if (cSound.GetRes() != NULL) {
                    cSound.Play(FALSE);
                    break;
                }

                if (v1 == m_nCustomSoundIndex) {
                    break;
                }
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5EA480
void CScreenCharacter::OnSoundItemSelect(INT nItem)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: Uninline.
    INT nGameSprite = pGame->GetCharacterId(m_nSelectedCharacter);

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (nItem != m_nCustomSoundSetIndex) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 12046
            UTIL_ASSERT(m_pSounds != NULL);

            CUIPanel* pPanel = m_cUIManager.GetPanel(20);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 12050
            UTIL_ASSERT(pPanel != NULL);

            CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(5));

            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 12052
            UTIL_ASSERT(pText != NULL);

            if (m_nCustomSoundSetIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nCustomSoundSetIndex),
                    pText->m_rgbTextColor);
            }

            m_nCustomSoundSetIndex = nItem;

            if (m_nCustomSoundSetIndex != -1) {
                pText->SetItemTextColor(pText->GetItemBossPosition(m_nCustomSoundSetIndex),
                    CBaldurChitin::TEXTDISPLAY_COLOR_SELECT);
            }

            m_nCustomSoundIndex = 3;

            UpdatePopupPanel(GetTopPopup()->m_nID, pSprite);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5EA620
void CScreenCharacter::ResetCustomizeBiographyPanel(CUIPanel* pPanel, CGameSprite* pSprite)
{
    CUIControlEditMultiLine* pEdit = static_cast<CUIControlEditMultiLine*>(pPanel->GetControl(4));
    pEdit->Remove();

    STRREF strBiography = pSprite->GetBaseStats()->m_biography;

    CString sBiography;
    STR_RES strRes;

    if (strBiography == -1) {
        strBiography = g_pBaldurChitin->GetTlkTable().m_override.Add(CString(""));
        pSprite->GetBaseStats()->m_biography = strBiography;
    }

    sBiography = FetchString(strBiography);
    pEdit->SetText(sBiography);

    field_1B8 = TRUE;
    m_cUIManager.SetCapture(pEdit, CUIManager::KEYBOARD);

    CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(6));
    pButton->SetEnabled(g_pBaldurChitin->GetTlkTable().m_override.Fetch(strBiography, strRes));
}

// 0x5F7850
void CScreenCharacter::ResetSkillsPanel(CUIPanel* pPanel, CGameSprite* pSprite, int a3)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    CString sNumber;

    if (a3) {
        LONG nHPCONBonusTotalOld = pSprite->m_nHPCONBonusTotalOld;
        WORD nMaxHitPointsBase = pSprite->m_baseStats.m_maxHitPointsBase;
        SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
        SHORT nMaxHitPoints = pSprite->m_derivedStats.m_nMaxHitPoints;
        pSprite->field_562C = 1;
        pSprite->ProcessEffectList();
        pSprite->m_baseStats.m_hitPoints = nHitPoints;
        pSprite->m_derivedStats.m_nMaxHitPoints = nMaxHitPoints;
        pSprite->m_baseStats.m_maxHitPointsBase = nMaxHitPointsBase;
        pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;

        m_nExtraSkillPoints = m_nLevelUpSkillPoints;

        for (m_nTopSkill = 0; m_nTopSkill < SKILL_COUNT; m_nTopSkill++) {
            m_storedSkills[m_nTopSkill] = pSprite->GetSkillValue(m_nTopSkill);
        }

        // Scroll to first available skill.
        for (m_nTopSkill = 0; m_nTopSkill < SKILL_COUNT; m_nTopSkill++) {
            if (pSprite->GetSkillCost(ruleTables.GetSkillId(m_nTopSkill), m_nClass) != 0) {
                break;
            }
        }
    } else {
        m_nTopSkill = 0;
    }

    sNumber.Format("%d", m_nExtraSkillPoints);
    g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_NUMBER, sNumber);
    UpdateHelp(pPanel->m_nID, 92, 17248);

    INT nSlot = 0;
    for (INT nButtonID = 15; nButtonID < 33; nButtonID += 2) {
        INT nCost = pSprite->GetSkillCost(ruleTables.GetSkillId(nSlot), m_nClass);
        BOOL bEnabled = nCost != 0 && m_nExtraSkillPoints >= nCost;

        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID - 1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17847
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(bEnabled);
        pButton->SetActive(bEnabled);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17854
        UTIL_ASSERT(pButton != NULL);

        pButton->SetEnabled(bEnabled);
        pButton->SetActive(bEnabled);

        nSlot++;
    }
}

// 0x5F7B00
void CScreenCharacter::UpdateSkillsPanel(CGameSprite* pSprite)
{
    CString sName;

    CUIPanel* pPanel = m_cUIManager.GetPanel(55);
    m_pCurrentScrollBar = static_cast<CUIControlScrollBar*>(pPanel->GetControl(104));

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    SHORT nMaxHitPoints = pSprite->m_derivedStats.m_nMaxHitPoints;
    SHORT nHitPoints = pSprite->m_baseStats.m_hitPoints;
    LONG nHPCONBonusTotalOld = pSprite->m_nHPCONBonusTotalOld;
    WORD nMaxHitPointsBase = pSprite->m_baseStats.m_maxHitPointsBase;
    pSprite->field_562C = 1;
    pSprite->ProcessEffectList();
    pSprite->m_baseStats.m_hitPoints = nHitPoints;
    pSprite->m_derivedStats.m_nMaxHitPoints = nMaxHitPoints;
    pSprite->m_baseStats.m_maxHitPointsBase = nMaxHitPointsBase;
    pSprite->m_nHPCONBonusTotalOld = nHPCONBonusTotalOld;

    HighlightLabel(pPanel,
        0x1000000C,
        m_nExtraSkillPoints != 0,
        COLOR_LABEL_HIGHLIGHT_BONUS);
    UpdateLabel(pPanel, 0x1000000C, "%d", m_nExtraSkillPoints);

    INT nTopSkill = static_cast<USHORT>(m_nTopSkill);
    INT nMaxTopSkill = SKILL_COUNT - SKILL_SLOTS;
    if (min(nTopSkill, nMaxTopSkill) < 0) {
        nTopSkill = 0;
    } else if (nTopSkill >= nMaxTopSkill) {
        nTopSkill = nMaxTopSkill;
    }
    m_nTopSkill = static_cast<SHORT>(nTopSkill);

    for (DWORD nButtonID = 14; nButtonID <= 30; nButtonID += 2) {
        DWORD nRow = (nButtonID - 14) >> 1;
        DWORD nSkillId = ruleTables.GetSkillId(static_cast<USHORT>(m_nTopSkill) + nRow);

        // NOTE: The original looks the skill id up again before asking for
        // the cost.
        INT nCost = pSprite->GetSkillCost(ruleTables.GetSkillId(nSkillId), m_nClass);

        sName = FetchString(ruleTables.GetSkillName(nSkillId));

        if (nCost > 0) {
            UpdateLabel(pPanel, 0x10000001 + nRow, "%s (%d)", (LPCSTR)sName, nCost);
        } else {
            UpdateLabel(pPanel, 0x10000001 + nRow, "%s", (LPCSTR)sName);
        }

        UpdateLabel(pPanel, 0x10000069 + nRow, "");
    }

    CUIControlButton* pDone = static_cast<CUIControlButton*>(pPanel->GetControl(0));
    pDone->SetEnabled(IsDoneButtonClickable(pSprite));

    CUIControlScrollBarCharacterSkills* pScroll = static_cast<CUIControlScrollBarCharacterSkills*>(pPanel->GetControl(104));

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18063
    UTIL_ASSERT(pScroll != NULL);

    // NOTE: Uninline.
    pScroll->UpdateScrollBar();

    INT nRow = 0;
    for (INT nButtonID = 15; nButtonID < 33; nButtonID += 2) {
        INT nCost = pSprite->GetSkillCost(ruleTables.GetSkillId(static_cast<USHORT>(m_nTopSkill) + nRow), m_nClass);
        BOOL bPlus = nCost != 0 && m_nExtraSkillPoints >= nCost;

        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID - 1));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18083
        UTIL_ASSERT(pButton != NULL);

        pButton->SetActive(bPlus);
        pButton->SetEnabled(bPlus);

        HighlightLabel(pPanel, 0x10000001 + nRow, bPlus == FALSE, COLOR_LABEL_DISABLE);

        INT nSkill = static_cast<USHORT>(m_nTopSkill) + nRow;
        BOOL bMinus = m_storedSkills[nSkill] < pSprite->GetSkillValue(nSkill);

        pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nButtonID));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18095
        UTIL_ASSERT(pButton != NULL);

        pButton->SetActive(bMinus);
        pButton->SetEnabled(bMinus);

        UpdateLabel(pPanel,
            0x10000069 + nRow,
            "%d",
            pSprite->GetSkillValue(static_cast<USHORT>(m_nTopSkill) + nRow));

        COLORREF rgbColor;
        if (bMinus) {
            rgbColor = 0xFFFF00;
        } else if (!bPlus) {
            rgbColor = COLOR_LABEL_DISABLE;
        } else {
            rgbColor = COLOR_LABEL_NORMAL;
        }
        HighlightLabel(pPanel, 0x10000069 + nRow, TRUE, rgbColor);

        nRow++;
    }

    pPanel->InvalidateRect(NULL);
}

// 0x5F89B0
void CScreenCharacter::sub_5F89B0(CGameSprite* pSprite)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    for (INT nFeat = 0; nFeat < ruleTables.m_tFeats.GetHeight(); nFeat++) {
        if (m_storedFeats[nFeat] < pSprite->GetFeatValue(nFeat)) {
            while (pSprite->GetFeatValue(nFeat) > 0) {
                if (pSprite->sub_763200(nFeat, 1)) {
                    break;
                }

                pSprite->SetFeatValue(nFeat, pSprite->GetFeatValue(nFeat) - 1);
                m_nExtraFeats++;
            }
        }
    }
}

// 0x5F8C90
void CScreenCharacter::sub_5F8C90(int a1)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    CUIPanel* pPanel = m_cUIManager.GetPanel(57);

    INT nIndex = 0;
    INT nCount;

    switch (a1) {
    case 0:
        field_184E = ruleTables.m_tHelp01.GetHeight();
        nCount = field_184E > 10 ? 11 : field_184E;
        for (nIndex = 0; nIndex < nCount; nIndex++) {
            // NOTE: The first two folders ignore the scroll position.
            UpdateLabel(pPanel,
                0x10000030 + nIndex,
                "%s",
                (LPCSTR)FetchString(iwd2r_atol(ruleTables.m_tHelp01.GetAt(CPoint(0, nIndex)))));
        }
        break;
    case 1:
        field_184E = ruleTables.m_tHelp02.GetHeight();
        nCount = field_184E > 10 ? 11 : field_184E;
        for (nIndex = 0; nIndex < nCount; nIndex++) {
            UpdateLabel(pPanel,
                0x10000030 + nIndex,
                "%s",
                (LPCSTR)FetchString(iwd2r_atol(ruleTables.m_tHelp02.GetAt(CPoint(0, nIndex)))));
        }
        break;
    case 2:
        field_184E = FEAT_COUNT;
        for (nIndex = 0; nIndex < 11; nIndex++) {
            UpdateLabel(pPanel,
                0x10000030 + nIndex,
                "%s",
                (LPCSTR)FetchString(ruleTables.GetFeatName(ruleTables.GetFeatId(field_1852 + nIndex))));
        }
        break;
    case 3:
        field_184E = ruleTables.m_tHelp03.GetHeight();
        nCount = field_184E > 10 ? 11 : field_184E;
        for (nIndex = 0; nIndex < nCount; nIndex++) {
            UpdateLabel(pPanel,
                0x10000030 + nIndex,
                "%s",
                (LPCSTR)FetchString(iwd2r_atol(ruleTables.m_tHelp03.GetAt(CPoint(0, field_1852 + nIndex)))));
        }
        break;
    case 4:
        field_184E = ruleTables.m_tHelp04.GetHeight();
        nCount = field_184E > 10 ? 11 : field_184E;
        for (nIndex = 0; nIndex < nCount; nIndex++) {
            UpdateLabel(pPanel,
                0x10000030 + nIndex,
                "%s",
                (LPCSTR)FetchString(iwd2r_atol(ruleTables.m_tHelp04.GetAt(CPoint(0, field_1852 + nIndex)))));
        }
        break;
    case 5:
        field_184E = SKILL_COUNT;
        for (nIndex = 0; nIndex < 11; nIndex++) {
            UpdateLabel(pPanel,
                0x10000030 + nIndex,
                "%s",
                (LPCSTR)FetchString(ruleTables.GetSkillName(ruleTables.GetSkillId(field_1852 + nIndex))));
        }
        break;
    }

    for (; nIndex < 11; nIndex++) {
        UpdateLabel(pPanel, 0x10000030 + nIndex, "");
    }

    for (INT nFolder = 0; nFolder < 6; nFolder++) {
        HighlightLabel(pPanel, 0x10000004 + nFolder, a1 == nFolder, COLOR_LABEL_HIGHLIGHT_BONUS);
    }

    if (field_1846 != a1) {
        field_1846 = a1;
        field_1852 = 0;
        sub_5F91F0(0);

        CUIControlScrollbarCharacter5F9410* pScrollBar = static_cast<CUIControlScrollbarCharacter5F9410*>(pPanel->GetControl(4));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 19090
        UTIL_ASSERT(pScrollBar != NULL);

        // NOTE: Uninline.
        pScrollBar->UpdateScrollBar();
    }
}

// 0x5F91F0
void CScreenCharacter::sub_5F91F0(int a1)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    CUIPanel* pPanel = m_cUIManager.GetPanel(57);

    STRREF strHelp = -1;

    switch (field_1846) {
    case 0:
        strHelp = iwd2r_atol(ruleTables.m_tHelp01.GetAt(CPoint(1, a1)));
        break;
    case 1:
        strHelp = iwd2r_atol(ruleTables.m_tHelp02.GetAt(CPoint(1, a1)));
        break;
    case 2:
        // NOTE: The description is looked up by row, not by feat id.
        strHelp = ruleTables.GetFeatDescription(a1);
        break;
    case 3:
        strHelp = iwd2r_atol(ruleTables.m_tHelp03.GetAt(CPoint(1, a1)));
        break;
    case 4:
        strHelp = iwd2r_atol(ruleTables.m_tHelp04.GetAt(CPoint(1, a1)));
        break;
    case 5:
        strHelp = ruleTables.GetSkillDescription(a1);
        break;
    }

    field_184A = a1;

    for (INT nIndex = 0; nIndex < 11; nIndex++) {
        HighlightLabel(pPanel,
            0x10000030 + nIndex,
            field_184A - field_1852 == nIndex,
            COLOR_LABEL_HIGHLIGHT_BONUS);

        CUIControlButton* pButton = static_cast<CUIControlButton*>(pPanel->GetControl(nIndex + 71));
        if (pButton != NULL) {
            pButton->SetEnabled(nIndex < field_184E);
        }
    }

    UpdateHelp(57, 2, strHelp);
}

// -----------------------------------------------------------------------------

// 0x5ED4A0
CUIControlButtonCharacterSpecializationSelection::CUIControlButtonCharacterSpecializationSelection(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton3State(panel, controlInfo, LBUTTON, 1)
{
    m_nSelectedFrame = 0;
}

// 0x5ED500
CUIControlButtonCharacterSpecializationSelection::~CUIControlButtonCharacterSpecializationSelection()
{
}

// 0x5ED5A0
DWORD CUIControlButtonCharacterSpecializationSelection::GetSpecialization()
{
    BYTE nClass = g_pBaldurChitin->m_pEngineCharacter->m_nClass;

    switch (nClass) {
    case CAIOBJECTTYPE_C_WIZARD:
        switch (m_nID) {
        case 2:
            return SPECMASK_WIZARD_UNIVERSAL;
        case 3:
            return SPECMASK_WIZARD_ABJURER;
        case 4:
            return SPECMASK_WIZARD_CONJURER;
        case 5:
            return SPECMASK_WIZARD_DIVINER;
        case 6:
            return SPECMASK_WIZARD_ENCHANTER;
        case 7:
            return SPECMASK_WIZARD_EVOKER;
        case 8:
            return SPECMASK_WIZARD_ILLUSIONIST;
        case 9:
            return SPECMASK_WIZARD_NECROMANCER;
        case 10:
            return SPECMASK_WIZARD_TRANSMUTER;
        }
        break;
    case CAIOBJECTTYPE_C_PALADIN:
        switch (m_nID) {
        case 2:
            return SPECMASK_PALADIN_ILMATER;
        case 3:
            return SPECMASK_PALADIN_HELM;
        case 4:
            return SPECMASK_PALADIN_MYSTRA;
        }
        break;
    case CAIOBJECTTYPE_C_MONK:
        switch (m_nID) {
        case 2:
            return SPECMASK_MONK_OLD_ORDER;
        case 3:
            return SPECMASK_MONK_BROKEN_ONES;
        case 4:
            return SPECMASK_MONK_DARK_MOON;
        }
        break;
    case CAIOBJECTTYPE_C_CLERIC:
        switch (m_nID) {
        case 2:
            return SPECMASK_CLERIC_ILMATER;
        case 3:
            return SPECMASK_CLERIC_LATHANDER;
        case 4:
            return SPECMASK_CLERIC_SELUNE;
        case 5:
            return SPECMASK_CLERIC_HELM;
        case 6:
            return SPECMASK_CLERIC_OGHMA;
        case 7:
            return SPECMASK_CLERIC_TEMPUS;
        case 8:
            return SPECMASK_CLERIC_BANE;
        case 9:
            return SPECMASK_CLERIC_MASK;
        case 10:
            return SPECMASK_CLERIC_TALOS;
        }
    }

    return 0;
}

// 0x5ED6F0
void CUIControlButtonCharacterSpecializationSelection::OnLButtonClick(CPoint pt)
{
    CUIControlButton3State::OnLButtonClick(pt);

    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    INT nGameSprite = g_pBaldurChitin->GetObjectGame()->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        DWORD nSpecialization = GetSpecialization();

        // NOTE: The original keeps only the low 16 bits of the old
        // specialization here (movzx), so a cleric's deity bits are dropped.
        pCharacter->m_nSpecialization = nSpecialization | static_cast<WORD>(pCharacter->field_77C);

        STRREF strDescription;
        switch (nSpecialization) {
        case SPECMASK_CLERIC_ILMATER:
            strDescription = 38118;
            break;
        case SPECMASK_CLERIC_LATHANDER:
            strDescription = 38119;
            break;
        case SPECMASK_CLERIC_SELUNE:
            strDescription = 38120;
            break;
        case SPECMASK_CLERIC_HELM:
            strDescription = 38121;
            break;
        case SPECMASK_CLERIC_OGHMA:
            strDescription = 38122;
            break;
        case SPECMASK_CLERIC_TEMPUS:
            strDescription = 38123;
            break;
        case SPECMASK_CLERIC_BANE:
            strDescription = 38124;
            break;
        case SPECMASK_CLERIC_MASK:
            strDescription = 38125;
            break;
        case SPECMASK_CLERIC_TALOS:
            strDescription = 38126;
            break;
        case SPECMASK_MONK_OLD_ORDER:
            strDescription = 39410;
            break;
        case SPECMASK_MONK_BROKEN_ONES:
            strDescription = 39411;
            break;
        case SPECMASK_MONK_DARK_MOON:
            strDescription = 39412;
            break;
        case SPECMASK_PALADIN_ILMATER:
            strDescription = 39407;
            break;
        case SPECMASK_PALADIN_HELM:
            strDescription = 39408;
            break;
        case SPECMASK_PALADIN_MYSTRA:
            strDescription = 39409;
            break;
        case SPECMASK_WIZARD_ABJURER:
            strDescription = 9564;
            break;
        case SPECMASK_WIZARD_CONJURER:
            strDescription = 9565;
            break;
        case SPECMASK_WIZARD_DIVINER:
            strDescription = 9566;
            break;
        case SPECMASK_WIZARD_ENCHANTER:
            strDescription = 9567;
            break;
        case SPECMASK_WIZARD_ILLUSIONIST:
            strDescription = 9568;
            break;
        case SPECMASK_WIZARD_EVOKER:
            strDescription = 9569;
            break;
        case SPECMASK_WIZARD_NECROMANCER:
            strDescription = 9570;
            break;
        case SPECMASK_WIZARD_TRANSMUTER:
            strDescription = 9571;
            break;
        case SPECMASK_WIZARD_UNIVERSAL:
            strDescription = 9563;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 14181
            UTIL_ASSERT(FALSE);

            strDescription = -1;
        }

        pCharacter->UpdateHelp(m_pPanel->m_nID, 13, strDescription);
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x5ED9D0
CUIControlButtonCharacterAbilitiesPlusMinus::CUIControlButtonCharacterAbilitiesPlusMinus(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButtonPlusMinus(panel, controlInfo)
{
}

// 0x5EDA10
CUIControlButtonCharacterAbilitiesPlusMinus::~CUIControlButtonCharacterAbilitiesPlusMinus()
{
}

// NOTE: Ghidra has no function at this address; the vtable (0x852734,
// slot 0x18) points here.
//
// 0x5EDAB0
BOOL CUIControlButtonCharacterAbilitiesPlusMinus::OnLButtonDown(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14242
    UTIL_ASSERT(pCharacter != NULL);

    CString sValue;

    STRREF strHelp;
    BYTE nMin;
    BYTE nMax;
    switch (m_nID) {
    case 16:
    case 17:
        strHelp = 9582;
        nMin = pCharacter->m_nMinSTR;
        nMax = pCharacter->m_nMaxSTR;
        break;
    case 18:
    case 19:
        strHelp = 9584;
        nMin = pCharacter->m_nMinDEX;
        nMax = pCharacter->m_nMaxDEX;
        break;
    case 20:
    case 21:
        strHelp = 9583;
        nMin = pCharacter->m_nMinCON;
        nMax = pCharacter->m_nMaxCON;
        break;
    case 22:
    case 23:
        strHelp = 9585;
        nMin = pCharacter->m_nMinINT;
        nMax = pCharacter->m_nMaxINT;
        break;
    case 24:
    case 25:
        strHelp = 9586;
        nMin = pCharacter->m_nMinWIS;
        nMax = pCharacter->m_nMaxWIS;
        break;
    case 26:
    case 27:
        strHelp = 9587;
        nMin = pCharacter->m_nMinCHR;
        nMax = pCharacter->m_nMaxCHR;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 14288
        UTIL_ASSERT(FALSE);

        strHelp = -1;
        nMin = 0;
        nMax = 0;
    }

    if (!m_bActive) {
        return FALSE;
    }

    if ((m_nMouseButtons & LBUTTON) == 0) {
        return FALSE;
    }

    sValue.Format("%d", nMin);
    g_pBaldurChitin->GetTlkTable().SetToken(CScreenCreateChar::TOKEN_MINIMUM, sValue);

    sValue.Format("%d", nMax);
    g_pBaldurChitin->GetTlkTable().SetToken(CScreenCreateChar::TOKEN_MAXIMUM, sValue);

    pCharacter->UpdateHelp(m_pPanel->m_nID, 29, strHelp);

    return CUIControlButtonPlusMinus::OnLButtonDown(pt);
}

// 0x5EDD50
void CUIControlButtonCharacterAbilitiesPlusMinus::AdjustValue()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    INT nGameSprite = pCharacter->field_1840;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        BYTE nMin;
        BYTE nMax;
        BYTE* pBase;
        BYTE* pValue;
        BOOL bInc;
        switch (m_nID) {
        case 16:
        case 17:
            nMin = pCharacter->m_nMinSTR;
            nMax = pCharacter->m_nMaxSTR;
            pBase = &(pCharacter->m_pTempBaseStats->m_STRBase);
            pValue = &(pSprite->GetBaseStats()->m_STRBase);
            bInc = m_nID == 16;
            break;
        case 18:
        case 19:
            nMin = pCharacter->m_nMinDEX;
            nMax = pCharacter->m_nMaxDEX;
            pBase = &(pCharacter->m_pTempBaseStats->m_DEXBase);
            pValue = &(pSprite->GetBaseStats()->m_DEXBase);
            bInc = m_nID == 18;
            break;
        case 20:
        case 21:
            nMin = pCharacter->m_nMinCON;
            nMax = pCharacter->m_nMaxCON;
            pBase = &(pCharacter->m_pTempBaseStats->m_CONBase);
            pValue = &(pSprite->GetBaseStats()->m_CONBase);
            bInc = m_nID == 20;
            break;
        case 22:
        case 23:
            nMin = pCharacter->m_nMinINT;
            nMax = pCharacter->m_nMaxINT;
            pBase = &(pCharacter->m_pTempBaseStats->m_INTBase);
            pValue = &(pSprite->GetBaseStats()->m_INTBase);
            bInc = m_nID == 22;
            break;
        case 24:
        case 25:
            nMin = pCharacter->m_nMinWIS;
            nMax = pCharacter->m_nMaxWIS;
            pBase = &(pCharacter->m_pTempBaseStats->m_WISBase);
            pValue = &(pSprite->GetBaseStats()->m_WISBase);
            bInc = m_nID == 24;
            break;
        case 26:
        case 27:
            nMin = pCharacter->m_nMinCHR;
            nMax = pCharacter->m_nMaxCHR;
            pBase = &(pCharacter->m_pTempBaseStats->m_CHRBase);
            pValue = &(pSprite->GetBaseStats()->m_CHRBase);
            bInc = m_nID == 26;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 14431
            UTIL_ASSERT(FALSE);
        }

        if (bInc) {
            if (pCharacter->m_nExtraAbilityPoints > 0) {
                if (*pValue < nMax) {
                    (*pValue)++;
                    pCharacter->m_nExtraAbilityPoints--;
                    pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);
                } else {
                    pCharacter->UpdateHelp(m_pPanel->m_nID, 29, 14838);
                }
            }
        } else {
            if (*pValue > *pBase) {
                if (*pValue > nMin) {
                    (*pValue)--;
                    pCharacter->m_nExtraAbilityPoints++;
                    pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);
                } else {
                    pCharacter->UpdateHelp(m_pPanel->m_nID, 29, 14840);
                }
            }
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x5EE000
void CUIControlButtonCharacterAbilitiesHotArea::OnHotAreaClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14501
    UTIL_ASSERT(pCharacter != NULL);

    CString sValue;
    DWORD strDescription;
    BYTE nMin;
    BYTE nMax;
    switch (m_nID) {
    case 30:
        strDescription = 9582;
        nMin = pCharacter->m_nMinSTR;
        nMax = pCharacter->m_nMaxSTR;
        break;
    case 31:
        strDescription = 9584;
        nMin = pCharacter->m_nMinDEX;
        nMax = pCharacter->m_nMaxDEX;
        break;
    case 32:
        strDescription = 9583;
        nMin = pCharacter->m_nMinCON;
        nMax = pCharacter->m_nMaxCON;
        break;
    case 33:
        strDescription = 9585;
        nMin = pCharacter->m_nMinINT;
        nMax = pCharacter->m_nMaxINT;
        break;
    case 34:
        strDescription = 9586;
        nMin = pCharacter->m_nMinWIS;
        nMax = pCharacter->m_nMaxWIS;
        break;
    case 35:
        strDescription = 9587;
        nMin = pCharacter->m_nMinCHR;
        nMax = pCharacter->m_nMaxCHR;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 14541
        UTIL_ASSERT(FALSE);

        strDescription = -1;
        nMin = 0;
        nMax = 0;
    }

    sValue.Format("%d", nMin);
    g_pBaldurChitin->GetTlkTable().SetToken(CScreenCreateChar::TOKEN_MINIMUM, sValue);

    sValue.Format("%d", nMax);
    g_pBaldurChitin->GetTlkTable().SetToken(CScreenCreateChar::TOKEN_MAXIMUM, sValue);

    pCharacter->UpdateHelp(m_pPanel->m_nID, 29, strDescription);
}

// -----------------------------------------------------------------------------

// 0x5EE210
CUIControlScrollBarCharacterHatedRace::CUIControlScrollBarCharacterHatedRace(CUIPanel* panel, UI_CONTROL_SCROLLBAR* controlInfo)
    : CUIControlScrollBar(panel, controlInfo)
{
}

// 0x632C00
CUIControlScrollBarCharacterHatedRace::~CUIControlScrollBarCharacterHatedRace()
{
}

// NOTE: Inlined.
void CUIControlScrollBarCharacterHatedRace::UpdateScrollBar()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14602
    UTIL_ASSERT(pCharacter != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14604
    UTIL_ASSERT(pGame != NULL);

    AdjustScrollBar(pCharacter->m_nTopHatedRace, CSCREENCREATECHAR_NUM_HATEDRACES, 11);
}

// 0x5EE230
void CUIControlScrollBarCharacterHatedRace::OnScrollUp()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14634
    UTIL_ASSERT(pCharacter != NULL);

    INT nTopHatedRace = max(pCharacter->m_nTopHatedRace - 1, 0);

    if (nTopHatedRace != pCharacter->m_nTopHatedRace) {
        // NOTE: Uninline.
        pCharacter->SetTopHatedRace(nTopHatedRace);

        InvalidateItems();

        // NOTE: Uninline.
        UpdateScrollBar();
    }
}

// 0x5EE310
void CUIControlScrollBarCharacterHatedRace::OnScrollDown()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14670
    UTIL_ASSERT(pCharacter != NULL);

    INT nTopHatedRace = min(pCharacter->m_nTopHatedRace + 1, 5);

    if (nTopHatedRace != pCharacter->m_nTopHatedRace) {
        // NOTE: Uninline.
        pCharacter->SetTopHatedRace(nTopHatedRace);

        InvalidateItems();

        // NOTE: Uninline.
        UpdateScrollBar();
    }
}

// 0x5EE3F0
void CUIControlScrollBarCharacterHatedRace::OnPageUp(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14706
    UTIL_ASSERT(pCharacter != NULL);

    INT nStep = min(nLines, 11);
    INT nTopHatedRace = max(pCharacter->m_nTopHatedRace - nStep, 0);

    if (nTopHatedRace != pCharacter->m_nTopHatedRace) {
        // NOTE: Uninline.
        pCharacter->SetTopHatedRace(nTopHatedRace);

        InvalidateItems();

        // NOTE: Uninline.
        UpdateScrollBar();
    }
}

// 0x5EE4F0
void CUIControlScrollBarCharacterHatedRace::OnPageDown(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14744
    UTIL_ASSERT(pCharacter != NULL);

    INT nStep = min(nLines, 11);
    INT nTopHatedRace = min(pCharacter->m_nTopHatedRace + nStep, 5);

    if (nTopHatedRace != pCharacter->m_nTopHatedRace) {
        // NOTE: Uninline.
        pCharacter->SetTopHatedRace(nTopHatedRace);

        InvalidateItems();

        // NOTE: Uninline.
        UpdateScrollBar();
    }
}

// 0x5EE5F0
void CUIControlScrollBarCharacterHatedRace::OnScroll()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14782
    UTIL_ASSERT(pCharacter != NULL);

    // NOTE: Uninline.
    pCharacter->SetTopHatedRace(5 * field_144 / field_142);

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5EE6D0
void CUIControlScrollBarCharacterHatedRace::InvalidateItems()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14814
    UTIL_ASSERT(pCharacter != NULL);

    CSingleLock renderLock(&(pCharacter->GetManager()->field_36), FALSE);
    renderLock.Lock(INFINITE);

    INT nGameSprite = g_pBaldurChitin->GetObjectGame()->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// -----------------------------------------------------------------------------

// 0x5EE820
CUIControlButtonCharacterHatedRaceSelection::CUIControlButtonCharacterHatedRaceSelection(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton3State(panel, controlInfo, LBUTTON, 0)
{
    m_nNotSelectedFrame = m_nNormalFrame;
    m_nSelectedFrame = 0;
}

// 0x5EE890
CUIControlButtonCharacterHatedRaceSelection::~CUIControlButtonCharacterHatedRaceSelection()
{
}

// 0x5EE930
void CUIControlButtonCharacterHatedRaceSelection::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 14903
    UTIL_ASSERT(pCharacter != NULL);

    INT nGameSprite = g_pBaldurChitin->GetObjectGame()->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        INT nIndex = pCharacter->m_nTopHatedRace + m_nID - 22;

        // NOTE: Uninline.
        pSprite->GetBaseStats()->m_favoredEnemies[pCharacter->m_nHatedRaceIndex] = pCharacter->GetHatedRace(nIndex);

        STRREF strHelp;
        switch (pSprite->GetBaseStats()->m_favoredEnemies[pCharacter->m_nHatedRaceIndex]) {
        case CAIOBJECTTYPE_R_OGRE:
            strHelp = 15998;
            break;
        case CAIOBJECTTYPE_R_WYVERN:
            strHelp = 37619;
            break;
        case CAIOBJECTTYPE_R_GIANT:
            strHelp = 3291;
            break;
        case CAIOBJECTTYPE_R_GOBLIN:
            strHelp = 3292;
            break;
        case CAIOBJECTTYPE_R_LIZARDMAN:
            strHelp = 3293;
            break;
        case CAIOBJECTTYPE_R_ORC:
            strHelp = 3294;
            break;
        case CAIOBJECTTYPE_R_SALAMANDER:
            strHelp = 3295;
            break;
        case CAIOBJECTTYPE_R_TROLL:
            strHelp = 3299;
            break;
        case CAIOBJECTTYPE_R_UMBERHULK:
            strHelp = 3300;
            break;
        case CAIOBJECTTYPE_R_UNDEAD:
            strHelp = 3297;
            break;
        case CAIOBJECTTYPE_R_YUANTI:
            strHelp = 3301;
            break;
        case CAIOBJECTTYPE_R_HARPY:
            strHelp = 37618;
            break;
        case CAIOBJECTTYPE_R_BUGBEAR:
            strHelp = 8032;
            break;
        case CAIOBJECTTYPE_R_HOOK_HORROR:
            strHelp = 37608;
            break;
        case CAIOBJECTTYPE_R_DRIDER:
            strHelp = 41079;
            break;
        case CAIOBJECTTYPE_R_SHAPESHIFTER:
            strHelp = 37609;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 15003
            UTIL_ASSERT(FALSE);
        }

        pCharacter->UpdateHelp(m_pPanel->m_nID, 8, strHelp);
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x5EA7D0
CUIControlButtonCharacter5EA7D0::CUIControlButtonCharacter5EA7D0(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
}

// 0x5EA820
CUIControlButtonCharacter5EA7D0::~CUIControlButtonCharacter5EA7D0()
{
}

// 0x5EA8C0
void CUIControlButtonCharacter5EA7D0::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12235
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->sub_5E4A20();
}

// -----------------------------------------------------------------------------

// 0x5EA900
CUIControlButtonCharacter5EA900::CUIControlButtonCharacter5EA900(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
}

// 0x5EA950
CUIControlButtonCharacter5EA900::~CUIControlButtonCharacter5EA900()
{
}

// 0x5EA9F0
void CUIControlButtonCharacter5EA900::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12289
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->sub_5E4B30();
}

// -----------------------------------------------------------------------------

// 0x5EAA30
CUIControlButtonCharacter5EAA30::CUIControlButtonCharacter5EAA30(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(16559, strRes);
    SetText(strRes.szText);
}

// 0x5EAB20
CUIControlButtonCharacter5EAA30::~CUIControlButtonCharacter5EAA30()
{
}

// 0x5EABC0
void CUIControlButtonCharacter5EAA30::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12346
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->sub_5E4560();
}

// -----------------------------------------------------------------------------

// 0x5EAC00
CUIControlButtonCharacterCustomize::CUIControlButtonCharacterCustomize(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(10645, strRes); // "Customize"
    SetText(strRes.szText);
}

// 0x5EACF0
CUIControlButtonCharacterCustomize::~CUIControlButtonCharacterCustomize()
{
}

// 0x5EAD90
void CUIControlButtonCharacterCustomize::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12402
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnCustomizeButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EADD0
CUIControlButtonCharacterInformation::CUIControlButtonCharacterInformation(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(11946, strRes); // "Information"
    SetText(strRes.szText);
}

// 0x5EAEC0
CUIControlButtonCharacterInformation::~CUIControlButtonCharacterInformation()
{
}

// 0x5EAF60
void CUIControlButtonCharacterInformation::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12458
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnInformationButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EAFA0
CUIControlButtonCharacterExport::CUIControlButtonCharacterExport(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(13956, strRes); // "Export"
    SetText(strRes.szText);
}

// 0x5EB090
CUIControlButtonCharacterExport::~CUIControlButtonCharacterExport()
{
}

// 0x5EB130
void CUIControlButtonCharacterExport::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12514
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnExportButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EB170
CUIControlButtonCharacterLevelUp::CUIControlButtonCharacterLevelUp(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(7175, strRes); // "Level Up"
    SetText(strRes.szText);
}

// 0x5EB260
CUIControlButtonCharacterLevelUp::~CUIControlButtonCharacterLevelUp()
{
}

// 0x5EB300
void CUIControlButtonCharacterLevelUp::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12570
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnLevelUpButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EB340
CUIControlButtonCharacterClassSelection::CUIControlButtonCharacterClassSelection(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton3State(panel, controlInfo, LBUTTON, 1)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    CString sClass;
    STR_RES strRes;

    // NOTE: Uninline.
    BYTE nClass = GetClass();

    ruleTables.GetClassStringLower(nClass, 0x4000, 0, sClass, 1);
    SetText(sClass);

    m_nSelectedFrame = 0;
    pCharacter->m_nClass = 0;
}

// 0x5EB540
CUIControlButtonCharacterClassSelection::~CUIControlButtonCharacterClassSelection()
{
}

// 0x5EB5E0
void CUIControlButtonCharacterClassSelection::OnLButtonClick(CPoint pt)
{
    CSingleLock renderLock(&(m_pPanel->m_pManager->field_36), FALSE);
    renderLock.Lock(INFINITE);

    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12641
    UTIL_ASSERT(pCharacter != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12643
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        STRREF strDescription;

        switch (m_nID) {
        case 2:
            strDescription = 37;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_BARBARIAN;
            break;
        case 3:
            strDescription = 9562;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_BARD;
            break;
        case 4:
            strDescription = 9559;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_CLERIC;
            break;
        case 5:
            strDescription = 9560;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_DRUID;
            break;
        case 6:
            strDescription = 9556;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_FIGHTER;
            break;
        case 7:
            strDescription = 36;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_MONK;
            break;
        case 8:
            strDescription = 9558;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_PALADIN;
            break;
        case 9:
            strDescription = 9557;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_RANGER;
            break;
        case 10:
            strDescription = 9561;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_ROGUE;
            break;
        case 11:
            strDescription = 35;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_SORCERER;
            break;
        case 12:
            strDescription = 9563;
            pCharacter->m_nClass = CAIOBJECTTYPE_C_WIZARD;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 12724
            UTIL_ASSERT(FALSE);

            strDescription = -1;
        }

        CAIObjectType typeAI(pSprite->m_startTypeAI);
        typeAI.AddClass(pCharacter->m_nClass);
        pSprite->SetAIType(typeAI, TRUE, TRUE);

        pCharacter->UpdateHelp(m_pPanel->m_nID, 13, strDescription);
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// NOTE: Inlined.
BYTE CUIControlButtonCharacterClassSelection::GetClass()
{
    BYTE nClass;

    switch (m_nID) {
    case 2:
        nClass = CAIOBJECTTYPE_C_BARBARIAN;
        break;
    case 3:
        nClass = CAIOBJECTTYPE_C_BARD;
        break;
    case 4:
        nClass = CAIOBJECTTYPE_C_CLERIC;
        break;
    case 5:
        nClass = CAIOBJECTTYPE_C_DRUID;
        break;
    case 6:
        nClass = CAIOBJECTTYPE_C_FIGHTER;
        break;
    case 7:
        nClass = CAIOBJECTTYPE_C_MONK;
        break;
    case 8:
        nClass = CAIOBJECTTYPE_C_PALADIN;
        break;
    case 9:
        nClass = CAIOBJECTTYPE_C_RANGER;
        break;
    case 10:
        nClass = CAIOBJECTTYPE_C_ROGUE;
        break;
    case 11:
        nClass = CAIOBJECTTYPE_C_SORCERER;
        break;
    case 12:
        nClass = CAIOBJECTTYPE_C_WIZARD;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 12808
        UTIL_ASSERT(FALSE);
    }

    return nClass;
}

// -----------------------------------------------------------------------------

// 0x5EB8C0
CUIControlButtonInformationDone::CUIControlButtonInformationDone(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(11973, strRes); // "Done"
    SetText(strRes.szText);
}

// 0x5EB9B0
CUIControlButtonInformationDone::~CUIControlButtonInformationDone()
{
}

// 0x5EBA50
void CUIControlButtonInformationDone::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12874
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnDoneButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EBA90
CUIControlButtonInformationBiography::CUIControlButtonInformationBiography(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(18003, strRes); // "Biography"
    SetText(strRes.szText);
}

// 0x5EBB80
CUIControlButtonInformationBiography::~CUIControlButtonInformationBiography()
{
}

// 0x5EBC20
void CUIControlButtonInformationBiography::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 12874
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnBiographyButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EBC60
CUIControlButtonCharacter5EBC60::CUIControlButtonCharacter5EBC60(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton3State(panel, controlInfo, LBUTTON, 0)
{
    m_nSelectedFrame = 0;
    m_spellResRef = "";
    m_iconResRef = "";
}

// 0x5EBD30
CUIControlButtonCharacter5EBC60::~CUIControlButtonCharacter5EBC60()
{
}

// FIXME: `cResRef` should be reference.
//
// 0x5EBDD0
void CUIControlButtonCharacter5EBC60::SetSpell(CResRef cResRef)
{
    CString sIconResRef;

    if (m_spellResRef != cResRef) {
        m_spellResRef = cResRef;
        m_iconResRef = "";

        if (m_spellResRef != "") {
            CSpell cSpell;
            cSpell.SetResRef(m_spellResRef, TRUE, TRUE);
            cSpell.Demand();

            if (cSpell.pRes != NULL) {
                RESREF iconResRef;
                cSpell.GetIcon(iconResRef);
                m_iconResRef = iconResRef;

                m_iconResRef.CopyToString(sIconResRef);
                sIconResRef.SetAt(sIconResRef.GetLength() - 1, 'B');
                m_iconResRef = sIconResRef;
            }

            SetToolTipStrRef(cSpell.GetGenericName(), -1, -1);

            cSpell.Release();
        } else {
            SetToolTipStrRef(-1, -1, -1);
        }
    }
}

// 0x5EBFA0
void CUIControlButtonCharacter5EBC60::OnLButtonClick(CPoint pt)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13049
    UTIL_ASSERT(pGame != NULL);

    // NOTE: Uninline.
    LONG nCharacterId = pGame->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        INT nExtraMageSpells = pCharacter->m_nExtraMageSpells;
        BYTE nClass = pCharacter->m_nClass;
        BYTE nSpellLevel = pCharacter->m_nSpellLevel;
        UINT nLevel = nSpellLevel - 1;

        CSpell cSpell;
        cSpell.SetResRef(m_spellResRef, TRUE, TRUE);
        cSpell.Demand();

        // NOTE: Unused.
        cSpell.GetDescription();

        if ((pSprite->GetSpecialization() & SPECMASK_WIZARD_UNIVERSAL) == 0) {
            if (!pSprite->IsSpellSchool(cSpell.GetSchool())) {
                g_pBaldurChitin->m_pEngineCreateChar->sub_618D10(static_cast<BYTE>(nLevel + 1), pSprite);
            }
        }

        cSpell.Release();

        INT nMaxKnownSpellsBonus = 0;
        INT nMaxKnownSpells = 1;
        if (nClass == CAIOBJECTTYPE_C_BARD || nClass == CAIOBJECTTYPE_C_SORCERER) {
            nMaxKnownSpells = pGame->GetRuleTables().GetMaxKnownSpells(nClass,
                pSprite->m_startTypeAI,
                *pSprite->GetDerivedStats(),
                pSprite->GetBaseStats()->m_specialization,
                nSpellLevel,
                nMaxKnownSpellsBonus);
            nMaxKnownSpells += nMaxKnownSpellsBonus;
        }

        UINT nClassIndex = pGame->GetSpellcasterIndex(nClass);

        UINT nID = 0;
        if (!pGame->m_spells.Find(m_spellResRef, nID)) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
            // __LINE__: 13095
            UTIL_ASSERT(FALSE);
        }

        if (m_bSelected) {
            SetSelected(FALSE);
            pCharacter->m_nExtraMageSpells = nExtraMageSpells + 1;

            // __FILE__: .\Include\ObjCreature.h
            // __LINE__: 1741
            UTIL_ASSERT(nClassIndex <= CSPELLLIST_NUM_CLASSES);

            unsigned int nZero1 = 0;
            unsigned int nZero2 = 0;
            pSprite->m_spells.Get(nClassIndex)->Remove(nID, nLevel, 1, nZero1, nZero2);
        } else {
            if (nExtraMageSpells > 0 && cSpell.pRes != NULL) {
                if (cSpell.CheckUsableBy(pSprite) == TRUE) {
                    SetSelected(TRUE);
                    pCharacter->m_nExtraMageSpells = nExtraMageSpells - 1;

                    // __FILE__: .\Include\ObjCreature.h
                    // __LINE__: 1741
                    UTIL_ASSERT(nClassIndex <= CSPELLLIST_NUM_CLASSES);

                    unsigned int nUses = nMaxKnownSpells;
                    unsigned int nZero1 = 0;
                    unsigned int nZero2 = 0;
                    pSprite->m_spells.Get(nClassIndex)->Add(nID, nLevel, nUses, nZero1, nZero2);
                }
            }
        }

        pCharacter->UpdateHelp(m_pPanel->m_nID, 30, cSpell.GetDescription());
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        pGame->GetObjectArray()->ReleaseDeny(nCharacterId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5EC420
BOOL CUIControlButtonCharacter5EBC60::Render(BOOL bForce)
{
    if (!m_bActive && !m_bInactiveRender) {
        return FALSE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return FALSE;
    }

    if (!CUIControlButton3State::Render(bForce)) {
        return FALSE;
    }

    if (m_spellResRef != "" && m_iconResRef != "") {
        CRect rControlFrame(m_pPanel->m_ptOrigin + m_ptOrigin, m_size);

        if (m_bPressed) {
            rControlFrame.OffsetRect(field_63E, field_642);
        }

        CPoint pos = rControlFrame.TopLeft();

        CRect rClip;
        rClip.IntersectRect(rControlFrame, m_rDirty);

        CIcon::RenderIcon(0,
            pos,
            m_size,
            rClip,
            m_iconResRef,
            m_pPanel->m_pManager->m_bDoubleSize,
            m_bEnabled ? 0 : 2,
            0,
            FALSE,
            0,
            FALSE,
            0);
    }

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x5EC5A0
CUIControlButtonCharacterPopupDone::CUIControlButtonCharacterPopupDone(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo, INT nType)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    if (nType == 0) {
        SetText(CBaldurEngine::FetchString(11973)); // "Done"
    } else {
        SetText(CBaldurEngine::FetchString(36789)); // "Next"
    }
}

// 0x5EC680
CUIControlButtonCharacterPopupDone::~CUIControlButtonCharacterPopupDone()
{
}

// 0x5EC720
void CUIControlButtonCharacterPopupDone::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13241
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnDoneButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EC760
CUIControlButtonCharacterPopupCancel::CUIControlButtonCharacterPopupCancel(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo, INT nType)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    if (nType == 1) {
        SetText(CBaldurEngine::FetchString(12896)); // "Back"
    } else {
        SetText(CBaldurEngine::FetchString(13727)); // "Cancel"
    }
}

// 0x5EC840
CUIControlButtonCharacterPopupCancel::~CUIControlButtonCharacterPopupCancel()
{
}

// 0x5EC8E0
void CUIControlButtonCharacterPopupCancel::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13302
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnCancelButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EC920
CUIControlButtonCharacterBiographyRevert::CUIControlButtonCharacterBiographyRevert(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    SetText(CBaldurEngine::FetchString(2240)); // "Revert"
}

// 0x5EC9F0
CUIControlButtonCharacterBiographyRevert::~CUIControlButtonCharacterBiographyRevert()
{
}

// 0x5ECA90
void CUIControlButtonCharacterBiographyRevert::OnLButtonClick(CPoint pt)
{
    if (m_bEnabled) {
        CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 13354
        UTIL_ASSERT(pCharacter != NULL);

        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 13358
        UTIL_ASSERT(pGame != NULL);

        // NOTE: Uninline.
        LONG nCharacterId = pGame->GetCharacterId(pCharacter->GetSelectedCharacter());

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            // NOTE: Unused.
            STR_RES strRes;

            CScreenCharacter::ResetBiography(pSprite);
            STRREF strBiography = pSprite->GetBaseStats()->m_biography;

            pCharacter->DismissPopup(pSprite);

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);

            CString sBiography = CBaldurEngine::FetchString(strBiography);
            BYTE nFixedPartyId = static_cast<BYTE>(g_pBaldurChitin->GetObjectGame()->GetFixedOrderCharacterPortraitNum(nCharacterId));
            g_pBaldurChitin->GetObjectGame()->ChangeBiography(nFixedPartyId, sBiography);
        }
    }
}

// -----------------------------------------------------------------------------

// 0x5ECC60
CUIControlButtonCharacterBiographyClear::CUIControlButtonCharacterBiographyClear(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    SetText(CBaldurEngine::FetchString(18622)); // "Clear"
}

// 0x5ECD40
CUIControlButtonCharacterBiographyClear::~CUIControlButtonCharacterBiographyClear()
{
}

// 0x5ECDE0
void CUIControlButtonCharacterBiographyClear::OnLButtonClick(CPoint pt)
{
    if (m_bEnabled) {
        CUIControlEditMultiLine* pEdit = static_cast<CUIControlEditMultiLine*>(m_pPanel->GetControl(4));
        pEdit->Remove();

        g_pBaldurChitin->m_pEngineCharacter->field_1B8 = TRUE;
        g_pBaldurChitin->m_pEngineCharacter->GetManager()->SetCapture(pEdit, CUIManager::KEYBOARD);
    }
}

// -----------------------------------------------------------------------------

// 0x5ECE40
CUIControlTextDisplayCharacterScriptScripts::CUIControlTextDisplayCharacterScriptScripts(CUIPanel* panel, UI_CONTROL_TEXTDISPLAY* controlInfo)
    : CUIControlTextDisplay(panel, controlInfo, TRUE)
{
    // NOTE: Uninline.
    SetNeedMouseMove();
}

// 0x621470
CUIControlTextDisplayCharacterScriptScripts::~CUIControlTextDisplayCharacterScriptScripts()
{
}

// 0x5ECE70
void CUIControlTextDisplayCharacterScriptScripts::OnItemSelected(LONG lMarker)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13512
    UTIL_ASSERT(pCharacter != NULL);

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13514
    UTIL_ASSERT(pGame != NULL);

    CSingleLock renderLock(&(pCharacter->GetManager()->field_36), FALSE);
    renderLock.Lock(INFINITE);

    INT nGameSprite = pGame->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        pCharacter->OnScriptItemSelect(lMarker);
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        pGame->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// -----------------------------------------------------------------------------

// 0x5ECFD0
CUIControlButtonCharacterError::CUIControlButtonCharacterError(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
}

// 0x5ED020
CUIControlButtonCharacterError::~CUIControlButtonCharacterError()
{
}

// 0x5ED0C0
void CUIControlButtonCharacterError::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13597
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnErrorButtonClick(m_nID);
}

// -----------------------------------------------------------------------------

// 0x5ED100
CUIControlTextDisplayCharacterExportCharacters::CUIControlTextDisplayCharacterExportCharacters(CUIPanel* panel, UI_CONTROL_TEXTDISPLAY* controlInfo)
    : CUIControlTextDisplay(panel, controlInfo, TRUE)
{
    // NOTE: Uninline.
    SetNeedMouseMove();
}

// 0x621470
CUIControlTextDisplayCharacterExportCharacters::~CUIControlTextDisplayCharacterExportCharacters()
{
}

// 0x5ED130
void CUIControlTextDisplayCharacterExportCharacters::OnItemSelected(LONG lMarker)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13651
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnCharacterItemSelect(lMarker);
}

// -----------------------------------------------------------------------------

// 0x5ED170
CUIControlEditCharacter::CUIControlEditCharacter(CUIPanel* panel, UI_CONTROL_EDIT* controlInfo)
    : CUIControlEdit(panel, controlInfo, 0)
{
}

// 0x5ED1C0
CUIControlEditCharacter::~CUIControlEditCharacter()
{
}

// 0x5ED290
void CUIControlEditCharacter::OnKeyDown(SHORT nKey)
{
    CUIControlEdit::OnKeyDown(nKey);

    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 13706
    UTIL_ASSERT(pCharacter != NULL);

    INT nGameSprite = g_pBaldurChitin->GetObjectGame()->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (m_nID == 6) {
            pCharacter->OnCharacterItemSelect(-1);
        }

        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// 0x5ED390
void CUIControlEditCharacter::OnEditReturn(CString sText)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    INT nGameSprite = g_pBaldurChitin->GetObjectGame()->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (m_nID == 6) {
            pCharacter->OnCharacterItemSelect(-1);
        }

        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x5EEBD0
CUIControlButtonCharacterCustomizeMenu::CUIControlButtonCharacterCustomizeMenu(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15039
    UTIL_ASSERT(pCharacter != NULL);

    STRREF dwText;
    switch (m_nID) {
    case 0:
        dwText = 11961; // "Appearance"
        break;
    case 1:
        dwText = 10647; // "Sounds"
        break;
    case 3:
        dwText = 17111; // "Script"
        break;
    case 9:
        dwText = 18003; // "Biography"
        break;
    default:
        dwText = -1;
        break;
    }
    SetText(CBaldurEngine::FetchString(dwText));
}

// 0x5EED00
CUIControlButtonCharacterCustomizeMenu::~CUIControlButtonCharacterCustomizeMenu()
{
}

// 0x5EEDA0
void CUIControlButtonCharacterCustomizeMenu::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15093
    UTIL_ASSERT(pCharacter != NULL);

    switch (m_nID) {
    case 0:
        pCharacter->OnAppearanceButtonClick();
        break;
    case 1:
        pCharacter->OnSoundsButtonClick();
        break;
    case 3:
        pCharacter->OnScriptButtonClick();
        break;
    case 9:
        pCharacter->OnCustomizeBiographyButtonClick();
        break;
    }
}

// -----------------------------------------------------------------------------

// 0x5EEE40
CUIControlTextDisplayCharacterPortraits::CUIControlTextDisplayCharacterPortraits(CUIPanel* panel, UI_CONTROL_TEXTDISPLAY* controlInfo)
    : CUIControlTextDisplay(panel, controlInfo, TRUE)
{
    // NOTE: Uninline.
    SetNeedMouseMove();
}

// 0x621470
CUIControlTextDisplayCharacterPortraits::~CUIControlTextDisplayCharacterPortraits()
{
}

// 0x5EEE70
void CUIControlTextDisplayCharacterPortraits::OnItemSelected(LONG lMarker)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15169
    UTIL_ASSERT(pCharacter != NULL);

    switch (m_nID) {
    case 2:
        pCharacter->OnPortraitLargeItemSelect(lMarker);
        break;
    case 3:
        pCharacter->OnPortraitSmallItemSelect(lMarker);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 15182
        UTIL_ASSERT(FALSE);
    }
}

// -----------------------------------------------------------------------------

// NOTE: Unclear why this function is separated from constructor/destructor
// pair (originally in `CUIControlButtons.cpp`, now in `CUIControlFactory.cpp`).
//
// 0x5EEEF0
void CUIControlButtonCharacterAppearanceLeft::OnLButtonClick(CPoint pt)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15211
    UTIL_ASSERT(pGame != NULL && pCharacter != NULL);

    INT nGameSprite = pGame->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        CString sPortrait;
        CResRef portraitResRef;

        pCharacter->DecCurrentPortrait(pSprite);
        sPortrait = pCharacter->GetCurrentPortrait(pSprite);

        pCharacter->m_cResPortraitSmallTemp = sPortrait + "S";
        pCharacter->m_cResPortraitLargeTemp = sPortrait + "L";

        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// NOTE: Unclear why this function is separated from constructor/destructor
// pair (originally in `CUIControlButtons.cpp`, now in `CUIControlFactory.cpp`).
//
// 0x5EF130
void CUIControlButtonCharacterAppearanceRight::OnLButtonClick(CPoint pt)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15273
    UTIL_ASSERT(pGame != NULL && pCharacter != NULL);

    INT nGameSprite = pGame->GetCharacterId(pCharacter->GetSelectedCharacter());

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        CString sPortrait;
        CResRef portraitResRef;

        pCharacter->IncCurrentPortrait(pSprite);
        sPortrait = pCharacter->GetCurrentPortrait(pSprite);

        pCharacter->m_cResPortraitSmallTemp = sPortrait + "S";
        pCharacter->m_cResPortraitLargeTemp = sPortrait + "L";

        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x5EF370
CUIControlButtonCharacterAppearanceCustom::CUIControlButtonCharacterAppearanceCustom(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
    STR_RES strRes;
    g_pBaldurChitin->GetTlkTable().Fetch(17545, strRes); // "Custom"
    SetText(strRes.szText);
}

// 0x5EF460
CUIControlButtonCharacterAppearanceCustom::~CUIControlButtonCharacterAppearanceCustom()
{
}

// 0x5EF500
void CUIControlButtonCharacterAppearanceCustom::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15363
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnCustomPortraitsButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EF540
CUIControlButtonCharacterSoundPlay::CUIControlButtonCharacterSoundPlay(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 0)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15391
    UTIL_ASSERT(pCharacter != NULL);

    SetText(CBaldurEngine::FetchString(17318));
}

// 0x5EF610
CUIControlButtonCharacterSoundPlay::~CUIControlButtonCharacterSoundPlay()
{
}

// 0x5EF6B0
void CUIControlButtonCharacterSoundPlay::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15418
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnPlayButtonClick();
}

// -----------------------------------------------------------------------------

// 0x5EF6F0
CUIControlTextDisplayCharacterSoundSounds::CUIControlTextDisplayCharacterSoundSounds(CUIPanel* panel, UI_CONTROL_TEXTDISPLAY* controlInfo)
    : CUIControlTextDisplay(panel, controlInfo, TRUE)
{
    // NOTE: Uninline.
    SetNeedMouseMove();
}

// 0x621470
CUIControlTextDisplayCharacterSoundSounds::~CUIControlTextDisplayCharacterSoundSounds()
{
}

// 0x5EF720
void CUIControlTextDisplayCharacterSoundSounds::OnItemSelected(LONG lMarker)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 15472
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->OnSoundItemSelect(lMarker);
}

// -----------------------------------------------------------------------------

// NOTE: Unclear why this function is separated from constructor/destructor
// pair (originally in `CUIControlButtons.cpp`, now in `CUIControlFactory.cpp`).
//
// 0x5F6830
void CUIControlButtonCharacterFeatsHotArea::OnHotAreaClick(CPoint pt)
{
    DWORD offset;
    switch (m_nID) {
    case 93:
        offset = 0;
        break;
    case 94:
        offset = 1;
        break;
    case 95:
        offset = 2;
        break;
    case 96:
        offset = 3;
        break;
    case 97:
        offset = 4;
        break;
    case 98:
        offset = 5;
        break;
    case 99:
        offset = 6;
        break;
    case 100:
        offset = 7;
        break;
    case 101:
        offset = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17020
        UTIL_ASSERT(FALSE);

        offset = -1;
    }

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    DWORD id = ruleTables.GetFeatId(offset + g_pBaldurChitin->m_pEngineCharacter->m_nTopFeat);
    STRREF strDescription = ruleTables.GetFeatDescription(id);
    g_pBaldurChitin->m_pEngineCharacter->UpdateHelp(m_pPanel->m_nID, 92, strDescription);
}

// -----------------------------------------------------------------------------

// 0x5F6910
CUIControlScrollBarCharacterFeats::CUIControlScrollBarCharacterFeats(CUIPanel* panel, UI_CONTROL_SCROLLBAR* controlInfo)
    : CUIControlScrollBar(panel, controlInfo)
{
}

// 0x632C00
CUIControlScrollBarCharacterFeats::~CUIControlScrollBarCharacterFeats()
{
}

// NOTE: Inlined.
void CUIControlScrollBarCharacterFeats::UpdateScrollBar()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17070
    UTIL_ASSERT(pCharacter != NULL);

    AdjustScrollBar(pCharacter->m_nTopFeat, FEAT_COUNT, FEAT_SLOTS);
}

// 0x5F6930
void CUIControlScrollBarCharacterFeats::OnScrollUp()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17095
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->m_nTopFeat = max(pCharacter->m_nTopFeat - 1, 0);

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F69D0
void CUIControlScrollBarCharacterFeats::OnScrollDown()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17119
    UTIL_ASSERT(pCharacter != NULL);

    if (pCharacter->m_nTopFeat < FEAT_COUNT - FEAT_SLOTS) {
        pCharacter->m_nTopFeat++;
    }

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F6A80
void CUIControlScrollBarCharacterFeats::OnPageUp(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17148
    UTIL_ASSERT(pCharacter != NULL);

    INT nStep = min(nLines, FEAT_SLOTS - 1);
    pCharacter->m_nTopFeat = max(pCharacter->m_nTopFeat - nStep, 0);

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F6B30
void CUIControlScrollBarCharacterFeats::OnPageDown(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17176
    UTIL_ASSERT(pCharacter != NULL);

    INT nStep = min(nLines, FEAT_SLOTS - 1);
    pCharacter->m_nTopFeat += nStep;

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F6BD0
void CUIControlScrollBarCharacterFeats::OnScroll()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17204
    UTIL_ASSERT(pCharacter != NULL);

    INT nTopFeat = max(FEAT_COUNT * field_144, 0) / field_142;
    pCharacter->m_nTopFeat = max(min(nTopFeat - FEAT_SLOTS, FEAT_COUNT - FEAT_SLOTS), 0);

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F6CA0
void CUIControlScrollBarCharacterFeats::InvalidateItems()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 17235
    UTIL_ASSERT(pCharacter != NULL);

    CSingleLock renderLock(&(pCharacter->GetManager()->field_36), FALSE);
    renderLock.Lock();

    INT nGameSprite = pCharacter->field_1840;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// -----------------------------------------------------------------------------

// 0x5F6DC0
CUIControlButtonCharacterFeatsPlusMinus::CUIControlButtonCharacterFeatsPlusMinus(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButtonPlusMinus(panel, controlInfo)
{
}

// 0x5F6E00
CUIControlButtonCharacterFeatsPlusMinus::~CUIControlButtonCharacterFeatsPlusMinus()
{
}

// 0x5F6EA0
BOOL CUIControlButtonCharacterFeatsPlusMinus::OnLButtonDown(CPoint pt)
{
    DWORD offset;
    switch (m_nID) {
    case 14:
    case 15:
        offset = 0;
        break;
    case 16:
    case 17:
        offset = 1;
        break;
    case 18:
    case 19:
        offset = 2;
        break;
    case 20:
    case 21:
        offset = 3;
        break;
    case 22:
    case 23:
        offset = 4;
        break;
    case 24:
    case 25:
        offset = 5;
        break;
    case 26:
    case 27:
        offset = 6;
        break;
    case 28:
    case 29:
        offset = 7;
        break;
    case 30:
    case 31:
        offset = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17366
        UTIL_ASSERT(FALSE);
        offset = -1;
    }

    if (!m_bActive) {
        return FALSE;
    }

    if ((m_nMouseButtons & LBUTTON) == 0) {
        return FALSE;
    }

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    DWORD id = ruleTables.GetFeatId(g_pBaldurChitin->m_pEngineCharacter->m_nTopFeat + offset);
    STRREF strDescription = ruleTables.GetFeatDescription(id);
    g_pBaldurChitin->m_pEngineCharacter->UpdateHelp(m_pPanel->m_nID, 92, strDescription);

    return CUIControlButtonPlusMinus::OnLButtonDown(pt);
}

// 0x5F6FD0
void CUIControlButtonCharacterFeatsPlusMinus::AdjustValue()
{
    BOOL bInc;
    switch (m_nID) {
    case 14:
    case 16:
    case 18:
    case 20:
    case 22:
    case 24:
    case 26:
    case 28:
    case 30:
        bInc = TRUE;
        break;
    case 15:
    case 17:
    case 19:
    case 21:
    case 23:
    case 25:
    case 27:
    case 29:
    case 31:
        bInc = FALSE;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17444
        UTIL_ASSERT(FALSE);
    }

    DWORD offset;
    switch (m_nID) {
    case 14:
    case 15:
        offset = 0;
        break;
    case 16:
    case 17:
        offset = 1;
        break;
    case 18:
    case 19:
        offset = 2;
        break;
    case 20:
    case 21:
        offset = 3;
        break;
    case 22:
    case 23:
        offset = 4;
        break;
    case 24:
    case 25:
        offset = 5;
        break;
    case 26:
    case 27:
        offset = 6;
        break;
    case 28:
    case 29:
        offset = 7;
        break;
    case 30:
    case 31:
        offset = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17497
        UTIL_ASSERT(FALSE);
    }

    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    INT nGameSprite = pCharacter->field_1840;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        DWORD id = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetFeatId(pCharacter->m_nTopFeat + offset);
        INT nValue = pSprite->GetFeatValue(id);

        if (bInc) {
            if (pSprite->sub_763A40(id, 1) && pCharacter->m_nExtraFeats > 0) {
                pSprite->SetFeatValue(id, nValue + 1);
                pCharacter->m_nExtraFeats--;
            }
        } else {
            if (nValue > 0 && pCharacter->m_storedFeats[id] < pSprite->GetFeatValue(id)) {
                pSprite->SetFeatValue(id, nValue - 1);
                pCharacter->m_nExtraFeats++;
                pCharacter->sub_5F89B0(pSprite);
            }
        }

        pCharacter->UpdatePopupPanel(pCharacter->GetTopPopup()->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// 0x5F7250
CUIControlButtonCharacterFeatsCircle::CUIControlButtonCharacterFeatsCircle(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton(panel, controlInfo, LBUTTON, 1)
{
}

// 0x5F72A0
CUIControlButtonCharacterFeatsCircle::~CUIControlButtonCharacterFeatsCircle()
{
}

// 0x5F7340
BOOL CUIControlButtonCharacterFeatsCircle::Render(BOOL bForce)
{
    CVidCell vcIcon;

    if (!m_bActive && !m_bInactiveRender) {
        return FALSE;
    }

    if (m_nRenderCount == 0 && !bForce) {
        return FALSE;
    }

    if (m_nRenderCount != 0) {
        CSingleLock lock(&(m_pPanel->m_pManager->field_56), FALSE);
        lock.Lock(INFINITE);
        m_nRenderCount--;
        lock.Unlock();
    }

    INT nFirstID;
    INT nIndex;
    switch (m_nID) {
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
        nFirstID = 36;
        nIndex = 0;
        break;
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
        nFirstID = 41;
        nIndex = 1;
        break;
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
        nFirstID = 46;
        nIndex = 2;
        break;
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
        nFirstID = 51;
        nIndex = 3;
        break;
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
        nFirstID = 56;
        nIndex = 4;
        break;
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
        nFirstID = 61;
        nIndex = 5;
        break;
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
        nFirstID = 66;
        nIndex = 6;
        break;
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
        nFirstID = 71;
        nIndex = 7;
        break;
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
        nFirstID = 76;
        nIndex = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17714
        UTIL_ASSERT(FALSE);
    }

    CScreenCreateChar* pCreateChar = g_pBaldurChitin->m_pEngineCreateChar;

    INT nGameSprite = pCreateChar->GetSpriteId();

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nGameSprite,
            CGameObjectArray::THREAD_1,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc != CGameObjectArray::SUCCESS) {
        return FALSE;
    }

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    DWORD v1 = ruleTables.GetFeatId(nIndex + pCreateChar->m_nTopFeat);
    INT nFeatNumber = ruleTables.GetFeatId(v1);
    INT nValue = pSprite->GetFeatValue(nFeatNumber);
    INT nMaxValue = pSprite->GetMaxFeatValue(nFeatNumber);
    INT nKnobValue = nFirstID - m_nID + 4;

    if ((nValue > 0 || pSprite->sub_763A40(nFeatNumber, 1))
        && nKnobValue < nMaxValue) {
        CRect rControlRect(m_pPanel->m_ptOrigin + m_ptOrigin,
            m_size);

        CRect rClip;
        rClip.IntersectRect(rControlRect, m_rDirty);

        // NOTE: Uninline.
        vcIcon.SetResRef(CResRef("GUIPFC"), m_pPanel->m_pManager->m_bDoubleSize, TRUE, TRUE);

        if (nKnobValue >= nValue) {
            vcIcon.FrameSet(1);
        } else {
            vcIcon.FrameSet(0);
        }

        BOOL bResult = vcIcon.Render(0,
            rControlRect.left,
            rControlRect.top,
            rClip,
            NULL,
            0,
            0,
            -1);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 17756
        UTIL_ASSERT(bResult);
    }

    pCreateChar->UpdatePopupPanel(pCreateChar->GetTopPopup()->m_nID, pSprite);

    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nGameSprite,
        CGameObjectArray::THREAD_1,
        INFINITE);

    return TRUE;
}

// -----------------------------------------------------------------------------

// 0x5F7F90
CUIControlScrollBarCharacterSkills::CUIControlScrollBarCharacterSkills(CUIPanel* panel, UI_CONTROL_SCROLLBAR* controlInfo)
    : CUIControlScrollBar(panel, controlInfo)
{
}

// 0x632C00
CUIControlScrollBarCharacterSkills::~CUIControlScrollBarCharacterSkills()
{
}

// NOTE: Inlined.
void CUIControlScrollBarCharacterSkills::UpdateScrollBar()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18162
    UTIL_ASSERT(pCharacter != NULL);

    AdjustScrollBar(pCharacter->m_nTopSkill, SKILL_COUNT, SKILL_SLOTS);
}

// 0x5F7FB0
void CUIControlScrollBarCharacterSkills::OnScrollUp()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18187
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->m_nTopSkill = max(pCharacter->m_nTopSkill - 1, 0);

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F8050
void CUIControlScrollBarCharacterSkills::OnScrollDown()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18211
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->m_nTopSkill++;

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F80E0
void CUIControlScrollBarCharacterSkills::OnPageUp(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18235
    UTIL_ASSERT(pCharacter != NULL);

    INT nStep = min(nLines, SKILL_SLOTS - 1);
    pCharacter->m_nTopSkill = max(pCharacter->m_nTopSkill - nStep, 0);

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F8190
void CUIControlScrollBarCharacterSkills::OnPageDown(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 11362
    UTIL_ASSERT(pCharacter != NULL);

    INT nStep = min(nLines, SKILL_SLOTS - 1);
    pCharacter->m_nTopSkill += nStep;

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F8230
void CUIControlScrollBarCharacterSkills::OnScroll()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18291
    UTIL_ASSERT(pCharacter != NULL);

    pCharacter->m_nTopSkill = max(SKILL_COUNT * field_144, 0) / field_142;

    InvalidateItems();

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F82E0
void CUIControlScrollBarCharacterSkills::InvalidateItems()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18318
    UTIL_ASSERT(pCharacter != NULL);

    CSingleLock renderLock(&(pCharacter->GetManager()->field_36), FALSE);
    renderLock.Lock();

    INT nGameSprite = pCharacter->field_1840;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        pCharacter->UpdatePopupPanel(m_pPanel->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    renderLock.Unlock();
}

// -----------------------------------------------------------------------------

// 0x5F8400
CUIControlButtonCharacterSkillsPlusMinus::CUIControlButtonCharacterSkillsPlusMinus(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButtonPlusMinus(panel, controlInfo)
{
}

// 0x5F8440
CUIControlButtonCharacterSkillsPlusMinus::~CUIControlButtonCharacterSkillsPlusMinus()
{
}

// 0x5F84E0
BOOL CUIControlButtonCharacterSkillsPlusMinus::OnLButtonDown(CPoint pt)
{
    DWORD offset;
    switch (m_nID) {
    case 14:
    case 15:
        offset = 0;
        break;
    case 16:
    case 17:
        offset = 1;
        break;
    case 18:
    case 19:
        offset = 2;
        break;
    case 20:
    case 21:
        offset = 3;
        break;
    case 22:
    case 23:
        offset = 4;
        break;
    case 24:
    case 25:
        offset = 5;
        break;
    case 26:
    case 27:
        offset = 6;
        break;
    case 28:
    case 29:
        offset = 7;
        break;
    case 30:
    case 31:
        offset = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18449
        UTIL_ASSERT(FALSE);
        offset = -1;
    }

    if (!m_bActive) {
        return FALSE;
    }

    if ((m_nMouseButtons & LBUTTON) == 0) {
        return FALSE;
    }

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    DWORD id = ruleTables.GetSkillId(g_pBaldurChitin->m_pEngineCharacter->m_nTopSkill + offset);
    STRREF strDescription = ruleTables.GetSkillDescription(id);
    g_pBaldurChitin->m_pEngineCharacter->UpdateHelp(m_pPanel->m_nID, 92, strDescription);

    return CUIControlButtonPlusMinus::OnLButtonDown(pt);
}

// 0x5F8610
void CUIControlButtonCharacterSkillsPlusMinus::AdjustValue()
{
    BOOL bInc;
    switch (m_nID) {
    case 14:
    case 16:
    case 18:
    case 20:
    case 22:
    case 24:
    case 26:
    case 28:
    case 30:
        bInc = TRUE;
        break;
    case 15:
    case 17:
    case 19:
    case 21:
    case 23:
    case 25:
    case 27:
    case 29:
    case 31:
        bInc = FALSE;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18527
        UTIL_ASSERT(FALSE);
    }

    DWORD offset;
    switch (m_nID) {
    case 14:
    case 15:
        offset = 0;
        break;
    case 16:
    case 17:
        offset = 1;
        break;
    case 18:
    case 19:
        offset = 2;
        break;
    case 20:
    case 21:
        offset = 3;
        break;
    case 22:
    case 23:
        offset = 4;
        break;
    case 24:
    case 25:
        offset = 5;
        break;
    case 26:
    case 27:
        offset = 6;
        break;
    case 28:
    case 29:
        offset = 7;
        break;
    case 30:
    case 31:
        offset = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18580
        UTIL_ASSERT(FALSE);
    }

    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    INT nGameSprite = pCharacter->field_1840;

    CGameSprite* pSprite;
    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

        DWORD id = ruleTables.GetSkillId(pCharacter->m_nTopSkill + offset);
        INT nValue = pSprite->GetSkillValue(id);

        // FIXME: Looks wrong (obtaining id from id).
        INT nCost = pSprite->GetSkillCost(ruleTables.GetSkillId(id),
            pCharacter->m_nClass);

        if (bInc) {
            if (nCost != 0 && pCharacter->m_nExtraSkillPoints >= nCost) {
                pSprite->SetSkillValue(id, nValue + 1);
                pCharacter->m_nExtraSkillPoints -= nCost;
            }
        } else {
            if (nValue > 0 && pCharacter->m_storedSkills[id] < pSprite->GetSkillValue(id)) {
                pSprite->SetSkillValue(id, nValue - 1);

                // FIXME: Looks wrong (obtaining id from id).
                pCharacter->m_nExtraSkillPoints += pSprite->GetSkillCost(ruleTables.GetSkillId(id),
                    pCharacter->m_nClass);
            }
        }

        pCharacter->UpdatePopupPanel(pCharacter->GetTopPopup()->m_nID, pSprite);

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nGameSprite,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }
}

// -----------------------------------------------------------------------------

// NOTE: Unclear why this function is separated from constructor/destructor
// pair (originally in `CUIControlButtons.cpp`, now in `CUIControlFactory.cpp`).
//
// 0x5F88D0
void CUIControlButtonCharacterSkillsHotArea::OnHotAreaClick(CPoint pt)
{
    DWORD offset;
    switch (m_nID) {
    case 93:
        offset = 0;
        break;
    case 94:
        offset = 1;
        break;
    case 95:
        offset = 2;
        break;
    case 96:
        offset = 3;
        break;
    case 97:
        offset = 4;
        break;
    case 98:
        offset = 5;
        break;
    case 99:
        offset = 6;
        break;
    case 100:
        offset = 7;
        break;
    case 101:
        offset = 8;
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18703
        UTIL_ASSERT(FALSE);

        offset = -1;
    }

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    DWORD id = ruleTables.GetSkillId(offset + g_pBaldurChitin->m_pEngineCharacter->m_nTopSkill);
    STRREF strDescription = ruleTables.GetSkillDescription(id);
    g_pBaldurChitin->m_pEngineCharacter->UpdateHelp(m_pPanel->m_nID, 92, strDescription);
}

// -----------------------------------------------------------------------------

// 0x5F8C10
void CUIControlCharacterHotArea778ED0::OnHotAreaClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18958
    UTIL_ASSERT(pCharacter != NULL);

    if (m_nID >= 27 && m_nID <= 37) {
        pCharacter->sub_5F8C90(m_nID - 27);
        pCharacter->UpdatePopupPanel(57, NULL);
        return;
    }

    if (m_nID >= 71 && m_nID <= 81) {
        pCharacter->sub_5F91F0(pCharacter->field_1852 + m_nID - 71);
    }

    pCharacter->UpdatePopupPanel(57, NULL);
}

// -----------------------------------------------------------------------------

// 0x5F8A50
CUIControlButtonCharacterInformationFolder::CUIControlButtonCharacterInformationFolder(CUIPanel* panel, UI_CONTROL_BUTTON* controlInfo)
    : CUIControlButton3State(panel, controlInfo, LBUTTON, 1)
{
    m_nSelectedFrame = 0;
}

// 0x5F8AB0
CUIControlButtonCharacterInformationFolder::~CUIControlButtonCharacterInformationFolder()
{
}

// 0x5F8B50
void CUIControlButtonCharacterInformationFolder::OnLButtonClick(CPoint pt)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 18906
    UTIL_ASSERT(pCharacter != NULL);

    CUIPanel* pPanel;
    CUIControlTextDisplay* pText;

    switch (m_nID) {
    case 60:
    case 61:
    case 62:
    case 63:
        pCharacter->field_1844 = static_cast<BYTE>(m_nID) - 60;

        pPanel = pCharacter->GetManager()->GetPanel(2);
        pText = static_cast<CUIControlTextDisplay*>(pPanel->GetControl(45));

        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18921
        UTIL_ASSERT(pText != NULL);

        pText->SetTopString(pText->m_plstStrings->FindIndex(0));
        pCharacter->UpdateMainPanel(FALSE);
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
        // __LINE__: 18932
        UTIL_ASSERT(FALSE);
    }
}

// -----------------------------------------------------------------------------

// 0x5F9410
CUIControlScrollbarCharacter5F9410::CUIControlScrollbarCharacter5F9410(CUIPanel* panel, UI_CONTROL_SCROLLBAR* controlInfo)
    : CUIControlScrollBar(panel, controlInfo)
{
}

// 0x632C00
CUIControlScrollbarCharacter5F9410::~CUIControlScrollbarCharacter5F9410()
{
}

// NOTE: Inlined.
void CUIControlScrollbarCharacter5F9410::UpdateScrollBar()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 19222
    UTIL_ASSERT(pCharacter);

    AdjustScrollBar(pCharacter->field_1852, pCharacter->field_184E, 11);
}

// 0x5F9430
void CUIControlScrollbarCharacter5F9410::OnScrollUp()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 19250
    UTIL_ASSERT(pCharacter);

    pCharacter->field_1852 = max(pCharacter->field_1852 - 1, 0);

    pCharacter->UpdatePopupPanel(57, NULL);

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F94C0
void CUIControlScrollbarCharacter5F9410::OnScrollDown()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 19277
    UTIL_ASSERT(pCharacter);

    if (pCharacter->field_1852 < pCharacter->field_184E - 11) {
        // NOTE: Original code is slightly different.
        pCharacter->field_1852 = min(pCharacter->field_1852 + 1, pCharacter->field_184E - 11);
    }

    pCharacter->UpdatePopupPanel(57, NULL);

    // NOTE: Uninline.
    UpdateScrollBar();
}

// 0x5F9580
void CUIControlScrollbarCharacter5F9410::OnPageUp(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 19310
    UTIL_ASSERT(pCharacter);

    if (pCharacter->field_184E > 11) {
        if (nLines >= 11) {
            nLines = 10;
        } else if (nLines == 0) {
            return;
        }

        pCharacter->field_1852 = max(pCharacter->field_1852 - static_cast<INT>(nLines & 0xFFFF), 0);

        pCharacter->UpdatePopupPanel(57, NULL);

        // NOTE: Uninline.
        UpdateScrollBar();
    }
}

// 0x5F9640
void CUIControlScrollbarCharacter5F9410::OnPageDown(DWORD nLines)
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 19353
    UTIL_ASSERT(pCharacter);

    if (pCharacter->field_184E > 11) {
        if (nLines >= 11) {
            nLines = 10;
        } else if (nLines == 0) {
            return;
        }

        INT nStep = min(static_cast<INT>(nLines & 0xFFFF), 10);
        INT nTop = pCharacter->field_1852 + nStep;
        if (nTop > pCharacter->field_184E - 11) {
            nStep += pCharacter->field_184E - 11 - nTop;
        }
        pCharacter->field_1852 = max(pCharacter->field_1852 + nStep, 0);

        pCharacter->UpdatePopupPanel(57, NULL);

        // NOTE: Uninline.
        UpdateScrollBar();
    }
}

// 0x5F9720
void CUIControlScrollbarCharacter5F9410::OnScroll()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\InfScreenCharacter.cpp
    // __LINE__: 19396
    UTIL_ASSERT(pCharacter);

    INT nMaxTop = pCharacter->field_184E - 11;
    INT nTop = field_144 * max(nMaxTop, 0) / field_142;
    if (min(nTop, nMaxTop) < 0) {
        nTop = 0;
    } else if (nTop >= nMaxTop) {
        nTop = nMaxTop;
    }
    pCharacter->field_1852 = nTop;

    if (pCharacter->field_184E > 11) {
        pCharacter->UpdatePopupPanel(57, NULL);
    }

    // NOTE: Uninline.
    UpdateScrollBar();
}
