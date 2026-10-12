// Modified for iwd2r; see NOTICE.md
#include "main.h"

#include "mfc.h"

#include <iostream>
#include <string>

#include "CBaldurChitin.h"
#include "platform/data_dir.h"
#include "platform/window.h"

// 0x421820 was IsSupportedOS, removed with the OS version check.

// Ruling R23. The resolutions whose start-up reaches the smoke hash and exits
// 0 headless against the real install (Task 21 fix round 1): 800 and 1024.
// 1280 crashes before the first frame; 1600 and 2048 abort after it.
static const int kSupportedResolutions[] = { 800, 1024 };

bool iwd2r_IsSupportedResolution(int width)
{
    for (int supported : kSupportedResolutions) {
        if (width == supported) {
            return true;
        }
    }
    return false;
}

int iwd2r_ScreenHeightFor(int width)
{
    switch (width) {
    case 2048:
        return 1536;
    case 1600:
        return 1200;
    case 1024:
        return 768;
    default:
        return 600;
    }
}

int iwd2r_main(int argc, char** argv)
{
    platform::window::InstallFatalMessageBox();
    // First: everything below reads icewind2.ini and game folders relative to the working directory.
    if (!platform::ResolveDataDir()) {
        return 2;
    }

    // Same section, key, default and file as CBaldurChitin's constructor.
    const int nResolution = GetPrivateProfileIntA("Program Options", "Resolution", 800, ".\\Icewind2.ini");
    if (!iwd2r_IsSupportedResolution(nResolution)) {
        const std::string text = "Resolution=" + std::to_string(nResolution)
            + " in icewind2.ini is not supported by iwd2r yet; set Resolution=800 or 1024"
              " (project 1 ports only the resolutions whose UI panels were decompiled).";
        platform::ReportFatal(text.c_str(), "iwd2r");
        return 3;
    }

    // Ruling R27: the window's client area is the engine's screen, so
    // GetSystemMetrics(SM_CXSCREEN) must report the game resolution before
    // CBaldurChitin's constructor reads it. Heights as that constructor's
    // switch derives them from Resolution.
    if (!platform::window::InitScreen(nResolution, iwd2r_ScreenHeightFor(nResolution))) {
        platform::ReportFatal("SDL video could not start.", "iwd2r");
        return 4;
    }

    std::cout << std::endl;
    std::cout << "BEGIN LOGGING SESSION";
    std::cout << std::endl;

    // NULL module: GetModuleFileNameA reports the running executable.
    CChitin::GetGameVersionInfo(NULL);

    CChitin::FixReadonlyPermissions();

    std::string cmdLine;
    for (int index = 1; index < argc; index++) {
        if (index > 1) {
            cmdLine += ' ';
        }
        cmdLine += argv[index];
    }

    CBaldurChitin baldurChitin;
    if (baldurChitin.field_1932 != 0) {
        return 0;
    }

    baldurChitin.Init(NULL);
    int rc = baldurChitin.WinMain(NULL, NULL, cmdLine.data(), SW_SHOW);
    return rc;
}
