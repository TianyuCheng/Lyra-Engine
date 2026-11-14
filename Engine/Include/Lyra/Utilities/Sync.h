#pragma once

#ifndef LYRA_ENGINE_UTILITIES_SYNC_H
#define LYRA_ENGINE_UTILITIES_SYNC_H

#include <mutex>
#include <atomic>
#include <utility>
#include <shared_mutex>
#include <condition_variable>

#include <Lyra/Utilities/Macros.h>

namespace lyra
{
    // Common synchronization aliases
    using Mutex          = std::mutex;
    using RecursiveMutex = std::recursive_mutex;
    using SharedMutex    = std::shared_mutex;

    template <typename M>
    using LockGuard = std::lock_guard<M>;

    template <typename M>
    using UniqueLock = std::unique_lock<M>;

    template <typename M>
    using SharedLock = std::shared_lock<M>;

    // Condition variable aliases
    using ConditionVariable    = std::condition_variable;
    using ConditionVariableAny = std::condition_variable_any;
    using CVStatus             = std::cv_status;

    // Atomic aliases & memory order
    template <typename T>
    using Atomic = std::atomic<T>;

    using AtomicFlag  = std::atomic_flag;
    using MemoryOrder = std::memory_order;

    inline constexpr auto memory_order_relaxed = std::memory_order_relaxed;
    inline constexpr auto memory_order_consume = std::memory_order_consume;
    inline constexpr auto memory_order_acquire = std::memory_order_acquire;
    inline constexpr auto memory_order_release = std::memory_order_release;
    inline constexpr auto memory_order_acq_rel = std::memory_order_acq_rel;
    inline constexpr auto memory_order_seq_cst = std::memory_order_seq_cst;

    using std::atomic_thread_fence;
    using std::atomic_signal_fence;

    /**
     * @brief Executes a callable while holding an exclusive lock on the mutex.
     */
    template <typename M, typename F>
    FORCE_INLINE decltype(auto) with_lock(M& mutex, F&& fn)
    {
        std::lock_guard lock(mutex);
        return std::forward<F>(fn)();
    }

    /**
     * @brief Executes a callable while holding a shared/read lock on the mutex.
     */
    template <typename M, typename F>
    FORCE_INLINE decltype(auto) with_shared_lock(M& mutex, F&& fn)
    {
        std::shared_lock lock(mutex);
        return std::forward<F>(fn)();
    }

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_SYNC_H
