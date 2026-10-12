// iwd2r. Distributed under LICENSE.md.
#define NOMINMAX
#include <catch2/catch_test_macros.hpp>

#include "platform/present.h"
#include "platform/win32_api.h"
#include "platform/window.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace {

struct Received {
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
};

std::vector<Received>& Log()
{
    static std::vector<Received> log;
    return log;
}

LRESULT CALLBACK RecordingProc(HWND, UINT message, WPARAM wParam, LPARAM lParam)
{
    Log().push_back({ message, wParam, lParam });
    return 42;
}

// Removes every queued message, stopping at the WM_QUIT a destroyed window
// reports forever.
void Drain()
{
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE) && m.message != WM_QUIT) {
    }
}

struct TestWindow {
    HWND hwnd = NULL;

    TestWindow()
    {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = RecordingProc;
        wc.lpszClassName = "C";
        REQUIRE(RegisterClassExA(&wc) != 0);
        hwnd = CreateWindowExA(0, "C", "t", WS_POPUP, 0, 0, 320, 200, NULL, NULL, NULL, NULL);
        REQUIRE(hwnd != NULL);
        Drain();
        Log().clear();
    }

    ~TestWindow()
    {
        if (platform::window::MainWindow() == hwnd)
            DestroyWindow(hwnd);
        Drain();
        Log().clear();
    }

    SDL_Window* Sdl() const
    {
        // The dummy driver hands out ids from 1 and only this test window is
        // alive, so the newest window is the main one.
        int count = 0;
        SDL_Window** windows = SDL_GetWindows(&count);
        SDL_Window* last = count > 0 ? windows[count - 1] : nullptr;
        SDL_free(windows);
        return last;
    }

    void Push(SDL_EventType type, Sint32 data1 = 0, Sint32 data2 = 0) const
    {
        SDL_Event e = {};
        e.type = type;
        e.window.windowID = SDL_GetWindowID(Sdl());
        e.window.data1 = data1;
        e.window.data2 = data2;
        REQUIRE(SDL_PushEvent(&e));
    }
};

bool NextMessage(MSG& m) { return PeekMessageA(&m, NULL, 0, 0, PM_REMOVE) != FALSE; }

}

TEST_CASE("window class registration creates a window with the requested client size", "[window]")
{
    TestWindow w;
    RECT r;
    REQUIRE(GetClientRect(w.hwnd, &r));
    CHECK(r.left == 0);
    CHECK(r.top == 0);
    CHECK(r.right == 320);
    CHECK(r.bottom == 200);
    CHECK(platform::window::MainWindow() == w.hwnd);
}

TEST_CASE("window queue peeks without removing then removes", "[window]")
{
    TestWindow w;
    REQUIRE(PostMessageA(w.hwnd, WM_USER + 1, 7, 8));

    MSG m = {};
    REQUIRE(PeekMessageA(&m, NULL, 0, 0, PM_NOREMOVE));
    CHECK(m.message == WM_USER + 1);
    CHECK(m.hwnd == w.hwnd);
    CHECK(m.wParam == 7);
    CHECK(m.lParam == 8);

    MSG again = {};
    REQUIRE(PeekMessageA(&again, NULL, 0, 0, PM_REMOVE));
    CHECK(again.message == WM_USER + 1);
    CHECK(again.wParam == 7);

    MSG none = {};
    CHECK_FALSE(PeekMessageA(&none, NULL, 0, 0, PM_REMOVE));
}

TEST_CASE("window DispatchMessageA calls the registered procedure", "[window]")
{
    TestWindow w;
    REQUIRE(PostMessageA(w.hwnd, WM_USER + 2, 11, 12));
    MSG m = {};
    REQUIRE(GetMessageA(&m, NULL, 0, 0));
    CHECK(TranslateMessage(&m) == FALSE);
    CHECK(DispatchMessageA(&m) == 42);
    REQUIRE(Log().size() == 1);
    CHECK(Log()[0].message == WM_USER + 2);
    CHECK(Log()[0].wParam == 11);
    CHECK(Log()[0].lParam == 12);

    CHECK(SendMessageA(w.hwnd, WM_USER + 3, 1, 2) == 42);
    REQUIRE(Log().size() == 2);
    CHECK(Log()[1].message == WM_USER + 3);
    CHECK(SendMessageA(NULL, WM_USER + 3, 1, 2) == 0);
    CHECK(DefWindowProcA(w.hwnd, WM_USER, 0, 0) == 0);
}

TEST_CASE("window DestroyWindow delivers WM_DESTROY synchronously and ends the loop", "[window]")
{
    TestWindow w;
    REQUIRE(DestroyWindow(w.hwnd));
    REQUIRE(Log().size() == 1);
    CHECK(Log()[0].message == WM_DESTROY);
    CHECK(platform::window::MainWindow() == NULL);

    MSG m = {};
    CHECK(GetMessageA(&m, NULL, 0, 0) == FALSE);
    CHECK(m.message == WM_QUIT);
    REQUIRE(PeekMessageA(&m, NULL, 0, 0, PM_NOREMOVE));
    CHECK(m.message == WM_QUIT);
}

TEST_CASE("window PostQuitMessage makes GetMessageA return FALSE", "[window]")
{
    TestWindow w;
    PostQuitMessage(3);
    MSG m = {};
    CHECK(GetMessageA(&m, NULL, 0, 0) == FALSE);
    CHECK(m.message == WM_QUIT);
    CHECK(m.wParam == 3);
}

TEST_CASE("window DestroyWindow clears the present target first", "[window]")
{
    std::vector<uint32_t> pixels(64 * 32, 0x00FF00FFu);
    platform::present::SetPrimary(&pixels, 64, 32);
    {
        TestWindow w;
        platform::present::MarkPrimaryDirty();
        platform::present::PresentIfDirty();
        if (!SDL_GetRenderer(w.Sdl())) {
            platform::present::SetPrimary(nullptr, 0, 0);
            SKIP("dummy video driver has no software renderer: " << SDL_GetError());
        }
        const int before = platform::present::PresentedFrames();
        REQUIRE(DestroyWindow(w.hwnd));

        // SDL destroyed the renderer present had cached together with the
        // window. Were the window still targeted, presenting would hand SDL
        // that dead renderer and its texture, which SDL rejects with an error.
        SDL_ClearError();
        platform::present::MarkPrimaryDirty();
        platform::present::PresentIfDirty();
        CHECK(std::string(SDL_GetError()).empty());
        CHECK(platform::present::PresentedFrames() == before + 1);
    }
    platform::present::SetPrimary(nullptr, 0, 0);

    // Without a primary nothing is presented after the window is gone.
    const int frames = platform::present::PresentedFrames();
    platform::present::MarkPrimaryDirty();
    platform::present::PresentIfDirty();
    CHECK(platform::present::PresentedFrames() == frames);
}

TEST_CASE("window RegisterWindowMessageA hands out stable ids", "[window]")
{
    const UINT a = RegisterWindowMessageA("A");
    CHECK(a >= 0xC000);
    CHECK(RegisterWindowMessageA("A") == a);
    CHECK(RegisterWindowMessageA("B") != a);
    CHECK(RegisterWindowMessageA("B") >= 0xC000);
}

TEST_CASE("window ScancodeForVK maps the engine's virtual keys", "[window]")
{
    using platform::window::ScancodeForVK;
    using platform::window::VKForScancode;
    CHECK(ScancodeForVK(VK_ESCAPE) == SDL_SCANCODE_ESCAPE);
    CHECK(ScancodeForVK(VK_RETURN) == SDL_SCANCODE_RETURN);
    CHECK(ScancodeForVK(VK_OEM_1) == SDL_SCANCODE_SEMICOLON);
    CHECK(ScancodeForVK(VK_F1) == SDL_SCANCODE_F1);
    CHECK(ScancodeForVK('A') == SDL_SCANCODE_A);
    CHECK(ScancodeForVK('Z') == SDL_SCANCODE_Z);
    CHECK(ScancodeForVK('0') == SDL_SCANCODE_0);
    CHECK(ScancodeForVK(VK_NUMPAD0) == SDL_SCANCODE_KP_0);
    CHECK(ScancodeForVK(VK_LSHIFT) == SDL_SCANCODE_LSHIFT);
    CHECK(ScancodeForVK(VK_RMENU) == SDL_SCANCODE_RALT);
    CHECK(VKForScancode(SDL_SCANCODE_RSHIFT) == VK_SHIFT);
    CHECK(VKForScancode(SDL_SCANCODE_RCTRL) == VK_CONTROL);
    CHECK(VKForScancode(SDL_SCANCODE_LALT) == VK_MENU);

    // Every VK_* in src/ except the mouse buttons, which GetAsyncKeyState
    // reads from the mouse state.
    const int engineKeys[] = { VK_ADD, VK_BACK, VK_CONTROL, VK_DECIMAL, VK_DELETE, VK_DIVIDE, VK_DOWN, VK_END,
        VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12,
        VK_HOME, VK_INSERT, VK_LEFT, VK_MENU, VK_MULTIPLY, VK_NEXT, VK_NUMLOCK, VK_NUMPAD0, VK_NUMPAD1,
        VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9,
        VK_OEM_1, VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7, VK_OEM_8, VK_OEM_COMMA,
        VK_OEM_MINUS, VK_OEM_PERIOD, VK_OEM_PLUS, VK_PRIOR, VK_PROCESSKEY, VK_RETURN, VK_RIGHT, VK_SEPARATOR,
        VK_SHIFT, VK_SNAPSHOT, VK_SPACE, VK_SUBTRACT, VK_TAB, VK_UP };
    for (int vk : engineKeys) {
        INFO("vk 0x" << std::hex << vk);
        const int sc = ScancodeForVK(vk);
        CHECK(sc != SDL_SCANCODE_UNKNOWN);
        CHECK(VKForScancode(sc) == vk);
    }
    for (int vk = 'A'; vk <= 'Z'; vk++)
        CHECK(VKForScancode(ScancodeForVK(vk)) == vk);
    for (int vk = '0'; vk <= '9'; vk++)
        CHECK(VKForScancode(ScancodeForVK(vk)) == vk);
    CHECK(ScancodeForVK(VK_LBUTTON) == SDL_SCANCODE_UNKNOWN);
    CHECK(VKForScancode(SDL_SCANCODE_UNKNOWN) == 0);
}

TEST_CASE("window GetAsyncKeyState is zero with no key down", "[window]")
{
    CHECK(GetAsyncKeyState(VK_ESCAPE) == 0);
    TestWindow w;
    CHECK(GetAsyncKeyState(VK_ESCAPE) == 0);
    CHECK(GetAsyncKeyState(VK_SHIFT) == 0);
    CHECK(GetAsyncKeyState(VK_LBUTTON) == 0);
}

TEST_CASE("window ShowCursor keeps the Win32 display count", "[window]")
{
    TestWindow w;
    CHECK(ShowCursor(FALSE) == -1);
    CHECK(ShowCursor(TRUE) == 0);
    HCURSOR arrow = LoadCursorA(NULL, IDC_ARROW);
    CHECK(arrow != NULL);
    SetCursor(arrow);
    CHECK(SetCursor(NULL) == arrow);
    CHECK(GetKeyboardLayout(0) == reinterpret_cast<HKL>(static_cast<uintptr_t>(0x04090409)));
}

TEST_CASE("window geometry follows Win32", "[window]")
{
    TestWindow w;
    CHECK(GetSystemMetrics(SM_CXSCREEN) == platform::present::ScreenSize().cx);
    CHECK(GetSystemMetrics(SM_CYSCREEN) == platform::present::ScreenSize().cy);
    // The dummy driver has no real desktop: the window size stands in.
    CHECK(GetSystemMetrics(SM_CXSCREEN) == 320);
    CHECK(GetSystemMetrics(SM_CYSCREEN) == 200);

    REQUIRE(SetWindowPos(w.hwnd, HWND_TOP, 10, 20, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    RECT wr;
    REQUIRE(GetWindowRect(w.hwnd, &wr));
    CHECK(wr.left == 10);
    CHECK(wr.top == 20);
    CHECK(wr.right == 330);
    CHECK(wr.bottom == 220);

    POINT p = { 5, 6 };
    REQUIRE(ClientToScreen(w.hwnd, &p));
    CHECK(p.x == 15);
    CHECK(p.y == 26);
    REQUIRE(ScreenToClient(w.hwnd, &p));
    CHECK(p.x == 5);
    CHECK(p.y == 6);

    CHECK(GetUpdateRect(w.hwnd, NULL, FALSE) == FALSE);
    PAINTSTRUCT ps;
    BeginPaint(w.hwnd, &ps);
    CHECK(EndPaint(w.hwnd, &ps));
}

TEST_CASE("window pump translates SDL window events", "[window]")
{
    TestWindow w;
    MSG m = {};

    w.Push(SDL_EVENT_WINDOW_MINIMIZED);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_SIZE);
    CHECK(m.wParam == SIZE_MINIMIZED);
    RECT r;
    REQUIRE(GetClientRect(w.hwnd, &r));
    CHECK(IsRectEmpty(&r));

    w.Push(SDL_EVENT_WINDOW_RESTORED);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_SIZE);
    CHECK(m.wParam == SIZE_RESTORED);
    REQUIRE(GetClientRect(w.hwnd, &r));
    CHECK(r.right == 320);

    w.Push(SDL_EVENT_WINDOW_FOCUS_LOST);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_ACTIVATEAPP);
    CHECK(m.wParam == 0);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_KILLFOCUS);

    w.Push(SDL_EVENT_WINDOW_FOCUS_GAINED);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_ACTIVATEAPP);
    CHECK(m.wParam == 1);

    w.Push(SDL_EVENT_WINDOW_MOVED, 30, 40);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_MOVE);
    CHECK(m.lParam == MAKELONG(30, 40));

    w.Push(SDL_EVENT_WINDOW_RESIZED, 300, 100);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_SIZE);
    CHECK(m.wParam == SIZE_RESTORED);
    CHECK(m.lParam == MAKELONG(300, 100));

    w.Push(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_CLOSE);
    CHECK(m.hwnd == w.hwnd);

    SDL_Event wheel = {};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.windowID = SDL_GetWindowID(w.Sdl());
    wheel.wheel.y = -1.0f;
    wheel.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    REQUIRE(SDL_PushEvent(&wheel));
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_MOUSEWHEEL);
    CHECK(static_cast<SHORT>(HIWORD(m.wParam)) == -120);

    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = SDL_GetWindowID(w.Sdl());
    key.key.scancode = SDL_SCANCODE_RETURN;
    key.key.mod = SDL_KMOD_LALT;
    key.key.down = true;
    REQUIRE(SDL_PushEvent(&key));
    key.key.mod = SDL_KMOD_NONE;
    REQUIRE(SDL_PushEvent(&key));
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_SYSKEYDOWN);
    CHECK(m.wParam == VK_RETURN);
    CHECK_FALSE(NextMessage(m));

    SDL_Event quit = {};
    quit.type = SDL_EVENT_QUIT;
    REQUIRE(SDL_PushEvent(&quit));
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_CLOSE);
}

TEST_CASE("window close request queues exactly one WM_CLOSE", "[window]")
{
    TestWindow w;
    // A real close request on the last window would make SDL add
    // SDL_EVENT_QUIT unless this hint is off.
    REQUIRE(SDL_GetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE) != nullptr);
    CHECK(std::string(SDL_GetHint(SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE)) == "0");

    w.Push(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    platform::window::PumpEvents();
    int closes = 0;
    MSG m = {};
    while (NextMessage(m)) {
        if (m.message == WM_CLOSE)
            closes++;
    }
    CHECK(closes == 1);
}

TEST_CASE("window pump coalesces display events and ignores AltGr", "[window]")
{
    TestWindow w;
    for (int i = 0; i < 3; i++) {
        SDL_Event e = {};
        e.type = i == 0 ? SDL_EVENT_DISPLAY_ORIENTATION : SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED;
        e.display.displayID = SDL_GetPrimaryDisplay();
        REQUIRE(SDL_PushEvent(&e));
    }
    SDL_Event key = {};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.windowID = SDL_GetWindowID(w.Sdl());
    key.key.scancode = SDL_SCANCODE_Q;
    key.key.mod = SDL_KMOD_RALT | SDL_KMOD_LCTRL;
    key.key.down = true;
    REQUIRE(SDL_PushEvent(&key));

    MSG m = {};
    REQUIRE(NextMessage(m));
    CHECK(m.message == WM_DISPLAYCHANGE);
    CHECK(m.lParam == MAKELONG(320, 200));
    CHECK_FALSE(NextMessage(m));
}

TEST_CASE("window Win32 stubs report fixed values", "[window]")
{
    OSVERSIONINFOA info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    REQUIRE(GetVersionExA(&info));
    CHECK(info.dwPlatformId == VER_PLATFORM_WIN32_NT);
    CHECK(info.dwMajorVersion == 5);
    CHECK(info.dwMinorVersion == 1);

    CHECK(LoadLibraryA("binkw32") == NULL);
    CHECK(GetProcAddress(NULL, "x") == NULL);
    CHECK(GetModuleHandleA("Kernel32") != NULL);
    CHECK(GetDeviceCaps(NULL, BITSPIXEL) == 32);
    CHECK(GetDeviceCaps(NULL, PLANES) == 1);

    MEMORYSTATUS ms = {};
    GlobalMemoryStatus(&ms);
    CHECK(ms.dwTotalPhys == 1024u * 1024u * 1024u);
    CHECK(ms.dwAvailPhys == 1024u * 1024u * 1024u);

    char buffer[32];
    CHECK(lstrcpyA(buffer, "ab") == buffer);
    CHECK(lstrcatA(buffer, "Cd") == buffer);
    CHECK(lstrlenA(buffer) == 4);
    CHECK(lstrcmpA(buffer, "abCd") == 0);
    CHECK(lstrcmpiA(buffer, "ABCD") == 0);
    CHECK(lstrcmpiA("a", "B") < 0);
    CHECK(wsprintfA(buffer, "%d-%s", 7, "x") == 3);
    CHECK(std::string(buffer) == "7-x");

    CHECK(ImmGetContext(NULL) == NULL);
    CHECK(GetLastError() == 0);
}

TEST_CASE("window MessageBoxA returns IDOK", "[window][.manual]")
{
    CHECK(MessageBoxA(NULL, "iwd2r MessageBoxA test: press OK", "iwd2r", MB_OK) == IDOK);
}

TEST_CASE("window system parameters report Windows defaults", "[window]")
{
    int delay = -1, speed = -1;
    UINT lines = 0;
    BOOL active = FALSE;
    CHECK(SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0) == TRUE);
    CHECK(delay == 1);
    CHECK(SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &speed, 0) == TRUE);
    CHECK(speed == 31);
    CHECK(SystemParametersInfoA(SPI_GETWHEELSCROLLLINES, 0, &lines, 0) == TRUE);
    CHECK(lines == 3);
    CHECK(SystemParametersInfoA(SPI_GETSCREENSAVEACTIVE, 0, &active, 0) == TRUE);
    CHECK(active == TRUE);
    CHECK(SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0) == TRUE);
    CHECK(SystemParametersInfoA(SPI_GETKEYBOARDDELAY, 0, NULL, 0) == FALSE);
    CHECK(SystemParametersInfoA(0x7FFF, 0, &delay, 0) == FALSE);

    CHECK(GetDoubleClickTime() == 500);
    CHECK(FindWindowA("MouseZ", "Magellan MSWHEEL") == NULL);
    CHECK(CloseWindow(NULL) == FALSE);
}

TEST_CASE("window inert process, GDI and locale stubs", "[window]")
{
    CHECK(GetStockObject(4) == NULL);
    CHECK(LoadIconA(NULL, "ICON") == NULL);
    CHECK(GetSystemDefaultLangID() == 0x0409);
    CHECK(PRIMARYLANGID(GetSystemDefaultLangID()) == LANG_ENGLISH);
    CHECK(SUBLANGID(GetSystemDefaultLangID()) == SUBLANG_ENGLISH_US);
    SetLastError(0);
    CHECK(OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId()) == NULL);
    CHECK(GetLastError() == ERROR_ACCESS_DENIED);
    SetLastError(0);
}
