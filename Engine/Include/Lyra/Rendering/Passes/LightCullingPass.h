#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
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

        [[nodiscard]] auto get_lights_buffer() const -> GPUBuffer { return lights_buffer; }
        [[nodiscard]] auto get_lights_buffer_ptr() const -> const GPUBuffer* { return &lights_buffer; }

        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene) -> LightCullingData;

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

        /**
         * @brief GPU light data layout matching struct `LightData` in `sys://Shaders/Common/Lighting.slang`.
         * Total size: exactly 64 bytes. Aligned to 16 bytes.
         */
        struct alignas(16) GPULightData
        {
            Vector3 position;
            float   range = 0.0f;
            Vector3 direction;
            float   intensity        = 1.0f;
            Vector3 color            = Vector3(1.0f);
            uint    type             = 0;
            float   spot_inner_angle = 0.0f;
            float   spot_outer_angle = 0.0f;
            uint    cast_shadows     = 0;
            uint    profile          = 0;
        };

        static_assert(sizeof(GPULightData) == 64, "GPULightData must be 64 bytes");
        static_assert(alignof(GPULightData) == 16, "GPULightData must be 16-byte aligned");

        GPUDevice                        device;
        GPUComputePipeline               pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  cshader;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        GPUBindGroupHeap                 heap;
        GPUBuffer                        lights_buffer;
        bool                             initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H
