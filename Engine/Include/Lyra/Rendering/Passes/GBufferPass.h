#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H

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
     * @brief outputs of the G-Buffer pass for deferred and hybrid rendering.
     */
    struct GBufferData
    {
        FrameGraphHandle<FrameGraphTexture> albedo_metallic;  ///< RGBA8UNORM: RGB albedo, A metallic
        FrameGraphHandle<FrameGraphTexture> normal_roughness; ///< RGBA16FLOAT: RGB encoded world normal, A roughness
        FrameGraphHandle<FrameGraphTexture> material_info;    ///< RGBA32UINT: R shading model, G material ID, BA custom
        FrameGraphHandle<FrameGraphTexture> depth_stencil;    ///< DEPTH32FLOAT or DEPTH24PLUS_STENCIL8
    };

    /**
     * @brief Composable G-Buffer pass rasterizes primary geometry into multi-target render buffers
     * storing surface material properties, supporting both standard PBR and custom shading models.
     */
    struct GBufferPass
    {
    public:
        explicit GBufferPass() = default;
        virtual ~GBufferPass() = default;

        GBufferPass(const GBufferPass&)            = delete;
        GBufferPass& operator=(const GBufferPass&) = delete;

        GBufferPass(GBufferPass&&) noexcept            = default;
        GBufferPass& operator=(GBufferPass&&) noexcept = default;

        /**
         * @brief initializes G-Buffer multi-target render pipeline and layouts.
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
         * @brief records G-Buffer pass into the frame graph builder.
         */
        auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            GPUExtent2D        extent) -> GBufferData;

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

#endif // LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H
