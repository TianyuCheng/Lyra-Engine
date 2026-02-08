#pragma once

#ifndef LYRA_LIBRARY_COMMON_POINTER_H
#define LYRA_LIBRARY_COMMON_POINTER_H

#include <memory>

namespace lyra
{
    template <typename T, class Deleter = std::default_delete<T>>
    using Own = std::unique_ptr<T, Deleter>;

    template <typename T>
    using Ref = std::shared_ptr<T>;

    template <typename T>
    struct Destroyer
    {
        void operator()(T* pointer)
        {
            pointer->destroy();
        }
    };

    template <typename T>
    using OwnedResource = std::unique_ptr<T, Destroyer<T>>;

} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_POINTER_H
