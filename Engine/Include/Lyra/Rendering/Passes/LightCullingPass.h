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

        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene) -> LightCullingData;

    private:
        struct LightCullingPushConstants
        {
            Matrix4x4 view_matrix;
            Matrix4x4 proj_matrix;
            Matrix4x4 inv_proj_matrix;
            Vector2   screen_resolution;
            float     near_plane;
            float     far_plane;
            uint      cluster_grid_x;
            uint      cluster_grid_y;
            uint      cluster_grid_z;
            uint      num_lights;
        };

        struct GPULightData
        {
            Vector3 position;
            float   range;
            Vector3 direction;
            float   intensity;
            Vector3 color;
            uint    type;
            float   spot_inner_angle;
            float   spot_outer_angle;
            uint    cast_shadows;
            uint    profile;
        };

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
