// iwd2r. Distributed under LICENSE.md.
// Internal: a minimal PNG writer for screenshots (8-bit RGB, one IDAT).
#pragma once

#include <cstdint>

namespace platform {

// Writes width x height XRGB8888 pixels (0x00RRGGBB, rows pitchBytes apart)
// to path as an 8-bit RGB PNG. False on bad arguments or an I/O error.
bool WritePng(const char* path, const uint32_t* xrgb, int width, int height, int pitchBytes);

}
