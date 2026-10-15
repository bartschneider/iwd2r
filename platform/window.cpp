// The engine's one popup window, its message queue and polled input, over
// SDL3. SDL events become WM_* messages in a Win32-shaped queue that the
// engine's PeekMessageA/GetMessageA loop drains and dispatches to its own
// window procedure.
#define NOMINMAX
#include "platform/window.h"
#include "platform/crash.h"

#include "platform/data_dir.h"
#include "platform/input_settings.h"
#include "platform/present.h"
#include "platform/replay.h"
#include "platform/script.h"
#include "platform/text_layer.h"
#include "platform/win32_api.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>

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
    int scriptFrame = 0; // PresentedFrames() at the script's last step
};

State& S()
{
    static State state;
    return state;
}

char g_arrowCursor;

// Win32's "pressed since the last GetAsyncKeyState call" bit, per virtual
// key: set by the pump on key and mouse-button presses (main thread),
// cleared by GetAsyncKeyState when it reports it (the engine's input
// thread). A tap released before the next poll is still seen once.
std::array<std::atomic<bool>, 256> g_pressedSincePoll;

void MarkPressed(int vk)
{
    if (vk > 0 && vk < static_cast<int>(g_pressedSincePoll.size()))
        g_pressedSincePoll[static_cast<size_t>(vk)].store(true);
}

bool TakePressed(int vk)
{
    if (vk <= 0 || vk >= static_cast<int>(g_pressedSincePoll.size()))
        return false;
    return g_pressedSincePoll[static_cast<size_t>(vk)].exchange(false);
}

// Marks the generic key and, for modifiers, the sided one too.
void MarkKeyPressed(SDL_Scancode sc)
{
    MarkPressed(platform::window::VKForScancode(sc));
    switch (sc) {
    case SDL_SCANCODE_LSHIFT:
        MarkPressed(VK_LSHIFT);
        break;
    case SDL_SCANCODE_RSHIFT:
        MarkPressed(VK_RSHIFT);
        break;
    case SDL_SCANCODE_LCTRL:
        MarkPressed(VK_LCONTROL);
        break;
    case SDL_SCANCODE_RCTRL:
        MarkPressed(VK_RCONTROL);
        break;
    case SDL_SCANCODE_LALT:
        MarkPressed(VK_LMENU);
        break;
    case SDL_SCANCODE_RALT:
        MarkPressed(VK_RMENU);
        break;
    default:
        break;
    }
}

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

// Desktop pixels per engine pixel (Program Options/Scale). The engine sees
// the desktop, its window and its client area in engine pixels: window
// sizes and positions it sets are multiplied by this, those it reads
// divided.
int Scale() { return platform::present::WindowScale(); }
int ToEngine(int desktopPixels) { return desktopPixels / Scale(); }
int ToDesktop(int enginePixels) { return enginePixels * Scale(); }

// UI scaling (InitScreen's uiScale): the UI panels' scale and where the
// regions for the current screen come from.
struct RegionState {
    int uiScale = 1;
    platform::window::RegionSource source = nullptr;
};

RegionState& UiRegions()
{
    static RegionState state;
    return state;
}

bool SameRect(const RECT& a, const RECT& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

bool SameRegions(const std::vector<platform::compose::Region>& a, const std::vector<platform::compose::Region>& b)
{
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); i++) {
        if (!SameRect(a[i].frame, b[i].frame) || !SameRect(a[i].window, b[i].window) || a[i].scale != b[i].scale
            || a[i].ui != b[i].ui)
            return false;
    }
    return true;
}

// The dummy and offscreen drivers have no real desktop: no global mouse, no
// message boxes.
bool Headless()
{
    const char* driver = VideoUp() ? SDL_GetCurrentVideoDriver() : SDL_GetHint(SDL_HINT_VIDEO_DRIVER);
    return driver && (SDL_strcasecmp(driver, "dummy") == 0 || SDL_strcasecmp(driver, "offscreen") == 0);
}

// The primary display's desktop size (the dummy driver reports a fixed
// 1024x768 one). Only the window's placement, SM_CXFULLSCREEN, the work
// area and WM_DISPLAYCHANGE use it: the engine's screen coordinates are the
// virtual screen's (present::ScreenSize).
void RefreshDesktopSize()
{
    if (!VideoUp())
        return;
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (mode && mode->w > 0 && mode->h > 0)
        platform::present::SetDesktopSize(mode->w, mode->h);
}

// Clamps one window coordinate so a size-long window starting there lies
// within [origin, origin + extent), or at origin when it cannot fit.
int ClampToDesktop(int pos, int size, int origin, int extent)
{
    if (size >= extent || pos < origin)
        return origin;
    return std::min(pos, origin + extent - size);
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

// Display events only set displayChanged; the pump coalesces them into one
// WM_DISPLAYCHANGE.
// In the replay mode (platform/replay.h) focus and real input never reach
// the engine, as the replay recorder's focus guard and message hooks keep
// them from the original.
bool ReplayDrops(const SDL_Event& e)
{
    switch (e.type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_KEY_DOWN:
        return true;
    default:
        return false;
    }
}

void Translate(const SDL_Event& e, bool& displayChanged)
{
    using platform::window::EnqueueMessage;
    if (platform::replay::Active() && ReplayDrops(e))
        return;
    WindowRecord* rec = S().current;
    const SDL_WindowID id = rec ? SDL_GetWindowID(rec->window) : 0;

    if (e.type == SDL_EVENT_QUIT) {
        EnqueueMessage(WM_CLOSE, 0, 0);
        return;
    }
    if (e.type >= SDL_EVENT_DISPLAY_FIRST && e.type <= SDL_EVENT_DISPLAY_LAST) {
        displayChanged = true;
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
            platform::window::RefreshRegions();
            EnqueueMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(ToEngine(e.window.data1), ToEngine(e.window.data2)));
            break;
        case SDL_EVENT_WINDOW_MOVED:
            EnqueueMessage(WM_MOVE, 0, MAKELONG(ToEngine(e.window.data1), ToEngine(e.window.data2)));
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
            platform::window::RefreshRegions();
            EnqueueMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(ToEngine(w), ToEngine(h)));
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
        // Screen coordinates are the engine's frame coordinates.
        float fx = 0.0f;
        float fy = 0.0f;
        platform::present::WindowToFrame(e.wheel.mouse_x, e.wheel.mouse_y, &fx, &fy);
        const int px = static_cast<int>(fx);
        const int py = static_cast<int>(fy);
        EnqueueMessage(WM_MOUSEWHEEL, static_cast<WPARAM>(static_cast<DWORD>(static_cast<WORD>(delta)) << 16), MAKELONG(px, py));
        return;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (e.button.button == SDL_BUTTON_LEFT)
            MarkPressed(VK_LBUTTON);
        else if (e.button.button == SDL_BUTTON_RIGHT)
            MarkPressed(VK_RBUTTON);
        else if (e.button.button == SDL_BUTTON_MIDDLE)
            MarkPressed(VK_MBUTTON);
        return;
    }
    if (e.type == SDL_EVENT_KEY_DOWN) {
        // Ctrl+Shift+R flips the presenter between gpu and classic; not a
        // key press for the engine.
        if (e.key.scancode == SDL_SCANCODE_R && (e.key.mod & SDL_KMOD_CTRL) != 0 && (e.key.mod & SDL_KMOD_SHIFT) != 0) {
            if (!e.key.repeat)
                std::fprintf(stderr, "presenter: %s\n", platform::present::BackendKindName(platform::present::ToggleBackend()));
            return;
        }
        // Ctrl+Shift+T switches the text between the bitmap fonts and the
        // scalable faces (platform/text_layer.h); not a key press either.
        if (e.key.scancode == SDL_SCANCODE_T && (e.key.mod & SDL_KMOD_CTRL) != 0 && (e.key.mod & SDL_KMOD_SHIFT) != 0) {
            if (!e.key.repeat)
                std::fprintf(stderr, "text: %s\n", platform::text::Toggle() ? "scalable" : "classic");
            return;
        }
        // Auto-repeat is not a new press: the engine times its own repeat.
        if (!e.key.repeat)
            MarkKeyPressed(e.key.scancode);
        // Left Alt only: AltGr arrives as right Alt (plus Ctrl) and types text.
        if (!rec || e.key.windowID != id || (e.key.mod & SDL_KMOD_LALT) == 0)
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

void PumpEvents()
{
    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        bool displayChanged = false;
        SDL_Event e;
        while (SDL_PollEvent(&e))
            Translate(e, displayChanged);
        if (displayChanged) {
            RefreshDesktopSize();
            RefreshRegions();
            const SIZE desktop = platform::present::DesktopSize();
            EnqueueMessage(WM_DISPLAYCHANGE, 32, MAKELONG(ToEngine(desktop.cx), ToEngine(desktop.cy)));
        }
    }
    // The engine switches screens (world, full UI) and the world screen its
    // view size between pumps; the regions follow before the next present.
    RefreshRegions();
    platform::present::PresentIfDirty();
    CheckSmoke();
    // The control channel and the script run once per presented frame (the
    // replay mode advances the script from its own frame instead).
    const int frames = platform::present::PresentedFrames();
    if (S().scriptFrame < frames)
        platform::script::PollControl();
    if (platform::replay::Active())
        S().scriptFrame = frames;
    while (S().scriptFrame < frames) {
        S().scriptFrame++;
        platform::script::Advance();
    }
}

void EnqueueMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    Enqueue(MainWindow(), message, wParam, lParam);
}

bool NativeDesktopSize(int* width, int* height)
{
    *width = 0;
    *height = 0;
    if (!VideoUp() && !SDL_Init(SDL_INIT_VIDEO))
        return false;
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode || mode->w <= 0 || mode->h <= 0)
        return false;
    *width = mode->w;
    *height = mode->h;
    return true;
}

bool InitScreen(int width, int height, int scale, int uiScale)
{
    platform::present::SetScreenSize(width, height);
    platform::present::SetWindowScale(scale);
    UiRegions().uiScale = uiScale >= 1 ? uiScale : platform::present::WindowScale();
    RefreshRegions();
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    if (!VideoUp() && !SDL_Init(SDL_INIT_VIDEO))
        return false;
    RefreshDesktopSize();
    return true;
}

void SetRegionSource(RegionSource source)
{
    UiRegions().source = source;
    if (source) {
        RefreshRegions();
    } else if (!platform::present::Regions()->empty()) {
        // The source's owner is going away: back to the whole frame.
        platform::present::SetRegions({});
    }
}

void RefreshRegions()
{
    const RegionState& r = UiRegions();
    const int worldScale = platform::present::WindowScale();
    if (r.uiScale == worldScale) {
        // Equal scales: the presenter's own path, untouched.
        if (!platform::present::Regions()->empty())
            platform::present::SetRegions({});
        return;
    }
    // The window in pixels: the live window's client area (not while
    // minimised, when it has none), else the virtual screen at the world
    // scale, which is what CreateWindowExA will make.
    const SIZE screen = platform::present::ScreenSize();
    SIZE window = { screen.cx * worldScale, screen.cy * worldScale };
    WindowRecord* rec = S().current;
    if (rec && VideoUp()) {
        if (rec->minimized || (SDL_GetWindowFlags(rec->window) & SDL_WINDOW_MINIMIZED))
            return;
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(rec->window, &w, &h);
        if (w > 0 && h > 0) {
            window.cx = w;
            window.cy = h;
        }
    }
    std::vector<platform::compose::Region> regions;
    if (r.source) {
        regions = r.source(window);
    } else {
        regions = platform::compose::Layout(screen, window, r.uiScale, worldScale, {}, nullptr);
    }
    if (!SameRegions(regions, *platform::present::Regions()))
        platform::present::SetRegions(std::move(regions));
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
        // MessageBoxA writes the line to stderr itself.
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

    // SDL would otherwise add SDL_EVENT_QUIT to the last window's close
    // request, and the engine would see two WM_CLOSE (two confirmations).
    SDL_SetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE, "0");
    if (!VideoUp() && !SDL_Init(SDL_INIT_VIDEO))
        return NULL;

    RefreshDesktopSize();
    if (width == CW_USEDEFAULT)
        width = 640;
    if (height == CW_USEDEFAULT)
        height = 480;
    width = ToDesktop(width);
    height = ToDesktop(height);
    // The engine asks for a SM_CXSCREEN x SM_CYSCREEN popup: the virtual
    // screen, game-sized. It stays on the desktop; the origin (where the
    // engine's own clamp lands when the game fills the desktop) and
    // CW_USEDEFAULT centre it.
    const bool centred = x == CW_USEDEFAULT || y == CW_USEDEFAULT || (x == 0 && y == 0);
    if (!centred) {
        x = ToDesktop(x);
        y = ToDesktop(y);
        SDL_Rect bounds = {};
        if (SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &bounds) && bounds.w > 0 && bounds.h > 0) {
            x = ClampToDesktop(x, width, bounds.x, bounds.w);
            y = ClampToDesktop(y, height, bounds.y, bounds.h);
        }
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title ? title : "");
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, centred ? SDL_WINDOWPOS_CENTERED : x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, centred ? SDL_WINDOWPOS_CENTERED : y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
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

    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.current = rec;
        // A new window (Alt+Enter recreates it) revives the loop.
        s.quitting = false;
    }
    // Win32 sends WM_SIZE and WM_MOVE while creating a window; the engine
    // takes its client rectangle (the windowed blit target and the pointer
    // bounds) from them. SDL's dummy driver reports neither.
    {
        int cw = 0;
        int ch = 0;
        int wx = 0;
        int wy = 0;
        SDL_GetWindowSize(window, &cw, &ch);
        SDL_GetWindowPosition(window, &wx, &wy);
        platform::window::EnqueueMessage(WM_SIZE, SIZE_RESTORED, MAKELONG(ToEngine(cw), ToEngine(ch)));
        platform::window::EnqueueMessage(WM_MOVE, 0, MAKELONG(ToEngine(wx), ToEngine(wy)));
    }
    platform::window::RefreshRegions();

    if (!s.smokeEnvRead) {
        s.smokeEnvRead = true;
        if (const char* frames = std::getenv("IWD2R_SMOKE_FRAMES"))
            platform::window::SetSmokeFrames(std::atoi(frames));
        S().scriptFrame = platform::present::PresentedFrames();
        platform::script::StartFromEnvironment();
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
        SDL_SetWindowPosition(rec->window, ToDesktop(x), ToDesktop(y));
    if ((flags & SWP_NOSIZE) == 0)
        SDL_SetWindowSize(rec->window, ToDesktop(cx), ToDesktop(cy));
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
    SetRect(rect, 0, 0, ToEngine(w), ToEngine(h));
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
    SetRect(rect, ToEngine(rect->left), ToEngine(rect->top), ToEngine(rect->right), ToEngine(rect->bottom));
    return TRUE;
}

// The client area is the engine's whole screen (the virtual screen windowed,
// the display mode fullscreen), so client and screen coordinates coincide.
// Only GetWindowRect, SetWindowPos and CreateWindowExA's position are in
// desktop coordinates.
BOOL ClientToScreen(HWND hWnd, LPPOINT point) { return Rec(hWnd) && point ? TRUE : FALSE; }

BOOL ScreenToClient(HWND hWnd, LPPOINT point) { return Rec(hWnd) && point ? TRUE : FALSE; }

HWND GetFocus()
{
    WindowRecord* rec = S().current;
    if (rec && (SDL_GetWindowFlags(rec->window) & SDL_WINDOW_INPUT_FOCUS))
        return ToHwnd(rec);
    return NULL;
}

int GetSystemMetrics(int index)
{
    // SM_CXSCREEN is the engine's screen (the window it creates is that
    // size); SM_CXFULLSCREEN stays the real desktop, which the engine
    // compares the game resolution against to allow windowed mode and to
    // place the window.
    switch (index) {
    case SM_CXSCREEN:
        return platform::present::ScreenSize().cx;
    case SM_CYSCREEN:
        return platform::present::ScreenSize().cy;
    case SM_CXFULLSCREEN:
        RefreshDesktopSize();
        return ToEngine(platform::present::DesktopSize().cx);
    case SM_CYFULLSCREEN:
        RefreshDesktopSize();
        return ToEngine(platform::present::DesktopSize().cy);
    case SM_CXDOUBLECLK:
        return platform::input_settings::kDoubleClickWidth;
    case SM_CYDOUBLECLK:
        return platform::input_settings::kDoubleClickHeight;
    case SM_SWAPBUTTON:
        return platform::input_settings::kSwapButton;
    default:
        return 0;
    }
}

BOOL PeekMessageA(LPMSG msg, HWND hWnd, UINT filterMin, UINT filterMax, UINT remove)
{
    if (!msg)
        return FALSE;
    platform::window::PumpEvents();
    if (Fetch(msg, hWnd, filterMin, filterMax, (remove & PM_REMOVE) != 0))
        return TRUE;
    // The engine's idle loop polls this back to back; give the core away.
    std::this_thread::yield();
    return FALSE;
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

// The pointer in the engine's screen coordinates: the window-relative mouse
// mapped onto the presented frame, clamped to it (or to the client area
// before the first frame) so the engine's edge scrolling still sees the
// edges when the pointer leaves the window or sits in a letterbox bar.
BOOL GetCursorPos(LPPOINT point)
{
    if (!point)
        return FALSE;
    point->x = 0;
    point->y = 0;
    if (platform::replay::Active()) {
        *point = platform::replay::Cursor();
        return TRUE;
    }
    // A running script's cursor is already in the engine's coordinates.
    if (platform::script::InjectedCursor(point))
        return TRUE;
    WindowRecord* rec = S().current;
    if (!VideoUp() || !rec)
        return TRUE;
    float wx = 0.0f;
    float wy = 0.0f;
    SDL_GetMouseState(&wx, &wy);
    float fx = 0.0f;
    float fy = 0.0f;
    platform::present::WindowToFrame(wx, wy, &fx, &fy);
    int w = 0;
    int h = 0;
    if (!platform::present::FrameSize(&w, &h)) {
        SDL_GetWindowSize(rec->window, &w, &h);
        w = ToEngine(w);
        h = ToEngine(h);
    }
    point->x = std::clamp(static_cast<LONG>(std::floor(fx)), LONG(0), static_cast<LONG>(std::max(w - 1, 0)));
    point->y = std::clamp(static_cast<LONG>(std::floor(fy)), LONG(0), static_cast<LONG>(std::max(h - 1, 0)));
    return TRUE;
}

// Warps to a point in the engine's screen coordinates, within the window.
BOOL SetCursorPos(int x, int y)
{
    if (platform::replay::Active()) {
        platform::replay::SetCursor(x, y);
        return TRUE;
    }
    WindowRecord* rec = S().current;
    if (!VideoUp() || !rec)
        return FALSE;
    float wx = 0.0f;
    float wy = 0.0f;
    platform::present::FrameToWindow(static_cast<float>(x), static_cast<float>(y), &wx, &wy);
    SDL_WarpMouseInWindow(rec->window, wx, wy);
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
    if (platform::replay::Active())
        return platform::replay::Key(vk);
    SHORT injected = 0;
    if (platform::script::InjectedKey(vk, &injected))
        return injected;
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
    // Bit 0: pressed since the last call for this key, as Win32 reports it.
    // The engine treats a state with it as a new press and 0x8000 alone as
    // a held key.
    const SHORT since = TakePressed(vk) ? SHORT(1) : SHORT(0);
    if (mask != 0)
        return static_cast<SHORT>(((SDL_GetMouseState(nullptr, nullptr) & mask) ? down : 0) | since);

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
    return static_cast<SHORT>((isDown ? down : 0) | since);
}

HKL GetKeyboardLayout(DWORD) { return reinterpret_cast<HKL>(static_cast<uintptr_t>(0x04090409)); }

int MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type)
{
    // First, whatever happens next: headless, a box shows nothing, and the
    // text is what the log needs.
    {
        char line[2048];
        platform::crash::FormatMessageBoxLine(caption, text, line, sizeof(line));
        std::fputs(line, stderr);
        std::fflush(stderr);
    }
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
    // Headless, and always in the replay mode (as the recorder answers the
    // original's), the first button without showing anything.
    // Likewise under a script or a state dump: nobody is there to click.
    const char* script = std::getenv("IWD2R_SCRIPT");
    const char* dump = std::getenv("IWD2R_STATE_DUMP");
    if (Headless() || platform::replay::Active() || (script != nullptr && *script != '\0') || (dump != nullptr && *dump != '\0'))
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

BOOL CloseWindow(HWND hWnd)
{
    if (!Rec(hWnd))
        return FALSE;
    ShowWindow(hWnd, SW_MINIMIZE);
    return TRUE;
}

BOOL SystemParametersInfoA(UINT action, UINT, LPVOID value, UINT)
{
    switch (action) {
    case SPI_GETKEYBOARDDELAY:
    case SPI_GETKEYBOARDSPEED:
    case SPI_GETWHEELSCROLLLINES:
    case SPI_GETSCREENSAVEACTIVE:
    case SPI_GETWORKAREA:
        if (!value)
            return FALSE;
        break;
    default:
        break;
    }
    switch (action) {
    case SPI_GETKEYBOARDDELAY:
        *static_cast<int*>(value) = platform::input_settings::kKeyboardDelay;
        return TRUE;
    case SPI_GETKEYBOARDSPEED:
        *static_cast<int*>(value) = platform::input_settings::kKeyboardSpeed;
        return TRUE;
    case SPI_GETWHEELSCROLLLINES:
        *static_cast<UINT*>(value) = platform::input_settings::kWheelScrollLines;
        return TRUE;
    case SPI_GETSCREENSAVEACTIVE:
        *static_cast<BOOL*>(value) = TRUE;
        return TRUE;
    case SPI_SETSCREENSAVEACTIVE:
        return TRUE;
    case SPI_GETWORKAREA: {
        RECT* work = static_cast<RECT*>(value);
        work->left = 0;
        work->top = 0;
        work->right = GetSystemMetrics(SM_CXFULLSCREEN);
        work->bottom = GetSystemMetrics(SM_CYFULLSCREEN);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

UINT GetDoubleClickTime() { return platform::input_settings::kDoubleClickTime; }

HWND FindWindowA(LPCSTR, LPCSTR) { return NULL; }
