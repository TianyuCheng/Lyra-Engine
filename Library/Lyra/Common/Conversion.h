#pragma once

#ifndef LYRA_LIBRARY_COMMON_CONVERSION_H
#define LYRA_LIBRARY_COMMON_CONVERSION_H

namespace lyra
{
    template <typename U, typename T>
    U astype(T value)
    {
        static_assert(sizeof(T) == sizeof(U), "Expect T/U to be the same size!");
        return *reinterpret_cast<U*>(&value);
    }
} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_CONVERSION_H
