#include "SoftThreadPool.h"
#include <algorithm>
#include <cstdlib>

using namespace lyra;
using namespace lyra::softgpu;

SoftThreadPool::~SoftThreadPool()
{
    shutdown();
}

void SoftThreadPool::init(uint thread_count)
{
    shutdown();
    stop.store(false, memory_order_relaxed);

    if (thread_count == 0) {
        // check environment variable override
        const char* env_threads = std::getenv("LYRA_SOFTGPU_THREADS");
        if (env_threads && *env_threads) {
            thread_count = static_cast<uint>(std::atoi(env_threads));
        } else {
            thread_count = std::thread::hardware_concurrency();
        }
    }
    if (thread_count < 1) thread_count = 1;
    thread_count = std::min(thread_count, 64u);

    // spawn thread_count - 1 background workers (caller thread also participates in work)
    workers.reserve(thread_count - 1);
    for (uint i = 0; i < thread_count - 1; ++i) {
        workers.emplace_back(&SoftThreadPool::worker_loop, this, i);
    }
}

void SoftThreadPool::shutdown()
{
    if (stop.load(memory_order_relaxed)) return;
    {
        UniqueLock lock(mutex);
        stop.store(true, memory_order_release);
        cv_work.notify_all();
    }
    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    workers.clear();
}

void SoftThreadPool::dispatch_work(uint count, SoftTaskDesc task)
{
    {
        UniqueLock lock(mutex);
        current_task = task;
        next_index.store(0, memory_order_relaxed);
        remaining.store(count, memory_order_release);
        active_workers.store(static_cast<uint>(workers.size()), memory_order_release);
        total_count = count;
        generation++;
        cv_work.notify_all();
    }

    // calling thread participates
    while (true) {
        uint idx = next_index.fetch_add(1, memory_order_relaxed);
        if (idx >= count) break;
        task.execute(idx);
        remaining.fetch_sub(1, memory_order_acq_rel);
    }

    // wait until all work is done and all workers have exited their loop
    UniqueLock lock(mutex);
    cv_done.wait(lock, [&] {
        return remaining.load(memory_order_acquire) == 0 &&
               active_workers.load(memory_order_acquire) == 0;
    });
    current_task = {};
}

void SoftThreadPool::worker_loop(uint worker_id)
{
    uint local_gen = 0;
    while (true) {
        SoftTaskDesc local_task{};
        uint         count = 0;
        {
            UniqueLock lock(mutex);
            cv_work.wait(lock, [&] {
                return stop.load(memory_order_relaxed) || generation != local_gen;
            });
            if (stop.load(memory_order_relaxed)) {
                break;
            }
            local_gen  = generation;
            local_task = current_task;
            count      = total_count;
        }

        while (true) {
            uint idx = next_index.fetch_add(1, memory_order_relaxed);
            if (idx >= count) break;
            local_task.execute(idx);
            remaining.fetch_sub(1, memory_order_acq_rel);
        }

        if (active_workers.fetch_sub(1, memory_order_acq_rel) == 1) {
            UniqueLock lock(mutex);
            cv_done.notify_all();
        }
    }
}
