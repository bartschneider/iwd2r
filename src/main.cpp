// Modified for iwd2r; see NOTICE.md
#include "main.h"

#include "mfc.h"

#include <iostream>
#include <string>

#include "CBaldurChitin.h"
#include "platform/data_dir.h"
#include "platform/window.h"

// 0x421820 was IsSupportedOS, removed with the OS version check.

int iwd2r_main(int argc, char** argv)
{
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

    platform::window::InstallFatalMessageBox();
    if (!platform::ResolveDataDir()) {
        return 2;
    }

    baldurChitin.Init(NULL);
    int rc = baldurChitin.WinMain(NULL, NULL, cmdLine.data(), SW_SHOW);
    return rc;
}
