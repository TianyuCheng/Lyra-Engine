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
#include <Lyra/Rendering/Pipeline.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the G-Buffer pass for deferred and hybrid rendering.
     */
    struct GBufferData
    {
        FrameGraphHandle<FrameGraphTexture> albedo_occlusion;          ///< RGBA16UNORM: RGB BaseColor, A Occlusion
        FrameGraphHandle<FrameGraphTexture> normal_roughness_metallic; ///< RGBA16SNORM: RG Oct Normal, B Roughness, A Metallic
        FrameGraphHandle<FrameGraphTexture> emissive;                  ///< RG11B10UFLOAT: RGB Emissive Radiance
        FrameGraphHandle<FrameGraphTexture> shading_model_clearcoat;   ///< RG8UINT: R ShadingModelID, G Clearcoat
        FrameGraphHandle<FrameGraphTexture> depth_stencil;             ///< DEPTH32FLOAT or DEPTH24PLUS_STENCIL8
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

        bool valid() const { return initialized && pipeline.valid(); }

    private:
        GPUDevice      device;
        PipelineBundle pipeline;

        GPUTextureFormat cached_depth_format = GPUTextureFormat::DEPTH32FLOAT;
        bool             initialized         = false;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H
