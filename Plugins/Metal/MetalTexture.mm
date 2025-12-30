#include "MetalUtils.h"

using namespace lyra;

// MetalTexture implementations
MetalTexture::MetalTexture()
{
    // do nothing
}

MetalTexture::MetalTexture(const GPUTextureDescriptor& desc)
{
    @autoreleasepool {
        auto rhi = get_rhi();

        MTLTextureDescriptor* mtl_desc = [MTLTextureDescriptor new];
        mtl_desc.textureType           = mtlenum(desc.dimension);
        mtl_desc.pixelFormat           = mtlenum(desc.format);
        mtl_desc.width                 = desc.size.width;
        mtl_desc.height                = desc.size.height;
        mtl_desc.depth                 = desc.size.depth;
        mtl_desc.mipmapLevelCount      = desc.mip_level_count;
        mtl_desc.arrayLength           = desc.array_layers;
        mtl_desc.sampleCount           = desc.sample_count;
        mtl_desc.usage                 = mtlenum(desc.usage);

        texture = [rhi->device newTextureWithDescriptor:mtl_desc];
        format  = mtl_desc.pixelFormat;
        type    = mtl_desc.textureType;

        // set debug label if provided
        if (desc.label && texture) {
            rhi->set_debug_label(texture, desc.label);
        }
    }
}

void MetalTexture::destroy()
{
    texture = nil;
}

// MetalTextureView implementations
MetalTextureView::MetalTextureView()
{
    // do nothing
}

MetalTextureView::MetalTextureView(const MetalTexture& parent, const GPUTextureViewDescriptor& desc)
{
    @autoreleasepool {
        NSRange levels = NSMakeRange(desc.base_mip_level, desc.mip_level_count);
        NSRange slices = NSMakeRange(desc.base_array_layer, desc.array_layer_count);

        texture = [parent.texture newTextureViewWithPixelFormat:mtlenum(desc.format)
                                                    textureType:mtlenum(desc.dimension)
                                                         levels:levels
                                                         slices:slices];
        format  = mtlenum(desc.format);
        type    = mtlenum(desc.dimension);
    }
}

void MetalTextureView::destroy()
{
    texture = nil;
}

bool api::create_texture(GPUTextureHandle& handle, const GPUTextureDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalTexture(desc);
    auto ind = rhi->textures.add(obj);
    handle   = GPUTextureHandle(ind);
    return obj.valid();
}

void api::delete_texture(GPUTextureHandle handle)
{
    get_rhi()->textures.remove(handle.value);
}

bool api::create_texture_view(GPUTextureViewHandle& handle, GPUTextureHandle texture, const GPUTextureViewDescriptor& desc)
{
    auto  rhi = get_rhi();
    auto& tex = fetch_resource(rhi->textures, texture);
    auto  obj = MetalTextureView(tex, desc);
    auto  ind = rhi->views.add(obj);
    handle    = GPUTextureViewHandle(ind);
    return obj.valid();
}

void api::delete_texture_view(GPUTextureViewHandle handle)
{
    get_rhi()->views.remove(handle.value);
}
