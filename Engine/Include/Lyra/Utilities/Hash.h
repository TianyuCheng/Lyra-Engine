#pragma once

#ifndef LYRA_ENGINE_UTILITIES_HASH_H
#define LYRA_ENGINE_UTILITIES_HASH_H

#include <functional>
#include <type_traits>

namespace lyra
{

    template <class T>
    constexpr void hash_combine(std::size_t& seed, const T& v)
    {
        if constexpr (std::is_integral_v<T>) {
            seed ^= static_cast<std::size_t>(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        } else {
            seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    }

} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_HASH_H
