#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_HIZ_PYRAMID_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_HIZ_PYRAMID_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameGraph.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the hierarchical Z depth pyramid pass.
     */
    struct HiZPyramidData
    {
        FrameGraphHandle<FrameGraphTexture> depth_input;
        FrameGraphHandle<FrameGraphTexture> hiz_texture;
    };

    /**
     * @brief Hi-Z Pyramid pass generates a hierarchical depth buffer pyramid via compute downsampling,
     * used for fast GPU-driven occlusion culling and screen-space ray marching.
     */
    struct HiZPyramidPass
    {
        static auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> depth_texture) -> HiZPyramidData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_HIZ_PYRAMID_PASS_H
