#pragma once

#ifndef LYRA_ENGINE_UTILITIES_COROUTINE_H
#define LYRA_ENGINE_UTILITIES_COROUTINE_H

#include <coroutine>

namespace lyra
{
    /**
     * @brief Strongly-typed or type-erased coroutine handle alias.
     */
    template <typename Promise = void>
    using CoroutineHandle = std::coroutine_handle<Promise>;

    using SuspendNever  = std::suspend_never;
    using SuspendAlways = std::suspend_always;

    template <typename T, typename... Args>
    using CoroutineTraits = std::coroutine_traits<T, Args...>;

    inline auto noop_coroutine() noexcept
    {
        return std::noop_coroutine();
    }

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_COROUTINE_H
