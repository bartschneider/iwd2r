// miniaudio's implementation, in a unit of its own: on Windows it includes
// <windows.h>, whose declarations clash with the shims'.
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
