#pragma once

#ifndef LYRA_ENGINE_UTILITIES_CONVERSION_H
#define LYRA_ENGINE_UTILITIES_CONVERSION_H

#include <Lyra/Utilities/Macros.h>

namespace lyra
{
    template <typename U, typename T>
    FORCE_INLINE U as_type(T value)
    {
        static_assert(sizeof(T) == sizeof(U), "Expect T/U to be the same size!");
        return *reinterpret_cast<U*>(&value);
    }
} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_CONVERSION_H
