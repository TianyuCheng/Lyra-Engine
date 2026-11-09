#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RenderScene.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the skybox / environment pass.
     */
    struct SkyboxData
    {
        FrameGraphHandle<FrameGraphTexture> color_output;
        FrameGraphHandle<FrameGraphTexture> depth_input;
    };

    /**
     * @brief Skybox pass renders background environment radiance or atmospheric procedural sky.
     */
    struct SkyboxPass
    {
        static auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> color_target,
            FrameGraphHandle<FrameGraphTexture> depth_target,
            const RenderView&                   view,
            const RenderScene&                  scene) -> SkyboxData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_SKYBOX_PASS_H
