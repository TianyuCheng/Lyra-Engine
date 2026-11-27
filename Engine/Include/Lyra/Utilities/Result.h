#pragma once

#ifndef LYRA_ENGINE_UTILITIES_RESULT_H
#define LYRA_ENGINE_UTILITIES_RESULT_H

#include <utility>
#include <type_traits>
#include <tl/expected.hpp>
#include <Lyra/Utilities/String.h>

namespace lyra
{
    // =========================================================================
    // Expected & Unexpected Type Aliases (matching Lyra convention for std types)
    // =========================================================================

    template <typename T, typename E>
    using Expected = tl::expected<T, E>;

    template <typename E>
    using Unexpected = tl::unexpected<E>;

    /// Synonym for Expected<T, E> for concise function signatures.
    /// Defaults error type to String if unspecified.
    template <typename T, typename E = String>
    using Result = Expected<T, E>;

    using unexpect_t = tl::unexpect_t;
    inline constexpr tl::unexpect_t unexpect{};

    template <typename E>
    using BadExpectedAccess = tl::bad_expected_access<E>;

    /// Factory function for Unexpected<E> matching std::make_unexpected / tl::make_unexpected
    template <typename E>
    constexpr auto make_unexpected(E&& e) -> tl::unexpected<std::decay_t<E>>
    {
        return tl::make_unexpected(std::forward<E>(e));
    }

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_RESULT_H
