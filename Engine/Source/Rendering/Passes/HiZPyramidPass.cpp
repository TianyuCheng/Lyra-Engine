#include <Lyra/Rendering/Passes/HiZPyramidPass.h>

using namespace lyra;
using namespace lyra::passes;

HiZPyramidData HiZPyramidPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> depth_texture)
{
    return builder.add_pass<HiZPyramidData>(
        "HiZPyramidPass",
        [&](HiZPyramidData& data, FrameGraphPassBuilder& pass_builder) {
            data.depth_input = pass_builder.sample(depth_texture);
            // TODO: allocate mip-mapped single-channel R32FLOAT texture for Hi-Z
        },
        [](const HiZPyramidData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Bind Hi-Z reduction compute pipeline
            // TODO: 2. Loop through mip levels:
            //          - Downsample previous mip using 2x2 conservative min/max depth filter
            //          - Insert compute execution barriers between mip levels
        });
}
