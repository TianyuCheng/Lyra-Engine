// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

D3D12Sampler::D3D12Sampler()
{
    // do nothing
}

D3D12Sampler::D3D12Sampler(const GPUSamplerDescriptor& desc)
{
    D3D12_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter         = d3d12enum(desc.min_filter, desc.mag_filter, desc.mipmap_filter);
    sampler_desc.AddressU       = d3d12enum(desc.address_mode_u);
    sampler_desc.AddressV       = d3d12enum(desc.address_mode_v);
    sampler_desc.AddressW       = d3d12enum(desc.address_mode_w);
    sampler_desc.ComparisonFunc = d3d12enum(desc.compare, desc.compare_enable);
    sampler_desc.MipLODBias     = 0;
    sampler_desc.MinLOD         = desc.lod_min_clamp;
    sampler_desc.MaxLOD         = desc.lod_max_clamp;
    sampler_desc.MaxAnisotropy  = desc.max_anisotropy;
    sampler_desc.BorderColor[0] = 0.0f;
    sampler_desc.BorderColor[1] = 0.0f;
    sampler_desc.BorderColor[2] = 0.0f;
    sampler_desc.BorderColor[3] = 0.0f;

    auto rhi = get_rhi();
    sampler  = rhi->sampler_heap.allocate();
    rhi->device->CreateSampler(&sampler_desc, sampler.handle);
}

void D3D12Sampler::destroy()
{
    if (sampler.valid()) {
        auto rhi = get_rhi();
        rhi->sampler_heap.recycle(sampler);
        sampler.handle.ptr = 0;
    }
}
