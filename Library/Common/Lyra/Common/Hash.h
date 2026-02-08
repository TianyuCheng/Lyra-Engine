#pragma once

#ifndef LYRA_LIBRARY_COMMON_HASH_H
#define LYRA_LIBRARY_COMMON_HASH_H

#include <functional>

namespace lyra
{

    template <class T>
    inline void hash_combine(std::size_t& seed, const T& v)
    {
        seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_HASH_H
