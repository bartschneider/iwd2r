// Internal: the presenter's backends, behind platform/present. The front
// (present.cpp) owns the engine-facing API and the frame loop; a backend
// turns one XRGB8888 frame and the region list into window pixels.
#pragma once

#include "platform/compose.h"
#include "platform/present.h"
#include "platform/text_overlay.h"

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
    // The scalable text (platform/text_overlay.h): the output's size in
    // pixels for this frame (false before BeginFrame succeeded), and the
    // overlay blended over everything the regions drew, after the regions
    // and before EndFrame. The overlay is output-sized; a backend uploads
    // only overlay.dirty when it saw the previous version.
    virtual bool OutputSize(int* w, int* h) = 0;
    virtual void DrawText(const text::Overlay& overlay) = 0;
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
    float sharpness = 0.5f; // CurrentSharpness(): the fsr and nis filters
    bool offscreen = false; // EndFrame does not show the frame (Readback only)
    int offscreenWidth = 0; // the target size without a window
    int offscreenHeight = 0;

    // Set by the backend.
    // Where each region drawn this frame landed, in output pixels (DrawRegion
    // appends; the front clears it before BeginFrame): the text overlay maps
    // glyphs through it.
    std::vector<text::Placement> placed;
    bool lost = false; // the last frame failed in an API call
    std::string error; // why Init failed or the frame was lost
};

// The compiled SPIR-V of platform/shaders/<name> (e.g. "quad.vert"),
// embedded at configure time; false for an unknown name.
bool ShaderCode(const char* name, const unsigned char** code, size_t* size);

// NVIDIA Image Scaling's coefficient tables in nis.frag's Coefficients
// block layout (present_nis.cpp).
const void* NisCoefficients();
size_t NisCoefficientsSize();

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
        NullSwapchain, // every frame's acquire succeeds without a texture (minimised)
    };
    void SetGpuFault(GpuFault fault);
    GpuFault TakeAcquireFault(); // the gpu backend: consumes AcquireOnce
    GpuFault CurrentGpuFault();

    // The gpu backend's FSR filter shows EASU's output without RCAS.
    void SetSkipRcas(bool skip);

    // The executable the gpu probe runs as `<exe> --probe-gpu` (nullptr:
    // this one, the default), and forgets the session's probe result.
    void SetProbeExe(const char* exe);
    // Why the last gpu (or auto) start fell back to classic, as the stderr
    // line gives it ("probe: ..." for the probe); empty after a gpu start.
    std::string LastFallbackReason();
    // Forgets a device loss and the rebuild it used: gpu can start again
    // (a run allows one rebuild and no gpu after a lost-again fallback).
    void ForgetGpuLoss();

    // Runs the front's backend without a window, presenting into an
    // offscreen target of w x h (0 x 0 stops it). The backend is chosen as
    // SetBackend would choose it with a window.
    void UseOffscreen(int w, int h);

}

}
