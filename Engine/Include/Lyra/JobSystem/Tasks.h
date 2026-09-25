#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_TASKS_H
#define LYRA_ENGINE_JOBSYSTEM_TASKS_H

#include <coroutine>
#include <optional>
#include <atomic>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <memory>

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/JobSystem/Jobs.h>

namespace lyra
{

    namespace detail
    {
        bool assist_work();
    }

    // -------------------------------------------------------------------------
    // Promise return-type base (isolates return_value vs return_void for MSVC)
    // -------------------------------------------------------------------------

    template <typename T>
    struct TaskPromiseBase
    {
        std::optional<T> storage{};

        template <typename Value>
            requires std::is_convertible_v<Value&&, T>
        void return_value(Value&& value) noexcept(std::is_nothrow_constructible_v<T, Value&&>)
        {
            storage.emplace(std::forward<Value>(value));
        }

        T result()
        {
            return std::move(*storage);
        }
    };

    template <>
    struct TaskPromiseBase<void>
    {
        void return_void() noexcept {}
        void result() noexcept {}
    };

    // -------------------------------------------------------------------------
    // Task<T>: Unified coroutine task
    // -------------------------------------------------------------------------

    template <typename T = void>
    struct [[nodiscard]] Task
    {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type : TaskPromiseBase<T>
        {
            std::coroutine_handle<> continuation{nullptr};

            Task get_return_object() noexcept
            {
                return Task{handle_type::from_promise(*this)};
            }

            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            FinalAwaiter final_suspend() noexcept
            {
                return FinalAwaiter{continuation};
            }

            void unhandled_exception() noexcept
            {
                DEBUG_BREAK();
            }
        };

        Task() noexcept : handle(nullptr) {}
        explicit Task(handle_type h) noexcept : handle(h) {}

        ~Task()
        {
            if (handle) {
                handle.destroy();
            }
        }

        Task(Task&& o) noexcept : handle(std::exchange(o.handle, nullptr)) {}
        Task& operator=(Task&& o) noexcept
        {
            if (this != &o) {
                if (handle) {
                    handle.destroy();
                }
                handle = std::exchange(o.handle, nullptr);
            }
            return *this;
        }

        Task(const Task&)            = delete;
        Task& operator=(const Task&) = delete;

        // direct awaiter protocol
        bool await_ready() const noexcept
        {
            return !handle || handle.done();
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept
        {
            handle.promise().continuation = cont;
            return handle; // symmetric transfer
        }

        decltype(auto) await_resume()
        {
            return handle.promise().result();
        }

        handle_type get_handle() const noexcept
        {
            return handle;
        }

    private:
        handle_type handle{nullptr};
    };

    // -------------------------------------------------------------------------
    // sync_wait: Bridge from synchronous code
    // -------------------------------------------------------------------------

    template <typename T>
    T sync_wait(Task<T>&& task)
    {
        if (!JobScheduler::is_main_thread() && JobScheduler::is_initialized()) {
            DEBUG_BREAK();
        }

        struct SyncContext
        {
            std::atomic<bool>                                             done{false};
            std::conditional_t<std::is_void_v<T>, bool, std::optional<T>> result{};
        } ctx;

        auto runner = [](Task<T> t, SyncContext* c) -> Task<void> {
            if constexpr (std::is_void_v<T>) {
                co_await std::move(t);
            } else {
                c->result = co_await std::move(t);
            }
            c->done.store(true, std::memory_order_release);
        };

        auto root = runner(std::move(task), &ctx);
        root.get_handle().resume();

        while (!ctx.done.load(std::memory_order_acquire)) {
            if (JobScheduler::is_main_thread()) {
                JobScheduler::drain_main_thread(1);
            }
            assist_work();
        }

        if constexpr (!std::is_void_v<T>) {
            return std::move(*ctx.result);
        }
    }

    // -------------------------------------------------------------------------
    // parallel_for: Range batching over BatchAwaiter
    // -------------------------------------------------------------------------

    template <typename Index, typename Func>
    inline Task<void> parallel_for(Index begin, Index end, Func&& func, size_t batch_size = 0)
    {
        if (begin >= end) {
            co_return;
        }

        size_t count = static_cast<size_t>(end - begin);
        if (batch_size == 0) {
            batch_size = (count + 15) / 16;
            if (batch_size < 1) {
                batch_size = 1;
            }
        }

        size_t num_batches = (count + batch_size - 1) / batch_size;
        auto   awaiter     = std::make_shared<BatchAwaiter>(static_cast<uint>(num_batches));

        for (size_t b = 0; b < num_batches; ++b) {
            Index b_start = begin + static_cast<Index>(b * batch_size);
            Index b_end   = std::min(end, begin + static_cast<Index>((b + 1) * batch_size));
            JobScheduler::schedule(JobPriority::HIGH, [b_start, b_end, func, awaiter]() {
                for (Index i = b_start; i < b_end; ++i) {
                    func(i);
                }
                awaiter->finish();
            });
        }

        co_await *awaiter;
    }

    // -------------------------------------------------------------------------
    // when_all: Fan-out and wait for multiple tasks concurrently
    // -------------------------------------------------------------------------

    template <typename... Tasks>
    inline Task<void> when_all(Tasks... tasks)
    {
        constexpr size_t N = sizeof...(Tasks);
        if constexpr (N == 0) {
            co_return;
        } else {
            auto awaiter = std::make_shared<BatchAwaiter>(static_cast<uint>(N));

            auto launch = [](auto t, std::shared_ptr<BatchAwaiter> a) -> Task<void> {
                co_await std::move(t);
                a->finish();
            };

            auto schedule_one = [&](auto& t) {
                JobScheduler::schedule([t = std::move(t), awaiter, launch]() mutable {
                    auto r = launch(std::move(t), awaiter);
                    r.get_handle().resume();
                });
            };

            (schedule_one(tasks), ...);
            co_await *awaiter;
        }
    }

} // namespace lyra

#endif // LYRA_ENGINE_JOBSYSTEM_TASKS_H
