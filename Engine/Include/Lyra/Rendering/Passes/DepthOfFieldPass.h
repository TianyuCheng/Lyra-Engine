#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_DEPTH_OF_FIELD_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_DEPTH_OF_FIELD_PASS_H

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
     * @brief settings for physical depth of field bokeh blur pass.
     */
    struct DOFSettings
    {
        float focal_distance  = 5.0f;  // distance in world units to the in-focus plane
        float focal_length    = 0.05f; // 50mm focal length lens (0.05m)
        float aperture_f_stop = 2.8f;  // lens f-number N (e.g. f/1.4, f/2.8, f/5.6)
        float max_coc_radius  = 16.0f; // maximum blur radius in pixels
        bool  enabled         = true;
    };

    /**
     * @brief outputs of the depth of field pass.
     */
    struct DOFData
    {
        FrameGraphHandle<FrameGraphTexture> color_input;
        FrameGraphHandle<FrameGraphTexture> depth_input;
        FrameGraphHandle<FrameGraphTexture> dof_output;
    };

    /**
     * @brief Physical depth of field with circle of confusion and golden spiral bokeh disk sampling.
     */
    struct DepthOfFieldPass
    {
    public:
        explicit DepthOfFieldPass() = default;
        ~DepthOfFieldPass()         = default;

        DepthOfFieldPass(const DepthOfFieldPass&)            = delete;
        DepthOfFieldPass& operator=(const DepthOfFieldPass&) = delete;

        DepthOfFieldPass(DepthOfFieldPass&&) noexcept            = default;
        DepthOfFieldPass& operator=(DepthOfFieldPass&&) noexcept = default;

        bool init(GPUDevice device, Compiler compiler);
        void destroy();

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> color_texture,
            FrameGraphHandle<FrameGraphTexture> depth_texture,
            GPUExtent2D                         extent,
            float                               near_plane,
            float                               far_plane,
            const DOFSettings&                  settings = {}) -> DOFData;

    private:
        struct alignas(16) DOFPushConstants
        {
            Vector2 inv_screen_size;
            float   focal_distance  = 5.0f;
            float   focal_length    = 0.05f;
            float   aperture_f_stop = 2.8f;
            float   max_coc_radius  = 16.0f;
            float   near_plane      = 0.1f;
            float   far_plane       = 100.0f;
            uint    screen_width    = 0;
            uint    screen_height   = 0;
            float   padding[2]      = {0.0f, 0.0f};
        };

        static_assert(sizeof(DOFPushConstants) == 48, "DOFPushConstants must be 48 bytes");

        GPUDevice                        device;
        PipelineManager                  pipeline_manager;
        GPUComputePipeline               pipeline;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        GPUSampler                       linear_sampler;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_DEPTH_OF_FIELD_PASS_H
