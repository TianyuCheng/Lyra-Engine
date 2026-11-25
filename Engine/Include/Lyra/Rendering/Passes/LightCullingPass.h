#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/SceneLighting.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Path.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the clustered light culling pass for Forward+ and Clustered Shading.
     */
    struct LightCullingData
    {
        FrameGraphHandle<FrameGraphBuffer> cluster_grid_buffer;
        FrameGraphHandle<FrameGraphBuffer> light_index_list;
        uint                               cluster_grid_x = 0;
        uint                               cluster_grid_y = 0;
        uint                               cluster_grid_z = 24;
    };

    /**
     * @brief Light culling pass divides camera view frustum into a 3D cluster grid (e.g. 16x9x24)
     * and assigns analytical lights (point, spot) to each cluster via compute shaders.
     */
    struct LightCullingPass
    {
    public:
        explicit LightCullingPass() = default;
        virtual ~LightCullingPass() = default;

        LightCullingPass(const LightCullingPass&)            = delete;
        LightCullingPass& operator=(const LightCullingPass&) = delete;

        LightCullingPass(LightCullingPass&&) noexcept            = default;
        LightCullingPass& operator=(LightCullingPass&&) noexcept = default;

        bool init(GPUDevice device, Compiler compiler);
        void destroy();

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

        [[nodiscard]] auto get_lights_slice() const -> GPUBufferSlice { return lights_slice; }

        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            GPUBufferSlice     in_lights_slice = {}) -> LightCullingData;

    private:
        /**
         * @brief Clustered light culling push constants matching `LightCullingParams` in `sys://Shaders/Forward/LightCulling.slang`.
         * Total size: exactly 224 bytes. Aligned to 16 bytes.
         */
        struct alignas(16) LightCullingPushConstants
        {
            Matrix4x4 view_matrix;
            Matrix4x4 proj_matrix;
            Matrix4x4 inv_proj_matrix;
            Vector2   screen_resolution;
            float     near_plane     = 0.1f;
            float     far_plane      = 100.0f;
            uint      cluster_grid_x = 16;
            uint      cluster_grid_y = 9;
            uint      cluster_grid_z = 24;
            uint      num_lights     = 0;
        };

        static_assert(sizeof(LightCullingPushConstants) == 224, "LightCullingPushConstants must be 224 bytes");
        static_assert(alignof(LightCullingPushConstants) == 16, "LightCullingPushConstants must be 16-byte aligned");

        GPUDevice                        device;
        GPUComputePipeline               pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  cshader;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        GPUBufferSlice                   lights_slice{};
        bool                             initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H
