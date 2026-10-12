#include <doctest/doctest.h>
#include <memory>
#include <Lyra/Graphics/GPURetireQueue.h>
#include <Lyra/Graphics/RHITypes.h>

using namespace lyra;

namespace
{
    struct MockRetireResource
    {
        int                   id            = 0;
        std::shared_ptr<int>  destroy_count = nullptr;
        bool                  is_valid      = true;

        bool valid() const
        {
            return is_valid;
        }

        void destroy()
        {
            if (destroy_count) {
                (*destroy_count)++;
            }
        }
    };
}

TEST_CASE("graphics::gpu_retire_queue" * doctest::description("GPURetireQueue lifecycle, aging, and drain"))
{
    auto destroy_counter = std::make_shared<int>(0);

    SUBCASE("Uninitialized queue destroys immediately")
    {
        GPURetireQueue<MockRetireResource> queue;
        CHECK(queue.empty());
        CHECK(queue.size() == 0);
        CHECK(queue.get_frames_in_flight() == 0);

        MockRetireResource res{1, destroy_counter, true};
        queue.retire(res);

        // with frames_in_flight == 0, resource is destroyed on retire
        CHECK(*destroy_counter == 1);
        CHECK(queue.empty());
    }

    SUBCASE("Invalid resources are discarded without retirement")
    {
        GPURetireQueue<MockRetireResource> queue;
        queue.init(3);

        MockRetireResource invalid_res{2, destroy_counter, false};
        queue.retire(invalid_res);

        CHECK(queue.empty());
        CHECK(queue.size() == 0);
        CHECK(*destroy_counter == 0);
    }

    SUBCASE("Deferred retirement across multi-frame lifecycle")
    {
        GPURetireQueue<MockRetireResource> queue;
        constexpr uint FRAMES_IN_FLIGHT = 3;
        queue.init(FRAMES_IN_FLIGHT);
        CHECK(queue.get_frames_in_flight() == 3);

        // retire resources in frame 0
        queue.retire(MockRetireResource{10, destroy_counter, true});
        queue.retire(MockRetireResource{11, destroy_counter, true});
        CHECK(queue.size() == 2);
        CHECK(!queue.empty());
        CHECK(*destroy_counter == 0);

        // tick 1 (advance to frame 1)
        queue.tick();
        CHECK(*destroy_counter == 0);

        // retire resource in frame 1
        queue.retire(MockRetireResource{20, destroy_counter, true});
        CHECK(queue.size() == 3);

        // tick 2 (advance to frame 2)
        queue.tick();
        CHECK(*destroy_counter == 0);

        // tick 3 (wraps around to frame 0 -> flushes frame 0's resources)
        queue.tick();
        CHECK(*destroy_counter == 2);
        CHECK(queue.size() == 1); // frame 1's resource is still pending

        // tick 4 (advances to frame 1 -> flushes frame 1's resource)
        queue.tick();
        CHECK(*destroy_counter == 3);
        CHECK(queue.empty());
    }

    SUBCASE("Drain flushes all in-flight resources immediately")
    {
        GPURetireQueue<MockRetireResource> queue;
        queue.init(3);

        queue.retire(MockRetireResource{1, destroy_counter, true});
        queue.tick();
        queue.retire(MockRetireResource{2, destroy_counter, true});
        CHECK(queue.size() == 2);
        CHECK(*destroy_counter == 0);

        queue.drain();
        CHECK(*destroy_counter == 2);
        CHECK(queue.empty());
        CHECK(queue.size() == 0);
    }

    SUBCASE("Retiring real GPUBuffer with Null backend")
    {
        auto device = RHI::get_current_device();

        GPUBufferDescriptor desc{};
        desc.label = "retire_test_buffer";
        desc.size  = 1024;
        desc.usage = GPUBufferUsage::STORAGE;

        auto buffer = device.create_buffer(desc);
        CHECK(buffer.valid());

        GPURetireQueue<GPUBuffer> queue;
        queue.init(2);

        queue.retire(buffer);
        CHECK(queue.size() == 1);

        queue.tick();
        CHECK(queue.size() == 1);

        queue.tick();
        CHECK(queue.empty());
    }
}
