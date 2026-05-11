// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

D3D12Texture::D3D12Texture()
{
    // do nothing
}

D3D12Texture::D3D12Texture(ID3D12Resource* texture) : texture(texture)
{
    samples = 1;
}

D3D12Texture::D3D12Texture(const GPUTextureDescriptor& desc)
{
    D3D12MA::ALLOCATION_DESC allocation_desc{};
    allocation_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource_desc.Width            = desc.size.width;
    resource_desc.Height           = desc.size.height;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels        = 1;
    resource_desc.Format           = infer_texture_format(desc.format);
    resource_desc.SampleDesc.Count = 1;
    resource_desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource_desc.Flags            = infer_texture_flags(desc.usage, desc.format);

    auto rhi = get_rhi();
    ThrowIfFailed(rhi->allocator->CreateResource(
        &allocation_desc,
        &resource_desc,
        D3D12_RESOURCE_STATE_COMMON,
        NULL,
        &allocation,
        IID_PPV_ARGS(&texture)));

    if (desc.label)
        texture->SetName(to_wstring(desc.label).c_str());

    // record basic information
    format      = resource_desc.Format;
    samples     = desc.sample_count;
    usages      = desc.usage;
    area.width  = desc.size.width;
    area.height = desc.size.height;
}

void D3D12Texture::destroy()
{
    if (texture) {
        texture->Release();
        texture = nullptr;
    }

    if (allocation) {
        allocation->Release();
        allocation = nullptr;
    }
}

D3D12TextureView::D3D12TextureView()
{
    // do nothing
}

D3D12TextureView::D3D12TextureView(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc)
{
    GPUTextureUsageFlags usage = desc.usage.value == 0 ? texture.usages : desc.usage;

    // create texture view for render target / depth stencil
    if (usage.contains(GPUTextureUsage::RENDER_ATTACHMENT)) {
        if (is_depth_stencil_format(desc.format)) {
            init_dsv(texture, desc);
        } else {
            init_rtv(texture, desc);
        }
    }

    // create texture view for shader resource (sampled texture)
    if (usage.contains(GPUTextureUsage::TEXTURE_BINDING)) {
        init_srv(texture, desc);
    }

    // create texture view for shader resource (storage texture)
    if (usage.contains(GPUTextureUsage::STORAGE_BINDING)) {
        init_uav(texture, desc);
    }
}

void D3D12TextureView::destroy()
{
    auto rhi = get_rhi();

    if (rtv_view.valid()) {
        rhi->rtv_heap.recycle(rtv_view);
        rtv_view.reset();
    }

    if (srv_view.valid()) {
        rhi->cbv_srv_uav_heap.recycle(srv_view);
        srv_view.reset();
    }

    if (uav_view.valid()) {
        rhi->cbv_srv_uav_heap.recycle(uav_view);
        uav_view.reset();
    }
}

void D3D12TextureView::init_rtv(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc)
{
    D3D12_RENDER_TARGET_VIEW_DESC view_desc = {};

    view_desc.Format = infer_texture_format(desc.format);
    switch (desc.dimension) {
        case GPUTextureViewDimension::x1D:
            view_desc.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = desc.base_mip_level;
            break;
        case GPUTextureViewDimension::x2D:
            view_desc.ViewDimension        = texture.samples == 1 ? D3D12_RTV_DIMENSION_TEXTURE2D : D3D12_RTV_DIMENSION_TEXTURE2DMS;
            view_desc.Texture2D.MipSlice   = desc.base_mip_level;
            view_desc.Texture2D.PlaneSlice = 0; // No support for multi-plane texture (yet)
            break;
        case GPUTextureViewDimension::x2D_ARRAY:
            if (texture.samples <= 1) {
                view_desc.ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                view_desc.Texture2DArray.MipSlice        = desc.base_mip_level;
                view_desc.Texture2DArray.PlaneSlice      = 0; // No support for multi-plane texture (yet)
                view_desc.Texture2DArray.ArraySize       = desc.array_layer_count;
                view_desc.Texture2DArray.FirstArraySlice = desc.base_array_layer;
            } else {
                view_desc.ViewDimension                    = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                view_desc.Texture2DMSArray.ArraySize       = desc.array_layer_count;
                view_desc.Texture2DMSArray.FirstArraySlice = desc.base_array_layer;
            }
            break;
        case GPUTextureViewDimension::x3D:
            view_desc.ViewDimension         = D3D12_RTV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.MipSlice    = desc.base_mip_level;
            view_desc.Texture3D.FirstWSlice = desc.base_array_layer;
            view_desc.Texture3D.WSize       = desc.array_layer_count;
            break;
        case GPUTextureViewDimension::CUBE:
        case GPUTextureViewDimension::CUBE_ARRAY:
            assert(!!!"infer_rtv_dimension receives unsupported cube texture view!");
            break;
        default:
            assert(!!!"infer_rtv_dimension receives unknown view dimension!");
            break;
    }

    auto rhi = get_rhi();
    rtv_view = rhi->rtv_heap.allocate();
    rhi->device->CreateRenderTargetView(texture.texture, &view_desc, rtv_view.handle);
}

void D3D12TextureView::init_dsv(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC view_desc = {};

    view_desc.Format = infer_texture_format(desc.format);
    switch (desc.dimension) {
        case GPUTextureViewDimension::x1D:
            view_desc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = desc.base_mip_level;
            break;
        case GPUTextureViewDimension::x2D:
            view_desc.ViewDimension      = texture.samples == 1 ? D3D12_DSV_DIMENSION_TEXTURE2D : D3D12_DSV_DIMENSION_TEXTURE2DMS;
            view_desc.Texture2D.MipSlice = desc.base_mip_level;
            break;
        case GPUTextureViewDimension::x2D_ARRAY:
            if (texture.samples <= 1) {
                view_desc.ViewDimension                  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                view_desc.Texture2DArray.MipSlice        = desc.base_mip_level;
                view_desc.Texture2DArray.ArraySize       = desc.array_layer_count;
                view_desc.Texture2DArray.FirstArraySlice = desc.base_array_layer;
            } else {
                view_desc.ViewDimension                    = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                view_desc.Texture2DMSArray.ArraySize       = desc.array_layer_count;
                view_desc.Texture2DMSArray.FirstArraySlice = desc.base_array_layer;
            }
            break;
        case GPUTextureViewDimension::x3D:
            assert(!!!"init_dsv() receives unsupported 3d texture view!");
            break;
        case GPUTextureViewDimension::CUBE:
        case GPUTextureViewDimension::CUBE_ARRAY:
            assert(!!!"init_dsv() receives unsupported cube texture view!");
            break;
        default:
            assert(!!!"init_dsv() receives unknown view dimension!");
            break;
    }

    // RTV and DSV share the same view (because a texture cannot be both RTV and DSV)
    auto rhi = get_rhi();
    dsv_view = rhi->dsv_heap.allocate();
    rhi->device->CreateDepthStencilView(texture.texture, &view_desc, dsv_view.handle);
}

void D3D12TextureView::init_srv(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC view_desc = {};

    view_desc.Format = infer_texture_format(desc.format);

    view_desc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
        D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3);

    switch (desc.dimension) {
        case GPUTextureViewDimension::x1D:
            view_desc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipLevels           = desc.mip_level_count;
            view_desc.Texture1D.MostDetailedMip     = desc.base_mip_level;
            view_desc.Texture1D.ResourceMinLODClamp = 0;
            break;
        case GPUTextureViewDimension::x2D:
            if (texture.samples <= 1) {
                view_desc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
                view_desc.Texture2D.MipLevels           = desc.mip_level_count;
                view_desc.Texture2D.MostDetailedMip     = desc.base_mip_level;
                view_desc.Texture2D.ResourceMinLODClamp = 0;
                view_desc.Texture2D.PlaneSlice          = 0; // No support for multi-plane texture (yet)
            } else {
                view_desc.ViewDimension                           = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                view_desc.Texture2DMS.UnusedField_NothingToDefine = 0;
            }
            break;
        case GPUTextureViewDimension::x2D_ARRAY:
            if (texture.samples <= 1) {
                view_desc.ViewDimension                      = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                view_desc.ViewDimension                      = D3D12_SRV_DIMENSION_TEXTURE2D;
                view_desc.Texture2DArray.MipLevels           = desc.mip_level_count;
                view_desc.Texture2DArray.MostDetailedMip     = desc.base_mip_level;
                view_desc.Texture2DArray.ResourceMinLODClamp = 0;
                view_desc.Texture2DArray.PlaneSlice          = 0; // No support for multi-plane texture (yet)
                view_desc.Texture2DArray.FirstArraySlice     = desc.base_array_layer;
                view_desc.Texture2DArray.ArraySize           = desc.array_layer_count;
            } else {
                view_desc.ViewDimension                    = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                view_desc.Texture2DMSArray.FirstArraySlice = desc.base_array_layer;
                view_desc.Texture2DMSArray.ArraySize       = desc.array_layer_count;
            }
            break;
        case GPUTextureViewDimension::CUBE:
            view_desc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURECUBE;
            view_desc.TextureCube.MipLevels           = desc.mip_level_count;
            view_desc.TextureCube.MostDetailedMip     = desc.base_mip_level;
            view_desc.TextureCube.ResourceMinLODClamp = 0;
            break;
        case GPUTextureViewDimension::CUBE_ARRAY:
            view_desc.ViewDimension                        = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            view_desc.TextureCubeArray.MipLevels           = desc.mip_level_count;
            view_desc.TextureCubeArray.MostDetailedMip     = desc.base_mip_level;
            view_desc.TextureCubeArray.ResourceMinLODClamp = 0;
            view_desc.TextureCubeArray.First2DArrayFace    = desc.base_array_layer;
            view_desc.TextureCubeArray.NumCubes            = desc.array_layer_count;
            break;
        case GPUTextureViewDimension::x3D:
            view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            break;
        default:
            assert(!!!"init_srv() receives unknown view dimension!");
            break;
    }

    auto rhi = get_rhi();
    srv_view = rhi->cbv_srv_uav_heap.allocate();
    rhi->device->CreateShaderResourceView(texture.texture, &view_desc, srv_view.handle);
}

void D3D12TextureView::init_uav(const D3D12Texture& texture, const GPUTextureViewDescriptor& desc)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC view_desc = {};

    view_desc.Format = infer_texture_format(desc.format);
    switch (desc.dimension) {
        case GPUTextureViewDimension::x1D:
            view_desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE1D;
            view_desc.Texture1D.MipSlice = desc.base_mip_level;
            break;
        case GPUTextureViewDimension::x2D:
            view_desc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
            view_desc.Texture2D.MipSlice   = desc.base_mip_level;
            view_desc.Texture2D.PlaneSlice = 0; // No support for multi-plane texture (yet)
            break;
        case GPUTextureViewDimension::x2D_ARRAY:
            view_desc.ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            view_desc.Texture2DArray.MipSlice        = desc.base_mip_level;
            view_desc.Texture2DArray.PlaneSlice      = 0; // No support for multi-plane texture (yet)
            view_desc.Texture2DArray.ArraySize       = desc.array_layer_count;
            view_desc.Texture2DArray.FirstArraySlice = desc.base_array_layer;
            break;
        case GPUTextureViewDimension::x3D:
            view_desc.ViewDimension         = D3D12_UAV_DIMENSION_TEXTURE3D;
            view_desc.Texture3D.MipSlice    = desc.base_mip_level;
            view_desc.Texture3D.FirstWSlice = desc.base_array_layer;
            view_desc.Texture3D.WSize       = desc.array_layer_count;
            break;
        case GPUTextureViewDimension::CUBE:
        case GPUTextureViewDimension::CUBE_ARRAY:
            assert(!!!"init_uav() receives unsupported cube texture view!");
            break;
        default:
            assert(!!!"init_uav() receives unknown view dimension!");
            break;
    }

    auto rhi = get_rhi();
    uav_view = rhi->cbv_srv_uav_heap.allocate();
    rhi->device->CreateUnorderedAccessView(texture.texture, nullptr, &view_desc, uav_view.handle);
}
