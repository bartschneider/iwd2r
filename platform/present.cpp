#define NOMINMAX
#include "platform/present.h"

namespace platform::present {

namespace {

    struct State {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;
        int textureWidth = 0;
        int textureHeight = 0;
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
    };

    State& S()
    {
        static State state;
        return state;
    }

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

    void DestroyRenderer()
    {
        State& s = S();
        if (s.texture) {
            SDL_DestroyTexture(s.texture);
            s.texture = nullptr;
        }
        if (s.renderer) {
            SDL_DestroyRenderer(s.renderer);
            s.renderer = nullptr;
        }
        s.textureWidth = 0;
        s.textureHeight = 0;
    }

    bool EnsureTexture(int width, int height)
    {
        State& s = S();
        if (!s.renderer) {
            s.renderer = SDL_CreateRenderer(s.window, nullptr);
            if (!s.renderer)
                s.renderer = SDL_CreateRenderer(s.window, "software");
            if (!s.renderer)
                return false;
        }
        if (s.texture && (s.textureWidth != width || s.textureHeight != height)) {
            SDL_DestroyTexture(s.texture);
            s.texture = nullptr;
        }
        if (!s.texture) {
            s.texture = SDL_CreateTexture(s.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
            if (!s.texture)
                return false;
            // Scaled frames keep their pixels square and sharp.
            if (s.scale > 1)
                SDL_SetTextureScaleMode(s.texture, SDL_SCALEMODE_NEAREST);
            s.textureWidth = width;
            s.textureHeight = height;
        }
        return true;
    }

}

void SetPresentTarget(SDL_Window* window)
{
    State& s = S();
    if (s.window == window)
        return;
    DestroyRenderer();
    s.window = window;
    if (s.window)
        SDL_SetWindowFullscreen(s.window, s.fullscreen);
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
    if (s.renderer && s.texture && SDL_RenderCoordinatesFromWindow(s.renderer, x, y, fx, fy))
        return;
    *fx = x / static_cast<float>(s.scale);
    *fy = y / static_cast<float>(s.scale);
}

void FrameToWindow(float fx, float fy, float* x, float* y)
{
    const State& s = S();
    if (s.renderer && s.texture && SDL_RenderCoordinatesToWindow(s.renderer, fx, fy, x, y))
        return;
    *x = fx * static_cast<float>(s.scale);
    *y = fy * static_cast<float>(s.scale);
}

bool FrameSize(int* w, int* h)
{
    const State& s = S();
    if (!s.renderer || !s.texture)
        return false;
    *w = s.textureWidth;
    *h = s.textureHeight;
    return true;
}

void Present(const uint32_t* xrgb, int width, int height, int pitchBytes)
{
    State& s = S();
    s.dirty = false;
    s.frames++;

    if (!s.window || width <= 0 || height <= 0)
        return;
    if (!EnsureTexture(width, height))
        return;

    SDL_UpdateTexture(s.texture, nullptr, xrgb, pitchBytes);
    // Windowed, the client area is the virtual screen and the frame fills it
    // (stretch only matters on high-DPI displays or a mismatched window);
    // fullscreen, the mode is letterboxed onto the desktop. WindowToFrame
    // maps the mouse through the same presentation.
    SDL_SetRenderLogicalPresentation(s.renderer, width, height,
        s.fullscreen ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_STRETCH);
    SDL_SetRenderDrawColor(s.renderer, 0, 0, 0, 255);
    SDL_RenderClear(s.renderer);
    SDL_RenderTexture(s.renderer, s.texture, nullptr, nullptr);
    SDL_RenderPresent(s.renderer);
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

}
