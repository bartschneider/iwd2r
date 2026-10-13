// Internal: presents the DirectDraw shim's primary surface through SDL. Used
// by the shim, the window code and the smoke hook; in src/ only by
// iwd2r_main, which sets the presenter from the INI.
#pragma once

#include "compat/win_types.h"
#include "platform/compose.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace platform::present {

// Which backend draws the frames: classic (SDL_Renderer, the original
// path), gpu (SDL3 GPU, Vulkan through SPIR-V shaders), or auto (gpu when a
// device can be made for the window, else classic). Program Options/Presenter.
enum class BackendKind {
    Auto,
    Gpu,
    Classic,
};

// How the gpu backend scales the frame (Program Options/Scale Filter):
// nearest, sharp (nearest at texel centres, linear within one output pixel
// of each texel edge), smooth (bilinear), fsr (AMD FidelityFX Super
// Resolution 1.0: EASU upscaling, then RCAS sharpening) or nis (NVIDIA
// Image Scaling). The classic backend is always nearest.
enum class Filter {
    Nearest,
    Sharp,
    Smooth,
    Fsr,
    Nis,
};

// "auto", "gpu", "classic" and "nearest", "sharp", "smooth", "fsr", "nis",
// any case, surrounding blanks ignored; false for anything else.
bool ParseBackendKind(const char* text, BackendKind* kind);
bool ParseFilter(const char* text, Filter* filter);
// A Sharpness value: a number from 0 to 1; false for anything else.
bool ParseSharpness(const char* text, float* sharpness);
const char* BackendKindName(BackendKind kind);

// Chooses the backend. With a target window it switches now: a gpu (or
// auto) choice that cannot start falls back to classic with one stderr line
// saying why. Without a window the choice waits for SetPresentTarget.
void SetBackend(BackendKind kind);
// The backend drawing now: Gpu or Classic (Classic before a window).
BackendKind ActiveBackend();
// Switches between gpu and classic (the Ctrl+Shift+R hotkey) and returns
// the backend now active (classic again when gpu cannot start).
BackendKind ToggleBackend();
void SetFilter(Filter filter);
Filter CurrentFilter();
// How much the fsr and nis filters sharpen (Program Options/Sharpness),
// 0 (none) to 1 (most), clamped; 0.5 until set. fsr: RCAS's linear amount
// (0 off, 1 its maximum, 0.5 one stop below it); nis: the SDK's sharpness.
void SetSharpness(float sharpness);
float CurrentSharpness();

// The window frames are presented to, or nullptr for none. Changing it
// shuts the previous window's backend down (the gpu device releases the
// window) and starts the chosen backend on the new one.
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
//
// With regions set (SetRegions), both map through compose's piecewise
// mapping instead.
void WindowToFrame(float x, float y, float* fx, float* fy);
void FrameToWindow(float fx, float fy, float* x, float* y);

// The regions of the back buffer to composite into the window (from
// compose::Layout); empty, the default, presents the whole frame as before.
void SetRegions(std::vector<compose::Region> regions);
const std::vector<compose::Region>& Regions();

// The size of the last presented frame, when WindowToFrame maps through a
// renderer; with regions set and no frame presented yet, the extent of the
// regions' frame rectangles; false while the mapping is the identity.
bool FrameSize(int* w, int* h);

// Uploads one XRGB8888 frame and presents it. Without a target window it only
// counts the frame. A frame the backend cannot draw (a minimised window, a
// failed swapchain acquire) is skipped but still counted; a gpu device lost
// during a frame is rebuilt once, and lost again falls back to classic.
void Present(const uint32_t* xrgb, int width, int height, int pitchBytes);

// Frames Present counted but the backend skipped.
int SkippedFrames();

// Draws the registered primary through the active backend without showing
// it and reads the result back (window size, XRGB8888): the presented
// frame, for IWD2R_SHOT_PRESENTED. False with no backend or primary.
bool ReadbackPresented(std::vector<uint32_t>* xrgb, int* width, int* height);

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
