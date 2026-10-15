#include "platform/present_backend.h"

namespace platform::present {

namespace {

    // The SDL_Renderer path the presenter always had: one streaming
    // texture, nearest-neighbour when scaled, the whole frame through the
    // renderer's logical presentation (stretched windowed, letterboxed
    // fullscreen).
    class ClassicBackend : public Backend {
    public:
        bool Init(SDL_Window* window) override
        {
            if (!window) {
                error = "the classic presenter needs a window";
                return false;
            }
            window_ = window;
            return true;
        }

        void Shutdown() override
        {
            if (overlayTexture_) {
                SDL_DestroyTexture(overlayTexture_);
                overlayTexture_ = nullptr;
            }
            overlayWidth_ = 0;
            overlayHeight_ = 0;
            overlayVersion_ = 0;
            if (texture_) {
                SDL_DestroyTexture(texture_);
                texture_ = nullptr;
            }
            if (renderer_) {
                SDL_DestroyRenderer(renderer_);
                renderer_ = nullptr;
            }
            textureWidth_ = 0;
            textureHeight_ = 0;
            window_ = nullptr;
        }

        bool BeginFrame() override
        {
            frameReady_ = false;
            return window_ != nullptr;
        }

        void Upload(const uint32_t* xrgb, int w, int h, int pitch) override
        {
            if (!EnsureTexture(w, h))
                return;
            SDL_UpdateTexture(texture_, nullptr, xrgb, pitch);
            frameReady_ = true;
            cleared_ = false;
        }

        void DrawRegion(const compose::Region& region, Filter) override
        {
            if (!frameReady_)
                return;
            if (region.scale <= 0) {
                // Windowed, the client area is the virtual screen and the
                // frame fills it (stretch only matters on high-DPI displays
                // or a mismatched window); fullscreen, the mode is
                // letterboxed onto the desktop. WindowToFrame maps the mouse
                // through the same presentation.
                SDL_SetRenderLogicalPresentation(renderer_, textureWidth_, textureHeight_,
                    letterbox ? SDL_LOGICAL_PRESENTATION_LETTERBOX : SDL_LOGICAL_PRESENTATION_STRETCH);
                Clear();
                SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
                int ow = 0;
                int oh = 0;
                if (SDL_GetCurrentRenderOutputSize(renderer_, &ow, &oh)) {
                    const SDL_FRect d = WholeFrameRect(textureWidth_, textureHeight_, ow, oh, letterbox);
                    Place(region.frame, d, region.ui);
                }
                return;
            }
            // A composition region: its frame rect at its scale from its
            // window rect's top-left, in output pixels.
            SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
            Clear();
            const RECT& f = region.frame;
            const SDL_FRect src = { static_cast<float>(f.left), static_cast<float>(f.top),
                static_cast<float>(f.right - f.left), static_cast<float>(f.bottom - f.top) };
            // Regions are in window points, the renderer's output in pixels.
            float px = 1.0f;
            float py = 1.0f;
            int ow = 0;
            int oh = 0;
            int ww = 0;
            int wh = 0;
            if (SDL_GetCurrentRenderOutputSize(renderer_, &ow, &oh) && SDL_GetWindowSize(window_, &ww, &wh) && ww > 0 && wh > 0) {
                px = static_cast<float>(ow) / static_cast<float>(ww);
                py = static_cast<float>(oh) / static_cast<float>(wh);
            }
            const float s = static_cast<float>(region.scale);
            const SDL_FRect dst = { static_cast<float>(region.window.left) * px, static_cast<float>(region.window.top) * py,
                src.w * s * px, src.h * s * py };
            SDL_RenderTexture(renderer_, texture_, &src, &dst);
            Place(region.frame, dst, region.ui);
        }

        bool OutputSize(int* w, int* h) override
        {
            return frameReady_ && renderer_ && SDL_GetCurrentRenderOutputSize(renderer_, w, h) && *w > 0 && *h > 0;
        }

        // The overlay as one texture the output's size, premultiplied,
        // drawn 1:1 over the whole output with the logical presentation
        // off (and put back after: the mouse maps through it).
        void DrawText(const text::Overlay& overlay) override
        {
            if (!frameReady_ || overlay.width <= 0 || overlay.height <= 0)
                return;
            if (!overlayTexture_ || overlayWidth_ != overlay.width || overlayHeight_ != overlay.height) {
                if (overlayTexture_)
                    SDL_DestroyTexture(overlayTexture_);
                overlayTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, overlay.width, overlay.height);
                if (!overlayTexture_)
                    return;
                SDL_SetTextureBlendMode(overlayTexture_, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
                SDL_SetTextureScaleMode(overlayTexture_, SDL_SCALEMODE_NEAREST);
                overlayWidth_ = overlay.width;
                overlayHeight_ = overlay.height;
                overlayVersion_ = 0;
            }
            if (overlayVersion_ != overlay.version) {
                SDL_Rect r = { 0, 0, overlay.width, overlay.height };
                if (overlayVersion_ + 1 == overlay.version) {
                    r = { static_cast<int>(overlay.dirty.left), static_cast<int>(overlay.dirty.top),
                        static_cast<int>(overlay.dirty.right - overlay.dirty.left), static_cast<int>(overlay.dirty.bottom - overlay.dirty.top) };
                }
                if (r.w > 0 && r.h > 0) {
                    const uint32_t* first = overlay.pixels.data() + static_cast<size_t>(r.y) * static_cast<size_t>(overlay.width) + static_cast<size_t>(r.x);
                    SDL_UpdateTexture(overlayTexture_, &r, first, overlay.width * 4);
                }
                overlayVersion_ = overlay.version;
            }
            if (overlay.empty)
                return;
            int lw = 0;
            int lh = 0;
            SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
            SDL_GetRenderLogicalPresentation(renderer_, &lw, &lh, &mode);
            SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            SDL_RenderTexture(renderer_, overlayTexture_, nullptr, nullptr);
            SDL_SetRenderLogicalPresentation(renderer_, lw, lh, mode);
        }

        void EndFrame() override
        {
            if (frameReady_ && !offscreen)
                SDL_RenderPresent(renderer_);
        }

        // The output as drawn by the last frame; only meaningful after an
        // offscreen frame (a presented back buffer is undefined).
        bool Readback(std::vector<uint32_t>* xrgb, int* w, int* h) override
        {
            if (!renderer_)
                return false;
            // The whole output, not the logical presentation's viewport.
            SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
            SDL_Surface* shot = SDL_RenderReadPixels(renderer_, nullptr);
            if (!shot)
                return false;
            SDL_Surface* converted = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_XRGB8888);
            SDL_DestroySurface(shot);
            if (!converted)
                return false;
            *w = converted->w;
            *h = converted->h;
            xrgb->assign(static_cast<size_t>(converted->w) * static_cast<size_t>(converted->h), 0u);
            for (int y = 0; y < converted->h; y++) {
                const uint32_t* row = reinterpret_cast<const uint32_t*>(
                    static_cast<const uint8_t*>(converted->pixels) + static_cast<size_t>(y) * static_cast<size_t>(converted->pitch));
                for (int x = 0; x < converted->w; x++)
                    (*xrgb)[static_cast<size_t>(y) * static_cast<size_t>(converted->w) + static_cast<size_t>(x)] = row[x] & 0xFFFFFFu;
            }
            SDL_DestroySurface(converted);
            return true;
        }

        bool WindowToFrame(float x, float y, float* fx, float* fy) override
        {
            return renderer_ && texture_ && SDL_RenderCoordinatesFromWindow(renderer_, x, y, fx, fy);
        }

        bool FrameToWindow(float fx, float fy, float* x, float* y) override
        {
            return renderer_ && texture_ && SDL_RenderCoordinatesToWindow(renderer_, fx, fy, x, y);
        }

        bool FrameSize(int* w, int* h) override
        {
            if (!renderer_ || !texture_)
                return false;
            *w = textureWidth_;
            *h = textureHeight_;
            return true;
        }

    private:
        bool EnsureTexture(int width, int height)
        {
            if (!renderer_) {
                renderer_ = SDL_CreateRenderer(window_, nullptr);
                if (!renderer_)
                    renderer_ = SDL_CreateRenderer(window_, "software");
                if (!renderer_)
                    return false;
            }
            if (texture_ && (textureWidth_ != width || textureHeight_ != height)) {
                SDL_DestroyTexture(texture_);
                texture_ = nullptr;
            }
            if (!texture_) {
                texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
                if (!texture_)
                    return false;
                // Scaled frames keep their pixels square and sharp.
                if (windowScale > 1)
                    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
                textureWidth_ = width;
                textureHeight_ = height;
            }
            return true;
        }

        // A region's placement in output pixels, for the text overlay.
        void Place(const RECT& frame, const SDL_FRect& d, bool ui)
        {
            const float fw = static_cast<float>(frame.right - frame.left);
            const float fh = static_cast<float>(frame.bottom - frame.top);
            if (fw <= 0.0f || fh <= 0.0f)
                return;
            text::Placement p;
            p.frame = frame;
            p.x = d.x;
            p.y = d.y;
            p.sx = d.w / fw;
            p.sy = d.h / fh;
            p.clip = { static_cast<LONG>(SDL_floorf(d.x)), static_cast<LONG>(SDL_floorf(d.y)),
                static_cast<LONG>(SDL_ceilf(d.x + d.w)), static_cast<LONG>(SDL_ceilf(d.y + d.h)) };
            p.ui = ui;
            placed.push_back(p);
        }

        // Black under the first region of a frame; later regions add to it.
        void Clear()
        {
            if (cleared_)
                return;
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
            SDL_RenderClear(renderer_);
            cleared_ = true;
        }

        SDL_Window* window_ = nullptr;
        SDL_Renderer* renderer_ = nullptr;
        SDL_Texture* texture_ = nullptr;
        int textureWidth_ = 0;
        int textureHeight_ = 0;
        bool frameReady_ = false;
        bool cleared_ = false;
        SDL_Texture* overlayTexture_ = nullptr;
        int overlayWidth_ = 0;
        int overlayHeight_ = 0;
        uint64_t overlayVersion_ = 0;
    };

}

std::unique_ptr<Backend> MakeClassicBackend() { return std::make_unique<ClassicBackend>(); }

}
