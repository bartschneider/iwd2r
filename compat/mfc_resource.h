#pragma once

#include "win_types.h"

// clang-format off
#define IDS_CHOOSE_PIXEL_FORMAT     1
#define IDS_SET_PIXEL_FORMAT        2
#define IDS_ENGLISH                 3
#define IDS_US_AND_CANADA           4
#define IDS_EXSLUSIVE_MODE          5
#define IDS_INVALID_VIDEO_MODE      6
#define IDS_OPENGLL_DLL             7
#define IDS_EXPIRED                 8
#define IDS_SET_GAME_RESOLUTION     9
#define IDS_SET_GAME_BIT_DEPTH      10
#define IDS_BAD_DESKTOP_BIT_DEPTH   11
#define IDS_WINDOWS_FONTS           12
// clang-format on

// Text of the engine string table. Returns NULL for unknown ids.
const char* iwd2r_ResourceString(UINT id);

// Copies at most bufferMax - 1 characters plus a terminator; returns the
// number of characters copied, 0 when the id is unknown.
int LoadStringA(HINSTANCE instance, UINT id, LPSTR buffer, int bufferMax);
