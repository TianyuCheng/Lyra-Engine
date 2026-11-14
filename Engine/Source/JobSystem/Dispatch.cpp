#include <atomic>

#include <Lyra/JobSystem/Dispatch.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Sync.h>

using namespace lyra;

namespace
{
    struct PendingFence
    {
        GPUFence         fence;
        JobPromise<void> promise;
    };

    Mutex                g_pending_mutex;
    Vector<PendingFence> g_pending_fences;
    Atomic<usize>        g_pending_count{0};
    Atomic<bool>         g_monitor_initialized{false};
}

void GPUCompletionMonitor::init()
{
    if (g_monitor_initialized.exchange(true)) {
        return;
    }

    // automatically hook into JobScheduler external pump
    JobScheduler::register_external_pump(&GPUCompletionMonitor::poll_completions);
}

void GPUCompletionMonitor::register_fence(GPUFence fence, JobPromise<void> promise)
{
    init();

    LockGuard lock(g_pending_mutex);
    g_pending_fences.push_back(PendingFence{fence, std::move(promise)});
    g_pending_count.store(g_pending_fences.size(), memory_order_relaxed);
}

void GPUCompletionMonitor::poll_completions()
{
    if (g_pending_count.load(memory_order_relaxed) == 0) {
        return;
    }

    Vector<JobPromise<void>> completed;
    {
        UniqueLock lock(g_pending_mutex, std::try_to_lock);
        if (!lock.owns_lock() || g_pending_fences.empty()) {
            return;
        }

        for (auto it = g_pending_fences.begin(); it != g_pending_fences.end();) {
            if (it->fence.poll()) {
                completed.push_back(std::move(it->promise));
                it = g_pending_fences.erase(it);
            } else {
                ++it;
            }
        }
        g_pending_count.store(g_pending_fences.size(), memory_order_relaxed);
    }

    for (auto& promise : completed) {
        promise.set_value();
    }
}

auto GPUCompletionMonitor::wait_fence(GPUFence fence, JobPriority priority) -> Task<void>
{
    if (fence.poll()) {
        co_return;
    }

    JobPromise<void> promise(priority);
    auto             task = promise.get_task();
    register_fence(fence, std::move(promise));
    co_await std::move(task);
}

auto GPUCompletionMonitor::pending_count() -> usize
{
    return g_pending_count.load(memory_order_relaxed);
}
