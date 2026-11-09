#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>

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
        static auto add(
            FrameGraphBuilder& builder,
            const RenderView&  view,
            const RenderScene& scene) -> LightCullingData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_LIGHT_CULLING_PASS_H
