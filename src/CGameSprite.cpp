#include "CGameSprite.h"

#include "CAIScript.h"
#include "CAIUtil.h"
#include "CBaldurChitin.h"
#include "CBaldurEngine.h"
#include "CBlood.h"
#include "CGameAnimationType.h"
#include "CGameArea.h"
#include "CGameButtonList.h"
#include "CGameContainer.h"
#include "CGameDoor.h"
#include "CGameTimer.h"
#include "CInfCursor.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CObjectMarker.h"
#include "CPathSearch.h"
#include "CProjectile.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenMap.h"
#include "CScreenWorld.h"
#include "CSpawn.h"
#include "CSpell.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CVariableHash.h"
#include "CVidInf.h"
#include "Icewind586B70.h"
#include "IcewindCGameEffects.h"
#include "IcewindCVisualEffect.h"
#include "IcewindMisc.h"
#include "marshal_ptr.h"
#include "msvc_atol.h"

// 0x8D86B0
//
// NOTE: The empty colour range CItem.cpp also passes (its own file-static
// copy of the same zeroed global).
static BYTE byte_8D86B0[7];

// 0x85BB38
const BYTE CGameSprite::DIR_S = 0;

// 0x85BB39
const BYTE CGameSprite::DIR_SW = 2;

// 0x85BB3A
const BYTE CGameSprite::DIR_W = 4;

// 0x85BB3B
const BYTE CGameSprite::DIR_NW = 6;

// 0x85BB3C
const BYTE CGameSprite::DIR_N = 8;

// 0x85BB3D
const BYTE CGameSprite::DIR_NE = 10;

// 0x85BB3E
const BYTE CGameSprite::DIR_E = 12;

// 0x85BB3F
const BYTE CGameSprite::DIR_SE = 14;

// 0x85BB40
const BYTE CGameSprite::DIR_SSW = 1;

// 0x85BB41
const BYTE CGameSprite::DIR_SWW = 3;

// 0x85BB42
const BYTE CGameSprite::DIR_NWW = 5;

// 0x85BB43
const BYTE CGameSprite::DIR_NNW = 7;

// 0x85BB44
const BYTE CGameSprite::DIR_NNE = 9;

// 0x85BB45
const BYTE CGameSprite::DIR_NEE = 11;

// 0x85BB46
const BYTE CGameSprite::DIR_SEE = 13;

// 0x85BB47
const BYTE CGameSprite::DIR_SSE = 15;

// 0x85BB48
const BYTE CGameSprite::NUM_DIR = 16;

// 0x85BB6A
const BYTE CGameSprite::SOUND_DYING = 7;

// 0x85BB6C
const BYTE CGameSprite::SOUND_SELECT = 9;

// 0x85BB6E
const BYTE CGameSprite::SOUND_SELECT_ACTION = 11;

// 0x85BB70
const BYTE CGameSprite::SOUND_REACT_TO_DEATH = 13;

// 0x85BB74
const SHORT CGameSprite::EXACT_SCALE = 10;

// 0x85BB9A
const SHORT CGameSprite::READY_COUNT = 75;

// 0x85BB9C
const SHORT CGameSprite::USER_OVERRIDE_COUNT = 75;

// 0x85BBB0
const BYTE CGameSprite::SEQ_ATTACK = CGAMESPRITE_SEQ_ATTACK;

// 0x85BBB1
const BYTE CGameSprite::SEQ_AWAKE = CGAMESPRITE_SEQ_AWAKE;

// 0x85BBB2
const BYTE CGameSprite::SEQ_CAST = CGAMESPRITE_SEQ_CAST;

// 0x85BBB3
const BYTE CGameSprite::SEQ_CONJURE = CGAMESPRITE_SEQ_CONJURE;

// 0x85BBB4
const BYTE CGameSprite::SEQ_DAMAGE = CGAMESPRITE_SEQ_DAMAGE;

// 0x85BBB5
const BYTE CGameSprite::SEQ_DIE = CGAMESPRITE_SEQ_DIE;

// 0x85BBB6
const BYTE CGameSprite::SEQ_EMERGE = CGAMESPRITE_SEQ_EMERGE;

// 0x85BBB7
const BYTE CGameSprite::SEQ_HEAD_TURN = CGAMESPRITE_SEQ_HEAD_TURN;

// 0x85BBB8
const BYTE CGameSprite::SEQ_HIDE = CGAMESPRITE_SEQ_HIDE;

// 0x85BBB9
const BYTE CGameSprite::SEQ_READY = CGAMESPRITE_SEQ_READY;

// 0x85BBBA
const BYTE CGameSprite::SEQ_SHOOT = CGAMESPRITE_SEQ_SHOOT;

// 0x85BBBB
const BYTE CGameSprite::SEQ_SLEEP = CGAMESPRITE_SEQ_SLEEP;

// 0x85BBBC
const BYTE CGameSprite::SEQ_TWITCH = CGAMESPRITE_SEQ_TWITCH;

// 0x85BBBD
const BYTE CGameSprite::SEQ_WALK = CGAMESPRITE_SEQ_WALK;

// 0x85BBBE
const BYTE CGameSprite::SEQ_ATTACK_SLASH = CGAMESPRITE_SEQ_ATTACK_SLASH;

// 0x85BBBF
const BYTE CGameSprite::SEQ_ATTACK_BACKSLASH = CGAMESPRITE_SEQ_ATTACK_BACKSLASH;

// 0x85BBC0
const BYTE CGameSprite::SEQ_ATTACK_JAB = CGAMESPRITE_SEQ_ATTACK_JAB;

// 0x85BBD0
const ITEM_ABILITY CGameSprite::DEFAULT_ATTACK = {
    1, // type
    1, // quickSlotType
    0, // largeDamageDice
    { 0 }, // quickSlotIcon
    1, // actionType
    1, // actionCount
    1, // range
    0, // launcherType
    0, // largeDamageDiceCount
    0, // speedFactor
    0, // largeDamageDiceBonus
    0, // thac0Bonus
    2, // damageDice
    9, // school
    1, // damageDiceCount
    0, // secondaryType
    0, // damageDiceBonus
    2, // damageType
    0, // effectCount
    0, // startingEffect
    WORD_MAX, // maxUsageCount
    0, // usageFlags
    0, // abilityFlags
    0, // missileType
    { 0 }, // attackProbability
};

// 0x85BCA0
const LONG CGameSprite::STANDARD_VERBAL_CONSTANT_LENGTH = 300;

// 0x85C09C
const SHORT CGameSprite::FLY_RIGHT = -1;

// 0x85C09E
const SHORT CGameSprite::FLY_LEFT = 1;

// 0x85C0A0
const SHORT CGameSprite::FLY_RADIUS_RANDOM = 0x380;

// 0x85C0A2
const SHORT CGameSprite::FLY_RADIUS_BASE = 0x80;

// 0x85C0A4
const SHORT CGameSprite::FLY_0 = 0;

// 0x85C0A6
const SHORT CGameSprite::FLY_90 = 1;

// 0x85C0A8
const SHORT CGameSprite::FLY_180 = 2;

// 0x85C0AA
const SHORT CGameSprite::FLY_270 = 3;

// 0x85C0AC
const SHORT CGameSprite::FLY_360 = 4;

// 0x85C0B4
const WORD CGameSprite::FEEDBACK_BACKSTAB = 1;

// 0x85C0B6
const WORD CGameSprite::FEEDBACK_CRITICALHIT = 2;

// 0x85C0B8
const WORD CGameSprite::FEEDBACK_CRITICALMISS = 3;

// 0x85C0BA
const WORD CGameSprite::FEEDBACK_TOHIT = 4;

// 0x85C0BC
const WORD CGameSprite::FEEDBACK_SPELL = 5;

// 0x85C0BE
const WORD CGameSprite::FEEDBACK_ATTACKS = 6;

// 0x85C0C0
const WORD CGameSprite::FEEDBACK_MODAL = 7;

// 0x85C0C2
const WORD CGameSprite::FEEDBACK_LOCKPICKSUCCEEDED = 8;

// 0x85C0C4
const WORD CGameSprite::FEEDBACK_LOCKPICKFAILED = 9;

// 0x85C0C6
const WORD CGameSprite::FEEDBACK_TRAPDISARMED = 10;

// 0x85C0C8
const WORD CGameSprite::FEEDBACK_TRAPDETECTED = 11;

// 0x85C0CA
const WORD CGameSprite::FEEDBACK_ITEMCURSED = 12;

// 0x85C0CC
const WORD CGameSprite::FEEDBACK_HIDEFAILED = 13;

// 0x85C0CE
const WORD CGameSprite::FEEDBACK_TOOHEAVY_SLOWED = 14;

// 0x85C0D0
const WORD CGameSprite::FEEDBACK_TOOHEAVY_STOPPED = 15;

// 0x85C0D2
const WORD CGameSprite::FEEDBACK_16 = 16;

// 0x85C0D4
const WORD CGameSprite::FEEDBACK_HIDESUCCEEDED = 17;

// 0x85C0D6
const WORD CGameSprite::FEEDBACK_MORALEFAILURE_PANIC = 18;

// 0x85C0D8
const WORD CGameSprite::FEEDBACK_MORALEFAILURE_RUN = 19;

// 0x85C0DA
const WORD CGameSprite::FEEDBACK_MORALEFAILURE_BERSERK = 20;

// 0x85C0DC
const WORD CGameSprite::FEEDBACK_LEVELUP = 21;

// 0x85C0DE
const WORD CGameSprite::FEEDBACK_SPELLFAILED_INDOORS = 22;

// 0x85C0E0
const WORD CGameSprite::FEEDBACK_LEAVING_SHADOWS = 23;

// 0x85C0E2
const WORD CGameSprite::FEEDBACK_BASHDOORSUCCEEDED = 24;

// 0x85C0E4
const WORD CGameSprite::FEEDBACK_BASHDOORFAILED = 25;

// 0x85C0E6
const WORD CGameSprite::FEEDBACK_BASHCHESTSUCCEEDED = 26;

// 0x85C0E8
const WORD CGameSprite::FEEDBACK_BASHCHESTFAILED = 27;

// 0x85C0EA
const WORD CGameSprite::FEEDBACK_BACKSTAB_UNUSABLE_WEAPON = 28;

// 0x85C0EC
const WORD CGameSprite::FEEDBACK_29 = 29;

// 0x85C0EE
const WORD CGameSprite::FEEDBACK_PICKPOCKETFAILED_ARMOR = 30;

// 0x85C0F0
const WORD CGameSprite::FEEDBACK_PICKPOCKETFAILED_HOSTILE = 31;

// 0x85C0F2
const WORD CGameSprite::FEEDBACK_PICKPOCKETFAILED_SKILL = 32;

// 0x85C0F4
const WORD CGameSprite::FEEDBACK_PICKPOCKETFAILED_NOITEMS = 33;

// 0x85C0F6
const WORD CGameSprite::FEEDBACK_PICKPOCKETFAILED_INVENTORYFULL = 34;

// 0x85C0F8
const WORD CGameSprite::FEEDBACK_PICKPOCKETSUCCEEDED = 35;

// 0x85C0FA
const WORD CGameSprite::FEEDBACK_GAINEDSPECIALABILITY = 36;

// 0x85C0FC
const WORD CGameSprite::FEEDBACK_IMMUNE_TO_WEAPON = 37;

// 0x85C0FE
const WORD CGameSprite::FEEDBACK_CHARACTER_CONTROL_SEIZURE = 38;

// 0x85C100
const WORD CGameSprite::FEEDBACK_TRAPDISARMEDFAILED = 39;

// 0x85C102
const WORD CGameSprite::FEEDBACK_CRITICALHITAVOIDED = 40;

// 0x85C104
const WORD CGameSprite::FEEDBACK_AURA_CLEANSED = 41;

// 0x85C106
const WORD CGameSprite::FEEDBACK_ITEM_DROPPED_FULL = 48;

// 0x85C108
const WORD CGameSprite::FEEDBACK_LOCK_UNPICKABLE = 49;

// 0x85C10A
const WORD CGameSprite::FEEDBACK_TOOMANYSUMMONED = 50;

// 0x85C10C
const WORD CGameSprite::FEEDBACK_SILENCED = 51;

// 0x85C10E
const WORD CGameSprite::FEEDBACK_CRIPPLINGSTRIKESUCCEEDED = 52;

// 0x85C110
const WORD CGameSprite::FEEDBACK_SNEAK_UNUSABLE_WEAPON = 53;

// 0x85C112
const WORD CGameSprite::FEEDBACK_EFFECTSEVADED = 54;

// 0x85C114
const WORD CGameSprite::FEEDBACK_SPELLFAILED_DISRUPTED = 55;

// 0x85C116
const WORD CGameSprite::FEEDBACK_GAINEDBARDSONG = 56;

// 0x85C118
const WORD CGameSprite::FEEDBACK_STATICCHARGEEND = 57;

// 0x85C11A
const WORD CGameSprite::FEEDBACK_IMMUNE_TO_RESOURCE = 58;

// 0x85C11C
const WORD CGameSprite::FEEDBACK_LEAVEFAILED_INVENTORY = 59;

// 0x85C11E
const WORD CGameSprite::FEEDBACK_BACKSTAB_AVOIDED = 60;

// 0x85C120
const WORD CGameSprite::FEEDBACK_SPELLFAILED_CASTFAILURE = 61;

// 0x85C122
const WORD CGameSprite::FEEDBACK_62 = 62;

// 0x85C124
const WORD CGameSprite::FEEDBACK_63 = 63;

// 0x85C126
const WORD CGameSprite::FEEDBACK_CRIPPLINGSTRIKEFAILED = 64;

// 0x85C128
const WORD CGameSprite::FEEDBACK_USEMAGICDEVICEFAILED = 65;

// 0x85C12A
const WORD CGameSprite::FEEDBACK_USEMAGICDEVICESUCCEEDED = 66;

// 0x85C12C
const WORD CGameSprite::FEEDBACK_SEARCHSTART = 67;

// 0x85C12E
const WORD CGameSprite::FEEDBACK_SEARCHEND = 68;

// 0x85C130
const WORD CGameSprite::FEEDBACK_69 = 69;

// 0x85C132
const WORD CGameSprite::FEEDBACK_TURNUNDEADEND = 70;

// 0x85C134
const WORD CGameSprite::FEEDBACK_71 = 71;

// 0x85C136
const WORD CGameSprite::FEEDBACK_BATTLESONGSTART = 72;

// 0x85C138
const WORD CGameSprite::FEEDBACK_BATTLESONGEND = 73;

// 0x85C13A
const WORD CGameSprite::FEEDBACK_ARTERIALSTRIKESUCCEEDED = 75;

// 0x85C13C
const WORD CGameSprite::FEEDBACK_HAMSTRINGSUCCEEDED = 76;

// 0x85C13E
const WORD CGameSprite::FEEDBACK_HEROICINSPIRATION = 80;

// 0x85C140
const WORD CGameSprite::FEEDBACK_TARGETCONCEALED = 81;

// 0x85C142
const WORD CGameSprite::FEEDBACK_UNCANNYDODGESUCCEEDED = 82;

// 0x85C144
const WORD CGameSprite::FEEDBACK_ROLL = 83;

// 0x85C146
const WORD CGameSprite::FEEDBACK_SPELLFAILURE_CONCENTRATION = 84;

// 0x85C148
const WORD CGameSprite::FEEDBACK_SPONTANEOUSCASTING = 85;

// 0x85C14A
const WORD CGameSprite::FEEDBACK_TOGGLEFEAT = 86;

// 0x85C14C
const WORD CGameSprite::FEEDBACK_CLEAVE = 87;

// 0x85C14E
const WORD CGameSprite::FEEDBACK_LINGERINGSONG = 88;

// 0x85C150
const WORD CGameSprite::FEEDBACK_89 = 89;

// 0x85C152
const WORD CGameSprite::FEEDBACK_CALLLIGHTNINGEND = 90;

// 0x85C154
const WORD CGameSprite::FEEDBACK_MAGICALWEAPONINUSE = 91;

// 0x85C156
const WORD CGameSprite::FEEDBACK_LEAVEFAILED_LEVELUP = 92;

// 0x85C158
const WORD CGameSprite::FEEDBACK_LEAVEFAILED_STORE = 93;

// 0x8F94B8
const CPoint CGameSprite::PORTRAIT_ICON_SIZE(10, 10);

// 0x8F93F8
const CResRef CGameSprite::SPIN110("SPIN110");

// 0x8F92A0
const CResRef CGameSprite::SPIN111("SPIN111");

// 0x8F9328
const CResRef CGameSprite::SPIN107("SPIN107");

// 0x8F93E8
const CResRef CGameSprite::SPIN141("SPIN141");

// 0x8F92D0
const CResRef CGameSprite::SPIN142("SPIN142");

// 0x8F9468
const CResRef CGameSprite::SPIN143("SPIN143");

// 0x8F92C8
const CResRef CGameSprite::SPIN244("SPIN244");

// 0x8F94A0
const CResRef CGameSprite::SPIN245("SPIN245");

// 0x8F9488
const CResRef CGameSprite::SPIN246("SPIN246");

// 0x8F8E60
const CResRef CGameSprite::SPIN108("SPIN108");

// 0x8F92E0
const CResRef CGameSprite::SPIN109("SPIN109");

// 0x8F92B8
const CResRef CGameSprite::SPIN120("SPIN120");

// 0x8F9298
const CResRef CGameSprite::SPIN121("SPIN121");

// 0x8F9320
const CResRef CGameSprite::SPIN125("SPIN125");

// 0x8F9330
const CResRef CGameSprite::SPIN139("SPIN139");

// 0x8F9478
const CResRef CGameSprite::SPIN152("SPIN152");

// 0x8F9338
const CResRef CGameSprite::SPIN275("SPIN275");

// 0x8F8E70
const CResRef CGameSprite::SPIN276("SPIN276");

// 0x8F92F8
const CResRef CGameSprite::SPIN277("SPIN277");

// 0x8F92E8
const CResRef CGameSprite::SPIN278("SPIN278");

// 0x8F93D0
const CResRef CGameSprite::SPIN279("SPIN279");

// 0x8F8E18
const CResRef CGameSprite::SPIN970("SPIN970");

// 0x8F94B0
const CResRef CGameSprite::SPIN197("SPIN197");

// 0x8F8E90
const CResRef CGameSprite::SPPR327("SPPR327");

// 0x8F94D0
const CResRef CGameSprite::SPPR328("SPPR328");

// 0x8F92A8
const CResRef CGameSprite::SPPR429("SPPR429");

// 0x8F9460
const CResRef CGameSprite::SPIN198("SPIN198");

// 0x8F9448
const CResRef CGameSprite::SPIN199("SPIN199");

// 0x8F9300
const CResRef CGameSprite::SPIN200("SPIN200");

// 0x8F8E30
const CResRef CGameSprite::SPIN243("SPIN243");

// 0x8F9438
const CResRef CGameSprite::SPIN201("SPIN201");

// 0x8F9308
const CResRef CGameSprite::SPIN202("SPIN202");

// 0x8F8E28
const CResRef CGameSprite::SPIN203("SPIN203");

// 0x8F9418
const CResRef CGameSprite::SPIN247("SPIN247");

// 0x8F9348
const CResRef CGameSprite::SPIN204("SPIN204");

// 0x8F9490
const CResRef CGameSprite::SPIN205("SPIN205");

// 0x8F92B0
const CResRef CGameSprite::SPIN206("SPIN206");

// 0x8F9458
const CResRef CGameSprite::SPIN207("SPIN207");

// 0x8F9470
const CResRef CGameSprite::SPIN208("SPIN208");

// 0x8F9498
const CResRef CGameSprite::SPIN209("SPIN209");

// 0x8F8E88
const CResRef CGameSprite::SPIN210("SPIN210");

// 0x8F94C0
const CResRef CGameSprite::SPIN231("SPIN231");

// 0x8F94A8
const CResRef CGameSprite::SPIN191("SPIN191");

// 0x8F8E48
const CResRef CGameSprite::SPIN211("SPIN211");

// 0x8F8E78
const CResRef CGameSprite::SPIN212("SPIN212");

// 0x8F8E40
const CResRef CGameSprite::SPIN213("SPIN213");

// 0x8F9428
const CResRef CGameSprite::SPIN214("SPIN214");

// 0x8F8E58
const CResRef CGameSprite::SPIN215("SPIN215");

// 0x8F94E8
const CResRef CGameSprite::SPIN216("SPIN216");

// 0x8F92C0
const CResRef CGameSprite::SPIN217("SPIN217");

// 0x8F9400
const CResRef CGameSprite::SPIN218("SPIN218");

// 0x8F9310
const CResRef CGameSprite::SPIN229("SPIN229");

// 0x8F94D8
const CResRef CGameSprite::SPIN232("SPIN232");

// 0x8F93E0
const CResRef CGameSprite::SPIN233("SPIN233");

// 0x8F8E68
const CResRef CGameSprite::SPIN234("SPIN234");

// 0x8F93D8
const CResRef CGameSprite::SPIN235("SPIN235");

// 0x8F9410
const CResRef CGameSprite::SPIN236("SPIN236");

// 0x8F8E20
const CResRef CGameSprite::SPIN260("SPIN260");

// 0x8F9340
const CString CGameSprite::FA("FA_");

// 0x8F9430
const CString CGameSprite::GA("GA_");

// 0x8F94E0
const CString CGameSprite::RA("RA_");

// 0x8F94C8
const CString CGameSprite::AP("AP_");

// 0x8F9480
const CResRef CGameSprite::SPWI110("SPWI110");

// 0x8F9408
const CResRef CGameSprite::SPIN122("SPIN122");

// 0x8F93F0
const CResRef CGameSprite::SPIN285("SPIN285");

// 0x8F9924
const CString CGameSprite::TOKEN_SPECIALABILITY("SPECIALABILITYNAME");

// 0x8F9350
BYTE CGameSprite::STATICBUFFER[STATICBUFFERSIZE_CGAMESPRITE];

// 0x8F950C
BOOLEAN CGameSprite::SHOW_CHARACTER_HP;

// 0x8F950D
BOOLEAN CGameSprite::GRAVITY_IS_DOWN;

// 0x8B85E0
INT CGameSprite::m_bRollFeedbackEnabled = -1;

// 0x8F9A18
CAIAction CGameSprite::m_aiDoAction;

// 0x6EF990
CGameSprite::CGameSprite(BYTE* pCreature, LONG creatureSize, int a3, WORD type, DWORD expirationTime, WORD huntingRange, WORD followRange, DWORD timeOfDayVisible, CPoint startPos, WORD facing)
    : m_portraitIconVidCell(CResRef("STATES"), g_pBaldurChitin->field_4A28)
{
    int index;

    field_7540 = 0;
    field_7544 = 0;
    m_spriteEffectSequenceLength = 0;
    m_spriteEffectBaseIntensity = 0;
    m_spriteEffectRandomIntensity = 0;
    m_bEscapingArea = 0;
    m_nSndWalk = 0;
    m_currSndWalk = 0;
    m_effectExtendDirection = 0;
    m_animationRunning = FALSE;
    m_posZDelta = 0;
    m_circleFacing = 0;
    m_radius = 0;
    m_fDirectionOffset = 0.0;
    m_skipDeltaDirection = 0;
    m_deltaDirection = 0;
    m_walkBackwards = FALSE;
    m_turningAbout = FALSE;
    m_bInCasting = FALSE;
    m_moveCount = 0;
    m_curDest.x = 0;
    m_curDest.y = 0;
    m_posLastVisMapEntry.x = 0;
    m_posLastVisMapEntry.y = 0;
    m_nCommandPause = 0;
    field_557A = 0;
    field_557E = 0;
    field_558E = 0;
    field_559E = 0;
    field_55A0 = 0;
    m_speedFactor = 0;
    m_lastActionID = 0;
    field_560E = 0;
    field_5610 = 0;
    field_5618 = 0;
    field_561C = 0;
    m_noActionCount = 0;
    field_5630 = 0;
    field_5632 = 0;
    field_5636 = 0;
    field_7118 = 0;
    field_711C = 0;
    m_firstActionSound = FALSE;
    m_bForceVisualEffects = FALSE;
    m_currentActionId = 0;
    field_72A8 = 0;
    m_bInUnmarshal = FALSE;
    field_72D6 = 0;
    field_72DE = 0;
    field_7532 = 0;
    field_7536 = 0;
    field_9D08 = 0;
    field_9D0C = 0;
    field_9D10 = 0;
    m_objectType = TYPE_SPRITE;
    m_resRef = "";
    m_fCircleChange = 0.0;
    m_fCurrCircleChange = 1.0;
    m_bGlobal = FALSE;
    m_posExact.x = 0;
    m_posExact.y = 0;
    m_posDelta.x = 0;
    m_posDelta.y = 0;
    m_posDest.x = 0;
    m_posDest.y = 0;
    m_posOld.x = 0;
    m_posOld.y = 0;
    field_536A = 0;
    field_536E = 0;
    m_nSequence = -1;
    m_nNewDirection = facing & 0xF;
    m_nDirection = facing & 0xF;
    m_pPath = 0;
    m_nPath = 0;
    m_currPath = 0;
    m_pathSearchInvalidDest = FALSE;
    m_selectedSound = 17;
    field_53D2 = 0;
    m_currentSearchRequest = 0;
    m_lastCharacterCount = 0;
    m_sequenceTest = FALSE;
    m_removeFromArea = FALSE;
    m_talkingCounter = 0;
    m_moveToFrontQueue = 0;
    m_moveToBackQueue = 0;
    m_curResponseNum = -1;
    m_curResponseSetNum = -1;
    m_curScriptNum = -1;
    m_curAction = CAIAction::NULL_ACTION;
    m_interrupt = FALSE;
    m_endOfDamageSeq = FALSE;
    m_inFormation = FALSE;
    m_lastRGBColor = 0x8000;
    m_bVisibilityUpdated = TRUE;
    m_targetId = CGameObjectArray::INVALID_INDEX;
    m_targetPoint.x = -1;
    m_targetPoint.y = -1;
    m_targetAreaSize = 0;
    m_destMarker.SetType(CMarker::RECTICLE);
    field_72E2 = CGameObjectArray::INVALID_INDEX;
    m_typeAI.Set(CAIObjectType::ANYONE);
    m_liveTypeAI.Set(CAIObjectType::ANYONE);
    m_startTypeAI.Set(CAIObjectType::ANYONE);
    m_overrideScript = 0;
    m_special1Script = 0;
    m_teamScript = 0;
    m_special2Script = 0;
    m_combatScript = 0;
    m_special3Script = 0;
    m_movementScript = NULL;
    m_followLeader = FALSE;
    m_followLeaderAdditive = FALSE;
    m_followLeaderNext = CGameObjectArray::INVALID_INDEX;
    m_followStart = 0;
    m_userCommandPause = 0;
    m_recoilFrame = 0;
    field_561E = 0;
    field_44A = 0;
    field_54C = 0;
    field_560C = 0;
    field_5616 = 0;
    field_5612 = 0;
    field_5614 = 0;
    m_castCounter = -1;
    m_bStartedCasting = FALSE;
    m_attackFrame = -2;
    field_72A2 = 0;
    m_bAllowEffectListCall = TRUE;
    field_72AA = 0;
    m_bForceVisualEffects = FALSE;
    field_5582 = 0;
    field_9D14 = 0;
    field_9D15 = 0;
    m_hasColorEffects = FALSE;
    m_hasColorRangeEffects = FALSE;
    field_5640 = 0;
    m_groupPosition = 0;
    m_groupMove = 0;
    field_562C = 0;
    m_curProjectile = 0;
    m_curSpell = 0;
    m_curItem = 0;
    m_nNumberOfTimesTalkedTo = 0;
    m_bSeenPartyBefore = 0;
    field_710A = 0;
    field_710C = 0;
    field_710E = 0;
    field_7110 = 0x7FFF;
    m_moraleFailure = FALSE;
    m_nHappiness = 0;
    m_bHappinessChanged = FALSE;
    m_bDeleteOnRemove = TRUE;
    m_lastRegenerationTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;

    // NOTE: The original zeroes 0x3428-0x3DFA here (rep stos at 0x6EFAAB);
    // nothing writes field_3428 afterwards.
    field_3428 = 0;

    CButtonData buttonData;

    for (index = 0; index < 8; index++) {
        m_quickWeapons[index] = buttonData;
    }

    for (index = 0; index < 9; index++) {
        m_quickSpells[index] = buttonData;
    }

    for (index = 0; index < 3; index++) {
        m_quickItems[index] = buttonData;
    }

    for (index = 0; index < 9; index++) {
        m_quickInnates[index] = buttonData;
    }

    for (index = 0; index < 9; index++) {
        m_quickSongs[index] = buttonData;
    }

    field_70F6 = rand() % 20 + 1;
    field_70F7 = rand() % 20 + 1;
    field_70F8 = rand() % 20 + 1;
    field_70F9 = rand() % 20 + 1;
    field_70FA = rand() % 100;
    field_55F2 = 0;
    field_55F6 = 0;
    field_55FA = 0;
    field_55FE = 0;
    field_5602 = 0;

    for (index = 0; index < 20; index++) {
        field_55A2[index] = -65538;
    }

    memset(m_nNumberOfTimesInteractedWith, 0, sizeof(m_nNumberOfTimesInteractedWith));

    field_54B8 = 0;
    field_54A8 = 0;
    m_bBumped = FALSE;
    m_ptBumpedFrom.x = -1;
    m_ptBumpedFrom.y = -1;
    m_bVisibleMonster = FALSE;
    m_bSelected = FALSE;
    m_bInfravisionOn = FALSE;
    m_bPortraitUpdate = FALSE;
    m_nBloodFlashAmount = 0;
    m_nDamageLocatorTime = 0;
    m_nDamageLocatorColor = 0x1E1EFF;
    m_bBloodFlashOn = 0;
    field_53E6 = 0;
    m_nTwitches = 0;

    // NOTE: See `CGameArea` note on default terrain tables.
    memcpy(m_terrainTable, DEFAULT_TERRAIN_TABLE, 16);
    memcpy(m_visibleTerrainTable, DEFAULT_VISIBLE_TERRAIN_TABLE, 16);
    memcpy(m_flightTerrainTable, DEFAULT_VISIBLE_TERRAIN_TABLE, 16);

    if (g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Add(&m_id, this, INFINITE) == CGameObjectArray::SUCCESS) {
        m_pSpriteEffectArray = NULL;
        m_pSpriteEffectArrayPosition = NULL;
        m_spriteEffectDuration = 0;
        m_spriteEffectFlags = 0;
        field_5304 = 0;
        m_doBounce = 3;
        m_nModalState = 0;
        field_4C54[0] = 0;
        field_4C54[1] = 0;
        field_4C54[2] = 0;
        field_4C54[3] = 0;
        field_4C54[4] = 0;
        m_nWeaponSet = 0;

        field_7548[IWD_VFX_SANCTUARY].SetResRef(CResRef("SanctuC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_ENTANGLE].SetResRef(CResRef("EntangC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_MINOR_GLOBE_OF_INVULNERABILITY].SetResRef(CResRef("MGoInvC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_SHIELD].SetResRef(CResRef("ShieldC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_GREASE].SetResRef(CResRef("GreaseC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_WEB].SetResRef(CResRef("WebC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_GLOBE_OF_INVULNERABILITY].SetResRef(CResRef("GoInvuC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_SHROUD_OF_FLAME].SetResRef(CResRef("SoFlamC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_ANTIMAGIC_MISSILE].SetResRef(CResRef("AMShelC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_OTILUKES_RESILIENT_SPHERE].SetResRef(CResRef("ORSpheC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_PROTECTION_FROM_MISSILES].SetResRef(CResRef("PFNMisC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_CLOAK_OF_FEAR].SetResRef(CResRef("CoFearC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_ENTROPY_SHIELD].SetResRef(CResRef("EShielC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_FIRE_AURA].SetResRef(CResRef("FiAuraC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_FROST_AURA].SetResRef(CResRef("FrAuraC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_INSECT_PLAGUE].SetResRef(CResRef("IPlaguC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_STORM_SHELL].SetResRef(CResRef("SShellC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_SHIELD_OF_LATHANDER].SetResRef(CResRef("SoLatC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_SHIELD_OF_LATHANDER_2].SetResRef(CResRef("SoLatC2"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_GREATER_SHIELD_OF_LATHANDER].SetResRef(CResRef("GSoLaC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_GREATER_SHIELD_OF_LATHANDER_2].SetResRef(CResRef("GSoLaC2"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_SEVEN_EYES].SetResRef(CResRef("SEyesC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_SEVEN_EYES_2].SetResRef(CResRef("SEyesC2"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_FIRE_SHIELD_RED].SetResRef(CResRef("FShiRC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_28].SetResRef(CResRef("FShiRC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_FIRE_SHIELD_BLUE].SetResRef(CResRef("FShiBC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_29].SetResRef(CResRef("FShiBC1"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_DEATH_ARMOR].SetResRef(CResRef("DArmorC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_TORTOISE_SHELL].SetResRef(CResRef("TShellC"), FALSE, TRUE, TRUE);
        field_7548[IWD_VFX_WISP].SetResRef(CResRef("Wisp"), FALSE, TRUE, TRUE);

        field_9088[IWD_VFX_GLOBE_OF_INVULNERABILITY].SetResRef(CResRef("AFT_M05"), TRUE, TRUE);
        field_9088[IWD_VFX_SHROUD_OF_FLAME].SetResRef(CResRef("MISC_01C"), TRUE, TRUE);
        field_9088[IWD_VFX_ANTIMAGIC_MISSILE].SetResRef(CResRef("AFT_M04"), TRUE, TRUE);
        field_9088[IWD_VFX_OTILUKES_RESILIENT_SPHERE].SetResRef(CResRef("AFT_M03"), TRUE, TRUE);
        field_9088[IWD_VFX_PROTECTION_FROM_MISSILES].SetResRef(CResRef("AFT_M01"), TRUE, TRUE);
        field_9088[IWD_VFX_CLOAK_OF_FEAR].SetResRef(CResRef("AFT_P02"), TRUE, TRUE);
        field_9088[IWD_VFX_ENTROPY_SHIELD].SetResRef(CResRef("AFT_P03"), TRUE, TRUE);
        field_9088[IWD_VFX_STORM_SHELL].SetResRef(CResRef("AFT_P25"), TRUE, TRUE);
        field_9088[IWD_VFX_SHIELD_OF_LATHANDER].SetResRef(CResRef("AFT_P20"), TRUE, TRUE);
        field_9088[IWD_VFX_GREATER_SHIELD_OF_LATHANDER].SetResRef(CResRef("AFT_P26"), TRUE, TRUE);
        field_9088[IWD_VFX_SANCTUARY].SetResRef(CResRef("AFT_P01"), TRUE, TRUE);
        field_9088[IWD_VFX_MINOR_GLOBE_OF_INVULNERABILITY].SetResRef(CResRef("AFT_M02"), TRUE, TRUE);
        field_9088[IWD_VFX_SHIELD].SetResRef(CResRef("EFF_M11C"), TRUE, TRUE);

        m_type = type;
        m_expirationTime = expirationTime;
        m_huntingRange = huntingRange;
        m_followRange = followRange;
        m_posStart = startPos;
        m_timeOfDayVisible = timeOfDayVisible;
        m_active = ((m_timeOfDayVisible >> g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->GetCurrentHour()) & 1) != 0;
        m_activeAI = TRUE;
        m_activeImprisonment = TRUE;

        memset(&m_baseStats, 0, sizeof(m_baseStats));

        m_pDialogData = NULL;
        m_nDialogData = 0;
        m_dialogWait = 0;
        m_dialogWaitTarget = CGameObjectArray::INVALID_INDEX;
        m_bAllowDialogInterrupt = TRUE;
        field_56E4 = "";
        m_talkingRenderCount = 0;
        m_clearAIOnRemoveFromArea = TRUE;

        if (m_portraitIconVidCell.pRes != NULL) {
            m_portraitIconVidCell.pRes->Request();
        }

        field_722A = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;
        m_interactingWith.Set(CAIObjectType::NOONE);
        m_nHPCONBonusTotalOld = 0;
        m_bHPCONBonusTotalUpdate = TRUE;
        m_modalCounter = 0;
        m_bHiding = FALSE;
        m_secondarySounds = "";

        memset(field_725A, 0, sizeof(field_725A));

        field_727E = 0;
        m_pLocalVariables = new CVariableHash(16);
        m_internalButtonList = NULL;

        SleepEx(10, FALSE);
        Unmarshal(pCreature, creatureSize, facing, a3);
        SleepEx(10, FALSE);
        UnmarshalScripts();
        SleepEx(10, FALSE);

        // NOTE: Inlining as `STR_RES` object is immediately destroyed.
        if (1) {
            STR_RES strRes;
            g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_name, strRes);
            m_sName = strRes.szText;
        }

        // NOTE: Uninline.
        FetchCommonStrings();

        for (index = 0; index < 2; index++) {
            m_sndWalk[index].m_nVolumeVariance = 50;
        }

        m_currSndArmor = 0;
        m_sndArmor[m_currSndArmor].m_nVolumeVariance = 50;
        m_animationRunning = (m_baseStats.m_animationType & 0xF000) != 0;
        m_triggerId = CGameObjectArray::INVALID_INDEX;
        field_562C = TRUE;
        m_firstCall = FALSE;
        m_berserkActive = FALSE;
        m_attackSoundDeadzone = 0;
        m_nTempSelectedWeapon = m_equipment.m_selectedWeapon;
        m_nTempSelectedWeaponAbility = static_cast<BYTE>(m_equipment.m_selectedWeaponAbility);

        SetSequence(SEQ_HEAD_TURN);
        CheckLoadState();

        m_bLevelUp = FALSE;
        m_nUnselectableCounter = 0;

        // NOTE: Uninline.
        m_inControlLastTime = InControl();

        m_nStealthGreyOut = 0;
        m_baseStats.m_flags &= ~0x80000000;

        INT bonusNew = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetHPCONBonusTotal(m_typeAI,
            m_derivedStats,
            static_cast<BYTE>(m_derivedStats.m_nCON));
        if (bonusNew > m_nHPCONBonusTotalOld) {
            if ((m_derivedStats.m_generalState & (STATE_DEAD | STATE_FROZEN_DEATH | STATE_STONE_DEATH)) == 0) {
                m_baseStats.m_hitPoints += static_cast<SHORT>(bonusNew) - static_cast<SHORT>(m_nHPCONBonusTotalOld);
            } else {
                m_baseStats.m_hitPoints = 0;
            }
            m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(bonusNew) - static_cast<SHORT>(m_nHPCONBonusTotalOld);
            m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(bonusNew) - static_cast<SHORT>(m_nHPCONBonusTotalOld);
        } else if (bonusNew < m_nHPCONBonusTotalOld) {
            // NOTE: The code in this branch looks exactly the same as above. It
            // might be different in earlier games.
            if ((m_derivedStats.m_generalState & (STATE_DEAD | STATE_FROZEN_DEATH | STATE_STONE_DEATH)) == 0) {
                m_baseStats.m_hitPoints += static_cast<SHORT>(bonusNew) - static_cast<SHORT>(m_nHPCONBonusTotalOld);
            } else {
                m_baseStats.m_hitPoints = 0;
            }
            m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(bonusNew) - static_cast<SHORT>(m_nHPCONBonusTotalOld);
            m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(bonusNew) - static_cast<SHORT>(m_nHPCONBonusTotalOld);
        }
        m_nHPCONBonusTotalOld = bonusNew;
        m_bHPCONBonusTotalUpdate = FALSE;
        m_derivedStats.m_nMaxHitPoints = max(m_derivedStats.m_nMaxHitPoints, m_derivedStats.m_nLevel);

        if (m_baseStats.m_hitPoints <= 0
            && (m_derivedStats.m_generalState & STATE_DEAD) == 0
            && (m_derivedStats.m_generalState & (STATE_STONE_DEATH | STATE_FROZEN_DEATH)) == 0
            && (m_baseStats.m_generalState & STATE_DEAD) != 0) {
            m_derivedStats.m_generalState |= STATE_DEAD;
        }

        field_7282 = -1;
        field_7283 = -1;
        field_7284 = -1;
        field_728E = -1;
        m_bPlayedEncumberedStopped = 0;
        m_bPlayedEncumberedSlowed = 0;
        field_72A0 = 0;
        m_nBounceCounter = 0;
        field_7106 = 0;

        sub_7204C0();

        field_70EE = 0;
        field_7430 = 0;
        field_70F2 = 0;
        if (IcewindMisc::IsLarge(this) == TRUE) {
            field_7548[IWD_VFX_OTILUKES_RESILIENT_SPHERE].SetResRef(CResRef("ORSpheB"), FALSE, TRUE, TRUE);
        }

        field_753A = 0;
        field_753C = 0;
    } else {
        // FIXME: Does not look cool.
        delete this;
    }
}

// 0x6F2990
BOOL CGameSprite::DoesIntersect(CRect r)
{
    r.InflateRect(0, 0, 1, 1);

    CRect rEllipse = m_animation.GetEllipseRect();
    rEllipse.OffsetRect(m_pos);

    CRect rIntersection;
    rIntersection.IntersectRect(r, rEllipse);

    return !rIntersection.IsRectNull();
}

// 0x6F2A90
BOOL CGameSprite::IsOver(const CPoint& pt)
{
    if (m_active
        && m_activeAI
        && m_activeImprisonment
        && Animate()
        && m_baseStats.field_294 == 0
        && (m_typeAI.GetEnemyAlly() <= CAIObjectType::EA_GOODCUTOFF
            || (m_derivedStats.m_generalState & STATE_INVISIBLE) == 0
            || (m_baseStats.field_2FC & 0x1) != 0)) {
        const CRect& rEllipse = m_animation.GetEllipseRect();
        CPoint ptRelative(pt.x - m_pos.x, pt.y - m_pos.y);
        return rEllipse.PtInRect(ptRelative);
    } else {
        return FALSE;
    }
}

// 0x6F2B50
const BYTE* CGameSprite::GetVisibleTerrainTable()
{
    return m_visibleTerrainTable;
}

// 0x6F2B60
BOOL CGameSprite::OnSearchMap()
{
    return ((m_listType == LIST_FRONT || (m_derivedStats.m_generalState & STATE_SLEEPING) != 0))
        && m_pArea != NULL
        && m_pos.x >= 0
        && m_pos.y >= 0;
}

// 0x6F2B90
void CGameSprite::SetAIType(const CAIObjectType& typeAI, BOOL updateLive, BOOL updateStart)
{
    m_typeAI.Set(typeAI);

    if (updateLive) {
        m_liveTypeAI.Set(typeAI);
    }

    if (updateStart) {
        m_startTypeAI.Set(typeAI);
    }
}

// 0x6F2BD0
void CGameSprite::ResetAITypeLive()
{
    m_typeAI.m_nGeneral = m_liveTypeAI.m_nGeneral;
    m_startTypeAI.m_nGeneral = m_liveTypeAI.m_nGeneral;
}

// 0x6F2BF0
void CGameSprite::ResetAIType()
{
    m_typeAI.Set(m_startTypeAI);

    BYTE prev = m_liveTypeAI.m_nGeneral;
    m_liveTypeAI.Set(m_startTypeAI);
    m_liveTypeAI.m_nGeneral = prev;
}

// 0x6F2C50
LONG CGameSprite::GetTargetId()
{
    return m_targetId;
}

// 0x6F2C60
const BYTE* CGameSprite::GetTerrainTable()
{
    return m_terrainTable;
}

// 0x6F2C70
BOOL CGameSprite::GetCanSeeInvisible()
{
    return m_curAction.m_actionID == CAIAction::DIALOGUE
        || m_curAction.m_actionID == CAIAction::STARTDIALOG
        || m_curAction.m_actionID == CAIAction::STARTDIALOGUENOSET
        || m_derivedStats.m_bSeeInvisible
        || (m_baseStats.m_flags & 0x10000) != 0;
}

// 0x6F40F0
void CGameSprite::AddToArea(CGameArea* pNewArea, const CPoint& pos, LONG posZ, BYTE listType)
{
    BOOLEAN v1 = FALSE;

    CGameObject::AddToArea(pNewArea, pos, posZ, listType);
    m_posExact.x = m_pos.x << 10;
    m_posExact.y = (m_pos.y << 12) / 3;
    m_posOld = m_pos;
    field_562C = 1;

    if (m_active && m_activeAI && m_activeImprisonment) {
        CheckIfVisible();
    }

    switch (listType) {
    case CGAMEOBJECT_LIST_FRONT:
        if (m_active
            && m_activeAI
            && m_activeImprisonment
            && (m_derivedStats.m_generalState & STATE_DEAD) == 0) {
            // NOTE: Uninline.
            pNewArea->AddToMarkers(m_id);

            m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                            m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
        }
        break;
    case CGAMEOBJECT_LIST_BACK:
        if (m_active && m_activeAI && m_activeImprisonment) {
            pNewArea->IncrHeightDynamic(m_pos);
        }
        break;
    case CGAMEOBJECT_LIST_FLIGHT:
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 1528
        UTIL_ASSERT(FALSE);
    }

    SetFootstepChannel();

    if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0) {
        if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
            || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1) {
            char* pSndReady = m_animation.GetSndReady();
            if (*pSndReady != '\0') {
                m_sndReady.SetResRef(CResRef(pSndReady), TRUE, TRUE);
                delete pSndReady;
                m_sndReady.SetLoopingFlag(1);
                m_sndReady.Play(m_pos.x, m_pos.y, m_posZ, FALSE);
            }
        }
    }

    m_currentArea = pNewArea->m_resRef;

    if (m_bGlobal) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 1548
        UTIL_ASSERT(pGame != NULL);

        if (pGame->GetCharacterPortraitNum(m_id) != -1
            && Animate()) {
            m_posLastVisMapEntry = pos;
            pNewArea->m_visibility.AddCharacter(m_posLastVisMapEntry,
                m_id,
                m_visibleTerrainTable);
            // NOTE: The original compares the BYTE count with 0xFF
            // (`cmp al, 0xff`); our BYTE against the int -1 never matched.
            if (pNewArea->m_nCharacters != 0xFF) {
                pNewArea->m_nCharacters++;
            } else {
                pNewArea->m_nCharacters = 1;
            }
        }
    } else if (m_type == 1) {
        pNewArea->m_nRandomMonster++;
    }

    if (!g_pBaldurChitin->GetObjectGame()->m_bInLoadGame
        && !g_pBaldurChitin->GetObjectGame()->m_bInLoadArea
        && !g_pBaldurChitin->GetObjectGame()->m_bInAreaTransition
        && !g_pBaldurChitin->GetBaldurMessage()->m_bInOnObjectAdd) {
        BYTE* pData = STATICBUFFER;
        DWORD dwSize;
        MarshalMessage(&pData, &dwSize);
        g_pBaldurChitin->GetBaldurMessage()->ObjectAdd(m_id, GetObjectType(), pData, dwSize);

        if (InControl()
            && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            m_cLastSpriteUpdate.Initialize(TRUE);
            CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        v1 = TRUE;
    }

    if (m_pArea == NULL) {
        // FIXME: Unused.
        CString sText("Sprite ");
        if (m_sName.GetLength() != 0) {
            sText += m_sName;
        } else {
            sText += "Unknown";
        }
        sText += " has a NULL m_pArea member in AddToArea() function!";
    }

    if (pNewArea != NULL) {
        if (g_pBaldurChitin->GetObjectGame()->m_bInLoadGame
            && pNewArea->mpSpawner->m_bInSpawn == TRUE
            && !v1) {
            BYTE* pData = STATICBUFFER;
            DWORD dwSize;
            MarshalMessage(&pData, &dwSize);
            g_pBaldurChitin->GetBaldurMessage()->ObjectAdd(m_id, GetObjectType(), pData, dwSize);
        }
    }

    if (m_baseStats.field_294) {
        m_baseStats.field_294 = FALSE;
        sub_75F3D0(1);
        if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL) {
            CMessage* message = new CMessage90(m_id, m_id, 1);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    if (m_baseStats.field_2F8) {
        m_removeFromArea = TRUE;
    }

    if (!m_baseStats.field_2E2) {
        sub_75F240();
        m_baseStats.field_2E2 = TRUE;
    }

    Icewind586B70::Instance()->sub_586FC0(this);
}

// 0x6F47F0
void CGameSprite::RemoveReplacementFromArea()
{
    if (m_pArea != NULL) {
        m_sndReady.Stop();
        DropSearchRequest();
        if (m_bInfravisionOn) {
            m_bInfravisionOn = FALSE;
            m_pArea->m_nInfravision--;
        }

        if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1) {
            if (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC || InControl()) {
                if (Animate()) {
                    m_pArea->m_visibility.RemoveCharacter(m_pos,
                        m_id,
                        m_visibleTerrainTable);
                }

                // NOTE: The BYTE count compared with 0xFF, as the original.
                if (m_pArea->m_nCharacters != 0xFF) {
                    m_pArea->m_nCharacters--;
                }
            }
        }

        ClearStoredPaths();

        if (m_pPath != NULL) {
            delete m_pPath;
            m_pPath = NULL;
        }

        if (m_clearAIOnRemoveFromArea) {
            ClearAI(TRUE);
        }

        m_nNewDirection = m_nDirection;
        g_pBaldurChitin->GetObjectGame()->RemoveCharacterFromAllies(m_id);

        switch (m_listType) {
        case CGAMEOBJECT_LIST_FRONT:
            if (m_active
                && m_activeAI
                && m_activeImprisonment) {
                if ((m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                    // NOTE: Uninline.
                    m_pArea->RemoveFromMarkers(m_id);
                }

                m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                   m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);
            }
            break;
        case CGAMEOBJECT_LIST_BACK:
            if (m_active
                && m_activeAI
                && m_activeImprisonment) {
                if ((m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
                    // NOTE: Uninline.
                    m_pArea->RemoveFromMarkers(m_id);

                    m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                       m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                        m_typeAI.GetEnemyAlly(),
                        m_animation.GetPersonalSpace(),
                        field_54A8,
                        field_7430);
                } else {
                    m_pArea->DecrHeightDynamic(m_pos);
                }
            }
            break;
        case CGAMEOBJECT_LIST_FLIGHT:
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 1773
            UTIL_ASSERT(FALSE);
        }

        if (m_bGlobal) {
            m_pArea = NULL;
            m_pos.x = -1;
            m_pos.y = -1;
            m_posZ = 0;
        } else {
            if (m_type == 1) {
                m_pArea->m_nRandomMonster--;
            }

            CGameObject::RemoveFromArea();

            BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
                CGameObjectArray::THREAD_ASYNCH,
                NULL,
                INFINITE);
            if (rc != CGameObjectArray::SUCCESS) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 1783
                UTIL_ASSERT(FALSE);
            }

            delete this;
        }
    }
}

// 0x6F4B90
void CGameSprite::RemoveFromArea()
{
    if (m_pArea != NULL) {
        POSITION pos = m_lstBlood.GetHeadPosition();
        while (pos != NULL) {
            CBlood* pBlood = m_lstBlood.GetNext(pos);
            delete pBlood;
        }
        m_lstBlood.RemoveAll();

        m_sndReady.Stop();
        DropSearchRequest();

        if (m_bBumped) {
            m_ptBumpedFrom.x = -1;
            m_ptBumpedFrom.y = -1;
            m_bBumped = FALSE;
        }

        if (m_bVisibleMonster) {
            m_pArea->m_nVisibleMonster--;
            m_bVisibleMonster = FALSE;
        }

        if (m_bInfravisionOn) {
            m_pArea->m_nInfravision--;
            m_bInfravisionOn = FALSE;
        }

        if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1) {
            if (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC || InControl()) {
                if (Animate()) {
                    m_pArea->m_visibility.RemoveCharacter(m_pos,
                        m_id,
                        m_visibleTerrainTable);
                }

                // NOTE: The BYTE count compared with 0xFF, as the original.
                if (m_pArea->m_nCharacters != 0xFF) {
                    m_pArea->m_nCharacters--;
                }
            }
        }

        ClearStoredPaths();

        if (m_pPath != NULL) {
            delete m_pPath;
            m_pPath = NULL;
        }

        if (m_clearAIOnRemoveFromArea) {
            ClearAI(TRUE);
        }

        m_nNewDirection = m_nDirection;
        g_pBaldurChitin->GetObjectGame()->RemoveCharacterFromAllies(m_id);

        switch (m_listType) {
        case CGAMEOBJECT_LIST_FRONT:
            if (m_active
                && m_activeAI
                && m_activeImprisonment) {
                if ((m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                    // NOTE: Uninline.
                    m_pArea->RemoveFromMarkers(m_id);
                }

                m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                   m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);
            }
            break;
        case CGAMEOBJECT_LIST_BACK:
            if (m_active
                && m_activeAI
                && m_activeImprisonment) {
                if ((m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
                    // NOTE: Uninline.
                    m_pArea->RemoveFromMarkers(m_id);

                    m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                       m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                        m_typeAI.GetEnemyAlly(),
                        m_animation.GetPersonalSpace(),
                        field_54A8,
                        field_7430);
                } else {
                    m_pArea->DecrHeightDynamic(m_pos);
                }
            }
            break;
        case CGAMEOBJECT_LIST_FLIGHT:
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 1922
            UTIL_ASSERT(FALSE);
        }

        if (m_bGlobal) {
            CPoint pos(m_pos);

            CGameObject::RemoveFromArea();

            if (InControl() && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
                if (!g_pBaldurChitin->GetObjectGame()->m_bInDestroyGame
                    && !g_pBaldurChitin->GetObjectGame()->m_bInAreaTransition) {
                    m_cLastSpriteUpdate.Initialize(TRUE);

                    CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
            }

            m_pos = pos;
        } else {
            if (m_type == 1) {
                m_pArea->m_nRandomMonster--;
            }

            CGameObject::RemoveFromArea();

            if (m_bDeleteOnRemove) {
                BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->Delete(m_id,
                    CGameObjectArray::THREAD_ASYNCH,
                    NULL,
                    INFINITE);
                if (rc != CGameObjectArray::SUCCESS) {
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                    // __LINE__: 1934
                    UTIL_ASSERT(FALSE);
                }

                delete this;
            }
        }
    }
}

// 0x6F5FF0
void CGameSprite::AIUpdate()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (m_inControlLastTime != InControl()) {
        JumpToPoint(m_pos, TRUE);
    }

    m_inControlLastTime = InControl();

    if (pGame->GetWorldTimer()->m_active) {
        if ((m_baseStats.m_generalState & STATE_FADE_OUT) != 0) {
            if (static_cast<int>(m_baseStats.field_2F9 + m_baseStats.m_fadeSpeed) > 255) {
                m_baseStats.m_generalState &= ~STATE_FADE_OUT;
                m_derivedStats.m_generalState &= ~STATE_FADE_OUT;
                m_baseStats.field_2F9 = -1;
            } else {
                m_baseStats.field_2F9 += m_baseStats.m_fadeSpeed;
            }
        } else if ((m_baseStats.m_generalState & STATE_FADE_IN) != 0) {
            if (static_cast<int>(m_baseStats.field_2F9 - m_baseStats.m_fadeSpeed) < 0) {
                m_baseStats.m_generalState &= ~STATE_FADE_IN;
                m_derivedStats.m_generalState &= ~STATE_FADE_IN;
                m_baseStats.field_2F9 = -1;
            } else {
                m_baseStats.field_2F9 -= m_baseStats.m_fadeSpeed;
            }
        }

        field_70F6 = rand() % 20 + 1;
        field_70F7 = rand() % 20 + 1;
        field_70F8 = rand() % 20 + 1;
        field_70F9 = rand() % 20 + 1;
        field_70FA = rand() % 100;

        if (m_nBounceCounter <= 0) {
            m_nBounceCounter = 25;
            m_lBounceList.Process(this);
        }
        m_nBounceCounter--;

        if (m_nStealthGreyOut > 0) {
            m_nStealthGreyOut--;

            if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == pGame->GetCharacterPortraitNum(m_id)) {
                INT nState = pGame->GetButtonArray()->m_nState;
                if (nState != 102
                    && nState != 101
                    && nState != 104
                    && nState != 103
                    && nState != 105
                    && nState != 112
                    && nState != 115
                    && nState != 116
                    && nState != 117
                    && nState != 118
                    && nState != 119
                    && nState != 120
                    && nState != 121
                    && nState != 122
                    && nState != 123
                    && nState != 106) {
                    pGame->GetButtonArray()->ResetState();
                }
            }

            if (m_nStealthGreyOut == 0) {
                if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == pGame->GetCharacterPortraitNum(m_id)) {
                    INT nState = pGame->GetButtonArray()->m_nState;
                    if (nState != 102
                        && nState != 101
                        && nState != 104
                        && nState != 103
                        && nState != 105
                        && nState != 112
                        && nState != 115
                        && nState != 116
                        && nState != 117
                        && nState != 118
                        && nState != 119
                        && nState != 120
                        && nState != 121
                        && nState != 122
                        && nState != 123
                        && nState != 106) {
                        pGame->GetButtonArray()->UpdateState();
                    }
                }
            }
        }

        if (m_attackSoundDeadzone > 0) {
            m_attackSoundDeadzone--;
        }

        if (m_expirationTime < pGame->GetWorldTimer()->m_gameTime
            && (!m_active || m_canBeSeen <= 0)) {
            RemoveFromArea();
            return;
        }

        if (m_active) {
            if (((m_timeOfDayVisible >> pGame->GetWorldTimer()->GetCurrentHour()) & 0x1) == 0
                && m_canBeSeen <= 0) {
                m_active = FALSE;
                if (m_pPath != NULL) {
                    delete m_pPath;
                    m_pPath = NULL;
                }

                DropSearchRequest();

                switch (m_listType) {
                case CGAMEOBJECT_LIST_FRONT:
                    if ((m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                        // NOTE: Uninline.
                        m_pArea->RemoveFromMarkers(m_id);
                    }

                    m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                       m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                        m_typeAI.GetEnemyAlly(),
                        m_animation.GetPersonalSpace(),
                        field_54A8,
                        field_7430);
                    break;
                case CGAMEOBJECT_LIST_BACK:
                    if ((m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
                        // NOTE: Uninline.
                        m_pArea->RemoveFromMarkers(m_id);

                        m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                           m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                            m_typeAI.GetEnemyAlly(),
                            m_animation.GetPersonalSpace(),
                            field_54A8,
                            field_7430);
                    } else {
                        m_pArea->IncrHeightDynamic(m_pos);
                    }
                    break;
                case CGAMEOBJECT_LIST_FLIGHT:
                    break;
                default:
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                    // __LINE__: 2424
                    UTIL_ASSERT(FALSE);
                }
            }
        } else {
            if (((m_timeOfDayVisible >> pGame->GetWorldTimer()->GetCurrentHour()) & 0x1) != 0) {
                if (m_canBeSeen > 0) {
                    m_canBeSeen--;
                    if (m_canBeSeen == 0) {
                        if (m_bVisibleMonster) {
                            m_bVisibleMonster = FALSE;
                            m_pArea->m_nVisibleMonster--;
                        }
                    }
                }

                if ((pGame->GetWorldTimer()->m_gameTime & VISIBLE_DELAY) == (m_id & VISIBLE_DELAY)) {
                    CheckIfVisible();
                }

                if (m_canBeSeen <= 0) {
                    m_active = TRUE;

                    switch (m_listType) {
                    case CGAMEOBJECT_LIST_FRONT:
                        if (m_baseStats.field_294 != TRUE) {
                            if ((m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                                // NOTE: Uninline.
                                m_pArea->AddToMarkers(m_id);
                            }

                            m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                            m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                                m_typeAI.GetEnemyAlly(),
                                m_animation.GetPersonalSpace(),
                                field_54A8,
                                field_7430);

                            JumpToPoint(m_pos, TRUE);
                        }
                        break;
                    case CGAMEOBJECT_LIST_BACK:
                        if (m_baseStats.field_294 != TRUE) {
                            if ((m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
                                m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                                m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                                    m_typeAI.GetEnemyAlly(),
                                    m_animation.GetPersonalSpace(),
                                    field_54A8,
                                    field_7430);

                                // NOTE: Uninline.
                                m_pArea->AddToMarkers(m_id);

                                JumpToPoint(m_pos, TRUE);
                            } else {
                                m_pArea->IncrHeightDynamic(m_pos);
                            }
                        }
                        break;
                    case CGAMEOBJECT_LIST_FLIGHT:
                        break;
                    default:
                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                        // __LINE__: 2488
                        UTIL_ASSERT(FALSE);
                    }
                }
            }
        }
    }

    if ((m_derivedStats.m_generalState & STATE_HELPLESS) == 0
        || (m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
        if ((m_derivedStats.m_generalState & STATE_DEAD) == 0 || m_nSequence == SEQ_DIE) {
            m_animationRunning = TRUE;
        }
    } else {
        m_animationRunning = FALSE;
    }

    if (m_AIInhibitor) {
        return;
    }

    if (pGame->GetCharacterPortraitNum(m_id) != CGameObjectArray::INVALID_INDEX
        && (m_derivedStats.m_generalState & STATE_DEAD) == 0) {
        if ((m_typeAI.GetRace() == CAIOBJECTTYPE_R_ELF
                && m_typeAI.GetSubRace() == CAIOBJECTTYPE_SUBRACE_ELF_DROW)
            || (m_typeAI.GetRace() == CAIOBJECTTYPE_R_DWARF
                && m_typeAI.GetSubRace() == CAIOBJECTTYPE_SUBRACE_DWARF_GRAY)) {
            if ((pGame->GetWorldTimer()->IsDay() || pGame->GetWorldTimer()->IsDawn())
                && m_pArea != NULL
                && (m_pArea->GetHeader()->m_areaType & 0x1) != 0
                && (m_pArea->GetHeader()->m_areaType & 0x2) != 0
                && (m_pArea->GetHeader()->m_areaType & 0x40) == 0) {
                // NOTE: Uninline.
                AddPortraitIcon(137);

                if (InControl() && !m_timedEffectList.IsTypeOnList(ICEWIND_CGAMEEFFECT_DAYBLINDNESS)) {
                    ITEM_EFFECT effect;
                    CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_DAYBLINDNESS);

                    CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect,
                        GetPos(),
                        m_id,
                        CPoint(-1, -1));

                    CMessage* message = new CMessageAddEffect(pEffect, m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
            } else {
                if (InControl() && m_timedEffectList.IsTypeOnList(ICEWIND_CGAMEEFFECT_DAYBLINDNESS)) {
                    m_timedEffectList.RemoveAllOfType(this,
                        ICEWIND_CGAMEEFFECT_DAYBLINDNESS,
                        m_timedEffectList.GetPosCurrent(),
                        -1);
                }
            }
        }
    }

    ProcessAI();

    if (m_bSendSpriteUpdate == TRUE) {
        CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        m_bSendSpriteUpdate = FALSE;
    }

    if (m_talkingCounter > 0) {
        m_talkingCounter--;
    }

    if (m_removeFromArea) {
        if (m_bSelected) {
            Unselect();
            pGame->SelectToolbar();
        }
        RemoveFromArea();
        return;
    }

    if (m_pArea == NULL || m_baseStats.field_294 == TRUE) {
        return;
    }

    if (m_pArea->m_iPicked == m_id) {
        m_bPortraitUpdate = TRUE;
        pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(m_id), 1);
    } else if (m_bPortraitUpdate) {
        m_bPortraitUpdate = FALSE;
        pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(m_id), 1);
    }

    m_marker.AsynchronousUpdate(this);

    if (Orderable(FALSE)
        && (m_bSelected || m_id == m_pArea->m_iPicked)
        && m_targetPoint.x != -1) {
        m_destMarker.AsynchronousUpdate(this);
    }

    if (((m_pArea->GetInfinity()->m_areaType & 0x20) != 0
            || ((m_pArea->GetInfinity()->m_areaType & 0x2) != 0
                && pGame->GetWorldTimer()->IsNight()))
        && m_bSelected
        && pGame->GetCharacterPortraitNum(m_id) != -1
        && (pGame->GetOptions()->m_bDarkvision
            || pGame->GetGroup()->GetCount() == 1)
        && (m_derivedStats.m_generalState & STATE_SLEEPING) == 0
        && (m_derivedStats.m_generalState & STATE_INFRAVISION) != 0) {
        CPoint pt;
        pt.x = m_pos.x;
        pt.y = m_pos.y + m_posZ + m_pArea->GetHeightOffset(m_pos, m_listType);
        COLORREF rgbTintColor = m_pArea->GetTintColor(pt, m_listType);
        BYTE v3 = static_cast<BYTE>((299 * GetRValue(CInfinity::RGB_NIGHT_COLOR) + 587 * GetGValue(CInfinity::RGB_NIGHT_COLOR) + 114 * GetBValue(CInfinity::RGB_NIGHT_COLOR)) / 2550);
        BYTE v4;
        if ((m_pArea->GetInfinity()->m_areaType & 0x2) == 0
            || ((m_pArea->GetInfinity()->m_areaType & 0x40) != 0
                && (m_pArea->GetInfinity()->m_renderDayNightCode & 0x2) != 0)) {
            v4 = static_cast<BYTE>((299 * GetRValue(rgbTintColor) + 587 * GetGValue(rgbTintColor) + 114 * GetBValue(rgbTintColor)) / 2550);
        } else {
            // TODO: Unclear math (merging global tint with area tint).
            v4 = 0;
        }

        if (v4 > v3) {
            if (m_bInfravisionOn) {
                m_bInfravisionOn = FALSE;
                m_pArea->m_nInfravision--;
            }
        } else {
            if (!m_bInfravisionOn) {
                m_bInfravisionOn = TRUE;
                m_pArea->m_nInfravision++;
            }
        }
    } else {
        if (m_bInfravisionOn) {
            m_bInfravisionOn = FALSE;
            m_pArea->m_nInfravision--;
        }
    }

    if (!pGame->GetWorldTimer()->m_active) {
        if (m_active
            && m_activeAI
            && m_activeImprisonment
            && m_lastCharacterCount != m_pArea->m_nCharacters) {
            CheckIfVisible();
        }
        m_lastCharacterCount = m_pArea->m_nCharacters;
        return;
    }

    BOOL v1 = sub_6FB440();
    if (v1 != field_54A8
        && Animate()
        && !m_baseStats.field_294) {
        if (!v1) {
            m_bBumped = FALSE;
            m_ptBumpedFrom.x = -1;
            m_ptBumpedFrom.y = -1;
        }

        m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                           m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            m_typeAI.GetEnemyAlly(),
            m_animation.GetPersonalSpace(),
            field_54A8,
            field_7430);

        field_54A8 = v1;

        m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            m_typeAI.GetEnemyAlly(),
            m_animation.GetPersonalSpace(),
            field_54A8,
            field_7430);
    }

    if (m_bBumped && !m_baseStats.field_294) {
        m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                           m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            m_typeAI.GetEnemyAlly(),
            m_animation.GetPersonalSpace(),
            field_54A8,
            field_7430);

        SHORT nTableIndex;
        if (m_pArea->m_search.GetCost(m_ptBumpedFrom, GetTerrainTable(), m_animation.GetPersonalSpace(), nTableIndex, TRUE) != CPathSearch::COST_IMPASSABLE) {
            m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                            m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
            JumpToPoint(CPoint(m_ptBumpedFrom.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                            m_ptBumpedFrom.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2),
                TRUE);
            m_bBumped = FALSE;
            m_ptBumpedFrom.x = -1;
            m_ptBumpedFrom.y = -1;
        } else {
            if (!m_baseStats.field_294) {
                m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);
            }
        }
    }

    if (m_nBloodFlashAmount > 0) {
        m_nBloodFlashAmount -= 5;
        if (m_nBloodFlashAmount < 0) {
            m_nBloodFlashAmount = 0;
        }
        pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(m_id), 1);
    }

    if (m_nDamageLocatorTime > 0) {
        ULONG nQuarterPeriod = CMarker::PICKED_FLASH_PERIOD / 4;
        ULONG nAsyncCounter = g_pBaldurChitin->nAUCounter % CMarker::PICKED_FLASH_PERIOD;
        COLORREF rgbDamageColor = RGB(255, 30, 30);
        BYTE red;
        BYTE green;
        BYTE blue;

        // TODO: Check, probably wrong.
        if (nAsyncCounter < nQuarterPeriod) {
            red = static_cast<BYTE>(nAsyncCounter * GetRValue(rgbDamageColor) / nQuarterPeriod);
            green = static_cast<BYTE>(nAsyncCounter * GetGValue(rgbDamageColor) / nQuarterPeriod);
            blue = static_cast<BYTE>(nAsyncCounter * GetBValue(rgbDamageColor) / nQuarterPeriod);
        } else if (nAsyncCounter < nQuarterPeriod * 2) {
            nAsyncCounter -= nQuarterPeriod;
            red = static_cast<BYTE>(GetRValue(rgbDamageColor) + nAsyncCounter * (255 - GetRValue(rgbDamageColor)) / nQuarterPeriod);
            green = static_cast<BYTE>(GetGValue(rgbDamageColor) + nAsyncCounter * (255 - GetGValue(rgbDamageColor)) / nQuarterPeriod);
            blue = static_cast<BYTE>(GetBValue(rgbDamageColor) + nAsyncCounter * (255 - GetBValue(rgbDamageColor)) / nQuarterPeriod);
        } else if (nAsyncCounter < nQuarterPeriod * 3) {
            nAsyncCounter -= nQuarterPeriod * 2;
            red = static_cast<BYTE>(255 - nAsyncCounter * (255 - GetRValue(rgbDamageColor)) / nQuarterPeriod);
            green = static_cast<BYTE>(255 - nAsyncCounter * (255 - GetGValue(rgbDamageColor)) / nQuarterPeriod);
            blue = static_cast<BYTE>(255 - nAsyncCounter * (255 - GetBValue(rgbDamageColor)) / nQuarterPeriod);
        } else {
            nAsyncCounter -= nQuarterPeriod * 3;
            red = static_cast<BYTE>(GetRValue(rgbDamageColor) - nAsyncCounter * GetRValue(rgbDamageColor) / nQuarterPeriod);
            green = static_cast<BYTE>(GetGValue(rgbDamageColor) - nAsyncCounter * GetGValue(rgbDamageColor) / nQuarterPeriod);
            blue = static_cast<BYTE>(GetBValue(rgbDamageColor) - nAsyncCounter * GetBValue(rgbDamageColor) / nQuarterPeriod);
        }

        m_nDamageLocatorColor = RGB(red, green, blue);

        m_nDamageLocatorTime--;
    }

    if (!m_lstBlood.IsEmpty()) {
        POSITION pos = m_lstBlood.GetHeadPosition();
        while (pos != NULL) {
            POSITION posOld = pos;
            CBlood* pBlood = m_lstBlood.GetNext(pos);
            if (!pBlood->AsynchronousUpdate()) {
                m_lstBlood.RemoveAt(posOld);
                delete pBlood;
            }
        }
    }

    if (m_AISpeed == AI_SPEED_SLOWED) {
        if (m_active
            && m_activeAI
            && m_activeImprisonment
            && (((pGame->GetWorldTimer()->m_gameTime / 2) & VISIBLE_DELAY) == ((m_id / 2) & VISIBLE_DELAY)
                || m_lastCharacterCount != m_pArea->m_nCharacters)) {
            CheckIfVisible();
        }
    } else {
        if (m_active
            && m_activeAI
            && m_activeImprisonment
            && ((pGame->GetWorldTimer()->m_gameTime & VISIBLE_DELAY) == (m_id & VISIBLE_DELAY)
                || m_lastCharacterCount != m_pArea->m_nCharacters)) {
            CheckIfVisible();
        }
    }

    m_lastCharacterCount = m_pArea->m_nCharacters;

    if (m_canBeSeen > 0) {
        m_canBeSeen--;
        if (m_canBeSeen == 0) {
            if (m_bVisibleMonster) {
                m_bVisibleMonster = FALSE;
                m_pArea->m_nVisibleMonster--;
            }
        }
    }

    UpdateSpriteEffect();

    if (m_animationRunning) {
        if (m_posZ < 0) {
            m_posZ -= m_posZDelta;
            m_posZDelta--;
            if (m_posZ >= 0) {
                m_posZ = 0;
                if (m_doBounce > 0) {
                    m_posZ = -1;
                    m_posZDelta = m_doBounce * (rand() % 3 + 1);
                    m_doBounce--;
                } else {
                    m_animationRunning = FALSE;
                }
            }
        }

        if (m_active) {
            if (pGame->m_nTimeStop == 0 || pGame->m_nTimeStopCaster == m_id) {
                ChangeDirection();
                if (m_nSequence == CGAMESPRITE_SEQ_ATTACK
                    || m_nSequence == CGAMESPRITE_SEQ_SHOOT
                    || m_nSequence == CGAMESPRITE_SEQ_ATTACK_SLASH
                    || m_nSequence == CGAMESPRITE_SEQ_ATTACK_BACKSLASH
                    || m_nSequence == CGAMESPRITE_SEQ_ATTACK_JAB) {
                    SHORT lSlot;
                    CItem* pItem = m_equipment.m_items[m_equipment.m_selectedWeapon];
                    pItem->Demand();
                    sub_756930(pItem, GetLauncher(pItem->GetAbility(m_equipment.m_selectedWeaponAbility), lSlot));
                    pItem->Release();
                }
                switch (m_nSequence) {
                case CGAMESPRITE_SEQ_ATTACK:
                    if (m_animation.IsEndOfSequence()) {
                        SetSequence(CGAMESPRITE_SEQ_READY);
                    } else {
                        if (m_animation.GetCurrentFrame() == field_740C) {
                            CSound cSound;
                            cSound.SetResRef(field_7408, TRUE, TRUE);
                            cSound.m_nPitchVariance = 5;
                            cSound.m_nVolumeVariance = 20;
                            cSound.SetChannel(3, reinterpret_cast<DWORD_PTR>(m_pArea));
                            if (!cSound.GetLooping()) {
                                cSound.SetFireForget(TRUE);
                            }
                            cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
                        }
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_AWAKE:
                    if (m_animation.GetAnimationId() < 0xE000) {
                        if (m_animation.IsBeginningOfSequence()) {
                            // NOTE: Uninline.
                            SetIdleSequence();
                        } else {
                            m_animation.DecrementFrame();
                        }
                    } else {
                        if (m_animation.IsEndOfSequence()) {
                            // NOTE: Uninline.
                            SetIdleSequence();
                        } else {
                            m_animation.IncrementFrame();
                        }
                    }
                    break;
                case CGAMESPRITE_SEQ_CAST:
                    if (m_animation.IsEndOfSequence()) {
                        SetSequence(GetIdleSequence());
                    } else {
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_CONJURE:
                    m_animation.IncrementFrame();
                    break;
                case CGAMESPRITE_SEQ_DAMAGE:
                    if (m_animation.IsBeginningOfSequence()) {
                        CString animationResRef;
                        m_animation.GetAnimationResRef(animationResRef, CGameAnimationType::RANGE_BODY);
                        if (animationResRef == "MWAV") {
                            CGameEffect* pEffect = IcewindMisc::CreateEffectVisualSpellHit(this, 99, 1, 0);
                            AddEffect(pEffect, EFFECT_LIST_TIMED, TRUE, TRUE);
                        }
                    }
                    if (m_animation.IsEndOfSequence()) {
                        m_endOfDamageSeq = TRUE;
                        if (m_pPath != NULL) {
                            SetSequence(CGAMESPRITE_SEQ_WALK);
                        } else {
                            // NOTE: Uninline.
                            SetIdleSequence();
                        }
                    } else {
                        if (field_70FD
                            && m_animation.GetCurrentFrame() == field_7414) {
                            CSound cSound;
                            cSound.SetResRef(CResRef(field_7410), TRUE, TRUE);
                            cSound.m_nPitchVariance = 5;
                            cSound.m_nVolumeVariance = 20;
                            cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                            if (!cSound.GetLooping()) {
                                cSound.SetFireForget(TRUE);
                            }
                            cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
                        }
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_DIE:
                    if (m_animation.IsBeginningOfSequence()) {
                        CString animationResRef;
                        m_animation.GetAnimationResRef(animationResRef, CGameAnimationType::RANGE_BODY);
                        if (animationResRef == "MWAV") {
                            CGameEffect* pEffect = IcewindMisc::CreateEffectVisualSpellHit(this, 97, 1, 0);
                            AddEffect(pEffect, EFFECT_LIST_TIMED, TRUE, TRUE);
                        }
                    }
                    if (m_animation.IsBeginningOfSequence()
                        && field_70FE
                        && !field_7398.IsEmpty()) {
                        INT nIndex = rand() % field_7398.GetCount();
                        POSITION pos = field_7398.GetHeadPosition();
                        while (pos != NULL && nIndex != 0) {
                            nIndex--;
                            field_7398.GetNext(pos);
                        }
                        CGameSpriteSoundEntry& pEntry = field_7398.GetAt(pos);
                        field_7418 = pEntry.field_0;
                        field_741C = pEntry.field_4;
                    }
                    if (m_animation.IsBeginningOfSequence()
                        && field_7101
                        && !field_73EC.IsEmpty()) {
                        INT nIndex = rand() % field_73EC.GetCount();
                        POSITION pos = field_73EC.GetHeadPosition();
                        while (pos != NULL && nIndex != 0) {
                            nIndex--;
                            field_73EC.GetNext(pos);
                        }
                        CGameSpriteSoundEntry& pEntry = field_73EC.GetAt(pos);
                        field_7428 = pEntry.field_0;
                        field_742C = pEntry.field_4;
                    }
                    if (field_1C != 0) {
                        pGame->GetObjectArray()->Delete(field_1C,
                            CGameObjectArray::THREAD_ASYNCH,
                            NULL,
                            INFINITE);
                        field_1C = 0;
                    }
                    m_nSequence = CGAMESPRITE_SEQ_DIE;
                    if (m_animation.IsEndOfSequence()) {
                        if (m_bBloodFlashOn == TRUE) {
                            m_bBloodFlashOn = FALSE;
                            m_nBloodFlashAmount = 128;
                            m_nDamageLocatorTime = 128;
                            pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(m_id), 1);
                        }
                        m_nTwitches = rand() % 4 + rand() % 4;
                        SetSequence(CGAMESPRITE_SEQ_TWITCH);
                    } else {
                        if (field_70FE
                            && m_animation.GetCurrentFrame() == field_741C) {
                            CSound cSound;
                            cSound.SetResRef(CResRef(field_7418), TRUE, TRUE);
                            cSound.m_nPitchVariance = 5;
                            cSound.m_nVolumeVariance = 20;
                            cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                            if (!cSound.GetLooping()) {
                                cSound.SetFireForget(TRUE);
                            }
                            cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
                        }
                        if (field_7101
                            && m_animation.GetCurrentFrame() == field_742C) {
                            CSound cSound;
                            cSound.SetResRef(CResRef(field_7428), TRUE, TRUE);
                            cSound.m_nPitchVariance = 5;
                            cSound.m_nVolumeVariance = 20;
                            cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                            if (!cSound.GetLooping()) {
                                cSound.SetFireForget(TRUE);
                            }
                            cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
                        }
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_HEAD_TURN:
                case CGAMESPRITE_SEQ_READY:
                    if (m_dialogWait <= 0
                        || m_typeAI.GetEnemyAlly() >= CAIObjectType::EA_EVILCUTOFF
                        || !m_bAllowDialogInterrupt
                        || m_derivedStats.m_bIgnoreDialogPause) {
                        AIUpdateWalk();
                    }

                    // NOTE: Uninline.
                    SetIdleSequence();

                    if (m_animation.IsBeginningOfSequence()
                        && field_70FF
                        && m_nSequence == CGAMESPRITE_SEQ_HEAD_TURN
                        && !field_73B4.IsEmpty()) {
                        INT nIndex = rand() % field_73B4.GetCount();
                        POSITION pos = field_73B4.GetHeadPosition();
                        while (pos != NULL && nIndex != 0) {
                            nIndex--;
                            field_73B4.GetNext(pos);
                        }
                        CGameSpriteSoundEntry& pEntry = field_73B4.GetAt(pos);
                        field_7420 = pEntry.field_0;
                        field_7424 = pEntry.field_4;
                    }
                    if (pGame->GetVisibleArea() == m_pArea
                        && field_70FF
                        && m_animation.GetCurrentFrame() == field_7424
                        && m_nSequence == CGAMESPRITE_SEQ_HEAD_TURN
                        && !m_baseStats.field_294) {
                        if (m_animation.GetCurrentFrame() == field_740C) {
                            CSound cSound;
                            cSound.SetResRef(field_7420, TRUE, TRUE);
                            cSound.m_nPitchVariance = 5;
                            cSound.m_nVolumeVariance = 20;
                            cSound.SetChannel(13, reinterpret_cast<DWORD_PTR>(m_pArea));
                            if (!cSound.GetLooping()) {
                                cSound.SetFireForget(TRUE);
                            }
                            cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
                        }
                    }
                    m_animation.IncrementFrame();
                    break;
                case CGAMESPRITE_SEQ_SHOOT:
                    if (m_animation.IsEndOfSequence()) {
                        // NOTE: Uninline.
                        SetIdleSequence();
                    } else {
                        if (m_animation.GetCurrentFrame() == field_740C) {
                            CSound cSound;
                            cSound.SetResRef(field_7408, TRUE, TRUE);
                            cSound.m_nPitchVariance = 5;
                            cSound.m_nVolumeVariance = 20;
                            cSound.SetChannel(3, reinterpret_cast<DWORD_PTR>(m_pArea));
                            if (!cSound.GetLooping()) {
                                cSound.SetFireForget(TRUE);
                            }
                            cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
                        }
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_TWITCH:
                    if (field_1C != 0) {
                        pGame->GetObjectArray()->Delete(field_1C,
                            CGameObjectArray::THREAD_ASYNCH,
                            NULL,
                            INFINITE);
                        field_1C = 0;
                    }
                    if (m_nTwitches != 0) {
                        if (m_animation.IsEndOfSequence()) {
                            if (rand() % 10 == 0) {
                                m_nTwitches--;
                            }
                        } else {
                            m_animation.IncrementFrame();
                        }
                    } else {
                        if ((pGame->GetCharacterPortraitNum(m_id) != -1
                                && (m_derivedStats.m_generalState & STATE_DEAD) != 0)
                            || (pGame->GetCharacterPortraitNum(m_id) != -1
                                && (m_derivedStats.m_generalState & STATE_STONE_DEATH) != 0)) {
                            if (InControl()) {
                                RemoveFromArea();
                                if (m_pArea->m_nCharacters == 0) {
                                    pGame->SelectCharacter(pGame->GetProtagonist(), FALSE);
                                    pGame->SelectToolbar();
                                    if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->GetScreenWorld()
                                        && g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == pGame->GetCharacterPortraitNum(m_id)) {
                                        g_pBaldurChitin->GetActiveEngine()->OnPortraitLClick(pGame->GetCharacterPortraitNum(pGame->GetProtagonist()));
                                    }
                                }
                            }
                        }
                    }
                    break;
                case CGAMESPRITE_SEQ_WALK:
                    AIUpdateFly();

                    if (m_dialogWait > 0
                        && m_typeAI.GetEnemyAlly() < CAIObjectType::EA_EVILCUTOFF
                        && m_bAllowDialogInterrupt
                        && !m_derivedStats.m_bIgnoreDialogPause) {
                        if (m_nSequence != GetIdleSequence()) {
                            CMessage* message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()),
                                m_id,
                                m_id);
                            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                        }
                    } else {
                        AIUpdateWalk();
                    }

                    if (m_walkBackwards || m_turningAbout) {
                        m_animation.DecrementFrame();
                    } else {
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_EMERGE:
                    if (m_animation.IsEndOfSequence()) {
                        // NOTE: Uninline.
                        SetIdleSequence();
                    } else {
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_HIDE:
                    if (m_animation.IsEndOfSequence()) {
                        // NOTE: Uninline.
                        SetIdleSequence();
                    } else {
                        m_animation.IncrementFrame();
                    }
                    break;
                case CGAMESPRITE_SEQ_SLEEP:
                    if (field_1C != 0) {
                        pGame->GetObjectArray()->Delete(field_1C,
                            CGameObjectArray::THREAD_ASYNCH,
                            NULL,
                            INFINITE);
                        field_1C = 0;
                    }
                    m_nSequence = CGAMESPRITE_SEQ_SLEEP;
                    if (m_animation.IsEndOfSequence()) {
                        if (m_bBloodFlashOn == TRUE) {
                            m_bBloodFlashOn = FALSE;
                            m_nBloodFlashAmount = 128;
                            m_nDamageLocatorTime = 128;
                            pGame->UpdatePortrait(pGame->GetCharacterPortraitNum(m_id), 1);
                        }
                        m_nTwitches = 0;
                    } else {
                        m_animation.IncrementFrame();
                    }
                    break;
                default:
                    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                    // __LINE__: 3274
                    UTIL_ASSERT(FALSE);
                }

                if (pGame->GetCharacterPortraitNum(m_id) != CGameObjectArray::INVALID_INDEX) {
                    if ((m_derivedStats.m_generalState & STATE_DEAD) == 0
                        && m_typeAI.GetRace() == CAIOBJECTTYPE_R_GNOME
                        && m_typeAI.GetSubRace() == CAIOBJECTTYPE_SUBRACE_GNOME_DEEP) {
                        if (InControl()
                            && (m_baseStats.m_generalState & STATE_NONDETECTION) == 0) {
                            ITEM_EFFECT effect;
                            CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_NONDETECTION);

                            CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect,
                                GetPos(),
                                m_id,
                                CPoint(-1, -1));

                            CMessage* message = new CMessageAddEffect(pEffect,
                                m_id,
                                m_id);
                            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                        }

                        // NOTE: Uninline.
                        AddPortraitIcon(31);
                    }

                    BOOL bHavePaladinNearby = FALSE;
                    for (SHORT nPortrait = 0; nPortrait < pGame->GetNumCharacters() && !bHavePaladinNearby; nPortrait++) {
                        LONG nCharacterId = pGame->GetCharacterId(nPortrait);

                        if (nCharacterId != CGameObjectArray::INVALID_INDEX) {
                            CGameSprite* pPaladin;

                            BYTE rc;
                            do {
                                rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                                    CGameObjectArray::THREAD_ASYNCH,
                                    reinterpret_cast<CGameObject**>(&pPaladin),
                                    INFINITE);
                            } while (rc != CGameObjectArray::SUCCESS);

                            CPoint ptPaladinPos = pPaladin->GetPos();
                            CPoint ptPos = GetPos();
                            INT nPaladinLevel = pPaladin->GetDerivedStats()->GetClassLevel(CAIOBJECTTYPE_C_PALADIN);
                            BOOL bPaladinAnimate = pPaladin->Animate();
                            BOOL bAnimate = Animate();

                            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);

                            if (bPaladinAnimate
                                && bAnimate
                                && nPaladinLevel >= 2
                                && (ptPos.x - ptPaladinPos.x) * (ptPos.x - ptPaladinPos.x) + (ptPos.y - ptPaladinPos.y) * (ptPos.y - ptPaladinPos.y) <= 10000) {
                                bHavePaladinNearby = TRUE;
                            }
                        }
                    }

                    if (bHavePaladinNearby == TRUE) {
                        // NOTE: Uninline.
                        AddPortraitIcon(67);
                    } else {
                        // NOTE: Uninline.
                        RemovePortraitIcon(67);
                    }
                }
            }
        }
    }
}

// 0x6F2D80
CGameSprite::~CGameSprite()
{
    POSITION pos;

    pos = m_lstBlood.GetHeadPosition();
    while (pos != NULL) {
        CBlood* blood = m_lstBlood.GetNext(pos);
        if (blood != NULL) {
            delete blood;
        }
    }
    m_lstBlood.RemoveAll();

    pos = m_equipedEffectList.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* effect = m_equipedEffectList.GetNext(pos);
        if (effect != NULL) {
            delete effect;
        }
    }
    m_equipedEffectList.RemoveAll();

    pos = m_timedEffectList.GetHeadPosition();
    while (pos != NULL) {
        CGameEffect* effect = m_timedEffectList.GetNext(pos);
        if (effect != NULL) {
            delete effect;
        }
    }
    m_timedEffectList.RemoveAll();

    pos = m_persistantEffects.GetHeadPosition();
    while (pos != NULL) {
        CPersistantEffect* effect = m_persistantEffects.GetNext(pos);
        if (effect != NULL) {
            delete effect;
        }
    }
    m_persistantEffects.RemoveAll();

    ClearStoredPaths();

    // NOTE: Uninline.
    DropPath();

    if (m_currentSearchRequest != NULL) {
        delete m_currentSearchRequest;
    }

    if (m_curProjectile != NULL) {
        delete m_curProjectile;
    }
    m_curProjectile = NULL;

    if (m_pSpriteEffectArray != NULL) {
        delete m_pSpriteEffectArray;
        delete m_pSpriteEffectArrayPosition;
    }

    if (m_curSpell != NULL) {
        delete m_curSpell;
        m_curSpell = NULL;
    }

    if (m_curItem != NULL) {
        m_curItem = NULL;
    }

    if (m_pLocalVariables != NULL) {
        delete m_pLocalVariables;
    }

    m_portraitIconVidCell.CancelRequest();

    ClearMarshal(FALSE);
}

// 0x6F3A30
void CGameSprite::AddBlood(SHORT nHeight, SHORT nDirection, SHORT nType)
{
    if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nGoreOption == 0) {
        if (nType != CBlood::EXPLODING_DEATH) {
            StartSpriteEffect(0, static_cast<BYTE>(nType), 0, 1);
        }
    } else {
        if (nType == CBlood::EXPLODING_DEATH) {
            CRect rFx;
            CPoint ptReference;

            // NOTE: Uninline.
            m_animation.CalculateFxRect(rFx, ptReference, m_posZ);

            CBlood* pBlood = new CBlood(m_pArea,
                m_pos.x,
                m_pos.y,
                5 * rFx.Height() / 10,
                nDirection,
                nType,
                rFx.Height());
            m_lstBlood.AddTail(pBlood);
        } else {
            StartSpriteEffect(0, static_cast<BYTE>(nType), 0, 1);
        }
    }
}

// 0x6F9040
void CGameSprite::AIUpdateWalk()
{
    CSingleLock pathLock(&(g_pBaldurChitin->GetObjectGame()->field_1B58), FALSE);
    CMessage* message;

    if (m_pPath == NULL && m_currentSearchRequest != NULL) {
        pathLock.Lock(INFINITE);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 3435
        UTIL_ASSERT(m_currentSearchRequest->m_serviceState != CSearchRequest::STATE_STALE);

        if (m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_WAITING) {
            if (m_currentSearchRequest->m_collisionDelay > 1) {
                m_currentSearchRequest->m_collisionDelay--;
                pathLock.Unlock();
                return;
            }

            if (m_currentSearchRequest->m_collisionDelay == 1) {
                m_currentSearchRequest->m_collisionDelay = 0;
                if (m_currentSearchRequest->m_frontList == CSearchRequest::LIST_FRONT) {
                    g_pBaldurChitin->GetObjectGame()->m_searchRequests.AddTail(m_currentSearchRequest);
                    g_pBaldurChitin->GetObjectGame()->m_searchRequestListEmpty = FALSE;
                } else {
                    g_pBaldurChitin->GetObjectGame()->m_searchRequestsBack.AddTail(m_currentSearchRequest);
                }
                ReleaseSemaphore(g_pBaldurChitin->GetObjectGame()->m_hSearchThread, 1, NULL);
                pathLock.Unlock();
                return;
            }
        }

        if (m_currentSearchRequest->m_frontList == CSearchRequest::LIST_BACK_PROMOTE
            && m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_WAITING) {
            m_currentSearchRequest->m_serviceState = CSearchRequest::STATE_STALE;
            m_currentSearchRequest = new CSearchRequest(*m_currentSearchRequest);
            pathLock.Unlock();
            if (m_currentSearchRequest != NULL) {
                SetTarget(m_currentSearchRequest, FALSE, CSearchRequest::LIST_FRONT);
            } else {
                SetSequence(CGAMESPRITE_SEQ_HEAD_TURN);
            }
            return;
        }

        if (m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_ERROR
            || m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_NO_TARGET) {
            if (m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_NO_TARGET) {
                m_pathSearchInvalidDest = TRUE;
            }
            pathLock.Unlock();
            delete m_currentSearchRequest;
            m_currentSearchRequest = NULL;
            SetSequence(CGAMESPRITE_SEQ_HEAD_TURN);
            return;
        }

        if (m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_DONE) {
            LONG* pPath = m_currentSearchRequest->m_pPath;
            SHORT nPath = m_currentSearchRequest->m_nPath;

            m_currentSearchRequest->m_pPath = NULL;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 3490
            UTIL_ASSERT(nPath > 0 && pPath != NULL);

            CPoint pt(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
            message = new CMessageSetPath(CPathSearch::PointToPosition(&pt),
                pPath,
                nPath,
                TRUE,
                m_curDest,
                m_id,
                m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            if (m_currentSearchRequest->m_searchRc != 0 || nPath == 1) {
                delete m_currentSearchRequest;
                m_currentSearchRequest = NULL;

                if (nPath == 1) {
                    pathLock.Unlock();

                    message = new CMessageDropPath(m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                    if (m_nSequence != GetIdleSequence()) {
                        message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()),
                            m_id,
                            m_id);
                        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                    }

                    delete pPath;
                    return;
                }
            } else {
                // NOTE: Uninline.
                CPathSearch::PositionToPoint(pPath[nPath - 1], &(m_currentSearchRequest->m_sourcePt));
                SetTarget(m_currentSearchRequest, FALSE, CSearchRequest::LIST_BACK_PROMOTE);
            }

            SetSequence(CGAMESPRITE_SEQ_WALK);
            InitializeWalkingSound();

            if (m_followLeader) {
                g_pBaldurChitin->GetObjectGame()->GetGroup()->HandleFollowPath(pPath,
                    nPath,
                    m_curDest,
                    m_pos,
                    m_followLeaderAdditive);
                m_followLeader = FALSE;
                m_followLeaderAdditive = FALSE;
            }

            if (m_followLeaderNext != CGameObjectArray::INVALID_INDEX) {
                message = new CMessageStartFollow(m_id, m_followLeaderNext);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                m_followLeaderNext = CGameObjectArray::INVALID_INDEX;
            }

            delete pPath;
        }

        pathLock.Unlock();
    }

    // No path (none yet, none found, or dropped): no step (0x6F95A6, on
    // every route).
    if (m_pPath == NULL) {
        return;
    }

    if ((m_posDest.x / 8 == (m_pos.x + 4) / 8 && m_posDest.y / 6 == (m_pos.y + 3) / 6)
        || (m_pos.y - m_posDest.y) * (m_pos.y - m_posDest.y) + (m_pos.x - m_posDest.x) * (m_pos.x - m_posDest.x) >= (m_posOld.y - m_posDest.y) * (m_posOld.y - m_posDest.y) + (m_posOld.x - m_posDest.x) * (m_posOld.x - m_posDest.x)) {
        if (m_currPath == m_nPath) {
            delete m_pPath;
            m_pPath = NULL;

            // NOTE: Uninline.
            SetIdleSequence();

            if (!m_bVisibilityUpdated) {
                m_pArea->m_visibility.UpDate(m_posLastVisMapEntry,
                    m_pos,
                    m_id,
                    m_visibleTerrainTable);
                m_posLastVisMapEntry = m_pos;
                m_bVisibilityUpdated = TRUE;
            }
            return;
        }

        // The next node: the sprite heads for its centre. The distance and
        // the step are taken in the 4/3-scaled space (a search square is
        // 16 by 16 there), as m_posExact is.
        CPoint goal;
        CPathSearch::PositionToPoint(m_pPath[m_currPath], &goal);
        m_currPath++;
        m_posDest.x = goal.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2;
        m_posDest.y = goal.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2;

        LONG dx = m_posDest.x - m_pos.x;
        LONG dy = goal.y * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2 - 4 * m_pos.y / 3;

        // NOTE: __ftol truncates the square root.
        int scale = static_cast<int>(sqrt(static_cast<double>(dx * dx + dy * dy)));
        if (scale == 0) {
            // NOTE: Uninline.
            DropPath();

            // NOTE: Uninline.
            SetIdleSequence();

            return;
        }

        if (m_animation.GetMoveScale() < 9) {
            m_posDelta.x = dx * m_animation.GetMoveScale() * (1 << EXACT_SCALE) / scale;
            m_posDelta.y = dy * m_animation.GetMoveScale() * (1 << EXACT_SCALE) / scale;
        } else {
            if (scale < m_animation.GetMoveScale()) {
                scale = 1;
            } else {
                scale = (m_animation.GetMoveScale() / 2 + scale) / m_animation.GetMoveScale();
            }

            m_posDelta.x = dx * (1 << EXACT_SCALE) / scale;
            m_posDelta.y = dy * (1 << EXACT_SCALE) / scale;
        }

        SetDirection(m_posDest);
    }

    m_turningAbout = !m_walkBackwards
        && abs(m_nNewDirection - m_nDirection) >= 5
        && abs(m_nNewDirection - m_nDirection) <= 8;

    SetSequence(CGAMESPRITE_SEQ_WALK);

    CPoint posExactOld(m_posExact);

    m_posExact += m_posDelta;
    m_posOld = m_pos;
    m_pos.x = m_posExact.x >> EXACT_SCALE;
    m_pos.y = (3 * m_posExact.y / 4) >> EXACT_SCALE;

    CPoint ptOldSearch(m_posOld.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_posOld.y / CPathSearch::GRID_SQUARE_SIZEY);
    CPoint ptSearch(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
        m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);

    if (ptOldSearch == ptSearch
        || m_animation.GetListType() == LIST_FLIGHT) {
        if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
            && (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
                || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1)
            && m_pArea == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()) {
            INT nSndWalk = m_nSndWalk % m_animation.GetSndWalkFreq();
            m_nSndWalk++;

            if (nSndWalk == 0) {
                BYTE currSndWalk = m_currSndWalk;
                m_currSndWalk = (m_currSndWalk + 1) % 2;

                CPoint ear;
                LONG earZ;
                g_pBaldurChitin->cSoundMixer.GetListenPosition(ear, earZ);

                m_sndWalk[m_currSndWalk].Stop();
                m_sndWalk[m_currSndWalk].SetResRef(m_sndWalk[currSndWalk].GetResRef(), TRUE, TRUE);

                LONG priority = max(99 - 99 * ((ear.y - m_pos.y) * (ear.y - m_pos.y) / 144 + (ear.x - m_pos.x) * (ear.x - m_pos.x) / 256) / 6400, 0);
                m_sndWalk[m_currSndWalk].SetPriority(static_cast<BYTE>(priority));

                m_sndWalk[m_currSndWalk].Play(m_pos.x, m_pos.y, m_posZ, FALSE);
            }
        }

        m_sndSpriteEffect.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
        m_sndReady.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
    } else {
        m_pArea->m_search.RemoveObject(ptOldSearch,
            m_typeAI.GetEnemyAlly(),
            m_animation.GetPersonalSpace(),
            field_54A8,
            field_7430);

        if (InControl()
            && m_pArea->m_search.GetMobileCost(ptSearch, m_terrainTable, m_animation.GetPersonalSpace(), TRUE) == CPathSearch::COST_IMPASSABLE
            && !ClearBumpPath(ptOldSearch, ptSearch)) {
            if (!m_baseStats.field_294) {
                m_pArea->m_search.AddObject(ptOldSearch,
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);

                m_posExact = posExactOld;
                m_pos = m_posOld;

                if (m_currentSearchRequest != NULL) {
                    pathLock.Lock(INFINITE);

                    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                    // __LINE__: 3776
                    UTIL_ASSERT(m_currentSearchRequest->m_serviceState != CSearchRequest::STATE_STALE);

                    if ((m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_WAITING
                            && m_currentSearchRequest->m_collisionDelay == 0)
                        || m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_PROCESSING) {
                        m_currentSearchRequest->m_serviceState = CSearchRequest::STATE_STALE;
                        m_currentSearchRequest = new CSearchRequest(*m_currentSearchRequest);
                        if (m_currentSearchRequest != NULL) {
                            SetTarget(m_currentSearchRequest, TRUE, CSearchRequest::LIST_FRONT);
                        } else {
                            SetSequence(CGAMESPRITE_SEQ_HEAD_TURN);
                        }
                    } else {
                        if (m_currentSearchRequest->m_pPath != NULL) {
                            delete m_currentSearchRequest->m_pPath;
                            m_currentSearchRequest->m_pPath = NULL;
                        }
                        SetTarget(m_currentSearchRequest, TRUE, CSearchRequest::LIST_FRONT);
                    }

                    pathLock.Unlock();
                } else {
                    CPoint pt;
                    CPathSearch::PositionToPoint(m_pPath[m_nPath - 1], &pt);
                    SetTarget(CPoint(pt.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                                  pt.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2),
                        TRUE);
                }
            }
        } else {
            if (!m_baseStats.field_294) {
                m_pArea->m_search.AddObject(ptSearch,
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);
            }

            char* pSndWalk = m_animation.GetSndWalk(m_pArea->m_search.GetTableIndex(ptOldSearch));
            if (pSndWalk != NULL) {
                m_sndWalk[m_currSndWalk].SetResRef(CResRef(pSndWalk), TRUE, TRUE);

                if (*pSndWalk != '\0') {
                    delete pSndWalk;
                }
            }

            if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
                && (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
                    || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1)
                && m_pArea == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()) {
                INT nSndWalk = m_nSndWalk % m_animation.GetSndWalkFreq();
                m_nSndWalk++;

                if (nSndWalk == 0) {
                    BYTE currSndWalk = m_currSndWalk;
                    m_currSndWalk = (m_currSndWalk + 1) % 2;

                    CPoint ear;
                    LONG earZ;
                    g_pBaldurChitin->cSoundMixer.GetListenPosition(ear, earZ);

                    m_sndWalk[m_currSndWalk].Stop();
                    m_sndWalk[m_currSndWalk].SetResRef(m_sndWalk[currSndWalk].GetResRef(), TRUE, TRUE);

                    LONG priority = max(99 - 99 * ((ear.y - m_pos.y) * (ear.y - m_pos.y) / 144 + (ear.x - m_pos.x) * (ear.x - m_pos.x) / 256) / 6400, 0);
                    m_sndWalk[m_currSndWalk].SetPriority(static_cast<BYTE>(priority));

                    m_sndWalk[m_currSndWalk].Play(m_pos.x, m_pos.y, m_posZ, FALSE);
                }
            }

            m_sndSpriteEffect.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
            m_sndReady.SetCoordinates(m_pos.x, m_pos.y, m_posZ);
        }
    }

    if ((m_posLastVisMapEntry.x / CVisibilityMap::SQUARE_SIZEX != m_pos.x / CVisibilityMap::SQUARE_SIZEX
            || m_posLastVisMapEntry.y / CVisibilityMap::SQUARE_SIZEY != m_pos.y / CVisibilityMap::SQUARE_SIZEY)
        && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1
        && (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC || InControl())) {
        SHORT nTableIndex;
        if (m_pArea->m_search.GetLOSCost(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX, m_pos.y / CPathSearch::GRID_SQUARE_SIZEY), m_terrainTable, nTableIndex, FALSE) != CPathSearch::COST_IMPASSABLE) {
            m_pArea->m_visibility.UpDate(m_posLastVisMapEntry,
                m_pos,
                m_id,
                m_visibleTerrainTable);
            m_posLastVisMapEntry = m_pos;
            m_bVisibilityUpdated = TRUE;
            if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
                && g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineMap) {
                SHORT nPortrait = g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id);
                CUIControlButtonMapAreaMap* pMap = static_cast<CUIControlButtonMapAreaMap*>(g_pBaldurChitin->m_pEngineMap->GetManager()->GetPanel(2)->GetControl(2));
                pMap->field_7DA |= 1 << nPortrait;
            }
        } else {
            m_bVisibilityUpdated = FALSE;
        }
    }
}

// 0x6FA810
void CGameSprite::SetPath(LONG* pPath, SHORT nPath)
{
    CPoint start;
    CPoint goal;

    // NOTE: Uninline.
    DropPath();

    m_nPath = nPath;
    m_pPath = pPath;
    m_currPath = 1;
    m_posDest.x = m_pos.x + 4;
    m_walkBackwards = FALSE;
    m_posDest.y = m_pos.y + 3;

    // NOTE: Uninline.
    CPathSearch::PositionToPoint(m_pPath[0], &start);

    // NOTE: Uninline.
    CPathSearch::PositionToPoint(m_pPath[nPath - 1], &goal);

    // A short path (within 4 squares each way, at most 4 nodes) is walked
    // backwards.
    if (abs(goal.y - start.y) <= 4 && abs(goal.x - start.x) <= 4 && nPath <= 4) {
        m_walkBackwards = TRUE;
    }

    SetSequence(CGAMESPRITE_SEQ_WALK);
}

// 0x6FA900
BOOL CGameSprite::ClearBumpPath(const CPoint& start, const CPoint& goal)
{
    if (GetAIType().m_nEnemyAlly > CAIObjectType::EA_GOODCUTOFF) {
        return FALSE;
    }

    CSearchBitmap* pSearch = &(m_pArea->m_search);
    CTypedPtrList<CPtrList, LONG*> targets(10);
    BOOL bTargetsFound = FALSE;

    // The squares around a bumped creature in the order they are tried:
    // first the square in the direction of this creature's step, last the
    // one it comes from. Index (dy + 1) * 3 + (dx + 1); 4, the creature's
    // own square, is never tried.
    BYTE order[9];
    order[0] = static_cast<BYTE>((goal.x - start.x) + (goal.y - start.y) * 3 + 4);
    order[1] = 1;
    order[2] = 2;
    order[3] = 3;
    order[4] = 255;
    order[5] = 5;
    order[6] = 6;
    order[7] = 7;
    // NOTE: The original writes these two on the stack unchecked; a start
    // and goal more than one square apart would write outside the table
    // (the callers pass neighbouring squares). Bounded here.
    INT nForward = (goal.y - start.y) * 3 + (goal.x - start.x) + 4;
    if (nForward >= 0 && nForward < 9) {
        order[nForward] = 0;
    }
    order[8] = static_cast<BYTE>((start.x - goal.x) + (start.y - goal.y) * 3 + 4);
    INT nBackward = (start.y - goal.y) * 3 + (start.x - goal.x) + 4;
    if (nBackward >= 0 && nBackward < 9) {
        order[nBackward] = 8;
    }

    if (start.x == goal.x && start.y == goal.y) {
        return TRUE;
    }

    SHORT nTableIndex;
    if (pSearch->GetCost(goal, GetTerrainTable(), m_animation.GetPersonalSpace(), nTableIndex, FALSE) == CPathSearch::COST_IMPASSABLE) {
        return FALSE;
    }

    INT radius = (m_animation.GetPersonalSpace() - 2) / 2;
    for (SHORT dx = static_cast<SHORT>(-radius); dx <= static_cast<SHORT>(radius); dx++) {
        if (dx + goal.x < 0 || dx + goal.x >= pSearch->m_GridSquareDimensions.cx) {
            continue;
        }

        for (SHORT dy = static_cast<SHORT>(-radius); dy <= static_cast<SHORT>(radius); dy++) {
            // NOTE: The original bounds the row by the width (`cmp ebp,
            // [search+0xE6]` at 0x6FAB39), not the height.
            if (dy + goal.y < 0 || dy + goal.y >= pSearch->m_GridSquareDimensions.cx) {
                continue;
            }

            if (abs(dx) + abs(dy) > static_cast<SHORT>(radius) + 1) {
                continue;
            }

            BYTE nBumps = pSearch->m_pDynamicCost[(dy + goal.y) * pSearch->m_GridSquareDimensions.cx + goal.x + dx] >> 1;
            if (nBumps > 7) {
                return FALSE;
            }

            if (!bTargetsFound) {
                bTargetsFound = TRUE;
                CPoint center(goal.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                    goal.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2);
                m_pArea->GetAllInRange(m_posVertList,
                    center,
                    CAIObjectType::ANYONE,
                    static_cast<SHORT>((m_animation.GetPersonalSpace() + 11) / 2 * CPathSearch::GRID_SQUARE_SIZEX),
                    m_terrainTable,
                    targets,
                    FALSE,
                    FALSE);
            }

            POSITION pos = targets.GetHeadPosition();
            while (pos != NULL) {
                if (nBumps == 0) {
                    break;
                }

                POSITION posCurrent = pos;
                LONG nBumpedId = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos)));

                CGameSprite* pBumped;
                BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nBumpedId,
                    CGameObjectArray::THREAD_ASYNCH,
                    reinterpret_cast<CGameObject**>(&pBumped),
                    INFINITE);
                if (rc != CGameObjectArray::SUCCESS) {
                    continue;
                }

                if (pBumped->field_54A8 && !pBumped->field_54B8) {
                    CPoint ptBumpedSquare(pBumped->m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                        pBumped->m_pos.y / CPathSearch::GRID_SQUARE_SIZEY);
                    SHORT bumpedRadius = pBumped->m_animation.GetPersonalSpace() >> 1;

                    BOOL bOnSquare = FALSE;
                    for (SHORT ddx = -bumpedRadius; ddx <= bumpedRadius && !bOnSquare; ddx++) {
                        for (SHORT ddy = -bumpedRadius; ddy <= bumpedRadius; ddy++) {
                            if (ptBumpedSquare.x + ddx == goal.x + dx
                                && ptBumpedSquare.y + ddy == goal.y + dy) {
                                bOnSquare = TRUE;
                                break;
                            }
                        }
                    }

                    if (bOnSquare) {
                        CPoint ptBumped(pBumped->GetPos().x / CPathSearch::GRID_SQUARE_SIZEX,
                            pBumped->GetPos().y / CPathSearch::GRID_SQUARE_SIZEY);

                        // This creature stands on the goal while the bumped
                        // one looks for a square.
                        pSearch->AddObject(goal,
                            m_typeAI.GetEnemyAlly(),
                            m_animation.GetPersonalSpace(),
                            field_54A8,
                            field_7430);

                        do {
                            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(nBumpedId,
                                CGameObjectArray::THREAD_ASYNCH,
                                reinterpret_cast<CGameObject**>(&pBumped),
                                INFINITE);
                        } while (rc == CGameObjectArray::SHARED);

                        if (rc == CGameObjectArray::SUCCESS) {
                            pSearch->RemoveObject(ptBumped,
                                pBumped->GetAIType().m_nEnemyAlly,
                                pBumped->m_animation.GetPersonalSpace(),
                                pBumped->field_54A8,
                                pBumped->field_7430);

                            BYTE nOrder;
                            for (nOrder = 0; nOrder < 9; nOrder++) {
                                BYTE nSquare = order[nOrder];
                                if (nSquare == 255) {
                                    continue;
                                }

                                CPoint ptTry(nSquare % 3 - 1 + ptBumped.x, nSquare / 3 - 1 + ptBumped.y);
                                SHORT nTryTableIndex;
                                if (pSearch->GetCost(ptTry, pBumped->GetTerrainTable(), pBumped->m_animation.GetPersonalSpace(), nTryTableIndex, TRUE) == CPathSearch::COST_IMPASSABLE) {
                                    continue;
                                }

                                pSearch->AddObject(ptBumped,
                                    pBumped->GetAIType().m_nEnemyAlly,
                                    pBumped->m_animation.GetPersonalSpace(),
                                    pBumped->field_54A8,
                                    pBumped->field_7430);
                                pBumped->JumpToPoint(CPoint(ptTry.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                                                         ptTry.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2),
                                    TRUE);
                                if (!pBumped->m_bBumped) {
                                    pBumped->m_bBumped = TRUE;
                                    pBumped->m_ptBumpedFrom = ptBumped;
                                }
                                targets.RemoveAt(posCurrent);
                                nBumps--;
                                break;
                            }

                            if (nOrder == 9) {
                                // No free square: the bumped creature
                                // bumps in turn.
                                field_54B8 = TRUE;

                                for (nOrder = 0; nOrder < 9; nOrder++) {
                                    BYTE nSquare = order[nOrder];
                                    if (nSquare == 255) {
                                        continue;
                                    }

                                    CPoint ptTry(nSquare % 3 - 1 + ptBumped.x, nSquare / 3 - 1 + ptBumped.y);
                                    if (!pBumped->ClearBumpPath(ptBumped, ptTry)) {
                                        continue;
                                    }

                                    pSearch->AddObject(ptBumped,
                                        pBumped->GetAIType().m_nEnemyAlly,
                                        pBumped->m_animation.GetPersonalSpace(),
                                        pBumped->field_54A8,
                                        pBumped->field_7430);
                                    pBumped->JumpToPoint(CPoint(ptTry.x * CPathSearch::GRID_SQUARE_SIZEX + CPathSearch::GRID_SQUARE_SIZEX / 2,
                                                             ptTry.y * CPathSearch::GRID_SQUARE_SIZEY + CPathSearch::GRID_SQUARE_SIZEY / 2),
                                        TRUE);
                                    if (!pBumped->m_bBumped) {
                                        pBumped->m_bBumped = TRUE;
                                        pBumped->m_ptBumpedFrom = ptBumped;
                                    }
                                    targets.RemoveAt(posCurrent);
                                    nBumps--;
                                    break;
                                }

                                field_54B8 = FALSE;

                                if (nOrder == 9) {
                                    pSearch->AddObject(ptBumped,
                                        pBumped->GetAIType().m_nEnemyAlly,
                                        pBumped->m_animation.GetPersonalSpace(),
                                        pBumped->field_54A8,
                                        pBumped->field_7430);
                                }
                            }

                            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(nBumpedId,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }

                        pSearch->RemoveObject(goal,
                            m_typeAI.GetEnemyAlly(),
                            m_animation.GetPersonalSpace(),
                            field_54A8,
                            field_7430);
                    }
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nBumpedId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }

            if (nBumps != 0) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

// 0x6FB440
BOOL CGameSprite::sub_6FB440()
{
    if (!InControl()) {
        return FALSE;
    }

    if (m_animation.GetMoveScale() == 0
        || m_typeAI.GetEnemyAlly() == CAIObjectType::EA_ENEMY
        || !Animate()) {
        return FALSE;
    }

    if (m_typeAI.GetEnemyAlly() > CAIObjectType::EA_GOODCUTOFF
        || m_curAction.m_actionID != CAIAction::NO_ACTION) {
        // 0x8F94F0
        static SHORT word_8F94F0[] = {
            CAIAction::RANDOMWALK,
            CAIAction::RANDOMWALKCONTINUOUS,
            CAIAction::WAIT,
            CAIAction::FACE,
            CAIAction::ATTACK,
            CAIAction::GROUPATTACK,
            CAIAction::ATTACKNOSOUND,
            CAIAction::ATTACKONEROUND,
            CAIAction::ATTACKREEVALUATE,
        };

        // 0x8F9504
        static SHORT word_8F9504[] = {
            CAIAction::RANDOMWALK,
            CAIAction::RANDOMWALKCONTINUOUS,
            CAIAction::WAIT,
            CAIAction::FACE,
        };

        WORD wType = 0;
        CItem* pItem = m_equipment.m_items[m_equipment.m_selectedWeapon];
        if (pItem != NULL) {
            pItem->Demand();
            const ITEM_ABILITY* ability = pItem->GetAbility(m_equipment.m_selectedWeaponAbility);
            if (ability != NULL) {
                wType = ability->type;
            }
            pItem->Release();
        }

        if (wType == 4 || wType == 2) {
            for (int index = 0; index < sizeof(word_8F94F0) / sizeof(word_8F94F0[0]); index++) {
                if (word_8F94F0[index] == m_curAction.m_actionID) {
                    return TRUE;
                }
            }
            return FALSE;
        } else {
            for (int index = 0; index < sizeof(word_8F9504) / sizeof(word_8F9504[0]); index++) {
                if (word_8F9504[index] == m_curAction.m_actionID) {
                    return TRUE;
                }
            }
            return FALSE;
        }
    }

    return TRUE;
}

// 0x6FB630
void CGameSprite::AIUpdateFly()
{
    if (m_fCurrCircleChange < m_fCircleChange) {
        double fCurrCircleChange = min(m_fCircleChange, m_fCurrCircleChange + 1.0);

        if (m_circleFacing != 0) {
            double fAngle = static_cast<double>(m_animation.GetMoveScale())
                    * static_cast<double>(m_circleFacing)
                    * fCurrCircleChange
                    / static_cast<double>(m_radius)
                + m_fDirectionOffset;

            m_pos.x = m_posOld.x + static_cast<LONG>(cos(fAngle) * static_cast<double>(m_radius));
            m_pos.y = m_posOld.y - static_cast<LONG>(sin(fAngle) * static_cast<double>(m_radius) * 3.0 / 4.0);

            if (m_circleFacing == FLY_LEFT) {
                if (m_pos != m_posOld) {
                    SetDirection((GetDirection(CPoint(m_pos.x, 4 * m_pos.y / 3),
                                      CPoint(m_posOld.x, 4 * m_posOld.y / 3))
                                     + 4)
                        % 16);
                } else {
                    SetDirection(m_nNewDirection);
                }
            } else {
                if (m_pos != m_posOld) {
                    SetDirection((GetDirection(CPoint(m_pos.x, 4 * m_pos.y / 3),
                                      CPoint(m_posOld.x, 4 * m_posOld.y / 3))
                                     + 12)
                        % 16);
                } else {
                    SetDirection(m_nNewDirection);
                }
            }
        } else {
            switch (m_nDirection) {
            case 0:
                m_pos.y = m_posOld.y + static_cast<LONG>(static_cast<double>(m_radius) * fCurrCircleChange / m_fCircleChange * 3.0 / 4.0);
                break;
            case 4:
                m_pos.x = m_posOld.x - static_cast<LONG>(static_cast<double>(m_radius) * fCurrCircleChange / m_fCircleChange);
                break;
            case 8:
                m_pos.y = m_posOld.y - static_cast<LONG>(static_cast<double>(m_radius) * fCurrCircleChange / m_fCircleChange * 3.0 / 4.0);
                break;
            case 12:
                m_pos.x = m_posOld.x + static_cast<LONG>(static_cast<double>(m_radius) * fCurrCircleChange / m_fCircleChange);
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 4554
                UTIL_ASSERT(FALSE);
            }
        }

        m_fCurrCircleChange += 1.0;
        if (m_fCurrCircleChange >= m_fCircleChange) {
            SetSequence(SEQ_READY);
        }
    }
}

// 0x6FBA50
void CGameSprite::GetNextWaypoint(CPoint* pt)
{
    if (m_pPath != NULL) {
        CPathSearch::PositionToPoint(m_pPath[m_currPath - 1], pt);

        pt->x *= CPathSearch::GRID_SQUARE_SIZEX;
        pt->x += CPathSearch::GRID_SQUARE_SIZEX / 2;

        pt->y *= CPathSearch::GRID_SQUARE_SIZEY;
        pt->y += CPathSearch::GRID_SQUARE_SIZEY / 2;
    } else {
        *pt = m_pos;
    }
}

// 0x6FBAD0
void CGameSprite::ChangeDirection()
{
    if (m_nNewDirection == m_nDirection) {
        return;
    }

    if (m_skipDeltaDirection > 0) {
        m_skipDeltaDirection -= 2;
        if (m_skipDeltaDirection <= 1) {
            m_nDirection = (m_nDirection + 2 * m_deltaDirection + 16) % 16;
        }
    }

    m_nDirection = (m_nDirection + m_deltaDirection + 16) % 16;
    if (m_nNewDirection != m_nDirection) {
        m_nDirection = (m_nDirection + m_deltaDirection + 16) % 16;
    }

    m_animation.ChangeDirection(m_nDirection);

    if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
        && (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
            || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1)
        && m_pArea == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()) {
        m_currSndArmor = 0;
        m_sndArmor[m_currSndArmor].Stop();

        // NOTE: Uninline.
        char* sndArmor = m_animation.GetSndArmor();

        if (sndArmor[0] != '\0') {
            // NOTE: Uninline.
            m_sndArmor[m_currSndArmor].SetResRef(CResRef(sndArmor), TRUE, TRUE);
            delete sndArmor;

            CPoint ear;
            LONG earZ;
            g_pBaldurChitin->cSoundMixer.GetListenPosition(ear, earZ);

            LONG priority = max(99 - 99 * ((ear.y - m_pos.y) * (ear.y - m_pos.y) / 144 + (ear.x - m_pos.x) * (ear.x - m_pos.x) / 256) / 6400, 0);
            m_sndArmor[m_currSndArmor].SetPriority(static_cast<BYTE>(priority));

            m_sndArmor[m_currSndArmor].Play(m_pos.x, m_pos.y, m_posZ, FALSE);
        }
    }
}

// 0x6FBF10
void CGameSprite::CheckIfVisible()
{
    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1
        || m_animation.GetListType() == LIST_FLIGHT) {
        if (Animate()) {
            m_canBeSeen = 4 * (VISIBLE_DELAY + 1);
            return;
        }
    }

    for (SHORT nPortrait = 0; nPortrait < g_pBaldurChitin->GetObjectGame()->GetNumCharacters(); nPortrait++) {
        CGameSprite* pSprite;

        BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait),
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
        if (rc == CGameObjectArray::SUCCESS) {
            if (pSprite->GetArea() == m_pArea
                && pSprite->Animate()
                && m_pArea->CheckLOS(m_pos, pSprite->GetPos(), m_visibleTerrainTable, TRUE)) {
                if (!m_bVisibleMonster
                    && m_typeAI.GetEnemyAlly() >= CAIObjectType::EA_EVILCUTOFF
                    && Animate()
                    && ((m_derivedStats.m_generalState & STATE_INVISIBLE) == 0 || (m_baseStats.field_2FC & 0x1) != 0)
                    && !m_baseStats.field_294
                    && (m_baseStats.m_flags & 0x8000) == 0) {
                    if (m_pArea->m_nVisibleMonster == 0) {
                        AutoPause(0x200);
                    }

                    m_bVisibleMonster = TRUE;
                    m_pArea->m_nVisibleMonster++;
                }

                if (m_canBeSeen == 0) {
                    CAITrigger becameVisible(CAITRIGGER_BECAMEVISIBLE, 0);
                    m_bSeenPartyBefore = TRUE;
                    CMessage* message = new CMessageSetTrigger(becameVisible, m_id, pSprite->GetId());
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }

                m_canBeSeen = 4 * (VISIBLE_DELAY + 1);

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);

                break;
            } else {
                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(g_pBaldurChitin->GetObjectGame()->GetCharacterId(nPortrait),
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }
}

// 0x6FF2F0
void CGameSprite::DropPath()
{
    if (m_pPath != NULL) {
        delete m_pPath;
        m_pPath = NULL;
    }
}

// 0x6FF320
void CGameSprite::DropSearchRequest()
{
    if (m_currentSearchRequest != NULL) {
        CSingleLock searchLock(&(g_pBaldurChitin->GetObjectGame()->field_1B58), TRUE);

        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 5590
        UTIL_ASSERT(m_currentSearchRequest->m_serviceState != CSearchRequest::STATE_STALE);

        if ((m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_WAITING
                && m_currentSearchRequest->m_collisionDelay == 0)
            || m_currentSearchRequest->m_serviceState == CSearchRequest::STATE_PROCESSING) {
            m_currentSearchRequest->m_serviceState = CSearchRequest::STATE_STALE;
        } else {
            delete m_currentSearchRequest;
        }

        m_currentSearchRequest = NULL;

        searchLock.Unlock();
    }
}

// 0x6FF410
BOOL CGameSprite::MoveToBack()
{
    if (m_animation.CanLieDown()) {
        if (m_listType == LIST_BACK) {
            if ((m_derivedStats.m_generalState & STATE_SLEEPING) == 0) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 5669
                UTIL_ASSERT(FALSE);
            }

            // NOTE: Uninline.
            m_pArea->RemoveFromMarkers(m_id);

            m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                               m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
            m_pArea->IncrHeightDynamic(m_pos);
            m_derivedStats.m_generalState &= ~STATE_SLEEPING;
            return FALSE;
        }

        if (m_posVertList != NULL) {
            m_pArea->RemoveObject(m_posVertList, m_listType, m_id);
            m_posVertList = NULL;

            if ((m_derivedStats.m_generalState & STATE_DEAD) != 0) {
                // NOTE: Uninline.
                m_pArea->RemoveFromMarkers(m_id);

                m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                   m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);
                m_pArea->IncrHeightDynamic(m_pos);
            }

            m_listType = LIST_BACK;
            m_pArea->AddObject(m_id, LIST_BACK);
            return TRUE;
        }

        if (m_moveToFrontQueue > 0) {
            m_moveToFrontQueue--;
        } else {
            m_moveToBackQueue++;
        }
        return FALSE;
    }

    if ((m_derivedStats.m_generalState & STATE_DEAD) != 0) {
        // NOTE: Uninline.
        m_pArea->RemoveFromMarkers(m_id);
    }
    return TRUE;
}

// 0x6FF6D0
BOOL CGameSprite::MoveToFront()
{
    if (m_animation.CanLieDown()) {
        if (m_listType == LIST_FRONT) {
            return FALSE;
        }

        if (m_posVertList != NULL) {
            m_pArea->RemoveObject(m_posVertList, m_listType, m_id);
            m_posVertList = NULL;

            if ((m_derivedStats.m_generalState & STATE_SLEEPING) == 0) {
                // NOTE: Uninline.
                m_pArea->AddToMarkers(m_id);

                m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                    m_typeAI.GetEnemyAlly(),
                    m_animation.GetPersonalSpace(),
                    field_54A8,
                    field_7430);
                m_pArea->DecrHeightDynamic(m_pos);
            }

            m_listType = LIST_FRONT;
            m_pArea->AddObject(m_id, LIST_FRONT);
            return TRUE;
        }

        if (m_moveToBackQueue > 0) {
            m_moveToBackQueue--;
        } else {
            m_moveToFrontQueue++;
        }
        return FALSE;
    }

    if ((m_derivedStats.m_generalState & STATE_SLEEPING) == 0) {
        // NOTE: Uninline.
        m_pArea->AddToMarkers(m_id);
    }
    return TRUE;
}

// 0x6FF8A0
void CGameSprite::OnActionButton(const CPoint& pt)
{
    if (m_canBeSeen > 0) {
        CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
        CAIGroup* pGroup = pGame->GetGroup();
        CAIAction action1;
        CAIAction action2;
        CAIObjectType targetType(0, 0, 0, 0, 0, 0, 0, 0, m_id, 0, 0);

        switch (pGame->GetState()) {
        case 0:
            if (pGame->GetCharacterPortraitNum(m_id) != -1
                || pGame->IsAlly(m_id)
                || pGame->IsFamiliar(m_id)) {
                if (!pGame->GetGameSave()->field_1AC || InControl()) {
                    if (InControl()) {
                        // NOTE: Uninline.
                        pGame->SetLastTarget(CGameObjectArray::INVALID_INDEX);

                        if (!m_moraleFailure) {
                            if (!g_pBaldurChitin->GetScreenWorld()->GetCtrlKey()
                                && !g_pBaldurChitin->GetScreenWorld()->GetShiftKey()) {
                                pGame->UnselectAll();
                            }
                            pGame->SelectCharacter(m_id, !m_bSelected);
                            pGame->SelectToolbar();
                        }
                    } else {
                        if (pGroup->GetCount() > 0
                            && m_pArea == pGame->GetVisibleArea()) {
                            action1 = CAIAction(CAIAction::MOVETOOBJECTFOLLOW,
                                targetType,
                                0,
                                0,
                                0);
                            pGroup->GroupAction(action1, TRUE, NULL);
                        } else {
                            pGame->UnselectAll();
                            pGame->GetVisibleArea()->m_bPicked = FALSE;
                            pGame->GetVisibleArea()->m_iPicked = CGameObjectArray::INVALID_INDEX;
                            pGame->GetVisibleArea()->m_nToolTip = 0;
                            pGame->GetVisibleArea()->OnDeactivation();
                            pGame->m_visibleArea = m_pArea->m_id;
                            pGame->GetVisibleArea()->OnActivation();
                        }
                    }
                }
            } else {
                if (pGroup->GetCount() > 0) {
                    if (pGroup->m_groupChanged
                        || pGame->m_lastTarget != m_id
                        || m_typeAI.GetEnemyAlly() < CAIObjectType::EA_EVILCUTOFF) {
                        if (m_typeAI.GetEnemyAlly() < CAIObjectType::EA_EVILCUTOFF) {
                            // A neutral or friendly creature: it waits for the
                            // party leader, who talks to it (0x7001DE-0x70066D).
                            if (pGroup->IsPartyLeader() && (~m_baseStats.m_flags & STATE_DISEASED) != 0) {
                                CMessage* message = new CMessageSetDialogWait(150,
                                    pGroup->GetGroupLeader(),
                                    m_id,
                                    m_id);
                                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                                action1 = CAIAction(CAIAction::NO_ACTION, CPoint(-1, -1), 0, -1);
                                action2 = CAIAction(CAIAction::PLAYERDIALOG,
                                    targetType,
                                    0,
                                    0,
                                    0);
                                pGroup->GroupAction(action1, TRUE, &action2);
                            }
                        } else {
                            pGroup->m_groupChanged = FALSE;

                            // NOTE: Uninline.
                            pGame->SetLastTarget(m_id);

                            pGroup->GroupSetTarget(m_id);
                        }
                    } else if (pGame->field_38A6) {
                        if (m_typeAI.GetEnemyAlly() >= CAIObjectType::EA_EVILCUTOFF) {
                            pGroup->m_groupChanged = FALSE;

                            // NOTE: Uninline.
                            pGame->SetLastTarget(m_id);

                            pGroup->GroupSetTarget(m_id);
                        }
                        pGame->field_38A6 = FALSE;
                    }
                } else {
                    // NOTE: Uninline.
                    pGame->SetLastTarget(CGameObjectArray::INVALID_INDEX);

                    PlaySound(9, TRUE, FALSE, FALSE);
                }
            }
            break;
        case 2:
            // NOTE: Uninline.
            pGame->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            switch (pGame->GetIconIndex()) {
            case 12:
                if (!m_bSelected || pGroup->GetCount() != 1) {
                    pGroup->GroupSetTarget(m_id);
                    pGame->SetState(0);
                    pGame->GetButtonArray()->SetSelectedButton(100);
                    pGame->GetButtonArray()->UpdateState();
                }
                break;
            case 18:
                if (pGroup->IsPartyLeader()) {
                    CMessage* message = new CMessageSetDialogWait(150,
                        pGroup->GetGroupLeader(),
                        m_id,
                        m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

                    action2 = CAIAction(CAIAction::PLAYERDIALOG,
                        targetType,
                        0,
                        0,
                        0);
                    pGroup->GroupAction(action1, TRUE, &action2);

                    pGame->SetState(0);
                    pGame->GetButtonArray()->SetSelectedButton(100);
                    pGame->GetButtonArray()->UpdateState();
                }
                break;
            case 20:
            case -1:
                pGame->UseMagicOnGround(m_id);

                pGame->SetState(0);
                pGame->GetButtonArray()->SetSelectedButton(100);
                pGame->GetButtonArray()->UpdateState();
                break;
            case 36:
            case 40:
                if (!m_bSelected) {
                    SHORT nPortrait = g_pBaldurChitin->GetScreenWorld()->GetSelectedCharacter();
                    LONG nCharacterId = pGame->GetCharacterId(nPortrait);

                    CGameSprite* pSprite;

                    BYTE rc;
                    do {
                        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        BYTE nPickPockets = pSprite->GetBaseStats()->m_skills[CGAMESPRITE_SKILL_PICK_POCKET];

                        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);

                        if (nPickPockets == 0) {
                            break;
                        }
                    }

                    action2 = CAIAction(CAIAction::PICKPOCKETS,
                        targetType,
                        0,
                        0,
                        0);
                    pGroup->GroupAction(action1, TRUE, &action2);

                    pGame->SetState(0);
                    pGame->GetButtonArray()->SetSelectedButton(100);
                    pGame->GetButtonArray()->UpdateState();
                }
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 5921
                UTIL_ASSERT(FALSE);
            }
            break;
        case 3:
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 5951
            UTIL_ASSERT(FALSE);
        }
    } else {
        CGameObject::OnActionButton(pt);
    }
}

// 0x700BB0
void CGameSprite::OnFormationButton(const CPoint& pt)
{
    if (m_canBeSeen > 0) {
        PlaySound(9, 1, 0, 1);
    } else {
        CGameObject::OnFormationButton(pt);
    }
}

// 0x700BE0
void CGameSprite::OnLightningStrike()
{
    CRect rView;
    rView.left = m_pArea->GetInfinity()->nNewX;
    rView.top = m_pArea->GetInfinity()->nNewY;
    rView.right = m_pArea->GetInfinity()->nNewX + m_pArea->GetInfinity()->rViewPort.Width();
    rView.bottom = m_pArea->GetInfinity()->nNewY + m_pArea->GetInfinity()->rViewPort.Height();

    BYTE nProb = 30;

    if (rView.PtInRect(m_pos)) {
        CItem* pItem = m_equipment.m_items[CGameSpriteEquipment::SLOT_ARMOR];
        if (pItem != NULL) {
            WORD animType = pItem->GetAnimationType();
            if (static_cast<BYTE>(animType) != 87) {
                switch (static_cast<BYTE>(animType >> 8)) {
                case 51:
                    nProb += 35;
                    break;
                case 52:
                    nProb += 40;
                    break;
                }
            }
        }

        if (rand() % 100 < nProb) {
            CAIAction strike(CAIAction::REALLYFORCESPELL, CPoint(-1, -1), 0, -1);
            strike.m_acteeID.Set(m_typeAI);
            strike.SetString1(CString("SpWi938"));

            CMessage* message = new CMessageAddAction(strike, m_pArea->m_nAIIndex, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }
}

// 0x700F20
void CGameSprite::MakeGlobal()
{
    if (!m_bGlobal) {
        m_bGlobal = TRUE;

        g_pBaldurChitin->GetObjectGame()->m_lstGlobalCreatures.AddTail(reinterpret_cast<int*>(m_id));

        CVariable v;
        v.SetName(CString(GetScriptName()));
        v.m_intValue = m_id;
        g_pBaldurChitin->GetObjectGame()->GetNamedCreatures()->AddKey(v);
    }
}

// 0x700FE0
SHORT CGameSprite::GetNumSounds(SHORT nOffset, SHORT nMaxNum)
{
    SHORT nSounds = 0;

    if (m_secondarySounds == "") {
        for (INT nIndex = 0; nIndex < nMaxNum; nIndex++) {
            if (m_baseStats.m_speech[nOffset + nIndex] == -1) {
                break;
            }

            nSounds++;
        }
    } else {
        nSounds = nMaxNum;
    }

    return nSounds;
}

// 0x7010A0
BOOL CGameSprite::CanSpeak(BOOL ignoreDeath, BOOL ignoreSilence)
{
    if (!m_active || !m_activeAI || !m_activeImprisonment) {
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene == TRUE) {
        return TRUE;
    }

    if (!ignoreSilence && (m_derivedStats.m_generalState & STATE_SILENCED) != 0) {
        return FALSE;
    }

    USHORT animationType = m_animation.GetAnimationId();

    if ((m_derivedStats.m_generalState & STATE_SLEEPING) != 0
        && ((animationType & 0xF000) != 0x4000 || animationType < 0x4400)) {
        return FALSE;
    }

    if (!ignoreDeath
        && ((m_derivedStats.m_generalState & STATE_DEAD) != 0
            || (m_derivedStats.m_generalState & STATE_STONE_DEATH) != 0
            || (m_derivedStats.m_generalState & STATE_FROZEN_DEATH) != 0)) {
        return FALSE;
    }

    if ((m_derivedStats.m_generalState & STATE_HELPLESS) != 0) {
        return FALSE;
    }

    if (m_derivedStats.m_spellStates.test(SPLSTATE_HELD)
        || m_derivedStats.m_spellStates.test(SPLSTATE_HOPELESSNESS)
        || m_derivedStats.m_spellStates.test(SPLSTATE_OTILUKES_RESILIENT_SPHERE)) {
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->m_nTimeStop != 0
        && g_pBaldurChitin->GetObjectGame()->m_nTimeStopCaster != m_id) {
        return FALSE;
    }

    if (!m_bAllowDialogInterrupt) {
        return FALSE;
    }

    if ((m_derivedStats.m_generalState & STATE_CHARMED) != 0) {
        return FALSE;
    }

    return TRUE;
}

// 0x7011E0
void CGameSprite::PlaySound(BYTE soundID, BOOL showText, BOOL showCircle, BOOL overrideOption)
{
    STR_RES strRes;
    BYTE nSoundIndex;

    // NOTE: Uninline.
    BYTE channel = GetChannel();

    // Whether the voice is played without a position (the selection and
    // the "select action" voices).
    BOOL bUnpositioned = FALSE;

    if (!CanSpeak(soundID == SOUND_DYING, FALSE)) {
        return;
    }

    // NOTE: Every pick below is an inlined helper that draws rand() only
    // when the count is not zero; the callers have already checked it.
    SHORT nSounds;
    INT nIndex;

    switch (soundID) {
    case 1:
        nSounds = GetNumSounds(0, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex);
        if (channel == 13) {
            strRes.cSound.SetChannel(13, reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(channel, 0);
        }
        break;
    case 2:
        nSounds = GetNumSounds(7, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[7 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 7);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    case 3:
        nSounds = GetNumSounds(9, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[9 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 9);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    case 4:
        nSounds = GetNumSounds(11, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[11 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 11);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    case 5:
        if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1) {
            if (g_pBaldurChitin->m_pEngineWorld->m_nBattleCryTimeOut >= 1) {
                return;
            }
            g_pBaldurChitin->m_pEngineWorld->m_nBattleCryTimeOut = 30;
        }

        if (!g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bAttackSounds
            || m_attackSoundDeadzone > 0) {
            return;
        }

        m_attackSoundDeadzone = 90;

        if (!field_70FC) {
            nSounds = GetNumSounds(2, 5);
            if (nSounds == 0) {
                return;
            }
            nIndex = nSounds != 0 ? rand() % nSounds : 0;
            g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[2 + nIndex], strRes);
            nSoundIndex = static_cast<BYTE>(nIndex + 2);
            strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
            break;
        }

        if (field_7360.GetCount() == 0) {
            return;
        }

        {
            CSound sound;
            CString sSound;
            INT nCount = field_7360.GetCount();
            nIndex = nCount != 0 ? rand() % nCount : 0;

            // NOTE: Inlined CList::GetAt(FindIndex(nIndex)), which reads
            // the entry at offset 8 of a null node when the index is out of
            // range; the index here is always in range.
            POSITION pos = field_7360.FindIndex(nIndex);
            sSound = field_7360.GetAt(pos).field_0;

            sound.SetResRef(CResRef(sSound), TRUE, TRUE);
            sound.m_nPitchVariance = 5;
            sound.m_nVolumeVariance = 20;
            sound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
            if (!sound.GetLooping()) {
                sound.SetFireForget(TRUE);
            }
            sound.Play(m_pos.x, m_pos.y, 0, FALSE);
        }
        return;
    case 6:
        if (!field_70FD) {
            nSounds = GetNumSounds(33, 3);
            if (nSounds == 0) {
                return;
            }
            nIndex = nSounds != 0 ? rand() % nSounds : 0;
            g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[33 + nIndex], strRes);
            nSoundIndex = static_cast<BYTE>(nIndex + 33);
            if (channel == 13) {
                strRes.cSound.SetChannel(13, reinterpret_cast<DWORD_PTR>(m_pArea));
            } else {
                strRes.cSound.SetChannel(channel, 0);
            }
            break;
        }

        if (field_737C.GetCount() != 0) {
            nIndex = rand() % field_737C.GetCount();

            // NOTE: Inlined CList::GetAt(FindIndex(nIndex)).
            POSITION pos = field_737C.FindIndex(nIndex);
            const CGameSpriteSoundEntry& entry = field_737C.GetAt(pos);
            field_7410 = entry.field_0;
            field_7414 = entry.field_4;
        }
        return;
    case SOUND_DYING:
        nSounds = GetNumSounds(36, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[36 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 36);
        if (channel == 13) {
            strRes.cSound.SetChannel(13, reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(channel, 0);
        }
        break;
    case 8:
        nSounds = GetNumSounds(13, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[13 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 13);
        if (channel == 13) {
            strRes.cSound.SetChannel(13, reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(channel, 0);
        }
        break;
    case SOUND_SELECT:
        if (!field_7100) {
            DWORD nFrequency = g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nSelectionSoundsFrequency;
            BOOL bPlay = FALSE;
            bUnpositioned = TRUE;

            if (nFrequency == 2) {
                if (rand() % 5 == 0) {
                    bPlay = TRUE;
                }
            } else if (nFrequency == 3) {
                bPlay = TRUE;
            }

            if (!overrideOption && !bPlay) {
                return;
            }

            if (field_710C < 9) {
                nSounds = GetNumSounds(15, 7);
                if (nSounds < 1) {
                    nIndex = 0;
                } else {
                    nIndex = field_710A % nSounds;
                }
                g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[15 + nIndex], strRes);
                nSoundIndex = static_cast<BYTE>(nIndex + 15);
                if (channel == 13) {
                    strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(m_pArea));
                } else {
                    strRes.cSound.SetChannel(channel, 0);
                }
                field_710A++;
                field_710C++;
            } else {
                g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[29 + field_710E], strRes);
                nSoundIndex = static_cast<BYTE>(field_710E + 29);
                if (channel == 13) {
                    strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(m_pArea));
                } else {
                    strRes.cSound.SetChannel(channel, 0);
                }
                field_710E++;
                if (GetNumSounds(29, 4) <= field_710E) {
                    field_710E = 0;
                    field_710C = 0;
                }
                field_710A = 0;
            }
            break;
        }

        if (field_73D0.GetCount() == 0) {
            return;
        }

        {
            CSound sound;
            CString sSound;
            INT nCount = field_73D0.GetCount();
            nIndex = nCount != 0 ? rand() % nCount : 0;

            // NOTE: Inlined CList::GetAt(FindIndex(nIndex)).
            POSITION pos = field_73D0.FindIndex(nIndex);
            sSound = field_73D0.GetAt(pos).field_0;

            sound.SetResRef(CResRef(sSound), TRUE, TRUE);
            sound.m_nPitchVariance = 5;
            sound.m_nVolumeVariance = 20;
            sound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
            if (!sound.GetLooping()) {
                sound.SetFireForget(TRUE);
            }
            sound.Play(m_pos.x, m_pos.y, 0, FALSE);
        }
        return;
    case 10:
        bUnpositioned = TRUE;
        nSounds = GetNumSounds(15, 7);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[15 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 15);
        if (channel == 13) {
            strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(channel, 0);
        }
        break;
    case SOUND_SELECT_ACTION: {
        DWORD nFrequency = g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nCommandSoundsFrequency;
        BOOL bPlay = FALSE;

        if (nFrequency == 2) {
            if (m_firstActionSound) {
                bPlay = TRUE;
            }
        } else if (nFrequency == 3) {
            bPlay = TRUE;
        }

        if (!overrideOption && !bPlay) {
            return;
        }

        nSounds = GetNumSounds(22, 7);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[22 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 22);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        m_firstActionSound = FALSE;
        break;
    }
    case 12:
        bUnpositioned = TRUE;
        nSounds = GetNumSounds(29, 4);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[29 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 29);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        if (channel == 13) {
            strRes.cSound.SetChannel(6, reinterpret_cast<DWORD_PTR>(m_pArea));
        } else {
            strRes.cSound.SetChannel(channel, 0);
        }
        break;
    case SOUND_REACT_TO_DEATH:
        nSounds = GetNumSounds(38, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[38 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 38);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    case 14:
        if (m_attackSoundDeadzone >= 1) {
            return;
        }
        m_attackSoundDeadzone = 90;
        nSounds = GetNumSounds(40, 4);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[40 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 40);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    case 15:
        nSounds = GetNumSounds(44, 20);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[44 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 44);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    case 16:
        nSounds = GetNumSounds(40, 2);
        if (nSounds == 0) {
            return;
        }
        nIndex = nSounds != 0 ? rand() % nSounds : 0;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[40 + nIndex], strRes);
        nSoundIndex = static_cast<BYTE>(nIndex + 40);
        strRes.cSound.SetChannel(channel, reinterpret_cast<DWORD_PTR>(m_pArea));
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 7005
        UTIL_ASSERT(FALSE);

        // NOTE: The original goes on into the common part with no string
        // resource (a null pointer: ebp is still 0 at 0x702071) and faults;
        // bounded here.
        return;
    }

    COLORREF nameColor = CVidPalette::RANGE_COLORS[m_baseStats.m_colors[CVIDPALETTE_RANGE_MAIN_CLOTH]];
    BOOL bSubtitles = TRUE;
    if (!g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bSubtitles) {
        bSubtitles = FALSE;
    }

    strRes.szText.TrimLeft();

    if (m_secondarySounds != "") {
        if (bSubtitles) {
            CString sSoundSet(reinterpret_cast<const char*>(field_725A));
            STRREF strRef = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetSpeechStringRef(sSoundSet, nSoundIndex);
            if (strRef != -1) {
                STR_RES strResSpeech;
                g_pBaldurChitin->GetTlkTable().Fetch(strRef, strResSpeech);
                if (strResSpeech.szText.GetLength() == 0) {
                    strRes.szText = "";
                } else {
                    strRes.szText = strResSpeech.szText;
                }
            }
        }

        CString sSoundName;
        CString sSoundSetName;
        BOOL bCustom = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetCustomSound(sSoundName, nSoundIndex);
        m_secondarySounds.CopyToString(sSoundSetName);
        sSoundName.TrimLeft();
        sSoundName.TrimRight();
        sSoundName = sSoundSetName + sSoundName;

        if (!bCustom) {
            strRes.cSound.SetResRef(CResRef(sSoundName), TRUE, TRUE);
        } else {
            CResRef cResRef(sSoundName);
            if (g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(cResRef, 4, TRUE) != NULL) {
                strRes.cSound.SetResRef(cResRef, TRUE, TRUE);
            }
        }
    }

    // NOTE: The original reads an uninitialized stack byte (0xA3 off esp,
    // tested at 0x7027C9) as "played" when the voice has no resource or is
    // already playing; it only decides whether m_talkingCounter is set when
    // showCircle is given. Taken as FALSE here.
    BOOL bPlayed = FALSE;

    if (m_typeAI.GetEnemyAlly() == CAIObjectType::EA_PC) {
        if (strRes.cSound.GetRes() != NULL) {
            if (!strRes.cSound.GetLooping()) {
                strRes.cSound.SetFireForget(TRUE);
            }

            if (strRes.cSound.Play(FALSE) && showCircle) {
                if ((strRes.cSound.GetPlayTime() / 33 & ~1) < 600) {
                    m_talkingCounter = strRes.cSound.GetPlayTime() / 66;
                } else {
                    m_talkingCounter = 300;
                }
            }
        }
    } else {
        if (strRes.cSound.GetRes() != NULL) {
            if (!strRes.cSound.GetLooping()) {
                strRes.cSound.SetFireForget(TRUE);
            }

            if (strRes.cSound.m_nArea != 0) {
                if (bUnpositioned) {
                    if (!m_sndVoice.IsSoundPlaying() || m_sndVoice.GetResRef() != strRes.cSound.GetResRef()) {
                        m_sndVoice.Stop();
                        m_sndVoice.SetResRef(strRes.cSound.GetResRef(), TRUE, TRUE);
                        m_sndVoice.SetChannel(strRes.cSound.m_nChannel, 0);
                        bPlayed = m_sndVoice.Play(FALSE);
                    }
                } else {
                    if (!m_sndVoice.IsSoundPlaying() || m_sndVoice.GetResRef() != strRes.cSound.GetResRef()) {
                        m_sndVoice.Stop();
                        m_sndVoice.SetResRef(strRes.cSound.GetResRef(), TRUE, TRUE);
                        m_sndVoice.SetChannel(strRes.cSound.m_nChannel, reinterpret_cast<DWORD_PTR>(m_pArea));
                        bPlayed = m_sndVoice.Play(m_pos.x, m_pos.y, 0, FALSE);
                    }
                }
            } else {
                if (!m_sndVoice.IsSoundPlaying() || m_sndVoice.GetResRef() != strRes.cSound.GetResRef()) {
                    m_sndVoice.Stop();
                    m_sndVoice.SetResRef(strRes.cSound.GetResRef(), TRUE, TRUE);
                    m_sndVoice.SetChannel(strRes.cSound.m_nChannel, 0);
                    bPlayed = m_sndVoice.Play(FALSE);
                }
            }
        }

        // NOTE: The play time is the string resource's sound's, not the
        // voice's (`lea edi, [ebp+4]` at 0x7027DF).
        if (bPlayed && showCircle) {
            if ((strRes.cSound.GetPlayTime() / 33 & ~1) < 600) {
                m_talkingCounter = strRes.cSound.GetPlayTime() / 66;
            } else {
                m_talkingCounter = 300;
            }
        }
    }

    if (strcmp(strRes.szText, "") != 0 && showText && bSubtitles) {
        g_pBaldurChitin->GetScreenWorld()->DisplayText(m_sName,
            strRes.szText,
            nameColor,
            nameColor,
            -1,
            FALSE);
    }
}

// 0x702900
void CGameSprite::VerbalConstant(LONG verbalConstant)
{
    STR_RES strRes;

    // NOTE: Uninline.
    BYTE channel = GetChannel();

    if (CanSpeak(verbalConstant >= 36 && verbalConstant < 38, FALSE)
        && verbalConstant >= 0 && verbalConstant < 64) {
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[verbalConstant], strRes);
        COLORREF nameColor = CVidPalette::RANGE_COLORS[m_baseStats.m_colors[CVIDPALETTE_RANGE_MAIN_CLOTH]];
        if (m_secondarySounds != "") {
            CString sSoundName;
            CString sSoundSetName;
            LONG nNumber = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetCustomSound(sSoundName, static_cast<BYTE>(verbalConstant));
            m_secondarySounds.CopyToString(sSoundSetName);
            sSoundName.TrimLeft();
            sSoundName.TrimRight();
            sSoundName = sSoundSetName + sSoundName;

            strRes.cSound.SetResRef(CResRef(sSoundName), TRUE, TRUE);
        }

        strRes.szText.TrimLeft();

        BOOL show = g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bSubtitles != FALSE;
        if (strRes.cSound.GetRes() == NULL) {
            show = TRUE;
        }

        if (m_typeAI.GetEnemyAlly() == CAIObjectType::EA_PC) {
            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                if (strRes.cSound.Play(FALSE)) {
                    m_talkingCounter = min(strRes.cSound.GetPlayTime() / 66, STANDARD_VERBAL_CONSTANT_LENGTH);
                }
            }

            CString sText;
            sText = strRes.szText;
            if (sText != "" && show) {
                g_pBaldurChitin->GetScreenWorld()->DisplayText(m_sName,
                    sText,
                    nameColor,
                    RGB(160, 200, 215),
                    -1,
                    FALSE);
            }
        } else {
            if (!strRes.cSound.GetLooping()) {
                strRes.cSound.SetFireForget(TRUE);
            }
            if (strRes.cSound.Play(FALSE)) {
                m_talkingCounter = min(strRes.cSound.GetPlayTime() / 66, STANDARD_VERBAL_CONSTANT_LENGTH);
            }

            CString sText;
            sText = strRes.szText;
            if (sText != "" && show) {
                g_pBaldurChitin->GetScreenWorld()->DisplayText(m_sName,
                    sText,
                    nameColor,
                    RGB(160, 200, 215),
                    -1,
                    FALSE);
            }
        }
    }
}

// 0x702E60
LONG CGameSprite::GetLength(LONG verbalConstant)
{
    STR_RES strRes;

    if (verbalConstant < 0 || verbalConstant >= 64) {
        return 0;
    }

    g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[verbalConstant], strRes);

    if (m_secondarySounds != "") {
        CString sSoundName;
        CString sSoundSetName;
        LONG nNumber = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetCustomSound(sSoundName, static_cast<BYTE>(verbalConstant));
        m_secondarySounds.CopyToString(sSoundSetName);
        sSoundName.TrimLeft();
        sSoundName.TrimRight();
        sSoundName = sSoundSetName + sSoundName;

        if (nNumber == 0
            || g_pBaldurChitin->cDimm.m_cKeyTable.FindKey(CResRef(sSoundName), 4, TRUE) != NULL) {
            strRes.cSound.SetResRef(CResRef(sSoundName), TRUE, TRUE);
        }
    }

    // FIXME: Calls `GetPlayTime` twice.
    return min(strRes.cSound.GetPlayTime() / 33, 300);
}

// 0x703170
BOOL CGameSprite::IsSpellSchool(BYTE spellSchool)
{
    return ((GetSpecialization() & SPECMASK_WIZARD_ABJURER) != 0 && spellSchool == 1)
        || ((GetSpecialization() & SPECMASK_WIZARD_CONJURER) != 0 && spellSchool == 2)
        || ((GetSpecialization() & SPECMASK_WIZARD_DIVINER) != 0 && spellSchool == 3)
        || ((GetSpecialization() & SPECMASK_WIZARD_ENCHANTER) != 0 && spellSchool == 4)
        || ((GetSpecialization() & SPECMASK_WIZARD_ILLUSIONIST) != 0 && spellSchool == 5)
        || ((GetSpecialization() & SPECMASK_WIZARD_EVOKER) != 0 && spellSchool == 6)
        || ((GetSpecialization() & SPECMASK_WIZARD_NECROMANCER) != 0 && spellSchool == 7)
        || ((GetSpecialization() & SPECMASK_WIZARD_TRANSMUTER) != 0 && spellSchool == 8)
        || ((GetSpecialization() & SPECMASK_WIZARD_UNIVERSAL) != 0 && spellSchool == 9);
}

// 0x703270
void CGameSprite::SetFootstepChannel()
{
    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1) {
        for (int index = 0; index < 2; index++) {
            m_sndWalk[index].SetChannel(19, reinterpret_cast<DWORD_PTR>(m_pArea));
            m_sndArmor[index].SetChannel(20, reinterpret_cast<DWORD_PTR>(m_pArea));
        }
        m_sndReady.SetChannel(19, reinterpret_cast<DWORD_PTR>(m_pArea));
        m_sndDeath.SetChannel(15, reinterpret_cast<DWORD_PTR>(m_pArea));
    } else {
        for (int index = 0; index < 2; index++) {
            m_sndWalk[index].SetChannel(18, reinterpret_cast<DWORD_PTR>(m_pArea));
            m_sndArmor[index].SetChannel(20, reinterpret_cast<DWORD_PTR>(m_pArea));
        }
        m_sndReady.SetChannel(18, reinterpret_cast<DWORD_PTR>(m_pArea));
        m_sndDeath.SetChannel(15, reinterpret_cast<DWORD_PTR>(m_pArea));
    }
}

// 0x703330
BYTE CGameSprite::GetSound(BYTE soundID)
{
    // NOTE: Unused.
    STR_RES strRes;

    SHORT nNumSounds;
    BYTE nSound;

    switch (soundID) {
    case 2:
        nNumSounds = GetNumSounds(7, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 7;
        } else {
            nSound = 7;
        }
        break;
    case 3:
        nNumSounds = GetNumSounds(9, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 9;
        } else {
            nSound = 9;
        }
        break;
    case 4:
        nNumSounds = GetNumSounds(11, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 11;
        } else {
            nSound = 11;
        }
        break;
    case 5:
        nNumSounds = GetNumSounds(2, 5);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 2;
        } else {
            nSound = 2;
        }
        break;
    case 6:
        nNumSounds = GetNumSounds(33, 3);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 33;
        } else {
            nSound = 33;
        }
        break;
    case 7:
        nNumSounds = GetNumSounds(36, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 36;
        } else {
            nSound = 36;
        }
        break;
    case 8:
        nNumSounds = GetNumSounds(13, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 13;
        } else {
            nSound = 13;
        }
        break;
    case 9:
        if (field_710C > 8) {
            nSound = field_710E + 29;
            field_710E++;
            if (field_710E >= GetNumSounds(29, 4)) {
                field_710E = 0;
            }
            field_710A = 0;
            field_710C = 0;
        } else {
            nNumSounds = GetNumSounds(15, 7);
            if (nNumSounds > 0) {
                nSound = field_710A % nNumSounds;
            } else {
                nSound = 0;
            }
            field_710A++;
            field_710C++;
        }
        break;
    case 10:
        nNumSounds = GetNumSounds(15, 7);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 15;
        } else {
            nSound = 15;
        }
        break;
    case 11:
        nNumSounds = GetNumSounds(22, 7);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 22;
        } else {
            nSound = 22;
        }
        break;
    case 12:
        nNumSounds = GetNumSounds(29, 4);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 29;
        } else {
            nSound = 29;
        }
        break;
    case 13:
        nNumSounds = GetNumSounds(38, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 38;
        } else {
            nSound = 38;
        }
        break;
    case 14:
        nNumSounds = GetNumSounds(40, 4);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 40;
        } else {
            nSound = 40;
        }
        break;
    case 15:
        // FIXME: Not sure if it can overrun `m_speech` and treat `m_biography`
        // as sound?
        nNumSounds = GetNumSounds(44, 20);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 44;
        } else {
            nSound = 44;
        }
        break;
    case 16:
        nNumSounds = GetNumSounds(40, 2);
        if (nNumSounds != 0) {
            nSound = rand() % nNumSounds + 40;
        } else {
            nSound = 40;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 7658
        UTIL_ASSERT(FALSE);
    }

    return nSound;
}

// 0x703700
void CGameSprite::Render(CGameArea* pArea, CVidMode* pVidMode, INT nSurface)
{
    BOOLEAN bDithered = FALSE;
    BOOLEAN bFadeOut = FALSE;
    DWORD dwRenderFlags = 0x20000;
    CVisibilityMap* pVisibility;
    COLORREF rgbTint;
    COLORREF rgbGlobalTint;
    BYTE transparency;
    CSearchBitmap* pSearch;
    CRect rFX;
    CRect rGCBounds;
    CRect rViewRect;
    CPoint ptReference;
    CPoint newPos;
    POSITION bloodLstPos;
    CBlood* pBlood;
    SHORT nBloodDir;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 7687
    UTIL_ASSERT(pVidMode != NULL);

    if (m_active
        && m_activeAI
        && m_activeImprisonment
        && ((m_derivedStats.m_generalState & STATE_DEAD) != 0
            || m_canBeSeen > 0)
        && m_pArea == pArea
        && (m_typeAI.GetEnemyAlly() <= CAIObjectType::EA_CONTROLCUTOFF
            || (m_derivedStats.m_generalState & STATE_INVISIBLE) == 0)
        && m_baseStats.field_294 != 1
        && m_baseStats.m_animationType != 0) {
        pSearch = &(m_pArea->m_search);
        pVisibility = &(m_pArea->m_visibility);

        newPos.x = min(max(m_pos.x, 0), m_pArea->GetInfinity()->nAreaX - 1);
        newPos.y = min(max(m_pos.y, 0), m_pArea->GetInfinity()->nAreaY - 1);

        if (pVisibility->IsTileExplored(pVisibility->PointToTile(newPos))) {
            if (m_canBeSeen < VISIBLE_DELAY + 1
                || !pVisibility->IsTileVisible(pVisibility->PointToTile(newPos))) {
                bFadeOut = TRUE;
            }

            rViewRect.left = m_pArea->GetInfinity()->nCurrentX;
            rViewRect.top = m_pArea->GetInfinity()->nCurrentY;
            rViewRect.right = rViewRect.left + m_pArea->GetInfinity()->rViewPort.Width();
            rViewRect.bottom = rViewRect.top + m_pArea->GetInfinity()->rViewPort.Height();

            if (!m_lstBlood.IsEmpty()) {
                bloodLstPos = m_lstBlood.GetHeadPosition();
                while (bloodLstPos != NULL) {
                    pBlood = m_lstBlood.GetNext(bloodLstPos);
                    nBloodDir = pBlood->GetDirection();
                    if (pBlood->BloodLeft()
                        && nBloodDir >= 4
                        && nBloodDir <= 12) {
                        pBlood->Render(pVidMode, nSurface);
                    }
                }
            }

            // NOTE: Uninline.
            m_animation.CalculateFxRect(rFX, ptReference, m_posZ);

            if (m_id == m_pArea->m_iPicked
                || g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bAlwaysDither
                || g_pBaldurChitin->GetObjectGame()->m_bForceDither) {
                bDithered = TRUE;
            }

            rgbTint = m_pArea->GetTintColor(CPoint(m_pos.x, m_pos.y + m_posZ),
                m_listType);

            if (m_pArea->m_nInfravision
                && m_animation.DetectedByInfravision()
                && Animate()) {
                if ((m_pArea->GetInfinity()->m_areaType & 0x2) != 0
                    && ((m_pArea->GetInfinity()->m_areaType & 0x40) == 0
                        || (m_pArea->GetInfinity()->m_renderDayNightCode & 0x2) == 0)) {
                    rgbGlobalTint = g_pBaldurChitin->GetCurrentVideoMode()->GetGlobalTintColor();
                } else {
                    rgbGlobalTint = RGB(255, 255, 255);
                }

                if (GetRValue(rgbTint) * GetRValue(rgbGlobalTint) / 255 > GetRValue(CInfinity::RGB_NIGHT_COLOR)
                    || GetGValue(rgbTint) * GetGValue(rgbGlobalTint) / 255 > GetGValue(CInfinity::RGB_NIGHT_COLOR)
                    || GetBValue(rgbTint) * GetBValue(rgbGlobalTint) / 255 > GetBValue(CInfinity::RGB_NIGHT_COLOR)) {
                    if ((m_pArea->GetInfinity()->m_areaType & 0x2) != 0
                        && ((m_pArea->GetInfinity()->m_areaType & 0x40) == 0
                            || (m_pArea->GetInfinity()->m_renderDayNightCode & 0x2) == 0)) {
                        dwRenderFlags |= 0x10000;
                    }
                } else {
                    rgbTint = RGB(200, 200, 200);
                }
            } else {
                if ((m_pArea->GetInfinity()->m_areaType & 0x2) != 0
                    && ((m_pArea->GetInfinity()->m_areaType & 0x40) == 0
                        || (m_pArea->GetInfinity()->m_renderDayNightCode & 0x2) == 0)) {
                    dwRenderFlags |= 0x10000;
                }
            }

            transparency = 0;

            if (m_derivedStats.m_visualEffects[IWD_VFX_BLUR]) {
                transparency = 180;
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_INVISIBILITY]) {
                transparency = static_cast<BYTE>(abs(g_pBaldurChitin->nAUCounter % 80 - 40)) + 140;
            }

            // NOTE: Unsigned compare.
            if (m_derivedStats.m_nTranslucent > 0 && transparency < static_cast<BYTE>(m_derivedStats.m_nTranslucent)) {
                transparency = static_cast<BYTE>(m_derivedStats.m_nTranslucent);
            }

            if (m_derivedStats.m_nMirrorImages > 0) {
                if (transparency < 96) {
                    transparency = 96;
                }
            }

            if (m_baseStats.field_2F9 > 0 && transparency < m_baseStats.field_2F9) {
                transparency = m_baseStats.field_2F9;
            }

            if (m_derivedStats.m_nMirrorImages > 0) {
                if (m_derivedStats.m_nMirrorImages > 2) {
                    RenderMirrorImage(2,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }

                if (m_derivedStats.m_nMirrorImages > 4) {
                    RenderMirrorImage(4,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }

                if (m_derivedStats.m_nMirrorImages > 6) {
                    RenderMirrorImage(6,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }

                if (m_derivedStats.m_nMirrorImages > 0) {
                    RenderMirrorImage(0,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }

                if (m_derivedStats.m_nMirrorImages > 1) {
                    RenderMirrorImage(1,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }
            }

            IcewindCVisualEffect vfx;
            vfx.sub_586A90(TRUE);

            if (m_derivedStats.m_visualEffects[IWD_VFX_GREASE]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_GREASE]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SHIELD_OF_LATHANDER_2]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SHIELD_OF_LATHANDER_2]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_GREATER_SHIELD_OF_LATHANDER_2]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_GREATER_SHIELD_OF_LATHANDER_2]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SEVEN_EYES_2]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SEVEN_EYES_2]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_28]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_28]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_29]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_29]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_BLUR]
                && m_pos != m_posOld) {
                LONG dx = (m_pos.x - m_posOld.x) / 2;
                LONG dy = (m_pos.y - m_posOld.y) / 2;
                for (int cnt = 0; cnt < 4; cnt++) {
                    m_animation.CalculateGCBoundsRect(rGCBounds,
                        newPos,
                        ptReference,
                        m_posZ,
                        rFX.Width(),
                        rFX.Height());

                    if (!IsRectEmpty(rViewRect & rGCBounds)) {
                        m_animation.Render(m_pArea->GetInfinity(),
                            pVidMode,
                            nSurface,
                            rFX,
                            newPos,
                            ptReference,
                            dwRenderFlags,
                            rgbTint,
                            rGCBounds,
                            bDithered,
                            bFadeOut,
                            m_posZ,
                            transparency);
                    }

                    newPos.x -= dx;
                    newPos.y -= dy;
                }
            } else {
                m_animation.CalculateGCBoundsRect(rGCBounds,
                    newPos,
                    ptReference,
                    m_posZ,
                    rFX.Width(),
                    rFX.Height());

                if (!IsRectEmpty(rViewRect & rGCBounds)) {
                    m_animation.Render(m_pArea->GetInfinity(),
                        pVidMode,
                        nSurface,
                        rFX,
                        newPos,
                        ptReference,
                        dwRenderFlags,
                        rgbTint,
                        rGCBounds,
                        bDithered,
                        bFadeOut,
                        m_posZ,
                        transparency);
                }
            }

            if (m_derivedStats.m_nMirrorImages > 0) {
                if (m_derivedStats.m_nMirrorImages > 7) {
                    RenderMirrorImage(7,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }

                if (m_derivedStats.m_nMirrorImages > 5) {
                    RenderMirrorImage(5,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }

                if (m_derivedStats.m_nMirrorImages > 3) {
                    RenderMirrorImage(3,
                        rFX,
                        rGCBounds,
                        rViewRect,
                        ptReference,
                        pSearch,
                        pVisibility,
                        pVidMode,
                        nSurface,
                        rgbTint,
                        bDithered,
                        bFadeOut,
                        dwRenderFlags);
                }
            }

            RenderSpriteEffect(pVidMode, nSurface);

            if (m_derivedStats.m_visualEffects[IWD_VFX_ENTANGLE]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_ENTANGLE]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_WEB]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_WEB]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SHROUD_OF_FLAME]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SHROUD_OF_FLAME]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_ANTIMAGIC_MISSILE]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_ANTIMAGIC_MISSILE]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_OTILUKES_RESILIENT_SPHERE]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_OTILUKES_RESILIENT_SPHERE]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_PROTECTION_FROM_MISSILES]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_PROTECTION_FROM_MISSILES]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_CLOAK_OF_FEAR]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_CLOAK_OF_FEAR]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_ENTROPY_SHIELD]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_ENTROPY_SHIELD]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_FIRE_AURA]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_FIRE_AURA]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_FROST_AURA]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_FROST_AURA]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SANCTUARY]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SANCTUARY]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_GLOBE_OF_INVULNERABILITY]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_GLOBE_OF_INVULNERABILITY]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_INSECT_PLAGUE]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_INSECT_PLAGUE]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_STORM_SHELL]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_STORM_SHELL]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SHIELD_OF_LATHANDER]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SHIELD_OF_LATHANDER]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_GREATER_SHIELD_OF_LATHANDER]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_GREATER_SHIELD_OF_LATHANDER]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SEVEN_EYES]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SEVEN_EYES]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_MINOR_GLOBE_OF_INVULNERABILITY]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_MINOR_GLOBE_OF_INVULNERABILITY]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_SHIELD]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_SHIELD]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_FIRE_SHIELD_RED]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_FIRE_SHIELD_RED]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_FIRE_SHIELD_BLUE]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_FIRE_SHIELD_BLUE]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_TORTOISE_SHELL]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_TORTOISE_SHELL]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_DEATH_ARMOR]) {
                vfx.sub_586A90(FALSE);
                vfx.sub_586AC0(TRUE, 192);
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_DEATH_ARMOR]),
                    vfx);
            }

            if (m_derivedStats.m_visualEffects[IWD_VFX_WISP]) {
                RenderSpriteCover(pVidMode,
                    nSurface,
                    &(field_7548[IWD_VFX_WISP]),
                    vfx);
            }

            if (!m_lstBlood.IsEmpty()) {
                bloodLstPos = m_lstBlood.GetHeadPosition();
                while (bloodLstPos != NULL) {
                    CBlood* blood = m_lstBlood.GetNext(bloodLstPos);
                    pBlood = m_lstBlood.GetNext(bloodLstPos);
                    nBloodDir = pBlood->GetDirection();
                    if (pBlood->BloodLeft()
                        && (nBloodDir < 4 || nBloodDir > 12)) {
                        pBlood->Render(pVidMode, nSurface);
                    }
                }
            }
        }
    }
}

// 0x704770
void CGameSprite::RenderMarkers(CVidMode* pVidMode, INT nSurface)
{
    CPoint pt;
    pt.x = min(max(m_pos.x, 0), m_pArea->GetInfinity()->nAreaX - 1);
    pt.y = min(max(m_pos.y, 0), m_pArea->GetInfinity()->nAreaY - 1);

    if (m_pArea->m_visibility.IsTileExplored(m_pArea->m_visibility.PointToTile(pt))
        && (m_typeAI.m_nEnemyAlly <= CAIObjectType::EA_CONTROLCUTOFF
            || (m_derivedStats.m_generalState & STATE_INVISIBLE) == 0
            || (m_baseStats.field_2FC & 0x1) != 0)
        && m_baseStats.field_294 != 1) {
        DWORD level = g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nGuiFeedbackLevel;
        if (g_pBaldurChitin->GetScreenWorld()->field_14A) {
            level = 5;
        }
        if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene) {
            level = 1;
        }

        BOOLEAN dialogTalker = FALSE;
        if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode == 386
            || g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode == 1282) {
            CRect rViewPort(m_pArea->GetInfinity()->rViewPort);

            INT x;
            INT y;
            m_pArea->GetInfinity()->GetViewPosition(x, y);

            if (abs(GetPos().x - (x + rViewPort.Width() / 2)) < 16
                && abs(GetPos().y - (y + rViewPort.Height() / 2)) < 12) {
                dialogTalker = TRUE;
            }
        }

        switch (level) {
        case 1:
            if (m_talkingCounter > 0
                || dialogTalker
                || m_id == m_pArea->m_iPicked
                || m_id == m_pArea->m_iPickedTarget
                || m_marker.m_nRecticleForceRenderTarget != 0) {
                m_marker.Render(pVidMode, nSurface, this);
            }

            if (m_id == m_pArea->m_iPicked
                || m_id == m_pArea->m_iPickedTarget
                || m_marker.m_nRecticleForceRender != 0
                || m_groupMove) {
                if (Orderable(FALSE) && m_targetPoint.x != -1) {
                    m_destMarker.Render(pVidMode,
                        nSurface,
                        m_pArea->GetInfinity(),
                        m_targetPoint,
                        CMarker::RECTICLE_DEST_SIZE,
                        3 * CMarker::RECTICLE_DEST_SIZE / 4);
                }
            }
            break;
        case 2:
            if (m_bSelected
                || m_id == m_pArea->m_iPicked
                || m_id == m_pArea->m_iPickedTarget
                || m_marker.m_nRecticleForceRender != 0) {
                m_marker.Render(pVidMode, nSurface, this);

                if (Orderable(FALSE) && m_targetPoint.x != -1) {
                    m_destMarker.Render(pVidMode,
                        nSurface,
                        m_pArea->GetInfinity(),
                        m_targetPoint,
                        CMarker::RECTICLE_DEST_SIZE,
                        3 * CMarker::RECTICLE_DEST_SIZE / 4);
                }
            } else if (m_talkingCounter > 0) {
                m_marker.Render(pVidMode, nSurface, this);
            }
            break;
        case 3:
            if (Orderable(FALSE)
                || m_id == m_pArea->m_iPicked
                || m_id == m_pArea->m_iPickedTarget
                || m_marker.m_nRecticleForceRender != 0) {
                m_marker.Render(pVidMode, nSurface, this);

                if (Orderable(FALSE)
                    && (m_bSelected
                        || m_id == m_pArea->m_iPicked
                        || m_id == m_pArea->m_iPickedTarget)
                    && m_targetPoint.x != -1) {
                    m_destMarker.Render(pVidMode,
                        nSurface,
                        m_pArea->GetInfinity(),
                        m_targetPoint,
                        CMarker::RECTICLE_DEST_SIZE,
                        3 * CMarker::RECTICLE_DEST_SIZE / 4);
                }
            } else if (m_talkingCounter > 0) {
                m_marker.Render(pVidMode, nSurface, this);
            }
            break;
        case 4:
            if (Orderable(FALSE)
                || m_marker.m_type == CMarker::RECTICLE
                || m_id == m_pArea->m_iPicked
                || m_id == m_pArea->m_iPickedTarget) {
                m_marker.Render(pVidMode, nSurface, this);

                if (Orderable(FALSE)
                    && (m_bSelected
                        || m_id == m_pArea->m_iPicked
                        || m_id == m_pArea->m_iPickedTarget)
                    && m_targetPoint.x != -1) {
                    m_destMarker.Render(pVidMode,
                        nSurface,
                        m_pArea->GetInfinity(),
                        m_targetPoint,
                        CMarker::RECTICLE_DEST_SIZE,
                        3 * CMarker::RECTICLE_DEST_SIZE / 4);
                }
            } else if (m_talkingCounter > 0) {
                m_marker.Render(pVidMode, nSurface, this);
            }
            break;
        case 5:
            m_marker.Render(pVidMode, nSurface, this);

            if (Orderable(FALSE)
                && (m_bSelected
                    || m_id == m_pArea->m_iPicked
                    || m_id == m_pArea->m_iPickedTarget)
                && m_targetPoint.x != -1) {
                m_destMarker.Render(pVidMode,
                    nSurface,
                    m_pArea->GetInfinity(),
                    m_targetPoint,
                    CMarker::RECTICLE_DEST_SIZE,
                    3 * CMarker::RECTICLE_DEST_SIZE / 4);
            }
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 8300
            UTIL_ASSERT(FALSE);
        }
    }
}

// 0x704D40
void CGameSprite::RenderPortrait(const CPoint& cpRenderPosition, const CSize& szControl, BOOL bPressed, BOOL reorderHighlight, BOOL selectFromMarker, const CRect& rClip, BOOL bDoubleSize)
{
    BOOL bDead = FALSE;

    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC
        && !InControl()) {
        return;
    }

    m_vbPortraitSmall.SetResRef(CResRef(m_baseStats.m_portraitSmall), TRUE, FALSE);
    m_vbPortraitSmall.m_bDoubleSize = bDoubleSize;

    if (CResRef(m_baseStats.m_portraitSmall) == "") {
        m_vbPortraitSmall.SetResRef(CResRef(CInfGame::SILHOUETTE_PORTRAIT_SM), TRUE, FALSE);
        m_vbPortraitSmall.m_bDoubleSize = bDoubleSize;
    }

    if (CResRef(m_baseStats.m_portraitSmall) != "" && m_vbPortraitSmall.GetRes() == NULL) {
        m_vbPortraitSmall.SetResRef(CResRef(CInfGame::SILHOUETTE_PORTRAIT_SM), TRUE, FALSE);
        m_vbPortraitSmall.m_bDoubleSize = bDoubleSize;
    }

    if (m_vbPortraitSmall.GetBitCount(FALSE) != 24
        && m_vbPortraitSmall.GetBitCount(FALSE) != 8) {
        m_vbPortraitSmall.SetResRef(CResRef(CInfGame::SILHOUETTE_PORTRAIT_SM), TRUE, TRUE);
        m_vbPortraitSmall.m_bDoubleSize = bDoubleSize;
    }

    INT nScale = bDoubleSize ? 2 : 1;

    CRect r1(cpRenderPosition.x,
        cpRenderPosition.y,
        cpRenderPosition.x + 44 * nScale,
        cpRenderPosition.y + 44 * nScale);

    INT v1;
    if (m_derivedStats.m_spellStates[SPLSTATE_SUPPRESS_HP_INFO]) {
        v1 = 42 * nScale;
    } else {
        v1 = 42 * nScale * (m_derivedStats.m_nMaxHitPoints - m_baseStats.m_hitPoints) / m_derivedStats.m_nMaxHitPoints;
    }

    if (m_baseStats.m_hitPoints <= 0) {
        bDead = TRUE;
    }

    COLORREF rgbColor;
    if (selectFromMarker) {
        if ((m_pArea != NULL && m_id == m_pArea->m_iPicked)
            || m_talkingCounter > 0) {
            rgbColor = m_marker.m_rgbColor;
        } else {
            rgbColor = CMarker::PC_SELECTED_COLOR;
            if (!m_bSelected) {
                rgbColor = RGB(0, 0, 0);
            }
        }
    } else {
        if (g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineMap) {
            rgbColor = GetMapScreenColor();
        } else {
            if (InControl() && m_nUnselectableCounter == 0) {
                rgbColor = CMarker::PC_SELECTED_COLOR;
            } else {
                rgbColor = CMarker::PC_NONECONTROLED_SELECTED_COLOR;
            }
            if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter()) {
                rgbColor = RGB(0, 0, 0);
            }
        }
    }

    INT v2 = 42 * nScale;

    CRect r2(cpRenderPosition.x + 2 * nScale,
        cpRenderPosition.y + 2 * nScale,
        cpRenderPosition.x + 2 * nScale + 42 * nScale,
        cpRenderPosition.y + 2 * nScale + 42 * nScale);
    CRect r3(r2);

    if (bDead) {
        m_vbPortraitSmall.SetTintColor(RGB(180, 180, 180));
        m_vbPortraitSmall.RenderDirect(CVIDINF_SURFACE_BACK,
            r2.left,
            r2.top,
            r2 & rClip,
            0xA0000,
            FALSE);
    } else {
        if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nOldPortraitHealth == TRUE) {
            if (m_bBloodFlashOn) {
                m_vbPortraitSmall.SetTintColor(RGB(120 + static_cast<BYTE>(m_nBloodFlashAmount), 30, 30));
                m_vbPortraitSmall.RenderDirect(CVIDINF_SURFACE_BACK,
                    r2.left,
                    r2.top,
                    r2 & rClip,
                    0x20000,
                    FALSE);
            } else {
                if (field_53E6) {
                    m_vbPortraitSmall.SetTintColor(RGB(50, 255, 50));
                } else {
                    m_vbPortraitSmall.SetTintColor(RGB(120, 30, 30));
                }
                m_vbPortraitSmall.RenderDirect(CVIDINF_SURFACE_BACK,
                    r2.left,
                    r2.top,
                    r2 & rClip,
                    0x20000,
                    FALSE);
            }
        }
    }

    if (!GRAVITY_IS_DOWN
        && g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nOldPortraitHealth == TRUE) {
        r2.bottom = r2.top + v2 - v1;
    }

    if (!bDead) {
        m_vbPortraitSmall.SetTintColor(RGB(255, 255, 255));

        if (GRAVITY_IS_DOWN) {
            CRect r3(rClip);

            if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nOldPortraitHealth == TRUE) {
                r3.top = r2.bottom + v1 - v2;
            }

            m_vbPortraitSmall.RenderDirect(CVIDINF_SURFACE_BACK,
                r2.left,
                r2.top,
                r2 & r3,
                0x20000,
                FALSE);
        } else {
            m_vbPortraitSmall.RenderDirect(CVIDINF_SURFACE_BACK,
                r2.left,
                r2.top,
                r2 & r2,
                0x20000,
                FALSE);
        }
    }

    CRect rLineClip(rClip);
    if (rgbColor != 0) {
        r1.right++;
        rLineClip.right++;

        g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.left,
            r1.top,
            r1.left,
            r1.bottom,
            CVIDINF_SURFACE_BACK,
            rLineClip & r1,
            rgbColor);

        if (rLineClip.bottom != r1.bottom) {
            g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.left,
                r1.bottom,
                r1.right,
                r1.bottom,
                CVIDINF_SURFACE_BACK,
                rLineClip & r1,
                rgbColor);
        }

        if (rLineClip.right != r1.right) {
            g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.right,
                r1.bottom,
                r1.right,
                r1.top,
                CVIDINF_SURFACE_BACK,
                rLineClip & r1,
                rgbColor);
        }

        g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.right,
            r1.top,
            r1.left,
            r1.top,
            CVIDINF_SURFACE_BACK,
            rLineClip & r1,
            rgbColor);
    }

    if (reorderHighlight) {
        g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.left,
            r1.top,
            r1.left,
            r1.bottom,
            CVIDINF_SURFACE_BACK,
            r1 & rLineClip,
            RGB(255, 255, 0));

        g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.left,
            r1.bottom,
            r1.right,
            r1.bottom,
            CVIDINF_SURFACE_BACK,
            r1 & rLineClip,
            RGB(255, 255, 0));

        g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.right,
            r1.bottom,
            r1.right,
            r1.top,
            CVIDINF_SURFACE_BACK,
            r1 & rLineClip,
            RGB(255, 255, 0));

        g_pBaldurChitin->GetCurrentVideoMode()->DrawLine(r1.right,
            r1.top,
            r1.left,
            r1.top,
            CVIDINF_SURFACE_BACK,
            r1 & rLineClip,
            RGB(255, 255, 0));
    }

    CPoint ptIcon(r3.left, r3.bottom);
    POSITION pos = m_portraitIcons.GetHeadPosition();
    while (pos != NULL) {
        SHORT nIcon = static_cast<SHORT>(static_cast<INT>(reinterpret_cast<INT_PTR>(m_portraitIcons.GetNext(pos))));
        if (nIcon < m_portraitIconVidCell.GetNumberSequences(FALSE)) {
            m_portraitIconVidCell.SequenceSet(nIcon + 65);
            m_portraitIconVidCell.FrameSet(0);
            m_portraitIconVidCell.Render(CVIDINF_SURFACE_BACK,
                ptIcon.x,
                ptIcon.y,
                r3 & rClip,
                NULL,
                0,
                0,
                -1);
            ptIcon.x += PORTRAIT_ICON_SIZE.x * nScale;
            if (ptIcon.x > r3.right - PORTRAIT_ICON_SIZE.x * nScale) {
                ptIcon.x = r3.left;
                ptIcon.y -= PORTRAIT_ICON_SIZE.y * nScale;
            }
        }
    }

    CVidFont font;
    if (!bDead) {
        if (SHOW_CHARACTER_HP) {
            font.SetResRef(CResRef("NUMFONT"), bDoubleSize, TRUE);
            font.SequenceSet(0);
            if (font.GetRes()->Demand() != NULL) {
                // 0x85BD4C
                static const INT HEALTH_VALUE[4] = {
                    100,
                    75,
                    50,
                    25,
                };

                // 0x85BD5C
                static const COLORREF HEALTH_COLOR[4] = {
                    RGB(255, 255, 255),
                    RGB(0, 250, 0),
                    RGB(255, 255, 0),
                    RGB(255, 128, 0),
                };

                CString sHealth;
                INT iHealthPercent = 100 * m_baseStats.m_hitPoints / m_derivedStats.m_nMaxHitPoints;
                COLORREF rgbColor;

                for (int index = 0; index < 4; index++) {
                    if (iHealthPercent >= HEALTH_VALUE[index]) {
                        rgbColor = HEALTH_COLOR[index];
                        break;
                    }
                }

                if (m_derivedStats.m_spellStates[SPLSTATE_SUPPRESS_HP_INFO]) {
                    sHealth.Format("%c/%c", '-', '-');
                    rgbColor = RGB(255, 0, 0);
                } else {
                    sHealth.Format("%d/%d", m_baseStats.m_hitPoints, m_derivedStats.m_nMaxHitPoints);
                }

                if (m_baseStats.m_hitPoints == 0) {
                    rgbColor = RGB(128, 128, 128);
                }

                font.SetColor(rgbColor, RGB(0, 0, 0), TRUE);
                font.TextOut(sHealth,
                    r3.left,
                    cpRenderPosition.y + font.GetFontHeight(FALSE),
                    rClip,
                    CVIDINF_SURFACE_BACK);
                font.GetRes()->Release();
            }
        }
    }

    if (m_bLevelUp) {
        m_portraitIconVidCell.SequenceSet(254);
        m_portraitIconVidCell.FrameSet(0);
        m_portraitIconVidCell.Render(CVIDINF_SURFACE_BACK,
            r3.right - PORTRAIT_ICON_SIZE.x * nScale,
            r3.top + PORTRAIT_ICON_SIZE.y * nScale,
            r3 & rClip,
            NULL,
            0,
            0,
            -1);
    }
}

// 0x705AD0
void CGameSprite::RenderToMapScreen(const CRect& rClipBase, const CPoint& ptCharPos)
{
    CRect rClip(rClipBase);
    INT nScale = g_pBaldurChitin->field_4A28 ? 2 : 1;
    if (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC
        || InControl()) {
        if (!IcewindMisc::IsDead(this)) {
            CPoint center;
            CSize axes;
            COLORREF rgbColor;

            center.x = nScale * (4 * ptCharPos.x / 32) - rClip.left;
            center.y = nScale * (4 * ptCharPos.y / 32) - rClip.top;
            rClip.OffsetRect(-rClip.left, -rClip.top);

            axes.cx = GetAnimation()->GetPersonalSpace();
            if (!g_pBaldurChitin->field_4A28) {
                axes.cx = max(axes.cx - 2, 3);
            }
            axes.cy = 3 * GetAnimation()->GetPersonalSpace() / 4;

            rgbColor = g_pBaldurChitin->GetCurrentVideoMode()->ApplyBrightnessContrast(GetMapScreenColor());

            static_cast<CVidInf*>(g_pBaldurChitin->GetCurrentVideoMode())->BKRenderEllipse(center, axes, rClip, rgbColor);
        }
    }
}

// 0x705CC0
COLORREF CGameSprite::GetMapScreenColor()
{
    COLORREF rgbColor;
    BYTE enemyAlly = m_liveTypeAI.GetEnemyAlly();
    if (enemyAlly == CAIObjectType::EA_PC
        || enemyAlly == CAIObjectType::EA_FAMILIAR
        || enemyAlly == CAIObjectType::EA_ALLY
        || enemyAlly == CAIObjectType::EA_0x847C3A
        || enemyAlly == CAIObjectType::EA_CONTROLLED
        || enemyAlly == CAIObjectType::EA_GOODCUTOFF) {
        if (m_moraleFailure) {
            rgbColor = CMarker::PC_MORALE_FAILURE_COLOR;
        } else if (InControl() && m_nUnselectableCounter == 0) {
            if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter()) {
                rgbColor = CMarker::PC_SELECTED_COLOR;
            } else {
                rgbColor = CMarker::PC_COLOR;
            }
        } else {
            if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter()) {
                rgbColor = CMarker::PC_NONECONTROLED_SELECTED_COLOR;
            } else {
                rgbColor = CMarker::PC_NONECONTROLED_COLOR;
            }
        }
    } else if (enemyAlly == CAIObjectType::EA_ENEMY
        || enemyAlly == CAIObjectType::EA_CHARMED_PC) {
        rgbColor = CMarker::ENEMY_COLOR;
    } else if (enemyAlly == CAIObjectType::EA_NEUTRAL) {
        rgbColor = CMarker::NEUTRAL_COLOR;
    } else {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 8754
        UTIL_ASSERT(FALSE);
    }

    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == g_pBaldurChitin->GetActiveEngine()->GetPickedCharacter()) {
        ULONG nQuarterPeriod = CMarker::PICKED_FLASH_PERIOD / 4;
        ULONG nAsyncCounter = g_pBaldurChitin->nAUCounter % CMarker::PICKED_FLASH_PERIOD;
        BYTE red;
        BYTE green;
        BYTE blue;

        if (nAsyncCounter < nQuarterPeriod) {
            red = static_cast<BYTE>(nAsyncCounter * GetRValue(rgbColor) / nQuarterPeriod);
            green = static_cast<BYTE>(nAsyncCounter * GetGValue(rgbColor) / nQuarterPeriod);
            blue = static_cast<BYTE>(nAsyncCounter * GetBValue(rgbColor) / nQuarterPeriod);
        } else if (nAsyncCounter < nQuarterPeriod * 2) {
            nAsyncCounter -= nQuarterPeriod;
            red = static_cast<BYTE>(GetRValue(rgbColor) + nAsyncCounter * (255 - GetRValue(rgbColor)) / nQuarterPeriod);
            green = static_cast<BYTE>(GetGValue(rgbColor) + nAsyncCounter * (255 - GetGValue(rgbColor)) / nQuarterPeriod);
            blue = static_cast<BYTE>(GetBValue(rgbColor) + nAsyncCounter * (255 - GetBValue(rgbColor)) / nQuarterPeriod);
        } else if (nAsyncCounter < nQuarterPeriod * 3) {
            nAsyncCounter -= nQuarterPeriod * 2;
            red = static_cast<BYTE>(255 - nAsyncCounter * (255 - GetRValue(rgbColor)) / nQuarterPeriod);
            green = static_cast<BYTE>(255 - nAsyncCounter * (255 - GetGValue(rgbColor)) / nQuarterPeriod);
            blue = static_cast<BYTE>(255 - nAsyncCounter * (255 - GetBValue(rgbColor)) / nQuarterPeriod);
        } else {
            nAsyncCounter -= nQuarterPeriod * 3;
            red = static_cast<BYTE>(GetRValue(rgbColor) - nAsyncCounter * GetRValue(rgbColor) / nQuarterPeriod);
            green = static_cast<BYTE>(GetGValue(rgbColor) - nAsyncCounter * GetGValue(rgbColor) / nQuarterPeriod);
            blue = static_cast<BYTE>(GetBValue(rgbColor) - nAsyncCounter * GetBValue(rgbColor) / nQuarterPeriod);
        }

        rgbColor = RGB(red, green, blue);
    }

    return rgbColor;
}

// 0x705FD0
void CGameSprite::Select()
{
    m_bSelected = TRUE;
    m_firstActionSound = TRUE;
    m_pArea->m_pGame->GetGroup()->Add(this);
    if (m_targetId != CGameObjectArray::INVALID_INDEX && Orderable(FALSE)) {
        CGameObject* pObject;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                static_cast<CGameSprite*>(pObject)->m_marker.SetType(CMarker::RECTICLE);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }
}

// 0x7060C0
void CGameSprite::Unselect()
{
    CGameSprite* pSprite = NULL;

    m_bSelected = FALSE;

    BYTE rc;
    do {
        rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_id,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pSprite),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

    if (rc == CGameObjectArray::SUCCESS) {
        if (pSprite != NULL) {
            g_pBaldurChitin->GetObjectGame()->GetGroup()->Remove(pSprite);
        }

        g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_id,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);

        if (m_targetId != CGameObjectArray::INVALID_INDEX && Orderable(FALSE)) {
            CGameObject* pObject;

            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                    static_cast<CGameSprite*>(pObject)->m_marker.SetType(CMarker::ELLIPSE);
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
    }
}

// 0x706220
void CGameSprite::SetCursor(LONG nToolTip)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (m_canBeSeen > 0) {
        switch (pGame->GetState()) {
        case 0:
            // NOTE: Unsigned compare.
            if (static_cast<DWORD>(nToolTip) < static_cast<DWORD>(pGame->GetOptions()->m_nTooltips)) {
                if (pGame->GetGroup()->GetCount() != 0) {
                    if ((pGame->GetCharacterPortraitNum(m_id) != -1
                            || pGame->IsAlly(m_id)
                            || pGame->IsFamiliar(m_id)
                            || m_typeAI.GetEnemyAlly() <= CAIObjectType::EA_CONTROLCUTOFF)
                        && (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC || InControl())) {
                        if (InControl()) {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
                        } else {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(46, FALSE);
                        }
                    } else {
                        if (m_typeAI.GetEnemyAlly() < CAIObjectType::EA_EVILCUTOFF) {
                            if (pGame->GetGroup()->IsPartyLeader()) {
                                if ((m_baseStats.m_flags & 0x80000) != 0) {
                                    g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
                                } else {
                                    g_pBaldurChitin->GetObjectCursor()->SetCursor(18, FALSE);
                                }
                            } else {
                                g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
                            }
                        } else {
                            g_pBaldurChitin->GetObjectCursor()->SetCursor(12, FALSE);
                        }
                    }
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(0, FALSE);
                }
            } else {
                SetCharacterToolTip(NULL);
            }
            break;
        case 1:
            if (pGame->GetIconIndex() != -1) {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(pGame->GetIconIndex(), FALSE);
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetCursor(20, FALSE);
            }
            break;
        case 2:
            switch (pGame->GetIconIndex()) {
            case 12:
                g_pBaldurChitin->GetObjectCursor()->SetCursor(12, FALSE);
                if (m_bSelected && pGame->GetGroup()->GetCount() == 1) {
                    g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                }
                break;
            case 18:
                g_pBaldurChitin->GetObjectCursor()->SetCursor(18, FALSE);
                if (!pGame->GetGroup()->IsPartyLeader()) {
                    g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                }
                break;
            case 36:
            case 40:
                if (1) {
                    g_pBaldurChitin->GetObjectCursor()->SetCursor(40, FALSE);
                    if (m_bSelected) {
                        g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                    }

                    SHORT nPortrait = g_pBaldurChitin->GetScreenWorld()->GetSelectedCharacter();
                    LONG nCharacterId = pGame->GetCharacterId(nPortrait);

                    CGameSprite* pSprite;

                    BYTE rc;
                    do {
                        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            reinterpret_cast<CGameObject**>(&pSprite),
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS) {
                        if (pSprite->m_baseStats.m_skills[CGAMESPRITE_SKILL_PICK_POCKET] == 0) {
                            g_pBaldurChitin->GetObjectCursor()->SetGreyScale(TRUE);
                            pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                                CGameObjectArray::THREAD_ASYNCH,
                                INFINITE);
                        }
                    }
                }
                break;
            case 255:
                g_pBaldurChitin->GetObjectCursor()->SetCursor(20, FALSE);
                break;
            default:
                g_pBaldurChitin->GetObjectCursor()->SetCursor(pGame->GetIconIndex(), FALSE);
                break;
            }
        case 3:
            g_pBaldurChitin->GetObjectCursor()->SetCursor(6, FALSE);
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 9053
            UTIL_ASSERT(FALSE);
        }
    } else {
        CGameObject::SetCursor(nToolTip);
    }
}

// 0x706720
void CGameSprite::SetCharacterToolTip(CUIControlBase* pControl)
{
    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1) {
        if (m_baseStats.m_name != -1) {
            CString sHitPoints;
            if (!m_derivedStats.m_spellStates[SPLSTATE_SUPPRESS_HP_INFO]) {
                sHitPoints.Format("%d/%d",
                    m_baseStats.m_hitPoints,
                    m_derivedStats.m_nMaxHitPoints);
            } else {
                sHitPoints.Format("%c/%c", '?', '?');
            }
            g_pBaldurChitin->GetObjectCursor()->SetToolTip(m_baseStats.m_name,
                pControl,
                sHitPoints);
        } else {
            CString sHitPoints;
            if (!m_derivedStats.m_spellStates[SPLSTATE_SUPPRESS_HP_INFO]) {
                sHitPoints.Format("%s\n%d/%d",
                    (LPCSTR)m_sName,
                    m_baseStats.m_hitPoints,
                    m_derivedStats.m_nMaxHitPoints);
            } else {
                sHitPoints.Format("%s\n%c/%c", (LPCSTR)m_sName, '?', '?');
            }
            g_pBaldurChitin->GetObjectCursor()->SetToolTip(m_baseStats.m_name,
                pControl,
                sHitPoints);
        }
    } else {
        BOOLEAN bReturn = FALSE;
        STR_RES strRes;
        if ((m_baseStats.m_flags & 0x1000) == 0) {
            if (m_typeAI.GetEnemyAlly() >= CAIObjectType::EA_EVILCUTOFF
                || m_typeAI.GetEnemyAlly() <= CAIObjectType::EA_GOODCUTOFF) {
                if (m_baseStats.m_hitPoints == m_derivedStats.m_nMaxHitPoints) {
                    bReturn = g_pBaldurChitin->GetTlkTable().Fetch(2943, strRes);
                } else {
                    INT nHealthPercent = 100 * m_baseStats.m_hitPoints / m_derivedStats.m_nMaxHitPoints;
                    if (nHealthPercent > 75) {
                        bReturn = g_pBaldurChitin->GetTlkTable().Fetch(2944, strRes);
                    } else if (nHealthPercent > 50) {
                        bReturn = g_pBaldurChitin->GetTlkTable().Fetch(2945, strRes);
                    } else if (nHealthPercent > 25) {
                        bReturn = g_pBaldurChitin->GetTlkTable().Fetch(2946, strRes);
                    } else {
                        bReturn = g_pBaldurChitin->GetTlkTable().Fetch(2947, strRes);
                    }
                }
            }
        }

        if ((m_baseStats.m_flags & 0x1) != 0) {
            if (m_baseStats.m_name != -1) {
                g_pBaldurChitin->GetObjectCursor()->SetToolTip(m_baseStats.m_name,
                    NULL,
                    strRes.szText);
            } else {
                if (bReturn) {
                    g_pBaldurChitin->GetObjectCursor()->SetToolTip(-1,
                        NULL,
                        m_sName + "\n" + strRes.szText);
                } else {
                    g_pBaldurChitin->GetObjectCursor()->SetToolTip(-1,
                        NULL,
                        m_sName);
                }
            }
        } else if (m_baseStats.m_apparentName != -1) {
            g_pBaldurChitin->GetObjectCursor()->SetToolTip(m_baseStats.m_apparentName,
                NULL,
                strRes.szText);
        } else {
            if (bReturn) {
                g_pBaldurChitin->GetObjectCursor()->SetToolTip(-1,
                    NULL,
                    m_sName + "\n" + strRes.szText);
            } else {
                g_pBaldurChitin->GetObjectCursor()->SetToolTip(-1,
                    NULL,
                    m_sName + "\n" + strRes.szText);
            }
        }
    }
}

// 0x706B40
void CGameSprite::SetDirection(const CPoint& posDest)
{
    if (m_pos == posDest) {
        // NOTE: Uninline.
        SetDirection(m_nNewDirection);
    } else {
        // NOTE: Uninline.
        SetDirection(GetDirection(posDest));
    }
}

// 0x706C80
void CGameSprite::SetDirection(SHORT direction)
{
    m_nNewDirection = direction;
    m_deltaDirection = direction > m_nDirection ? 1 : -1;

    if (direction - m_nDirection > 8 || direction - m_nDirection <= -8) {
        // FIXME: Same value?
        m_deltaDirection = direction > m_nDirection ? 1 : -1;
    }

    if (abs(direction - m_nDirection) == 8) {
        m_skipDeltaDirection = 4;
    } else {
        m_skipDeltaDirection = 0;
    }

    if (m_nSequence == CGAMESPRITE_SEQ_WALK && m_walkBackwards) {
        if (abs(direction - m_nDirection) == 8) {
            m_nNewDirection = (direction + 8) % 16;
            m_deltaDirection = -m_deltaDirection;
        } else {
            m_walkBackwards = FALSE;
        }
    }
}

// 0x706F80
void CGameSprite::SetFacing(SHORT direction)
{
    m_nDirection = direction;
    m_nNewDirection = direction;

    // NOTE: Uninline.
    GetAnimation()->ChangeDirection(direction);

    if (m_derivedStats.m_visualEffects[IWD_VFX_BLUR]) {
        InitBlur();
    }
}

// 0x706FE0
void CGameSprite::InitBlur()
{
    m_posOld = m_pos;

    if (m_nDirection >= 2 && m_nDirection <= 7) {
        // NOTE: Uninline.
        m_posOld.x += GetAnimation()->GetMoveScale();
    } else if (m_nDirection >= 10) {
        // NOTE: Uninline.
        m_posOld.x -= GetAnimation()->GetMoveScale();
    }

    if (m_nDirection <= 3 || m_nDirection >= 14) {
        // NOTE: Uninline.
        m_posOld.y -= 3 * GetAnimation()->GetMoveScale() / 4;
    } else if (m_nDirection >= 6 && m_nDirection <= 11) {
        // NOTE: Uninline.
        m_posOld.y += 3 * GetAnimation()->GetMoveScale() / 4;
    }
}

// 0x707170
void CGameSprite::SetIdleSequence()
{
    // NOTE: Uninline.
    SHORT nIdleSequence = GetIdleSequence();

    // NOTE: Uninline.
    BOOL bIsEndOfSequence = m_animation.IsEndOfSequence();

    if (m_nSequence != nIdleSequence || bIsEndOfSequence) {
        SetSequence(nIdleSequence);
    }
}

// 0x7071F0
SHORT CGameSprite::GetIdleSequence()
{
    if (m_pArea != NULL
        && m_pArea->m_bAreaLoaded
        && g_pBaldurChitin->GetObjectGame()->sub_5AC0A0(m_pArea)) {
        return 7; // SEQ_READY
    } else {
        return 6; // SEQ_HEAD_TURN
    }
}

// 0x707230
void CGameSprite::SetSequence(SHORT nSequence)
{
    if (nSequence == SEQ_WALK) {
        // NOTE: Uninline.
        if (m_animation.GetMoveScale() == 0) {
            return;
        }
    }

    if (m_nSequence == nSequence) {
        // NOTE: Uninline.
        if (m_nSequence != GetIdleSequence()) {
            return;
        }
    }

    if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
        && (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
            || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1)
        && m_nSequence != nSequence
        && m_pArea == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()
        && m_pArea != NULL) {
        m_currSndArmor = 0;
        m_sndArmor[m_currSndArmor].Stop();

        // NOTE: Uninline.
        char* sndArmor = m_animation.GetSndArmor();

        if (sndArmor[0] != '\0') {
            // NOTE: Uninline.
            m_sndArmor[m_currSndArmor].SetResRef(CResRef(sndArmor), TRUE, TRUE);
            delete sndArmor;

            CPoint ear;
            LONG earZ;
            g_pBaldurChitin->cSoundMixer.GetListenPosition(ear, earZ);

            LONG priority = max(99 - 99 * ((ear.y - m_pos.y) * (ear.y - m_pos.y) / 144 + (ear.x - m_pos.x) * (ear.x - m_pos.x) / 256) / 6400, 0);
            m_sndArmor[m_currSndArmor].SetPriority(static_cast<BYTE>(priority));

            m_sndArmor[m_currSndArmor].Play(m_pos.x, m_pos.y, m_posZ, FALSE);
        }
    }

    if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
        && (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
            || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1)
        && m_nSequence != nSequence
        && m_pArea == g_pBaldurChitin->GetObjectGame()->GetVisibleArea()
        && m_pArea != NULL
        && nSequence != SEQ_DIE
        && nSequence != SEQ_SLEEP
        && nSequence != SEQ_TWITCH
        && nSequence != SEQ_WALK
        && nSequence != SEQ_AWAKE) {
        if (!m_sndReady.IsSoundPlaying()) {
            // NOTE: Uninline.
            char* sndReady = m_animation.GetSndReady();

            if (sndReady[0] != '\0') {
                // NOTE: Uninline.
                m_sndReady.SetResRef(CResRef(sndReady), TRUE, TRUE);

                delete sndReady;

                m_sndReady.SetLoopingFlag(1);
                m_sndReady.Play(m_pos.x, m_pos.y, m_posZ, FALSE);
            }
        }
    } else {
        m_sndReady.Stop();
    }

    SHORT nDirection = m_nDirection & 0xFE;
    CItem* curWeapon;
    const ITEM_ABILITY* curAttack;

    switch (nSequence) {
    case SEQ_ATTACK:
        curWeapon = m_equipment.m_items[m_equipment.m_selectedWeapon];
        if (curWeapon != NULL) {
            curWeapon->Demand();
            curAttack = curWeapon->GetAbility(m_equipment.m_selectedWeaponAbility);
        } else {
            SelectWeaponAbility(CGameSpriteEquipment::SLOT_FIST, 0, 0, 1);
            curAttack = &DEFAULT_ATTACK;
        }

        if (curAttack == NULL) {
            curAttack = &DEFAULT_ATTACK;
        }

        if (curAttack->type != 2) {
            LONG totalProb = 0;
            LONG prob = rand() % 100 + 1;
            for (int index = 0; index < 6 && prob > totalProb; index++) {
                totalProb += curAttack->attackProbability[index];
                if (prob <= totalProb) {
                    switch (index) {
                    case 0:
                        nSequence = SEQ_ATTACK_SLASH;
                        break;
                    case 1:
                        nSequence = SEQ_ATTACK_BACKSLASH;
                        break;
                    case 2:
                        nSequence = SEQ_ATTACK_JAB;
                        break;
                    default:
                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                        // __LINE__: 9748
                        UTIL_ASSERT(FALSE);
                    }
                    break;
                }
            }
        } else {
            nSequence = SEQ_SHOOT;
        }

        if (curWeapon != NULL) {
            curWeapon->Release();
        }

        if (nSequence == SEQ_HEAD_TURN) {
            // NOTE: Uninline.
            nSequence = GetIdleSequence();
        }
        break;
    case SEQ_DAMAGE:
        m_nBloodFlashAmount = 128;
        m_nDamageLocatorTime = 128;
        m_bBloodFlashOn = TRUE;
        g_pBaldurChitin->GetObjectGame()->UpdatePortrait(g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id), 1);
        break;
    case SEQ_TWITCH:
        m_nTwitches = 0;
        break;
    case SEQ_WALK:
        nDirection = m_nDirection;
        break;
    }

    if (nSequence != SEQ_DIE || m_nSequence != SEQ_SLEEP) {
        // NOTE: Uninline.
        m_nSequence = m_animation.SetSequence(nSequence);
    }

    if (m_nNewDirection == m_nDirection) {
        m_nDirection = nDirection;
        m_nNewDirection = nDirection;
    } else {
        m_nDirection = nDirection;
    }
}

// 0x707980
void CGameSprite::SetTarget(const CPoint& target, BOOL collisionPath)
{
    m_pathSearchInvalidDest = FALSE;
    if (m_pPath != NULL) {
        delete m_pPath;
        m_pPath = NULL;
    }

    DropSearchRequest();

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && InControl()) {
        CMessage* message = new CMessageDropPath(m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE, CMessage::BROADCAST_OTHERS);
    }

    m_currentSearchRequest = new CSearchRequest();
    if (m_currentSearchRequest != NULL) {
        m_currentSearchRequest->m_searchBitmap = &(m_pArea->m_search);

        if (m_animation.GetListType() == LIST_FLIGHT) {
            memcpy(m_currentSearchRequest->m_terrainTable, m_flightTerrainTable, sizeof(m_currentSearchRequest->m_terrainTable));
        } else {
            memcpy(m_currentSearchRequest->m_terrainTable, m_terrainTable, sizeof(m_currentSearchRequest->m_terrainTable));
        }

        m_currentSearchRequest->m_collisionSearch = collisionPath;
        if (collisionPath) {
            m_currentSearchRequest->m_collisionDelay = rand() % 15;
        } else {
            m_currentSearchRequest->m_collisionDelay = 0;
        }
        m_currentSearchRequest->m_removeSelf = m_animation.GetListType() != LIST_FLIGHT;
        m_currentSearchRequest->m_pathSmooth = m_animation.GetPathSmooth();
        m_currentSearchRequest->m_sourceId = m_id;
        m_currentSearchRequest->m_nTargetPoints = 1;
        m_currentSearchRequest->m_targetPoints = new POINT[m_currentSearchRequest->m_nTargetPoints];
        if (m_currentSearchRequest->m_targetPoints) {
            m_currentSearchRequest->m_targetPoints[0] = target;
            if (m_currentSearchRequest->m_collisionDelay == 0) {
                CSingleLock lock(&(g_pBaldurChitin->GetObjectGame()->field_1B58), TRUE);
                g_pBaldurChitin->GetObjectGame()->m_searchRequests.AddTail(m_currentSearchRequest);
                g_pBaldurChitin->GetObjectGame()->m_searchRequestListEmpty = FALSE;
                ReleaseSemaphore(g_pBaldurChitin->GetObjectGame()->m_hSearchThread, 1, NULL);
                lock.Unlock();
            }

            // NOTE: Uninline.
            SetIdleSequence();
        } else {
            delete m_currentSearchRequest;
            m_currentSearchRequest = NULL;
        }
    }
}

// 0x707D40
void CGameSprite::SetTarget(CSearchRequest* pSearchRequest, BOOL collisionPath, BYTE frontList)
{
    m_pathSearchInvalidDest = FALSE;
    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && InControl()) {
        CMessage* message = new CMessageDropPath(m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE, CMessage::BROADCAST_OTHERS);
    }

    if (frontList == CSearchRequest::LIST_FRONT) {
        if (m_pPath != NULL) {
            delete m_pPath;
            m_pPath = NULL;
        }
    }

    if (m_currentSearchRequest != pSearchRequest) {
        DropSearchRequest();
    }
    m_currentSearchRequest = pSearchRequest;

    m_currentSearchRequest->m_serviceState = CSearchRequest::STATE_WAITING;
    m_currentSearchRequest->m_collisionSearch = collisionPath;
    if (collisionPath) {
        m_currentSearchRequest->m_collisionDelay = rand() % 15;
    } else {
        m_currentSearchRequest->m_collisionDelay = 0;
    }
    m_currentSearchRequest->m_removeSelf = pSearchRequest->m_removeSelf;
    m_currentSearchRequest->m_frontList = frontList;

    if (!m_currentSearchRequest->m_collisionDelay) {
        CSingleLock lock(&(g_pBaldurChitin->GetObjectGame()->field_1B58), TRUE);
        if (frontList == CSearchRequest::LIST_FRONT) {
            g_pBaldurChitin->GetObjectGame()->m_searchRequests.AddTail(m_currentSearchRequest);
            g_pBaldurChitin->GetObjectGame()->m_searchRequestListEmpty = FALSE;
        } else {
            g_pBaldurChitin->GetObjectGame()->m_searchRequestsBack.AddTail(m_currentSearchRequest);
        }
        ReleaseSemaphore(g_pBaldurChitin->GetObjectGame()->m_hSearchThread, 1, NULL);
        lock.Unlock();
    }

    // NOTE: Uninline.
    SetIdleSequence();
}

// 0x707FB0
void CGameSprite::SetTargetFly(BYTE circleType, BYTE facing, LONG radius)
{
    m_pathSearchInvalidDest = FALSE;
    if (m_pPath != NULL) {
        delete m_pPath;
        m_pPath = NULL;
    }

    DropSearchRequest();

    m_radius = static_cast<SHORT>(radius);
    m_circleFacing = facing;
    m_fCurrCircleChange = 0.0;
    m_posOld = m_pos;
    m_turningAbout = FALSE;
    m_walkBackwards = FALSE;
    SetSequence(CGAMESPRITE_SEQ_WALK);

    if (circleType == FLY_0) {
        m_circleFacing = 0;
        m_fCircleChange = static_cast<double>(m_radius / m_animation.GetMoveScale());
    } else {
        m_fCircleChange = static_cast<double>(m_radius) * static_cast<double>(circleType) * 3.1415926535 / static_cast<double>(m_animation.GetMoveScale()) / 2.0;

        switch (abs(4 * ((m_nDirection + 2) / 4))) {
        case 0:
            m_posOld.x += m_radius * m_circleFacing;
            break;
        case 4:
            m_posOld.y += 3 * m_radius * m_circleFacing / 4;
            break;
        case 8:
            m_posOld.x -= m_radius * m_circleFacing;
            break;
        case 12:
            m_posOld.y -= 3 * m_radius * m_circleFacing / 4;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 10009
            UTIL_ASSERT(FALSE);
        }

        m_fDirectionOffset = static_cast<double>(8 - m_nDirection) * 3.1415926535 / 8.0;
        if (m_circleFacing == FLY_RIGHT) {
            m_fDirectionOffset += 3.1415926535;
        }
    }
}

// 0x708280
void CGameSprite::StartSpriteEffect(BYTE spriteEffect, BYTE intensityLevel, BYTE effectDuration, BOOLEAN a4)
{
    m_spriteEffectDuration = 0;
    if (m_pSpriteEffectArray != NULL) {
        delete m_pSpriteEffectArray;
        m_pSpriteEffectArray = NULL;
        delete m_pSpriteEffectArrayPosition;
        m_pSpriteEffectArrayPosition = NULL;
    }

    switch (spriteEffect) {
    case 0:
        field_5304 = 0;
        m_spriteEffectFlags = 0;

        if (m_animation.GetColorBlood() != 255) {
            m_spriteSplashPalette.SetRange(0,
                m_animation.GetColorBlood(),
                *g_pBaldurChitin->GetObjectGame()->GetMasterBitmap());
            m_spriteSplashVidCell.SetPalette(m_spriteSplashPalette);
            if (intensityLevel == CBlood::GUSH_LOW) {
                m_spriteSplashVidCell.SetResRef(CResRef("BLOODS"), FALSE, TRUE, TRUE);
            } else if (intensityLevel == CBlood::GUSH_MEDIUM) {
                m_spriteSplashVidCell.SetResRef(CResRef("BLOODM"), FALSE, TRUE, TRUE);
            } else if (intensityLevel == CBlood::GUSH_HIGH
                || intensityLevel == CBlood::EXPLODING_DEATH) {
                m_spriteSplashVidCell.SetResRef(CResRef("BLOODL"), FALSE, TRUE, TRUE);
            } else if (intensityLevel == CBlood::PULSATING_ARTERY) {
                m_spriteSplashVidCell.SetResRef(CResRef("BLOODCR"), FALSE, TRUE, TRUE);
            } else {
                m_spriteEffectDuration = 0;
                return;
            }

            m_spriteSplashVidCell.SequenceSet(0);
            m_spriteSplashVidCell.FrameSet(0);
            m_spriteEffectSequenceLength = 1;
            m_spriteEffectBaseIntensity = 0;
            m_spriteEffectRandomIntensity = 1;
        } else {
            m_spriteEffectDuration = 0;
            return;
        }
        break;
    case 1:
        if (a4 == TRUE) {
            AddEffect(IcewindMisc::CreateEffectVisualSpellHit(this, 0x3F, 10, 0),
                EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        m_spriteEffectFlags = 0x200;
        m_spriteEffectVidCell.SetResRef(CResRef("FireL"), FALSE, TRUE, TRUE);
        m_spriteEffectVidCell.SequenceSet(0);
        m_spriteEffectSequenceNumber = 1;
        m_spriteEffectSequenceLength = m_spriteEffectVidCell.GetSequenceLength(m_spriteEffectVidCell.m_nCurrentSequence, FALSE);
        m_effectExtendDirection = 1;
        switch (intensityLevel) {
        case 0:
            m_spriteEffectBaseIntensity = -2;
            m_spriteEffectRandomIntensity = 4;
            break;
        case 1:
            m_spriteEffectBaseIntensity = -1;
            m_spriteEffectRandomIntensity = 3;
            break;
        case 2:
            m_spriteEffectBaseIntensity = 0;
            m_spriteEffectRandomIntensity = 2;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 10204
            UTIL_ASSERT(FALSE);
        }
        break;
    case 2:
        if (a4 == TRUE) {
            AddEffect(IcewindMisc::CreateEffectVisualSpellHit(this, 0x41, 10, 0),
                EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        m_spriteEffectFlags = 0x200;
        m_spriteEffectVidCell.SetResRef(CResRef("ElectrL"), FALSE, TRUE, TRUE);
        m_spriteEffectVidCell.SequenceSet(0);
        m_spriteEffectSequenceNumber = static_cast<BYTE>(m_spriteEffectVidCell.GetNumberSequences(FALSE));
        m_spriteEffectSequenceLength = m_spriteEffectVidCell.GetSequenceLength(m_spriteEffectVidCell.m_nCurrentSequence, FALSE);
        m_effectExtendDirection = 0;
        switch (intensityLevel) {
        case 0:
            m_spriteEffectBaseIntensity = -2;
            m_spriteEffectRandomIntensity = 4;
            break;
        case 1:
            m_spriteEffectBaseIntensity = -1;
            m_spriteEffectRandomIntensity = 3;
            break;
        case 2:
            m_spriteEffectBaseIntensity = 0;
            m_spriteEffectRandomIntensity = 2;
            break;
        default:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 10259
            UTIL_ASSERT(FALSE);
        }
        break;
    case 3:
        if (a4 == TRUE) {
            AddEffect(IcewindMisc::CreateEffectVisualSpellHit(this, 0x40, 10, 0),
                EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        m_spriteEffectFlags = 0x200;
        m_spriteEffectVidCell.SetResRef(CResRef("ColdL"), FALSE, TRUE, TRUE);
        m_spriteEffectVidCell.SequenceSet(0);
        m_spriteEffectSequenceNumber = 1;
        m_spriteEffectSequenceLength = m_spriteEffectVidCell.GetSequenceLength(m_spriteEffectVidCell.m_nCurrentSequence, FALSE);
        m_effectExtendDirection = 1;
        m_spriteEffectBaseIntensity = 0;
        m_spriteEffectBaseIntensity = 1;
        break;
    case 4:
        if (a4 == TRUE) {
            AddEffect(IcewindMisc::CreateEffectVisualSpellHit(this, 0x42, 10, 0),
                EFFECT_LIST_TIMED,
                TRUE,
                TRUE);
        }
        m_spriteEffectFlags = 0x200;
        m_spriteEffectVidCell.SetResRef(CResRef("None"), FALSE, TRUE, TRUE);
        m_spriteEffectVidCell.SequenceSet(0);
        m_spriteEffectSequenceNumber = static_cast<BYTE>(m_spriteEffectVidCell.GetNumberSequences(FALSE));
        m_spriteEffectSequenceLength = m_spriteEffectVidCell.GetSequenceLength(m_spriteEffectVidCell.m_nCurrentSequence, FALSE);
        m_effectExtendDirection = 0;
        // FIXME: `m_spriteEffectBaseIntensity` and `m_spriteEffectRandomIntensity` not initialized.
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 10324
        UTIL_ASSERT(FALSE);
    }

    int cnt = m_spriteEffectSequenceLength * (m_spriteEffectBaseIntensity + m_spriteEffectRandomIntensity - 1);
    if (m_spriteEffectSequenceLength * (m_spriteEffectBaseIntensity + m_spriteEffectRandomIntensity - 1) > 0) {
        m_pSpriteEffectArray = new USHORT[cnt];
        if (m_pSpriteEffectArray != NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 10332
            UTIL_ASSERT(FALSE);
        }

        m_pSpriteEffectArrayPosition = new POINT[cnt];
        if (m_pSpriteEffectArrayPosition != NULL) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 10338
            UTIL_ASSERT(FALSE);
        }

        memset(m_pSpriteEffectArray, 0xFF, sizeof(USHORT) * cnt);
    }

    m_spriteEffectDuration = m_spriteEffectSequenceLength + effectDuration + 9;
    m_animation.CalculateFxRect(m_rSpriteEffectFX, m_ptSpriteEffectReference, m_posZ);
}

// 0x708D50
void CGameSprite::RenderSpriteCover(CVidMode* pVidMode, INT nSurface, CVidCell* pVidCell, const IcewindCVisualEffect& vfx)
{
    CRect rFX;
    CSize objectSize;
    CRect rGCBounds;
    CPoint newPos;
    CPoint ptReference;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 10436
    UTIL_ASSERT(pVidMode != NULL);

    DWORD dwRenderFlags = vfx.m_dwFlags;

    BOOLEAN bDithered = m_id == m_pArea->m_iPicked
        || g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bAlwaysDither
        || g_pBaldurChitin->GetObjectGame()->m_bForceDither;

    pVidCell->GetCurrentCenterPoint(ptReference, FALSE);
    pVidCell->GetCurrentFrameSize(objectSize, FALSE);

    rFX.SetRect(0, 0, objectSize.cx, objectSize.cy);

    newPos.x = m_pos.x;
    newPos.y = m_pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y + 1;

    if (vfx.field_1 == TRUE || vfx.field_3 == TRUE) {
        dwRenderFlags |= CInfinity::FXPREP_COPYFROMBACK | 0x2;
    } else {
        dwRenderFlags |= CInfinity::FXPREP_CLEARFILL | 0x1;
    }

    m_pArea->GetInfinity()->FXPrep(rFX,
        dwRenderFlags,
        nSurface,
        newPos,
        ptReference);

    if (m_pArea->GetInfinity()->FXLock(rFX, dwRenderFlags)) {
        m_pArea->GetInfinity()->FXRender(pVidCell,
            ptReference.x,
            ptReference.y,
            vfx.m_dwFlags,
            vfx.m_nTransValue);

        rGCBounds.left = newPos.x - ptReference.x;
        rGCBounds.top = newPos.y + m_posZ - ptReference.y;
        rGCBounds.right = rGCBounds.left + rFX.Width();
        rGCBounds.bottom = rGCBounds.top + rFX.Height() - m_posZ;
        m_pArea->GetInfinity()->FXRenderClippingPolys(newPos.x,
            newPos.y - m_posZ,
            m_posZ,
            CPoint(ptReference.x, ptReference.y + m_posZ),
            rGCBounds,
            bDithered,
            FALSE);

        m_pArea->GetInfinity()->FXUnlock(dwRenderFlags, NULL, CPoint(0, 0));
        m_pArea->GetInfinity()->FXBltFrom(nSurface,
            rFX,
            newPos.x,
            newPos.y + m_posZ,
            ptReference.x,
            ptReference.y,
            vfx.m_dwFlags | 0x1);
    }
}

// 0x708FC0
void CGameSprite::RenderDamageArrow(CGameArea* pArea, CVidMode* pVidMode, INT nSurface)
{
    // TODO: Incomplete.
}

// 0x7093E0
void CGameSprite::RenderSpriteEffect(CVidMode* pVidMode, INT nSurface)
{
    // TODO: Incomplete.
}

// 0x709B60
void CGameSprite::UpdateSpriteEffect()
{
    SHORT nSequence;
    SHORT nNumberSequences;

    for (INT nIndex = 0; nIndex < 32; nIndex++) {
        if (!m_derivedStats.m_visualEffects[nIndex]) {
            if (nIndex == IWD_VFX_ENTANGLE
                || nIndex == IWD_VFX_WEB
                || nIndex == IWD_VFX_CLOAK_OF_FEAR) {
                field_7548[nIndex].SequenceSet(0);
                field_7548[nIndex].FrameSet(0);
            }

            if (nIndex == IWD_VFX_FIRE_SHIELD_RED
                || nIndex == IWD_VFX_FIRE_SHIELD_BLUE) {
                field_7548[nIndex].SequenceSet(0);
                field_7548[nIndex].FrameSet(0);
            }

            if (nIndex == IWD_VFX_28
                || nIndex == IWD_VFX_29) {
                field_7548[nIndex].SequenceSet(1);
                field_7548[nIndex].FrameSet(0);
            }
        }

        if (m_derivedStats.m_visualEffects[nIndex]
            && nIndex != IWD_VFX_INVISIBILITY
            && nIndex != IWD_VFX_BLUR) {
            switch (nIndex) {
            case IWD_VFX_SHROUD_OF_FLAME:
                if (field_7548[nIndex].IsEndOfSequence(FALSE)) {
                    m_sndSpriteEffect.Stop();
                    m_sndSpriteEffect.SetResRef(CResRef("MISC_01C"), TRUE, TRUE);
                    m_sndSpriteEffect.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                    m_sndSpriteEffect.Play(m_pos.x, m_pos.y, 0, FALSE);

                    nNumberSequences = field_7548[nIndex].GetNumberSequences(FALSE);
                    if (nNumberSequences != 0) {
                        field_7548[nIndex].SequenceSet(rand() % nNumberSequences);
                    } else {
                        field_7548[nIndex].SequenceSet(0);
                    }
                } else {
                    field_7548[nIndex].FrameAdvance();
                }
                break;
            case IWD_VFX_FIRE_SHIELD_RED:
            case IWD_VFX_FIRE_SHIELD_BLUE:
                if (field_7548[nIndex].IsEndOfSequence(FALSE)) {
                    field_7548[nIndex].SequenceSet(2);
                } else {
                    field_7548[nIndex].FrameAdvance();
                }
                break;
            case IWD_VFX_28:
            case IWD_VFX_29:
                if (field_7548[nIndex].IsEndOfSequence(FALSE)) {
                    field_7548[nIndex].SequenceSet(3);
                } else {
                    field_7548[nIndex].FrameAdvance();
                }
                break;
            case IWD_VFX_ENTANGLE:
            case IWD_VFX_WEB:
            case IWD_VFX_CLOAK_OF_FEAR:
                if (field_7548[nIndex].IsEndOfSequence(FALSE)) {
                    nSequence = field_7548[nIndex].GetCurrentSequenceId() + 1;
                    nNumberSequences = field_7548[nIndex].GetNumberSequences(FALSE);
                    if (nSequence >= nNumberSequences) {
                        nSequence--;
                    }
                    field_7548[nIndex].SequenceSet(nSequence);
                } else {
                    field_7548[nIndex].FrameAdvance();
                }
                break;
            case IWD_VFX_SEVEN_EYES:
                nSequence = 0;
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_THE_MIND]) {
                    nSequence++;
                }
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_THE_SWORD]) {
                    nSequence++;
                }
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_THE_MAGE]) {
                    nSequence++;
                }
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_VENOM]) {
                    nSequence++;
                }
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_THE_SPIRIT]) {
                    nSequence++;
                }
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_FORTITUDE]) {
                    nSequence++;
                }
                if (!m_derivedStats.m_spellStates[SPLSTATE_EYE_OF_STONE]) {
                    nSequence++;
                }
                if (!field_9088[IWD_VFX_SEVEN_EYES].IsSoundPlaying()) {
                    if (nSequence == 7) {
                        field_9088[IWD_VFX_SEVEN_EYES].Stop();
                    } else {
                        if (nSequence >= 5) {
                            field_9088[IWD_VFX_SEVEN_EYES].SetResRef(CResRef("AFT_M17"), TRUE, TRUE);
                        } else if (nSequence >= 2) {
                            field_9088[IWD_VFX_SEVEN_EYES].SetResRef(CResRef("AFT_M16"), TRUE, TRUE);
                        } else {
                            field_9088[IWD_VFX_SEVEN_EYES].SetResRef(CResRef("AFT_M15"), TRUE, TRUE);
                        }
                        field_9088[IWD_VFX_SEVEN_EYES].SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                        field_9088[IWD_VFX_SEVEN_EYES].Play(m_pos.x, m_pos.y, 0, FALSE);
                    }
                }
                field_7548[IWD_VFX_SEVEN_EYES].SequenceSet(nSequence);
                field_7548[IWD_VFX_SEVEN_EYES_2].SequenceSet(nSequence);
                if (field_7548[IWD_VFX_SEVEN_EYES].IsEndOfSequence(FALSE)) {
                    field_7548[IWD_VFX_SEVEN_EYES].FrameSet(0);
                    field_7548[IWD_VFX_SEVEN_EYES_2].FrameSet(0);
                } else {
                    field_7548[IWD_VFX_SEVEN_EYES].FrameAdvance();
                    field_7548[IWD_VFX_SEVEN_EYES_2].FrameAdvance();
                }
                break;
            case IWD_VFX_SEVEN_EYES_2:
                break;
            default:
                if (!field_9088[nIndex].IsSoundPlaying()) {
                    field_9088[nIndex].SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                    field_9088[nIndex].Play(m_pos.x, m_pos.y, 0, FALSE);
                }
                if (field_7548[nIndex].IsEndOfSequence(FALSE)) {
                    nNumberSequences = field_7548[nIndex].GetNumberSequences(FALSE);
                    if (nNumberSequences != 0) {
                        field_7548[nIndex].SequenceSet(rand() % nNumberSequences);
                    } else {
                        field_7548[nIndex].SequenceSet(0);
                    }
                } else {
                    field_7548[nIndex].FrameAdvance();
                }
            }
        }
    }

    if (m_spriteEffectDuration != 0) {
        if (m_spriteSplashVidCell.IsEndOfSequence(FALSE)) {
            if (m_pSpriteEffectArray != NULL) {
                CRect rFx;
                CPoint ptReference;
                m_animation.CalculateFxRect(rFx, ptReference, m_posZ);

                for (BYTE nIndex = 0; nIndex < m_spriteEffectSequenceLength; nIndex++) {
                    if ((g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime / 2) % m_spriteEffectSequenceLength == nIndex) {
                        BYTE intensity = m_spriteEffectBaseIntensity + m_spriteEffectRandomIntensity - 1;
                        memset(&(m_pSpriteEffectArray[intensity]),
                            -1,
                            sizeof(USHORT) * (intensity));
                        // TODO: Incomplete.
                    } else {
                        BYTE intensity = m_spriteEffectBaseIntensity + m_spriteEffectRandomIntensity - 1;

                        // TODO: Incomplete.
                    }
                }
            }
        } else {
            m_spriteSplashVidCell.FrameAdvance();
        }
        m_spriteEffectDuration--;
    } else {
        if (m_pSpriteEffectArray != NULL) {
            delete m_pSpriteEffectArray;
            m_pSpriteEffectArray = NULL;
            delete m_pSpriteEffectArrayPosition;
            m_pSpriteEffectArrayPosition = NULL;
        }
    }
}

// 0x70AE20
void CGameSprite::ClearMarshal(BOOL unequip)
{
    UINT nClass;
    UINT nLevel;
    POSITION pos;
    CGameEffect* nodeEffect;

    for (nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            m_spells.m_spellsByClass[nClass].m_lists[nLevel].m_List.clear();
        }
        m_spells.m_spellsByClass[nClass].m_nHighestLevel = 0;
    }

    for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        if (m_domainSpells.m_lists[nLevel].m_List.size() > 0) {
            m_domainSpells.m_lists[nLevel].m_List.clear();
            m_domainSpells.m_lists[nLevel].field_14 = 0;
            m_domainSpells.m_lists[nLevel].field_18 = 0;
        }
    }
    m_domainSpells.m_nHighestLevel = 0;

    if (m_innateSpells.m_List.size() > 0) {
        m_innateSpells.m_List.clear();
        m_innateSpells.field_14 = 0;
        m_innateSpells.field_18 = 0;
    }

    if (m_songs.m_List.size() > 0) {
        m_songs.m_List.clear();
        m_songs.field_14 = 0;
        m_songs.field_18 = 0;
    }

    if (m_shapeshifts.m_List.size() > 0) {
        m_shapeshifts.m_List.clear();
        m_shapeshifts.field_14 = 0;
        m_shapeshifts.field_18 = 0;
    }

    // NOTE: Uninline.
    m_equipment.SetSpritePointer(this);

    m_equipment.ClearMarshal(unequip);

    memset(&m_baseStats, 0, sizeof(m_baseStats));

    pos = m_timedEffectList.GetHeadPosition();
    while (pos != NULL) {
        nodeEffect = m_timedEffectList.GetNext(pos);
        delete nodeEffect;
    }

    pos = m_equipedEffectList.GetHeadPosition();
    while (pos != NULL) {
        nodeEffect = m_equipedEffectList.GetNext(pos);
        delete nodeEffect;
    }

    if (m_pDialogData != NULL) {
        delete m_pDialogData;
        m_pDialogData = NULL;
        m_nDialogData = 0;
    }
}

// 0x70AFD0
void CGameSprite::Marshal(CAreaFileCreature** pCreature)
{
    BOOL areaMove = FALSE;

    if (m_pArea == NULL) {
        m_pArea = g_pBaldurChitin->GetObjectGame()->m_pGameAreaMaster;
        m_pos.x = 0;
        m_pos.y = 0;
        areaMove = TRUE;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 11809
    UTIL_ASSERT(pCreature != NULL);

    m_baseStats.m_flags &= ~0x80000000;

    *pCreature = new CAreaFileCreature();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 11817
    UTIL_ASSERT(*pCreature != NULL);

    // FIXME: Redundant, memset is a part of constructor.
    memset(*pCreature, 0, sizeof(CAreaFileCreature));

    if (m_pos.x < -100) {
        m_pos.x = -100;
        m_fCircleChange = 0.0;
        m_fCurrCircleChange = 1.0;
    } else if (m_pos.x > m_pArea->GetInfinity()->nAreaX + 100) {
        m_pos.x = m_pArea->GetInfinity()->nAreaX + 100;
        m_fCircleChange = 0.0;
        m_fCurrCircleChange = 1.0;
    }

    if (m_pos.y < -100) {
        m_pos.y = -100;
        m_fCircleChange = 0.0;
        m_fCurrCircleChange = 1.0;
    } else if (m_pos.y > m_pArea->GetInfinity()->nAreaY + 100) {
        m_pos.y = m_pArea->GetInfinity()->nAreaY + 100;
        m_fCircleChange = 0.0;
        m_fCurrCircleChange = 1.0;
    }

    (*pCreature)->m_posX = static_cast<WORD>(m_pos.x);
    (*pCreature)->m_posY = static_cast<WORD>(m_pos.y);
    (*pCreature)->m_startingPosX = static_cast<WORD>(m_posStart.x);
    (*pCreature)->m_startingPosY = static_cast<WORD>(m_posStart.y);
    (*pCreature)->m_type = m_type;
    (*pCreature)->m_expirationTime = m_expirationTime != -1
        ? m_expirationTime / CTimerWorld::TIMESCALE_MSEC_PER_SEC
        : -1;
    (*pCreature)->m_huntingRange = m_huntingRange;
    (*pCreature)->m_followRange = m_followRange;
    (*pCreature)->m_timeOfDayVisible = m_timeOfDayVisible;

    BYTE* creatureData;
    LONG creatureSize;
    Marshal(&creatureData, &creatureSize, &((*pCreature)->m_facing), TRUE, TRUE);

    // NOTE: The original stores the pointer; a 32-bit handle on every build.
    (*pCreature)->m_creatureOffset = iwd2r::MarshalPtr(creatureData);
    (*pCreature)->m_creatureSize = creatureSize;

    (*pCreature)->m_numberTimesTalkedTo = m_nNumberOfTimesTalkedTo;

    strncpy((*pCreature)->m_scriptName, m_scriptName, SCRIPTNAME_SIZE);

    if (m_overrideScript != NULL) {
        // NOTE: Uninline.
        m_overrideScript->m_cResRef.GetResRef((*pCreature)->m_overrideScriptOverride);
    }

    if (m_special3Script != NULL) {
        // NOTE: Uninline.
        m_special3Script->m_cResRef.GetResRef((*pCreature)->m_special3ScriptOverride);
    }

    if (m_special2Script != NULL) {
        // NOTE: Uninline.
        m_special2Script->m_cResRef.GetResRef((*pCreature)->m_special2ScriptOverride);
    }

    if (m_combatScript != NULL) {
        // NOTE: Uninline.
        m_combatScript->m_cResRef.GetResRef((*pCreature)->m_combatScriptOverride);
    }

    if (m_movementScript != NULL) {
        // NOTE: Uninline.
        m_movementScript->m_cResRef.GetResRef((*pCreature)->m_movementScriptOverride);
    }

    if (m_teamScript != NULL) {
        // NOTE: Uninline.
        m_teamScript->m_cResRef.GetResRef((*pCreature)->m_teamScriptOverride);
    }

    if (m_special1Script != NULL) {
        // NOTE: Uninline.
        m_special1Script->m_cResRef.GetResRef((*pCreature)->m_special1ScriptOverride);
    }

    if (areaMove) {
        m_pArea = NULL;
    }
}

// 0x70B2F0
void CGameSprite::Marshal(BYTE** pCreature, LONG* creatureSize, WORD* facing, BOOLEAN a4, BOOLEAN a5)
{
    CCreatureFileEquipment equipment;
    CCreatureFileItem* pItems;
    DWORD nItems;
    BYTE* pEffects;
    ULONG nEffects;
    ULONG nEffectsSize;
    CCreatureFileOffsets* offsets;
    DWORD cnt;
    DWORD nIndex;
    UINT nClass;
    UINT nLevel;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 11953
    UTIL_ASSERT(pCreature != NULL && creatureSize != NULL && facing != NULL);

    // The file holds hit points without the Constitution bonus.
    m_baseStats.m_hitPoints -= static_cast<SHORT>(m_nHPCONBonusTotalOld);
    m_baseStats.m_maxHitPointsBase -= static_cast<SHORT>(m_nHPCONBonusTotalOld);
    m_derivedStats.m_nMaxHitPoints -= static_cast<SHORT>(m_nHPCONBonusTotalOld);

    m_baseStats.m_effectVersion = 1;

    *pCreature = NULL;
    memset(&equipment, 0, sizeof(equipment));
    *creatureSize = 0;
    *facing = m_nDirection;

    for (nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            *creatureSize += sizeof(CCreatureFileSpell) * static_cast<LONG>(m_spells.m_spellsByClass[nClass].m_lists[nLevel].m_List.size())
                + sizeof(unsigned int)
                + sizeof(unsigned int);
        }
    }

    for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        *creatureSize += sizeof(CCreatureFileSpell) * static_cast<LONG>(m_domainSpells.m_lists[nLevel].m_List.size())
            + sizeof(unsigned int)
            + sizeof(unsigned int);
    }

    *creatureSize += sizeof(CCreatureFileSpell) * static_cast<LONG>(m_innateSpells.m_List.size())
        + sizeof(unsigned int)
        + sizeof(unsigned int);
    *creatureSize += sizeof(CCreatureFileSpell) * static_cast<LONG>(m_songs.m_List.size())
        + sizeof(unsigned int)
        + sizeof(unsigned int);
    *creatureSize += sizeof(CCreatureFileSpell) * static_cast<LONG>(m_shapeshifts.m_List.size())
        + sizeof(unsigned int)
        + sizeof(unsigned int);

    m_equipment.Marshal(&equipment, &pItems, &nItems, a5);
    *creatureSize += sizeof(CCreatureFileEquipment) + sizeof(CCreatureFileItem) * nItems;

    if (a4) {
        m_pLocalVariables->MarshalToCharacter(this);
    } else {
        // The magic weapon slot is not saved (0x70B539).
        equipment.m_miscItem[27] = 0xFFFF;
    }

    nEffects = m_timedEffectList.Marshal(&pEffects, 1, a4 == FALSE);
    nEffects /= sizeof(CGameEffectBase);
    nEffectsSize = sizeof(CGameEffectBase) * nEffects;

    m_baseStats.field_2F8 = static_cast<BOOLEAN>(m_removeFromArea);

    *creatureSize += nEffectsSize + 900 + sizeof(CCreatureFileOffsets);

    *pCreature = new BYTE[*creatureSize];

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 12051
    UTIL_ASSERT(*pCreature != NULL);

    memset(*pCreature, 0, *creatureSize);
    memcpy(*pCreature, "CRE V2.2", 8);

    m_baseStats.m_subrace = m_startTypeAI.m_nSubRace;
    memcpy(*pCreature + 8, &m_baseStats, sizeof(m_baseStats));

    offsets = reinterpret_cast<CCreatureFileOffsets*>(*pCreature + 900);
    offsets->m_enemyAlly = m_startTypeAI.m_nEnemyAlly;
    offsets->m_general = m_liveTypeAI.m_nGeneral;
    offsets->m_race = m_startTypeAI.m_nRace;
    offsets->m_class = m_startTypeAI.m_nClass;
    offsets->m_specifics = m_startTypeAI.m_nSpecific;
    offsets->m_gender = m_startTypeAI.m_nGender;
    offsets->m_alignment = m_startTypeAI.m_nAlignment;
    offsets->m_instance = m_startTypeAI.m_nInstance;
    offsets->m_avClass = m_startTypeAI.m_nAvClass;
    offsets->m_classMask = m_startTypeAI.m_nClassMask;
    memcpy(offsets->m_specialCase, m_startTypeAI.m_SpecialCase, 5);
    strncpy(offsets->m_name, m_scriptName, SCRIPTNAME_SIZE);
    m_dialog.GetResRef(offsets->m_dialog);

    cnt = 900 + sizeof(CCreatureFileOffsets);

    for (nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            CGameSpriteSpellList* pList = &(m_spells.m_spellsByClass[nClass].m_lists[nLevel]);

            offsets->m_spellListOffset[nClass][nLevel] = cnt;
            offsets->m_spellListCount[nClass][nLevel] = static_cast<DWORD>(pList->m_List.size());
            for (nIndex = 0; nIndex < offsets->m_spellListCount[nClass][nLevel]; nIndex++) {
                memcpy(*pCreature + cnt, &(pList->m_List[nIndex]), sizeof(CCreatureFileSpell));
                cnt += sizeof(CCreatureFileSpell);
            }

            *reinterpret_cast<unsigned int*>(*pCreature + cnt) = pList->field_14;
            *reinterpret_cast<unsigned int*>(*pCreature + cnt + 4) = pList->field_18;
            cnt += sizeof(unsigned int) + sizeof(unsigned int);
        }
    }

    for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        CGameSpriteSpellList* pList = &(m_domainSpells.m_lists[nLevel]);

        offsets->m_domainListOffset[nLevel] = cnt;
        offsets->m_domainListCount[nLevel] = static_cast<DWORD>(pList->m_List.size());
        for (nIndex = 0; nIndex < offsets->m_domainListCount[nLevel]; nIndex++) {
            memcpy(*pCreature + cnt, &(pList->m_List[nIndex]), sizeof(CCreatureFileSpell));
            cnt += sizeof(CCreatureFileSpell);
        }

        *reinterpret_cast<unsigned int*>(*pCreature + cnt) = pList->field_14;
        *reinterpret_cast<unsigned int*>(*pCreature + cnt + 4) = pList->field_18;
        cnt += sizeof(unsigned int) + sizeof(unsigned int);
    }

    offsets->m_innateListOffset = cnt;
    offsets->m_innateListCount = static_cast<DWORD>(m_innateSpells.m_List.size());
    for (nIndex = 0; nIndex < offsets->m_innateListCount; nIndex++) {
        memcpy(*pCreature + cnt, &(m_innateSpells.m_List[nIndex]), sizeof(CCreatureFileSpell));
        cnt += sizeof(CCreatureFileSpell);
    }
    *reinterpret_cast<unsigned int*>(*pCreature + cnt) = m_innateSpells.field_14;
    *reinterpret_cast<unsigned int*>(*pCreature + cnt + 4) = m_innateSpells.field_18;
    cnt += sizeof(unsigned int) + sizeof(unsigned int);

    offsets->m_songListOffset = cnt;
    offsets->m_songListCount = static_cast<DWORD>(m_songs.m_List.size());
    for (nIndex = 0; nIndex < offsets->m_songListCount; nIndex++) {
        memcpy(*pCreature + cnt, &(m_songs.m_List[nIndex]), sizeof(CCreatureFileSpell));
        cnt += sizeof(CCreatureFileSpell);
    }
    *reinterpret_cast<unsigned int*>(*pCreature + cnt) = m_songs.field_14;
    *reinterpret_cast<unsigned int*>(*pCreature + cnt + 4) = m_songs.field_18;
    cnt += sizeof(unsigned int) + sizeof(unsigned int);

    offsets->m_shapeListOffset = cnt;
    offsets->m_shapeListCount = static_cast<DWORD>(m_shapeshifts.m_List.size());
    for (nIndex = 0; nIndex < offsets->m_shapeListCount; nIndex++) {
        memcpy(*pCreature + cnt, &(m_shapeshifts.m_List[nIndex]), sizeof(CCreatureFileSpell));
        cnt += sizeof(CCreatureFileSpell);
    }
    *reinterpret_cast<unsigned int*>(*pCreature + cnt) = m_shapeshifts.field_14;
    *reinterpret_cast<unsigned int*>(*pCreature + cnt + 4) = m_shapeshifts.field_18;
    cnt += sizeof(unsigned int) + sizeof(unsigned int);

    offsets->m_equipmentListOffset = cnt;
    memcpy(*pCreature + cnt, &equipment, sizeof(CCreatureFileEquipment));
    cnt += sizeof(CCreatureFileEquipment);

    if (nItems != 0) {
        offsets->m_itemListOffset = cnt;
        offsets->m_itemListCount = nItems;
        memcpy(*pCreature + cnt, pItems, sizeof(CCreatureFileItem) * nItems);
        cnt += sizeof(CCreatureFileItem) * nItems;
        delete[] pItems;
    }

    if (nEffects != 0) {
        offsets->m_effectListCount = nEffects;
        offsets->m_effectListOffset = cnt;
        memcpy(*pCreature + cnt, pEffects, nEffectsSize);
        cnt += nEffectsSize;
        delete[] reinterpret_cast<CGameEffectBase*>(pEffects);
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 12187
    UTIL_ASSERT(cnt == (DWORD)*creatureSize);

    m_baseStats.m_hitPoints += static_cast<SHORT>(m_nHPCONBonusTotalOld);
    m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(m_nHPCONBonusTotalOld);
    m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(m_nHPCONBonusTotalOld);

    m_equipedEffectList.RemoveAllOfType(this, CGAMEEFFECT_JUMPTOAREA, m_equipedEffectList.m_posCurrent, -1);
    m_timedEffectList.RemoveAllOfType(this, CGAMEEFFECT_JUMPTOAREA, m_timedEffectList.m_posCurrent, -1);
}

// 0x70BEE0
void CGameSprite::Marshal(CSavedGamePartyCreature& partyCreature, BOOLEAN bNetworkMessage)
{
    DWORD nIndex;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 12226
    UTIL_ASSERT(m_bGlobal);

    partyCreature.m_wFlags = 0;

    if (m_bSelected) {
        partyCreature.m_wFlags |= 0x1;
    }

    if (bNetworkMessage == TRUE) {
        partyCreature.m_wFlags |= 0x8000;
    }

    partyCreature.m_portraitId = g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id);

    memset(partyCreature.m_creatureResRef, 0, RESREF_SIZE);

    BYTE* pCreature;
    LONG nCreatureSize;
    WORD facing;
    Marshal(&pCreature, &nCreatureSize, &facing, TRUE, TRUE);

    // NOTE: A 32-bit handle (compat/marshal_ptr.h) where the original stores
    // the pointer; CInfGame::Marshal and CBaldurMessage::
    // DemandCharacterSlotReply resolve it.
    partyCreature.m_creatureOffset = iwd2r::MarshalPtr(pCreature);
    partyCreature.m_creatureSize = nCreatureSize;
    partyCreature.m_creatureFacing = facing;

    BOOLEAN v1 = FALSE;
    for (INT nCharacterSlot = 0; nCharacterSlot < 6; nCharacterSlot++) {
        if (g_pBaldurChitin->GetObjectGame()->GetCharacterSlot(nCharacterSlot) == m_id) {
            v1 = TRUE;
            break;
        }
    }

    if (m_pArea != NULL) {
        m_pArea->m_resRef.GetResRef(partyCreature.m_areaName);

        INT x;
        INT y;
        m_pArea->GetInfinity()->GetViewPosition(x, y);
        partyCreature.m_posViewX = static_cast<WORD>(x);
        partyCreature.m_posViewY = static_cast<WORD>(y);
    } else {
        if (v1 == TRUE) {
            CResRef sBlankResRef;
            sBlankResRef = "NO_AREA";

            partyCreature.m_wFlags |= 0x8000;
            sBlankResRef.GetResRef(partyCreature.m_areaName);
        } else {
            m_currentArea.GetResRef(partyCreature.m_areaName);
        }

        partyCreature.m_posViewX = 0;
        partyCreature.m_posViewY = 0;
    }

    partyCreature.m_posX = static_cast<WORD>(m_pos.x);
    partyCreature.m_posY = static_cast<WORD>(m_pos.y);
    partyCreature.m_nModalState = m_nModalState;

    for (nIndex = 0; nIndex < 5; nIndex++) {
        partyCreature.field_28A[nIndex] = field_4C54[nIndex];
    }

    partyCreature.m_nWeaponSet = m_nWeaponSet;

    for (nIndex = 0; nIndex < 8; nIndex++) {
        partyCreature.m_quickWeaponsItemNum[nIndex] = m_quickWeapons[nIndex].m_abilityId.m_itemNum;
        partyCreature.m_quickWeaponsAbilityNum[nIndex] = m_quickWeapons[nIndex].m_abilityId.m_abilityNum;
        partyCreature.field_2A7[nIndex] = field_3D3A[nIndex];
    }

    for (nIndex = 0; nIndex < 9; nIndex++) {
        m_quickSpells[nIndex].m_abilityId.m_res.GetResRef(partyCreature.m_quickSpellsSpellId[nIndex]);
        partyCreature.m_quickSpellsClass[nIndex] = m_quickSpells[nIndex].m_abilityId.m_nClass;
        partyCreature.field_280[nIndex] = m_quickSpells[nIndex].m_abilityId.field_1D;
        partyCreature.field_29E[nIndex] = static_cast<unsigned char>(m_quickSpells[nIndex].m_abilityId.field_1E);
    }

    for (nIndex = 0; nIndex < 3; nIndex++) {
        partyCreature.m_quickItemsItemNum[nIndex] = m_quickItems[nIndex].m_abilityId.m_itemNum;
        partyCreature.m_quickItemsAbilityNum[nIndex] = m_quickItems[nIndex].m_abilityId.m_abilityNum;
    }

    for (nIndex = 0; nIndex < 9; nIndex++) {
        m_quickInnates[nIndex].m_abilityId.m_res.GetResRef(partyCreature.m_quickInnatesSpellId[nIndex]);
    }

    for (nIndex = 0; nIndex < 9; nIndex++) {
        m_quickSongs[nIndex].m_abilityId.m_res.GetResRef(partyCreature.m_quickSongsSpellId[nIndex]);
    }

    for (nIndex = 0; nIndex < 9; nIndex++) {
        partyCreature.field_19A[nIndex] = field_3D14[nIndex];
    }

    partyCreature.m_nLastSpellbookClassIndex = m_nLastSpellbookClassIndex;
    partyCreature.m_nLastSpellbookSpellLevel = m_nLastSpellbookSpellLevel;
    partyCreature.m_nLastSong = m_nLastSong;
    partyCreature.m_strStrongestKillName = m_cGameStats.m_strStrongestKillName;
    partyCreature.m_nStrongestKillXPValue = m_cGameStats.m_nStrongestKillXPValue;
    partyCreature.m_nPreviousTimeWithParty = m_cGameStats.m_nPreviousTimeWithParty;
    partyCreature.m_nJoinPartyTime = m_cGameStats.m_nJoinPartyTime;
    partyCreature.m_bWithParty = m_cGameStats.m_bWithParty;
    partyCreature.m_nChapterKillsXPValue = m_cGameStats.m_nChapterKillsXPValue;
    partyCreature.m_nChapterKillsNumber = m_cGameStats.m_nChapterKillsNumber;
    partyCreature.m_nGameKillsXPValue = m_cGameStats.m_nGameKillsXPValue;
    partyCreature.m_nGameKillsNumber = m_cGameStats.m_nGameKillsNumber;

    for (nIndex = 0; nIndex < 4; nIndex++) {
        m_cGameStats.m_pSpellStats[nIndex].m_cResRef.GetResRef(partyCreature.m_lSpellStatsName[nIndex]);
        partyCreature.m_lSpellStatsCount[nIndex] = m_cGameStats.m_pSpellStats[nIndex].m_nTimesUsed;
    }

    for (nIndex = 0; nIndex < 4; nIndex++) {
        m_cGameStats.m_pWeaponStats[nIndex].m_cResRef.GetResRef(partyCreature.m_lWeaponStatsName[nIndex]);
        partyCreature.m_lWeaponStatsCount[nIndex] = m_cGameStats.m_pWeaponStats[nIndex].m_nTimesUsed;
    }

    partyCreature.m_nHappiness = m_nHappiness;

    for (nIndex = 0; nIndex < 24; nIndex++) {
        partyCreature.m_nNumberOfTimesInteractedWith[nIndex] = m_nNumberOfTimesInteractedWith[nIndex];
    }

    if (m_baseStats.m_name == -1) {
        strncpy(partyCreature.m_name, (LPCSTR)m_sName, SCRIPTNAME_SIZE);
    } else {
        partyCreature.m_name[0] = '\0';
    }

    partyCreature.m_nNumberOfTimesTalkedTo = m_nNumberOfTimesTalkedTo;
    m_secondarySounds.GetResRef(partyCreature.m_secondarySounds);
    memcpy(partyCreature.field_25E, field_725A, 32);
}

// 0x70C3F0
void CGameSprite::MarshalMessage(BYTE** pData, DWORD* dwSize)
{
    CResRef spriteResRef;
    CString sSpriteString;
    BYTE nSpriteStringLength;
    CResRef areaResRef;
    CString sAreaString;
    BYTE nAreaStringLength;
    CPoint pos;
    BYTE enemyAlly;
    DWORD cnt;

    spriteResRef = m_resRef;
    spriteResRef.CopyToString(sSpriteString);
    nSpriteStringLength = static_cast<BYTE>(sSpriteString.GetLength());

    areaResRef = m_pArea->m_resRef;
    areaResRef.CopyToString(sAreaString);
    nAreaStringLength = static_cast<BYTE>(sAreaString.GetLength());

    pos = GetPos();
    enemyAlly = m_startTypeAI.GetEnemyAlly();

    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 12428
    UTIL_ASSERT(nAreaStringLength != 0);

    *dwSize = sizeof(BYTE)
        + nSpriteStringLength
        + sizeof(BYTE)
        + nAreaStringLength
        + sizeof(LONG)
        + sizeof(LONG)
        + sizeof(BYTE);

    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 12440
    UTIL_ASSERT(*dwSize <= STATICBUFFERSIZE_CGAMESPRITE);

    cnt = 0;

    *reinterpret_cast<BYTE*>(*pData + cnt) = nSpriteStringLength;
    cnt += sizeof(BYTE);

    memcpy(*pData + cnt, sSpriteString.GetBuffer(nSpriteStringLength), nSpriteStringLength);
    cnt += nSpriteStringLength;

    *reinterpret_cast<BYTE*>(*pData + cnt) = nAreaStringLength;
    cnt += sizeof(BYTE);

    memcpy(*pData + cnt, sSpriteString.GetBuffer(nAreaStringLength), nAreaStringLength);
    cnt += nAreaStringLength;

    *reinterpret_cast<LONG*>(*pData + cnt) = pos.x;
    cnt += sizeof(LONG);

    *reinterpret_cast<LONG*>(*pData + cnt) = pos.y;
    cnt += sizeof(LONG);

    *reinterpret_cast<BYTE*>(*pData + cnt) = enemyAlly;
    cnt += sizeof(BYTE);

    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 12481
    UTIL_ASSERT(cnt == *dwSize);
}

// 0x70C600
void CGameSprite::Unmarshal(CSavedGamePartyCreature* pCreature, BOOLEAN bPartyMember, BOOLEAN bProgressBarInPlace)
{
    DWORD nIndex;

    m_bInUnmarshal = TRUE;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    m_bGlobal = TRUE;
    m_currentArea = pCreature->m_areaName;

    // NOTE: Uninline.
    SetFacing(static_cast<SHORT>(pCreature->m_creatureFacing));

    m_nModalState = static_cast<BYTE>(pCreature->m_nModalState);

    for (nIndex = 0; nIndex < 5; nIndex++) {
        field_4C54[nIndex] = pCreature->field_28A[nIndex];
    }

    sub_726570();

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_QUICK_WEAPONS22; nIndex++) {
        InitQuickItemData(pCreature->m_quickWeaponsItemNum[nIndex],
            pCreature->m_quickWeaponsAbilityNum[nIndex],
            pCreature->field_2A7[nIndex],
            m_quickWeapons[nIndex]);

        // NOTE: Uninline.
        SetQuickWeapon(static_cast<BYTE>(nIndex), pCreature->field_2A7[nIndex]);
    }

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_QUICK_SPELLS22; nIndex++) {
        if (pCreature->m_quickSpellsClass[nIndex] != 0) {
            InitQuickSpellData(CResRef(pCreature->m_quickSpellsSpellId[nIndex]),
                1,
                m_quickSpells[nIndex],
                pCreature->m_quickSpellsClass[nIndex],
                pCreature->field_280[nIndex],
                pCreature->field_29E[nIndex]);
        }
    }

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_QUICK_ITEMS22; nIndex++) {
        InitQuickItemData(pCreature->m_quickItemsItemNum[nIndex],
            pCreature->m_quickItemsAbilityNum[nIndex],
            0,
            m_quickItems[nIndex]);
    }

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_QUICK_ABILITIES22; nIndex++) {
        InitQuickSpellData(CResRef(pCreature->m_quickInnatesSpellId[nIndex]),
            2,
            m_quickInnates[nIndex],
            0,
            0,
            0);
    }

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_QUICK_SONGS22; nIndex++) {
        InitQuickSpellData(CResRef(pCreature->m_quickSongsSpellId[nIndex]),
            3,
            m_quickSongs[nIndex],
            0,
            0,
            0);
    }

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_CUSTOM_BUTTONS22; nIndex++) {
        field_3D14[nIndex] = pCreature->field_19A[nIndex];
    }

    m_nLastSpellbookClassIndex = pCreature->m_nLastSpellbookClassIndex;
    m_nLastSpellbookSpellLevel = pCreature->m_nLastSpellbookSpellLevel;
    m_nLastSong = pCreature->m_nLastSong;

    m_nNumberOfTimesTalkedTo = pCreature->m_nNumberOfTimesTalkedTo;
    m_nHappiness = pCreature->m_nHappiness;

    for (nIndex = 0; nIndex < 24; nIndex++) {
        m_nNumberOfTimesInteractedWith[nIndex] = pCreature->m_nNumberOfTimesInteractedWith[nIndex];
    }

    if (pCreature->m_strStrongestKillName == 0) {
        pCreature->m_strStrongestKillName = -1;
    }

    m_cGameStats.m_strStrongestKillName = pCreature->m_strStrongestKillName;
    m_cGameStats.m_nStrongestKillXPValue = pCreature->m_nStrongestKillXPValue;
    m_cGameStats.m_nPreviousTimeWithParty = pCreature->m_nPreviousTimeWithParty;
    m_cGameStats.m_nJoinPartyTime = pCreature->m_nJoinPartyTime;
    m_cGameStats.m_bWithParty = pCreature->m_bWithParty;
    m_cGameStats.m_nChapterKillsXPValue = pCreature->m_nChapterKillsXPValue;
    m_cGameStats.m_nChapterKillsNumber = pCreature->m_nChapterKillsNumber;
    m_cGameStats.m_nGameKillsXPValue = pCreature->m_nGameKillsXPValue;
    m_cGameStats.m_nGameKillsNumber = pCreature->m_nGameKillsNumber;

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_STATS_SPELLS; nIndex++) {
        m_cGameStats.SetSpellStats(static_cast<BYTE>(nIndex),
            pCreature->m_lSpellStatsName[nIndex],
            pCreature->m_lSpellStatsCount[nIndex]);
    }

    for (nIndex = 0; nIndex < CGAMESAVECHARACTER_NUM_STATS_WEAPONS; nIndex++) {
        m_cGameStats.SetWeaponStats(static_cast<BYTE>(nIndex),
            pCreature->m_lWeaponStatsName[nIndex],
            pCreature->m_lWeaponStatsCount[nIndex]);
    }

    m_secondarySounds = pCreature->m_secondarySounds;
    memcpy(field_725A, pCreature->field_25E, sizeof(field_725A));

    CString sPath = g_pBaldurChitin->GetObjectGame()->GetDirSounds() + field_725A + '\\';
    g_pBaldurChitin->cDimm.AddToDirectoryList(sPath, TRUE);

    if (bPartyMember) {
        CString areaName;
        m_currentArea.CopyToString(areaName);

        CGameArea* pArea;
        if ((pCreature->m_wFlags & 0x8000) == 0) {
            pArea = pGame->LoadArea(areaName,
                255,
                FALSE,
                bProgressBarInPlace);
            if (pArea == NULL) {
                return;
            }

            pArea->GetInfinity()->SetViewPosition(pCreature->m_posViewX,
                pCreature->m_posViewY,
                TRUE);
        } else {
            if (bProgressBarInPlace) {
                g_pChitin->cProgressBar.AddActionTarget(-static_cast<LONG>(CInfGame::PROGRESSBAR_CACHING_ADDITIONAL));
            }
        }

        if (pGame->m_bInLoadGame == TRUE) {
            pGame->AddCharacterToParty(m_id, pCreature->m_portraitId);
        }

        CPoint pos;
        pos.x = pCreature->m_posX != -1 ? pCreature->m_posX : -1;
        pos.y = pCreature->m_posY != -1 ? pCreature->m_posY : -1;

        if ((pCreature->m_wFlags & 0x8000) == 0) {
            if ((m_baseStats.m_generalState & STATE_DEAD) != 0
                && GetAnimation()->CanLieDown()) {
                AddToArea(pArea, pos, 0, LIST_BACK);
            } else {
                AddToArea(pArea, pos, 0, LIST_FRONT);
            }

            if ((pCreature->m_wFlags & 0x1) != 0) {
                pGame->SelectCharacter(m_id, FALSE);
            }
        }

        if (m_baseStats.m_name == -1) {
            m_sName = pCreature->m_name;
        }

        if (m_baseStats.m_resistMagicBase < 0) {
            m_baseStats.m_resistMagicBase = 0;
        } else if (m_baseStats.m_resistMagicBase > 50) {
            m_baseStats.m_resistMagicBase = 50;
        }

        sub_71E760(m_derivedStats, TRUE);

        for (UINT nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
            for (UINT nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
                BYTE nClass = g_pBaldurChitin->GetObjectGame()->GetSpellcasterClass(nClassIndex);

                INT nBonus;
                INT nMaxSpells = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetMaxKnownSpells(nClass,
                    m_startTypeAI,
                    m_derivedStats,
                    m_baseStats.m_specialization,
                    nLevel + 1,
                    nBonus);

                // __FILE__: .\Include\ObjCreature.h
                // __LINE__: 1751
                UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

                m_spells.m_spellsByClass[nClassIndex].m_lists[nLevel].field_14 = nMaxSpells + nBonus;
            }
        }
    } else {
        m_pos.x = pCreature->m_posX;
        m_pos.y = pCreature->m_posY;
        if (m_baseStats.m_name == -1) {
            m_sName = pCreature->m_name;
        }
    }

    sub_726810(pCreature->m_nWeaponSet);

    INT nDruidLevel = m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_DRUID);
    if (nDruidLevel > 0) {
        INT nMaxShapeshifts = pGame->GetRuleTables().GetMaxDruidShapeshifts(m_baseStats, nDruidLevel) - m_shapeshifts.field_14;
        if (nMaxShapeshifts) {
            sub_724C40(nMaxShapeshifts);
            m_shapeshifts.sub_725D30(nMaxShapeshifts, FALSE);
        }
    }

    m_bInUnmarshal = FALSE;
}

// 0x70CF90
void CGameSprite::Unmarshal(BYTE* pCreature, LONG creatureSize, WORD facing, int a4)
{
    CCreatureFileOffsets* offsets;

    m_bInUnmarshal = TRUE;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 12738
    UTIL_ASSERT(pCreature != NULL);

    BOOL bNeedUpgrade = FALSE;
    BOOL bAlloced = FALSE;
    if (memcmp(pCreature, "CRE V2.2", 8) != 0) {
        bNeedUpgrade = TRUE;
    }

    if (bNeedUpgrade) {
        // TODO: Incomplete. The original upgrades older creature files in
        // place (0x70D013-0x70D1B6): a "CRE V2.0" or "CRE V9.2" file is
        // copied into a buffer 0xCA bytes larger (bAlloced) and converted to
        // V2.1 by 0x5A7740; a "CRE V2.1" file then grows by 0x240 plus what
        // 0x5A69B0 returns for it and is converted to V2.2 by 0x5A7940
        // (through 0x5A3DB0 and 0x5A5060). Those converters are InfGame.cpp
        // functions src/ does not define yet. Every CRE of the vanilla data
        // (1655 in CHITIN.KEY and Override) is V2.2, apart from four V9.1
        // files the original rejects as corrupt.
    }

    ClearMarshal(TRUE);
    creatureSize -= 900;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 12812
    UTIL_ASSERT(creatureSize > 0);

    memcpy(&m_baseStats, pCreature + 8, sizeof(m_baseStats));

    if (m_baseStats.m_scriptTeam[0] == 0xFF) {
        memcpy(m_baseStats.m_scriptTeam, "None", strlen("None") + 1);
    }

    if (m_baseStats.m_scriptSpecial1[0] == 0xFF) {
        memcpy(m_baseStats.m_scriptSpecial1, "None", strlen("None") + 1);
    }

    STR_RES nameRes;
    g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_name, nameRes);

    for (BYTE nRange = 0; nRange < 7; nRange++) {
        if (m_baseStats.m_colors[nRange] == 0xFF) {
            m_baseStats.m_colors[nRange] = 0;
        }
    }

    // NOTE: Uninline.
    GetAnimation()->SetAnimationType(m_baseStats.m_animationType, m_baseStats.m_colors, facing);

    if (GetAnimation()->GetListType() == LIST_FLIGHT) {
        // NOTE: Uninline. The facing is rounded to a multiple of 4 and
        // wrapped to 0..15 (0x70D2FE-0x70D322).
        SetFacing((4 * ((facing + 2) / 4)) % 16);
    }

    m_vbPortraitSmall.SetResRef(CResRef(m_baseStats.m_portraitSmall), TRUE, TRUE);
    m_vbPortraitSmall.m_bDoubleSize = FALSE;

    offsets = reinterpret_cast<CCreatureFileOffsets*>(pCreature + 900);
    creatureSize -= sizeof(CCreatureFileOffsets);

    if (m_baseStats.m_subrace == -1) {
        m_baseStats.m_subrace = 0;
    }

    if (offsets->m_class == 0 || offsets->m_class == -1) {
        offsets->m_class = CAIOBJECTTYPE_C_FIGHTER;
        offsets->m_avClass = CAIOBJECTTYPE_C_FIGHTER;
        offsets->m_classMask = CLASSMASK_FIGHTER;
    }

    m_derivedStats.m_classMask = 0;
    for (INT iClassType = 1; iClassType <= 11; iClassType++) {
        INT nLevel = GetClassLevel(iClassType);
        if (nLevel > 0) {
            m_derivedStats.SetClassLevel(iClassType, nLevel);
        }
    }

    SetAIType(CAIObjectType(offsets->m_enemyAlly,
                  offsets->m_general,
                  offsets->m_race,
                  m_baseStats.m_subrace,
                  offsets->m_class,
                  offsets->m_specifics,
                  offsets->m_gender,
                  offsets->m_alignment,
                  m_id,
                  offsets->m_specialCase,
                  CString(offsets->m_name),
                  offsets->m_avClass,
                  offsets->m_classMask),
        TRUE, TRUE);

    if ((m_baseStats.m_generalState & STATE_DEAD) != 0) {
        m_startTypeAI.m_nGeneral = CAIObjectType::G_DEAD;
        m_typeAI.m_nGeneral = CAIObjectType::G_DEAD;
    }

    strncpy(m_scriptName, offsets->m_name, SCRIPTNAME_SIZE);

    if (m_baseStats.m_attackBase == 0) {
        int v1;
        int v2;
        int v3;
        g_pBaldurChitin->GetObjectGame()->GetRuleTables().sub_546B60(this,
            v1,
            v2,
            v3,
            FALSE);
        m_baseStats.m_attackBase = v1;
    }

    if (m_baseStats.m_numberOfAttacksBase == 0) {
        int v1;
        int v2;
        int v3;
        g_pBaldurChitin->GetObjectGame()->GetRuleTables().sub_546B60(this,
            v1,
            v2,
            v3,
            FALSE);
        m_baseStats.m_numberOfAttacksBase = v2;
    }

    m_typeAI.m_nSubRace = m_baseStats.m_subrace;
    m_startTypeAI.m_nSubRace = m_baseStats.m_subrace;
    m_liveTypeAI.m_nSubRace = m_baseStats.m_subrace;

    if (m_baseStats.m_effectVersion == 0) {
        m_timedEffectList.Unmarshal(pCreature + offsets->m_effectListOffset,
            sizeof(ITEM_EFFECT) * offsets->m_effectListCount,
            this,
            0);
        creatureSize -= sizeof(ITEM_EFFECT) * offsets->m_effectListCount;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 12914
        UTIL_ASSERT(creatureSize > 0);
    } else {
        m_timedEffectList.Unmarshal(pCreature + offsets->m_effectListOffset,
            sizeof(CGameEffectBase) * offsets->m_effectListCount,
            this,
            1);
        creatureSize -= sizeof(CGameEffectBase) * offsets->m_effectListCount;

        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 12920
        UTIL_ASSERT(creatureSize > 0);
    }

    m_equipment.Unmarshal(reinterpret_cast<CCreatureFileEquipment*>(pCreature + offsets->m_equipmentListOffset),
        reinterpret_cast<CCreatureFileItem*>(pCreature + offsets->m_itemListOffset),
        offsets->m_itemListCount,
        this);
    creatureSize -= sizeof(CCreatureFileEquipment) + sizeof(CCreatureFileItem) * offsets->m_itemListCount;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 12925
    UTIL_ASSERT(creatureSize > 0);

    // NOTE: The original re-equips the weapon set here for creatures that
    // are not PCs (`test al, al; jne` past the call at 0x70D6E2); a PC's set
    // is re-equipped by the party creature's Unmarshal (0x70C600).
    if (!IcewindMisc::IsPC(this)) {
        if (m_equipment.m_selectedWeapon >= 43) {
            m_nWeaponSet = (m_equipment.m_selectedWeapon - 43) / 2;
        }
        sub_726810(m_nWeaponSet);
    }

    for (INT buttonNum = 0; buttonNum < g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetNumQuickWeaponSlots(m_typeAI.m_nClass); buttonNum++) {
        INT abilityNum = buttonNum + 43 == m_equipment.m_selectedWeapon
            ? m_equipment.m_selectedWeaponAbility
            : 0;

        CGameButtonList* buttons = GetItemUsages(buttonNum + 43, TRUE, abilityNum);
        if (!buttons->IsEmpty()) {
            // NOTE: Uninline.
            SetQuickWeapon(static_cast<BYTE>(buttonNum), *buttons->GetHead());

            while (!buttons->IsEmpty()) {
                delete buttons->RemoveHead();
            }
        }
        delete buttons;
    }

    CCreatureFileSpell* pSpell = new CCreatureFileSpell();
    DWORD nOffset;
    DWORD nIndex;
    UINT nClass;
    UINT nLevel;

    for (nClass = 0; nClass < CSPELLLIST_NUM_CLASSES; nClass++) {
        for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
            nOffset = offsets->m_spellListOffset[nClass][nLevel];
            for (nIndex = 0; nIndex < offsets->m_spellListCount[nClass][nLevel]; nIndex++) {
                *pSpell = *reinterpret_cast<CCreatureFileSpell*>(pCreature + nOffset);
                nOffset += sizeof(CCreatureFileSpell);

                const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(pSpell->field_0);
                if (memcmp(resRef.GetResRef(), "**", strlen("**")) == 0) {
                    CString sError;
                    sError.Format("*** %s has OBSOLETE SPELL FILE: %s.\n",
                        m_scriptName,
                        (LPCSTR)resRef.GetResRefStr());
                } else {
                    // NOTE: Uninline.
                    m_spells.Get(nClass)->Add(pSpell->field_0,
                        nLevel,
                        pSpell->field_4,
                        pSpell->field_8,
                        pSpell->field_C);
                }
            }
            creatureSize -= sizeof(CCreatureFileSpell) * offsets->m_spellListCount[nClass][nLevel];

            // NOTE: There are some inlining.

            m_spells.m_spellsByClass[nClass].m_lists[nLevel].field_14 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
            nOffset += sizeof(unsigned int);
            creatureSize -= sizeof(unsigned int);

            m_spells.m_spellsByClass[nClass].m_lists[nLevel].field_18 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
            nOffset += sizeof(unsigned int);
            creatureSize -= sizeof(unsigned int);
        }
    }

    for (nLevel = 0; nLevel < CSPELLLIST_MAX_LEVELS; nLevel++) {
        nOffset = offsets->m_domainListOffset[nLevel];
        for (nIndex = 0; nIndex < offsets->m_domainListCount[nLevel]; nIndex++) {
            *pSpell = *reinterpret_cast<CCreatureFileSpell*>(pCreature + nOffset);
            nOffset += sizeof(CCreatureFileSpell);

            const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(pSpell->field_0);
            if (memcmp(resRef.GetResRef(), "**", strlen("**")) == 0) {
                CString sError;
                sError.Format("*** %s has OBSOLETE DOMAIN SPELL FILE: %s.\n",
                    m_scriptName,
                    (LPCSTR)resRef.GetResRefStr());
            } else {
                // NOTE: Uninline.
                m_domainSpells.Add(pSpell->field_0,
                    nLevel,
                    pSpell->field_4,
                    pSpell->field_8,
                    pSpell->field_C);
            }
        }
        creatureSize -= sizeof(CCreatureFileSpell) * offsets->m_domainListCount[nLevel];

        m_domainSpells.m_lists[nLevel].field_14 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
        nOffset += sizeof(unsigned int);
        creatureSize -= sizeof(unsigned int);

        m_domainSpells.m_lists[nLevel].field_18 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
        nOffset += sizeof(unsigned int);
        creatureSize -= sizeof(unsigned int);
    }

    nOffset = offsets->m_innateListOffset;
    for (nIndex = 0; nIndex < offsets->m_innateListCount; nIndex++) {
        *pSpell = *reinterpret_cast<CCreatureFileSpell*>(pCreature + nOffset);
        nOffset += sizeof(CCreatureFileSpell);

        const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->GetInnateSpells()->Get(pSpell->field_0);
        if (memcmp(resRef.GetResRef(), "**", strlen("**")) == 0) {
            CString sError;
            sError.Format("*** %s has OBSOLETE INNATE SPELL FILE: %s.\n",
                m_scriptName,
                (LPCSTR)resRef.GetResRefStr());
        } else {
            m_innateSpells.Add(pSpell->field_0,
                pSpell->field_4,
                pSpell->field_8,
                pSpell->field_C);
        }
    }
    creatureSize -= sizeof(CCreatureFileSpell) * offsets->m_innateListCount;

    m_innateSpells.field_14 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
    nOffset += sizeof(unsigned int);
    creatureSize -= sizeof(unsigned int);

    m_innateSpells.field_18 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
    nOffset += sizeof(unsigned int);
    creatureSize -= sizeof(unsigned int);

    nOffset = offsets->m_songListOffset;
    for (nIndex = 0; nIndex < offsets->m_songListCount; nIndex++) {
        *pSpell = *reinterpret_cast<CCreatureFileSpell*>(pCreature + nOffset);
        nOffset += sizeof(CCreatureFileSpell);

        const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->GetMasterSongLookup().Get(pSpell->field_0);
        if (memcmp(resRef.GetResRef(), "**", strlen("**")) == 0) {
            CString sError;
            sError.Format("*** %s has OBSOLETE SONG FILE: %s.\n",
                m_scriptName,
                (LPCSTR)resRef.GetResRefStr());
        } else {
            m_songs.Add(pSpell->field_0,
                pSpell->field_4,
                pSpell->field_8,
                pSpell->field_C);
        }
    }
    creatureSize -= sizeof(CCreatureFileSpell) * offsets->m_songListCount;

    m_songs.field_14 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
    nOffset += sizeof(unsigned int);
    creatureSize -= sizeof(unsigned int);

    m_songs.field_18 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
    nOffset += sizeof(unsigned int);
    creatureSize -= sizeof(unsigned int);

    nOffset = offsets->m_shapeListOffset;
    for (nIndex = 0; nIndex < offsets->m_shapeListCount; nIndex++) {
        *pSpell = *reinterpret_cast<CCreatureFileSpell*>(pCreature + nOffset);
        nOffset += sizeof(CCreatureFileSpell);

        const CResRef& resRef = g_pBaldurChitin->GetObjectGame()->GetShapeshifts()->Get(pSpell->field_0);
        if (memcmp(resRef.GetResRef(), "**", strlen("**")) == 0) {
            CString sError;
            sError.Format("*** %s has OBSOLETE SHAPESHIFT FILE: %s.\n",
                m_scriptName,
                (LPCSTR)resRef.GetResRefStr());
        } else {
            m_shapeshifts.Add(pSpell->field_0,
                pSpell->field_4,
                pSpell->field_8,
                pSpell->field_C);
        }
    }
    creatureSize -= sizeof(CCreatureFileSpell) * offsets->m_shapeListCount;

    m_shapeshifts.field_14 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
    nOffset += sizeof(unsigned int);
    creatureSize -= sizeof(unsigned int);

    m_shapeshifts.field_18 = *reinterpret_cast<unsigned int*>(pCreature + nOffset);
    nOffset += sizeof(unsigned int);
    creatureSize -= sizeof(unsigned int);

    delete pSpell;

    for (BYTE nHatedRaceIndex = 0; nHatedRaceIndex < 8; nHatedRaceIndex++) {
        if (m_baseStats.m_favoredEnemies[nHatedRaceIndex] == 0) {
            m_baseStats.m_favoredEnemies[nHatedRaceIndex] = CAIObjectType::R_NO_RACE;
        }
    }

    m_derivedStats.Reload(this, &m_baseStats, &m_spells, &m_domainSpells);
    m_bInUnmarshal = FALSE;

    if (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nNightmareMode == TRUE
        && (m_baseStats.field_2FB & 0x1) == 0
        && a4 == NULL) {
        BOOL bIncreaseStats = FALSE;
        if (m_type == 2) {
            m_baseStats.m_hitPoints = 2 * (m_baseStats.m_hitPoints + 10);
            m_baseStats.m_maxHitPointsBase = 2 * (m_baseStats.m_maxHitPointsBase + 10);
            bIncreaseStats = TRUE;
        } else {
            if (offsets->m_enemyAlly > CAIObjectType::EA_GOODCUTOFF) {
                m_baseStats.m_hitPoints = 3 * (m_baseStats.m_hitPoints + 80);
                m_baseStats.m_maxHitPointsBase = 3 * (m_baseStats.m_maxHitPointsBase + 80);
                bIncreaseStats = TRUE;
            }
        }

        if (bIncreaseStats) {
            // NOTE: Uninline.
            BYTE moveScale = GetAnimation()->GetMoveScale();

            // NOTE: Uninline.
            GetAnimation()->SetMoveScale(static_cast<BYTE>(static_cast<float>(moveScale) * 1.3f + 1.0f));

            if (m_baseStats.m_gold != 0) {
                m_baseStats.m_gold += 75;
            }

            for (INT iClassType = 1; iClassType <= 11; iClassType++) {
                INT nLevel = GetClassLevel(iClassType);
                if (nLevel > 0) {
                    SetClassLevel(iClassType, min(nLevel + 12, 30));
                }
            }

            m_baseStats.m_STRBase += 10;
            m_baseStats.m_INTBase += 10;
            m_baseStats.m_WISBase += 10;
            m_baseStats.m_DEXBase += 10;
            m_baseStats.m_CONBase += 10;
            m_baseStats.m_CHRBase += 10;
            m_baseStats.field_252 += 10;
            m_baseStats.field_2FB |= 0x1;
        }
    }

    m_baseStats.field_2FC &= ~0x1;
    m_dialog = offsets->m_dialog;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 13227
    UTIL_ASSERT(creatureSize == 0);

    m_derivedStats.Reload(this, &m_baseStats, &m_spells, &m_domainSpells);
    m_tempStats = m_derivedStats;
    m_bonusStats.BonusInit();

    if (bAlloced) {
        delete pCreature;
    }
}

// 0x70E750
void CGameSprite::UnmarshalScripts()
{
    CString sFileName;

    sFileName = m_baseStats.m_scriptOverRide;
    if (sFileName != "") {
        m_overrideScript = new CAIScript(CResRef(sFileName));
        if (m_overrideScript->IsEmpty()) {
            delete m_overrideScript;
            m_overrideScript = NULL;
        }
    }

    sFileName = m_baseStats.m_scriptSpecial1;
    if (sFileName != "") {
        m_special1Script = new CAIScript(CResRef(sFileName));
        if (m_special1Script->IsEmpty()) {
            delete m_special1Script;
            m_special1Script = NULL;
        }
    }

    sFileName = m_baseStats.m_scriptTeam;
    if (sFileName != "") {
        m_teamScript = new CAIScript(CResRef(sFileName));
        if (m_teamScript->IsEmpty()) {
            delete m_teamScript;
            m_teamScript = NULL;
        }
    }

    sFileName = m_baseStats.m_scriptSpecial2;
    if (sFileName != "") {
        m_special2Script = new CAIScript;
        m_special2Script->Read(CResRef(sFileName), g_pBaldurChitin->GetObjectGame()->m_bPlayerScriptStyle);
        if (m_special2Script->IsEmpty()) {
            delete m_special2Script;
            m_special2Script = NULL;
        }
    }

    sFileName = m_baseStats.m_scriptCombat;
    if (sFileName != "") {
        m_combatScript = new CAIScript(CResRef(sFileName));
        if (m_combatScript->IsEmpty()) {
            delete m_combatScript;
            m_combatScript = NULL;
        }
    }

    sFileName = m_baseStats.m_scriptSpecial3;
    if (sFileName != "") {
        m_special3Script = new CAIScript(CResRef(sFileName));
        if (m_special3Script->IsEmpty()) {
            delete m_special3Script;
            m_special3Script = NULL;
        }
    }

    sFileName = m_baseStats.m_scriptMovement;
    if (sFileName != "") {
        m_movementScript = new CAIScript(CResRef(sFileName));
        if (m_movementScript->IsEmpty()) {
            delete m_movementScript;
            m_movementScript = NULL;
        }
    }
}

// 0x70EC00
void CGameSprite::FetchCommonStrings()
{
    int index;

    for (index = 0; index < 3; index++) {
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[33 + index],
            m_speech[33 + index]);
    }

    for (index = 0; index < 7; index++) {
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_speech[15 + index],
            m_speech[15 + index]);
    }
}

// 0x70EC70
void CGameSprite::LoadAreaInformation(CAreaFileCreature* pCreature)
{
    m_nNumberOfTimesTalkedTo = pCreature->m_numberTimesTalkedTo;

    CString tempRes;

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_dialogOverride), RESREF_SIZE);
    if (tempRes != "") {
        m_dialog = tempRes;
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_overrideScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptOverRide);
        SetScript(0, pScript);
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_special3ScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptSpecial3);
        SetScript(5, pScript);
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_special2ScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptSpecial2);
        SetScript(3, pScript);
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_combatScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptCombat);
        SetScript(4, pScript);
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_movementScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptMovement);
        SetScript(6, pScript);
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_teamScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptTeam);
        SetScript(2, pScript);
    }

    tempRes = CString(reinterpret_cast<char*>(pCreature->m_special1ScriptOverride), RESREF_SIZE);
    tempRes.TrimLeft();
    if (tempRes != "") {
        CAIScript* pScript = new CAIScript(CResRef(tempRes));

        CResRef(tempRes).GetResRef(m_baseStats.m_scriptSpecial1);
        SetScript(1, pScript);
    }

    memcpy(m_scriptName, pCreature->m_scriptName, SCRIPTNAME_SIZE);
}

// 0x70F270
BYTE CGameSprite::GetChannel()
{
    SHORT nPortraitNum = g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id);
    if (nPortraitNum != -1) {
        return nPortraitNum + 7;
    }
    return 13;
}

// 0x70F2A0
void CGameSprite::RenderMirrorImage(INT placement, CRect& rFX, CRect& rGCBounds, CRect& rViewRect, CPoint& ptReference, CSearchBitmap* pSearch, CVisibilityMap* pVisibility, CVidMode* pVidMode, INT nSurface, COLORREF& rgbTint, BOOLEAN& bDithered, BOOLEAN& bFadeOut, DWORD& dwRenderFlags)
{
    // 0x8B7308
    static int mirrorImagePlacementX[] = {
        -25,
        25,
        0,
        0,
        -18,
        18,
        18,
        -18,
    };

    // 0x8B7328
    static int mirrorImagePlacementY[] = {
        0,
        0,
        -25,
        25,
        -18,
        18,
        -18,
        18,
    };

    CPoint mirrorPos;
    CPoint mirrorSquare;
    SHORT searchSquareCode;
    CPoint pos;

    pos.x = m_pos.x + mirrorImagePlacementX[placement];
    pos.y = m_pos.y += mirrorImagePlacementY[placement];
    pos.y += m_pArea->GetHeightOffset(pos, m_listType);

    m_animation.CalculateGCBoundsRect(rGCBounds,
        pos,
        ptReference,
        m_posZ,
        rFX.Width(),
        rFX.Height());

    if (!IsRectEmpty(rViewRect & rGCBounds)) {
        mirrorPos.x = max(pos.x, m_pArea->GetInfinity()->nAreaX - 1) / CPathSearch::GRID_SQUARE_SIZEX;
        mirrorPos.y = max(pos.y, m_pArea->GetInfinity()->nAreaY - 1) / CPathSearch::GRID_SQUARE_SIZEY;
        if ((pSearch->GetLOSCost(mirrorPos, m_terrainTable, searchSquareCode, FALSE) != CPathSearch::COST_IMPASSABLE
                || searchSquareCode == 14)
            && pVisibility->IsTileExplored(pVisibility->PointToTile(mirrorPos))) {
            m_animation.Render(m_pArea->GetInfinity(),
                pVidMode,
                nSurface,
                rFX,
                mirrorPos,
                ptReference,
                dwRenderFlags | 0x2,
                rgbTint,
                rGCBounds,
                bDithered,
                bFadeOut,
                m_posZ,
                96);
        }
    }
}

// 0x713FE0
void CGameSprite::GetSelectedWeaponButton(CButtonData& cButtonData)
{
    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    cButtonData.m_icon = "NOICON";
    cButtonData.m_count = 0;
    cButtonData.m_abilityId.m_itemType = 2;
    cButtonData.m_abilityId.m_itemNum = 10;
    cButtonData.m_abilityId.m_abilityNum = 1;
    cButtonData.m_abilityId.field_10 = -1;

    CItem* pItem = m_equipment.m_items[m_nTempSelectedWeapon];
    if (pItem != NULL) {
        pItem->Demand();
        ITEM_ABILITY* pAbility = pItem->GetAbility(m_nTempSelectedWeaponAbility);
        if (pAbility != NULL) {
            cButtonData.m_icon = CString(pAbility->quickSlotIcon);
            cButtonData.m_abilityId.m_itemNum = m_nTempSelectedWeapon;
            cButtonData.m_abilityId.m_itemType = 2;
            cButtonData.m_abilityId.m_abilityNum = m_nTempSelectedWeaponAbility;
            cButtonData.m_abilityId.field_10 = rule.GetItemAbilityDescription(pItem->cResRef,
                m_nTempSelectedWeaponAbility);
            if (cButtonData.m_abilityId.field_10 == -1) {
                cButtonData.m_abilityId.field_10 = pItem->GetGenericName();
            }

            cButtonData.m_count = 0;
            if (pItem->GetMaxStackable() > 1) {
                cButtonData.m_count = pItem->GetUsageCount(m_nTempSelectedWeaponAbility);
            }
        }
        pItem->Release();
    }
}

// 0x714130
void CGameSprite::GetWeaponButton(BYTE nButtonNum, CButtonData& cButtonData)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 15766
    UTIL_ASSERT_MSG(nButtonNum < CCREATUREFILEEQUIPMENT21_WEAPON_NUM, "Invalid button number.");

    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    cButtonData.m_icon = "";
    cButtonData.m_count = 0;
    cButtonData.m_abilityId.m_itemType = 2;
    cButtonData.m_abilityId.m_itemNum = 10;
    cButtonData.m_abilityId.m_abilityNum = 1;
    cButtonData.m_abilityId.field_10 = -1;

    CItem* pItem = m_equipment.m_items[nButtonNum + 43];
    if (pItem != NULL) {
        pItem->Demand();
        ITEM_ABILITY* pAbility = pItem->GetAbility(field_3D3A[nButtonNum]);
        if (pAbility != NULL) {
            cButtonData.m_icon = CString(pAbility->quickSlotIcon);
            cButtonData.m_abilityId.m_itemType = 2;
            cButtonData.m_abilityId.m_itemNum = nButtonNum + 43;
            cButtonData.m_abilityId.m_abilityNum = field_3D3A[nButtonNum];
            cButtonData.m_abilityId.field_10 = rule.GetItemAbilityDescription(pItem->cResRef,
                field_3D3A[nButtonNum]);
            if (cButtonData.m_abilityId.field_10 == -1) {
                cButtonData.m_abilityId.field_10 = pItem->GetGenericName();
            }

            cButtonData.m_count = 0;
            if (pItem->GetMaxStackable() > 1) {
                cButtonData.m_count = pItem->GetUsageCount(field_3D3A[nButtonNum]);
            }
        }
        pItem->Release();
    }
}

// 0x7142D0
void CGameSprite::SetSelectedWeaponButton(SHORT buttonNum)
{
    if (m_equipment.m_items[42] != NULL) {
        g_pBaldurChitin->GetObjectGame()->SetState(2);
        g_pBaldurChitin->GetObjectGame()->SetIconIndex(12);
        g_pBaldurChitin->GetObjectGame()->m_iconResRef = "";
        g_pBaldurChitin->GetObjectGame()->field_38A6 = 1;
    } else {
        g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

        SHORT itemNum = m_quickWeapons[buttonNum].m_abilityId.m_itemNum;
        SHORT abilityNum = m_quickWeapons[buttonNum].m_abilityId.m_abilityNum;
        if (itemNum != -1 && abilityNum != -1) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE_: 15877
            m_nTempSelectedWeapon = static_cast<BYTE>(itemNum);
            m_nTempSelectedWeaponAbility = static_cast<BYTE>(abilityNum);
            SelectWeaponAbility(static_cast<BYTE>(itemNum),
                static_cast<BYTE>(abilityNum),
                0,
                1);
            m_interrupt = TRUE;
            if (m_equipment.m_selectedWeapon == itemNum) {
                g_pBaldurChitin->GetObjectGame()->SetState(2);
                g_pBaldurChitin->GetObjectGame()->SetIconIndex(12);
                g_pBaldurChitin->GetObjectGame()->m_iconResRef = "";
            }
            g_pBaldurChitin->GetObjectGame()->field_38A6 = 1;
        }
    }
}

// 0x714410
void CGameSprite::InitQuickSpellData(CResRef resRef, BYTE type, CButtonData& cButtonData, BYTE nClass, BYTE a5, BYTE nKitIndex)
{
    CSpell cSpell;
    cSpell.SetResRef(resRef, TRUE, TRUE);
    BYTE count = 0;

    if (!resRef.IsValid()) {
        return;
    }

    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->m_shapeshifts.Find(resRef, nID) == TRUE) {
        type = 4;
    }

    UINT v1 = 0;
    UINT v2 = 0;
    UINT nClassIndex;

    switch (type) {
    case 1:
        if (!g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, v1)) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 15953
            UTIL_ASSERT(FALSE);
        }

        nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);

        if (nKitIndex != 0) {
            if (nClass == CAIOBJECTTYPE_C_CLERIC) {
                BOOLEAN bFound = FALSE;
                for (UINT nLevel = 0; nLevel < m_domainSpells.m_nHighestLevel; nLevel++) {
                    if (!bFound) {
                        break;
                    }

                    if (m_domainSpells.Find(v1, nLevel, v2) == TRUE) {
                        bFound = TRUE;
                        count += m_domainSpells.m_lists[nLevel].Get(v2)->field_8;
                    }
                }
            }
        } else {
            BOOLEAN bFound = FALSE;
            for (UINT nLevel = 0; nLevel < m_spells.m_spellsByClass[nClass].m_nHighestLevel; nLevel++) {
                if (!bFound) {
                    break;
                }

                if (m_spells.m_spellsByClass[nClass].Find(v1, nLevel, v2) == TRUE) {
                    bFound = TRUE;
                    count += m_spells.m_spellsByClass[nClass].m_lists[nLevel].Get(v2)->field_8;
                }
            }
        }
        break;
    case 2:
        if (!g_pBaldurChitin->GetObjectGame()->m_innateSpells.Find(resRef, v1)) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 15994
            UTIL_ASSERT(FALSE);
        }

        if (m_innateSpells.Find(v1, v2) == TRUE) {
            count += m_innateSpells.Get(v2)->field_8;
        }
        break;
    case 3:
        if (!g_pBaldurChitin->GetObjectGame()->m_songs.Find(resRef, v1)) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 16012
            UTIL_ASSERT(FALSE);
        }

        if (m_songs.Find(v1, v2) == TRUE) {
            count += m_songs.Get(v2)->field_8;
        }
        break;
    case 4:
        if (!g_pBaldurChitin->GetObjectGame()->m_shapeshifts.Find(resRef, v1)) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 16030
            UTIL_ASSERT(FALSE);
        }

        if (m_shapeshifts.Find(v1, v2) == TRUE) {
            count += m_shapeshifts.Get(v2)->field_8;
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 16044
        UTIL_ASSERT(FALSE);
    }

    DWORD nSpecialization = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetSpecializationMask(nClass, nKitIndex);
    cSpell.Demand();

    SHORT nCasterLevel = GetCasterLevel(&cSpell, nClass, nSpecialization);
    if (nCasterLevel <= 1) {
        nCasterLevel = 1;
    }

    SPELL_ABILITY* pBestAbility = NULL;
    for (INT abilityNum = 0; abilityNum < cSpell.GetAbilityCount(); abilityNum++) {
        if (cSpell.GetAbility(abilityNum)->minCasterLevel > nCasterLevel) {
            break;
        }

        // FIXME: Calls `GetAbility` one more time.
        pBestAbility = cSpell.GetAbility(abilityNum);
    }

    if (pBestAbility != NULL) {
        cButtonData.m_icon = CString(pBestAbility->quickSlotIcon);
        cButtonData.m_name = cSpell.GetGenericName();
        cButtonData.m_abilityId.m_itemType = 1;
        cButtonData.m_abilityId.m_targetType = pBestAbility->actionType;
        cButtonData.m_abilityId.field_1D = a5;
        cButtonData.m_abilityId.m_nClass = nClass;
        cButtonData.m_abilityId.field_1E = nKitIndex;
        cButtonData.m_abilityId.field_10 = cSpell.GetGenericName();

        if (type == 1) {
            cButtonData.m_abilityId.field_18 = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetClassSuffixStringRef(nClass);
            if (!CanCast(nClass, 0, &cSpell)) {
                cButtonData.m_bDisabled = TRUE;
                cButtonData.m_abilityId.field_18 = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetClassBeyondCastingAbilityStringRef(nClass);
            }
        }

        cButtonData.m_count = count;

        if (count == 0) {
            cButtonData.m_bDisabled = TRUE;
        }

        if (type == 2) {
            if (resRef == CGameSprite::SPIN277) {
                cButtonData.m_bDisplayCount = FALSE;
                cButtonData.m_count = 0;

                if (!sub_763150(CGAMESPRITE_FEAT_ARTERIAL_STRIKE)) {
                    cButtonData.m_bDisabled = TRUE;
                }
            } else if (resRef == CGameSprite::SPIN278) {
                cButtonData.m_bDisplayCount = FALSE;
                cButtonData.m_count = 0;

                if (!sub_763150(CGAMESPRITE_FEAT_HAMSTRING)) {
                    cButtonData.m_bDisabled = TRUE;
                }
            } else if (resRef == CGameSprite::SPIN279) {
                cButtonData.m_bDisplayCount = FALSE;
                cButtonData.m_count = 0;

                if (!sub_763150(CGAMESPRITE_FEAT_RAPID_SHOT)) {
                    cButtonData.m_bDisabled = TRUE;
                }
            }
        }
    }

    cSpell.Release();
}

// 0x714DB0
void CGameSprite::InitQuickItemData(SHORT itemNum, SHORT abilityNum, int a3, CButtonData& cButtonData)
{
    const CRuleTables& cRule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    if (itemNum != -1 && abilityNum != -1) {
        CItem* pItem = m_equipment.m_items[itemNum];
        if (pItem != NULL) {
            pItem->Demand();

            ITEM_ABILITY* ability = pItem->GetAbility(abilityNum);
            if (ability != NULL) {
                if (ability->type != 4
                    && (ability->type != 2 || CheckLauncherType(ability, NULL))) {
                    cButtonData.m_icon = CString(ability->quickSlotIcon);
                    cButtonData.m_name = pItem->GetGenericName();
                    cButtonData.m_abilityId.m_itemNum = itemNum;
                    cButtonData.m_abilityId.m_abilityNum = abilityNum;
                    cButtonData.m_abilityId.m_targetType = ability->actionType;
                    cButtonData.m_abilityId.field_10 = cRule.GetItemAbilityDescription(pItem->GetResRef(), abilityNum);
                    if (cButtonData.m_abilityId.field_10 == -1) {
                        cButtonData.m_abilityId.field_10 = pItem->GetGenericName();
                    }
                    cButtonData.m_count = 0;
                    if (pItem->GetMaxStackable() > 1
                        || pItem->GetMaxUsageCount(abilityNum) > 0) {
                        cButtonData.m_count = pItem->GetUsageCount(abilityNum);
                    }

                    SHORT launcherSlot;
                    CItem* pLauncher = GetLauncher(ability, launcherSlot);
                    if (pLauncher != NULL) {
                        cButtonData.m_launcherIcon = pLauncher->GetItemIcon();
                        cButtonData.m_launcherName = pLauncher->GetGenericName();
                    }
                }
            }

            pItem->Release();
        }
    }
}

// 0x716540
CGameButtonList* CGameSprite::GetSongsButtonList()
{
    CGameButtonList* buttons = new CGameButtonList();
    RESREF resRef;

    for (size_t index = 0; index < m_songs.m_List.size(); index++) {
        // NOTE: Uninline.
        CGameSpriteSpellListEntry* entry = m_songs.Get(index);

        // NOTE: Uninline.
        g_pBaldurChitin->GetObjectGame()->GetMasterSongLookup().Get(entry->m_nID).GetResRef(resRef);

        buttons->AddTail(IcewindMisc::CreateButtonData(resRef));
    }

    return buttons;
}

// NOTE: This function correctly accepts `nClass` by value (as opposed by to
// many others which passes `nClass` as a reference for unknown reason).
//
// 0x7166D0
BOOLEAN CGameSprite::CanCast(BYTE nClass, DWORD nSpecialization, CSpell* pSpell)
{
    INT nValue = 0;
    switch (nClass) {
    case CAIOBJECTTYPE_C_BARBARIAN:
    case CAIOBJECTTYPE_C_FIGHTER:
    case CAIOBJECTTYPE_C_MONK:
    case CAIOBJECTTYPE_C_ROGUE:
        return FALSE;
    case CAIOBJECTTYPE_C_BARD:
    case CAIOBJECTTYPE_C_SORCERER:
        nValue = m_derivedStats.m_nCHR;
        break;
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
        nValue = m_derivedStats.m_nWIS;
        break;
    case CAIOBJECTTYPE_C_WIZARD:
        nValue = m_derivedStats.m_nINT;
        break;
    }

    INT nLevel = g_pBaldurChitin->GetObjectGame()->GetSpellLevel(pSpell->GetResRef(), nClass, nSpecialization);
    return nLevel < CSPELLLIST_MAX_LEVELS && nValue >= nLevel + 11;
}

// 0x717620
CGameButtonList* CGameSprite::GetInternalButtonList()
{
    CGameButtonList* buttons = new CGameButtonList();

    if (m_internalButtonList != NULL) {
        POSITION pos = m_internalButtonList->GetHeadPosition();
        while (pos != NULL) {
            CButtonData* node = m_internalButtonList->GetNext(pos);
            CButtonData* copy = new CButtonData();
            *copy = *node;
            buttons->AddTail(copy);
        }

        // FIXME: Meaninless memory leak.
        new CButtonData();
    }

    return buttons;
}

// 0x717850
CGameButtonList* CGameSprite::GetItemUsages(SHORT slotNum, WORD buttonType, SHORT abilityNum)
{
    const CRuleTables& cRule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    CGameButtonList* buttons = new CGameButtonList();

    // FIXME: Unused.
    STR_RES strRes;

    if (slotNum >= 51) {
        return buttons;
    }

    if (buttonType == 1 && m_equipment.m_items[42] != NULL) {
        if (slotNum == 43) {
            delete buttons;
            return GetItemUsages(42, 1, 0);
        }

        if (slotNum > 43) {
            return buttons;
        }
    }

    CItem* pItem = m_equipment.m_items[slotNum];
    if (pItem == NULL) {
        return buttons;
    }

    if (slotNum == 43
        || slotNum == 45
        || slotNum == 47
        || slotNum == 49) {
        if ((pItem->GetFlagsFile() & 0x2) != 0) {
            if (m_equipment.m_items[slotNum + 1] != NULL) {
                return buttons;
            }
        }
    }

    pItem->Demand();

    INT nStart = abilityNum;
    INT nEnd;
    if (abilityNum == -1) {
        nStart = 0;
        if (pItem->GetItemType() == 11) {
            nEnd = 1;
        } else {
            nEnd = pItem->GetAbilityCount();
        }
    } else {
        nEnd = abilityNum + 1;
    }

    for (INT nAbility = nStart; nAbility < nEnd; nAbility++) {
        const ITEM_ABILITY* curAbility = pItem->GetAbility(nAbility);
        if (curAbility == NULL) {
            continue;
        }

        if ((curAbility->type & 0x100) != 0 && (pItem->m_flags & 0x1) == 0) {
            continue;
        }

        if ((curAbility->type & 0x200) != 0 && (pItem->m_flags & 0x1) != 0) {
            continue;
        }

        if (curAbility->quickSlotType == buttonType && (curAbility->type & 0xFF) != 4) {
            if ((curAbility->type & 0xFF) != 2 || CheckLauncherType(curAbility, NULL)) {
                CButtonData* pButtonData = new CButtonData();
                pButtonData->m_icon = CString(curAbility->quickSlotIcon);
                pButtonData->m_name = pItem->GetGenericName();
                pButtonData->m_abilityId.m_itemType = 2;
                pButtonData->m_abilityId.m_itemNum = slotNum;
                pButtonData->m_abilityId.m_abilityNum = nAbility;
                pButtonData->m_abilityId.m_targetType = curAbility->actionType;
                pButtonData->m_abilityId.field_10 = cRule.GetItemAbilityDescription(pItem->GetResRef(), nAbility);
                if (pButtonData->m_abilityId.field_10 == -1) {
                    pButtonData->m_abilityId.field_10 = pItem->GetGenericName();
                }
                pButtonData->m_count = 0;
                if (pItem->GetMaxStackable() > 1 || pItem->GetMaxUsageCount(nAbility) > 0) {
                    pButtonData->m_count = pItem->GetUsageCount(nAbility);
                }

                SHORT launcherSlot;
                CItem* pLauncher = GetLauncher(curAbility, launcherSlot);
                if (pLauncher != NULL) {
                    pButtonData->m_launcherIcon = pLauncher->GetItemIcon();
                    pButtonData->m_launcherName = pLauncher->GetGenericName();
                }

                buttons->AddTail(pButtonData);
            }
        } else {
            if ((curAbility->type & 0xFF) == 4) {
                for (INT ammoSlotNum = 11; ammoSlotNum < 15; ammoSlotNum++) {
                    CItem* pAmmo = m_equipment.m_items[ammoSlotNum];
                    if (pAmmo == NULL) {
                        continue;
                    }

                    pAmmo->Demand();

                    for (INT nAmmoAbility = nStart; nAmmoAbility < nEnd; nAmmoAbility++) {
                        const ITEM_ABILITY* curAmmoAbility = pAmmo->GetAbility(nAmmoAbility);
                        if (curAmmoAbility != NULL
                            && curAmmoAbility->quickSlotType == buttonType
                            && (curAmmoAbility->type & 0xFF) != 4
                            && CheckLauncherType(curAmmoAbility, pItem)) {
                            CButtonData* pButtonData = new CButtonData();
                            pButtonData->m_icon = CString(curAmmoAbility->quickSlotIcon);
                            pButtonData->m_name = pAmmo->GetGenericName();
                            pButtonData->m_count = pAmmo->GetUsageCount(nAmmoAbility);
                            pButtonData->m_abilityId.m_itemType = 2;
                            pButtonData->m_abilityId.m_itemNum = ammoSlotNum;
                            pButtonData->m_abilityId.m_abilityNum = nAmmoAbility;
                            pButtonData->m_abilityId.m_targetType = curAmmoAbility->actionType;
                            if (curAmmoAbility->maxUsageCount == 0) {
                                pButtonData->m_bDisplayCount = FALSE;
                            }
                            pButtonData->m_abilityId.field_10 = cRule.GetItemAbilityDescription(pAmmo->GetResRef(), nAmmoAbility);
                            if (pButtonData->m_abilityId.field_10 == -1) {
                                pButtonData->m_abilityId.field_10 = pAmmo->GetGenericName();
                            }
                            pButtonData->m_launcherIcon = pItem->GetItemIcon();
                            pButtonData->m_launcherName = pItem->GetGenericName();

                            buttons->AddTail(pButtonData);
                        }
                    }

                    pAmmo->Release();
                }
            }
        }
    }

    pItem->Release();

    return buttons;
}

// 0x718650
CItem* CGameSprite::GetLauncher(const ITEM_ABILITY* ability, SHORT& launcherSlot)
{
    int index;
    launcherSlot = 0;

    if (ability == NULL || ability->type != 2) {
        return NULL;
    }

    switch (ability->launcherType) {
    case 1:
        launcherSlot = 2 * m_nWeaponSet + 43;

        if (m_equipment.m_items[launcherSlot] != NULL
            && m_equipment.m_items[launcherSlot]->GetItemType() == 15) {
            return m_equipment.m_items[launcherSlot];
        }

        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 15) {
                return m_equipment.m_items[43 + index];
            }
        }

        break;
    case 2:
        launcherSlot = 2 * m_nWeaponSet + 43;

        if (m_equipment.m_items[launcherSlot] != NULL
            && m_equipment.m_items[launcherSlot]->GetItemType() == 27) {
            return m_equipment.m_items[launcherSlot];
        }

        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 27) {
                return m_equipment.m_items[43 + index];
            }
        }

        break;
    case 3:
        launcherSlot = 2 * m_nWeaponSet + 43;

        if (m_equipment.m_items[launcherSlot] != NULL
            && m_equipment.m_items[launcherSlot]->GetItemType() == 18) {
            return m_equipment.m_items[launcherSlot];
        }

        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 18) {
                return m_equipment.m_items[43 + index];
            }
        }

        break;
    }

    return NULL;
}

// 0x7187E0
SHORT CGameSprite::GetLauncherSlot(SHORT slotNum, SHORT abilityNum)
{
    int index;

    CItem* pItem = m_equipment.m_items[slotNum];
    if (pItem == NULL) {
        return -1;
    }

    pItem->Demand();

    ITEM_ABILITY* ability = pItem->GetAbility(abilityNum);
    if (ability == NULL || ability->type != 2) {
        pItem->Release();
        return -1;
    }

    switch (ability->launcherType) {
    case 1:
        if (m_equipment.m_items[2 * m_nWeaponSet + 43] != NULL
            && m_equipment.m_items[2 * m_nWeaponSet + 43]->GetItemType() == 15) {
            pItem->Release();
            return 2 * m_nWeaponSet + 43;
        }
        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 15) {
                pItem->Release();
                return 43 + index;
            }
        }
        break;
    case 2:
        if (m_equipment.m_items[2 * m_nWeaponSet + 43] != NULL
            && m_equipment.m_items[2 * m_nWeaponSet + 43]->GetItemType() == 27) {
            pItem->Release();
            return 2 * m_nWeaponSet + 43;
        }
        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 27) {
                pItem->Release();
                return 43 + index;
            }
        }
        break;
    case 3:
        if (m_equipment.m_items[2 * m_nWeaponSet + 43] != NULL
            && m_equipment.m_items[2 * m_nWeaponSet + 43]->GetItemType() == 18) {
            pItem->Release();
            return 2 * m_nWeaponSet + 43;
        }
        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 18) {
                pItem->Release();
                return 43 + index;
            }
        }
        break;
    }

    pItem->Release();
    return -1;
}

// 0x718980
BOOL CGameSprite::CheckLauncherType(const ITEM_ABILITY* ability, CItem* pLauncher)
{
    int index;

    if (pLauncher != NULL) {
        switch (ability->launcherType) {
        case 1:
            return pLauncher->GetItemType() == 15;
        case 2:
            return pLauncher->GetItemType() == 27;
        case 3:
            return pLauncher->GetItemType() == 18;
        default:
            return FALSE;
        }
    }

    switch (ability->launcherType) {
    case 0:
        return TRUE;
    case 1:
        if (m_equipment.m_items[2 * m_nWeaponSet + 43] != NULL
            && m_equipment.m_items[2 * m_nWeaponSet + 43]->GetItemType() == 15) {
            return TRUE;
        }
        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 15) {
                return TRUE;
            }
        }
        break;
    case 2:
        if (m_equipment.m_items[2 * m_nWeaponSet + 43] != NULL
            && m_equipment.m_items[2 * m_nWeaponSet + 43]->GetItemType() == 27) {
            return TRUE;
        }
        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 27) {
                return TRUE;
            }
        }
        break;
    case 3:
        if (m_equipment.m_items[2 * m_nWeaponSet + 43] != NULL
            && m_equipment.m_items[2 * m_nWeaponSet + 43]->GetItemType() == 18) {
            return TRUE;
        }
        for (index = 0; index < 8; index++) {
            if (m_equipment.m_items[43 + index] != NULL
                && m_equipment.m_items[43 + index]->GetItemType() == 18) {
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

// 0x718B30
BOOL CGameSprite::ReadyCursor()
{
    switch (m_currentUseButton.m_abilityId.m_targetType) {
    case 1:
        g_pBaldurChitin->GetObjectGame()->SetState(2);
        g_pBaldurChitin->GetObjectGame()->SetIconIndex(20);
        g_pBaldurChitin->GetObjectGame()->m_iconResRef = "";
        return TRUE;
    case 2:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 18306
        UTIL_ASSERT(FALSE);
    case 3:
        g_pBaldurChitin->GetObjectGame()->SetState(2);
        g_pBaldurChitin->GetObjectGame()->SetIconIndex(20);
        g_pBaldurChitin->GetObjectGame()->m_iconResRef = "";
        return TRUE;
    case 4:
        g_pBaldurChitin->GetObjectGame()->SetState(1);
        g_pBaldurChitin->GetObjectGame()->SetIconIndex(20);
        g_pBaldurChitin->GetObjectGame()->m_iconResRef = "";
        return TRUE;
    case 5:
    case 6:
        g_pBaldurChitin->GetObjectGame()->SetState(0);
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->SetSelectedButton(100);
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->UpdateState();
        return FALSE;
    default:
        return TRUE;
    }
}

// 0x718CE0
void CGameSprite::ReadySpell(SHORT buttonNum, INT nType, BOOLEAN firstCall)
{
    switch (nType) {
    case 2:
        m_currentUseButton = m_quickSpells[buttonNum];
        break;
    case 4:
        m_currentUseButton = m_quickInnates[buttonNum];
        break;
    case 6:
        m_currentUseButton = m_quickSongs[buttonNum];
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 18354
        UTIL_ASSERT(FALSE);
    }

    CAIAction action;
    CString string;
    BOOL cursor = ReadyCursor();

    if (!firstCall && !cursor) {
        switch (m_currentUseButton.m_abilityId.m_targetType) {
        case 5:
            // FIXME: One time is usually enough.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            m_currentUseButton.m_abilityId.m_res.CopyToString(string);
            action = CAIAction(CAIAction::SPELL,
                m_typeAI,
                string,
                0,
                m_currentUseButton.m_abilityId.m_nClass | (m_currentUseButton.m_abilityId.field_1E << 8));
            action.m_specificID3 = m_currentUseButton.m_abilityId.field_1D;
            ClearActions(FALSE);
            m_userCommandPause = 75;
            m_triggerId = CGameObjectArray::INVALID_INDEX;
            AddAction(action);
            m_interrupt = TRUE;
            break;
        case 6:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 18385
            UTIL_ASSERT(FALSE);
            break;
        }
    }
}

// 0x719860
void CGameSprite::ReadyItem(SHORT buttonNum, BOOLEAN firstCall)
{
    m_currentUseButton = m_quickItems[buttonNum];

    CAIAction action;
    CString string;
    BOOL cursor = ReadyCursor();

    if (!firstCall && !cursor) {
        switch (m_currentUseButton.m_abilityId.m_targetType) {
        case 5:
            // FIXME: One time is usually enough.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            // FIXME: Unused.
            m_currentUseButton.m_abilityId.m_res.CopyToString(string);

            // NOTE: Uninline.
            action = CAIAction(CAIAction::USEITEM,
                m_typeAI,
                m_currentUseButton.m_abilityId.m_itemNum,
                m_currentUseButton.m_abilityId.m_abilityNum,
                0);

            ClearActions(FALSE);
            m_userCommandPause = 75;
            m_triggerId = CGameObjectArray::INVALID_INDEX;
            AddAction(action);
            m_interrupt = TRUE;
            break;
        case 6:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 18538
            UTIL_ASSERT(FALSE);
            break;
        }
    }
}

// FIXME: `buttonData` should be reference.
//
// 0x719CA0
void CGameSprite::ReadyOffInternalList(CButtonData buttonData, BOOLEAN firstCall)
{
    m_currentUseButton = buttonData;

    CAIAction action;
    CString string;
    BOOL cursor = ReadyCursor();

    if (!firstCall && !cursor) {
        switch (m_currentUseButton.m_abilityId.m_targetType) {
        case 5:
            // FIXME: One time is usually enough.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            // FIXME: Unused.
            m_currentUseButton.m_abilityId.m_res.CopyToString(string);

            // NOTE: Uninline.
            action = CAIAction(CAIAction::USEITEM,
                m_typeAI,
                m_currentUseButton.m_abilityId.m_itemNum,
                m_currentUseButton.m_abilityId.m_abilityNum,
                0);

            ClearActions(FALSE);
            m_userCommandPause = 75;
            m_triggerId = CGameObjectArray::INVALID_INDEX;
            AddAction(action);
            m_interrupt = TRUE;
            break;
        case 6:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 18593
            UTIL_ASSERT(FALSE);
            break;
        }
    }
}

// FIXME: `buttonData` should be reference.
//
// 0x71A0E0
void CGameSprite::sub_71A0E0(CButtonData buttonData, BOOLEAN firstCall)
{
    m_currentUseButton = buttonData;

    CAIAction action;
    CString string;
    BOOL cursor = ReadyCursor();

    if (!firstCall && !cursor) {
        switch (m_currentUseButton.m_abilityId.m_targetType) {
        case 5:
            // FIXME: One time is usually enough.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            m_currentUseButton.m_abilityId.m_res.CopyToString(string);

            // NOTE: Uninline.
            action = CAIAction(CAIAction::SPELL,
                m_typeAI,
                string,
                0,
                m_currentUseButton.m_abilityId.m_nClass | (m_currentUseButton.m_abilityId.field_1E << 8));

            action.m_specificID3 = m_currentUseButton.m_abilityId.field_1D;
            ClearActions(FALSE);
            m_userCommandPause = 75;
            m_triggerId = CGameObjectArray::INVALID_INDEX;
            AddAction(action);
            m_interrupt = TRUE;
            break;
        case 6:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 16649
            UTIL_ASSERT(FALSE);
            break;
        }
    }
}

// FIXME: `buttonData` should be reference.
//
// 0x71A550
void CGameSprite::sub_71A550(CButtonData buttonData, BOOLEAN firstCall)
{
    m_currentUseButton = buttonData;

    CAIAction action;
    CString string;
    BOOL cursor = ReadyCursor();

    if (!firstCall && !cursor) {
        switch (m_currentUseButton.m_abilityId.m_itemType) {
        case 3:
            m_castCounter = -1;

            // FIXME: One time is usually enough.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            m_currentUseButton.m_abilityId.m_res.CopyToString(string);

            // NOTE: Uninline.
            action = CAIAction(CAIAction::SPELL,
                m_typeAI,
                string,
                0,
                m_currentUseButton.m_abilityId.m_nClass | (m_currentUseButton.m_abilityId.field_1E << 8));

            ClearActions(FALSE);
            m_userCommandPause = 75;
            m_triggerId = CGameObjectArray::INVALID_INDEX;
            AddAction(action);
            m_interrupt = TRUE;
            break;
        case 4:
            // FIXME: One time is usually enough.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);

            m_currentUseButton.m_abilityId.m_res.CopyToString(string);

            // NOTE: This call is not inlined (see above).
            action = CAIAction(CAIAction::SPELL,
                m_typeAI,
                string,
                0,
                m_currentUseButton.m_abilityId.m_nClass | (m_currentUseButton.m_abilityId.field_1E << 8));
            action.m_specificID3 = m_currentUseButton.m_abilityId.field_1D;
            ClearActions(FALSE);
            m_userCommandPause = 75;
            m_triggerId = CGameObjectArray::INVALID_INDEX;
            AddAction(action);
            m_interrupt = TRUE;
            break;
        case 5:
            // NOTE: For unknown reason the first call of `SetLastTarget` is
            // inlined, but the second is not.
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            g_pBaldurChitin->GetObjectGame()->SetLastTarget(CGameObjectArray::INVALID_INDEX);
            FireSpell(m_currentUseButton.m_abilityId.m_res, this);
            break;
        }
    }
}

// 0x71B3B0
void CGameSprite::CheckToolTipItem(BYTE buttonNum)
{
    SHORT nItemNum = m_quickItems[buttonNum].m_abilityId.m_itemNum;
    if (nItemNum != -1) {
        CItem* pItem = m_equipment.m_items[nItemNum];
        if (pItem != NULL) {
            // FIXME: Calls `GetGenericName` twice.
            m_quickItems[nItemNum].m_name = pItem->GetGenericName();
            m_quickItems[nItemNum].m_abilityId.field_10 = pItem->GetGenericName();
        }
    }
}

// 0x71B400
CItem* CGameSprite::GetQuickItem(BYTE buttonNum)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 18909
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ITEMS21);

    SHORT nItemNum = m_quickItems[buttonNum].m_abilityId.m_itemNum;
    if (nItemNum != -1) {
        return m_equipment.m_items[nItemNum];
    } else {
        return NULL;
    }
}

// 0x71B460
void CGameSprite::UnequipAll(BOOL animationOnly)
{
    if (m_equipment.m_items[6] != NULL) {
        m_equipment.m_items[6]->Unequip(this, 6, TRUE, animationOnly);
    }

    if (m_equipment.m_items[1] != NULL) {
        m_equipment.m_items[1]->Unequip(this, 1, TRUE, animationOnly);
    }

    if (m_equipment.m_items[5] != NULL) {
        m_equipment.m_items[5]->Unequip(this, 5, TRUE, animationOnly);
    }

    if (m_equipment.m_items[7] != NULL) {
        m_equipment.m_items[7]->Unequip(this, 7, TRUE, animationOnly);
    }

    if (m_equipment.m_items[8] != NULL) {
        m_equipment.m_items[8]->Unequip(this, 8, TRUE, animationOnly);
    }

    if (m_equipment.m_items[0] != NULL) {
        m_equipment.m_items[0]->Unequip(this, 0, TRUE, animationOnly);
    }

    if (m_equipment.m_items[2] != NULL) {
        m_equipment.m_items[2]->Unequip(this, 2, TRUE, animationOnly);
    }

    if (m_equipment.m_items[3] != NULL) {
        m_equipment.m_items[3]->Unequip(this, 3, TRUE, animationOnly);
    }

    if (m_equipment.m_items[4] != NULL) {
        m_equipment.m_items[4]->Unequip(this, 4, TRUE, animationOnly);
    }

    BYTE nIndex = 2 * (m_nWeaponSet + 22);
    if (m_equipment.m_items[nIndex] != NULL) {
        m_equipment.m_items[nIndex]->Unequip(this, nIndex, TRUE, animationOnly);
    }

    if (m_equipment.m_items[m_equipment.m_selectedWeapon] != NULL) {
        m_equipment.m_items[m_equipment.m_selectedWeapon]->Unequip(this, nIndex, TRUE, animationOnly);

        m_equipment.m_items[m_equipment.m_selectedWeapon]->Demand();
        ITEM_ABILITY* pAbility = m_equipment.m_items[m_equipment.m_selectedWeapon]->GetAbility(m_equipment.m_selectedWeaponAbility);

        SHORT lSlot;
        CItem* pLauncher = GetLauncher(pAbility, lSlot);
        if (pLauncher != NULL) {
            pLauncher->Unequip(this, lSlot, TRUE, animationOnly);
        }

        m_equipment.m_items[m_equipment.m_selectedWeapon]->Release();
    }
}

// 0x71B5E0
void CGameSprite::EquipAll(BOOL animationOnly)
{
    if (m_equipment.m_items[6] != NULL) {
        m_equipment.m_items[6]->Unequip(this, 6, TRUE, animationOnly);
    }

    if (m_equipment.m_items[1] != NULL) {
        m_equipment.m_items[1]->Unequip(this, 1, TRUE, animationOnly);
    }

    if (m_equipment.m_items[5] != NULL) {
        m_equipment.m_items[5]->Unequip(this, 5, TRUE, animationOnly);
    }

    if (m_equipment.m_items[7] != NULL) {
        m_equipment.m_items[7]->Unequip(this, 7, TRUE, animationOnly);
    }

    if (m_equipment.m_items[8] != NULL) {
        m_equipment.m_items[8]->Unequip(this, 8, TRUE, animationOnly);
    }

    if (m_equipment.m_items[0] != NULL) {
        m_equipment.m_items[0]->Unequip(this, 0, TRUE, animationOnly);
    }

    if (m_equipment.m_items[2] != NULL) {
        m_equipment.m_items[2]->Unequip(this, 2, TRUE, animationOnly);
    }

    if (m_equipment.m_items[3] != NULL) {
        m_equipment.m_items[3]->Unequip(this, 3, TRUE, animationOnly);
    }

    if (m_equipment.m_items[4] != NULL) {
        m_equipment.m_items[4]->Unequip(this, 4, TRUE, animationOnly);
    }

    BOOL v1 = FALSE;
    BYTE v2 = 2 * m_nWeaponSet + 43;
    if (m_equipment.m_selectedWeapon == v2) {
        v1 = TRUE;
    }

    if (m_equipment.m_items[m_equipment.m_selectedWeapon] != NULL) {
        m_equipment.m_items[m_equipment.m_selectedWeapon]->Equip(this, m_equipment.m_selectedWeapon, animationOnly);

        m_equipment.m_items[m_equipment.m_selectedWeapon]->Demand();

        ITEM_ABILITY* pAbility = m_equipment.m_items[m_equipment.m_selectedWeapon]->GetAbility(m_equipment.m_selectedWeaponAbility);

        SHORT lSlot;
        CItem* pLauncher = GetLauncher(pAbility, lSlot);
        if (pLauncher != NULL) {
            pLauncher->Equip(this, lSlot, animationOnly);

            if (lSlot == v2) {
                v1 = TRUE;
            }
        }
    }

    BYTE v3 = 2 * (m_nWeaponSet + 22);
    if (m_equipment.m_items[v3] != NULL && v1 == TRUE) {
        m_equipment.m_items[v3]->Equip(this, v3, animationOnly);
    }
}

// 0x71B770
void CGameSprite::Equip(SHORT slotNum)
{
    if (slotNum <= 51) {
        if (m_equipment.m_items[slotNum] != NULL) {
            if (slotNum >= 0 && slotNum <= 8) {
                m_equipment.m_items[slotNum]->Equip(this, slotNum, FALSE);
            }
        }
    }
}

// 0x71B7A0
void CGameSprite::Unequip(SHORT slotNum)
{
    if (slotNum <= 51) {
        if (m_equipment.m_items[slotNum] != NULL) {
            switch (slotNum) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                m_equipment.m_items[slotNum]->Unequip(this, slotNum, TRUE, FALSE);
                break;
            case 11:
            case 12:
            case 13:
            case 14:
            case 43:
            case 45:
            case 46:
            case 47:
            case 48:
            case 49:
            case 50:
                if (slotNum == m_equipment.m_selectedWeapon) {
                    m_equipment.m_items[slotNum]->Unequip(this, slotNum, TRUE, FALSE);

                    m_equipment.m_items[slotNum]->Demand();

                    ITEM_ABILITY* pAbility = m_equipment.m_items[slotNum]->GetAbility(m_equipment.m_selectedWeaponAbility);

                    SHORT lSlot;
                    CItem* pLauncher = GetLauncher(pAbility, lSlot);
                    if (pLauncher != NULL) {
                        pLauncher->Unequip(this, slotNum, TRUE, FALSE);
                    }

                    m_equipment.m_items[slotNum]->Release();

                    SelectWeaponAbility(10, 0, 0, 1);
                } else {
                    m_equipment.m_items[slotNum]->Unequip(this, slotNum, TRUE, FALSE);
                }
                break;
            }
        }
    }
}

// 0x71B8D0
SHORT CGameSprite::GetTurnUndeadLevel()
{
    DWORD nLevel = m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_CLERIC);

    // FIXME: Calculates paladin class twice.
    if (m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_PALADIN) > 2) {
        nLevel += m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_PALADIN) - 2;
    }

    return static_cast<SHORT>(nLevel);
}

// 0x71B910
SHORT CGameSprite::GetBackstabDamageMultiplier()
{
    if (!m_typeAI.IsClassValid(CAIObjectType::C_ROGUE)) {
        return 0;
    }

    // NOTE: Uninline.
    CDerivedStats* DStats = GetActiveStats();

    INT nLevel = DStats->GetBardMonkRogueLevel();
    return static_cast<SHORT>(atol(g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_tBackstabMultiplier.GetAt(CPoint(nLevel, 0))));
}

// 0x71B9A0
SHORT CGameSprite::GetLayOnHandsAmount()
{
    return g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetLayOnHandsAmount(m_typeAI, m_derivedStats);
}

// 0x71B9C0
void CGameSprite::AddSpecialAbility(const CResRef& cResInnateSpell, BOOL feedBack)
{
    if (g_pChitin->cDimm.m_cKeyTable.FindKey(cResInnateSpell, 1006, TRUE) == NULL) {
        return;
    }

    // NOTE: The original reads the game's lists directly (+0x4C00, +0x4C08,
    // +0x4C10).
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    WORD nFeedBackId = FEEDBACK_GAINEDSPECIALABILITY;
    UINT nID = 0;

    if (pGame->m_songs.Find(cResInnateSpell, nID) == TRUE) {
        unsigned int a2 = 0;
        unsigned int a3 = 1;
        unsigned int a4 = 1;
        UINT nSongID = 0;
        nFeedBackId = FEEDBACK_GAINEDBARDSONG;
        if (pGame->m_songs.Find(cResInnateSpell, nSongID) == TRUE) {
            m_songs.Add(nSongID, a4, a3, a2);
        }
    } else if (pGame->m_shapeshifts.Find(cResInnateSpell, nID) == TRUE) {
        unsigned int a2 = 0;
        unsigned int a3 = 0;
        unsigned int a4 = 0;
        UINT nShapeID = 0;
        if (pGame->m_shapeshifts.Find(cResInnateSpell, nShapeID) == TRUE) {
            m_shapeshifts.Add(nShapeID, a2, a3, a4);
        }
    } else if (pGame->m_innateSpells.Find(cResInnateSpell, nID) == TRUE) {
        unsigned int a2 = 1;
        unsigned int a3 = 1;
        unsigned int a4 = 0;
        UINT nInnateID = 0;
        if (pGame->m_innateSpells.Find(cResInnateSpell, nInnateID) == TRUE) {
            m_innateSpells.Add(nInnateID, a2, a3, a4);
        }
    }

    if (feedBack == TRUE) {
        // NOTE: Inlined CSpell::CSpell.
        CSpell cSpell(cResInnateSpell);
        cSpell.Demand();
        if (cSpell.pRes != NULL) {
            FeedBack(nFeedBackId, 0, 0, 0, cSpell.GetGenericName(), 0, 0);
        }
        cSpell.Release();
    }
}

// 0x71BC60
void CGameSprite::RemoveSpecialAbility(const CResRef& cResInnateSpell)
{
    // TODO: Incomplete.
}

// 0x71BE80
void CGameSprite::CheckLoadState()
{
    if (m_baseStats.m_generalState == 0) {
        return;
    }

    if ((m_baseStats.m_generalState & STATE_DEAD) != 0) {
        SetSequence(SEQ_TWITCH);
    }

    if ((m_baseStats.m_generalState & STATE_FLAME_DEATH) != 0) {
        GetAnimation()->SetColorEffectAll(0, RGB(75, 75, 75), 1);
    }

    if ((m_baseStats.m_generalState & STATE_STONE_DEATH) != 0) {
        m_animationRunning = FALSE;
        if (GetAnimation()->IsFalseColor()) {
            m_hasColorRangeEffects = TRUE;

            for (BYTE range = 0; range < 7; range++) {
                CColorRange* pColorRange = new CColorRange();
                pColorRange->m_range = range;
                pColorRange->m_color = CVidPalette::STONE;
                m_derivedStats.m_appliedColorRanges.AddTail(pColorRange);

                GetAnimation()->SetColorRange(range, CVidPalette::STONE);
            }
        }
    }

    if ((m_baseStats.m_generalState & STATE_FROZEN_DEATH) != 0) {
        m_animationRunning = FALSE;
        if (GetAnimation()->IsFalseColor()) {
            m_hasColorRangeEffects = TRUE;

            for (BYTE range = 0; range < 7; range++) {
                CColorRange* pColorRange = new CColorRange();
                pColorRange->m_range = range;
                pColorRange->m_color = CVidPalette::ICE;
                m_derivedStats.m_appliedColorRanges.AddTail(pColorRange);

                GetAnimation()->SetColorRange(range, CVidPalette::ICE);
            }
        }
    }
}

// 0x71C0A0
INT CGameSprite::GetNumQuickWeaponSlots()
{
    return g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetNumQuickWeaponSlots(m_typeAI.m_nClass);
}

// 0x71CFF0
BOOL CGameSprite::sub_71CFF0()
{
    if ((m_baseStats.field_2FB & 0x1) != 0) {
        return TRUE;
    }

    BYTE nMorale = m_baseStats.m_morale;
    if (nMorale > 19) {
        nMorale = 20;
    }
    m_baseStats.m_morale = nMorale;

    // NOTE: The original clamps the unsigned byte at 0 too, which changes
    // nothing.
    if (nMorale == 0) {
        nMorale = 0;
    }
    m_baseStats.m_morale = nMorale;

    if (m_derivedStats.m_nMoraleRecoveryTime == 0) {
        m_baseStats.m_morale = 10;
    } else if (static_cast<DWORD>(field_44A) % (m_derivedStats.m_nMoraleRecoveryTime * 15) == 0) {
        if (nMorale > 10) {
            m_baseStats.m_morale = nMorale - 1;
        } else if (nMorale < 10) {
            m_baseStats.m_morale = nMorale + 1;
        }
    }

    if (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene
        && (m_curAction.m_internalFlags & 0x1) == 0) {
        return m_derivedStats.m_nMoraleBreak < m_baseStats.m_morale;
    }

    return TRUE;
}

// 0x71D0B0
void CGameSprite::sub_71D0B0()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    ULONG nGameTime = pGame->GetWorldTimer()->m_gameTime;
    DWORD nHours = (nGameTime - field_722A) / 18000;
    DWORD nFatigue = m_baseStats.m_fatigue;

    if (nFatigue < nHours) {
        ITEM_EFFECT effect;
        CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_FATIGUE);
        effect.effectAmount = nHours;
        effect.dwFlags = 1;
        effect.durationType = 1;

        CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));

        // NOTE: The original leaks the effect when the sprite is immune to
        // it.
        if (!m_derivedStats.m_cImmunitiesEffect.OnList(pEffect)
            && m_derivedStats.m_naturalImmunities.find(pEffect->m_effectID) == m_derivedStats.m_naturalImmunities.end()) {
            CMessage* message = new CMessageAddEffect(pEffect, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            // NOTE: The original's atol (0x7E7B03) is VC6's, which wraps on overflow.
            char nLuck = static_cast<char>(iwd2r_atol(pGame->GetRuleTables().m_tFatigueMod.GetAt(CPoint(0, nHours))));
            m_derivedStats.m_nLuck += nLuck;

            if (nLuck != 0) {
                // NOTE: Uninline.
                AddPortraitIcon(39);
            }

            if (nHours > 6) {
                message = new CMessagePlaySound(3, TRUE, TRUE, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
    } else if (nHours < nFatigue) {
        field_722A = nGameTime - nFatigue * 18000;

        if (m_baseStats.m_fatigue > 6) {
            CMessage* message = new CMessagePlaySound(3, TRUE, TRUE, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }
}

// 0x71D310
SHORT CGameSprite::FindItemPersonal(const CString& sName, int number, BOOL restrictToEquiped)
{
    CString sMutableName(sName);
    sMutableName.MakeUpper();

    SHORT end = 51;
    if (restrictToEquiped) {
        end = 18;
    }

    for (SHORT slot = 0; slot < end; slot++) {
        if (m_equipment.m_items[slot] != NULL
            && m_equipment.m_items[slot]->cResRef == sName) {
            if (number > 0) {
                number--;
            } else {
                return slot;
            }
        }
    }

    return 0;
}

// 0x71D3D0
SHORT CGameSprite::CountItemPersonal(const CString& sName)
{
    SHORT number = 0;

    CString sMutableName(sName);
    sMutableName.MakeUpper();

    for (SHORT slot = 0; slot < 51; slot++) {
        if (m_equipment.m_items[slot] != NULL
            && m_equipment.m_items[slot]->cResRef == sName) {
            if (m_equipment.m_items[slot]->GetMaxStackable() > 1) {
                number += m_equipment.m_items[slot]->GetUsageCount(0);
            } else {
                number++;
            }
        }
    }

    return number;
}

// 0x71D480
DWORD CGameSprite::GetCarriedWeight()
{
    DWORD weight = 0;

    for (int index = 0; index < 51; index++) {
        if (m_equipment.m_items[index] != NULL) {
            weight += m_equipment.m_items[index]->GetWeight();
        }
    }

    return weight;
}

// 0x71D4B0
void CGameSprite::GetQuickWeapon(BYTE buttonNum, CButtonData& buttonData)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 20520
    UTIL_ASSERT_MSG(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_WEAPONS22, "Trying to access illegal quick weapon slot");

    if (m_equipment.m_items[42] != NULL) {
        CGameButtonList* buttons = GetItemUsages(42, 1, 0);
        if (!buttons->IsEmpty()) {
            CButtonData* node = buttons->RemoveHead();
            if (node != NULL) {
                buttonData = *node;
                delete node;
            }
            while (!buttons->IsEmpty()) {
                delete buttons->RemoveHead();
            }
        }
        delete buttons;
    }

    buttonData = m_quickWeapons[buttonNum];

    if (buttonNum == 0
        || buttonNum == 2
        || buttonNum == 4
        || buttonNum == 6) {
        CItem* pItem = m_equipment.m_items[buttonNum + 44];
        if (pItem != NULL) {
            // FIXME: Calls `GetItemType` four times.
            if (pItem->GetItemType() == 47
                || pItem->GetItemType() == 53
                || pItem->GetItemType() == 49
                || pItem->GetItemType() == 41) {
                if (buttonData.m_abilityId.m_itemNum == -1
                    || buttonData.m_abilityId.m_itemNum == 10) {
                    CGameButtonList* buttons = GetItemUsages(10, 1, 0);
                    CButtonData* node = buttons->RemoveHead();
                    if (node != NULL) {
                        buttonData = *node;
                        m_quickWeapons[buttonNum] = *node;
                        delete node;
                    }
                    while (!buttons->IsEmpty()) {
                        delete buttons->RemoveHead();
                    }
                    delete buttons;
                }
            }
        } else if (buttonData.m_abilityId.m_itemNum == -1) {
            if (buttonData.m_abilityId.m_itemNum == -1
                || buttonData.m_abilityId.m_itemNum == 10) {
                CGameButtonList* buttons = GetItemUsages(10, 1, 0);
                CButtonData* node = buttons->RemoveHead();
                if (node != NULL) {
                    buttonData = *node;
                    m_quickWeapons[buttonNum] = *node;
                    delete node;
                }
                while (!buttons->IsEmpty()) {
                    delete buttons->RemoveHead();
                }
                delete buttons;
            }
        }
    } else {
        CItem* pItem = m_equipment.m_items[buttonNum + 43];
        if (pItem != NULL) {
            // FIXME: Calls `GetItemType` four times.
            if (pItem->GetItemType() == 47
                || pItem->GetItemType() == 53
                || pItem->GetItemType() == 49
                || pItem->GetItemType() == 41) {
                buttonData.m_icon = pItem->GetItemIcon();
                buttonData.m_name = pItem->GetGenericName();
                buttonData.m_abilityId.m_itemType = 2;
                buttonData.m_abilityId.m_itemNum = buttonNum + 43;
                buttonData.m_abilityId.m_targetType = -1;
                buttonData.m_abilityId.field_10 = pItem->GetGenericName();
            }
        }
    }
}

// 0x71DAB0
void CGameSprite::SetScript(SHORT level, CAIScript* script)
{
    // FIXME: Unused.
    CString v1;

    switch (level) {
    case 0:
        if (m_overrideScript != NULL) {
            delete m_overrideScript;
            m_overrideScript = NULL;
        }

        m_overrideScript = script;
        m_overrideScript->m_cResRef.GetResRef(m_baseStats.m_scriptOverRide);
        break;
    case 1:
        if (m_special1Script != NULL) {
            delete m_special1Script;
            m_special1Script = NULL;
        }
        m_special1Script = script;
        // FIXME: Probably missing copying resref to `m_baseStats`.
        break;
    case 2:
        if (m_teamScript != NULL) {
            delete m_teamScript;
            m_teamScript = NULL;
        }

        m_teamScript = script;
        m_teamScript->m_cResRef.GetResRef(m_baseStats.m_scriptTeam);
        break;
    case 3:
        if (m_special2Script != NULL) {
            delete m_special2Script;
            m_special2Script = NULL;
        }
        m_special2Script = script;
        m_special2Script->m_cResRef.GetResRef(m_baseStats.m_scriptSpecial2);
        break;
    case 4:
        if (m_combatScript != NULL) {
            delete m_combatScript;
            m_combatScript = NULL;
        }
        m_combatScript = script;
        m_combatScript->m_cResRef.GetResRef(m_baseStats.m_scriptCombat);
        break;
    case 5:
        if (m_special3Script != NULL) {
            delete m_special3Script;
            m_special3Script = NULL;
        }
        m_special3Script = script;
        m_special3Script->m_cResRef.GetResRef(m_baseStats.m_scriptSpecial3);
        break;
    case 6:
        if (m_movementScript != NULL) {
            delete m_movementScript;
            m_movementScript = NULL;
        }
        m_movementScript = script;
        m_movementScript->m_cResRef.GetResRef(m_baseStats.m_scriptMovement);
        break;
    }
}

// 0x71DD20
void CGameSprite::InitializeWalkingSound()
{
    m_nSndWalk = 0;
    m_currSndWalk = 0;

    // NOTE: Uninline.
    char* pSndWalk = m_animation.GetSndWalk(m_pArea->m_search.GetTableIndex(m_pos));
    if (pSndWalk != NULL) {
        m_sndWalk[m_currSndWalk].SetResRef(CResRef(pSndWalk), TRUE, TRUE);
        if (pSndWalk[0] != '\0') {
            delete pSndWalk;
        }
    } else {
        m_sndWalk[m_currSndWalk].SetResRef(CResRef(""), TRUE, TRUE);
    }
}

// 0x71DFB0
void CGameSprite::AutoPause(DWORD type)
{
    COLORREF nameColor;
    STRREF strPause;

    if (type == 256
        || (type == 128 && g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bAutoPauseOnTrap)
        || (type & g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nAutoPauseState) != 0) {
        if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1
            || g_pBaldurChitin->GetObjectGame()->IsAlly(m_id)
            || g_pBaldurChitin->GetObjectGame()->IsFamiliar(m_id)) {
            nameColor = CVidPalette::RANGE_COLORS[m_baseStats.m_colors[CVIDPALETTE_RANGE_MAIN_CLOTH]];
            if (g_pChitin->cNetwork.GetSessionOpen()
                || g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->GetScreenWorld()) {
                switch (type) {
                case 0x1:
                    strPause = 17113; // "Auto-Paused: Weapon Unusable"
                    break;
                case 0x2:
                    strPause = 17114; // "Auto-Paused: Attacked"
                    break;
                case 0x4:
                    strPause = 17115; // "Auto-Paused: Hit"
                    break;
                case 0x8:
                    strPause = 17116; // "Auto-Paused: Badly Wounded"
                    break;
                case 0x10:
                    strPause = 17117; // "Auto-Paused: Dead"
                    break;
                case 0x20:
                    strPause = 17118; // "Auto-Paused: Target Gone"
                    break;
                case 0x40:
                    strPause = 10014; // "Auto-Paused: Round End"
                    break;
                case 0x80:
                    strPause = 18559; // "Trap Detected"
                    break;
                case 0x100:
                    strPause = 7666; // "Auto-Paused: Scripted"
                    break;
                case 0x200:
                    strPause = 23516; // "Enemy Sighted"
                    break;
                case 0x400:
                    strPause = 26310; // "Spell Cast"
                    break;
                default:
                    strPause = 17324; // "Auto-Paused: Unknown Reason"
                    break;
                }
            } else {
                strPause = 17324; // "Auto-Paused: Unknown Reason"
            }

            CScreenWorld* pWorld = g_pBaldurChitin->GetScreenWorld();
            pWorld->m_autoPauseColor = nameColor;
            pWorld->m_autoPauseId = m_id;
            pWorld->m_autoPauseRef = strPause;
            pWorld->m_autoPauseName = GetNameRef();
        }
    }
}

// 0x71E1B0
void CGameSprite::PlayDialogSound(CGameSprite* pNPC)
{
    if (g_pChitin->cNetwork.GetSessionOpen()) {
        CMessage* message = new CMessagePlaySound(SOUND_SELECT,
            TRUE,
            TRUE,
            m_id,
            pNPC->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    } else {
        PlaySound(SOUND_SELECT, TRUE, TRUE, FALSE);
    }
}

// 0x71E9F0
void CGameSprite::AddKnownDivineSpells(const BYTE& nClass)
{
    switch (nClass) {
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 21545
        UTIL_ASSERT(FALSE);
    }

    if ((nClass == CAIOBJECTTYPE_C_CLERIC && m_startTypeAI.IsClassValid(CAIOBJECTTYPE_C_CLERIC) != 0)
        || (nClass == CAIOBJECTTYPE_C_DRUID && m_startTypeAI.IsClassValid(CAIOBJECTTYPE_C_DRUID) != 0)
        || (nClass == CAIOBJECTTYPE_C_PALADIN && m_startTypeAI.IsClassValid(CAIOBJECTTYPE_C_PALADIN) != 0)
        || (nClass == CAIOBJECTTYPE_C_RANGER && m_startTypeAI.IsClassValid(CAIOBJECTTYPE_C_RANGER) != 0)) {
        // NOTE: Original code is slightly different (full of inlined
        // functions).
        UINT nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);
        CGroupedSpellList* pGameSpells = &(g_pBaldurChitin->GetObjectGame()->m_spellsByClass[nClassIndex]);

        // NOTE: Uninline.
        CGameSpriteGroupedSpellList* pSpriteSpells = GetSpells(nClass);

        CResRef resRef;

        UINT nClassLevel = m_derivedStats.GetClassLevel(nClass);
        UINT nMaxSpellLevel = g_pBaldurChitin->GetObjectGame()->GetRuleTables().GetMaxSpellLevel(nClass, nClassLevel);

        for (UINT nLevel = 0; nLevel < nMaxSpellLevel; nLevel++) {
            for (UINT nIndex = 0; nIndex < pGameSpells->m_lists[nLevel].m_nCount; nIndex++) {
                // NOTE: Uninline.
                UINT nID = pGameSpells->m_lists[nLevel].Get(nIndex);

                UINT temp;
                if (!pSpriteSpells->m_lists[nLevel].Find(nID, temp)) {
                    // NOTE: Uninline.
                    resRef = g_pBaldurChitin->GetObjectGame()->m_spells.Get(nID);

                    CSpell cSpell;
                    cSpell.SetResRef(resRef, TRUE, TRUE);

                    DWORD dwClassFlags;
                    switch (nClass) {
                    case CAIOBJECTTYPE_C_CLERIC:
                    case CAIOBJECTTYPE_C_PALADIN:
                        dwClassFlags = 0x40000000;
                        break;
                    case CAIOBJECTTYPE_C_DRUID:
                    case CAIOBJECTTYPE_C_RANGER:
                        dwClassFlags = 0x80000000;
                        break;
                    default:
                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                        // __LINE__: 21606
                        UTIL_ASSERT(FALSE);
                    }

                    if (cSpell.pRes != NULL) {
                        // FIXME: Calls `GetNotUsableBy` many times.
                        if ((cSpell.GetNotUsableBy() & dwClassFlags) != 0) {
                            continue;
                        }

                        if ((cSpell.GetNotUsableBy() & 0x4) != 0
                            && IcewindMisc::IsGood(this)) {
                            continue;
                        }

                        if ((cSpell.GetNotUsableBy() & 0x2) != 0
                            && IcewindMisc::IsEvil(this)) {
                            continue;
                        }

                        if ((cSpell.GetNotUsableBy() & 0x8) != 0
                            && !IcewindMisc::IsGood(this)
                            && !IcewindMisc::IsEvil(this)) {
                            continue;
                        }

                        if ((cSpell.GetNotUsableBy() & 0x10) != 0
                            && IcewindMisc::IsLawful(this)) {
                            continue;
                        }

                        if ((cSpell.GetNotUsableBy() & 0x1) != 0
                            && IcewindMisc::IsChaotic(this)) {
                            continue;
                        }

                        if ((cSpell.GetNotUsableBy() & 0x20) != 0
                            && !IcewindMisc::IsLawful(this)
                            && !IcewindMisc::IsChaotic(this)) {
                            continue;
                        }

                        // NOTE: Uninline.
                        bool bResult = AddKnownSpell(nClass, nLevel, resRef, 0, 0, 0);

                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                        // __LINE__: 21648
                        UTIL_ASSERT(bResult == true);
                    }
                }
            }
        }
    }
}

// 0x71F170
void CGameSprite::DisplayTextRef(STRREF nameRef, STRREF textRef, COLORREF nameColor, COLORREF textColor)
{
    STR_RES strResName;
    STR_RES strResText;
    CString name;

    if (nameRef >= -7 && nameRef != -1) {
        LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterSlot(-2 - nameRef);

        CGameSprite* pSprite;

        BYTE rc;
        do {
            rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc == CGameObjectArray::SUCCESS) {
            name = pSprite->GetName();

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    } else {
        g_pBaldurChitin->GetTlkTable().Fetch(nameRef, strResName);
        name = strResName.szText;
    }

    g_pBaldurChitin->GetTlkTable().Fetch(textRef, strResText);
    if (strResText.cSound.GetRes() != NULL) {
        if (!strResText.cSound.GetLooping()) {
            strResText.cSound.SetFireForget(TRUE);
        }
        strResText.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
        strResText.cSound.Play(GetPos().x, GetPos().y, 0, FALSE);
    }

    strResText.szText.TrimLeft();
    strResText.szText.TrimRight();
    if (strResText.szText != "") {
        g_pBaldurChitin->GetScreenWorld()->DisplayText(name,
            strResText.szText,
            nameColor,
            textColor,
            -1,
            FALSE);
    }
}

// 0x71FBA0
void CGameSprite::GetNumInventoryPersonalSlots(INT& nUsedSlots, INT& nTotalSlots)
{
    nTotalSlots = CScreenInventory::PERSONAL_INVENTORY_SIZE;
    nUsedSlots = 0;

    for (int index = 0; index < nTotalSlots; index++) {
        if (m_equipment.m_items[index + 18] != NULL) {
            nUsedSlots++;
        }
    }
}

// 0x71FC00
void CGameSprite::SetModalState(BYTE modalState, BOOL bUpdateToolbar)
{
    if (modalState == m_nModalState) {
        return;
    }

    switch (m_nModalState) {
    case 1:
        // The battle song ends. With the Lingering Song feat (0x24) the song
        // lingers on everyone in range instead of the "battle song ends"
        // feedback.
        if (sub_763150(CGAMESPRITE_FEAT_LINGERING_SONG) != 1) {
            FeedBack(FEEDBACK_BATTLESONGEND, 0, 0, 0, -1, 0, 0);
            break;
        }

        if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
            && (m_baseStats.m_generalState & STATE_SILENCED) == 0
            && g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene != TRUE) {
            UINT nSongIndex = m_nLastSong;

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 22207
            UTIL_ASSERT_MSG(nSongIndex < g_pBaldurChitin->GetObjectGame()->m_songs.m_nCount,
                "Invalid Song Index");

            CResRef songResRef = g_pBaldurChitin->GetObjectGame()->m_songs.Get(nSongIndex);

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 22209
            UTIL_ASSERT(songResRef != "");

            CSpell cSpell(songResRef);
            cSpell.Demand();

            if (cSpell.GetCasterType() != 5) {
                cSpell.Release();
                break;
            }

            FeedBack(FEEDBACK_LINGERINGSONG, 0, 0, 0, cSpell.GetGenericName(), 0, 0);

            SPELL_ABILITY* pAbility = cSpell.GetAbility(0);

            CTypedPtrList<CPtrList, LONG*> targets(10);
            m_pArea->GetAllInRange(GetVertListPos(),
                GetPos(),
                CAIObjectType::ANYONE,
                400,
                GetTerrainTable(),
                targets,
                TRUE,
                FALSE);
            m_pArea->GetAllInRangeBack(GetPos(),
                CAIObjectType::ANYONE,
                400,
                GetTerrainTable(),
                targets,
                TRUE,
                FALSE,
                FALSE);
            targets.AddTail(reinterpret_cast<LONG*>(static_cast<INT_PTR>(m_id)));

            POSITION pos = targets.GetHeadPosition();
            while (pos != NULL) {
                LONG nTargetId = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos)));

                CGameObject* pObject;
                BYTE rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nTargetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
                if (rc != CGameObjectArray::SUCCESS) {
                    continue;
                }

                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                    CGameSprite* pTarget = static_cast<CGameSprite*>(pObject);
                    if ((songResRef != "SPIN147"
                            || (IcewindMisc::sub_585230(this, pTarget) != TRUE
                                && IcewindMisc::sud_585070(pTarget) != TRUE
                                && !pTarget->m_derivedStats.m_spellStates.test(SPLSTATE_DEAFENED)))
                        && IcewindMisc::sub_585230(this, pTarget)) {
                        // TODO: Incomplete (0x720004-0x7201A0): one effect
                        // per effect of the song's first ability
                        // (CSpell_DecodeEffect, 0x54A510, with the caster's
                        // position and id, IcewindMisc::sub_5860F0, a
                        // permanent effect of duration type 7 made timed for
                        // 200 ticks), each wrapped in a CMessageAddEffect
                        // (vtable 0x8487CC) and all sent as one CMessageGroup
                        // (0x5152C0). Neither CSpell_DecodeEffect nor
                        // CMessageGroup is in src/ yet.
                        (void)pAbility;
                    }
                }

                g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nTargetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }

            cSpell.Release();
        }
        break;
    case 2:
        FeedBack(FEEDBACK_SEARCHEND, 0, 0, 0, -1, 0, 0);
        break;
    case 3:
        if (m_bHiding) {
            FeedBack(FEEDBACK_LEAVING_SHADOWS, 0, 0, 0, -1, 0, 0);
            m_bHiding = FALSE;
        }
        break;
    case 4:
        FeedBack(FEEDBACK_TURNUNDEADEND, 0, 0, 0, -1, 0, 0);
        break;
    }

    m_nModalState = modalState;

    if (bUpdateToolbar
        && g_pBaldurChitin->GetObjectGame()->GetGroup()->GetGroupLeader() == m_id) {
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->UpdateButtons();
    }
}

// 0x7202E0
BOOL CGameSprite::sub_7202E0()
{
    if (g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene) {
        return FALSE;
    } else {
        return (m_curAction.m_internalFlags & 0x1) == 0;
    }
}

// 0x720310
void CGameSprite::CheckSequence(BYTE& sequence)
{
    // TODO: Incomplete.
}

// 0x7204C0
void CGameSprite::sub_7204C0()
{
    CString animationResRef;
    CMemINI* pINI = &(g_pBaldurChitin->GetObjectGame()->m_INISounds);
    CMemINIValue* pValue;

    // NOTE: Uninline.
    m_animation.GetAnimationResRef(animationResRef, CGameAnimationType::RANGE_BODY);

    field_70FB = FALSE;
    field_70FC = FALSE;
    field_70FD = FALSE;
    field_70FE = FALSE;
    field_70FF = FALSE;
    field_7100 = FALSE;
    field_7101 = FALSE;

    pValue = pINI->GetFast(animationResRef, CString("att1"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_70FB = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("att1frame")));

        pValue = pINI->GetFast(animationResRef, CString("att2"));
        if (pValue != NULL && pValue->GetValue() != "") {
            sub_720B50(pValue, pINI->GetFast(animationResRef, CString("att2frame")));
        }

        pValue = pINI->GetFast(animationResRef, CString("att3"));
        if (pValue != NULL && pValue->GetValue() != "") {
            sub_720B50(pValue, pINI->GetFast(animationResRef, CString("att3frame")));
        }

        pValue = pINI->GetFast(animationResRef, CString("att4"));
        if (pValue != NULL && pValue->GetValue() != "") {
            sub_720B50(pValue, pINI->GetFast(animationResRef, CString("att4frame")));
        }
    }

    pValue = pINI->GetFast(animationResRef, CString("btlcry"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_70FC = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("btlcryframe")));
    }

    pValue = pINI->GetFast(animationResRef, CString("damage"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_70FD = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("damageframe")));
    }

    pValue = pINI->GetFast(animationResRef, CString("death"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_70FE = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("deathframe")));
    }

    pValue = pINI->GetFast(animationResRef, CString("fidget"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_70FF = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("fidgetframe")));
    }

    pValue = pINI->GetFast(animationResRef, CString("selected"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_7100 = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("selectedframe")));
    }

    pValue = pINI->GetFast(animationResRef, CString("fall"));
    if (pValue != NULL && pValue->GetValue() != "") {
        field_7101 = TRUE;
        sub_720B50(pValue, pINI->GetFast(animationResRef, CString("fallframe")));
    }
}

// 0x720B50
void CGameSprite::sub_720B50(CMemINIValue* a1, CMemINIValue* a2)
{
    CString v1;
    CString v2;
    CString v3(a1->GetValue());
    CString v4;
    if (a2 != NULL) {
        v4 = a2->GetValue();
    }
    CGameSpriteSoundEntry entry;
    INT pos1;
    INT pos2;

    if (v3[v3.GetLength() - 1] != ',') {
        v3 += ',';
    }

    if (v4 != "" && v4[v4.GetLength() - 1] != ',') {
        v4 += ',';
    }

    if (a1->GetName() == "att1") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_72F0.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "att2") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_730C.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "att3") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_7328.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "att4") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_7344.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "btlcry") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_7360.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "damage") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_737C.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "death") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_7398.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "fidget") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_73B4.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "selected") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_73D0.AddTail(entry);
            }
        }
    }

    if (a1->GetName() == "fall") {
        while (1) {
            v1 = "";
            v2 = "";
            entry.field_0 = "";
            entry.field_4 = 0;

            if (v3.IsEmpty()) {
                break;
            }

            pos1 = v3.FindOneOf(",");
            if (!v4.IsEmpty()) {
                pos2 = v4.FindOneOf(",");
            }

            if (pos1 >= 0) {
                v1 = v3.Left(pos1);
                v3 = v3.Right(v3.GetLength() - pos1 - 1);
                v3.TrimRight();
                v3.TrimLeft();
                v1.TrimRight();
                v1.TrimLeft();
            }

            if (pos2 >= 0) {
                v2 = v4.Left(pos2);
                v4 = v4.Right(v4.GetLength() - pos2 - 1);
                v4.TrimRight();
                v4.TrimLeft();
                v2.TrimRight();
                v2.TrimLeft();
            }

            if (!v1.IsEmpty()) {
                entry.field_0 = v1;
                if (!v2.IsEmpty()) {
                    entry.field_4 = atoi(v2);
                } else {
                    entry.field_4 = 0;
                }
                field_73EC.AddTail(entry);
            }
        }
    }
}

// 0x722530
SHORT CGameSprite::FindItemBags(const CString& sName, LONG number, BOOL checkForIdentified)
{
    CString sMutableName(sName);
    sMutableName.MakeUpper();

    if (number <= 1) {
        number = 1;
    }

    for (SHORT slot = 0; slot < 51; slot++) {
        CItem* pItem = m_equipment.m_items[slot];
        if (pItem != NULL && pItem->GetItemType() == 58) {
            number -= g_pBaldurChitin->GetObjectGame()->FindItemInStore(pItem->GetResRef(),
                CResRef(sMutableName),
                checkForIdentified);
            if (number <= 0) {
                return slot;
            }
        }
    }

    return -1;
}

// 0x722610
SHORT CGameSprite::TakeItemBags(const CString& sName, LONG number, SHORT slotNum)
{
    CString sMutableName(sName);
    sMutableName.MakeUpper();

    SHORT numTaken = 0;
    if (slotNum != -1) {
        CItem* pItem = m_equipment.m_items[slotNum];
        if (pItem != NULL
            && pItem->GetItemType() == 58) {
            numTaken = g_pBaldurChitin->GetObjectGame()->TakeItemFromStore(pItem->GetResRef(),
                CResRef(sName),
                number);
        }
    } else {
        for (SHORT slot = 0; slot < 51; slot++) {
            CItem* pItem = m_equipment.m_items[slot];
            if (pItem != NULL && pItem->GetItemType() == 58) {
                numTaken += g_pBaldurChitin->GetObjectGame()->TakeItemFromStore(pItem->GetResRef(),
                    CResRef(sName),
                    number);
                if (number - numTaken <= 0) {
                    break;
                }
            }
        }
    }
    return numTaken;
}

// 0x722820
void CGameSprite::sub_722820()
{
    CAIObjectType typeAI(m_startTypeAI);

    switch (typeAI.m_nRace) {
    case CAIOBJECTTYPE_R_HUMAN:
        if (typeAI.m_nSubRace == 1 || typeAI.m_nSubRace == 2) {
            m_baseStats.m_generalState |= STATE_INFRAVISION;
        }
        break;
    case CAIOBJECTTYPE_R_ELF:
        switch (typeAI.m_nSubRace) {
        case 0:
            break;
        case 1:
            m_baseStats.m_generalState |= STATE_INFRAVISION;
            break;
        case 2:
            break;
        default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 23869
        UTIL_ASSERT(FALSE);
            break;
        }
        break;
    case CAIOBJECTTYPE_R_HALF_ELF:
        if (typeAI.m_nSubRace != 0) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 23887
            UTIL_ASSERT(FALSE);
        }
        break;
    case CAIOBJECTTYPE_R_DWARF:
        m_baseStats.m_generalState |= STATE_INFRAVISION;
        if (typeAI.m_nSubRace != 0 && typeAI.m_nSubRace != 1 && typeAI.m_nSubRace != 2) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 23918
            UTIL_ASSERT(FALSE);
        }
        break;
    case CAIOBJECTTYPE_R_HALFLING:
        if (typeAI.m_nSubRace != 0 && typeAI.m_nSubRace != 1 && typeAI.m_nSubRace != 2) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 23946
            UTIL_ASSERT(FALSE);
        }
        break;
    case CAIOBJECTTYPE_R_GNOME:
        switch (typeAI.m_nSubRace) {
        case 0:
            break;
        case 1:
            m_baseStats.m_generalState |= STATE_INFRAVISION | STATE_NONDETECTION;
            m_baseStats.field_3E = 14;
            break;
        default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 23977
        UTIL_ASSERT(FALSE);
            break;
        }
        break;
    case CAIOBJECTTYPE_R_HALF_ORC:
        m_baseStats.m_generalState |= STATE_INFRAVISION;
        if (typeAI.m_nSubRace != 0) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 23998
            UTIL_ASSERT(FALSE);
        }
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 24006
        UTIL_ASSERT(FALSE);
        break;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    const CRuleTables& ruleTables = pGame->GetRuleTables();

    CString sRace = ruleTables.GetRaceString(typeAI.m_nRace, typeAI.m_nSubRace);
    CString sAbilities = ruleTables.m_tRaceSpAb.GetAt(CRuleTables::SPECIAL_ABILITIES_FILE, sRace);
    if (strcmp(sAbilities, "*") != 0) {
        C2DArray tAbilities;
        tAbilities.Load(CResRef(sAbilities));

        for (UINT nRow = 0; nRow < static_cast<UINT>(tAbilities.GetHeight()); nRow++) {
            CResRef cResAbility(tAbilities.GetAt(CPoint(0, nRow)));
            if (strcmp(cResAbility.GetResRefStr(), "*") != 0) {
                // NOTE: The original's atol (0x7E7B03) is VC6's, which wraps on overflow.
                UINT nCount = iwd2r_atol(tAbilities.GetAt(CPoint(1, nRow)));
                if (nCount == 1) {
                    UINT nID = 0;
                    if (pGame->m_innateSpells.Find(cResAbility, nID) == TRUE) {
                        BOOL bKnown = FALSE;
                        for (UINT nIndex = 0; nIndex < m_innateSpells.m_List.size(); nIndex++) {
                            if (m_innateSpells.m_List[nIndex].m_nID == nID) {
                                bKnown = TRUE;
                                break;
                            }
                        }

                        if (!bKnown) {
                            AddSpecialAbility(cResAbility, FALSE);
                        }
                    }
                } else {
                    for (; nCount != 0; nCount--) {
                        AddSpecialAbility(cResAbility, FALSE);
                    }
                }
            }
        }
    }
}

// Returns `TRUE` if object contains quest items or containers (such as potion
// bags). See STREF #26585.
//
// 0x723350
BOOL CGameSprite::HaveUnexportableItems()
{
    for (INT nSlot = 0; nSlot < 51; nSlot++) {
        CItem* pItem = m_equipment.m_items[nSlot];
        if (pItem != NULL) {
            if (pItem->GetItemType() == 58
                || (pItem->GetFlagsFile() & 0x800) != 0) {
                return TRUE;
            }
        }
    }

    return FALSE;
}

// 0x723390
void CGameSprite::SetMonkAbilities()
{
    if (GetAIType().IsClassValid(CAIObjectType::C_MONK)) {
        if (m_equipment.m_selectedWeapon == 10) {
            if (m_equipment.m_items[10] != NULL) {
                m_equipment.m_items[10]->Unequip(this, 10, FALSE, FALSE);
            }
        }

        if (m_equipment.m_items[10] != NULL) {
            delete m_equipment.m_items[10];
        }

        // NOTE: Uninline.
        CDerivedStats* pStats = GetActiveStats();

        CItem* pItem;
        switch (pStats->GetClassLevel(CAIOBJECTTYPE_C_MONK)) {
        case 1:
        case 2:
        case 3:
            pItem = new CItem(CResRef("00MFIST1"), 0, 0, 0, 0, 0);
            break;
        case 4:
        case 5:
        case 6:
        case 7:
            pItem = new CItem(CResRef("00MFIST2"), 0, 0, 0, 0, 0);
            break;
        case 8:
        case 9:
            pItem = new CItem(CResRef("00MFIST3"), 0, 0, 0, 0, 0);
            break;
        case 10:
        case 11:
            pItem = new CItem(CResRef("00MFIST4"), 0, 0, 0, 0, 0);
            break;
        case 12:
            pItem = new CItem(CResRef("00MFIST5"), 0, 0, 0, 0, 0);
            break;
        case 13:
        case 14:
        case 15:
            pItem = new CItem(CResRef("00MFIST6"), 0, 0, 0, 0, 0);
            break;
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
            pItem = new CItem(CResRef("00MFIST7"), 0, 0, 0, 0, 0);
            break;
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            pItem = new CItem(CResRef("00MFIST8"), 0, 0, 0, 0, 0);
            break;
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
            pItem = new CItem(CResRef("00MFIST9"), 0, 0, 0, 0, 0);
            break;
        default:
            pItem = new CItem(CResRef("00MFIST1"), 0, 0, 0, 0, 0);
            break;
        }

        m_equipment.m_items[10] = pItem;

        if (m_equipment.m_selectedWeapon == 10) {
            m_equipment.m_items[10]->Equip(this, 10, FALSE);
        }
    }
}

// 0x7238A0
LONG CGameSprite::GetLevel()
{
    return m_derivedStats.GetAtOffset(STAT_CLASSLEVELSUM);
}

// 0x7238C0
BOOLEAN CGameSprite::DoAIUpdate(BOOLEAN active, LONG counter)
{
    // NOTE: Unlike CGameObject::DoAIUpdate (0x44CC00) the sprite's version
    // ignores `active`: sprites get their AI update while the world is
    // paused too (CGameSprite::AIUpdate and ProcessAI decide what runs).
    if ((counter & m_AISpeed) != (m_AISpeed & m_id)) {
        return FALSE;
    }

    if ((m_baseStats.m_flags & 0x200000) == 0) {
        m_AIInhibitor = (m_AIInhibitor + 1) % 3;
    } else {
        if (m_AIInhibitor == 0) {
            return TRUE;
        }
        m_AIInhibitor = 0;
    }

    if (m_AIInhibitor != 0) {
        if (m_pArea != NULL
            && g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_mode != 0x142) {
            CDerivedStats* pStats = m_bAllowEffectListCall ? &m_derivedStats : &m_tempStats;
            if (!pStats->m_bPreventAISlowDown) {
                if (GetAIType().m_nEnemyAlly < CAIObjectType::EA_EVILCUTOFF
                    && GetAIType().m_nEnemyAlly > CAIObjectType::EA_CONTROLCUTOFF
                    && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1) {
                    CGameArea* pArea = m_pArea;
                    BOOL bVisible = TRUE;
                    if (g_pChitin->cNetwork.GetSessionOpen() != TRUE) {
                        CRect rFx;
                        CPoint ptReference;
                        m_animation.m_animation->CalculateFxRectMax(rFx,
                            ptReference,
                            pArea->GetHeightOffset(m_pos, m_listType));

                        LONG nBottom = pArea->GetHeightOffset(m_pos, m_listType) + m_pos.y;

                        CRect rSprite;
                        rSprite.left = m_pos.x - ptReference.x;
                        rSprite.top = nBottom - ptReference.y;
                        rSprite.right = rFx.right - ptReference.x - rFx.left + m_pos.x;
                        rSprite.bottom = rFx.bottom - rFx.top - ptReference.y + nBottom;

                        const CInfinity& cInfinity = pArea->m_cInfinity;
                        CRect rView;
                        rView.left = cInfinity.nCurrentX;
                        rView.top = cInfinity.nCurrentY;
                        rView.right = cInfinity.rViewPort.right - cInfinity.rViewPort.left + cInfinity.nCurrentX;
                        rView.bottom = cInfinity.rViewPort.bottom - cInfinity.rViewPort.top + cInfinity.nCurrentY;

                        CRect rIntersect;
                        IntersectRect(&rIntersect, &rSprite, &rView);
                        bVisible = !IsRectEmpty(&rIntersect);
                    }

                    if (m_canBeSeen == 0) {
                        return TRUE;
                    }

                    if (!bVisible) {
                        return TRUE;
                    }
                }
            }
        }

        m_AIInhibitor = 0;
    }

    return TRUE;
}

// 0x723B20
void CGameSpriteLastUpdate::Initialize(BOOL bFullUpdateRequired)
{
    // TODO: Incomplete.
}

// 0x723BF0
void CGameSprite::sub_723BF0(BOOLEAN a1, BOOLEAN a2)
{
    // TODO: Incomplete.
}

// 0x723CC0
INT CGameSprite::GetAC()
{
    // TODO: Incomplete.

    return 0;
}

// 0x723F60
INT CGameSprite::sub_723F60()
{
    // TODO: Incomplete.

    return 0;
}

// 0x71E240
void CGameSprite::sub_71E240(CCreatureFileHeader& oldBaseStats, CDerivedStats& oldDStats)
{
    // CGAMESPRITE_FEAT_AEGIS_OF_RIME
    if ((oldBaseStats.m_feats[0] & 0x1) == 0 && (m_baseStats.m_feats[0] & 0x1) != 0) {
        m_baseStats.m_resistColdBase += 5;
        if (m_baseStats.m_resistColdBase < 0) {
            m_baseStats.m_resistColdBase = 0;
        } else if (m_baseStats.m_resistColdBase > 100) {
            m_baseStats.m_resistColdBase = 100;
        }
    }

    // CGAMESPRITE_FEAT_AQUA_MORTIS
    if ((oldBaseStats.m_feats[0] & 0x4) == 0 && (m_baseStats.m_feats[0] & 0x4) != 0) {
        m_baseStats.m_resistAcidBase += 5;
        if (m_baseStats.m_resistAcidBase < 0) {
            m_baseStats.m_resistAcidBase = 0;
        } else if (m_baseStats.m_resistAcidBase > 100) {
            m_baseStats.m_resistAcidBase = 100;
        }
    }

    // CGAMESPRITE_FEAT_BULLHEADED
    if ((oldBaseStats.m_feats[0] & 0x80) == 0 && (m_baseStats.m_feats[0] & 0x80) != 0) {
        m_baseStats.m_saveVSWillBase += 1;
    }

    // CGAMESPRITE_FEAT_DISCIPLINE
    if ((oldBaseStats.m_feats[0] & 0x8000) == 0 && (m_baseStats.m_feats[0] & 0x8000) != 0) {
        m_baseStats.m_saveVSWillBase += 1;
    }

    // CGAMESPRITE_FEAT_GREAT_FORTITUDE
    if ((oldBaseStats.m_feats[0] & 0x4000000) == 0 && (m_baseStats.m_feats[0] & 0x4000000) != 0) {
        m_baseStats.m_saveVSFortitudeBase += 2;
    }

    // CGAMESPRITE_FEAT_IRON_WILL
    if ((oldBaseStats.m_feats[1] & 0x4) == 0 && (m_baseStats.m_feats[1] & 0x4) != 0) {
        m_baseStats.m_saveVSWillBase += 2;
    }

    // CGAMESPRITE_FEAT_LIGHTNING_REFLEXES
    if ((oldBaseStats.m_feats[1] & 0x8) == 0 && (m_baseStats.m_feats[1] & 0x8) != 0) {
        m_baseStats.m_saveVSReflexBase += 2;
    }

    // CGAMESPRITE_FEAT_LUCK_OF_HEROES
    if ((oldBaseStats.m_feats[1] & 0x20) == 0 && (m_baseStats.m_feats[1] & 0x20) != 0) {
        m_baseStats.m_saveVSReflexBase += 1;
        m_baseStats.m_saveVSFortitudeBase += 1;
        m_baseStats.m_saveVSWillBase += 1;
    }

    // CGAMESPRITE_FEAT_SCION_OF_STORMS
    if ((oldBaseStats.m_feats[1] & 0x80000) == 0 && (m_baseStats.m_feats[1] & 0x80000) != 0) {
        m_baseStats.m_resistElectricityBase += 5;
        if (m_baseStats.m_resistElectricityBase < 0) {
            m_baseStats.m_resistElectricityBase = 0;
        } else if (m_baseStats.m_resistElectricityBase > 100) {
            m_baseStats.m_resistElectricityBase = 100;
        }
    }

    // CGAMESPRITE_FEAT_SNAKE_BLOOD
    if ((oldBaseStats.m_feats[1] & 0x8000000) == 0 && (m_baseStats.m_feats[1] & 0x8000000) != 0) {
        m_baseStats.m_saveVSReflexBase += 1;
    }

    // CGAMESPRITE_FEAT_SPIRIT_OF_FLAME
    if ((oldBaseStats.m_feats[2] & 0x2) == 0 && (m_baseStats.m_feats[2] & 0x2) != 0) {
        m_baseStats.m_resistFireBase += 5;
        if (m_baseStats.m_resistFireBase < 0) {
            m_baseStats.m_resistFireBase = 0;
        } else if (m_baseStats.m_resistFireBase > 100) {
            m_baseStats.m_resistFireBase = 100;
        }
    }

    // CGAMESPRITE_FEAT_ENVENOM_WEAPON
    if ((oldBaseStats.m_feats[0] & 0x20000) == 0 && (m_baseStats.m_feats[0] & 0x20000) != 0) {
        AddSpecialAbility(SPIN231, FALSE);
    }

    // CGAMESPRITE_FEAT_MAXIMIZED_ATTACKS
    if ((oldBaseStats.m_feats[1] & 0x2000) == 0 && (m_baseStats.m_feats[1] & 0x2000) != 0) {
        AddSpecialAbility(SPIN197, FALSE);
    }

    // CGAMESPRITE_FEAT_WILDSHAPE_BOAR
    if ((oldBaseStats.m_feats[2] & 0x100) == 0 && (m_baseStats.m_feats[2] & 0x100) != 0) {
        AddSpecialAbility(SPIN111, FALSE);
    }

    // CGAMESPRITE_FEAT_WILDSHAPE_PANTHER
    if ((oldBaseStats.m_feats[2] & 0x200) == 0 && (m_baseStats.m_feats[2] & 0x200) != 0) {
        AddSpecialAbility(SPIN245, FALSE);
    }

    // CGAMESPRITE_FEAT_WILDSHAPE_SHAMBLER
    if ((oldBaseStats.m_feats[2] & 0x400) == 0 && (m_baseStats.m_feats[2] & 0x400) != 0) {
        AddSpecialAbility(SPIN246, FALSE);
    }

    if (sub_763150(CGAMESPRITE_FEAT_STUNNING_FIST) == TRUE) {
        INT nLevel = m_derivedStats.GetClassMaskLevel(0xFFF);

        INT nStart;
        if ((oldBaseStats.m_feats[2] & 0x8) == 0 && (m_baseStats.m_feats[2] & 0x8) != 0) {
            nStart = 1;
        } else {
            nStart = oldDStats.GetClassMaskLevel(0xFFF);
        }

        for (; nStart <= nLevel; nStart++) {
            if (nStart % 4 == 0) {
                AddSpecialAbility(SPIN232, FALSE);
            }
        }
    }

    // CGAMESPRITE_FEAT_POWER_ATTACK
    if ((oldBaseStats.m_feats[1] & 0x8000) == 0 && (m_baseStats.m_feats[1] & 0x8000) != 0) {
        AddSpecialAbility(SPIN275, FALSE);
    }

    // CGAMESPRITE_FEAT_EXPERTISE
    if ((oldBaseStats.m_feats[0] & 0x80000) == 0 && (m_baseStats.m_feats[0] & 0x80000) != 0) {
        AddSpecialAbility(SPIN276, FALSE);
    }

    // CGAMESPRITE_FEAT_ARTERIAL_STRIKE
    if ((oldBaseStats.m_feats[0] & 0x20) == 0 && (m_baseStats.m_feats[0] & 0x20) != 0) {
        AddSpecialAbility(SPIN277, FALSE);
    }

    // CGAMESPRITE_FEAT_HAMSTRING
    if ((oldBaseStats.m_feats[0] & 0x8000000) == 0 && (m_baseStats.m_feats[0] & 0x8000000) != 0) {
        AddSpecialAbility(SPIN278, FALSE);
    }

    // CGAMESPRITE_FEAT_RAPID_SHOT
    if ((oldBaseStats.m_feats[1] & 0x20000) == 0 && (m_baseStats.m_feats[1] & 0x20000) != 0) {
        AddSpecialAbility(SPIN279, FALSE);
    }

    INT nDelta;

    // CGAMESPRITE_FEAT_TOUGHNESS
    if ((m_baseStats.m_feats[2] & 0x20) != 0) {
        nDelta = m_baseStats.m_featToughness - oldBaseStats.m_featToughness;
        if (nDelta != 0) {
            m_baseStats.m_hitPoints += static_cast<SHORT>(nDelta) * 3;
            m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(nDelta) * 3;
        }
    }

    // CGAMESPRITE_FEAT_EXTRA_RAGE
    if ((m_baseStats.m_feats[0] & 0x100000) != 0) {
        nDelta = m_baseStats.m_featExtraRage - oldBaseStats.m_featExtraRage;
        if (nDelta != 0) {
            CResRef cResRage;
            if (m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_BARBARIAN) < 15
                || oldDStats.GetClassLevel(CAIOBJECTTYPE_C_BARBARIAN) > 14) {
                cResRage = SPIN236;
            } else {
                cResRage = SPIN260;
            }

            for (; nDelta > 0; nDelta--) {
                AddSpecialAbility(cResRage, FALSE);
            }
        }
    }

    // CGAMESPRITE_FEAT_EXTRA_SMITING
    if ((m_baseStats.m_feats[0] & 0x400000) != 0) {
        nDelta = m_baseStats.m_featExtraSmiting - oldBaseStats.m_featExtraSmiting;
        if (nDelta != 0) {
            for (; nDelta > 0; nDelta--) {
                AddSpecialAbility(SPIN152, FALSE);
            }
        }
    }

    // CGAMESPRITE_FEAT_EXTRA_TURNING
    if ((m_baseStats.m_feats[0] & 0x800000) != 0) {
        nDelta = m_baseStats.m_featExtraTurning - oldBaseStats.m_featExtraTurning;
        if (nDelta != 0) {
            for (; nDelta > 0; nDelta--) {
                AddSpecialAbility(SPIN970, FALSE);
            }
        }
    }
}

// NOTE: The original's body is empty (`ret 8`); its address is shared with
// CBaldurEngine::SetChatEditBoxStatus by identical code folding.
//
// 0x71E750
void CGameSprite::sub_71E750(CCreatureFileHeader& oldBaseStats, CDerivedStats& oldDStats)
{
}

// 0x71E760
void CGameSprite::sub_71E760(CDerivedStats& DStats, int a2)
{
    // TODO: Incomplete.
}

// 0x71F6E0
int CGameSprite::sub_71F6E0()
{
    int v1;

    switch (m_typeAI.m_nClass) {
    case CAIOBJECTTYPE_C_BARBARIAN:
    case CAIOBJECTTYPE_C_CLERIC:
    case CAIOBJECTTYPE_C_DRUID:
    case CAIOBJECTTYPE_C_FIGHTER:
    case CAIOBJECTTYPE_C_PALADIN:
    case CAIOBJECTTYPE_C_RANGER:
        v1 = 10;
        break;
    case CAIOBJECTTYPE_C_BARD:
    case CAIOBJECTTYPE_C_MONK:
    case CAIOBJECTTYPE_C_ROGUE:
        v1 = 15;
        break;
    default:
        v1 = 5;
        break;
    }

    switch (m_typeAI.m_nRace) {
    case CAIOBJECTTYPE_R_ELF:
        v1 += 20;
        break;
    case CAIOBJECTTYPE_R_DWARF:
        v1 += 10;
        break;
    case CAIOBJECTTYPE_R_HALFLING:
        v1 += 5;
        break;
    }

    if (m_nModalState == 2) {
        v1 = 100;
    }

    return v1;
}

// 0x71F760
const CString& CGameSprite::GetName()
{
    if (m_baseStats.m_name != -1) {
        STR_RES strRes;
        g_pBaldurChitin->GetTlkTable().Fetch(m_baseStats.m_name, strRes);
        m_sName = strRes.szText;
    }

    return m_sName;
}

// 0x71F820
STRREF CGameSprite::GetNameRef()
{
    STRREF strName = m_baseStats.m_name;

    if (strName == -1) {
        SHORT nSlot = g_pBaldurChitin->GetObjectGame()->GetCharacterSlotFromId(m_id);
        if (nSlot < 0) {
            nSlot = 0;
        }

        strName = -2 - nSlot;
    }

    return strName;
}

// 0x727720
BOOL CGameSprite::sub_727720(const CResRef& resRef, BYTE& nClass, DWORD& nSpecialization, BYTE& nLevel)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    BOOL bFound = FALSE;
    UINT nID;

    if (!pGame->m_spells.Find(resRef, nID)) {
        return FALSE;
    }

    UINT nMinLevel = 0;
    UINT nMaxLevel = CSPELLLIST_MAX_LEVELS;
    if (nLevel > 0 && nLevel <= CSPELLLIST_MAX_LEVELS) {
        nMinLevel = nLevel - 1;
        nMaxLevel = nLevel;
    }

    if (nSpecialization == 0) {
        UINT nClassIndex = 0;
        if (nClass > 0 && nClass <= CAIOBJECTTYPE_C_WIZARD) {
            nClassIndex = pGame->GetSpellcasterIndex(nClass);
        }

        for (; nClassIndex < CSPELLLIST_NUM_CLASSES && !bFound; nClassIndex++) {
            UINT nLevels = nMaxLevel < m_spells.Get(nClassIndex)->m_nHighestLevel
                ? nMaxLevel
                : m_spells.Get(nClassIndex)->m_nHighestLevel;

            for (UINT nCurLevel = nMinLevel; nCurLevel < nLevels && !bFound; nCurLevel++) {
                CGameSpriteSpellList* pSpells = m_spells.GetSpellsAtLevel(nClassIndex, nCurLevel);

                UINT nIndex = 0;
                for (auto it = pSpells->m_List.begin(); it != pSpells->m_List.end(); it++) {
                    if (it->m_nID == nID) {
                        if (m_spells.GetSpellsAtLevel(nClassIndex, nCurLevel)->Get(nIndex)->field_8 > 0) {
                            nClass = pGame->GetSpellcasterClass(nClassIndex);
                            nLevel = static_cast<BYTE>(nCurLevel + 1);
                            nSpecialization = 0;
                            bFound = TRUE;
                        }
                        break;
                    }
                    nIndex++;
                }
            }
        }
    }

    UINT nLevels = nMaxLevel < m_domainSpells.m_nHighestLevel
        ? nMaxLevel
        : m_domainSpells.m_nHighestLevel;

    for (UINT nCurLevel = nMinLevel; nCurLevel < nLevels && !bFound; nCurLevel++) {
        CGameSpriteSpellList* pSpells = m_domainSpells.GetSpellsAtLevel(nCurLevel);

        UINT nIndex = 0;
        for (auto it = pSpells->m_List.begin(); it != pSpells->m_List.end(); it++) {
            if (it->m_nID == nID) {
                if (m_domainSpells.GetSpellsAtLevel(nCurLevel)->Get(nIndex)->field_8 > 0) {
                    nClass = CAIOBJECTTYPE_C_CLERIC;
                    nLevel = static_cast<BYTE>(nCurLevel + 1);
                    nSpecialization = m_baseStats.m_specialization;
                    bFound = TRUE;
                }
                break;
            }
            nIndex++;
        }
    }

    return bFound;
}

// 0x727B40
BOOL CGameSprite::sub_727B40()
{
    CItem* pItem = m_equipment.m_items[CGameSpriteEquipment::SLOT_WEAPON + 2 * m_nWeaponSet];
    CItem* pSecondItem = m_equipment.m_items[CGameSpriteEquipment::SLOT_WEAPON + 2 * m_nWeaponSet + 1];
    if (pItem != NULL && ((pItem->GetFlagsFile() & 0x2) != 0 || pSecondItem != NULL)) {
        return FALSE;
    }
    return TRUE;
}

// NOTE: The name is the Ghidra project's: whether a caller sees this
// creature. Without seeing invisible, an invisible creature is hidden unless
// the low bit of its creature header's field_2FC is set.
//
// 0x727B80
BOOL CGameSprite::IsVisibleToCaller(BOOL bSeeInvisible)
{
    BOOL bVisible = TRUE;
    if (!bSeeInvisible
        && (m_derivedStats.m_generalState & STATE_INVISIBLE) != 0
        && (m_baseStats.field_2FC & 0x1) == 0) {
        bVisible = FALSE;
    }
    return bVisible;
}

// 0x728270
void CGameSprite::PlaySound(const CResRef& res)
{
    CSound cSound;

    CMessage* message = new CMessagePlaySoundRef(res, m_id, m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    cSound.SetResRef(res, TRUE, TRUE);

    if (cSound.m_nLooping == 0) {
        cSound.SetFireForget(TRUE);
    }

    cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
    cSound.Play(GetPos().x, GetPos().y, 0, FALSE);
}

// 0x728460
BOOL CGameSprite::Orderable(BOOL bIgnoreControl)
{
    if (!bIgnoreControl && !InControl()) {
        return FALSE;
    }

    if ((m_derivedStats.m_generalState & STATE_DEAD) != 0) {
        return FALSE;
    }

    if (m_nUnselectableCounter > 0) {
        return FALSE;
    }

    if (m_typeAI.GetEnemyAlly() > CAIObjectType::EA_CONTROLCUTOFF) {
        return FALSE;
    }

    if (m_moraleFailure) {
        return FALSE;
    }

    if ((m_derivedStats.m_generalState & STATE_STONE_DEATH) != 0
        || (m_derivedStats.m_generalState & STATE_FROZEN_DEATH) != 0
        || (m_derivedStats.m_generalState & STATE_HELPLESS) != 0
        || (m_derivedStats.m_generalState & STATE_STUNNED) != 0
        || (m_derivedStats.m_generalState & STATE_PANIC) != 0
        || ((m_derivedStats.m_generalState & STATE_BERSERK) != 0
            && m_berserkActive)
        || m_derivedStats.m_spellStates[SPLSTATE_ANIMAL_RAGE]
        || (m_derivedStats.m_generalState & STATE_SLEEPING) != 0
        || ((m_derivedStats.m_generalState & STATE_CHARMED) != 0
            && m_typeAI.GetEnemyAlly() != CAIObjectType::EA_0x847C3A
            && m_typeAI.GetEnemyAlly() != CAIObjectType::EA_CONTROLLED)
        || (m_derivedStats.m_generalState & STATE_FEEBLEMINDED) != 0
        || (m_derivedStats.m_generalState & STATE_CONFUSED) != 0) {
        return FALSE;
    }

    if (g_pBaldurChitin->GetObjectGame()->m_nTimeStop != 0
        && g_pBaldurChitin->GetObjectGame()->m_nTimeStopCaster != m_id) {
        return FALSE;
    }

    if (m_derivedStats.m_spellStates[SPLSTATE_179]) {
        return FALSE;
    }

    return TRUE;
}

// 0x728560
BOOL CGameSprite::Animate()
{
    return (~m_derivedStats.m_generalState >> 11) & 0x1;
}

// 0x728570
DWORD CGameSprite::GetSpecialization()
{
    return m_baseStats.m_specialization;
}

// 0x728B70
void CGameSprite::ProcessPendingTriggers()
{
    if ((m_derivedStats.m_generalState & STATE_PANIC) == 0
        && !m_inCutScene
        && !g_pBaldurChitin->GetObjectGame()->GetGameSave()->m_cutScene) {
        if (sub_7202E0() && (m_baseStats.m_flags & 0x80000000) == 0) {
            CGameAIBase::ProcessPendingTriggers();
        }
    }
}

// 0x728BC0
void CGameSprite::ResolveInstants(BOOL dropNonInstants)
{
    if (m_moveToFrontQueue > 0) {
        m_moveToFrontQueue--;
        MoveToFront();
    }

    if (m_moveToBackQueue > 0) {
        m_moveToBackQueue--;
        MoveToBack();
    }

    if (m_castCounter > -1) {
        m_castCounter++;
        if (m_castCounter >= 100 && !m_bInCasting) {
            m_castCounter = -1;
        }
    }

    if (m_attackFrame > -2) {
        m_attackFrame++;
        if (m_attackFrame >= 100) {
            m_attackFrame = -2;
        }
    }

    if (((m_derivedStats.m_generalState & STATE_DEAD) == 0
            && (m_derivedStats.m_generalState & STATE_SLEEPING) == 0)
        || ((g_pChitin->cNetwork.GetSessionOpen() != TRUE || m_curAction.GetActionID() != CAIACTION_LEAVEAREALUA)
            && g_pBaldurChitin->GetObjectGame()->GetRuleTables().m_lInstantActions.Find(m_curAction.GetActionID()) != NULL)) {
        m_typeAI.SetInstance(m_id);
        m_liveTypeAI.SetInstance(m_id);
        m_startTypeAI.SetInstance(m_id);

        if (Orderable(FALSE) && m_curAction.GetActionID() != 0) {
            g_pBaldurChitin->GetScreenWorld()->m_bored = FALSE;
            g_pBaldurChitin->GetScreenWorld()->m_boredCount = 0;
        }

        SHORT actionReturn = ExecuteAction();
        if (m_curAction.GetActionID() != CAIACTION_ATTACK) {
            m_lastActionID = m_curAction.GetActionID();
        }

        if (actionReturn == ACTION_DONE
            || actionReturn == ACTION_ERROR
            || actionReturn == ACTION_STOPPED) {
            SetCurrAction(GetNextAction(m_aiDoAction));
            if (m_curAction.GetActionID() == CAIAction::NO_ACTION) {
                m_curResponseNum = -1;
                m_curResponseSetNum = -1;
                m_curScriptNum = -1;
            }
        } else if (m_interrupt
            && actionReturn == ACTION_INTERRUPTABLE
            && (m_baseStats.m_flags & 0x80000000) == 0) {
            if (m_pPath != NULL) {
                CMessage* message = new CMessageDropPath(m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
            m_actionCount++;
            SetCurrAction(GetNextAction(m_aiDoAction));
            m_interrupt = FALSE;
        } else {
            m_actionCount++;
        }

        if (dropNonInstants
            && !m_sequenceTest
            && m_noActionCount > READY_COUNT
            && m_nSequence != SEQ_TWITCH
            && m_nSequence != SEQ_DIE
            && m_nSequence != SEQ_SLEEP
            && m_nSequence != SEQ_DAMAGE) {
            if (m_pPath != NULL || GetVertListType() == LIST_FLIGHT) {
                if (m_nSequence != SEQ_WALK) {
                    CMessage* message = new CMessageSetSequence(SEQ_WALK, m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
            } else {
                if (m_nSequence != GetIdleSequence()) {
                    CMessage* message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()), m_id, m_id);
                    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                }
            }
        }

        field_5582 = 0;
        field_9D14 = 0;
        field_9D15 = 0;
    }
}

// 0x728F80
SHORT CGameSprite::ExecuteAction()
{
    CGameObject* pTarget = NULL;
    BOOL bShare = FALSE;
    SHORT returnValue = ACTION_DONE;
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (m_curAction.m_actionID == 178) {
        // BreakInstants.
        SetCurrAction(GetNextAction(m_aiAction));
    }

    // NOTE: The original dispatches through the byte table 0x72B520 (ids 0
    // to 0x142) and the jump table 0x72B348; ids above 0x142 and those the
    // table sends to its case 0x75 go to `CGameAIBase::ExecuteAction`. The
    // cases below are grouped by the table's case; the address ranges are
    // the case's code up to the next case's.
    switch (m_curAction.m_actionID) {
    case 0:
        // NoAction (0x729013-0x729029).
        returnValue = ACTION_DONE;
        m_noActionCount++;
        goto done;
    case 1:
    case 36:
        // ActionOverride, Continue: the table's entry is the epilogue
        // (0x72B30B).
        goto done;
    case 3:
    case 98:
        // Attack, AttackNoSound
        // TODO: 0x7290DB-0x72920D not ported (CGameSprite_Attack 0x737EF0).
        returnValue = ACTION_ERROR;
        goto done;
    case 5:
        // BackStab (0x72920D-0x729234): the target is shared and released,
        // the action fails.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        returnValue = ACTION_ERROR;
        goto release;
    case 6:
        // (0x729234-0x72924A)
        // __FILE__: .\Include\ObjCreature.h
        // __LINE__: 1077
        UTIL_ASSERT(FALSE);
        returnValue = ACTION_NORMAL;
        goto done;
    case 8:
        // Dialogue (0x72924A-0x7293C4)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget != NULL) {
            if ((static_cast<CGameSprite*>(pTarget)->m_baseStats.m_generalState & STATE_DEAD) == 0
                && (static_cast<CGameSprite*>(pTarget)->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                bShare = TRUE;
                returnValue = Dialog(static_cast<CGameSprite*>(pTarget));
                goto release;
            }
            bShare = TRUE;
        }

        {
            // A dead (or missing) target: the first live party member is
            // talked to instead.
            SHORT nIndex = 0;
            if (pGame->GetNumCharacters() > 0) {
                do {
                    LONG nCharacterId = nIndex < pGame->GetNumCharacters()
                        ? pGame->GetCharacterId(nIndex)
                        : CGameObjectArray::INVALID_INDEX;

                    CGameObject* pObject;
                    BYTE rc;
                    do {
                        rc = pGame->GetObjectArray()->GetShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            &pObject,
                            INFINITE);
                    } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                    if (rc == CGameObjectArray::SUCCESS
                        && (static_cast<CGameSprite*>(pObject)->m_baseStats.m_generalState & STATE_DEAD) == 0
                        && (static_cast<CGameSprite*>(pObject)->m_derivedStats.m_generalState & STATE_DEAD) == 0) {
                        returnValue = Dialog(static_cast<CGameSprite*>(pObject));
                        pGame->GetObjectArray()->ReleaseShare(nCharacterId,
                            CGameObjectArray::THREAD_ASYNCH,
                            INFINITE);
                        break;
                    }

                    // NOTE: The original does not release a dead party
                    // member it shared (0x729363); kept.
                    nIndex++;
                } while (nIndex < pGame->GetNumCharacters());
            }

            if (nIndex == pGame->GetNumCharacters()) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 1066
                UTIL_ASSERT(FALSE);
                returnValue = ACTION_ERROR;
            }
        }
        goto release;
    case 9:
        // DropItem
        // TODO: 0x7293C4-0x72944E not ported (FindItemPersonal,
        // CGameSprite_DropItemFromBag 0x722740, DropItem 0x7618E0,
        // 0x754D70).
        returnValue = ACTION_ERROR;
        goto done;
    case 10:
        // Enemy (0x72944E-0x72945E)
        returnValue = Enemy();
        goto done;
    case 11:
        // EquipItem
        // TODO: 0x72945E-0x7294F6 not ported (CItem::Equip/Unequip).
        returnValue = ACTION_ERROR;
        goto done;
    case 12:
        // (0x7294F6-0x72951F)
        // __FILE__: .\Include\ObjCreature.h
        // __LINE__: 1137
        UTIL_ASSERT(FALSE);
        returnValue = ACTION_NORMAL;
        goto done;
    case 13:
        // FindTraps (0x72951F-0x72952F)
        returnValue = FindTraps();
        goto done;
    case 14:
        // GetItem (0x72952F-0x729538, shares the tail at 0x7295AD): the
        // target is shared and released, the action is done.
        pTarget = GetTargetShare();
        bShare = TRUE;
        returnValue = ACTION_DONE;
        goto release;
    case 15:
        // GiveItem
        // TODO: 0x729538-0x72955F not ported (0x757540).
        returnValue = ACTION_ERROR;
        goto done;
    case 17:
        // Help
        // TODO: 0x72955F-0x72956F not ported (0x74FF10).
        returnValue = ACTION_ERROR;
        goto done;
    case 18:
        // Hide
        // TODO: 0x72956F-0x72958D not ported (CGameSprite_Hide 0x757B40).
        returnValue = ACTION_ERROR;
        goto done;
    case 19:
        // JoinParty
        // TODO: 0x72958D-0x72959D not ported (0x750F10).
        returnValue = ACTION_ERROR;
        goto done;
    case 20:
        // LayHands (0x72959D-0x7295C5): the target is shared and released,
        // the action is done.
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        returnValue = ACTION_DONE;
        goto release;
    case 21:
        // LeaveParty (0x7295F5-0x729605)
        returnValue = LeaveParty();
        goto done;
    case 22:
        // MoveToObject (0x72B229-0x72B2E9)
        if (GetObjectType() != CGameObject::TYPE_SPRITE) {
            returnValue = ACTION_ERROR;
            goto done;
        }

        pTarget = GetTargetShare();
        if (pTarget == NULL) {
            goto done;
        }

        bShare = TRUE;
        if (pTarget->GetObjectType() == CGameObject::TYPE_DOOR) {
            // NOTE: The original passes an uninitialized stack point
            // (esp+0x14 at 0x72B2AB) to GetCloserDestPt, so which of the
            // door's two points it picks depends on the last use of that
            // stack slot (case 8's GetShare out-pointer, among others); ours
            // passes (0, 0).
            CPoint ptUnset(0, 0);
            CPoint ptDest = static_cast<CGameDoor*>(pTarget)->GetCloserDestPt(ptUnset);
            returnValue = MoveToPoint(ptDest, 0);
        } else if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
            if ((m_baseStats.m_flags & 0x100000) == 0
                && pTarget->GetAIType().m_nEnemyAlly <= CAIObjectType::EA_GOODCUTOFF) {
                m_baseStats.m_flags |= 0x100000;
            }
            returnValue = MoveToObject(pTarget);
            m_lastActionID = 22;
        } else {
            CPoint ptDest = pTarget->GetPos();
            returnValue = MoveToPoint(ptDest, 0);
        }
        goto release;
    case 23:
    case 90:
    case 207:
        // MoveToPoint, MoveToPointNoRecticle, MoveToPointNoInterrupt
        // (0x729029-0x729048)
        ResolveTargetPoint(&m_curAction, m_queuedActions.GetHeadPosition());
        returnValue = MoveToPoint();
        goto done;
    case 24:
        // Panic (0x729654-0x729664)
        returnValue = Panic();
        goto done;
    case 25:
        // PickPockets
        // TODO: 0x729664-0x72968C not ported (CGameSprite_PickPockets
        // 0x7586E0).
        returnValue = ACTION_ERROR;
        goto done;
    case 27:
        // ProtectPoint
        // TODO: 0x72968C-0x7296AB not ported (CGameSprite_ProtectPoint
        // 0x7496E0).
        returnValue = ACTION_ERROR;
        goto done;
    case 28:
        // RemoveTraps
        // TODO: 0x7296AB-0x7296D2 not ported (0x759240).
        returnValue = ACTION_ERROR;
        goto done;
    case 29:
    case 184:
        // RunAwayFrom, RunAwayFromNoInterrupt
        // TODO: 0x7296D2-0x7296FA not ported (0x744C00).
        returnValue = ACTION_ERROR;
        goto done;
    case 31:
    case 191:
        // Spell, SpellNoDec
        // TODO: 0x729A84-0x729AAC not ported (CGameSprite_Spell 0x740270).
        returnValue = ACTION_ERROR;
        goto done;
    case 33:
        // Turn (0x729AAC-0x729ABC)
        returnValue = Turn();
        goto done;
    case 34:
        // UseItem
        // TODO: 0x729ABC-0x729AE4 not ported (CGameSprite_UseItem
        // 0x746480).
        returnValue = ACTION_ERROR;
        goto done;
    case 38:
        // Swing (0x729AF4-0x729B0D)
        m_attackFrame = 0;
        returnValue = OneSwing();
        goto done;
    case 39:
        // Recoil (0x729B0D-0x729B1D)
        returnValue = Recoil();
        goto done;
    case 40:
        // PlayDead (0x729B1D-0x729B2D)
        returnValue = PlayDead();
        goto done;
    case 47:
        // Formation
        // TODO: 0x729B2D-0x729B4C not ported (0x745450).
        returnValue = ACTION_ERROR;
        goto done;
    case 48:
        // JumpToPoint (0x729B4C-0x729B6C)
        returnValue = JumpToPoint(m_curAction.m_dest, TRUE);
        goto done;
    case 69:
        // (0x729BDE-0x729C08)
        pGame->StepAnimation(static_cast<BYTE>(m_curAction.m_specificID + 0x30));
        returnValue = ACTION_DONE;
        goto done;
    case 84:
    case 206:
        // Face (0x729C08-0x729C18)
        returnValue = Face();
        goto done;
    case 85:
    case 130:
        // RandomWalk, RandomTurn
        // TODO: 0x7290BB-0x7290CB not ported (0x747F80).
        returnValue = ACTION_ERROR;
        goto done;
    case 86:
        // SetInterrupt (0x729CF3-0x729D26)
        if (m_curAction.m_specificID == 0) {
            m_baseStats.m_flags |= 0x80000000;
        } else {
            m_baseStats.m_flags &= ~0x80000000;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 87:
        // ProtectObject
        // TODO: 0x729D26-0x729D4E not ported (CGameSprite_ProtectObject
        // 0x749FC0).
        returnValue = ACTION_ERROR;
        goto done;
    case 88:
        // Leader (0x729C18-0x729CE3)
        m_followLeader = TRUE;
        m_followLeaderAdditive = m_curAction.m_specificID;
        {
            CAIAction action(CAIAction::MOVETOPOINT, m_curAction.m_dest, 0, -1);
            AddAction(action);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 89:
        // Follow
        // TODO: 0x729CE3-0x729CF3 not ported (0x74A7C0).
        returnValue = ACTION_ERROR;
        goto done;
    case 91:
        // LeaveArea
        // TODO: 0x7295D5-0x7295E5 not ported (0x74C0D0).
        returnValue = ACTION_ERROR;
        goto done;
    case 92:
        // SelectWeaponAbility (0x729D4E-0x729D77)
        SelectWeaponAbility(static_cast<BYTE>(m_curAction.m_specificID),
            static_cast<BYTE>(m_curAction.m_specificID2),
            TRUE,
            TRUE);
        returnValue = ACTION_DONE;
        goto done;
    case 93:
        // (0x7295E5-0x7295F5)
        // TODO: not ported (CGameSprite_LeaveAreaName 0x74CDB0).
        returnValue = ACTION_ERROR;
        goto done;
    case 94:
        // GroupAttack
        // TODO: 0x729E07-0x729E8F not ported (GetEnemyOf and
        // GetTargetShareAIType before GroupAttack).
        returnValue = ACTION_ERROR;
        goto done;
    case 95:
    case 192:
        // SpellPoint, SpellPointNoDec
        // TODO: 0x729E8F-0x729E9F not ported (0x742840).
        returnValue = ACTION_ERROR;
        goto done;
    case 96:
        // Rest
        // TODO: 0x729E9F-0x729EAF not ported (0x7500F0).
        returnValue = ACTION_ERROR;
        goto done;
    case 97:
        // UseItemPoint
        // TODO: 0x729AE4-0x729AF4 not ported (0x7473E0).
        returnValue = ACTION_ERROR;
        goto done;
    case 99:
        // (0x729EAF-0x729ED8)
        if (m_followStart) {
            m_followStart = FALSE;
            returnValue = ACTION_DONE;
            goto done;
        }
        returnValue = ACTION_INTERRUPTABLE;
        goto done;
    case 100:
        // RandomFly
        // TODO: 0x729ED8-0x729EE8 not ported (0x7526D0).
        returnValue = ACTION_ERROR;
        goto done;
    case 101:
        // TODO: 0x729048-0x7290BB not ported (SetTargetFly).
        returnValue = ACTION_ERROR;
        goto done;
    case 102:
    case 103:
    case 104:
        // MoraleSet, MoraleInc, MoraleDec
        // TODO: 0x729EE8-0x72A094 not ported (the morale effects).
        returnValue = ACTION_ERROR;
        goto done;
    case 105:
        // AttackOneRound
        // TODO: 0x72A094-0x72A0E7 not ported (CGameSprite_Attack).
        returnValue = ACTION_ERROR;
        goto done;
    case 107:
        // MoveToOffset (0x72A0E7-0x72A197)
        if (m_interrupt) {
            returnValue = ACTION_INTERRUPTABLE;
            goto done;
        }
        {
            CPoint ptDest(m_pos.x + CPathSearch::GRID_SQUARE_SIZEX * m_curAction.m_dest.x,
                m_pos.y + CPathSearch::GRID_SQUARE_SIZEY * m_curAction.m_dest.y);
            CAIAction action(CAIAction::MOVETOPOINT, ptDest, 0, -1);
            AddAction(action);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 108:
    case 176:
        // EscapeArea, EscapeAreaDestroy
        // TODO: 0x72A3F3-0x72A403 not ported (CGameSprite_EscapeArea
        // 0x75B280).
        returnValue = ACTION_ERROR;
        goto done;
    case 110:
        // LeaveAreaLUA
        // TODO: 0x7295C5-0x7295D5 not ported (0x74B340).
        returnValue = ACTION_ERROR;
        goto done;
    case 111:
        // DestroySelf (0x72A1A7-0x72A1C9)
        sub_761650();
        m_removeFromArea = TRUE;
        returnValue = ACTION_DONE;
        goto done;
    case 112:
        // UseContainer
        // TODO: 0x72A197-0x72A1A7 not ported (0x7555F0).
        returnValue = ACTION_ERROR;
        goto done;
    case 119:
        // DropInventory
        // TODO: 0x72A764-0x72A787 not ported (0x754EE0).
        returnValue = ACTION_ERROR;
        goto done;
    case 124:
        // Berserk
        // TODO: 0x72A3E3-0x72A3F3 not ported (0x75A4B0).
        returnValue = ACTION_ERROR;
        goto done;
    case 127:
        // CutSceneId (0x72AE43-0x72AE54)
        returnValue = ACTION_DONE;
        goto done;
    case 128:
    case 129:
        // AnkhegEmerge, AnkhegHide
        // TODO: 0x72A1C9-0x72A33E not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 134:
        // AttackReevaluate
        // TODO: 0x72A33E-0x72A34E not ported (0x738E70).
        returnValue = ACTION_ERROR;
        goto done;
    case 135:
        // LockScroll (0x72A34E-0x72A374)
        g_pBaldurChitin->m_pEngineWorld->m_scrollLockId = m_id;
        returnValue = ACTION_DONE;
        goto done;
    case 137:
    case 198:
        // StartDialog, StartDialogueNoSet
        // TODO: 0x72A374-0x72A39B not ported (0x7543B0).
        returnValue = ACTION_ERROR;
        goto done;
    case 138:
        // SetDialog (0x72A39B-0x72A3AB)
        returnValue = SetDialog();
        goto done;
    case 139:
        // PlayerDialog (0x72A3AB-0x72A3D3)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        returnValue = PlayerDialog(static_cast<CGameSprite*>(pTarget));
        goto release;
    case 142:
        // UseDoor (0x72A3D3-0x72A3E3)
        returnValue = UseDoor();
        goto done;
    case 145:
        // PickLock
        // TODO: 0x72A403-0x72A42B not ported (0x75BC70).
        returnValue = ACTION_ERROR;
        goto done;
    case 146:
        // Polymorph
        // TODO: 0x72A42B-0x72A4FE not ported (the polymorph effect).
        returnValue = ACTION_ERROR;
        goto done;
    case 147:
        // RemoveSpell
        // TODO: 0x72A4FE-0x72A50E not ported (0x75C980).
        returnValue = ACTION_ERROR;
        goto done;
    case 148:
        // BashDoor
        // TODO: 0x72A50E-0x72A536 not ported (0x75CBD0).
        returnValue = ACTION_ERROR;
        goto done;
    case 149:
        // EquipMostDamagingMelee (0x72A536-0x72A546)
        returnValue = EquipMostDamagingMelee();
        goto done;
    case 150:
        // StartStore
        // TODO: 0x72A546-0x72A56E not ported (0x75E630).
        returnValue = ACTION_ERROR;
        goto done;
    case 152:
        // ChangeAIType
        // TODO: 0x72A56E-0x72A5AC not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 166:
        // SetNumTimesTalkedTo (0x72A5AC-0x72A5DE)
        {
            CMessage* message = new CMessageSetNumTimesTalkedTo(m_curAction.m_specificID, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_DONE;
        goto done;
    case 168:
        // Interact
        // TODO: 0x72A5DE-0x72A606 not ported (0x754980).
        returnValue = ACTION_ERROR;
        goto done;
    case 174:
        // EquipRanged
        // TODO: 0x72AE54-0x72AE64 not ported (0x75E150).
        returnValue = ACTION_ERROR;
        goto done;
    case 175:
        // SetLeavePartyDialogFile
        // TODO: 0x72A606-0x72A747 not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 179:
        // DialogInterrupt (0x72A747-0x72A764)
        m_bAllowDialogInterrupt = static_cast<BOOLEAN>(m_curAction.m_specificID);
        returnValue = ACTION_DONE;
        goto done;
    case 180:
    case 208:
    case 263:
        // MoveToObjectFollow, MoveToObjectNoInterrupt, MoveToObjectUntilSee
        // (0x729605-0x72962C)
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = MoveToObject(pTarget);
        goto release;
    case 182:
        // MakeUnselectable (0x72A787-0x72A7A9)
        m_nUnselectableCounter = m_curAction.m_specificID * 15;
        returnValue = ACTION_DONE;
        goto done;
    case 194:
    case 219:
        // ChangeAnimation
        // TODO: 0x72A7A9-0x72A7B9 not ported (0x75EC50).
        returnValue = ACTION_ERROR;
        goto done;
    case 200:
        // RandomWalkContinuous
        // TODO: 0x7290CB-0x7290DB not ported (0x748CA0).
        returnValue = ACTION_ERROR;
        goto done;
    case 215:
        // TODO: 0x72962C-0x729654 not ported (0x73EA80).
        returnValue = ACTION_ERROR;
        goto done;
    case 221:
        // (0x72A7B9-0x72A7F6)
        field_72DE = m_curAction.m_specificID;
        if (field_72DE < 1) {
            field_72DE = 1;
        }
        if (field_72DE > 5) {
            field_72DE = 5;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 222:
        // (0x72A7F6-0x72A83D)
        field_72DE += m_curAction.m_specificID;
        if (field_72DE < 1) {
            field_72DE = 1;
        }
        if (field_72DE > 5) {
            field_72DE = 5;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 223:
        // TODO: 0x72A83D-0x72A976 not ported (the quick list check).
        returnValue = ACTION_ERROR;
        goto done;
    case 224:
        // TODO: 0x72A9A7-0x72A9B7 not ported (0x757720).
        returnValue = ACTION_ERROR;
        goto done;
    case 225:
        // TODO: 0x72A976-0x72A9A7 not ported (JumpToArea).
        returnValue = ACTION_ERROR;
        goto done;
    case 229:
        // FaceObject (0x72A9B7-0x72A9DF)
        pTarget = GetTargetShareType(CGameObject::TYPE_AIBASE);
        bShare = TRUE;
        returnValue = FaceObject(static_cast<CGameAIBase*>(pTarget));
        goto release;
    case 231:
        // WaitRandom (0x72A9DF-0x72AA2E): a Wait for a random time in the
        // range.
        m_curAction.m_actionID = CAIAction::WAIT;
        {
            LONG nMin = m_curAction.m_specificID2 < m_curAction.m_specificID
                ? m_curAction.m_specificID2
                : m_curAction.m_specificID;
            LONG nMax = m_curAction.m_specificID2 > m_curAction.m_specificID
                ? m_curAction.m_specificID2
                : m_curAction.m_specificID;
            LONG nRange = nMax - nMin + 1;
            m_curAction.m_specificID = nMin + (nRange != 0 ? rand() % nRange : 0);
        }
        returnValue = Wait();
        goto done;
    case 236:
        // SetSavedLocation (0x72AA2E-0x72AA61)
        m_baseStats.field_2E4 = static_cast<SHORT>(m_pos.x);
        m_baseStats.field_2E6 = static_cast<SHORT>(m_pos.y);
        m_baseStats.field_2E8 = static_cast<BYTE>(m_nDirection);
        returnValue = ACTION_DONE;
        goto done;
    case 237:
    case 262:
    case 302:
        // ReturnToSavedLocation, ReturnToSavedLocationDelete,
        // ReturnToStartLoc (0x72AA61-0x72AA71)
        returnValue = ReturnToStartLoc();
        goto done;
    case 239:
    case 240:
        // SetInternal, IncrementInternal
        // TODO: 0x72AAA8-0x72AB5C not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 249:
        // SetSavedLocationPoint (0x72AA71-0x72AAA8)
        m_baseStats.field_2E4 = static_cast<SHORT>(m_curAction.m_specificID);
        m_baseStats.field_2E6 = static_cast<SHORT>(m_curAction.m_specificID2);
        m_baseStats.field_2E8 = static_cast<BYTE>(m_curAction.m_specificID3);
        returnValue = ACTION_DONE;
        goto done;
    case 250:
        // RunAwayFromNoLeaveArea
        // TODO: 0x72AB5C-0x72AB84 not ported (0x760C50).
        returnValue = ACTION_ERROR;
        goto done;
    case 255:
        // SetBestWeapon
        // TODO: 0x72AB84-0x72ACB5 not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 256:
        // SetDialogueRange (0x72ACB5-0x72ACD1)
        field_7106 = m_curAction.m_specificID;
        returnValue = ACTION_DONE;
        goto done;
    case 257:
        // ForceHide (0x72ACD1-0x72AD07)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        if (pTarget == NULL) {
            returnValue = ACTION_ERROR;
            goto done;
        }
        // NOTE: The original never releases the target it shared here
        // (0x72ACED goes to the epilogue); kept.
        returnValue = ForceHide(static_cast<CGameSprite*>(pTarget));
        goto done;
    case 261:
        // TimedMoveToPoint (0x72AD07-0x72AD26)
        ResolveTargetPoint(&m_curAction, m_queuedActions.GetHeadPosition());
        returnValue = MoveToPoint();
        goto done;
    case 269:
        // SetStartPos (0x72AD26-0x72AD5D)
        {
            CPoint pt = m_curAction.m_dest;
            if (pt.x == -1 && pt.y == -1) {
                pt = m_pos;
            }
            m_posStart = pt;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 276:
        // PlayBardSong (0x72AD5D-0x72AD6D)
        returnValue = PlayBardSong();
        goto done;
    case 277:
        // TurnAMT
        // TODO: 0x72AD6D-0x72AE1F not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 299:
        // EquipWeapon
        // TODO: 0x72AE1F-0x72AE43 not ported (0x75E150).
        returnValue = ACTION_ERROR;
        goto done;
    case 300:
        // JumpToSavedLocation (0x729BA8-0x729BDE)
        SetDirection(m_baseStats.field_2E8);
        returnValue = JumpToPoint(CPoint(static_cast<WORD>(m_baseStats.field_2E4),
                                      static_cast<WORD>(m_baseStats.field_2E6)),
            TRUE);
        goto done;
    case 301:
        // SetExtendedNight
        // TODO: 0x72AE64-0x72AEE4 not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 309:
        // SetMarkedSpell
        // TODO: 0x7296FA-0x7297AC not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 310:
        // SetTeamBit (0x729A3C-0x729A84)
        if (m_curAction.m_specificID2 == 0) {
            field_58C &= ~m_curAction.m_specificID;
        } else {
            field_58C |= m_curAction.m_specificID;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 311:
        // SpellWait
        // TODO: 0x72AEE4-0x72B229 not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 317:
        // JumpToObject (0x729B6C-0x729BA8)
        pTarget = GetTargetShare();
        if (pTarget == NULL) {
            goto done;
        }
        bShare = TRUE;
        returnValue = JumpToPoint(pTarget->GetPos(), TRUE);
        goto release;
    case 319:
        // ForceMarkedSpell (0x7297AC-0x7297D3)
        if (field_588 != m_curAction.m_specificID) {
            field_588 = m_curAction.m_specificID;
        }
        returnValue = ACTION_DONE;
        goto done;
    case 321:
        // MarkSpellAndObject
        // TODO: 0x7297D3-0x729A3C not ported.
        returnValue = ACTION_ERROR;
        goto done;
    case 322:
        // MatchHP (0x729D77-0x729E07)
        pTarget = GetTargetShareType(CGameObject::TYPE_SPRITE);
        bShare = TRUE;
        if (pTarget != NULL) {
            CGameSprite* pSprite = static_cast<CGameSprite*>(pTarget);
            if (m_curAction.m_specificID == 0) {
                pSprite->m_baseStats.m_maxHitPointsBase = m_baseStats.m_maxHitPointsBase;
            } else if (m_baseStats.m_hitPoints <= static_cast<INT>(pSprite->m_baseStats.m_maxHitPointsBase)) {
                pSprite->m_baseStats.m_hitPoints = m_baseStats.m_hitPoints;
            } else {
                pSprite->m_baseStats.m_hitPoints = pSprite->m_baseStats.m_maxHitPointsBase;
            }
        }
        returnValue = ACTION_DONE;
        goto release;
    default:
        // The ids above 0x142 and those the byte table sends to case 0x75
        // (0x72B2E9-0x72B30B).
        returnValue = CGameAIBase::ExecuteAction();
        goto done;
    }

release:
    if (pTarget != NULL) {
        if (bShare) {
            pGame->GetObjectArray()->ReleaseShare(pTarget->GetId(),
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            pGame->GetObjectArray()->ReleaseDeny(pTarget->GetId(),
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

done:
    SetLastActionReturn(returnValue);
    return returnValue;
}

// 0x72B670
void CGameSprite::ResolvePausedAction(const CAIAction* curAction, POSITION pos)
{
    CGameObject* pObject;
    BYTE rc;

    if (m_groupMove) {
        return;
    }

    ResolveTargetPoint(curAction, pos);

    if (curAction->GetActionID() == 23
        && m_targetId != CGameObjectArray::INVALID_INDEX) {
        if (m_bSelected && Orderable(FALSE)) {
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                return;
            }

            if (pObject->GetObjectType() == TYPE_SPRITE) {
                static_cast<CGameSprite*>(pObject)->m_marker.SetType(CMarker::ELLIPSE);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
        m_targetId = CGameObjectArray::INVALID_INDEX;
    } else if (curAction->GetActionID() == 27
        && m_targetId != CGameObjectArray::INVALID_INDEX) {
        if (m_bSelected && Orderable(FALSE)) {
            do {
                rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                return;
            }

            if (pObject->GetObjectType() == TYPE_SPRITE) {
                static_cast<CGameSprite*>(pObject)->m_marker.SetType(CMarker::ELLIPSE);
            }

            g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
        m_targetId = CGameObjectArray::INVALID_INDEX;
    }
}

// NOTE: Assembly is tail-call optimized.
//
// 0x72B870
void CGameSprite::ResolveTargetPoint(const CAIAction* curAction, POSITION pos)
{
    CAIAction* workAction;

    switch (curAction->GetActionID()) {
    case 23:
    case 88:
        if (pos != NULL) {
            workAction = m_queuedActions.GetNext(pos);
            switch (workAction->GetActionID()) {
            case 23:
            case 88:
                ResolveTargetPoint(workAction, pos);
                break;
            default:
                if (!m_groupMove) {
                    m_targetPoint = curAction->m_dest;
                }
                break;
            }
        } else {
            m_targetPoint = curAction->m_dest;
        }
        break;
    case 27:
        m_targetPoint = curAction->m_dest;
        break;
    case 83:
    case 84:
        if (pos != NULL) {
            workAction = m_queuedActions.GetNext(pos);
            ResolveTargetPoint(workAction, pos);
        } else {
            m_targetPoint.x = -1;
            m_targetPoint.y = -1;
        }
        break;
    default:
        m_targetPoint.x = -1;
        m_targetPoint.y = -1;
        break;
    }
}

// 0x72B9A0
void CGameSprite::ProcessAI()
{
    // NOTE: A function-local static in the original (0x8F9928, built on the
    // first call).
    static CAIAction action;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    BOOL bInstant;

    if (g_pBaldurChitin->m_pEngineWorld->m_bPaused == TRUE
        && g_pChitin->cNetwork.m_bIsHost == TRUE
        && pGame->GetGameSave()->m_cutScene == TRUE) {
        g_pBaldurChitin->m_pEngineWorld->TogglePauseGame(0, 1, 0);
    }

    if (pGame->GetGameSave()->field_1AC
        && m_pArea != NULL
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != m_remotePlayerID
        && m_pArea->m_visibility.IsCharacterIdOnMap(m_id)) {
        m_pArea->m_visibility.RemoveCharacter(m_pos, m_id, m_visibleTerrainTable);
    }

    if (pGame->GetGameSave()->field_1AC
        && g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != m_remotePlayerID
        && m_typeAI.m_nEnemyAlly <= CAIOBJECTTYPE_EA_GOODCUTOFF) {
        m_typeAI.m_nEnemyAlly = CAIOBJECTTYPE_EA_ARENAPC;
        m_liveTypeAI.m_nEnemyAlly = CAIOBJECTTYPE_EA_ARENAPC;
        m_startTypeAI.m_nEnemyAlly = CAIOBJECTTYPE_EA_ARENAPC;
    }

    if (m_userCommandPause > 0) {
        m_baseStats.m_flags &= ~0x80000000;
    }

    if (field_72A0 > 0) {
        field_72A0--;
    }

    // The move scale of game mode 0x142 (0x72BB64-0x72C0CD): the animation
    // walks at 6 while the mode is on, and gets its old scale back after.
    // The step towards the destination is computed again each time (the
    // same code three times in the original).
    LONG nDistance;
    if (pGame->GetGameSave()->m_mode == 0x142) {
        if (field_72AA) {
            if (pGame->GetGameSave()->m_mode == 0x142 && m_animation.GetMoveScale() != 6) {
                field_72AA = TRUE;
                m_animation.SetMoveScale(6);

                nDistance = static_cast<LONG>(sqrt(static_cast<double>((m_posDest.y - m_pos.y) * (m_posDest.y - m_pos.y) * 16 / 9
                    + (m_posDest.x - m_pos.x) * (m_posDest.x - m_pos.x))));
                if (nDistance != 0) {
                    m_posDelta.x = static_cast<LONG>((m_posDest.x - m_pos.x) * 1024 * static_cast<UINT>(m_animation.GetMoveScale())) / nDistance;
                    m_posDelta.y = static_cast<LONG>((m_posDest.y - m_pos.y) * 4 / 3 * 1024 * static_cast<UINT>(m_animation.GetMoveScale())) / nDistance;
                } else {
                    m_posDelta.x = 0;
                    m_posDelta.y = 0;
                }
            }
        } else {
            field_72A8 = m_animation.GetMoveScale();
            field_72AA = TRUE;
            m_animation.SetMoveScale(6);

            if (field_72A8 != 6) {
                nDistance = static_cast<LONG>(sqrt(static_cast<double>((m_posDest.y - m_pos.y) * (m_posDest.y - m_pos.y) * 16 / 9
                    + (m_posDest.x - m_pos.x) * (m_posDest.x - m_pos.x))));
                if (nDistance != 0) {
                    m_posDelta.x = static_cast<LONG>((m_posDest.x - m_pos.x) * 1024 * static_cast<UINT>(m_animation.GetMoveScale())) / nDistance;
                    m_posDelta.y = static_cast<LONG>((m_posDest.y - m_pos.y) * 4 / 3 * 1024 * static_cast<UINT>(m_animation.GetMoveScale())) / nDistance;
                } else {
                    m_posDelta.x = 0;
                    m_posDelta.y = 0;
                }
            }
        }
    } else if (field_72AA) {
        field_72AA = FALSE;
        m_animation.SetMoveScale(field_72A8);
        field_562C = TRUE;

        if (field_72A8 != 6) {
            nDistance = static_cast<LONG>(sqrt(static_cast<double>((m_posDest.y - m_pos.y) * (m_posDest.y - m_pos.y) * 16 / 9
                + (m_posDest.x - m_pos.x) * (m_posDest.x - m_pos.x))));
            if (nDistance != 0) {
                m_posDelta.x = static_cast<LONG>((m_posDest.x - m_pos.x) * 1024 * static_cast<UINT>(m_animation.GetMoveScale())) / nDistance;
                m_posDelta.y = static_cast<LONG>((m_posDest.y - m_pos.y) * 4 / 3 * 1024 * static_cast<UINT>(m_animation.GetMoveScale())) / nDistance;
            } else {
                m_posDelta.x = 0;
                m_posDelta.y = 0;
            }
        }
    }

    m_randValue = rand() % 0x7FFF;

    SHORT nSequence = m_nSequence;
    if ((m_derivedStats.m_generalState & STATE_DEAD) != 0
        && nSequence != SEQ_DIE
        && nSequence != SEQ_TWITCH
        && nSequence != SEQ_SLEEP) {
        SetSequence(SEQ_TWITCH);
    }

    if (m_pArea == NULL && (m_derivedStats.m_generalState & STATE_DEAD) != 0) {
        if (!ProcessEffectList()) {
            return;
        }

        bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
        if ((g_pChitin->cNetwork.m_bConnectionEstablished != TRUE || m_curAction.m_actionID != 110)
            && bInstant) {
            while (1) {
                ResolveInstants(FALSE);
                bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
                if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                    && m_curAction.m_actionID == 110) {
                    break;
                }
                if (!bInstant) {
                    return;
                }
            }
            return;
        }

        SetCurrAction(GetNextAction(action));
        m_interrupt = FALSE;
        return;
    }

    if (m_pPath == NULL && nSequence == SEQ_WALK) {
        if (GetVertListType() != CGAMEOBJECT_LIST_FLIGHT) {
            SetIdleSequence();
        }
    }

    if (m_bSelected) {
        if (!Orderable(FALSE)) {
            Unselect();
            pGame->SelectToolbar();
        }
    }

    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != m_remotePlayerID) {
        return;
    }

    if (m_bEscapingArea == TRUE
        || m_curAction.m_actionID == 108
        || m_curAction.m_actionID == 176
        || m_currentActionId == 108
        || m_currentActionId == 176) {
        m_dialogWait = 0;
    }

    SHORT nActionID = m_curAction.m_actionID;
    if (nActionID == 137
        || nActionID == 8
        || nActionID == 198
        || m_currentActionId == 137
        || m_currentActionId == 8
        || m_currentActionId == 198) {
        m_dialogWait = 0;
    }

    if (nActionID == 207
        || nActionID == 208
        || m_currentActionId == 207
        || m_currentActionId == 208) {
        m_dialogWait = 0;
    }

    if (!sub_7202E0()) {
        m_dialogWait = 0;
    }

    if (m_dialogWait > 0 && m_pendingTriggers.GetHeadPosition() != NULL) {
        SHORT nTriggerID = m_pendingTriggers.GetHead()->m_triggerID;
        if (nTriggerID == CAITRIGGER_ATTACKEDBY || nTriggerID == CAITRIGGER_HITBY) {
            m_dialogWait = 0;
        }
    }

    if (m_curAction.m_actionID == 27) {
        if (GetAIType().m_nEnemyAlly >= CAIOBJECTTYPE_EA_EVILCUTOFF) {
            ClearActions(FALSE);

            CMessage* message = new CMessageDropPath(m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            message = new CMessageAddAction(CAIAction(CAIAction::NO_ACTION, CPoint(-1, -1), 0, -1),
                m_id,
                m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
        if (m_queuedActions.GetCount() != 0) {
            SetCurrAction(GetNextAction(action));
            m_interrupt = FALSE;
        }

        if (m_curAction.m_actionID == CAIAction::NO_ACTION) {
            m_inCutScene = FALSE;
            m_nCommandPause = 0;
        }
    }

    if (((pGame->GetCharacterPortraitNum(m_id) != -1 && static_cast<DWORD>(field_44A) % 16 == static_cast<DWORD>(m_id) % 16)
            || static_cast<DWORD>(field_44A) % 0x800 == static_cast<DWORD>(m_id) % 0x800)
        && (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
            || g_pChitin->cNetwork.m_idLocalPlayer == m_remotePlayerID)) {
        if (pGame->GetCharacterPortraitNum(m_id) != -1 && m_nSequence != SEQ_WALK) {
            JumpToPoint(m_pos, FALSE);
        }

        CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (pGame->GetCharacterPortraitNum(m_id) != -1
        && static_cast<DWORD>(field_44A) % 0x803 == static_cast<DWORD>(m_id) % 0x803) {
        sub_71D0B0();
    }

    if (pGame->GetWorldTimer()->m_active && m_nUnselectableCounter > 0) {
        m_nUnselectableCounter--;
    }

    if (!pGame->GetWorldTimer()->m_active) {
        // The world is paused: only the paused action's target and the
        // instant actions (0x72DD1B).
        if (!m_interrupt
            || (m_baseStats.m_flags & 0x80000000) != 0
            || (m_userCommandPause < 1 && m_nCommandPause < 1)) {
            ResolvePausedAction(&m_curAction, m_queuedActions.GetHeadPosition());
        } else {
            POSITION pos = m_queuedActions.GetHeadPosition();
            if (pos != NULL) {
                CAIAction* pAction = m_queuedActions.GetNext(pos);
                ResolvePausedAction(pAction, pos);
            }
        }

        if (m_curAction.m_actionID == CAIAction::NO_ACTION && m_queuedActions.GetCount() != 0) {
            SetCurrAction(GetNextAction(action));
            m_interrupt = FALSE;
        }

        if (m_interrupt && (m_baseStats.m_flags & 0x80000000) == 0) {
            SetCurrAction(GetNextAction(action));
            m_interrupt = FALSE;
        }

        bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
        if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
            && m_curAction.m_actionID == 110) {
            return;
        }

        if (bInstant) {
            ResolveInstants(FALSE);
        }
        return;
    }

    if (!m_active || !m_activeAI || !m_activeImprisonment) {
        if (static_cast<DWORD>(field_44A) % 16 == static_cast<DWORD>(m_id) % 16
            && m_userCommandPause < 1
            && m_nCommandPause < 1
            && (field_3428 & 0x7) == 0
            && ((m_derivedStats.m_generalState & STATE_DEAD) == 0 || field_558E)) {
            field_54C++;
            field_558E = TRUE;

            BOOL bProcess = TRUE;
            if (m_curAction.m_actionID != CAIAction::NO_ACTION
                && m_pendingTriggers.GetCount() == 0
                && field_550 < 120
                && field_552 <= 0) {
                CRect rView(m_pArea->GetInfinity()->rViewPort);
                INT x;
                INT y;
                m_pArea->GetInfinity()->GetViewPosition(x, y);
                rView.OffsetRect(x, y);
                if (!rView.PtInRect(m_pos)) {
                    field_550++;
                    bProcess = FALSE;
                }
            }

            if (bProcess) {
                if ((m_derivedStats.m_generalState & STATE_DEAD) != 0) {
                    field_558E = FALSE;
                }

                if (m_pendingTriggers.GetCount() != 0) {
                    field_552 = 5;
                }

                ProcessPendingTriggers();

                if (field_552 > 0) {
                    field_552--;
                }

                field_550 = 0;
            }

            m_userCommandPause = 0;
            m_nCommandPause = 0;
        }

        if (m_curAction.m_actionID == CAIAction::NO_ACTION && m_queuedActions.GetCount() != 0) {
            SetCurrAction(GetNextAction(action));
            m_interrupt = FALSE;
        }

        bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
        if ((g_pChitin->cNetwork.m_bConnectionEstablished != TRUE || m_curAction.m_actionID != 110)
            && bInstant) {
            while (1) {
                ResolveInstants(FALSE);
                bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
                if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                    && m_curAction.m_actionID == 110) {
                    break;
                }
                if (!bInstant) {
                    return;
                }
            }
            return;
        }

        SetCurrAction(GetNextAction(action));
        m_interrupt = FALSE;
        return;
    }

    m_userCommandPause--;
    m_nCommandPause--;
    field_44A++;

    if ((m_derivedStats.m_generalState & STATE_DEAD) == 0 && !sub_71CFF0()) {
        // The morale broke (0x72CA52-0x72CC41).
        if (!m_moraleFailure) {
            m_moraleFailure = TRUE;

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == m_remotePlayerID) {
                CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            PlaySound(1, TRUE, FALSE, FALSE);

            BYTE nRoll = static_cast<BYTE>(rand() % 100);
            if (nRoll < 10) {
                FeedBack(18, 0, 0, 0, -1, 0, 0);

                // Panic()
                action.m_actionID = 24;
                action.m_specificID = 0;
                action.m_specificID2 = -1;
                action.m_dest.x = -1;
                action.m_dest.y = -1;
                action.m_specificID3 = 0;
                action.m_string1 = "";
                action.m_string2 = "";
                action.m_internalFlags = 0;

                m_baseStats.m_generalState |= STATE_PANIC;
            } else if (nRoll < 90) {
                FeedBack(19, 0, 0, 0, -1, 0, 0);

                // RunAwayFromNoInterrupt()
                action.m_actionID = 184;
                action.m_specificID = 0x7FFFFFFF;
                action.m_specificID2 = 0;
                action.m_dest.x = -1;
                action.m_dest.y = -1;
                action.m_specificID3 = 0;
                action.m_string1 = "";
                action.m_string2 = "";
                action.m_internalFlags = 0;

                m_baseStats.m_generalState |= STATE_PANIC;
            } else {
                FeedBack(20, 0, 0, 0, -1, 0, 0);

                m_berserkActive = TRUE;

                // Berserk()
                action.m_actionID = 124;
                action.m_specificID = 0x7FFFFFFF;
                action.m_dest.x = -1;
                action.m_dest.y = -1;
                action.m_specificID2 = 0;
                action.m_specificID3 = 0;
                action.m_string1 = "";
                action.m_string2 = "";
                action.m_internalFlags = 0;
            }

            ClearActions(FALSE);
            m_interrupt = TRUE;
            AddAction(action);
        }
    } else if (m_dialogWait > 0
        && m_typeAI.m_nEnemyAlly < CAIOBJECTTYPE_EA_EVILCUTOFF
        && (m_derivedStats.m_generalState & STATE_SLEEPING) == 0
        && m_bAllowDialogInterrupt
        && !m_derivedStats.m_bIgnoreDialogPause) {
        // Waiting for a dialog (0x72CC46-0x72CE7C).
        if (m_moraleFailure) {
            m_moraleFailure = FALSE;

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == m_remotePlayerID) {
                CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            ClearActions(FALSE);

            CMessage* message = new CMessageDropPath(m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            SetCurrAction(GetNextAction(action));

            m_baseStats.m_generalState &= ~STATE_PANIC;
            m_derivedStats.m_generalState &= ~STATE_PANIC;
        }

        nSequence = m_nSequence;
        m_dialogWait--;

        if (nSequence != GetIdleSequence()
            && m_nSequence != SEQ_DIE
            && m_nSequence != SEQ_SLEEP
            && m_nSequence != SEQ_TWITCH) {
            CMessage* message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()), m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        CGameObject* pObject;
        if (pGame->GetObjectArray()->GetShare(m_dialogWaitTarget,
                CGameObjectArray::THREAD_ASYNCH,
                &pObject,
                INFINITE)
            == CGameObjectArray::SUCCESS) {
            if (m_nDirection != GetDirection(pObject->GetPos())
                && m_nSequence != SEQ_SLEEP) {
                CMessage* message = new CMessageSetDirection(pObject->GetPos(), m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            pGame->GetObjectArray()->ReleaseShare(m_dialogWaitTarget,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        if (m_pPath != NULL) {
            CMessage* message = new CMessageDropPath(m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        // 0x72CE81: the morale holds (or the sprite is dead) and no dialog
        // holds it.
        if (m_moraleFailure) {
            m_moraleFailure = FALSE;

            if (g_pChitin->cNetwork.GetServiceProvider() == CNetwork::SERV_PROV_NULL
                || g_pChitin->cNetwork.m_idLocalPlayer == m_remotePlayerID) {
                CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            ClearActions(FALSE);

            CMessage* message = new CMessageDropPath(m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

            SetCurrAction(GetNextAction(action));

            m_baseStats.m_generalState &= ~STATE_PANIC;
            m_derivedStats.m_generalState &= ~STATE_PANIC;
        }

        if (((m_id ^ field_44A) & 0xF) == 0) {
            CheckTimers(1);
        }

        if ((m_derivedStats.m_generalState & STATE_SLEEPING) != 0
            && m_nSequence != SEQ_TWITCH
            && m_nSequence != SEQ_DIE
            && m_nSequence != SEQ_SLEEP) {
            CMessage* message = new CMessageSetSequence(SEQ_SLEEP, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }

        // 0: the scripts run (0x72D816); 1: the state's action replaces the
        // current one, then field_727E and ApplyTriggers (0x72D1B4); 2: only
        // field_727E and ApplyTriggers (0x72D1D5).
        INT nState = 0;

        if (sub_7202E0()) {
            DWORD dwState = m_derivedStats.m_generalState;
            if ((dwState & STATE_HELPLESS) != 0) {
                if (m_curAction.m_actionID != CAIAction::NO_ACTION) {
                    action = CAIAction::NULL_ACTION;
                    nState = 1;
                } else {
                    nState = 2;
                }
            } else if ((dwState & STATE_BERSERK) != 0 && m_berserkActive) {
                sub_72FBF0();

                if (m_curAction.m_actionID != 3 && m_curAction.m_actionID != 124) {
                    // Berserk()
                    action.m_actionID = 124;
                    action.m_specificID = 0x7FFFFFFF;
                    action.m_dest.x = -1;
                    action.m_dest.y = -1;
                    action.m_specificID2 = 0;
                    action.m_specificID3 = 0;
                    action.m_string1 = "";
                    action.m_string2 = "";
                    action.m_internalFlags = 0;
                    nState = 1;
                } else {
                    nState = 2;
                }
            } else if ((dwState & STATE_FEEBLEMINDED) != 0) {
                if (m_curAction.m_actionID != CAIAction::NO_ACTION) {
                    action = CAIAction::NULL_ACTION;
                    nState = 1;
                } else {
                    nState = 2;
                }
            } else if ((dwState & STATE_CONFUSED) != 0) {
                if (static_cast<DWORD>(field_44A) % 100 == static_cast<DWORD>(m_id) % 100) {
                    switch (rand() % 10) {
                    case 0:
                    case 1:
                    case 2:
                        // RunAwayFromNoInterrupt()
                        action.m_actionID = 184;
                        action.m_specificID = 0x7FFFFFFF;
                        action.m_dest.x = -1;
                        action.m_dest.y = -1;
                        action.m_specificID2 = 0;
                        action.m_specificID3 = 0;
                        action.m_string1 = "";
                        action.m_string2 = "";
                        action.m_internalFlags = 0;
                        break;
                    case 3:
                    case 4:
                    case 5:
                        action = CAIAction::NULL_ACTION;
                        break;
                    case 6:
                    case 7:
                    case 8:
                    case 9:
                        // GroupAttack([ANYONE])
                        action.m_actionID = CAIAction::GROUPATTACK;
                        action.m_acteeID.Set(CAIObjectType::ANYONE);
                        action.m_specificID = 0;
                        action.m_specificID2 = 0;
                        action.m_specificID3 = 0;
                        action.m_string1 = "";
                        action.m_string2 = "";
                        action.m_internalFlags = 0;
                        break;
                    }
                    nState = 1;
                } else {
                    nState = 2;
                }
            } else if (m_derivedStats.m_spellStates.test(SPLSTATE_ANIMAL_RAGE)) {
                nState = 2;

                if (m_curAction.m_actionID != 149) {
                    if (m_typeAI.m_nEnemyAlly < CAIOBJECTTYPE_EA_EVILCUTOFF) {
                        if (m_typeAI.m_nEnemyAlly <= CAIOBJECTTYPE_EA_GOODCUTOFF
                            && (m_curAction.m_actionID != 134 || m_curAction.m_acteeID.m_nEnemyAlly < CAIOBJECTTYPE_EA_EVILCUTOFF)) {
                            // AttackReevaluate([EVILCUTOFF], 45)
                            action.m_actionID = 134;
                            action.m_specificID = 0;
                            action.m_dest.x = -1;
                            action.m_dest.y = -1;
                            action.m_specificID2 = -1;
                            action.m_specificID3 = 0;
                            action.m_string1 = "";
                            action.m_string2 = "";
                            action.m_internalFlags = 0;

                            CAIObjectType type;
                            type.m_nEnemyAlly = CAIOBJECTTYPE_EA_EVILCUTOFF;
                            action.m_acteeID.Set(type);
                            action.m_specificID = 45;

                            ClearActions(FALSE);
                            DropPath();
                            m_interrupt = TRUE;
                            AddAction(action);
                        }
                    } else if (m_curAction.m_actionID != 134 || m_curAction.m_acteeID.m_nEnemyAlly > CAIOBJECTTYPE_EA_GOODCUTOFF) {
                        // AttackReevaluate([GOODCUTOFF], 45)
                        action.m_actionID = 134;
                        action.m_specificID = 0;
                        action.m_dest.x = -1;
                        action.m_dest.y = -1;
                        action.m_specificID2 = -1;
                        action.m_specificID3 = 0;
                        action.m_string1 = "";
                        action.m_string2 = "";
                        action.m_internalFlags = 0;

                        CAIObjectType type;
                        type.m_nEnemyAlly = CAIOBJECTTYPE_EA_GOODCUTOFF;
                        action.m_acteeID.Set(type);
                        action.m_specificID = 45;

                        ClearActions(FALSE);
                        DropPath();
                        m_interrupt = TRUE;
                        AddAction(action);
                    }
                }
            } else if (m_derivedStats.m_spellStates.test(SPLSTATE_179)) {
                if (m_curAction.m_actionID != CAIAction::NO_ACTION) {
                    action = CAIAction::NULL_ACTION;
                    nState = 1;
                } else {
                    nState = 2;
                }
            } else if ((dwState & STATE_CHARMED) != 0
                && m_startTypeAI.m_nEnemyAlly <= CAIOBJECTTYPE_EA_GOODCUTOFF
                && m_typeAI.m_nEnemyAlly == 254) {
                if (m_curAction.m_actionID != CAIAction::NO_ACTION) {
                    action = CAIAction::NULL_ACTION;
                    nState = 1;
                } else {
                    nState = 2;
                }
            }
        }

        if (nState == 0) {
            // 0x72D816
            if (field_727E) {
                field_727E = FALSE;

                if (m_curAction.m_actionID != CAIAction::NO_ACTION) {
                    action = CAIAction::NULL_ACTION;

                    ClearActions(FALSE);
                    DropPath();
                    m_interrupt = TRUE;
                    AddAction(action);
                }

                ApplyTriggers();
            } else {
                sub_72FBF0();

                CheckModal();

                if (m_userCommandPause > 0 || m_nCommandPause > 0) {
                    ClearTriggers();
                }

                if (static_cast<DWORD>(field_44A) % 16 == static_cast<DWORD>(m_id) % 16
                    && m_userCommandPause < 1
                    && m_nCommandPause < 1
                    && (field_3428 & 0x7) == 0
                    && ((m_derivedStats.m_generalState & STATE_DEAD) == 0 || field_558E)) {
                    field_54C++;
                    field_558E = TRUE;

                    BOOL bProcess = TRUE;
                    if (m_curAction.m_actionID != CAIAction::NO_ACTION
                        && m_pendingTriggers.GetCount() == 0
                        && field_550 < 120
                        && field_552 <= 0) {
                        CRect rView(m_pArea->GetInfinity()->rViewPort);
                        INT x;
                        INT y;
                        m_pArea->GetInfinity()->GetViewPosition(x, y);
                        rView.OffsetRect(x, y);
                        if (!rView.PtInRect(m_pos)) {
                            field_550++;
                            bProcess = FALSE;
                        }
                    }

                    if (bProcess) {
                        if ((m_derivedStats.m_generalState & STATE_DEAD) != 0) {
                            field_558E = FALSE;
                        }

                        if (m_pendingTriggers.GetCount() != 0) {
                            field_552 = 5;
                        }

                        ProcessPendingTriggers();

                        if (field_552 > 0) {
                            field_552--;
                        }

                        field_550 = 0;
                    }

                    m_userCommandPause = 0;
                    m_nCommandPause = 0;
                }

                if (m_curAction.m_actionID == CAIAction::NO_ACTION && m_queuedActions.GetCount() != 0) {
                    SetCurrAction(GetNextAction(action));
                    m_interrupt = FALSE;
                }
            }
        } else {
            if (nState == 1) {
                ClearActions(FALSE);
                DropPath();
                m_interrupt = TRUE;
                AddAction(action);
            }

            field_727E = TRUE;
            ApplyTriggers();
        }
    }

    // 0x72DAC5
    if (!ProcessEffectList()) {
        return;
    }

    if (m_posVertList == NULL) {
        return;
    }

    if (m_dialogWait > 0
        && m_typeAI.m_nEnemyAlly < CAIOBJECTTYPE_EA_EVILCUTOFF
        && m_bAllowDialogInterrupt
        && !m_derivedStats.m_bIgnoreDialogPause) {
        return;
    }

    if ((m_derivedStats.m_generalState & STATE_DEAD) == 0) {
        if (!sub_7245D0()
            && !m_derivedStats.m_bCasterHold
            && (m_derivedStats.m_generalState & (STATE_STONE_DEATH | STATE_FROZEN_DEATH)) == 0
            && (pGame->m_nTimeStop == 0 || pGame->m_nTimeStopCaster == m_id)) {
            do {
                ResolveInstants(TRUE);
                bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
                if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                    && m_curAction.m_actionID == 110) {
                    return;
                }
            } while (bInstant);
        } else {
            bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
            if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                && m_curAction.m_actionID == 110
                && !sub_7245D0()) {
                return;
            }

            do {
                if (!bInstant) {
                    return;
                }
                ResolveInstants(FALSE);
                bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
            } while (g_pChitin->cNetwork.m_bConnectionEstablished != TRUE
                || m_curAction.m_actionID != 110);
        }
    } else {
        bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
        if ((g_pChitin->cNetwork.m_bConnectionEstablished == TRUE && m_curAction.m_actionID == 110)
            || !bInstant) {
            SetCurrAction(GetNextAction(action));
            m_interrupt = FALSE;
        } else {
            do {
                ResolveInstants(FALSE);
                bInstant = pGame->GetRuleTables().m_lInstantActions.Find(m_curAction.m_actionID) != NULL;
                if (g_pChitin->cNetwork.m_bConnectionEstablished == TRUE
                    && m_curAction.m_actionID == 110) {
                    return;
                }
            } while (bInstant);
        }
    }
}

// NOTE: placed before 0x72DE60, out of address order, to keep clear of the
// movement stream's insertion after ProcessEffectList; move after the wave.
// 0x72FBF0
void CGameSprite::sub_72FBF0()
{
    if (!sub_7202E0()) {
        m_berserkActive = FALSE;
    }

    if (m_derivedStats.m_spellStates.test(SPLSTATE_BERSERK_ALWAYS_ACTIVE)) {
        m_berserkActive = TRUE;
        return;
    }

    if (static_cast<DWORD>(field_44A) % 100 == static_cast<DWORD>(m_id) % 100) {
        if (GetVertListType() == CGAMEOBJECT_LIST_FRONT) {
            CAIObjectType enemy = m_typeAI.GetEnemyOf();
            if (m_pArea->GetNearest(m_id,
                    enemy,
                    GetVisualRange(),
                    m_terrainTable,
                    TRUE,
                    GetCanSeeInvisible(),
                    FALSE,
                    0,
                    FALSE)
                == CGameObjectArray::INVALID_INDEX) {
                m_berserkActive = FALSE;
            } else {
                m_berserkActive = TRUE;
            }
        } else {
            m_berserkActive = FALSE;
        }
    }
}

// 0x72DE60
BOOL CGameSprite::ProcessEffectList()
{
    if (g_pChitin->cNetwork.GetServiceProvider() != CNetwork::SERV_PROV_NULL
        && g_pChitin->cNetwork.m_idLocalPlayer != m_remotePlayerID) {
        return FALSE;
    }

    BOOL bResult = TRUE;

    if (!m_bAllowEffectListCall) {
        field_72A2++;
        return TRUE;
    }

    while (1) {
        m_bAllowEffectListCall = FALSE;

        if (field_72A2 > 4) {
            field_72A2 = 5;
        }

        if (((m_id ^ field_44A) & 0xF) == 0 || field_562C || m_bHPCONBonusTotalUpdate) {
            m_lastRegenerationTime = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime;

            // The saving throw rolls (CGameEffect::CheckSave reads them).
            field_70F6 = rand() % 20 + 1;
            field_70F7 = rand() % 20 + 1;
            field_70F8 = rand() % 20 + 1;
            field_70F9 = rand() % 20 + 1;
            field_70FA = rand() % 100;

            // Whether the carried weight no longer matches the animation's
            // move scale (0x72DFC4-0x72E1A6).
            BOOL bEncumbranceChanged = FALSE;
            if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1) {
                DWORD nWeight = GetCarriedWeight();
                const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
                SHORT nAllowance = static_cast<SHORT>(ruleTables.m_tStrengthMod.GetAtLong(CPoint(3, m_derivedStats.m_nSTR)));

                // NOTE: The original computes on the x87 stack in double
                // precision from the two values rounded to float, and
                // truncates (0x72E05B-0x72E080).
                DWORD nMaxWeight = static_cast<DWORD>(static_cast<double>(static_cast<float>(ruleTables.GetEncumbranceMod(this)))
                    / 100.0
                    * static_cast<double>(static_cast<float>(nAllowance)));

                if (nWeight > nMaxWeight * 120 / 100) {
                    // NOTE: Uninline.
                    bEncumbranceChanged = GetAnimation()->GetMoveScale() != 0;
                } else if (nWeight > nMaxWeight) {
                    // NOTE: Uninline.
                    bEncumbranceChanged = GetAnimation()->GetMoveScale() != (GetAnimation()->GetMoveScaleDefault() >> 1);
                } else {
                    // NOTE: Uninline.
                    bEncumbranceChanged = GetAnimation()->GetMoveScale() < GetAnimation()->GetMoveScaleDefault();
                }
            }

            if (m_timedEffectList.CheckEffects()
                || m_equipedEffectList.CheckEffects()
                || field_562C
                || bEncumbranceChanged
                || m_bHPCONBonusTotalUpdate) {
                field_562C = FALSE;
                m_tempStats = m_derivedStats;

                // NOTE: Uninline.
                field_72EA = GetAnimation()->GetMoveScale();
                field_72EC = m_derivedStats.m_generalState;

                // NOTE: Uninline.
                GetAnimation()->ResetMoveScale();

                // The move scale bonuses (0x72E270-0x72E3A8): a barbarian
                // not in heavy armour moves at 1.2 times (in floats,
                // truncated), a monk unarmoured (or in armour sub_724690(0)
                // allows) one more per three monk levels, Dash one more.
                if (GetAnimation()->GetMoveScale() > 0) {
                    INT nMoveScale = GetAnimation()->GetMoveScale();
                    if (m_typeAI.IsClassValid(CAIOBJECTTYPE_C_BARBARIAN) && !sub_724690(3)) {
                        nMoveScale = static_cast<INT>(static_cast<float>(nMoveScale) * 1.2f);
                    }
                    if (GetAIType().IsClassValid(CAIOBJECTTYPE_C_MONK)
                        && (m_equipment.m_items[1] == NULL || sub_724690(0))) {
                        nMoveScale += m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_MONK) / 3;
                    }
                    if (sub_763150(CGAMESPRITE_FEAT_DASH)) {
                        nMoveScale++;
                    }
                    GetAnimation()->SetMoveScale(static_cast<BYTE>(nMoveScale));
                }

                // TODO: Incomplete (0x72E3A8-0x72E70E): the color ranges
                // (m_hasColorRangeEffects), the color
                // effects (m_hasColorEffects), the animation change with
                // its search bitmap update and sequence reset (field_5640),
                // and their messages. The sequence reset calls SetSequence
                // (0x707230) twice, which can draw rand (0x7077AE), so a
                // creature whose animation changes here leaves our rand
                // state behind the original's.

                m_hasColorRangeEffects = FALSE;
                m_hasColorEffects = FALSE;
                field_5640 = FALSE;
                m_portraitIcons.RemoveAll();

                g_pBaldurChitin->GetObjectGame()->UpdatePortrait(g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id), 1);

                bResult = HandleEffects();

                m_bForceVisualEffects = TRUE;

                // TODO: Incomplete (0x72E759-0x72FB24): the AI actions and
                // messages that follow a change of state (the general
                // state against field_72EC, the current action).
            }

            g_pBaldurChitin->GetActiveEngine()->UpdateCharacterStatus(m_id);

            // NOTE: Uninline.
            field_72EA = GetAnimation()->GetMoveScale();
            field_72EC = m_derivedStats.m_generalState;
        }

        if (field_72A2 < 1) {
            m_bAllowEffectListCall = TRUE;

            if (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_active) {
                // TODO: Incomplete: HandlePersistantEffects (0x733200, with
                // 1; 0x72FBA7-0x72FBC3), which src/ does not define yet.
            }

            return bResult;
        }

        field_72A2--;
    }
}

// 0x72FD20
void CGameSprite::CheckModal()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (static_cast<DWORD>(field_44A) % 100 != static_cast<DWORD>(m_id) % 100) {
        return;
    }

    m_modalCounter++;

    if (pGame->GetCharacterPortraitNum(m_id) != -1) {
        // A party member looks for the secret doors around it.
        CTypedPtrList<CPtrList, LONG*> targets;

        INT nSearch = sub_71F6E0();
        INT nRoll = CUtil::UtilRandInt(100, -m_derivedStats.m_nLuck);
        SHORT nRange = GetVisualRange() >> 1;

        m_pArea->GetAllInRange(m_pos,
            CAIObjectType::ANYONE,
            nRange,
            GetVisibleTerrainTable(),
            targets,
            FALSE,
            TRUE);

        POSITION pos = targets.GetHeadPosition();
        while (pos != NULL) {
            LONG nId = static_cast<LONG>(reinterpret_cast<INT_PTR>(targets.GetNext(pos)));

            CGameObject* pObject;
            BYTE rc;
            do {
                rc = pGame->GetObjectArray()->GetDeny(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                continue;
            }

            if (pObject->GetObjectType() != CGameObject::TYPE_DOOR) {
                pGame->GetObjectArray()->ReleaseDeny(nId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
                continue;
            }

            CGameDoor* pDoor = static_cast<CGameDoor*>(pObject);
            if ((pDoor->m_dwFlags & 0x80) != 0) {
                if ((pDoor->m_dwFlags & 0x100) == 0) {
                    INT nDifficulty = pDoor->m_detectionDifficulty;
                    if (nDifficulty == 0) {
                        nDifficulty = 100;
                    }

                    if (nSearch >= nDifficulty + nRoll - 101) {
                        PlaySound(CResRef("ACT_09"));
                        pDoor->SetDrawPoly(400);
                        pDoor->m_dwFlags |= 0x100;

                        CMessage* message = new CMessageDoorStatus(pDoor, m_id, pDoor->GetId());
                        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
                    }
                } else {
                    pDoor->SetDrawPoly(400);
                }
            }

            pGame->GetObjectArray()->ReleaseDeny(nId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }
    }

    // The modal actions (the byte switch at 0x730029, jump table 0x731B20).
    switch (m_nModalState) {
    case 1:
        // TODO: 0x730030-0x73055C not ported (the battle song: the song's
        // spell effects on everyone in range).
        break;
    case 2:
        // TODO: 0x73055C-0x730EA2 not ported (detect traps).
        break;
    case 3:
        // TODO: 0x730EA2-0x730EC4 not ported (stealth: CGameSprite_Hide
        // 0x757B40 every [0x85BD08] modal counts).
        break;
    case 4:
        // TODO: 0x730EC4-0x731B04 not ported (turn undead).
        break;
    }
}

// NOTE: The sprite's override of the status triggers (the vtable at
// 0x85BDD4, slot 0x64). Any trigger it does not know goes to
// CGameAIBase::EvaluateStatusTrigger (0x453840).
//
// 0x731B30
BOOL CGameSprite::EvaluateStatusTrigger(const CAITrigger& trigger)
{
    CAITrigger triggerCopy(trigger);
    BOOL bResult = FALSE;

    switch (triggerCopy.m_triggerID) {
    case 0x400A: // Alignment
        triggerCopy.m_triggerCause.Decode(this);
        switch (triggerCopy.m_specificID) {
        case 1:
        case 2:
        case 3:
            bResult = (triggerCopy.m_triggerCause.m_nAlignment & 0xF) == triggerCopy.m_specificID;
            break;
        case 0x10:
        case 0x20:
        case 0x30:
            bResult = (triggerCopy.m_triggerCause.m_nAlignment & 0xF0) == triggerCopy.m_specificID;
            break;
        default:
            bResult = triggerCopy.m_triggerCause.m_nAlignment == triggerCopy.m_specificID;
            break;
        }
        break;
    case 0x4031: // HaveSpell
        // TODO: Incomplete (0x731C09-0x731C8E): CGameAIBase_DecodeSpell
        // and sub_725330 over the sprite's spells.
        bResult = FALSE;
        break;
    case 0x4032: // HaveAnySpells
        // TODO: Incomplete (0x731D53-0x731FCF): GetSpells,
        // GetSpellsAtLevel and the grouped spell lists.
        bResult = FALSE;
        break;
    case 0x40E2: // SetMarkedSpell
        if (trigger.m_specificID == 0) {
            field_588 = 0;
        } else if (field_588 == 0) {
            // TODO: Incomplete (0x731CC5-0x731D26): the spell's resref
            // through CGameAIBase_DecodeSpell (0x45D3E0) and, when not
            // empty and sub_725330 (0x725330) finds it among the sprite's
            // spells, field_588 = the trigger's specific.
        }
        bResult = TRUE;
        break;
    case 0x4039: // NumTimesTalkedTo
        bResult = m_nNumberOfTimesTalkedTo == triggerCopy.m_specificID;
        break;
    case 0x403A: // NumTimesTalkedToGT
        bResult = triggerCopy.m_specificID < m_nNumberOfTimesTalkedTo;
        break;
    case 0x403B: // NumTimesTalkedToLT
        bResult = m_nNumberOfTimesTalkedTo < triggerCopy.m_specificID;
        break;
    case 0x4058: // HaveUsableWeaponEquipped
        // TODO: Incomplete (0x732016-0x732193): the selected weapon's
        // ability (CItem::Demand, GetAbility, GetUsageCount).
        bResult = FALSE;
        break;
    case 0x4063: // InWeaponRange
        // TODO: Incomplete (0x732237-0x732661): the weapon's range against
        // the target from CGameAIBase_GetTargetShareAIType.
        bResult = FALSE;
        break;
    case 0x40F3: // IsFacingObject
        // TODO: Incomplete (0x732662-0x7326B1): the target from
        // CGameAIBase_GetTargetShareAIType.
        bResult = FALSE;
        break;
    case 0x40A0: // Internal
    case 0x40A1: // InternalGT
    case 0x40A2: // InternalLT
    case 0x40A3: // School
    case 0x40A4: // IsWeaponRanged
    case 0x40B1: // ModalState
        // TODO: Incomplete (0x7329D8-0x732C3F): the target from
        // CGameAIBase_GetTargetShareAIType and its members.
        bResult = FALSE;
        break;
    case 0x40F2: // IsSpellTargetValid
        // TODO: Incomplete (0x732C40-0x732CA1): CGameSprite_CanTarget and
        // CGameSprite_IsSpellTargetValid on the target.
        bResult = FALSE;
        break;
    case 0x40F8: // OutOfAmmo
        // TODO: Incomplete (0x732CA2-0x732E62): the selected weapon's
        // ability and usage count.
        bResult = FALSE;
        break;
    case 0x4059: // NumTimesInteracted
        if (triggerCopy.m_specificID < 24) {
            bResult = m_nNumberOfTimesInteractedWith[triggerCopy.m_specificID] == triggerCopy.m_specific2;
        } else {
            bResult = FALSE;
        }
        break;
    case 0x405A: // NumTimesInteractedGT
        if (triggerCopy.m_specificID < 24) {
            bResult = triggerCopy.m_specific2 < m_nNumberOfTimesInteractedWith[triggerCopy.m_specificID];
        } else {
            bResult = FALSE;
        }
        break;
    case 0x405B: // NumTimesInteractedLT
        if (triggerCopy.m_specificID < 24) {
            bResult = m_nNumberOfTimesInteractedWith[triggerCopy.m_specificID] < triggerCopy.m_specific2;
        } else {
            bResult = FALSE;
        }
        break;
    case 0x4062: // InteractingWith
        // NOTE: The original decodes the sprite's own m_interactingWith
        // (0x73220F), not the trigger's object.
        m_interactingWith.Decode(this);
        bResult = m_interactingWith.OfType(triggerCopy.m_triggerCause, FALSE, FALSE);
        break;
    case 0x406D: // UnselectableVariable
        bResult = static_cast<DWORD>(m_nUnselectableCounter) == static_cast<DWORD>(triggerCopy.m_specificID);
        break;
    case 0x406E: // UnselectableVariableGT
        bResult = triggerCopy.m_specificID < m_nUnselectableCounter;
        break;
    case 0x406F: // UnselectableVariableLT
        bResult = m_nUnselectableCounter < triggerCopy.m_specificID;
        break;
    case 0x4097: { // TimerActive
        POSITION pos = m_timers.GetHeadPosition();
        while (pos != NULL) {
            CGameTimer* pTimer = m_timers.GetNext(pos);
            if (pTimer != NULL && static_cast<LONG>(pTimer->m_id) == triggerCopy.m_specificID) {
                bResult = TRUE;
            }
        }
        break;
    }
    case 0x4098: { // ClassEx
        triggerCopy.m_triggerCause.Decode(this);
        DWORD mask = triggerCopy.m_triggerCause.m_nClassMask;
        LONG nClass = triggerCopy.m_specificID;
        if (((mask & 0x1) != 0 && (nClass == 1 || nClass == 0xCD))
            || ((mask & 0x2) != 0 && (nClass == 2 || nClass == 0xCA))
            || ((mask & 0x4) != 0 && (nClass == 3 || nClass == 0xCB))
            || ((mask & 0x8) != 0 && (nClass == 4 || nClass == 0xCB || nClass == 0xCC))
            || ((mask & 0x10) != 0 && (nClass == 5 || nClass == 0xCD))) {
            bResult = TRUE;
            break;
        }

        bResult = FALSE;
        if ((mask & 0x20) != 0 && nClass == 6) {
            bResult = TRUE;
            break;
        }
        if ((mask & 0x40) != 0) {
            if (nClass == 7 || nClass == 0xCD || nClass == 0xCE) {
                bResult = TRUE;
                break;
            }
        }
        if ((mask & 0x80) != 0) {
            if (nClass == 8 || nClass == 0xCD || nClass == 0xCF) {
                bResult = TRUE;
                break;
            }
        }
        if ((mask & 0x100) != 0) {
            if (nClass == 9 || nClass == 0xD0) {
                bResult = TRUE;
                break;
            }
        }
        if ((mask & 0x200) != 0) {
            if (nClass == 10 || nClass == 0xD1) {
                bResult = TRUE;
                break;
            }
        }
        if ((mask & 0x400) != 0 && (nClass == 11 || nClass == 0xD1)) {
            bResult = TRUE;
        }
        break;
    }
    case 0x4099: // NearSavedLocation
        if (m_baseStats.field_2E4 != 0 && m_baseStats.field_2E6 != 0) {
            INT dx = static_cast<INT>(GetPos().x - static_cast<USHORT>(m_baseStats.field_2E4)) / CPathSearch::GRID_SQUARE_SIZEX;
            INT dy = static_cast<INT>(GetPos().y - static_cast<USHORT>(m_baseStats.field_2E6)) / CPathSearch::GRID_SQUARE_SIZEY;
            if (dy * dy + dx * dx <= triggerCopy.m_specificID * triggerCopy.m_specificID) {
                bResult = TRUE;
            }
        }
        break;
    case 0x40E0: // IsMarkedSpell
        bResult = trigger.m_specificID == field_588;
        break;
    case 0x40F1: // ForceMarkedSpell
        if (field_588 != trigger.m_specificID) {
            field_588 = trigger.m_specificID;
        }
        bResult = TRUE;
        break;
    default:
        // Global, GlobalGT, GlobalLT, RandomNum and the other base triggers.
        bResult = CGameAIBase::EvaluateStatusTrigger(trigger);
        break;
    }

    return bResult;
}

// 0x733050
void CGameSprite::AddEffect(CGameEffect* pEffect, BYTE list, BOOL noSave, BOOL immediateApply)
{
    if (pEffect->m_effectID == 150) {
        if (pEffect != NULL) {
            delete pEffect;
        }
        return;
    }

    if (m_derivedStats.m_spellStates.test(SPLSTATE_OTILUKES_RESILIENT_SPHERE)
        && pEffect->m_effectID != 58
        && pEffect->m_effectID != 402
        && pEffect->m_effectID != 146
        && pEffect->m_effectID != 148) {
        if (pEffect != NULL) {
            delete pEffect;
        }
        return;
    }

    // NOTE: Inlined CAIObjectType::Equal.
    if (GetAIType().Equal(CAIObjectType::NOT_SPRITE)) {
        return;
    }

    if (!pEffect->sub_4A3310(this, &field_70F6, &field_70F7, &field_70F8, &field_70F9, &field_70FA)
        && !noSave) {
        if (pEffect != NULL) {
            delete pEffect;
        }
    } else {
        field_562C = 1;

        if (immediateApply) {
            m_tempStats = m_derivedStats;
            pEffect->ResolveEffect(this);
            if (pEffect->m_done) {
                if (pEffect != NULL) {
                    delete pEffect;
                }
                return;
            }
        }

        // NOTE: The duration type is the WORD constant at 0x8491AC (2).
        if (list != EFFECT_LIST_TIMED) {
            m_equipedEffectList.AddTail(pEffect);
        } else if (pEffect->m_durationType == 2) {
            m_equipedEffectList.AddTail(pEffect);
        } else {
            m_timedEffectList.AddTail(pEffect);
        }
    }

    if (!g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_active && immediateApply) {
        ProcessEffectList();
    }
}

// 0x733290
void CGameSprite::ClearStoredPaths()
{
    POSITION pos = m_pPathTemp.GetHeadPosition();
    while (pos != NULL) {
        int* node = m_pPathTemp.GetNext(pos);
        delete node;
    }

    m_pPathTemp.RemoveAll();
    m_nPathTemp.RemoveAll();
}

// 0x7332D0
void CGameSprite::ClearDialogActions()
{
    SHORT actionID = m_curAction.m_actionID;
    if (actionID == 8
        || actionID == 137
        || actionID == 139
        || actionID == 198) {
        SetCurrAction(CAIAction::NULL_ACTION);

        CMessageDropPath* pMessage = new CMessageDropPath(m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }
}

// 0x733340
CGameObject* CGameSprite::GetTargetShare()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameObject* pOld;
    BYTE rc;

    CGameObject* pObject = m_curAction.m_acteeID.sub_40CCA0(this, FALSE);
    if (pObject == NULL) {
        if (m_targetId == CGameObjectArray::INVALID_INDEX) {
            return NULL;
        }

        if (m_bSelected && Orderable(FALSE)) {
            do {
                rc = pGame->GetObjectArray()->GetDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pOld,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                return NULL;
            }

            if (pOld->GetObjectType() == CGameObject::TYPE_SPRITE) {
                static_cast<CGameSprite*>(pOld)->m_marker.SetType(CMarker::ELLIPSE);
            }

            pGame->GetObjectArray()->ReleaseDeny(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        m_targetId = CGameObjectArray::INVALID_INDEX;
        return NULL;
    }

    if (m_curAction.m_acteeID.m_nInstance == -1) {
        CAIObjectType acteeID;
        acteeID.Set(m_curAction.m_acteeID);
        acteeID.m_nInstance = pObject->GetId();
        m_curAction.m_acteeID.Set(acteeID);
    }

    if (m_targetId != pObject->GetId()) {
        if (m_targetId != CGameObjectArray::INVALID_INDEX) {
            do {
                rc = pGame->GetObjectArray()->GetDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pOld,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc != CGameObjectArray::SUCCESS) {
                // NOTE: The old target cannot be had: the new one is
                // returned without becoming the marked target (0x733422).
                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
                    && static_cast<CGameSprite*>(pObject)->m_derivedStats.m_cImmunitiesAIType.OnList(m_typeAI)) {
                    pGame->GetObjectArray()->ReleaseShare(pObject->GetId(),
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                    return NULL;
                }
                return pObject;
            }

            if (m_bSelected
                && Orderable(FALSE)
                && pOld->GetObjectType() == CGameObject::TYPE_SPRITE) {
                static_cast<CGameSprite*>(pOld)->m_marker.SetType(CMarker::ELLIPSE);
            }

            pGame->GetObjectArray()->ReleaseDeny(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        }

        if (m_bSelected
            && Orderable(FALSE)
            && pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
            static_cast<CGameSprite*>(pObject)->m_marker.SetType(CMarker::RECTICLE);
        }

        m_targetId = pObject->GetId();
    }

    if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE
        && static_cast<CGameSprite*>(pObject)->m_derivedStats.m_cImmunitiesAIType.OnList(m_typeAI)) {
        pGame->GetObjectArray()->ReleaseShare(pObject->GetId(),
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return NULL;
    }

    return pObject;
}

// 0x733660
void CGameSprite::SetTargetObject(CGameObject* pTarget)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (pTarget != NULL && m_targetId == pTarget->GetId()) {
        return;
    }

    if (m_targetId != CGameObjectArray::INVALID_INDEX) {
        CGameObject* pOld;
        BYTE rc;
        do {
            rc = pGame->GetObjectArray()->GetDeny(m_targetId,
                CGameObjectArray::THREAD_ASYNCH,
                &pOld,
                INFINITE);
        } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

        if (rc != CGameObjectArray::SUCCESS) {
            return;
        }

        if (pOld != NULL
            && pOld->GetObjectType() == CGameObject::TYPE_SPRITE
            && m_bSelected
            && Orderable(FALSE)) {
            static_cast<CGameSprite*>(pOld)->m_marker.SetType(CMarker::ELLIPSE);
        }

        pGame->GetObjectArray()->ReleaseDeny(m_targetId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
    }

    if (pTarget != NULL) {
        if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE
            && m_bSelected
            && Orderable(FALSE)) {
            static_cast<CGameSprite*>(pTarget)->m_marker.SetType(CMarker::RECTICLE);
            if (m_actionCount == 0 && m_userCommandPause > 73) {
                static_cast<CGameSprite*>(pTarget)->m_marker.m_nRecticleForceRenderTarget = 15;
            }
        }

        m_targetId = pTarget->GetId();
        return;
    }

    m_targetId = CGameObjectArray::INVALID_INDEX;
}

// 0x7337D0
void CGameSprite::ClearAI(BOOLEAN bSetSequence)
{
    ClearActions(FALSE);
    ClearTriggers();
    SetCurrAction(CAIAction::NULL_ACTION);

    if (bSetSequence) {
        if (m_nSequence != GetIdleSequence()
            && m_nSequence != SEQ_TWITCH
            && m_nSequence != SEQ_SLEEP
            && m_nSequence != SEQ_DIE) {
            if (GetVertListType() != LIST_FLIGHT) {
                CMessage* pMessage = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()),
                    m_id,
                    m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
            }
        }
    }
}

// 0x7338E0
void CGameSprite::SetCurrAction(const CAIAction& action)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    // NOTE: The original reads the world screen's panel through its getter
    // at 0x691130 (`return field_EA4`).
    if (action.m_actionID != CAIAction::NO_ACTION
        && g_pBaldurChitin->m_pEngineWorld->field_EA4 == 8
        && m_id == pGame->m_iContainerSprite) {
        g_pBaldurChitin->m_pEngineWorld->StopContainer();
    }

    m_curDest.x = -1;
    m_curDest.y = -1;
    field_7118 = 0;
    field_5616 = 0;
    field_711C = 0;
    m_actionCount = 0;
    m_bStartedCasting = FALSE;
    m_interrupt = FALSE;
    field_5630 = 0;
    field_5632 = 0;
    field_5636 = 0;
    m_moveCount = 0;
    m_bInCasting = FALSE;

    if (m_nModalState != 0
        && action.m_actionID != CAIAction::NO_ACTION
        && action.m_actionID != CAIAction::MOVETOPOINT
        && action.m_actionID != 22 // MoveToObject
        && action.m_actionID != CAIAction::NO_ACTION
        && action.m_actionID != CAIAction::FACE
        && action.m_actionID != 18 // Hide
        && action.m_actionID != CAIAction::SMALLWAIT
        && action.m_actionID != 99
        && action.m_actionID != 89 // Follow
        && action.m_actionID != CAIAction::MOVETOPOINTNORECTICLE
        && action.m_actionID != 215
        && action.m_actionID != 13 // FindTraps
        && action.m_actionID != 33 // Turn
        && action.m_actionID != 234 // Debug
        && action.m_actionID != 273 // HideGUI
        && action.m_actionID != 274 // UnhideGUI
        && action.m_actionID != 293 // UnHide
        && action.m_actionID != 183 // MultiPlayerSync
        && action.m_actionID != 233 // HideCreature
        && action.m_actionID != 30 // SetGlobal
        && action.m_actionID != CAIAction::WAIT
        && action.m_actionID != 10 // Enemy
        && action.m_actionID != 257 // ForceHide
        && action.m_actionID != 236 // SetSavedLocation
        && action.m_actionID != 249 // SetSavedLocationPoint
        && action.m_actionID != 252 // ChangeCurrentScript
        && action.m_actionID != CAIAction::CONTINUE
        && action.m_actionID != CAIAction::RANDOMWALK) {
        if (m_nModalState == 2 || m_nModalState == 4) {
            SetModalState(0, TRUE);

            pGame->GetButtonArray()->m_nSelectedButton = 100;

            if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == pGame->GetCharacterPortraitNum(m_id)) {
                INT nState = pGame->GetButtonArray()->m_nState;
                if (nState != 102
                    && nState != 101
                    && nState != 104
                    && nState != 103
                    && nState != 105
                    && nState != 112
                    && nState != 115
                    && nState != 116
                    && nState != 117
                    && nState != 118
                    && nState != 119
                    && nState != 120
                    && nState != 121
                    && nState != 122
                    && nState != 123
                    && nState != 106) {
                    pGame->GetButtonArray()->ResetState();
                }
            }
        } else {
            SetModalState(0, TRUE);
        }
    }

    if (m_curAction.m_actionID != CAIAction::NO_ACTION || action.m_actionID != CAIAction::NO_ACTION) {
        ResolveTargetPoint(&action, m_queuedActions.GetHeadPosition());
    }

    if (action.m_actionID != 92) { // SelectWeaponAbility
        m_nTempSelectedWeapon = m_equipment.m_selectedWeapon;
        m_nTempSelectedWeaponAbility = static_cast<BYTE>(m_equipment.m_selectedWeaponAbility);
    }

    if (action.m_actionID == 28) { // RemoveTraps
        CMessage* pMessage = new CMessage107(TRUE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (m_curAction.m_actionID == 28 && action.m_actionID != 28) { // RemoveTraps
        CMessage* pMessage = new CMessage107(FALSE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    m_curAction.m_actionID = action.m_actionID;
    m_curAction.m_specificID = action.m_specificID;
    m_curAction.m_actorID.Set(action.m_actorID);
    m_curAction.m_acteeID.Set(action.m_acteeID);
    m_curAction.m_acteeID2.Set(action.m_acteeID2);
    m_curAction.m_dest = action.m_dest;
    m_curAction.m_specificID2 = action.m_specificID2;
    m_curAction.m_specificID3 = action.m_specificID3;
    m_curAction.m_string1 = action.m_string1;
    m_curAction.m_string2 = action.m_string2;
    m_curAction.m_internalFlags = action.m_internalFlags;

    m_bPlayedEncumberedStopped = FALSE;
    m_bPlayedEncumberedSlowed = FALSE;

    if (m_currentActionId != action.m_actionID) {
        if (action.m_actionID == 108 // EscapeArea
            || action.m_actionID == 176 // EscapeAreaDestroy
            || action.m_actionID == CAIAction::DIALOGUE
            || action.m_actionID == CAIAction::STARTDIALOG
            || action.m_actionID == CAIAction::STARTDIALOGUENOSET) {
            CMessage* pMessage = new CMessageSetCurrentActionId(action.m_actionID, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        } else if (m_currentActionId != 0) {
            CMessage* pMessage = new CMessageSetCurrentActionId(0, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
        }
    }

    if (m_targetId != CGameObjectArray::INVALID_INDEX) {
        if (m_bSelected && Orderable(FALSE)) {
            CGameObject* pObject;

            BYTE rc;
            do {
                rc = pGame->GetObjectArray()->GetDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    &pObject,
                    INFINITE);
            } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

            if (rc == CGameObjectArray::SUCCESS) {
                if (pObject->GetObjectType() == CGameObject::TYPE_SPRITE) {
                    static_cast<CGameSprite*>(pObject)->m_marker.SetType(CMarker::ELLIPSE);
                }

                pGame->GetObjectArray()->ReleaseDeny(m_targetId,
                    CGameObjectArray::THREAD_ASYNCH,
                    INFINITE);
            }
        }
        m_targetId = CGameObjectArray::INVALID_INDEX;
    }

    if (m_curSpell != NULL) {
        delete m_curSpell;
        m_curSpell = NULL;
    }

    if (m_curItem != NULL) {
        m_curItem = NULL;
    }

    if (action.m_actionID == CAIAction::NO_ACTION) {
        return;
    }

    field_710C = 0;

    if (m_curProjectile != NULL) {
        delete m_curProjectile;
        m_curProjectile = NULL;
    }

    field_5618 = 0;

    if (pGame->GetRuleTables().m_lNoDecodeList.Find(action.m_actionID) == NULL) {
        m_curAction.Decode(this);
    }

    // The path search counter (the WORD at 0x85BBA4 is 8).
    field_5616 = static_cast<SHORT>(CUtil::UtilRandInt(8, -m_derivedStats.m_nLuck));
    m_noActionCount = 0;

    if (m_nSequence != SEQ_TWITCH
        && m_nSequence != SEQ_DIE
        && m_nSequence != SEQ_SLEEP
        && m_nSequence != SEQ_WALK) {
        SetIdleSequence();
    }

    if (m_userCommandPause > CTimerWorld::TIMESCALE_MSEC_PER_SEC * 5 - 2) {
        m_marker.m_nRecticleForceRender = CTimerWorld::TIMESCALE_MSEC_PER_SEC;
    }

    if (action.m_actionID == CAIAction::MOVETOPOINT
        || action.m_actionID == 22 // MoveToObject
        || action.m_actionID == 89 // Follow
        || action.m_actionID == CAIAction::MOVETOPOINTNORECTICLE
        || action.m_actionID == 27 // ProtectPoint
        || action.m_actionID == 215) {
        if (m_derivedStats.m_nEncumberance == 2) {
            FeedBack(15, 0, 0, 0, -1, 0, 0);
        } else if (m_derivedStats.m_nEncumberance == 1) {
            FeedBack(14, 0, 0, 0, -1, 0, 0);
        }
    }

    if (m_curAction.m_actionID == CAIAction::SPELL
        || m_curAction.m_actionID == CAIAction::SPELLPOINT
        || m_curAction.m_actionID == CAIAction::FORCESPELL
        || m_curAction.m_actionID == CAIAction::FORCESPELLPOINT
        || m_curAction.m_actionID == CAIAction::REALLYFORCESPELL
        || m_curAction.m_actionID == CAIAction::SPELLNODEC
        || m_curAction.m_actionID == CAIAction::SPELLPOINTNODEC) {
        CString sSpell;

        if (strcmp(m_curAction.GetString1(), "") == 0) {
            DecodeSpell(m_curAction.m_specificID, sSpell);
        } else {
            sSpell = m_curAction.GetString1();
        }

        CResRef cResSpell(sSpell);
        if (pGame->GetSpellType(cResSpell) == 1) {
            BYTE nClass = static_cast<BYTE>(m_curAction.m_specificID2);
            DWORD nSpecialization = pGame->GetRuleTables().GetSpecializationMask(nClass,
                static_cast<BYTE>(m_curAction.m_specificID2 >> 8));
            BYTE nLevel = static_cast<BYTE>(m_curAction.m_specificID3);

            if (nClass == 0 || nLevel == 0) {
                m_curSpell = new CSpell(cResSpell);
                if (m_curSpell != NULL) {
                    if (!sub_727720(cResSpell, nClass, nSpecialization, nLevel)) {
                        if (m_curSpell->Demand() == TRUE) {
                            switch (m_curSpell->GetCasterType()) {
                            case 1:
                                nClass = CAIOBJECTTYPE_C_WIZARD;
                                break;
                            case 2:
                                nClass = CAIOBJECTTYPE_C_CLERIC;
                                break;
                            default:
                                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
                                // __LINE__: 6082
                                UTIL_ASSERT_MSG(FALSE, "Invalid spell type.");
                            }

                            nSpecialization = 0;
                            nLevel = static_cast<BYTE>(m_curSpell->GetLevel());
                            m_curSpell->Release();
                        }

                        CString sError(" ");
                        sError.Format("Error setting action for %s (%s): could not find spell (%s) in any spell casting class.",
                            (LPCSTR)m_sName,
                            (LPCSTR)m_resRef.GetResRefStr(),
                            (LPCSTR)cResSpell.GetResRefStr());

                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
                        // __LINE__: 6102
                        UTIL_ASSERT_MSG((nClass == CAIOBJECTTYPE_C_BARD)
                                || (nClass == CAIOBJECTTYPE_C_CLERIC)
                                || (nClass == CAIOBJECTTYPE_C_DRUID)
                                || (nClass == CAIOBJECTTYPE_C_PALADIN)
                                || (nClass == CAIOBJECTTYPE_C_RANGER)
                                || (nClass == CAIOBJECTTYPE_C_SORCERER)
                                || (nClass == CAIOBJECTTYPE_C_WIZARD),
                            sError);

                        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
                        // __LINE__: 6105
                        UTIL_ASSERT_MSG(nLevel <= CSPELLLIST_MAX_LEVELS, "Error setting action: Invalid spell level.");
                    }

                    m_curAction.m_specificID2 = MAKEWORD(nClass,
                        pGame->GetRuleTables().GetSpecializationIndex(nClass, nSpecialization));
                    m_curAction.m_specificID3 = nLevel;
                }

                if (m_curSpell != NULL) {
                    delete m_curSpell;
                    m_curSpell = NULL;
                }
            }
        }
    }
}

// 0x734550
BOOL CGameSprite::HandleEffects()
{
    BOOL bRetry;
    BOOL bResult;
    UINT nSkill;

    do {
        bRetry = FALSE;

        m_derivedStats.Reload(this, &m_baseStats, &m_spells, &m_domainSpells);
        m_derivedStats.m_nTurnUndeadLevel = GetTurnUndeadLevel();
        m_derivedStats.m_nBackstabDamageMultiplier = GetBackstabDamageMultiplier();
        m_derivedStats.m_nLayOnHandsAmount = GetLayOnHandsAmount();

        ResetAIType();

        m_activeImprisonment = TRUE;

        // TODO: Incomplete. The original takes the visual effect object
        // field_72E2 from the object array (GetDeny, retried while it
        // reports busy) and marks its effects for deletion with 0x555200
        // before ReleaseDeny (0x7345C2-0x734634); IcewindCVisualEffect
        // has no port of 0x555200 yet.

        m_bonusStats.BonusInit();

        bResult = m_equipedEffectList.HandleList(this) & 1;
        if (m_equipedEffectList.m_retry) {
            bRetry = TRUE;
        }

        bResult &= m_timedEffectList.HandleList(this);
        if (m_timedEffectList.m_retry) {
            bRetry = TRUE;
        }

        m_derivedStats += m_bonusStats;

        for (nSkill = 0; nSkill < 16; nSkill++) {
            m_derivedStats.m_nSkills[nSkill] += static_cast<signed char>(sub_763E40(nSkill));
            if (m_derivedStats.m_nSkills[nSkill] < 1) {
                m_derivedStats.m_nSkills[nSkill] = 0;
            }
        }

        if (g_pBaldurChitin->GetActiveEngine()->GetSelectedCharacter() == g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id)) {
            g_pBaldurChitin->GetObjectGame()->m_cButtonArray.UpdateState();
        }
    } while (bRetry);

    return bResult;
}

// 0x7349A0
void CGameSprite::FeedBack(WORD nFeedBackId, LONG a3, LONG a4, LONG a5, LONG a6, LONG a7, LONG a8)
{
    COLORREF rgbNameColor = CVidPalette::RANGE_COLORS[m_baseStats.m_colors[CVIDPALETTE_RANGE_MAIN_CLOTH]];
    CString v1;
    CString v2;

    switch (nFeedBackId) {
    case FEEDBACK_BACKSTAB:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            STRREF strBackstabDamage;
            switch (m_derivedStats.m_nBackstabDamageMultiplier) {
            case 1:
                strBackstabDamage = 32110; // "Backstab Normal Damage"
                break;
            case 2:
                strBackstabDamage = 16471; // "Backstab Double Damage"
                break;
            case 3:
                strBackstabDamage = 16472; // "Backstab Triple Damage"
                break;
            case 4:
                strBackstabDamage = 16473; // "Backstab Quadruple Damage"
                break;
            case 5:
                strBackstabDamage = 16474; // "Backstab Quintuple Damage"
                break;
            case 6:
                strBackstabDamage = 16475; // "Backstab Sextuple Damage"
                break;
            default:
                strBackstabDamage = -1;
                break;
            }

            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                strBackstabDamage,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_CRITICALHIT:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            if (a3 != 0) {
                STR_RES strRes;
                CString sText;
                g_pBaldurChitin->GetTlkTable().Fetch(16462, strRes); // "Critical Hit!"
                sText.Format("%s (%+d)", strRes.szText, a3);
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            } else {
                g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                    16462, // "Critical Hit!"
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_CRITICALMISS:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                16463, // "Critical Miss!"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_TOHIT:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x1) != 0) {
            CString sText1;
            CString sText2;
            sText2.Format("%d ", a8);

            if (a7 > 0) {
                STR_RES strRes;
                g_pBaldurChitin->GetTlkTable().Fetch(733, strRes); // "Off Hand"
                sText2 += "(" + strRes.szText + ")";
            }

            if (a5 > 0) {
                sText1.Format(": %d + %d = %d", a3, a5, a3 + a5);
            } else if (a5 < 0) {
                sText1.Format(": %d - %d = %d", a3, -a5, a3 + a5);
            } else {
                sText1.Format(": %d", a3);
            }

            STR_RES strRes;
            g_pBaldurChitin->GetTlkTable().Fetch(14643, strRes); // "Attack Roll "

            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            if (strRes.szText != "") {
                sText1 = strRes.szText + " " + sText2 + sText1;
            }

            if (a3 == 1) {
                g_pBaldurChitin->GetTlkTable().Fetch(16463, strRes); // "Critical Miss!"
            } else {
                if (a4 != 0) {
                    g_pBaldurChitin->GetTlkTable().Fetch(16460, strRes); // "Hit"
                } else {
                    g_pBaldurChitin->GetTlkTable().Fetch(16461, strRes); // "Miss"
                }
            }

            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            if (strRes.szText != "") {
                sText1 += " : " + strRes.szText;
            }

            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                sText1,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_SPELL:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x4) != 0) {
            STR_RES strRes;
            g_pBaldurChitin->GetTlkTable().Fetch(16464, strRes); // "Casts"

            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            if (strRes.szText != "") {
                v1 = strRes.szText;
            }

            g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);

            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }
        }
        break;
    case FEEDBACK_ATTACKS:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x4) != 0) {
            STR_RES strRes1;
            g_pBaldurChitin->GetTlkTable().Fetch(16465, strRes1); // "Attacks"

            if (strRes1.cSound.GetRes() != NULL) {
                if (!strRes1.cSound.GetLooping()) {
                    strRes1.cSound.SetFireForget(TRUE);
                }
                strRes1.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes1.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            if (strRes1.szText != "") {
                v1 = strRes1.szText;
            }

            CString sName;
            STR_RES strRes2;
            if (a6 >= -7 && a6 != -1) {
                LONG nCharacterId = g_pBaldurChitin->GetObjectGame()->GetCharacterSlot(-2 - a6);

                CGameSprite* pSprite;

                BYTE rc;
                do {
                    rc = g_pBaldurChitin->GetObjectGame()->GetObjectArray()->GetShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        reinterpret_cast<CGameObject**>(&pSprite),
                        INFINITE);
                } while (rc == CGameObjectArray::SHARED || rc == CGameObjectArray::DENIED);

                if (rc == CGameObjectArray::SUCCESS) {
                    sName = pSprite->GetName();
                    g_pBaldurChitin->GetObjectGame()->GetObjectArray()->ReleaseShare(nCharacterId,
                        CGameObjectArray::THREAD_ASYNCH,
                        INFINITE);
                }
            } else {
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes2);
                sName = strRes2.szText;
            }

            if (sName != "") {
                v1 += " " + sName;
            }

            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                v1,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_MODAL:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x4) != 0) {
            STR_RES strRes;
            g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);

            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            if (strRes.szText != "") {
                v1 = strRes.szText;
            }

            if (a3 != 0) {
                g_pBaldurChitin->GetTlkTable().Fetch(16469, strRes); // "On"
            } else {
                g_pBaldurChitin->GetTlkTable().Fetch(16470, strRes); // "Off"
            }

            if (strRes.cSound.GetRes() != NULL) {
                if (!strRes.cSound.GetLooping()) {
                    strRes.cSound.SetFireForget(TRUE);
                }
                strRes.cSound.SetChannel(14, reinterpret_cast<DWORD_PTR>(m_pArea));
                strRes.cSound.Play(m_pos.x, m_pos.y, 0, FALSE);
            }

            if (strRes.szText != "") {
                v1 += strRes.szText;
            }

            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                v1,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_LOCKPICKSUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                16517, // "Lock Pick Succeeded"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_LOCKPICKFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                16518, // "Lock Pick Failed"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_TRAPDISARMED:
        PlaySound(CResRef("ACT_09"));
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                16520, // "Trap Disarmed"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_TRAPDETECTED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                16519, // "Trap Detected"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_ITEMCURSED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                16304, // "Item Cursed!"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_HIDEFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                17120, // "Hide Failed"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_TOOHEAVY_SLOWED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (m_bPlayedEncumberedSlowed || a3 == 1) {
                g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                    19503, // "Encumbered: Slowed"
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_TOOHEAVY_STOPPED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (m_bPlayedEncumberedStopped || a3 == 1) {
                g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                    19504, // "Encumbered: Can not move"
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_16:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                a6,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_HIDESUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                19944, // "Hide Succeeded"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_MORALEFAILURE_PANIC:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                20568, // "Morale Failure: Panic"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_MORALEFAILURE_RUN:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                20569, // "Morale Failure: Running"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_MORALEFAILURE_BERSERK:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                20570, // "Morale Failure: Berserk"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_LEVELUP:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            17119, //  "Level Up"
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    case FEEDBACK_SPELLFAILED_INDOORS:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            9744, // "Spell Failed: Indoors"
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    case FEEDBACK_LEAVING_SHADOWS:
        PlaySound(CResRef("ACT_07"));
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                4188, // "Leaving Shadows"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_BASHDOORSUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                9915, // "You have successfully forced the door open!"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_BASHDOORFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                9913, // "You have failed to force the door."
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_BASHCHESTSUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                9916, // "You have successfully forced the lock open!"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_BASHCHESTFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                9914, // "You have failed to force the lock."
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_BACKSTAB_UNUSABLE_WEAPON:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            DisplayTextRef(GetNameRef(),
                10013, // "Your weapon is unsuitable for backstab."
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_PICKPOCKETFAILED_ARMOR:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                10067, // "Pick Pocket Disabled Due To Armor"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_PICKPOCKETFAILED_HOSTILE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                10068, // "Cannot pick pocket hostile creatures"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_PICKPOCKETFAILED_SKILL:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                10069, // "Pick Pocket Failed"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_PICKPOCKETFAILED_NOITEMS:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                18297, // "Pick Pocket Succeeded -- Target Had Nothing to Steal"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_PICKPOCKETFAILED_INVENTORYFULL:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                10071, // "Inventory Full"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_PICKPOCKETSUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                10072, // "Pick Pocket Succeeded"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_GAINEDSPECIALABILITY:
        if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineCreateChar) {
            STR_RES strRes;
            g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
            g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_SPECIALABILITY, strRes.szText);
            g_pBaldurChitin->GetTlkTable().Fetch(10514, strRes); // "Gained Special Ability: <SPECIALABILITYNAME>"
            g_pBaldurChitin->m_pEngineWorld->DisplayText(GetName(),
                strRes.szText,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                FALSE);
            if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineCharacter) {
                g_pBaldurChitin->GetActiveEngine()->PlayGUISound(CResRef("GAM_11"));
            }
        }
        break;
    case FEEDBACK_IMMUNE_TO_WEAPON:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                11025, // "Weapon Ineffective."
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_CHARACTER_CONTROL_SEIZURE:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            20679, // "Timeout: Server Assumed Control Of Character"
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    case FEEDBACK_TRAPDISARMEDFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                1608, // "Trap Not Disarmed"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_CRITICALHITAVOIDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x1) != 0) {
            CString sText1;
            CString sText2;
            STR_RES strRes;
            sText2.Format("%d ", a8);

            if (a7 > 0) {
                g_pBaldurChitin->GetTlkTable().Fetch(733, strRes); // "Off Hand"
                sText2 += "(" + strRes.szText + ")";
            }

            if (a5 > 0) {
                sText1.Format(": %d + %d = %d", a3, a5, a3 + a5);
            } else if (a5 < 0) {
                sText1.Format(": %d - %d = %d", a3, -a5, a3 + a5);
            } else {
                sText1.Format(": %d", a3);
            }

            g_pBaldurChitin->GetTlkTable().Fetch(39874, strRes); // "Critical Threat Roll"

            if (strRes.szText != "") {
                sText1 = strRes.szText + " " + sText2 + sText1;
            }

            if (a4 != 0) {
                g_pBaldurChitin->GetTlkTable().Fetch(16462, strRes); // "Critical Hit!"
            } else {
                g_pBaldurChitin->GetTlkTable().Fetch(33752, strRes); // "Failed!"
            }

            if (strRes.szText != "") {
                sText1 += " : " + strRes.szText;
            }

            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                sText1,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);

            if (a4 != 0) {
                PlaySound(16, TRUE, FALSE, FALSE);
            }
        }
        break;
    case FEEDBACK_AURA_CLEANSED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                23797, // "Aura Cleansed"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_ITEM_DROPPED_FULL:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                10959, // "Inventory Full: The item has been dropped."
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_LOCK_UNPICKABLE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                23169, // "The mechanism that operates this does not have a conventional lock, and may be warded against simple spells."
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_TOOMANYSUMMONED:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            18955, // "Summoning Limit Reached"
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    case FEEDBACK_SILENCED:
        DisplayTextRef(GetNameRef(),
            21298, // "Bard Song Silenced"
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_CRIPPLINGSTRIKESUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            STR_RES strRes;
            CString sText;
            g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
            sText.Format(strRes.szText, a3);
            if (sub_763150(CGAMESPRITE_FEAT_CRIPPLING_STRIKE)) {
                g_pBaldurChitin->GetTlkTable().Fetch(25070, strRes); // " with Crippling Strike (-1 Str)"
                sText.Format(strRes.szText, a3);
            }
            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                sText,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_SNEAK_UNUSABLE_WEAPON:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                24899, // "Weapon is unsuitable for sneak attack"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_EFFECTSEVADED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            STRREF strEvasion;
            if (a6 != -1) {
                strEvasion = 25368; // "Evades effects from <RESOURCE>"

                STR_RES strRes;
                CString sText;
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
                g_pBaldurChitin->GetTlkTable().SetToken(CString("RESOURCE"), strRes.szText);
            } else {
                strEvasion = 25369; // "Evades effects"
            }

            STR_RES strRes;
            CString sText;
            g_pBaldurChitin->GetTlkTable().Fetch(strEvasion, strRes);
            sText = strRes.szText;
            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                sText,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_SPELLFAILED_DISRUPTED:
        DisplayTextRef(GetNameRef(),
            14796, // "Spell Disrupted"
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_GAINEDBARDSONG:
        if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineCreateChar) {
            STR_RES strRes;
            CString sText;
            g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
            g_pBaldurChitin->GetTlkTable().SetToken(TOKEN_SPECIALABILITY, strRes.szText);
            g_pBaldurChitin->GetTlkTable().Fetch(26320, strRes); // "Gained Bard Song: <SPECIALABILITYNAME>"
            g_pBaldurChitin->m_pEngineWorld->DisplayText(GetName(),
                sText,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                FALSE);
        }
        if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineCharacter) {
            g_pBaldurChitin->GetActiveEngine()->PlayGUISound(CResRef("GAM_11"));
        }
        break;
    case FEEDBACK_STATICCHARGEEND:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                26518, // "Static charge buildup dissipates harmlessly."
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_IMMUNE_TO_RESOURCE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            if (a6 != -1) {
                STR_RES strRes;
                CString sText;
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
                g_pBaldurChitin->GetTlkTable().SetToken(CString("RESOURCE"), strRes.szText);
                g_pBaldurChitin->GetTlkTable().Fetch(26818, strRes); // "Unaffected by effects from <RESOURCE>"
                sText.Format("%s (%+d)", strRes.szText, a3);
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_LEAVEFAILED_INVENTORY:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            26826, // "Cannot leave area while remote player is in inventory screen."
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    case FEEDBACK_BACKSTAB_AVOIDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                121, // "The backstab seems to have failed."
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_SPELLFAILED_CASTFAILURE:
        DisplayTextRef(GetNameRef(),
            10453, // "Casting Failure"
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_62:
        DisplayTextRef(GetNameRef(),
            18352, // "Blinks out of existence and attack fails"
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_63:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                18353, // "Blinks out of existence and avoids an attack"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_CRIPPLINGSTRIKEFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                18759, // "The crippling strike seems to have failed."
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_USEMAGICDEVICEFAILED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                24197, // "Use Magic Device Failed"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_USEMAGICDEVICESUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                24198, // "Use Magic Device Succeeded"
                rgbNameColor,
                RGB(215, 215, 190));
        }
    case FEEDBACK_SEARCHSTART:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                25085, // "Searching..."
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_SEARCHEND:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                25090, // "Stopped Searching"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_TURNUNDEADEND:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                25136, // "Stopped Turning Undead"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_BATTLESONGSTART:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (a6 != 0) {
                STR_RES strRes1;
                STR_RES strRes2;
                CString sText;

                g_pBaldurChitin->GetTlkTable().Fetch(25138, strRes1); // "Singing Battlesong"
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes2);
                sText.Format("%s: %s", strRes1.szText, strRes2.szText);

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_BATTLESONGEND:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                25141, // "Stopped Singing Battlesong"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_ARTERIALSTRIKESUCCEEDED:
        DisplayTextRef(GetNameRef(),
            37673, // "Arterial Strike feat was used successfully to cause a bleeding wound."
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_HAMSTRINGSUCCEEDED:
        DisplayTextRef(GetNameRef(),
            37675, // "Hamstring feat was used successfully to hinder your opponent's movement by 50%."
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_HEROICINSPIRATION:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            DisplayTextRef(GetNameRef(),
                24196, // "Heroic Inspiration"
                rgbNameColor,
                RGB(215, 215, 190));
        }
        break;
    case FEEDBACK_TARGETCONCEALED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x1) != 0) {
            STR_RES strRes;
            CString sText;
            g_pBaldurChitin->GetTlkTable().Fetch(37668, strRes); // "Missed <TARGETNAME> due to concealment."
            sText.Format("%s (%+d)", strRes.szText, a3);
            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                sText,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_UNCANNYDODGESUCCEEDED:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                39308, // "Uncanny Dodge prevented flank attack."
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_ROLL:
        if (m_bRollFeedbackEnabled == -1) {
            if (GetPrivateProfileIntA("Program Options", "EnableRollFeedback", 0, g_pBaldurChitin->GetIniFileName()) != 0) {
                m_bRollFeedbackEnabled = 1;
            } else {
                m_bRollFeedbackEnabled = 0;
            }
        }

        if (m_bRollFeedbackEnabled != 0) {
            if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
                STR_RES strRes;
                CString sText;
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
                sText.Format(strRes.szText, a3, a4, a5, a7, a8);
                g_pBaldurChitin->m_pEngineWorld->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    FALSE);
                if (g_pBaldurChitin->GetActiveEngine() != g_pBaldurChitin->m_pEngineCharacter) {
                    g_pBaldurChitin->GetActiveEngine()->PlayGUISound(CResRef("GAM_11"));
                }
            }
        }
        break;
    case FEEDBACK_SPELLFAILURE_CONCENTRATION:
        DisplayTextRef(GetNameRef(),
            39265, // Spell disrupted because caster failed concentration check required due to of nearness of enemies."
            rgbNameColor,
            RGB(215, 215, 190));
        break;
    case FEEDBACK_SPONTANEOUSCASTING:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (a6 != 0) {
                STR_RES strRes1;
                STR_RES strRes2;
                CString sText;

                g_pBaldurChitin->GetTlkTable().Fetch(39742, strRes1); // "Spontaneous Casting"
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes2);
                sText.Format("%s: %s", strRes1.szText, strRes2.szText);

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_TOGGLEFEAT:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (a6 != 0) {
                STR_RES strRes1;
                STR_RES strRes2;
                CString sText;

                // 39853: "Stopped Using Feat"
                // 39823: "Using Feat"
                g_pBaldurChitin->GetTlkTable().Fetch(a3 == 1 ? 39853 : 39823, strRes1);
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes2);

                if (a4 > 0) {
                    sText.Format("%s: %s %d", strRes1.szText, strRes2.szText, a4);
                } else {
                    sText.Format("%s: %s", strRes1.szText, strRes2.szText);
                }

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_CLEAVE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x4) != 0) {
            STR_RES strRes;
            CString sText;
            g_pBaldurChitin->GetTlkTable().Fetch(39846, strRes); // "Cleave feat adds another level %d attack."
            sText.Format("%s (%+d)", strRes.szText, a3);
            g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                sText,
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_LINGERINGSONG:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (a6 != 0) {
                STR_RES strRes1;
                STR_RES strRes2;
                CString sText;

                g_pBaldurChitin->GetTlkTable().Fetch(39881, strRes1); // "Using Feat: Lingering Song"
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes2);
                sText.Format("%s: %s", strRes1.szText, strRes2.szText);

                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_89:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            if (a6 != 0) {
                STR_RES strRes;
                CString sText(" ");
                g_pBaldurChitin->GetTlkTable().Fetch(a6, strRes);
                sText.Format("%s", strRes.szText);
                g_pBaldurChitin->GetBaldurMessage()->DisplayText(GetName(),
                    sText,
                    rgbNameColor,
                    RGB(215, 215, 190),
                    -1,
                    m_id,
                    m_id);
            }
        }
        break;
    case FEEDBACK_CALLLIGHTNINGEND:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x2) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                41008, // "Call Lightning buildup dissipates harmlessly"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_MAGICALWEAPONINUSE:
        if ((g_pBaldurChitin->GetObjectGame()->GetOptions()->m_nEffectTextLevel & 0x10) != 0) {
            g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
                10141, // "Magical weapon in use"
                rgbNameColor,
                RGB(215, 215, 190),
                -1,
                m_id,
                m_id);
        }
        break;
    case FEEDBACK_LEAVEFAILED_LEVELUP:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            41398, // "Cannot leave area while remote player is in the level up screen."
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    case FEEDBACK_LEAVEFAILED_STORE:
        g_pBaldurChitin->GetBaldurMessage()->DisplayTextRef(GetNameRef(),
            41400, // "Cannot leave area while remote player is in the store screen."
            rgbNameColor,
            RGB(215, 215, 190),
            -1,
            m_id,
            m_id);
        break;
    }
}

// 0x737910
BOOLEAN CGameSprite::sub_737910(BOOL a1)
{
    if (!a1
        && m_equipment.m_items[m_equipment.m_selectedWeapon] != NULL
        && (m_equipment.m_items[m_equipment.m_selectedWeapon]->GetFlagsFile() & 0x10) != 0) {
        m_equipment.m_items[m_equipment.m_selectedWeapon]->m_flags |= 0x1;
        return FALSE;
    }

    if ((m_equipment.m_selectedWeapon == 43
            || m_equipment.m_selectedWeapon == 45
            || m_equipment.m_selectedWeapon == 47
            || m_equipment.m_selectedWeapon == 49)
        && m_equipment.m_items[m_equipment.m_selectedWeapon + 1] != NULL
        && (m_equipment.m_items[m_equipment.m_selectedWeapon + 1]->GetFlagsFile() & 0x10) != 0) {
        m_equipment.m_items[m_equipment.m_selectedWeapon + 1]->m_flags |= 0x1;
        return FALSE;
    }

    return TRUE;
}

// 0x73B740
SHORT CGameSprite::GetCriticalHitBonus()
{
    SHORT nCriticalHitBonus = m_derivedStats.m_nCriticalHitBonus;
    if (sub_763150(CGAMESPRITE_FEAT_IMPROVED_CRITICAL)) {
        nCriticalHitBonus++;
    }
    return nCriticalHitBonus;
}

// 0x73C6A0
SHORT CGameSprite::sub_73C6A0(CGameSprite* target, CItem* curWeapon, const ITEM_ABILITY* curAttack)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 9840
    UTIL_ASSERT(target != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 9841
    UTIL_ASSERT(curWeapon != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 9842
    UTIL_ASSERT(curAttack != NULL);

    SHORT mod = 0;

    switch (curAttack->damageType) {
    case 1:
        mod += target->GetDerivedStats()->m_nACPiercingMod;
        break;
    case 2:
        mod += target->GetDerivedStats()->m_nACCrushingMod;
        break;
    case 3:
        mod += target->GetDerivedStats()->m_nACSlashingMod;
        break;
    case 4:
        mod += target->GetDerivedStats()->m_nACMissileMod;
        mod += target->GetDerivedStats()->m_nACPiercingMod;
        break;
    case 5:
        mod += target->GetDerivedStats()->m_nACCrushingMod;
        break;
    case 6:
        mod += max(target->GetDerivedStats()->m_nACPiercingMod, target->GetDerivedStats()->m_nACCrushingMod);
        break;
    case 7:
        mod += max(target->GetDerivedStats()->m_nACPiercingMod, target->GetDerivedStats()->m_nACSlashingMod);
        break;
    case 8:
        mod += max(target->GetDerivedStats()->m_nACCrushingMod, target->GetDerivedStats()->m_nACSlashingMod);
        break;
    case 9:
        mod += target->GetDerivedStats()->m_nACMissileMod;
        mod += target->GetDerivedStats()->m_nACCrushingMod;
        break;
    }

    return mod;
}

// 0x73C7E0
SHORT CGameSprite::sub_73C7E0()
{
    SHORT mod = 0;

    if (m_typeAI.GetRace() == CAIOBJECTTYPE_R_DWARF
        && m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_DWARF_GRAY) {
        if (m_pArea != NULL
            && (m_pArea->GetHeader()->m_areaType & 0x1) != 0
            && (m_pArea->GetHeader()->m_areaType & 0x2) != 0
            && (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay()
                || g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDawn())) {
            mod -= 2;
        }
    }

    if (m_typeAI.GetRace() == CAIOBJECTTYPE_R_ELF
        && m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_ELF_DROW) {
        if (m_pArea != NULL
            && (m_pArea->GetHeader()->m_areaType & 0x1) != 0
            && (m_pArea->GetHeader()->m_areaType & 0x2) != 0
            && (g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDay()
                || g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->IsDawn())) {
            mod -= 1;
        }
    }

    return mod;
}

// 0x73C8C0
SHORT CGameSprite::sub_73C8C0(CGameSprite* target)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 9925
    UTIL_ASSERT(target != NULL);

    SHORT mod = 0;

    if (m_typeAI.GetRace() == CAIOBJECTTYPE_R_DWARF) {
        if (m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_DWARF_GOLD
            && IcewindMisc::IsAberration(target) == TRUE) {
            mod++;
        } else {
            switch (target->m_typeAI.GetRace()) {
            case CAIOBJECTTYPE_R_HOBGOBLIN:
            case CAIOBJECTTYPE_R_GOBLIN:
            case CAIOBJECTTYPE_R_ORC:
            case CAIOBJECTTYPE_R_BUGBEAR:
                mod++;
                break;
            }
        }
    }

    if (m_typeAI.GetRace() == CAIOBJECTTYPE_R_GNOME) {
        switch (target->m_typeAI.GetRace()) {
        case CAIOBJECTTYPE_R_HOBGOBLIN:
        case CAIOBJECTTYPE_R_KOBOLD:
        case CAIOBJECTTYPE_R_GOBLIN:
        case CAIOBJECTTYPE_R_BUGBEAR:
            mod++;
            break;
        }
    }

    if (m_typeAI.GetRace() == CAIOBJECTTYPE_R_GIANT
        || m_typeAI.GetRace() == CAIOBJECTTYPE_R_OGRE) {
        if ((target->m_typeAI.GetRace() == CAIOBJECTTYPE_R_GNOME
                && target->m_typeAI.m_nSubRace != CAIOBJECTTYPE_SUBRACE_DWARF_GOLD)
            || target->m_typeAI.GetRace() == CAIOBJECTTYPE_R_DWARF) {
            mod -= 4;
        }
    }

    return mod;
}

// 0x73CA20
SHORT CGameSprite::sub_73CA20(CItem* curWeapon, const ITEM_ABILITY* curAttack)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 9988
    UTIL_ASSERT(curWeapon != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 9989
    UTIL_ASSERT(curAttack != NULL);

    SHORT mod = 0;

    if (m_typeAI.GetRace() == CAIOBJECTTYPE_R_HALFLING
        && curAttack->type == 2) {
        switch (curWeapon->GetItemType()) {
        case 14:
        case 16:
        case 21:
        case 24:
        case 25:
        case 29:
            mod++;
            break;
        }
    }

    return mod;
}

// 0x73CAD0
SHORT CGameSprite::sub_73CAD0()
{
    return 0;
}

// 0x73CAE0
SHORT CGameSprite::sub_73CAE0(CItem* curWeapon, const ITEM_ABILITY* curAttack)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 10073
    UTIL_ASSERT(curAttack != NULL);

    return 0;
}

// 0x73CB10
SHORT CGameSprite::sub_73CB10(CItem* curWeapon, const ITEM_ABILITY* curAttack)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 10083
    UTIL_ASSERT(curWeapon != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 10084
    UTIL_ASSERT(curAttack != NULL);

    const CRuleTables& cRule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    if (curAttack->type == 2 || curAttack->type == 4) {
        return cRule.GetAbilityScoreModifier(m_derivedStats.m_nDEX);
    }

    WORD itemType = curWeapon->GetItemType();
    INT mod = cRule.GetAbilityScoreModifier(m_derivedStats.m_nSTR);

    if (sub_763150(CGAMESPRITE_FEAT_WEAPON_FINESSE)
        && (itemType == 19 || itemType == 16)) {
        INT nDEXMod = cRule.GetAbilityScoreModifier(m_derivedStats.m_nDEX);
        if (mod < nDEXMod) {
            mod = nDEXMod;
        }
    }

    if (sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)
        && sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK) > 0) {
        mod -= sub_726270(CGAMESPRITE_FEAT_POWER_ATTACK);
    }

    if (sub_763150(CGAMESPRITE_FEAT_EXPERTISE)
        && sub_726270(CGAMESPRITE_FEAT_EXPERTISE) > 0) {
        mod -= sub_726270(CGAMESPRITE_FEAT_EXPERTISE);
    }

    return static_cast<SHORT>(mod);
}

// 0x73CC40
SHORT CGameSprite::sub_73CC40(CItem* curWeapon, const ITEM_ABILITY* curAttack)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 10144
    UTIL_ASSERT(curWeapon != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 10145
    UTIL_ASSERT(curAttack != NULL);

    SHORT mod = 0;

    if (curAttack->thac0Bonus != SHORT_MAX) {
        mod += curAttack->thac0Bonus;
    }

    if (curAttack->type == 2 || curAttack->type == 4) {
        mod += m_derivedStats.m_nMissileTHAC0Bonus;
    }

    SHORT lSlot;
    CItem* pLauncher = GetLauncher(curAttack, lSlot);
    if (pLauncher != NULL) {
        pLauncher->Demand();
        const ITEM_ABILITY* ability = pLauncher->GetAbility(0);
        if (ability != NULL) {
            mod += ability->thac0Bonus;
        }
        pLauncher->Release();
    }

    return mod;
}

// 0x73D420
SHORT CGameSprite::sub_73D420()
{
    SHORT mod = 0;

    if ((m_derivedStats.m_generalState & STATE_BERSERK) != 0) {
        mod += 2;
    }

    mod += m_derivedStats.m_nTHAC0;

    return mod;
}

// 0x73D440
SHORT CGameSprite::sub_73D440(CItem* curWeapon)
{
    SHORT mod = 0;
    switch (curWeapon->GetItemType()) {
    case 0:
    case 28:
        mod++;
        break;
    case 15:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_BOW));
        break;
    case 16:
    case 19:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE));
        break;
    case 17:
    case 22:
    case 44:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_SIMPLE_MACE));
        break;
    case 18:
    case 24:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_SIMPLE_MISSILE));
        break;
    case 20:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_LARGESWORD));
        break;
    case 21:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_HAMMER));
        break;
    case 23:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_FLAIL));
        break;
    case 25:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_AXE));
        break;
    case 26:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF));
        break;
    case 27:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_SIMPLE_CROSSBOW));
        break;
    case 29:
    case 30:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_POLEARM));
        break;
    case 57:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_GREATSWORD));
        break;
    case 69:
        mod = static_cast<SHORT>(GetFeatValue(CGAMESPRITE_FEAT_EXOTIC_BASTARD));
        break;
    }

    if (mod < 0) {
        mod = 0;
    }

    if (m_derivedStats.m_spellStates[SPLSTATE_TENSERS_TRANSFORMATION] && mod == 0) {
        mod = 1;
    }

    return mod;
}

// 0x73EDD0
SHORT CGameSprite::MoveToObject(CGameObject* pTarget)
{
    if (m_derivedStats.m_nEncumberance == 2) {
        FeedBack(FEEDBACK_TOOHEAVY_STOPPED, 0, 0, 0, -1, 0, 0);
        return ACTION_ERROR;
    }

    if (pTarget == NULL) {
        return ACTION_ERROR;
    }

    if (pTarget->GetArea() == NULL) {
        return ACTION_DONE;
    }

    if (m_curAction.m_actionID == 263) {
        // MoveToObjectUntilSee: done once the target is seen.
        const CPoint& ptTarget = pTarget->GetPos();
        LONG nTargetX = ptTarget.x;
        LONG nTargetY = ptTarget.y;
        const CPoint& ptSelf = GetPos();
        LONG dx = ptSelf.x - nTargetX;
        LONG dy = ptSelf.y - nTargetY;
        LONG nDistance = dx * dx + dy * dy;
        LONG nRange = GetVisualRange() >> 1;

        BOOL bSeen;
        if (nRange * nRange < nDistance) {
            bSeen = FALSE;
        } else if (m_pArea->CheckLOS(pTarget->GetPos(), GetPos(), GetVisibleTerrainTable(), FALSE)) {
            bSeen = TRUE;
        } else {
            bSeen = FALSE;
        }

        if (bSeen && pTarget->GetArea() == m_pArea) {
            if (!m_lSeen.Equal(pTarget->GetAIType())) {
                if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(pTarget->GetId()) != -1) {
                    m_canBeSeen = 4 * (CGameObject::VISIBLE_DELAY + 1);
                }

                m_lSeen.Set(pTarget->GetAIType());

                CMessage* message = new CMessageSetLastObject(pTarget->GetAIType(), 0x401C, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
            return ACTION_DONE;
        }
    }

    CPoint ptWaypoint;
    pTarget->GetNextWaypoint(&ptWaypoint);

    if (m_curAction.m_actionID == 105) {
        SHORT searchSquareCode;
        CPoint ptGrid(ptWaypoint.x / CPathSearch::GRID_SQUARE_SIZEX, ptWaypoint.y / CPathSearch::GRID_SQUARE_SIZEY);
        if (m_pArea->m_search.GetLOSCost(ptGrid, m_terrainTable, searchSquareCode, FALSE) == CPathSearch::COST_IMPASSABLE) {
            return ACTION_ERROR;
        }
    }

    CPoint ptTargetGrid(ptWaypoint.x / CPathSearch::GRID_SQUARE_SIZEX, ptWaypoint.y / CPathSearch::GRID_SQUARE_SIZEY);
    const CPoint& ptSelf = GetPos();
    CPoint ptSelfGrid(ptSelf.x / CPathSearch::GRID_SQUARE_SIZEX, ptSelf.y / CPathSearch::GRID_SQUARE_SIZEY);
    LONG nSquares = CAIUtil::CountSquares(ptSelfGrid, ptTargetGrid);
    LONG nDistance = nSquares * nSquares;

    BYTE nPersonalSpace = m_animation.GetPersonalSpace();
    if (pTarget->GetObjectType() == CGameObject::TYPE_SPRITE) {
        BYTE nTargetPersonalSpace = static_cast<CGameSprite*>(pTarget)->m_animation.GetPersonalSpace();
        nPersonalSpace = static_cast<BYTE>(((nTargetPersonalSpace - 1) >> 1) + ((nPersonalSpace - 1) >> 1) - 1);
    }

    WORD nActionID = m_curAction.m_actionID;
    if (nActionID == 180) {
        // MoveToObjectFollow: stop when close.
        LONG nFollow = nPersonalSpace + 4;
        if (nDistance <= nFollow * nFollow) {
            if (m_pPath != NULL) {
                CMessage* message = new CMessageDropPath(m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }

            if (m_curAction.m_actionID != 208) {
                return ACTION_INTERRUPTABLE;
            }
            return ACTION_NORMAL;
        }
    }

    LONG nReach = nPersonalSpace + 1;
    if (nDistance <= nReach * nReach) {
        if (nActionID == 180) {
            return ACTION_INTERRUPTABLE;
        }
        return ACTION_DONE;
    }

    m_followRange = 0;

    if (ptWaypoint.x / CPathSearch::GRID_SQUARE_SIZEX == m_curDest.x / CPathSearch::GRID_SQUARE_SIZEX
        && ptWaypoint.y / CPathSearch::GRID_SQUARE_SIZEY == m_curDest.y / CPathSearch::GRID_SQUARE_SIZEY
        && field_5618
        && (m_pPath != NULL || m_currentSearchRequest != NULL)) {
        // Already on the way to the target's square.
        if (nActionID != 208) {
            return ACTION_INTERRUPTABLE;
        }
        return ACTION_NORMAL;
    }

    field_5618 = 0;

    // NOTE: The original compares the squared grid distance to the
    // waypoint with 16 squared (0x85BB7C, 0x73F31F) and then runs the same
    // code on both arms (0x73F32F and 0x73F40B); one arm here.

    // A new path every eighth call (the WORD at 0x85BBA4).
    if (field_5616 % 8 == 0) {
        m_curDest.x = ptWaypoint.x;
        m_curDest.y = ptWaypoint.y;

        CSearchRequest* pSearchRequest = new CSearchRequest();
        if (pSearchRequest == NULL) {
            return ACTION_ERROR;
        }

        pSearchRequest->m_searchBitmap = &(m_pArea->m_search);
        memcpy(pSearchRequest->m_terrainTable, m_terrainTable, sizeof(m_terrainTable));
        pSearchRequest->m_pathSmooth = m_animation.GetPathSmooth();
        pSearchRequest->m_sourceId = m_id;
        pSearchRequest->m_nTargetIds = 1;

        pSearchRequest->m_targetIds = new LONG[1];
        if (pSearchRequest->m_targetIds == NULL) {
            delete pSearchRequest;
            return ACTION_ERROR;
        }

        pSearchRequest->m_targetIds[0] = pTarget->GetId();
        pSearchRequest->m_sourceSide = m_typeAI.GetEnemyAlly();
        field_5618 = 1;
        SetTarget(pSearchRequest, FALSE, LIST_FRONT);
    }

    field_5616++;

    if (m_curAction.m_actionID != 208) {
        return ACTION_INTERRUPTABLE;
    }
    return ACTION_NORMAL;
}

// 0x73F560
SHORT CGameSprite::MoveToPoint()
{
    LONG x = m_curAction.m_dest.x;
    LONG y = m_curAction.m_dest.y;
    if (m_curAction.GetActionID() == CAIAction::TIMEDMOVETOPOINT) {
        LONG n = m_curAction.m_specificID - 1;
        if (n < 1) {
            m_curAction.m_actionID = CAIAction::JUMPTOPOINT;
            return JumpToPoint(m_curAction.m_dest, TRUE);
        }
        m_curAction.m_specificID = n;
    }

    if (m_derivedStats.m_nEncumberance == 2) {
        FeedBack(FEEDBACK_TOOHEAVY_STOPPED, 0, 0, 0, -1, 0, 0);
        return ACTION_ERROR;
    }

    if (x == -1) {
        x = m_posStart.x;
    } else if (x == -2) {
        x = m_baseStats.field_2E4;
    }

    if (y == -1) {
        y = m_posStart.y;
    } else if (y == -2) {
        y = m_baseStats.field_2E6;
    }

    if (x / CPathSearch::GRID_SQUARE_SIZEX == m_pos.x / CPathSearch::GRID_SQUARE_SIZEX
        && y / CPathSearch::GRID_SQUARE_SIZEY == m_pos.y / CPathSearch::GRID_SQUARE_SIZEY) {
        return ACTION_DONE;
    }

    if (x < 0
        || x >= m_pArea->GetInfinity()->nAreaX
        || y < 0
        || y >= m_pArea->GetInfinity()->nAreaY) {
        return ACTION_ERROR;
    }

    if (m_pPath == NULL
        && m_currentSearchRequest == NULL
        && m_actionCount > 0
        && m_curAction.m_specificID != CAIAction::BACKGROUND) {
        m_curAction.m_specificID2++;
        if (m_curAction.m_specificID2 > 4) {
            return ACTION_ERROR;
        }

        SHORT searchSquareCode;
        if (m_pArea->m_search.GetLOSCost(CPoint(x / CPathSearch::GRID_SQUARE_SIZEX, y / CPathSearch::GRID_SQUARE_SIZEY), m_terrainTable, searchSquareCode, FALSE) == CPathSearch::COST_IMPASSABLE) {
            return ACTION_ERROR;
        }

        m_curDest.x = x;
        m_curDest.y = y;

        if (!m_interrupt) {
            if (m_curAction.GetActionID() == CAIAction::MOVETOPOINTNORECTICLE) {
                CAIAction action(CAIAction::MOVETOPOINTNORECTICLE,
                    CPoint(x, y),
                    m_curAction.m_specificID,
                    m_curAction.m_specificID2);
                AddAction(action);
            } else {
                CAIAction action(CAIAction::MOVETOPOINT,
                    CPoint(x, y),
                    m_curAction.m_specificID,
                    m_curAction.m_specificID2);
                AddAction(action);
            }

            CAIAction action(CAIAction::SMALLWAIT,
                CPoint(-1, -1),
                rand() % 8 + 1,
                -1);
            AddAction(action);
        }

        return ACTION_DONE;
    }

    if (m_pPath == NULL
        && m_currentSearchRequest == NULL
        && m_actionCount > 0
        && m_curAction.m_specificID == CAIAction::BACKGROUND) {
        return ACTION_ERROR;
    }

    if (x != m_curDest.x || y != m_curDest.y) {
        m_curDest.x = x;
        m_curDest.y = y;

        CSearchRequest* pSearchRequest = new CSearchRequest();
        if (pSearchRequest == NULL) {
            return ACTION_ERROR;
        }

        pSearchRequest->m_searchBitmap = &(m_pArea->m_search);
        if (m_animation.GetListType() == LIST_FLIGHT) {
            memcpy(pSearchRequest->m_terrainTable, m_flightTerrainTable, sizeof(m_flightTerrainTable));
        } else {
            memcpy(pSearchRequest->m_terrainTable, m_terrainTable, sizeof(m_terrainTable));
        }

        pSearchRequest->m_removeSelf = m_animation.GetListType() != LIST_FLIGHT;
        pSearchRequest->m_pathSmooth = m_animation.GetPathSmooth();
        pSearchRequest->m_sourceId = m_id;
        pSearchRequest->m_nTargetPoints = 1;
        pSearchRequest->m_exclusiveTargetPoints = TRUE;

        pSearchRequest->m_targetPoints = new POINT[pSearchRequest->m_nTargetPoints];
        if (pSearchRequest->m_targetPoints == NULL) {
            delete pSearchRequest;
            return ACTION_ERROR;
        }

        if (m_pArea->m_pGame->GetGroup()->InList(m_id)) {
            pSearchRequest->m_nPartyIds = m_pArea->m_pGame->GetGroup()->GetCount();
            pSearchRequest->m_partyIds = m_pArea->m_pGame->GetGroup()->GetGroupList();
        }

        pSearchRequest->m_targetPoints[0] = m_curDest;
        pSearchRequest->m_sourceSide = m_typeAI.GetEnemyAlly();

        if (m_curAction.m_specificID == CAIAction::BACKGROUND) {
            pSearchRequest->m_frontList = 2;
            pSearchRequest->m_minNodesBack = 110;
            pSearchRequest->m_sourcePt.x = m_pos.x / CPathSearch::GRID_SQUARE_SIZEX;
            pSearchRequest->m_sourcePt.y = m_pos.y / CPathSearch::GRID_SQUARE_SIZEY;
            pSearchRequest->m_maxNodesBack = CSearchRequest::MINNODESBACK;
            SetTarget(pSearchRequest, FALSE, LIST_FLIGHT);
        } else {
            SetTarget(pSearchRequest, FALSE, LIST_FRONT);
        }
    }

    if (m_curAction.GetActionID() == 207) {
        return ACTION_NORMAL;
    }

    return ACTION_INTERRUPTABLE;
}

// 0x73FEC0
SHORT CGameSprite::MoveToPoint(const CPoint& dest, LONG range)
{
    if (range == 0) {
        if (dest.x / CPathSearch::GRID_SQUARE_SIZEX == m_pos.x / CPathSearch::GRID_SQUARE_SIZEX
            && dest.y / CPathSearch::GRID_SQUARE_SIZEY == m_pos.y / CPathSearch::GRID_SQUARE_SIZEY) {
            return ACTION_DONE;
        }
    } else {
        LONG dx = dest.x / CPathSearch::GRID_SQUARE_SIZEX - m_pos.x / CPathSearch::GRID_SQUARE_SIZEX;
        // NOTE: The original's y term is 1 when the rows are equal and 0
        // otherwise (neg/sbb/inc at 0x73FF66), not the row difference; kept.
        LONG dy = (dest.y / CPathSearch::GRID_SQUARE_SIZEY - m_pos.y / CPathSearch::GRID_SQUARE_SIZEY) == 0 ? 1 : 0;
        if (dy * dy + dx * dx <= range * range) {
            return ACTION_DONE;
        }
    }

    if (dest.x < 0
        || dest.x >= m_pArea->GetInfinity()->nAreaX
        || dest.y < 0
        || dest.y >= m_pArea->GetInfinity()->nAreaY) {
        return ACTION_ERROR;
    }

    if (m_pPath == NULL
        && m_currentSearchRequest == NULL
        && m_moveCount > 0
        && m_curAction.m_specificID == CAIAction::BACKGROUND) {
        return ACTION_ERROR;
    }

    if (dest.x == m_curDest.x && dest.y == m_curDest.y) {
        return ACTION_INTERRUPTABLE;
    }

    m_curDest.x = dest.x;
    m_curDest.y = dest.y;

    CSearchRequest* pSearchRequest = new CSearchRequest();
    if (pSearchRequest == NULL) {
        return ACTION_ERROR;
    }

    pSearchRequest->m_searchBitmap = &(m_pArea->m_search);
    if (m_animation.GetListType() == LIST_FLIGHT) {
        memcpy(pSearchRequest->m_terrainTable, m_flightTerrainTable, sizeof(m_flightTerrainTable));
    } else {
        memcpy(pSearchRequest->m_terrainTable, m_terrainTable, sizeof(m_terrainTable));
    }

    pSearchRequest->m_removeSelf = m_animation.GetListType() != LIST_FLIGHT;
    pSearchRequest->m_pathSmooth = m_animation.GetPathSmooth();
    pSearchRequest->m_sourceId = m_id;
    pSearchRequest->m_nTargetPoints = 1;
    pSearchRequest->m_exclusiveTargetPoints = TRUE;

    pSearchRequest->m_targetPoints = new POINT[1];
    if (pSearchRequest->m_targetPoints == NULL) {
        delete pSearchRequest;
        return ACTION_ERROR;
    }

    if (m_pArea->m_pGame->GetGroup()->InList(m_id)) {
        pSearchRequest->m_nPartyIds = static_cast<BYTE>(m_pArea->m_pGame->GetGroup()->GetCount());
        pSearchRequest->m_partyIds = m_pArea->m_pGame->GetGroup()->GetGroupList();
    }

    pSearchRequest->m_targetPoints[0] = m_curDest;
    pSearchRequest->m_sourceSide = m_typeAI.GetEnemyAlly();

    if (m_curAction.m_specificID == CAIAction::BACKGROUND) {
        pSearchRequest->m_frontList = LIST_FLIGHT;
        pSearchRequest->m_minNodesBack = 110;
        pSearchRequest->m_sourcePt.x = m_pos.x / CPathSearch::GRID_SQUARE_SIZEX;
        pSearchRequest->m_sourcePt.y = m_pos.y / CPathSearch::GRID_SQUARE_SIZEY;
        pSearchRequest->m_maxNodesBack = CSearchRequest::MINNODESBACK;
        SetTarget(pSearchRequest, FALSE, LIST_FLIGHT);
    } else {
        SetTarget(pSearchRequest, FALSE, LIST_FRONT);
    }

    return ACTION_INTERRUPTABLE;
}

// 0x7446F0
SHORT CGameSprite::OneSwing()
{
    SHORT returnValue = ACTION_NORMAL;
    CMessage* message;

    if (m_attackFrame == 0) {
        m_speedFactor = 5;
        m_speedFactor += rand() % 6;
        m_speedFactor -= 3;
        if (m_speedFactor < 0) {
            m_speedFactor = 0;
        } else if (m_speedFactor > 10) {
            m_speedFactor = 10;
        }
    }

    BYTE frameType;
    if (m_derivedStats.m_nNumberOfAttacks > 0) {
        frameType = m_animation.GetAttackFrameType(static_cast<BYTE>(m_derivedStats.m_nNumberOfAttacks),
            static_cast<BYTE>(m_speedFactor),
            static_cast<BYTE>(m_attackFrame));
    } else {
        frameType = 0;
    }

    switch (frameType) {
    case 0:
        if (m_nSequence == GetIdleSequence()) {
            message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()), m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_INTERRUPTABLE;
        break;
    case 6:
        if (m_nSequence != SEQ_ATTACK) {
            message = new CMessageSetSequence(SEQ_ATTACK, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        break;
    case 7:
        if (m_nSequence != SEQ_ATTACK) {
            message = new CMessageSetSequence(SEQ_ATTACK, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_NORMAL;
        break;
    case 9:
        if (m_nSequence != SEQ_ATTACK) {
            message = new CMessageSetSequence(SEQ_ATTACK, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
        returnValue = ACTION_NORMAL;
        break;
    case 15:
        returnValue = ACTION_DONE;
        break;
    }

    return returnValue;
}

// 0x7449D0
SHORT CGameSprite::Recoil()
{
    CMessage* message;

    if (m_recoilFrame == 0 && m_baseStats.m_hitPoints == 0) {
        m_endOfDamageSeq = FALSE;
        m_recoilFrame = 0;
        return ACTION_DONE;
    }

    if (m_nSequence != SEQ_DAMAGE) {
        message = new CMessageSetSequence(SEQ_DAMAGE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    m_recoilFrame++;
    if (!m_endOfDamageSeq) {
        return ACTION_NORMAL;
    }

    m_endOfDamageSeq = FALSE;
    m_recoilFrame = 0;

    message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()), m_id, m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    return ACTION_DONE;
}

// 0x744B20
SHORT CGameSprite::PlayDead()
{
    CMessage* message;

    if (m_nSequence != SEQ_DIE || m_nSequence != SEQ_TWITCH) {
        message = new CMessageSetSequence(SEQ_DIE, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (m_actionCount == 0) {
        PlaySound(SOUND_DYING, TRUE, FALSE, FALSE);
    }

    if (m_curAction.m_specificID > 0) {
        m_curAction.m_specificID--;
        return ACTION_NORMAL;
    }

    message = new CMessageSetSequence(SEQ_AWAKE, m_id, m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    return ACTION_DONE;
}

// 0x745950
SHORT CGameSprite::JumpToPoint(CPoint dest, BOOL spriteUpdate)
{
    CPoint ptGrid(dest.x / CPathSearch::GRID_SQUARE_SIZEX, dest.y / CPathSearch::GRID_SQUARE_SIZEY);
    CPoint ptDest = dest;

    if (m_pArea == NULL) {
        return ACTION_ERROR;
    }

    // NOTE: A point past the area's right or bottom edge goes to 0, not to
    // the edge; the original compares with the low words of nAreaX and
    // nAreaY.
    if (static_cast<SHORT>(m_pArea->GetInfinity()->nAreaX) < dest.x) {
        dest.x = 0;
    }

    if (static_cast<SHORT>(m_pArea->GetInfinity()->nAreaY) < dest.y) {
        dest.y = 0;
    }

    BOOL bMoved = FALSE;
    CPoint ptSearch(dest.x / CPathSearch::GRID_SQUARE_SIZEX, dest.y / CPathSearch::GRID_SQUARE_SIZEY);

    if (m_pPath != NULL) {
        CMessage* message = new CMessageDropPath(m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    switch (m_listType) {
    case CGAMEOBJECT_LIST_FRONT:
        if (m_active && m_activeAI && m_activeImprisonment) {
            m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                               m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
        }
        break;
    case CGAMEOBJECT_LIST_BACK:
        if (m_active
            && m_activeAI
            && m_activeImprisonment
            && (m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
            m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                               m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
        }
        break;
    case CGAMEOBJECT_LIST_FLIGHT:
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 14822
        UTIL_ASSERT(FALSE);
    }

    CPoint ptOpen = ptSearch;
    if (!m_derivedStats.m_bDoNotJump) {
        USHORT animationID = m_animation.GetAnimationId();
        if ((animationID & 0xF000) == 0x4000 && animationID >= 0x4400) {
            ptOpen = m_pArea->m_search.GetNearestOpenSquare(ptSearch,
                m_visibleTerrainTable,
                m_animation.GetPersonalSpace(),
                -1);
        } else {
            ptOpen = m_pArea->m_search.GetNearestOpenSquare(ptSearch,
                m_terrainTable,
                m_animation.GetPersonalSpace(),
                -1);
        }
    }

    // NOTE: The comparison is with the square of the destination as given,
    // before the edge check.
    if (ptOpen.x == ptGrid.x && ptOpen.y == ptGrid.y) {
        m_pos.x = ptDest.x;
        m_pos.y = ptDest.y;
    } else {
        bMoved = TRUE;
        m_pos.x = ptOpen.x * CPathSearch::GRID_SQUARE_SIZEX;
        m_pos.y = ptOpen.y * CPathSearch::GRID_SQUARE_SIZEY;
    }

    m_posExact.x = m_pos.x << EXACT_SCALE;
    m_posExact.y = (m_pos.y << (EXACT_SCALE + 2)) / 3;

    InitBlur();

    switch (m_listType) {
    case CGAMEOBJECT_LIST_FRONT:
        if (m_active && m_activeAI && m_activeImprisonment) {
            m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                            m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
        }
        break;
    case CGAMEOBJECT_LIST_BACK:
        if (m_active
            && m_activeAI
            && m_activeImprisonment
            && (m_derivedStats.m_generalState & STATE_SLEEPING) != 0) {
            m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                            m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
        }
        break;
    case CGAMEOBJECT_LIST_FLIGHT:
        break;
    default:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 14869
        UTIL_ASSERT(FALSE);
    }

    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1
        && (!g_pBaldurChitin->GetObjectGame()->GetGameSave()->field_1AC || InControl())) {
        m_pArea->m_visibility.UpDate(m_posLastVisMapEntry,
            m_pos,
            m_id,
            m_visibleTerrainTable);
        m_posLastVisMapEntry = m_pos;
    }

    if ((m_derivedStats.m_generalState & STATE_SILENCED) == 0
        && (g_pBaldurChitin->GetObjectGame()->GetOptions()->m_bFootStepsSounds
            || g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1)
        && m_nSequence != CGAMESPRITE_SEQ_DIE
        && m_nSequence != CGAMESPRITE_SEQ_TWITCH
        && m_nSequence != CGAMESPRITE_SEQ_SLEEP
        && m_nSequence != CGAMESPRITE_SEQ_WALK
        && m_nSequence != CGAMESPRITE_SEQ_AWAKE) {
        if (!m_sndReady.IsSoundPlaying()) {
            char* pSndReady = m_animation.GetSndReady();
            if (*pSndReady != '\0') {
                m_sndReady.SetResRef(CResRef(pSndReady), TRUE, TRUE);
                delete pSndReady;
                m_sndReady.SetLoopingFlag(1);
                m_sndReady.Play(m_pos.x, m_pos.y, m_posZ, FALSE);
            }
        }
    } else {
        m_sndReady.Stop();
    }

    if (spriteUpdate && InControl()) {
        CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE
        && g_pBaldurChitin->GetActiveEngine() == g_pBaldurChitin->m_pEngineMap
        && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) != -1
        && bMoved) {
        BYTE nPortrait = static_cast<BYTE>(g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id));
        CUIControlButtonMapAreaMap* pMap = static_cast<CUIControlButtonMapAreaMap*>(g_pBaldurChitin->m_pEngineMap->GetManager()->GetPanel(2)->GetControl(2));
        pMap->field_7DA |= 1 << nPortrait;
    }

    return ACTION_DONE;
}

// 0x7462D0
SHORT CGameSprite::Face()
{
    SetDirection(static_cast<SHORT>(m_curAction.m_specificID));

    if (m_pos.x != -1 || m_pos.y != -1 || m_pArea != NULL) {
        if (InControl()) {
            CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    return ACTION_DONE;
}

// 0x746390
SHORT CGameSprite::FaceObject(CGameAIBase* pObject)
{
    if (pObject == NULL) {
        return ACTION_ERROR;
    }

    SetDirection(GetDirection(pObject->GetPos()));

    if (m_pos.x != -1 || m_pos.y != -1 || m_pArea != NULL) {
        if (InControl()) {
            CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    }

    return ACTION_DONE;
}

// 0x751CD0
SHORT CGameSprite::LeaveParty()
{
    // TODO: Incomplete.

    return ACTION_DONE;
}

// 0x74F6D0
void CGameSprite::MoveGlobal(const CString& sArea, const CPoint& ptStart)
{
    // TODO: Incomplete.
}

// 0x74FC50
SHORT CGameSprite::GroupAttack(CGameSprite* pTarget)
{
    if (pTarget == NULL
        || !pTarget->m_active
        || !pTarget->m_activeAI
        || !pTarget->m_activeImprisonment) {
        return ACTION_DONE;
    }

    if (m_interrupt) {
        return ACTION_INTERRUPTABLE;
    }

    CAIObjectType typeAI(GetAIType());
    typeAI.SetName(CString(""));
    typeAI.SetInstance(-1);
    m_curAction.m_acteeID.Set(typeAI);
    AddAction(m_curAction);
    AddAction(CAIAction(CAIAction::ATTACK, m_curAction.m_acteeID, 0, 0, 0));

    return ACTION_DONE;
}

// 0x750DA0
SHORT CGameSprite::Enemy()
{
    m_typeAI.SetEnemyAlly(CAIObjectType::EA_ENEMY);
    m_liveTypeAI.SetEnemyAlly(CAIObjectType::EA_ENEMY);
    m_startTypeAI.SetEnemyAlly(CAIObjectType::EA_ENEMY);

    if (InControl()) {
        CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    m_equipedEffectList.RemoveAllOfType(this, CGAMEEFFECT_CHARM, m_equipedEffectList.m_posCurrent, -1);
    m_equipedEffectList.RemoveAllOfType(this, ICEWIND_CGAMEEFFECT_CHARM, m_equipedEffectList.m_posCurrent, -1);
    m_timedEffectList.RemoveAllOfType(this, CGAMEEFFECT_CHARM, m_timedEffectList.m_posCurrent, -1);
    m_timedEffectList.RemoveAllOfType(this, ICEWIND_CGAMEEFFECT_CHARM, m_timedEffectList.m_posCurrent, -1);
    m_timedEffectList.RemoveAllOfType(this, CGAMEEFFECT_SETAISCRIPT, m_timedEffectList.m_posCurrent, -1);
    m_equipedEffectList.RemoveAllOfType(this, CGAMEEFFECT_SETAISCRIPT, m_equipedEffectList.m_posCurrent, -1);
    g_pBaldurChitin->GetObjectGame()->RemoveCharacterFromAllies(m_id);
    g_pBaldurChitin->GetObjectGame()->RemoveCharacterFromFamiliars(m_id);

    return ACTION_DONE;
}

// NOTE: Inlined in `Dialog` and `PlayerDialog` after a dialog starts: the
// target's dialog actions are cleared, both sides count the talk, and both
// get the Said trigger (0x75307D-0x7532B8 and 0x753B5F-0x753D7E).
static void DialogStarted(CGameSprite* pSprite, CGameSprite* pTarget)
{
    CMessage* message = new CMessageClearDialogActions(pTarget->GetId(), pTarget->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    if (pSprite->GetAIType().m_nEnemyAlly != CAIObjectType::EA_PC
        && (pSprite->m_derivedStats.m_generalState & STATE_CHARMED) == 0) {
        message = new CMessageSetNumTimesTalkedTo(pSprite->m_nNumberOfTimesTalkedTo + 1,
            pSprite->GetId(),
            pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    // NOTE: The charm test is the speaker's, for the target too (0x753111).
    if (pTarget->GetAIType().m_nEnemyAlly != CAIObjectType::EA_PC
        && (pSprite->m_derivedStats.m_generalState & STATE_CHARMED) == 0) {
        message = new CMessageSetNumTimesTalkedTo(pTarget->m_nNumberOfTimesTalkedTo + 1,
            pSprite->GetId(),
            pTarget->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    CAITrigger targetSaid(CAITRIGGER_SAID, pTarget->GetAIType(), 0);
    message = new CMessageSetTrigger(targetSaid, pSprite->GetId(), pSprite->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CAITrigger spriteSaid(CAITRIGGER_SAID, pSprite->GetAIType(), 0);
    message = new CMessageSetTrigger(spriteSaid, pSprite->GetId(), pTarget->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
}

// 0x752DD0
SHORT CGameSprite::Dialog(CGameSprite* pTarget)
{
    if (pTarget == NULL) {
        return ACTION_ERROR;
    }

    if (!CanSpeak(FALSE, FALSE)) {
        return ACTION_ERROR;
    }

    if (!pTarget->CanSpeak(FALSE, TRUE)) {
        return ACTION_ERROR;
    }

    if ((m_derivedStats.m_generalState & STATE_CHARMED) != 0) {
        return ACTION_ERROR;
    }

    if (m_dialog == "" && (m_baseStats.m_flags & 0x800) != 0
        && g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == -1) {
        m_dialog = "multig";
    }

    if (m_actionCount == 0) {
        // NOTE: The original passes the speaker itself (0x752E89).
        PlayDialogSound(this);
    }

    const CPoint& ptTarget = pTarget->GetPos();
    CPoint ptTargetGrid(ptTarget.x / CPathSearch::GRID_SQUARE_SIZEX, ptTarget.y / CPathSearch::GRID_SQUARE_SIZEY);
    const CPoint& ptSelf = GetPos();
    CPoint ptSelfGrid(ptSelf.x / CPathSearch::GRID_SQUARE_SIZEX, ptSelf.y / CPathSearch::GRID_SQUARE_SIZEY);
    LONG nSquares = CAIUtil::CountSquares(ptSelfGrid, ptTargetGrid);
    LONG nDistance = nSquares * nSquares;

    LONG nRange;
    if (field_7106 == 0) {
        BYTE nPersonalSpace = m_animation.GetPersonalSpace();
        BYTE nTargetPersonalSpace = pTarget->m_animation.GetPersonalSpace();
        nRange = ((nTargetPersonalSpace - 1) >> 1) + ((nPersonalSpace - 1) >> 1) - 1;
    } else {
        nRange = field_7106 * field_7106;
    }

    SHORT returnValue;
    if (field_7106 != 0) {
        if (nDistance <= nRange) {
            goto talk;
        }
        goto walk;
    }

    if (nDistance > (nRange + 2) * (nRange + 2)) {
        goto walk;
    }

    if (!m_pArea->CheckLOS(pTarget->GetPos(), GetPos(), m_visibleTerrainTable, pTarget->Orderable(FALSE))) {
        goto walk;
    }

talk:
    m_lTalkedTo.Set(pTarget->GetAIType());
    pTarget->m_lTalkedTo.Set(GetAIType());

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        BYTE rc = g_pBaldurChitin->m_cBaldurMessage.SendDialogRequestToServer(FALSE);
        if (rc == CBaldurMessage::DIALOG_REQUESTPENDING) {
            return ACTION_NORMAL;
        }
        if (rc == 0 || rc == CBaldurMessage::DIALOG_NOT_PERMITTED) {
            return ACTION_ERROR;
        }
    }

    if (!g_pBaldurChitin->m_pEngineWorld->StartDialog(pTarget, this, FALSE, FALSE)) {
        return ACTION_ERROR;
    }

    DialogStarted(this, pTarget);
    return ACTION_DONE;

walk:
    returnValue = MoveToObject(pTarget);
    if (returnValue == ACTION_DONE) {
        returnValue = ACTION_INTERRUPTABLE;
    }

    if (returnValue == ACTION_ERROR
        && m_pArea->CheckLOS(pTarget->GetPos(), GetPos(), m_visibleTerrainTable, pTarget->Orderable(FALSE))) {
        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            BYTE rc = g_pBaldurChitin->m_cBaldurMessage.SendDialogRequestToServer(FALSE);
            if (rc == CBaldurMessage::DIALOG_REQUESTPENDING) {
                return ACTION_NORMAL;
            }
            if (rc == 0 || rc == CBaldurMessage::DIALOG_NOT_PERMITTED) {
                return ACTION_ERROR;
            }
        }

        if (g_pBaldurChitin->m_pEngineWorld->StartDialog(pTarget, this, FALSE, FALSE)) {
            DialogStarted(this, pTarget);
            return ACTION_DONE;
        }

        returnValue = ACTION_ERROR;
    }

    return returnValue;
}

// NOTE: Inlined in `PlayerDialog` (0x7539CE-0x753AD1 and its second copy):
// a speaker who cannot be seen becomes visible, and a sanctuary ends.
static void PlayerDialogReveal(CGameSprite* pSprite)
{
    ITEM_EFFECT effect;

    if (!pSprite->IsVisibleToCaller(FALSE)) {
        CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_FORCEVISIBLE);
        effect.durationType = 1;
        CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, pSprite->m_pos, pSprite->GetId(), CPoint(-1, -1));
        CMessage* message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (pSprite->m_derivedStats.m_spellStates[SPLSTATE_SANCTUARY]) {
        CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_DISPELSANCTUARY);
        effect.durationType = 1;
        CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect, pSprite->m_pos, pSprite->GetId(), CPoint(-1, -1));
        CMessage* message = new CMessageAddEffect(pEffect, pSprite->GetId(), pSprite->GetId());
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }
}

// 0x7537A0
SHORT CGameSprite::PlayerDialog(CGameSprite* pTarget)
{
    if (pTarget == NULL) {
        return ACTION_ERROR;
    }

    if (pTarget->m_bEscapingArea == TRUE) {
        return ACTION_ERROR;
    }

    if (pTarget->m_curAction.m_actionID == 108 || pTarget->m_curAction.m_actionID == 176) {
        return ACTION_ERROR;
    }

    if (m_currentActionId == 108 || m_currentActionId == 176) {
        return ACTION_ERROR;
    }

    if (!CanSpeak(FALSE, FALSE)) {
        return ACTION_ERROR;
    }

    if (!pTarget->CanSpeak(FALSE, TRUE)) {
        return ACTION_ERROR;
    }

    if (pTarget->m_moraleFailure) {
        return ACTION_ERROR;
    }

    DWORD dwState = pTarget->m_derivedStats.m_generalState;
    if ((dwState & (STATE_STUNNED | STATE_PANIC)) != 0) {
        return ACTION_ERROR;
    }

    if ((dwState & STATE_BERSERK) != 0 && m_berserkActive) {
        return ACTION_ERROR;
    }

    if ((dwState & (STATE_CONFUSED | STATE_FEEBLEMINDED)) != 0) {
        return ACTION_ERROR;
    }

    if (m_actionCount == 0) {
        PlayDialogSound(pTarget);
    }

    const CPoint& ptTarget = pTarget->GetPos();
    CPoint ptTargetGrid(ptTarget.x / CPathSearch::GRID_SQUARE_SIZEX, ptTarget.y / CPathSearch::GRID_SQUARE_SIZEY);
    const CPoint& ptSelf = GetPos();
    CPoint ptSelfGrid(ptSelf.x / CPathSearch::GRID_SQUARE_SIZEX, ptSelf.y / CPathSearch::GRID_SQUARE_SIZEY);
    LONG nSquares = CAIUtil::CountSquares(ptSelfGrid, ptTargetGrid);
    LONG nDistance = nSquares * nSquares;

    BYTE nPersonalSpace = m_animation.GetPersonalSpace();
    BYTE nTargetPersonalSpace = pTarget->m_animation.GetPersonalSpace();
    // NOTE: A byte sum here, an int one in `Dialog` (0x75397B).
    BYTE nRange = static_cast<BYTE>(((nTargetPersonalSpace - 1) >> 1) + ((nPersonalSpace - 1) >> 1) - 1);

    SHORT returnValue;
    if (nDistance <= (nRange + 2) * (nRange + 2)
        && m_pArea->CheckLOS(pTarget->GetPos(), GetPos(), m_visibleTerrainTable, pTarget->Orderable(FALSE))) {
        PlayerDialogReveal(this);

        m_lTalkedTo.Set(pTarget->GetAIType());
        pTarget->m_lTalkedTo.Set(GetAIType());

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            BYTE rc = g_pBaldurChitin->m_cBaldurMessage.SendDialogRequestToServer(TRUE);
            if (rc == CBaldurMessage::DIALOG_REQUESTPENDING) {
                return ACTION_NORMAL;
            }
            if (rc == CBaldurMessage::DIALOG_NOT_PERMITTED) {
                goto denied;
            }
            if (rc == 0) {
                return ACTION_ERROR;
            }
        }

        if (!g_pBaldurChitin->m_pEngineWorld->StartDialog(this, pTarget, TRUE, FALSE)) {
            return ACTION_ERROR;
        }

        DialogStarted(this, pTarget);
        return ACTION_DONE;
    }

    returnValue = MoveToObject(pTarget);
    if (returnValue == ACTION_DONE) {
        returnValue = ACTION_INTERRUPTABLE;
    }

    if (returnValue == ACTION_ERROR
        && m_pArea->CheckLOS(pTarget->GetPos(), GetPos(), m_visibleTerrainTable, pTarget->Orderable(FALSE))) {
        PlayerDialogReveal(this);

        if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            BYTE rc = g_pBaldurChitin->m_cBaldurMessage.SendDialogRequestToServer(TRUE);
            if (rc == CBaldurMessage::DIALOG_REQUESTPENDING) {
                return ACTION_NORMAL;
            }
            if (rc == CBaldurMessage::DIALOG_NOT_PERMITTED) {
                goto denied;
            }
            if (rc == 0) {
                return ACTION_ERROR;
            }
        }

        if (g_pBaldurChitin->m_pEngineWorld->StartDialog(this, pTarget, TRUE, FALSE)) {
            DialogStarted(this, pTarget);
            return ACTION_DONE;
        }

        returnValue = ACTION_ERROR;
    }

    return returnValue;

denied:
    DisplayTextRef(GetNameRef(), 10945, 0xFF, 0xFF);
    return ACTION_ERROR;
}

// 0x754870
SHORT CGameSprite::SetDialog()
{
    m_dialog = m_curAction.GetString1();

    if (g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
        CMessage* message = new CMessageSetDialogResRef(CResRef(m_curAction.GetString1()), m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    return ACTION_DONE;
}

// 0x74F830
void CGameSprite::SelectWeaponAbility(unsigned char buttonNum, unsigned char abilityNum, unsigned char feedBack, unsigned char retry)
{
    BYTE nOldSlot = m_equipment.m_selectedWeapon;
    BYTE nOldAbility = static_cast<BYTE>(m_equipment.m_selectedWeaponAbility);

    CItem* pOldItem = m_equipment.m_items[m_equipment.m_selectedWeapon];
    if (pOldItem != NULL) {
        if ((pOldItem->GetFlagsFile() & 0x10) != 0 && feedBack) {
            FeedBack(FEEDBACK_ITEMCURSED, 0, 0, 0, -1, 0, 0);
            return;
        }

        pOldItem->Demand();

        ITEM_ABILITY* ability = pOldItem->GetAbility(m_equipment.m_selectedWeaponAbility);
        SHORT launcherSlot;
        if (ability != NULL) {
            CItem* pLauncher = GetLauncher(ability, launcherSlot);
            if (pLauncher != NULL) {
                pLauncher->Unequip(this, launcherSlot, TRUE, FALSE);
            }
        }

        pOldItem->Unequip(this, m_equipment.m_selectedWeapon, TRUE, FALSE);
        pOldItem->Release();
    }

    m_equipment.m_selectedWeapon = buttonNum;
    m_nTempSelectedWeapon = buttonNum;
    m_nTempSelectedWeaponAbility = abilityNum;

    BOOL bEquipped = FALSE;
    m_equipment.m_selectedWeaponAbility = abilityNum;

    CItem* pItem = m_equipment.m_items[buttonNum];
    if (pItem != NULL) {
        pItem->Demand();

        ITEM_ABILITY* ability = pItem->GetAbility(m_equipment.m_selectedWeaponAbility);
        if (ability != NULL) {
            if ((ability->type & 0xFF) == 4) {
                for (INT nSlot = CGameSpriteEquipment::SLOT_AMMO; nSlot < CGameSpriteEquipment::SLOT_AMMO + 4; nSlot++) {
                    CItem* pAmmo = m_equipment.m_items[nSlot];
                    if (pAmmo != NULL) {
                        pAmmo->Demand();

                        for (INT nAbility = 0; nAbility < 3; nAbility++) {
                            ITEM_ABILITY* ammoAbility = pAmmo->GetAbility(nAbility);
                            if (ammoAbility != NULL
                                && (ammoAbility->type & 0xFF) != 4
                                && CheckLauncherType(ammoAbility, pItem)) {
                                SelectWeaponAbility(static_cast<BYTE>(nSlot),
                                    static_cast<BYTE>(nAbility),
                                    feedBack,
                                    TRUE);
                                pAmmo->Release();
                                pItem->Release();
                                return;
                            }
                        }

                        pAmmo->Release();
                    }
                }
            } else {
                if (CheckLauncherType(ability, NULL)) {
                    SHORT launcherSlot;
                    CItem* pLauncher = GetLauncher(ability, launcherSlot);
                    if (pLauncher != NULL) {
                        pLauncher->Equip(this, launcherSlot, FALSE);
                    }

                    pItem->Equip(this, m_equipment.m_selectedWeapon, FALSE);
                    bEquipped = TRUE;
                }
            }
        }

        pItem->Release();
    }

    if (bEquipped) {
        if (InControl() && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
            CMessage* message = new CMessageSpriteEquipment(this, m_id, m_id);
            g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
        }
    } else {
        if (retry) {
            SelectWeaponAbility(nOldSlot, nOldAbility, feedBack, FALSE);
        } else {
            if (nOldSlot == CGameSpriteEquipment::SLOT_FIST) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
                // __LINE__: 18870
                UTIL_ASSERT_MSG(FALSE, "Failed equiping Weapon::Fist");
            } else {
                SelectWeaponAbility(CGameSpriteEquipment::SLOT_FIST, 0, feedBack, TRUE);
            }

            if (InControl() && g_pChitin->cNetwork.GetSessionOpen() == TRUE) {
                CMessage* message = new CMessageSpriteEquipment(this, m_id, m_id);
                g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
            }
        }
    }

    if ((GetActiveStats()->m_generalState & STATE_DEAD) == 0) {
        CMessage* message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()), m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }
}

// 0x756930
void CGameSprite::sub_756930(CItem* pItem, CItem* pLauncher)
{
    // TODO: Incomplete.
}

// 0x7579C0
SHORT CGameSprite::EquipItem()
{
    SHORT slot = FindItemPersonal(m_curAction.GetString1(), 0, FALSE);
    if (slot == -1) {
        return ACTION_ERROR;
    }

    if (m_curAction.GetSpecifics()) {
        m_equipment.m_items[slot]->Equip(this, slot, FALSE);
    } else {
        m_equipment.m_items[slot]->Unequip(this, slot, TRUE, FALSE);
    }

    return ACTION_DONE;
}

// 0x757A80
SHORT CGameSprite::FindTraps()
{
    if (!m_typeAI.IsClassValid(CAIObjectType::C_ROGUE)
        || !m_typeAI.IsClassValid(CAIObjectType::C_MONK)) {
        return ACTION_ERROR;
    }

    SetModalState(2, TRUE);
    ClearActions(FALSE);

    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == g_pBaldurChitin->m_pEngineWorld->GetSelectedCharacter()) {
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->m_nSelectedButton = 100;
        g_pBaldurChitin->GetObjectGame()->SetState(0);
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->UpdateButtons();
    }

    return ACTION_DONE;
}

// 0x758440
SHORT CGameSprite::Panic()
{
    CMessage* message;

    if (m_actionCount == 0) {
        message = new CMessageDropPath(m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

        message = new CMessageSetSequence(static_cast<BYTE>(GetIdleSequence()),
            m_id,
            m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    CAIAction randomWalk(CAIAction::RANDOMWALK, CPoint(-1, -1), 0, -1);
    AddAction(randomWalk);

    return ACTION_DONE;
}

// 0x75A3F0
SHORT CGameSprite::Turn()
{
    if (!m_typeAI.IsClassValid(CAIObjectType::C_CLERIC)
        || !m_typeAI.IsClassValid(CAIObjectType::C_PALADIN)) {
        return ACTION_ERROR;
    }

    SetModalState(4, TRUE);
    ClearActions(FALSE);

    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == g_pBaldurChitin->m_pEngineWorld->GetSelectedCharacter()) {
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->m_nSelectedButton = 100;
        g_pBaldurChitin->GetObjectGame()->SetState(0);
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->UpdateButtons();
    }

    return ACTION_DONE;
}

// 0x75ACB0
SHORT CGameSprite::UseDoor()
{
    if (m_actionCount > 0) {
        return ACTION_INTERRUPTABLE;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CAIGroup* pGroup = &(pGame->m_group);
    LONG nDoorId = m_curAction.m_specificID;

    if ((pGame->GetCharacterPortraitNum(m_id) == -1
            && pGame->m_familiars.Find(reinterpret_cast<int*>(m_id), NULL) == NULL)
        || !Orderable(FALSE)
        || !m_bSelected) {
        return ACTION_DONE;
    }

    CGameDoor* pDoor;
    if (pGame->GetObjectArray()->GetShare(nDoorId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pDoor),
            INFINITE)
        != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 24158
        UTIL_ASSERT(FALSE);
        return ACTION_ERROR;
    }

    CMessage* message = new CMessageSetForceActionPick(TRUE, m_id, pDoor->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CAITrigger trigger(CAITRIGGER_CLICKED, m_typeAI, 0);
    message = new CMessageSetTrigger(trigger, m_id, pDoor->GetId());
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    CPoint ptDest = pDoor->GetCloserDestPt(m_pos);

    LONG nRange = CGameDoor::RANGE_DOOR;
    if (_strnicmp(reinterpret_cast<const char*>(m_pArea->m_header.m_areaName), "ar5004", 7) == 0) {
        nRange = CGameDoor::RANGE_DOOR / 10;
    } else if (_strnicmp(reinterpret_cast<const char*>(m_pArea->m_header.m_areaName), "ar5202", 7) == 0) {
        nRange = CGameDoor::RANGE_DOOR / 5;
    }

    LONG dy = ptDest.y - m_pos.y;
    LONG dx = ptDest.x - m_pos.x;
    if (dy * dy * 16 / 9 + dx * dx > nRange) {
        // Not next to the door yet.
        pGame->GetObjectArray()->ReleaseShare(nDoorId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return ACTION_DONE;
    }

    BYTE rc;
    do {
        rc = pGame->GetObjectArray()->GetDeny(nDoorId,
            CGameObjectArray::THREAD_ASYNCH,
            reinterpret_cast<CGameObject**>(&pDoor),
            INFINITE);
    } while (rc == CGameObjectArray::SHARED);

    if (rc != CGameObjectArray::SUCCESS) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 24206
        UTIL_ASSERT(FALSE);
        pGame->GetObjectArray()->ReleaseShare(nDoorId,
            CGameObjectArray::THREAD_ASYNCH,
            INFINITE);
        return ACTION_ERROR;
    }

    if (pDoor->InControl()) {
        pDoor->ToggleDoor(m_typeAI, FALSE);
    } else {
        // TODO: 0x75AFD1-0x75B029 not ported: a multiplayer client asks the
        // door's owner to toggle it (message vtable 0x848C58, the open
        // state's inverse); a single-player game never gets here.
    }

    pGame->GetObjectArray()->ReleaseDeny(nDoorId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);
    pGame->GetObjectArray()->ReleaseShare(nDoorId,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    // The rest of the group stops walking.
    LONG* pGroupList = pGroup->GetGroupList();
    if (pGroupList == NULL) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 24216
        UTIL_ASSERT(FALSE);
        return ACTION_ERROR;
    }

    for (SHORT nIndex = 0; nIndex < pGroup->GetCount(); nIndex++) {
        if (m_id == pGroupList[nIndex]) {
            continue;
        }

        CGameSprite* pSprite;
        do {
            rc = pGame->GetObjectArray()->GetDeny(pGroupList[nIndex],
                CGameObjectArray::THREAD_ASYNCH,
                reinterpret_cast<CGameObject**>(&pSprite),
                INFINITE);
        } while (rc == CGameObjectArray::SHARED);

        if (rc == CGameObjectArray::SUCCESS) {
            pSprite->ClearActions(FALSE);
            if (pSprite->m_curAction.m_actionID == CAIAction::MOVETOPOINT) {
                pSprite->m_curAction.m_specificID2 = 5;
            }
            pGame->GetObjectArray()->ReleaseDeny(pGroupList[nIndex],
                CGameObjectArray::THREAD_ASYNCH,
                INFINITE);
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 24228
            UTIL_ASSERT(FALSE);
        }
    }

    delete pGroupList;

    return ACTION_DONE;
}

// 0x75DFD0
SHORT CGameSprite::EquipMostDamagingMelee()
{
    return ACTION_ERROR;
}

// 0x75E330
SHORT CGameSprite::sub_75E330(CItem* pLauncher, BOOL bEquipMelee)
{
    // Select the quiver item with the most uses that fits the launcher.
    SHORT nMaxCount = 0;
    INT nBest = -1;
    for (INT nIndex = 0; nIndex < 4; nIndex++) {
        CItem* pAmmo = m_equipment.m_items[11 + nIndex];
        if (pAmmo != NULL) {
            pAmmo->Demand();
            ITEM_ABILITY* pAbility = pAmmo->GetAbility(0);
            if (pAbility != NULL && *reinterpret_cast<BYTE*>(pAbility) == 2) {
                SHORT launcherSlot;
                if (GetLauncher(pAbility, launcherSlot) == pLauncher) {
                    if (nMaxCount < pAmmo->GetUsageCount(0)) {
                        nBest = nIndex;
                        nMaxCount = pAmmo->GetUsageCount(0);
                    }
                }
            }
            pAmmo->Release();
        }
    }

    if (static_cast<SHORT>(nBest) == -1) {
        if (bEquipMelee == TRUE) {
            // TODO: EquipMostDamagingMelee (0x75DFD0) is still a stub that
            // returns ACTION_ERROR. The original picks the best melee weapon
            // here (and re-enters sub_726810 at 0x75E0B4); ours equips
            // nothing, so a non-PC with a launcher and no fitting ammunition
            // keeps the launcher. Listed in docs/followups.md.
            EquipMostDamagingMelee();
            return CGameAIBase::ACTION_DONE;
        }
        return CGameAIBase::ACTION_ERROR;
    }

    SelectWeaponAbility(static_cast<BYTE>(11 + nBest), 0, TRUE, TRUE);

    for (INT nSlot = 43; nSlot < 51; nSlot++) {
        if (m_equipment.m_items[nSlot] == pLauncher) {
            CGameButtonList* buttons = GetItemUsages(static_cast<SHORT>(11 + nBest), 1, 0);
            if (buttons != NULL) {
                // NOTE: The original reads the head node without checking
                // that the list has one; the ammunition found above always
                // gives it one.
                CButtonData* pButtonData = buttons->GetHead();
                if (pButtonData != NULL) {
                    BYTE buttonNum = static_cast<BYTE>(nSlot - 43);

                    // NOTE: Uninline.
                    SetQuickWeapon(buttonNum, *pButtonData);

                    SHORT nItemNum = pButtonData->m_abilityId.m_itemNum;
                    if (nItemNum < 11 || nItemNum > 14) {
                        SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
                    } else {
                        SetQuickWeapon(buttonNum, static_cast<BYTE>(nItemNum));
                    }

                    // TODO: The original calls CheckQuickLists (0x71ABA0,
                    // not ported) with a default CAbilityId and 0, FALSE,
                    // FALSE here: it refreshes the quick lists' counts and
                    // the action bar.

                    // NOTE: The original deletes the list without its other
                    // nodes' button data (a leak, kept).
                    delete pButtonData;
                    delete buttons;
                    return CGameAIBase::ACTION_DONE;
                }
                delete buttons;
            }
        }
    }

    return CGameAIBase::ACTION_DONE;
}

// 0x75E880
const CAIObjectType& CGameSprite::GetLiveAIType()
{
    return m_liveTypeAI;
}

// 0x75E890
BOOLEAN CGameSprite::CanSaveGame(STRREF& strError)
{
    if (!m_baseStats.field_294
        && m_typeAI.GetEnemyAlly() >= CAIObjectType::EA_EVILCUTOFF
        && m_canBeSeen > 0
        && (m_derivedStats.m_generalState & STATE_DEAD) == 0
        && (m_baseStats.m_flags & 0x8000) == 0) {
        strError = 16501;
        return FALSE;
    }

    if (sub_7202E0()) {
        if (m_curAction.m_actionID != CAIAction::DIALOGUE
            && m_curAction.m_actionID != CAIAction::STARTDIALOG
            && m_currentActionId != CAIAction::DIALOGUE
            && m_currentActionId != CAIAction::STARTDIALOG) {
            strError = -1;
            return TRUE;
        }
    }

    strError = 16502; // "You cannot save at this time."
    return FALSE;
}

// 0x75E940
SHORT CGameSprite::GetCasterLevel(CSpell* pSpell, BYTE nClass, DWORD nSpecialization)
{
    // NOTE: Unsigned compare below in the loop.
    UINT nLevel = 1;
    UINT nBestCasterLevel = 0;

    if (pSpell == NULL) {
        if (nClass != 0 || nSpecialization != 0) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26119
            UTIL_ASSERT(pSpell != NULL);
        }
        return 1;
    }

    if (pSpell->GetCasterType() == 4) {
        if (pSpell->GetResRef() == "SPIN263") {
            nClass = CAIOBJECTTYPE_C_CLERIC;
            nSpecialization = SPECMASK_CLERIC_TALOS;
        } else if (pSpell->GetResRef() == "SPIN264") {
            nClass = CAIOBJECTTYPE_C_CLERIC;
            nSpecialization = SPECMASK_CLERIC_TEMPUS;
        } else if (pSpell->GetResRef() == "SPIN265"
            || pSpell->GetResRef() == "SPIN266") {
            nClass = CAIOBJECTTYPE_C_CLERIC;
            nSpecialization = SPECMASK_CLERIC_HELM;
        } else if (pSpell->GetResRef() == "SPIN267"
            || pSpell->GetResRef() == "SPIN270") {
            nClass = CAIOBJECTTYPE_C_CLERIC;
            nSpecialization = SPECMASK_CLERIC_ILMATER;
        } else if (pSpell->GetResRef() == "SPIN268"
            || pSpell->GetResRef() == "SPIN269") {
            nClass = CAIOBJECTTYPE_C_CLERIC;
            nSpecialization = SPECMASK_CLERIC_SELUNE;
        } else if (pSpell->GetResRef() == "SPIN271") {
            nClass = CAIOBJECTTYPE_C_CLERIC;
            nSpecialization = SPECMASK_CLERIC_LATHANDER;
        } else {
            nBestCasterLevel = m_derivedStats.GetLevel();
            if (nBestCasterLevel == 0) {
                nBestCasterLevel = 1;
            }
            return static_cast<SHORT>(nBestCasterLevel);
        }
    } else {
        switch (nClass) {
        case CAIOBJECTTYPE_C_BARBARIAN:
        case CAIOBJECTTYPE_C_FIGHTER:
        case CAIOBJECTTYPE_C_MONK:
        case CAIOBJECTTYPE_C_ROGUE:
            return static_cast<SHORT>(m_derivedStats.GetClassLevel(GetAIType().m_nClass));
        }
    }

    UINT nStart = 0;
    UINT nEnd = CSPELLLIST_NUM_CLASSES;

    if (nClass != 0) {
        nStart = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);
        nEnd = nStart + 1;
    }

    for (UINT nClassIndex = nStart; nClassIndex < nEnd; nClassIndex++) {
        nClass = g_pBaldurChitin->GetObjectGame()->GetSpellcasterClass(nStart);
        switch (nClass) {
        case CAIOBJECTTYPE_C_BARD:
        case CAIOBJECTTYPE_C_DRUID:
        case CAIOBJECTTYPE_C_SORCERER:
        case CAIOBJECTTYPE_C_WIZARD:
            nLevel = m_derivedStats.GetClassLevel(nClass);
            break;
        case CAIOBJECTTYPE_C_CLERIC:
            nLevel = m_derivedStats.GetClassLevel(nClass);
            if (nSpecialization != 0) {
                nLevel++;
            }
            break;
        case CAIOBJECTTYPE_C_PALADIN:
        case CAIOBJECTTYPE_C_RANGER:
            nLevel = m_derivedStats.GetClassLevel(nClass);
            if (nLevel >= 4) {
                nLevel /= 2;
            } else {
                nLevel = 0;
            }
            break;
        }

        // NOTE: Unsigned compare.
        if (nLevel > nBestCasterLevel) {
            nBestCasterLevel = nLevel;
        }
    }

    if (nBestCasterLevel == 0) {
        nBestCasterLevel = m_derivedStats.GetClassLevel(GetAIType().m_nClass);
    }
    if (nBestCasterLevel == 0) {
        nBestCasterLevel = 1;
    }
    return static_cast<SHORT>(nBestCasterLevel);
}

// 0x75F240
SHORT CGameSprite::sub_75F240()
{
    m_baseStats.field_2E4 = static_cast<SHORT>(m_pos.x);
    m_baseStats.field_2E6 = static_cast<SHORT>(m_pos.y);
    m_baseStats.field_2E8 = static_cast<BYTE>(m_nDirection);
    return ACTION_DONE;
}

// 0x75F270
SHORT CGameSprite::ReturnToStartLoc()
{
    if (m_baseStats.field_2E4 == 0 || m_baseStats.field_2E6 == 0) {
        return ACTION_DONE;
    }

    CPoint ptDest;
    if (m_curAction.m_actionID == 302) {
        ptDest = m_pos;
    } else {
        ptDest.x = static_cast<WORD>(m_baseStats.field_2E4);
        ptDest.y = static_cast<WORD>(m_baseStats.field_2E6);
    }

    SHORT returnValue = MoveToPoint(ptDest, m_curAction.m_specificID);
    if (returnValue != ACTION_DONE && returnValue != ACTION_ERROR) {
        m_lastActionID = 23;
        return returnValue;
    }

    SetDirection(m_baseStats.field_2E8);

    if (m_pos.x != -1 || m_pos.y != -1 || m_pArea != NULL) {
        CMessage* message = new CMessageSpriteUpdate(this, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
    }

    if (m_curAction.m_actionID == 262) {
        m_removeFromArea = TRUE;
    }

    return returnValue;
}

// 0x75F3D0
//
// Hides (1) or shows (0) the creature: m_baseStats.field_294, the CRE's
// byte 0x29C. The argument is read as a byte.
SHORT CGameSprite::sub_75F3D0(int a1)
{
    const BYTE bHide = static_cast<BYTE>(a1);

    if (m_id == 0 || m_pArea == NULL) {
        return ACTION_ERROR;
    }

    if (bHide == 1) {
        if (m_baseStats.field_294 == 1) {
            return ACTION_DONE;
        }

        // NOTE: sub_40CCA0 shares the object and nothing releases it, as in
        // the original. For THREAD_ASYNCH no share count is taken, so the
        // missing release is harmless.
        if (m_typeAI.sub_40CCA0(this, TRUE) != NULL) {
            // NOTE: Uninline (no NULL check of the marker in the original).
            m_pArea->m_pObjectMarker->RemoveObject(m_id);
        }

        if (field_7430 == TRUE) {
            m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                               m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
                m_typeAI.GetEnemyAlly(),
                m_animation.GetPersonalSpace(),
                field_54A8,
                field_7430);
        }

        m_baseStats.field_294 = TRUE;
        return ACTION_DONE;
    }

    if (bHide != 0) {
        return ACTION_ERROR;
    }

    if (!m_baseStats.field_294) {
        return ACTION_DONE;
    }

    // NOTE: The original looks for the nearest open square and never reads
    // the result (0x75F65D; the local is overwritten at 0x75F715).
    if ((m_animation.GetAnimationId() & 0xF000) == 0x4000 && m_animation.GetAnimationId() >= 0x4400) {
        m_pArea->m_search.GetNearestOpenSquare(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                   m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            m_visibleTerrainTable,
            m_animation.GetPersonalSpace(),
            -1);
    } else {
        m_pArea->m_search.GetNearestOpenSquare(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                                   m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            m_terrainTable,
            m_animation.GetPersonalSpace(),
            -1);
    }

    // NOTE: As above, the share is not released (THREAD_ASYNCH: no count).
    if (m_typeAI.sub_40CCA0(this, TRUE) != NULL) {
        // NOTE: Uninline.
        m_pArea->m_pObjectMarker->RemoveObject(m_id);
    }

    if (field_7430 == TRUE) {
        m_pArea->m_search.RemoveObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                           m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
            m_typeAI.GetEnemyAlly(),
            m_animation.GetPersonalSpace(),
            field_54A8,
            field_7430);
    }

    // NOTE: Uninline.
    m_pArea->m_pObjectMarker->AddObject(m_id);

    m_pArea->m_search.AddObject(CPoint(m_pos.x / CPathSearch::GRID_SQUARE_SIZEX,
                                    m_pos.y / CPathSearch::GRID_SQUARE_SIZEY),
        m_typeAI.GetEnemyAlly(),
        m_animation.GetPersonalSpace(),
        field_54A8,
        field_7430);

    m_baseStats.field_294 = FALSE;
    return ACTION_DONE;
}

// 0x75F800
SHORT CGameSprite::SetAtOffset(DWORD stat, DWORD value, BOOL modify)
{
    switch (stat) {
    case STAT_MAXHITPOINTS:
        if (modify == TRUE) {
            m_baseStats.m_maxHitPointsBase += static_cast<SHORT>(value);
            m_derivedStats.m_nMaxHitPoints += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_maxHitPointsBase = static_cast<SHORT>(value);
            m_derivedStats.m_nMaxHitPoints = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_ARMORCLASS:
        if (modify == TRUE) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26506
            UTIL_ASSERT_MSG(FALSE, "this option is no longer available -rjf");
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26513
            UTIL_ASSERT_MSG(FALSE, "this option is no longer available -rjf");
        }
    case STAT_ACBLUDGEONINGMOD:
        if (modify == TRUE) {
            m_baseStats.m_armorClassCrushingAdjustment += static_cast<SHORT>(value);
            m_derivedStats.m_nACCrushingMod += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_armorClassCrushingAdjustment = static_cast<SHORT>(value);
            m_derivedStats.m_nACCrushingMod = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_ACMISSILEMOD:
        if (modify == TRUE) {
            m_baseStats.m_armorClassMissileAdjustment += static_cast<SHORT>(value);
            m_derivedStats.m_nACMissileMod += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_armorClassMissileAdjustment = static_cast<SHORT>(value);
            m_derivedStats.m_nACMissileMod = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_ACPIERCINGMOD:
        if (modify == TRUE) {
            m_baseStats.m_armorClassPiercingAdjustment += static_cast<SHORT>(value);
            m_derivedStats.m_nACPiercingMod += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_armorClassPiercingAdjustment = static_cast<SHORT>(value);
            m_derivedStats.m_nACPiercingMod = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_ACSLASHINGMOD:
        if (modify == TRUE) {
            m_baseStats.m_armorClassSlashingAdjustment += static_cast<SHORT>(value);
            m_derivedStats.m_nACSlashingMod += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_armorClassSlashingAdjustment = static_cast<SHORT>(value);
            m_derivedStats.m_nACSlashingMod = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_THAC0:
        if (modify == TRUE) {
            m_baseStats.m_attackBase += static_cast<BYTE>(value);
            m_derivedStats.m_nTHAC0 += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_attackBase = static_cast<BYTE>(value);
            m_derivedStats.m_nTHAC0 = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_NUMBEROFATTACKS:
        if (modify == TRUE) {
            m_baseStats.m_numberOfAttacksBase += static_cast<BYTE>(value);
            m_derivedStats.m_nNumberOfAttacks += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_numberOfAttacksBase = static_cast<BYTE>(value);
            m_derivedStats.m_nNumberOfAttacks = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SAVEFORTITUDE:
        if (modify == TRUE) {
            m_baseStats.m_saveVSFortitudeBase += static_cast<BYTE>(value);
            m_derivedStats.m_nSaveVSFortitude += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_saveVSFortitudeBase = static_cast<BYTE>(value);
            m_derivedStats.m_nSaveVSFortitude = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SAVEREFLEX:
        if (modify == TRUE) {
            m_baseStats.m_saveVSReflexBase += static_cast<BYTE>(value);
            m_derivedStats.m_nSaveVSReflex += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_saveVSReflexBase = static_cast<BYTE>(value);
            m_derivedStats.m_nSaveVSReflex = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SAVEWILL:
        if (modify == TRUE) {
            m_baseStats.m_saveVSWillBase += static_cast<BYTE>(value);
            m_derivedStats.m_nSaveVSWill += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_saveVSWillBase = static_cast<BYTE>(value);
            m_derivedStats.m_nSaveVSWill = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_KNOWLEDGEARCANA:
    case STAT_13:
        return 1;
    case STAT_RESISTFIRE:
        if (modify == TRUE) {
            m_baseStats.m_resistFireBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistFire += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistFireBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistFire = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTCOLD:
        if (modify == TRUE) {
            m_baseStats.m_resistColdBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistCold += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistColdBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistCold = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTELECTRICITY:
        if (modify == TRUE) {
            m_baseStats.m_resistElectricityBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistElectricity += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistElectricityBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistElectricity = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTACID:
        if (modify == TRUE) {
            m_baseStats.m_resistAcidBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistAcid += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistAcidBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistAcid = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTMAGIC:
        if (modify == TRUE) {
            m_baseStats.m_resistMagicBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistMagic += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistMagicBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistMagic = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTMAGICFIRE:
        if (modify == TRUE) {
            m_baseStats.m_resistMagicFireBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistMagicFire += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistMagicFireBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistMagicFire = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTMAGICCOLD:
        if (modify == TRUE) {
            m_baseStats.m_resistMagicColdBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistMagicCold += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistMagicColdBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistMagicCold = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTSLASHING:
        if (modify == TRUE) {
            m_baseStats.m_resistSlashingBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistSlashing += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistSlashingBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistSlashing = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTBLUDGEONING:
        if (modify == TRUE) {
            m_baseStats.m_resistCrushingBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistCrushing += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistCrushingBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistCrushing = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTPIERCING:
        if (modify == TRUE) {
            m_baseStats.m_resistPiercingBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistPiercing += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistPiercingBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistPiercing = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTMISSILE:
        if (modify == TRUE) {
            m_baseStats.m_resistMissileBase += static_cast<signed char>(value);
            m_derivedStats.m_nResistMissile += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_resistMissileBase = static_cast<signed char>(value);
            m_derivedStats.m_nResistMissile = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_ALCHEMY:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_ALCHEMY] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_LOCKPICKING:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_OPEN_LOCK] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_OPEN_LOCK] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_OPEN_LOCK] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_MOVESILENTLY:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_MOVE_SILENTLY] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_MOVE_SILENTLY] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_MOVE_SILENTLY] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_MOVE_SILENTLY] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_TRAPS:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_SEARCH] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_SEARCH] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_SEARCH] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_SEARCH] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_PICKPOCKET:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_PICK_POCKET] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_PICK_POCKET] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_PICK_POCKET] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_PICK_POCKET] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_FATIGUE:
        if (modify == TRUE) {
            m_baseStats.m_fatigue += static_cast<BYTE>(value);
            m_derivedStats.m_nFatigue += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_fatigue = static_cast<BYTE>(value);
            m_derivedStats.m_nFatigue = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_INTOXICATION:
        if (modify == TRUE) {
            m_baseStats.m_intoxication += static_cast<BYTE>(value);
            // FIXME: Looks like a bug - should be `m_nIntoxication`.
            m_derivedStats.m_nFatigue += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_intoxication = static_cast<BYTE>(value);
            // FIXME: Looks like a bug - should be `m_nIntoxication`.
            m_derivedStats.m_nFatigue = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_LUCK:
        if (modify == TRUE) {
            m_baseStats.m_luckBase += static_cast<BYTE>(value);
            m_derivedStats.m_nLuck += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_luckBase = static_cast<BYTE>(value);
            m_derivedStats.m_nLuck = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_TRACKING:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_WILDERNESS_LORE] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_WILDERNESS_LORE] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_LEVEL:
        if (modify == TRUE) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26891
            UTIL_ASSERT_MSG(FALSE, "Scripts and dialogs may no longer set Class Levels");
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26897
            UTIL_ASSERT_MSG(FALSE, "Scripts and dialogs may no longer set Class Levels");
        }
    case STAT_SEX:
        if (modify == TRUE) {
            m_baseStats.m_sex += static_cast<BYTE>(value);
            m_derivedStats.m_nSex += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_sex = static_cast<BYTE>(value);
            m_derivedStats.m_nSex = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_STR:
        if (modify == TRUE) {
            m_baseStats.m_STRBase += static_cast<BYTE>(value);
            m_derivedStats.m_nSTR += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_STRBase = static_cast<BYTE>(value);
            m_derivedStats.m_nSTR = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_STREXTRA:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 26969
        UTIL_ASSERT(FALSE);
    case STAT_INT:
        if (modify == TRUE) {
            m_baseStats.m_INTBase += static_cast<BYTE>(value);
            m_derivedStats.m_nINT += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_INTBase = static_cast<BYTE>(value);
            m_derivedStats.m_nINT = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_WIS:
        if (modify == TRUE) {
            m_baseStats.m_WISBase += static_cast<BYTE>(value);
            m_derivedStats.m_nWIS += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_WISBase = static_cast<BYTE>(value);
            m_derivedStats.m_nWIS = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_DEX:
        if (modify == TRUE) {
            m_baseStats.m_DEXBase += static_cast<BYTE>(value);
            m_derivedStats.m_nDEX += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_DEXBase = static_cast<BYTE>(value);
            m_derivedStats.m_nDEX = static_cast<SHORT>(value);
        }
        field_562C = TRUE;
        ProcessEffectList();
        return 1;
    case STAT_CON:
        if (modify == TRUE) {
            m_baseStats.m_CONBase += static_cast<BYTE>(value);
            m_derivedStats.m_nCON += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_CONBase = static_cast<BYTE>(value);
            m_derivedStats.m_nCON = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_CHR:
        if (modify == TRUE) {
            m_baseStats.m_CHRBase += static_cast<BYTE>(value);
            m_derivedStats.m_nCHR += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_CHRBase = static_cast<BYTE>(value);
            m_derivedStats.m_nCHR = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_XPVALUE:
        if (modify == TRUE) {
            m_baseStats.m_xpValue += value;
            m_derivedStats.m_nXPValue += value;
        } else {
            m_baseStats.m_xpValue = value;
            m_derivedStats.m_nXPValue = value;
        }
        return 1;
    case STAT_XP:
        if (modify == TRUE) {
            m_baseStats.m_xp += value;
            m_derivedStats.m_nXP += value;
        } else {
            m_baseStats.m_xp = value;
            m_derivedStats.m_nXP = value;
        }
        return 1;
    case STAT_GOLD:
        if (modify == TRUE) {
            m_baseStats.m_gold += value;
            m_derivedStats.m_nGold += value;
        } else {
            m_baseStats.m_gold = value;
            m_derivedStats.m_nGold = value;
        }
        return 1;
    case STAT_MORALEBREAK:
        if (modify == TRUE) {
            m_baseStats.m_moraleBreak += static_cast<BYTE>(value);
            m_derivedStats.m_nMoraleBreak += value;
        } else {
            m_baseStats.m_moraleBreak = static_cast<BYTE>(value);
            m_derivedStats.m_nMoraleBreak = value;
        }
        return 1;
    case STAT_MORALERECOVERYTIME:
        if (modify == TRUE) {
            m_baseStats.m_moraleRecoveryTime += static_cast<WORD>(value);
            m_derivedStats.m_nMoraleRecoveryTime += value;
        } else {
            m_baseStats.m_moraleRecoveryTime = static_cast<WORD>(value);
            m_derivedStats.m_nMoraleRecoveryTime = value;
        }
        return 1;
    case STAT_REPUTATION:
        if (modify == TRUE) {
            m_baseStats.m_reputation += static_cast<BYTE>(value);
            m_derivedStats.m_nReputation += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_reputation = static_cast<BYTE>(value);
            m_derivedStats.m_nReputation = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_HATEDRACE:
        if (modify == TRUE) {
            m_baseStats.m_favoredEnemies[0] += static_cast<BYTE>(value);
            m_derivedStats.m_favoredEnemies[0] += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_favoredEnemies[0] = static_cast<BYTE>(value);
            m_derivedStats.m_favoredEnemies[0] = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_DAMAGEBONUS:
        if (modify == TRUE) {
            m_derivedStats.m_nDamageBonus += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nDamageBonus = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SPELLFAILUREMAGE:
        if (modify == TRUE) {
            m_derivedStats.m_nSpellFailureArcane += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nSpellFailureArcane = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SPELLFAILUREPRIEST:
        if (modify == TRUE) {
            m_derivedStats.m_nSpellFailureDivine += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nSpellFailureDivine = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SPELLDURATIONMODMAGE:
        if (modify == TRUE) {
            m_derivedStats.m_nSpellDurationModMage += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nSpellDurationModMage = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SPELLDURATIONMODPRIEST:
        if (modify == TRUE) {
            m_derivedStats.m_nSpellDurationModPriest += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nSpellDurationModPriest = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_TURNUNDEADLEVEL:
        if (modify == TRUE) {
            m_baseStats.m_turnUndeadLevel += static_cast<BYTE>(value);
            m_derivedStats.m_nTurnUndeadLevel += static_cast<SHORT>(value);
        } else {
            m_baseStats.m_turnUndeadLevel = static_cast<BYTE>(value);
            m_derivedStats.m_nTurnUndeadLevel = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_BACKSTABDAMAGEMULTIPLIER:
        if (modify == TRUE) {
            m_derivedStats.m_nBackstabDamageMultiplier += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nBackstabDamageMultiplier = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_LAYONHANDSAMOUNT:
        if (modify == TRUE) {
            m_derivedStats.m_nLayOnHandsAmount += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nLayOnHandsAmount = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_TRANSLUCENT:
        if (modify == TRUE) {
            m_derivedStats.m_nTranslucent += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nTranslucent = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_LEVEL2:
        if (modify == TRUE) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26905
            UTIL_ASSERT_MSG(FALSE, "Scripts and dialogs may no longer set Class Levels");
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26911
            UTIL_ASSERT_MSG(FALSE, "Scripts and dialogs may no longer set Class Levels");
        }
    case STAT_LEVEL3:
        if (modify == TRUE) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 29919
            UTIL_ASSERT_MSG(FALSE, "Scripts and dialogs may no longer set Class Levels");
        } else {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 26925
            UTIL_ASSERT_MSG(FALSE, "Scripts and dialogs may no longer set Class Levels");
        }
    case STAT_ENCUMBERANCE:
        if (modify == TRUE) {
            m_derivedStats.m_nEncumberance += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nEncumberance = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_MISSILETHAC0BONUS:
        if (modify == TRUE) {
            m_derivedStats.m_nMissileTHAC0Bonus += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nMissileTHAC0Bonus = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_MAGICDAMAGERESISTANCE:
        if (modify == TRUE) {
            m_derivedStats.m_nMagicDamageResistance += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nMagicDamageResistance = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_RESISTPOISON:
        if (modify == TRUE) {
            m_derivedStats.m_nResistPoison += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nResistPoison = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_MENTALSPEED:
        if (modify == TRUE) {
            m_derivedStats.m_nMentalSpeed += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nMentalSpeed = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_PHYSICALSPEED:
        if (modify == TRUE) {
            m_derivedStats.m_nPhysicalSpeed += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nPhysicalSpeed = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_CASTINGLEVELBONUSMAGE:
        if (modify == TRUE) {
            m_derivedStats.m_nCastingLevelBonusMage += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nCastingLevelBonusMage = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_CASTINGLEVELBONUSCLERIC:
        if (modify == TRUE) {
            m_derivedStats.m_nCastingLevelBonusCleric += static_cast<SHORT>(value);
        } else {
            m_derivedStats.m_nCastingLevelBonusCleric = static_cast<SHORT>(value);
        }
        return 1;
    case STAT_SEEINVISIBLE:
        if (modify == TRUE) {
            m_derivedStats.m_bSeeInvisible += value;
        } else {
            m_derivedStats.m_bSeeInvisible = value;
        }
        return 1;
    case STAT_IGNOREDIALOGPAUSE:
        if (modify == TRUE) {
            m_derivedStats.m_bIgnoreDialogPause += value;
        } else {
            m_derivedStats.m_bIgnoreDialogPause = value;
        }
        return 1;
    case STAT_MINHITPOINTS:
        if (modify == TRUE) {
            m_derivedStats.m_nMinHitPoints += value;
        } else {
            m_derivedStats.m_nMinHitPoints = value;
        }
        return 1;
    case STAT_THAC0BONUSRIGHT:
        if (modify == TRUE) {
            m_derivedStats.m_THAC0BonusRight += value;
        } else {
            m_derivedStats.m_THAC0BonusRight = value;
        }
        return 1;
    case STAT_THAC0BONUSLEFT:
        if (modify == TRUE) {
            m_derivedStats.m_THAC0BonusLeft += value;
        } else {
            m_derivedStats.m_THAC0BonusLeft = value;
        }
        return 1;
    case STAT_DAMAGEBONUSRIGHT:
        if (modify == TRUE) {
            m_derivedStats.m_DamageBonusRight += value;
        } else {
            m_derivedStats.m_DamageBonusRight = value;
        }
        return 1;
    case STAT_DAMAGEBONUSLEFT:
        if (modify == TRUE) {
            m_derivedStats.m_DamageBonusLeft += value;
        } else {
            m_derivedStats.m_DamageBonusLeft = value;
        }
        return 1;
    case STAT_HIDEINSHADOWS:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_HIDE] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_HIDE] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_HIDE] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_HIDE] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_MOVESILENTLYMTPBONUS:
        if (modify == TRUE) {
            m_derivedStats.m_nMoveSilentlyMTPBonus += value;
        } else {
            m_derivedStats.m_nMoveSilentlyMTPBonus = value;
        }
        return 1;
    case STAT_HIDEINSHADOWSMTPBONUS:
        if (modify == TRUE) {
            m_derivedStats.m_nHideInShadowsMTPBonus += value;
        } else {
            m_derivedStats.m_nHideInShadowsMTPBonus = value;
        }
        return 1;
    case STAT_SUBRACE:
        if (modify == TRUE) {
            m_baseStats.m_subrace += static_cast<BYTE>(value);
            m_derivedStats.m_nSubRace += value;
        } else {
            m_baseStats.m_subrace = static_cast<BYTE>(value);
            m_derivedStats.m_nSubRace = value;
        }
        return 1;
    case STAT_USEMAGICDEVICE:
        if (modify == TRUE) {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] += static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] += static_cast<BYTE>(value);
        } else {
            m_baseStats.m_skills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = static_cast<BYTE>(value);
            m_derivedStats.m_nSkills[CGAMESPRITE_SKILL_USE_MAGIC_DEVICE] = static_cast<BYTE>(value);
        }
        return 1;
    case STAT_CLASSLEVELBARBARIAN:
    case STAT_CLASSLEVELBARD:
    case STAT_CLASSLEVELCLERIC:
    case STAT_CLASSLEVELDRUID:
    case STAT_CLASSLEVELFIGHTER:
    case STAT_CLASSLEVELMONK:
    case STAT_CLASSLEVELPALADIN:
    case STAT_CLASSLEVELRANGER:
    case STAT_CLASSLEVELROGUE:
    case STAT_CLASSLEVELSORCERER:
    case STAT_CLASSLEVELWIZARD:
        if (modify == TRUE) {
            m_derivedStats.SetClassLevel(stat - STAT_CLASSLEVELSUM,
                m_derivedStats.GetClassLevel(stat - STAT_CLASSLEVELSUM) + value);
            SetClassLevel(stat - STAT_CLASSLEVELSUM,
                GetClassLevel(stat - STAT_CLASSLEVELSUM) + value);
        } else {
            m_derivedStats.SetClassLevel(stat - STAT_CLASSLEVELSUM, value);
            SetClassLevel(stat - STAT_CLASSLEVELSUM, value);
        }
        return 1;
    case STAT_PREVENTAISLOWDOWN:
        if (modify == TRUE) {
            m_derivedStats.m_bPreventAISlowDown += value;
        } else {
            m_derivedStats.m_bPreventAISlowDown = value;
        }
        return 1;
    }

    return 0;
}

// 0x761470
SHORT CGameSprite::ForceHide(CGameSprite* pSprite)
{
    if (m_bHiding) {
        return ACTION_DONE;
    }

    if (g_pBaldurChitin->GetObjectGame()->GetState() != 0) {
        return ACTION_DONE;
    }

    SetModalState(3, TRUE);
    g_pBaldurChitin->GetObjectGame()->GetButtonArray()->SetSelectedButton(5);

    if (!m_bHiding && !m_baseStats.field_294) {
        PlaySound(CResRef("ACT_07"));
    }

    if (!m_baseStats.field_294) {
        FeedBack(FEEDBACK_HIDESUCCEEDED,
            0,
            0,
            0,
            -1,
            0,
            0);
    }

    ITEM_EFFECT effect;
    CGameEffect::ClearItemEffect(&effect, CGAMEEFFECT_INVISIBLE);
    effect.durationType = 0x100;
    effect.dwFlags = 0;
    effect.effectAmount = 1;
    effect.duration = g_pBaldurChitin->GetObjectGame()->GetWorldTimer()->m_gameTime + 300;

    CGameEffect* pEffect = CGameEffect::DecodeEffect(&effect,
        m_pos,
        m_id,
        CPoint(-1, -1));

    CMessage* message = new CMessageAddEffect(pEffect, m_id, m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);

    m_modalCounter = 0;
    m_bHiding = TRUE;

    return ACTION_DONE;
}

// 0x7615F0
SHORT CGameSprite::sub_7615F0(int a1)
{
    if (a1 == 0) {
        m_pArea->m_visibility.SetAreaVisible(FALSE);
        a1 = CVisibilityMap::EXPLORED_RANGE_3;
    }

    m_pArea->m_visibility.field_E = static_cast<short>(a1);
    m_pArea->m_visibility.field_10 = static_cast<short>(3 * a1 / 4);
    m_pArea->m_visibility.sub_5518A0();

    return ACTION_DONE;
}

// 0x761650
void CGameSprite::sub_761650()
{
    // TODO: Incomplete.
}

// 0x7618E0
SHORT CGameSprite::DropItem(CItem* pItem)
{
    if (pItem == NULL) {
        return ACTION_ERROR;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    LONG iContainer = pGame->GetGroundPile(m_id);
    if (iContainer == CGameObjectArray::INVALID_INDEX) {
        return ACTION_ERROR;
    }

    CGameContainer* pContainer;

    BYTE rc = pGame->GetObjectArray()->GetDeny(iContainer,
        CGameObjectArray::THREAD_ASYNCH,
        reinterpret_cast<CGameObject**>(&pContainer),
        INFINITE);
    if (rc != CGameObjectArray::SUCCESS) {
        return ACTION_ERROR;
    }

    if ((pItem->GetFlagsFile() & 0x4) != 0) {
        pContainer->PlaceItemInBlankSlot(pItem, 1, SHORT_MAX);
    }

    pGame->GetObjectArray()->ReleaseDeny(iContainer,
        CGameObjectArray::THREAD_ASYNCH,
        INFINITE);

    return ACTION_DONE;
}

// 0x761990
void CGameSprite::sub_761990()
{
    m_timedEffectList.RemoveAllEffectsIgnoreMoreThenPermanent(NULL, FALSE, FALSE, 0, 0);
    UnequipAll(FALSE);
    EquipAll(FALSE);
}

// 0x7619C0
SHORT CGameSprite::PlayBardSong()
{
    if ((GetAIType().m_nClassMask & CLASSMASK_BARD) == 0) {
        return ACTION_ERROR;
    }

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();

    if (pGame->GetButtonArray()->m_nState == 113) {
        return ACTION_DONE;
    }

    BYTE nSong = static_cast<BYTE>(m_curAction.m_specificID);
    if (nSong >= pGame->GetMasterSongLookup().m_nCount) {
        return ACTION_ERROR;
    }

    m_nLastSong = nSong;
    SetModalState(1, TRUE);
    ClearActions(FALSE);

    if (g_pBaldurChitin->GetObjectGame()->GetCharacterPortraitNum(m_id) == g_pBaldurChitin->GetScreenWorld()->GetSelectedCharacter()) {
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->SetSelectedButton(100);
        g_pBaldurChitin->GetObjectGame()->SetState(0);
        g_pBaldurChitin->GetObjectGame()->GetButtonArray()->UpdateButtons();
    }

    return ACTION_DONE;
}

// 0x762740
BOOL CGameSprite::HasClassMask(DWORD dwMask)
{
    return (dwMask & GetAIType().m_nClassMask) != 0;
}

// 0x762760
INT CGameSprite::GetClassLevel(INT iClassType)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28701
    UTIL_ASSERT(iClassType > 0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28702
    UTIL_ASSERT(iClassType <= CAIOBJECT_CLASS_MAX);

    return m_baseStats.m_classLevels[iClassType - 1];
}

// 0x7627C0
void CGameSprite::SetClassLevel(INT iClassType, INT nLevel)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28718
    UTIL_ASSERT(iClassType > 0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28719
    UTIL_ASSERT(iClassType <= CAIOBJECT_CLASS_MAX);

    m_baseStats.m_characterLevel += static_cast<BYTE>(nLevel) - m_baseStats.m_classLevels[iClassType];
    m_baseStats.m_classLevels[iClassType - 1] = static_cast<BYTE>(nLevel);
}

// 0x762830
BOOL CGameSprite::HasClassLevel(INT iClassType)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28739
    UTIL_ASSERT(iClassType > 0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28740
    UTIL_ASSERT(iClassType <= CAIOBJECT_CLASS_MAX);

    return m_baseStats.m_classLevels[iClassType - 1] != 0;
}

// 0x762890
void CGameSprite::SetFeatValue(UINT nFeatNumber, INT iFeatValue)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28756
    UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

    if (iFeatValue > 0) {
        m_baseStats.m_feats[nFeatNumber >> 5] |= 1 << (nFeatNumber & 0x1F);
    } else {
        m_baseStats.m_feats[nFeatNumber >> 5] &= ~(1 << (nFeatNumber & 0x1F));
    }

    switch (nFeatNumber) {
    case CGAMESPRITE_FEAT_ARMOR_PROF:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28774
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_ARMOR_PROFICIENCY_UPGRADE);

        m_baseStats.m_featArmorProficiency = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_ARMORED_ARCANA:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28781
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_ARMORED_ARCANA_UPGRADE);

        m_baseStats.m_featArmoredArcana = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_CLEAVE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28781
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_CLEAVE_UPGRADE);

        m_baseStats.m_featCleave = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_EXOTIC_BASTARD:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28795
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featBastardSword = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_EXTRA_RAGE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28802
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_EXTRA_RAGE_UPGRADE);

        m_baseStats.m_featExtraRage = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_EXTRA_SHAPESHIFTING:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28809
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_EXTRA_SHAPESHIFTING_UPGRADE);

        m_baseStats.m_featExtraShapeshifting = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_EXTRA_SMITING:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28816
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_EXTRA_SMITING_UPGRADE);

        m_baseStats.m_featExtraSmiting = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_EXTRA_TURNING:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28823
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_EXTRA_TURNING_UPGRADE);

        m_baseStats.m_featExtraTurning = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_AXE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28830
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featAxe = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_BOW:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28837
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featBow = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_FLAIL:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28844
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featFlail = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_GREATSWORD:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28851
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featGreatSword = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_HAMMER:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28858
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featHammer = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_LARGESWORD:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28865
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featLargeSword = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_MARTIAL_POLEARM:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28872
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featPolearm = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SIMPLE_CROSSBOW:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28879
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featCrossbow = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SIMPLE_MACE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28886
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featMace = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SIMPLE_MISSILE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28893
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featMissile = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28890
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featQuarterStaff = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28897
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE);

        m_baseStats.m_featSmallBlade = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28914
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_ENCHANTMENT_UPGRADE);

        m_baseStats.m_featFocusEnchantment = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28921
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_EVOCATION_UPGRADE);

        m_baseStats.m_featFocusEvocation = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28928
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_NECROMANCY_UPGRADE);

        m_baseStats.m_featFocusNecromancy = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28935
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_TRANSMUTATION_UPGRADE);

        m_baseStats.m_featFocusTransmutation = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_SPELL_PENETRATION:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28942
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_MAX_SPELL_PENETRATION_UPGRADE);

        m_baseStats.m_featSpellPenetration = iFeatValue;
        break;
    case CGAMESPRITE_FEAT_TOUGHNESS:
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
        // __LINE__: 28949
        UTIL_ASSERT(iFeatValue <= CGAMESPRITE_FEAT_TOUGHNESS_MAX_LEVEL);

        m_baseStats.m_featToughness = iFeatValue;
        break;
    }
}

// 0x762E20
INT CGameSprite::GetFeatValue(UINT nFeatNumber)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 28968
    UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

    if ((m_baseStats.m_feats[nFeatNumber >> 5] & (1 << (nFeatNumber & 0x1F))) == 0) {
        return 0;
    }

    switch (nFeatNumber) {
    case CGAMESPRITE_FEAT_ARMOR_PROF:
        return m_baseStats.m_featArmorProficiency;
    case CGAMESPRITE_FEAT_ARMORED_ARCANA:
        return m_baseStats.m_featArmoredArcana;
    case CGAMESPRITE_FEAT_CLEAVE:
        return m_baseStats.m_featCleave;
    case CGAMESPRITE_FEAT_EXOTIC_BASTARD:
        return m_baseStats.m_featBastardSword;
    case CGAMESPRITE_FEAT_EXTRA_RAGE:
        return m_baseStats.m_featExtraRage;
    case CGAMESPRITE_FEAT_EXTRA_SHAPESHIFTING:
        return m_baseStats.m_featExtraShapeshifting;
    case CGAMESPRITE_FEAT_EXTRA_SMITING:
        return m_baseStats.m_featExtraSmiting;
    case CGAMESPRITE_FEAT_EXTRA_TURNING:
        return m_baseStats.m_featExtraTurning;
    case CGAMESPRITE_FEAT_MARTIAL_AXE:
        return m_baseStats.m_featAxe;
    case CGAMESPRITE_FEAT_MARTIAL_BOW:
        return m_baseStats.m_featBow;
    case CGAMESPRITE_FEAT_MARTIAL_FLAIL:
        return m_baseStats.m_featFlail;
    case CGAMESPRITE_FEAT_MARTIAL_GREATSWORD:
        return m_baseStats.m_featGreatSword;
    case CGAMESPRITE_FEAT_MARTIAL_HAMMER:
        return m_baseStats.m_featHammer;
    case CGAMESPRITE_FEAT_MARTIAL_LARGESWORD:
        return m_baseStats.m_featLargeSword;
    case CGAMESPRITE_FEAT_MARTIAL_POLEARM:
        return m_baseStats.m_featPolearm;
    case CGAMESPRITE_FEAT_SIMPLE_CROSSBOW:
        return m_baseStats.m_featCrossbow;
    case CGAMESPRITE_FEAT_SIMPLE_MACE:
        return m_baseStats.m_featMace;
    case CGAMESPRITE_FEAT_SIMPLE_MISSILE:
        return m_baseStats.m_featMissile;
    case CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF:
        return m_baseStats.m_featQuarterStaff;
    case CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE:
        return m_baseStats.m_featSmallBlade;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT:
        return m_baseStats.m_featFocusEnchantment;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION:
        return m_baseStats.m_featFocusEvocation;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY:
        return m_baseStats.m_featFocusNecromancy;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE:
        return m_baseStats.m_featFocusTransmutation;
    case CGAMESPRITE_FEAT_SPELL_PENETRATION:
        return m_baseStats.m_featSpellPenetration;
    case CGAMESPRITE_FEAT_TOUGHNESS:
        return m_baseStats.m_featToughness;
    }
    return 1;
}

// 0x7630A0
INT CGameSprite::GetMaxFeatValue(UINT nFeatNumber)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 29126
    UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

    switch (nFeatNumber) {
    case CGAMESPRITE_FEAT_ARMOR_PROF:
        return CGAMESPRITE_FEAT_MAX_ARMOR_PROFICIENCY_UPGRADE;
    case CGAMESPRITE_FEAT_ARMORED_ARCANA:
        return CGAMESPRITE_FEAT_MAX_ARMORED_ARCANA_UPGRADE;
    case CGAMESPRITE_FEAT_CLEAVE:
        return CGAMESPRITE_FEAT_MAX_CLEAVE_UPGRADE;
    case CGAMESPRITE_FEAT_EXOTIC_BASTARD:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_EXTRA_RAGE:
        return CGAMESPRITE_FEAT_MAX_EXTRA_RAGE_UPGRADE;
    case CGAMESPRITE_FEAT_EXTRA_SHAPESHIFTING:
        return CGAMESPRITE_FEAT_MAX_EXTRA_SHAPESHIFTING_UPGRADE;
    case CGAMESPRITE_FEAT_EXTRA_SMITING:
        return CGAMESPRITE_FEAT_MAX_EXTRA_SMITING_UPGRADE;
    case CGAMESPRITE_FEAT_EXTRA_TURNING:
        return CGAMESPRITE_FEAT_MAX_EXTRA_TURNING_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_AXE:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_BOW:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_FLAIL:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_GREATSWORD:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_HAMMER:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_LARGESWORD:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_MARTIAL_POLEARM:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_SIMPLE_CROSSBOW:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_SIMPLE_MACE:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_SIMPLE_MISSILE:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE:
        return CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT:
        return CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_ENCHANTMENT_UPGRADE;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION:
        return CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_EVOCATION_UPGRADE;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY:
        return CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_NECROMANCY_UPGRADE;
    case CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE:
        return CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_TRANSMUTATION_UPGRADE;
    case CGAMESPRITE_FEAT_SPELL_PENETRATION:
        return CGAMESPRITE_FEAT_MAX_SPELL_PENETRATION_UPGRADE;
    case CGAMESPRITE_FEAT_TOUGHNESS:
        return CGAMESPRITE_FEAT_TOUGHNESS_MAX_LEVEL;
    }

    return 1;
}

// 0x763150
BOOL CGameSprite::sub_763150(UINT nFeatNumber)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 29278
    UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

    if ((m_baseStats.m_feats[nFeatNumber >> 5] & (1 << (nFeatNumber & 0x1F))) == 0) {
        return 0;
    }

    if (nFeatNumber == CGAMESPRITE_FEAT_CLEAVE) {
        if (m_derivedStats.m_nSTR >= 13) {
            if (sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK)) {
                INT nValue = GetFeatValue(CGAMESPRITE_FEAT_CLEAVE);
                if (nValue == 1 || nValue == 2) {
                    if (m_baseStats.m_attackBase >= 4) {
                        return TRUE;
                    }
                }
            }
        }
        return FALSE;
    }

    return sub_763200(nFeatNumber, 0);
}

// 0x763200
BOOL CGameSprite::sub_763200(UINT nFeatNumber, INT a2)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 29333
    UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

    SHORT nSTR = m_derivedStats.m_nSTR;
    SHORT nDEX = m_derivedStats.m_nDEX;
    SHORT nINT = m_derivedStats.m_nINT;
    SHORT nWIS = m_derivedStats.m_nWIS;

    if (a2 == 1) {
        nSTR = m_baseStats.m_STRBase;
        nDEX = m_baseStats.m_DEXBase;
        nINT = m_baseStats.m_INTBase;
        nWIS = m_baseStats.m_WISBase;
    }

    switch (nFeatNumber) {
    case CGAMESPRITE_FEAT_AEGIS_OF_RIME:
    case CGAMESPRITE_FEAT_AQUA_MORTIS:
    case CGAMESPRITE_FEAT_SCION_OF_STORMS:
    case CGAMESPRITE_FEAT_SPIRIT_OF_FLAME:
        return (m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_BARD) >= 11
                   || m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_CLERIC) >= 7
                   || m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_DRUID) >= 7
                   || m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_WIZARD) >= 7
                   || m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_PALADIN) >= 15
                   || m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_RANGER) >= 15
                   || m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_RANGER) >= 8)
            && m_derivedStats.m_nSkills[13] >= 10;
    case CGAMESPRITE_FEAT_AMBIDEXTERITY:
        return nDEX >= 15;
    case CGAMESPRITE_FEAT_ARMOR_PROF:
    case CGAMESPRITE_FEAT_BLIND_FIGHT:
    case CGAMESPRITE_FEAT_COURTEOUS_MAGOCRACY:
    case CGAMESPRITE_FEAT_DASH:
    case CGAMESPRITE_FEAT_DISCIPLINE:
    case CGAMESPRITE_FEAT_FORESTER:
    case CGAMESPRITE_FEAT_GREAT_FORTITUDE:
    case CGAMESPRITE_FEAT_IMPROVED_INITIATIVE:
    case CGAMESPRITE_FEAT_IRON_WILL:
    case CGAMESPRITE_FEAT_LIGHTNING_REFLEXES:
    case CGAMESPRITE_FEAT_PRECISE_SHOT:
    case CGAMESPRITE_FEAT_SHIELD_PROF:
    case CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT:
    case CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION:
    case CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY:
    case CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE:
    case CGAMESPRITE_FEAT_SPELL_PENETRATION:
    case CGAMESPRITE_FEAT_STRONG_BACK:
    case CGAMESPRITE_FEAT_TOUGHNESS:
    case CGAMESPRITE_FEAT_TWO_WEAPON_FIGHTING:
        return TRUE;
    case CGAMESPRITE_FEAT_ARMORED_ARCANA:
        return m_derivedStats.HasClassMask(CLASSMASK_WIZARD | CLASSMASK_SORCERER | CLASSMASK_BARD);
    case CGAMESPRITE_FEAT_ARTERIAL_STRIKE:
        return m_baseStats.m_rogueLevel > 0 && m_baseStats.m_attackBase >= 4;
    case CGAMESPRITE_FEAT_BULLHEADED:
        return m_typeAI.m_nRace == CAIOBJECTTYPE_R_HUMAN
            || m_typeAI.m_nRace == CAIOBJECTTYPE_R_DWARF;
    case CGAMESPRITE_FEAT_CLEAVE:
        if (!(nSTR >= 13 && sub_763150(CGAMESPRITE_FEAT_POWER_ATTACK))) {
            return FALSE;
        }

        if (GetFeatValue(CGAMESPRITE_FEAT_CLEAVE) >= 1) {
            return m_baseStats.m_attackBase >= 4;
        }

        return TRUE;
    case CGAMESPRITE_FEAT_COMBAT_CASTING:
        return m_derivedStats.HasClassMask(CLASSMASK_WIZARD | CLASSMASK_SORCERER | CLASSMASK_BARD)
            || m_derivedStats.HasClassMask(CLASSMASK_RANGER | CLASSMASK_PALADIN | CLASSMASK_DRUID | CLASSMASK_CLERIC);
    case CGAMESPRITE_FEAT_CRIPPLING_STRIKE:
        return m_baseStats.m_rogueLevel >= 10;
    case CGAMESPRITE_FEAT_DEFLECT_ARROWS:
    case CGAMESPRITE_FEAT_DODGE:
    case CGAMESPRITE_FEAT_RAPID_SHOT:
        return nDEX >= 13;
    case CGAMESPRITE_FEAT_DIRTY_FIGHTING:
        return nDEX >= 13 && m_baseStats.m_attackBase >= 2;
    case CGAMESPRITE_FEAT_ENVENOM_WEAPON:
        return m_baseStats.m_rogueLevel > 0
            && m_baseStats.m_skills[CGAMESPRITE_SKILL_ALCHEMY] >= 8;
    case CGAMESPRITE_FEAT_EXOTIC_BASTARD:
        return GetFeatValue(CGAMESPRITE_FEAT_EXOTIC_BASTARD) != 2
            || m_baseStats.m_fighterLevel >= 4;
    case CGAMESPRITE_FEAT_EXPERTISE:
        return nINT >= 13;
    case CGAMESPRITE_FEAT_EXTRA_RAGE:
        return m_baseStats.m_barbarianLevel > 0;
    case CGAMESPRITE_FEAT_EXTRA_SHAPESHIFTING:
        return m_baseStats.m_druidLevel >= 3;
    case CGAMESPRITE_FEAT_EXTRA_SMITING:
        return m_baseStats.m_paladinLevel >= 2;
    case CGAMESPRITE_FEAT_EXTRA_TURNING:
        return m_baseStats.m_paladinLevel >= 3
            || m_baseStats.m_clericLevel > 0;
    case CGAMESPRITE_FEAT_FIENDSLAYER:
        return (GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_LARGESWORD) >= 2
                   || GetFeatValue(CGAMESPRITE_FEAT_MARTIAL_GREATSWORD) >= 2)
            && m_baseStats.m_paladinLevel >= 3;
    case CGAMESPRITE_FEAT_HAMSTRING:
        return m_baseStats.m_rogueLevel >= 3;
    case CGAMESPRITE_FEAT_HERETICS_BANE:
        return m_baseStats.m_attackBase >= 4
            && m_derivedStats.HasClassMask(CLASSMASK_RANGER | CLASSMASK_PALADIN | CLASSMASK_DRUID | CLASSMASK_CLERIC);
    case CGAMESPRITE_FEAT_HEROIC_INSPIRATION:
        return m_baseStats.m_barbarianLevel > 0
            || m_baseStats.m_barbarianLevel > 0
            || m_baseStats.m_paladinLevel > 0;
    case CGAMESPRITE_FEAT_IMPROVED_CRITICAL:
        return m_baseStats.m_attackBase >= 8;
    case CGAMESPRITE_FEAT_IMPROVED_EVASION:
        return m_baseStats.m_rogueLevel >= 10
            || m_baseStats.m_monkLevel >= 9;
    case CGAMESPRITE_FEAT_IMPROVED_TURNING:
        return m_baseStats.m_clericLevel > 0
            || m_baseStats.m_paladinLevel >= 3;
    case CGAMESPRITE_FEAT_LINGERING_SONG:
        return m_baseStats.m_bardLevel > 0;
    case CGAMESPRITE_FEAT_LUCK_OF_HEROES:
        return m_typeAI.m_nRace == CAIOBJECTTYPE_R_HUMAN;
    case CGAMESPRITE_FEAT_MARTIAL_AXE:
    case CGAMESPRITE_FEAT_MARTIAL_BOW:
    case CGAMESPRITE_FEAT_MARTIAL_FLAIL:
    case CGAMESPRITE_FEAT_MARTIAL_GREATSWORD:
    case CGAMESPRITE_FEAT_MARTIAL_HAMMER:
    case CGAMESPRITE_FEAT_MARTIAL_LARGESWORD:
    case CGAMESPRITE_FEAT_MARTIAL_POLEARM:
        return GetFeatValue(nFeatNumber) != 3 || m_baseStats.m_fighterLevel >= 4;
    case CGAMESPRITE_FEAT_MAXIMIZED_ATTACKS:
        if (1) {
            INT nWeaponSpecializations = 0;
            for (INT nFeatNumber = CGAMESPRITE_FEAT_MARTIAL_AXE; nFeatNumber <= CGAMESPRITE_FEAT_MARTIAL_POLEARM; nFeatNumber++) {
                if (GetFeatValue(nFeatNumber) == 3) {
                    nWeaponSpecializations++;
                }
            }

            if (GetFeatValue(CGAMESPRITE_FEAT_EXOTIC_BASTARD) == 3) {
                nWeaponSpecializations++;
            }

            if (nWeaponSpecializations < 2) {
                for (INT nFeatNumber = CGAMESPRITE_FEAT_SIMPLE_CROSSBOW; nFeatNumber <= CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE; nFeatNumber++) {
                    if (GetFeatValue(nFeatNumber) == 3) {
                        nWeaponSpecializations++;
                    }
                }
            }

            return nWeaponSpecializations >= 2
                && m_baseStats.m_skills[CGAMESPRITE_SKILL_CONCENTRATION] >= 4;
        }
    case CGAMESPRITE_FEAT_MERCANTILE_BACKGROUND:
        // TODO: Incomplete.
        return FALSE;
    case CGAMESPRITE_FEAT_SIMPLE_CROSSBOW:
    case CGAMESPRITE_FEAT_SIMPLE_MACE:
    case CGAMESPRITE_FEAT_SIMPLE_MISSILE:
    case CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF:
    case CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE:
        return GetFeatValue(nFeatNumber) != 3 || m_baseStats.m_fighterLevel >= 4;
    case CGAMESPRITE_FEAT_SNAKE_BLOOD:
        // TODO: Incomplete.
        return FALSE;
    case CGAMESPRITE_FEAT_STUNNING_FIST:
        return (nDEX >= 13 && nWIS >= 13 && m_baseStats.m_attackBase >= 8)
            || m_baseStats.m_monkLevel > 0;
    case CGAMESPRITE_FEAT_SUBVOCAL_CASTING:
        return m_derivedStats.HasClassMask(CLASSMASK_WIZARD | CLASSMASK_SORCERER | CLASSMASK_BARD)
            || m_derivedStats.HasClassMask(CLASSMASK_RANGER | CLASSMASK_PALADIN | CLASSMASK_DRUID | CLASSMASK_CLERIC);
    case CGAMESPRITE_FEAT_WEAPON_FINESSE:
        return GetFeatValue(CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE)
            && m_baseStats.m_attackBase >= 1;
    case CGAMESPRITE_FEAT_WILDSHAPE_BOAR:
    case CGAMESPRITE_FEAT_WILDSHAPE_PANTHER:
        return m_baseStats.m_druidLevel >= 5;
    case CGAMESPRITE_FEAT_WILDSHAPE_SHAMBLER:
        return m_baseStats.m_druidLevel >= 8;
    }

    return FALSE;
}

// 0x763A40
BOOL CGameSprite::sub_763A40(UINT nFeatNumber, INT a2)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 29873
    UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

    switch (nFeatNumber) {
    case CGAMESPRITE_FEAT_ARMOR_PROF:
        return m_baseStats.m_featArmorProficiency < CGAMESPRITE_FEAT_MAX_ARMOR_PROFICIENCY_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_ARMORED_ARCANA:
        return m_baseStats.m_featArmoredArcana < CGAMESPRITE_FEAT_MAX_ARMORED_ARCANA_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_CLEAVE:
        return m_baseStats.m_featCleave < CGAMESPRITE_FEAT_MAX_CLEAVE_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_EXOTIC_BASTARD:
        return m_baseStats.m_featBastardSword < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_EXTRA_RAGE:
        return m_baseStats.m_featExtraRage < CGAMESPRITE_FEAT_MAX_EXTRA_RAGE_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_EXTRA_SHAPESHIFTING:
        return m_baseStats.m_featExtraShapeshifting < CGAMESPRITE_FEAT_MAX_EXTRA_SHAPESHIFTING_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_EXTRA_SMITING:
        return m_baseStats.m_featExtraSmiting < CGAMESPRITE_FEAT_MAX_EXTRA_SMITING_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_EXTRA_TURNING:
        return m_baseStats.m_featExtraTurning < CGAMESPRITE_FEAT_MAX_EXTRA_TURNING_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_AXE:
        return (m_baseStats.m_featAxe != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featAxe < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_BOW:
        return (m_baseStats.m_featBow != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featBow < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_FLAIL:
        return (m_baseStats.m_featFlail != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featFlail < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_GREATSWORD:
        return (m_baseStats.m_featGreatSword != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featGreatSword < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_HAMMER:
        return (m_baseStats.m_featHammer != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featHammer < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_LARGESWORD:
        return (m_baseStats.m_featLargeSword != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featLargeSword < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_MARTIAL_POLEARM:
        return (m_baseStats.m_featPolearm != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featPolearm < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SIMPLE_CROSSBOW:
        return (m_baseStats.m_featCrossbow != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featCrossbow < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SIMPLE_MACE:
        return (m_baseStats.m_featMace != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featMace < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SIMPLE_MISSILE:
        return (m_baseStats.m_featMissile != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featMissile < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF:
        return (m_baseStats.m_featQuarterStaff != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featQuarterStaff < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE:
        return (m_baseStats.m_featSmallBlade != CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE - 1
                   || m_baseStats.m_attackBase >= 4)
            && m_baseStats.m_featSmallBlade < CGAMESPRITE_FEAT_MAX_WEAPON_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT:
        return m_baseStats.m_featFocusEnchantment < CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_ENCHANTMENT_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION:
        return m_baseStats.m_featFocusEvocation < CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_EVOCATION_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY:
        return m_baseStats.m_featFocusNecromancy < CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_NECROMANCY_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE:
        return m_baseStats.m_featFocusTransmutation < CGAMESPRITE_FEAT_MAX_SPELL_FOCUS_TRANSMUTATION_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_SPELL_PENETRATION:
        return m_baseStats.m_featSpellPenetration < CGAMESPRITE_FEAT_MAX_SPELL_PENETRATION_UPGRADE
            && sub_763200(nFeatNumber, 1);
    case CGAMESPRITE_FEAT_TOUGHNESS:
        return m_baseStats.m_featToughness < CGAMESPRITE_FEAT_TOUGHNESS_MAX_LEVEL
            && sub_763200(nFeatNumber, 1);
    }

    if ((m_baseStats.m_feats[nFeatNumber >> 5] & (1 << (nFeatNumber & 0x1F))) != 0) {
        return FALSE;
    }

    if (a2) {
        return sub_763200(nFeatNumber, 1);
    }

    return TRUE;
}

// 0x763CB0
int CGameSprite::GetExtraFeats(BYTE nClass)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    int classLevel = m_derivedStats.GetClassLevel(nClass);

    int feats = atol(ruleTables.m_tFeatLevel.GetAt(CPoint(0, m_derivedStats.m_nLevel - 1)));
    feats += atol(ruleTables.m_tFeatClass.GetAt(CPoint(nClass - 1, classLevel - 1)));

    if (m_derivedStats.m_nLevel == 1) {
        if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_HUMAN
            && m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_PURERACE) {
            feats++;
        } else if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_HALFLING
            && m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_HALFLING_STRONGHEART) {
            feats++;
        }
    }

    return feats;
}

// 0x763DA0
int CGameSprite::GetExtraSkillPoints(BYTE nClass)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    int sp = atol(ruleTables.m_tSkillPoints.GetAt(CPoint(nClass - 1, 0)));

    sp += ruleTables.GetAbilityScoreModifier(m_baseStats.m_INTBase);

    if (sp < 1) {
        sp = 1;
    }

    if (m_derivedStats.m_nLevel == 1) {
        sp *= 4;
    }

    if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_HUMAN
        && m_typeAI.m_nSubRace == CAIOBJECTTYPE_SUBRACE_PURERACE) {
        if (m_derivedStats.m_nLevel == 1) {
            sp++;
        }
        sp++;
    }

    return sp;
}

// 0x763E40
INT CGameSprite::sub_763E40(UINT nSkill)
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    INT nBonus;
    INT nScore;

    LONG bUntrained = ruleTables.m_tSkills.GetAtLong(CPoint(3, nSkill));

    nBonus = 0;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 30444
    UTIL_ASSERT(nSkill < 16);

    if (m_baseStats.m_skills[nSkill] == 0 && bUntrained == 0) {
        return 0;
    }

    if (m_derivedStats.HasClass(CAIOBJECTTYPE_C_CLERIC)) {
        UINT nColumn;
        switch (m_baseStats.m_specialization & 0xFF8000) {
        case 0x8000:
            nColumn = 0;
            break;
        case 0x10000:
            nColumn = 1;
            break;
        case 0x20000:
            nColumn = 2;
            break;
        case 0x40000:
            nColumn = 3;
            break;
        case 0x80000:
            nColumn = 4;
            break;
        case 0x100000:
            nColumn = 5;
            break;
        case 0x200000:
            nColumn = 6;
            break;
        case 0x400000:
            nColumn = 7;
            break;
        case 0x800000:
            nColumn = 8;
            break;
        default:
            nColumn = nSkill;
            break;
        }

        nBonus = ruleTables.m_tSkills.GetAtLong(CPoint(nColumn + 15, nSkill));
    }

    switch (nSkill) {
    case 0:
        if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_GNOME && m_typeAI.m_nSubRace == 0) {
            nBonus += 2;
        }
        if (m_derivedStats.HasClass(CAIOBJECTTYPE_C_BARD)) {
            nBonus++;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT) + nBonus;
    case 1:
    case 2:
    case 14:
        if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_HUMAN
            && m_typeAI.m_nSubRace == 2
            && nSkill == 2) {
            nBonus += 2;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nCHR) + nBonus;
    case 3:
        nScore = ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nCON);
        if (sub_763150(15)) {
            nScore += 2;
        }
        if (sub_763150(9)) {
            nScore += 4;
        }
        return nScore;
    case 4:
        if (sub_763150(10)) {
            nBonus += 2;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nCHR) + nBonus;
    case 5:
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT);
    case 6:
        if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_HUMAN) {
            if (m_typeAI.m_nSubRace == 2) {
                nBonus += 2;
            }
        } else if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_GNOME && m_typeAI.m_nSubRace == 1) {
            nBonus += 2;
        }
        nScore = m_derivedStats.m_nDEX;
        break;
    case 7:
        if (sub_763150(7)) {
            nBonus += 2;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nCHR) + nBonus;
    case 8:
        if (m_derivedStats.HasClass(CAIOBJECTTYPE_C_BARD)) {
            nBonus++;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT) + nBonus;
    case 9:
        if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_HALFLING
            || (m_typeAI.m_nRace == CAIOBJECTTYPE_R_DWARF && m_typeAI.m_nSubRace == 2)) {
            nBonus += 4;
        }
        nScore = m_derivedStats.m_nDEX;
        break;
    case 10:
    case 11:
        nScore = m_derivedStats.m_nDEX;
        break;
    case 12:
        switch (m_typeAI.m_nRace) {
        case CAIOBJECTTYPE_R_ELF:
        case CAIOBJECTTYPE_R_DWARF:
            return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT) + nBonus + 2;
        case CAIOBJECTTYPE_R_HALF_ELF:
            nBonus++;
            break;
        case CAIOBJECTTYPE_R_GNOME:
            if (m_typeAI.m_nRace == CAIOBJECTTYPE_R_GNOME && m_typeAI.m_nSubRace == 1) {
                return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT) + nBonus + 2;
            }
            break;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT) + nBonus;
    case 13:
        if (sub_763150(10)) {
            nBonus += 2;
        }
        if (m_derivedStats.HasClass(CAIOBJECTTYPE_C_BARD)) {
            nBonus++;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nINT) + nBonus;
    case 15:
        if (sub_763150(25)) {
            nBonus += 2;
        }
        return ruleTables.GetAbilityScoreModifier(m_derivedStats.m_nWIS) + nBonus;
    default:
        return 0;
    }

    return sub_724010(ruleTables.GetAbilityScoreModifier(nScore)) + nBonus;
}

// 0x764270
void CGameSprite::SetSkillValue(UINT iSkillNumber, INT iSkillValue)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 30424
    UTIL_ASSERT(iSkillValue >= 0);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 30425
    UTIL_ASSERT(iSkillNumber < CGAMESPRITE_SKILL_NUMSKILLS);

    m_baseStats.m_skills[iSkillNumber] = static_cast<BYTE>(iSkillValue);
}

// 0x7642D0
INT CGameSprite::GetSkillValue(UINT iSkillNumber)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 30444
    UTIL_ASSERT(iSkillNumber < CGAMESPRITE_SKILL_NUMSKILLS);

    return m_baseStats.m_skills[iSkillNumber];
}

// 0x764310
INT CGameSprite::GetSkillCost(UINT iSkillNumber, BYTE nClass)
{
    INT iClassType;

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 30463
    UTIL_ASSERT(iSkillNumber < CGAMESPRITE_SKILL_NUMSKILLS);

    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    int v1 = 0;
    int v2 = 0;
    int v3 = 0;

    // A way to bypass condition below without `goto`. Need a better
    // understanding of the calculations to get rid of it.
    // TODO: Review calculations.
    bool check = true;

    for (iClassType = 1; iClassType <= CAIOBJECT_CLASS_MAX; iClassType++) {
        if (m_derivedStats.HasClass(iClassType)) {
            INT nCost = atol(ruleTables.m_tSkillCosts.GetAt(CPoint(iClassType - 1, iSkillNumber)));
            if (nCost == 1) {
                v3 = 1;
                if (iClassType == nClass) {
                    v2 = 1;
                    check = false;
                    break;
                }

                if (v1 > 0) {
                    check = false;
                    break;
                }
            } else if (nCost == 2) {
                if (v3 == 0) {
                    v3 = 2;
                }

                if (iClassType == nClass) {
                    v2 = 2;
                    v1 = 2;
                }
            } else if (nCost > 2) {
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
                // __LINE__: 30508
                UTIL_ASSERT_MSG(FALSE, "Invalid Skill Cost (not 0, 1 or 2) in SKILCOST.2DA");
            }
        }
    }

    if (check) {
        if (v1 <= 0) {
            return v1;
        }
    }

    INT nLevels = 0;
    for (iClassType = 1; iClassType <= CAIOBJECT_CLASS_MAX; iClassType++) {
        INT nLevel = m_derivedStats.GetClassLevel(iClassType);
        if (nLevel > 0) {
            INT nCost = atol(ruleTables.m_tSkillCosts.GetAt(CPoint(iClassType - 1, iSkillNumber)));
            if (nCost > 0) {
                nLevels += nLevel;
            }
        }
    }

    if (m_baseStats.m_skills[iSkillNumber] < (nLevels + 3) / v3) {
        return v2;
    }

    return 0;
}

// 0x764520
INT CGameSprite::GetEffectiveCharacterLevel()
{
    INT nLevel = m_derivedStats.m_nLevel;

    switch (m_typeAI.m_nRace) {
    case CAIOBJECTTYPE_R_HUMAN:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_HUMAN_AASIMAR:
        case CAIOBJECTTYPE_SUBRACE_HUMAN_TIEFLING:
            nLevel += 1;
            break;
        }
        break;
    case CAIOBJECTTYPE_R_ELF:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_ELF_DROW:
            nLevel += 2;
            break;
        }
        break;
    case CAIOBJECTTYPE_R_DWARF:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_DWARF_GRAY:
            nLevel += 2;
            break;
        }
        break;
    case CAIOBJECTTYPE_R_GNOME:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_GNOME_DEEP:
            nLevel += 3;
            break;
        }
        break;
    }

    return nLevel;
}

// 0x764590
INT CGameSprite::GetRacialFavoredClass(DWORD& nSpecialization)
{
    INT iClass;

    nSpecialization = 0;

    switch (m_typeAI.m_nRace) {
    case CAIOBJECTTYPE_R_HUMAN:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_PURERACE:
            iClass = m_derivedStats.GetBestClass();
            if (m_derivedStats.m_nLevel == 0) {
                iClass = CAIOBJECTTYPE_C_FIGHTER;
            }
            break;
        case CAIOBJECTTYPE_SUBRACE_HUMAN_AASIMAR:
            iClass = CAIOBJECTTYPE_C_PALADIN;
            break;
        case CAIOBJECTTYPE_SUBRACE_HUMAN_TIEFLING:
            iClass = CAIOBJECTTYPE_C_ROGUE;
            break;
        }
        break;
    case CAIOBJECTTYPE_R_ELF:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_PURERACE:
            iClass = CAIOBJECTTYPE_C_WIZARD;
            break;
        case CAIOBJECTTYPE_SUBRACE_ELF_DROW:
            switch (m_typeAI.m_nGender) {
            case CAIOBJECTTYPE_SEX_MALE:
                iClass = CAIOBJECTTYPE_C_WIZARD;
                break;
            case CAIOBJECTTYPE_SEX_FEMALE:
                iClass = CAIOBJECTTYPE_C_CLERIC;
                break;
            default:
                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
                // __LINE__: 30684
                UTIL_ASSERT_MSG(FALSE, "Character has no gender");
            }
            break;
        case CAIOBJECTTYPE_SUBRACE_ELF_WILD:
            iClass = CAIOBJECTTYPE_C_SORCERER;
            break;
        }
        break;
    case CAIOBJECTTYPE_R_HALF_ELF:
        iClass = m_derivedStats.GetBestClass();
        if (m_derivedStats.m_nLevel == 0) {
            iClass = CAIOBJECTTYPE_C_FIGHTER;
        }
        break;
    case CAIOBJECTTYPE_R_DWARF:
        iClass = CAIOBJECTTYPE_C_FIGHTER;
        break;
    case CAIOBJECTTYPE_R_HALFLING:
        switch (m_typeAI.m_nSubRace) {
        case CAIOBJECTTYPE_SUBRACE_PURERACE:
        case CAIOBJECTTYPE_SUBRACE_HALFLING_STRONGHEART:
            iClass = CAIOBJECTTYPE_C_ROGUE;
            break;
        case CAIOBJECTTYPE_SUBRACE_HALFLING_GHOSTWISE:
            // TODO: This does not look right.
            iClass = CAIOBJECTTYPE_C_BARBARIAN;
            break;
        }
        break;
    case CAIOBJECTTYPE_R_GNOME:
        iClass = CAIOBJECTTYPE_C_WIZARD;
        nSpecialization = 0x400; // WIZARD_ILLUSIONIST
        break;
    case CAIOBJECTTYPE_R_HALF_ORC:
        iClass = CAIOBJECTTYPE_C_BARBARIAN;
        break;
    }

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 30733
    UTIL_ASSERT_MSG(iClass != 0, "GetRacialFavoredClass() returned 0!");

    return iClass;
}

// 0x7646E0
INT CGameSprite::GetMulticlassingPenalty()
{
    if (m_derivedStats.m_nLevel == 0) {
        return 0;
    }

    DWORD nSpecialization = 0;
    INT iExcludedRacialClass = 0;
    INT iRacialFavoredClass = GetRacialFavoredClass(nSpecialization);
    INT iBestClass = m_derivedStats.GetBestClass();

    if (iRacialFavoredClass == iBestClass
        && (nSpecialization == 0
            || (m_baseStats.m_specialization & nSpecialization) != 0)) {
        if (m_derivedStats.GetClassLevel(iBestClass) == m_derivedStats.m_nLevel) {
            // Means there is no multiclassing.
            return 0;
        }

        iExcludedRacialClass = iRacialFavoredClass;
        iBestClass = 0;
        for (INT iClass = 1; iClass <= CAIOBJECT_CLASS_MAX; iClass++) {
            if (iClass != iRacialFavoredClass) {
                if (m_derivedStats.GetClassLevel(iClass) != 0) {
                    if (iBestClass == 0
                        || m_derivedStats.GetClassLevel(iClass) > m_derivedStats.GetClassLevel(iBestClass)) {
                        iBestClass = iClass;
                    }
                }
            }
        }
    }

    INT nBestClassLevel = m_derivedStats.GetClassLevel(iBestClass);
    if (nBestClassLevel <= 2) {
        return 0;
    }

    INT nPenalty = 0;
    for (INT iClass = 1; iClass <= CAIOBJECT_CLASS_MAX; iClass++) {
        if (iClass != iExcludedRacialClass && iClass != iBestClass) {
            INT nClassLevel = m_derivedStats.GetClassLevel(iClass);
            if (nClassLevel != 0) {
                if (nBestClassLevel - nClassLevel > 1
                    && iClass != iRacialFavoredClass) {
                    nPenalty += 20;
                }
            }
        }
    }

    // 100% penalty, really?
    if (nPenalty > 100) {
        nPenalty = 100;
    }

    return nPenalty;
}

// 0x765BD0
void CGameSprite::SetColorRange(BYTE rangeValue)
{
    m_hasColorRangeEffects = TRUE;

    // NOTE: Uninline.
    m_animation.SetColorRangeAll(rangeValue);

    CColorRange* pColorRange = new CColorRange();
    pColorRange->m_range = -1;
    pColorRange->m_color = rangeValue;
    m_derivedStats.m_appliedColorRanges.AddTail(pColorRange);
}

// 0x765C50
void CGameSprite::DisplayFeats(CUIControlTextDisplay* pText)
{
    CBaldurEngine* pEngine = g_pBaldurChitin->GetActiveEngine();
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 31489
    UTIL_ASSERT(pText != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 31490
    UTIL_ASSERT(pEngine != NULL);

    for (int index = 0; index < CGAMESPRITE_FEAT_NUMFEATS; index++) {
        UINT nFeatNumber = ruleTables.GetFeatId(index);
        INT nFeatRank = GetFeatValue(nFeatNumber);
        if (nFeatRank > 0) {
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
            // __LINE__: 29233
            UTIL_ASSERT(nFeatNumber < CGAMESPRITE_FEAT_NUMFEATS);

            if ((m_baseStats.m_feats[nFeatNumber >> 5] & (1 << (nFeatNumber & 0x1F))) != 0) {
                switch (nFeatNumber) {
                case CGAMESPRITE_FEAT_ARMOR_PROF:
                case CGAMESPRITE_FEAT_ARMORED_ARCANA:
                case CGAMESPRITE_FEAT_CLEAVE:
                case CGAMESPRITE_FEAT_EXOTIC_BASTARD:
                case CGAMESPRITE_FEAT_EXTRA_RAGE:
                case CGAMESPRITE_FEAT_EXTRA_SHAPESHIFTING:
                case CGAMESPRITE_FEAT_EXTRA_SMITING:
                case CGAMESPRITE_FEAT_EXTRA_TURNING:
                case CGAMESPRITE_FEAT_MARTIAL_AXE:
                case CGAMESPRITE_FEAT_MARTIAL_BOW:
                case CGAMESPRITE_FEAT_MARTIAL_FLAIL:
                case CGAMESPRITE_FEAT_MARTIAL_GREATSWORD:
                case CGAMESPRITE_FEAT_MARTIAL_HAMMER:
                case CGAMESPRITE_FEAT_MARTIAL_LARGESWORD:
                case CGAMESPRITE_FEAT_MARTIAL_POLEARM:
                case CGAMESPRITE_FEAT_SIMPLE_CROSSBOW:
                case CGAMESPRITE_FEAT_SIMPLE_MACE:
                case CGAMESPRITE_FEAT_SIMPLE_MISSILE:
                case CGAMESPRITE_FEAT_SIMPLE_QUARTERSTAFF:
                case CGAMESPRITE_FEAT_SIMPLE_SMALLBLADE:
                case CGAMESPRITE_FEAT_SPELL_FOCUS_ENCHANTMENT:
                case CGAMESPRITE_FEAT_SPELL_FOCUS_EVOCATION:
                case CGAMESPRITE_FEAT_SPELL_FOCUS_NECROMANCY:
                case CGAMESPRITE_FEAT_SPELL_FOCUS_TRANSMUTE:
                case CGAMESPRITE_FEAT_SPELL_PENETRATION:
                case CGAMESPRITE_FEAT_TOUGHNESS:
                    CBaldurEngine::UpdateText(pText,
                        "%s: %d",
                        (LPCSTR)CBaldurEngine::FetchString(ruleTables.GetFeatName(nFeatNumber)),
                        nFeatRank);
                    break;
                default:
                    CBaldurEngine::UpdateText(pText,
                        "%s",
                        (LPCSTR)CBaldurEngine::FetchString(ruleTables.GetFeatName(nFeatNumber)));
                    break;
                }
            } else {
                CBaldurEngine::UpdateText(pText,
                    "%s",
                    (LPCSTR)CBaldurEngine::FetchString(ruleTables.GetFeatName(nFeatNumber)));
            }
        }
    }
}

// 0x765E40
void CGameSprite::DisplaySkills(CUIControlTextDisplay* pText)
{
    CBaldurEngine* pEngine = g_pBaldurChitin->GetActiveEngine();
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 31521
    UTIL_ASSERT(pText != NULL);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreatureAI.cpp
    // __LINE__: 31522
    UTIL_ASSERT(pEngine != NULL);

    for (int index = 0; index < CGAMESPRITE_SKILL_NUMSKILLS; index++) {
        INT nSkillId = ruleTables.GetSkillId(index);
        if (m_baseStats.m_skills[nSkillId] == m_derivedStats.m_nSkills[nSkillId]) {
            if (m_derivedStats.m_nSkills[nSkillId] != 0) {
                CBaldurEngine::UpdateText(pText,
                    "%s: %d",
                    (LPCSTR)CBaldurEngine::FetchString(ruleTables.GetSkillName(nSkillId)),
                    m_derivedStats.m_nSkills[nSkillId]);
            }
        } else {
            CBaldurEngine::UpdateText(pText,
                "%s: %d (%d)",
                (LPCSTR)CBaldurEngine::FetchString(ruleTables.GetSkillName(nSkillId)),
                m_derivedStats.m_nSkills[nSkillId],
                m_baseStats.m_skills[nSkillId]);
        }
    }
}

// 0x766140
SHORT CGameSprite::JumpToArea(CString areaName, const CPoint& dest, SHORT facingDirection, SHORT delay)
{
    // TODO: Incomplete.

    return ACTION_ERROR;
}

// 0x766380
void CGameSprite::MoveOntoArea(CGameArea* pArea, const CPoint& dest, SHORT facingDirection)
{
    // TODO: Incomplete.
}

// 0x42FDC0
void CGameSprite::SetResRef(const CResRef& resRef)
{
    m_resRef = resRef;
}

// 0x453160
void CGameSprite::sub_453160(int a1)
{
    m_bHiding = a1;
}

// 0x453170
int CGameSprite::sub_453170()
{
    return m_bHiding;
}

// 0x453180
SHORT CGameSprite::GetSequence()
{
    return m_nSequence;
}

// 0x453190
CCreatureFileHeader* CGameSprite::GetBaseStats()
{
    return &m_baseStats;
}

// 0x4531A0
CDerivedStats* CGameSprite::GetDerivedStats()
{
    return &m_derivedStats;
}

// 0x4531B0
void CGameSprite::sub_4531B0()
{
    field_562C = 1;
}

// 0x4531C0
CGameEffectList* CGameSprite::GetEquipedEffectList()
{
    return &m_equipedEffectList;
}

// 0x4531D0
CGameEffectList* CGameSprite::GetTimedEffectList()
{
    return &m_timedEffectList;
}

// 0x4531E0
void CGameSprite::SetStealthGreyOut(LONG greyOut)
{
    m_nStealthGreyOut = greyOut;
}

// 0x4AEF20
void CGameSprite::AddPortraitIcon(int icon)
{
    if (m_portraitIcons.Find(reinterpret_cast<int*>(icon), 0) == NULL) {
        m_portraitIcons.AddTail(reinterpret_cast<int*>(icon));
    }
}

// NOTE: Inlined.
void CGameSprite::RemovePortraitIcon(int icon)
{
    POSITION pos = m_portraitIcons.Find(reinterpret_cast<int*>(icon), 0);
    if (pos != NULL) {
        m_portraitIcons.RemoveAt(pos);
    }
}

// 0x5F6810
CGameSpriteSpellList* CGameSprite::GetInnateSpells()
{
    return &m_innateSpells;
}

// 0x5F6820
CGameSpriteSpellList* CGameSprite::GetSongs()
{
    return &m_songs;
}

// 0x58FED0
CGameSpriteSpellList* CGameSprite::GetShapeshifts()
{
    return &m_shapeshifts;
}

// 0x5940D0
DWORD CGameSprite::sub_5940D0()
{
    return m_baseStats.m_specialization;
}

// 0x5940E0
INT CGameSprite::sub_5940E0(BYTE buttonNum)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2028
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_CUSTOM_BUTTONS22);

    return field_3D14[buttonNum];
}

// 0x594120
void CGameSprite::sub_594120(BYTE buttonNum, int a2)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2036
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_CUSTOM_BUTTONS22);

    field_3D14[buttonNum] = a2;
}

// 0x594160
BYTE CGameSprite::GetModalState()
{
    return m_nModalState;
}

// 0x45B710
int CGameSprite::sub_45B710()
{
    return field_70F2;
}

// 0x706D50
SHORT CGameSprite::GetDirection(const CPoint& target)
{
    if (m_pos == target) {
        return m_nNewDirection;
    }

    CPoint ptStart(m_pos.x, 4 * m_pos.y / 3);
    CPoint ptTarget(target.x, 4 * target.y / 3);
    return GetDirection(ptStart, ptTarget);
}

// 0x45B720
SHORT CGameSprite::GetDirection()
{
    return m_nDirection;
}

// 0x706DD0
SHORT CGameSprite::GetDirection(const CPoint& ptStart, const CPoint& ptTarget)
{
    LONG dx;
    LONG dy;

    if (ptStart == ptTarget) {
        return 0;
    }

    if (ptTarget.y < ptStart.y) {
        if (ptTarget.x < ptStart.x) {
            dx = ptStart.x - ptTarget.y;
            dy = ptStart.y - ptTarget.y;

            if (27 * dy <= 6 * dx) {
                return 4;
            } else if (27 * dy <= 16 * dx) {
                return 6;
            } else if (24 * dy <= 38 * dx) {
                return 6;
            } else if (7 * dy <= 38 * dx) {
                return 7;
            } else {
                return 8;
            }
        } else {
            dx = ptTarget.x - ptStart.x;
            dy = ptStart.y - ptTarget.y;

            if (27 * dy <= 6 * dx) {
                return 12;
            } else if (27 * dy <= 16 * dx) {
                return 11;
            } else if (24 * dy <= 38 * dx) {
                return 10;
            } else if (7 * dy <= 38 * dx) {
                return 9;
            } else {
                return 8;
            }
        }
    } else {
        if (ptTarget.x < ptStart.x) {
            dx = ptStart.x - ptTarget.x;
            dy = ptTarget.y - ptStart.y;

            if (27 * dy <= 6 * dx) {
                return 4;
            } else if (27 * dy <= 16 * dx) {
                return 3;
            } else if (24 * dy <= 38 * dx) {
                return 2;
            } else if (7 * dy <= 38 * dx) {
                return 1;
            } else {
                return 0;
            }
        } else {
            dx = ptTarget.x - ptStart.x;
            dy = ptTarget.y - ptStart.y;

            if (27 * dy <= 6 * dx) {
                return 12;
            } else if (27 * dy <= 16 * dx) {
                return 13;
            } else if (24 * dy <= 38 * dx) {
                return 14;
            } else if (7 * dy <= 38 * dx) {
                return 15;
            } else {
                return 0;
            }
        }
    }
}

// 0x45B730
CGameAnimation* CGameSprite::GetAnimation()
{
    return &m_animation;
}

// 0x45B740
CGameSpriteEquipment* CGameSprite::GetEquipment()
{
    return &m_equipment;
}

// 0x45B750
SHORT CGameSprite::GetHappiness()
{
    return m_nHappiness;
}

// 0x45B760
BOOL CGameSprite::GetActive()
{
    return m_active;
}

// 0x45B770
CVariableHash* CGameSprite::GetLocalVariables()
{
    return m_pLocalVariables;
}

// 0x724010
INT CGameSprite::sub_724010(INT a1)
{
    CItem* pItem = m_equipment.m_items[1];
    if (pItem == NULL) {
        return 1;
    }

    switch (pItem->GetItemType()) {
    case 60:
        return min(6, a1);
    case 61:
        return min(5, a1);
    case 62:
        return min(2, a1);
    case 63:
    case 64:
        return min(0, a1);
    case 65:
        return min(1, a1);
    case 66:
        return min(4, a1);
    case 68:
        return min(3, a1);
    default:
        return a1;
    }
}

// 0x7240A0
INT CGameSprite::sub_7240A0()
{
    CItem* pItem = m_equipment.m_items[1];
    if (pItem == NULL) {
        return 0;
    }

    INT v1 = 0;

    INT nItemType = pItem->GetItemType();
    INT nFeatValue = GetFeatValue(CGAMESPRITE_FEAT_ARMOR_PROF);
    switch (nItemType) {
    case 60:
        if (nFeatValue >= 1) {
            return v1;
        }
        return 0;
    case 61:
        if (nFeatValue >= 1) {
            return v1;
        }
        v1 = -1;
        break;
    case 62:
        if (nFeatValue >= 2) {
            return v1;
        }
        v1 = -5;
        break;
    case 63:
    case 64:
        if (nFeatValue >= 3) {
            return v1;
        }
        v1 = -7;
        break;
    case 65:
        if (nFeatValue >= 3) {
            return v1;
        }
        v1 = -6;
        break;
    case 66:
        if (nFeatValue >= 2) {
            return v1;
        }
        v1 = -3;
        break;
    case 68:
        if (nFeatValue >= 3) {
            return v1;
        }
        v1 = -4;
        break;
    default:
        return v1;
    }

    if ((pItem->m_flags & 0x8) != 0) {
        v1++;
    }

    return v1;
}

// 0x724170
INT CGameSprite::sub_724170()
{
    INT nSlot = 2 * (m_nWeaponSet + 22);
    CItem* pItem = m_equipment.m_items[nSlot];
    if (pItem == NULL) {
        return 0;
    }

    INT v1 = 0;

    INT nItemType = pItem->GetItemType();
    if (GetFeatValue(CGAMESPRITE_FEAT_SHIELD_PROF) < 1) {
        switch (nItemType) {
        case 41:
            if (pItem->GetResRef() != CResRef("00BARD04")
                && pItem->GetResRef() != CResRef("00HFBD04")) {
                v1 = -1;
                if ((pItem->m_flags & 0x8) != 0) {
                    v1++;
                }
            }
            break;
        case 47:
        case 49:
            v1 = -2;
            if ((pItem->m_flags & 0x8) != 0) {
                v1++;
            }
            break;
        case 53:
            v1 = -1;
            if ((pItem->m_flags & 0x8) != 0) {
                v1++;
            }
            break;
        }
    }

    return v1;
}

// 0x724270
INT CGameSprite::sub_724270()
{
    INT nSlot = 2 * (m_nWeaponSet + 22);
    CItem* pItem = m_equipment.m_items[nSlot];
    if (pItem == NULL) {
        return 0;
    }

    switch (pItem->GetItemType()) {
    case 41:
        if (pItem->GetResRef() != CResRef("00BARD04")
            && pItem->GetResRef() != CResRef("00HFBD04")) {
            return 5;
        }
        return 0;
    case 47:
        return 50;
    case 49:
        return 15;
    case 53:
        return 5;
    default:
        return 0;
    }
}

// 0x724360
INT CGameSprite::sub_724360()
{
    CItem* pItem = m_equipment.m_items[1];
    if (pItem == NULL) {
        return 0;
    }

    switch (pItem->GetItemType()) {
    case 60:
        return 10;
    case 61:
        return 15;
    case 62:
        return 30;
    case 63:
    case 64:
        return 40;
    case 65:
        return 35;
    case 66:
        return 20;
    case 68:
        return 25;
    default:
        return 0;
    }
}

// 0x7243F0
BOOL CGameSprite::CheckAranceFailure(INT nRoll)
{
    INT nFailureChance = sub_724430();

    if (nRoll >= nFailureChance) {
        return FALSE;
    }

    // "Spell Failure check: Roll d100 %d vs. Spell failure chance %d"
    FeedBack(83, nRoll, nFailureChance, 0, 40955, 0, 0);

    return TRUE;
}

// 0x724430
int CGameSprite::sub_724430()
{
    INT nMod = 0;

    if (m_derivedStats.m_spellStates[SPLSTATE_MENTAL_DOMINATION]) {
        nMod = 100;
    }

    if (m_derivedStats.m_spellStates[SPLSTATE_BLINK]) {
        nMod += 20;
    }

    if (m_derivedStats.m_spellStates[SPLSTATE_DEAFENED]) {
        nMod += 20;
    }

    INT nFailureChance = m_derivedStats.m_nSpellFailureArcane
        + nMod
        + sub_724360()
        + sub_724270();

    if (sub_763150(CGAMESPRITE_FEAT_ARMORED_ARCANA)) {
        nFailureChance -= 5 * GetFeatValue(CGAMESPRITE_FEAT_ARMORED_ARCANA);
    }

    if (nFailureChance > 100) {
        nFailureChance = 100;
    } else if (nFailureChance < 0) {
        nFailureChance = 0;
    }

    return nFailureChance;
}

// 0x724540
BOOL CGameSprite::CheckDivineFailure(INT nRoll)
{
    INT nMod = 0;

    if (m_derivedStats.m_spellStates[SPLSTATE_MENTAL_DOMINATION]) {
        nMod = 100;
    }

    if (m_derivedStats.m_spellStates[SPLSTATE_BLINK]) {
        nMod += 20;
    }

    if (m_derivedStats.m_spellStates[SPLSTATE_DEAFENED]) {
        nMod += 20;
    }

    INT nFailureChance = m_derivedStats.m_nSpellFailureDivine + nMod;
    if (nFailureChance > 100) {
        nFailureChance = 100;
    } else if (nFailureChance < 0) {
        nFailureChance = 0;
    }

    if (nRoll >= nFailureChance) {
        return FALSE;
    }

    // "Spell Failure check: Roll d100 %d vs. Spell failure chance %d"
    FeedBack(FEEDBACK_ROLL, nRoll, nFailureChance, 0, 40955, 0, 0);

    return TRUE;
}

// 0x7245D0
BOOLEAN CGameSprite::sub_7245D0()
{
    return m_derivedStats.m_spellStates[SPLSTATE_HELD]
        || m_derivedStats.m_spellStates[SPLSTATE_HOPELESSNESS]
        || m_derivedStats.m_spellStates[SPLSTATE_OTILUKES_RESILIENT_SPHERE];
}

// 0x7245F0
INT CGameSprite::GetNextHatedRacesSlot()
{
    INT nIndex;

    for (nIndex = 0; nIndex < MAX_HATED_RACES; nIndex++) {
        if (m_baseStats.m_favoredEnemies[nIndex] == CAIObjectType::R_NO_RACE) {
            break;
        }
    }

    return nIndex;
}

// 0x724610
void CGameSprite::ResetQuickSlots()
{
    const CRuleTables& ruleTables = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    INT nClass = m_derivedStats.GetBestClass() - 1;

    for (int nSlot = 0; nSlot < CGAMESAVECHARACTER_NUM_CUSTOM_BUTTONS22; nSlot++) {
        if (field_3D14[nSlot] == 0) {
            field_3D14[nSlot] = atol(ruleTables.m_tQuickSlots.GetAt(CPoint(nSlot, nClass)));
        }
    }
}

// 0x724690
BOOL CGameSprite::sub_724690(SHORT a1)
{
    CItem* pItem = m_equipment.m_items[1];
    if (pItem == NULL) {
        return FALSE;
    }

    WORD nItemType = pItem->GetItemType();
    switch (a1) {
    case 0:
        switch (nItemType) {
        case 67:
            return TRUE;
        }
        break;
    case 1:
        switch (nItemType) {
        case 60:
        case 61:
            return TRUE;
        }
        break;
    case 2:
        switch (nItemType) {
        case 62:
        case 66:
        case 68:
            return TRUE;
        }
        break;
    case 3:
        switch (nItemType) {
        case 63:
        case 64:
        case 65:
            return TRUE;
        }
        break;
    }

    return FALSE;
}

// FIXME: `nClass` should not be reference.
//
// 0x724730
CGameSpriteGroupedSpellList* CGameSprite::GetSpells(const BYTE& nClass)
{
    UINT nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 25826
    UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

    return &(m_spells.m_spellsByClass[nClassIndex]);
}

// FIXME: `nClass` should not be reference.
// FIXME: `nLevel` should not be reference.
//
// 0x724790
CGameSpriteSpellList* CGameSprite::GetSpellsAtLevel(const BYTE& nClass, const UINT& nLevel)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 25835
    UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

    UINT nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 25838
    UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 2521
    UTIL_ASSERT(nLevel < CSPELLLIST_MAX_LEVELS);

    return &(m_spells.m_spellsByClass[nClassIndex].m_lists[nLevel]);
}

// 0x724840
UINT CGameSprite::GetNumSpells()
{
    UINT nCount = 0;

    for (int index = 0; index < CSPELLLIST_NUM_CLASSES; index++) {
        // NOTE: Uninline.
        nCount += m_spells.m_spellsByClass[index].GetNumSpells();
    }

    // NOTE: Uninline.
    nCount += m_domainSpells.GetNumSpells();

    return nCount;
}

// 0x724900
BOOLEAN CGameSprite::sub_724900()
{
    DWORD dwClassMask = GetAIType().m_nClassMask;

    return (dwClassMask & (CLASSMASK_BARD | CLASSMASK_CLERIC | CLASSMASK_DRUID | CLASSMASK_PALADIN | CLASSMASK_RANGER)) != 0
        || (dwClassMask & (CLASSMASK_SORCERER | CLASSMASK_WIZARD)) != 0;
}

// 0x724920
BOOLEAN CGameSprite::sub_724920()
{
    return (GetAIType().m_nClassMask & CLASSMASK_BARD) != 0;
}

// FIXME: Review references (latter params unclear).
//
// 0x724930
BOOLEAN CGameSprite::AddKnownSpell(const BYTE& nClass, const UINT& nSpellLevel, const CResRef& resRef, const unsigned int& a4, const unsigned int& a5, const unsigned int& a6)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 25936
    UTIL_ASSERT(nSpellLevel < CSPELLLIST_MAX_LEVELS);

    UINT nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);

    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    // NOTE: Uninline.
    CGameSpriteGroupedSpellList* pSpellsByLevel = m_spells.Get(nClassIndex);

    // NOTE: Uninline.
    return pSpellsByLevel->Add(nID, nSpellLevel, a4, a5, a6);
}

// 0x724A40
BOOLEAN CGameSprite::AddDomainSpell(const UINT& nSpellLevel, const CResRef& resRef, const unsigned int& a3, const unsigned int& a4, const unsigned int& a5)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 25954
    UTIL_ASSERT(nSpellLevel < CSPELLLIST_MAX_LEVELS);

    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    // NOTE: Uninline.
    m_domainSpells.Add(nID, nSpellLevel, a3, a4, a5);

    return TRUE;
}

// 0x724B10
BOOLEAN CGameSprite::AddInnateSpell(const CResRef& resRef, const unsigned int& a2, const unsigned int& a3, const unsigned int& a4)
{
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->GetInnateSpells()->Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    m_innateSpells.Add(nID, a2, a3, a4);

    return TRUE;
}

// 0x724B70
BOOLEAN CGameSprite::AddSong(const CResRef& resRef, const unsigned int& a2, const unsigned int& a3, const unsigned int& a4)
{
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->GetMasterSongLookup().Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    m_songs.Add(nID, a2, a3, a4);

    return TRUE;
}

// 0x724BD0
BOOLEAN CGameSprite::AddShapeshift(const CResRef& resRef, const unsigned int& a2)
{
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->GetShapeshifts()->Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    m_shapeshifts.Add(nID, 0, 0, a2);

    return TRUE;
}

// 0x724C40
BOOLEAN CGameSprite::sub_724C40(const unsigned int& a1)
{
    m_shapeshifts.field_14 += a1;

    for (UINT nIndex = 0; nIndex < m_shapeshifts.m_List.size(); nIndex++) {
        CGameSpriteSpellListEntry* pEntry = m_shapeshifts.Get(nIndex);

        m_shapeshifts.Add(pEntry->m_nID,
            m_shapeshifts.field_14 - pEntry->field_4,
            0,
            0);
    }

    return FALSE;
}

// 0x724D30
BOOLEAN CGameSprite::RemoveKnownSpell(const BYTE& nClass, const UINT& nSpellLevel, const CResRef& resRef, const unsigned int& a4, const unsigned int& a5, const unsigned int& a6)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 25954
    UTIL_ASSERT(nSpellLevel < CSPELLLIST_MAX_LEVELS);

    UINT nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);

    UINT nID;
    if (g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    // NOTE: Uninline.
    CGameSpriteGroupedSpellList* pSpellsByLevel = m_spells.Get(nClassIndex);

    return pSpellsByLevel->Remove(nID, nSpellLevel, a4, a5, a6);
}

// 0x724E00
BOOLEAN CGameSprite::RemoveDomainSpell(const UINT& nSpellLevel, const CResRef& resRef, const unsigned int& a3, const unsigned int& a4, const unsigned int& a5)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 26073
    UTIL_ASSERT(nSpellLevel < CSPELLLIST_MAX_LEVELS);

    if (!m_derivedStats.HasClass(CAIOBJECTTYPE_C_CLERIC)) {
        return FALSE;
    }

    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    return m_domainSpells.Remove(nID, nSpellLevel, a3, a4, a5);
}

// 0x724EA0
BOOLEAN CGameSprite::RemoveInnateSpell(const CResRef& resRef, const unsigned int& a2, const unsigned int& a3, const unsigned int& a4)
{
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->GetInnateSpells()->Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    return m_innateSpells.Remove(nID, a2, a3, a4);
}

// 0x724F00
BOOLEAN CGameSprite::RemoveSong(const CResRef& resRef, const unsigned int& a2, const unsigned int& a3, const unsigned int& a4)
{
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->GetMasterSongLookup().Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    return m_songs.Remove(nID, a2, a3, a4);
}

// 0x724F60
BOOLEAN CGameSprite::RemoveShapeshift(const CResRef& resRef)
{
    UINT nID = 0;
    if (g_pBaldurChitin->GetObjectGame()->GetShapeshifts()->Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    return m_shapeshifts.Remove(nID, 1, 0, 0);
}

// 0x724FD0
BOOLEAN CGameSprite::sub_724FD0(const BYTE& nClass, const UINT& nSpellLevel, const CResRef& resRef, const unsigned int& a4, const unsigned int& a5)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 26177
    UTIL_ASSERT(nSpellLevel < CSPELLLIST_MAX_LEVELS);

    UINT nClassIndex = g_pBaldurChitin->GetObjectGame()->GetSpellcasterIndex(nClass);

    UINT nID;
    if (g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    // NOTE: Uninline.
    CGameSpriteGroupedSpellList* pSpellsByLevel = m_spells.Get(nClassIndex);

    // NOTE: Uninline.
    return pSpellsByLevel->sub_7260B0(nID, nSpellLevel, a4, a5);
}

// 0x725110
BOOLEAN CGameSprite::sub_725110(const UINT& nSpellLevel, const CResRef& resRef, const unsigned int& a4, const unsigned int& a5)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 26211
    UTIL_ASSERT(nSpellLevel < CSPELLLIST_MAX_LEVELS);

    UINT nID;
    if (g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    // NOTE: Uninline.
    CGameSpriteSpellList* pList = m_domainSpells.GetSpellsAtLevel(nSpellLevel);

    // NOTE: Uninline.
    return pList->sub_725CC0(nID, a4, a5);
}

// 0x725210
BOOLEAN CGameSprite::sub_725210(const CResRef& resRef, const unsigned int& a2, const unsigned int& a3)
{
    UINT nID;
    if (g_pBaldurChitin->GetObjectGame()->GetInnateSpells()->Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    return m_innateSpells.sub_725C50(nID, a2, a3);
}

// 0x725270
BOOLEAN CGameSprite::sub_725270(const CResRef& resRef, const unsigned int& a2, const unsigned int& a3)
{
    UINT nID;
    if (g_pBaldurChitin->GetObjectGame()->GetInnateSpells()->Find(resRef, nID) != TRUE) {
        return FALSE;
    }

    // NOTE: Uninline.
    return m_innateSpells.sub_725CC0(nID, a2, a3);
}

// FIXME: `dwClassMask` should not be reference.
//
// 0x725330
BOOLEAN CGameSprite::sub_725330(const CResRef& resRef, const DWORD& dwClassMask, UINT nLevel, BOOLEAN a4)
{
    BOOLEAN v1 = FALSE;
    UINT nStartLevel = 0;
    UINT nEndLevel = 9;

    if (nLevel < CSPELLLIST_MAX_LEVELS) {
        nStartLevel = nLevel;
        nStartLevel = nLevel + 1;
    }

    UINT nID;
    if (!g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID)) {
        return FALSE;
    }

    for (UINT nClassIndex = 0; nClassIndex < CSPELLLIST_NUM_CLASSES; nClassIndex++) {
        if (v1) {
            break;
        }

        if ((g_pBaldurChitin->GetObjectGame()->GetSpellcasterClassMask(nClassIndex) & dwClassMask) != 0) {
            for (UINT nCurrLevel = nStartLevel; nCurrLevel < nEndLevel; nCurrLevel++) {
                if (v1) {
                    break;
                }

                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 26902
                UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

                // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
                // __LINE__: 26903
                UTIL_ASSERT(nCurrLevel < CSPELLLIST_MAX_LEVELS);

                UINT nIndex = 0;
                v1 = m_spells.m_spellsByClass[nClassIndex].m_lists[nCurrLevel].Find(nID, nIndex);
                if (v1 == TRUE && a4 == TRUE) {
                    // __FILE__: .\Include\FileFormat.h
                    // __LINE__: 2565
                    UTIL_ASSERT(nClassIndex < CSPELLLIST_NUM_CLASSES);

                    // NOTE: Uninline.
                    v1 = m_spells.m_spellsByClass[nClassIndex].m_lists[nCurrLevel].CheckF8(nIndex);
                }
            }
        }
    }

    if (!v1) {
        if ((dwClassMask & CLASSMASK_CLERIC) == CLASSMASK_CLERIC) {
            for (UINT nCurrLevel = nStartLevel; nCurrLevel < nEndLevel; nCurrLevel++) {
                if (v1) {
                    break;
                }

                UINT nIndex = 0;

                // NOTE: Uninline.
                v1 = m_domainSpells.Find(nID, nStartLevel, nIndex);
                if (v1 == TRUE && a4 == TRUE) {
                    // NOTE: Uninline.
                    v1 = m_innateSpells.CheckF8(nIndex);
                }
            }
        }
    }

    return v1;
}

// 0x7256B0
BOOLEAN CGameSprite::sub_7256B0(const CResRef& resRef, const UINT& nLevel, BOOLEAN a3)
{
    BOOLEAN v1 = FALSE;
    UINT nStartLevel = 0;
    UINT nEndLevel = 9;

    if (nLevel < CSPELLLIST_MAX_LEVELS) {
        nStartLevel = nLevel;
        // TODO: Looks odd, should it be `nLevel + 1` as in the function above
        // (0x725330) to check only one specific level?
        nEndLevel = nLevel - 1;
    }

    UINT nID;
    if (!g_pBaldurChitin->GetObjectGame()->m_spells.Find(resRef, nID)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 26395
        UTIL_ASSERT(FALSE);
    }

    for (UINT nCurrLevel = nStartLevel; nCurrLevel < nEndLevel; nCurrLevel++) {
        if (v1) {
            break;
        }

        UINT nIndex;

        // NOTE: Uninline.
        v1 = m_domainSpells.Find(nID, nStartLevel, nIndex);
        if (v1 == TRUE && a3 == TRUE) {
            // NOTE: Uninline.
            v1 = m_innateSpells.CheckF8(nIndex);
        }
    }

    return v1;
}

// 0x725840
BOOLEAN CGameSprite::sub_725840(const CResRef& resRef, BOOLEAN a2)
{
    UINT nID;
    if (!g_pBaldurChitin->GetObjectGame()->GetInnateSpells()->Find(resRef, nID)) {
        // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
        // __LINE__: 26420
        UTIL_ASSERT(FALSE);
    }

    UINT nIndex;
    BOOLEAN v1 = m_innateSpells.Find(nID, nIndex);

    if (v1 && a2 == TRUE) {
        v1 = m_innateSpells.CheckF8(nIndex);
    }

    return v1;
}

// 0x726270
INT CGameSprite::sub_726270(UINT nFeatNumber)
{
    INT v1 = 0;
    if (sub_763150(nFeatNumber)) {
        switch (nFeatNumber) {
        case CGAMESPRITE_FEAT_ARTERIAL_STRIKE:
            v1 = field_4C54[2];
            break;
        case CGAMESPRITE_FEAT_EXPERTISE:
            v1 = field_4C54[0];
            break;
        case CGAMESPRITE_FEAT_HAMSTRING:
            v1 = field_4C54[3];
            break;
        case CGAMESPRITE_FEAT_POWER_ATTACK:
            v1 = field_4C54[1];
            break;
        case CGAMESPRITE_FEAT_RAPID_SHOT:
            v1 = field_4C54[4];
            break;
        }
    }
    return v1;
}

// 0x726330
void CGameSprite::sub_726330(UINT nFeatNumber, INT nValue)
{
    if (sub_763150(nFeatNumber)) {
        switch (nFeatNumber) {
        case CGAMESPRITE_FEAT_ARTERIAL_STRIKE:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26976
            UTIL_ASSERT_MSG(nValue >= 0, "Invalid feat level.");

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26977
            UTIL_ASSERT_MSG(nValue <= 1, "Invalid feat level.");

            field_4C54[2] = nValue;
            field_4C54[3] = 0;
            break;
        case CGAMESPRITE_FEAT_EXPERTISE:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26962
            UTIL_ASSERT_MSG(nValue >= 0, "Invalid feat level.");

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26963
            UTIL_ASSERT_MSG(nValue <= MAX_SELECTABLE_FEAT_USE_LEVELS, "Invalid feat level.");

            field_4C54[0] = nValue;
            field_4C54[1] = 0;
            break;
        case CGAMESPRITE_FEAT_HAMSTRING:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26983
            UTIL_ASSERT_MSG(nValue >= 0, "Invalid feat level.");

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26984
            UTIL_ASSERT_MSG(nValue <= 1, "Invalid feat level.");

            field_4C54[3] = nValue;
            field_4C54[2] = 0;
            break;
        case CGAMESPRITE_FEAT_POWER_ATTACK:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26969
            UTIL_ASSERT_MSG(nValue >= 0, "Invalid feat level.");

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26970
            UTIL_ASSERT_MSG(nValue <= MAX_SELECTABLE_FEAT_USE_LEVELS, "Invalid feat level.");

            field_4C54[1] = nValue;
            field_4C54[0] = 0;
            break;
        case CGAMESPRITE_FEAT_RAPID_SHOT:
            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 26990
            UTIL_ASSERT_MSG(nValue >= 0, "Invalid feat level.");

            // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
            // __LINE__: 269991
            UTIL_ASSERT_MSG(nValue <= 1, "Invalid feat level.");

            field_4C54[4] = nValue;
            break;
        }
    }
}

// 0x726570
void CGameSprite::sub_726570()
{
    ITEM_EFFECT effect;
    CGameEffect* pEffect;
    CMessageAddEffect* pMessage;

    if (field_4C54[1] > 0) {
        CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_FEATPOWERATTACK);
        pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
        pMessage = new CMessageAddEffect(pEffect, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (field_4C54[0] > 0) {
        CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_FEATEXPERTISE);
        pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
        pMessage = new CMessageAddEffect(pEffect, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (field_4C54[2] > 0) {
        CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_FEATARTERIALSTRIKE);
        pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
        pMessage = new CMessageAddEffect(pEffect, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (field_4C54[3] > 0) {
        CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_FEATHAMSTRING);
        pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
        pMessage = new CMessageAddEffect(pEffect, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }

    if (field_4C54[4] > 0) {
        CGameEffect::ClearItemEffect(&effect, ICEWIND_CGAMEEFFECT_FEATRAPIDSHOT);
        pEffect = CGameEffect::DecodeEffect(&effect, m_pos, m_id, CPoint(-1, -1));
        pMessage = new CMessageAddEffect(pEffect, m_id, m_id);
        g_pBaldurChitin->GetMessageHandler()->AddMessage(pMessage, FALSE);
    }
}

// 0x726800
INT CGameSprite::sub_726800()
{
    return 2 * m_nWeaponSet + 44;
}

// 0x726810
void CGameSprite::sub_726810(BYTE nWeaponSet)
{
    // __FILE__: C:\Projects\Icewind2\src\Baldur\ObjCreature.cpp
    // __LINE__: 27066
    UTIL_ASSERT(nWeaponSet < CCREATUREFILEEQUIPMENT21_WEAPON_NUM_SETS);

    if (m_equipment.m_items[42] != NULL) {
        SelectWeaponAbility(42, static_cast<BYTE>(m_equipment.m_selectedWeaponAbility), TRUE, TRUE);
        return;
    }

    // Take off the current set: a launcher's ammunition, the weapon and the
    // off-hand item.
    INT nWeaponSlot = 2 * m_nWeaponSet + 43;
    INT nShieldSlot = nWeaponSlot + 1;

    CItem* pSelected = m_equipment.m_items[m_equipment.m_selectedWeapon];
    if (pSelected != NULL) {
        pSelected->Demand();
        SHORT launcherSlot;
        if (GetLauncher(pSelected->GetAbility(m_equipment.m_selectedWeaponAbility), launcherSlot) != NULL) {
            pSelected->Unequip(this, m_equipment.m_selectedWeapon, TRUE, FALSE);
        }
        pSelected->Release();
    }

    if (m_equipment.m_items[nWeaponSlot] != NULL) {
        m_equipment.m_items[nWeaponSlot]->Unequip(this, nWeaponSlot, TRUE, FALSE);
    } else {
        WORD attackProbability[6] = { 34, 33, 33, 0, 0, 0 };
        GetAnimation()->EquipWeapon(CString(""), byte_8D86B0, 0, attackProbability);
    }

    if (m_equipment.m_items[nShieldSlot] != NULL) {
        m_equipment.m_items[nShieldSlot]->Unequip(this, nShieldSlot, TRUE, FALSE);
    }

    GetAnimation()->EquipShield(CString(""), byte_8D86B0);

    // Put on the new set.
    m_nWeaponSet = nWeaponSet;
    nWeaponSlot = 2 * nWeaponSet + 43;
    nShieldSlot = nWeaponSlot + 1;
    BYTE buttonNum = static_cast<BYTE>(nWeaponSlot - 43);

    CButtonData weaponButton;
    GetQuickWeapon(buttonNum, weaponButton);

    CButtonData shieldButton;
    GetQuickWeapon(static_cast<BYTE>(nShieldSlot - 43), shieldButton);

    CItem* pWeapon = m_equipment.m_items[nWeaponSlot];
    INT nSelectSlot = 0;
    INT nAbility = 0;
    // NOTE: The launcher slot the button remembers; the original also uses
    // it as GetLauncher's out parameter.
    INT nLauncherSlot = 0;
    BOOL bReequipShield = FALSE;

    if (pWeapon != NULL) {
        SHORT nType = 0;
        pWeapon->Demand();
        ITEM_ABILITY* pAbility = pWeapon->GetAbility(0);
        if (pAbility != NULL) {
            nType = *reinterpret_cast<BYTE*>(pAbility);
        }
        pWeapon->Release();

        if (nType == 4) {
            // __FILE__: .\Include\ObjCreature.h
            // __LINE__: 2021
            UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_WEAPONS22);

            BYTE nAmmoSlot = field_3D3A[buttonNum];
            BOOL bFindAmmo;
            if (nAmmoSlot >= 11 && nAmmoSlot <= 14 && m_equipment.m_items[nAmmoSlot] != NULL) {
                CItem* pAmmo = m_equipment.m_items[nAmmoSlot];
                SHORT launcherSlot = 0;
                pAmmo->Demand();
                CItem* pLauncher = GetLauncher(pAmmo->GetAbility(0), launcherSlot);
                nLauncherSlot = launcherSlot;
                pAmmo->Release();

                if (pLauncher == pWeapon) {
                    nSelectSlot = nAmmoSlot;
                    nAbility = weaponButton.m_abilityId.m_abilityNum;
                    nLauncherSlot = nAmmoSlot;
                    bFindAmmo = nAmmoSlot == 0;
                } else {
                    bFindAmmo = TRUE;
                }
            } else {
                bFindAmmo = nAmmoSlot == 0;
            }

            if (bFindAmmo) {
                if (sub_75E330(pWeapon, !IcewindMisc::IsPC(this)) == CGameAIBase::ACTION_ERROR) {
                    nSelectSlot = 10;
                    nAbility = 0;
                    nLauncherSlot = 0;
                } else {
                    nSelectSlot = m_equipment.m_selectedWeapon;
                    nLauncherSlot = m_equipment.m_selectedWeapon;
                    nAbility = m_equipment.m_selectedWeaponAbility;
                }
            }
        } else {
            nSelectSlot = nWeaponSlot;
            nAbility = weaponButton.m_abilityId.m_abilityNum;
            nLauncherSlot = 0;
        }

        if (m_equipment.m_items[nShieldSlot] != NULL) {
            bReequipShield = TRUE;
        }
    } else {
        if (m_equipment.m_items[nShieldSlot] == NULL) {
            nSelectSlot = 10;
            nAbility = 0;
        } else {
            // FIXME: Calls `GetItemType` four times.
            if (m_equipment.m_items[nShieldSlot]->GetItemType() == 47
                || m_equipment.m_items[nShieldSlot]->GetItemType() == 53
                || m_equipment.m_items[nShieldSlot]->GetItemType() == 49
                || m_equipment.m_items[nShieldSlot]->GetItemType() == 41) {
                nSelectSlot = 10;
                nAbility = 0;
                m_equipment.m_items[nShieldSlot]->Equip(this, nShieldSlot, FALSE);
            } else {
                nSelectSlot = nShieldSlot;
                nAbility = shieldButton.m_abilityId.m_abilityNum;
            }
        }
        nLauncherSlot = 0;
    }

    SelectWeaponAbility(static_cast<BYTE>(nSelectSlot), static_cast<BYTE>(nAbility), FALSE, TRUE);

    // NOTE: Uninline.
    SetQuickWeapon(buttonNum, static_cast<BYTE>(nLauncherSlot));

    if (bReequipShield == TRUE && m_equipment.m_items[nShieldSlot] != NULL) {
        m_equipment.m_items[nShieldSlot]->Equip(this, nShieldSlot, FALSE);
    }

    sub_726F20();

    // NOTE: The original calls the virtual GetAIType here and drops the result.
    GetAIType();

    int v1 = 0;
    int v2 = 0;
    int v3 = 0;
    g_pBaldurChitin->GetObjectGame()->GetRuleTables().sub_546B60(this, v1, v2, v3, FALSE);
    m_baseStats.m_numberOfAttacksBase = static_cast<BYTE>(v2);
    if (static_cast<BYTE>(v2) > 5) {
        m_baseStats.m_numberOfAttacksBase = 5;
    }

    CMessage* message = new CMessageSpriteEquipment(this, m_id, m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
}

// 0x726F20
void CGameSprite::sub_726F20()
{
    // Rebuild the quick weapon buttons of all four weapon sets from the
    // equipped items.
    INT nSet = 0;
    INT nShieldSlot = 44;
    do {
        INT nWeaponSlot = nSet * 2 + 43;
        BYTE buttonNum = static_cast<BYTE>(nWeaponSlot - 43);

        CButtonData cButtonData;
        GetQuickWeapon(buttonNum, cButtonData);

        CItem* pWeapon = m_equipment.m_items[static_cast<SHORT>(nWeaponSlot)];
        INT nAbility = 0;
        INT nLauncherSlot = 0;
        if (pWeapon != NULL) {
            SHORT nType = 0;
            pWeapon->Demand();
            ITEM_ABILITY* pAbility = pWeapon->GetAbility(0);
            if (pAbility != NULL) {
                nType = *reinterpret_cast<BYTE*>(pAbility);
            }
            pWeapon->Release();

            if (nType == 4) {
                // __FILE__: .\Include\ObjCreature.h
                // __LINE__: 2021
                UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_WEAPONS22);

                BYTE nAmmoSlot = field_3D3A[buttonNum];
                if (nAmmoSlot > 10 && nAmmoSlot < 15 && m_equipment.m_items[nAmmoSlot] != NULL) {
                    CItem* pAmmo = m_equipment.m_items[nAmmoSlot];
                    SHORT launcherSlot = 0;
                    pAmmo->Demand();
                    CItem* pLauncher = GetLauncher(pAmmo->GetAbility(0), launcherSlot);
                    pAmmo->Release();

                    // NOTE: Both branches of the original's launcher test
                    // do the same.
                    if (pLauncher == pWeapon) {
                        nAbility = cButtonData.m_abilityId.m_abilityNum;
                        nLauncherSlot = nAmmoSlot;
                    } else {
                        nAbility = cButtonData.m_abilityId.m_abilityNum;
                        nLauncherSlot = nAmmoSlot;
                    }
                }
            } else {
                nAbility = cButtonData.m_abilityId.m_abilityNum;
                nLauncherSlot = 0;
            }
        }

        CGameButtonList* buttons = GetItemUsages(static_cast<SHORT>(nSet * 2 + 43), 1, static_cast<SHORT>(nAbility));
        if (buttons->IsEmpty()) {
            // NOTE: Uninline.
            SetQuickWeapon(buttonNum, CButtonData());
            SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
        } else {
            POSITION pos = buttons->GetHeadPosition();
            while (pos != NULL) {
                CButtonData* pButtonData = buttons->GetNext(pos);
                if (pButtonData != NULL && pButtonData->m_abilityId.m_itemNum == nLauncherSlot) {
                    // NOTE: Uninline.
                    SetQuickWeapon(buttonNum, *pButtonData);
                    SetQuickWeapon(buttonNum, static_cast<BYTE>(nLauncherSlot));
                }
                delete pButtonData;
            }
        }
        buttons->RemoveAll();
        delete buttons;

        buttonNum = static_cast<BYTE>(nShieldSlot - 43);
        buttons = GetItemUsages(static_cast<SHORT>(nShieldSlot), 1, 0);
        if (buttons->IsEmpty()) {
            // NOTE: Uninline.
            SetQuickWeapon(buttonNum, CButtonData());
            SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
        } else {
            POSITION pos = buttons->GetHeadPosition();
            CButtonData* pButtonData = buttons->GetNext(pos);

            // NOTE: Uninline.
            SetQuickWeapon(buttonNum, *pButtonData);

            SHORT nItemNum = pButtonData->m_abilityId.m_itemNum;
            if (nItemNum < 11 || nItemNum > 14) {
                SetQuickWeapon(buttonNum, static_cast<BYTE>(0));
            } else {
                SetQuickWeapon(buttonNum, static_cast<BYTE>(nItemNum));
            }
            delete pButtonData;

            while (pos != NULL) {
                delete buttons->GetNext(pos);
            }
        }
        buttons->RemoveAll();
        delete buttons;

        nShieldSlot += 2;
        nSet++;
    } while (nShieldSlot < 52);

    CMessage* message = new CMessageSpriteEquipment(this, m_id, m_id);
    g_pBaldurChitin->GetMessageHandler()->AddMessage(message, FALSE);
}

// 0x58FEE0
BYTE CGameSprite::GetLastSong()
{
    return m_nLastSong;
}

// NOTE: Inlined.
void CGameSprite::GetQuickItem(BYTE buttonNum, CButtonData& buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2022
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ITEMS22);

    CheckToolTipItem(buttonNum);

    buttonData = m_quickItems[buttonNum];
}

// NOTE: Inlined.
void CGameSprite::GetQuickSpell(BYTE buttonNum, CButtonData& buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2023
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_SPELLS22);

    buttonData = m_quickSpells[buttonNum];
}

// NOTE: Inlined.
void CGameSprite::GetQuickAbility(BYTE buttonNum, CButtonData& buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2024
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ABILITIES22);

    buttonData = m_quickInnates[buttonNum];
}

// NOTE: Inlined.
void CGameSprite::GetQuickSong(BYTE buttonNum, CButtonData& buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2025
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_SONGS22);

    buttonData = m_quickSongs[buttonNum];
}

// FIXME: `buttonData` should be reference.
//
// NOTE: Inlined.
void CGameSprite::SetQuickWeapon(BYTE buttonNum, CButtonData buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2030
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_WEAPONS22);

    m_quickWeapons[buttonNum] = buttonData;
}

// NOTE: Inlined.
void CGameSprite::SetQuickWeapon(BYTE buttonNum, BYTE index)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2031
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_WEAPONS22);

    field_3D3A[buttonNum] = index;
}

// FIXME: `buttonData` should be reference.
//
// NOTE: Inlined.
void CGameSprite::SetQuickItem(BYTE buttonNum, CButtonData buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2032
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ITEMS22);

    m_quickItems[buttonNum] = buttonData;
}

// FIXME: `buttonData` should be reference.
//
// NOTE: Inlined.
void CGameSprite::SetQuickSpell(BYTE buttonNum, CButtonData buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2033
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_SPELLS22);

    m_quickSpells[buttonNum] = buttonData;
}

// FIXME: `buttonData` should be reference.
//
// NOTE: Inlined.
void CGameSprite::SetQuickAbility(BYTE buttonNum, CButtonData buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2034
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_ABILITIES22);

    m_quickInnates[buttonNum] = buttonData;
}

// FIXME: `buttonData` should be reference.
//
// NOTE: Inlined.
void CGameSprite::SetQuickSong(BYTE buttonNum, CButtonData buttonData)
{
    // __FILE__: .\Include\ObjCreature.h
    // __LINE__: 2035
    UTIL_ASSERT(buttonNum < CGAMESAVECHARACTER_NUM_QUICK_SONGS22);

    m_quickSongs[buttonNum] = buttonData;
}

// NOTE: Inlined.
CDerivedStats* CGameSprite::GetActiveStats()
{
    if (m_bAllowEffectListCall) {
        return &m_derivedStats;
    } else {
        return &m_tempStats;
    }
}
