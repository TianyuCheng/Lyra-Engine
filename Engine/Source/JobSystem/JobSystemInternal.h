#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_INTERNAL_H
#define LYRA_ENGINE_JOBSYSTEM_INTERNAL_H

#include <mutex>
#include <atomic>

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/JobSystem/Jobs.h>

namespace lyra
{

    // -------------------------------------------------------------------------
    // Chase-Lev lock-free work-stealing deque
    // -------------------------------------------------------------------------

    template <typename T, size_t Capacity = 4096>
    struct ChaseLevDeque
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "capacity must be power of two");
        static constexpr size_t MASK = Capacity - 1;

        alignas(64) std::atomic<int64_t> top{0};
        alignas(64) std::atomic<int64_t> bottom{0};
        alignas(64) Array<T, Capacity> buffer{};

    public:
        ChaseLevDeque() = default;

        bool push_bottom(const T& item) noexcept
        {
            int64_t b = bottom.load(std::memory_order_relaxed);
            int64_t t = top.load(std::memory_order_acquire);
            if (b - t >= static_cast<int64_t>(Capacity)) {
                return false;
            }
            buffer[b & MASK] = item;
            std::atomic_thread_fence(std::memory_order_release);
            bottom.store(b + 1, std::memory_order_relaxed);
            return true;
        }

        bool pop_bottom(T& item) noexcept
        {
            int64_t b = bottom.load(std::memory_order_relaxed) - 1;
            bottom.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t t = top.load(std::memory_order_relaxed);
            if (t <= b) {
                item = buffer[b & MASK];
                if (t == b) {
                    if (!top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                        bottom.store(b + 1, std::memory_order_relaxed);
                        return false;
                    }
                    bottom.store(b + 1, std::memory_order_relaxed);
                }
                return true;
            } else {
                bottom.store(b + 1, std::memory_order_relaxed);
                return false;
            }
        }

        bool steal(T& item) noexcept
        {
            int64_t t = top.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            int64_t b = bottom.load(std::memory_order_acquire);
            if (t < b) {
                item = buffer[t & MASK];
                if (top.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    return true;
                }
            }
            return false;
        }

        bool empty() const noexcept
        {
            int64_t b = bottom.load(std::memory_order_relaxed);
            int64_t t = top.load(std::memory_order_relaxed);
            return b <= t;
        }
    };

    // -------------------------------------------------------------------------
    // Concurrent injection queue
    // -------------------------------------------------------------------------

    struct ConcurrentJobQueue
    {
        mutable std::mutex mutex;
        Deque<Job>         queue;

        void push(const Job& job)
        {
            std::lock_guard lock(mutex);
            queue.push_back(job);
        }

        bool pop(Job& job)
        {
            std::lock_guard lock(mutex);
            if (queue.empty()) {
                return false;
            }
            job = queue.front();
            queue.pop_front();
            return true;
        }

        bool empty() const
        {
            std::lock_guard lock(mutex);
            return queue.empty();
        }

        size_t drain(uint max_jobs = 0)
        {
            Vector<Job> to_run;
            {
                std::lock_guard lock(mutex);
                if (queue.empty()) {
                    return 0;
                }
                if (max_jobs == 0 || queue.size() <= max_jobs) {
                    to_run.assign(queue.begin(), queue.end());
                    queue.clear();
                } else {
                    to_run.assign(queue.begin(), queue.begin() + max_jobs);
                    queue.erase(queue.begin(), queue.begin() + max_jobs);
                }
            }
            for (const auto& job : to_run) {
                job.execute();
            }
            return to_run.size();
        }
    };

} // namespace lyra

#endif // LYRA_ENGINE_JOBSYSTEM_INTERNAL_H
