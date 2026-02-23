#pragma once

#ifndef LYRA_LIBRARY_COMMON_FUNCTION_H
#define LYRA_LIBRARY_COMMON_FUNCTION_H

#include <utility>
#include <cstddef>
#include <etl/delegate.h>
#include <absl/functional/function_ref.h>
#include <absl/functional/any_invocable.h>
#include <Lyra/Common/Macros.h>

namespace lyra
{

    template <typename F>
    FORCE_INLINE decltype(auto) execute(F&& fn)
    {
        return std::forward<F>(fn)();
    }

    template <typename F>
    FORCE_INLINE void execute_once(F&& fn)
    {
        static bool init = true;
        if (init) {
            init = false;
            std::forward<F>(fn)();
        }
    }

    template <typename... Args>
    using Function = absl::AnyInvocable<Args...>;

    template <typename... Args>
    using Delegate = etl::delegate<Args...>;

    // primary template for non-function types (default case)
    template <typename T>
    struct function_traits;

    // specialization for free functions
    template <typename R, typename... Args>
    struct function_traits<R (*)(Args...)>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        using return_type                  = R;
        template <std::size_t N>
        using arg_type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    };

    // specialization for member functions
    template <typename Class, typename R, typename... Args>
    struct function_traits<R (Class::*)(Args...)>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        using return_type                  = R;
        using class_type                   = Class;
        template <std::size_t N>
        using arg_type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    };

    // specialization for const member functions
    template <typename Class, typename R, typename... Args>
    struct function_traits<R (Class::*)(Args...) const>
    {
        static constexpr std::size_t arity = sizeof...(Args);
        using return_type                  = R;
        using class_type                   = Class;
        template <std::size_t N>
        using arg_type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    };

} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_FUNCTION_H
