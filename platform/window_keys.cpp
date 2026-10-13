// Win32 virtual key <-> SDL scancode table for the window shim.
#define NOMINMAX
#include "platform/window.h"

#include <SDL3/SDL.h>

namespace {

struct KeyEntry {
    int vk;
    SDL_Scancode sc;
};

// Every VK_* the engine references, plus the sided modifiers. Letters and
// digits are computed in ScancodeForVK. Generic modifiers name the left key.
const KeyEntry kKeys[] = {
    { VK_BACK, SDL_SCANCODE_BACKSPACE },
    { VK_TAB, SDL_SCANCODE_TAB },
    { VK_RETURN, SDL_SCANCODE_RETURN },
    { VK_SHIFT, SDL_SCANCODE_LSHIFT },
    { VK_CONTROL, SDL_SCANCODE_LCTRL },
    { VK_MENU, SDL_SCANCODE_LALT },
    { VK_ESCAPE, SDL_SCANCODE_ESCAPE },
    { VK_SPACE, SDL_SCANCODE_SPACE },
    { VK_PRIOR, SDL_SCANCODE_PAGEUP },
    { VK_NEXT, SDL_SCANCODE_PAGEDOWN },
    { VK_END, SDL_SCANCODE_END },
    { VK_HOME, SDL_SCANCODE_HOME },
    { VK_LEFT, SDL_SCANCODE_LEFT },
    { VK_UP, SDL_SCANCODE_UP },
    { VK_RIGHT, SDL_SCANCODE_RIGHT },
    { VK_DOWN, SDL_SCANCODE_DOWN },
    { VK_SNAPSHOT, SDL_SCANCODE_PRINTSCREEN },
    { VK_INSERT, SDL_SCANCODE_INSERT },
    { VK_DELETE, SDL_SCANCODE_DELETE },
    { VK_NUMPAD0, SDL_SCANCODE_KP_0 },
    { VK_NUMPAD1, SDL_SCANCODE_KP_1 },
    { VK_NUMPAD2, SDL_SCANCODE_KP_2 },
    { VK_NUMPAD3, SDL_SCANCODE_KP_3 },
    { VK_NUMPAD4, SDL_SCANCODE_KP_4 },
    { VK_NUMPAD5, SDL_SCANCODE_KP_5 },
    { VK_NUMPAD6, SDL_SCANCODE_KP_6 },
    { VK_NUMPAD7, SDL_SCANCODE_KP_7 },
    { VK_NUMPAD8, SDL_SCANCODE_KP_8 },
    { VK_NUMPAD9, SDL_SCANCODE_KP_9 },
    { VK_MULTIPLY, SDL_SCANCODE_KP_MULTIPLY },
    { VK_ADD, SDL_SCANCODE_KP_PLUS },
    { VK_SEPARATOR, SDL_SCANCODE_KP_COMMA },
    { VK_SUBTRACT, SDL_SCANCODE_KP_MINUS },
    { VK_DECIMAL, SDL_SCANCODE_KP_PERIOD },
    { VK_DIVIDE, SDL_SCANCODE_KP_DIVIDE },
    { VK_F1, SDL_SCANCODE_F1 },
    { VK_F2, SDL_SCANCODE_F2 },
    { VK_F3, SDL_SCANCODE_F3 },
    { VK_F4, SDL_SCANCODE_F4 },
    { VK_F5, SDL_SCANCODE_F5 },
    { VK_F6, SDL_SCANCODE_F6 },
    { VK_F7, SDL_SCANCODE_F7 },
    { VK_F8, SDL_SCANCODE_F8 },
    { VK_F9, SDL_SCANCODE_F9 },
    { VK_F10, SDL_SCANCODE_F10 },
    { VK_F11, SDL_SCANCODE_F11 },
    { VK_F12, SDL_SCANCODE_F12 },
    { VK_NUMLOCK, SDL_SCANCODE_NUMLOCKCLEAR },
    { VK_LSHIFT, SDL_SCANCODE_LSHIFT },
    { VK_RSHIFT, SDL_SCANCODE_RSHIFT },
    { VK_LCONTROL, SDL_SCANCODE_LCTRL },
    { VK_RCONTROL, SDL_SCANCODE_RCTRL },
    { VK_LMENU, SDL_SCANCODE_LALT },
    { VK_RMENU, SDL_SCANCODE_RALT },
    { VK_OEM_1, SDL_SCANCODE_SEMICOLON },
    { VK_OEM_PLUS, SDL_SCANCODE_EQUALS },
    { VK_OEM_COMMA, SDL_SCANCODE_COMMA },
    { VK_OEM_MINUS, SDL_SCANCODE_MINUS },
    { VK_OEM_PERIOD, SDL_SCANCODE_PERIOD },
    { VK_OEM_2, SDL_SCANCODE_SLASH },
    { VK_OEM_3, SDL_SCANCODE_GRAVE },
    { VK_OEM_4, SDL_SCANCODE_LEFTBRACKET },
    { VK_OEM_5, SDL_SCANCODE_BACKSLASH },
    { VK_OEM_6, SDL_SCANCODE_RIGHTBRACKET },
    { VK_OEM_7, SDL_SCANCODE_APOSTROPHE },
    // No US-layout key; the ISO key beside Enter is the nearest free position.
    { VK_OEM_8, SDL_SCANCODE_NONUSHASH },
    // An IME-processed keystroke has no physical key; LANG1 is the IME toggle.
    { VK_PROCESSKEY, SDL_SCANCODE_LANG1 },
};

}

namespace platform::window {

int ScancodeForVK(int vk)
{
    if (vk >= 'A' && vk <= 'Z')
        return SDL_SCANCODE_A + (vk - 'A');
    if (vk >= '1' && vk <= '9')
        return SDL_SCANCODE_1 + (vk - '1');
    if (vk == '0')
        return SDL_SCANCODE_0;
    for (const KeyEntry& entry : kKeys) {
        if (entry.vk == vk)
            return entry.sc;
    }
    return SDL_SCANCODE_UNKNOWN;
}

int VKForScancode(int sc)
{
    switch (sc) {
    case SDL_SCANCODE_UNKNOWN:
        return 0;
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
        return VK_SHIFT;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
        return VK_CONTROL;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
        return VK_MENU;
    case SDL_SCANCODE_KP_ENTER:
        return VK_RETURN;
    case SDL_SCANCODE_0:
        return '0';
    default:
        break;
    }
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return 'A' + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return '1' + (sc - SDL_SCANCODE_1);
    for (const KeyEntry& entry : kKeys) {
        if (entry.sc == sc)
            return entry.vk;
    }
    return 0;
}

}
