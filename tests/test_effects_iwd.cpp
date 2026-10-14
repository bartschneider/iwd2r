#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameEffect.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CMessage.h"
#include "CScreenCreateChar.h"
#include "CScreenWorld.h"
#include "IcewindCGameEffects.h"
#include "ScreenLayout.h"
#include "data_view.h"
#include "platform/data_dir.h"
#include "platform/window.h"

#include <algorithm>
#include <bitset>
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

// The ApplyEffect of the Icewind effect classes (IcewindCGameEffects.cpp)
// on a party member standing in AR1000. Each effect is decoded from an
// ITEM_EFFECT as the game decodes it (CGameEffect::DecodeEffect) and applied
// directly. Every engine works on a private view of the install
// (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

const char* DataDir()
{
    const char* env = std::getenv("IWD2_DATA");
    return env != nullptr && *env != '\0' ? env : nullptr;
}

std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

fs::path Child(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (Lower(entry.path().filename().string()) == Lower(name))
            return entry.path();
    }
    return dir / name;
}

uint32_t Le32(const std::string& d, size_t at)
{
    return uint32_t(static_cast<unsigned char>(d[at])) | (uint32_t(static_cast<unsigned char>(d[at + 1])) << 8)
        | (uint32_t(static_cast<unsigned char>(d[at + 2])) << 16) | (uint32_t(static_cast<unsigned char>(d[at + 3])) << 24);
}

fs::path g_view;

// The engine as iwd2r_main builds it (tests/test_gam.cpp's StartEngine).
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
    view = std::make_unique<DataView>(original);
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

// The main loop's drawn frames (tests/test_gam.cpp's FramePump).
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
        std::ifstream in(file, std::ios::binary);
        const std::string chr((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (chr.size() > 0x224 && chr.compare(0, 8, "CHR V2.2") == 0) {
            const uint32_t offset = Le32(chr, 0x28);
            const uint32_t size = Le32(chr, 0x2C);
            if (offset + size <= chr.size() && chr.compare(offset, 8, "CRE V2.2") == 0)
                return std::vector<BYTE>(chr.begin() + offset, chr.begin() + offset + size);
        }
    }
    return std::vector<BYTE>();
}

// A new game with one party member standing in AR1000, the world screen's
// panels loaded (tests/test_dialog.cpp's NewGameInStartArea).
CGameSprite* NewGameInStartArea(std::vector<BYTE>& bytes)
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
    pSprite->m_sName = "Probe";
    pSprite->m_bGlobal = TRUE;
    pGame->SetCharacterSlot(0, pSprite->GetId());
    pGame->UpdateCharacterSlots();
    pGame->SetupCharacters(FALSE);
    for (int n = 0; n < CINFGAME_MAX_AREAS; n++) {
        if (pGame->m_gameAreas[n] != NULL)
            pGame->m_gameAreas[n]->SortLists();
    }

    CScreenWorld* pWorld = g_pBaldurChitin->m_pEngineWorld;
    pWorld->EngineGameInit();
    // The world screen is the active engine, as after the game loads (the
    // test does not switch engines: EngineActivated starts the sound).
    g_pBaldurChitin->pActiveEngine = pWorld;
    pGame->m_bGameLoaded = TRUE;
    return pSprite;
}


void ClearMessages()
{
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    while (!handler.m_messageList.IsEmpty()) {
        delete handler.m_messageList.RemoveHead();
    }
}

// The party member every case works on, made once.
CGameSprite* Probe()
{
    static CGameSprite* pSprite = NULL;
    if (pSprite == NULL) {
        std::vector<BYTE> cre = FinishedCharacter();
        if (cre.empty()) {
            return NULL;
        }
        pSprite = NewGameInStartArea(cre);
    }
    return pSprite;
}

// An effect of this opcode decoded as the game decodes it, its source the
// sprite itself.
std::unique_ptr<CGameEffect> Decode(CGameSprite* pSprite, WORD effectID, DWORD dwFlags, LONG effectAmount)
{
    ITEM_EFFECT effect;
    memset(&effect, 0, sizeof(effect));
    CGameEffect::ClearItemEffect(&effect, effectID);
    effect.dwFlags = dwFlags;
    effect.effectAmount = effectAmount;
    return std::unique_ptr<CGameEffect>(CGameEffect::DecodeEffect(&effect, pSprite->GetPos(), pSprite->GetId(), pSprite->GetPos()));
}

bool HasPortraitIcon(CGameSprite* pSprite, int icon)
{
    return pSprite->m_portraitIcons.Find(reinterpret_cast<int*>(static_cast<INT_PTR>(icon)), NULL) != NULL;
}

#define PROBE_OR_SKIP(pSprite)                                                  \
    if (DataDir() == nullptr) {                                                  \
        SKIP("[iwd2r-skip] IWD2_DATA unset");                                    \
    }                                                                           \
    std::string error;                                                          \
    REQUIRE(StartEngine(error));                                                \
    INFO(error);                                                                \
    FramePump pump;                                                             \
    CGameSprite* pSprite = Probe();                                             \
    if (pSprite == NULL) {                                                      \
        SKIP("[iwd2r-skip] no CHR V2.2 file in the Characters folder");          \
    }

}

TEST_CASE("effects-iwd: immunity to effect (261) adds the opcode to the natural immunities", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_IMMUNITYTOEFFECTRESOURCE, 24, 0);
    REQUIRE(pEffect != nullptr);
    pSprite->GetDerivedStats()->m_naturalImmunities.erase(24);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_naturalImmunities.count(24) == 1);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_naturalImmunities.count(24) == 1);
    pSprite->GetDerivedStats()->m_naturalImmunities.erase(24);
}

TEST_CASE("effects-iwd: visual effect (413) sets the bit and the Lathander and Seven Eyes partners", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::bitset<32> saved = pSprite->GetDerivedStats()->m_visualEffects;
    pSprite->GetDerivedStats()->m_visualEffects.reset();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALEFFECT, IWD_VFX_SHIELD_OF_LATHANDER, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_visualEffects.to_ulong() == 0xC0000);
    pSprite->GetDerivedStats()->m_visualEffects.reset();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALEFFECT, 3, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pSprite->GetDerivedStats()->m_visualEffects.to_ulong() == 0x8);
    pSprite->GetDerivedStats()->m_visualEffects = saved;
}

TEST_CASE("effects-iwd: bleeding wounds (416) adds a regenerating persistant effect and its icon", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CDerivedStats* pStats = pSprite->GetDerivedStats();
    for (DWORD flags = 0; flags <= 2; flags++) {
        INT_PTR before = pStats->m_cRegeneratedPersistantEffectList.GetCount();
        std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_BLEEDINGWOUNDS, flags, 3);
        CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
        CHECK(pStats->m_cRegeneratedPersistantEffectList.GetCount() == before + 1);
        CHECK(HasPortraitIcon(pSprite, 85));
    }
    while (!pStats->m_cRegeneratedPersistantEffectList.IsEmpty()) {
        delete pStats->m_cRegeneratedPersistantEffectList.RemoveTail();
    }
}

TEST_CASE("effects-iwd: float text (283) queues the string's message only for type 0", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 0, 4392);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CMessageFloatText* message = dynamic_cast<CMessageFloatText*>(handler.m_messageList.GetHead());
    REQUIRE(message != nullptr);
    CHECK(message->m_strRef == 4392);
    ClearMessages();
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_FLOATTEXT, 1, 4392);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(handler.m_messageList.IsEmpty());
}

TEST_CASE("effects-iwd: visual spell hit (233) queues the projectile message and is done", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    CMessageHandler& handler = g_pBaldurChitin->m_cMessageHandler;
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VISUALSPELLHIT, 4, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == FALSE);
    CHECK(pEffect->m_done == TRUE);
    REQUIRE(handler.m_messageList.GetCount() == 1);
    CHECK(dynamic_cast<CMessageFireProjectile*>(handler.m_messageList.GetHead()) != nullptr);
    ClearMessages();
}

TEST_CASE("effects-iwd: apply effects list (402) stops on a failed condition and is done", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    // 1: only undead. The party member is not: nothing more happens.
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 1, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    // 43: only the source itself, which it is.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 43, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    // 41: compares with the source; an unknown source returns FALSE.
    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_APPLYEFFECTSLIST, 41, 0);
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == FALSE);
}

TEST_CASE("effects-iwd: Beltyn's burning blood (247) queues one delayed copy per further round", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    CGameEffectList* pList = pSprite->GetTimedEffectList();
    auto delayedCopies = [&]() {
        int n = 0;
        POSITION pos = pList->GetHeadPosition();
        while (pos != NULL) {
            CGameEffect* pOther = pList->GetNext(pos);
            if (pOther->m_effectID == ICEWIND_CGAMEEFFECT_BELTYNSBURNINGBLOOD && pOther->m_durationType == 7) {
                n++;
            }
        }
        return n;
    };
    // The copies are delayed (duration type 4), which ResolveEffect turns
    // into 7, waiting for their time.
    const int before = delayedCopies();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_BELTYNSBURNINGBLOOD, 0, 3);
    // An unknown source stops it after the copies: the damage and berserk
    // effects it adds next have no ApplyEffect in CGameEffect.cpp yet.
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(delayedCopies() == before + 2);
}

TEST_CASE("effects-iwd: animation remove (260) and vitriolic sphere (281) apply and finish", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    ClearMessages();
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_ANIMATIONREMOVE, 0, 0);
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    // Single player: no CMessage90 for the other machines.
    CHECK(g_pBaldurChitin->m_cMessageHandler.m_messageList.IsEmpty());

    pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VITRIOLICSPHERE, 0, 0);
    pEffect->m_sourceID = -1;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
}

TEST_CASE("effects-iwd: vampiric touch (241) with an unknown source only finishes", "[effects][data]")
{
    PROBE_OR_SKIP(pSprite);
    std::unique_ptr<CGameEffect> pEffect = Decode(pSprite, ICEWIND_CGAMEEFFECT_VAMPIRICTOUCH, 0, 5);
    pEffect->m_sourceID = -1;
    const SHORT hp = pSprite->GetBaseStats()->m_hitPoints;
    CHECK(pEffect->ApplyEffect(pSprite) == TRUE);
    CHECK(pEffect->m_done == TRUE);
    CHECK(pSprite->GetBaseStats()->m_hitPoints == hp);
}
