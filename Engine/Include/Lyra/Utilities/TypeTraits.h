#pragma once

#ifndef LYRA_ENGINE_UTILITIES_TYPE_TRAITS_H
#define LYRA_ENGINE_UTILITIES_TYPE_TRAITS_H

#include <type_traits>

namespace lyra
{
    /**
     * @brief Type alias for std::conditional_t.
     */
    template <bool B, typename T, typename F>
    using Conditional = std::conditional_t<B, T, F>;

    /**
     * @brief Type alias for std::enable_if_t.
     */
    template <bool B, typename T = void>
    using EnableIf = std::enable_if_t<B, T>;

    /**
     * @brief Type alias for std::remove_cvref_t.
     */
    template <typename T>
    using RemoveCvRef = std::remove_cvref_t<T>;

    /**
     * @brief Type alias for std::decay_t.
     */
    template <typename T>
    using Decay = std::decay_t<T>;

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_TYPE_TRAITS_H
