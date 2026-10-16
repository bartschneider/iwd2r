#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CButtonData.h"
#include "CDerivedStats.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfButtonArray.h"
#include "CInfGame.h"
#include "CItem.h"
#include "CRuleTables.h"
#include "CScreenCharacter.h"
#include "CScreenCreateChar.h"
#include "CScreenInventory.h"
#include "CScreenWorld.h"
#include "CUIControlTextDisplay.h"
#include "CUIManager.h"
#include "CUIPanel.h"
#include "CUtil.h"
#include "CWarp.h"
#include "ScreenLayout.h"
#include "compat/msvc_rand.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The ui-misc stream's second round: the character screen's combat page
// (CScreenCharacter::UpdateEquipmentInformation and the sprite's dexterity
// bonus), the quick lists (CGameSprite::CheckQuickLists), the world
// screen's keys, the toolbar and CUtil::FindOneOf, on the engine in this
// process and a private view of the install without saves
// (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

// The entry of `dir` whose name matches `name` without regard to case.
fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

// The engine as iwd2r_main builds it, on a private view of the install (as
// tests/test_gam.cpp does), every engine's panels loaded. Built once and
// leaked: the engine's globals live until the process ends.
fs::path g_view;

bool StartEngine(std::string& error)
{
    static bool started = false;
    static std::string startError;
    static bool tried = false;
    if (tried) {
        error = startError;
        return started;
    }
    tried = true;

#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
    _putenv_s("SDL_AUDIO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
    setenv("SDL_AUDIO_DRIVER", "dummy", 1);
#endif
    static std::unique_ptr<DataView> view;
    const std::string original = DataDir();
    view = std::make_unique<DataView>(original, DataView::Saves::None);
    g_view = view->Path();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", view->Path().c_str());
#else
    setenv("IWD2_DATA", view->Path().c_str(), 1);
#endif
    const bool found = platform::ResolveDataDir();
#ifdef _WIN32
    _putenv_s("IWD2_DATA", original.c_str());
#else
    setenv("IWD2_DATA", original.c_str(), 1);
#endif
    if (!found) {
        startError = "data folder not found";
        return false;
    }
    std::error_code ec;
    if (!fs::exists(Child(g_view, "mpsave"), ec)) {
        fs::create_directories(g_view / "mpsave", ec);
    }
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        startError = screen.error;
        return false;
    }
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale)) {
        startError = "SDL video could not start";
        return false;
    }
    CBaldurChitin* chitin = new CBaldurChitin();
    if (chitin->field_1932 != 0) {
        startError = "CBaldurChitin refused to start";
        return false;
    }
    chitin->Init(NULL);
    POSITION pos = chitin->lEngines.GetHeadPosition();
    while (pos != NULL) {
        CWarp* pEngine = static_cast<CWarp*>(chitin->lEngines.GetNext(pos));
        if (pEngine != NULL) {
            pEngine->EngineInitialized();
        }
    }
    chitin->m_pEngineCreateChar->SetVideoMode(0);
    chitin->pActiveEngine = chitin->m_pEngineCreateChar;
    started = true;
    return true;
}

// Stands in for the main loop: the engine's waits for a drawn frame return
// as soon as they begin.
class FramePump {
public:
    FramePump()
        : m_running(true)
        , m_thread([this] {
            while (m_running) {
                g_pChitin->m_bDisplayStale = FALSE;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        })
    {
    }
    ~FramePump()
    {
        m_running = false;
        m_thread.join();
    }
    FramePump(const FramePump&) = delete;
    FramePump& operator=(const FramePump&) = delete;

private:
    std::atomic<bool> m_running;
    std::thread m_thread;
};

// The CRE of the first finished character in the view's Characters folder.
std::vector<BYTE> FinishedCharacter()
{
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(Child(g_view, "Characters"), ec)) {
        if (Lower(entry.path().extension().string()) == ".chr")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        const std::string chr = ReadFile(file);
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

// A new game in AR1000 with one party member, as Party Formation's Done
// leaves it before it saves.
void NewGameWithOneCharacter(std::vector<BYTE>& bytes, const char* name)
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);

    CGameSprite* pSprite = new CGameSprite(bytes.data(),
        static_cast<LONG>(bytes.size()),
        0,
        0,
        -1,
        0,
        0,
        INT_MAX,
        CPoint(-1, -1),
        0);
    pSprite->m_sName = name;
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }
}

// The party member in slot 0.
CGameSprite* PartySprite()
{
    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    CGameSprite* pSprite = NULL;
    LONG nId = pGame->GetCharacterId(0);
    if (pGame->GetObjectArray()->GetShare(nId, CGameObjectArray::THREAD_ASYNCH, reinterpret_cast<CGameObject**>(&pSprite), INFINITE) != CGameObjectArray::SUCCESS)
        return NULL;
    pGame->GetObjectArray()->ReleaseShare(nId, CGameObjectArray::THREAD_ASYNCH, INFINITE);
    return pSprite;
}

// The party member of a new one-character game, or NULL. The game is made
// once per process (CInfGame::NewGame wants no characters).
CGameSprite* PartyMember()
{
    static bool made = false;
    if (!made) {
        std::vector<BYTE> cre = FinishedCharacter();
        if (cre.empty())
            return NULL;
        NewGameWithOneCharacter(cre, "Probe");
        made = true;
    }
    return PartySprite();
}

// Every item of a text display, as GetItem joins its lines.
std::vector<std::string> DisplayItems(CUIControlTextDisplay* pText)
{
    std::vector<std::string> items;
    CString sLabel;
    CString sString;
    for (INT n = 0; pText->GetItem(n, sLabel, sString); n++) {
        items.push_back(std::string(static_cast<LPCSTR>(sString)));
    }
    return items;
}

bool Has(const std::vector<std::string>& items, const std::string& item)
{
    return std::find(items.begin(), items.end(), item) != items.end();
}

// The first item that starts with `prefix`, or "".
std::string Starting(const std::vector<std::string>& items, const std::string& prefix)
{
    for (const std::string& item : items) {
        if (item.compare(0, prefix.size(), prefix) == 0)
            return item;
    }
    return std::string();
}

std::string Format(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

std::string Str(STRREF strRef)
{
    return std::string(static_cast<LPCSTR>(CBaldurEngine::FetchString(strRef)));
}

// The character screen's information text (panel 2, control 45) with the
// combat page (tab 61) shown for the party member in slot 0.
std::vector<std::string> CombatPage()
{
    CScreenCharacter* pCharacter = g_pBaldurChitin->m_pEngineCharacter;
    pCharacter->m_nSelectedCharacter = 0;
    pCharacter->CheckMultiPlayerViewableModifyable();
    pCharacter->field_1844 = 1;
    pCharacter->UpdateMainPanel(TRUE);
    CUIControlTextDisplay* pText = static_cast<CUIControlTextDisplay*>(pCharacter->GetManager()->GetPanel(2)->GetControl(45));
    return DisplayItems(pText);
}

}

// 0x723EA0: GetAC's dexterity part on its own, capped by the armour's type
// and zero while blind without Blind Fight (a penalty stays).
TEST_CASE("sprite: the dexterity bonus to the armor class", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CDerivedStats* pStats = pSprite->GetDerivedStats();
    CItem* pArmor = pSprite->m_equipment.m_items[1];
    pSprite->m_equipment.m_items[1] = NULL;
    const SHORT nDex = pStats->m_nDEX;
    const DWORD nState = pStats->m_generalState;
    auto rest = [&]() {
        return pStats->field_C + pStats->field_A + pStats->field_8 + pStats->field_6 + pSprite->sub_723F60() + 10;
    };

    pStats->m_nDEX = 18;
    CHECK(pSprite->sub_723EA0() == 4);
    CHECK(pSprite->GetAC() == rest() + 4);

    // Chain mail (62) caps it at 2; plate (64) at 0.
    CItem chain(CResRef("00CHAN01"), 0, 0, 0, 0, 0);
    REQUIRE(chain.GetItemType() == 62);
    pSprite->m_equipment.m_items[1] = &chain;
    CHECK(pSprite->sub_723EA0() == 2);
    CHECK(pSprite->GetAC() == rest() + 2);
    CItem plate(CResRef("00PLAT01"), 0, 0, 0, 0, 0);
    REQUIRE(plate.GetItemType() == 64);
    pSprite->m_equipment.m_items[1] = &plate;
    CHECK(pSprite->sub_723EA0() == 0);
    pSprite->m_equipment.m_items[1] = NULL;

    // Blind: the bonus goes unless the creature has Blind Fight; a penalty
    // stays.
    pStats->m_generalState |= STATE_BLIND;
    const DWORD nFeats = pSprite->m_baseStats.m_feats[0];
    pSprite->m_baseStats.m_feats[0] &= ~(1u << CGAMESPRITE_FEAT_BLIND_FIGHT);
    CHECK(pSprite->sub_723EA0() == 0);
    pSprite->m_baseStats.m_feats[0] |= 1u << CGAMESPRITE_FEAT_BLIND_FIGHT;
    CHECK(pSprite->sub_723EA0() == 4);
    pSprite->m_baseStats.m_feats[0] &= ~(1u << CGAMESPRITE_FEAT_BLIND_FIGHT);
    pStats->m_nDEX = 6;
    CHECK(pSprite->sub_723EA0() == -2);
    CHECK(pSprite->GetAC() == rest() - 2);

    pSprite->m_baseStats.m_feats[0] = nFeats;
    pStats->m_generalState = nState;
    pStats->m_nDEX = nDex;
    pSprite->m_equipment.m_items[1] = pArmor;
}

// UpdateEquipmentInformation (0x5DE2C0) and its weapon statistics (0x5DF910):
// a long sword in the first weapon slot gives the attack line, the armour
// class with its dexterity part, the number of attacks and the sword's
// damage and critical range.
TEST_CASE("character screen: the combat page of a character with a long sword", "[uimisc][character][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    CItem* pOld = pSprite->m_equipment.m_items[43];
    CItem* pSword = new CItem(CResRef("00SWDL01"), 0, 0, 0, 0, 0x1);
    pSprite->m_equipment.m_items[43] = pSword;
    pSprite->SelectWeaponAbility(43, 0, FALSE, TRUE);
    REQUIRE(pSprite->m_equipment.m_selectedWeapon == 43);

    std::vector<std::string> items = CombatPage();
    INFO([&] {
        std::string all;
        for (const std::string& item : items)
            all += "[" + item + "]\n";
        return all;
    }());

    CDerivedStats* pStats = pSprite->GetDerivedStats();
    CHECK(Has(items, Str(9457))); // "Attack Roll Modifiers"
    CHECK(Has(items, Format("%s: %d", Str(33553).c_str(), pSprite->GetAC())));
    CHECK(Has(items, Format("  %s: %d", Str(31353).c_str(), 10)));
    CHECK(Has(items, Format("%s: %d", Str(9458).c_str(), pStats->m_nNumberOfAttacks)));
    if (pSprite->sub_723EA0() != 0) {
        CHECK(Has(items, Format("  %s: %+d", Str(1151).c_str(), pSprite->sub_723EA0())));
    } else {
        CHECK(Starting(items, Format("  %s: ", Str(1151).c_str())).empty());
    }

    // The main hand's first attack: the base attack bonus and every
    // modifier the page lists.
    const CRuleTables& rule = g_pBaldurChitin->GetObjectGame()->GetRuleTables();
    int nBase = 0;
    int nAttacks = 0;
    int nStep = 0;
    rule.sub_546B60(pSprite, nBase, nAttacks, nStep, TRUE);
    pSword->Demand();
    const ITEM_ABILITY* pAbility = pSword->GetAbility(0);
    REQUIRE(pAbility != NULL);
    const int nFirst = static_cast<SHORT>(rule.GetIterativeAttackBonus(nBase, nStep, 0)
        + pSprite->sub_724170() + pSprite->sub_7240A0()
        + pSprite->sub_73C7E0() + static_cast<WORD>(pSprite->sub_73D420()) + pSprite->sub_73CAD0()
        + pSprite->sub_73CB10(pSword, pAbility)
        + pSprite->sub_73CA20(pSword, pAbility) + pSprite->sub_73CF10(pSword, pAbility) + pSprite->sub_73D080(FALSE)
        + pSprite->sub_73CAE0(pSword, pAbility) + pSprite->sub_73CC40(pSword, pAbility));
    const std::string sMain = Starting(items, Str(734) + ": "); // "Main Hand: "
    REQUIRE_FALSE(sMain.empty());
    const std::string sFirst = Format("%s: %+d", Str(734).c_str(), nFirst);
    CHECK(sMain.compare(0, sFirst.size(), sFirst) == 0);
    CHECK((sMain.size() == sFirst.size() || sMain[sFirst.size()] == '/'));
    CHECK(Starting(items, Format("  %s: %+d", Str(31353).c_str(), rule.GetIterativeAttackBonus(nBase, nStep, 0))).size() > 0);

    // The weapon statistics: "Main Hand - <name>", the dice, the threat.
    CHECK(Has(items, Str(41119))); // "Weapon Statistics"
    // UpdateText trims the line ("Longsword " has a trailing space).
    std::string sHand = Str(734) + " - " + Str(pSword->GetGenericName());
    sHand.erase(sHand.find_last_not_of(' ') + 1);
    CHECK(Has(items, sHand));
    CHECK(Has(items, Format("  %s: %dd%d", Str(39518).c_str(), pAbility->damageDiceCount, pAbility->damageDice)));
    const int nThreat = static_cast<SHORT>(pSword->sub_4E9840(pAbility) + pSprite->GetCriticalHitBonus());
    if (nThreat != 0) {
        CHECK(Has(items, Format("  %s: %d-20 / x%d", Str(41122).c_str(), std::max(1, 20 - nThreat), static_cast<int>(pSword->GetCriticalHitMultiplier()))));
    } else {
        CHECK(Has(items, Format("  %s: 20 / x%d", Str(41122).c_str(), static_cast<int>(pSword->GetCriticalHitMultiplier()))));
    }
    pSword->Release();

    // The saving throws close the page.
    CHECK_FALSE(items.empty());

    pSprite->SelectWeaponAbility(CGameSpriteEquipment::SLOT_FIST, 0, FALSE, TRUE);
    pSprite->m_equipment.m_items[43] = pOld;
    delete pSword;
}

namespace {

// The rand() draws from the state `before` to `after`, or -1.
int Draws(unsigned int before, unsigned int after, int limit = 1000)
{
    unsigned int state = before;
    for (int n = 0; n <= limit; n++) {
        if (state == after) {
            return n;
        }
        state = state * 214013u + 2531011u;
    }
    return -1;
}

CAbilityId QuickItemAbility(SHORT nSlot)
{
    // As the inventory screen builds it for a scroll (0x62B0B0).
    CAbilityId abilityId;
    abilityId.m_res = "";
    abilityId.m_itemType = 2;
    abilityId.m_itemNum = nSlot;
    abilityId.m_abilityNum = 0;
    return abilityId;
}

CButtonData Button(const CAbilityId& abilityId, SHORT nCount)
{
    CButtonData buttonData;
    buttonData.m_icon = "PROBE";
    buttonData.m_name = 1;
    buttonData.m_abilityId = abilityId;
    buttonData.m_count = nCount;
    return buttonData;
}

bool Cleared(const CButtonData& buttonData)
{
    return buttonData.m_count == 0
        && buttonData.m_abilityId.m_itemType == -1
        && buttonData.m_icon == ""
        && buttonData.m_name == static_cast<STRREF>(-1)
        && buttonData.m_bDisplayCount == TRUE;
}

}

// CheckQuickLists (0x71ABA0): the matching buttons of every quick list take
// the change; an emptied weapon, item or song is cleared, an emptied spell
// or innate greys out unless removeSpellIfZero; `remove` clears every match;
// only the first quick item and the first six songs count. It draws no rand.
TEST_CASE("sprite: the quick lists follow a spent ability", "[uimisc][quicklists][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }
    REQUIRE(pSprite->m_bGlobal);

    CAbilityId spell;
    spell.m_res = "SPWI112";
    spell.m_itemType = 1;
    spell.m_nClass = 11;
    spell.field_1D = 1;
    CAbilityId other = spell;
    other.m_res = "SPWI104";

    pSprite->m_quickSpells[0] = Button(spell, 2);
    pSprite->m_quickSpells[1] = Button(other, 2);
    pSprite->m_quickSpells[2] = Button(spell, 2);
    pSprite->m_quickInnates[0] = Button(spell, 1);
    pSprite->m_quickWeapons[3] = Button(spell, 1);
    pSprite->field_3D3A[3] = 7;
    pSprite->m_quickItems[0] = Button(spell, 3);
    pSprite->m_quickItems[1] = Button(spell, 3);
    pSprite->m_quickSongs[5] = Button(spell, 1);
    pSprite->m_quickSongs[6] = Button(spell, 1);

    unsigned int before = iwd2r_rand_state();
    pSprite->CheckQuickLists(&spell, -1, FALSE, FALSE);
    CHECK(Draws(before, iwd2r_rand_state()) == 0);

    CHECK(pSprite->m_quickSpells[0].m_count == 1);
    CHECK_FALSE(pSprite->m_quickSpells[0].m_bDisabled);
    CHECK(pSprite->m_quickSpells[1].m_count == 2);
    CHECK(pSprite->m_quickSpells[2].m_count == 1);
    // The innate is spent: greyed out, still there.
    CHECK(pSprite->m_quickInnates[0].m_count == 0);
    CHECK(pSprite->m_quickInnates[0].m_bDisabled);
    CHECK(pSprite->m_quickInnates[0].m_abilityId.m_res == spell.m_res);
    // The weapon button is cleared with its flag.
    CHECK(Cleared(pSprite->m_quickWeapons[3]));
    CHECK(pSprite->field_3D3A[3] == 0);
    // Only the first quick item.
    CHECK(pSprite->m_quickItems[0].m_count == 2);
    CHECK(pSprite->m_quickItems[1].m_count == 3);
    // Songs: the sixth is spent (and kept: songs go only with
    // removeSpellIfZero), the seventh is not checked.
    CHECK(pSprite->m_quickSongs[5].m_count == 0);
    CHECK(pSprite->m_quickSongs[5].m_abilityId.m_res == spell.m_res);
    CHECK(pSprite->m_quickSongs[6].m_count == 1);

    // Spent to zero with removeSpellIfZero: the spells go.
    pSprite->CheckQuickLists(&spell, -5, FALSE, TRUE);
    CHECK(Cleared(pSprite->m_quickSpells[0]));
    CHECK(Cleared(pSprite->m_quickSpells[2]));
    CHECK(pSprite->m_quickSpells[1].m_count == 2);
    CHECK(Cleared(pSprite->m_quickItems[0]));

    // A gain re-enables a greyed spell; `remove` clears it whatever its
    // count.
    pSprite->m_quickSpells[0] = Button(spell, 0);
    pSprite->m_quickSpells[0].m_bDisabled = TRUE;
    pSprite->CheckQuickLists(&spell, 1, FALSE, FALSE);
    CHECK(pSprite->m_quickSpells[0].m_count == 1);
    CHECK_FALSE(pSprite->m_quickSpells[0].m_bDisabled);
    pSprite->CheckQuickLists(&spell, 1, TRUE, FALSE);
    CHECK(Cleared(pSprite->m_quickSpells[0]));
    CHECK(Cleared(pSprite->m_quickItems[1]));

    // A creature outside the party and its allies is left alone.
    pSprite->m_bGlobal = FALSE;
    pSprite->m_quickSpells[0] = Button(spell, 2);
    pSprite->CheckQuickLists(&spell, -1, FALSE, FALSE);
    CHECK(pSprite->m_quickSpells[0].m_count == 2);
    pSprite->m_bGlobal = TRUE;

    for (int n = 0; n < 9; n++) {
        pSprite->m_quickSpells[n] = CButtonData();
        pSprite->m_quickInnates[n] = CButtonData();
        pSprite->m_quickSongs[n] = CButtonData();
    }
    pSprite->m_quickItems[0] = CButtonData();
    pSprite->m_quickItems[1] = CButtonData();
}

// CopySpell (0x62B0B0): writing the last charge of a scroll into the
// spellbook uses it up (CheckIfUsedUp 0x75D450: the scroll vanishes) and
// clears its quick item (CheckQuickLists, removeSpellIfZero).
TEST_CASE("inventory: copying a scroll's last charge removes it and its quick item", "[uimisc][inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    const SHORT nSlot = CGameSpriteEquipment::SLOT_MISC;
    CItem* pOld = pSprite->m_equipment.m_items[nSlot];
    CItem* pScroll = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
    pScroll->Demand();
    const ITEM_ABILITY* pLearn = pScroll->GetAbility(1);
    REQUIRE(pLearn != NULL);
    pScroll->Release();
    pSprite->m_equipment.m_items[nSlot] = pScroll;
    const CAbilityId item = QuickItemAbility(nSlot);
    pSprite->m_quickItems[0] = Button(item, 1);

    CScreenInventory* pInventory = g_pBaldurChitin->m_pEngineInventory;
    pInventory->m_nSelectedCharacter = 0;
    pInventory->m_nRequesterButtonId = 5; // slot 15
    REQUIRE(pInventory->MapButtonIdToInventoryId(5) == nSlot);

    // CheckIfUsedUp on its own: a spent scroll that stacks (5) vanishes
    // without a sound. It draws 22 rands, none of its own: unequipping the
    // quick slot and reselecting the weapon set run the effect list four
    // times (ProcessEffectList's five saving throw rolls each) and CheckAdd
    // (0x4A3310) twice.
    {
        CItem* pSpent = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
        pSpent->SetUsageCount(0, 0);
        pSprite->m_equipment.m_items[nSlot + 1] = pSpent;
        const unsigned int before = iwd2r_rand_state();
        CHECK(pSprite->CheckIfUsedUp(static_cast<SHORT>(nSlot + 1), 0));
        CHECK(Draws(before, iwd2r_rand_state()) == 22);
        CHECK(pSprite->m_equipment.m_items[nSlot + 1] == NULL);
    }

    pInventory->CopySpell();

    CHECK(pSprite->m_equipment.m_items[nSlot] == NULL);
    CHECK(Cleared(pSprite->m_quickItems[0]));

    pSprite->m_equipment.m_items[nSlot] = pOld;
}

// IdentifyWithScroll (0x62AE70): the identify scroll's last charge goes
// (CheckIfUsedUp 0x75D450) and the requested item is identified.
TEST_CASE("inventory: identifying with the last scroll uses it up", "[uimisc][inventory][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    CGameSprite* pSprite = PartyMember();
    if (pSprite == NULL) {
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");
    }

    const SHORT nItemSlot = 16; // button 6
    const SHORT nScrollSlot = 20;
    CItem* pOldItem = pSprite->m_equipment.m_items[nItemSlot];
    CItem* pOldScroll = pSprite->m_equipment.m_items[nScrollSlot];
    CItem* pSword = new CItem(CResRef("00SWDL01"), 0, 0, 0, 0, 0);
    pSprite->m_equipment.m_items[nItemSlot] = pSword;
    pSprite->m_equipment.m_items[nScrollSlot] = new CItem(CResRef("SPWI110Z"), 1, 1, 0, 0, 0x1);
    REQUIRE(pSprite->FindItemPersonal(CString("SPWI110Z"), 0, FALSE) == nScrollSlot);

    CScreenInventory* pInventory = g_pBaldurChitin->m_pEngineInventory;
    pInventory->m_nSelectedCharacter = 0;
    pInventory->m_nRequesterButtonId = 6;
    REQUIRE(pInventory->MapButtonIdToInventoryId(6) == nItemSlot);

    pInventory->IdentifyWithScroll();
    CHECK(pSprite->m_equipment.m_items[nScrollSlot] == NULL);
    CHECK((pSword->m_flags & 0x1) != 0);

    pSprite->m_equipment.m_items[nItemSlot] = pOldItem;
    pSprite->m_equipment.m_items[nScrollSlot] = pOldScroll;
    delete pSword;
}
