#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "CBaldurChitin.h"
#include "CGameArea.h"
#include "CGameObjectArray.h"
#include "CGameSprite.h"
#include "CInfGame.h"
#include "CScreenCreateChar.h"
#include "CScreenOptions.h"
#include "CUIControlButton3State.h"
#include "CUIControlSlider.h"
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

// The options screen (CScreenOptions): the game play popup's difficulty
// slider and buttons, on the engine in this process and a private view of
// the install without saves (tests/data_view.h).

namespace {

namespace fs = std::filesystem;

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

}

// The difficulty slider (OnThumbFinalChange 0x6580B0, control 12 of the game
// play popup 8) stores the level and its damage multiplier and, outside a
// hosted session too, the multiplayer multiplier; UpdateGamePlayPanel
// (0x6565B0) shows the weather option on button 47.
TEST_CASE("options screen: the game play popup's difficulty slider and weather button", "[options][data]")
{
    if (DataDir() == nullptr) {
        SKIP("[iwd2r-skip] IWD2_DATA unset");
    }
    std::string error;
    REQUIRE(StartEngine(error));
    INFO(error);
    FramePump pump;

    CInfGame* pGame = g_pBaldurChitin->GetObjectGame();
    pGame->NewGame(FALSE, FALSE);
    CGameOptions* pOptions = pGame->GetOptions();
    const CGameOptions saved = *pOptions;

    CScreenOptions* pScreen = g_pBaldurChitin->m_pEngineOptions;
    CUIPanel* pPanel = pScreen->GetManager()->GetPanel(8);
    REQUIRE(pPanel != NULL);
    CUIControlSliderOptionsSlider* pSlider = static_cast<CUIControlSliderOptionsSlider*>(pPanel->GetControl(12));
    REQUIRE(pSlider != NULL);
    REQUIRE(pSlider->m_nKnobJumpCount >= 5);

    pOptions->m_nMPDifficultyMultiplier = 0;
    pSlider->m_nValue = 3;
    pSlider->OnThumbFinalChange();
    CHECK(pOptions->m_nDifficultyLevel == 4);
    CHECK(pOptions->m_nDifficultyMultiplier == 50);
    CHECK(pOptions->m_nMPDifficultyMultiplier == 50);

    pSlider->m_nValue = 0;
    pSlider->OnThumbFinalChange();
    CHECK(pOptions->m_nDifficultyLevel == 1);
    CHECK(pOptions->m_nDifficultyMultiplier == -50);
    CHECK(pOptions->m_nMPDifficultyMultiplier == -50);

    CUIControlButton3State* pWeather = static_cast<CUIControlButton3State*>(pPanel->GetControl(47));
    REQUIRE(pWeather != NULL);
    pOptions->m_bWeatherEnabled = TRUE;
    pScreen->UpdateGamePlayPanel(TRUE);
    CHECK(pWeather->m_bSelected);
    pOptions->m_bWeatherEnabled = FALSE;
    pScreen->UpdateGamePlayPanel(TRUE);
    CHECK_FALSE(pWeather->m_bSelected);

    *pOptions = saved;
}
