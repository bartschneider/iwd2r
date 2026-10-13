// Internal: the SDL side of the Win32 window shim in window.cpp. The
// Win32-named functions themselves are declared in win32_api.h.
#pragma once

#include "compat/win_types.h"
#include "platform/compose.h"

#include <vector>

namespace platform::window {

// SDL_Scancode for a Win32 virtual key, or 0 (SDL_SCANCODE_UNKNOWN) when the
// key has none (the mouse buttons). VK_SHIFT, VK_CONTROL and VK_MENU give the
// left-hand key; GetAsyncKeyState checks both sides for them.
int ScancodeForVK(int vk);

// Win32 virtual key for an SDL_Scancode, or 0. Both sides of a modifier give
// the generic VK_SHIFT, VK_CONTROL or VK_MENU, as WM_KEYDOWN reports them.
int VKForScancode(int sc);

// Translates pending SDL events into queued window messages, presents the
// primary surface when dirty, runs the smoke-frame check and advances the
// input script (platform/script.h) once per presented frame.
void PumpEvents();

// Posts a message addressed to the main window.
void EnqueueMessage(UINT message, WPARAM wParam, LPARAM lParam);

// Sets the virtual screen (the game resolution, width x height) that
// GetSystemMetrics(SM_CXSCREEN) reports and CreateWindowExA's window takes,
// and starts SDL video so the desktop size is known before the engine reads
// SM_CXFULLSCREEN. scale (Program Options/World Scale, else Scale) is the
// number of desktop pixels per engine pixel: the window is width x height
// times scale, and every window size, position and desktop size the engine
// sees is in engine pixels. uiScale (the largest scale a UI panel is shown
// at, the Scale that fits; 0 for scale) is the UI panels' scale: when it
// differs from scale the presenter composes
// regions (platform/compose.h), installed here with compose::Layout (the
// whole frame as the world until SetRegionSource gives the screen's panels)
// and again on WM_SIZE, WM_DISPLAYCHANGE and every pump, where a change of
// screen shows. False when SDL video cannot start.
bool InitScreen(int width, int height, int scale = 1, int uiScale = 0);

// The regions for a window of the given size (pixels), for the screen the
// engine shows now: src/ScreenLayout's CurrentRegions.
using RegionSource = std::vector<compose::Region> (*)(SIZE window);

// Installs the region source and refreshes the regions; nullptr removes it
// and the regions. Only used while the UI and world scales differ; with
// equal scales no regions are installed and the presenter shows the frame
// as it always has.
void SetRegionSource(RegionSource source);

// Recomputes the regions for the current window and screen and installs
// them (present::SetRegions) when they changed.
void RefreshRegions();

// The primary display's native desktop size in pixels (starting SDL video
// when needed), for Resolution=desktop. False, with zeros, when unknown.
bool NativeDesktopSize(int* width, int* height);

// The live window, or NULL.
HWND MainWindow();

// After n presented frames the pump prints "framebuffer-hash: <hex>" once and
// posts WM_DESTROY. 0 disables. CreateWindowExA sets it from
// IWD2R_SMOKE_FRAMES when that variable is present, and starts the script
// named by IWD2R_SCRIPT.
void SetSmokeFrames(int n);

// Routes platform::SetFatalMessageHandler messages to MessageBoxA with
// MB_OK | MB_ICONERROR.
void InstallFatalMessageBox();

}
