#define NOMINMAX
#include "platform/present_backend.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace platform::present {

namespace testing {

    namespace {
        GpuFault& Fault()
        {
            static GpuFault fault = GpuFault::None;
            return fault;
        }
    }

    void SetGpuFault(GpuFault fault) { Fault() = fault; }

    GpuFault CurrentGpuFault() { return Fault(); }

    namespace {
        bool& SkipRcasFlag()
        {
            static bool skip = false;
            return skip;
        }
    }

    void SetSkipRcas(bool skip) { SkipRcasFlag() = skip; }

    GpuFault TakeAcquireFault()
    {
        const GpuFault f = Fault();
        if (f == GpuFault::AcquireOnce)
            Fault() = GpuFault::None;
        return f;
    }

}

namespace {

    // The frame texture and the render target hold XRGB8888 as it lies in
    // memory: bytes B, G, R, X.
    const SDL_GPUTextureFormat kFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

    // The fragment shaders' uniform block (std140: vec4s). params is read
    // by nis.frag only (x: sharpness).
    struct RegionUniforms {
        float frameRect[4]; // left, top, right, bottom in texels
        float dest[4]; // destination origin x, y; scale x, y
        float params[4];
    };

    // fsr_rcas.frag's uniform block.
    struct RcasUniforms {
        float clipRect[4]; // left, top, right, bottom in output pixels
        float params[4]; // x: RCAS's linear sharpness
    };

    // The pipelines, one per fragment shader.
    enum Pipeline {
        kNearest,
        kSharp,
        kSmooth,
        kEasu,
        kRcas,
        kNis,
        kPipelineCount,
    };
    // Each pipeline's fragment shader and its uniform buffer count.
    struct PipelineInfo {
        const char* fragment;
        Uint32 uniformBuffers;
    };
    const PipelineInfo kPipelines[kPipelineCount] = {
        { "nearest.frag", 1 },
        { "sharp.frag", 1 },
        { "smooth.frag", 1 },
        { "fsr_easu.frag", 1 },
        { "fsr_rcas.frag", 1 },
        { "nis.frag", 2 }, // Region, Coefficients
    };

    // How a filter draws into the target.
    struct FilterInfo {
        Pipeline pipeline;
        bool linear; // the linear sampler (else nearest)
        bool easu; // FSR: an EASU pass first; the target pass reads its output with RcasUniforms
        bool coefficients; // NIS: push the coefficient tables as uniform buffer 1
    };

    FilterInfo Describe(Filter filter)
    {
        switch (filter) {
        case Filter::Nearest:
            break;
        case Filter::Sharp:
            return { kSharp, true, false, false };
        case Filter::Smooth:
            return { kSmooth, true, false, false };
        case Filter::Fsr:
            return { kRcas, false, true, false };
        case Filter::Nis:
            return { kNis, true, false, true };
        }
        return { kNearest, false, false, false };
    }

    // One region's draw, recorded by DrawRegion and issued by EndFrame.
    struct Draw {
        Filter filter;
        SDL_GPUViewport viewport;
        SDL_Rect scissor;
        SDL_Rect covered; // the pixels whose centres the viewport covers, inside the scissor
        RegionUniforms u;
    };

    // The SDL3 GPU presenter: the frame is uploaded into a sampled texture,
    // each region is drawn as a viewport-sized triangle with its filter's
    // pipeline into a render target the output's size, and the target is
    // blitted to the swapchain. Without a window (or for a Readback frame)
    // the target is the result. FSR takes two passes: EASU draws its
    // regions into an intermediate texture the output's size, then RCAS
    // draws them from there into the target.
    class GpuBackend : public Backend {
    public:
        bool Init(SDL_Window* window) override
        {
            if (testing::CurrentGpuFault() == testing::GpuFault::Device) {
                error = "no device (test hook)";
                return false;
            }
            device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
            if (!device_) {
                error = std::string("no Vulkan device: ") + SDL_GetError();
                return false;
            }
            if (window) {
                if (!SDL_ClaimWindowForGPUDevice(device_, window)) {
                    error = std::string("cannot claim the window: ") + SDL_GetError();
                    return false;
                }
                window_ = window;
                // Like the classic renderer: no vsync wait in Present.
                SDL_GPUPresentMode mode = SDL_GPU_PRESENTMODE_VSYNC;
                if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_IMMEDIATE))
                    mode = SDL_GPU_PRESENTMODE_IMMEDIATE;
                else if (SDL_WindowSupportsGPUPresentMode(device_, window_, SDL_GPU_PRESENTMODE_MAILBOX))
                    mode = SDL_GPU_PRESENTMODE_MAILBOX;
                SDL_SetGPUSwapchainParameters(device_, window_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);
            } else if (offscreenWidth <= 0 || offscreenHeight <= 0) {
                error = "no window and no offscreen size";
                return false;
            }
            return CreatePipelines() && CreateSamplers();
        }

        void Shutdown() override
        {
            if (!device_)
                return;
            if (cmd_) {
                SDL_CancelGPUCommandBuffer(cmd_);
                cmd_ = nullptr;
            }
            SDL_WaitForGPUIdle(device_);
            draws_.clear();
            for (SDL_GPUGraphicsPipeline*& p : pipelines_) {
                if (p)
                    SDL_ReleaseGPUGraphicsPipeline(device_, p);
                p = nullptr;
            }
            for (SDL_GPUSampler*& s : samplers_) {
                if (s)
                    SDL_ReleaseGPUSampler(device_, s);
                s = nullptr;
            }
            if (frame_)
                SDL_ReleaseGPUTexture(device_, frame_);
            if (target_)
                SDL_ReleaseGPUTexture(device_, target_);
            if (easu_)
                SDL_ReleaseGPUTexture(device_, easu_);
            if (upload_)
                SDL_ReleaseGPUTransferBuffer(device_, upload_);
            frame_ = nullptr;
            target_ = nullptr;
            easu_ = nullptr;
            targetW_ = targetH_ = 0;
            easuW_ = easuH_ = 0;
            frameW_ = frameH_ = 0;
            uploadSize_ = 0;
            upload_ = nullptr;
            if (window_)
                SDL_ReleaseWindowFromGPUDevice(device_, window_);
            window_ = nullptr;
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }

        bool BeginFrame() override
        {
            lost = false;
            pass_ = nullptr;
            swapchain_ = nullptr;
            uploaded_ = false;
            draws_.clear();
            const testing::GpuFault fault = testing::TakeAcquireFault();
            if (fault == testing::GpuFault::Acquire || fault == testing::GpuFault::AcquireOnce)
                return Lose("swapchain acquire failed (test hook)");
            cmd_ = SDL_AcquireGPUCommandBuffer(device_);
            if (!cmd_)
                return Lose("no command buffer");
            if (window_ && !offscreen) {
                Uint32 w = 0;
                Uint32 h = 0;
                if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd_, window_, &swapchain_, &w, &h)) {
                    SDL_CancelGPUCommandBuffer(cmd_);
                    cmd_ = nullptr;
                    return Lose("swapchain acquire failed");
                }
                if (!swapchain_) {
                    // Minimised or occluded: nothing to draw into, not a loss.
                    SDL_SubmitGPUCommandBuffer(cmd_);
                    cmd_ = nullptr;
                    return false;
                }
                outW_ = static_cast<int>(w);
                outH_ = static_cast<int>(h);
            } else if (window_) {
                SDL_GetWindowSizeInPixels(window_, &outW_, &outH_);
            } else {
                outW_ = offscreenWidth;
                outH_ = offscreenHeight;
            }
            // Regions are in window points; the target is in pixels.
            pixelsX_ = 1.0f;
            pixelsY_ = 1.0f;
            int pw = 0;
            int ph = 0;
            if (window_ && SDL_GetWindowSize(window_, &pw, &ph) && pw > 0 && ph > 0) {
                pixelsX_ = static_cast<float>(outW_) / static_cast<float>(pw);
                pixelsY_ = static_cast<float>(outH_) / static_cast<float>(ph);
            }
            if (outW_ <= 0 || outH_ <= 0 || !EnsureTexture(target_, targetW_, targetH_, outW_, outH_, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
                Abandon();
                return Lose("no render target");
            }
            return true;
        }

        void Upload(const uint32_t* xrgb, int w, int h, int pitch) override
        {
            if (!cmd_)
                return;
            if (!EnsureTexture(frame_, frameW_, frameH_, w, h, SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
                Lose("no frame texture");
                return;
            }
            const Uint32 bytes = static_cast<Uint32>(w) * static_cast<Uint32>(h) * 4u;
            if (!upload_ || uploadSize_ < bytes) {
                if (upload_)
                    SDL_ReleaseGPUTransferBuffer(device_, upload_);
                SDL_GPUTransferBufferCreateInfo info = {};
                info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                info.size = bytes;
                upload_ = SDL_CreateGPUTransferBuffer(device_, &info);
                uploadSize_ = upload_ ? bytes : 0;
                if (!upload_) {
                    Lose("no transfer buffer");
                    return;
                }
            }
            uint8_t* dst = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(device_, upload_, true));
            if (!dst) {
                Lose("cannot map the transfer buffer");
                return;
            }
            const uint8_t* src = reinterpret_cast<const uint8_t*>(xrgb);
            const size_t row = static_cast<size_t>(w) * 4u;
            for (int y = 0; y < h; y++)
                std::memcpy(dst + static_cast<size_t>(y) * row, src + static_cast<size_t>(y) * static_cast<size_t>(pitch), row);
            SDL_UnmapGPUTransferBuffer(device_, upload_);

            SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd_);
            SDL_GPUTextureTransferInfo from = {};
            from.transfer_buffer = upload_;
            from.pixels_per_row = static_cast<Uint32>(w);
            from.rows_per_layer = static_cast<Uint32>(h);
            SDL_GPUTextureRegion to = {};
            to.texture = frame_;
            to.w = static_cast<Uint32>(w);
            to.h = static_cast<Uint32>(h);
            to.d = 1;
            SDL_UploadToGPUTexture(copy, &from, &to, true);
            SDL_EndGPUCopyPass(copy);
            uploaded_ = true;
            lastFrameW_ = w;
            lastFrameH_ = h;
        }

        void DrawRegion(const compose::Region& region, Filter filter) override
        {
            if (!cmd_ || !uploaded_)
                return;
            const RECT& f = region.frame;
            const float fw = static_cast<float>(f.right - f.left);
            const float fh = static_cast<float>(f.bottom - f.top);
            if (fw <= 0.0f || fh <= 0.0f)
                return;
            SDL_FRect dest;
            SDL_Rect clip;
            if (region.scale <= 0) {
                dest = WholeFrameRect(frameW_, frameH_, outW_, outH_, letterbox);
                clip = { 0, 0, outW_, outH_ };
            } else {
                const float s = static_cast<float>(region.scale);
                const RECT& win = region.window;
                dest = { static_cast<float>(win.left) * pixelsX_, static_cast<float>(win.top) * pixelsY_,
                    fw * s * pixelsX_, fh * s * pixelsY_ };
                const int cx0 = static_cast<int>(std::lround(static_cast<float>(win.left) * pixelsX_));
                const int cy0 = static_cast<int>(std::lround(static_cast<float>(win.top) * pixelsY_));
                const int cx1 = static_cast<int>(std::lround(static_cast<float>(win.right) * pixelsX_));
                const int cy1 = static_cast<int>(std::lround(static_cast<float>(win.bottom) * pixelsY_));
                clip = { cx0, cy0, cx1 - cx0, cy1 - cy0 };
            }
            // The scissor: the destination's whole pixels, inside the region
            // and the target.
            const int x0 = std::max({ static_cast<int>(std::floor(dest.x)), clip.x, 0 });
            const int y0 = std::max({ static_cast<int>(std::floor(dest.y)), clip.y, 0 });
            const int x1 = std::min({ static_cast<int>(std::ceil(dest.x + dest.w)), clip.x + clip.w, outW_ });
            const int y1 = std::min({ static_cast<int>(std::ceil(dest.y + dest.h)), clip.y + clip.h, outH_ });
            if (x1 <= x0 || y1 <= y0)
                return;

            Draw d = {};
            d.filter = filter;
            d.viewport = { dest.x, dest.y, dest.w, dest.h, 0.0f, 1.0f };
            d.scissor = { x0, y0, x1 - x0, y1 - y0 };
            // The pixels rasterised: centres inside the viewport, in the
            // scissor. RCAS reads EASU's output only inside them.
            const int cx0 = std::max(x0, static_cast<int>(std::ceil(dest.x - 0.5f)));
            const int cy0 = std::max(y0, static_cast<int>(std::ceil(dest.y - 0.5f)));
            const int cx1 = std::min(x1, static_cast<int>(std::ceil(dest.x + dest.w - 0.5f)));
            const int cy1 = std::min(y1, static_cast<int>(std::ceil(dest.y + dest.h - 0.5f)));
            d.covered = { cx0, cy0, std::max(cx1 - cx0, 1), std::max(cy1 - cy0, 1) };
            d.u = {
                { static_cast<float>(f.left), static_cast<float>(f.top), static_cast<float>(f.right), static_cast<float>(f.bottom) },
                { dest.x, dest.y, dest.w / fw, dest.h / fh },
                { sharpness, 0.0f, 0.0f, 0.0f },
            };
            draws_.push_back(d);
        }

        void EndFrame() override
        {
            if (!cmd_)
                return;
            if (!EasuPass())
                return;
            if (!BeginPass(target_))
                return;
            for (const Draw& d : draws_)
                Issue(d);
            draws_.clear();
            SDL_EndGPURenderPass(pass_);
            pass_ = nullptr;
            if (swapchain_) {
                SDL_GPUBlitInfo blit = {};
                blit.source.texture = target_;
                blit.source.w = static_cast<Uint32>(outW_);
                blit.source.h = static_cast<Uint32>(outH_);
                blit.destination.texture = swapchain_;
                blit.destination.w = static_cast<Uint32>(outW_);
                blit.destination.h = static_cast<Uint32>(outH_);
                blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
                blit.filter = SDL_GPU_FILTER_NEAREST;
                SDL_BlitGPUTexture(cmd_, &blit);
            }
            SDL_GPUCommandBuffer* cmd = cmd_;
            cmd_ = nullptr;
            swapchain_ = nullptr;
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                Lose("submit failed");
                return;
            }
            if (!lost) {
                lastOutW_ = outW_;
                lastOutH_ = outH_;
                lastLetterbox_ = letterbox;
            }
        }

        bool Readback(std::vector<uint32_t>* xrgb, int* w, int* h) override
        {
            if (!device_ || !target_ || cmd_)
                return false;
            const Uint32 bytes = static_cast<Uint32>(targetW_) * static_cast<Uint32>(targetH_) * 4u;
            SDL_GPUTransferBufferCreateInfo info = {};
            info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            info.size = bytes;
            SDL_GPUTransferBuffer* down = SDL_CreateGPUTransferBuffer(device_, &info);
            if (!down)
                return false;
            bool ok = false;
            SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
            if (cmd) {
                SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
                SDL_GPUTextureRegion from = {};
                from.texture = target_;
                from.w = static_cast<Uint32>(targetW_);
                from.h = static_cast<Uint32>(targetH_);
                from.d = 1;
                SDL_GPUTextureTransferInfo to = {};
                to.transfer_buffer = down;
                to.pixels_per_row = static_cast<Uint32>(targetW_);
                to.rows_per_layer = static_cast<Uint32>(targetH_);
                SDL_DownloadFromGPUTexture(copy, &from, &to);
                SDL_EndGPUCopyPass(copy);
                SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
                if (fence) {
                    ok = SDL_WaitForGPUFences(device_, true, &fence, 1);
                    SDL_ReleaseGPUFence(device_, fence);
                }
            }
            if (ok) {
                const uint32_t* px = static_cast<const uint32_t*>(SDL_MapGPUTransferBuffer(device_, down, false));
                ok = px != nullptr;
                if (ok) {
                    // B, G, R, A bytes: XRGB8888 once alpha is dropped.
                    xrgb->assign(px, px + static_cast<size_t>(targetW_) * static_cast<size_t>(targetH_));
                    for (uint32_t& p : *xrgb)
                        p &= 0xFFFFFFu;
                    SDL_UnmapGPUTransferBuffer(device_, down);
                    *w = targetW_;
                    *h = targetH_;
                }
            }
            SDL_ReleaseGPUTransferBuffer(device_, down);
            return ok;
        }

        bool WindowToFrame(float x, float y, float* fx, float* fy) override
        {
            SDL_FRect r;
            if (!Placement(&r))
                return false;
            const float density = window_ ? SDL_GetWindowPixelDensity(window_) : 1.0f;
            *fx = (x * density - r.x) * static_cast<float>(lastFrameW_) / r.w;
            *fy = (y * density - r.y) * static_cast<float>(lastFrameH_) / r.h;
            return true;
        }

        bool FrameToWindow(float fx, float fy, float* x, float* y) override
        {
            SDL_FRect r;
            if (!Placement(&r))
                return false;
            const float density = window_ ? SDL_GetWindowPixelDensity(window_) : 1.0f;
            *x = (r.x + fx * r.w / static_cast<float>(lastFrameW_)) / density;
            *y = (r.y + fy * r.h / static_cast<float>(lastFrameH_)) / density;
            return true;
        }

        bool FrameSize(int* w, int* h) override
        {
            if (lastFrameW_ <= 0 || lastFrameH_ <= 0)
                return false;
            *w = lastFrameW_;
            *h = lastFrameH_;
            return true;
        }

    private:
        bool Lose(const char* why)
        {
            lost = true;
            error = why;
            const char* sdl = SDL_GetError();
            if (sdl && *sdl && std::strstr(why, "test hook") == nullptr)
                error += std::string(": ") + sdl;
            return false;
        }

        void Abandon()
        {
            if (!cmd_)
                return;
            if (swapchain_)
                SDL_SubmitGPUCommandBuffer(cmd_); // an acquired swapchain cannot be cancelled
            else
                SDL_CancelGPUCommandBuffer(cmd_);
            cmd_ = nullptr;
            swapchain_ = nullptr;
        }

        bool Placement(SDL_FRect* r) const
        {
            if (lastFrameW_ <= 0 || lastFrameH_ <= 0 || lastOutW_ <= 0 || lastOutH_ <= 0)
                return false;
            *r = WholeFrameRect(lastFrameW_, lastFrameH_, lastOutW_, lastOutH_, lastLetterbox_);
            return r->w > 0.0f && r->h > 0.0f;
        }

        // FSR's first pass: every FSR region's EASU into easu_.
        bool EasuPass()
        {
            const bool any = std::any_of(draws_.begin(), draws_.end(), [](const Draw& d) { return Describe(d.filter).easu; });
            if (!any)
                return true;
            if (!EnsureTexture(easu_, easuW_, easuH_, outW_, outH_, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER)) {
                Abandon();
                Lose("no FSR intermediate texture");
                return false;
            }
            if (!BeginPass(easu_))
                return false;
            for (const Draw& d : draws_) {
                if (!Describe(d.filter).easu)
                    continue;
                SDL_BindGPUGraphicsPipeline(pass_, pipelines_[kEasu]);
                SDL_SetGPUViewport(pass_, &d.viewport);
                SDL_SetGPUScissor(pass_, &d.scissor);
                SDL_GPUTextureSamplerBinding binding = {};
                binding.texture = frame_;
                binding.sampler = samplers_[0];
                SDL_BindGPUFragmentSamplers(pass_, 0, &binding, 1);
                SDL_PushGPUFragmentUniformData(cmd_, 0, &d.u, sizeof(d.u));
                SDL_DrawGPUPrimitives(pass_, 3, 1, 0, 0);
            }
            SDL_EndGPURenderPass(pass_);
            pass_ = nullptr;
            return true;
        }

        // One region into the target.
        void Issue(const Draw& d)
        {
            const FilterInfo info = Describe(d.filter);
            // The test hook copies EASU's output with the nearest shader.
            const bool easuOnly = info.easu && testing::SkipRcasFlag();
            const int index = easuOnly ? kNearest : info.pipeline;
            SDL_GPUTexture* texture = info.easu ? easu_ : frame_;
            SDL_GPUSampler* sampler = samplers_[info.linear ? 1 : 0];
            SDL_BindGPUGraphicsPipeline(pass_, pipelines_[index]);
            SDL_SetGPUViewport(pass_, &d.viewport);
            SDL_SetGPUScissor(pass_, &d.scissor);
            SDL_GPUTextureSamplerBinding binding = {};
            binding.texture = texture;
            binding.sampler = sampler;
            SDL_BindGPUFragmentSamplers(pass_, 0, &binding, 1);
            if (easuOnly) {
                const SDL_Rect& c = d.covered;
                const RegionUniforms copy = {
                    { static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.x + c.w), static_cast<float>(c.y + c.h) },
                    { static_cast<float>(c.x), static_cast<float>(c.y), 1.0f, 1.0f },
                    { 0.0f, 0.0f, 0.0f, 0.0f },
                };
                SDL_PushGPUFragmentUniformData(cmd_, 0, &copy, sizeof(copy));
            } else if (info.easu) {
                const SDL_Rect& c = d.covered;
                const RcasUniforms r = {
                    { static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.x + c.w), static_cast<float>(c.y + c.h) },
                    { d.u.params[0], 0.0f, 0.0f, 0.0f },
                };
                SDL_PushGPUFragmentUniformData(cmd_, 0, &r, sizeof(r));
            } else {
                SDL_PushGPUFragmentUniformData(cmd_, 0, &d.u, sizeof(d.u));
                if (info.coefficients)
                    SDL_PushGPUFragmentUniformData(cmd_, 1, NisCoefficients(), static_cast<Uint32>(NisCoefficientsSize()));
            }
            SDL_DrawGPUPrimitives(pass_, 3, 1, 0, 0);
        }

        bool BeginPass(SDL_GPUTexture* texture)
        {
            SDL_GPUColorTargetInfo target = {};
            target.texture = texture;
            target.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            target.cycle = true;
            pass_ = SDL_BeginGPURenderPass(cmd_, &target, 1, nullptr);
            if (!pass_) {
                Abandon();
                Lose("cannot begin the render pass");
                return false;
            }
            return true;
        }

        bool EnsureTexture(SDL_GPUTexture*& texture, int& tw, int& th, int w, int h, SDL_GPUTextureUsageFlags usage)
        {
            if (texture && tw == w && th == h)
                return true;
            if (texture)
                SDL_ReleaseGPUTexture(device_, texture);
            SDL_GPUTextureCreateInfo info = {};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = kFormat;
            info.usage = usage;
            info.width = static_cast<Uint32>(w);
            info.height = static_cast<Uint32>(h);
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            info.sample_count = SDL_GPU_SAMPLECOUNT_1;
            texture = SDL_CreateGPUTexture(device_, &info);
            tw = texture ? w : 0;
            th = texture ? h : 0;
            return texture != nullptr;
        }

        SDL_GPUShader* MakeShader(const char* name, SDL_GPUShaderStage stage, Uint32 samplers, Uint32 uniforms)
        {
            const unsigned char* code = nullptr;
            size_t size = 0;
            if (!ShaderCode(name, &code, &size))
                return nullptr;
            SDL_GPUShaderCreateInfo info = {};
            info.code_size = size;
            info.code = code;
            info.entrypoint = "main";
            info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            info.stage = stage;
            info.num_samplers = samplers;
            info.num_uniform_buffers = uniforms;
            return SDL_CreateGPUShader(device_, &info);
        }

        bool CreatePipelines()
        {
            if (testing::CurrentGpuFault() == testing::GpuFault::Pipeline) {
                error = "the graphics pipeline cannot be created (test hook)";
                return false;
            }
            SDL_GPUShader* vert = MakeShader("quad.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0);
            if (!vert) {
                error = std::string("the quad shader does not load: ") + SDL_GetError();
                return false;
            }
            bool ok = true;
            for (int i = 0; i < kPipelineCount && ok; i++) {
                const char* name = kPipelines[i].fragment;
                SDL_GPUShader* frag = MakeShader(name, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, kPipelines[i].uniformBuffers);
                if (!frag) {
                    error = std::string(name) + " does not load: " + SDL_GetError();
                    ok = false;
                    break;
                }
                SDL_GPUColorTargetDescription color = {};
                color.format = kFormat;
                SDL_GPUGraphicsPipelineCreateInfo info = {};
                info.vertex_shader = vert;
                info.fragment_shader = frag;
                info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                info.target_info.color_target_descriptions = &color;
                info.target_info.num_color_targets = 1;
                pipelines_[i] = SDL_CreateGPUGraphicsPipeline(device_, &info);
                SDL_ReleaseGPUShader(device_, frag);
                if (!pipelines_[i]) {
                    error = std::string("the ") + name + " pipeline cannot be created: " + SDL_GetError();
                    ok = false;
                }
            }
            SDL_ReleaseGPUShader(device_, vert);
            return ok;
        }

        bool CreateSamplers()
        {
            const SDL_GPUFilter filters[2] = { SDL_GPU_FILTER_NEAREST, SDL_GPU_FILTER_LINEAR };
            for (int i = 0; i < 2; i++) {
                SDL_GPUSamplerCreateInfo info = {};
                info.min_filter = filters[i];
                info.mag_filter = filters[i];
                info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
                info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                samplers_[i] = SDL_CreateGPUSampler(device_, &info);
                if (!samplers_[i]) {
                    error = std::string("no sampler: ") + SDL_GetError();
                    return false;
                }
            }
            return true;
        }

        SDL_GPUDevice* device_ = nullptr;
        SDL_Window* window_ = nullptr;
        SDL_GPUGraphicsPipeline* pipelines_[kPipelineCount] = {};
        SDL_GPUSampler* samplers_[2] = {}; // nearest, linear
        SDL_GPUTexture* frame_ = nullptr;
        int frameW_ = 0;
        int frameH_ = 0;
        SDL_GPUTransferBuffer* upload_ = nullptr;
        Uint32 uploadSize_ = 0;
        SDL_GPUTexture* target_ = nullptr;
        int targetW_ = 0;
        int targetH_ = 0;
        SDL_GPUTexture* easu_ = nullptr; // FSR: EASU's output, the target's size
        int easuW_ = 0;
        int easuH_ = 0;
        std::vector<Draw> draws_;
        // The frame being recorded.
        SDL_GPUCommandBuffer* cmd_ = nullptr;
        SDL_GPUTexture* swapchain_ = nullptr;
        SDL_GPURenderPass* pass_ = nullptr;
        bool uploaded_ = false;
        int outW_ = 0;
        int outH_ = 0;
        float pixelsX_ = 1.0f; // output pixels per window point
        float pixelsY_ = 1.0f;
        // The last frame shown, for the whole-frame mouse mapping.
        int lastFrameW_ = 0;
        int lastFrameH_ = 0;
        int lastOutW_ = 0;
        int lastOutH_ = 0;
        bool lastLetterbox_ = false;
    };

}

std::unique_ptr<Backend> MakeGpuBackend() { return std::make_unique<GpuBackend>(); }

}
