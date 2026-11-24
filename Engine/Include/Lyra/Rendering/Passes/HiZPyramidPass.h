#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_HIZ_PYRAMID_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_HIZ_PYRAMID_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Path.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the hierarchical Z depth pyramid pass.
     */
    struct HiZPyramidData
    {
        FrameGraphHandle<FrameGraphTexture> depth_input;
        FrameGraphHandle<FrameGraphTexture> hiz_texture;
    };

    /**
     * @brief Hi-Z Pyramid pass generates a hierarchical depth buffer pyramid via compute downsampling,
     * used for fast GPU-driven occlusion culling and screen-space ray marching.
     */
    struct HiZPyramidPass
    {
    public:
        explicit HiZPyramidPass() = default;
        virtual ~HiZPyramidPass() = default;

        HiZPyramidPass(const HiZPyramidPass&)            = delete;
        HiZPyramidPass& operator=(const HiZPyramidPass&) = delete;

        HiZPyramidPass(HiZPyramidPass&&) noexcept            = default;
        HiZPyramidPass& operator=(HiZPyramidPass&&) noexcept = default;

        bool init(GPUDevice device, Compiler compiler);
        void destroy();

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

        auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> depth_texture,
            GPUExtent2D                         depth_extent) -> HiZPyramidData;

    private:
        struct HiZPushConstants
        {
            uint src_width;
            uint src_height;
            uint dst_width;
            uint dst_height;
            uint is_reverse_z;
            uint padding[3];
        };

        GPUDevice                        device;
        GPUComputePipeline               pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  cshader;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        bool                             initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_HIZ_PYRAMID_PASS_H
