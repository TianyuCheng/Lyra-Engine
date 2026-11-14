#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_JOB_SCHEDULER_H
#define LYRA_ENGINE_JOBSYSTEM_JOB_SCHEDULER_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/TypeTraits.h>

namespace lyra
{

    // -------------------------------------------------------------------------
    // Forward declarations for coroutine awaiters
    // -------------------------------------------------------------------------

    struct WorkerAwaiter;
    struct MainThreadAwaiter;

    // -------------------------------------------------------------------------
    // Priority levels & POD Job
    // -------------------------------------------------------------------------

    enum class JobPriority : uint8_t
    {
        HIGH       = 0, // frame-critical work (culling, render prep, scene update)
        NORMAL     = 1, // general engine and gameplay computation (default)
        BACKGROUND = 2  // low-urgency background compute
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
        static void* allocate_closure(usize size);
        static void  deallocate_closure(void* ptr, usize size);

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
                schedule(priority,
                    Job{[](void* data) {
                            auto callable = static_cast<DecayedF*>(data);
                            (*callable)();
                            callable->~DecayedF();
                            deallocate_closure(data, sizeof(DecayedF));
                        },
                        fn});
            } else {
                auto fn = new DecayedF(std::forward<F>(func));
                schedule(priority,
                    Job{[](void* data) {
                            auto callable = static_cast<DecayedF*>(data);
                            (*callable)();
                            delete callable;
                        },
                        fn});
            }
        }

        // main-thread dispatch & draining
        static void schedule_main(Job job);
        static void drain_main_thread(uint max_jobs = 0);

        // register external completion pump (e.g. GPU fence monitor)
        using ExternalPumpFn = void (*)();
        static void register_external_pump(ExternalPumpFn fn);

        // coroutine awaiters (defined in Task.h)
        static MainThreadAwaiter main_thread() noexcept;
        static WorkerAwaiter     worker(JobPriority priority = JobPriority::NORMAL) noexcept;
    };

    // internal helper to assist running available work while waiting
    bool assist_work();

} // namespace lyra

#endif // LYRA_ENGINE_JOBSYSTEM_JOB_SCHEDULER_H
