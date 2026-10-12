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

// The desktop size; the windowed primary surface takes this size. 1024x768
// until the window code sets it.
void SetScreenSize(int w, int h);
SIZE ScreenSize();

// Switches the target window to borderless desktop fullscreen and back, or
// remembers the flag while no window is set.
void SetFullscreen(bool fullscreen);

// Uploads one XRGB8888 frame and presents it. Without a target window it only
// counts the frame and records its hash.
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

}
