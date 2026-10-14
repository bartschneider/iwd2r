#define NOMINMAX
#include "platform/present.h"

#include "platform/gpu_probe.h"
#include "platform/present_backend.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace platform::present {

namespace {

    struct State {
        SDL_Window* window = nullptr;
        std::unique_ptr<Backend> backend;
        BackendKind requested = BackendKind::Classic;
        BackendKind active = BackendKind::Classic;
        Filter filter = Filter::Nearest;
        float sharpness = 0.5f;
        // A device loss was already answered with a rebuild (one per run).
        bool rebuilt = false;
        // The gpu backend was lost and fell back to classic: classic for the
        // rest of the run, whatever later asks for gpu (a new window, the
        // hotkey).
        bool gpuLost = false;
        // The child probe's verdict, kept for the session (probed: it ran).
        bool probed = false;
        bool probeOk = false;
        std::string probeWhy;
        std::string probeExe; // testing::SetProbeExe; empty: this executable
        std::string fallback; // why the last gpu start fell back

        int skipped = 0;
        // testing::UseOffscreen: a backend without a window.
        int offscreenWidth = 0;
        int offscreenHeight = 0;
        SIZE screen = { 1024, 768 };
        SIZE desktop = { 0, 0 };
        SIZE mode = { 0, 0 };
        int scale = 1;
        bool fullscreen = false;
        bool dirty = false;
        int frames = 0;
        const std::vector<uint32_t>* primary = nullptr;
        int primaryWidth = 0;
        int primaryHeight = 0;
        // SetRegions and Present run on the main thread (the engine draws
        // and flips in CChitin::SynchronousUpdate there), but the mouse
        // mapping is read from the AI thread too (GetCursorPos in
        // CChitin::AsynchronousUpdate): the list is replaced whole under the
        // lock, and readers keep the snapshot they took.
        std::mutex regionsMutex;
        std::shared_ptr<const std::vector<compose::Region>> regions = std::make_shared<const std::vector<compose::Region>>();
    };

    State& S()
    {
        static State state;
        return state;
    }

    const int kProbeTimeoutMs = 10000;

    const uint64_t kFnvOffset = 14695981039346656037ull;
    const uint64_t kFnvPrime = 1099511628211ull;

    uint64_t Fnv1a(uint64_t hash, const void* data, size_t size)
    {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; i++) {
            hash ^= bytes[i];
            hash *= kFnvPrime;
        }
        return hash;
    }

    std::string Lower(const char* text)
    {
        std::string s = text ? text : "";
        const size_t first = s.find_first_not_of(" \t");
        const size_t last = s.find_last_not_of(" \t");
        s = first == std::string::npos ? std::string() : s.substr(first, last - first + 1);
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::shared_ptr<const std::vector<compose::Region>> RegionsSnapshot()
    {
        State& s = S();
        std::lock_guard<std::mutex> lock(s.regionsMutex);
        return s.regions;
    }

    bool HasTarget()
    {
        const State& s = S();
        return s.window || (s.offscreenWidth > 0 && s.offscreenHeight > 0);
    }

    void StopBackend()
    {
        State& s = S();
        if (s.backend) {
            s.backend->Shutdown();
            s.backend.reset();
        }
    }

    bool TryStart(std::unique_ptr<Backend> backend, std::string& why)
    {
        State& s = S();
        backend->offscreenWidth = s.offscreenWidth;
        backend->offscreenHeight = s.offscreenHeight;
        if (!backend->Init(s.window)) {
            why = backend->error;
            backend->Shutdown();
            return false;
        }
        s.backend = std::move(backend);
        return true;
    }

    // Whether the game's process may create a GPU device: the child probe
    // (IWD2R_GPU_PROBE skips or fails it). False with why set.
    bool ProbeAllows(std::string& why)
    {
        State& s = S();
        const char* mode = std::getenv("IWD2R_GPU_PROBE");
        const std::string m = Lower(mode);
        if (m == "skip")
            return true;
        if (m == "fail") {
            why = "probe: forced by IWD2R_GPU_PROBE=fail";
            return false;
        }
        if (!s.probed) {
            const std::string exe = s.probeExe.empty() ? gpu_probe::SelfExe() : s.probeExe;
            if (exe.empty()) {
                s.probeOk = false;
                s.probeWhy = "this executable's path is unknown";
            } else {
                const gpu_probe::Result r = gpu_probe::Run(exe, kProbeTimeoutMs);
                s.probeOk = r.outcome == gpu_probe::Outcome::Ok;
                s.probeWhy = r.reason;
                if (s.probeOk)
                    std::fprintf(stderr, "presenter: gpu probe %s\n", r.reason.c_str());
            }
            s.probed = true;
        }
        if (!s.probeOk)
            why = "probe: " + s.probeWhy;
        return s.probeOk;
    }

    // Starts the chosen backend on the current target; gpu (or auto) falls
    // back to classic, saying why.
    void StartBackend(BackendKind kind)
    {
        State& s = S();
        StopBackend();
        s.active = BackendKind::Classic;
        if (!HasTarget())
            return;
        std::string why;
        if (kind != BackendKind::Classic) {
            if (s.gpuLost)
                why = "lost earlier in this run";
            else if (ProbeAllows(why) && TryStart(MakeGpuBackend(), why)) {
                s.active = BackendKind::Gpu;
                s.fallback.clear();
                return;
            }
            s.fallback = why;
            std::fprintf(stderr, "presenter: gpu unavailable (%s), using classic\n", why.c_str());
        }
        if (!TryStart(MakeClassicBackend(), why))
            std::fprintf(stderr, "presenter: classic unavailable (%s)\n", why.c_str());
    }

    // A lost device: classic for the rest of the run (gpuLost).
    void FallBackToClassic(const std::string& reason)
    {
        State& s = S();
        std::fprintf(stderr, "presenter: gpu %s, using classic\n", reason.c_str());
        s.gpuLost = true;
        s.fallback = "gpu " + reason;
        StartBackend(BackendKind::Classic);
    }

    // A frame failed in an API call: rebuild the device once, and fall back
    // to classic when it is lost again or cannot be rebuilt.
    void HandleLoss()
    {
        State& s = S();
        if (s.active != BackendKind::Gpu)
            return;
        const std::string why = s.backend ? s.backend->error : std::string();
        if (s.rebuilt) {
            FallBackToClassic("lost again (" + why + ")");
            return;
        }
        std::fprintf(stderr, "presenter: gpu frame failed (%s), rebuilding the device\n", why.c_str());
        StopBackend();
        std::string failed;
        if (TryStart(MakeGpuBackend(), failed)) {
            s.rebuilt = true;
            return;
        }
        FallBackToClassic("could not be rebuilt (" + failed + ")");
    }

    // One frame through the backend; false when it was skipped.
    bool DrawFrame(const uint32_t* xrgb, int width, int height, int pitchBytes, bool offscreen)
    {
        State& s = S();
        Backend& b = *s.backend;
        b.letterbox = s.fullscreen;
        b.windowScale = s.scale;
        b.sharpness = s.sharpness;
        b.offscreen = offscreen;
        b.lost = false;
        bool drawn = false;
        if (b.BeginFrame()) {
            b.Upload(xrgb, width, height, pitchBytes);
            const auto regions = RegionsSnapshot();
            if (regions->empty()) {
                b.DrawRegion(WholeFrameRegion(width, height), s.filter);
            } else {
                for (const compose::Region& r : *regions)
                    b.DrawRegion(r, s.filter);
            }
            b.EndFrame();
            drawn = !b.lost;
        }
        b.offscreen = false;
        if (!drawn) {
            s.skipped++;
            if (b.lost)
                HandleLoss();
        }
        return drawn;
    }

}

compose::Region WholeFrameRegion(int width, int height)
{
    compose::Region r = {};
    r.frame = { 0, 0, width, height };
    r.window = { 0, 0, 0, 0 };
    r.scale = 0;
    r.ui = false;
    return r;
}

SDL_FRect WholeFrameRect(int frameW, int frameH, int outW, int outH, bool letterbox)
{
    // SDL_render.c's UpdateLogicalPresentation, for STRETCH and LETTERBOX.
    SDL_FRect r = { 0.0f, 0.0f, static_cast<float>(outW), static_cast<float>(outH) };
    const float lw = static_cast<float>(frameW);
    const float lh = static_cast<float>(frameH);
    const float ow = static_cast<float>(outW);
    const float oh = static_cast<float>(outH);
    if (!letterbox || lw <= 0.0f || lh <= 0.0f || ow <= 0.0f || oh <= 0.0f)
        return r;
    const float want = lw / lh;
    const float real = ow / oh;
    if (SDL_fabsf(want - real) < 0.0001f)
        return r;
    if (want > real) {
        const float scale = ow / lw;
        r.h = SDL_floorf(lh * scale);
        r.y = (oh - r.h) / 2.0f;
    } else {
        const float scale = oh / lh;
        r.w = SDL_floorf(lw * scale);
        r.x = (ow - r.w) / 2.0f;
    }
    return r;
}

bool ParseBackendKind(const char* text, BackendKind* kind)
{
    const std::string s = Lower(text);
    if (s == "auto")
        *kind = BackendKind::Auto;
    else if (s == "gpu")
        *kind = BackendKind::Gpu;
    else if (s == "classic")
        *kind = BackendKind::Classic;
    else
        return false;
    return true;
}

bool ParseFilter(const char* text, Filter* filter)
{
    const std::string s = Lower(text);
    if (s == "nearest")
        *filter = Filter::Nearest;
    else if (s == "sharp")
        *filter = Filter::Sharp;
    else if (s == "smooth")
        *filter = Filter::Smooth;
    else if (s == "fsr")
        *filter = Filter::Fsr;
    else if (s == "nis")
        *filter = Filter::Nis;
    else
        return false;
    return true;
}

bool ParseSharpness(const char* text, float* sharpness)
{
    const std::string s = Lower(text);
    if (s.empty())
        return false;
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    if (end != s.c_str() + s.size() || !(v >= 0.0f && v <= 1.0f))
        return false;
    *sharpness = v;
    return true;
}

const char* BackendKindName(BackendKind kind)
{
    switch (kind) {
    case BackendKind::Auto:
        return "auto";
    case BackendKind::Gpu:
        return "gpu";
    case BackendKind::Classic:
        return "classic";
    }
    return "classic";
}

void SetBackend(BackendKind kind)
{
    State& s = S();
    s.requested = kind;
    if (HasTarget())
        StartBackend(kind);
}

BackendKind ActiveBackend() { return S().active; }

BackendKind ToggleBackend()
{
    SetBackend(S().active == BackendKind::Gpu ? BackendKind::Classic : BackendKind::Gpu);
    return S().active;
}

void SetFilter(Filter filter) { S().filter = filter; }

Filter CurrentFilter() { return S().filter; }

void SetSharpness(float sharpness)
{
    S().sharpness = sharpness > 1.0f ? 1.0f : sharpness >= 0.0f ? sharpness
                                                                : 0.0f;
}

float CurrentSharpness() { return S().sharpness; }

void SetPresentTarget(SDL_Window* window)
{
    State& s = S();
    if (s.window == window)
        return;
    StopBackend();
    s.active = BackendKind::Classic;
    s.window = window;
    if (s.window)
        SDL_SetWindowFullscreen(s.window, s.fullscreen);
    if (HasTarget())
        StartBackend(s.requested);
}

void SetScreenSize(int w, int h)
{
    S().screen.cx = w;
    S().screen.cy = h;
}

void SetWindowScale(int scale) { S().scale = scale >= 1 ? scale : 1; }

int WindowScale() { return S().scale; }

SIZE ScreenSize()
{
    const State& s = S();
    if (s.fullscreen && s.mode.cx > 0 && s.mode.cy > 0)
        return s.mode;
    return s.screen;
}

void SetDesktopSize(int w, int h)
{
    S().desktop.cx = w;
    S().desktop.cy = h;
}

SIZE DesktopSize()
{
    const State& s = S();
    if (s.desktop.cx > 0 && s.desktop.cy > 0)
        return s.desktop;
    return ScreenSize();
}

void SetFullscreen(bool fullscreen)
{
    State& s = S();
    s.fullscreen = fullscreen;
    if (s.window)
        SDL_SetWindowFullscreen(s.window, fullscreen);
}

void SetDisplayMode(int w, int h)
{
    State& s = S();
    if (w > 0 && h > 0) {
        s.mode.cx = w;
        s.mode.cy = h;
        SetFullscreen(true);
        return;
    }
    s.mode.cx = 0;
    s.mode.cy = 0;
    SetFullscreen(false);
}

void WindowToFrame(float x, float y, float* fx, float* fy)
{
    const State& s = S();
    if (compose::WindowToFrame(*RegionsSnapshot(), x, y, fx, fy))
        return;
    if (s.backend && s.backend->WindowToFrame(x, y, fx, fy))
        return;
    *fx = x / static_cast<float>(s.scale);
    *fy = y / static_cast<float>(s.scale);
}

void FrameToWindow(float fx, float fy, float* x, float* y)
{
    const State& s = S();
    if (compose::FrameToWindow(*RegionsSnapshot(), fx, fy, x, y))
        return;
    if (s.backend && s.backend->FrameToWindow(fx, fy, x, y))
        return;
    *x = fx * static_cast<float>(s.scale);
    *y = fy * static_cast<float>(s.scale);
}

void SetRegions(std::vector<compose::Region> regions)
{
    auto list = std::make_shared<const std::vector<compose::Region>>(std::move(regions));
    State& s = S();
    std::lock_guard<std::mutex> lock(s.regionsMutex);
    s.regions.swap(list);
}

std::shared_ptr<const std::vector<compose::Region>> Regions() { return RegionsSnapshot(); }

bool FrameSize(int* w, int* h)
{
    const State& s = S();
    if (s.backend && s.backend->FrameSize(w, h))
        return true;
    const auto regions = RegionsSnapshot();
    if (regions->empty())
        return false;
    LONG right = 0;
    LONG bottom = 0;
    for (const compose::Region& r : *regions) {
        right = std::max(right, r.frame.right);
        bottom = std::max(bottom, r.frame.bottom);
    }
    *w = right;
    *h = bottom;
    return true;
}

void Present(const uint32_t* xrgb, int width, int height, int pitchBytes)
{
    State& s = S();
    s.dirty = false;
    s.frames++;

    if (!s.backend || width <= 0 || height <= 0)
        return;
    DrawFrame(xrgb, width, height, pitchBytes, false);
}

int SkippedFrames() { return S().skipped; }

bool ReadbackPresented(std::vector<uint32_t>* xrgb, int* width, int* height)
{
    State& s = S();
    const uint32_t* pixels = nullptr;
    int w = 0;
    int h = 0;
    int pitch = 0;
    if (!s.backend || !PrimaryFrame(&pixels, &w, &h, &pitch))
        return false;
    if (!DrawFrame(pixels, w, h, pitch, true) || !s.backend)
        return false;
    return s.backend->Readback(xrgb, width, height);
}

void MarkPrimaryDirty() { S().dirty = true; }

void PresentIfDirty()
{
    State& s = S();
    if (!s.dirty)
        return;
    if (!s.primary) {
        s.dirty = false;
        return;
    }
    Present(s.primary->data(), s.primaryWidth, s.primaryHeight, s.primaryWidth * 4);
}

uint64_t HashPrimary()
{
    const State& s = S();
    if (!s.primary)
        return kFnvOffset;
    return Fnv1a(kFnvOffset, s.primary->data(), s.primary->size() * sizeof(uint32_t));
}

int PresentedFrames() { return S().frames; }

void SetPrimary(const std::vector<uint32_t>* pixels, int width, int height)
{
    State& s = S();
    s.primary = pixels;
    s.primaryWidth = pixels ? width : 0;
    s.primaryHeight = pixels ? height : 0;
}

const std::vector<uint32_t>* Primary() { return S().primary; }

bool PrimaryFrame(const uint32_t** pixels, int* width, int* height, int* pitchBytes)
{
    const State& s = S();
    if (!s.primary || s.primaryWidth <= 0 || s.primaryHeight <= 0
        || s.primary->size() < static_cast<size_t>(s.primaryWidth) * static_cast<size_t>(s.primaryHeight))
        return false;
    *pixels = s.primary->data();
    *width = s.primaryWidth;
    *height = s.primaryHeight;
    *pitchBytes = s.primaryWidth * 4;
    return true;
}

namespace testing {

    void SetProbeExe(const char* exe)
    {
        State& s = S();
        s.probeExe = exe ? exe : "";
        s.probed = false;
        s.probeOk = false;
        s.probeWhy.clear();
    }

    std::string LastFallbackReason() { return S().fallback; }

    void ForgetGpuLoss()
    {
        State& s = S();
        s.gpuLost = false;
        s.rebuilt = false;
    }

    void UseOffscreen(int w, int h)
    {
        State& s = S();
        StopBackend();
        s.active = BackendKind::Classic;
        s.offscreenWidth = w > 0 && h > 0 ? w : 0;
        s.offscreenHeight = w > 0 && h > 0 ? h : 0;
        if (HasTarget())
            StartBackend(s.requested);
    }

}

}
