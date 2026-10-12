// iwd2r. Distributed under LICENSE.md.
// The engine's one popup window, its message queue and polled input, over
// SDL3. SDL events become WM_* messages in a Win32-shaped queue that the
// engine's PeekMessageA/GetMessageA loop drains and dispatches to its own
// window procedure.
#define NOMINMAX
#include "platform/window.h"

#include "platform/data_dir.h"
#include "platform/present.h"
#include "platform/win32_api.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <string>

namespace {

struct WindowRecord {
    SDL_Window* window = nullptr;
    WNDPROC proc = nullptr;
    std::string title;
    bool minimized = false;
    bool destroying = false;
};

struct State {
    // Guards queue, current, quitting, quitCode, classes and messageIds.
    // The engine may post from worker threads; everything else runs on the
    // main thread, which is also the only writer of current.
    std::mutex mutex;
    std::deque<MSG> queue;
    WindowRecord* current = nullptr;
    bool quitting = false;
    std::map<std::string, WNDPROC> classes;
    std::map<std::string, UINT> messageIds;
    UINT nextMessageId = 0xC000;
    ATOM nextAtom = 0xC000;

    int cursorCount = 0;
    HCURSOR cursor = nullptr;

    int smokeFrames = 0;
    bool smokeDone = false;
    bool smokeEnvRead = false;
};

State& S()
{
    static State state;
    return state;
}

char g_arrowCursor;

HWND ToHwnd(WindowRecord* record) { return reinterpret_cast<HWND>(record); }

// The live window when hWnd names it, else nullptr. Compares the pointer
// only, so stale handles are never dereferenced.
WindowRecord* Rec(HWND hWnd)
{
    WindowRecord* current = S().current;
    return hWnd != NULL && hWnd == ToHwnd(current) ? current : nullptr;
}

std::string Lower(const char* text)
{
    std::string s(text);
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool VideoUp() { return SDL_WasInit(SDL_INIT_VIDEO) != 0; }

// The dummy and offscreen drivers have no real desktop: no global mouse, no
// message boxes.
bool Headless()
{
    const char* driver = VideoUp() ? SDL_GetCurrentVideoDriver() : SDL_GetHint(SDL_HINT_VIDEO_DRIVER);
    return driver && (SDL_strcasecmp(driver, "dummy") == 0 || SDL_strcasecmp(driver, "offscreen") == 0);
}

void RefreshScreenSize(SDL_Window* window)
{
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (mode && mode->w > 0 && mode->h > 0) {
        platform::present::SetScreenSize(mode->w, mode->h);
        return;
    }
    int w = 0;
    int h = 0;
    if (window && SDL_GetWindowSize(window, &w, &h) && w > 0 && h > 0)
        platform::present::SetScreenSize(w, h);
}

void WindowOrigin(const WindowRecord* record, int& x, int& y)
{
    x = 0;
    y = 0;
    if (record)
        SDL_GetWindowPosition(record->window, &x, &y);
}

// msg.pt is left zero: the engine never reads it, and filling it would call
// SDL from whichever thread posts.
// Posts unless a non-null hWnd no longer names the live window; checked and
// queued under one lock so a worker cannot post to a window being destroyed.
bool Enqueue(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MSG msg = {};
    msg.hwnd = hWnd;
    msg.message = message;
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.time = GetTickCount();
    std::lock_guard<std::mutex> lock(S().mutex);
    if (hWnd != NULL && hWnd != ToHwnd(S().current))
        return false;
    S().queue.push_back(msg);
    return true;
}

bool Matches(const MSG& msg, HWND hWnd, UINT filterMin, UINT filterMax)
{
    if (hWnd != NULL && msg.hwnd != hWnd)
        return false;
    if (filterMin == 0 && filterMax == 0)
        return true;
    return msg.message >= filterMin && msg.message <= filterMax;
}

// Takes the first matching message; once the window is destroyed and the
// queue holds nothing matching, reports WM_QUIT every time.
bool Fetch(LPMSG msg, HWND hWnd, UINT filterMin, UINT filterMax, bool remove)
{
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = std::find_if(s.queue.begin(), s.queue.end(),
        [&](const MSG& m) { return Matches(m, hWnd, filterMin, filterMax); });
    if (it != s.queue.end()) {
        *msg = *it;
        if (remove)
            s.queue.erase(it);
        return true;
    }
    if (s.quitting) {
        *msg = MSG();
        msg->message = WM_QUIT;
        return true;
    }
    return false;
}

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
    { VK_NUMPAD0, SDL_SCANCODE_KP_0 }, { VK_NUMPAD1, SDL_SCANCODE_KP_1 }, { VK_NUMPAD2, SDL_SCANCODE_KP_2 }, { VK_NUMPAD3, SDL_SCANCODE_KP_3 },
    { VK_NUMPAD4, SDL_SCANCODE_KP_4 }, { VK_NUMPAD5, SDL_SCANCODE_KP_5 }, { VK_NUMPAD6, SDL_SCANCODE_KP_6 }, { VK_NUMPAD7, SDL_SCANCODE_KP_7 },
    { VK_NUMPAD8, SDL_SCANCODE_KP_8 }, { VK_NUMPAD9, SDL_SCANCODE_KP_9 },
    { VK_MULTIPLY, SDL_SCANCODE_KP_MULTIPLY },
    { VK_ADD, SDL_SCANCODE_KP_PLUS },
    { VK_SEPARATOR, SDL_SCANCODE_KP_COMMA },
    { VK_SUBTRACT, SDL_SCANCODE_KP_MINUS },
    { VK_DECIMAL, SDL_SCANCODE_KP_PERIOD },
    { VK_DIVIDE, SDL_SCANCODE_KP_DIVIDE },
    { VK_F1, SDL_SCANCODE_F1 }, { VK_F2, SDL_SCANCODE_F2 }, { VK_F3, SDL_SCANCODE_F3 }, { VK_F4, SDL_SCANCODE_F4 },
    { VK_F5, SDL_SCANCODE_F5 }, { VK_F6, SDL_SCANCODE_F6 }, { VK_F7, SDL_SCANCODE_F7 }, { VK_F8, SDL_SCANCODE_F8 },
    { VK_F9, SDL_SCANCODE_F9 }, { VK_F10, SDL_SCANCODE_F10 }, { VK_F11, SDL_SCANCODE_F11 }, { VK_F12, SDL_SCANCODE_F12 },
    { VK_NUMLOCK, SDL_SCANCODE_NUMLOCKCLEAR },
    { VK_LSHIFT, SDL_SCANCODE_LSHIFT }, { VK_RSHIFT, SDL_SCANCODE_RSHIFT }, { VK_LCONTROL, SDL_SCANCODE_LCTRL }, { VK_RCONTROL, SDL_SCANCODE_RCTRL },
    { VK_LMENU, SDL_SCANCODE_LALT }, { VK_RMENU, SDL_SCANCODE_RALT },
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

void Translate(const SDL_Event& e)
{
    using platform::window::EnqueueMessage;
    WindowRecord* rec = S().current;
    const SDL_WindowID id = rec ? SDL_GetWindowID(rec->window) : 0;

    if (e.type == SDL_EVENT_QUIT) {
        EnqueueMessage(WM_CLOSE, 0, 0);
        return;
    }
    if (e.type >= SDL_EVENT_DISPLAY_FIRST && e.type <= SDL_EVENT_DISPLAY_LAST) {
        RefreshScreenSize(rec ? rec->window : nullptr);
        const SIZE screen = platform::present::ScreenSize();
        EnqueueMessage(WM_DISPLAYCHANGE, 32, MAKELONG(screen.cx, screen.cy));
        return;
    }
    if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST) {
        if (!rec || e.window.windowID != id)
            return;
        switch (e.type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            EnqueueMessage(WM_CLOSE, 0, 0);
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            EnqueueMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(e.window.data1, e.window.data2));
            break;
        case SDL_EVENT_WINDOW_MOVED:
            EnqueueMessage(WM_MOVE, 0, MAKELONG(e.window.data1, e.window.data2));
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            rec->minimized = true;
            EnqueueMessage(WM_SIZE, SIZE_MINIMIZED, 0);
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED: {
            rec->minimized = false;
            int w = 0;
            int h = 0;
            SDL_GetWindowSize(rec->window, &w, &h);
            EnqueueMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(w, h));
            break;
        }
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            EnqueueMessage(WM_ACTIVATEAPP, 1, 0);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            EnqueueMessage(WM_ACTIVATEAPP, 0, 0);
            EnqueueMessage(WM_KILLFOCUS, 0, 0);
            break;
        default:
            break;
        }
        return;
    }
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        if (!rec || e.wheel.windowID != id)
            return;
        float dy = e.wheel.y;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            dy = -dy;
        const short delta = static_cast<short>(std::lround(dy * 120.0f));
        int wx = 0;
        int wy = 0;
        WindowOrigin(rec, wx, wy);
        const int px = wx + static_cast<int>(e.wheel.mouse_x);
        const int py = wy + static_cast<int>(e.wheel.mouse_y);
        EnqueueMessage(WM_MOUSEWHEEL, static_cast<WPARAM>(static_cast<DWORD>(static_cast<WORD>(delta)) << 16), MAKELONG(px, py));
        return;
    }
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (!rec || e.key.windowID != id || (e.key.mod & SDL_KMOD_ALT) == 0)
            return;
        const int vk = platform::window::VKForScancode(e.key.scancode);
        if (vk == 0)
            return;
        // Repeat count 1, context code (Alt held) in bit 29.
        const LPARAM lParam = static_cast<LPARAM>(1u | (static_cast<DWORD>(e.key.scancode & 0xFF) << 16) | (1u << 29));
        EnqueueMessage(WM_SYSKEYDOWN, static_cast<WPARAM>(vk), lParam);
    }
}

void CheckSmoke()
{
    State& s = S();
    if (s.smokeFrames <= 0 || s.smokeDone || platform::present::PresentedFrames() < s.smokeFrames)
        return;
    s.smokeDone = true;
    std::printf("framebuffer-hash: %016llx\n", static_cast<unsigned long long>(platform::present::HashPrimary()));
    std::fflush(stdout);
    platform::window::EnqueueMessage(WM_DESTROY, 0, 0);
}

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

void PumpEvents()
{
    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            Translate(e);
    }
    platform::present::PresentIfDirty();
    CheckSmoke();
}

void EnqueueMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    Enqueue(MainWindow(), message, wParam, lParam);
}

HWND MainWindow()
{
    std::lock_guard<std::mutex> lock(S().mutex);
    return ToHwnd(S().current);
}

void SetSmokeFrames(int n)
{
    S().smokeFrames = n;
    S().smokeDone = false;
}

void InstallFatalMessageBox()
{
    platform::SetFatalMessageHandler([](const char* text, const char* caption) {
        MessageBoxA(NULL, text, caption, MB_OK | MB_ICONERROR);
    });
}

}

ATOM RegisterClassExA(const WNDCLASSEXA* windowClass)
{
    if (!windowClass || !windowClass->lpszClassName || !windowClass->lpfnWndProc)
        return 0;
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.classes[Lower(windowClass->lpszClassName)] = windowClass->lpfnWndProc;
    return s.nextAtom++;
}

HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y, int width, int height, HWND, HMENU, HINSTANCE, LPVOID)
{
    State& s = S();
    WNDPROC proc = nullptr;
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.current || !className)
            return NULL;
        auto it = s.classes.find(Lower(className));
        if (it == s.classes.end())
            return NULL;
        proc = it->second;
    }

    if (!VideoUp() && !SDL_Init(SDL_INIT_VIDEO))
        return NULL;

    const bool defaultPos = x == CW_USEDEFAULT || y == CW_USEDEFAULT;
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title ? title : "");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, defaultPos ? SDL_WINDOWPOS_CENTERED : x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, defaultPos ? SDL_WINDOWPOS_CENTERED : y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width == CW_USEDEFAULT ? 640 : width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height == CW_USEDEFAULT ? 480 : height);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, (style & WS_POPUP) != 0);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, (style & WS_VISIBLE) == 0);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, (exStyle & WS_EX_TOPMOST) != 0);
    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window)
        return NULL;

    WindowRecord* rec = new WindowRecord();
    rec->window = window;
    rec->proc = proc;
    rec->title = title ? title : "";

    platform::present::SetPresentTarget(window);
    RefreshScreenSize(window);

    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.current = rec;
        // A new window (Alt+Enter recreates it) revives the loop.
        s.quitting = false;
    }

    if (!s.smokeEnvRead) {
        s.smokeEnvRead = true;
        if (const char* frames = std::getenv("IWD2R_SMOKE_FRAMES"))
            platform::window::SetSmokeFrames(std::atoi(frames));
    }
    return ToHwnd(rec);
}

BOOL DestroyWindow(HWND hWnd)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec)
        return FALSE;
    // The engine's WM_DESTROY handler shuts down, which destroys the window
    // again.
    if (rec->destroying)
        return TRUE;
    rec->destroying = true;
    if (rec->proc)
        rec->proc(hWnd, WM_DESTROY, 0, 0);

    // SDL destroys the window's renderer with it; drop present's cached one
    // first.
    platform::present::SetPresentTarget(nullptr);
    SDL_DestroyWindow(rec->window);

    {
        State& s = S();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.queue.erase(std::remove_if(s.queue.begin(), s.queue.end(), [&](const MSG& m) { return m.hwnd == hWnd; }),
            s.queue.end());
        s.current = nullptr;
        s.quitting = true;
    }
    delete rec;
    return TRUE;
}

BOOL ShowWindow(HWND hWnd, int command)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec)
        return FALSE;
    const BOOL wasVisible = (SDL_GetWindowFlags(rec->window) & SDL_WINDOW_HIDDEN) == 0;
    switch (command) {
    case SW_HIDE:
        SDL_HideWindow(rec->window);
        break;
    case SW_MINIMIZE:
    case SW_SHOWMINIMIZED:
        SDL_ShowWindow(rec->window);
        SDL_MinimizeWindow(rec->window);
        break;
    case SW_MAXIMIZE:
        SDL_ShowWindow(rec->window);
        SDL_MaximizeWindow(rec->window);
        break;
    case SW_RESTORE:
        SDL_ShowWindow(rec->window);
        SDL_RestoreWindow(rec->window);
        break;
    default:
        SDL_ShowWindow(rec->window);
        break;
    }
    return wasVisible;
}

BOOL UpdateWindow(HWND hWnd) { return Rec(hWnd) ? TRUE : FALSE; }

BOOL SetWindowPos(HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec)
        return FALSE;
    if ((flags & SWP_NOMOVE) == 0)
        SDL_SetWindowPosition(rec->window, x, y);
    if ((flags & SWP_NOSIZE) == 0)
        SDL_SetWindowSize(rec->window, cx, cy);
    if ((flags & SWP_NOZORDER) == 0) {
        if (insertAfter == HWND_TOPMOST)
            SDL_SetWindowAlwaysOnTop(rec->window, true);
        else if (insertAfter == HWND_NOTOPMOST)
            SDL_SetWindowAlwaysOnTop(rec->window, false);
    }
    if (flags & SWP_SHOWWINDOW)
        SDL_ShowWindow(rec->window);
    if (flags & SWP_HIDEWINDOW)
        SDL_HideWindow(rec->window);
    SDL_SyncWindow(rec->window);
    return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT rect)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec || !rect)
        return FALSE;
    if (rec->minimized || (SDL_GetWindowFlags(rec->window) & SDL_WINDOW_MINIMIZED)) {
        SetRectEmpty(rect);
        return TRUE;
    }
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(rec->window, &w, &h);
    SetRect(rect, 0, 0, w, h);
    return TRUE;
}

BOOL GetWindowRect(HWND hWnd, LPRECT rect)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec || !rect)
        return FALSE;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    SDL_GetWindowPosition(rec->window, &x, &y);
    SDL_GetWindowSize(rec->window, &w, &h);
    SetRect(rect, x, y, x + w, y + h);
    int top = 0;
    int left = 0;
    int bottom = 0;
    int right = 0;
    if ((SDL_GetWindowFlags(rec->window) & SDL_WINDOW_BORDERLESS) == 0
        && SDL_GetWindowBordersSize(rec->window, &top, &left, &bottom, &right)) {
        rect->left -= left;
        rect->top -= top;
        rect->right += right;
        rect->bottom += bottom;
    }
    return TRUE;
}

BOOL ClientToScreen(HWND hWnd, LPPOINT point)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec || !point)
        return FALSE;
    int x = 0;
    int y = 0;
    WindowOrigin(rec, x, y);
    point->x += x;
    point->y += y;
    return TRUE;
}

BOOL ScreenToClient(HWND hWnd, LPPOINT point)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec || !point)
        return FALSE;
    int x = 0;
    int y = 0;
    WindowOrigin(rec, x, y);
    point->x -= x;
    point->y -= y;
    return TRUE;
}

HWND GetFocus()
{
    WindowRecord* rec = S().current;
    if (rec && (SDL_GetWindowFlags(rec->window) & SDL_WINDOW_INPUT_FOCUS))
        return ToHwnd(rec);
    return NULL;
}

int GetSystemMetrics(int index)
{
    switch (index) {
    case SM_CXSCREEN:
    case SM_CXFULLSCREEN:
        return platform::present::ScreenSize().cx;
    case SM_CYSCREEN:
    case SM_CYFULLSCREEN:
        return platform::present::ScreenSize().cy;
    case SM_CXDOUBLECLK:
    case SM_CYDOUBLECLK:
        return 4;
    case SM_SWAPBUTTON:
    default:
        return 0;
    }
}

BOOL PeekMessageA(LPMSG msg, HWND hWnd, UINT filterMin, UINT filterMax, UINT remove)
{
    if (!msg)
        return FALSE;
    platform::window::PumpEvents();
    return Fetch(msg, hWnd, filterMin, filterMax, (remove & PM_REMOVE) != 0) ? TRUE : FALSE;
}

BOOL GetMessageA(LPMSG msg, HWND hWnd, UINT filterMin, UINT filterMax)
{
    if (!msg)
        return FALSE;
    for (;;) {
        platform::window::PumpEvents();
        if (Fetch(msg, hWnd, filterMin, filterMax, true))
            return msg->message == WM_QUIT ? FALSE : TRUE;
        if (SDL_WasInit(SDL_INIT_EVENTS))
            SDL_WaitEventTimeout(nullptr, 10);
        else
            Sleep(10);
    }
}

BOOL TranslateMessage(const MSG*) { return FALSE; }

LRESULT DispatchMessageA(const MSG* msg)
{
    if (!msg)
        return 0;
    WindowRecord* rec = Rec(msg->hwnd);
    if (!rec || !rec->proc)
        return 0;
    return rec->proc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

BOOL PostMessageA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return Enqueue(hWnd, message, wParam, lParam) ? TRUE : FALSE;
}

LRESULT SendMessageA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WindowRecord* rec = Rec(hWnd);
    if (!rec || !rec->proc)
        return 0;
    return rec->proc(hWnd, message, wParam, lParam);
}

void PostQuitMessage(int exitCode) { Enqueue(NULL, WM_QUIT, static_cast<WPARAM>(exitCode), 0); }

LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }

UINT RegisterWindowMessageA(LPCSTR name)
{
    if (!name || !*name)
        return 0;
    State& s = S();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto inserted = s.messageIds.emplace(Lower(name), s.nextMessageId);
    if (inserted.second)
        s.nextMessageId++;
    return inserted.first->second;
}

BOOL GetUpdateRect(HWND, LPRECT, BOOL) { return FALSE; }

HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT paint)
{
    if (paint) {
        *paint = PAINTSTRUCT();
        GetClientRect(hWnd, &paint->rcPaint);
    }
    return NULL;
}

BOOL EndPaint(HWND, const PAINTSTRUCT*) { return TRUE; }

HCURSOR SetCursor(HCURSOR cursor)
{
    State& s = S();
    HCURSOR previous = s.cursor;
    s.cursor = cursor;
    if (VideoUp()) {
        // The engine draws its own pointer and passes NULL to hide the
        // system one; the arrow brings it back.
        if (cursor == NULL)
            SDL_HideCursor();
        else if (s.cursorCount >= 0)
            SDL_ShowCursor();
    }
    return previous;
}

HCURSOR LoadCursorA(HINSTANCE, LPCSTR name)
{
    return name == IDC_ARROW ? reinterpret_cast<HCURSOR>(&g_arrowCursor) : NULL;
}

BOOL GetCursorPos(LPPOINT point)
{
    if (!point)
        return FALSE;
    point->x = 0;
    point->y = 0;
    if (!VideoUp())
        return TRUE;
    float fx = 0.0f;
    float fy = 0.0f;
    if (Headless()) {
        // No global mouse here: window-relative plus the window position.
        SDL_GetMouseState(&fx, &fy);
        int wx = 0;
        int wy = 0;
        WindowOrigin(S().current, wx, wy);
        fx += static_cast<float>(wx);
        fy += static_cast<float>(wy);
    } else {
        SDL_GetGlobalMouseState(&fx, &fy);
    }
    point->x = static_cast<LONG>(fx);
    point->y = static_cast<LONG>(fy);
    return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
    if (!VideoUp())
        return FALSE;
    WindowRecord* rec = S().current;
    if (!Headless() && SDL_WarpMouseGlobal(static_cast<float>(x), static_cast<float>(y)))
        return TRUE;
    if (!rec)
        return FALSE;
    int wx = 0;
    int wy = 0;
    WindowOrigin(rec, wx, wy);
    SDL_WarpMouseInWindow(rec->window, static_cast<float>(x - wx), static_cast<float>(y - wy));
    return TRUE;
}

int ShowCursor(BOOL show)
{
    State& s = S();
    s.cursorCount += show ? 1 : -1;
    if (VideoUp()) {
        if (s.cursorCount >= 0)
            SDL_ShowCursor();
        else
            SDL_HideCursor();
    }
    return s.cursorCount;
}

SHORT GetAsyncKeyState(int vk)
{
    if (!VideoUp())
        return 0;
    const SHORT down = -32768; // 0x8000: the key is down now.

    SDL_MouseButtonFlags mask = 0;
    if (vk == VK_LBUTTON)
        mask = SDL_BUTTON_LMASK;
    else if (vk == VK_RBUTTON)
        mask = SDL_BUTTON_RMASK;
    else if (vk == VK_MBUTTON)
        mask = SDL_BUTTON_MMASK;
    if (mask != 0)
        return (SDL_GetMouseState(nullptr, nullptr) & mask) ? down : 0;

    int count = 0;
    const bool* keys = SDL_GetKeyboardState(&count);
    auto pressed = [&](int sc) { return keys && sc > 0 && sc < count && keys[sc]; };
    bool isDown = false;
    switch (vk) {
    case VK_SHIFT:
        isDown = pressed(SDL_SCANCODE_LSHIFT) || pressed(SDL_SCANCODE_RSHIFT);
        break;
    case VK_CONTROL:
        isDown = pressed(SDL_SCANCODE_LCTRL) || pressed(SDL_SCANCODE_RCTRL);
        break;
    case VK_MENU:
        isDown = pressed(SDL_SCANCODE_LALT) || pressed(SDL_SCANCODE_RALT);
        break;
    case VK_RETURN:
        isDown = pressed(SDL_SCANCODE_RETURN) || pressed(SDL_SCANCODE_KP_ENTER);
        break;
    default:
        isDown = pressed(platform::window::ScancodeForVK(vk));
        break;
    }
    return isDown ? down : 0;
}

HKL GetKeyboardLayout(DWORD) { return reinterpret_cast<HKL>(static_cast<uintptr_t>(0x04090409)); }

int MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type)
{
    SDL_MessageBoxButtonData buttons[3] = {};
    int count = 0;
    auto add = [&](int id, const char* label) {
        buttons[count].buttonID = id;
        buttons[count].text = label;
        count++;
    };
    switch (type & 0x0Fu) {
    case MB_OKCANCEL:
        add(IDOK, "OK");
        add(IDCANCEL, "Cancel");
        break;
    case MB_YESNO:
        add(IDYES, "Yes");
        add(IDNO, "No");
        break;
    case MB_YESNOCANCEL:
        add(IDYES, "Yes");
        add(IDNO, "No");
        add(IDCANCEL, "Cancel");
        break;
    default:
        add(IDOK, "OK");
        break;
    }
    const int defaultId = buttons[0].buttonID;
    const int escapeId = buttons[count - 1].buttonID;
    if (Headless())
        return defaultId;
    buttons[0].flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[count - 1].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

    SDL_MessageBoxData box = {};
    switch (type & 0xF0u) {
    case MB_ICONERROR:
        box.flags = SDL_MESSAGEBOX_ERROR;
        break;
    case MB_ICONWARNING:
        box.flags = SDL_MESSAGEBOX_WARNING;
        break;
    default:
        box.flags = SDL_MESSAGEBOX_INFORMATION;
        break;
    }
    WindowRecord* rec = Rec(hWnd);
    box.window = rec ? rec->window : nullptr;
    box.title = caption ? caption : "Error";
    box.message = text ? text : "";
    box.numbuttons = count;
    box.buttons = buttons;
    int chosen = -1;
    if (!SDL_ShowMessageBox(&box, &chosen))
        return IDOK;
    return chosen < 0 ? escapeId : chosen;
}
