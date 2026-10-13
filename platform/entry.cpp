// Process entry point for the future iwd2r executable (Task 21); not part of iwd2r_platform.
#include <SDL3/SDL_main.h>

#include "platform/crash.h"
#include "src/main.h"

int main(int argc, char** argv)
{
    platform::crash::Install();
    return iwd2r_main(argc, argv);
}
