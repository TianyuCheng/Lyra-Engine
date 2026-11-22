#pragma once

#ifndef LYRA_ENGINE_UTILITIES_STDINT_H
#define LYRA_ENGINE_UTILITIES_STDINT_H

#include <cstdint>
#include <cstddef>

namespace lyra
{
    using ubyte  = uint8_t;
    using ushort = uint16_t;
    using uint   = uint32_t;
    using ulong  = uint64_t;
    using usize  = size_t;
    using isize  = ptrdiff_t;
} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_STDINT_H
