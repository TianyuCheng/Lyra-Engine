#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/PipelineManager.h>

namespace lyra::passes
{
    /**
     * @brief settings for the tonemapping and color grading pass.
     */
    struct TonemappingSettings
    {
        float exposure          = 1.0f;
        uint  operator_type     = 0;     // 0 = ACES, 1 = Reinhard, 2 = AgX, 3 = Uncharted2
        float bloom_intensity   = 1.0f;  // bloom multiplier
        bool  use_auto_exposure = false; // whether to sample exposure from exposure_texture
    };

    /**
     * @brief outputs of the tonemapping pass.
     */
    struct TonemappingData
    {
        FrameGraphHandle<FrameGraphTexture> hdr_input;
        FrameGraphHandle<FrameGraphTexture> bloom_input;
        FrameGraphHandle<FrameGraphTexture> exposure_input;
        FrameGraphHandle<FrameGraphTexture> ldr_output;
    };

    /**
     * @brief Tonemapping pass maps high dynamic range (HDR) radiance values
     * to standard low dynamic range (LDR) display color space with bloom composition,
     * auto exposure, and AgX / ACES tone curve reproduction.
     */
    struct TonemappingPass
    {
    public:
        explicit TonemappingPass() = default;
        ~TonemappingPass()         = default;

        TonemappingPass(const TonemappingPass&)            = delete;
        TonemappingPass& operator=(const TonemappingPass&) = delete;

        TonemappingPass(TonemappingPass&&) noexcept            = default;
        TonemappingPass& operator=(TonemappingPass&&) noexcept = default;

        /**
         * @brief initializes tonemapping pipeline and sampler.
         */
        bool init(
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat output_format = GPUTextureFormat::RGBA8UNORM);

        /**
         * @brief safely destroys GPU pipeline and sampler states.
         */
        void destroy();

        /**
         * @brief records the tonemapping pass into the frame graph builder.
         */
        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> hdr_input,
            GPUExtent2D                         extent,
            FrameGraphHandle<FrameGraphTexture> bloom_input    = {},
            FrameGraphHandle<FrameGraphTexture> exposure_input = {},
            const TonemappingSettings&          settings       = {},
            GPUTextureFormat                    output_format  = GPUTextureFormat::RGBA8UNORM,
            FrameGraphHandle<FrameGraphTexture> output_target  = {}) -> TonemappingData;

        bool valid() const { return initialized && pipeline.valid(); }

    private:
        GPUDevice      device;
        PipelineBundle pipeline;
        GPUSampler     sampler;

        // fallback 1x1 textures when bloom or auto-exposure are not supplied
        GPUTexture     default_bloom_tex;
        GPUTextureView default_bloom_view;
        GPUTexture     default_exposure_tex;
        GPUTextureView default_exposure_view;

        GPUTextureFormat cached_output_format = GPUTextureFormat::RGBA8UNORM;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H
