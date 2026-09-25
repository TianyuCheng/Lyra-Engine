#include "helper.h"

#include <atomic>
#include <vector>

#include <Lyra/JobSystem/JobSystem.h>

using namespace lyra;

TEST_CASE("jobs::job_system" * doctest::description("Job System and Coroutine Tests"))
{
    if (!JobScheduler::is_initialized()) {
        JobScheduler::init({.workers = 4});
    }

    SUBCASE("basic_job_scheduling")
    {
        std::atomic<int> counter_val{0};

        for (int i = 0; i < 100; ++i) {
            JobScheduler::schedule([&counter_val] {
                counter_val.fetch_add(1, std::memory_order_relaxed);
            });
        }

        JobScheduler::wait_idle();

        CHECK_EQ(counter_val.load(), 100);
    }

    SUBCASE("coroutine_task_return_value")
    {
        auto compute = []() -> Task<int> {
            co_return 100 + 42;
        };

        int result = sync_wait(compute());
        CHECK_EQ(result, 142);
    }

    SUBCASE("coroutine_chain_symmetric_transfer")
    {
        auto step3 = []() -> Task<int> {
            co_return 42;
        };

        auto step2 = [&]() -> Task<int> {
            int v = co_await step3();
            co_return v + 1;
        };

        auto step1 = [&]() -> Task<int> {
            int v = co_await step2();
            co_return v * 2;
        };

        int result = sync_wait(step1());
        CHECK_EQ(result, 86);
    }

    SUBCASE("parallel_for")
    {
        constexpr size_t count = 50000;
        std::vector<int> data(count, 0);

        auto parallel_job = [&]() -> Task<void> {
            co_await parallel_for(static_cast<size_t>(0), count, [&](size_t i) {
                data[i] = static_cast<int>(i * 3);
            }, 512);
        };

        sync_wait(parallel_job());

        bool correct = true;
        for (size_t i = 0; i < count; ++i) {
            if (data[i] != static_cast<int>(i * 3)) {
                correct = false;
                break;
            }
        }
        CHECK(correct);
    }

    SUBCASE("main_thread_hop")
    {
        auto hop_test = []() -> Task<bool> {
            CHECK(JobScheduler::is_main_thread());

            co_await JobScheduler::worker(JobPriority::NORMAL);
            bool was_worker = !JobScheduler::is_main_thread();

            co_await JobScheduler::main_thread();
            bool is_main = JobScheduler::is_main_thread();

            co_return was_worker&& is_main;
        };

        bool ok = sync_wait(hop_test());
        CHECK(ok);
    }

    SUBCASE("when_all")
    {
        std::atomic<int> completed{0};

        auto task_a = [&]() -> Task<void> {
            completed.fetch_add(10, std::memory_order_relaxed);
            co_return;
        };

        auto task_b = [&]() -> Task<void> {
            completed.fetch_add(20, std::memory_order_relaxed);
            co_return;
        };

        auto task_c = [&]() -> Task<void> {
            completed.fetch_add(30, std::memory_order_relaxed);
            co_return;
        };

        auto joined = [&]() -> Task<void> {
            co_await when_all(task_a(), task_b(), task_c());
        };

        sync_wait(joined());
        CHECK_EQ(completed.load(), 60);
    }

    SUBCASE("wait_idle")
    {
        std::atomic<int> count{0};

        for (int i = 0; i < 50; ++i) {
            JobScheduler::schedule([&count] {
                count.fetch_add(1, std::memory_order_relaxed);
            });
        }

        JobScheduler::wait_idle();

        CHECK_EQ(count.load(), 50);
    }

    SUBCASE("batch_awaiter")
    {
        std::atomic<int> sum{0};
        constexpr uint batch_count = 20;

        auto test_batch = [&]() -> Task<int> {
            auto awaiter = std::make_shared<BatchAwaiter>(batch_count);
            for (uint i = 0; i < batch_count; ++i) {
                JobScheduler::schedule([awaiter, &sum] {
                    sum.fetch_add(1, std::memory_order_relaxed);
                    awaiter->finish();
                });
            }
            co_await *awaiter;
            co_return sum.load();
        };

        int res = sync_wait(test_batch());
        CHECK_EQ(res, 20);
    }
}
