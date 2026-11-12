#include <Lyra/Rendering/Passes/GBufferPass.h>

using namespace lyra;
using namespace lyra::passes;

GBufferData GBufferPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene,
    GPUExtent2D        extent)
{
    return builder.add_pass<GBufferData>(
        "GBufferPass",
        [&](GBufferData& data, FrameGraphPassBuilder& pass_builder) {
            auto albedo_desc        = GPUTextureDescriptor{};
            albedo_desc.label       = "gbuffer_albedo_metallic";
            albedo_desc.size.width  = extent.width;
            albedo_desc.size.height = extent.height;
            albedo_desc.size.depth  = 1;
            albedo_desc.dimension   = GPUTextureDimension::x2D;
            albedo_desc.format      = GPUTextureFormat::RGBA8UNORM;
            albedo_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.albedo_metallic    = pass_builder.create<FrameGraphTexture>(albedo_desc);

            auto normal_desc        = GPUTextureDescriptor{};
            normal_desc.label       = "gbuffer_normal_roughness";
            normal_desc.size.width  = extent.width;
            normal_desc.size.height = extent.height;
            normal_desc.size.depth  = 1;
            normal_desc.dimension   = GPUTextureDimension::x2D;
            normal_desc.format      = GPUTextureFormat::RGBA16FLOAT;
            normal_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.normal_roughness   = pass_builder.create<FrameGraphTexture>(normal_desc);

            auto mat_desc        = GPUTextureDescriptor{};
            mat_desc.label       = "gbuffer_material_info";
            mat_desc.size.width  = extent.width;
            mat_desc.size.height = extent.height;
            mat_desc.size.depth  = 1;
            mat_desc.dimension   = GPUTextureDimension::x2D;
            mat_desc.format      = GPUTextureFormat::RGBA8UINT;
            mat_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.material_info   = pass_builder.create<FrameGraphTexture>(mat_desc);

            auto depth_desc        = GPUTextureDescriptor{};
            depth_desc.label       = "gbuffer_depth_stencil";
            depth_desc.size.width  = extent.width;
            depth_desc.size.height = extent.height;
            depth_desc.size.depth  = 1;
            depth_desc.dimension   = GPUTextureDimension::x2D;
            depth_desc.format      = GPUTextureFormat::DEPTH24PLUS_STENCIL8;
            depth_desc.usage       = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;
            data.depth_stencil     = pass_builder.create<FrameGraphTexture>(depth_desc);

            data.albedo_metallic  = pass_builder.render(data.albedo_metallic);
            data.normal_roughness = pass_builder.render(data.normal_roughness);
            data.material_info    = pass_builder.render(data.material_info);
            data.depth_stencil    = pass_builder.render(data.depth_stencil);
        },
        [](const GBufferData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Set multiple render targets (MRT) attachments (Albedo, Normal, MaterialID, Depth)
            // TODO: 2. Bind G-Buffer rasterization pipeline
            // TODO: 3. Set camera uniform buffer
            // TODO: 4. Draw mesh primitives writing material parameters into targets
            // TODO: 5. Support user custom material properties written into MaterialInfo buffer
        });
}
