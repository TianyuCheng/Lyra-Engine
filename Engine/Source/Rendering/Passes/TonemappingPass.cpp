#include <Lyra/Rendering/Passes/TonemappingPass.h>

using namespace lyra;
using namespace lyra::passes;

TonemappingData TonemappingPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> hdr_input,
    const RendererSettings&             settings,
    GPUTextureFormat                    output_format)
{
    return builder.add_pass<TonemappingData>(
        "TonemappingPass",
        [&](TonemappingData& data, FrameGraphPassBuilder& pass_builder) {
            data.hdr_input = pass_builder.sample(hdr_input);
            // TODO: allocate or target the destination backbuffer
        },
        [](const TonemappingData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Bind fullscreen tonemapping pipeline (ACES / Reinhard / AgX operator)
            // TODO: 2. Bind HDR color texture sampler
            // TODO: 3. Set exposure push constants / uniform
            // TODO: 4. Draw procedural fullscreen quad (6 vertices)
        });
}
