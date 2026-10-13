#include "mfc_resource.h"

#include <cstring>

namespace {
struct StringEntry {
    UINT id;
    const char* text;
};

const StringEntry kStrings[] = {
    { IDS_CHOOSE_PIXEL_FORMAT, "ChoosePixelFormat failed." },
    { IDS_SET_PIXEL_FORMAT, "SetPixelFormat failed." },
    { IDS_ENGLISH, "This version will only run on English language operating systems!" },
    { IDS_US_AND_CANADA, "This version will only run in the United States and Canada!" },
    { IDS_EXSLUSIVE_MODE, "This program needs to run in \nDirect Draw Exclusive Mode\n\nPlease restart after closing other \nDirect Draw programs." },
    { IDS_INVALID_VIDEO_MODE, "Windows video mode not supported, use <ALT-Tab> to switch to Windows." },
    { IDS_OPENGLL_DLL, "Can not open OpenGL DLL." },
    { IDS_EXPIRED, "This Version of the Icewind Key file has expired please contact icewind@blackisle.com for a valid file." },
    { IDS_SET_GAME_RESOLUTION, "Can not set game resolution, user must change desktop resolution through Control Panel." },
    { IDS_SET_GAME_BIT_DEPTH, "Can not set game bit depth, user must change desktop bit depth through Control Panel." },
    { IDS_BAD_DESKTOP_BIT_DEPTH, "Can not run 3d when desktop bit depth is less than 16 bit (65536 colors), running 2d instead." },
    { IDS_WINDOWS_FONTS, "Window's fonts do not work with 3d, running 2d instead." },
};
}

const char* iwd2r_ResourceString(UINT id)
{
    for (const StringEntry& entry : kStrings) {
        if (entry.id == id)
            return entry.text;
    }
    return NULL;
}

int LoadStringA(HINSTANCE, UINT id, LPSTR buffer, int bufferMax)
{
    if (bufferMax <= 0)
        return 0;
    const char* text = iwd2r_ResourceString(id);
    if (text == NULL) {
        buffer[0] = '\0';
        return 0;
    }
    size_t length = std::strlen(text);
    size_t room = static_cast<size_t>(bufferMax - 1);
    if (length > room)
        length = room;
    std::memcpy(buffer, text, length);
    buffer[length] = '\0';
    return static_cast<int>(length);
}
