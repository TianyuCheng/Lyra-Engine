#include <stb_image_write.h>

#include "render.h"

GPUTexelCopyTextureInfo RenderTarget::copy_src() const
{
    auto texture_region      = GPUTexelCopyTextureInfo{};
    texture_region.texture   = texture;
    texture_region.aspect    = GPUTextureAspect::COLOR;
    texture_region.mip_level = 0;
    texture_region.origin    = GPUOrigin3D{0, 0, 0};
    return texture_region;
}

GPUTexelCopyBufferInfo RenderTarget::copy_dst() const
{
    auto buffer_region           = GPUTexelCopyBufferInfo{};
    buffer_region.buffer         = buffer;
    buffer_region.offset         = 0;
    buffer_region.bytes_per_row  = 0;
    buffer_region.rows_per_image = height;
    return buffer_region;
}

GPUExtent3D RenderTarget::copy_ext() const
{
    return GPUExtent3D{width, height, 1};
}

void RenderTarget::save(CString filename)
{
    buffer.map(GPUMapMode::READ);
    {
        auto data = buffer.get_mapped_range<uint8_t>();
        stbi_write_png(filename, width, height, 4, data.data, width * 4);
    }
    buffer.unmap();
}

RenderTarget RenderTarget::create(GPUTextureFormat format, uint width, uint height, uint samples)
{
    RenderTarget res = {};

    GPUTextureDescriptor tex_desc{};
    tex_desc.label           = "render target";
    tex_desc.format          = format;
    tex_desc.dimension       = GPUTextureDimension::x2D;
    tex_desc.size.width      = width;
    tex_desc.size.height     = height;
    tex_desc.size.depth      = 1;
    tex_desc.array_layers    = 1;
    tex_desc.mip_level_count = 1;
    tex_desc.sample_count    = samples;
    tex_desc.usage           = GPUTextureUsage::COPY_SRC | GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::STORAGE_BINDING;

    GPUBufferDescriptor buf_desc{};
    buf_desc.label = "host backbuffer";
    buf_desc.usage = GPUBufferUsage::COPY_DST | GPUBufferUsage::MAP_READ;
    buf_desc.size  = sizeof(uint32_t) * width * height;

    auto& device = RHI::get_current_device();
    res.format   = tex_desc.format;
    res.width    = width;
    res.height   = height;
    res.buffer   = device.create_buffer(buf_desc);
    res.texture  = device.create_texture(tex_desc);
    res.view     = res.texture.create_view();
    return res;
}
