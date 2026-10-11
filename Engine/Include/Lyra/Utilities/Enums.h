#pragma once

#ifndef LYRA_ENGINE_UTILITIES_ENUMS_H
#define LYRA_ENGINE_UTILITIES_ENUMS_H

#include <cstddef>
#include <magic_enum/magic_enum.hpp>

#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>

namespace lyra
{
    // import magic_enum namespace for unqualified access
    using namespace magic_enum;

    // qualified imports into lyra namespace
    using magic_enum::enum_cast;
    using magic_enum::enum_contains;
    using magic_enum::enum_count;
    using magic_enum::enum_entries;
    using magic_enum::enum_index;
    using magic_enum::enum_integer;
    using magic_enum::enum_name;
    using magic_enum::enum_names;
    using magic_enum::enum_type_name;
    using magic_enum::enum_underlying;
    using magic_enum::enum_value;
    using magic_enum::enum_values;
    using magic_enum::underlying_type_t;

    // compile-time variable templates
    template <typename E>
    inline constexpr size_t enum_count_v = magic_enum::enum_count<E>();

    template <typename E>
    inline constexpr size_t enum_size_v = magic_enum::enum_count<E>();

    template <typename E>
    inline constexpr auto enum_values_v = magic_enum::enum_values<E>();

    template <typename E>
    inline constexpr auto enum_names_v = magic_enum::enum_names<E>();

    template <typename E>
    inline constexpr auto enum_entries_v = magic_enum::enum_entries<E>();

    // type aliases
    template <typename E>
    using enum_underlying_type_t = magic_enum::underlying_type_t<E>;

    template <typename E>
    using enum_underlying_t = magic_enum::underlying_type_t<E>;

    // lyra string & parsing helpers
    template <typename E>
    inline auto enum_to_string(E value) -> String
    {
        return String(magic_enum::enum_name(value));
    }

    template <typename E>
    FORCE_INLINE constexpr auto enum_from_name(StringView name) noexcept -> Optional<E>
    {
        return magic_enum::enum_cast<E>(name);
    }

    template <typename E>
    FORCE_INLINE constexpr size_t enum_size() noexcept
    {
        return magic_enum::enum_count<E>();
    }

    template <typename E>
    FORCE_INLINE constexpr bool enum_is_valid(E value) noexcept
    {
        return magic_enum::enum_contains<E>(value);
    }

    template <typename E, typename F>
    constexpr void enum_for_each(F&& func)
    {
        for (const auto& value : magic_enum::enum_values<E>()) {
            func(value);
        }
    }
} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_ENUMS_H
