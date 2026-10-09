#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_JOBS_H
#define LYRA_ENGINE_JOBSYSTEM_JOBS_H

#include <atomic>
#include <coroutine>
#include <type_traits>

#include <Lyra/Utilities/Stdint.h>

namespace lyra
{

    // -------------------------------------------------------------------------
    // Priority levels & POD Job
    // -------------------------------------------------------------------------

    enum class JobPriority : uint8_t
    {
        HIGH       = 0, // frame-critical work (culling, render prep, scene update)
        NORMAL     = 1, // general engine and gameplay computation (default)
        BACKGROUND = 2, // low-urgency background compute
        COUNT      = 3
    };

    struct Job
    {
        void (*fn)(void*) = nullptr;
        void* data        = nullptr;

        void execute() const noexcept
        {
            if (fn) fn(data);
        }
    };

    // -------------------------------------------------------------------------
    // Coroutine awaiters
    // -------------------------------------------------------------------------

    struct MainThreadAwaiter
    {
        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };

    struct WorkerAwaiter
    {
        JobPriority priority;

        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };

    struct FinalAwaiter
    {
        std::coroutine_handle<> continuation = nullptr;

        bool await_ready() const noexcept
        {
            return false;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept
        {
            return continuation ? continuation : std::noop_coroutine();
        }

        void await_resume() const noexcept {}
    };

    struct BatchAwaiter
    {
        std::atomic<uint>       remaining;
        std::coroutine_handle<> continuation = nullptr;
        JobPriority             priority     = JobPriority::NORMAL;

        explicit BatchAwaiter(uint count, JobPriority priority = JobPriority::NORMAL)
            : remaining(count), priority(priority) {}

        bool await_ready() const noexcept
        {
            return remaining.load(std::memory_order_acquire) == 0;
        }

        void await_resume() const noexcept {}
        void await_suspend(std::coroutine_handle<> h) noexcept;
        void finish();
    };

    // -------------------------------------------------------------------------
    // JobSystemDescriptor: Scheduler configuration
    // -------------------------------------------------------------------------

    struct JobSystemDescriptor
    {
        uint max_workers            = 0; // 0 = auto-detect hardware concurrency
        uint max_background_workers = 0; // 0 = auto quota, or explicit maximum concurrent background workers
    };

    // -------------------------------------------------------------------------
    // JobScheduler: Process-wide scheduler and dispatch interface
    // -------------------------------------------------------------------------

    struct JobScheduler
    {
        // lifecycle
        static void init(const JobSystemDescriptor& desc = {});
        static void shutdown();
        static bool is_initialized();
        static bool is_main_thread();
        static bool is_worker_thread();
        static void wait_idle(); // drains and waits for all active jobs to complete (e.g. before hot reload)

        // schedule raw POD job (priority is first argument)
        static void schedule(Job job) { schedule(JobPriority::NORMAL, job); }
        static void schedule(JobPriority priority, Job job);

        // closure allocator for high-performance lambda dispatch without heap churn
        static void* allocate_closure(size_t size);
        static void  deallocate_closure(void* ptr, size_t size);

        // schedule arbitrary invocable / lambda (default priority)
        template <typename F>
        requires(!std::is_same_v<std::decay_t<F>, Job> && !std::is_same_v<std::decay_t<F>, JobPriority>)
        static void schedule(F&& func)
        {
            schedule(JobPriority::NORMAL, std::forward<F>(func));
        }

        // schedule arbitrary invocable / lambda (custom priority)
        template <typename F>
        requires(!std::is_same_v<std::decay_t<F>, Job> && !std::is_same_v<std::decay_t<F>, JobPriority>)
        static void schedule(JobPriority priority, F&& func)
        {
            using DecayedF = std::decay_t<F>;
            if constexpr (sizeof(DecayedF) <= 128 && alignof(DecayedF) <= alignof(std::max_align_t)) {
                auto mem = allocate_closure(sizeof(DecayedF));
                auto fn  = new (mem) DecayedF(std::forward<F>(func));
                schedule(priority, Job{[](void* data) {
                    auto* callable = static_cast<DecayedF*>(data);
                    (*callable)();
                    callable->~DecayedF();
                    deallocate_closure(data, sizeof(DecayedF));
                }, fn});
            } else {
                auto fn = new DecayedF(std::forward<F>(func));
                schedule(priority, Job{[](void* data) {
                    auto* callable = static_cast<DecayedF*>(data);
                    (*callable)();
                    delete callable;
                }, fn});
            }
        }

        // main-thread dispatch & draining
        static void schedule_main(Job job);
        static void drain_main_thread(uint max_jobs = 0);

        // coroutine awaiters
        static auto main_thread() noexcept { return MainThreadAwaiter{}; }
        static auto worker(JobPriority priority = JobPriority::NORMAL) noexcept { return WorkerAwaiter{priority}; }
    };

    inline void BatchAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
    {
        continuation = h;
        if (remaining.load(std::memory_order_acquire) == 0) {
            JobScheduler::schedule(priority, Job{[](void* ptr) {
                std::coroutine_handle<>::from_address(ptr).resume();
            }, h.address()});
        }
    }

    inline void BatchAwaiter::finish()
    {
        if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (continuation) {
                JobScheduler::schedule(priority, Job{[](void* ptr) {
                    std::coroutine_handle<>::from_address(ptr).resume();
                }, continuation.address()});
            }
        }
    }

    // internal helper to assist running available work while waiting
    bool assist_work();

} // namespace lyra

#endif // LYRA_ENGINE_JOBSYSTEM_JOBS_H
