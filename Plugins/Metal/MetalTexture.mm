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
        mtl_desc.storageMode           = determine_texture_storage_mode(desc.format);

        texture = [rhi->device newTextureWithDescriptor:mtl_desc];

        if (!texture) {
            get_logger()->error("Failed to create Metal texture: {}x{}x{}, format={}", 
                desc.size.width, desc.size.height, desc.size.depth, (uint32_t)desc.format);
            return;
        }

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

        if (!texture) {
            get_logger()->error("Failed to create Metal texture view");
            return;
        }

        format  = mtlenum(desc.format);
        type    = mtlenum(desc.dimension);
    }
}

void MetalTextureView::destroy()
{
    texture = nil;
}
