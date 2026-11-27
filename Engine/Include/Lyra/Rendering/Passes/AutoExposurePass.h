#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_AUTO_EXPOSURE_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_AUTO_EXPOSURE_PASS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/PipelineManager.h>

namespace lyra::passes
{
    /**
     * @brief settings for the histogram auto-exposure and eye adaptation pass.
     */
    struct AutoExposureSettings
    {
        float min_ev       = -4.0f; // minimum EV100 clamp (darkness limit)
        float max_ev       = 16.0f; // maximum EV100 clamp (bright sunlight limit)
        float speed_up     = 3.0f;  // adaptation speed from dark to bright (faster)
        float speed_down   = 1.0f;  // adaptation speed from bright to dark (slower)
        float low_percent  = 0.10f; // low percentile to ignore (discard darkest 10%)
        float high_percent = 0.90f; // high percentile to ignore (discard brightest 10%)
        bool  enabled      = true;  // auto exposure toggle
    };

    /**
     * @brief outputs of the auto exposure pass.
     */
    struct AutoExposureData
    {
        FrameGraphHandle<FrameGraphTexture> hdr_input;
        FrameGraphHandle<FrameGraphTexture> exposure_data; // 1x1 texture containing { exposure_scale, ev100, avg_luminance, 1.0 }
    };

    /**
     * @brief 64-bin luminance histogram compute pass + temporal eye adaptation.
     */
    struct AutoExposurePass
    {
    public:
        explicit AutoExposurePass() = default;
        ~AutoExposurePass()         = default;

        AutoExposurePass(const AutoExposurePass&)            = delete;
        AutoExposurePass& operator=(const AutoExposurePass&) = delete;

        AutoExposurePass(AutoExposurePass&&) noexcept            = default;
        AutoExposurePass& operator=(AutoExposurePass&&) noexcept = default;

        bool init(GPUDevice device, Compiler compiler);
        void destroy();

        bool valid() const { return initialized && histogram_pipeline.valid() && adapt_pipeline.valid(); }

        AutoExposureData add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> hdr_input,
            GPUExtent2D                         extent,
            float                               delta_time,
            const AutoExposureSettings&         settings = {});

        [[nodiscard]] GPUTexture     get_exposure_texture() const { return exposure_texture; }
        [[nodiscard]] GPUTextureView get_exposure_view() const { return exposure_view; }

    private:
        struct alignas(16) AutoExposurePushConstants
        {
            float min_ev        = -4.0f;
            float max_ev        = 16.0f;
            float speed_up      = 3.0f;
            float speed_down    = 1.0f;
            float low_percent   = 0.10f;
            float high_percent  = 0.90f;
            float delta_time    = 0.016f;
            uint  pixel_count   = 0;
            uint  screen_width  = 0;
            uint  screen_height = 0;
            float padding[2]    = {0.0f, 0.0f};
        };

        static_assert(sizeof(AutoExposurePushConstants) == 48, "AutoExposurePushConstants must be 48 bytes");

        GPUDevice                        device;
        PipelineManager                  pipeline_manager;
        GPUComputePipeline               histogram_pipeline;
        GPUComputePipeline               adapt_pipeline;
        Vector<GPUBindGroupLayoutHandle> histogram_blayouts;
        Vector<GPUBindGroupLayoutHandle> adapt_blayouts;

        // persistent state across frames
        GPUBuffer      histogram_buffer; // 64 uints (256 bytes)
        GPUTexture     exposure_texture; // 1x1 RGBA16FLOAT
        GPUTextureView exposure_view;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_AUTO_EXPOSURE_PASS_H
