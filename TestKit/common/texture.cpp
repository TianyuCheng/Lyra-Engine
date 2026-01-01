#include "./texture.h"

void SimpleTexture2D::upload()
{
    auto& device = RHI::get_current_device();

    auto command = execute([&]() {
        auto desc  = GPUCommandBufferDescriptor{};
        desc.queue = GPUQueueType::DEFAULT;
        return device.create_command_buffer(desc);
    });

    auto copy_src           = GPUTexelCopyBufferInfo{};
    copy_src.buffer         = buffer;
    copy_src.bytes_per_row  = 0; // expect texture to be tightly packed
    copy_src.offset         = 0;
    copy_src.rows_per_image = height;

    auto copy_dst      = GPUTexelCopyTextureInfo{};
    copy_dst.texture   = texture;
    copy_dst.aspect    = GPUTextureAspect::COLOR;
    copy_dst.mip_level = 0;
    copy_dst.origin    = {0, 0, 0};

    auto copy_ext   = GPUExtent3D{};
    copy_ext.width  = width;
    copy_ext.height = height;
    copy_ext.depth  = 1;

    command.resource_barrier(state_transition(texture, undefined_state(), copy_dst_state()));
    command.copy_buffer_to_texture(copy_src, copy_dst, copy_ext);
    command.resource_barrier(state_transition(texture, copy_dst_state(), shader_resource_state(GPUBarrierSync::PIXEL_SHADING)));
    command.submit();

    device.wait();
}

SimpleTexture2D SimpleTexture2D::gradient(uint width, uint height)
{
    SimpleTexture2D tex;

    auto& adapter = RHI::get_current_adapter();
    auto& device  = RHI::get_current_device();

    uint row_pitch = align(
        static_cast<uint>(width * sizeof(uint)),
        adapter.properties.texture_row_pitch_alignment);

    uint texels_per_row = row_pitch / sizeof(uint);

    tex.width  = width;
    tex.height = height;

    tex.buffer = execute([&] {
        auto desc  = GPUBufferDescriptor{};
        desc.size  = row_pitch * height;
        desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;
        desc.label = "staging";
        return device.create_buffer(desc);
    });

    tex.texture = execute([&] {
        auto desc            = GPUTextureDescriptor{};
        desc.size.width      = width;
        desc.size.height     = width;
        desc.size.depth      = 1;
        desc.array_layers    = 1;
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = GPUTextureFormat::RGBA8UNORM;
        desc.mip_level_count = 1;
        desc.sample_count    = 1;
        desc.usage           = GPUTextureUsage::COPY_DST | GPUTextureUsage::TEXTURE_BINDING;
        desc.label           = "texture";
        return device.create_texture(desc);
    });

    tex.view = tex.texture.create_view();

    tex.buffer.map(GPUMapMode::WRITE);
    auto content = tex.buffer.get_mapped_range<uint>();
    for (uint y = 0; y < height; y++) {
        for (uint x = 0; x < width; x++) {
            uint index        = y * texels_per_row + x;
            uint red          = uint(float(y) / float(height) * 255.0) & 0xFFu;
            uint green        = uint(float(x) / float(width) * 255.0) & 0xFFu;
            uint color        = (red << 0) | (green << 8) | 0xFF000000u;
            content.at(index) = color;
        }
    }
    tex.buffer.unmap();
    return tex;
}
