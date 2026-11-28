#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_FXAA_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_FXAA_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/PipelineManager.h>

namespace lyra::passes
{
    /**
     * @brief settings for Fast Approximate Anti-Aliasing (FXAA).
     */
    struct FXAASettings
    {
        float subpixel_quality   = 0.75f;   // 0.75 standard, 1.0 maximum sharpness
        float edge_threshold     = 0.166f;  // standard edge detection threshold
        float edge_threshold_min = 0.0833f; // dark/low-contrast edge threshold
        bool  enabled            = true;
    };

    /**
     * @brief outputs of the FXAA pass.
     */
    struct FXAAData
    {
        FrameGraphHandle<FrameGraphTexture> color_input;
        FrameGraphHandle<FrameGraphTexture> color_output;
    };

    /**
     * @brief Fast Approximate Anti-Aliasing (FXAA) fullscreen raster pass.
     */
    struct FXAAPass
    {
    public:
        explicit FXAAPass() = default;
        ~FXAAPass()         = default;

        FXAAPass(const FXAAPass&)            = delete;
        FXAAPass& operator=(const FXAAPass&) = delete;

        FXAAPass(FXAAPass&&) noexcept            = default;
        FXAAPass& operator=(FXAAPass&&) noexcept = default;

        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat output_format = GPUTextureFormat::RGBA8UNORM);

        void destroy();

        bool valid() const { return initialized && pipeline.valid(); }

        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> input_color,
            GPUExtent2D                         extent,
            GPUTextureFormat                    format        = GPUTextureFormat::RGBA8UNORM,
            const FXAASettings&                 settings      = {},
            FrameGraphHandle<FrameGraphTexture> output_target = {}) -> FXAAData;

    private:
        struct alignas(16) FXAAPushConstants
        {
            Vector2 inv_screen_size;
            float   subpixel_quality   = 0.75f;
            float   edge_threshold     = 0.166f;
            float   edge_threshold_min = 0.0833f;
            uint    screen_width       = 0;
            uint    screen_height      = 0;
            float   padding            = 0.0f;
        };

        static_assert(sizeof(FXAAPushConstants) == 32, "FXAAPushConstants must be 32 bytes");

        GPUDevice        device;
        PipelineBundle   pipeline;
        GPUSampler       linear_sampler;
        GPUTextureFormat cached_output_format = GPUTextureFormat::RGBA8UNORM;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_FXAA_PASS_H
