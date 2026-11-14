#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_DISPATCH_H
#define LYRA_ENGINE_JOBSYSTEM_DISPATCH_H

#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Utilities/TypeTraits.h>
#include <Lyra/JobSystem/Task.h>
#include <Lyra/JobSystem/JobScheduler.h>

namespace lyra
{

    // -------------------------------------------------------------------------
    // GPUCompletionMonitor: Non-blocking GPU fence polling & coroutine fulfillment
    // -------------------------------------------------------------------------

    struct GPUCompletionMonitor
    {
        static void init();
        static void poll_completions();
        static void register_fence(GPUFence fence, JobPromise<void> promise);
        static auto wait_fence(GPUFence fence, JobPriority priority = JobPriority::NORMAL) -> Task<void>;
        static auto pending_count() -> usize;
    };

    // -------------------------------------------------------------------------
    // operator co_await: Direct awaiting of GPUFence
    // -------------------------------------------------------------------------

    inline auto operator co_await(const GPUFence& fence) noexcept
    {
        return GPUCompletionMonitor::wait_fence(fence);
    }

    // -------------------------------------------------------------------------
    // dispatch (CPU): Asynchronous CPU task execution returning Task<ReturnType>
    // -------------------------------------------------------------------------

    template <typename F>
    requires std::invocable<F>
    inline Task<std::invoke_result_t<F>> dispatch(F&& func, JobPriority priority = JobPriority::NORMAL)
    {
        using ReturnType = std::invoke_result_t<F>;
        JobPromise<ReturnType> promise(priority);
        auto                   task = promise.get_task();

        JobScheduler::schedule(priority, [p = promise, f = std::forward<F>(func)]() mutable {
            if constexpr (std::is_void_v<ReturnType>) {
                f();
                p.set_value();
            } else {
                p.set_value(f());
            }
        });

        return task;
    }

    template <typename F>
    requires std::invocable<F>
    inline Task<std::invoke_result_t<F>> dispatch(JobPriority priority, F&& func)
    {
        return dispatch(std::forward<F>(func), priority);
    }

    // -------------------------------------------------------------------------
    // dispatch (GPU): Asynchronously record, submit, and await GPU work
    // -------------------------------------------------------------------------

    template <typename F>
    requires std::invocable<F, GPUCommandEncoder&>
    inline Task<void> dispatch(
        GPUDevice&   device,
        GPUQueueType queue,
        F&&          record_fn,
        JobPriority  priority = JobPriority::NORMAL)
    {
        auto fence = device.create_fence();
        auto cmd   = device.create_command_buffer({.queue = queue});

        record_fn(cmd);
        cmd.signal(fence, GPUBarrierSync::ALL);
        cmd.submit();

        co_await GPUCompletionMonitor::wait_fence(fence, priority);

        fence.destroy();
    }

    template <typename F>
    requires std::invocable<F, GPUCommandEncoder&>
    inline Task<void> dispatch(
        GPUDevice&   device,
        GPUQueueType queue,
        JobPriority  priority,
        F&&          record_fn)
    {
        return dispatch(device, queue, std::forward<F>(record_fn), priority);
    }

    template <typename F>
    requires std::invocable<F, GPUCommandEncoder&>
    inline Task<void> dispatch(
        GPUQueueType queue,
        F&&          record_fn,
        JobPriority  priority = JobPriority::NORMAL)
    {
        return dispatch(RHI::get_current_device(), queue, std::forward<F>(record_fn), priority);
    }

    template <typename F>
    requires std::invocable<F, GPUCommandEncoder&>
    inline Task<void> dispatch(
        GPUQueueType queue,
        JobPriority  priority,
        F&&          record_fn)
    {
        return dispatch(RHI::get_current_device(), queue, std::forward<F>(record_fn), priority);
    }

} // namespace lyra

#endif // LYRA_ENGINE_JOBSYSTEM_DISPATCH_H
