#pragma once

#ifndef LYRA_LIBRARY_COMMON_MEMORY_H
#define LYRA_LIBRARY_COMMON_MEMORY_H

#include <Lyra/Common/Detail/MemoryArena.h>

namespace lyra
{

    template <typename... T>
    using MemoryArena = detail::MemoryArena<T...>;

} // end of namespace lyra

#endif // LYRA_LIBRARY_COMMON_MEMORY_H
