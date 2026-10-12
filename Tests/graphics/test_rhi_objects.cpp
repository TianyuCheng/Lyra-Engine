#include <doctest/doctest.h>
#include <cstring>
#include <Lyra/Graphics/RHITypes.h>

using namespace lyra;

TEST_CASE("graphics::rhi_objects" * doctest::description("GPUBuffer, GPUTexture, GPUCommandBuffer with Null backend"))
{
    auto device = RHI::get_current_device();

    SUBCASE("GPUBuffer creation, mapping, and unmapping")
    {
        GPUBufferDescriptor desc{};
        desc.label = "mapping_buffer";
        desc.size  = 256;
        desc.usage = GPUBufferUsage::MAP_WRITE | GPUBufferUsage::COPY_SRC;

        auto buffer = device.create_buffer(desc);
        REQUIRE(buffer.valid());
        CHECK(buffer.size == 256);

        buffer.map(GPUMapMode::WRITE, 0, 256);
        CHECK(buffer.get_mapped_state() == GPUMapState::MAPPED);

        auto range = buffer.get_mapped_range();
        REQUIRE(range.data != nullptr);
        CHECK(range.size >= 256);

        // write test pattern
        const char test_data[] = "Hello Null RHI Graphics Backend!";
        std::memcpy(range.data, test_data, sizeof(test_data));

        buffer.unmap();
        CHECK(buffer.get_mapped_state() == GPUMapState::UNMAPPED);

        buffer.destroy();
    }

    SUBCASE("GPUCommandBuffer copy_buffer_to_buffer and submit")
    {
        constexpr usize BUFFER_SIZE = 512;

        GPUBufferDescriptor src_desc{};
        src_desc.label = "src_buffer";
        src_desc.size  = BUFFER_SIZE;
        src_desc.usage = GPUBufferUsage::COPY_SRC | GPUBufferUsage::MAP_WRITE;

        GPUBufferDescriptor dst_desc{};
        dst_desc.label = "dst_buffer";
        dst_desc.size  = BUFFER_SIZE;
        dst_desc.usage = GPUBufferUsage::COPY_DST | GPUBufferUsage::MAP_READ;

        auto src_buffer = device.create_buffer(src_desc);
        auto dst_buffer = device.create_buffer(dst_desc);
        REQUIRE(src_buffer.valid());
        REQUIRE(dst_buffer.valid());

        // write source pattern
        src_buffer.map(GPUMapMode::WRITE, 0, BUFFER_SIZE);
        auto src_range = src_buffer.get_mapped_range();
        REQUIRE(src_range.data != nullptr);
        for (usize i = 0; i < BUFFER_SIZE; ++i) {
            static_cast<uint8_t*>(src_range.data)[i] = static_cast<uint8_t>(i & 0xFF);
        }
        src_buffer.unmap();

        // record and submit copy commands via GPUCommandBuffer
        GPUCommandBufferDescriptor cmd_desc{};
        cmd_desc.label = "test_cmd_buffer";
        auto cmd       = device.create_command_buffer(cmd_desc);
        REQUIRE(cmd.valid());

        cmd.copy_buffer_to_buffer(src_buffer, 0, dst_buffer, 0, BUFFER_SIZE);
        cmd.submit();

        // read destination to verify mock copy succeeded
        dst_buffer.map(GPUMapMode::READ, 0, BUFFER_SIZE);
        auto dst_range = dst_buffer.get_mapped_range();
        REQUIRE(dst_range.data != nullptr);
        for (usize i = 0; i < BUFFER_SIZE; ++i) {
            CHECK(static_cast<uint8_t*>(dst_range.data)[i] == static_cast<uint8_t>(i & 0xFF));
        }
        dst_buffer.unmap();

        src_buffer.destroy();
        dst_buffer.destroy();
    }

    SUBCASE("GPUTexture, GPUTextureView, and GPUSampler creation")
    {
        GPUTextureDescriptor tex_desc{};
        tex_desc.label        = "test_texture";
        tex_desc.size         = GPUExtent3D{128, 128, 1};
        tex_desc.format       = GPUTextureFormat::RGBA8UNORM;
        tex_desc.usage        = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::RENDER_ATTACHMENT;
        tex_desc.sample_count = 1;

        auto texture = device.create_texture(tex_desc);
        REQUIRE(texture.valid());

        GPUTextureViewDescriptor view_desc{};
        view_desc.label  = "test_texture_view";
        view_desc.format = GPUTextureFormat::RGBA8UNORM;
        auto view        = texture.create_view(view_desc);
        REQUIRE(view.valid());

        GPUSamplerDescriptor samp_desc{};
        samp_desc.label      = "test_sampler";
        samp_desc.min_filter = GPUFilterMode::LINEAR;
        samp_desc.mag_filter = GPUFilterMode::LINEAR;
        auto sampler         = device.create_sampler(samp_desc);
        REQUIRE(sampler.valid());

        sampler.destroy();
        view.destroy();
        texture.destroy();
    }

    SUBCASE("GPUFence signal and wait")
    {
        auto fence = device.create_fence();
        REQUIRE(fence.valid());

        fence.wait();
        fence.destroy();
    }
}
