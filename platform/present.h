// iwd2r. Distributed under LICENSE.md.
// Internal: presents the DirectDraw shim's primary surface through SDL. Used
// by the shim, the window code and the smoke hook; never included by src/.
#pragma once

#include "compat/win_types.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace platform::present {

// The window frames are presented to, or nullptr for none. Changing it
// destroys the renderer and texture made for the previous window.
void SetPresentTarget(SDL_Window* window);

// The virtual screen the engine sees in windowed mode: the game resolution
// (iwd2r_main sets it from the INI before the engine starts). The window's
// client area is this screen, and the windowed primary surface takes its
// size. 1024x768 until set.
void SetScreenSize(int w, int h);

// Desktop pixels per engine pixel (Program Options/Scale, 1 to 4): the
// window is the virtual screen times this, and frames are scaled up with
// nearest-neighbour filtering. 1 until set.
void SetWindowScale(int scale);
int WindowScale();

// The screen the engine sees: the display mode set by SetDisplayMode while
// fullscreen, otherwise the virtual screen. GetSystemMetrics(SM_CXSCREEN)
// reports it.
SIZE ScreenSize();

// The real desktop's size, kept by the window code; the window is placed
// within it and GetSystemMetrics(SM_CXFULLSCREEN) reports it. Falls back to
// ScreenSize() until set.
void SetDesktopSize(int w, int h);
SIZE DesktopSize();

// Switches the target window to borderless desktop fullscreen and back, or
// remembers the flag while no window is set.
void SetFullscreen(bool fullscreen);

// DirectDraw's SetDisplayMode: fullscreen at w x h (letterboxed onto the
// desktop). w or h 0 (RestoreDisplayMode) clears the mode and leaves
// fullscreen.
void SetDisplayMode(int w, int h);

// Maps a point between window coordinates and the presented frame's
// (the engine's screen coordinates) through the renderer's logical
// presentation: stretched onto the window in windowed mode, letterboxed in
// fullscreen. Before the first frame is presented or without a renderer
// (the dummy video driver before a present), a division (multiplication) by
// WindowScale().
void WindowToFrame(float x, float y, float* fx, float* fy);
void FrameToWindow(float fx, float fy, float* x, float* y);

// The size of the last presented frame, when WindowToFrame maps through a
// renderer; false while it is the identity.
bool FrameSize(int* w, int* h);

// Uploads one XRGB8888 frame and presents it. Without a target window it only
// counts the frame.
void Present(const uint32_t* xrgb, int width, int height, int pitchBytes);

void MarkPrimaryDirty();

// Presents the registered primary when it was marked dirty since the last
// present.
void PresentIfDirty();

// FNV-1a 64 over the registered primary's pixel bytes.
uint64_t HashPrimary();

int PresentedFrames();

// Registers the primary surface's pixels (the vector itself, so swapping its
// contents keeps the registration valid); nullptr unregisters. Called by the
// DirectDraw shim only.
void SetPrimary(const std::vector<uint32_t>* pixels, int width, int height);
const std::vector<uint32_t>* Primary();

// The registered primary's pixels, size and pitch, for screenshots; false
// when none is registered. Main thread only, like Present: the DirectDraw
// shim registers and draws into the primary there.
bool PrimaryFrame(const uint32_t** pixels, int* width, int* height, int* pitchBytes);

}
