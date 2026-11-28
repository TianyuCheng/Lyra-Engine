#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_BLOOM_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_BLOOM_PASS_H

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
     * @brief settings for the dual-filtering threshold bloom pass.
     */
    struct BloomSettings
    {
        float threshold       = 1.0f; // luminance threshold for bright pass
        float knee            = 0.5f; // soft knee threshold falloff [0, 1]
        float intensity       = 1.0f; // bloom glow intensity multiplier
        float upsample_radius = 1.0f; // upsample tent filter radius scale
        bool  enabled         = true;
    };

    /**
     * @brief outputs of the bloom pass.
     */
    struct BloomData
    {
        FrameGraphHandle<FrameGraphTexture>           input_color;
        Array<FrameGraphHandle<FrameGraphTexture>, 5> down_mips;
        Array<FrameGraphHandle<FrameGraphTexture>, 4> up_mips;
        FrameGraphHandle<FrameGraphTexture>           bloom_output;
    };

    /**
     * @brief Dual-filtering threshold bloom pass (Jimenez 2014).
     * 5-level downsample pyramid with 13-tap Karis anti-flicker filter and soft knee,
     * followed by 5-level upsample pyramid with 9-tap tent filter and additive blending.
     */
    struct BloomPass
    {
    public:
        explicit BloomPass() = default;
        ~BloomPass()         = default;

        BloomPass(const BloomPass&)            = delete;
        BloomPass& operator=(const BloomPass&) = delete;

        BloomPass(BloomPass&&) noexcept            = default;
        BloomPass& operator=(BloomPass&&) noexcept = default;

        bool init(GPUDevice device, Compiler compiler);
        void destroy();

        bool valid() const { return initialized && downsample_pipeline.valid() && upsample_pipeline.valid(); }

        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> input_color,
            GPUExtent2D                         extent,
            const BloomSettings&                settings = {}) -> BloomData;

    private:
        struct alignas(16) BloomPushConstants
        {
            Vector2 inv_src_size;
            Vector2 inv_dst_size;
            Vector4 threshold; // x: threshold, y: threshold - knee, z: 2*knee, w: 0.25/knee
            float   bloom_intensity = 1.0f;
            float   upsample_radius = 1.0f;
            uint    dst_width       = 0;
            uint    dst_height      = 0;
        };

        static_assert(sizeof(BloomPushConstants) == 48, "BloomPushConstants must be 48 bytes");

        GPUDevice             device;
        ComputePipelineBundle downsample_pipeline;
        ComputePipelineBundle upsample_pipeline;
        GPUSampler            linear_sampler;

        bool initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_BLOOM_PASS_H
