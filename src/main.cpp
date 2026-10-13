#include "main.h"

#include "mfc.h"

#include <cstdio>
#include <iostream>
#include <string>

#include "CBaldurChitin.h"
#include "ScreenLayout.h"
#include "platform/data_dir.h"
#include "platform/window.h"

// 0x421820 was IsSupportedOS, removed with the OS version check.

int iwd2r_main(int argc, char** argv)
{
    platform::window::InstallFatalMessageBox();
    // First: everything below reads icewind2.ini and game folders relative to the working directory.
    if (!platform::ResolveDataDir()) {
        return 2;
    }

    // The same decision as CBaldurChitin's constructor (iwd2r::ReadScreenSize):
    // Resolution (a width or "desktop"), Height and Scale, at least 800x600
    // inside the engine. Any size runs; the layout is generalised by
    // iwd2r::ComputeScreenLayout.
    const iwd2r::ScreenSize screen = iwd2r::ReadScreenSize(".\\Icewind2.ini");
    if (!screen.ok) {
        platform::ReportFatal(screen.error.c_str(), "iwd2r");
        return 3;
    }
    for (const std::string& notice : screen.notices) {
        std::fprintf(stderr, "iwd2r: %s\n", notice.c_str());
    }

    // Ruling R27: the window's client area is the engine's screen, so
    // GetSystemMetrics(SM_CXSCREEN) must report the game resolution before
    // CBaldurChitin's constructor reads it. The window is that screen times
    // the scale (World Scale, which defaults to Scale); the UI panels are
    // shown at the Scale that fits (screen.worldUiScale, fullUiScale).
    if (!platform::window::InitScreen(screen.width, screen.height, screen.scale, screen.worldUiScale)) {
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

    // The constructor applied the layout; the presenter's regions follow
    // the screen the engine shows from here (only while World Scale differs
    // from Scale), until baldurChitin goes out of scope.
    platform::window::SetRegionSource(iwd2r::CurrentRegions);
    struct RegionSourceGuard {
        ~RegionSourceGuard() { platform::window::SetRegionSource(nullptr); }
    } regionSourceGuard;

    baldurChitin.Init(NULL);
    int rc = baldurChitin.WinMain(NULL, NULL, cmdLine.data(), SW_SHOW);
    return rc;
}
