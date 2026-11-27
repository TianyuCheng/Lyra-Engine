#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H
#define LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/GeometryBindGroupCache.h>
#include <Lyra/Rendering/PipelineManager.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the depth prepass.
     */
    struct DepthPrepassData
    {
        FrameGraphHandle<FrameGraphTexture> depth_texture;
    };

    /**
     * @brief Composable depth prepass eliminates overdraw by rasterizing opaque geometry to the depth buffer
     * with color writes disabled before forward/deferred shading.
     */
    struct DepthPrepass
    {
    public:
        explicit DepthPrepass() = default;
        virtual ~DepthPrepass() = default;

        DepthPrepass(const DepthPrepass&)            = delete;
        DepthPrepass& operator=(const DepthPrepass&) = delete;

        DepthPrepass(DepthPrepass&&) noexcept            = default;
        DepthPrepass& operator=(DepthPrepass&&) noexcept = default;

        /**
         * @brief initializes depth-only rasterization pipeline and layouts.
         */
        bool init(
            GPUDevice                   device,
            Compiler                    compiler,
            GPUTextureFormat            depth_format = GPUTextureFormat::DEPTH32FLOAT,
            const Path&                 shaders_root = "sys://Shaders",
            Ref<GeometryBindGroupCache> cache        = nullptr);

        /**
         * @brief safely destroys GPU pipeline and buffers.
         */
        void destroy();

        void               set_geometry_cache(Ref<GeometryBindGroupCache> cache) { geometry_cache = cache; }
        [[nodiscard]] auto get_geometry_cache() const -> Ref<GeometryBindGroupCache> { return geometry_cache; }
        [[nodiscard]] auto get_vertex_layout() const -> GPUBindGroupLayoutHandle { return blayouts.size() > 1 ? blayouts[1] : GPUBindGroupLayoutHandle{}; }

        /**
         * @brief records depth prepass into the frame graph builder.
         */
        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            GPUExtent2D        extent) -> DepthPrepassData;

        [[nodiscard]] bool valid() const { return initialized && pipeline.valid(); }

    private:
        /**
         * @brief Per-instance immediate push constants for depth prepass vertex pulling.
         * @note Memory layout directly mirrors the GPU shader layout of struct `InstanceData` in
         *       `sys://Shaders/Forward/DepthPrepass.slang`. Any layout modification must be kept strictly
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

    private:
        GPUDevice                        device;
        PipelineManager                  pipeline_manager;
        GPURenderPipeline                pipeline;
        Vector<GPUBindGroupLayoutHandle> blayouts;
        Ref<GeometryBindGroupCache>      geometry_cache;

        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H
