#include <doctest/doctest.h>

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/JobSystem/JobSystem.h>

using namespace lyra;

TEST_CASE("graphics::gpu_dispatch" * doctest::description("Asynchronous GPU job dispatching and fence awaiting"))
{
    struct ScopedJobSystem
    {
        ScopedJobSystem()
        {
            if (!JobScheduler::is_initialized()) {
                JobScheduler::init({.max_workers = 4});
            }
        }
        ~ScopedJobSystem()
        {
            // keep alive or drain
        }
    } job_system_scope;

    auto device = RHI::get_current_device();

    SUBCASE("fence_poll_query")
    {
        auto fence = device.create_fence();
        REQUIRE(fence.valid());

        // fence should report polled ready on Null backend
        CHECK(fence.poll());

        fence.destroy();
    }

    SUBCASE("operator_co_await_fence")
    {
        auto fence = device.create_fence();
        REQUIRE(fence.valid());

        auto test_coro = [&]() -> Task<bool> {
            co_await fence;
            co_return true;
        };

        bool ok = sync_wait(test_coro());
        CHECK(ok);

        fence.destroy();
    }

    SUBCASE("gpu_dispatch_compute")
    {
        Atomic<bool> recorded{false};

        auto test_dispatch = [&]() -> Task<void> {
            co_await dispatch(GPUQueueType::COMPUTE, [&](GPUCommandEncoder& cmd) {
                cmd.insert_debug_marker("test_compute_dispatch");
                recorded.store(true, memory_order_release);
            });
        };

        sync_wait(test_dispatch());
        CHECK(recorded.load());
    }

    SUBCASE("gpu_dispatch_with_device_and_priority")
    {
        Atomic<bool> recorded{false};

        auto test_dispatch = [&]() -> Task<void> {
            co_await dispatch(device, GPUQueueType::TRANSFER, JobPriority::HIGH, [&](GPUCommandEncoder& cmd) {
                cmd.insert_debug_marker("test_transfer_high_priority");
                recorded.store(true, memory_order_release);
            });
        };

        sync_wait(test_dispatch());
        CHECK(recorded.load());
    }

    SUBCASE("composition_cpu_gpu_when_all")
    {
        Atomic<int> cpu_done{0};
        Atomic<int> gpu_done{0};

        auto combined = [&]() -> Task<void> {
            auto cpu_task = dispatch([&] {
                cpu_done.fetch_add(10, memory_order_relaxed);
            });

            auto gpu_task = dispatch(GPUQueueType::COMPUTE, [&](GPUCommandEncoder& cmd) {
                cmd.insert_debug_marker("combined_gpu_pass");
                gpu_done.fetch_add(20, memory_order_relaxed);
            });

            co_await when_all(std::move(cpu_task), std::move(gpu_task));
        };

        sync_wait(combined());
        CHECK_EQ(cpu_done.load(), 10);
        CHECK_EQ(gpu_done.load(), 20);
    }
}
