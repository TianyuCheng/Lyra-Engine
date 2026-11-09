#include <Lyra/Rendering/Passes/SkyboxPass.h>

using namespace lyra;
using namespace lyra::passes;

SkyboxData SkyboxPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> color_target,
    FrameGraphHandle<FrameGraphTexture> depth_target,
    const RenderView&                   view,
    const RenderScene&                  scene)
{
    return builder.add_pass<SkyboxData>(
        "SkyboxPass",
        [&](SkyboxData& data, FrameGraphPassBuilder& pass_builder) {
            data.color_output = pass_builder.render(color_target);
            data.depth_input  = pass_builder.read(depth_target);
        },
        [](const SkyboxData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Bind skybox pipeline with depth test EQUAL (depth at far plane 1.0)
            // TODO: 2. Bind cubemap / latlong environment texture or atmospheric scattering parameters
            // TODO: 3. Draw procedural cube or fullscreen sky quad
        });
}
