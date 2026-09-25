#include <mutex>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <cassert>
#include <condition_variable>

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/JobSystem/Jobs.h>
#include "JobSystemInternal.h"
#include "JobClosurePool.h"

#if defined(__APPLE__)
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void  objc_autoreleasePoolPop(void*);

struct AutoReleaseScope
{
    void* pool = nullptr;
    AutoReleaseScope() : pool(objc_autoreleasePoolPush()) {}
    ~AutoReleaseScope()
    {
        if (pool) {
            objc_autoreleasePoolPop(pool);
        }
    }
};
#endif

using namespace lyra;

#pragma region Types & Aliases
constexpr size_t PRIORITY_COUNT = static_cast<size_t>(JobPriority::COUNT);

using JobDeque            = ChaseLevDeque<Job>;
using JobDequeArray       = Array<JobDeque, PRIORITY_COUNT>;
using InjectionQueueArray = Array<ConcurrentJobQueue, PRIORITY_COUNT>;

struct WorkerState
{
    uint          id = 0;
    JobDequeArray deques;
};

using WorkerPtr  = Own<WorkerState>;
using WorkerList = Vector<WorkerPtr>;
using ThreadList = Vector<std::thread>;

struct SchedulerState
{
    std::thread::id         main_thread_id;
    std::atomic<bool>       initialized               = false;
    std::atomic<bool>       running                   = false;
    std::atomic<uint>       active_workers            = 0;
    std::atomic<uint>       active_background_workers = 0;
    uint                    worker_count              = 0;
    uint                    max_background_workers    = 0;
    WorkerList              workers;
    ThreadList              threads;
    InjectionQueueArray     injection_queues;
    ConcurrentJobQueue      main_queue;
    std::mutex              sleep_mutex;
    std::condition_variable sleep_cv;
    CentralClosurePool      closure_pool;

    ~SchedulerState();
};
#pragma endregion Types& Aliases

#pragma region Internal Helpers
static SchedulerState   g_scheduler;
static thread_local int g_tl_worker_index = -1;

static bool has_any_work()
{
    for (const auto& q : g_scheduler.injection_queues) {
        if (!q.empty()) {
            return true;
        }
    }
    for (const auto& w : g_scheduler.workers) {
        for (const auto& d : w->deques) {
            if (!d.empty()) {
                return true;
            }
        }
    }
    return false;
}

static bool pop_job_for_worker(uint worker_id, Job& job, bool& is_background)
{
    auto& worker = *g_scheduler.workers[worker_id];

    // 1. check own deques in priority order
    for (size_t p = 0; p < PRIORITY_COUNT; ++p) {
        if (p == static_cast<size_t>(JobPriority::BACKGROUND)) {
            if (g_scheduler.active_background_workers.load(std::memory_order_relaxed) >= g_scheduler.max_background_workers) {
                continue;
            }
        }
        if (worker.deques[p].pop_bottom(job)) {
            is_background = (p == static_cast<size_t>(JobPriority::BACKGROUND));
            return true;
        }
    }

    // 2. check global injection queues
    for (size_t p = 0; p < PRIORITY_COUNT; ++p) {
        if (p == static_cast<size_t>(JobPriority::BACKGROUND)) {
            if (g_scheduler.active_background_workers.load(std::memory_order_relaxed) >= g_scheduler.max_background_workers) {
                continue;
            }
        }
        if (g_scheduler.injection_queues[p].pop(job)) {
            is_background = (p == static_cast<size_t>(JobPriority::BACKGROUND));
            return true;
        }
    }

    // 3. steal from other workers
    uint num_workers = g_scheduler.worker_count;
    if (num_workers > 1) {
        uint victim = (worker_id + 1) % num_workers;
        for (uint i = 0; i < num_workers - 1; ++i) {
            auto& peer = *g_scheduler.workers[victim];
            for (size_t p = 0; p < PRIORITY_COUNT; ++p) {
                if (p == static_cast<size_t>(JobPriority::BACKGROUND)) {
                    if (g_scheduler.active_background_workers.load(std::memory_order_relaxed) >= g_scheduler.max_background_workers)
                        continue;
                }
                if (peer.deques[p].steal(job)) {
                    is_background = (p == static_cast<size_t>(JobPriority::BACKGROUND));
                    return true;
                }
            }
            victim = (victim + 1) % num_workers;
        }
    }

    return false;
}

static void worker_thread_main(uint worker_id)
{
    g_tl_worker_index = static_cast<int>(worker_id);

    while (g_scheduler.running.load(std::memory_order_relaxed)) {
        Job  job;
        bool is_background = false;
        if (pop_job_for_worker(worker_id, job, is_background)) {
            g_scheduler.active_workers.fetch_add(1, std::memory_order_relaxed);
            if (is_background) {
                g_scheduler.active_background_workers.fetch_add(1, std::memory_order_relaxed);
            }
#if defined(__APPLE__)
            AutoReleaseScope pool;
#endif
            job.execute();
            if (is_background) {
                g_scheduler.active_background_workers.fetch_sub(1, std::memory_order_relaxed);
            }
            g_scheduler.active_workers.fetch_sub(1, std::memory_order_relaxed);
            continue;
        }

        // back off and sleep if no work is found
        std::unique_lock lock(g_scheduler.sleep_mutex);
        if (has_any_work()) {
            continue;
        }
        g_scheduler.sleep_cv.wait_for(lock, std::chrono::microseconds(100), [&] {
            return !g_scheduler.running.load(std::memory_order_relaxed) || has_any_work();
        });
    }
}

bool lyra::assist_work()
{
    Job  job;
    bool is_background = false;
    if (g_tl_worker_index >= 0 && g_tl_worker_index < static_cast<int>(g_scheduler.worker_count)) {
        if (pop_job_for_worker(static_cast<uint>(g_tl_worker_index), job, is_background)) {
            job.execute();
            return true;
        }
    } else {
        // main / external thread only steals from workers' deques to help with distributed parallel work
        for (const auto& w : g_scheduler.workers) {
            for (size_t p = 0; p < PRIORITY_COUNT; ++p) {
                if (w->deques[p].steal(job)) {
                    job.execute();
                    return true;
                }
            }
        }
        std::this_thread::yield();
    }
    return false;
}
#pragma endregion Internal Helpers

#pragma region JobScheduler
void JobScheduler::init(const JobSystemDescriptor& desc)
{
    if (g_scheduler.initialized.exchange(true)) {
        return;
    }

    g_scheduler.main_thread_id = std::this_thread::get_id();

    uint worker_count = desc.max_workers;
    if (worker_count == 0) {
        uint hw      = std::thread::hardware_concurrency();
        worker_count = hw > 1 ? hw - 1 : 1;
    }

    g_scheduler.worker_count = worker_count;

    uint bg_workers = desc.max_background_workers;
    if (bg_workers == 0) {
        bg_workers = std::max(1u, (worker_count > 2 ? worker_count - 2 : 1));
    } else {
        bg_workers = std::min(bg_workers, worker_count);
    }
    g_scheduler.max_background_workers = bg_workers;

    g_scheduler.running.store(true);

    g_scheduler.workers.reserve(worker_count);
    for (uint i = 0; i < worker_count; ++i) {
        auto w = std::make_unique<WorkerState>();
        w->id  = i;
        g_scheduler.workers.push_back(std::move(w));
    }

    g_scheduler.threads.reserve(worker_count);
    for (uint i = 0; i < worker_count; ++i) {
        g_scheduler.threads.emplace_back(worker_thread_main, i);
    }
}

void JobScheduler::shutdown()
{
    if (!g_scheduler.initialized.exchange(false)) {
        return;
    }

    g_scheduler.running.store(false);
    g_scheduler.sleep_cv.notify_all();

    for (auto& th : g_scheduler.threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    g_scheduler.threads.clear();
    g_scheduler.workers.clear();
    g_scheduler.main_queue.drain();
}

SchedulerState::~SchedulerState()
{
    JobScheduler::shutdown();
    closure_pool.clear();
}

bool JobScheduler::is_initialized()
{
    return g_scheduler.initialized.load(std::memory_order_acquire);
}

bool JobScheduler::is_main_thread()
{
    return std::this_thread::get_id() == g_scheduler.main_thread_id;
}

bool JobScheduler::is_worker_thread()
{
    return g_tl_worker_index >= 0;
}

void JobScheduler::wait_idle()
{
    while (has_any_work() || g_scheduler.active_workers.load(std::memory_order_relaxed) > 0 || !g_scheduler.main_queue.empty()) {
        if (is_main_thread()) {
            drain_main_thread(0);
        }
        assist_work();
        std::this_thread::yield();
    }
}

void JobScheduler::schedule(JobPriority priority, Job job)
{
    assert(is_initialized() && "JobScheduler must be initialized before scheduling jobs!");

    size_t p = static_cast<size_t>(priority);
    if (g_tl_worker_index >= 0 && g_tl_worker_index < static_cast<int>(g_scheduler.worker_count)) {
        if (!g_scheduler.workers[g_tl_worker_index]->deques[p].push_bottom(job)) {
            g_scheduler.injection_queues[p].push(job);
        }
    } else {
        g_scheduler.injection_queues[p].push(job);
    }

    g_scheduler.sleep_cv.notify_one();
}

void JobScheduler::schedule_main(Job job)
{
    assert(is_initialized() && "JobScheduler must be initialized before scheduling jobs!");
    g_scheduler.main_queue.push(job);
}

void JobScheduler::drain_main_thread(uint max_jobs)
{
    g_scheduler.main_queue.drain(max_jobs);
}

static thread_local ThreadClosureCache g_tl_closure_cache;

void* JobScheduler::allocate_closure(size_t size)
{
    if (size <= CLOSURE_BLOCK_SIZE) {
        if (g_tl_closure_cache.free_list) {
            auto* b                      = g_tl_closure_cache.free_list;
            g_tl_closure_cache.free_list = b->next;
            g_tl_closure_cache.count--;
            return b;
        }
        return g_scheduler.closure_pool.allocate();
    }
    return ::operator new(size);
}

void JobScheduler::deallocate_closure(void* ptr, size_t size)
{
    if (size <= CLOSURE_BLOCK_SIZE && ptr) {
        if (g_tl_closure_cache.count < ThreadClosureCache::MAX_CACHED) {
            auto* b                      = static_cast<ClosureBlock*>(ptr);
            b->next                      = g_tl_closure_cache.free_list;
            g_tl_closure_cache.free_list = b;
            g_tl_closure_cache.count++;
            return;
        }
        g_scheduler.closure_pool.deallocate(ptr);
        return;
    }
    ::operator delete(ptr);
}
#pragma endregion JobScheduler

#pragma region Coroutine Awaiters
bool MainThreadAwaiter::await_ready() const noexcept
{
    return JobScheduler::is_main_thread();
}

void MainThreadAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
    JobScheduler::schedule_main(Job{[](void* ptr) {
        std::coroutine_handle<>::from_address(ptr).resume();
    }, handle.address()});
}

bool WorkerAwaiter::await_ready() const noexcept
{
    return JobScheduler::is_worker_thread();
}

void WorkerAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
{
    JobScheduler::schedule(priority, Job{[](void* ptr) {
        std::coroutine_handle<>::from_address(ptr).resume();
    }, handle.address()});
}
#pragma endregion Coroutine Awaiters
