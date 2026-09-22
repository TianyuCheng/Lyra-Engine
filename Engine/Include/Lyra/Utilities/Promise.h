#pragma once

#ifndef LYRA_ENGINE_UTILITIES_PROMISE_H
#define LYRA_ENGINE_UTILITIES_PROMISE_H

#include <future>

namespace lyra
{
    /**
     * @brief Type aliases for standard C++ asynchronous primitives.
     */
    template <typename T>
    using Promise = std::promise<T>;

    template <typename T>
    using Future = std::future<T>;

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_PROMISE_H
