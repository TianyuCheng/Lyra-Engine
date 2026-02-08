#pragma once

#ifndef LYRA_LIBRARY_COMMON_BITFLAGS_H
#define LYRA_LIBRARY_COMMON_BITFLAGS_H

#include <type_traits>

template <typename E>
struct enable_bitflags : std::false_type
{
};

#define ENABLE_BIT_FLAGS(E)                    \
    template <>                                \
    struct enable_bitflags<E> : std::true_type \
    {                                          \
    };

template <typename E>
constexpr bool enable_bitflags_v = enable_bitflags<E>::value;

template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
struct BitFlags
{
    using T = BitFlags<E>;
    using U = std::underlying_type_t<E>;

    U value;

    constexpr BitFlags() : value(U(0)) {}

    constexpr BitFlags(U value) : value(value) {}

    constexpr BitFlags(E value) : value(static_cast<U>(value)) {}

    constexpr bool contains(E flag) const
    {
        return value & static_cast<U>(flag);
    }

    constexpr void set(E flag, bool enabled)
    {
        if (enabled)
            set(flag);
        else
            unset(flag);
    }

    constexpr void set(E flag)
    {
        *this = *this | flag;
    }

    constexpr void unset(E flag)
    {
        *this = *this & ~BitFlags(flag);
    }

    constexpr T& operator|=(T rhs)
    {
        *this = *this | rhs;
        return *this;
    }

    constexpr T& operator&=(T rhs)
    {
        *this = *this & rhs;
        return *this;
    }

    constexpr T& operator^=(T rhs)
    {
        *this = *this ^ rhs;
        return *this;
    }

    constexpr friend bool operator==(T lhs, T rhs)
    {
        return lhs.value == rhs.value;
    }

    constexpr friend T operator~(T lhs)
    {
        return T(~lhs.value);
    }

    constexpr friend T operator|(T lhs, T rhs)
    {
        return T(lhs.value | rhs.value);
    }

    constexpr friend T operator&(T lhs, T rhs)
    {
        return T(lhs.value & rhs.value);
    }

    constexpr friend T operator^(T lhs, T rhs)
    {
        return T(lhs.value ^ rhs.value);
    }
};

template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && enable_bitflags_v<E>>>
constexpr BitFlags<E> operator~(E lhs)
{
    using U = std::underlying_type_t<E>;
    return BitFlags<E>(~static_cast<U>(lhs));
}

template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && enable_bitflags_v<E>>>
constexpr BitFlags<E> operator|(E lhs, E rhs)
{
    using U = std::underlying_type_t<E>;
    return BitFlags<E>(static_cast<U>(lhs) | static_cast<U>(rhs));
}

template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && enable_bitflags_v<E>>>
constexpr BitFlags<E> operator&(E lhs, E rhs)
{
    using U = std::underlying_type_t<E>;
    return BitFlags<E>(static_cast<U>(lhs) & static_cast<U>(rhs));
}

template <typename E, typename = std::enable_if_t<std::is_enum_v<E> && enable_bitflags_v<E>>>
constexpr BitFlags<E> operator^(E lhs, E rhs)
{
    using U = std::underlying_type_t<E>;
    return BitFlags<E>(static_cast<U>(lhs) ^ static_cast<U>(rhs));
}

#endif // LYRA_LIBRARY_COMMON_BITFLAGS_H
