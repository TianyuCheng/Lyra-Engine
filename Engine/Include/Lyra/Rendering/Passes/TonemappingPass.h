#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/FrameGraph.h>
#include <Lyra/Rendering/RendererTypes.h>

namespace lyra::passes
{
    /**
     * @brief outputs of the tonemapping pass.
     */
    struct TonemappingData
    {
        FrameGraphHandle<FrameGraphTexture> hdr_input;
        FrameGraphHandle<FrameGraphTexture> ldr_output;
    };

    /**
     * @brief Tonemapping pass maps high dynamic range (HDR) radiance values
     * to standard low dynamic range (LDR) display color space with exposure and gamma correction.
     */
    struct TonemappingPass
    {
        static auto add(
            FrameGraphBuilder&                  builder,
            FrameGraphHandle<FrameGraphTexture> hdr_input,
            const RendererSettings&             settings,
            GPUTextureFormat                    output_format) -> TonemappingData;
    };

} // namespace lyra::passes

#endif // LYRA_ENGINE_RENDERING_PASSES_TONEMAPPING_PASS_H
