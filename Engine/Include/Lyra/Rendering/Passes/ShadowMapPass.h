#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/GeometryBindGroupCache.h>
#include <Lyra/Rendering/PipelineManager.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Path.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the cascaded shadow mapping pass.
     */
    struct ShadowMapData
    {
        FrameGraphHandle<FrameGraphTexture> shadow_map_array;
    };

    /**
     * @brief Cascaded shadow mapping pass renders scene geometry from the directional light's perspective
     * into layered shadow map cascades.
     */
    struct ShadowMapPass
    {
    public:
        explicit ShadowMapPass() = default;
        virtual ~ShadowMapPass() = default;

        ShadowMapPass(const ShadowMapPass&)            = delete;
        ShadowMapPass& operator=(const ShadowMapPass&) = delete;

        ShadowMapPass(ShadowMapPass&&) noexcept            = default;
        ShadowMapPass& operator=(ShadowMapPass&&) noexcept = default;

        bool init(
            GPUDevice                   device,
            Compiler                    compiler,
            GPUTextureFormat            depth_format = GPUTextureFormat::DEPTH32FLOAT,
            const Path&                 shaders_root = "sys://Shaders",
            Ref<GeometryBindGroupCache> cache        = nullptr);

        void destroy();

        void               set_geometry_cache(Ref<GeometryBindGroupCache> cache) { geometry_cache = cache; }
        [[nodiscard]] auto get_geometry_cache() const -> Ref<GeometryBindGroupCache> { return geometry_cache; }
        [[nodiscard]] auto get_vertex_layout() const -> GPUBindGroupLayoutHandle { return blayouts.size() > 1 ? blayouts[1] : GPUBindGroupLayoutHandle{}; }

        bool               valid() const { return initialized && pipeline.valid(); }
        [[nodiscard]] auto get_light_view_proj() const -> const Matrix4x4& { return light_vp; }

        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            uint               cascade_count = 1,
            uint               resolution    = 2048) -> ShadowMapData;

    private:
        struct ShadowCascadeData
        {
            Matrix4x4 light_view_proj;
        };

        /**
         * @brief Per-instance immediate push constants for shadow depth vertex pulling.
         * @note Memory layout directly mirrors the GPU shader layout of struct `InstanceData` in
         *       `sys://Shaders/Forward/ShadowDepth.slang`. Any layout modification must be kept strictly
         *       in sync across both C++ and Slang shader definitions.
         */
        struct alignas(16) InstanceData
        {
            Matrix4x4 model;
            uint      position_offset = 0;
            uint      padding0        = 0;
            uint      padding1        = 0;
            uint      padding2        = 0;
        };
        static_assert(sizeof(InstanceData) == 80);
        static_assert(offsetof(InstanceData, position_offset) == 64);

        GPUDevice                        device;
        PipelineManager                  pipeline_manager;
        GPURenderPipeline                pipeline;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        Ref<GeometryBindGroupCache>      geometry_cache;
        GPUTextureFormat                 cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        Matrix4x4                        light_vp{1.0f};
        bool                             initialized = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H
