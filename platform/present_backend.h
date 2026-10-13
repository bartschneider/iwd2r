// Internal: the presenter's backends, behind platform/present. The front
// (present.cpp) owns the engine-facing API and the frame loop; a backend
// turns one XRGB8888 frame and the region list into window pixels.
#pragma once

#include "platform/compose.h"
#include "platform/present.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace platform::present {

// A region whose scale is 0 is the whole frame shown the way the presenter
// has always shown it with no region list: stretched onto the window, or
// letterboxed (Backend::letterbox) in fullscreen. Its window rect is unused.
compose::Region WholeFrameRegion(int width, int height);

struct Backend {
    virtual ~Backend() = default;

    // window nullptr: offscreen (the gpu backend renders into a target the
    // size of offscreenSize, for tests and Readback without a window); the
    // classic backend needs a window. False with error set.
    virtual bool Init(SDL_Window* window) = 0;
    virtual void Shutdown() = 0;
    // Starts a frame: a target to draw into and black over all of it. False
    // skips the frame; lost then says whether it failed (device loss) or
    // only had nothing to draw into (a minimised window).
    virtual bool BeginFrame() = 0;
    virtual void Upload(const uint32_t* xrgb, int w, int h, int pitch) = 0;
    virtual void DrawRegion(const compose::Region& region, Filter filter) = 0;
    // Shows the frame, unless offscreen is set. lost is set when it failed.
    virtual void EndFrame() = 0;
    // The last frame's pixels as drawn (window size), XRGB8888.
    virtual bool Readback(std::vector<uint32_t>* xrgb, int* w, int* h) = 0;

    // The whole-frame mapping (no region list): window point to frame and
    // back through the last presented frame's placement. False before a
    // frame was presented.
    virtual bool WindowToFrame(float x, float y, float* fx, float* fy) = 0;
    virtual bool FrameToWindow(float fx, float fy, float* x, float* y) = 0;
    // The last uploaded frame's size; false before the first.
    virtual bool FrameSize(int* w, int* h) = 0;

    // Set by the front.
    bool letterbox = false; // the whole-frame region is letterboxed, not stretched
    int windowScale = 1; // WindowScale(): the classic texture is nearest above 1
    bool offscreen = false; // EndFrame does not show the frame (Readback only)
    int offscreenWidth = 0; // the target size without a window
    int offscreenHeight = 0;

    // Set by the backend.
    bool lost = false; // the last frame failed in an API call
    std::string error; // why Init failed or the frame was lost
};

// The compiled SPIR-V of platform/shaders/<name> (e.g. "quad.vert"),
// embedded at configure time; false for an unknown name.
bool ShaderCode(const char* name, const unsigned char** code, size_t* size);

std::unique_ptr<Backend> MakeClassicBackend();
std::unique_ptr<Backend> MakeGpuBackend();

// Where the whole frame lands in an output of outW x outH pixels, the way
// SDL's logical presentation places it (stretched, or letterboxed centred).
SDL_FRect WholeFrameRect(int frameW, int frameH, int outW, int outH, bool letterbox);

namespace testing {

    // Faults the gpu backend injects, for the fallback tests.
    enum class GpuFault {
        None,
        Device, // SDL_CreateGPUDevice fails
        Pipeline, // the graphics pipeline cannot be created
        AcquireOnce, // the next frame's acquire fails, then works again
        Acquire, // every frame's acquire fails
    };
    void SetGpuFault(GpuFault fault);
    GpuFault TakeAcquireFault(); // the gpu backend: consumes AcquireOnce
    GpuFault CurrentGpuFault();

    // Runs the front's backend without a window, presenting into an
    // offscreen target of w x h (0 x 0 stops it). The backend is chosen as
    // SetBackend would choose it with a window.
    void UseOffscreen(int w, int h);

}

}
