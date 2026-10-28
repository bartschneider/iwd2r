#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCreateChar.h"
#include "CScreenSpellbook.h"
#include "CUIControlButton.h"
#include "CUIPanel.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
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

// The spellbook screen (CScreenSpellbook): the class and level tabs, the
// memorized slots, memorizing and forgetting, on the engine in this process
// and a private view of the install without saves (tests/data_view.h).

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

// The CRE of the named pregenerated character (Characters/<name>.chr).
std::vector<BYTE> NamedCharacter(const std::string& name)
{
    const std::string chr = ReadFile(Child(Child(g_view, "Characters"), name + ".chr"));
    if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
        const uint32_t offset = Le32(chr, 0x28);
        const uint32_t size = Le32(chr, 0x2C);
        if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
            return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
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
        static_cast<DWORD>(-1),
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

// The party member in slot 0, denied to this thread while the test reads it.
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

}

// sub_669830 (0x669830) lists the cleric's spell classes with the domain tab
// right after the cleric tab, and UpdateMainPanel (0x66A980) lists the known
// spells of the level. Clicking a known spell (0x66D7E0) memorizes it
// (0x66C790) into the first memorized slot with "1/1"; clicking that slot
// while it is fully memorized asks first (popup 5), and UnmemorizeSpell
// (0x66CAF0) forgets it and empties the slot.
TEST_CASE("spellbook screen: a cleric memorizes and forgets a level 1 spell", "[spellbook][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;
    // Laeris, the Lady's Lament's cleric of Ilmater (Party.ini).
    std::vector<BYTE> cre = NamedCharacter("llchar4");
    if (cre.empty()) {
        SKIP("[iwd2r-skip] no llchar4.chr in the Characters folder");
    }
    NewGameWithOneCharacter(cre, "Laeris");
    CGameSprite* pSprite = PartySprite();
    REQUIRE(pSprite != NULL);
    if (pSprite->m_derivedStats.GetClassLevel(CAIOBJECTTYPE_C_CLERIC) < 1) {
        SKIP("[iwd2r-skip] llchar4 is not a cleric");
    }

    CScreenSpellbook* pSpellbook = g_pBaldurChitin->m_pEngineSpellbook;
    pSpellbook->m_nSelectedCharacter = 0;
    pSpellbook->CheckMultiPlayerViewable();
    pSprite->m_nLastSpellbookClassIndex = 0;
    pSprite->m_nLastSpellbookSpellLevel = 0;
    pSpellbook->sub_669830(0);
    pSpellbook->UpdateMainPanel();

    REQUIRE(pSpellbook->m_nNumberOfSpellClasses >= 2);
    CHECK(pSpellbook->field_1654[0] == CAIOBJECTTYPE_C_CLERIC);
    CHECK(pSpellbook->field_1670 == 1);
    CHECK(pSpellbook->field_1654[1] == 7);
    CHECK(pSpellbook->m_nClassIndex == 0);
    CHECK(pSpellbook->m_nSpellLevel == 0);
    REQUIRE(pSpellbook->m_nNumKnownSpells > 0);

    CUIPanel* pPanel = pSpellbook->GetManager()->GetPanel(2);
    CUIControlButtonSpellbookSpell* pKnown = static_cast<CUIControlButtonSpellbookSpell*>(pPanel->GetControl(30));
    REQUIRE(pKnown != NULL);
    CHECK(pKnown->m_spellResRef != "");
    CHECK(pKnown->m_bEnabled);

    // Forget whatever the character came with so the first slot is free.
    for (int n = 0; n < 100 && pSpellbook->field_1488 > 0; n++) {
        pSpellbook->UnmemorizeSpell(pSprite, 0);
    }
    pSpellbook->sub_669830(0);
    pSpellbook->UpdateMainPanel();
    REQUIRE(pSpellbook->field_1488 == 0);
    const INT nRemaining = pSpellbook->GetRemainingSpellSlots();
    REQUIRE(nRemaining > 0);

    const CResRef spell = pSpellbook->field_160C[0];
    pKnown->OnLButtonClick(CPoint(0, 0));
    CHECK(pSpellbook->m_bFlashMemorize);
    REQUIRE(pSpellbook->field_1488 == 1);
    CHECK(pSpellbook->field_148C[0] == spell);
    // Memorized, not yet castable: "0/1" until the party rests.
    CHECK(pSpellbook->field_154C[0] == 0);
    CHECK(pSpellbook->field_15AC[0] == 1);
    CHECK(pSpellbook->GetRemainingSpellSlots() == nRemaining - 1);
    CUIControlButtonSpellbookSpell* pSlot = static_cast<CUIControlButtonSpellbookSpell*>(pPanel->GetControl(6));
    REQUIRE(pSlot != NULL);
    CHECK(pSlot->m_spellResRef == spell);
    CHECK(pSlot->m_bEnabled);

    // As after a rest: a castable, fully memorized slot asks before
    // forgetting.
    CGameSpriteSpellList* pSpells = pSprite->GetSpellsAtLevel(CAIOBJECTTYPE_C_CLERIC, 0);
    UINT nID = 0;
    UINT nIndex = 0;
    REQUIRE(g_pBaldurChitin->GetObjectGame()->m_spells.Find(spell, nID));
    REQUIRE(pSpells->Find(nID, nIndex));
    pSpells->m_List[nIndex].field_8 = pSpells->m_List[nIndex].field_4;
    pSpellbook->sub_669830(0);
    pSpellbook->UpdateMainPanel();
    REQUIRE(pSpellbook->field_1488 == 1);
    CHECK(pSpellbook->field_154C[0] == 1);
    CHECK(pSpellbook->field_15AC[0] == 1);
    pSpellbook->m_bFlash = FALSE;
    pSpellbook->m_bFlashMemorize = FALSE;
    pSlot->OnLButtonClick(CPoint(0, 0));
    REQUIRE(pSpellbook->GetTopPopup() != NULL);
    CHECK(pSpellbook->GetTopPopup()->m_nID == 5);
    CHECK(pSpellbook->field_582 == 0);
    CHECK(pSpellbook->field_1488 == 1);
    pSpellbook->DismissPopup();

    pSpellbook->UnmemorizeSpell(pSprite, 0);
    CHECK(pSpellbook->m_bFlashUnmemorize);
    CHECK(pSpellbook->field_1488 == 0);
    CHECK(pSpellbook->GetRemainingSpellSlots() == nRemaining);
    pSpellbook->UpdateMainPanel();
    CHECK(pSlot->m_spellResRef == "");
    CHECK_FALSE(pSlot->m_bEnabled);
}
