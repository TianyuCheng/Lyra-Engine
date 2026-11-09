#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H
#define LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H

#include <Lyra/Graphics/RHITypes.h>
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
     * @brief Depth prepass eliminates overdraw by rasterizing opaque geometry to the depth buffer
     * with color writes disabled before forward/deferred shading.
     */
    struct DepthPrepass
    {
        static auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            GPUExtent2D        extent) -> DepthPrepassData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_DEPTH_PREPASS_H
