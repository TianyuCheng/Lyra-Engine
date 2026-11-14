#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_TASK_H
#define LYRA_ENGINE_JOBSYSTEM_TASK_H

#include <memory>
#include <variant>
#include <utility>
#include <optional>
#include <algorithm>

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Sync.h>
#include <Lyra/Utilities/Coroutine.h>
#include <Lyra/Utilities/TypeTraits.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/JobSystem/JobScheduler.h>

namespace lyra
{

    // -------------------------------------------------------------------------
    // Coroutine awaiters
    // -------------------------------------------------------------------------

    struct MainThreadAwaiter
    {
        bool await_ready() const noexcept;
        void await_resume() const noexcept {}
        void await_suspend(CoroutineHandle<> handle) noexcept;
    };

    struct WorkerAwaiter
    {
        JobPriority priority;

        bool await_ready() const noexcept;
        void await_resume() const noexcept {}
        void await_suspend(CoroutineHandle<> handle) noexcept;
    };

    struct FinalAwaiter
    {
        CoroutineHandle<> continuation = nullptr;

        bool await_ready() const noexcept
        {
            return false;
        }

        CoroutineHandle<> await_suspend(CoroutineHandle<>) const noexcept
        {
            return continuation ? continuation : noop_coroutine();
        }

        void await_resume() const noexcept {}
    };

    struct BatchAwaiter
    {
        Atomic<uint>      remaining;
        CoroutineHandle<> continuation = nullptr;
        JobPriority       priority     = JobPriority::NORMAL;

        explicit BatchAwaiter(uint count, JobPriority priority = JobPriority::NORMAL)
            : remaining(count), priority(priority) {}

        bool await_ready() const noexcept
        {
            return remaining.load(memory_order_acquire) == 0;
        }

        void await_resume() const noexcept {}
        void await_suspend(CoroutineHandle<> h) noexcept;
        void finish();
    };

    inline void BatchAwaiter::await_suspend(CoroutineHandle<> h) noexcept
    {
        continuation = h;
        if (remaining.load(memory_order_acquire) == 0) {
            JobScheduler::schedule(priority,
                Job{[](void* ptr) {
                        CoroutineHandle<>::from_address(ptr).resume();
                    },
                    h.address()});
        }
    }

    inline void BatchAwaiter::finish()
    {
        if (remaining.fetch_sub(1, memory_order_acq_rel) == 1) {
            if (continuation) {
                JobScheduler::schedule(priority,
                    Job{[](void* ptr) {
                            CoroutineHandle<>::from_address(ptr).resume();
                        },
                        continuation.address()});
            }
        }
    }

    inline MainThreadAwaiter JobScheduler::main_thread() noexcept
    {
        return MainThreadAwaiter{};
    }

    inline WorkerAwaiter JobScheduler::worker(JobPriority priority) noexcept
    {
        return WorkerAwaiter{priority};
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
        using handle_type = CoroutineHandle<promise_type>;

        struct promise_type : TaskPromiseBase<T>
        {
            CoroutineHandle<> continuation{nullptr};

            Task get_return_object() noexcept
            {
                return Task{handle_type::from_promise(*this)};
            }

            SuspendAlways initial_suspend() noexcept
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

        explicit Task() noexcept : handle(nullptr) {}
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

        CoroutineHandle<> await_suspend(CoroutineHandle<> cont) noexcept
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
    // JobPromise<T>: Universal producer-side promise for Task<T>
    // -------------------------------------------------------------------------

    template <typename T = void>
    struct JobPromise
    {
        struct State
        {
            using StorageType = Conditional<std::is_void_v<T>, std::monostate, std::optional<T>>;

            Mutex             mutex;
            bool              ready{false};
            CoroutineHandle<> continuation{nullptr};
            JobPriority       priority = JobPriority::NORMAL;

            [[no_unique_address]] StorageType storage{};

            template <typename... Args>
            void fulfill(Args&&... args)
            {
                CoroutineHandle<> cont{nullptr};
                {
                    LockGuard lock(mutex);
                    if (ready) return;
                    if constexpr (!std::is_void_v<T>) {
                        storage.emplace(std::forward<Args>(args)...);
                    }
                    ready = true;
                    cont  = continuation;
                }
                if (cont) {
                    JobScheduler::schedule(priority,
                        Job{[](void* ptr) {
                                CoroutineHandle<>::from_address(ptr).resume();
                            },
                            cont.address()});
                }
            }
        };

        Ref<State> state = make_ref<State>();

        explicit JobPromise(JobPriority priority = JobPriority::NORMAL)
        {
            state->priority = priority;
        }

        Task<T> get_task()
        {
            return make_task(state);
        }

    private:
        static Task<T> make_task(Ref<State> s)
        {
            struct PromiseAwaiter
            {
                Ref<State> s;

                bool await_ready() const noexcept
                {
                    LockGuard lock(s->mutex);
                    return s->ready;
                }

                T await_resume()
                {
                    if constexpr (!std::is_void_v<T>) {
                        return std::move(*s->storage);
                    }
                }

                bool await_suspend(CoroutineHandle<> h) noexcept
                {
                    LockGuard lock(s->mutex);
                    if (s->ready) {
                        return false;
                    }
                    s->continuation = h;
                    return true;
                }
            };

            if constexpr (std::is_void_v<T>) {
                co_await PromiseAwaiter{s};
                co_return;
            } else {
                co_return co_await PromiseAwaiter{s};
            }
        }

    public:
        template <typename... Args>
        void set_value(Args&&... args)
        {
            state->fulfill(std::forward<Args>(args)...);
        }
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
            using ResultType = Conditional<std::is_void_v<T>, bool, std::optional<T>>;

            Atomic<bool> done{false};
            ResultType   result{};
        } ctx;

        auto runner = [](Task<T> t, SyncContext* c) -> Task<void> {
            if constexpr (std::is_void_v<T>) {
                co_await std::move(t);
            } else {
                c->result = co_await std::move(t);
            }
            c->done.store(true, memory_order_release);
        };

        auto root = runner(std::move(task), &ctx);
        root.get_handle().resume();

        while (!ctx.done.load(memory_order_acquire)) {
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
    inline Task<void> parallel_for(Index begin, Index end, Func&& func, usize batch_size = 0)
    {
        if (begin >= end) {
            co_return;
        }

        usize count = static_cast<usize>(end - begin);
        if (batch_size == 0) {
            batch_size = (count + 15) / 16;
            if (batch_size < 1) {
                batch_size = 1;
            }
        }

        usize num_batches = (count + batch_size - 1) / batch_size;
        auto  awaiter     = make_ref<BatchAwaiter>(static_cast<uint>(num_batches));

        for (usize b = 0; b < num_batches; ++b) {
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
        constexpr usize N = sizeof...(Tasks);
        if constexpr (N == 0) {
            co_return;
        } else {
            auto awaiter = make_ref<BatchAwaiter>(static_cast<uint>(N));

            auto launch = [](auto t, Ref<BatchAwaiter> a) -> Task<void> {
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

#endif // LYRA_ENGINE_JOBSYSTEM_TASK_H
