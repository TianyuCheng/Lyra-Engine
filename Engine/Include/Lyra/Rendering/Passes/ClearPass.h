#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_CLEAR_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_CLEAR_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameGraph.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the clear pass.
     */
    struct ClearPassData
    {
        FrameGraphHandle<FrameGraphTexture> color;
        FrameGraphHandle<FrameGraphTexture> depth;
    };

    /**
     * @brief composable fallback or explicit attachment clear pass.
     * Clears color, depth, or both attachments in a single render pass.
     */
    struct ClearPass
    {
        static auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> color_target,
            FrameGraphHandle<FrameGraphTexture> depth_target,
            GPUExtent2D                         extent,
            GPUColor                            clear_color = GPUColor{0.12f, 0.12f, 0.14f, 1.0f},
            float                               clear_depth = 1.0f) -> ClearPassData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_CLEAR_PASS_H
