// iwd2r. Distributed under LICENSE.md.
// Internal: the SDL side of the Win32 window shim in window.cpp. The
// Win32-named functions themselves are declared in win32_api.h.
#pragma once

#include "compat/win_types.h"

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
// SM_CXFULLSCREEN. False when SDL video cannot start.
bool InitScreen(int width, int height);

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
