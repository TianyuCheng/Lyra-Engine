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
            GPUDevice        device,
            Compiler         compiler,
            GPUTextureFormat depth_format = GPUTextureFormat::DEPTH32FLOAT);

        /**
         * @brief safely destroys GPU pipeline and buffers.
         */
        void destroy();

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
        void update_camera_uniform(const RenderView& view);

    private:
        GPUDevice                        device;
        GPURenderPipeline                pipeline;
        GPUPipelineLayout                playout;
        GPUShaderModule                  vshader;
        GPUShaderModule                  fshader;
        GPUBuffer                        camera_buffer;
        GPUBindGroupHeap                 heap;
        GPUBindGroup                     bind_group;
        Vector<GPUBindGroupLayoutHandle> blayouts;

        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H
