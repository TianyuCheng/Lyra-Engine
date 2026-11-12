#include <Lyra/Rendering/Passes/ShadowMapPass.h>

using namespace lyra;
using namespace lyra::passes;

ShadowMapData ShadowMapPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    uint               cascade_count,
    uint               resolution)
{
    return builder.add_pass<ShadowMapData>(
        "ShadowMapPass",
        [&](ShadowMapData& data, FrameGraphPassBuilder& pass_builder) {
            auto desc             = GPUTextureDescriptor{};
            desc.label            = "shadow_map_cascades";
            desc.size.width       = resolution;
            desc.size.height      = resolution;
            desc.size.depth       = 1;
            desc.array_layers     = cascade_count;
            desc.dimension        = GPUTextureDimension::x2D;
            desc.format           = GPUTextureFormat::DEPTH24PLUS_STENCIL8;
            desc.usage            = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.shadow_map_array = pass_builder.create<FrameGraphTexture>(desc);
            data.shadow_map_array = pass_builder.render(data.shadow_map_array);
        },
        [](const ShadowMapData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Loop through each shadow cascade
            // TODO: 2. Compute tight light orthographic view-projection matrix for camera frustum slice
            // TODO: 3. Bind shadow depth rasterization pipeline with slope-scaled depth bias
            // TODO: 4. Draw casting mesh primitives into corresponding cascade array slice
        });
}
