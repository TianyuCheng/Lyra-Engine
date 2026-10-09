#pragma once

#ifndef LYRA_ENGINE_UTILITIES_SYNC_H
#define LYRA_ENGINE_UTILITIES_SYNC_H

#include <mutex>
#include <utility>
#include <shared_mutex>

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
