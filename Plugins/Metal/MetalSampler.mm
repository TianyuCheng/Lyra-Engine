#include "MetalUtils.h"

using namespace lyra;

MetalSampler::MetalSampler()
{
    // do nothing
}

MetalSampler::MetalSampler(const GPUSamplerDescriptor& desc)
{
    @autoreleasepool {
        auto rhi = get_rhi();

        MTLSamplerDescriptor* mtl_desc = [MTLSamplerDescriptor new];
        mtl_desc.sAddressMode          = mtlenum(desc.address_mode_u);
        mtl_desc.tAddressMode          = mtlenum(desc.address_mode_v);
        mtl_desc.rAddressMode          = mtlenum(desc.address_mode_w);
        mtl_desc.minFilter             = mtlenum(desc.min_filter);
        mtl_desc.magFilter             = mtlenum(desc.mag_filter);
        mtl_desc.mipFilter             = mtlenum(desc.mipmap_filter);
        mtl_desc.lodMinClamp           = desc.lod_min_clamp;
        mtl_desc.lodMaxClamp           = desc.lod_max_clamp;
        mtl_desc.maxAnisotropy         = desc.max_anisotropy;
        if (desc.compare_enable)
            mtl_desc.compareFunction = mtlenum(desc.compare);

        sampler = [rhi->device newSamplerStateWithDescriptor:mtl_desc];

        if (!sampler) {
            get_logger()->error("Failed to create Metal sampler state");
        }
    }
}

void MetalSampler::destroy()
{
    sampler = nil;
}
