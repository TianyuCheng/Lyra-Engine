#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H

#include <Lyra/Graphics/RHITypes.h>
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
        FrameGraphHandle<FrameGraphTexture> material_info;    ///< RGBA8UINT: R shading model, G material ID, BA custom
        FrameGraphHandle<FrameGraphTexture> depth_stencil;    ///< DEPTH24PLUS_STENCIL8 or DEPTH32FLOAT
    };

    /**
     * @brief G-Buffer pass rasterizes primary geometry into multi-target render buffers
     * storing surface material properties, supporting both standard PBR and custom shading models.
     */
    struct GBufferPass
    {
        static auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            GPUExtent2D        extent) -> GBufferData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_GBUFFER_PASS_H
