#include <Lyra/Rendering/Passes/DepthPrepass.h>

using namespace lyra;
using namespace lyra::passes;

DepthPrepassData DepthPrepass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUExtent2D        extent)
{
    return builder.add_pass<DepthPrepassData>(
        "DepthPrepass",
        [&](DepthPrepassData& data, FrameGraphPassBuilder& pass_builder) {
            auto desc        = GPUTextureDescriptor{};
            desc.label       = "depth_prepass_buffer";
            desc.size.width  = extent.width;
            desc.size.height = extent.height;
            desc.size.depth  = 1;
            desc.dimension   = GPUTextureDimension::x2D;
            desc.format      = GPUTextureFormat::DEPTH24PLUS_STENCIL8;
            desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;

            data.depth_texture = pass_builder.create<FrameGraphTexture>(desc);
            data.depth_texture = pass_builder.render(data.depth_texture);
        },
        [](const DepthPrepassData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Bind depth-only render pipeline (cull mode back, depth test LESS, depth write enabled)
            // TODO: 2. Set viewport & scissor rect
            // TODO: 3. Bind camera uniform buffer
            // TODO: 4. Draw all opaque scene mesh primitives without pixel shader output
            // TODO: 5. For alpha-tested / masked materials, bind minimal alpha test fragment shader
        });
}
