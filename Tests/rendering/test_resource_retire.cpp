#include <doctest/doctest.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/ResourceRetire.h>

using namespace lyra;

TEST_CASE("rendering::resource_retire" * doctest::description("ResourceRetire composite multi-type deferred retirement"))
{
    auto device = RHI::get_current_device();

    SUBCASE("Initialization and initial state")
    {
        ResourceRetire retire;
        CHECK(retire.empty());
        CHECK(retire.size() == 0);

        retire.init(3);
        CHECK(retire.empty());
        CHECK(retire.size() == 0);
    }

    SUBCASE("Retiring diverse GPU resource types")
    {
        ResourceRetire retire;
        retire.init(2);

        // create real mock GPU resources
        GPUBufferDescriptor buf_desc{};
        buf_desc.label = "retire_buffer";
        buf_desc.size  = 128;
        buf_desc.usage = GPUBufferUsage::STORAGE;
        auto buffer    = device.create_buffer(buf_desc);

        GPUTextureDescriptor tex_desc{};
        tex_desc.label  = "retire_texture";
        tex_desc.size   = GPUExtent3D{64, 64, 1};
        tex_desc.format = GPUTextureFormat::RGBA8UNORM;
        tex_desc.usage  = GPUTextureUsage::TEXTURE_BINDING;
        auto texture    = device.create_texture(tex_desc);

        GPUSamplerDescriptor samp_desc{};
        samp_desc.label = "retire_sampler";
        auto sampler    = device.create_sampler(samp_desc);

        auto fence = device.create_fence();

        REQUIRE(buffer.valid());
        REQUIRE(texture.valid());
        REQUIRE(sampler.valid());
        REQUIRE(fence.valid());

        // retire individually
        retire.retire(buffer);
        CHECK(retire.size() == 1);
        CHECK(!retire.empty());

        // retire multiple via variadic template
        retire.retire(texture, sampler, fence);
        CHECK(retire.size() == 4);

        // typed queue sizes
        CHECK(retire.get_queue<GPUBuffer>().size() == 1);
        CHECK(retire.get_queue<GPUTexture>().size() == 1);
        CHECK(retire.get_queue<GPUSampler>().size() == 1);
        CHECK(retire.get_queue<GPUFence>().size() == 1);

        // advance 1 frame (resources remain pending)
        retire.tick();
        CHECK(retire.size() == 4);

        // advance 2nd frame (exceeds frames_in_flight=2 -> flushes)
        retire.tick();
        CHECK(retire.empty());
        CHECK(retire.size() == 0);
    }

    SUBCASE("Drain on shutdown flushes all queues immediately")
    {
        ResourceRetire retire;
        retire.init(3);

        GPUBufferDescriptor desc1{};
        desc1.size  = 64;
        desc1.usage = GPUBufferUsage::STORAGE;
        auto buf1   = device.create_buffer(desc1);

        GPUBufferDescriptor desc2{};
        desc2.size  = 64;
        desc2.usage = GPUBufferUsage::STORAGE;
        auto buf2   = device.create_buffer(desc2);

        retire.retire(buf1);
        retire.tick();
        retire.retire(buf2);

        CHECK(retire.size() == 2);

        retire.drain();
        CHECK(retire.empty());
        CHECK(retire.size() == 0);
    }
}
