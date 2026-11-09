#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>

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
        static auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene,
            uint               cascade_count = 4,
            uint               resolution    = 2048) -> ShadowMapData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_SHADOW_MAP_PASS_H
